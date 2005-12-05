
extern "C" {
#include "micq.h"
#include "msn_base.h"
#include "connection.h"
#include "contact.h"
#include "util_io.h"
}

#include "msn/msn.h"
#include "msn/externals.h"

class MICQMSN : public MSN::Callbacks
{
  public:
    MSN::NotificationServerConnection *mainConnection;
    ::Connection *serv;
    virtual void   registerSocket (int s, int read, int write);
    virtual void   unregisterSocket (int s);
    virtual void   showError (MSN::Connection *msnconn, std::string msg);
    virtual void   buddyChangedStatus (MSN::Connection *msnconn, MSN::Passport buddy, std::string friendlyname, MSN::BuddyStatus state);
    virtual void   buddyOffline (MSN::Connection *msnconn, MSN::Passport buddy);
    virtual void   log (int writing, const char *buf);
    virtual void   gotFriendlyName (MSN::Connection *msnconn, std::string friendlyname);
    virtual void   gotBuddyListInfo (MSN::NotificationServerConnection *msnconn, MSN::ListSyncInfo *data);
    virtual void   gotLatestListSerial (MSN::Connection *msnconn, int serial);
    virtual void   gotGTC (MSN::Connection *msnconn, char c);
    virtual void   gotBLP (MSN::Connection *msnconn, char c);
    virtual void   gotNewReverseListEntry (MSN::Connection *msnconn, MSN::Passport buddy, std::string friendlyname);
    virtual void   addedListEntry (MSN::Connection *msnconn, std::string list, MSN::Passport buddy, int groupID);
    virtual void   removedListEntry (MSN::Connection *msnconn, std::string list, MSN::Passport buddy, int groupID);
    virtual void   addedGroup (MSN::Connection *msnconn, std::string groupName, int groupID);
    virtual void   removedGroup (MSN::Connection *msnconn, int groupID);
    virtual void   renamedGroup (MSN::Connection *msnconn, int groupID, std::string newGroupName);
    virtual void   gotSwitchboard (MSN::SwitchboardServerConnection *msnconn, const void *tag);
    virtual void   buddyJoinedConversation (MSN::SwitchboardServerConnection *msnconn, MSN::Passport buddy, std::string friendlyname, int is_initial);
    virtual void   buddyLeftConversation (MSN::SwitchboardServerConnection *msnconn, MSN::Passport buddy);
    virtual void   gotInstantMessage (MSN::SwitchboardServerConnection *msnconn, MSN::Passport buddy, std::string friendlyname, MSN::Message *msg);
    virtual void   failedSendingMessage (MSN::Connection *msnconn);
    virtual void   buddyTyping (MSN::Connection *msnconn, MSN::Passport buddy, std::string friendlyname);
    virtual void   gotInitialEmailNotification (MSN::Connection *msnconn, int unread_inbox, int unread_folders);
    virtual void   gotNewEmailNotification (MSN::Connection *msnconn, std::string from, std::string subject);
    virtual void   gotFileTransferInvitation (MSN::Connection *msnconn, MSN::Passport buddy, std::string friendlyname, MSN::FileTransferInvitation *inv);
    virtual void   fileTransferProgress (MSN::FileTransferInvitation *inv, std::string status, unsigned long recv, unsigned long total);
    virtual void   fileTransferFailed (MSN::FileTransferInvitation *inv, int error, std::string message);
    virtual void   fileTransferSucceeded (MSN::FileTransferInvitation *inv);
    virtual void   gotNewConnection (MSN::Connection *msnconn);
    virtual void   closingConnection (MSN::Connection *msnconn);
    virtual void   changedStatus (MSN::Connection *msnconn, MSN::BuddyStatus state);
    virtual int          connectToServer (std::string server, int port, bool *connected);
    virtual int          listenOnPort (int port);
    virtual std::string  getOurIP ();
    virtual std::string  getSecureHTTPProxy ();
};

extern "C" {
    static jump_conn_f MsnCallbackDispatch;
    static jump_conn_f MsnCallbackReconn;
    static jump_conn_err_f MsnCallbackError;
    static jump_conn_f MsnCallbackClose;
    static jump_conn_f MsnCallbackConnectedStub;
}

