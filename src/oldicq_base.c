/* Copyright: This file may be distributed under version 2 of the GPL licence.
 * $Id$
 */

#include "micq.h"
#include <assert.h>
#include <errno.h>
#include "util_io.h"
#include "connection.h"
#include "contact.h"
#include "oldicq_base.h"
#include "oldicq_compat.h"
#include "oldicq_client.h"
#include "oldicq_server.h"

static void CallBackClosev5 (Connection *conn);

Event *ConnectionInitServerV5 (Connection *conn)
{
    if (conn->version < 5)
    {
        rl_print (i18n (1869, "Protocol versions less than 5 are not supported anymore.\n"));
        return NULL;
    }
    
    conn->close = &CallBackClosev5;
    conn->cont = ContactUIN (conn, conn->uin);
    if (!conn->server || !*conn->server)
        s_repl (&conn->server, "icq.icq.com");
    if (!conn->port)
        conn->port = 4000;
    if (!conn->passwd || !*conn->passwd)
    {
        strc_t pwd;
        rl_printf ("%s ", i18n (1063, "Enter password:"));
        Echo_Off ();
        pwd = UtilIOReadline (stdin);
        Echo_On ();
        conn->passwd = strdup (pwd ? pwd->txt : "");
    }
    QueueEnqueueData (conn, /* FIXME: */ 0, 0, time (NULL), NULL, 0, NULL, &CallBackServerInitV5);
    return NULL;
}

static void CallBackClosev5 (Connection *conn)
{
    conn->connect = 0;
    if (conn->sok == -1)
        return;
    CmdPktCmdSendTextCode (conn, "B_USER_DISCONNECTED");
    sockclose (conn->sok);
    conn->sok = -1;
}

void CallBackServerInitV5 (Event *event)
{
    Connection *conn = event->conn;
    int rc;

    if (!conn)
    {
        EventD (event);
        return;
    }
    ASSERT_SERVER_OLD (conn);

    if (conn->assoc && !(conn->assoc->connect & CONNECT_OK))
    {
        event->due = time (NULL) + 1;
        QueueEnqueue (event);
        return;
    }
    EventD (event);
    
    rl_printf (i18n (9999, "Opening v5 connection to %s:%s%ld%s... "),
              s_wordquote (conn->server), COLQUOTE, conn->port, COLNONE);
    
    if (conn->sok < 0)
    {
        UtilIOConnectUDP (conn);

#ifdef __BEOS__
        if (conn->sok == -1)
#else
        if (conn->sok == -1 || conn->sok == 0)
#endif
        {
            rc = errno;
            rl_printf (i18n (1872, "failed: %s (%d)\n"), strerror (rc), rc);
            conn->connect = 0;
            conn->sok = -1;
            return;
        }
    }
    rl_print (i18n (1877, "ok.\n"));
    conn->our_seq2    = 0;
    conn->connect = 1 | CONNECT_SELECT_R;
    conn->dispatch = &CmdPktSrvRead;
    CmdPktCmdLogin (conn);
}

