#ifndef DISPLAY_H
#define DISPLAY_H

#include <base.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define TERMINAL_COLOR_DEFAULT 15

void terminal_init();

int terminal_putchar_color(char c, unsigned char color);

static inline int terminal_putchar(char c) {
  return terminal_putchar_color(c, TERMINAL_COLOR_DEFAULT);
}

int terminal_print(const char *str);

PRINTFLIKE(1, 2) int terminal_printk(const char *fmt, ...);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* DISPLAY_H */