static inline MICQMSN *MyCallbackFromMSN (MSN::Connection *msnconn)
{
    return (MICQMSN *)msnconn->user_data;
}

static inline void MyCallbackSetMSN (MSN::Connection *msnconn, MICQMSN *msn)
{
    msnconn->user_data = (void *)msn;
}

static inline MICQMSN *MyCallbackFromMICQ (Connection *conn)
{
    return (MICQMSN *)conn->tlv;
}

static inline void MyCallbackSetMICQ (Connection *conn, MICQMSN *msn)
{
    conn->tlv = (void *)msn;
}

Event *ConnectionInitMSNServer (Connection *serv)
{
    MICQMSN *cb = new MICQMSN;
    
    assert (serv->type = TYPE_MSN_SERVER);

    rl_printf ("ConnectionInitMSNServer: %p {%s} {%s} {%s} {%lu}\n", serv, serv->screen, serv->passwd, serv->server, serv->port);
    
    if (!serv->screen || !serv->passwd || !strchr (serv->screen, '@'))
        return NULL;
    
    if (!serv->server)
        serv->server = "messenger.hotmail.com";
    if (!serv->port)
        serv->port = 1863;
    
    cb->serv = serv;
    MyCallbackSetMICQ (serv, cb);
    serv->open = &ConnectionInitMSNServer;
    serv->reconnect = &MsnCallbackReconn;
    serv->error = &MsnCallbackError;
    serv->close = &MsnCallbackClose;
    
    cb->mainConnection = new MSN::NotificationServerConnection (serv->screen, serv->passwd, *cb);
    MyCallbackSetMSN (cb->mainConnection, cb);
    rl_printf ("ConnectionInitMSNServer: %p %p %p\n", serv, cb, cb->mainConnection);
    cb->mainConnection->connect (serv->server, serv->port);
    return NULL;
}

void MsnCallbackDispatch (Connection *conn)
{
    int s = conn->sok;
    rl_printf ("MsnCallbackDispatch: %p %d\n", conn, s);
    
    if (s == -1)
        return;
    
    MICQMSN *cb = MyCallbackFromMICQ (conn);
    MSN::Connection *c = cb->mainConnection->connectionWithSocket (conn->sok);
    
    if (!c)
    {
        rl_printf ("FIXME: MSN: avoid spinning %d.\n", conn->sok);
        conn->connect &= ~(CONNECT_SELECT_R | CONNECT_SELECT_W | CONNECT_SELECT_X);
        return;
    }
    
    if (UtilIOSelectIs (conn->sok, READFDS))
        c->dataArrivedOnSocket ();
    else if (UtilIOSelectIs (conn->sok, WRITEFDS))
        c->socketIsWritable ();
}

void MsnCallbackReconn (Connection *conn)
{
    rl_printf ("MsnCallbackReconn: %p\n", conn);
}

void MsnCallbackClose (Connection *conn)
{
    rl_printf ("MsnCallbackClose: %p %d\n", conn, conn->sok);
    
    if (conn->sok == -1)
        return;

    MICQMSN *cb = MyCallbackFromMICQ (conn);
    MSN::Connection *c = cb->mainConnection->connectionWithSocket (conn->sok);

    if (!c)
        return;

    delete c;
}

BOOL MsnCallbackError (Connection *conn, UDWORD rc, UDWORD flags)
{
    rl_printf ("MsnCallbackError: %p %lu %lu\n", conn, rc, flags);

    MICQMSN *cb = MyCallbackFromMICQ (conn);
    MSN::Connection *c = cb->mainConnection->connectionWithSocket (conn->sok);

    if (!c)
        rl_printf ("FIXME: MSN: avoid spinning %d.\n", conn->sok);

    return 0;
}

