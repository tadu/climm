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

Message *MsgC ()
{
    Message *msg;
    
    msg = calloc (1, sizeof *msg);
    DebugH (DEB_MSG, "M<" STR_DOT STR_DOT STR_DOT " %p:", msg);
    return msg;
}

void MsgD (Message *msg)
{
    char *s, *t;
    if (!msg)
        return;
    DebugH (DEB_MSG, "M>" STR_DOT STR_DOT STR_DOT " %p [%s] %lu %lu %lu %u %u (%s) (%s) ",
           msg, msg->cont ? msg->cont->screen : "",
           msg->type, msg->origin, msg->trans, msg->otrinjected, msg->force,
           msg->send_message ? msg->send_message : "(null)",
           msg->plain_message ? msg->plain_message : "(null)");
                                    
    s = msg->send_message;
    t = msg->plain_message;
    msg->send_message = NULL;
    msg->plain_message = NULL;
    free (msg);
    s_free (s);
    s_free (t);
}

Message *cb_msg_log (Message *msg)
{
    putlog (msg->cont->serv, NOW, msg->cont, ims_online, 0,
            msg->type == MSG_AUTO ? LOG_AUTO : LOG_SENT, msg->type,
            msg->plain_message ? msg->plain_message : msg->send_message);
    return msg;
}

#ifdef ENABLE_OTR
Message *cb_msg_otr (Message *msg)
{
    char *otr_text = NULL;

    /* process outgoing messages for OTR */
    if (msg->type != MSG_NORM || msg->otrinjected || !libotr_is_present)
        return msg;

    if (OTRMsgOut (msg->send_message, msg->cont->serv, msg->cont, &otr_text))
    {
        rl_print (COLERROR);
        rl_printf (i18n (2641, "Message for %s could not be encrypted and was NOT sent!"),
                msg->cont->nick);
        rl_printf ("%s\n", COLNONE);
        MsgD (msg);
        if (otr_text)
            OTRFree (otr_text);
        return NULL;
    }
    /* replace text if OTR changed it */
    if (otr_text)
    {
        assert (!msg->plain_message);
        msg->plain_message = msg->send_message;
        msg->send_message = strdup (otr_text);
        OTRFree (otr_text);
    }
    return msg;
}
#endif

Message *cb_msg_revealinv (Message *msg)
{
    UDWORD reveal_time;
    Event *event;

    if (msg->cont->serv->type != TYPE_SERVER)
        return msg;
    if (msg->type & MSGF_GETAUTO)
        return msg;
    if (!ContactIsInv (msg->cont->serv->status))
        return msg;
    if (ContactPrefVal (msg->cont, CO_INTIMATE) || ContactPrefVal (msg->cont, CO_HIDEFROM))
        return msg;
    if (!(reveal_time = ContactPrefVal (msg->cont, CO_REVEALTIME)))
        return msg;

    event = QueueDequeue2 (msg->cont->serv, QUEUE_TOGVIS, 0, msg->cont);
    if (event)
        EventD (event);
    else
        SnacCliAddvisible (msg->cont->serv, msg->cont);
    QueueEnqueueData (msg->cont->serv, QUEUE_TOGVIS, 0, time (NULL) + reveal_time, NULL, msg->cont, NULL, CallbackTogvis);
    return msg;
}


UBYTE IMCliMsg (Contact *cont, UDWORD type, const char *text, Opt *opt)
{
    Message *msg;
    UDWORD tmp;
    
    if (!cont || !cont->serv)
    {
        OptD (opt);
        return RET_FAIL;
    }
    msg = MsgC ();
    if (!msg)
        return RET_FAIL;
    msg->cont = cont;
    msg->type = type;
    msg->send_message = strdup (text);
    if (!opt || !OptGetVal (opt, CO_MSGTRANS, &msg->trans))
        msg->trans = CV_MSGTRANS_ANY;
    if (opt && OptGetVal (opt, CO_FORCE, &tmp))
        msg->force = tmp;
    if (opt && OptGetVal (opt, CO_OTRINJECT, &tmp))
        msg->otrinjected = tmp;

#ifdef ENABLE_OTR
             msg = cb_msg_otr (msg);
#endif
    if (msg) msg = cb_msg_log (msg);
    if (msg)
        return IMCliReMsg (cont, msg);
    return RET_FAIL;
}

UBYTE IMCliReMsg (Contact *cont, Message *msg)
{
    UBYTE ret;

    assert (cont);
    assert (msg);
    assert (msg->cont == cont);    
    assert (msg->send_message);
    
    msg = cb_msg_revealinv (msg);
    if (!msg)
        return RET_FAIL;
    
#ifdef ENABLE_PEER2PEER
    if (msg->trans & CV_MSGTRANS_DC)
        if (cont->serv->assoc)
            if (RET_IS_OK (ret = PeerSendMsgFat (cont->serv->assoc, cont, msg)))
                return ret;
    msg->trans &= ~CV_MSGTRANS_DC;
#endif
    if (msg->trans & CV_MSGTRANS_TYPE2)
        if (cont->serv->connect & CONNECT_OK && cont->serv->type == TYPE_SERVER)
            if (RET_IS_OK (ret = SnacCliSendmsg (cont->serv, cont, 2, msg)))
                return ret;
    msg->trans &= ~CV_MSGTRANS_TYPE2;
    if (msg->trans & CV_MSGTRANS_ICQv8)
        if (cont->serv->connect & CONNECT_OK && cont->serv->type == TYPE_SERVER)
        {
            if (RET_IS_OK (ret = SnacCliSendmsg (cont->serv, cont, 1, msg)))
                return ret;
            if (RET_IS_OK (ret = SnacCliSendmsg (cont->serv, cont, 4, msg)))
                return ret;
        }
    msg->trans &= ~CV_MSGTRANS_ICQv8;
    if (msg->trans & CV_MSGTRANS_ICQv5)
        if (cont->serv->connect & CONNECT_OK && cont->serv->type == TYPE_SERVER_OLD)
        {
            CmdPktCmdSendMessage (cont->serv, cont, msg->send_message, msg->type);
            MsgD (msg);
            return RET_OK;
        }
#if ENABLE_XMPP
    if (msg->trans & CV_MSGTRANS_XMPP)
        if (cont->serv->connect & CONNECT_OK && cont->serv->type == TYPE_XMPP_SERVER)
            if (RET_IS_OK (ret = XMPPSendmsg (cont->serv, cont, msg)));
                return ret;
#endif
    ret = RET_OK;
    if (cont->serv->type == TYPE_SERVER && msg->type == MSG_AUTH_DENY)
        SnacCliAuthorize (cont->serv, cont, 0, msg->send_message);
    else if (cont->serv->type == TYPE_SERVER && msg->type == MSG_AUTH_REQ)
        SnacCliReqauth (cont->serv, cont, msg->send_message);
    else if (cont->serv->type == TYPE_SERVER && msg->type == MSG_AUTH_ADDED)
        SnacCliGrantauth (cont->serv, cont);
    else if (cont->serv->type == TYPE_SERVER && msg->type == MSG_AUTH_GRANT)
        SnacCliAuthorize (cont->serv, cont, 1, NULL);
    else
        ret = RET_INPR;
    MsgD (msg);
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
        IMCliMsg (cont, msgtype, msg, NULL);
    }
#if ENABLE_XMPP
    else if (cont->serv->type == TYPE_XMPP_SERVER)
        XMPPAuthorize (cont->serv, cont, how, msg);
#endif
}
