/* $Id$ */
/* Copyright: This file may be distributed under version 2 of the GPL licence. */

#include "micq.h"
#include "util_ui.h"
#include "datatype.h"
#include "mreadline.h"
#include "server.h"
#include "util.h"
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

void Auto_Reply (Session *sess, UDWORD uin)
{
    char *temp;

    if (!(prG->flags & FLAG_AUTOREPLY))
        return;
    
          if (sess->status & STATUSF_DND)
         temp = prG->auto_dnd;
     else if (sess->status & STATUSF_OCC)
         temp = prG->auto_occ;
     else if (sess->status & STATUSF_NA)
         temp = prG->auto_na;
     else if (sess->status & STATUSF_AWAY)
         temp = prG->auto_away;
     else if (sess->status & STATUSF_INV)
         temp = prG->auto_inv;
     else
         return;
    
    icq_sendmsg (sess, uin, temp, AUTO_MESS);
}

void icq_sendurl (Session *sess, UDWORD uin, char *description, char *url)
{
    char buf[strlen (url) + strlen (description) + 2];

    snprintf (buf, sizeof (buf), "%s%c%s", url, ConvSep (), description);
    icq_sendmsg (sess, uin, buf, URL_MESS);
}

void icq_sendmsg (Session *sess, UDWORD uin, char *text, UDWORD msg_type)
{
    char *old;

    putlog (sess, NOW, uin, STATUS_ONLINE, 
        msg_type == AUTO_MESS ? LOG_AUTO : LOG_SENT, msg_type, "%s\n", text);
#ifdef TCP_COMM
    if (!sess->assoc || !TCPSendMsg (sess->assoc, uin, text, msg_type))
#endif
    {
        if (sess->type == TYPE_SERVER)
            SnacCliSendmsg (sess, uin, text, msg_type);
        else
            CmdPktCmdSendMessage (sess, uin, text, msg_type);
    }

    old = uiG.last_message_sent;
    uiG.last_message_sent      = strdup (text);
    uiG.last_message_sent_type = msg_type;
    if (old)
        free (old);
}
