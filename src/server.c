/* $Id$ */
/* Copyright: This file may be distributed under version 2 of the GPL licence. */

#include "micq.h"
#include "util_ui.h"
#include "datatype.h"
#include "mreadline.h"
#include "server.h"
#include "util.h"
#include "util_str.h"
#include "util_extra.h"
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
    Contact *cont = ContactUIN (conn, uin);
    
    if (!cont)
        return;

    putlog (conn, NOW, cont, STATUS_ONLINE, 
        msg_type == MSG_AUTO ? LOG_AUTO : LOG_SENT, msg_type, text);
#ifdef ENABLE_PEER2PEER
    if (!conn->assoc || !TCPSendMsg (conn->assoc, cont, text, msg_type))
#endif
    {
        if (~conn->connect & CONNECT_OK)
            return;
        if (conn->type == TYPE_SERVER)
            SnacCliSendmsg (conn, cont, text, msg_type, 0);
        else
            CmdPktCmdSendMessage (conn, cont, text, msg_type);
    }

    old = uiG.last_message_sent;
    uiG.last_message_sent      = strdup (text);
    uiG.last_message_sent_type = msg_type;
    if (old)
        free (old);
}

UBYTE IMCliMsg (Connection *conn, Contact *cont, Extra *extra)
{
    Extra *e_msg, *e_trans;
    char *old;
    UBYTE ret;

    if (!cont || !conn)
    {
        ExtraD (extra);
        return RET_FAIL;
    }
    
    if (!(e_msg = ExtraFind (extra, EXTRA_MESSAGE)))
    {
        ExtraD (extra);
        return RET_FAIL;
    }
    if (!(e_trans = ExtraFind (extra, EXTRA_TRANS)))
    {
        extra = ExtraSet (extra, EXTRA_TRANS, EXTRA_TRANS_ANY, NULL);
        e_trans = ExtraFind (extra, EXTRA_TRANS);
    }

    old = uiG.last_message_sent;
    uiG.last_message_sent      = strdup (e_msg->text);
    uiG.last_message_sent_type = e_msg->data;
    uiG.last_sent_uin          = cont->uin;
    if (old)
        free (old);

    putlog (conn, NOW, cont, STATUS_ONLINE, 
            e_msg->data == MSG_AUTO ? LOG_AUTO : LOG_SENT, e_msg->data, e_msg->text);

#ifdef ENABLE_PEER2PEER
    if (e_trans->data & EXTRA_TRANS_DC)
        if (conn->assoc)
            if (RET_IS_OK (ret = PeerSendMsg (conn->assoc, cont, extra)))
                return ret;
    e_trans->data &= ~EXTRA_TRANS_DC;
#endif
    if (e_trans->data & EXTRA_TRANS_TYPE2)
        if (conn->type == TYPE_SERVER && HAS_CAP (cont->caps, CAP_SRVRELAY))
            if (RET_IS_OK (ret = SnacCliSendmsg2 (conn, cont, extra)))
                return ret;
    e_trans->data &= ~EXTRA_TRANS_TYPE2;
    if (e_trans->data & EXTRA_TRANS_ICQv8)
        if (conn->connect & CONNECT_OK && conn->type == TYPE_SERVER)
        {
            SnacCliSendmsg (conn, cont, e_msg->text, e_msg->data, 0);
            ExtraD (extra);
            return RET_OK;
        }
    e_trans->data &= ~EXTRA_TRANS_ICQv8;
    if (e_trans->data & EXTRA_TRANS_ICQv5)
        if (conn->connect & CONNECT_OK && conn->type == TYPE_SERVER_OLD)
        {
            CmdPktCmdSendMessage (conn, cont, e_msg->text, e_msg->data);
            ExtraD (extra);
            return RET_OK;
        }
    ExtraD (extra);
    return RET_FAIL;
}
