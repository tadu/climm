
extern "C" {
#include "climm.h"
#include "msn_base.h"
#include "connection.h"
#include "contact.h"
#include "util_io.h"
}

#include "msn/msn.h"
#include "msn/externals.h"

class CLIMMMSN : public MSN::Callbacks
{
  public:
    MSN::NotificationServerConnection *mainConnection;
    ::Server *serv;
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

static inline CLIMMMSN *MyCallbackFromMSN (MSN::Connection *msnconn)
{
    return (CLIMMMSN *)msnconn->user_data;
}

static inline void MyCallbackSetMSN (MSN::Connection *msnconn, CLIMMMSN *msn)
{
    msnconn->user_data = (void *)msn;
}

static inline CLIMMMSN *MyCallbackFromClimm (Connection *conn)
{
    return (CLIMMMSN *)conn->tlv;
}

static inline void MyCallbackSetClimm (Connection *conn, CLIMMMSN *msn)
{
    conn->tlv = (void *)msn;
    conn->serv->tlv = (void *)msn;
}

Event *ConnectionInitMSNServer (Server *serv)
{
    CLIMMMSN *cb = new CLIMMMSN;
    
    assert (serv->type = TYPE_MSN_SERVER);
    if (!serv->screen || !serv->passwd)
        return NULL;

    rl_printf ("ConnectionInitMSNServer: %p {%s} {%s} {%s} {%lu}\n", serv, serv->screen, serv->passwd, serv->server, serv->port);
    
    if (!strchr (serv->screen, '@'))
        return NULL;
    
    if (!serv->server)
        serv->server = "messenger.hotmail.com";
    if (!serv->port)
        serv->port = 1863;
    
    cb->serv = serv;
    MyCallbackSetClimm (serv->conn, cb);
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
    
    CLIMMMSN *cb = MyCallbackFromClimm (conn);
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

    CLIMMMSN *cb = MyCallbackFromClimm (conn);
    MSN::Connection *c = cb->mainConnection->connectionWithSocket (conn->sok);

    if (!c)
        return;

    delete c;
}

BOOL MsnCallbackError (Connection *conn, UDWORD rc, UDWORD flags)
{
    rl_printf ("MsnCallbackError: %p %lu %lu\n", conn, rc, flags);

    CLIMMMSN *cb = MyCallbackFromClimm (conn);
    MSN::Connection *c = cb->mainConnection->connectionWithSocket (conn->sok);

    if (!c)
        rl_printf ("FIXME: MSN: avoid spinning %d.\n", conn->sok);

    return 0;
}

void CLIMMMSN::registerSocket (int s, int read, int write)
{
    rl_printf ("CLIMMMSN::registerSocket %d %d %d...", s, read, write);
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
            MyCallbackSetClimm (conn, this);
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
    MyCallbackSetClimm (conn, this);
    rl_printf (" %p (new)\n", conn);
}

void CLIMMMSN::unregisterSocket (int s)
{
    rl_printf ("CLIMMMSN::unregisterSocket %d...", s);
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
    CLIMMMSN *cb = MyCallbackFromClimm (conn);

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

void CLIMMMSN::showError (MSN::Connection * msnconn, std::string msg)
{
    rl_printf ("CLIMMMSN::showError %p {%s}\n", (void *)msnconn, msg.c_str ());
}

void CLIMMMSN::log (int writing, const char* buf)
{
    char *line = strdup (buf);
    size_t len = strlen (line);
    while (len && strchr ("\r\n", line[len - 1]))
        line[--len] = 0;
    rl_printf ("CLIMMMSN::log %d {%s}\n", writing, line);
    free (line);
}

void CLIMMMSN::buddyChangedStatus (MSN::Connection * msnconn, MSN::Passport buddy, std::string friendlyname, MSN::BuddyStatus state)
{
    rl_printf ("CLIMMMSN::buddyChangedStatus %p %p {%s} %u\n", (void *)msnconn, (void *)&buddy, friendlyname.c_str(), state);
}

void CLIMMMSN::buddyOffline (MSN::Connection * msnconn, MSN::Passport buddy)
{
    rl_printf ("CLIMMMSN::buddyOffline %p %p\n", (void *)msnconn, (void *)&buddy);
}

void CLIMMMSN::gotFriendlyName (MSN::Connection *msnconn, std::string friendlyname)
{
    rl_printf ("CLIMMMSN::gotFriendlyName %p {%s}\n", (void *)msnconn, friendlyname.c_str());
    Server *serv = MyCallbackFromMSN (msnconn->myNotificationServer())->serv;
    serv->cont = ContactScreen (serv, friendlyname.c_str());
}

void CLIMMMSN::gotBuddyListInfo (MSN::NotificationServerConnection * msnconn, MSN::ListSyncInfo * data)
{
    rl_printf ("CLIMMMSN::gotBuddyListInfo %p %p\n", (void *)msnconn, (void *)&data);
    // !!
}

void CLIMMMSN::gotLatestListSerial (MSN::Connection * msnconn, int serial)
{
    rl_printf ("CLIMMMSN::gotLatestListSerial %p %d\n", (void *)msnconn, serial);
//        mainConnection->addToGroup (MSN::Passport ("Msnsucks@gmx.dE"), 0);
//        mainConnection->addToList ("TolleList", MSN::Passport ("mucky@gmx.de"));
//        mainConnection->addGroup ("climmGroupi");
}

void CLIMMMSN::gotGTC (MSN::Connection * msnconn, char c)
{
    rl_printf ("CLIMMMSN::gotGTC %p %02x {%c}\n", (void *)msnconn, c, c);
}

void CLIMMMSN::gotBLP (MSN::Connection * msnconn, char c)
{
    rl_printf ("CLIMMMSN::gotBLP %p %02x {%c}\n", (void *)msnconn, c, c);
}

void CLIMMMSN::gotNewReverseListEntry (MSN::Connection * msnconn, MSN::Passport buddy, std::string friendlyname)
{
    rl_printf ("CLIMMMSN::gotNewReverseListEntry %p {%s} {%s}\n", (void *)msnconn, buddy.c_str(), friendlyname.c_str());
}

void CLIMMMSN::addedListEntry (MSN::Connection * msnconn, std::string list, MSN::Passport buddy, int groupID)
{
    rl_printf ("CLIMMMSN::addedListEntry %p {%s} %p %d\n", (void *)msnconn, list.c_str(), (void *)&buddy, groupID);
}

void CLIMMMSN::removedListEntry (MSN::Connection * msnconn, std::string list, MSN::Passport buddy, int groupID)
{
    rl_printf ("CLIMMMSN::removedListEntry %p {%s} %p %d\n", (void *)msnconn, list.c_str(), (void *)&buddy, groupID);
}

void CLIMMMSN::addedGroup (MSN::Connection * msnconn, std::string groupName, int groupID)
{
    rl_printf ("CLIMMMSN::addedGroup %p {%s} %d\n", (void *)msnconn, groupName.c_str(), groupID);
}

void CLIMMMSN::removedGroup (MSN::Connection * msnconn, int groupID)
{
    rl_printf ("CLIMMMSN::removedGroup %p %d\n", (void *)msnconn, groupID);
}

void CLIMMMSN::renamedGroup (MSN::Connection * msnconn, int groupID, std::string newGroupName)
{
    rl_printf ("CLIMMMSN::renamedGroup %p %d {%s}\n", (void *)msnconn, groupID, newGroupName.c_str ());
}

void CLIMMMSN::gotSwitchboard (MSN::SwitchboardServerConnection * msnconn, const void * tag)
{
    rl_printf ("CLIMMMSN::gotSwitchboard %p %p\n", (void *)msnconn, tag);
}

void CLIMMMSN::buddyJoinedConversation (MSN::SwitchboardServerConnection * msnconn, MSN::Passport buddy, std::string friendlyname, int is_initial)
{
    rl_printf ("CLIMMMSN::buddyJoinedConversation %p %p {%s} %d\n", (void *)msnconn, (void *)&buddy, friendlyname.c_str(), is_initial);
}

void CLIMMMSN::buddyLeftConversation (MSN::SwitchboardServerConnection * msnconn, MSN::Passport buddy)
{
    rl_printf ("CLIMMMSN::buddyLeftConversation %p %p\n", (void *)msnconn, (void *)&buddy);
}

void CLIMMMSN::gotInstantMessage (MSN::SwitchboardServerConnection * msnconn, MSN::Passport buddy, std::string friendlyname, MSN::Message * msg)
{
    rl_printf ("CLIMMMSN::gotInstantMessage %p %p {%s} %p\n", (void *)msnconn, (void *)&buddy, friendlyname.c_str(), (void *)&msg);
}

void CLIMMMSN::failedSendingMessage (MSN::Connection * msnconn)
{
    rl_printf ("CLIMMMSN::failedSendingMessage %p\n", (void *)msnconn);
}

void CLIMMMSN::buddyTyping (MSN::Connection * msnconn, MSN::Passport buddy, std::string friendlyname)
{
    rl_printf ("CLIMMMSN::buddyTyping %p %p {%s}\n", (void *)msnconn, (void *)&buddy, friendlyname.c_str());
}

void CLIMMMSN::gotInitialEmailNotification (MSN::Connection * msnconn, int unread_inbox, int unread_folders)
{
    rl_printf ("CLIMMMSN::gotInitialEmailNotification %p %d %d\n", (void *)msnconn, unread_inbox, unread_folders);
}

void CLIMMMSN::gotNewEmailNotification (MSN::Connection * msnconn, std::string from, std::string subject)
{
    rl_printf ("CLIMMMSN::gotNewEmailNotification %p {%s} {%s}\n", (void *)msnconn, from.c_str(), subject.c_str());
}

void CLIMMMSN::gotFileTransferInvitation (MSN::Connection * msnconn, MSN::Passport buddy, std::string friendlyname, MSN::FileTransferInvitation * i)
{
    rl_printf ("CLIMMMSN::gotFileTransferInvitation %p %p {%s} %p\n", (void *)msnconn, (void *)&buddy, friendlyname.c_str(), (void *)&i);
}

void CLIMMMSN::fileTransferProgress (MSN::FileTransferInvitation * inv, std::string status, unsigned long recv, unsigned long total)
{
    rl_printf ("CLIMMMSN::fileTransferProgress %p {%s} %lu %lu\n", inv, status.c_str(), recv, total);
}

void CLIMMMSN::fileTransferFailed (MSN::FileTransferInvitation * inv, int error, std::string message)
{
    rl_printf ("CLIMMMSN::fileTransferFailed %p %d {%s}\n", inv, error, message.c_str());
}

void CLIMMMSN::fileTransferSucceeded (MSN::FileTransferInvitation * inv)
{
    rl_printf ("CLIMMMSN::fileTransferSucceeded %p\n", (void *)&inv);
}

void CLIMMMSN::gotNewConnection (MSN::Connection * msnconn)
{
    rl_printf ("CLIMMMSN::gotNewConnection %p ...", (void *)msnconn);

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
        serv->conn->connect = conn->connect;
        serv->dispatch = conn->dispatch;
        serv->conn->sok = conn->sok;
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

void CLIMMMSN::closingConnection (MSN::Connection * msnconn)
{
    rl_printf ("CLIMMMSN::closingConnection %p\n", (void *)msnconn);
}

void CLIMMMSN::changedStatus (MSN::Connection * msnconn, MSN::BuddyStatus state)
{
    rl_printf ("CLIMMMSN::changedStatus %p %u\n", (void *)msnconn, state);
}

int CLIMMMSN::connectToServer (std::string server, int port, bool *connected)
{
    rl_printf ("CLIMMMSN:%p:connectToServer {%s} %d %p %d\n", this, server.c_str(), port, connected, *connected);
    *connected = 0;

    ::Connection *conn = ConnectionC (TYPE_MSN_TEMP);
    conn->connect = 0;
    conn->port = port;
    MyCallbackSetClimm (conn, this);
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

int CLIMMMSN::listenOnPort (int port)
{
    rl_printf ("CLIMMMSN::listenOnPort %d\n", port);
    return -1;
}

std::string CLIMMMSN::getOurIP (void)
{
    rl_printf ("CLIMMMSN::getOurIP\n");
    return "<getOutIP>";
}

std::string CLIMMMSN::getSecureHTTPProxy (void)
{
    rl_printf ("CLIMMMSN::getSecureHTTPProxy\n");
    return "";
}
