/* $Id$ */
/* Copyright: This file may be distributed under version 2 of the GPL licence. */

#include "climm.h"
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
#include "oscar_snac.h"
#include "oscar_icbm.h"
#include "oscar_oldicq.h"
#include "oscar_bos.h"
#include "cmd_user.h"
#include "contact.h"
#include "connection.h"
#include "preferences.h"
#include "oscar_dc.h"
#include "packet.h"
#include "conv.h"
#include "oscar_service.h"
#include "oscar_roster.h"
#include "xmpp_base.h"
#include "oscar_base.h"
#include "msn_base.h"
#include "util_otr.h"
#include "remote.h"

static void CallbackMeta (Event *event);
static void CallbackTogvis (Event *event);

static void CallbackTogvis (Event *event)
{
    if (!event)
        return;
    if (!event->cont || !event->conn || ContactPrefVal (event->cont, CO_INTIMATE)
        || !ContactIsInv (event->conn->serv->status))
    {
        EventD (event);
        return;
    }
    SnacCliRemvisible (event->conn->serv, event->cont);
    EventD (event);
}

Message *MsgC ()
{
    Message *msg;
    
    msg = calloc (1, sizeof *msg);
    DebugH (DEB_MSG, "M" STR_DOT STR_DOT STR_DOT "> %p:", msg);
    return msg;
}

