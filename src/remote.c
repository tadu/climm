/*
 * FIFO sockets for remote controlling mICQ
 *
 * mICQ Copyright (C) © 2001,2002,2003 Rüdiger Kuhlmann
 *
 * mICQ is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 dated June, 1991.
 *
 * mICQ is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * In addition, as a special exception permission is granted to link the
 * code of this release of mICQ with the OpenSSL project's "OpenSSL"
 * library, and distribute the linked executables.  You must obey the GNU
 * General Public License in all respects for all of the code used other
 * than "OpenSSL".  If you modify this file, you may extend this exception
 * to your version of the file, but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your version
 * of this file.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * $Id$
 */

#include "micq.h"
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#include "util_io.h"
#include "util_ui.h"
#include "cmd_user.h"
#include "connection.h"
#include "packet.h"
#include "contact.h"
#include "remote.h"
#include "preferences.h"

#ifdef ENABLE_REMOTECONTROL

static void RemoteDispatch (Connection *remo);
static void RemoteClose (Connection *remo);

/*
 * "Logs in" TCP connection by opening listening socket.
 */
Event *RemoteOpen (Connection *remo)
{
    s_repl (&remo->server, s_realpath (remo->pref_server));

    rl_printf (i18n (2223, "Opening remote control FIFO at %s... "), s_wordquote (remo->server));

    remo->connect     = 0;
    remo->our_seq     = 0;
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
    return NULL;
}

static void RemoteDispatch (Connection *remo)
{
    Packet *pak;
    
    while ((pak = UtilIOReceiveF (remo)))
    {
        CmdUser ((const char *)pak->data);
        PacketD (pak);
    }
}

static void RemoteClose (Connection *remo)
{
    sockclose (remo->sok);
    remo->sok = -1;
    remo->connect = 0;
    remo->open = &RemoteOpen;
    unlink (remo->server);
}

#endif
