#include <stdint.h>

#include <display/display.h>

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
#define VIDEO_HEIGHT 20

struct terminal_t {
  unsigned int x;
  unsigned int y;
};

static struct terminal_t terminal_state;

// char unused_data[4 << 20] = {1, 0};

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
}

static inline void terminal_putchar_raw(char c, unsigned char color) {

  VIDEO_MEM[terminal_state.y * VIDEO_WIDTH + terminal_state.x] =
      terminal_make_char(c, color);
  terminal_state.x++;

  if (terminal_state.x >= VIDEO_WIDTH)
    terminal_linebreak();
}

int terminal_putchar_color(char c, unsigned char color) {
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