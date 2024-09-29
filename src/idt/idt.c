#include <stdbool.h>
#include <stdint.h>

#include <config.h>

#include <display/display.h>
#include <idt/idt.h>
#include <io/io.h>
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

#define X86_RESERVED_EXCEPTION_BASE 0
#define X86_RESERVED_EXCEPTION_COUNT 0x20
#define X86_PIC_EXCEPTION_8259_PRIMARY_BASE                                    \
  (X86_RESERVED_EXCEPTION_BASE + X86_RESERVED_EXCEPTION_COUNT)
#define X86_PIC_EXCEPTION_8259_PRIMARY_COUNT 0x08
#define X86_PIC_EXCEPTION_8259_SECONDARY_BASE                                  \
  (X86_PIC_EXCEPTION_8259_PRIMARY_BASE + X86_PIC_EXCEPTION_8259_PRIMARY_COUNT)
#define X86_PIC_EXCEPTION_8259_SECONDARY_COUNT 0x08
#define X86_USER_EXCEPTION_BASE                                                \
  (X86_PIC_EXCEPTION_8259_SECONDARY_BASE + X86_PIC_EXCEPTION_SECONDARY_COUNT)

#define X86_IRQ_PRIMARY_OFFSET_TIMER_INTERRUPT 0x00
#define X86_IRQ_PRIMARY_OFFSET_KEYBOARD_INTERRUPT 0x01
#define X86_IRQ_PRIMARY_OFFSET_CASCADE 0x02
#define X86_IRQ_PRIMARY_OFFSET_COM2 0x03
#define X86_IRQ_PRIMARY_OFFSET_COM1 0x04
#define X86_IRQ_PRIMARY_OFFSET_LPT2 0x05
#define X86_IRQ_PRIMARY_OFFSET_FLOPPY_DISK 0x06
#define X86_IRQ_PRIMARY_OFFSET_LPT1 0x07
#define X86_IRQ_SECONDARY_OFFSET_CMOS 0x00
#define X86_IRQ_SECONDARY_OFFSET_FREE0 0x01
#define X86_IRQ_SECONDARY_OFFSET_FREE1 0x02
#define X86_IRQ_SECONDARY_OFFSET_FREE2 0x03
#define X86_IRQ_SECONDARY_OFFSET_PS2 0x04
#define X86_IRQ_SECONDARY_OFFSET_FPU 0x05
#define X86_IRQ_SECONDARY_OFFSET_PRIMARY_ATA 0x06
#define X86_IRQ_SECONDARY_OFFSET_SECONDARY_ATA 0x07

#define X86_IRQ_TIMER_INTERRUPT                                                \
  (X86_PIC_EXCEPTION_8259_PRIMARY_BASE + X86_IRQ_PRIMARY_OFFSET_TIMER_INTERRUPT)
#define X86_IRQ_KEYBOARD_INTERRUPT                                             \
  (X86_PIC_EXCEPTION_8259_PRIMARY_BASE +                                       \
   X86_IRQ_PRIMARY_OFFSET_KEYBOARD_INTERRUPT)
#define X86_IRQ_CASCADE                                                        \
  (X86_PIC_EXCEPTION_8259_PRIMARY_BASE + X86_IRQ_PRIMARY_OFFSET_CASCADE)
#define X86_IRQ_COM2                                                           \
  (X86_PIC_EXCEPTION_8259_PRIMARY_BASE + X86_IRQ_PRIMARY_OFFSET_COM2)
#define X86_IRQ_COM1                                                           \
  (X86_PIC_EXCEPTION_8259_PRIMARY_BASE + X86_IRQ_PRIMARY_OFFSET_COM1)
#define X86_IRQ_LPT2                                                           \
  (X86_PIC_EXCEPTION_8259_PRIMARY_BASE + X86_IRQ_PRIMARY_OFFSET_LPT2)
#define X86_IRQ_FLOPPY_DISK                                                    \
  (X86_PIC_EXCEPTION_8259_PRIMARY_BASE + X86_IRQ_PRIMARY_OFFSET_FLOPPY_DISK)
#define X86_IRQ_LPT1                                                           \
  (X86_PIC_EXCEPTION_8259_PRIMARY_BASE + X86_IRQ_PRIMARY_OFFSET_LPT1)
