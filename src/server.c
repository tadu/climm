/* $Id$ */
/* Copyright? */

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

    switch (sess->status & 0x1FF)
    {
        case STATUS_OCCUPIED:
            return strdup (prG->auto_occ);
            break;

        case STATUS_AWAY:
            return strdup (prG->auto_away);
            break;

        case STATUS_DND:
            return strdup (prG->auto_dnd);
            break;

        case STATUS_NA:
            return strdup (prG->auto_na);
            break;

        case STATUSF_INVISIBLE:
        case STATUS_ONLINE:
        case STATUS_FREE_CHAT:
        default:
            return nullmsg;
    }
}

void Auto_Reply (Session *sess, UDWORD uin)
{
    char *temp;

    if (!(prG->flags & FLAG_AUTOREPLY))
        return;
    
    if (sess->status == STATUS_ONLINE || sess->status == STATUS_FREE_CHAT)
        return;

    switch (sess->status & 0x1ff)
    {
        case STATUS_OCCUPIED:
            temp = prG->auto_occ;
            break;
        case STATUS_AWAY:
            temp = prG->auto_away;
            break;
        case STATUS_DND:
            temp = prG->auto_dnd;
            break;
        case STATUSF_INVISIBLE:
            temp = prG->auto_inv;
            break;
        case STATUS_NA:
            temp = prG->auto_na;
            break;
        default:
            /* shouldn't happen */
            return;
    }

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
