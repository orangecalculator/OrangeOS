#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <wctype.h>

#include <sys/types.h>

#include <limits>
#include <type_traits>

#include <display/format.h>

constexpr int PRINT_CONVERSION_FLAG_LEFTADJUST = (1 << 0);
constexpr int PRINT_CONVERSION_FLAG_SIGNPREPEND = (1 << 1);
constexpr int PRINT_CONVERSION_FLAG_SPACEPREPEND = (1 << 2);
constexpr int PRINT_CONVERSION_FLAG_ALTFORMAT = (1 << 3);
constexpr int PRINT_CONVERSION_FLAG_ZEROPADDING = (1 << 4);
constexpr int PRINT_CONVERSION_FLAG_MINWIDTH = (1 << 5);
constexpr int PRINT_CONVERSION_FLAG_PRECISION = (1 << 6);

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

  void PutDecimalInt(intmax_t x, int flags, unsigned int minwidth,
                     unsigned int precision) {
    return PutIntImpl<intmax_t, 10, false>(x, flags, minwidth, precision);
  }

  void PutHexadecimalInt(intmax_t x, int flags, unsigned int minwidth,
                         unsigned int precision) {
    return PutIntImpl<intmax_t, 16, false>(x, flags, minwidth, precision);
  }

  size_t GetWrittenLength() const { return written_len; }

protected:
  static constexpr char tochar(unsigned int x, unsigned int radix,
                               bool capital) {
    if (x < 10)
      return (char)('0' + x);
    else
      return (char)((capital ? 'A' : 'a') + (x - 10));
  }

  template <typename IntType, unsigned int Radix, bool Capital>
  void PutIntImpl(IntType x, int flags, unsigned int minwidth,
                  unsigned int precision) {

    if (!(flags & PRINT_CONVERSION_FLAG_MINWIDTH))
      minwidth = 0;

    if (!(flags & PRINT_CONVERSION_FLAG_PRECISION))
      precision = 1;

    /* Determine prefix. */
    char prefix[16] = {};
    size_t prefix_len = 0;

    if (x < 0) {
      prefix[prefix_len++] = '-';
      x = -x;
    } else if (flags & PRINT_CONVERSION_FLAG_SIGNPREPEND) {
      prefix[prefix_len++] = '+';
    } else if (flags & PRINT_CONVERSION_FLAG_SPACEPREPEND) {
      prefix[prefix_len++] = ' ';
    }

    /* Number to string. */
    constexpr size_t BUFSIZE = CHAR_BIT * sizeof(IntType);

    /* The number of bits in the type would be sufficient here. */
    char valstr[BUFSIZE] = {};
    size_t valstrlen = 0;

    {
      /**
       * This may be calculated using the actual type,
       * but is done by long to support types larger than native values.
       *
       * Caution: the numbers are reversed at this point. 
       * When printing. the characters should be printed in reverse order.
       */

      using BaseIntType = unsigned long;

      static_assert(std::is_unsigned<BaseIntType>::value);

      constexpr size_t NWORD =
          (sizeof(IntType) + sizeof(BaseIntType) - 1) / sizeof(BaseIntType);
      static_assert(std::numeric_limits<BaseIntType>::max() > Radix * Radix);
      constexpr size_t BITS_IN_WORD = CHAR_BIT * sizeof(BaseIntType);
      constexpr BaseIntType WORD_MASK = ~static_cast<BaseIntType>(0);

      constexpr BaseIntType FULL_WORD_DIV =
          WORD_MASK / Radix + (WORD_MASK % Radix + 1 >= Radix ? 1 : 0);
      constexpr BaseIntType FULL_WORD_REM = (WORD_MASK % Radix + 1) % Radix;

      unsigned long word[NWORD];
      {
        IntType cur = x;
        for (unsigned int k = 0; k < NWORD; ++k) {
          word[NWORD - 1 - k] = (cur & WORD_MASK);
          cur >>= BITS_IN_WORD;
        }
      }

      while (true) {
        bool iszero = true;
        for (unsigned int k = 0; k < NWORD; ++k) {
          if (word[k] != 0) {
            iszero = false;
          }
        }

        if (iszero)
          break;

        BaseIntType rem = 0;
        for (unsigned int k = 0; k < NWORD; ++k) {
          BaseIntType nextrem = word[k] % Radix + rem * FULL_WORD_REM;
          word[k] = word[k] / Radix + rem * FULL_WORD_DIV + nextrem / Radix;
          rem = nextrem % Radix;
        }

        valstr[valstrlen++] = tochar((unsigned int)rem, Radix, Capital);
      }
    }

    /**
      * Alternative format is implementation defined for other specifiers.
      * Here, we choose to just ignore it.
      */
    if constexpr (Radix == 0x10) {
      if ((flags & PRINT_CONVERSION_FLAG_ALTFORMAT) && (x != 0)) {
        prefix[prefix_len++] = '0';
        prefix[prefix_len++] = (Capital ? 'x' : 'X');
      }
    }

    /* Determine padding. */
    size_t zeropad = 0;

    if (flags & PRINT_CONVERSION_FLAG_PRECISION) {
      /**
       * The precision is already determined,
       * but whether it was explicit or implicit matters here.
       */
      if (precision > valstrlen)
        zeropad = precision - valstrlen;
    } else if (flags & PRINT_CONVERSION_FLAG_LEFTADJUST) {
      if (1 > valstrlen)
        zeropad = 1 - valstrlen;
    } else if (flags & PRINT_CONVERSION_FLAG_ZEROPADDING) {
      if ((flags & PRINT_CONVERSION_FLAG_MINWIDTH) &&
          minwidth > prefix_len + valstrlen) {
        zeropad = minwidth - (prefix_len + valstrlen);
      }
      if (precision > valstrlen + zeropad)
        zeropad = precision - valstrlen;
    } else {
      if (precision > valstrlen)
        zeropad = precision - valstrlen;
    }

    if constexpr (Radix == 8) {
      if ((flags & PRINT_CONVERSION_FLAG_ALTFORMAT) && (zeropad < 1)) {
        zeropad = 1;
      }
    }

    size_t pad = (minwidth > prefix_len + zeropad + valstrlen
                      ? minwidth - (prefix_len + zeropad + valstrlen)
                      : 0);

    if (!(flags & PRINT_CONVERSION_FLAG_LEFTADJUST)) {
      for (size_t i = 0; i < pad; ++i)
        PutChar(' ');
    }

    for (size_t i = 0; i < prefix_len; ++i)
      PutChar(prefix[i]);

    for (size_t i = 0; i < zeropad; ++i)
      PutChar('0');

    for (size_t i = 0; i < valstrlen; ++i)
      PutChar(valstr[valstrlen - 1 - i]);

    if (flags & PRINT_CONVERSION_FLAG_LEFTADJUST) {
      for (size_t i = 0; i < pad; ++i)
        PutChar(' ');
    }
  }

