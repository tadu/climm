
extern "C" {
#include "micq.h"
#include <sys/types.h>
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
#include <fcntl.h>
#include "jabber_base.h"
#include "connection.h"
#include "contact.h"
#include "util_io.h"
#include "im_response.h"
#include "im_request.h"
#include "util_ui.h"
#include "buildmark.h"
#include "preferences.h"
}

#include <cassert>
#include <gloox/gloox.h>
#include <gloox/client.h>
#include <gloox/connectionlistener.h>
#include <gloox/messagehandler.h>
#include <gloox/subscriptionhandler.h>
#include <gloox/presencehandler.h>
#include <gloox/stanza.h>
#include <gloox/disco.h>

extern "C" {
    static jump_conn_f XMPPCallbackDispatch;
    static jump_conn_f XMPPCallbackReconn;
    static jump_conn_err_f XMPPCallbackError;
    static jump_conn_f XMPPCallbackClose;
}

class MICQXMPP : public gloox::ConnectionListener, public gloox::MessageHandler,
                 public gloox::PresenceHandler,    public gloox::SubscriptionHandler,
                 public gloox::LogHandler {
    private :
        Connection *m_conn;
        gloox::Client *m_client;
        char *m_stamp;
    public :
                      MICQXMPP (Connection *serv);
        virtual       ~MICQXMPP ();
        virtual void  onConnect ();
        virtual void  onDisconnect (gloox::ConnectionError e);
        virtual void  onResourceBindError (gloox::ResourceBindError error);
        virtual void  onSessionCreateError (gloox::SessionCreateError error);
        virtual bool  onTLSConnect (const gloox::CertInfo &info);
        virtual void  handleMessage (gloox::Stanza *stanza);
        void handleMessage2 (gloox::Stanza *t, gloox::JID from, std::string tof, std::string id, gloox::StanzaSubType subtype);
        bool handleXEP22 (gloox::Tag *t, Contact *cfrom, gloox::JID from, std::string tof, std::string id);
        void handleXEP22a (gloox::Tag *XEP22, Contact *cfrom);
        void handleXEP22b (gloox::Tag *XEP22, gloox::JID from, std::string tof, std::string id);
        void handleXEP22c (gloox::JID from, std::string tof, std::string id, std::string type);
        void handleXEP71 (gloox::Tag *t);
        void handleXEP85 (gloox::Tag *t);
        void handleGoogleNosave (gloox::Tag *t);
        void handleXEP136 (gloox::Tag *t);
        time_t handleXEP91 (gloox::Tag *t);
        virtual void  handlePresence (gloox::Stanza *stanza);
        void handlePresence2 (gloox::Tag *s, gloox::JID from, gloox::JID to, std::string msg);
        virtual void  handleSubscription (gloox::Stanza *stanza);
        virtual void  handleLog (gloox::LogLevel level, gloox::LogArea area, const std::string &message);
        gloox::Client *getClient () { return m_client; }
        UBYTE XMPPSendmsg (Connection *conn, Contact *cont, Message *msg);
        void  XMPPSetstatus (Connection *serv, Contact *cont, status_t status, const char *msg);
        void  XMPPAuthorize (Connection *serv, Contact *cont, auth_t how, const char *msg);
};

MICQXMPP::MICQXMPP (Connection *serv)
{
    m_conn = serv;
    m_stamp = (char *)malloc (15);
    time_t now = time (NULL);
    strftime (m_stamp, 15, "%Y%m%d%H%M%S", gmtime (&now));
    assert (serv->passwd);
    assert (*serv->passwd);
    m_client = new gloox::Client (gloox::JID (serv->screen), serv->passwd, serv->port);
    m_client->setResource ("mICQ");
    if (serv->server)
        m_client->setServer (serv->server);

    m_client->disableRoster ();
    m_client->registerConnectionListener (this);
    m_client->registerMessageHandler (this);
    m_client->registerSubscriptionHandler (this);
    m_client->registerPresenceHandler (this);
    m_client->logInstance ().registerLogHandler (gloox::LogLevelDebug,   gloox::LogAreaAll, this);
    m_client->disco()->setVersion ("mICQ", BuildVersionStr, BuildPlatformStr);
    m_client->disco()->setIdentity ("client", "console");
    m_client->setAutoPresence (true);
    m_client->setInitialPriority (5);

    m_client->connect (false);
    
    serv->sok = m_client->fileDescriptor ();
}

