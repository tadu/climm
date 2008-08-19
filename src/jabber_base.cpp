
extern "C" {
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
}

#include <cassert>
#include <map>
#include <gloox/gloox.h>
#include <gloox/client.h>
#include <gloox/connectionlistener.h>
#include <gloox/disco.h>
#include <gloox/discohandler.h>
#include <gloox/messagehandler.h>
#include <gloox/subscriptionhandler.h>
#if defined(LIBGLOOX_VERSION) && LIBGLOOX_VERSION >= 0x000900
#include <gloox/connectiontcpbase.h>
#endif
#ifdef CLIMM_XMPP_FILE_TRANSFER
#include <gloox/socks5bytestream.h>
#include <gloox/siprofilefthandler.h>
#include <gloox/socks5bytestreamdatahandler.h>
#include <gloox/siprofileft.h>
#endif
#include <gloox/presencehandler.h>
#include <gloox/stanza.h>
#include <gloox/disco.h>

extern "C" {
    static jump_conn_f XMPPCallbackDispatch;
    static jump_conn_f XMPPCallbackReconn;
    static jump_conn_f XMPPCallbackClose;
    static jump_conn_err_f XMPPCallbackError;
    static void XMPPCallBackDoReconn (Event *event);
#ifdef CLIMM_XMPP_FILE_TRANSFER
    static jump_conn_f XMPPFTCallbackDispatch;
    static jump_conn_f XMPPFTCallbackClose;
    static jump_conn_err_f XMPPFTCallbackError;
    static void XMPPCallBackFileAccept (Event *event);
#endif
    //void PeerFileIODispatchClose (Connection *ffile);
}

#ifdef CLIMM_XMPP_FILE_TRANSFER
std::map<UWORD, std::string> l_tSeqTranslate;
std::map<const std::string, std::string> sid2id;
#endif

class CLIMMXMPP: public gloox::ConnectionListener, public gloox::MessageHandler,
                 public gloox::PresenceHandler,    public gloox::SubscriptionHandler,
                 public gloox::DiscoHandler,       public gloox::IqHandler,
#ifdef CLIMM_XMPP_FILE_TRANSFER
                 public gloox::SIProfileFTHandler,
                 public gloox::SOCKS5BytestreamDataHandler,
#endif

                 public gloox::LogHandler {
    private :
        Server *m_serv;
        gloox::Client *m_client;
        char *m_stamp;
#ifdef CLIMM_XMPP_FILE_TRANSFER
        gloox::SIProfileFT *m_pFT;
#endif

        void handleMessage2 (gloox::Stanza *t, gloox::JID from, std::string tof, std::string id, gloox::StanzaSubType subtype);
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
        void handlePresence2 (gloox::Tag *s, gloox::JID from, gloox::JID to, std::string msg);

    public :
                      CLIMMXMPP (Server *serv);
        virtual       ~CLIMMXMPP ();
        virtual void  onConnect ();
        virtual void  onDisconnect (gloox::ConnectionError e);
        virtual void  onResourceBindError (gloox::ResourceBindError error);
        virtual void  onSessionCreateError (gloox::SessionCreateError error);
        virtual bool  onTLSConnect (const gloox::CertInfo &info);
#if defined(LIBGLOOX_VERSION) && LIBGLOOX_VERSION >= 0x000900
        virtual void  handleMessage (gloox::Stanza *stanza, gloox::MessageSession *session = NULL);
#else
        virtual void  handleMessage (gloox::Stanza *stanza);
#endif
#ifdef CLIMM_XMPP_FILE_TRANSFER
        virtual void  handleFTRequestError (gloox::Stanza *stanza);
        virtual void  handleFTSOCKS5Bytestream (gloox::SOCKS5Bytestream *s5b);
        virtual void handleFTRequest (const gloox::JID & from, const std::string & id, const std::string & sid,
                const std::string & name, long size, const std::string & hash, const std::string & date,
                const std::string & mimetype, const std::string & desc, int stypes, long offset, long length);
        void XMPPAcceptDenyFT (gloox::JID contact, std::string id, std::string sid, const char *reason);
        virtual void handleFTRequestError(gloox::Stanza*, const std::string&)  {}
        virtual void handleSOCKS5Data (gloox::SOCKS5Bytestream *s5b, const std::string &data);
        virtual void handleSOCKS5Error (gloox::SOCKS5Bytestream *s5b, gloox::Stanza *stanza);
        virtual void handleSOCKS5Open (gloox::SOCKS5Bytestream *s5b);
        virtual void handleSOCKS5Close (gloox::SOCKS5Bytestream *s5b);
#endif
        virtual void  handlePresence (gloox::Stanza *stanza);
        virtual void  handleSubscription (gloox::Stanza *stanza);
        virtual void  handleLog (gloox::LogLevel level, gloox::LogArea area, const std::string &message);

        // GMail support
        void sendIqGmail (int64_t newer = 0ULL, std::string newertid = "", std::string q = "", bool isauto = 1);
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

        gloox::Client *getClient () { return m_client; }
        UBYTE XMPPSendmsg (Server *conn, Contact *cont, Message *msg);
        void  XMPPSetstatus (Server *serv, Contact *cont, status_t status, const char *msg);
        void  XMPPAuthorize (Server *serv, Contact *cont, auth_t how, const char *msg);
};

