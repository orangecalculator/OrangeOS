#include <stddef.h>
#include <wctype.h>

#include <limits>
#include <type_traits>

#include <display/format.h>

constexpr int PRINT_CONVERSION_FLAG_LEFTADJUST = (1 << 0);
constexpr int PRINT_CONVERSION_FLAG_SIGNPREPEND = (1 << 1);
constexpr int PRINT_CONVERSION_FLAG_SPACEPREPEND = (1 << 2);
constexpr int PRINT_CONVERSION_FLAG_ALTFORMAT = (1 << 3);
constexpr int PRINT_CONVERSION_FLAG_ZEROPADDING = (1 << 4);
constexpr int PRINT_CONVERSION_FLAG_MINWIDTH = (1 << 5);
constexpr int PRINT_CONVERSION_FLAG_PRECISION = (1 < 6);

enum LengthModifier {
  hh,
  h,
  NONE,
  l,
  ll,
  j,
  z,
  t,
  L,
};

struct print_wrap_ctx_t {
  size_t written_len;
  format_putc_cb cb;
  void *cb_ctx;
};

static inline void print_wrap_cb(struct print_wrap_ctx_t *ctx, char c) {
  ctx->written_len++;
  ctx->cb(ctx->cb_ctx, c);
}

namespace {
class PrintFormatter final {
public:
  PrintFormatter(format_putc_cb cb_, void *cb_ctx_)
      : cb(cb_), cb_ctx(cb_ctx_) {}

  void PutChar(char c) {
    written_len++;
    cb(cb_ctx, c);
  }
  void PutChar(wchar_t c) {
    PutChar('?');
    PutChar('?');
  }

  template <typename CharType> void PutStr(CharType *s) {
    while (true) {
      CharType c = *s;

      if (c == 0)
        break;

      PutChar(c);
    }
  }

  static constexpr char tochar(int x, int radix, bool capital) {
    if (x < 10)
      return (char)('0' + x);
    else
      return (char)((capital ? 'A' : 'a') + (x - 10));
  }

  template <typename IntType, unsigned int Radix, bool Capital>
  void PutInt(IntType x, int flags, int minwidth, int precision) {
    if constexpr (std::is_signed<IntType>::value) {
      constexpr size_t BUFSIZE = 32;
      for (IntType val : {std::numeric_limits<IntType>::max(),
                          std::numeric_limits<IntType>::min()}) {
        for (unsigned int i = 0; i < BUFSIZE - 1; ++i)
          val /= Radix;
        static_assert(val == 0);
      }

      char buf[BUFSIZE] = {};
      size_t len = 0;

      {
        IntType cur = x;
        do {
          buf[len++] = tochar(cur % Radix, Radix, Capital);
          cur /= Radix;
        } while (cur != 0);
      }

    } else {
    }
  }

  size_t GetWrittenLength() const { return written_len; }

private:
  size_t written_len = 0;
  format_putc_cb cb = nullptr;
  void *cb_ctx = nullptr;
};
} // namespace

namespace {
static inline constexpr bool isdigit(char c) { return ('0' <= c && c <= '9'); }
static inline constexpr int appenddigit(int x, char c) {
  if (x < 0 || x > std::numeric_limits<int>::max() / 10)
    return -1;
  else {
    int cadd = c - '0';

    x = x * 10;
    if (x > std::numeric_limits<int>::max() - cadd)
      return -1;
    return x + cadd;
  }
}

struct PrintConversionFlag {
  int flags = 0;
  int minwidth = 0;
  int precision = 0;
  LengthModifier modifier = LengthModifier::NONE;

  void AppendFlag(int flag) { flags |= flags; }
  void AppendMinWidth(char c) { minwidth = appenddigit(minwidth, c); }
  void AppendPrecision(char c) { precision = appenddigit(precision, c); }
};

} // namespace

