/* $Id$ */
/* Copyright: This file may be distributed under version 2 of the GPL licence. */

#include "micq.h"
#include <fcntl.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>
#include <limits.h>
#include "util_ui.h"
#include "datatype.h"
#include "server.h"
#include "util.h"
#include "im_response.h"
#include "oldicq_compat.h"
#include "oldicq_client.h"
#include "oscar_snac.h"
#include "oscar_icbm.h"
#include "oscar_oldicq.h"
#include "oscar_bos.h"
#include "cmd_user.h"
#include "contact.h"
#include "connection.h"
#include "preferences.h"
#include "tcp.h"
#include "packet.h"
#include "conv.h"
#include "oscar_service.h"
#include "oscar_roster.h"
#include "jabber_base.h"

static void CallbackMeta (Event *event);
static void CallbackTogvis (Event *event);

static void CallbackTogvis (Event *event)
{
    if (!event)
        return;
    if (!event->cont || !event->conn || ContactPrefVal (event->cont, CO_INTIMATE)
        || !ContactIsInv (event->conn->status))
    {
        EventD (event);
        return;
    }
    SnacCliRemvisible (event->conn, event->cont);
    EventD (event);
}

UBYTE IMCliMsg (Connection *conn, Contact *cont, Opt *opt)
{
    const char *opt_text;
    UDWORD opt_type, opt_trans;
    
    if (!cont || !conn || !OptGetStr (opt, CO_MSGTEXT, &opt_text))
    {
        OptD (opt);
        return RET_FAIL;
    }
    if (!OptGetVal (opt, CO_MSGTYPE, &opt_type))
        OptSetVal (opt, CO_MSGTYPE, opt_type = MSG_NORM);
    if (!OptGetVal (opt, CO_MSGTRANS, &opt_trans))
        OptSetVal (opt, CO_MSGTRANS, opt_trans = CV_MSGTRANS_ANY);
    
    putlog (conn, NOW, cont, ims_online, 0,
            opt_type == MSG_AUTO ? LOG_AUTO : LOG_SENT, opt_type, opt_text);

    return IMCliReMsg (conn, cont, opt);
}