MICQXMPP::~MICQXMPP ()
{
    s_free (m_stamp);
}

void MICQXMPP::onConnect ()
{
    m_conn->connect = CONNECT_OK | CONNECT_SELECT_R;
//    m_client->send (gloox::Stanza::createPresenceStanza (gloox::JID (""), "", gloox::PresenceChat));
}

void MICQXMPP::onDisconnect (gloox::ConnectionError e)
{
    m_conn->connect = 0;
    if (m_conn->sok != -1)
        close (m_conn->sok);
    m_conn->sok = -1;
    switch (e)
    {
        case gloox::ConnNoError:
            rl_printf ("onDisconnect: NoError.\n");
            break;
        case gloox::ConnStreamError:
            {
                std::string sET = m_client->streamErrorText();
                rl_printf ("onDisconnect: Error %d: ConnStreamError: Error %d: %s\n",
                           e, m_client->streamError(), sET.c_str());
            }
            break;
        case gloox::ConnStreamClosed:         rl_printf ("onDisconnect: Error StreamClosed %d.\n", e); break;
        case gloox::ConnIoError:              rl_printf ("onDisconnect: Error IoError %d.\n", e); break;
        case gloox::ConnOutOfMemory:          rl_printf ("onDisconnect: Error OutOfMemory %d.\n", e); break;
        case gloox::ConnNoSupportedAuth:      rl_printf ("onDisconnect: Error NoSupportedAuth %d.\n", e); break;
        case gloox::ConnTlsFailed:            rl_printf ("onDisconnect: Error TlsFailed %d.\n", e); break;
        case gloox::ConnAuthenticationFailed: rl_printf ("onDisconnect: Error AuthenticationFailed %d.\n", e);
            s_repl (&m_conn->passwd, NULL);
            break;
        case gloox::ConnUserDisconnected:     rl_printf ("onDisconnect: Error UserDisconnected %d.\n", e); break;
        case gloox::ConnNotConnected:         rl_printf ("onDisconnect: Error NotConnected %d.\n", e); break;
        default:
            rl_printf ("onDisconnect: Error %d.\n", e);
    }
}

void MICQXMPP::onResourceBindError (gloox::ResourceBindError e)
{
    rl_printf ("onResourceBindError: Error %d.\n", e);
}

void MICQXMPP::onSessionCreateError (gloox::SessionCreateError e)
{
    rl_printf ("onSessionCreateError: Error %d.\n", e);
}

