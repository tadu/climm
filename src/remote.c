/*
 * FIFO sockets for remote controlling mICQ
 *
 * This file is Copyright Rüdiger Kuhlmann; it may be distributed under
 * version 2 of the GPL licence.
 *
 * $Id$
 */

#include "micq.h"
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "util_io.h"
#include "util_ui.h"
#include "util_str.h"
#include "cmd_user.h"
#include "session.h"
#include "packet.h"
#include "preferences.h"

static void RemoteDispatch (Connection *remo);
static void RemoteClose (Connection *remo);

/*
 * "Logs in" TCP connection by opening listening socket.
 */
void RemoteOpen (Connection *remo)
{
    s_repl (&remo->server, s_realpath (remo->spref->server));

    M_printf (i18n (2223, "Opening remote control FIFO at %s... "), remo->server);

    remo->connect     = 0;
    remo->our_seq     = 0;
    remo->type        = TYPE_REMOTE;
    remo->flags       = 0;
    remo->open        = &RemoteOpen;
    remo->dispatch    = &RemoteDispatch;
    remo->reconnect   = NULL;
    remo->close       = &RemoteClose;
    remo->our_session = 0;
    remo->ip          = 0;
    remo->port        = 0;

    UtilIOConnectF (remo);
    
    if (remo->connect)
        remo->connect = CONNECT_OK | CONNECT_SELECT_R;
}

static void RemoteDispatch (Connection *remo)
{
    Packet *pak;
    
    while ((pak = UtilIOReceiveF (remo)))
    {
        CmdUser (pak->data);
        PacketD (pak);
    }
}

static void RemoteClose (Connection *remo)
{
    M_print ("FIXME: closing down.\n");
    sockclose (remo->sok);
    remo->sok = -1;
    remo->connect = 0;
    remo->open = &RemoteOpen;
    unlink (remo->server);
}

