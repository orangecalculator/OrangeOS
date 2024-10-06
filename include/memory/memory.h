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

/**
 * @brief Simple implementation of memcpy with x86 specific functionality.
 *
 * @param dst Memory address to copy to.
 * @param src Memory address to copy from.
 * @param size Size of memory region to copy values.
 * @return void* The value of dst is returned as is.
 */
static inline void *kmemcpy(void *dst, const void *src, size_t size) {
  // #if defined(__GNUC__) || defined(__clang__)
  //   __asm__ inline("rep movsb"
  //                  :
  //                  /* ecx */ "+c"(size)
  //                  : /* edi */ "D"(dst),
  //                    /* eax */ "S"(src)
  //                  : "memory");
  // #else
  char *__dst = (char *)dst;
  char *__src = (char *)src;
  for (size_t i = 0; i < size; ++i) {
    __dst[i] = __src[i];
  }
  // #endif

  return dst;
}

/**
 * @brief Simple implementation of memcmp with x86 specific functionality.
 *
 * @param dst Memory address to compare to.
 * @param src Memory address to compare from.
 * @param size Size of memory region to copy values.
 * @return int The difference of the first different byte is returned.
 */
static inline int kmemcmp(const void *dst, const void *src, size_t size) {
  char *__dst = (char *)dst;
  char *__src = (char *)src;
  for (size_t i = 0; i < size; ++i) {
    if (__dst[i] != __src[i])
      return (int)__dst[i] - (int)__src[i];
  }

  return 0;
}

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* MEMORY_H */