void MICQMSN::registerSocket (int s, int read, int write)
{
    rl_printf ("MICQMSN::registerSocket %d %d %d...", s, read, write);
    assert (s != -1);
    
    ::Connection *conn;

    for (int i = 0; (conn = ConnectionNr (i)); i++)
        if (conn->sok == s)
        {
            assert (conn->type == TYPE_MSN_TEMP);
            rl_printf (" %p\n", conn);
            conn->connect &= ~(CONNECT_SELECT_R | CONNECT_SELECT_W | CONNECT_SELECT_X);
            if (read)
                conn->connect |= CONNECT_SELECT_R;
            if (write)
                conn->connect |= CONNECT_SELECT_W;
            MyCallbackSetMICQ (conn, this);
            return;
        }

    conn = ConnectionC (TYPE_MSN_TEMP);
    conn->connect = CONNECT_OK;
    conn->port = 0;
    conn->sok = s;
    conn->dispatch = &MsnCallbackDispatch;
    conn->error = &MsnCallbackError;
    conn->close = &MsnCallbackClose;
    if (read)
        conn->connect |= CONNECT_SELECT_R;
    if (write)
        conn->connect |= CONNECT_SELECT_W;
    MyCallbackSetMICQ (conn, this);
    rl_printf (" %p (new)\n", conn);
}

void MICQMSN::unregisterSocket (int s)
{
    rl_printf ("MICQMSN::unregisterSocket %d...", s);
    ::Connection *conn;
    for (int i = 0; (conn = ConnectionNr (i)); i++)
        if (conn->sok == s)
        {
            conn->sok = -1;
            if (conn->type == TYPE_MSN_TEMP)
                ConnectionD (conn);
            rl_printf (" %p\n", conn);
            return;
        }
    rl_printf (" ???\n");
}

void MsnCallbackConnected (Connection *conn)
{
    MICQMSN *cb = MyCallbackFromMICQ (conn);

    rl_printf ("MsnCallbackConnected %p %p\n", conn, cb);
    conn->dispatch = &MsnCallbackDispatch;
    
    MSN::Connection *c = cb->mainConnection->connectionWithSocket (conn->sok);

    if (c)
        c->socketConnectionCompleted ();
}

void MsnCallbackConnectedStub (Connection *conn)
{
    rl_printf ("MsnCallbackConnectedStub %p\n", conn);
}

void MICQMSN::showError (MSN::Connection * msnconn, std::string msg)
{
    rl_printf ("MICQMSN::showError %p {%s}\n", (void *)msnconn, msg.c_str ());
}

void MICQMSN::log (int writing, const char* buf)
{
    char *line = strdup (buf);
    size_t len = strlen (line);
    while (len && strchr ("\r\n", line[len - 1]))
        line[--len] = 0;
    rl_printf ("MICQMSN::log %d {%s}\n", writing, line);
    free (line);
}

void MICQMSN::buddyChangedStatus (MSN::Connection * msnconn, MSN::Passport buddy, std::string friendlyname, MSN::BuddyStatus state)
{
    rl_printf ("MICQMSN::buddyChangedStatus %p %p {%s} %u\n", (void *)msnconn, (void *)&buddy, friendlyname.c_str(), state);
}

void MICQMSN::buddyOffline (MSN::Connection * msnconn, MSN::Passport buddy)
{
    rl_printf ("MICQMSN::buddyOffline %p %p\n", (void *)msnconn, (void *)&buddy);
}

void MICQMSN::gotFriendlyName (MSN::Connection *msnconn, std::string friendlyname)
{
    rl_printf ("MICQMSN::gotFriendlyName %p {%s}\n", (void *)msnconn, friendlyname.c_str());
    Connection *serv = MyCallbackFromMSN (msnconn->myNotificationServer())->serv;
    serv->cont = ContactScreen (serv, friendlyname.c_str());
}

void MICQMSN::gotBuddyListInfo (MSN::NotificationServerConnection * msnconn, MSN::ListSyncInfo * data)
{
    rl_printf ("MICQMSN::gotBuddyListInfo %p %p\n", (void *)msnconn, (void *)&data);
    // !!
}

void MICQMSN::gotLatestListSerial (MSN::Connection * msnconn, int serial)
{
    rl_printf ("MICQMSN::gotLatestListSerial %p %d\n", (void *)msnconn, serial);
//        mainConnection->addToGroup (MSN::Passport ("Msnsucks@gmx.dE"), 0);
//        mainConnection->addToList ("TolleList", MSN::Passport ("mucky@gmx.de"));
//        mainConnection->addGroup ("mICQGroupi");
}

