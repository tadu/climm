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

UBYTE IMCliMsg (Connection *conn, Contact *cont, ContactOptions *opt)
{
    const char *opt_text;
    UDWORD opt_type, opt_trans;
    UBYTE ret;

    if (!cont || !conn || !ContactOptionsGetStr (opt, CO_MSGTEXT, &opt_text))
    {
        ContactOptionsD (opt);
        return RET_FAIL;
    }
    if (!ContactOptionsGetVal (opt, CO_MSGTYPE, &opt_type))
        ContactOptionsSetVal (opt, CO_MSGTYPE, opt_type = MSG_NORM);
    if (!ContactOptionsGetVal (opt, CO_MSGTRANS, &opt_trans))
        ContactOptionsSetVal (opt, CO_MSGTRANS, opt_trans = CV_MSGTRANS_ANY);

    putlog (conn, NOW, cont, STATUS_ONLINE, 
            opt_type == MSG_AUTO ? LOG_AUTO : LOG_SENT, opt_type, opt_text);

#ifdef ENABLE_PEER2PEER
    if (opt_trans & CV_MSGTRANS_DC)
        if (conn->assoc)
            if (RET_IS_OK (ret = PeerSendMsg (conn->assoc, cont, opt)))
                return ret;
    ContactOptionsSetVal (opt, CO_MSGTRANS, opt_trans &= ~CV_MSGTRANS_DC);
#endif
    if (opt_trans & CV_MSGTRANS_TYPE2)
        if (conn->connect & CONNECT_OK && conn->type == TYPE_SERVER)
            if (RET_IS_OK (ret = SnacCliSendmsg2 (conn, cont, opt)))
                return ret;
    ContactOptionsSetVal (opt, CO_MSGTRANS, opt_trans &= ~CV_MSGTRANS_TYPE2);
    if (opt_trans & CV_MSGTRANS_ICQv8)
        if (conn->connect & CONNECT_OK && conn->type == TYPE_SERVER)
            if (RET_IS_OK (ret = SnacCliSendmsg (conn, cont, opt_text, opt_type, 0)))
                return ret;
    ContactOptionsSetVal (opt, CO_MSGTRANS, opt_trans &= ~CV_MSGTRANS_ICQv8);
    if (opt_trans & CV_MSGTRANS_ICQv5)
        if (conn->connect & CONNECT_OK && conn->type == TYPE_SERVER_OLD)
        {
            CmdPktCmdSendMessage (conn, cont, opt_text, opt_type);
            ContactOptionsD (opt);
            return RET_OK;
        }
    ContactOptionsD (opt);
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
