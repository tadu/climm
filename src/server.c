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

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#include <winsock2.h>
#else
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#ifndef __BEOS__
#include <arpa/inet.h>
#endif
#include <sys/wait.h>
#include "mreadline.h"
#endif

/* 
 * Returns auto response string.
 * Result must be free()ed.
 */
char *Get_Auto_Reply (Session *sess)
{
    char *nullmsg = strdup ("");
    ASSERT_SERVER (sess);

    if (!(prG->flags & FLAG_AUTOREPLY))
        return nullmsg;

    if (sess->status & STATUSF_DND)
        return strdup (prG->auto_dnd);
    if (sess->status & STATUSF_OCC)
        return strdup (prG->auto_occ);
    if (sess->status & STATUSF_NA)
        return strdup (prG->auto_na);
    if (sess->status & STATUSF_AWAY)
        return strdup (prG->auto_away);

    return nullmsg;
}

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
    
    icq_sendmsg (sess, uin, temp, NORM_MESS);

    if (ContactFindNick (uiG.last_rcvd_uin) != NULL)
        log_event (uin, LOG_AUTO_MESS,
                   "Sending an auto-reply message to %s\n", ContactFindNick (uin));
    else
        log_event (uin, LOG_AUTO_MESS,
                   "Sending an auto-reply message to %d\n", uin);
}

void icq_sendurl (Session *sess, UDWORD uin, char *description, char *url)
{
    char buf[450];

    sprintf (buf, "%s%c%s", url, ConvSep (), description);
    icq_sendmsg (sess, uin, buf, URL_MESS);
}

void icq_sendmsg (Session *sess, UDWORD uin, char *text, UDWORD msg_type)
{
    log_event (uin, LOG_MESS, "You sent instant message to %s\n%s\n", ContactFindName (uin), text);
#ifdef TCP_COMM
    if (!sess->assoc || !TCPSendMsg (sess->assoc, uin, text, msg_type))
#endif
    {
        if (sess->type == TYPE_SERVER)
            SnacCliSendmsg (sess, uin, text, msg_type);
        else
            CmdPktCmdSendMessage (sess, uin, text, msg_type);
    }
}
