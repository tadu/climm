/* $Id$ */
/* Copyright: This file may be distributed under version 2 of the GPL licence. */

#include "micq.h"
#include "util_ui.h"
#include "datatype.h"
#include "mreadline.h"
#include "server.h"
#include "util.h"
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

static void CallbackMeta (Event *event);

UBYTE IMCliMsg (Connection *conn, Contact *cont, Extra *extra)
{
    Extra *e_msg, *e_trans;
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
        if (conn->connect & CONNECT_OK && conn->type == TYPE_SERVER)
            if (RET_IS_OK (ret = SnacCliSendmsg2 (conn, cont, extra)))
                return ret;
    e_trans->data &= ~EXTRA_TRANS_TYPE2;
    if (e_trans->data & EXTRA_TRANS_ICQv8)
        if (conn->connect & CONNECT_OK && conn->type == TYPE_SERVER)
            if (RET_IS_OK (ret = SnacCliSendmsg (conn, cont, e_msg->text, e_msg->data, 0)))
                return ret;
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

void IMCliInfo (Connection *conn, Contact *cont, int group)
{
    UDWORD ref;
    if (cont)
    {
        cont->updated = 0;
        if (conn->type == TYPE_SERVER)
            ref = SnacCliMetareqinfo (conn, cont);
        else
            ref = CmdPktCmdMetaReqInfo (conn, cont);
    }
    else
    {
        if (conn->type == TYPE_SERVER)
            ref = SnacCliSearchrandom (conn, group);
        else
            ref = CmdPktCmdRandSearch (conn, group);
    }
    QueueEnqueueData (conn, QUEUE_REQUEST_META, ref, time (NULL) + 60, NULL,
                      cont, NULL, &CallbackMeta);
}

static void CallbackMeta (Event *event)
{
    Contact *cont;
    
    assert (event->conn && event->conn->type & TYPEF_ANY_SERVER);

    cont = event->cont;
    if ((cont->updated != UP_INFO) && !(event->flags & QUEUE_FLAG_CONSIDERED))
        QueueEnqueue (event);
    else
    {
        UtilUIDisplayMeta (cont);
        if (~cont->oldflags & CONT_ISEDITED)
            ContactMetaSave (cont);
        else
            cont->updated &= ~UPF_DISC;
        EventD (event);
    }
}
