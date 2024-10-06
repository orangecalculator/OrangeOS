#ifndef FORMAT_H
#define FORMAT_H

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef void (*format_putc_cb)(void *ctx, char c);

int vcbprintf(format_putc_cb cb, void *cb_ctx, const char *fmt, va_list arg);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* FORMAT_H */