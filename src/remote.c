/*
 * FIFO sockets for scripting climm
 *
 * climm Copyright (C) © 2001-2006 Rüdiger Kuhlmann
 *
 * climm is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 dated June, 1991.
 *
 * climm is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * In addition, as a special exception permission is granted to link the
 * code of this release of climm with the OpenSSL project's "OpenSSL"
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

#include "climm.h"
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#include <assert.h>
#include "util_io.h"
#include "util_ui.h"
#include "cmd_user.h"
#include "connection.h"
#include "packet.h"
#include "contact.h"
#include "remote.h"
#include "preferences.h"
#include "io/io_fifo.h"

#ifdef ENABLE_REMOTECONTROL

static void RemoteDispatch (Connection *remo);

/*
 * "Logs in" TCP connection by opening listening socket.
 */
Event *RemoteOpen (Connection *remo)
{
    const char *path = NULL;
    io_err_t rc;

    if (!OptGetStr (&prG->copts, CO_SCRIPT_PATH, &path) && !remo->server)
    {
        remo->server = strdup ("scripting");
        OptSetStr (&prG->copts, CO_SCRIPT_PATH, remo->server);
    }
    if (!remo->server || strcmp (remo->server, path))
        s_repl (&remo->server, path);
    if (*remo->server != '/')
        s_repl (&remo->server, s_realpath (remo->server));

    rl_printf (i18n (2223, "Opening scripting FIFO at %s... "), s_wordquote (remo->server));

    remo->connect     = 0;

    IOConnectFifo (remo);
    rc = UtilIORead (remo, NULL, 0);
    assert (rc < 0);
    if (rc == IO_CONNECTED)
    {
        remo->connect |= CONNECT_OK;
        remo->dispatch = &RemoteDispatch;
        rl_print (i18n (1634, "ok.\n"));
        return NULL;
    }
    rl_print ("\n");
    rc = UtilIOShowError (remo, rc);
    remo->connect = 0;
    return NULL;
}

static void RemoteDispatch (Connection *remo)
{
    int rc;
    Packet *pak;
    UBYTE *end, *beg;
    
    if (!(pak = remo->incoming))
        remo->incoming = pak = PacketC ();
    remo->connect &= ~CONNECT_SELECT_A;
    
    rc = UtilIORead (remo, (char *)(pak->data + pak->len), sizeof (pak->data) - pak->len);
    if (rc < 0)
    {
        rc = UtilIOShowError (remo, rc);
        PacketD (pak);
        remo->incoming = NULL;
        UtilIOClose (remo);
        return;
    }
    
    pak->len += rc;
    pak->data[pak->len] = '\0';
    if (!(beg = end = (UBYTE *)strpbrk ((const char *)pak->data, "\r\n")))
        return;

    *end = '\0';
    end++;
    while (*end && strchr ("\r\n\t ", *end))
        end++;

    if (*end)
    {
        remo->incoming = PacketC ();
        remo->incoming->len = pak->len - (end - pak->data);
        memcpy (remo->incoming->data, end, remo->incoming->len);
        remo->connect |= CONNECT_SELECT_A;
    }
    else
        remo->incoming = NULL;

    CmdUser ((const char *)pak->data);
    PacketD (pak);
}
#endif
