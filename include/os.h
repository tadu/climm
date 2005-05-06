#ifndef MICQ_OS_H
#define MICQ_OS_H

#if defined(_WIN32) || (defined(__CYGWIN__) && defined(_X86_))

/*
 * should work on Windows 98/ME
 * works on Windows NT, if screen saver is password protected
 * works on Windows 2000 and later
 *
 * detects a running screen saver
 * detects a locked workstation on Windows NT
 *
 * result codes:
 *   <0     error; cannot detect anything; wrong OS version
 *   >=0 see following, set/active, if bit is set (==1)
 *       bit  0:  screen saver active
 *       bit  1:  NT workstation locked
 *       bit 16:  init mode active (please ignore this)
 */
int os_DetectLockedWorkstation();
const char *os_packagedatadir (void);
const char *os_packagehomedir (void);

#else /* !_WIN32 && (!__CYGWIN__ || !_X86) */

#define os_DetectLockedWorkstation() -1
#define os_packagedatadir() PKGDATADIR


#endif /* !_WIN32 && (!__CYGWIN__ || !_X86) */

#endif /* MICQ_OS_H */
