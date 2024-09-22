#include <stdint.h>

#include <display.h>
#include <kernel.h>

void kernel_main() {

  terminal_init();

  terminal_print("Hello from kernel!\n");

  while (1) {
  }
}