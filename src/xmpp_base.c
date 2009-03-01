
#include "climm.h"
#include <sys/types.h>
#include <errno.h>
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
#include <fcntl.h>
#include <assert.h>

#include <iksemel.h>

#include "jabber_base.h"
#include "connection.h"
#include "contact.h"
#include "conv.h"
#include "oscar_dc.h"
#include "util_io.h"
#include "im_response.h"
#include "im_request.h"
#include "util_ui.h"
#include "util_rl.h"
#include "buildmark.h"
#include "preferences.h"
#include "oscar_dc_file.h"


static jump_conn_f XMPPCallbackDispatch;
static jump_conn_f XMPPCallbackReconn;
static jump_conn_f XMPPCallbackClose;
static jump_conn_err_f XMPPCallbackError;
static void XMPPCallBackDoReconn (Event *event);

static void XMPPCallBackTimeout (Event *event)
{
    Connection *conn = event->conn;

    if (!conn)
    {
        EventD (event);
        return;
    }
    assert (conn->serv);
    assert (conn->serv->type == TYPE_XMPP_SERVER);
    if (~conn->connect & CONNECT_OK)
        rl_print ("# XMPP timeout\n");
    EventD (event);
}

void XmppStreamError (Server *serv, const char *text)
{
    rl_printf ("Stream level error occurred: %s\n", text);
    XMPPCallbackClose (serv->conn);
}

Event *ConnectionInitXMPPServer (Server *serv)
{
    const char *sp;
    Event *event;

    assert (serv->type == TYPE_XMPP_SERVER);

    if (!serv->screen || !serv->passwd)
        return NULL;
    if (!strchr (serv->screen, '@'))
    {
        const char *jid;
        if (!serv->conn->server)
            serv->conn->server = strdup ("jabber.com");
        jid = s_sprintf ("%s@%s", serv->screen, serv->conn->server);
        s_repl (&serv->screen, jid);
    }
    if (!serv->conn->server)
        s_repl (&serv->conn->server, strchr (serv->screen, '@') + 1);
    
    XMPPCallbackClose (serv->conn);

    s_repl (&serv->xmpp_stamp, "YYYYmmddHHMMSS");
    time_t now = time (NULL);
    strftime (serv->xmpp_stamp, 15, "%Y%m%d%H%M%S", gmtime (&now));

    sp = s_sprintf ("%s", s_wordquote (s_sprintf ("%s:%lu", serv->conn->server, serv->conn->port)));
    rl_printf (i18n (2620, "Opening XMPP connection for %s at %s...\n"),
        s_wordquote (serv->screen), sp);

    if (!serv->conn->port)
        serv->conn->port = ~0;

    serv->c_open = &ConnectionInitXMPPServer;
    serv->conn->reconnect = &XMPPCallbackReconn;
    serv->conn->error = &XMPPCallbackError;
    serv->conn->close = &XMPPCallbackClose;
    serv->conn->dispatch = &XMPPCallbackDispatch;
    serv->conn->connect = 0;

    if ((event = QueueDequeue2 (serv->conn, QUEUE_DEP_WAITLOGIN, 0, NULL)))
    {
        event->attempts++;
        event->due = time (NULL) + 10 * event->attempts + 10;
        event->callback = &XMPPCallBackTimeout;
        QueueEnqueue (event);
    }
    else
        event = QueueEnqueueData (serv->conn, QUEUE_DEP_WAITLOGIN, 0, time (NULL) + 5,
                                  NULL, serv->conn->cont, NULL, &XMPPCallBackTimeout);

    UtilIOConnectTCP (serv->conn);
    return event;
}

