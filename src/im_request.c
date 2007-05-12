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
#include "im_request.h"
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
#include "util_otr.h"

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

UBYTE IMCliMsg (Contact *cont, Opt *opt)
{
    const char *opt_text;
    UDWORD opt_type, opt_trans;
#ifdef ENABLE_OTR
    char *otr_text = NULL;
    UDWORD injected;
#endif
    
    if (!cont || !cont->serv || !OptGetStr (opt, CO_MSGTEXT, &opt_text))
    {
        OptD (opt);
        return RET_FAIL;
    }
    if (!OptGetVal (opt, CO_MSGTYPE, &opt_type))
        OptSetVal (opt, CO_MSGTYPE, opt_type = MSG_NORM);
    if (!OptGetVal (opt, CO_MSGTRANS, &opt_trans))
        OptSetVal (opt, CO_MSGTRANS, opt_trans = CV_MSGTRANS_ANY);

#ifdef ENABLE_OTR
    if (!OptGetVal (opt, CO_OTRINJECT, &injected))
        injected = FALSE;
    /* process outgoing messages for OTR */
    if (opt_type == MSG_NORM && !injected && libotr_is_present)
    {
        if (OTRMsgOut (opt_text, cont->serv, cont, &otr_text))
        {
            rl_print (COLERROR);
            rl_printf (i18n (2641, "Message for %s could not be encrypted and was NOT sent!"),
                    cont->nick);
            rl_printf ("%s\n", COLNONE);
            OptD (opt);
            if (otr_text)
                OTRFree (otr_text);
            return RET_FAIL;
        }
    }
    /* log original message */
#endif
    
    putlog (cont->serv, NOW, cont, ims_online, 0,
            opt_type == MSG_AUTO ? LOG_AUTO : LOG_SENT, opt_type, opt_text);

#ifdef ENABLE_OTR
    /* replace text if OTR changed it */
    if (otr_text)
    {
        OptSetStr (opt, CO_MSGTEXT, otr_text);
        OTRFree (otr_text);
    }
#endif

    return IMCliReMsg (cont, opt);
}