int vcbprintf(format_putc_cb _cb, void *_cb_ctx, const char *fmt,
              va_list args) {

  PrintFormatter formatter(_cb, _cb_ctx);

  while (true) {
    char c = *fmt;
    if (c == '\0')
      break;

    if (c != '%') {
      formatter.PutChar(c);
      fmt++;
      continue;
    }

    c = *++fmt;

    PrintConversionFlag flags;

    while (true) {
      if (c == '-') {
        flags.AppendFlag(PRINT_CONVERSION_FLAG_LEFTADJUST);
      } else if (c == '+') {
        flags.AppendFlag(PRINT_CONVERSION_FLAG_SIGNPREPEND);
      } else if (c == ' ') {
        flags.AppendFlag(PRINT_CONVERSION_FLAG_SPACEPREPEND);
      } else if (c == '#') {
        flags.AppendFlag(PRINT_CONVERSION_FLAG_ALTFORMAT);
      } else if (c == '0') {
        flags.AppendFlag(PRINT_CONVERSION_FLAG_ZEROPADDING);
      } else {
        break;
      }
      c = *++fmt;
    }

    if (isdigit(c)) {
      flags.AppendFlag(PRINT_CONVERSION_FLAG_MINWIDTH);

      while (true) {
        if (!isdigit(c))
          break;

        flags.AppendMinWidth(c);
        c = *++fmt;
      }
      if (flags.minwidth < 0)
        return -1;
    } else if (c == '*') {
      flags.AppendFlag(PRINT_CONVERSION_FLAG_MINWIDTH);

      int w = va_arg(args, int);
      if (w < 0)
        flags.AppendFlag(PRINT_CONVERSION_FLAG_LEFTADJUST);
      w = -w;
      flags.minwidth = w;
      c = *++fmt;
    }

    if (c == '.') {
      flags.AppendFlag(PRINT_CONVERSION_FLAG_PRECISION);

      c = *++fmt;
      if (isdigit(c) && c != '0') {
        while (true) {
          flags.AppendPrecision(c);
          c = *++fmt;
          if (isdigit(c))
            flags.AppendPrecision(c);
          else
            break;
        }
      } else if (c == '*') {
        int p = va_arg(args, int);
        if (p >= 0)
          flags.precision = p;
        c = *++fmt;
      }
    }

    switch (c) {
    case 'h': {
      c = *++fmt;
      if (c != 'h') {
        flags.modifier = LengthModifier::h;
      } else {
        c = *++fmt;
        flags.modifier = LengthModifier::hh;
      }
    } break;
    case 'l': {
      c = *++fmt;
      if (c != 'l') {
        flags.modifier = LengthModifier::l;
      } else {
        c = *++fmt;
        flags.modifier = LengthModifier::ll;
      }
    } break;
    case 'j': {
      c = *++fmt;
      flags.modifier = LengthModifier::j;
    } break;
    case 'z': {
      c = *++fmt;
      flags.modifier = LengthModifier::z;
    } break;
    case 't': {
      c = *++fmt;
      flags.modifier = LengthModifier::t;
    } break;
    case 'L': {
      c = *++fmt;
      flags.modifier = LengthModifier::L;
    } break;
    }

    switch (c) {
    case '%': {
      if (flags.flags != 0 || flags.modifier != LengthModifier::NONE)
        return -1;
      formatter.PutChar('%');
      fmt++;
    } break;

    case 'c': {
      switch (flags.modifier) {
      case LengthModifier::NONE: {
        formatter.PutChar((char)va_arg(args, int));
      } break;

      case LengthModifier::l: {
        formatter.PutChar((wchar_t)va_arg(args, wint_t));
      } break;
      default:
        return -1;
      }
    } break;

    case 's': {
      switch (flags.modifier) {
      case LengthModifier::NONE: {
        formatter.PutStr(va_arg(args, char *));
      } break;

      case LengthModifier::l: {
        formatter.PutStr(va_arg(args, wchar_t *));
      } break;
      default:
        return -1;
      }
    } break;

    /* Not implemented. */
    default:
      return -1;
    }
  }

  return 0;
}