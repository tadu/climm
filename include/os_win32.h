#ifndef MICQ_OSWIN32_H
#define MICQ_OSWIN32_H

#if defined(_WIN32) || defined(__CYGWIN__)

/*
 * should work on Windows 98/ME
 * works on Windows NT and later
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

#endif /* if defined(_WIN32) || defined(__CYGWIN__) */

#endif /* MICQ_OSWIN32_H */