UBYTE IMCliReMsg (Contact *cont, Opt *opt)
{
    const char *opt_text;
    Event *event;
    UDWORD opt_type, opt_trans, reveal_time;
    UBYTE ret;
    
    if (!cont || !cont->serv || !OptGetStr (opt, CO_MSGTEXT, &opt_text))
    {
        OptD (opt);
        return RET_FAIL;
    }
    if (!OptGetVal (opt, CO_MSGTYPE, &opt_type))
        OptSetVal (opt, CO_MSGTYPE, opt_type = MSG_NORM);
    if (!OptGetVal (opt, CO_MSGTRANS, &opt_trans))
        OptSetVal (opt, CO_MSGTRANS, opt_trans = CV_MSGTRANS_ANY);
    
    if (cont->serv->type == TYPE_SERVER
        && ContactIsInv (cont->serv->status)  && !ContactPrefVal (cont, CO_INTIMATE)
        && !(opt_type & MSGF_GETAUTO) && !ContactPrefVal (cont, CO_HIDEFROM)
        && (reveal_time = ContactPrefVal (cont, CO_REVEALTIME)))
    {
        event = QueueDequeue2 (cont->serv, QUEUE_TOGVIS, 0, cont);
        if (event)
            EventD (event);
        else
            SnacCliAddvisible (cont->serv, cont);
        QueueEnqueueData (cont->serv, QUEUE_TOGVIS, 0, time (NULL) + reveal_time, NULL, cont, NULL, CallbackTogvis);
    }

#ifdef ENABLE_PEER2PEER
    if (opt_trans & CV_MSGTRANS_DC)
        if (cont->serv->assoc)
            if (RET_IS_OK (ret = PeerSendMsg (cont->serv->assoc, cont, opt)))
                return ret;
    OptSetVal (opt, CO_MSGTRANS, opt_trans &= ~CV_MSGTRANS_DC);
#endif
    if (opt_trans & CV_MSGTRANS_TYPE2)
        if (cont->serv->connect & CONNECT_OK && cont->serv->type == TYPE_SERVER)
            if (RET_IS_OK (ret = SnacCliSendmsg2 (cont->serv, cont, opt)))
                return ret;
    OptSetVal (opt, CO_MSGTRANS, opt_trans &= ~CV_MSGTRANS_TYPE2);
    if (opt_trans & CV_MSGTRANS_ICQv8)
        if (cont->serv->connect & CONNECT_OK && cont->serv->type == TYPE_SERVER)
            if (RET_IS_OK (ret = SnacCliSendmsg (cont->serv, cont, opt_text, opt_type, 0)))
            {
                OptD (opt);
                return ret;
            }
    OptSetVal (opt, CO_MSGTRANS, opt_trans &= ~CV_MSGTRANS_ICQv8);
    if (opt_trans & CV_MSGTRANS_ICQv5)
        if (cont->serv->connect & CONNECT_OK && cont->serv->type == TYPE_SERVER_OLD)
        {
            CmdPktCmdSendMessage (cont->serv, cont, opt_text, opt_type);
            OptD (opt);
            return RET_OK;
        }
#if ENABLE_XMPP
    if (opt_trans & CV_MSGTRANS_XMPP)
        if (cont->serv->connect & CONNECT_OK && cont->serv->type == TYPE_XMPP_SERVER)
            if (RET_IS_OK (ret = XMPPSendmsg (cont->serv, cont, opt_type, opt_text)))
                return ret;
#endif
    ret = RET_OK;
    if (cont->serv->type == TYPE_SERVER && opt_type == MSG_AUTH_DENY)
        SnacCliAuthorize (cont->serv, cont, 0, opt_text);
    else if (cont->serv->type == TYPE_SERVER && opt_type == MSG_AUTH_REQ)
        SnacCliReqauth (cont->serv, cont, opt_text);
    else if (cont->serv->type == TYPE_SERVER && opt_type == MSG_AUTH_ADDED)
        SnacCliGrantauth (cont->serv, cont);
    else if (cont->serv->type == TYPE_SERVER && opt_type == MSG_AUTH_GRANT)
        SnacCliAuthorize (cont->serv, cont, 1, NULL);
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
        if (cont->serv->type == TYPE_SERVER)
            ref = SnacCliMetareqinfo (cont->serv, cont);
        else if (cont->serv->type == TYPE_SERVER_OLD)
            ref = CmdPktCmdMetaReqInfo (cont->serv, cont);
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

void IMSetStatus (Connection *serv, Contact *cont, status_t status, const char *msg)
{
    assert (serv || cont);
    if (cont)
    {
#if ENABLE_XMPP
        if (cont->serv->type == TYPE_XMPP_SERVER)
            XMPPSetstatus (cont->serv, cont, status, msg);
#endif
    }
    else
    {
        if (~serv->connect & CONNECT_OK)
        {
            serv->status = status;
            serv->nativestatus = 0;
        }
        else if (serv->type == TYPE_SERVER && !cont)
            SnacCliSetstatus (serv, status, 1);
        else if (serv->type == TYPE_SERVER_OLD && !cont)
        {
            CmdPktCmdStatusChange (serv, status);
            rl_printf ("%s %s\n", s_now, s_status (serv->status, serv->nativestatus));
        }
#if ENABLE_XMPP
        else if (serv->type == TYPE_XMPP_SERVER)
            XMPPSetstatus (serv, NULL, status, msg);
#endif
    }
}

void IMCliAuth (Contact *cont, const char *msg, auth_t how)
{
    assert (cont);
    assert (cont->serv);
    if (~cont->serv->connect & CONNECT_OK)
        return;

    if (cont->serv->type == TYPE_SERVER || cont->serv->type == TYPE_SERVER_OLD)
    {
        UDWORD msgtype;
        switch (how)
        {
            case auth_deny:
                if (!msg)         /* FIXME: let it untranslated? */
                    msg = "Authorization refused\n";
                msgtype = MSG_AUTH_DENY;
                break;
            case auth_req:
                if (!msg)         /* FIXME: let it untranslated? */
                    msg = "Please authorize my request and add me to your Contact List\n";
                msgtype = MSG_AUTH_REQ;
                break;
            case auth_add:
                if (!msg)
                    msg = "\x03";
                msgtype = MSG_AUTH_ADDED;
                break;
            default:
            case auth_grant:
                if (!msg)
                    msg = "\x03";
                msgtype = MSG_AUTH_GRANT;
        }
        IMCliMsg (cont, OptSetVals (NULL, CO_MSGTYPE, msgtype, CO_MSGTEXT, msg, 0));
    }
#if ENABLE_XMPP
    else if (cont->serv->type == TYPE_XMPP_SERVER)
        XMPPAuthorize (cont->serv, cont, how, msg);
#endif
}
