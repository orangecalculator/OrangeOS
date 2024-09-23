#include <stdint.h>

#include <config.h>

#include <display/display.h>
#include <idt/idt.h>
#include <memory/memory.h>

#define IDT_ATTR_GATETYPE_BIT_COUNT 4
#define IDT_ATTR_RESERVED0_BIT_COUNT 1
#define IDT_ATTR_DPL_BIT_COUNT 2
#define IDT_ATTR_PRESENT_BIT_COUNT 1

#define IDT_ATTR_GATETYPE_BIT_OFFSET 0
#define IDT_ATTR_RESERVED0_BIT_OFFSET                                          \
  (IDT_ATTR_GATETYPE_BIT_OFFSET + IDT_ATTR_GATETYPE_BIT_COUNT)
#define IDT_ATTR_DPL_BIT_OFFSET                                                \
  (IDT_ATTR_RESERVED0_BIT_OFFSET + IDT_ATTR_RESERVED0_BIT_COUNT)
#define IDT_ATTR_PRESENT_BIT_OFFSET                                            \
  (IDT_ATTR_DPL_BIT_OFFSET + IDT_ATTR_DPL_BIT_COUNT)

#define IDT_ATTR_GATETYPE_BIT_MASK                                             \
  (((1U << IDT_ATTR_GATETYPE_BIT_COUNT) - 1U) << IDT_ATTR_GATETYPE_BIT_OFFSET)
#define IDT_ATTR_RESERVED0_BIT_MASK                                            \
  (((1U << IDT_ATTR_RESERVED0_BIT_COUNT) - 1U) << IDT_ATTR_RESERVED0_BIT_OFFSET)
#define IDT_ATTR_DPL_BIT_MASK                                                  \
  (((1U << IDT_ATTR_DPL_BIT_COUNT) - 1U) << IDT_ATTR_DPL_BIT_OFFSET)
#define IDT_ATTR_PRESENT_BIT_MASK                                              \
  (((1U << IDT_ATTR_PRESENT_BIT_COUNT) - 1U) << IDT_ATTR_PRESENT_BIT_OFFSET)

#define IDT_ATTR_GATETYPE_INTERRUPT32 (0b1110U << IDT_ATTR_GATETYPE_BIT_OFFSET)
#define IDT_ATTR_DPL_KERNEL (0U << IDT_ATTR_DPL_BIT_OFFSET)
#define IDT_ATTR_DPL_USER (3U << IDT_ATTR_DPL_BIT_OFFSET)
#define IDT_ATTR_PRESENT_ON (1U << IDT_ATTR_PRESENT_BIT_OFFSET)

struct __attribute__((packed)) idt_entry {
  uint16_t handler_lo;
  uint16_t segment;
  uint8_t reserved;
  uint8_t attr;
  uint16_t handler_hi;
};

struct __attribute__((packed)) idt_descriptor {
  uint16_t size;
  uint32_t table;
};

static struct idt_entry idt_table[CONFIG_NUM_INTERRUPTS];

static inline void idt_set(struct idt_entry *entry, void (*handler)()) {
  uint32_t handler32 = (uint32_t)handler;

  entry->handler_lo = (uint16_t)((handler32 & 0x0000ffffUL) >> 0);
  entry->segment = KERNEL_CODE_SELECTOR;
  /* Only support 32 bit interrupt available to user for now. */
  entry->attr =
      IDT_ATTR_PRESENT_ON | IDT_ATTR_DPL_USER | IDT_ATTR_GATETYPE_INTERRUPT32;
  entry->handler_hi = (uint16_t)((handler32 & 0xffff0000UL) >> 16);
}

static inline void idt_load_descriptor(struct idt_entry *table,
                                       size_t interrupt_count) {

  if (interrupt_count > UINT16_MAX / sizeof(struct idt_entry)) {
    terminal_print(
        "Cannot register interrupt descriptor. Provided interrupt count is too big.");
    return;
  }

  struct idt_descriptor descriptor = {
      .size = (uint16_t)(sizeof(struct idt_entry) * interrupt_count - 1),
      .table = (uint32_t)table,
  };

  // Reference: https://stackoverflow.com/questions/43577715/how-to-use-lidt-from-inline-assembly-to-load-an-interrupt-vector-table
  __asm__ inline("lidt %[descriptor]" : : [descriptor] "m"(descriptor));
}

static void interrupt_handler_divzero() { terminal_print("Divide by zero.\n"); }

void idt_init() {
  terminal_print("idt_init");

  kmemset(idt_table, 0, sizeof(idt_table));
  idt_set(&idt_table[0], interrupt_handler_divzero);

  idt_load_descriptor(idt_table, sizeof(idt_table) / sizeof(idt_table[0]));
}