void MsgD (Message *msg)
{
    char *s, *t;
    if (!msg)
        return;
    DebugH (DEB_MSG, "M<" STR_DOT STR_DOT STR_DOT " %p [%s] %lu %lu %lu %u %u (%s) (%s) ",
           msg, msg->cont ? msg->cont->screen : "",
           UD2UL (msg->type), UD2UL (msg->origin), UD2UL (msg->trans), msg->otrinjected, msg->force,
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
    /* process outgoing messages for OTR */
    if (msg->type != MSG_NORM || msg->otrinjected || !libotr_is_present)
        return msg;

    return OTRMsgOut (msg);
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

    event = QueueDequeue2 (msg->cont->serv->conn, QUEUE_TOGVIS, 0, msg->cont);
    if (event)
        EventD (event);
    else
        SnacCliAddvisible (msg->cont->serv, msg->cont);
    QueueEnqueueData (msg->cont->serv->conn, QUEUE_TOGVIS, 0, time (NULL) + reveal_time, NULL, msg->cont, NULL, CallbackTogvis);
    return msg;
}


UBYTE IMCliMsg (Contact *cont, UDWORD type, const char *text, Opt *opt)
{
    Message *msg;
    UBYTE ret = RET_FAIL;
    UDWORD m_trans, m_force, m_otrinject;
    
    if (!cont || !cont->serv || !(msg = MsgC ()))
    {
        OptD (opt);
        return RET_FAIL;
    }
    if (!opt || !OptGetVal (opt, CO_MSGTRANS, &m_trans))
        m_trans = CV_MSGTRANS_ANY;
    if (!opt || !OptGetVal (opt, CO_FORCE, &m_force))
        m_force = 0;
    if (!opt || !OptGetVal (opt, CO_OTRINJECT, &m_otrinject))
        m_otrinject = 0;
    OptD (opt);

    msg->cont = cont;
    msg->type = type;
    msg->send_message = strdup (text);
    msg->trans = m_trans;
    msg->force = m_force;
    msg->otrinjected = m_otrinject;

#ifdef ENABLE_OTR
    msg = cb_msg_otr (msg);
    if (!msg)
        return RET_FAIL;
#endif
    msg = cb_msg_log (msg);
    if (!msg)
        return RET_FAIL;
    ret = IMCliReMsg (cont, msg);
    while (!RET_IS_OK(ret) && msg->maxsize && !msg->otrinjected && !msg->otrencrypted)
    {
        strc_t str = s_split (&text, msg->maxenc, msg->maxsize);
        s_repl (&msg->send_message, str->txt);
        msg->maxsize = 0;
        msg->trans = m_trans;
        ret = IMCliReMsg (cont, msg);
        if (!RET_IS_OK (ret))
            break;
        msg = MsgC ();
        if (!msg)
            return RET_FAIL;
        msg->cont = cont;
        msg->type = type;
        msg->send_message = strdup (text);
        msg->trans = m_trans;
        msg->force = m_force;
        msg->otrinjected = m_otrinject;
        ret = IMCliReMsg (cont, msg);                                                            
    }
    if (!RET_IS_OK(ret) && msg->maxsize && (msg->otrinjected || msg->otrencrypted))
    {
        str_s str = { NULL, 0, 0 };
        UDWORD fragments, frag;
        char *message = msg->send_message;

        fragments = strlen (message) / msg->maxsize + 1;
        frag = 1;
        assert (fragments < 65536);
        while (frag <= fragments)
        {
        
            s_init (&str, "", msg->maxsize);
            s_catf (&str, "?OTR,%hu,%hu,", (unsigned short)frag, (unsigned short)fragments);
            s_catn (&str, message, strlen (message) / (fragments + 1 - frag));
            s_catc (&str, ',');
            message += strlen (message) / (fragments + 1 - frag);
            if (frag < fragments)
            {
                Message *msg2 = MsgC ();
                if (!msg2)
                {
                    MsgD (msg);
                    s_done (&str);
                    return RET_FAIL;
                }
                msg2->cont = cont;
                msg2->type = type;
                msg2->send_message = strdup (str.txt);
                msg2->trans = CV_MSGTRANS_ANY;
                msg2->otrinjected = 1;
                ret = IMCliReMsg (cont, msg2);
                if (!RET_IS_OK (ret))
                {
                    MsgD (msg2);
                    MsgD (msg);
                    s_done (&str);
                    return RET_FAIL;
                }
            }
            else
            {
                s_repl (&msg->send_message, str.txt);
                msg->trans = m_trans;
                msg->otrinjected = 0;
                ret = IMCliReMsg (cont, msg);
                if (!RET_IS_OK (ret))
                {
                    MsgD (msg);
                    s_done (&str);
                    return RET_FAIL;
                }
            }
            frag++;
        }
        s_done (&str);
        return RET_OK;
    }

    if (!RET_IS_OK (ret))
        MsgD (msg);
    return ret;
}

UBYTE IMCliReMsg (Contact *cont, Message *msg)
{
    UBYTE ret = RET_FAIL;

    assert (cont);
    assert (msg);
    assert (msg->cont == cont);    
    assert (msg->send_message);
    
    msg = cb_msg_revealinv (msg);
    if (!msg)
        return RET_FAIL;
    
#ifdef ENABLE_PEER2PEER
    if (msg->trans & CV_MSGTRANS_DC)
    {
        if (cont->serv->oscar_dc)
            if (RET_IS_OK (ret = PeerSendMsgFat (cont->serv->oscar_dc, cont, msg)))
                return ret;
        msg->trans &= ~CV_MSGTRANS_DC;
    }
#endif
    if (msg->trans & CV_MSGTRANS_TYPE2)
    {
        if (cont->serv->conn->connect & CONNECT_OK && cont->serv->type == TYPE_SERVER)
            if (RET_IS_OK (ret = SnacCliSendmsg (cont->serv, cont, 2, msg)))
                return ret;
        msg->trans &= ~CV_MSGTRANS_TYPE2;
    }
    if (msg->trans & CV_MSGTRANS_ICQv8)
    {
        if (cont->serv->conn->connect & CONNECT_OK && cont->serv->type == TYPE_SERVER)
        {
            if (RET_IS_OK (ret = SnacCliSendmsg (cont->serv, cont, 1, msg)))
                return ret;
            if (RET_IS_OK (ret = SnacCliSendmsg (cont->serv, cont, 4, msg)))
                return ret;
        }
        msg->trans &= ~CV_MSGTRANS_ICQv8;
    }
#if ENABLE_XMPP
    if (msg->trans & CV_MSGTRANS_XMPP)
        if (cont->serv->conn->connect & CONNECT_OK && cont->serv->type == TYPE_XMPP_SERVER)
            if (RET_IS_OK (ret = XMPPSendmsg (cont->serv, cont, msg)))
                return ret;
#endif
    return ret;
}

void IMCliInfo (Server *serv, Contact *cont, int group)
{
    UDWORD ref = 0;
    if (cont)
    {
        cont->updated = 0;
        if (cont->serv->type == TYPE_SERVER)
            ref = SnacCliMetareqinfo (cont->serv, cont);
    }
    else
    {
        if (serv->type == TYPE_SERVER)
            ref = SnacCliSearchrandom (serv, group);
    }
    if (ref)
        QueueEnqueueData (serv->conn, QUEUE_REQUEST_META, ref, time (NULL) + 60, NULL,
                          cont, NULL, &CallbackMeta);
}

static void CallbackMeta (Event *event)
{
    Contact *cont;
    
    assert (event && event->conn && event->conn->serv && event->conn == event->conn->serv->conn);

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

void IMSetStatus (Server *serv, Contact *cont, status_t status, const char *msg)
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
        if (~serv->conn->connect & CONNECT_OK)
        {
            serv->status = status;
            serv->nativestatus = 0;
        }
        else if (serv->type == TYPE_SERVER)
            SnacCliSetstatus (serv, status, 1);
#if ENABLE_XMPP
        else if (serv->type == TYPE_XMPP_SERVER)
            XMPPSetstatus (serv, NULL, status, msg);
#endif
    }
}

