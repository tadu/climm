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

/* TCP: auto response 
 * Returns the appropriate auto-reply string IF user has enabled auto-replys
 * dup's the string to prevent Russian conversion from happening on the
 * original */
char *Get_Auto_Reply (Session *sess)
{
    char *nullmsg;
    nullmsg = malloc (1);
    *nullmsg = '\0';

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

        case STATUS_INVISIBLE:
        case STATUS_ONLINE:
        case STATUS_FREE_CHAT:
        default:
            return nullmsg;
    }
}

void Auto_Reply (Session *sess, SIMPLE_MESSAGE_PTR s_mesg)
{
    char *temp;

/* If TCP communication is enabled, don't send auto-reply msgs
   since the client can request one if they want to see it.   */
#ifndef TCP_COMM
/*** TCP: handle auto response ***/
    if (prG->flags & FLAG_AUTOREPLY)
    { 
#endif
        /*** !!!!???? What does this if do?  Why is it here ????!!!!! ****/
    if (0xfe != *(((unsigned char *) s_mesg) + sizeof (s_mesg)))
    {
        if (prG->flags & FLAG_AUTOREPLY && (sess->status != STATUS_ONLINE) && (sess->status != STATUS_FREE_CHAT))
        {
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
                case STATUS_INVISIBLE:
                    temp = prG->auto_inv;
                    break;
                case STATUS_NA:
                    temp = prG->auto_na;
                    break;
                default:
                    temp = prG->auto_occ;
                    M_print (i18n (1635, "You have encounterd a bug in my code :( I now own you a beer!\nGreetings Fryslan!\n"));
            }

            icq_sendmsg (sess, Chars_2_DW (s_mesg->uin), temp, NORM_MESS);
            free (temp);

            if (ContactFindNick (uiG.last_rcvd_uin) != NULL)
                log_event (uiG.last_rcvd_uin, LOG_AUTO_MESS,
                           "Sending an auto-reply message to %s\n", ContactFindNick (uiG.last_rcvd_uin));
            else
                log_event (uiG.last_rcvd_uin, LOG_AUTO_MESS,
                           "Sending an auto-reply message to %d\n", uiG.last_rcvd_uin);
        }
    }
#ifndef TCP_COMM
    }
#endif
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
