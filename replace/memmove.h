
#ifndef _PORTABLE_MEMMOVE_H_
#define _PORTABLE_MEMMOVE_H_

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#ifdef HAVE_MEMMOVE
  #include <string.h>
#else
  extern void *memmove (void *dest, const void *src, size_t n);
#endif

#if defined(HAVE_MEMMOVE) && defined(PREFER_PORTABLE_MEMMOVE)
extern void *portable_memmove (void *dest, const void *src, size_t n);
#define memmove portable_memmove
#endif

#endif
