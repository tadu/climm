
#include "config.h"

#if defined(_WIN32) || (defined(__CYGWIN__) && defined(_X86_))
#include "os_win32.c"
#endif

#if defined(HAVE_GETUTENT) || defined(HAVE_GETUTXENT)
#include "os_unix.c"
#endif

int dummy_to_make_file_nonempty;