private:
  size_t written_len = 0;
  format_putc_cb cb = nullptr;
  void *cb_ctx = nullptr;
};
} // namespace

namespace {
static inline constexpr bool isdigit(char c) { return ('0' <= c && c <= '9'); }
static inline constexpr unsigned int appenddigit(unsigned int x, char c) {
  if (x > std::numeric_limits<unsigned int>::max() / 10)
    return -1;
  else {
    int cadd = c - '0';

    x = x * 10;
    if (x > std::numeric_limits<unsigned int>::max() - cadd)
      return -1;
    return x + cadd;
  }
}

struct PrintConversionFlag {
  int flags = 0;
  unsigned int minwidth = 0;
  unsigned int precision = 0;
  LengthModifier modifier = LengthModifier::NONE;

  void AppendFlag(int flag) { flags |= flag; }
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
      if (w < 0) {
        flags.AppendFlag(PRINT_CONVERSION_FLAG_LEFTADJUST);
        w = -w;
      }
      flags.minwidth = static_cast<unsigned int>(w);
      c = *++fmt;
    }

    if (c == '.') {
      flags.AppendFlag(PRINT_CONVERSION_FLAG_PRECISION);

      c = *++fmt;
      if (isdigit(c)) {
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
      } else {
        /* The default precision is 0 in this case. */
        flags.precision = 0;
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

    case 'd': {
      intmax_t x;
      switch (flags.modifier) {
      case LengthModifier::NONE:
        x = va_arg(args, int);
        break;
      case LengthModifier::hh:
        x = (signed char)va_arg(args, int);
        break;
      case LengthModifier::h:
        x = (short)va_arg(args, int);
        break;
      case LengthModifier::l:
        x = va_arg(args, long);
        break;
      case LengthModifier::ll:
        x = va_arg(args, long long);
        break;
      case LengthModifier::j:
        x = va_arg(args, intmax_t);
        break;
      case LengthModifier::z:
        x = va_arg(args, ssize_t);
        break;
      case LengthModifier::t:
        x = va_arg(args, ptrdiff_t);
        break;
      default:
        return -1;
      }
      formatter.PutDecimalInt(x, flags.flags, flags.minwidth, flags.precision);
    } break;

    /* Not implemented. */
    default:
      return -1;
    }

    fmt++;
  }

  return 0;
}