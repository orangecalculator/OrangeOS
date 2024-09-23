#include <stdint.h>

#include <display/display.h>
#include <idt/idt.h>
#include <kernel.h>

void kernel_main() {

  terminal_init();

  terminal_print("Hello from kernel!\n");
  terminal_print("Hello from kernel!\n");
  terminal_print("Hello from kernel!\rHello from second line!\n");

  idt_init();

  while (1) {
  }
}