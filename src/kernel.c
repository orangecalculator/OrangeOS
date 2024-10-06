#include <stdint.h>

#include <display/display.h>
#include <idt/idt.h>
#include <kernel.h>

#ifdef TEST_VCBPRINTF
#include <display/vcbprintf_test.h>
#endif /* TEST_VCBPRINTF */

void kernel_main() {

  terminal_init();

  terminal_print("Hello from kernel!\n");
  terminal_print("Hello from kernel!\n");
  terminal_print("Hello from kernel!\rHello from second line!\n");

  idt_init();

#ifdef TEST_VCBPRINTF
  vcbprintf_test();
#endif

  while (1) {
  }
}