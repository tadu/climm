
#include "memmove.h"

#if !defined(HAVE_MEMMOVE)
#define portable_memmove memmove
#endif

void *portable_memmove (void *dsta, const void *srca, size_t len)
{
    char *dst = dsta;
    const char *src = srca;

    if (dst > src && dst < src + len)
    {
        src += len;
        dst += len;
        while (len-- > 0)
            *--dst = *--src;
    }
    else
        while (len-- > 0)
            *dst++ = *src++;

    return dsta;
}
