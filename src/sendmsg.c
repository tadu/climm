/* $Id$ */

/*
Send Message Function for ICQ... 
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Author : zed@mentasm.com
*/
#include <stdlib.h>
#include <stdio.h>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/socket.h>
#endif
#include <string.h>
#include <assert.h>
#include <limits.h>
#include "micq.h"
#include "util_ui.h"
#include "sendmsg.h"
#include "cmd_pkt_cmd_v5.h"
#include "util.h"
#include "contact.h"
#include "tcp.h"

void icq_sendurl (Session *sess, UDWORD uin, char *description, char *url)
{
    char buf[450];

    sprintf (buf, "%s\xFE%s", url, description);
    icq_sendmsg (sess, uin, buf, URL_MESS);
}

void icq_sendmsg (Session *sess, UDWORD uin, char *text, UDWORD msg_type)
{	
#ifdef TCP_COMM
    if (!TCPSendMsg (sess, uin, text, msg_type))
#endif
    CmdPktCmdSendMessage (sess, uin, text, msg_type);
}

size_t SOCKREAD (Session *sess, void *ptr, size_t len)
{
    size_t sz;

    sz = sockread (sess->sok, ptr, len);
    sess->Packets_Recv++;

/* SOCKS5 stuff begin */
    if (s5G.s5Use)
    {
        sz -= 10;
        memcpy (ptr, ptr + 10, sz);
    }
/* SOCKS5 stuff end */

    return sz;
}