void MICQMSN::gotGTC (MSN::Connection * msnconn, char c)
{
    rl_printf ("MICQMSN::gotGTC %p %02x {%c}\n", (void *)msnconn, c, c);
}

void MICQMSN::gotBLP (MSN::Connection * msnconn, char c)
{
    rl_printf ("MICQMSN::gotBLP %p %02x {%c}\n", (void *)msnconn, c, c);
}

void MICQMSN::gotNewReverseListEntry (MSN::Connection * msnconn, MSN::Passport buddy, std::string friendlyname)
{
    rl_printf ("MICQMSN::gotNewReverseListEntry %p {%s} {%s}\n", (void *)msnconn, buddy.c_str(), friendlyname.c_str());
}

void MICQMSN::addedListEntry (MSN::Connection * msnconn, std::string list, MSN::Passport buddy, int groupID)
{
    rl_printf ("MICQMSN::addedListEntry %p {%s} %p %d\n", (void *)msnconn, list.c_str(), (void *)&buddy, groupID);
}

void MICQMSN::removedListEntry (MSN::Connection * msnconn, std::string list, MSN::Passport buddy, int groupID)
{
    rl_printf ("MICQMSN::removedListEntry %p {%s} %p %d\n", (void *)msnconn, list.c_str(), (void *)&buddy, groupID);
}

void MICQMSN::addedGroup (MSN::Connection * msnconn, std::string groupName, int groupID)
{
    rl_printf ("MICQMSN::addedGroup %p {%s} %d\n", (void *)msnconn, groupName.c_str(), groupID);
}

void MICQMSN::removedGroup (MSN::Connection * msnconn, int groupID)
{
    rl_printf ("MICQMSN::removedGroup %p %d\n", (void *)msnconn, groupID);
}

void MICQMSN::renamedGroup (MSN::Connection * msnconn, int groupID, std::string newGroupName)
{
    rl_printf ("MICQMSN::renamedGroup %p %d {%s}\n", (void *)msnconn, groupID, newGroupName.c_str ());
}

void MICQMSN::gotSwitchboard (MSN::SwitchboardServerConnection * msnconn, const void * tag)
{
    rl_printf ("MICQMSN::gotSwitchboard %p %p\n", (void *)msnconn, tag);
}

void MICQMSN::buddyJoinedConversation (MSN::SwitchboardServerConnection * msnconn, MSN::Passport buddy, std::string friendlyname, int is_initial)
{
    rl_printf ("MICQMSN::buddyJoinedConversation %p %p {%s} %d\n", (void *)msnconn, (void *)&buddy, friendlyname.c_str(), is_initial);
}

void MICQMSN::buddyLeftConversation (MSN::SwitchboardServerConnection * msnconn, MSN::Passport buddy)
{
    rl_printf ("MICQMSN::buddyLeftConversation %p %p\n", (void *)msnconn, (void *)&buddy);
}

void MICQMSN::gotInstantMessage (MSN::SwitchboardServerConnection * msnconn, MSN::Passport buddy, std::string friendlyname, MSN::Message * msg)
{
    rl_printf ("MICQMSN::gotInstantMessage %p %p {%s} %p\n", (void *)msnconn, (void *)&buddy, friendlyname.c_str(), (void *)&msg);
}

void MICQMSN::failedSendingMessage (MSN::Connection * msnconn)
{
    rl_printf ("MICQMSN::failedSendingMessage %p\n", (void *)msnconn);
}

void MICQMSN::buddyTyping (MSN::Connection * msnconn, MSN::Passport buddy, std::string friendlyname)
{
    rl_printf ("MICQMSN::buddyTyping %p %p {%s}\n", (void *)msnconn, (void *)&buddy, friendlyname.c_str());
}

void MICQMSN::gotInitialEmailNotification (MSN::Connection * msnconn, int unread_inbox, int unread_folders)
{
    rl_printf ("MICQMSN::gotInitialEmailNotification %p %d %d\n", (void *)msnconn, unread_inbox, unread_folders);
}

void MICQMSN::gotNewEmailNotification (MSN::Connection * msnconn, std::string from, std::string subject)
{
    rl_printf ("MICQMSN::gotNewEmailNotification %p {%s} {%s}\n", (void *)msnconn, from.c_str(), subject.c_str());
}

