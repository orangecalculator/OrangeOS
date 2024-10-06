#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#include <display/display.h>
#include <display/format.h>

typedef uint16_t video_mem_entry_t;
static inline uint16_t terminal_make_char(char c, unsigned char color) {
  return color << 8 | c;
}

/**
 * For now, we just use most of the data as constant.
 * However, this may be extended to support multiple output channels.
 */
#define VIDEO_MEM ((video_mem_entry_t *)0xB8000)
#define VIDEO_WIDTH 80
#define VIDEO_HEIGHT 25

struct terminal_t {
  unsigned int x;
  unsigned int y;
};

static inline bool terminal_state_writable(const struct terminal_t *state) {
  if (state == NULL)
    return false;

  return (0 <= state->x && state->x < VIDEO_WIDTH && 0 <= state->y &&
          state->y < VIDEO_HEIGHT);
}

static struct terminal_t terminal_state;

void terminal_init() {
  for (unsigned int y = 0; y < VIDEO_HEIGHT; ++y) {
    for (unsigned int x = 0; x < VIDEO_WIDTH; ++x) {
      VIDEO_MEM[y * VIDEO_WIDTH + x] = terminal_make_char(' ', 0);
    }
  }

  terminal_state.x = 0;
  terminal_state.y = 0;
}

static inline void terminal_linebreak() {
  terminal_state.y++;
  terminal_state.x = 0;

  if (terminal_state.y < 0 || terminal_state.y >= 2 * VIDEO_HEIGHT - 1)
    return terminal_init();
  else if (terminal_state.y >= VIDEO_HEIGHT) {
    int nbreak = terminal_state.y - (VIDEO_HEIGHT - 1);

    for (int i = 0; i < VIDEO_WIDTH * (VIDEO_HEIGHT - nbreak); ++i)
      VIDEO_MEM[i] = VIDEO_MEM[i + (VIDEO_WIDTH * nbreak)];
    for (int i = VIDEO_WIDTH * (VIDEO_HEIGHT - nbreak);
         i < VIDEO_WIDTH * VIDEO_HEIGHT; ++i)
      VIDEO_MEM[i] = terminal_make_char(' ', 0);

    terminal_state.y -= nbreak;
  }
}

static inline void terminal_putchar_raw(char c, unsigned char color) {

  VIDEO_MEM[terminal_state.y * VIDEO_WIDTH + terminal_state.x] =
      terminal_make_char(c, color);
  terminal_state.x++;

  if (terminal_state.x >= VIDEO_WIDTH)
    terminal_linebreak();
}

int terminal_putchar_color(char c, unsigned char color) {

  /* Just ignore input if terminal is not in writable state for now. */
  if (!terminal_state_writable(&terminal_state))
    return -1;

  switch (c) {
  case '\r':
    terminal_state.x = 0;
    break;
  case '\n':
    terminal_linebreak();
    break;

  default:
    terminal_putchar_raw(c, color);
    break;
  }

  return 0;
}

static inline int
terminal_print_color_cb(int (*putchar_color_cb)(char c, unsigned char color),
                        const char *str, unsigned char color) {
  int ret;
  char c;

  if (putchar_color_cb == NULL || str == NULL)
    return -1;

  while (1) {
    c = *str++;
    if (!c)
      break;

    ret = putchar_color_cb(c, color);
    if (ret)
      return ret;
  }

  return 0;
}

int terminal_print(const char *str) {
  return terminal_print_color_cb(terminal_putchar_color, str,
                                 TERMINAL_COLOR_DEFAULT);
}

static inline void terminal_putchar_default_color_cb(void *ctx, char c) {
  int ret = terminal_putchar_color(c, TERMINAL_COLOR_DEFAULT);
  if (ret != 0) {
    if (ctx != NULL) {
      /* Save the first error. */
      int *ictx = (int *)ctx;
      if (*ictx == 0)
        *ictx = ret;
    }
  }
}

int terminal_printk(const char *fmt, ...) {
  int ret = 0, pret = 0;
  va_list args;

  va_start(args, fmt);

  ret = vcbprintf(terminal_putchar_default_color_cb, (void *)&pret, fmt, args);

  va_end(args);

  return (ret || pret);
}

// static inline void __mock_x86_outb(unsigned short port, unsigned char val) {
//   char buf[0x100] = "outb: ";
//   size_t count = 6;

//   const char HEX_CHARS[0x10] = "0123456789ABCDEF";

//   buf[count++] = HEX_CHARS[(port >> 12) & 0xf];
//   buf[count++] = HEX_CHARS[(port >> 8) & 0xf];
//   buf[count++] = HEX_CHARS[(port >> 4) & 0xf];
//   buf[count++] = HEX_CHARS[(port >> 0) & 0xf];
//   buf[count++] = ' ';
//   buf[count++] = HEX_CHARS[(val >> 4) & 0xf];
//   buf[count++] = HEX_CHARS[(val >> 0) & 0xf];
//   buf[count++] = '\n';

//   terminal_print(buf);

//   x86_outb(port, val);
// }
// static inline unsigned char __mock_x86_inb(unsigned short port) {
//   unsigned char val = x86_inb(port);

//   char buf[0x100] = "inb: ";
//   size_t count = 5;

//   const char HEX_CHARS[0x10] = "0123456789ABCDEF";

//   buf[count++] = HEX_CHARS[(port >> 12) & 0xf];
//   buf[count++] = HEX_CHARS[(port >> 8) & 0xf];
//   buf[count++] = HEX_CHARS[(port >> 4) & 0xf];
//   buf[count++] = HEX_CHARS[(port >> 0) & 0xf];
//   buf[count++] = ' ';
//   buf[count++] = '-';
//   buf[count++] = '>';
//   buf[count++] = ' ';
//   buf[count++] = HEX_CHARS[(val >> 4) & 0xf];
//   buf[count++] = HEX_CHARS[(val >> 0) & 0xf];
//   buf[count++] = '\n';

//   terminal_print(buf);

//   return val;
// }