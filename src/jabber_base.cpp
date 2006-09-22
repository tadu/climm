
extern "C" {
#include "micq.h"
#include "jabber_base.h"
#include "connection.h"
#include "contact.h"
#include "util_io.h"
#include "icq_response.h"
#include "util_ui.h"
}

#include <gloox/gloox.h>
#include <gloox/client.h>
#include <gloox/connectionlistener.h>
#include <gloox/messagehandler.h>
#include <gloox/subscriptionhandler.h>
#include <gloox/presencehandler.h>
#include <gloox/stanza.h>
#include <gloox/disco.h>

extern "C" {
    static jump_conn_f JabberCallbackDispatch;
    static jump_conn_f JabberCallbackReconn;
    static jump_conn_err_f JabberCallbackError;
    static jump_conn_f JabberCallbackClose;
}

class MICQJabber : public gloox::ConnectionListener, public gloox::MessageHandler,
                   public gloox::PresenceHandler,    public gloox::SubscriptionHandler,
                   public gloox::LogHandler {
    private :
        Connection *m_conn;
        gloox::Client *m_client;
        char *m_stamp;
    public :
                      MICQJabber (Connection *serv);
        virtual       ~MICQJabber ();
        virtual void  onConnect ();
        virtual void  onDisconnect (gloox::ConnectionError e);
        virtual void  onResourceBindError (gloox::ResourceBindError error);
        virtual void  onSessionCreateError (gloox::SessionCreateError error);
        virtual bool  onTLSConnect (const gloox::CertInfo &info);
        virtual void  handleMessage (gloox::Stanza *stanza);
        void handleMessage2 (gloox::Stanza *t, std::string fromf, std::string tof, std::string id, gloox::StanzaSubType subtype);
        bool handleJEP22 (gloox::Tag *t, Contact *cfrom, std::string fromf, std::string tof, std::string id);
        void handleJEP22a (gloox::Tag *JEP22, Contact *cfrom);
        void handleJEP22b (gloox::Tag *JEP22, std::string fromf, std::string tof, std::string id);
        void handleJEP22c (std::string fromf, std::string tof, std::string id, std::string type);
        void handleJEP85 (gloox::Tag *t);
        time_t handleJEP91 (gloox::Tag *t);
        virtual void  handlePresence (gloox::Stanza *stanza);
        void handlePresence2 (gloox::Tag *s, gloox::JID from, gloox::JID to, std::string msg);
        virtual void  handleSubscription (gloox::Stanza *stanza);
        virtual void  handleLog (gloox::LogLevel level, gloox::LogArea area, const std::string &message);
        gloox::Client *getClient () { return m_client; }
        UBYTE JabberSendmsg (Connection *conn, Contact *cont, const char *text, UDWORD type);
        void  JabberSetstatus (Connection *serv, Contact *cont, status_t status, const char *msg);
};

MICQJabber::MICQJabber (Connection *serv)
{
    m_conn = serv;
    m_stamp = (char *)malloc (15);
    time_t now = time (NULL);
    strftime (m_stamp, 15, "%Y%m%d%H%M%S", gmtime (&now));
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
    m_client->disco()->setVersion ("mICQ testing", "0.5.2");
    m_client->disco()->setIdentity ("client", "bot");
    m_client->setAutoPresence (true);
    m_client->setInitialPriority (5);

    m_client->connect (false);
    
    serv->sok = m_client->fileDescriptor ();
}

MICQJabber::~MICQJabber ()
{
    s_free (m_stamp);
}

void MICQJabber::onConnect ()
{
    m_conn->connect = CONNECT_OK | CONNECT_SELECT_R;
//    m_client->send (gloox::Stanza::createPresenceStanza (gloox::JID (""), "", gloox::PresenceChat));
}

void MICQJabber::onDisconnect (gloox::ConnectionError e)
{
    m_conn->connect = 0;
    if (m_conn->sok != -1)
        close (m_conn->sok);
    m_conn->sok = -1;
    switch (e)
    {
        case gloox::ConnNoError:
            break;
        case gloox::ConnStreamError:
            rl_printf ("onDisconnect: Error %d: ConnStreamError: Error %d: %s\n",
                       e, m_client->streamError(), m_client->streamErrorText().c_str());
            break;
        case gloox::ConnStreamClosed:
        case gloox::ConnIoError:
        case gloox::ConnOutOfMemory:
        case gloox::ConnNoSupportedAuth:
        case gloox::ConnTlsFailed:
        case gloox::ConnAuthenticationFailed:
        case gloox::ConnUserDisconnected:
        case gloox::ConnNotConnected:
        default:
            rl_printf ("onDisconnect: Error %d.\n", e);
    }
}

void MICQJabber::onResourceBindError (gloox::ResourceBindError e)
{
    rl_printf ("onResourceBindError: Error %d.\n", e);
}