static inline CLIMMXMPP *getXMPPClient (Server *serv)
{
    return serv ? (CLIMMXMPP *)serv->xmpp_private : NULL;
}

CLIMMXMPP::CLIMMXMPP (Server *serv)
{
    m_serv = serv;
    m_stamp = (char *)malloc (15);
    time_t now = time (NULL);
    strftime (m_stamp, 15, "%Y%m%d%H%M%S", gmtime (&now));
    assert (serv->passwd);
    assert (*serv->passwd);
    m_client = new gloox::Client (gloox::JID (serv->screen), serv->passwd, serv->conn->port);
    m_client->setResource ("climm");
    if (serv->conn->server)
        m_client->setServer (serv->conn->server);

    m_client->disableRoster ();
    m_client->registerConnectionListener (this);
    m_client->registerMessageHandler (this);
    m_client->registerSubscriptionHandler (this);
    m_client->registerPresenceHandler (this);
    m_client->logInstance ().registerLogHandler (gloox::LogLevelDebug,   gloox::LogAreaAll, this);
    m_client->disco()->setVersion ("climm", BuildVersionStr, BuildPlatformStr);
    m_client->disco()->setIdentity ("client", "console");
    m_client->disco()->registerDiscoHandler (this);
#if defined(LIBGLOOX_VERSION) && LIBGLOOX_VERSION >= 0x000900
    m_client->setPresence (gloox::PresenceAvailable, 5);

#ifdef CLIMM_XMPP_FILE_TRANSFER
    m_pFT = new gloox::SIProfileFT (m_client, this);
    //m_pFT->addStreamHost (gloox::JID ("proxy.jabber.org"), "208.245.212.98", 7777);
#endif

#else
    m_client->setAutoPresence (true);
    m_client->setInitialPriority (5);
#endif

    m_client->connect (false);
#if defined(LIBGLOOX_VERSION) && LIBGLOOX_VERSION >= 0x000900
    // Yes http proxy is now avail in gloox, but not used in climm, so != NULL
    serv->conn->sok = dynamic_cast<gloox::ConnectionTCPBase *>(m_client->connectionImpl())->socket();
#else
    serv->conn->sok = m_client->fileDescriptor ();
#endif
}

CLIMMXMPP::~CLIMMXMPP ()
{
    s_free (m_stamp);
}

