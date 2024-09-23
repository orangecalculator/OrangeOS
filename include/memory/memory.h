#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @brief Simple implementation of memset with x86 specific functionality.
 *
 * @param mem Memory address to set value.
 * @param value Value to fill memory region.
 * @param size Size of memory region to set value.
 * @return void* The value of mem is returned as is.
 */
static inline void *kmemset(void *mem, int value, size_t size) {
#if defined(__GNUC__) || defined(__clang__)
  __asm__ inline("rep stosb"
                 :
                 /* ecx */ "+c"(size)
                 : /* edi */ "D"(mem),
                   /* eax */ "a"(value)
                 : "memory");
#else
  char *__mem = (char *)mem;
  for (size_t i = 0; i < size; ++i) {
    __mem[i] = (char)value;
  }
#endif

  return mem;
}

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* MEMORY_H */