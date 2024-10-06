#ifndef BASE_H
#define BASE_H

#ifndef NULL
#define NULL ((void *)0)
#endif /* NULL */

#ifdef __GNUC__
#define PRINTFLIKE(a, b) __attribute__((format(printf, (a), (b))))
#else
#define PRINTFLIKE(a, b)
#endif /* __GNUC__ */

#endif /* BASE_H */