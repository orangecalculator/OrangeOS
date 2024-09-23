#ifndef IO_H
#define IO_H

static inline unsigned char inb(unsigned short port) {
  unsigned char val = 0;
  __asm__ inline("inb (%%dx), %%al" : "=a"(val) : "d"(port));
  return val;
}

static inline unsigned short inw(unsigned short port) {
  unsigned short val = 0;
  __asm__ inline("inw (%%dx), %%ax" : "=a"(val) : "d"(port));
  return val;
}

static inline void outb(unsigned short port, unsigned char val) {
  __asm__ inline("outb %%al, (%%dx)" : : "d"(port), "a"(val));
}

static inline void outw(unsigned short port, unsigned short val) {
  __asm__ inline("outw %%ax, (%%dx)" : : "d"(port), "a"(val));
}

#endif /* IO_H */