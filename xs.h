#ifndef __XS_H_
#define __XS_H_
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_STR_LEN_BITS (54)
#define MAX_STR_LEN ((1UL << MAX_STR_LEN_BITS) - 1)

#define LARGE_STRING_LEN 256

// http://gundambox.github.io/2015/10/30/C%E8%AA%9E%E8%A8%80-struct%E3%80%81union%E3%80%81enum/
typedef union {
  /* allow strings up to 15 bytes to stay on the stack
   * use the last byte as a null terminator and to store flags
   * much like fbstring:
   * https://github.com/facebook/folly/blob/master/folly/docs/FBString.md
   */
  char data[16];

  // stack
  struct {
    uint8_t filler[15],
        /* how many free bytes in this stack allocated string
         * same idea as fbstring
         */
        space_left : 4,
        /* if it is on heap, set 1 */
        is_ptr : 1, is_large_string : 1, sharing : 1, reclaim : 1;
    /* if is cow that is sharing string, "sharing" set 1 */
  };

  /* heap allocated */
  struct {
    char *ptr;
    /* supports strings up to 2^MAX_STR_LEN_BITS - 1 bytes */
    size_t size : MAX_STR_LEN_BITS,
                  /* capacity is always a power of 2 (unsigned)-1 */
                  capacity : 6;
    /* the last 4 bits are important flags */
  };
} xs;

static inline bool xs_is_ptr(const xs *x) { return x->is_ptr; }

static inline bool xs_is_large_string(const xs *x) {
  return x->is_large_string;
}

static inline size_t xs_size(const xs *x) {
  return xs_is_ptr(x) ? x->size : 15 - x->space_left;
}

static inline char *xs_data(const xs *x) {
  if (!xs_is_ptr(x))
    return (char *)x->data;

  if (xs_is_large_string(x))
    return (char *)(x->ptr + 4);
  /**
   * linD026:
   * x->ptr is "char *ptr;"
   * =>   _ _ _ _ ..
   *    (ref count)   https://en.wikipedia.org/wiki/Reference_counting#Rust
   * can see the xs_allocate_data function:
   *     x->ptr = reallocate ? realloc(x->ptr, (size_t)(1 << x->capacity) + 4)
   *                         : malloc((size_t)(1 << x->capacity) + 4);
   * and the xs_dec_refcnt function:
   *     return --(*(int *) ((size_t) x->ptr));
   * it transform to the int.
   */

  // midium
  return (char *)x->ptr;
}

static inline size_t xs_capacity(const xs *x) {
  return xs_is_ptr(x) ? ((size_t)1 << x->capacity) - 1 : 15;
}

static inline void xs_set_refcnt(const xs *x, int val) {
  *((int *)((size_t)x->ptr)) = val;
}

static inline void xs_inc_refcnt(const xs *x) {
  if (xs_is_large_string(x))
    ++(*(int *)((size_t)x->ptr));
}

static inline int xs_dec_refcnt(const xs *x) {
  if (!xs_is_large_string(x))
    return 0;
  return --(*(int *)((size_t)x->ptr));
}

static inline int xs_get_refcnt(const xs *x) {
  if (!xs_is_large_string(x))
    return 0;
  return *(int *)((size_t)x->ptr);
}

#define xs_literal_empty()                                                     \
  (xs) { .space_left = 15 }

/* lowerbound (floor log2) */
static inline int ilog2(uint32_t n) { return 32 - __builtin_clz(n) - 1; }
/**
 * linD026: int is 32 bit,32 - __builtin_clz(n) is the number of the position.
 * ex: n = 3    ..0010 => 2
 * lowerbound:  Dually, a lower bound or minorant of S is defined to be an
 * element of K which is less than or equal to every element of S. ex: n = 3  =>
 * 32 - __builtin_clz(n) get 2 => log2(2 - 1) = 2
 */
/**
 * linD026:
 * https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html
 *
 * Built-in Function: int __builtin_clz (unsigned int x)
 * Returns the number of leading 0-bits in x, starting at the most significant
 * bit position. If x is 0, the result is undefined.
 */

/* Memory leaks happen if the string is too long but it is still useful for
 * short strings.
 */
#define xs_tmp(x)                                                              \
  ((void)((struct {                                                            \
     _Static_assert(sizeof(x) <= MAX_STR_LEN, "it is too big");                \
     int dummy;                                                                \
   }){1}),                                                                     \
   xs_new(&xs_literal_empty(), x))

xs *xs_new(xs *x, const void *p);
xs *xs_grow(xs *x, size_t len);
xs *xs_concat(xs *string, const xs *prefix, const xs *suffix);
xs *xs_trim(xs *x, const char *trimset);

bool xs_cow_copy(xs *dest, xs *src);
void __xs_cow_write(xs *dest, xs *src);

#define xs_cow_write_trim(cpy, trimeset, src)                                  \
  do {                                                                         \
    __xs_cow_write(cpy, src);                                                       \
    xs_trim(cpy, trimeset);                                                     \
  } while (0)

#define xs_cow_write_concat(cpy, prefix, suffix, src)                          \
  do {                                                                         \
    __xs_cow_write(cpy, src);                                                  \
    xs_concat(cpy, prefix, suffix);                                            \
  } while (0)

void xs_reclaim_data(xs *x, bool fixed);

#endif