void MICQJabber::onSessionCreateError (gloox::SessionCreateError e)
{
    rl_printf ("onSessionCreateError: Error %d.\n", e);
}

bool MICQJabber::onTLSConnect (const gloox::CertInfo &info)
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

void MICQJabber::handleJEP22a (gloox::Tag *JEP22, Contact *cfrom)
{
    std::string refid;
    
    if (gloox::Tag *tid = JEP22->findChild ("id"))
    {
        refid = tid->cdata();
        DropCData (tid);
        CheckInvalid (tid);
    }
    
    if (gloox::Tag *dotag = JEP22->findChild ("delivered"))
    {
        IMIntMsg (cfrom, m_conn, NOW, ims_offline, INT_MSGACK_V8, "", NULL);
        CheckInvalid (dotag);
    }
    else if (gloox::Tag *dotag = JEP22->findChild ("displayed"))
    {
        IMIntMsg (cfrom, m_conn, NOW, ims_offline, INT_MSGDISPL, "", NULL);
        CheckInvalid (dotag);
    }
    else if (gloox::Tag *dotag = JEP22->findChild ("composing"))
    {
        IMIntMsg (cfrom, m_conn, NOW, ims_offline, INT_MSGCOMP, "", NULL);
        CheckInvalid (dotag);
    }
    else if (gloox::Tag *dotag = JEP22->findChild ("offline"))
    {
        IMIntMsg (cfrom, m_conn, NOW, ims_offline, INT_MSGOFF, "", NULL);
        CheckInvalid (dotag);
    }
    else
        IMIntMsg (cfrom, m_conn, NOW, ims_offline, INT_MSGNOCOMP, "", NULL);
    CheckInvalid (JEP22);
}

void MICQJabber::handleJEP22c (std::string fromf, std::string tof, std::string id, std::string type)
{
    gloox::Stanza *msg = gloox::Stanza::createMessageStanza (gloox::JID (fromf), "");
    msg->addAttribute ("id", s_sprintf ("%s-%x", m_stamp, m_conn->our_seq++));
    msg->addAttribute ("from", s_sprintf ("%s/mICQ", m_conn->screen));
    gloox::Tag *x = new gloox::Tag (msg, "x");
    x->addAttribute ("xmlns", "jabber:x:event");
    new gloox::Tag (x, type);
    new gloox::Tag (x, "id", id);
    m_client->send (msg);
}

void MICQJabber::handleJEP22b (gloox::Tag *JEP22, std::string fromf, std::string tof, std::string id)
{
    if (gloox::Tag *dotag = JEP22->findChild ("delivered"))
    {
        handleJEP22c (fromf, tof, id, "delivered");
        CheckInvalid (dotag);
    }
    if (gloox::Tag *dotag = JEP22->findChild ("displayed"))
    {
        handleJEP22c (fromf, tof, id, "displayed");
        CheckInvalid (dotag);
    }
    if (gloox::Tag *dotag = JEP22->findChild ("composing"))
    {
        CheckInvalid (dotag);
    }
    if (gloox::Tag *dotag = JEP22->findChild ("offline"))
    {
        CheckInvalid (dotag);
    }
}

bool MICQJabber::handleJEP22 (gloox::Tag *t, Contact *cfrom, std::string fromf, std::string tof, std::string id)
{
    if (gloox::Tag *JEP22 = t->findChild ("x", "xmlns", "jabber:x:event"))
    {
        DropAttrib (JEP22, "xmlns");
        if (!t->hasChild ("body"))
        {
            handleJEP22a (JEP22, cfrom);
            return true;
        }
        handleJEP22b (JEP22, fromf, tof, id);
    }
    return false;
}

void MICQJabber::handleJEP85 (gloox::Tag *t)
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

time_t MICQJabber::handleJEP91 (gloox::Tag *t)
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

void MICQJabber::handleMessage2 (gloox::Stanza *t, std::string fromf, std::string tof, std::string id, gloox::StanzaSubType subtype)
{
    Contact *cfrom = ContactScreen (m_conn, fromf.c_str());
    Contact *cto = ContactScreen (m_conn, tof.c_str());
    std::string subtypeval = t->findAttribute ("type");
    std::string body = t->body();
    std::string subject = t->subject();
    DropAttrib (t, "type");
    time_t delay;

    handleJEP85 (t);
    if (handleJEP22 (t, cfrom, fromf, tof, id))
        return;
    delay = handleJEP91 (t);

    Opt *opt = OptSetVals (NULL, CO_ORIGIN, CV_ORIGIN_v8, CO_MSGTYPE, MSG_NORM, CO_MSGTEXT, body.c_str(), 0);
    if (!subject.empty())
        opt = OptSetVals (opt, CO_MSGTYPE, MSG_NORM_SUBJ, CO_SUBJECT, subject.c_str(), 0);
    IMSrvMsg (cfrom, m_conn, delay, opt);

    DropAllChilds (t, "body");
    DropAllChilds (t, "subject");
    if (gloox::Tag *x = t->findChild ("x"))
        CheckInvalid (x);
}