bool MICQXMPP::onTLSConnect (const gloox::CertInfo &info)
{
    return TRUE;
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
    s->attributes().erase (a);
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

static void GetBothContacts (const gloox::JID &j, Connection *conn, Contact **b, Contact **f, bool crea)
{
    Contact *bb, *ff, **t;
    std::string jb = j.bare();
    std::string jr = j.resource();
    
    if ((bb = ContactFindScreen (conn->contacts, jb.c_str())))
    {
        if (!(ff = ContactFindScreenP (conn->contacts, bb, jr.c_str())))
        {
            ff = ContactScreenP (conn, bb, jr.c_str());
        }
    }
    else
    {
        bb = ContactScreen (conn, jb.c_str());
        if (crea)
            ContactCreate (conn, bb);
        ff = ContactScreenP (conn, bb, jr.c_str());
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


void MICQXMPP::handleXEP22a (gloox::Tag *XEP22, Contact *cfrom)
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
        Event *event = QueueDequeue (m_conn, QUEUE_XMPP_RESEND_ACK, ref);
        if (event)
        {
            Message *msg = (Message *)event->data;
            assert (msg);
            if (msg->send_message && !msg->otrinjected)
                IMIntMsg (cfrom, NOW, ims_offline, type, msg->plain_message ? msg->plain_message : msg->send_message);
            event->attempts += 5;
            QueueEnqueue (event);
        }
    }
    
    CheckInvalid (XEP22);
}

void MICQXMPP::handleXEP22c (gloox::JID from, std::string tof, std::string id, std::string type)
{
    gloox::Stanza *msg = gloox::Stanza::createMessageStanza (from, "");
    msg->addAttribute ("id", s_sprintf ("ack-%s-%x", m_stamp, m_conn->our_seq++));
    std::string res = m_client->resource();
    msg->addAttribute ("from", s_sprintf ("%s/%s", m_conn->screen, res.c_str()));
    gloox::Tag *x = new gloox::Tag (msg, "x");
    x->addAttribute ("xmlns", "jabber:x:event");
    new gloox::Tag (x, type);
    new gloox::Tag (x, "id", id);
    m_client->send (msg);
}

void MICQXMPP::handleXEP22b (gloox::Tag *XEP22, gloox::JID from, std::string tof, std::string id)
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

bool MICQXMPP::handleXEP22 (gloox::Tag *t, Contact *cfrom, gloox::JID from, std::string tof, std::string id)
{
    if (gloox::Tag *XEP22 = t->findChild ("x", "xmlns", "jabber:x:event"))
    {
        DropAttrib (XEP22, "xmlns");
        if (!t->hasChild ("body"))
        {
            handleXEP22a (XEP22, cfrom);
            return true;
        }
        handleXEP22b (XEP22, from, tof, id);
    }
    return false;
}

void MICQXMPP::handleXEP71 (gloox::Tag *t)
{
    if (gloox::Tag *xhtmlim = t->findChild ("html", "xmlns", "http://jabber.org/protocol/xhtml-im"))
    {
        DropAttrib (xhtmlim, "xmlns");
        DropAllChilds (xhtmlim, "body");
        CheckInvalid (xhtmlim);
    }
}

void MICQXMPP::handleXEP85 (gloox::Tag *t)
{
    if (gloox::Tag *active = t->findChild ("active", "xmlns", "http://jabber.org/protocol/chatstates"))
    {
        DropAttrib (active, "xmlns");
        CheckInvalid (active);
    }
    if (gloox::Tag *composing = t->findChild ("composing", "xmlns", "http://jabber.org/protocol/chatstates"))
    {
        DropAttrib (composing, "xmlns");
        CheckInvalid (composing);
    }
    if (gloox::Tag *paused = t->findChild ("paused", "xmlns", "http://jabber.org/protocol/chatstates"))
    {
        DropAttrib (paused, "xmlns");
        CheckInvalid (paused);
    }
    if (gloox::Tag *inactive = t->findChild ("inactive", "xmlns", "http://jabber.org/protocol/chatstates"))
    {
        DropAttrib (inactive, "xmlns");
        CheckInvalid (inactive);
    }
    if (gloox::Tag *gone = t->findChild ("gone", "xmlns", "http://jabber.org/protocol/chatstates"))
    {
        DropAttrib (gone, "xmlns");
        CheckInvalid (gone);
    }
}

time_t MICQXMPP::handleXEP91 (gloox::Tag *t)
{
    time_t date = NOW;
    if (gloox::Tag *delay = t->findChild ("x", "xmlns", "jabber:x:delay"))
    {
        struct tm;
        std::string dfrom = delay->findAttribute ("from");
        std::string stamp = delay->findAttribute ("stamp");
        date = ParseUTCDate (stamp);
        if (date != NOW)
            DropAttrib (delay, "stamp");
        DropAttrib (delay, "xmlns");
        DropAttrib (delay, "from");
        CheckInvalid (delay);
    }
    return date;
}

void MICQXMPP::handleGoogleNosave (gloox::Tag *t)
{
    if (gloox::Tag *nosave = t->findChild ("nos:x", "xmlns:nos", "google:nosave"))
    {
        DropAttrib (nosave, "xmlns:nos");
        DropAttrib (nosave, "value");
        CheckInvalid (nosave);
    }
}

void MICQXMPP::handleXEP136 (gloox::Tag *t)
{
    if (gloox::Tag *arc = t->findChild ("arc:record", "xmlns:arc", "http://jabber.org/protocol/archive"))
    {
        DropAttrib (arc, "xmlns:arc");
        DropAttrib (arc, "otr");
        CheckInvalid (arc);
    }
}

void MICQXMPP::handleMessage2 (gloox::Stanza *t, gloox::JID from, std::string tof, std::string id, gloox::StanzaSubType subtype)
{
    Contact *cto = ContactScreen (m_conn, tof.c_str());
    std::string subtypeval = t->findAttribute ("type");
    std::string body = t->body();
    std::string subject = t->subject();
    DropAttrib (t, "type");
    time_t delay;
    Contact *contb, *contr;
    
    GetBothContacts (from, m_conn, &contb, &contr, 0);

    handleXEP71 (t);
    handleXEP85 (t);
    handleGoogleNosave (t);
    handleXEP136 (t);
    if (handleXEP22 (t, contr, from, tof, id))
        return;
    delay = handleXEP91 (t);

    Opt *opt = OptSetVals (NULL, CO_ORIGIN, CV_ORIGIN_v8, CO_MSGTYPE, MSG_NORM, CO_MSGTEXT, body.c_str(), 0);
    if (!subject.empty())
        opt = OptSetVals (opt, CO_MSGTYPE, MSG_NORM_SUBJ, CO_SUBJECT, subject.c_str(), 0);
    IMSrvMsgFat (contr, delay, opt);

    DropAllChilds (t, "body");
    DropAllChilds (t, "subject");
    if (gloox::Tag *x = t->findChild ("x"))
        CheckInvalid (x);
}

void MICQXMPP::handleMessage (gloox::Stanza *s)
{
    assert (s);
    assert (s->type() == gloox::StanzaMessage);
    
    gloox::Stanza *t = s->clone();
    handleMessage2 (t, s->from(), s->to().full(), s->id(), s->subtype ());

    DropAttrib (t, "from");
    DropAttrib (t, "to");
    DropAttrib (t, "id");
    if (t->hasAttribute ("xmlns", "jabber:client"))
        DropAttrib (t, "xmlns");
    if (!CheckInvalid (t))
    {
        std::string txml = t->xml();
        rl_printf ("handleMessage %s\n", txml.c_str());
    }
    delete t;
}

void MICQXMPP::handlePresence2 (gloox::Tag *s, gloox::JID from, gloox::JID to, std::string msg)
{
    ContactGroup *tcg;
    Contact *contb, *contr, *c;
    status_t status;
    std::string pri;
    time_t delay;

    GetBothContacts (from, m_conn, &contb, &contr, 1);
    
    if (gloox::Tag *priority = s->findChild ("priority"))
    {
        pri = priority->cdata();
        DropCData (priority);
        CheckInvalid (priority);
    }
    
    delay = handleXEP91 (s);
    
    // XEP-115
    if (gloox::Tag *caps = s->findChild ("c", "xmlns", "http://jabber.org/protocol/caps"))
    {
        std::string node = caps->findAttribute ("node");
        std::string ver = caps->findAttribute ("ver");
        std::string ext = caps->findAttribute ("ext");
        if (ext.empty())
            s_repl (&contr->version, s_sprintf ("%s (%s)", node.c_str(), ver.c_str()));
        else
            s_repl (&contr->version, s_sprintf ("%s (%s) [%s]", node.c_str(), ver.c_str(), ext.c_str()));
        DropAttrib (caps, "xmlns");
        DropAttrib (caps, "ver");
        DropAttrib (caps, "ext");
        DropAttrib (caps, "node");
        CheckInvalid (caps);
    }
    else if (gloox::Tag *caps = s->findChild ("caps:c", "xmlns:caps", "http://jabber.org/protocol/caps"))
    {
        std::string node = caps->findAttribute ("node");
        std::string ver = caps->findAttribute ("ver");
        std::string ext = caps->findAttribute ("ext");
        if (ext.empty())
            s_repl (&contr->version, s_sprintf ("%s (%s)", node.c_str(), ver.c_str()));
        else
            s_repl (&contr->version, s_sprintf ("%s (%s) [%s]", node.c_str(), ver.c_str(), ext.c_str()));
        DropAttrib (caps, "xmlns:caps");
        DropAttrib (caps, "ver");
        DropAttrib (caps, "ext");
        DropAttrib (caps, "node");
        CheckInvalid (caps);
    }
    
    if (gloox::Tag *vcard = s->findChild ("x", "xmlns", "vcard-temp:x:update"))
    {
        // %%FIXME%%
        if (gloox::Tag *photo = vcard->findChild ("photo"))
        {
            DropCData (photo);
            CheckInvalid (photo);
        }
        DropAttrib (vcard, "xmlns");
        CheckInvalid (vcard);
    }

    if (s->hasAttribute ("type", "unavailable"))
    {
        status = ims_offline;
        DropAttrib (s, "type");
        
        IMOffline (contr);
    }
    else if (gloox::Tag *show = s->findChild ("show"))
    {
        if      (show->cdata() == "chat")  status = ims_ffc;
        else if (show->cdata() == "dnd")   status = ims_dnd;
        else if (show->cdata() == "xa")    status = ims_na;
        else if (show->cdata() == "away")  status = ims_away;
        else
            return;
        DropCData (show);
        CheckInvalid (show);
    }
    else
        status = ims_online;
    
    IMOnline (contr, status, imf_none, status, NULL);
}

void MICQXMPP::handlePresence (gloox::Stanza *s)
{
    assert (s);
    assert (s->type() == gloox::StanzaPresence);
    
    gloox::Stanza *t = s->clone();
    handlePresence2 (t, s->from(), s->to(), s->status());

    DropAttrib (t, "from");
    DropAttrib (t, "to");
    DropAttrib (t, "id");
    if (t->hasAttribute ("xmlns", "jabber:client"))
        DropAttrib (t, "xmlns");
    DropAllChilds (t, "status");
    if (!CheckInvalid (t))
    {
        std::string txml = t->xml();
        rl_printf ("handlePresence %s\n", txml.c_str());
    }
    delete t;
}

void MICQXMPP::handleSubscription (gloox::Stanza *s)
{
    assert (s);
    assert (s->type() == gloox::StanzaS10n);
    rl_printf ("handleSubscription from:<%s> to:<%s> type:<%d> id:<%s> status:<%s> prio:<%d> body:<%s> subj:<%s> thread:<%s> pres:<%d> xml:<%s>\n",
               s->from().full().c_str(), s->to().full().c_str(), s->subtype(),
               s->id().c_str(), s->status().c_str(), s->priority(),
               s->body().c_str(), s->subject().c_str(),
               s->thread().c_str(), s->show(),
               s->xml().c_str());
}

void MICQXMPPSave (Connection *serv, const char *text, bool in)
{
    const char *data;
    
    if (serv->logfd < 0)
    {
        const char *dir, *file;
        dir = s_sprintf ("%sdebug", PrefUserDir (prG));
        mkdir (dir, 0700);
        file = s_sprintf ("%sdebug" _OS_PATHSEPSTR "packets.xmpp.%s.%lu", PrefUserDir (prG), serv->screen, time (NULL));
        serv->logfd = open (file, O_WRONLY | O_CREAT | O_APPEND, 0600);
    }
    if (serv->logfd < 0)
        return;

    data = s_sprintf ("%s %s\n", s_now, in ? "<<<" : ">>>");
    write (serv->logfd, data, strlen (data));

    text = s_ind (text);
    write (serv->logfd, text, strlen (text));
    write (serv->logfd, "\n", 1);
}

void MICQXMPP::handleLog (gloox::LogLevel level, gloox::LogArea area, const std::string &message)
{
    const char *lt = "";
    const char *la = "";
    switch (area)
    {
        case gloox::LogAreaClassParser: la = "parser"; break;
        case gloox::LogAreaClassConnection: la = "conn"; break;
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
        if (ConnectionPrefVal (m_conn, CO_LOGSTREAM))
            MICQXMPPSave (m_conn, message.c_str(), 1);
        DebugH (DEB_XMPPIN, "%s/%s: %s", lt, la, message.c_str());
    }
    else if (area == gloox::LogAreaXmlOutgoing)
    {
        if (ConnectionPrefVal (m_conn, CO_LOGSTREAM))
            MICQXMPPSave (m_conn, message.c_str(), 0);
        DebugH (DEB_XMPPOUT, "%s/%s: %s", lt, la, message.c_str());
    }
    else
        DebugH (DEB_XMPPOTHER, "%s/%s: %s", lt, la, message.c_str());
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
            IMIntMsg (event->cont, NOW, ims_offline, INT_MSGACK_V8, msg->plain_message ? msg->plain_message : msg->send_message);
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

UBYTE MICQXMPP::XMPPSendmsg (Connection *serv, Contact *cont, Message *msg)
{
    assert (serv == m_conn);

    assert (cont);
    assert (msg->send_message);

    if (~m_conn->connect & CONNECT_OK)
        return RET_DEFER;
    if (msg->type != MSG_NORM)
        return RET_DEFER;

    gloox::Stanza *msgs = gloox::Stanza::createMessageStanza (gloox::JID (cont->screen), msg->send_message);
    msgs->addAttribute ("id", s_sprintf ("xmpp-%s-%x", m_stamp, ++m_conn->our_seq));
    
    std::string res = m_client->resource();
    msgs->addAttribute ("from", s_sprintf ("%s/%s", serv->screen, res.c_str()));
    gloox::Tag *x = new gloox::Tag (msgs, "x");
    x->addAttribute ("xmlns", "jabber:x:event");
    new gloox::Tag (x, "offline");
    new gloox::Tag (x, "delivered");
    new gloox::Tag (x, "displayed");
    new gloox::Tag (x, "composing");
    m_client->send (msgs);

    Event *event = QueueEnqueueData2 (serv, QUEUE_XMPP_RESEND_ACK, m_conn->our_seq, 120, msg, &SnacCallbackXmpp, &SnacCallbackXmppCancel);
    event->cont = cont;

    return RET_OK;
}

void MICQXMPP::XMPPSetstatus (Connection *serv, Contact *cont, status_t status, const char *msg)
{
    gloox::Presence p;
    gloox::JID j = cont ? gloox::JID (cont->screen) : gloox::JID ();
    
    switch (status)
    {
        case ims_online:   msg = NULL;
                           p = gloox::PresenceAvailable;   break;
        case ims_ffc:      p = gloox::PresenceChat;        break;
        case ims_away:     p = gloox::PresenceAway;        break;
        case ims_occ:      status = ims_dnd;
        case ims_dnd:      p = gloox::PresenceDnd;         break;
        case ims_na:       p = gloox::PresenceXa;          break;
        case ims_offline:  status = ims_inv;
        default:           p = gloox::PresenceUnavailable; break;
    }
    gloox::Stanza *pres = gloox::Stanza::createPresenceStanza (j, msg ? msg : "", p);
    if (p != gloox::PresenceUnavailable)
    {
        gloox::Tag *vers = new gloox::Tag (pres, "c");
        vers->addAttribute ("xmlns", "http://jabber.org/protocol/caps");
        vers->addAttribute ("node", "http://www.mICQ.org/xmpp/caps");
        vers->addAttribute ("ver", BuildVersionStr);
        // vers->addAttribute ("ext", "ext1 ext2");
    }
    m_client->send (pres);
    m_conn->status = status;
    m_conn->nativestatus = p;
}

void MICQXMPP::XMPPAuthorize (Connection *serv, Contact *cont, auth_t how, const char *msg)
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
    
static void XMPPCallBackTimeout (Event *event)
{
    Connection *conn = event->conn;
    
    if (!conn)
    {
        EventD (event);
        return;
    }
    assert (conn->type == TYPE_XMPP_SERVER);
    if (~conn->connect & CONNECT_OK)
        rl_print ("# XMPP timeout\n");
    EventD (event);
}

Event *ConnectionInitXMPPServer (Connection *serv)
{
    const char *sp;
    Event *event;
    
    assert (serv->type == TYPE_XMPP_SERVER);
    
    if (!serv->screen || !serv->passwd)
        return NULL;
    if (!strchr (serv->screen, '@'))
    {
        const char *jid;
        if (!serv->server)
            serv->server = "jabber.com";
        jid = s_sprintf ("%s@%s", serv->screen, serv->server);
        s_repl (&serv->screen, jid);
    }

    sp = s_sprintf ("%s", s_wordquote (s_sprintf ("%s:%lu", serv->server, serv->port)));
    rl_printf (i18n (2620, "Opening XMPP connection for %s at %s...\n"),
        s_wordquote (serv->screen), sp);
    
    if (!serv->port)
        serv->port = -1UL;
    
    serv->open = &ConnectionInitXMPPServer;
    serv->reconnect = &XMPPCallbackReconn;
    serv->error = &XMPPCallbackError;
    serv->close = &XMPPCallbackClose;
    serv->dispatch = &XMPPCallbackDispatch;
    
    if ((event = QueueDequeue2 (serv, QUEUE_DEP_WAITLOGIN, 0, NULL)))
    {
        event->attempts++;
        event->due = time (NULL) + 10 * event->attempts + 10;
        event->callback = &XMPPCallBackTimeout;
        QueueEnqueue (event);
    }
    else
        event = QueueEnqueueData (serv, QUEUE_DEP_WAITLOGIN, 0, time (NULL) + 5,
                                  NULL, serv->cont, NULL, &XMPPCallBackTimeout);

    MICQXMPP *l = new MICQXMPP (serv);
    serv->xmpp_private = l;
    serv->connect |= CONNECT_SELECT_R;
    
    return event;
}

static inline MICQXMPP *getXMPPClient (Connection *conn)
{
    return conn ? (MICQXMPP *)conn->xmpp_private : NULL;
}

void XMPPCallbackDispatch (Connection *conn)
{
    if (conn->sok == -1)
        return;

    MICQXMPP *j = getXMPPClient (conn);
    if (!j)
    {
        rl_printf ("Avoid spinning.\n");
        conn->sok = -1;
        return;
    }
    
    j->getClient()->recv (0);
}

void XMPPCallbackReconn (Connection *conn)
{
    rl_printf ("XMPPCallbackReconn: %p\n", conn);
}

void XMPPCallbackClose (Connection *conn)
{
    rl_printf ("XMPPCallbackClose: %p %d\n", conn, conn->sok);
    
    if (conn->sok == -1)
        return;

    MICQXMPP *j = getXMPPClient (conn);
    if (!j)
    {
        rl_printf ("Avoid spinning.\n");
        conn->sok = -1;
        return;
    }
    
    j->getClient()->disconnect ();
    rl_printf ("Disconnected.\n");
    delete j;
    conn->xmpp_private = NULL;
}

BOOL XMPPCallbackError (Connection *conn, UDWORD rc, UDWORD flags)
{
    rl_printf ("XMPPCallbackError: %p %lu %lu\n", conn, rc, flags);
    return 0;
}

UBYTE XMPPSendmsg (Connection *serv, Contact *cont, Message *msg)
{
    MICQXMPP *j = getXMPPClient (serv);
    assert (j);
    return j->XMPPSendmsg (serv, cont, msg);
}

void XMPPSetstatus (Connection *serv, Contact *cont, status_t status, const char *msg)
{
    MICQXMPP *j = getXMPPClient (serv);
    assert (j);
    j->XMPPSetstatus (serv, cont, status, msg);
}

void XMPPAuthorize (Connection *serv, Contact *cont, auth_t how, const char *msg)
{
    MICQXMPP *j = getXMPPClient (serv);
    assert (j);
    assert (cont);
    j->XMPPAuthorize (serv, cont, how, msg);
}

