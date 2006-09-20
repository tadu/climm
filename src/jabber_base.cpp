
extern "C" {
#include "micq.h"
#include "jabber_base.h"
#include "connection.h"
#include "contact.h"
#include "util_io.h"
#include "icq_response.h"
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
    public :
                      MICQJabber (Connection *serv);
        virtual       ~MICQJabber ();
        virtual void  onConnect ();
        virtual void  onDisconnect (gloox::ConnectionError e);
        virtual void  onResourceBindError (gloox::ResourceBindError error);
        virtual void  onSessionCreateError (gloox::SessionCreateError error);
        virtual bool  onTLSConnect (const gloox::CertInfo &info);
        virtual void  handleMessage (gloox::Stanza *stanza);
        virtual void  handlePresence (gloox::Stanza *stanza);
        virtual void  handleSubscription (gloox::Stanza *stanza);
        virtual void  handleLog (gloox::LogLevel level, gloox::LogArea area, const std::string &message);
        gloox::Client *getClient () { return m_client; }
        UBYTE JabberSendmsg (Connection *conn, Contact *cont, const char *text, UDWORD type);
};

MICQJabber::MICQJabber (Connection *serv)
{
    m_conn = serv;
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
}

void MICQJabber::onConnect ()
{
    m_conn->connect = CONNECT_OK | CONNECT_SELECT_R;
    rl_printf ("Tralala connected.\n");
    m_client->send (gloox::Stanza::createPresenceStanza (gloox::JID (""), "", gloox::PresenceChat));
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

void MICQJabber::handleMessage (gloox::Stanza *s)
{
    assert (s);
    assert (s->type() == gloox::StanzaMessage);
    
    IMSrvMsg (ContactScreen (m_conn, s->from().bare().c_str()), m_conn, NOW,
      OptSetVals (NULL, CO_ORIGIN, CV_ORIGIN_v5, CO_MSGTYPE, MSG_NORM, CO_MSGTEXT, s->body().c_str(), 0));
    rl_printf ("handleMessage from:<%s> to:<%s> type:<%d> id:<%s> status:<%s> prio:<%d> body:<%s> subj:<%s> thread:<%s> pres:<%d> xml:<%s>\n",
               s->from().full().c_str(), s->to().full().c_str(), s->subtype(),
               s->id().c_str(), s->status().c_str(), s->priority(),
               s->body().c_str(), s->subject().c_str(),
               s->thread().c_str(), s->show(),
               s->xml().c_str());
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

void MICQJabber::handlePresence (gloox::Stanza *s)
{
    ContactGroup *tcg;
    Contact *contb, *contf, *c;
    status_t status;

    assert (s);
    assert (s->type() == gloox::StanzaPresence);
    
    GetBothContacts (s->from(), m_conn, &contb, &contf);
    
    switch (s->show())
    {
        default:
        case gloox::PresenceUnknown:
        case gloox::PresenceAvailable: status = ims_online; break;
        case gloox::PresenceChat:      status = ims_ffc;    break;
        case gloox::PresenceAway:      status = ims_away;   break;
        case gloox::PresenceDnd:       status = ims_dnd;    break;
        case gloox::PresenceXa:        status = ims_na;
    }
    
    switch (s->subtype())
    {
        case gloox::StanzaPresenceUnavailable:
            c = SomeOnlineRessourceExcept (m_conn, contb, contf);
            if (c)
            {
                IMOffline (contf, m_conn);
                contb->status = c->status;
                rl_printf ("Using %s %p %p %p.\n", c->screen, c, contb, contf);
            }
            else
                IMOffline (contb, m_conn);
            break;
        case gloox::StanzaPresenceAvailable:
            IMOnline (contf, m_conn, status);
            tcg = contb->group;
            contb->group = NULL;
            IMOnline (contb, m_conn, status);
            contb->group = tcg;
            break;
        case gloox::StanzaPresenceProbe:
        case gloox::StanzaPresenceError:
            break;
        default:
            assert (0);
    }

    rl_printf ("handlePresence from:<%s> to:<%s> type:<%d> id:<%s> status:<%s> prio:<%d> body:<%s> subj:<%s> thread:<%s> pres:<%d> xml:<%s>\n",
               s->from().full().c_str(), s->to().full().c_str(), s->subtype(),
               s->id().c_str(), s->status().c_str(), s->priority(),
               s->body().c_str(), s->subject().c_str(),
               s->thread().c_str(), s->show(),
               s->xml().c_str());
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
    rl_printf ("LogJabber: %s/%s: %s\n", lt, la, message.c_str());
}

UBYTE MICQJabber::JabberSendmsg (Connection *conn, Contact *cont, const char *text, UDWORD type)
{
    assert (conn == m_conn);

    if (!cont || !text)
        return RET_DEFER;
    if (~m_conn->connect & CONNECT_OK)
        return RET_DEFER;
    
    m_client->send (gloox::Stanza::createMessageStanza (gloox::JID (cont->screen), text));
}

Event *ConnectionInitJabberServer (Connection *serv)
{
    const char *sp;
    
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
    
    MICQJabber *l = new MICQJabber (serv);
    serv->jabber_private = l;
    serv->connect |= CONNECT_SELECT_R;
    
    return NULL;
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
    j->JabberSendmsg (serv, cont, text, type);
}


