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
#include "packet.h"
#include "conv.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <limits.h>

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

UBYTE IMCliMsg (Connection *conn, Contact *cont, const char *text, UDWORD msg_type, MetaList *extra)
{
    char *old;
    UBYTE ret;

    if (!cont)
    {
        Meta_Free (extra);
        return RET_FAIL;
    }
    
    if (!extra || extra->tag != EXTRA_TRANS)
    {
        MetaList *tmp = calloc (1, sizeof (MetaList));
        tmp->tag  = EXTRA_TRANS;
        tmp->data = EXTRA_TRANS_ANY;
        tmp->more = extra;
        extra = tmp;
    }

    old = uiG.last_message_sent;
    uiG.last_message_sent      = strdup (text);
    uiG.last_message_sent_type = msg_type;
    text = uiG.last_message_sent;
    if (old)
        free (old);

    putlog (conn, NOW, cont->uin, STATUS_ONLINE, 
            msg_type == MSG_AUTO ? LOG_AUTO : LOG_SENT, msg_type, "%s\n", text);

#ifdef ENABLE_PEER2PEER
    if (extra->data & EXTRA_TRANS_DC)
        if (conn->assoc)
            if (RET_IS_OK (ret = PeerSendMsg (conn->assoc, cont, text, msg_type, extra)))
                return ret;
    extra->data &= ~EXTRA_TRANS_DC;
#endif
    if (extra->data & EXTRA_TRANS_TYPE2)
        if (conn->type == TYPE_SERVER && HAS_CAP (cont->caps, CAP_ISICQ) && HAS_CAP (cont->caps, CAP_SRVRELAY))
            if (RET_IS_OK (ret = SnacCliSendmsg2 (conn, cont, text, msg_type, extra)))
                return ret;
    extra->data &= ~EXTRA_TRANS_TYPE2;
    if (extra->data & EXTRA_TRANS_ICQv8)
        if (conn->connect & CONNECT_OK && conn->type == TYPE_SERVER)
        {
            SnacCliSendmsg (conn, cont->uin, text, msg_type, 0);
            Meta_Free (extra);
            return RET_OK;
        }
    extra->data &= ~EXTRA_TRANS_ICQv8;
    if (extra->data & EXTRA_TRANS_ICQv5)
        if (conn->connect & CONNECT_OK && conn->type == TYPE_SERVER_OLD)
        {
            CmdPktCmdSendMessage (conn, cont->uin, text, msg_type);
            Meta_Free (extra);
            return RET_OK;
        }
    Meta_Free (extra);
    return RET_FAIL;
}