#define X86_IRQ_CMOS                                                           \
  (X86_PIC_EXCEPTION_8259_SECONDARY_BASE + X86_IRQ_SECONDARY_OFFSET_CMOS)
#define X86_IRQ_FREE0                                                          \
  (X86_PIC_EXCEPTION_8259_SECONDARY_BASE + X86_IRQ_SECONDARY_OFFSET_FREE0)
#define X86_IRQ_FREE1                                                          \
  (X86_PIC_EXCEPTION_8259_SECONDARY_BASE + X86_IRQ_SECONDARY_OFFSET_FREE1)
#define X86_IRQ_FREE2                                                          \
  (X86_PIC_EXCEPTION_8259_SECONDARY_BASE + X86_IRQ_SECONDARY_OFFSET_FREE2)
#define X86_IRQ_PS2                                                            \
  (X86_PIC_EXCEPTION_8259_SECONDARY_BASE + X86_IRQ_SECONDARY_OFFSET_PS2)
#define X86_IRQ_FPU                                                            \
  (X86_PIC_EXCEPTION_8259_SECONDARY_BASE + X86_IRQ_SECONDARY_OFFSET_FPU)
#define X86_IRQ_PRIMARY_ATA                                                    \
  (X86_PIC_EXCEPTION_8259_SECONDARY_BASE + X86_IRQ_SECONDARY_OFFSET_PRIMARY_ATA)
#define X86_IRQ_SECONDARY_ATA                                                  \
  (X86_PIC_EXCEPTION_8259_SECONDARY_BASE +                                     \
   X86_IRQ_SECONDARY_OFFSET_SECONDARY_ATA)

#define X86_PIC_8259_PRIMARY_CONTROL_PORT 0x20
#define X86_PIC_8259_PRIMARY_MASK_PORT 0x21
#define X86_PIC_8259_SECONDARY_CONTROL_PORT 0xA0
#define X86_PIC_8259_SECONDARY_MASK_PORT 0xA1
#define X86_KBD_8259_DATA_PORT 0x60
#define X86_KBD_8259_COMMAND_PORT 0x64

#define X86_PIC_8259_COMMAND_ICW1_ICW4 0x01
#define X86_PIC_8259_COMMAND_ICW1_SINGLE 0x02
#define X86_PIC_8259_COMMAND_ICW1_INTERVAL4 0x04
#define X86_PIC_8259_COMMAND_ICW1_LEVEL 0x08
#define X86_PIC_8259_COMMAND_ICW1_INIT 0x10

#define X86_PIC_8259_COMMAND_ICW4_8086 0x01
#define X86_PIC_8259_COMMAND_ICW4_AUTO 0x02
#define X86_PIC_8259_COMMAND_ICW4_BUF_SLAVE 0x04
#define X86_PIC_8259_COMMAND_ICW4_BUF_MASTER 0x0C
#define X86_PIC_8259_COMMAND_ICW4_SFNM 0x10

#define X86_PIC_8259_COMMAND_ACK 0x20

static inline void x86_sti() { __asm__ inline("sti"); }

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

static void interrupt_handler_divzero() {
  terminal_print("Divide by zero.\n");
  while (1) { /* Do not return since this does not iret. */
  }
}