void MICQJabber::handleMessage (gloox::Stanza *s)
{
    assert (s);
    assert (s->type() == gloox::StanzaMessage);
    
    gloox::Stanza *t = s->clone();
    handleMessage2 (t, s->from().full(), s->to().full(), s->id(), s->subtype ());

    DropAttrib (t, "from");
    DropAttrib (t, "to");
    DropAttrib (t, "id");
    if (t->hasAttribute ("xmlns", "jabber:client"))
        DropAttrib (t, "xmlns");
    if (!CheckInvalid (t))
        rl_printf ("handleMessage %s\n", t->xml().c_str());
    delete t;
}

static void GetBothContacts (const gloox::JID &j, Connection *conn, Contact **b, Contact **f)
{
    Contact *bb, *ff;
    if ((bb = ContactFindScreen (conn->contacts, j.bare().c_str())))
    {
        if (!(ff = ContactFindScreen (conn->contacts, j.full().c_str())))
        {
            ff = ContactScreen (conn, j.full().c_str());
            ContactCreate (conn, ff);
        }
    }
    else
    {
        bb = ContactScreen (conn, j.bare().c_str());
        ff = ContactScreen (conn, j.full().c_str());
    }
    *b = bb;
    *f = ff;
}

static Contact *SomeOnlineRessourceExcept (Connection *conn, Contact *full, Contact *bare)
{
    Contact *c;
    int i;
    
    for (i = 0; (c = ContactIndex (conn->contacts, i)); i++)
        if (c != full && c != bare && c->status != ims_offline
            && !strncasecmp (c->screen, bare->screen, strlen (bare->screen)))
            return c;
    return NULL;
}

void MICQJabber::handlePresence2 (gloox::Tag *s, gloox::JID from, gloox::JID to, std::string msg)
{
    ContactGroup *tcg;
    Contact *contb, *contf, *c;
    status_t status;
    std::string pri;
    time_t delay;

    GetBothContacts (from, m_conn, &contb, &contf);
    
    if (gloox::Tag *priority = s->findChild ("priority"))
    {
        pri = priority->cdata();
        DropCData (priority);
        CheckInvalid (priority);
    }
    
    delay = handleJEP91 (s);
    
    // JEP-115
    if (gloox::Tag *caps = s->findChild ("c", "xmlns", "http://jabber.org/protocol/caps"))
    {
        std::string node = caps->findAttribute ("node");
        std::string ver = caps->findAttribute ("ver");
        std::string ext = caps->findAttribute ("ext");
        if (ext.empty())
            s_repl (&contf->version, s_sprintf ("%s (%s)", node.c_str(), ver.c_str()));
        else
            s_repl (&contf->version, s_sprintf ("%s (%s) [%s]", node.c_str(), ver.c_str(), ext.c_str()));
        DropAttrib (caps, "xmlns");
        DropAttrib (caps, "ver");
        DropAttrib (caps, "ext");
        DropAttrib (caps, "node");
        CheckInvalid (caps);
    }

    if (s->hasAttribute ("type", "unavailable"))
    {
        status = ims_offline;
        DropAttrib (s, "type");

        c = SomeOnlineRessourceExcept (m_conn, contb, contf);
        if (c)
        {
            IMOffline (contf, m_conn);
            contb->status = c->status;
            rl_printf ("Using %s %p %p %p.\n", c->screen, c, contb, contf);
        }
        else
            IMOffline (contb, m_conn);
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
    
    IMOnline (contf, m_conn, status, imf_none, status, msg.c_str());
    tcg = contb->group;
    contb->group = NULL;
    IMOnline (contb, m_conn, status, imf_none, status, NULL);
    contb->group = tcg;
}

void MICQJabber::handlePresence (gloox::Stanza *s)
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
        rl_printf ("handlePresence %s\n", t->xml().c_str());
    delete t;
}



void MICQJabber::handleSubscription (gloox::Stanza *s)
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

void MICQJabber::handleLog (gloox::LogLevel level, gloox::LogArea area, const std::string &message)
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
        DebugH (DEB_JABBERIN, "%s/%s: %s", lt, la, message.c_str());
    else if (area == gloox::LogAreaXmlOutgoing)
        DebugH (DEB_JABBEROUT, "%s/%s: %s", lt, la, message.c_str());
    else
        DebugH (DEB_JABBEROTHER, "%s/%s: %s", lt, la, message.c_str());
}

