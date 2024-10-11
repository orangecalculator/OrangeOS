#include <stdarg.h>
#include <stddef.h>

#include <display/display.h>
#include <display/format.h>
#include <memory/memory.h>

#include <display/vcbprintf_test.h>

enum {
  TEST_ERROR_OK = 0,
  TEST_ERROR_NOSPACE = -1,
  TEST_ERROR_MISMATCH = -2,
};

struct test_buffer_t {
  static constexpr size_t TEST_BUFFER_SIZE = 0x1000;

  void clear() {
    kmemset(buf, 0, sizeof(buf));
    buflen = 0;
    ret = 0;
  }

  void append_char(char c) {
    if (buflen + 1 < TEST_BUFFER_SIZE)
      buf[buflen++] = c;
    else
      ret = TEST_ERROR_NOSPACE;
  }

  const char *get_buf() const { return buf; }

private:
  char buf[TEST_BUFFER_SIZE] = {};
  size_t buflen = 0;
  int ret = 0;
};

static inline void append_char(void *_ctx, char c) {
  test_buffer_t *ctx = static_cast<test_buffer_t *>(_ctx);

  ctx->append_char(c);
}

class vcbprintf_test_worker {
public:
  static void print_integer_raw(int value) {
    if (value < 0) {
      terminal_putchar('-');
      value = -value;
    }

    for (int k = 0; value != 0 && k < 16; ++k) {
      terminal_putchar((char)('0' + value % 10));
      value /= 10;
    }
  }

  void test(const char *expect, const char *fmt, ...) {
    int ret;
    va_list args;

    buffer.clear();

    va_start(args, fmt);

    ret = vcbprintf(append_char, &buffer, fmt, args);

    va_end(args);

    if (ret) {
      terminal_print("vcbprintf test worker error ");
      print_integer_raw(ret);
      terminal_putchar('\n');
      return;
    }

    {
      const char *__expect = expect;
      const char *__actual = buffer.get_buf();

      while (true) {
        char e = *__expect++;
        char a = *__actual++;

        if (e == 0 && a == 0)
          return;

        if (e != a) {
          ret = TEST_ERROR_MISMATCH;
          break;
        }
      }

      if (ret) {
        terminal_print("vcbprintf test worker expected from format |");
        terminal_print(fmt);
        terminal_print("| but got\n");

        terminal_print("expect: ");
        terminal_print(expect);
        terminal_putchar('\n');

        terminal_print("actual: ");
        terminal_print(buffer.get_buf());
        terminal_putchar('\n');
      }
    }
  }

  void test_error(int ret_expect, const char *fmt, ...) {
    int ret;
    va_list args;

    buffer.clear();

    va_start(args, fmt);

    ret = vcbprintf(append_char, &buffer, fmt, args);

    va_end(args);

    if (!ret) {
      terminal_print("vcbprintf test worker expected error from format |");
      terminal_print(fmt);
      terminal_print("| error ");
      print_integer_raw(ret_expect);
      terminal_print(" but got success: \nexpect: ");
      terminal_print(buffer.get_buf());
      terminal_putchar('\n');
      return;
    }

    if (ret != ret_expect) {
      terminal_print("vcbprintf test worker expected error |");
      terminal_print(fmt);
      terminal_print("| error ");
      print_integer_raw(ret_expect);
      terminal_print(" but got error ");
      print_integer_raw(ret);
      terminal_putchar('\n');
    }
  }

private:
  test_buffer_t buffer;
};

void vcbprintf_test() {
  vcbprintf_test_worker test;

  terminal_print("vcbprintf_test enabled.\n");
  test.test("%%", "%%");
  test.test_error(-5, "%%");
  terminal_print("vcbprintf_test finished.\n");
}