static void __idt_init_pic() {
  uint8_t primary_saved_mask;
  uint8_t secondary_saved_mask;

  primary_saved_mask = x86_inb(X86_PIC_8259_PRIMARY_MASK_PORT);
  secondary_saved_mask = x86_inb(X86_PIC_8259_SECONDARY_MASK_PORT);

  x86_outb(X86_PIC_8259_PRIMARY_CONTROL_PORT,
           X86_PIC_8259_COMMAND_ICW1_INIT | X86_PIC_8259_COMMAND_ICW1_ICW4);
  x86_io_wait();
  x86_outb(X86_PIC_8259_SECONDARY_CONTROL_PORT,
           X86_PIC_8259_COMMAND_ICW1_INIT | X86_PIC_8259_COMMAND_ICW1_ICW4);
  x86_io_wait();
  x86_outb(X86_PIC_8259_PRIMARY_MASK_PORT, X86_PIC_EXCEPTION_8259_PRIMARY_BASE);
  x86_io_wait();
  x86_outb(X86_PIC_8259_SECONDARY_MASK_PORT,
           X86_PIC_EXCEPTION_8259_SECONDARY_BASE);
  x86_io_wait();
  x86_outb(X86_PIC_8259_PRIMARY_MASK_PORT, 1 << X86_IRQ_PRIMARY_OFFSET_CASCADE);
  x86_io_wait();
  x86_outb(X86_PIC_8259_SECONDARY_MASK_PORT, X86_IRQ_PRIMARY_OFFSET_CASCADE);
  x86_io_wait();
  x86_outb(X86_PIC_8259_PRIMARY_MASK_PORT, X86_PIC_8259_COMMAND_ICW4_8086);
  x86_io_wait();
  x86_outb(X86_PIC_8259_SECONDARY_MASK_PORT, X86_PIC_8259_COMMAND_ICW4_8086);
  x86_io_wait();

  x86_outb(X86_PIC_8259_PRIMARY_MASK_PORT, primary_saved_mask);
  x86_io_wait();
  x86_outb(X86_PIC_8259_SECONDARY_MASK_PORT, secondary_saved_mask);
  x86_io_wait();
}

static void pic_notify_eoi(bool secondary) {
  if (secondary)
    x86_outb(X86_PIC_8259_SECONDARY_CONTROL_PORT, X86_PIC_8259_COMMAND_ACK);

  x86_outb(X86_PIC_8259_PRIMARY_CONTROL_PORT, X86_PIC_8259_COMMAND_ACK);
}

void irq_handler_entrypoint_timer();
void irq_handler_timer() {
  const int IRQ_TIMER_LOG_COUNT_MAX = 5;
  static int log_count = 0;

  if (log_count < IRQ_TIMER_LOG_COUNT_MAX) {
    terminal_print(".");
    log_count++;
  } else if (log_count == IRQ_TIMER_LOG_COUNT_MAX) {
    terminal_print(
        "Timer interrupt maximum log limit exceeded. Suppressing log from now on.\n");
    log_count++;
  }

  pic_notify_eoi(false);
}

void irq_handler_entrypoint_keyboard();
void irq_handler_keyboard() {
  terminal_print("irq_handler_keyboard\n");
  pic_notify_eoi(false);
}

void irq_handler_entrypoint_primary_unknown();

void irq_handler_primary_unknown() {
  x86_outb(X86_PIC_8259_PRIMARY_CONTROL_PORT, X86_PIC_8259_COMMAND_ACK);
}

void irq_handler_entrypoint_secondary_unknown();

void irq_handler_secondary_unknown() {
  x86_outb(X86_PIC_8259_PRIMARY_CONTROL_PORT, X86_PIC_8259_COMMAND_ACK);
}

void idt_init() {
  __idt_init_pic();

  kmemset(idt_table, 0, sizeof(idt_table));
  idt_set(&idt_table[0], interrupt_handler_divzero);

  for (int interruptno = X86_PIC_EXCEPTION_8259_PRIMARY_BASE;
       interruptno < X86_PIC_EXCEPTION_8259_PRIMARY_BASE +
                         X86_PIC_EXCEPTION_8259_PRIMARY_COUNT;
       ++interruptno) {
    idt_set(&idt_table[interruptno], irq_handler_entrypoint_primary_unknown);
  }
  for (int interruptno = X86_PIC_EXCEPTION_8259_SECONDARY_BASE;
       interruptno < X86_PIC_EXCEPTION_8259_SECONDARY_BASE +
                         X86_PIC_EXCEPTION_8259_SECONDARY_COUNT;
       ++interruptno) {
    idt_set(&idt_table[interruptno], irq_handler_entrypoint_secondary_unknown);
  }

  idt_set(&idt_table[X86_IRQ_TIMER_INTERRUPT], irq_handler_entrypoint_timer);
  idt_set(&idt_table[X86_IRQ_KEYBOARD_INTERRUPT],
          irq_handler_entrypoint_keyboard);

  idt_load_descriptor(idt_table, sizeof(idt_table) / sizeof(idt_table[0]));

  x86_sti();
}