UBYTE MICQJabber::JabberSendmsg (Connection *conn, Contact *cont, const char *text, UDWORD type)
{
    assert (conn == m_conn);

    if (!cont || !text)
        return RET_DEFER;
    if (~m_conn->connect & CONNECT_OK)
        return RET_DEFER;

    gloox::Stanza *msg = gloox::Stanza::createMessageStanza (gloox::JID (cont->screen), text);    
    msg->addAttribute ("id", s_sprintf ("%s-%x", m_stamp, m_conn->our_seq++));
    msg->addAttribute ("from", s_sprintf ("%s/mICQ", conn->screen));
    gloox::Tag *x = new gloox::Tag (msg, "x");
    x->addAttribute ("xmlns", "jabber:x:event");
    new gloox::Tag (x, "offline");
    new gloox::Tag (x, "delivered");
    new gloox::Tag (x, "displayed");
    new gloox::Tag (x, "composing");
    m_client->send (msg);
    return RET_OK;
}

void MICQJabber::JabberSetstatus (Connection *serv, Contact *cont, status_t status, const char *msg)
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
        vers->addAttribute ("node", "http://www.mICQ.org/jabber/caps");
        vers->addAttribute ("vers", "0.5.2");
        // vers->addAttribute ("ext", "ext1 ext2");
    }
    m_client->send (pres);
    m_conn->status = status;
    m_conn->nativestatus = p;
}


static void JabberCallBackTimeout (Event *event)
{
    Connection *conn = event->conn;
    
    if (!conn)
    {
        EventD (event);
        return;
    }
    assert (conn->type == TYPE_JABBER_SERVER);
    if (~conn->connect & CONNECT_OK)
        rl_print ("# Jabber timeout\n");
    EventD (event);
}

Event *ConnectionInitJabberServer (Connection *serv)
{
    const char *sp;
    Event *event;
    
    assert (serv->type = TYPE_JABBER_SERVER);
    
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
    rl_printf (i18n (9999, "Opening Jabber connection for %s at %s...\n"),
        s_wordquote (serv->screen), sp);
    
    if (!serv->port)
        serv->port = -1UL;
    
    serv->open = &ConnectionInitJabberServer;
    serv->reconnect = &JabberCallbackReconn;
    serv->error = &JabberCallbackError;
    serv->close = &JabberCallbackClose;
    serv->dispatch = &JabberCallbackDispatch;
    
    if ((event = QueueDequeue2 (serv, QUEUE_DEP_WAITLOGIN, 0, NULL)))
    {
        event->attempts++;
        event->due = time (NULL) + 10 * event->attempts + 10;
        event->callback = &JabberCallBackTimeout;
        QueueEnqueue (event);
    }
    else
        event = QueueEnqueueData (serv, QUEUE_DEP_WAITLOGIN, 0, time (NULL) + 5,
                                  NULL, serv->cont, NULL, &JabberCallBackTimeout);

    MICQJabber *l = new MICQJabber (serv);
    serv->jabber_private = l;
    serv->connect |= CONNECT_SELECT_R;
    
    return event;
}

static inline MICQJabber *getJabberClient (Connection *conn)
{
    return conn ? (MICQJabber *)conn->jabber_private : NULL;
}

void JabberCallbackDispatch (Connection *conn)
{
    if (conn->sok == -1)
        return;

    MICQJabber *j = getJabberClient (conn);
    if (!j)
    {
        rl_printf ("Avoid spinning.\n");
        conn->sok = -1;
        return;
    }
    
    j->getClient()->recv (0);
}

void JabberCallbackReconn (Connection *conn)
{
    rl_printf ("JabberCallbackReconn: %p\n", conn);
}

void JabberCallbackClose (Connection *conn)
{
    rl_printf ("JabberCallbackClose: %p %d\n", conn, conn->sok);
    
    if (conn->sok == -1)
        return;

    MICQJabber *j = getJabberClient (conn);
    if (!j)
    {
        rl_printf ("Avoid spinning.\n");
        conn->sok = -1;
        return;
    }
    
    j->getClient()->disconnect ();
    delete j;
    conn->jabber_private = NULL;
}

BOOL JabberCallbackError (Connection *conn, UDWORD rc, UDWORD flags)
{
    rl_printf ("JabberCallbackError: %p %lu %lu\n", conn, rc, flags);
    return 0;
}

UBYTE JabberSendmsg (Connection *serv, Contact *cont, const char *text, UDWORD type)
{
    MICQJabber *j = getJabberClient (serv);
    assert (j);
    return j->JabberSendmsg (serv, cont, text, type);
}

void JabberSetstatus (Connection *serv, Contact *cont, status_t status, const char *msg)
{
    MICQJabber *j = getJabberClient (serv);
    assert (j);
    j->JabberSetstatus (serv, cont, status, msg);
}
