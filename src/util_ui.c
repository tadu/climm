/* $Id$ */
/* Copyright: This file may be distributed under version 2 of the GPL licence. */

#include "micq.h"
#include <stdarg.h>
#include "util_ui.h"
#include "util_str.h"
#include "util_table.h"
#include "contact.h"
#include "preferences.h"

static const char *DebugStr (UDWORD level);

/*
 * Returns string describing debug level
 */
static const char *DebugStr (UDWORD level)
{
    if (level & DEB_PACKET)      return "Packet ";
    if (level & DEB_QUEUE)       return "Queue  ";
    if (level & DEB_PACK5DATA)   return "v5 data";
    if (level & DEB_PACK8)       return "v8 pack";
    if (level & DEB_PACK8DATA)   return "v8 data";
    if (level & DEB_PACK8SAVE)   return "v8 save";
    if (level & DEB_PACKTCP)     return "TCPpack";
    if (level & DEB_PACKTCPDATA) return "TCPdata";
    if (level & DEB_PACKTCPSAVE) return "TCPsave";
    if (level & DEB_PROTOCOL)    return "Protocl";
    if (level & DEB_TCP)         return "TCP HS ";
    if (level & DEB_IO)          return "  I/O  ";
    if (level & DEB_CONNECT)     return "Connect";
    return "unknown";
}

/*
 * Output a given string iff the debug level is appropriate.
 */
BOOL Debug (UDWORD level, const char *str, ...)
{
    va_list args;
    char buf[2048], c;

    if (!(prG->verbose & level) && level)
        return 0;

    va_start (args, str);
    vsnprintf (buf, sizeof (buf), str, args);
    va_end (args);

    M_print ("");
    if ((c = M_pos ()))
        M_print ("\n");
    M_printf ("%s %s%7.7s%s %s", s_now, COLDEBUG, DebugStr (level & prG->verbose), COLNONE, buf);

    if (!c)
        M_print ("\n");

    return 1;
}