void MICQMSN::gotFileTransferInvitation (MSN::Connection * msnconn, MSN::Passport buddy, std::string friendlyname, MSN::FileTransferInvitation * i)
{
    rl_printf ("MICQMSN::gotFileTransferInvitation %p %p {%s} %p\n", (void *)msnconn, (void *)&buddy, friendlyname.c_str(), (void *)&i);
}

void MICQMSN::fileTransferProgress (MSN::FileTransferInvitation * inv, std::string status, unsigned long recv, unsigned long total)
{
    rl_printf ("MICQMSN::fileTransferProgress %p {%s} %lu %lu\n", inv, status.c_str(), recv, total);
}

void MICQMSN::fileTransferFailed (MSN::FileTransferInvitation * inv, int error, std::string message)
{
    rl_printf ("MICQMSN::fileTransferFailed %p %d {%s}\n", inv, error, message.c_str());
}

void MICQMSN::fileTransferSucceeded (MSN::FileTransferInvitation * inv)
{
    rl_printf ("MICQMSN::fileTransferSucceeded %p\n", (void *)&inv);
}

void MICQMSN::gotNewConnection (MSN::Connection * msnconn)
{
    rl_printf ("MICQMSN::gotNewConnection %p ...", (void *)msnconn);

    ::Connection *conn;
    for (int i = 0; (conn = ConnectionNr (i)); i++)
        if (conn->sok == msnconn->sock && conn->type == TYPE_MSN_TEMP)
            break;
    if (!conn)
    {
        rl_printf (" ???\n");
        return;
    }
    MyCallbackSetMSN (msnconn, this);
    if (msnconn == msnconn->myNotificationServer())
    {
        serv->connect = conn->connect;
        serv->dispatch = conn->dispatch;
        serv->sok = conn->sok;
        conn->sok = -1;
        ConnectionD (conn);
        serv->error = &MsnCallbackError;
        serv->close = &MsnCallbackClose;
        mainConnection->setState (MSN::MSN_STATUS_AVAILABLE);
        mainConnection->synchronizeLists (0);
    }
    else
        conn->type = TYPE_MSN_CHAT;
    rl_printf (" %p\n", conn);
    return;
}

void MICQMSN::closingConnection (MSN::Connection * msnconn)
{
    rl_printf ("MICQMSN::closingConnection %p\n", (void *)msnconn);
}

void MICQMSN::changedStatus (MSN::Connection * msnconn, MSN::BuddyStatus state)
{
    rl_printf ("MICQMSN::changedStatus %p %u\n", (void *)msnconn, state);
}

int MICQMSN::connectToServer (std::string server, int port, bool *connected)
{
    rl_printf ("MICQMSN:%p:connectToServer {%s} %d %p %d\n", this, server.c_str(), port, connected, *connected);
    *connected = 0;

    ::Connection *conn = ConnectionC (TYPE_MSN_TEMP);
    conn->connect = 0;
    conn->port = port;
    MyCallbackSetMICQ (conn, this);
    s_repl (&conn->server, server.c_str());
    conn->dispatch = &MsnCallbackConnectedStub;
    UtilIOConnectTCP (conn);
    if (conn->connect & 2)
        return -1;
    conn->connect &= ~(CONNECT_SELECT_R | CONNECT_SELECT_W | CONNECT_SELECT_X);
    if (conn->connect & 1)
    {
        *connected = 1;
        conn->dispatch = &MsnCallbackDispatch;
    }
    else
        conn->dispatch = &MsnCallbackConnected;
    conn->error = &MsnCallbackError;
    conn->close = &MsnCallbackClose;
    return conn->sok;
}

int MICQMSN::listenOnPort (int port)
{
    rl_printf ("MICQMSN::listenOnPort %d\n", port);
    return -1;
}

std::string MICQMSN::getOurIP (void)
{
    rl_printf ("MICQMSN::getOurIP\n");
    return "<getOutIP>";
}

std::string MICQMSN::getSecureHTTPProxy (void)
{
    rl_printf ("MICQMSN::getSecureHTTPProxy\n");
    return "";
}
