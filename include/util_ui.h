/* $Id$ */

#ifndef MICQ_UTIL_UI_H
#define MICQ_UTIL_UI_H

void UtilUIDisplayMeta (Contact *cont);
BOOL   Debug (UDWORD level, const char *str, ...) __attribute__ ((format (__printf__, 2, 3)));;

#define DEB_PROTOCOL      0x00000008L
#define DEB_PACKET        0x00000010L
#define DEB_QUEUE         0x00000020L
#define DEB_CONNECT       0x00000040L
#define DEB_EVENT         0x00000080L
#define DEB_EXTRA         0x00000100L
#define DEB_CONTACT       0x00000200L
#define DEB_PACK5DATA     0x00000800L
#define DEB_PACK8         0x00001000L
#define DEB_PACK8DATA     0x00002000L
#define DEB_PACK8SAVE     0x00004000L
#define DEB_PACKTCP       0x00010000L
#define DEB_PACKTCPDATA   0x00020000L
#define DEB_PACKTCPSAVE   0x00040000L
#define DEB_TCP           0x00200000L
#define DEB_IO            0x00400000L

#define AVPFMT COLSERVER "%-15s" COLNONE " %s\n"

#endif /* MICQ_UTIL_UI_H */