UBYTE IMCliReMsg (Connection *conn, Contact *cont, Opt *opt)
{
    const char *opt_text;
    Event *event;
    UDWORD opt_type, opt_trans, reveal_time;
    UBYTE ret;
    
    if (!cont || !conn || !OptGetStr (opt, CO_MSGTEXT, &opt_text))
    {
        OptD (opt);
        return RET_FAIL;
    }
    if (!OptGetVal (opt, CO_MSGTYPE, &opt_type))
        OptSetVal (opt, CO_MSGTYPE, opt_type = MSG_NORM);
    if (!OptGetVal (opt, CO_MSGTRANS, &opt_trans))
        OptSetVal (opt, CO_MSGTRANS, opt_trans = CV_MSGTRANS_ANY);
    
    if (conn->type == TYPE_SERVER
        && ContactIsInv (conn->status)  && !ContactPrefVal (cont, CO_INTIMATE)
        && !(opt_type & MSGF_GETAUTO) && !ContactPrefVal (cont, CO_HIDEFROM)
        && (reveal_time = ContactPrefVal (cont, CO_REVEALTIME)))
    {
        event = QueueDequeue2 (conn, QUEUE_TOGVIS, 0, cont);
        if (event)
            EventD (event);
        else
            SnacCliAddvisible (conn, cont);
        QueueEnqueueData (conn, QUEUE_TOGVIS, 0, time (NULL) + reveal_time, NULL, cont, NULL, CallbackTogvis);
    }

#ifdef ENABLE_PEER2PEER
    if (opt_trans & CV_MSGTRANS_DC)
        if (conn->assoc)
            if (RET_IS_OK (ret = PeerSendMsg (conn->assoc, cont, opt)))
                return ret;
    OptSetVal (opt, CO_MSGTRANS, opt_trans &= ~CV_MSGTRANS_DC);
#endif
    if (opt_trans & CV_MSGTRANS_TYPE2)
        if (conn->connect & CONNECT_OK && conn->type == TYPE_SERVER)
            if (RET_IS_OK (ret = SnacCliSendmsg2 (conn, cont, opt)))
                return ret;
    OptSetVal (opt, CO_MSGTRANS, opt_trans &= ~CV_MSGTRANS_TYPE2);
    if (opt_trans & CV_MSGTRANS_ICQv8)
        if (conn->connect & CONNECT_OK && conn->type == TYPE_SERVER)
            if (RET_IS_OK (ret = SnacCliSendmsg (conn, cont, opt_text, opt_type, 0)))
            {
                OptD (opt);
                return ret;
            }
    OptSetVal (opt, CO_MSGTRANS, opt_trans &= ~CV_MSGTRANS_ICQv8);
    if (opt_trans & CV_MSGTRANS_ICQv5)
        if (conn->connect & CONNECT_OK && conn->type == TYPE_SERVER_OLD)
        {
            CmdPktCmdSendMessage (conn, cont, opt_text, opt_type);
            OptD (opt);
            return RET_OK;
        }
#if ENABLE_XMPP
    if (opt_trans & CV_MSGTRANS_XMPP)
        if (conn->connect & CONNECT_OK && conn->type == TYPE_XMPP_SERVER)
            if (RET_IS_OK (ret = XMPPSendmsg (conn, cont, opt_text, opt_type)))
                return ret;
#endif
    ret = RET_OK;
    if (conn->type == TYPE_SERVER && opt_type == MSG_AUTH_DENY)
        SnacCliAuthorize (conn, cont, 0, opt_text);
    else if (conn->type == TYPE_SERVER && opt_type == MSG_AUTH_REQ)
        SnacCliReqauth (conn, cont, opt_text);
    else if (conn->type == TYPE_SERVER && opt_type == MSG_AUTH_ADDED)
        SnacCliGrantauth (conn, cont);
    else if (conn->type == TYPE_SERVER && opt_type == MSG_AUTH_GRANT)
        SnacCliAuthorize (conn, cont, 1, NULL);
    else
        ret = RET_INPR;
    OptD (opt);
    return ret;
}

void IMCliInfo (Connection *conn, Contact *cont, int group)
{
    UDWORD ref = 0;
    if (cont)
    {
        cont->updated = 0;
        if (conn->type == TYPE_SERVER)
            ref = SnacCliMetareqinfo (conn, cont);
        else if (conn->type == TYPE_SERVER_OLD)
            ref = CmdPktCmdMetaReqInfo (conn, cont);
    }
    else
    {
        if (conn->type == TYPE_SERVER)
            ref = SnacCliSearchrandom (conn, group);
        else if (conn->type == TYPE_SERVER_OLD)
            ref = CmdPktCmdRandSearch (conn, group);
    }
    if (ref)
        QueueEnqueueData (conn, QUEUE_REQUEST_META, ref, time (NULL) + 60, NULL,
                          cont, NULL, &CallbackMeta);
}

static void CallbackMeta (Event *event)
{
    Contact *cont;
    
    assert (event && event->conn && event->conn->type & TYPEF_ANY_SERVER);

    cont = event->cont;
    if (!cont)
        EventD (event);
    else if ((cont->updated != UP_INFO) && !(event->flags & QUEUE_FLAG_CONSIDERED))
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

void IMSetStatus (Connection *conn, Contact *cont, status_t status, const char *msg)
{
    if (~conn->connect & CONNECT_OK)
    {
        conn->status = status;
        conn->nativestatus = 0;
    }
    else if (conn->type == TYPE_SERVER)
        SnacCliSetstatus (conn, status, 1);
    else if (conn->type == TYPE_SERVER_OLD)
    {
        CmdPktCmdStatusChange (conn, status);
        rl_printf ("%s %s\n", s_now, s_status (conn->status, conn->nativestatus));
    }
#if ENABLE_XMPP
    else if (conn->type == TYPE_XMPP_SERVER)
        XMPPSetstatus (conn, cont, status, msg);
#endif
}

