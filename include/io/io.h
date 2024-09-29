#ifndef IO_H
#define IO_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

static inline unsigned char x86_inb(unsigned short port) {
  unsigned char val = 0;
  __asm__ inline("inb (%%dx), %%al" : "=a"(val) : "d"(port));
  return val;
}

static inline unsigned short x86_inw(unsigned short port) {
  unsigned short val = 0;
  __asm__ inline("inw (%%dx), %%ax" : "=a"(val) : "d"(port));
  return val;
}

static inline void x86_outb(unsigned short port, unsigned char val) {
  __asm__ inline("outb %%al, (%%dx)" : : "d"(port), "a"(val));
}

static inline void x86_outw(unsigned short port, unsigned short val) {
  __asm__ inline("outw %%ax, (%%dx)" : : "d"(port), "a"(val));
}

static inline void x86_io_wait() {
  /**
   * @brief IO wait by reading from an unused port.
   * @ref https://wiki.osdev.org/Inline_Assembly/Examples#I/O_access
   */
  x86_outb(0x80, 0);
}

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* IO_H */