/* $Id$ */

#include "micq.h"
#include "util_ui.h"
#include "datatype.h"
#include "mreadline.h"
#include "server.h"
#include "sendmsg.h"
#include "util.h"
#include "icq_response.h"
#include "cmd_user.h"
#include "contact.h"
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
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/socket.h>
#ifndef __BEOS__
#include <arpa/inet.h>
#endif
#include <netdb.h>
#include <sys/wait.h>
#include "mreadline.h"
#endif

/* TCP: auto response 
 * Returns the appropriate auto-reply string IF user has enabled auto-replys
 * dup's the string to prevent Russian conversion from happening on the
 * original */
char *Get_Auto_Reply ()
{
    char *nullmsg;
    nullmsg = malloc (1);
    *nullmsg = '\0';

    if (uiG.auto_resp == FALSE)
        return nullmsg;

    switch (uiG.Current_Status & 0x1FF)
    {
        case STATUS_OCCUPIED:
            return strdup (uiG.auto_rep_str_occ);
            break;

        case STATUS_AWAY:
            return strdup (uiG.auto_rep_str_away);
            break;

        case STATUS_DND:
            return strdup (uiG.auto_rep_str_dnd);
            break;

        case STATUS_NA:
            return strdup (uiG.auto_rep_str_na);
            break;

        case STATUS_INVISIBLE:
        case STATUS_ONLINE:
        case STATUS_FREE_CHAT:
        default:
            return nullmsg;
    }
}

void Auto_Reply (SOK_T sok, SIMPLE_MESSAGE_PTR s_mesg)
{
    char *temp;

/* If TCP communication is enabled, don't send auto-reply msgs
   since the client can request one if they want to see it.   */
#ifndef TCP_COMM
/*** TCP: handle auto response ***/
    if (uiG.auto_resp)
    { 
#endif
        /*** !!!!???? What does this if do?  Why is it here ????!!!!! ****/
    if (0xfe != *(((unsigned char *) s_mesg) + sizeof (s_mesg)))
    {
        if (uiG.auto_resp && (uiG.Current_Status != STATUS_ONLINE) && (uiG.Current_Status != STATUS_FREE_CHAT))
        {
            switch (uiG.Current_Status & 0x1ff)
            {
                case STATUS_OCCUPIED:
                    /* Dup the string so the russian translation only happens once */
                    temp = strdup (uiG.auto_rep_str_occ);
                    break;
                case STATUS_AWAY:
                    temp = strdup (uiG.auto_rep_str_away);
                    break;
                case STATUS_DND:
                    temp = strdup (uiG.auto_rep_str_dnd);
                    break;
                case STATUS_INVISIBLE:
                    temp = strdup (uiG.auto_rep_str_inv);
                    break;
                case STATUS_NA:
                    temp = strdup (uiG.auto_rep_str_na);
                    break;
                default:
                    temp = strdup (uiG.auto_rep_str_occ);
                    M_print (i18n (635, "You have encounterd a bug in my code :( I now own you a beer!\nGreetings Fryslan!\n"));
            }

            icq_sendmsg (sok, Chars_2_DW (s_mesg->uin), temp, NORM_MESS);
            free (temp);

            if (ContactFindNick (uiG.last_recv_uin) != NULL)
                log_event (uiG.last_recv_uin, LOG_AUTO_MESS,
                           "Sending an auto-reply message to %s\n", ContactFindNick (uiG.last_recv_uin));
            else
                log_event (uiG.last_recv_uin, LOG_AUTO_MESS,
                           "Sending an auto-reply message to %d\n", uiG.last_recv_uin);
        }
    }
#ifndef TCP_COMM
    }
#endif
}
