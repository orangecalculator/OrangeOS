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
  static void print_error(int ret) {
    constexpr char errstr[] = "vcbprintf test worker error";

    char retbuf[sizeof(errstr) + 32] = "";
    size_t retbuflen = sizeof(errstr) - 1;

    kmemcpy(retbuf, errstr, sizeof(errstr) - 1);

    retbuf[retbuflen++] = ' ';

    if (ret < 0) {
      retbuf[retbuflen++] = '-';
      ret = -ret;
    }

    for (int k = 0; ret != 0 && k < 16; ++k) {
      retbuf[retbuflen++] = (char)('0' + ret % 10);
      ret /= 10;
    }

    retbuf[retbuflen++] = '\n';

    terminal_print(retbuf);
  }

  void test(const char *expect, const char *fmt, ...) {
    int ret;
    va_list args;

    buffer.clear();

    va_start(args, fmt);

    ret = vcbprintf(append_char, &buffer, fmt, args);

    va_end(args);

    if (ret) {
      print_error(ret);
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
        print_error(ret);

        terminal_print(expect);
        terminal_putchar('\n');

        terminal_print(buffer.get_buf());
        terminal_putchar('\n');
      }
    }
  }

private:
  test_buffer_t buffer;
};

void vcbprintf_test() {
  vcbprintf_test_worker test;

  terminal_print("vcbprintf_test enabled.\n");
  test.test("%%", "%%");
  terminal_print("vcbprintf_test finished.\n");
}