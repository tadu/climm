
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
#include "contact.h"
#include "session.h"
#include "packet.h"
#include "preferences.h"
#include "cmd_pkt_v8_flap.h"
#include "cmd_pkt_v8_snac.h"
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <assert.h>

jump_sess_f SrvCallBackReceive;
static jump_sess_f SrvCallBackReconn;
static void SrvCallBackTimeout (Event *event);
static void SrvCallBackDoReconn (Event *event);

int reconn = 0;

void SessionInitServer (Session *sess)
{
    if (!sess->server || !*sess->server || !sess->port)
        return;

    M_print (i18n (1871, "Opening v8 connection to %s:%d... "), sess->server, sess->port);

    sess->our_seq  = rand () & 0x7fff;
    sess->connect  = 0;
    sess->dispatch = &SrvCallBackReceive;
    sess->reconnect= &SrvCallBackReconn;
    sess->server   = strdup (sess->spref->server);
    sess->type     = TYPE_SERVER;
    QueueEnqueueData (sess, sess->our_seq, sess->connect,
                      sess->uin, time (NULL) + 10,
                      NULL, NULL, &SrvCallBackTimeout);
    UtilIOConnectTCP (sess);
}

static void SrvCallBackReconn (Session *sess)
{
    Contact *cont;

    Time_Stamp ();
    M_print (" %s%10s%s ", COLCONTACT, ContactFindName (sess->uin), COLNONE);
    sess->connect = 0;
    if (reconn < 5)
    {
        M_print (i18n (2032, "Scheduling v8 reconnect in %d seconds.\n"), 10 << reconn);
        QueueEnqueueData (sess, 0, 0, sess->uin, time (NULL) + (10 << reconn), NULL, NULL, &SrvCallBackDoReconn);
        reconn++;
    }
    else
    {
        M_print (i18n (2031, "Connecting failed too often, giving up.\n"));
        reconn = 0;
    }
    for (cont = ContactStart (); ContactHasNext (cont); cont = ContactNext (cont))
    {
        cont->status = STATUS_OFFLINE;
    }
}

static void SrvCallBackDoReconn (Event *event)
{
    if (event->sess)
        SessionInitServer (event->sess);
    free (event);
}

static void SrvCallBackTimeout (Event *event)
{
    Session *sess = event->sess;
    
    if (!sess)
    {
        free (event);
        return;
    }
    
    if ((sess->connect & CONNECT_MASK) && !(sess->connect & CONNECT_OK))
    {
        if (sess->connect == event->type)
        {
            M_print (i18n (1885, "Connection v8 timed out.\n"));
            sess->connect = 0;
            sockclose (sess->sok);
            sess->sok = -1;
            SrvCallBackReconn (sess);
        }
        else
        {
            event->due = time (NULL) + 10;
            sess->connect |= CONNECT_SELECT_R;
            event->type = sess->connect;
            QueueEnqueue (event);
            return;
        }
    }
    free (event);
}

void SrvCallBackReceive (Session *sess)
{
    Packet *pak;

    if (!(sess->connect & CONNECT_OK))
    {
        switch (sess->connect & 7)
        {
            case 1:
            case 5:
                if (sess->assoc && !(sess->assoc->connect & CONNECT_OK) && (sess->assoc->flags & CONN_AUTOLOGIN))
                {
                    printf ("Buggy: avoiding deadlock\n");
                    sess->connect &= ~CONNECT_SELECT_R;
                }
                else
                    sess->connect |= 4 | CONNECT_SELECT_R;
                sess->connect &= ~CONNECT_SELECT_W & ~CONNECT_SELECT_X & ~3;
                return;
            case 2:
            case 6:
                sess->connect = 0;
                return;
            case 4:
                break;
            default:
                assert (0);
        }
    }

    pak = UtilIOReceiveTCP (sess);
    
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
        Time_Stamp ();
        M_print (" " COLINDENT COLSERVER "%s ", i18n (1033, "Incoming v8 server packet:"));
        FlapPrint (pak);
        M_print (COLEXDENT "\r");
    }
    if (prG->verbose & DEB_PACK8SAVE)
        FlapSave (pak, TRUE);
    
    QueueEnqueueData (sess, pak->id, QUEUE_FLAP,
                      0, time (NULL),
                      pak, NULL, &SrvCallBackFlap);
    pak = NULL;
}

Session *SrvRegisterUIN (Session *sess, const char *pass)
{
    Session *new;
    
    new = SessionC ();
    if (!new)
        return NULL;
    new->spref = PreferencesSessionC ();
    if (!new->spref)
        return NULL;
    if (sess)
    {
        assert (sess->spref->type == TYPE_SERVER);
        
        memcpy (new->spref, sess->spref, sizeof (*new->spref));
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
    
    SessionInitServer (new);
    return new;
}