void CLIMMXMPP::onConnect ()
{
    m_serv->conn->connect = CONNECT_OK | CONNECT_SELECT_R;
    m_client->disco()->getDiscoInfo (m_client->jid().server(), "", this, 0);
//    m_client->send (gloox::Stanza::createPresenceStanza (gloox::JID (""), "", gloox::PresenceChat));
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

void CLIMMXMPP::onResourceBindError (gloox::ResourceBindError e)
{
    rl_printf ("#onResourceBindError: Error %d.\n", e);
}

void CLIMMXMPP::onSessionCreateError (gloox::SessionCreateError e)
{
    rl_printf ("#onSessionCreateError: Error %d.\n", e);
}

bool CLIMMXMPP::onTLSConnect (const gloox::CertInfo &info)
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

static void GetBothContacts (const gloox::JID &j, Server *conn, Contact **b, Contact **f, bool crea)
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


void CLIMMXMPPSave (Server *serv, const char *text, char in)
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

    data = s_sprintf ("%s %s%s\n", s_now, in & 1 ? "<<<" : ">>>", in & 2 ? " residual:" : "");
    write (serv->logfd, data, strlen (data));

    text = s_ind (text);
    write (serv->logfd, text, strlen (text));
    write (serv->logfd, "\n", 1);
}


void CLIMMXMPP::handleXEP8 (gloox::Tag *t)
{
    if (gloox::Tag *avatar = t->findChild ("x", "xmlns", "jabber:x:avatar"))
    {
        if (gloox::Tag *hash = avatar->findChild ("hash"))
        {
            DropCData (hash);
            CheckInvalid (hash);
        }
        DropAttrib (avatar, "xmlns");
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
    if (gloox::Tag *XEP22 = t->findChild ("x", "xmlns", "jabber:x:event"))
    {
        ret = true;
        DropAttrib (XEP22, "xmlns");
        if (!t->hasChild ("body"))
            handleXEP22a (XEP22, cfrom);
        else
            handleXEP22b (XEP22, from, tof, id);
    }
    if (gloox::Tag *address = t->findChild ("addresses", "xmlns", "http://jabber.org/protocol/address"))
    {
        DropAttrib (address, "xmlns");
        DropAllChildsTree (address, "address");
        CheckInvalid (address);
        ret = true;
    }
    
    if (gloox::Tag *active = t->findChild ("active", "xmlns", "http://jabber.org/protocol/chatstates"))
    {
        DropAttrib (active, "xmlns");
        CheckInvalid (active);
        ret = true;
    }
    if (gloox::Tag *composing = t->findChild ("composing", "xmlns", "http://jabber.org/protocol/chatstates"))
    {
        DropAttrib (composing, "xmlns");
        CheckInvalid (composing);
        IMIntMsg (cfrom, NOW, ims_offline, INT_MSGCOMP, "");
        ret = true;
    }
    if (gloox::Tag *paused = t->findChild ("paused", "xmlns", "http://jabber.org/protocol/chatstates"))
    {
        DropAttrib (paused, "xmlns");
        CheckInvalid (paused);
        IMIntMsg (cfrom, NOW, ims_offline, INT_MSGNOCOMP, "");
        ret = true;
    }
    if (gloox::Tag *inactive = t->findChild ("inactive", "xmlns", "http://jabber.org/protocol/chatstates"))
    {
        DropAttrib (inactive, "xmlns");
        CheckInvalid (inactive);
        ret = true;
    }
    if (gloox::Tag *gone = t->findChild ("gone", "xmlns", "http://jabber.org/protocol/chatstates"))
    {
        DropAttrib (gone, "xmlns");
        CheckInvalid (gone);
        ret = true;
    }
    if (ret && !t->hasChild ("body"))
        return true;
    return false;
}

void CLIMMXMPP::handleXEP27 (gloox::Tag *t)
{
    if (gloox::Tag *sig = t->findChild ("x", "xmlns", "jabber:x:signed"))
    {
        DropCData (sig);
        DropAttrib (sig, "xmlns");
        CheckInvalid (sig);
    }
}

void CLIMMXMPP::handleXEP71 (gloox::Tag *t)
{
    if (gloox::Tag *xhtmlim = t->findChild ("html", "xmlns", "http://jabber.org/protocol/xhtml-im"))
    {
        DropAttrib (xhtmlim, "xmlns");
        DropAllChildsTree (xhtmlim, "body");
        CheckInvalid (xhtmlim);
    }
}


time_t CLIMMXMPP::handleXEP91 (gloox::Tag *t)
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

void CLIMMXMPP::handleXEP115 (gloox::Tag *t, Contact *contr)
{
    gloox::Tag *caps;
    if ((caps = t->findChild ("c", "xmlns", "http://jabber.org/protocol/caps"))
        || (caps = t->findChild ("caps:c", "xmlns:caps", "http://jabber.org/protocol/caps")))
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
        else if (!strcmp (node.c_str(), "http://trillian.im/caps"))
            node = "Trillian";
        if (ext.empty())
            s_repl (&contr->version, s_sprintf ("%s %s", node.c_str(), ver.c_str()));
        else
            s_repl (&contr->version, s_sprintf ("%s %s [%s]", node.c_str(), ver.c_str(), ext.c_str()));
        DropAttrib (caps, "xmlns");
        DropAttrib (caps, "xmlns:caps");
        DropAttrib (caps, "ver");
        DropAttrib (caps, "ext");
        DropAttrib (caps, "node");
        CheckInvalid (caps);
    }
}

