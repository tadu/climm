
#include "micq.h"
#include "util.h"
#include "util_ui.h"
#include "preferences.h"
#include "cmd_pkt_v8_flap.h"
#include "cmd_pkt_v8_snac.h"
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

jump_sess_f SrvCallBackReceive;
static void SrvCallBackTimeout (struct Event *event);

void SessionInitServer (Session *sess)
{
    int rc, flags;
    struct sockaddr_in sin;
    struct hostent *host;

    sess->connect = 0;
    
    if (!sess->server || !*sess->server || !sess->server_port)
        return;

    M_print (i18n (871, "Opening v8 connection to %s:%d... "), sess->server, sess->server_port);

    sess->sok = socket (AF_INET, SOCK_STREAM, 0);
    if (sess->sok <= 0)
    {
        sess->sok = -1;
        rc = errno;
        M_print (i18n (872, "failed: %s (%d)\n"), strerror (rc), rc);
        return;
    }

    flags = fcntl (sess->sok, F_GETFL, 0);
    if (flags != -1)
        flags = fcntl (sess->sok, F_SETFL, flags | O_NONBLOCK);
    if (flags == -1 && prG->verbose)
    {
        rc = errno;
        M_print (i18n (872, "failed: %s (%d)\n"), strerror (rc), rc);
        sockclose (sess->sok);
        sess->sok = -1;
        return;
    }

    sin.sin_family = AF_INET;
    sin.sin_port = htons (sess->server_port);

    sin.sin_addr.s_addr = inet_addr (sess->server);
    if (sin.sin_addr.s_addr == -1)
    {
        host = gethostbyname (sess->server);
        if (!host)
        {
#ifdef HAVE_HSTRERROR
            M_print (i18n (874, "failed: can't find hostname %s: %s."), sess->server, hstrerror (h_errno));
#else
            M_print (i18n (875, "failed: can't find hostname %s."), sess->server);
#endif
            M_print ("\n");
            return;
        }
        sin.sin_addr = *((struct in_addr *) host->h_addr);
    }
    
    sess->connect = 1 | CONNECT_SELECT_R;
    sess->dispatch = SrvCallBackReceive;

    rc = connect (sess->sok, (struct sockaddr *) &sin, sizeof (struct sockaddr));

    sess->our_seq = rand () & 0x7fff;
    if (rc >= 0)
    {
        sess->connect++;
        M_print (i18n (873, "ok.\n"));
        QueueEnqueueData (queue, sess, sess->our_seq, sess->connect,
                          sess->uin, time (NULL) + 10,
                          NULL, NULL, &SrvCallBackTimeout);
        return;
    }

    if ((rc = errno) == EINPROGRESS)
    {
        M_print ("\n");
        QueueEnqueueData (queue, sess, sess->our_seq, sess->connect,
                          sess->uin, time (NULL) + 10,
                          NULL, NULL, &SrvCallBackTimeout);
        return;
    }
    M_print (i18n (872, "failed: %s (%d)\n"), strerror (rc), rc);
    sess->connect = 0;
    sockclose (sess->sok);
    sess->sok = -1;
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
            QueueEnqueue (queue, event);
            return;
        }
    }
    free (event);
}

#define pak sess->incoming

void SrvCallBackReceive (Session *sess)
{
    int rc;

    if (!pak)
        pak = PacketC ();
    
    if ((sess->connect & CONNECT_MASK) == 1)
        sess->connect ++;
    
    if (pak->len >= 6)
    {
        assert (PacketReadBAt2 (pak, 4) + 6 < sizeof (pak->data));
    }
    
    rc = sockread (sess->sok, pak->data + pak->len,
                   6 + PacketReadBAt2 (pak, 4) - pak->len);
    if (rc == -1)
    {
        rc = errno;
        if (rc == EAGAIN)
            return;
        M_print (i18n (878, "Error while reading from socket: %s (%d)\n"), strerror (rc), rc);
        sess->connect = 0;
        sockclose (sess->sok);
        sess->sok = -1;
        return;
    }
    pak->len += rc;
    if (pak->len < 6 + PacketReadBAt2 (pak, 4))
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
        M_print (" " ESC "«");
        M_print (i18n (879, COLSERV "Incoming v8 server packet (FLAP): channel %d seq %08x length %d" COLNONE "\n"),
                 pak->cmd, pak->id, pak->len - 6);
        if (pak->len > 6)
        {
            if (pak->cmd == 2 && pak->len >= 16)
            {
                M_print (i18n (905, "SNAC (%x,%x) [%s] flags %x ref %x\n"),
                         PacketReadBAt2 (pak, 6), PacketReadBAt2 (pak, 8),
                         SnacName (PacketReadBAt2 (pak, 6), PacketReadBAt2 (pak, 8)),
                         PacketReadBAt2 (pak, 10), PacketReadBAt4 (pak, 12));
                Hex_Dump (pak->data + 16, pak->len - 16);
            }
            else
                Hex_Dump (pak->data + 6, pak->len - 6);
        }
        M_print (ESC "»");
        M_print ("\n");
    }
    
    QueueEnqueueData (queue, sess, pak->id, QUEUE_TYPE_FLAC,
                      0, time (NULL),
                      pak, NULL, &SrvCallBackFlap);
    pak = NULL;
}