static void XMPPCallbackReconn (Connection *conn)
{
    Server *serv = conn->serv;
    ContactGroup *cg = serv->contacts;
    Event *event;
    Contact *cont;
    int i;

    if (!(cont = conn->cont))
        return;

    if (!(event = QueueDequeue2 (conn, QUEUE_DEP_WAITLOGIN, 0, NULL)))
    {
        ConnectionInitXMPPServer (serv);
        return;
    }

    conn->connect = 0;
    rl_log_for (cont->nick, COLCONTACT);
    if (event->attempts < 5)
    {
        rl_printf (i18n (2032, "Scheduling v8 reconnect in %d seconds.\n"), 10 << event->attempts);
        event->due = time (NULL) + (10 << event->attempts);
        event->callback = &XMPPCallBackDoReconn;
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

void XmppSaveLog (void *user_data, const char *text, size_t size, int in)
{
    Server *serv = user_data;
    iksparser *prs = serv->xmpp_private;
    const char *data;
    size_t rc;

    if (serv->logfd < 0)
    {
        const char *dir, *file;
        dir = s_sprintf ("%sdebug", PrefUserDir (prG));
        mkdir (dir, 0700);
        dir = s_sprintf ("%sdebug" _OS_PATHSEPSTR "packets.xmpp.%s", PrefUserDir (prG), serv->screen);
        mkdir (dir, 0700);
        file = s_sprintf ("%sdebug" _OS_PATHSEPSTR "packets.xmpp.%s/%lu", PrefUserDir (prG), serv->screen, time (NULL));
        serv->logfd = open (file, O_WRONLY | O_CREAT | O_APPEND, 0600);
    }
    if (serv->logfd < 0)
        return;

    data = s_sprintf ("%s%s %s%s\n",
        iks_is_secure (prs) ? "SSL " :  "", s_now,
        in & 1 ? "<<<" : ">>>", in & 2 ? " residual:" : "");
    rc = write (serv->logfd, data, strlen (data));

    text = s_ind (text);
    rc = write (serv->logfd, text, strlen (text));
    rc = write (serv->logfd, "\n", 1);
}

static void GetBothContacts (iksid *j, Server *conn, Contact **b, Contact **f, char crea)
{
    Contact *bb, *ff, **t;
    char *jb = j->partial;
    char *jr = j->resource;

    if ((bb = ContactFindScreen (conn->contacts, jb)))
    {
        if (!(ff = ContactFindScreenP (conn->contacts, bb, jr ? jr : "")))
        {
            ff = ContactScreenP (conn, bb, jr ? jr : "");
        }
    }
    else
    {
        bb = ContactScreen (conn, jb);
        if (crea)
            ContactCreate (conn, bb);
        ff = ContactScreenP (conn, bb, jr ? jr : "");
    }
    assert (bb);
    if (ff)
    {
        /* make ff the firstchild or the firstchild->next of bb */
        if (!bb->firstchild)
            bb->firstchild = ff;
        else if (bb->firstchild != ff && bb->firstchild->next != ff)
        {
            for (t = &bb->firstchild; *t; t = &((*t)->next))
                if (*t == ff)
                {
                    *t = ff->next;
                    ff->next = bb->firstchild->next;
                    bb->firstchild->next = ff;
                    t = NULL;
                    break;
                }
            if (t)
            {
                ff->next = bb->firstchild->next;
                bb->firstchild->next = ff;
            }
        }
    }
    else
        ff = bb;
    *b = bb;
    *f = ff;
}

static int XmppHandleMessage (void *user_data, ikspak *pak)
{
    Server *serv = user_data;
    iksparser *prs = serv->xmpp_private;
//    char *toX = iks_find_attrib (pak->x, "to");
//    iksid *to = iks_id_new (iks_stack (pak->x), toX);

//    Contact *cto = ContactScreen (serv, pak->tof);
//    std::string subtypeval = t->findAttribute ("type");
//    std::string body = t->body();
//    std::string subject = t->subject();
//    std::string html;
//    gloox::Tag *htmltag = findNamespacedChild (t, "html", "http://jabber.org/protocol/xhtml-im");
//    if (htmltag)
//        htmltag = findNamespacedChild (htmltag, "body", "http://www.w3.org/1999/xhtml");
//    if (htmltag)
//        html = htmltag->cdata();
//    DropAttrib (t, "type");
    time_t delay = -1;
    Contact *contb, *contr;

    GetBothContacts (pak->from, serv, &contb, &contr, 0);

//    DropAllChilds (t, "subject");
//    DropAllChilds (t, "thread");
//    handleXEP71 (t);
//    handleGoogleNosave (t);
//    handleGoogleSig (t);
//    handleGoogleChatstate (t);
//    handleXEP136 (t);
//    delay = handleXEP91 (t);
//    handleXEP115 (t, contr); // entity capabilities (used also for client version)
//    if (handleXEP22and85 (t, contr, from, tof, id))
//        return;
//    DropAllChilds (t, "body");

    Opt *opt = OptSetVals (NULL, CO_ORIGIN, CV_ORIGIN_v8, CO_MSGTYPE, MSG_NORM, CO_MSGTEXT, iks_find_cdata (pak->x, "body"), 0);

//    if (!subject.empty())
//        opt = OptSetVals (opt, CO_MSGTYPE, MSG_NORM_SUBJ, CO_SUBJECT, subject.c_str(), 0);
//    if (!strcmp (html.c_str(), body.c_str()))
//        opt = OptSetVals (opt, CO_SAMEHTML, 1);
    IMSrvMsgFat (contr, delay, opt);

//    if (gloox::Tag *x = t->findChild ("x"))
//        CheckInvalid (x);
    return IKS_FILTER_EAT;
}



static int XmppHandlePresence (void *user_data, ikspak *pak)
{
    Server *serv = user_data;
    iksparser *prs = serv->xmpp_private;

    ContactGroup *tcg;
    Contact *contb, *contr, *c;
    status_t status;
//    std::string pri;
//    time_t delay;
    char *statustext;

    GetBothContacts (pak->from, serv, &contb, &contr, 1);

//    if (gloox::Tag *priority = s->findChild ("priority"))
//    {
//        pri = priority->cdata();
//        DropCData (priority);
//        CheckInvalid (priority);
//    }

//    delay = handleXEP91 (s);

//    handleXEP115 (s, contr); // entity capabilities (used also for client version)
//    handleXEP153 (s, contb); // vcard-based avatar, nickname
//    handleXEP27 (s);         // OpenPGP signature (obsolete)
//    handleXEP8 (s);          // iq-based avatar (obsolete)

    if (pak->subtype == IKS_TYPE_UNAVAILABLE)
    {
        status = ims_offline;
//        DropAttrib (s, "type");
        IMOffline (contr);
        return IKS_FILTER_EAT;
    }
    else if (pak->show == IKS_SHOW_CHAT) status = ims_ffc;
    else if (pak->show == IKS_SHOW_AWAY) status = ims_away;
    else if (pak->show == IKS_SHOW_DND) status = ims_dnd;
    else if (pak->show == IKS_SHOW_XA) status = ims_na;
    else if (pak->show == IKS_SHOW_AVAILABLE) status = ims_online;
    else assert (0);
    
    IMOnline (contr, status, imf_none, pak->show, iks_find_cdata (pak->x, "status"));
    return IKS_FILTER_EAT;
}


static int XmppUnknown (void *user_data, ikspak *pak)
{
    printf ("Got unexpected packet:\n  %s\n", iks_string (iks_stack (pak->x), pak->x));
    return IKS_FILTER_EAT;
}

static void XmppLoggedIn (Server *serv)
{
    iksparser *prs = serv->xmpp_private;

    if (serv->xmpp_filter)
        iks_filter_delete (serv->xmpp_filter);
    serv->xmpp_filter = iks_filter_new ();
    iks_filter_add_rule (serv->xmpp_filter, XmppUnknown, serv, IKS_RULE_DONE);
    iks_filter_add_rule (serv->xmpp_filter, XmppHandlePresence, serv, IKS_RULE_TYPE, IKS_PAK_PRESENCE, IKS_RULE_DONE);
    iks_filter_add_rule (serv->xmpp_filter, XmppHandleMessage, serv, IKS_RULE_TYPE, IKS_PAK_MESSAGE, IKS_RULE_DONE);

    serv->conn->connect = CONNECT_OK | CONNECT_SELECT_R;
    
    XMPPSetstatus (serv, NULL, serv->status, serv->conn->cont->status_message);

    iks *x = iks_make_iq (IKS_TYPE_GET, IKS_NS_ROSTER);
    iks_insert_attrib (x, "id", s_sprintf ("roster-%s-%x", serv->xmpp_stamp, serv->conn->our_seq++));
    iks_send (prs, x);
    iks_delete (x);
                                                                    
//    QueueEnqueueData2 (serv->conn, QUEUE_SRV_KEEPALIVE, 0, 300, NULL, &sendIqTimeReqs, NULL);

    x = iks_make_iq (IKS_TYPE_GET, "http://jabber.org/protocol/disco#info");
    iks_insert_attrib (x, "id", s_sprintf ("disco-%s-%x", serv->xmpp_stamp, serv->conn->our_seq++));
    iks_insert_attrib (x, "to", serv->xmpp_id->server);
    iks_send (prs, x);
    iks_delete (x);
}

static int XmppSessionResult (void *user_data, ikspak *pak)
{
    if (pak->subtype != IKS_TYPE_RESULT)
        XmppStreamError (user_data, s_sprintf ("Couldn't get session %s.", iks_string (iks_stack (pak->x), pak->x)));
    else
        XmppLoggedIn (user_data);
    return IKS_FILTER_EAT;
}

static int XmppBindResult (void *user_data, ikspak *pak)
{
    Server *serv = user_data;
    iksparser *prs = serv->xmpp_private;
    if (pak->subtype == IKS_TYPE_RESULT)
    {
        iks *bind = iks_find_with_attrib (pak->x, "bind", "xmlns", "urn:ietf:params:xml:ns:xmpp-bind");
        char *newjid;
        if  (bind && (newjid = iks_find_cdata (bind, "jid")))
        {
            iksid *newid = iks_id_new (iks_parser_stack (prs), newjid);
            if (strchr (serv->screen,  '/'))
                s_repl (&serv->screen, s_sprintf ("%s/%s", newid->partial, strchr (serv->screen,  '/')  + 1));
            else
                s_repl (&serv->screen, newid->partial);
            if (strcmp (serv->xmpp_id->partial, newid->partial))
                rl_printf ("Server changed JID from %s to %s.\n", serv->xmpp_id->partial, newid->partial);
            serv->xmpp_id = newid;
        }
    } else
        XmppStreamError (serv, s_sprintf ("Couldn't bind. %s", iks_string (iks_stack (pak->x), pak->x)));
    return IKS_FILTER_EAT;
}

static int XmppStreamHook (void *user_data, int type, iks *node)
{
    Server *serv = user_data;
    iksparser *prs = serv->xmpp_private;
    switch (type)
    {
        case IKS_NODE_STOP:
            XmppStreamError (serv, "server disconnect");
            if (node) iks_delete (node);
            return IKS_NET_DROPPED;
        case IKS_NODE_ERROR:
            XmppStreamError (serv, "stream error");
            if (node) iks_delete (node);
            return IKS_NET_DROPPED;
        case IKS_NODE_START:
            // nothing to do with it
            break;
        case IKS_NODE_NORMAL:
            if (!strcmp ("stream:features", iks_name (node)))
            {
                int feat = iks_stream_features (node);
                if (feat & IKS_STREAM_STARTTLS && !iks_is_secure (prs))
                    iks_start_tls (prs);
                else if (feat & IKS_STREAM_SASL_MD5)
                    iks_start_sasl (prs, IKS_SASL_DIGEST_MD5, serv->screen, serv->passwd);
                else if (feat & IKS_STREAM_SASL_PLAIN)
                    iks_start_sasl (prs, IKS_SASL_PLAIN, serv->screen, serv->passwd);
                else
                {
                    if (feat & (IKS_STREAM_BIND | IKS_STREAM_SESSION))
                    {
                        if (serv->xmpp_filter)
                            iks_filter_delete (serv->xmpp_filter);
                        serv->xmpp_filter = iks_filter_new ();
                        iks_filter_add_rule (serv->xmpp_filter, XmppUnknown, serv, IKS_RULE_DONE);
                        if (feat & IKS_STREAM_BIND)
                        {
                            iks *t = iks_make_resource_bind (serv->xmpp_id);
                            iks_insert_attrib (t, "id", "bind");
                            iks_filter_add_rule (serv->xmpp_filter, XmppBindResult, serv,
                                IKS_RULE_TYPE, IKS_PAK_IQ, IKS_RULE_ID, "bind", IKS_RULE_DONE);
                            iks_send (prs, t);
                            iks_delete (t);
                        }
                        if (feat & IKS_STREAM_SESSION)
                        {
                            iks *t = iks_make_session ();
                            iks_insert_attrib (t, "id", "auth");
                            iks_filter_add_rule (serv->xmpp_filter, XmppSessionResult, serv,
                                IKS_RULE_TYPE, IKS_PAK_IQ, IKS_RULE_ID, "auth", IKS_RULE_DONE);
                            iks_send (prs, t);
                            iks_delete (t);
                        }
                    }
                    else
                        XmppLoggedIn (serv);
                }
            }
            else if (strcmp ("failure", iks_name (node)) == 0)
                XmppStreamError (serv, "sasl authentication failed");
            else if (strcmp ("success", iks_name (node)) == 0)
                iks_send_header (prs, serv->xmpp_id->server);
            else {
                ikspak *pak = iks_packet (node);
                iks_filter_packet (serv->xmpp_filter, pak);
            }
    }
    if (node) iks_delete (node);
    return IKS_OK;
}

void XMPPCallbackDispatch (Connection *conn)
{
    iksparser *prs = conn->serv->xmpp_private;
    int rc;

    assert (conn->sok >= 0);
    if (!(conn->connect & (CONNECT_OK | 4)))
    {
        switch  (conn->connect & 3)
        {
            case 0:
            case 3:
                assert (0);
            case 2:
                XMPPCallbackReconn (conn);
                return;
            case 1:
                conn->connect |= CONNECT_SELECT_R | 4;
                conn->connect &= ~CONNECT_SELECT_W & ~CONNECT_SELECT_X & ~3;
                prs = iks_stream_new (IKS_NS_CLIENT, conn->serv, &XmppStreamHook);

                conn->serv->xmpp_id = iks_id_new (iks_parser_stack (prs), conn->serv->screen);
                if (!conn->serv->xmpp_id->resource)
                    conn->serv->xmpp_id = iks_id_new (iks_parser_stack (prs), s_sprintf ("%s@%s/%s", conn->serv->xmpp_id->user, conn->serv->xmpp_id->server, "iks"));
                iks_set_log_hook (prs, XmppSaveLog);
                assert  (!conn->serv->xmpp_private);
                conn->serv->xmpp_private = prs;
                rc = iks_connect_fd (prs, conn->sok);
                if (rc != IKS_OK)
                {
                    XmppStreamError (conn->serv, "could not link in fd");
                    return;
                }
                rc = iks_send_header (prs, conn->serv->xmpp_id->server);
                if (rc != IKS_OK)
                {
                    XmppStreamError (conn->serv, "could not send stream header");
                    return;
                }
                conn->connect |= 4;
                return;
        }
    }
    rc = iks_recv (prs, 0);
    if (rc != IKS_OK)
        XmppStreamError (conn->serv, s_sprintf ("failing with error code %d", rc));
}

static void XMPPCallBackDoReconn (Event *event)
{
    if (!event || !event->conn)
    {
        EventD (event);
        return;
    }
    assert (event->conn->serv);
    assert (event->conn->serv->type == TYPE_XMPP_SERVER);
    QueueEnqueue (event);
    ConnectionInitXMPPServer (event->conn->serv);
}

void XMPPCallbackClose (Connection *conn)
{
    if (!conn)
        return;

    QueueCancel (conn);

    if (conn->serv->xmpp_private)
    {
        iksparser *prs = conn->serv->xmpp_private;
        iks_parser_delete (prs);
        conn->serv->xmpp_private = NULL;
        conn->serv->xmpp_id = NULL;
        conn->serv->xmpp_filter = NULL;
    }
    
    if (conn->sok >= 0)
    {
        close (conn->sok);
        conn->sok = -1;
    }

    conn->connect = 0;
}

BOOL XMPPCallbackError (Connection *conn, UDWORD rc, UDWORD flags)
{
    rl_printf ("#XMPPCallbackError: %p %lu %lu\n", conn, rc, flags);
    return 0;
}



static void SnacCallbackXmpp (Event *event)
{
    Message *msg = (Message *)event->data;

    assert (event);
    assert (msg);
    assert (event->cont);

    if (event->attempts < 5)
    {
        if (msg->send_message && !msg->otrinjected)
        {
            msg->type = INT_MSGACK_V8;
//            IMIntMsgMsg (msg, NOW, ims_offline);
        }
        event->attempts = 20;
        event->due = time (NULL) + 600;
        QueueEnqueue (event);
    }
    else
    {
        MsgD (msg);
        event->data = NULL;
        EventD (event);
    }
}

static void SnacCallbackXmppCancel (Event *event)
{
    Message *msg = (Message *)event->data;
    if (msg->send_message && !msg->otrinjected)
        rl_printf (i18n (2234, "Message %s discarded - lost session.\n"),
    msg->plain_message ? msg->plain_message : msg->send_message);
    MsgD (msg);
    event->data = NULL;
    EventD (event);
}

UBYTE XMPPSendmsg (Server *serv, Contact *cont, Message *msg)
{
    iksparser *prs = serv->xmpp_private;

    assert (cont);
    assert (msg->send_message);

    if (~serv->conn->connect & CONNECT_OK)
        return RET_DEFER;
    if (msg->type != MSG_NORM)
        return RET_DEFER;
    
    iks *x = iks_make_msg (IKS_TYPE_CHAT, cont->screen, msg->send_message);
    iks_insert_attrib (x, "id", s_sprintf ("xmpp-%s-%x", serv->xmpp_stamp, ++serv->conn->our_seq));
    iks_insert_attrib (x, "from", serv->xmpp_id->full);
//    gloox::Tag *x = new gloox::Tag (msgs, "x");
//    x->addAttribute ("xmlns", "jabber:x:event");
//    new gloox::Tag (x, "offline");
//    new gloox::Tag (x, "delivered");
//    new gloox::Tag (x, "displayed");
//    new gloox::Tag (x, "composing");
//    m_client->send (msgs);
    iks_send (prs, x);
    iks_delete (x);

    Event *event = QueueEnqueueData2 (serv->conn, QUEUE_XMPP_RESEND_ACK, serv->conn->our_seq, 120, msg, &SnacCallbackXmpp, &SnacCallbackXmppCancel);
    event->cont = cont;

    return RET_OK;
}

static enum ikshowtype StatusToIksstatus (status_t *status)
{
    switch (*status)
    {
        case ims_online:   return IKS_SHOW_AVAILABLE;   break;
        case ims_ffc:      return IKS_SHOW_CHAT;        break;
        case ims_away:     return IKS_SHOW_AWAY;        break;
        case ims_occ:      *status = ims_dnd;
        case ims_dnd:      return IKS_SHOW_DND;         break;
        case ims_na:       return IKS_SHOW_XA;          break;
        case ims_offline:  *status = ims_inv;
        default:           return IKS_SHOW_UNAVAILABLE; break;
    }
}

void XMPPSetstatus (Server *serv, Contact *cont, status_t status, const char *msg)
{
    iksparser *prs = serv->xmpp_private;
    enum ikshowtype p = StatusToIksstatus (&status);
    iks *x = iks_make_pres (p, msg);
    if (p != IKS_SHOW_UNAVAILABLE)
    {
        iks *caps = iks_insert (x, "c");
        iks_insert_attrib (caps, "xmlns", "http://jabber.org/protocol/caps");
        iks_insert_attrib (caps, "node", "http://www.climm.org/xmpp/caps");
        iks_insert_attrib (caps, "ver", s_sprintf ("[iksemel] %s", BuildVersionStr));
    }
    if (cont)
        iks_insert_attrib (x, "to", cont->screen);
    else
    {
        s_repl (&serv->conn->cont->status_message, msg);
        serv->status = status;
        serv->nativestatus = p;
    }
    iks_send (prs, x);
    iks_delete (x);
}

void XMPPAuthorize (Server *serv, Contact *cont, auth_t how, const char *msg)
{
}

void  XMPPGoogleMail (Server *serv, time_t since, const char *query)
{
}

#if 0

class CLIMMXMPP: public gloox::ConnectionListener, public gloox::MessageHandler,
    private :
        void handleXEP8 (gloox::Tag *t);
        bool handleXEP22and85 (gloox::Tag *t, Contact *cfrom, gloox::JID from, std::string tof, std::string id);
        void handleXEP22a (gloox::Tag *XEP22, Contact *cfrom);
        void handleXEP22b (gloox::Tag *XEP22, gloox::JID from, std::string tof, std::string id);
        void handleXEP22c (gloox::JID from, std::string tof, std::string id, std::string type);
        void handleXEP27 (gloox::Tag *t);
        void handleXEP71 (gloox::Tag *t);
        void handleXEP115 (gloox::Tag *t, Contact *contr);
        void handleXEP153 (gloox::Tag *t, Contact *contr);
        void handleGoogleNosave (gloox::Tag *t);
        void handleGoogleSig (gloox::Tag *t);
        void handleGoogleChatstate (gloox::Tag *t);
        void handleXEP136 (gloox::Tag *t);
        time_t handleXEP91 (gloox::Tag *t);

    public :
        virtual void  onDisconnect (gloox::ConnectionError e);
        virtual void  handleSubscription (gloox::Stanza *stanza);
        virtual void  handleLog (gloox::LogLevel level, gloox::LogArea area, const std::string &message);

        // GMail support
        void sendIqGmail (int64_t newer = 0ULL, std::string newertid = "", std::string q = "", bool isauto = 1);
        void sendIqTime (void);
        std::string gmail_new_newertid;
        std::string gmail_newertid;
        std::string gmail_query;
        int64_t gmail_new_newer;
        int64_t gmail_newer;
        
        // IqHandler
        virtual bool handleIq (gloox::Stanza *stanza);
        virtual bool handleIqID (gloox::Stanza *stanza, int context);
        
        // DiscoHandler 
        virtual void handleDiscoInfoResult (gloox::Stanza *stanza, int context);
        virtual void handleDiscoItemsResult (gloox::Stanza *stanza, int context);
        virtual void handleDiscoError (gloox::Stanza *stanza, int context);
        virtual bool handleDiscoSet (gloox::Stanza *stanza);

        void  XMPPAuthorize (Server *serv, Contact *cont, auth_t how, const char *msg);
};

CLIMMXMPP::CLIMMXMPP (Server *serv)
{
    m_client->disco()->setVersion ("climm", BuildVersionStr, BuildPlatformStr);
    m_client->disco()->setIdentity ("client", "console");
    m_client->disco()->registerDiscoHandler (this);
    m_client->setInitialPriority (5);

    gmail_new_newer = 0ULL;
    gmail_newer = 0ULL;
}

void CLIMMXMPP::onDisconnect (gloox::ConnectionError e)
{
    m_serv->conn->connect = 0;
    if (m_serv->conn->sok != -1)
        close (m_serv->conn->sok);
    m_serv->conn->sok = -1;
    switch (e)
    {
        case gloox::ConnNoError:
            rl_printf ("#onDisconnect: NoError.\n");
            break;
        case gloox::ConnStreamError:
            {
                std::string sET = m_client->streamErrorText();
                rl_printf ("#onDisconnect: Error %d: ConnStreamError: Error %d: %s\n",
                           e, m_client->streamError(), sET.c_str());
            }
            break;
        case gloox::ConnStreamClosed:
        case gloox::ConnIoError:
            XMPPCallbackReconn (m_serv->conn);
            return;

        case gloox::ConnOutOfMemory:          rl_printf ("#onDisconnect: Error OutOfMemory %d.\n", e); break;
        case gloox::ConnNoSupportedAuth:      rl_printf ("#onDisconnect: Error NoSupportedAuth %d.\n", e); break;
        case gloox::ConnTlsFailed:            rl_printf ("#onDisconnect: Error TlsFailed %d.\n", e); break;
        case gloox::ConnAuthenticationFailed: rl_printf ("#onDisconnect: Error AuthenticationFailed %d.\n", e);
            s_repl (&m_serv->passwd, NULL);
            break;
        case gloox::ConnUserDisconnected:     return;
        case gloox::ConnNotConnected:         rl_printf ("#onDisconnect: Error NotConnected %d.\n", e); break;
        default:
            rl_printf ("#onDisconnect: Error %d.\n", e);
    }
}

static bool DropChild (gloox::Tag *s, gloox::Tag *c)
{
    if (s->children().size())
        s->setCData ("");
    s->children().remove (c);
    delete c;
}

static bool DropAttrib (gloox::Tag *s, const std::string &a)
{
    if (s->children().size())
        s->setCData ("");
#if defined(LIBGLOOX_VERSION) && LIBGLOOX_VERSION >= 0x000900
    s->attributes().remove (gloox::Tag::Attribute (a, s->findAttribute (a)));
#else
    s->attributes().erase (a);
#endif
}

static bool DropCData (gloox::Tag *s)
{
    s->setCData ("");
}

static bool CheckInvalid (gloox::Tag *s)
{
    if (!s->attributes().size() && !s->children().size() && s->cdata().empty())
    {
        if (s->parent())
            DropChild (s->parent(), s);
        return true;
    }
    return false;
}

static void DropAllChilds (gloox::Tag *s, const std::string &a)
{
    gloox::Tag::TagList::const_iterator it;
    for (it = s->children().begin(); it != s->children().end(); ++it)
    {
        if ((*it)->name() == a)
        {
            gloox::Tag *b = *it;
            DropCData (b);
            DropAttrib (b, "xml:lang");
            if (CheckInvalid (b))
                return (DropAllChilds (s, a));
        }
    }
}

static void DropAllChildsTree (gloox::Tag *s, const std::string &a)
{
    gloox::Tag::TagList::const_iterator it;
    if (a.empty ())
    {
        s->attributes().clear();
        for (it = s->children().begin(); it != s->children().end(); it = s->children().begin())
        {
            gloox::Tag *b = *it;
            DropCData (b);
            DropAllChildsTree (b, "");
            CheckInvalid (b);
        }
    }
    else
    {
        for (it = s->children().begin(); it != s->children().end(); ++it)
        {
            while ((*it)->name() == a)
            {
                gloox::Tag *b = *it;
                DropCData (b);
                DropAllChildsTree (b, "");
                CheckInvalid (b);
                it = s->children().begin();
            }
        }
    }
}

static int __SkipChar (const char **s, char c)
{
    if (**s == c && **s)
        (*s)++;
    if (**s)
        return *((*s)++) - '0';
    return 0;
}

static time_t ParseUTCDate (std::string str)
{
   const char *s = str.c_str();
   struct tm tm;
   tm.tm_year =  __SkipChar (&s, 0) * 1000;
   tm.tm_year += __SkipChar (&s, 0) * 100;
   tm.tm_year += __SkipChar (&s, 0) * 10;
   tm.tm_year += __SkipChar (&s, 0) - 1900;
   tm.tm_mon  =  __SkipChar (&s, '-') * 10;
   tm.tm_mon  += __SkipChar (&s, 0) - 1;
   tm.tm_mday =  __SkipChar (&s, '-') * 10;
   tm.tm_mday += __SkipChar (&s, 0);
   tm.tm_hour =  __SkipChar (&s, 'T') * 10;
   tm.tm_hour += __SkipChar (&s, 0);
   tm.tm_min  =  __SkipChar (&s, ':') * 10;
   tm.tm_min  += __SkipChar (&s, 0);
   tm.tm_sec  =  __SkipChar (&s, ':') * 10;
   if (!*s)
       return -1;
   tm.tm_sec  += __SkipChar (&s, 0);
   if (*s)
       return -1;
    return timegm (&tm);
}

static gloox::Tag *findNamespacedChild (gloox::Tag *tag, const char *childname, const char *childnamespace)
{
    gloox::Tag::TagList::const_iterator it = tag->children ().begin ();
    while (it != tag->children ().end())
    {
#if defined(LIBGLOOX_VERSION) && LIBGLOOX_VERSION >= 0x000900
        gloox::Tag::AttributeList::const_iterator ait = (*it)->attributes().begin ();
#else
        gloox::StringMap::iterator ait = (*it)->attributes().begin ();
#endif
        while (ait != (*it)->attributes().end ())
        {
            if ((*ait).first == "xmlns" && (*ait).second == childnamespace)
            {
#if defined(LIBGLOOX_VERSION) && LIBGLOOX_VERSION >= 0x000900
                (*it)->attributes ().remove (*ait);
#else
                (*it)->attributes ().erase (ait);
#endif
                return *it;
            }
            if (!(*ait).first.compare (0, 6, "xmlns:") && (*ait).second == childnamespace)
            {
#if defined(LIBGLOOX_VERSION) && LIBGLOOX_VERSION >= 0x000900
                (*it)->attributes ().remove (*ait);
#else
                (*it)->attributes ().erase (ait);
#endif
                return *it;
            }
            ait++;
        }
        it++;
    }
    return NULL;
}

void CLIMMXMPP::handleXEP8 (gloox::Tag *t)
{
    if (gloox::Tag *avatar = findNamespacedChild (t, "x", "jabber:x:avatar"))
    {
        if (gloox::Tag *hash = avatar->findChild ("hash"))
        {
            DropCData (hash);
            CheckInvalid (hash);
        }
        CheckInvalid (avatar);
    }
}

void CLIMMXMPP::handleXEP22a (gloox::Tag *XEP22, Contact *cfrom)
{
    std::string refid;
    int ref = -1;
    int_msg_t type;

    if (gloox::Tag *tid = XEP22->findChild ("id"))
    {
        refid = tid->cdata();
        DropCData (tid);
        CheckInvalid (tid);
    }
    if (!strncmp (refid.c_str(), "xmpp-", 5) && !strncmp (refid.c_str() + 5, m_stamp, 14)
        && strlen (refid.c_str()) > 19 && refid.c_str()[19] == '-')
        sscanf (refid.c_str() + 20, "%x", &ref);

    if (gloox::Tag *dotag = XEP22->findChild ("offline"))
    {
        type = INT_MSGOFF;
        CheckInvalid (dotag);
    }
    else if (gloox::Tag *dotag = XEP22->findChild ("paused"))
    {
        CheckInvalid (dotag);
        IMIntMsg (cfrom, NOW, ims_offline, INT_MSGNOCOMP, "");
        ref = -1;
    }
    else if (gloox::Tag *dotag = XEP22->findChild ("delivered"))
    {
        type = INT_MSGACK_TYPE2;
        CheckInvalid (dotag);
    }
    else if (gloox::Tag *dotag = XEP22->findChild ("displayed"))
    {
        type = INT_MSGDISPL;
        CheckInvalid (dotag);
    }
    else if (gloox::Tag *dotag = XEP22->findChild ("composing"))
    {
        CheckInvalid (dotag);
        IMIntMsg (cfrom, NOW, ims_offline, INT_MSGCOMP, "");
        ref = -1;
    }
    else
        ref = -1;

    if (ref != -1)
    {
        Event *event = QueueDequeue (m_serv->conn, QUEUE_XMPP_RESEND_ACK, ref);
        if (event)
        {
            Message *msg = (Message *)event->data;
            assert (msg);
            if (msg->send_message && !msg->otrinjected)
            {
                msg->type = type;
//                assert (msg->cont == cfrom); ->parent
                IMIntMsgMsg (msg, NOW, ims_offline);
            }
            event->attempts += 5;
            QueueEnqueue (event);
        }
    }

    CheckInvalid (XEP22);
}

void CLIMMXMPP::handleXEP22c (gloox::JID from, std::string tof, std::string id, std::string type)
{
    gloox::Stanza *msg = gloox::Stanza::createMessageStanza (from, "");
    msg->addAttribute ("id", s_sprintf ("ack-%s-%x", m_stamp, m_serv->conn->our_seq++));
    std::string res = m_client->resource();
    msg->addAttribute ("from", s_sprintf ("%s/%s", m_serv->screen, res.c_str()));
    gloox::Tag *x = new gloox::Tag (msg, "x");
    x->addAttribute ("xmlns", "jabber:x:event");
    new gloox::Tag (x, type);
    new gloox::Tag (x, "id", id);
    m_client->send (msg);
}

void CLIMMXMPP::handleXEP22b (gloox::Tag *XEP22, gloox::JID from, std::string tof, std::string id)
{
    if (gloox::Tag *dotag = XEP22->findChild ("delivered"))
    {
        handleXEP22c (from, tof, id, "delivered");
        CheckInvalid (dotag);
    }
    if (gloox::Tag *dotag = XEP22->findChild ("displayed"))
    {
        handleXEP22c (from, tof, id, "displayed");
        CheckInvalid (dotag);
    }
    if (gloox::Tag *dotag = XEP22->findChild ("composing"))
    {
        CheckInvalid (dotag);
    }
    if (gloox::Tag *dotag = XEP22->findChild ("offline"))
    {
        CheckInvalid (dotag);
    }
}

bool CLIMMXMPP::handleXEP22and85 (gloox::Tag *t, Contact *cfrom, gloox::JID from, std::string tof, std::string id)
{
    bool ret = false;
    if (gloox::Tag *XEP22 = findNamespacedChild (t, "x", "jabber:x:event"))
    {
        ret = true;
        if (!t->hasChild ("body"))
            handleXEP22a (XEP22, cfrom);
        else
            handleXEP22b (XEP22, from, tof, id);
    }
    if (gloox::Tag *address = findNamespacedChild (t, "addresses", "http://jabber.org/protocol/address"))
    {
        DropAllChildsTree (address, "address");
        CheckInvalid (address);
        ret = true;
    }
    
    if (gloox::Tag *active = findNamespacedChild (t, "active", "http://jabber.org/protocol/chatstates"))
    {
        CheckInvalid (active);
        ret = true;
    }
    if (gloox::Tag *composing = findNamespacedChild (t, "composing", "http://jabber.org/protocol/chatstates"))
    {
        CheckInvalid (composing);
        IMIntMsg (cfrom, NOW, ims_offline, INT_MSGCOMP, "");
        ret = true;
    }
    if (gloox::Tag *paused = findNamespacedChild (t, "paused", "http://jabber.org/protocol/chatstates"))
    {
        CheckInvalid (paused);
        IMIntMsg (cfrom, NOW, ims_offline, INT_MSGNOCOMP, "");
        ret = true;
    }
    if (gloox::Tag *inactive = findNamespacedChild (t, "inactive", "http://jabber.org/protocol/chatstates"))
    {
        CheckInvalid (inactive);
        ret = true;
    }
    if (gloox::Tag *gone = findNamespacedChild (t, "gone", "http://jabber.org/protocol/chatstates"))
    {
        CheckInvalid (gone);
        ret = true;
    }
    if (ret && !t->hasChild ("body"))
        return true;
    return false;
}

void CLIMMXMPP::handleXEP27 (gloox::Tag *t)
{
    if (gloox::Tag *sig = findNamespacedChild (t, "x", "jabber:x:signed"))
    {
        DropCData (sig);
        CheckInvalid (sig);
    }
}

void CLIMMXMPP::handleXEP71 (gloox::Tag *t)
{
    if (gloox::Tag *xhtmlim = findNamespacedChild (t, "html", "http://jabber.org/protocol/xhtml-im"))
    {
        DropAllChildsTree (xhtmlim, "body");
        CheckInvalid (xhtmlim);
    }
}


time_t CLIMMXMPP::handleXEP91 (gloox::Tag *t)
{
    time_t date = NOW;
    if (gloox::Tag *delay = findNamespacedChild (t, "x", "jabber:x:delay"))
    {
        struct tm;
        std::string dfrom = delay->findAttribute ("from");
        std::string stamp = delay->findAttribute ("stamp");
        date = ParseUTCDate (stamp);
        if (date != NOW)
            DropAttrib (delay, "stamp");
        DropAttrib (delay, "from");
        CheckInvalid (delay);
    }
    return date;
}

void CLIMMXMPP::handleXEP115 (gloox::Tag *t, Contact *contr)
{
    gloox::Tag *caps;
    if ((caps = findNamespacedChild (t, "c", "http://jabber.org/protocol/caps")))
    {
        std::string node = caps->findAttribute ("node");
        std::string ver = caps->findAttribute ("ver");
        std::string ext = caps->findAttribute ("ext");
        if (!strcmp (node.c_str(), "http://www.climm.org/xmpp/caps"))
            node = "climm";
        else if (!strcmp (node.c_str(), "http://mail.google.com/xmpp/client/caps"))
            node = "GoogleMail";
        else if (!strcmp (node.c_str(), "http://www.google.com/xmpp/client/caps"))
            node = "GoogleTalk";
        else if (!strcmp (node.c_str(), "http://pidgin.im/caps"))
            node = "Pidgin";
        else if (!strcmp (node.c_str(), "http://gaim.sf.net/caps"))
            node = "Gaim";
        else if (!strcmp (node.c_str(), "http://kopete.kde.org/jabber/caps"))
            node = "Kopete";
        else if (!strcmp (node.c_str(), "http://psi-im.org/caps"))
            node = "Psi";
        else if (!strcmp (node.c_str(), "http://miranda-im.org/caps"))
            node = "Miranda";
        else if (!strcmp (node.c_str(), "apple:ichat:caps") || !strcmp (node.c_str(), "http://www.apple.com/ichat/caps"))
            node = "iChat";
        else if (!strcmp (node.c_str(), "http://telepathy.freedesktop.org/caps"))
            node = "Telepathy";
        else if (!strcmp (node.c_str(), "http://talkgadget.google.com/client/caps"))
            node = "TalkGadget";
        else if (!strcmp (node.c_str(), "http://trillian.im/caps"))
            node = "Trillian";
        s_repl (&contr->cap_string, ext.c_str ());
        s_repl (&contr->version, s_sprintf ("%s %s", node.c_str(), ver.c_str()));
        DropAttrib (caps, "ver");
        DropAttrib (caps, "ext");
        DropAttrib (caps, "node");
        CheckInvalid (caps);
    }
}

void CLIMMXMPP::handleXEP136 (gloox::Tag *t)
{
    if (gloox::Tag *arc = findNamespacedChild (t, "record", "http://jabber.org/protocol/archive"))
    {
        DropAttrib (arc, "otr");
        CheckInvalid (arc);
    }
}

void CLIMMXMPP::handleXEP153 (gloox::Tag *t, Contact *contr)
{
    if (gloox::Tag *vcard = findNamespacedChild (t, "x", "vcard-temp:x:update"))
    {
        if (gloox::Tag *photo = vcard->findChild ("photo"))
        {
            DropCData (photo);
            CheckInvalid (photo);
        }
        if (gloox::Tag *nick = vcard->findChild ("nickname"))
        {
            std::string nickname = nick->cdata();
            ContactAddAlias (contr, nickname.c_str());
            DropCData (nick);
            CheckInvalid (nick);
        }
        CheckInvalid (vcard);
    }
}

void CLIMMXMPP::handleGoogleNosave (gloox::Tag *t)
{
    if (gloox::Tag *nosave = findNamespacedChild (t, "x", "google:nosave"))
    {
        DropAttrib (nosave, "value");
        CheckInvalid (nosave);
    }
}

void CLIMMXMPP::handleGoogleSig (gloox::Tag *t)
{
    if (gloox::Tag *sig = findNamespacedChild (t, "google-mail-signature", "google:metadata"))
    {
        DropCData (sig);
        CheckInvalid (sig);
    }
}

void CLIMMXMPP::handleGoogleChatstate(gloox::Tag *t)
{
    if (gloox::Tag *chat = findNamespacedChild (t, "active", "http://jabber.org/protocol/chatstates"))
        CheckInvalid (chat);
}


#if defined(LIBGLOOX_VERSION) && LIBGLOOX_VERSION >= 0x000900
void CLIMMXMPP::handleMessage (gloox::Stanza *s, gloox::MessageSession *session)
#else
void CLIMMXMPP::handleMessage (gloox::Stanza *s)
#endif
{
    assert (s);
    assert (s->type() == gloox::StanzaMessage);
    
#if defined(LIBGLOOX_VERSION) && LIBGLOOX_VERSION >= 0x000900
    gloox::Stanza *t = new gloox::Stanza (s);
#else
    gloox::Stanza *t = s->clone();
#endif

    if (t->subtype() == gloox::StanzaMessageError)
        DropAllChildsTree (t, "");
    else
        handleMessage2 (t, s->from(), s->to().full(), s->id(), s->subtype ());

    DropAttrib (t, "from");
    DropAttrib (t, "to");
    DropAttrib (t, "id");
    DropAttrib (t, "xml:lang");
    if (t->hasAttribute ("xmlns", "jabber:client"))
        DropAttrib (t, "xmlns");
    if (!CheckInvalid (t))
    {
        std::string txml = t->xml();
        CLIMMXMPPSave (m_serv, txml.c_str(), 3);
    }
    delete t;
}

void CLIMMXMPP::handlePresence (gloox::Stanza *s)
{
    assert (s);
    assert (s->type() == gloox::StanzaPresence);

#if defined(LIBGLOOX_VERSION) && LIBGLOOX_VERSION >= 0x000900
    gloox::Stanza *t = new gloox::Stanza(s);
#else
    gloox::Stanza *t = s->clone();
#endif

    if (t->subtype() == gloox::StanzaPresenceError)
        DropAllChildsTree (t, "");
    else
        handlePresence2 (t, s->from(), s->to(), s->status());

    DropAttrib (t, "from");
    DropAttrib (t, "to");
    DropAttrib (t, "id");
    DropAttrib (t, "xml:lang");
    if (t->hasAttribute ("xmlns", "jabber:client"))
        DropAttrib (t, "xmlns");
    DropAllChilds (t, "status");

    if (!CheckInvalid (t))
    {
        std::string txml = t->xml();
        CLIMMXMPPSave (m_serv, txml.c_str(), 3);
    }
    delete t;
}

void CLIMMXMPP::handleSubscription (gloox::Stanza *s)
{
    assert (s);
    assert (s->type() == gloox::StanzaS10n);
    rl_printf ("handleSubscription from:<%s> to:<%s> type:<%d> id:<%s> status:<%s> prio:<%d> body:<%s> subj:<%s> thread:<%s> pres:<%d> xml:<%s>\n",
               s->from().full().c_str(), s->to().full().c_str(), s->subtype(),
               s->id().c_str(), s->status().c_str(), s->priority(),
               s->body().c_str(), s->subject().c_str(),
#if defined(LIBGLOOX_VERSION) && LIBGLOOX_VERSION >= 0x000900
               s->thread().c_str(), s->presence(),
#else
               s->thread().c_str(), s->show(),
#endif
               s->xml().c_str());
}

void CLIMMXMPP::handleLog (gloox::LogLevel level, gloox::LogArea area, const std::string &message)
{
    const char *lt = "";
    const char *la = "";
    switch (area)
    {
        case gloox::LogAreaClassParser: la = "parser"; break;
#if defined(LIBGLOOX_VERSION) && LIBGLOOX_VERSION < 0x000900
        case gloox::LogAreaClassConnection: la = "conn"; break;
#endif
        case gloox::LogAreaClassClient: la = "client"; break;
        case gloox::LogAreaClassClientbase: la = "clbase"; break;
        case gloox::LogAreaClassComponent: la = "comp"; break;
        case gloox::LogAreaClassDns: la = "dns"; break;
        case gloox::LogAreaXmlIncoming: la = "xmlin"; break;
        case gloox::LogAreaXmlOutgoing: la = "xmlout"; break;
        case gloox::LogAreaUser: la = "user"; break;
    }
    switch (level)
    {
        case gloox::LogLevelDebug: lt = "debug"; break;
        case gloox::LogLevelWarning: lt = "warn"; break;
        case gloox::LogLevelError: lt = "error"; break;
    }
    if (area == gloox::LogAreaXmlIncoming)
    {
        if (ConnectionPrefVal (m_serv, CO_LOGSTREAM))
            CLIMMXMPPSave (m_serv, message.c_str(), 1);
        DebugH (DEB_XMPPIN, "%s/%s: %s", lt, la, message.c_str());
    }
    else if (area == gloox::LogAreaXmlOutgoing)
    {
        if (ConnectionPrefVal (m_serv, CO_LOGSTREAM))
            CLIMMXMPPSave (m_serv, message.c_str(), 0);
        DebugH (DEB_XMPPOUT, "%s/%s: %s", lt, la, message.c_str());
    }
    else
        DebugH (DEB_XMPPOTHER, "%s/%s: %s", lt, la, message.c_str());
}

/***************** Time Query *******************/

void CLIMMXMPP::sendIqTime (void)
{
    gloox::Tag *iq = new gloox::Tag ("iq");
    iq->addAttribute ("type", "get");
    iq->addAttribute ("to", m_client->jid().bare ());
    iq->addAttribute ("id", s_sprintf ("time-%s-%x", m_stamp, m_serv->conn->our_seq++));
    gloox::Tag *qq = new gloox::Tag (iq, "time");
    qq->addAttribute ("xmlns", "urn:xmpp:time");
    m_client->send (iq);
}

static void sendIqTimeReqs (Event *event)
{
    if (!event->conn)
    {
        EventD (event);
        return;
    }
    CLIMMXMPP *j = getXMPPClient (event->conn->serv);
    assert (j);
    j->sendIqTime ();
    event->due += 300;
    QueueEnqueue (event);
}

/****************** GoogleMail ******************/

void CLIMMXMPP::sendIqGmail (int64_t newer, std::string newertid, std::string q, bool isauto)
{
    gloox::Tag *iq = new gloox::Tag ("iq");
    iq->addAttribute ("type", "get");
    iq->addAttribute ("from", m_client->jid().full ());
    iq->addAttribute ("to", m_client->jid().bare ());
    iq->addAttribute ("id", s_sprintf ("%s-%s-%x", isauto ? "mail" : "mailq", m_stamp, m_serv->conn->our_seq++));
    if (newer == 1000ULL)
    {
        newer = gmail_newer;
        newertid = gmail_newertid;
        q = gmail_query;
    }
    if (isauto)
    {
        newer = gmail_new_newer;
        newertid = gmail_new_newertid;
    }
    else
        gmail_query = q;
    gloox::Tag *qq = new gloox::Tag (iq, "query");
    qq->addAttribute ("xmlns", "google:mail:notify");
    if (newer != 0)
        qq->addAttribute ("newer-than-time", s_sprintf ("%llu", newer));
    if (*newertid.c_str ())
        qq->addAttribute ("newer-than-tid", newertid);
    if (*q.c_str ())
        qq->addAttribute ("q", q);
    m_client->send (iq);
}

static void sendIqGmailReqs (Event *event)
{
    if (!event->conn)
    {
        EventD (event);
        return;
    }
    CLIMMXMPP *j = getXMPPClient (event->conn->serv);
    assert (j);
    j->sendIqGmail ((event->due - 300ULL)*1000ULL, "", "", 1);
    event->due += 300;
    QueueEnqueue (event);
}

/****************** IqHandler **********/

bool CLIMMXMPP::handleIq (gloox::Stanza *stanza)
{
    if (stanza->subtype() == gloox::StanzaIqError)
        DropAllChildsTree (stanza, "");
    
    if (gloox::Tag *mb = findNamespacedChild (stanza, "mailbox", "google:mail:notify"))
    {
        int n = 0;
        int ismail = 0;
        if (mb->hasAttribute ("total-matched"))
        {
            std::string a = mb->findAttribute ("total-matched");
            n = atoi (a.c_str());
        }
        std::string from = stanza->from().bare();
        std::string id = stanza->id();
        if (!strncmp (id.c_str (), "mail-", 5))
        {
            ismail = 1;
            if (n || !strcmp (id.c_str () + strlen (id.c_str()) - 2, "-0"))
                rl_printf (i18n (2738, "Found %d new mails for %s.\n"), n, from.c_str());
        }
        else
            rl_printf (i18n (2739, "Found %d mails for %s.\n"), n, from.c_str());
        if (!n)
            return true;
        gloox::Tag::TagList ml = mb->children ();
        gloox::Tag::TagList::iterator mlit = ml.begin();
        std::string ntid;
        Contact *cont = m_serv->conn->cont;
        for ( ; mlit != ml.end(); mlit++)
        {
            if ((*mlit)->name() != "mail-thread-info")
                continue;
            std::string sub = (*mlit)->findChild ("subject")->cdata();
            std::string snip = (*mlit)->findChild ("snippet")->cdata();
            std::string dato = (*mlit)->findAttribute ("date");
            ntid = (*mlit)->findAttribute ("tid");
            time_t t = atoll (dato.c_str ()) / 1000ULL;
            rl_printf ("%s ", s_time (&t));
            rl_printf ("%s%s %s%s%s", COLMESSAGE, sub.c_str (), COLQUOTE, COLSINGLE, snip.c_str ());
            rl_print ("\n");
            gloox::Tag::TagList mls = (*mlit)->findChild ("senders")->children ();
            gloox::Tag::TagList::iterator mlsit = mls.begin ();
            for ( ; mlsit !=mls.end(); mlsit++)
            {
                if ((*mlsit)->name() != "sender")
                    continue;
                if (!ismail || (*mlsit)->hasAttribute ("unread", "1"))
                {
                    std::string email = (*mlsit)->findAttribute ("address");
                    std::string name = (*mlsit)->findAttribute ("name");
                    rl_printf ("            %s%s%s <%s%s%s>\n", COLQUOTE, name.c_str(), COLNONE, COLCONTACT, email.c_str(), COLNONE);
                }
            }
        }
        std::string foundnewer = mb->findAttribute ("result-time");
        if (ismail)
        {
            gmail_new_newer = atoll (foundnewer.c_str ());
            gmail_new_newertid = ntid;
        }
        else
        {
            gmail_newer = atoll (foundnewer.c_str ());
            gmail_newertid = ntid;
        }
        return true;
    }
    CLIMMXMPPSave (m_serv, stanza->xml().c_str(), 3);
    return 0;
}

bool CLIMMXMPP::handleIqID (gloox::Stanza *stanza, int context)
{
    return 0;
}

/****************** DiscoHandler ****************/

void CLIMMXMPP::handleDiscoInfoResult (gloox::Stanza *stanza, int context)
{
    if (stanza->from().server() == m_client->jid().server())
    {
        gloox::Tag *id = stanza->findChild ("query");
        if (id)
        {
            if (id->hasChild ("feature", "var", "google:mail:notify"))
            {
                sendIqGmail ();
                QueueEnqueueData2 (m_serv->conn, QUEUE_XMPP_GMAIL, 0, 300, NULL, &sendIqGmailReqs, NULL);
                m_client->registerIqHandler (this, "google:mail:notify");
            }
            return;
        }
    }
    CLIMMXMPPSave (m_serv, stanza->xml().c_str(), 3);
}

void CLIMMXMPP::handleDiscoItemsResult (gloox::Stanza *stanza, int context)
{
    CLIMMXMPPSave (m_serv, stanza->xml().c_str(), 3);
}

void CLIMMXMPP::handleDiscoError (gloox::Stanza *stanza, int context)
{
    CLIMMXMPPSave (m_serv, stanza->xml().c_str(), 3);
}

bool CLIMMXMPP::handleDiscoSet (gloox::Stanza *stanza)
{
    CLIMMXMPPSave (m_serv, stanza->xml().c_str(), 3);
    return 0;
}

/**************/



void CLIMMXMPP::XMPPAuthorize (Server *serv, Contact *cont, auth_t how, const char *msg)
{
    gloox::JID j = gloox::JID (cont->screen);
    gloox::Tag *pres = new gloox::Tag ("presence");
    std::string res = m_client->resource();
    pres->addAttribute ("from", s_sprintf ("%s/%s", serv->screen, res.c_str()));
    while (cont->parent && cont->parent->serv == cont->serv)
        cont = cont->parent;
    pres->addAttribute ("to", cont->screen);
    pres->addAttribute ("type", how == auth_grant ? "subscribed"
                              : how == auth_deny  ? "unsubscribed"
                              : how == auth_req   ? "subscribe"
                                                  : "unsubscribe");
    if (msg)
        new gloox::Tag (pres, "status", msg);
    m_client->send (pres);
}

UBYTE XMPPSendmsg (Server *serv, Contact *cont, Message *msg)
{
    CLIMMXMPP *j = getXMPPClient (serv);
    assert (j);
    return j->XMPPSendmsg (serv, cont, msg);
}

void XMPPAuthorize (Server *serv, Contact *cont, auth_t how, const char *msg)
{
    CLIMMXMPP *j = getXMPPClient (serv);
    assert (j);
    assert (cont);
    j->XMPPAuthorize (serv, cont, how, msg);
}

void  XMPPGoogleMail (Server *serv, time_t since, const char *query)
{
    CLIMMXMPP *j = getXMPPClient (serv);
    assert (j);
    assert (query);
    j->sendIqGmail (since * 1000ULL, "", query, 0);
}

#endif
