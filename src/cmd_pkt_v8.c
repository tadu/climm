
/*
 * Initialization and basic support for v8 of the ICQ protocl.
 *
 * This file is Copyright © Rüdiger Kuhlmann; it may be distributed under
 * version 2 of the GPL licence.
 *
 * $Id$
 */

#include "micq.h"
#include "util.h"
#include "util_ui.h"
#include "util_io.h"
#include "conv.h"
#include "contact.h"
#include "session.h"
#include "packet.h"
#include "preferences.h"
#include "cmd_pkt_v8_flap.h"
#include "cmd_pkt_v8_snac.h"
#include "util_str.h"
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <assert.h>

jump_conn_f SrvCallBackReceive;
static jump_conn_f SrvCallBackReconn;
static void SrvCallBackTimeout (Event *event);
static void SrvCallBackDoReconn (Event *event);

int reconn = 0;

void ConnectionInitServer (Connection *conn)
{
    if (!conn->server || !*conn->server || !conn->port)
        return;

    M_printf (i18n (1871, "Opening v8 connection to %s:%ld... "), conn->server, conn->port);

    conn->our_seq  = rand () & 0x7fff;
    conn->connect  = 0;
    conn->dispatch = &SrvCallBackReceive;
    conn->reconnect= &SrvCallBackReconn;
    conn->close    = &FlapCliGoodbye;
    s_repl (&conn->server, conn->spref->server);
    conn->type     = TYPE_SERVER;
    QueueEnqueueData (conn, conn->connect, conn->our_seq,
                      time (NULL) + 10,
                      NULL, conn->uin, NULL, &SrvCallBackTimeout);
    UtilIOConnectTCP (conn);
}

static void SrvCallBackReconn (Connection *conn)
{
    ContactGroup *cg = conn->contacts;
    Contact *cont;
    int i;

    cont = ContactByUIN (conn->uin, 1);
    if (!cont)
        return;

    M_printf ("%s %s%*s%s ", s_now, COLCONTACT, uiG.nick_len + s_delta (cont->nick), cont->nick, COLNONE);
    conn->connect = 0;
    if (reconn < 5)
    {
        M_printf (i18n (2032, "Scheduling v8 reconnect in %d seconds.\n"), 10 << reconn);
        QueueEnqueueData (conn, /* FIXME: */ 0, 0, time (NULL) + (10 << reconn), NULL, conn->uin, NULL, &SrvCallBackDoReconn);
        reconn++;
    }
    else
    {
        M_print (i18n (2031, "Connecting failed too often, giving up.\n"));
        reconn = 0;
    }
    for (i = 0; (cont = ContactIndex (cg, i)); i++)
        cont->status = STATUS_OFFLINE;
}

static void SrvCallBackDoReconn (Event *event)
{
    if (event->conn)
        ConnectionInitServer (event->conn);
    EventD (event);
}

static void SrvCallBackTimeout (Event *event)
{
    Connection *conn = event->conn;
    
    if (!conn)
    {
        EventD (event);
        return;
    }
    
    if ((conn->connect & CONNECT_MASK) && !(conn->connect & CONNECT_OK))
    {
        if (conn->connect == event->type)
        {
            M_print (i18n (1885, "Connection v8 timed out.\n"));
            conn->connect = 0;
            sockclose (conn->sok);
            conn->sok = -1;
            SrvCallBackReconn (conn);
        }
        else
        {
            event->due = time (NULL) + 10;
            conn->connect |= CONNECT_SELECT_R;
            event->type = conn->connect;
            QueueEnqueue (event);
            return;
        }
    }
    EventD (event);
}

void SrvCallBackReceive (Connection *conn)
{
    Packet *pak;

    if (!(conn->connect & CONNECT_OK))
    {
        switch (conn->connect & 7)
        {
            case 0:
            case 1:
            case 5:
                if (conn->assoc && !(conn->assoc->connect & CONNECT_OK) && (conn->assoc->flags & CONN_AUTOLOGIN))
                {
                    printf ("Buggy: avoiding deadlock\n");
                    conn->connect &= ~CONNECT_SELECT_R;
                }
                else
                    conn->connect |= 4 | CONNECT_SELECT_R;
                conn->connect &= ~CONNECT_SELECT_W & ~CONNECT_SELECT_X & ~3;
                return;
            case 2:
            case 6:
                conn->connect = 0;
                SrvCallBackReconn (conn);
                return;
            case 4:
                break;
            default:
                assert (0);
        }
    }

    pak = UtilIOReceiveTCP (conn);
    
    if (!pak)
        return;

    if (PacketRead1 (pak) != 0x2a)
    {
        Debug (DEB_PROTOCOL, i18n (1880, "Incoming packet is not a FLAP: id is %d.\n"), PacketRead1 (pak));
        return;
    }
    
    pak->cmd = PacketRead1 (pak);
    pak->id =  PacketReadB2 (pak);
               PacketReadB2 (pak);
    
    if (prG->verbose & DEB_PACK8)
    {
        M_printf ("%s " COLINDENT COLSERVER "%s ",
                 s_now, i18n (1033, "Incoming v8 server packet:"));
        FlapPrint (pak);
        M_print (COLEXDENT "\r");
    }
    if (prG->verbose & DEB_PACK8SAVE)
        FlapSave (pak, TRUE);
    
    QueueEnqueueData (conn, QUEUE_FLAP, pak->id, time (NULL),
                      pak, 0, NULL, &SrvCallBackFlap);
    pak = NULL;
}

Connection *SrvRegisterUIN (Connection *conn, const char *pass)
{
    Connection *new;
    
    new = ConnectionC ();
    if (!new)
        return NULL;
    new->spref = PreferencesConnectionC ();
    if (!new->spref)
        return NULL;
    if (conn)
    {
        assert (conn->spref->type == TYPE_SERVER);
        
        memcpy (new->spref, conn->spref, sizeof (*new->spref));
        new->spref->server = strdup (new->spref->server);
        new->spref->uin = 0;
    }
    else
    {
        new->spref->type = TYPE_SERVER;
        new->spref->flags = 0;
        new->spref->version = 8;
        new->spref->server = strdup ("login.icq.com");
        new->spref->port = 5190;
    }
    new->spref->passwd = strdup (pass);
    new->type    = TYPE_SERVER;
    new->flags   = 0;
    new->ver  = new->spref->version;
    new->server = strdup (new->spref->server);
    new->port = new->spref->port;
    new->passwd = strdup (pass);
    
    ConnectionInitServer (new);
    return new;
}

void SrvMsgAdvanced (Packet *pak, UDWORD seq, UWORD type, UWORD flags, UWORD status, const char *msg)
{
    PacketWriteLen    (pak);
     PacketWrite2      (pak, seq);        /* sequence number            */
     PacketWrite4      (pak, 0);
     PacketWrite4      (pak, 0);
     PacketWrite4      (pak, 0);
    PacketWriteLenDone (pak);
    PacketWrite2      (pak, type);       /* message type               */
    PacketWrite2      (pak, status);     /* flags                      */
    PacketWrite2      (pak, flags);      /* status                     */
    PacketWriteLNTS   (pak, msg);        /* the message                */
}


