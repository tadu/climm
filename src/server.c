/* $Id$ */
/* Copyright: This file may be distributed under version 2 of the GPL licence. */

#include "micq.h"
#include "util_ui.h"
#include "datatype.h"
#include "mreadline.h"
#include "server.h"
#include "util.h"
#include "util_str.h"
#include "icq_response.h"
#include "cmd_pkt_v8_snac.h"
#include "cmd_pkt_cmd_v5.h"
#include "cmd_user.h"
#include "contact.h"
#include "session.h"
#include "preferences.h"
#include "tcp.h"
#include "conv.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <limits.h>

void Auto_Reply (Connection *conn, UDWORD uin)
{
    char *temp;

    if (!(prG->flags & FLAG_AUTOREPLY))
        return;
    
          if (conn->status & STATUSF_DND)
         temp = prG->auto_dnd;
     else if (conn->status & STATUSF_OCC)
         temp = prG->auto_occ;
     else if (conn->status & STATUSF_NA)
         temp = prG->auto_na;
     else if (conn->status & STATUSF_AWAY)
         temp = prG->auto_away;
     else if (conn->status & STATUSF_INV)
         temp = prG->auto_inv;
     else
         return;
    
    icq_sendmsg (conn, uin, temp, MSG_AUTO);
}

void icq_sendurl (Connection *conn, UDWORD uin, const char *description, const char *url)
{
    char *buf;

    icq_sendmsg (conn, uin, buf = strdup (s_sprintf ("%s%c%s", url, ConvSep (), description)), MSG_URL);
    free (buf);
}

void icq_sendmsg (Connection *conn, UDWORD uin, const char *text, UDWORD msg_type)
{
    char *old;

    putlog (conn, NOW, uin, STATUS_ONLINE, 
        msg_type == MSG_AUTO ? LOG_AUTO : LOG_SENT, msg_type, "%s\n", text);
#ifdef ENABLE_PEER2PEER
    if (!conn->assoc || !TCPSendMsg (conn->assoc, uin, text, msg_type))
#endif
    {
        if (~conn->connect & CONNECT_OK)
            return;
        if (conn->type == TYPE_SERVER)
            SnacCliSendmsg (conn, uin, text, msg_type, 0);
        else
            CmdPktCmdSendMessage (conn, uin, text, msg_type);
    }

    old = uiG.last_message_sent;
    uiG.last_message_sent      = strdup (text);
    uiG.last_message_sent_type = msg_type;
    if (old)
        free (old);
}
