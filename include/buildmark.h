/* $Id$
 *
 * climm version functions and ICQ client identifiers.
 */

#ifndef CLIMM_BUILDMARK_H
#define CLIMM_BUILDMARK_H

#define CLIMM_VERSION VERSION

#define BUILD_CLIMM    0xffffff42L
#define BUILD_MICQ_OLD 0x7d0001eaL

#define BUILD_PLATFORM_OTHER 0x01000000
#define BUILD_PLATFORM_WIN   0x02000000
#define BUILD_PLATFORM_BSD   0x03000000
#define BUILD_PLATFORM_LINUX 0x04000000
#define BUILD_PLATFORM_UNIX  0x05000000

#define BUILD_PLATFORM_AMIGAOS BUILD_PLATFORM_OTHER | 0x000001
#define BUILD_PLATFORM_BEOS    BUILD_PLATFORM_OTHER | 0x000002
#define BUILD_PLATFORM_QNX     BUILD_PLATFORM_OTHER | 0x000004
#define BUILD_PLATFORM_CYGWIN  BUILD_PLATFORM_WIN   | 0x000010
#define BUILD_PLATFORM_WINDOWS BUILD_PLATFORM_WIN   | 0x000020
#define BUILD_PLATFORM_OPENBSD BUILD_PLATFORM_BSD   | 0x000100
#define BUILD_PLATFORM_NETBSD  BUILD_PLATFORM_BSD   | 0x000200
#define BUILD_PLATFORM_FREEBSD BUILD_PLATFORM_BSD   | 0x000400
#define BUILD_PLATFORM_MACOSX  BUILD_PLATFORM_BSD   | 0x000800
#define BUILD_PLATFORM_AIX     BUILD_PLATFORM_UNIX  | 0x001000
#define BUILD_PLATFORM_HPUX    BUILD_PLATFORM_UNIX  | 0x002000
#define BUILD_PLATFORM_SOLARIS BUILD_PLATFORM_UNIX  | 0x004000
#define BUILD_PLATFORM_DEBIAN  BUILD_PLATFORM_LINUX | 0x010000

const        char  *BuildVersion (void);     /* used for ver command */
const        char  *BuildAttribution (void); /* welcome message */
extern const UDWORD BuildVersionNum;         /* e.g. 0x00050100, put into ICQ version time stamps */
extern const char  *BuildVersionStr;         /* e.g. "0.5.1" or "0.5.2 CVS 2007-04-06 12:01:02 UTC", put into XMPP version */
extern const UDWORD BuildPlatformID;         /* one of BUILD_PLATFORM_*, put into ICQ version time stamps */
extern const char  *BuildPlatformStr;        /* e.g. "Debian" or "AmigaOS", put into XMPP OS */
extern const char  *BuildVersionText;        /* e.g. "$VER: climm 0.5.2 SSL P2P Linux hand compiled" */

#endif /* CLIMM_BUILDMARK_H */