void CLIMMXMPP::handleXEP136 (gloox::Tag *t)
{
    if (gloox::Tag *arc = t->findChild ("arc:record", "xmlns:arc", "http://jabber.org/protocol/archive"))
    {
        DropAttrib (arc, "xmlns:arc");
        DropAttrib (arc, "otr");
        CheckInvalid (arc);
    }
}

void CLIMMXMPP::handleXEP153 (gloox::Tag *t, Contact *contr)
{
    if (gloox::Tag *vcard = t->findChild ("x", "xmlns", "vcard-temp:x:update"))
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
        DropAttrib (vcard, "xmlns");
        CheckInvalid (vcard);
    }
}

void CLIMMXMPP::handleGoogleNosave (gloox::Tag *t)
{
    if (gloox::Tag *nosave = t->findChild ("nos:x", "xmlns:nos", "google:nosave"))
    {
        DropAttrib (nosave, "xmlns:nos");
        DropAttrib (nosave, "value");
        CheckInvalid (nosave);
    }
}

void CLIMMXMPP::handleGoogleSig (gloox::Tag *t)
{
    if (gloox::Tag *sig = t->findChild ("met:google-mail-signature", "xmlns:met", "google:metadata"))
    {
        DropAttrib (sig, "xmlns:met");
        DropCData (sig);
        CheckInvalid (sig);
    }
}

void CLIMMXMPP::handleGoogleChatstate(gloox::Tag *t)
{
    if (gloox::Tag *chat = t->findChild ("cha:active", "xmlns:cha", "http://jabber.org/protocol/chatstates"))
    {
        DropAttrib (chat, "xmlns:cha");
        CheckInvalid (chat);
    }
}


