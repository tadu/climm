
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
static void SrvCallBackTimeout (struct Event *event);

void SessionInitServer (Session *sess)
{
    if (!sess->server || !*sess->server || !sess->port)
        return;

    M_print (i18n (871, "Opening v8 connection to %s:%d... "), sess->server, sess->port);

    sess->our_seq = rand () & 0x7fff;
    sess->connect = 0;
    sess->dispatch = &SrvCallBackReceive;
    sess->server = strdup (sess->spref->server);
    sess->type = TYPE_SERVER;
    UtilIOConnectTCP (sess);
    
    if (sess->connect)
    {
        QueueEnqueueData (queue, sess, sess->our_seq, sess->connect,
                          sess->uin, time (NULL) + 10,
                          NULL, NULL, &SrvCallBackTimeout);
    }
}

void SrvCallBackTimeout (struct Event *event)
{
    Session *sess = event->sess;
    
    if ((sess->connect & CONNECT_MASK) && !(sess->connect & CONNECT_OK))
    {
        if (sess->connect == event->type)
        {
            M_print (i18n (885, "Connection v8 timed out.\n"));
            sess->connect = 0;
            sockclose (sess->sok);
            sess->sok = -1;
        }
        else
        {
            event->due = time (NULL) + 10;
            event->type = sess->connect;
            QueueEnqueue (queue, event);
            return;
        }
    }
    free (event);
}

void SrvCallBackReceive (Session *sess)
{
    Packet *pak;

    pak = UtilIOReceiveTCP (sess);
    
    if (!pak)
        return;

    if (PacketRead1 (pak) != 0x2a)
    {
        if (prG->verbose)
            M_print (i18n (880, "Incoming packet is not a FLAP: id is %d.\n"), PacketRead1 (pak));
        return;
    }
    
    pak->cmd = PacketRead1 (pak);
    pak->id =  PacketReadB2 (pak);
               PacketReadB2 (pak);
    
    if (prG->verbose & 128)
    {
        Time_Stamp ();
        M_print (" " ESC "«" COLSERV "%s ", i18n (879, COLSERV "Incoming v8 server packet:"));
        FlapPrint (pak);
        M_print (ESC "»\r");
    }
    
    QueueEnqueueData (queue, sess, pak->id, QUEUE_TYPE_FLAC,
                      0, time (NULL),
                      pak, NULL, &SrvCallBackFlap);
    pak = NULL;
}