void IMCliAuth (Contact *cont, const char *text, auth_t how)
{
    assert (cont);
    assert (cont->serv);
    if (~cont->serv->conn->connect & CONNECT_OK)
        return;

    if (cont->serv->type == TYPE_SERVER)
    {
        UDWORD msgtype;
        switch (how)
        {
            case auth_deny:
                if (!text)         /* FIXME: let it untranslated? */
                    text = "Authorization refused\n";
                msgtype = MSG_AUTH_DENY;
                break;
            case auth_req:
                if (!text)         /* FIXME: let it untranslated? */
                    text = "Please authorize my request and add me to your Contact List\n";
                text = s_sprintf ("\xfe%s", text);
                msgtype = MSG_AUTH_REQ;
                break;
            case auth_add:
                if (!text)
                    text = "\x03";
                msgtype = MSG_AUTH_ADDED;
                break;
            default:
            case auth_grant:
                if (!text)
                    text = "\x03";
                msgtype = MSG_AUTH_GRANT;
        }
        if (cont->uin)
        {
            Message *msg = MsgC ();
            if (!msg)
                return;
            msg->cont = cont;
            msg->type = msgtype;
            msg->force = 1;
            msg->send_message = strdup (text);
            if (!RET_IS_OK (SnacCliSendmsg (cont->serv, cont, 4, msg)))
                MsgD (msg);
        }
        else
        {
            if (how == auth_deny)
                SnacCliAuthorize (cont->serv, cont, 0, text);
            else if (how == auth_req)
                SnacCliReqauth (cont->serv, cont, text);
            else if (how == auth_add)
                SnacCliGrantauth (cont->serv, cont);
            else if (how == auth_grant)
                SnacCliAuthorize (cont->serv, cont, 1, NULL);
        }
    }
#if ENABLE_XMPP
    else if (cont->serv->type == TYPE_XMPP_SERVER)
        XMPPAuthorize (cont->serv, cont, how, text);
#endif
}

Event *IMLogin (Server *serv)
{
    switch (serv->type)
    {
        case TYPE_SERVER:      return OscarLogin (serv);
#ifdef ENABLE_MSN
        case TYPE_MSN_SERVER:  return MSNLogin (serv);
#endif
#ifdef ENABLE_XMPP
        case TYPE_XMPP_SERVER: return XMPPLogin (serv);
#endif
        default:               return NULL;
    }
}

static void IMCallBackDoReconn (Event *event)
{
    if (!event || !event->conn)
    {
        EventD (event);
        return;
    }
    assert (event->conn->serv);
    QueueEnqueue (event);
    IMLogin (event->conn->serv);
}

void IMCallBackReconn (Connection *conn)
{
    ContactGroup *cg;
    Server *serv;
    Event *event;
    Contact *cont;
    int i;

    assert (conn);
    assert (conn->serv);
    assert (conn->serv->conn == conn);

    serv = conn->serv;
    cg = serv->contacts;
    
    if (!(cont = conn->cont))
        return;
    
    if (!(event = QueueDequeue2 (conn, QUEUE_DEP_WAITLOGIN, 0, NULL)))
    {
        IMLogin (serv);
        return;
    }
    
    conn->connect = 0;
    rl_log_for (cont->nick, COLCONTACT);
    if (event->attempts < 5)
    {
        rl_printf (i18n (2032, "Scheduling v8 reconnect in %d seconds.\n"), 10 << event->attempts);
        event->due = time (NULL) + (10 << event->attempts);
        event->callback = &IMCallBackDoReconn;
        QueueEnqueue (event);
    }
    else
    {
        rl_print (i18n (2031, "Connecting failed too often, giving up.\n"));
        EventD (event);
    }
    for (i = 0; (cont = ContactIndex (cg, i)); i++)
        cont->status = ims_offline;
}

void  IMConnOpen (Connection *conn)
{
    if      (conn->type == TYPE_REMOTE)    RemoteOpen (conn);
    else if (conn->type == TYPE_MSGDIRECT) ConnectionInitPeer (conn);
    else
        rl_printf (i18n (2726, "Don't know how to open connection type %s for #%ld.\n"),
                         ConnectionStrType (conn), 0L);
}

void IMLogout (Server *serv)
{
    rl_printf (i18n (2729, "Logging of from connection %ld.\n"), 0L);
    switch (serv->type)
    {
        case TYPE_SERVER:      return OscarLogout (serv);
#ifdef ENABLE_MSN
        case TYPE_MSN_SERVER:  return MSNLogout (serv);
#endif
#ifdef ENABLE_XMPP
        case TYPE_XMPP_SERVER: return XMPPLogout (serv);
#endif
        default:
            return;
    }
}