void CLIMMXMPP::handleMessage2 (gloox::Stanza *t, gloox::JID from, std::string tof, std::string id, gloox::StanzaSubType subtype)
{
    Contact *cto = ContactScreen (m_serv, tof.c_str());
    std::string subtypeval = t->findAttribute ("type");
    std::string body = t->body();
    std::string subject = t->subject();
    std::string html;
    gloox::Tag *htmltag = t->findChild ("html", "xmlns", "http://jabber.org/protocol/xhtml-im");
    if (htmltag)
        htmltag = htmltag->findChild ("body", "xmlns", "http://www.w3.org/1999/xhtml");
    if (htmltag)
        html = htmltag->cdata();
    DropAttrib (t, "type");
    time_t delay;
    Contact *contb, *contr;

    GetBothContacts (from, m_serv, &contb, &contr, 0);

    DropAllChilds (t, "subject");
    DropAllChilds (t, "thread");
    handleXEP71 (t);
    handleGoogleNosave (t);
    handleGoogleSig (t);
    handleGoogleChatstate (t);
    handleXEP136 (t);
    delay = handleXEP91 (t);
    handleXEP115 (t, contr); // entity capabilities (used also for client version)
    if (handleXEP22and85 (t, contr, from, tof, id))
        return;
    DropAllChilds (t, "body");

    Opt *opt = OptSetVals (NULL, CO_ORIGIN, CV_ORIGIN_v8, CO_MSGTYPE, MSG_NORM, CO_MSGTEXT, body.c_str(), 0);
    if (!subject.empty())
        opt = OptSetVals (opt, CO_MSGTYPE, MSG_NORM_SUBJ, CO_SUBJECT, subject.c_str(), 0);
    if (!strcmp (html.c_str(), body.c_str()))
        opt = OptSetVals (opt, CO_SAMEHTML, 1);
    IMSrvMsgFat (contr, delay, opt);

    if (gloox::Tag *x = t->findChild ("x"))
        CheckInvalid (x);
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

void CLIMMXMPP::handlePresence2 (gloox::Tag *s, gloox::JID from, gloox::JID to, std::string msg)
{
    ContactGroup *tcg;
    Contact *contb, *contr, *c;
    status_t status;
    std::string pri;
    time_t delay;
    std::string statustext;

    GetBothContacts (from, m_serv, &contb, &contr, 1);

    if (gloox::Tag *priority = s->findChild ("priority"))
    {
        pri = priority->cdata();
        DropCData (priority);
        CheckInvalid (priority);
    }

    delay = handleXEP91 (s);

    handleXEP115 (s, contr); // entity capabilities (used also for client version)
    handleXEP153 (s, contb); // vcard-based avatar, nickname
    handleXEP27 (s);         // OpenPGP signature (obsolete)
    handleXEP8 (s);          // iq-based avatar (obsolete)

    if (s->hasAttribute ("type", "unavailable") || s->hasAttribute ("type", "error"))
    {
        if (s->hasAttribute ("type", "error"))
            s_repl (&contr->version, "");
        status = ims_offline;
        DropAttrib (s, "type");

        IMOffline (contr);
        return;
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
    if (gloox::Tag *stat = s->findChild ("status"))
    {
        statustext = stat->cdata ();
        IMOnline (contr, status, imf_none, status, statustext.c_str());
    }
    else
        IMOnline (contr, status, imf_none, status, NULL);
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
    if (gloox::Tag *mb = stanza->findChild ("mailbox", "xmlns", "google:mail:notify"))
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

UBYTE CLIMMXMPP::XMPPSendmsg (Server *serv, Contact *cont, Message *msg)
{
    assert (serv == m_serv);

    assert (cont);
    assert (msg->send_message);

    if (~m_serv->conn->connect & CONNECT_OK)
        return RET_DEFER;
    if (msg->type != MSG_NORM)
        return RET_DEFER;

    gloox::Stanza *msgs = gloox::Stanza::createMessageStanza (gloox::JID (cont->screen), msg->send_message);
    msgs->addAttribute ("id", s_sprintf ("xmpp-%s-%x", m_stamp, ++m_serv->conn->our_seq));

    std::string res = m_client->resource();
    msgs->addAttribute ("from", s_sprintf ("%s/%s", serv->screen, res.c_str()));
    gloox::Tag *x = new gloox::Tag (msgs, "x");
    x->addAttribute ("xmlns", "jabber:x:event");
    new gloox::Tag (x, "offline");
    new gloox::Tag (x, "delivered");
    new gloox::Tag (x, "displayed");
    new gloox::Tag (x, "composing");
    m_client->send (msgs);

    Event *event = QueueEnqueueData2 (serv->conn, QUEUE_XMPP_RESEND_ACK, m_serv->conn->our_seq, 120, msg, &SnacCallbackXmpp, &SnacCallbackXmppCancel);
    event->cont = cont;

    return RET_OK;
}

void CLIMMXMPP::XMPPSetstatus (Server *serv, Contact *cont, status_t status, const char *msg)
{
    gloox::Presence p;
    gloox::JID j = cont ? gloox::JID (cont->screen) : gloox::JID ();

    switch (status)
    {
        case ims_online:   p = gloox::PresenceAvailable;   break;
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
        vers->addAttribute ("node", "http://www.climm.org/xmpp/caps");
        vers->addAttribute ("ver", BuildVersionStr);
        // vers->addAttribute ("ext", "ext1 ext2");
    }
    m_client->send (pres);
    m_serv->status = status;
    m_serv->nativestatus = p;
}

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

#ifdef CLIMM_XMPP_FILE_TRANSFER

void CLIMMXMPP::handleSOCKS5Data (gloox::SOCKS5Bytestream *s5b, const std::string &data)
{
    Contact *cont, *contr;
    Connection *fpeer;
    
    GetBothContacts (s5b->initiator().full(), m_serv, &cont, &contr, 0);
    fpeer = ServerFindChild (m_serv, contr, TYPE_XMPPDIRECT);
    assert (fpeer);

    int len = write (fpeer->xmpp_file->sok, data.c_str(), data.length());
    if (len != data.length())
    {
        rl_log_for (contr->screen, COLCONTACT);
        rl_printf (i18n (2575, "Error writing to file (%lu bytes written out of %u).\n"), len, data.length());
    }
    fpeer->xmpp_file->xmpp_file_done += len;
    if (fpeer->xmpp_file->xmpp_file_len == fpeer->xmpp_file->xmpp_file_done)
    {
        ReadLinePromptReset ();
        rl_log_for (cont->screen, COLCONTACT);
        rl_print  (i18n (2166, "Finished receiving file.\n"));
#if HAVE_FSYNC
        fsync (fpeer->xmpp_file->sok);
#endif
        close (fpeer->xmpp_file->sok);
        fpeer->xmpp_file->sok = -1;
    }
    else if (fpeer->xmpp_file->xmpp_file_len)
    {
        ReadLinePromptUpdate (s_sprintf ("[%s%ld %02d%%%s] %s%s",
                  COLINCOMING, fpeer->xmpp_file->xmpp_file_done, (int)((100.0 * fpeer->xmpp_file->xmpp_file_done) / fpeer->xmpp_file->xmpp_file_len),
                  COLNONE, COLSERVER, i18n (2467, "climm>")));
    }
}

void CLIMMXMPP::handleSOCKS5Error (gloox::SOCKS5Bytestream *s5b, gloox::Stanza *stanza)
{
}

void CLIMMXMPP::handleSOCKS5Open (gloox::SOCKS5Bytestream *s5b)
{
}

void CLIMMXMPP::handleSOCKS5Close (gloox::SOCKS5Bytestream *s5b)
{
    Contact *cont, *contr;
    Connection *fpeer;
    
    GetBothContacts (s5b->initiator(), m_serv, &cont, &contr, 0);
    fpeer = ServerFindChild (m_serv, contr, TYPE_XMPPDIRECT);
    assert (fpeer);
    ConnectionD (fpeer);
}

void CLIMMXMPP::handleFTRequestError (gloox::Stanza *stanza)
{
    rl_printf (i18n (2234, "Message %s discarded - lost session.\n"), stanza->errorText().c_str());
}

void CLIMMXMPP::handleFTSOCKS5Bytestream (gloox::SOCKS5Bytestream *s5b)
{
    strc_t name;
    int len, off;
    Contact *cont, *contr;
    GetBothContacts (s5b->initiator(), m_serv, &cont, &contr, 0);
    s5b->registerSOCKS5BytestreamDataHandler (this);
    rl_printf (i18n (2709, "Opening file listener connection at %slocalhost%s:%s%lp%s... "),
               COLQUOTE, COLNONE, COLQUOTE, s5b->connectionImpl(), COLNONE);
    if (s5b->connect())
    {
        rl_print (i18n (1634, "ok.\n"));
        gloox::ConnectionTCPBase *conn = dynamic_cast<gloox::ConnectionTCPBase *>(s5b->connectionImpl());
        if (!conn)
            return;
        Connection *child = ServerFindChild (m_serv, contr, TYPE_XMPPDIRECT);
        if (!child)
            return;

        /**
         * Insert socket into climms TCP handler
         */
        child->sok = conn->socket();
        child->connect = CONNECT_OK | CONNECT_SELECT_R;
        child->xmpp_file_private = s5b;
    }
}

void CLIMMXMPP::handleFTRequest (const gloox::JID & from, const std::string & id, const std::string & sid,
        const std::string & name, long size, const std::string & hash, const std::string & date,
        const std::string & mimetype, const std::string & desc, int stypes, long offset, long length)
{
    Contact *cont, *contr;
    // Build a nicer description
    std::string pretty = name;

    if (desc.size())
        pretty += " (" + desc + ")";

    UWORD seq = ++m_serv->xmpp_file_seq;
    l_tSeqTranslate[seq] = sid;
    sid2id[sid] = id; //This will be changed in the 1.0 release of gloox :)
    GetBothContacts (from, m_serv, &cont, &contr, 0);

    Opt *opt2 = OptC ();
    OptSetVal (opt2, CO_BYTES, size);
    OptSetStr (opt2, CO_MSGTEXT, pretty.c_str());
    OptSetVal (opt2, CO_REF, seq);
    OptSetVal (opt2, CO_MSGTYPE, MSG_FILE);
    IMSrvMsgFat (contr, NOW, opt2);
    opt2 = OptC ();
    OptSetVal (opt2, CO_FILEACCEPT, 0);
    OptSetStr (opt2, CO_REFUSE, i18n (2514, "refused (ignored)"));
    Event *e1 = QueueEnqueueData (m_serv->conn, QUEUE_USERFILEACK, seq, time (NULL) + 120,
                           NULL, cont, opt2, &PeerFileTO); //Timeout Handler
    QueueEnqueueDep (m_serv->conn, 0, seq, e1,
                     NULL, contr, opt2, XMPPCallBackFileAccept); // Route whatever there ;)
    
    // Prepare a FileListener
    Connection *child = ServerChild (m_serv, contr, TYPE_XMPPDIRECT);
    if (!child)
        return; //Failed

    /**
     * Insert socket into climms TCP handler
     */
    child->sok = -1;
    child->connect = CONNECT_FAIL;
    child->dispatch = &XMPPFTCallbackDispatch;
    child->reconnect = NULL;
    child->error = &XMPPFTCallbackError;
    child->close = &XMPPFTCallbackClose;

    /**
     * Create the output file
     */
    Connection *ffile = ServerChild (m_serv, contr, TYPE_FILEXMPP);
    char buf[200], *p;
    str_s filename = { (char *)name.c_str(), name.length(), name.length() };
    int pos = 0;

    assert (ffile);
    pos = snprintf (buf, sizeof (buf), "%sfiles" _OS_PATHSEPSTR "%s" _OS_PATHSEPSTR,
                    PrefUserDir (prG), cont->screen);
    snprintf (buf + pos, sizeof (buf) - pos, "%s", (ConvFrom (&filename, ENC_UTF8))->txt);
    for (p = buf + pos; *p; p++)
        if (*p == '/')
            *p = '_';
    child->xmpp_file = ffile;
    s_repl (&ffile->server, buf);
    ffile->connect = CONNECT_FAIL;
    ffile->xmpp_file_len = size;
    ffile->xmpp_file_done = offset;
}
    
void CLIMMXMPP::XMPPAcceptDenyFT (gloox::JID contact, std::string id, std::string sid, const char *reason)
{
    Connection *conn;
    if (!reason) 
        return m_pFT->acceptFT (contact, id, gloox::SIProfileFT::FTTypeS5B);
    m_pFT->declineFT (contact, id, gloox::SIManager::RequestRejected, reason);
}

void XMPPFTCallbackClose (Connection *conn)
{
    if (conn->sok == -1)
        return;

    gloox::SOCKS5Bytestream *s = (gloox::SOCKS5Bytestream *)(conn->xmpp_file_private);
    conn->sok = -1;
}

BOOL XMPPFTCallbackError (Connection *conn, UDWORD rc, UDWORD flags)
{
    XMPPFTCallbackClose (conn);
    return 0;
}

void XMPPFTCallbackDispatch (Connection *conn)
{
    if (conn->sok == -1)
        return;

    gloox::SOCKS5Bytestream *s = (gloox::SOCKS5Bytestream *)(conn->xmpp_file_private);
    if (!s)
    {
        rl_printf ("#Avoid spinning.\n");
        conn->sok = -1;
        return;
    }

    s->recv (0);
}

#endif

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

    XMPPCallbackClose (serv->conn);

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

    CLIMMXMPP *l = new CLIMMXMPP (serv);
    serv->xmpp_private = l;
    serv->conn->connect |= CONNECT_SELECT_R;

    return event;
}

void XMPPCallbackDispatch (Connection *conn)
{
    if (conn->sok == -1)
        return;

    CLIMMXMPP *j = getXMPPClient (conn->serv);
    if (!j)
    {
        rl_printf ("#Avoid spinning.\n");
        conn->sok = -1;
        return;
    }

    j->getClient()->recv (0);
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

    CLIMMXMPP *j = getXMPPClient (conn->serv);
    if (!j && conn->sok == -1)
        return;
    if (!j)
    {
        rl_printf ("#Avoid spinning.\n");
        conn->sok = -1;
        return;
    }

    j->getClient()->disconnect ();
    conn->serv->xmpp_private = NULL;
    delete j;
}

BOOL XMPPCallbackError (Connection *conn, UDWORD rc, UDWORD flags)
{
    rl_printf ("#XMPPCallbackError: %p %lu %lu\n", conn, rc, flags);
    return 0;
}

#ifdef CLIMM_XMPP_FILE_TRANSFER
void XMPPCallBackFileAccept (Event *event)
{
    Connection *conn = event->conn;
    Contact *id, *res = NULL;
    CLIMMXMPP *j;
    UDWORD opt_acc;
    std::string seq, sid;
    char *reason = NULL;

    std::map<UWORD, std::string>::iterator l_it;
    l_it = l_tSeqTranslate.find (event->seq);
    if (l_it != l_tSeqTranslate.end())
        sid = l_tSeqTranslate[event->seq];

    seq = sid2id[sid];

    if (!conn)
    {
        EventD (event);
        return;
    }
    j = getXMPPClient (conn->serv);
    assert (conn->serv);
    assert (conn->serv->type == TYPE_XMPP_SERVER);
    assert (event->cont);
    assert (event->cont->parent);

    res = event->cont;
    id = res->parent;

    assert (id);
    
    OptGetVal (event->opt, CO_FILEACCEPT, &opt_acc);
    if (!opt_acc)
        OptGetStr (event->opt, CO_REFUSE, (const char **)&reason);
    else
    {
        Connection *conn = ServerFindChild (event->conn->serv, event->cont, TYPE_XMPPDIRECT);
        Connection *ffile = conn->xmpp_file;
        Contact *cont = res->parent;
        assert (ffile->type == TYPE_FILEXMPP);
        ffile->sok = open (ffile->server, O_CREAT | O_WRONLY | (ffile->xmpp_file_done ? O_APPEND : O_TRUNC), 0660);
        if (ffile->sok == -1)
        {
            int rc = errno;
            if (rc == ENOENT)
            {
                mkdir (s_sprintf ("%sfiles", PrefUserDir (prG)), 0700);
                mkdir (s_sprintf ("%sfiles" _OS_PATHSEPSTR "%s", PrefUserDir (prG), cont->screen), 0700);
                ffile->sok = open (ffile->server, O_CREAT | O_WRONLY | (ffile->xmpp_file_done ? O_APPEND : O_TRUNC), 0660);
            }
            if (ffile->sok == -1)
            {
                rl_log_for (cont->screen, COLCONTACT);
                rl_printf (i18n (2083, "Cannot open file %s: %s (%d).\n"),
                         ffile->server, strerror (rc), rc);
                ConnectionD (conn);
                return;
            }
        }
        ffile->connect = CONNECT_OK;
    }
    j->XMPPAcceptDenyFT (gloox::JID (std::string (id->screen) + "/" + res->screen), seq, sid, reason);
    event->opt = NULL;
    EventD (QueueDequeueEvent (event->wait));
    EventD (event);
}
#endif

UBYTE XMPPSendmsg (Server *serv, Contact *cont, Message *msg)
{
    CLIMMXMPP *j = getXMPPClient (serv);
    assert (j);
    return j->XMPPSendmsg (serv, cont, msg);
}

void XMPPSetstatus (Server *serv, Contact *cont, status_t status, const char *msg)
{
    CLIMMXMPP *j = getXMPPClient (serv);
    assert (j);
    j->XMPPSetstatus (serv, cont, status, msg);
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
