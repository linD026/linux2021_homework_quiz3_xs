#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "xs.h"

/**
 * fix  grow, / , %
 *
 */

static void xs_allocate_data(xs *x, size_t len, bool reallocate) {
  /* Medium string */
  if (len < LARGE_STRING_LEN) {
    x->ptr = reallocate ? realloc(x->ptr, (size_t)1 << x->capacity)
                        : malloc((size_t)1 << x->capacity);
    return;
  }

  /* Large string */
  x->is_large_string = 1;

  /* The extra 4 bytes are used to store the reference count */
  x->ptr = reallocate ? realloc(x->ptr, (size_t)(1 << x->capacity) + 4)
                      : malloc((size_t)(1 << x->capacity) + 4);

  xs_set_refcnt(x, 1);
}

xs *xs_new(xs *x, const void *p) {
  *x = xs_literal_empty();
  size_t len = strlen(p) + 1;
  /*linD026: this compare will plus 1 because size_t len = strlen(p) + 1; */
  if (len > 16) {
    x->capacity = ilog2(len) + 1;
    x->size = len - 1;
    x->is_ptr = true;
    x->sharing = false;
    x->reclaim = false;
    xs_allocate_data(x, x->size, 0);
    memcpy(xs_data(x), p, len);
  } else {
    memcpy(x->data, p, len);
    x->space_left = 15 - (len - 1);
  }
  return x;
}

/* grow up to specified size */
// fix
xs *xs_grow(xs *x, size_t len) {
  char buf[16];

  if (len <= xs_capacity(x))
    return x;

  /* Backup first */
  if (!xs_is_ptr(x))
    memcpy(buf, x->data, 16);

  x->is_ptr = true;
  x->capacity = ilog2(len) + 1;

  if (xs_is_ptr(x)) {
    xs_allocate_data(x, len, 1);
  } else {
    xs_allocate_data(x, len, 0);
    memcpy(xs_data(x), buf, 16);
  }
  return x;
}

static inline xs *xs_newempty(xs *x) {
  *x = xs_literal_empty();
  return x;
}

static inline xs *xs_free(xs *x) {
  if (xs_is_ptr(x) && xs_dec_refcnt(x) <= 0)
    free(x->ptr);
  return xs_newempty(x);
}

/**Lazy copy
A lazy copy is an implementation of a deep copy.
When initially copying an object, a (fast) shallow copy is used.
A counter is also used to track how many objects share the data.
When the program wants to modify an object, it can determine if
 the data is shared (by examining the counter) and can do a deep copy if needed.
 */

static bool xs_cow_lazy_copy(xs *x, char **data) {
  if (xs_get_refcnt(x) <= 1)
    return false;

  /* Lazy copy */
  xs_dec_refcnt(x);
  xs_allocate_data(x, x->size, 0);

  if (data) {
    memcpy(xs_data(x), *data, x->size);

    /* Update the newly allocated pointer */
    *data = xs_data(x);
  }
  return true;
}

xs *xs_concat(xs *string, const xs *prefix, const xs *suffix) {
  size_t pres = xs_size(prefix), sufs = xs_size(suffix), size = xs_size(string),
         capacity = xs_capacity(string);

  char *pre = xs_data(prefix), *suf = xs_data(suffix), *data = xs_data(string);

  // xs_cow_lazy_copy(string, &data);

  if (size + pres + sufs <= capacity) {
    memmove(data + pres, data, size);
    memcpy(data, pre, pres);
    memcpy(data + pres + size, suf, sufs + 1);

    if (xs_is_ptr(string))
      string->size = size + pres + sufs;
    else
      string->space_left = 15 - (size + pres + sufs);
  } else {
    xs tmps = xs_literal_empty();
    xs_grow(&tmps, size + pres + sufs);
    char *tmpdata = xs_data(&tmps);
    memcpy(tmpdata + pres, data, size);
    memcpy(tmpdata, pre, pres);
    memcpy(tmpdata + pres + size, suf, sufs + 1);
    xs_free(string);
    *string = tmps;
    string->size = size + pres + sufs;
  }
  return string;
}

xs *xs_trim(xs *x, const char *trimset) {
  if (!trimset[0])
    return x;

  char *dataptr = xs_data(x), *orig = dataptr;

  //if (xs_cow_lazy_copy(x, &dataptr))
  //  orig = dataptr;

  /* similar to strspn/strpbrk but it operates on binary data */
  uint8_t mask[32] = {0};
  /*linD026: trmset max char num is 32.*/

/*linD026: it doesn't reset the bit, just check, skip when is it.*/
// bitwise  first << >> then & |
#define check_bit(byte) (mask[(uint8_t)byte / 8] & 1 << (uint8_t)byte % 8)
#define set_bit(byte) (mask[(uint8_t)byte / 8] |= 1 << (uint8_t)byte % 8)
  size_t i, slen = xs_size(x), trimlen = strlen(trimset);

  for (i = 0; i < trimlen; i++)
    set_bit(trimset[i]);
  for (i = 0; i < slen; i++)
    if (!check_bit(dataptr[i]))
      break;
  for (; slen > 0; slen--)
    if (!check_bit(dataptr[slen - 1]))
      break;
  dataptr += i;
  slen -= i;
  /*linD026: dataptr finially will point to the string that we want.*/

  /* reserved space as a buffer on the heap.
   * Do not reallocate immediately. Instead, reuse it as possible.
   * Do not shrink to in place if < 16 bytes.
   */
  memmove(orig, dataptr, slen);
  /* do not dirty memory unless it is needed */
  if (orig[slen])
    orig[slen] = 0;

  if (xs_is_ptr(x))
    x->size = slen;
  else
    x->space_left = 15 - slen;
  return x;
#undef check_bit
#undef set_bit
}

// flag 2 (sharing) : set shared or no
// need change allocate
bool xs_cow_copy(xs *dest, xs *src) {
  if (xs_is_ptr(src) && xs_is_large_string(src)) {
    xs_free(dest);
    memcpy(dest, src, sizeof(xs));
    dest->sharing = true;
    xs_set_refcnt(dest, 1);
    xs_inc_refcnt(src);
    return true;
  }
  return false;
}

// src can not dec_refcnt
void __xs_cow_write(xs *dest, xs *src) {
  if (!xs_is_ptr(dest) && !xs_is_ptr(src) &&
      !strncmp(xs_data(dest), xs_data(src), xs_size(src)))
    return;
  dest->sharing = false;
  char *temp = xs_data(dest);
  xs_allocate_data(dest, dest->size, 0);
  memcpy(xs_data(dest), temp, xs_size(dest));

  xs_dec_refcnt(src);
  if (xs_get_refcnt(src) < 1)
    xs_free(src);
}

void xs_reclaim_data(xs *x, bool fixed) {
  if (!xs_is_ptr(x) || x->sharing)
    return;
  
  if (fixed) {
    if (xs_is_large_string(x))
      x->ptr = realloc(x->ptr, x->size + 4);
    else 
      x->ptr = realloc(x->ptr, x->size);
    x->reclaim = true;
  }
  else {
    if (xs_is_large_string(x))
      x->ptr = realloc(x->ptr, (size_t)(1 << x->capacity) + 4);
    else 
      x->ptr = realloc(x->ptr, (size_t)(1 << x->capacity));
    x->reclaim = false;
  }
}