/*
 * FIFO sockets for remote controlling mICQ
 *
 * This file is Copyright © Rüdiger Kuhlmann; it may be distributed under
 * version 2 of the GPL licence.
 *
 * $Id$
 */

#include "micq.h"
#include "util_io.h"
#include "cmd_user.h"

static void RemoteDispatch (Connection *remo);
static void RemoteClose (Connetion *remo);

/*
 * "Logs in" TCP connection by opening listening socket.
 */
void RemoteOpen (Connection *remo)
{
    assert (remo);
    
    M_printf (i18n (2223, "Opening remote control FIFO socket at %s... "), remo->server);

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

    UtilIOConnectS (remo);
}

static void RemoteDispatch (Connection *remo)
{
    Packet *pak;
    
    if (!(pak = UtilIOReceiveS (remo)))
        return;
    
    CmdUser (pak->data);
}

static void RemoteClose (Connetion *remo)
{
    sockclose (remo->sok);
    remo->sok = -1;
    remo->connect = 0;
    remo->open = &RemoteOpen;
}

