/*
 * This file handles incoming and outgoing file requests.
 *
 * This file is Copyright © Rüdiger Kuhlmann; it may be distributed under
 * version 2 of the GPL licence.
 *
 * $Id$
 */

#include "micq.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <sys/stat.h>

#include "preferences.h"
#include "session.h"
#include "packet.h"
#include "util_ui.h"
#include "util_io.h"
#include "util_str.h"
#include "util.h"
#include "tcp.h"
#include "conv.h"

static void PeerFileDispatchClose   (Connection *ffile);
static void PeerFileDispatchDClose  (Connection *ffile);
static void PeerFileIODispatchClose (Connection *ffile);


/*
 * Create a new file listener unless one already exists.
 */
Connection *PeerFileCreate (Connection *serv)
{
    Connection *flist;
    
    ASSERT_ANY_SERVER(serv);
    assert (serv->assoc);
    assert (serv->assoc->ver >= 6);
    assert (serv->assoc->ver <= 8);
    
    if ((flist = ConnectionFind (TYPE_FILELISTEN, 0, serv)))
        return flist;
    
    flist = ConnectionClone (serv->assoc, TYPE_FILELISTEN);
    if (!flist)
        return NULL;

    if (prG->verbose)
        M_printf (i18n (2082, "Opening file listener connection at localhost:%d... "), serv->assoc->spref->port);

    flist->spref       = NULL;
    flist->parent      = serv;
    flist->connect     = 0;
    flist->flags       = 0;
    flist->our_seq     = -1;
    flist->our_session = 0;
    flist->port        = serv->assoc->spref->port;
    flist->server      = NULL;
    flist->ip          = 0;
    flist->dispatch    = &TCPDispatchMain;
    flist->reconnect   = &TCPDispatchReconn;
    flist->close       = &PeerFileDispatchClose;
    
    UtilIOConnectTCP (flist);
    
    return flist;
}

/*
 * Handles an incoming file request.
 */
BOOL PeerFileRequested (Connection *peer, const char *files, UDWORD bytes)
{
    Connection *flist, *fpeer;
    Contact *cont;
    struct stat finfo;
    char buf[300];

    ASSERT_MSGDIRECT(peer);
    
    if (peer->ver < 6)
        return 0;
    
    cont = ContactFind (peer->uin);
    assert (cont);
    
    if (cont->flags & CONT_TEMPORARY)
        return 0;

    flist = PeerFileCreate (peer->parent->parent);
    if (!flist)
        return 0;
    ASSERT_FILELISTEN (flist);
    
    snprintf (buf, sizeof (buf), "%sfiles/%ld", PrefUserDir (prG), peer->uin);
    if (stat (buf, &finfo))
    {
        M_printf ("%s " COLACK "%10s" COLNONE " ", s_now, cont->nick);
        M_printf (i18n (2193, "Directory %s does not exists.\n"), buf);
        return 0;
    }
    
    fpeer = ConnectionClone (flist, TYPE_FILEDIRECT);
    if (!fpeer)
        return 0;

    fpeer->port    = 0;
    fpeer->ip      = 0;
    fpeer->connect = 0;
    fpeer->server  = NULL;
    fpeer->uin     = peer->uin;
    fpeer->len     = bytes;
    fpeer->done    = 0;
    fpeer->close   = &PeerFileDispatchDClose;
    
    return 1;
}

/*
 * Checks the file request response.
 */
BOOL PeerFileAccept (Connection *peer, UWORD status, UDWORD port)
{
    Connection *flist, *fpeer;
    
    flist = PeerFileCreate (peer->parent->parent);
    fpeer = ConnectionFind (TYPE_FILEDIRECT, peer->uin, flist);
    
    if (!flist || !fpeer || !port || (status == TCP_STAT_REFUSE))
    {
        if (fpeer)
            TCPClose (fpeer);

        return 0;
    }

    ASSERT_MSGDIRECT(peer);
    ASSERT_FILELISTEN(flist);
    ASSERT_FILEDIRECT(fpeer);
    
    fpeer->connect  = 0;
    fpeer->type     = TYPE_FILEDIRECT;
    fpeer->flags    = 0;
    fpeer->our_seq  = 0;
    fpeer->port     = port;
    fpeer->ip       = peer->ip;
    fpeer->server   = NULL;
    fpeer->close    = &PeerFileDispatchDClose;
    
    if (prG->verbose)
        M_printf (i18n (2068, "Opening file transfer connection at %s:%d... \n"),
                 fpeer->server = strdup (s_ip (fpeer->ip)), fpeer->port);

    TCPDispatchConn (fpeer);
    
    return 1;
}

#define FAIL(x) { err = x; break; }
#define PeerFileClose TCPClose

/*
 * Close a file listener.
 */
static void PeerFileDispatchClose (Connection *flist)
{
    flist->connect = 0;
    PeerFileClose (flist);
}

/*
 * Close a file transfer connection.
 */
static void PeerFileDispatchDClose (Connection *fpeer)
{
    fpeer->connect = 0;
    PeerFileClose (fpeer);
    R_resetprompt ();
}

/*
 * Close a file i/o connection.
 */
static void PeerFileIODispatchClose (Connection *ffile)
{
    if (ffile->sok != -1)
        close (ffile->sok);
    ffile->sok = -1;
    ffile->connect = 0;
    R_resetprompt ();
}

/*
 * Dispatches incoming packets on the file transfer connection.
 */
void PeerFileDispatch (Connection *fpeer)
{
    Contact *cont;
    Packet *pak;
    UDWORD err = 0;
    
    ASSERT_FILEDIRECT (fpeer);
    
    if (!(pak = UtilIOReceiveTCP (fpeer)))
        return;

    if (prG->verbose & DEB_PACKTCP)
        TCPPrint (pak, fpeer, FALSE);

    cont = ContactFind (fpeer->uin);
    assert (cont);

    switch (PacketRead1 (pak))
    {
        char *name, *text;
        UDWORD len, off, nr, speed;

        case 0:
                   PacketRead4 (pak); /* EMPTY */
            nr   = PacketRead4 (pak); /* COUNT */
            len  = PacketRead4 (pak); /* BYTES */
            speed= PacketRead4 (pak); /* SPEED */
            name = PacketReadLNTS (pak); /* NICK  */
            PacketD (pak);
            
            M_printf ("%s " COLCONTACT "%10s" COLNONE " ", s_now, cont->nick);
            M_printf (i18n (2161, "Receiving %d files with together %d bytes at speed %x from %s.\n"),
                     nr, len, speed, c_in (name));
            
            if (len != fpeer->len)
            {
                M_printf ("FIXME: byte len different than in file request: requested %d, sending %d.\n",
                         fpeer->len, len);
                fpeer->len = len;
            }
            
            pak = PeerPacketC (fpeer, 1);
            PacketWrite4 (pak, 64);
            PacketWriteLNTS (pak, "my Nick0");
            PeerPacketSend (fpeer, pak);
            
            free (name);
            return;
        
        case 1:
            speed= PacketRead4 (pak); /* SPEED */
            name = PacketReadLNTS (pak); /* NICK  */
            PacketD (pak);
            
            M_printf ("%s " COLCONTACT "%10s" COLNONE " ", s_now, cont->nick);
            M_printf (i18n (2170, "Sending with speed %x to %s.\n"), speed, c_in (name));
            
            fpeer->our_seq = 1;
            QueueRetry (fpeer, QUEUE_PEER_FILE, fpeer->uin);
            
            free (name);
            return;
            
        case 2:
                   PacketRead1 (pak); /* EMPTY */
            name = PacketReadLNTS (pak);
            text = PacketReadLNTS (pak);
            len  = PacketRead4 (pak);
                   PacketRead4 (pak); /* EMPTY */
                   PacketRead4 (pak); /* SPEED */
            off  = 0;
            PacketD (pak);
            
            {
                Connection *ffile = ConnectionClone (fpeer, TYPE_FILE);
                char buf[200], *p;
                int pos = 0;
                struct stat finfo;

                assert (ffile);
                snprintf (buf, sizeof (buf), "%s/files/%ld/%n%s", PrefUserDir (prG), fpeer->uin, &pos, c_in (name));
                for (p = buf + pos; *p; p++)
                    if (*p == '/')
                        *p = '_';
                finfo.st_size = 0;
                if (!stat (buf, &finfo))
                    if (finfo.st_size < len)
                        off = finfo.st_size;
                fpeer->assoc = ffile;
                ffile->sok = open (buf, O_CREAT | O_WRONLY | (off ? O_APPEND : O_TRUNC), 0660);
                if (ffile->sok == -1)
                {
                    int rc = errno;
                    M_printf ("%s " COLCONTACT "%10s" COLNONE " ", s_now, cont->nick);
                    M_printf (i18n (2083, "Cannot open file %s: %s (%d).\n"),
                             buf, strerror (rc), rc);
                    ConnectionClose (fpeer);
                    free (name);
                    return;
                }
                ffile->connect = CONNECT_OK;
                ffile->len = len;
                ffile->done = off;
                ffile->close = &PeerFileIODispatchClose;

                M_printf ("%s " COLCONTACT "%10s" COLNONE " ", s_now, cont->nick);
                M_printf (i18n (2162, "Receiving file %s (%s) with %d bytes as %s.\n"),
                         name, text, len, buf);
            }
            pak = PeerPacketC (fpeer, 3);
            PacketWrite4 (pak, off);
            PacketWrite4 (pak, 0);
            PacketWrite4 (pak, 64);
            PacketWrite4 (pak, 1);
            PeerPacketSend (fpeer, pak);
            
            free (name);
            return;

        case 3:
            off = PacketRead4 (pak);
                  PacketRead4 (pak); /* EMPTY */
                  PacketRead4 (pak); /* SPEED */
            nr  = PacketRead4 (pak); /* NR */
            PacketD (pak);
            
            M_printf ("%s " COLCONTACT "%10s" COLNONE " ", s_now, cont->nick);
            M_printf (i18n (2163, "Sending file %d at offset %d.\n"),
                     nr, off);
            
            err = lseek (fpeer->assoc->sok, off, SEEK_SET);
            if (err == -1)
            {
                err = errno;
                M_printf ("%s " COLCONTACT "%10s" COLNONE " ", s_now, cont->nick);
                M_printf (i18n (2084, "Error while seeking to offset %d: %s (%d).\n"),
                         off, strerror (err), err);
                TCPClose (fpeer);
                return;
            }
            fpeer->assoc->done = off;
            fpeer->assoc->connect = CONNECT_OK;
            
            QueueRetry (fpeer, QUEUE_PEER_FILE, fpeer->uin);
            return;
            
        case 4:
            M_printf ("%s " COLCONTACT "%10s" COLNONE " ", s_now, cont->nick);
            M_printf (i18n (2169, "File transfer aborted by peer (%d).\n"),
                     PacketRead1 (pak));
            PacketD (pak);
            PeerFileClose (fpeer);
            return;

        case 5:
            M_printf ("FIXME: Ignoring speed change to %d.\n",
                     PacketRead1 (pak));
            PacketD (pak);
            return;

        case 6:
            len = write (fpeer->assoc->sok, pak->data + 1, pak->len - 1);
            if (len != pak->len - 1)
            {
                M_printf ("%s " COLCONTACT "%10s" COLNONE " ", s_now, cont->nick);
                M_print  (i18n (2164, "Error writing to file.\n"));
                TCPClose (fpeer);
                return;
            }
            fpeer->assoc->done += len;
            if (fpeer->assoc->done > fpeer->assoc->len)
            {
                M_printf ("%s " COLCONTACT "%10s" COLNONE " ", s_now, cont->nick);
                M_printf (i18n (2165, "The peer sent more bytes (%d) than file length (%d).\n"),
                         fpeer->assoc->done, fpeer->assoc->len);
                TCPClose (fpeer);
                return;
            }
            else if (fpeer->assoc->len == fpeer->assoc->done)
            {
                R_resetprompt ();
                M_printf ("%s " COLCONTACT "%10s" COLNONE " ", s_now, cont->nick);
                M_print  (i18n (2166, "Finished receiving file.\n"));
            }
            else if (fpeer->assoc->len)
            {
                R_setpromptf ("[" COLINCOMING "%d %02d%%" COLNONE "] " COLSERVER "%s" COLNONE "",
                              fpeer->assoc->done, (int)((100.0 * fpeer->assoc->done) / fpeer->assoc->len),
                              i18n (1040, "mICQ> "));
            }
                                          
            PacketD (pak);
            return;
        default:
            M_printf ("%s " COLCONTACT "%10s" COLNONE " ", s_now, cont->nick);
            M_print  (i18n (2167, "Error - unknown packet.\n"));
            Hex_Dump (pak->data, pak->len);
            PacketD (pak);
            PeerFileClose (fpeer);
    }
    if ((prG->verbose & DEB_TCP) && err)
    {
        M_printf ("%s %s: %d\n", s_now, i18n (2029, "Protocol error on peer-to-peer connection"), err);
        PeerFileClose (fpeer);
    }
}

void PeerFileDispatchW (Connection *fpeer)
{
    Packet *pak = fpeer->outgoing;
    
    fpeer->outgoing = NULL;
    fpeer->connect = CONNECT_OK | CONNECT_SELECT_R;
    fpeer->dispatch = &PeerFileDispatch;
    fpeer->assoc->connect = CONNECT_OK;

    if (!UtilIOSendTCP (fpeer, pak))
        TCPClose (fpeer);
    
    QueueRetry (fpeer, QUEUE_PEER_FILE, fpeer->uin);
}

BOOL PeerFileError (Connection *fpeer, UDWORD rc, UDWORD flags)
{
    switch (rc)
    {
        case EPIPE:
            if (fpeer->close)
                fpeer->close (fpeer);
            return 1;
        case EAGAIN:
            if (flags == CONNERR_WRITE)
            {
                fpeer->connect = CONNECT_OK | CONNECT_SELECT_W;
                fpeer->dispatch = &PeerFileDispatchW;
                fpeer->assoc->connect = CONNECT_OK | 1;
                return 1;
            }
    }
    return 0;
}

void PeerFileResend (Event *event)
{
    Contact *cont;
    Connection *fpeer = event->conn;
    Packet *pak;
    Event *event2;
    int rc;
    
    if (!fpeer)
    {
        free (event->info);
        free (event);
        return;
    }
    
    ASSERT_FILEDIRECT (fpeer);

    cont = ContactFind (event->uin);
    assert (cont);

    if (event->attempts >= MAX_RETRY_ATTEMPTS || (!event->pak && !event->seq))
    {
        M_printf ("%s " COLCONTACT "%10s" COLNONE " ", s_now, cont->nick);
        M_printf (i18n (2168, "File transfer #%d (%s) dropped after %d attempts because of timeout.\n"),
                 event->seq, event->info, event->attempts);
        TCPClose (fpeer);
    }
    else if (!(fpeer->connect & CONNECT_MASK))
    {
        M_printf ("%s " COLCONTACT "%10s" COLNONE " ", s_now, cont->nick);
        M_printf (i18n (2072, "File transfer #%d (%s) dropped because of closed connection.\n"),
                 event->seq, event->info);
    }
    else if (~fpeer->connect & CONNECT_OK)
    {
        if (event->attempts > 1)
        {
            M_printf ("%s " COLACK "%10s" COLNONE " %s [%d] %x %s\n",
                     s_now, cont->nick, " + ", event->attempts, fpeer->connect, event->info);
        }
        if (!event->seq)
            event->attempts++;
        event->due = time (NULL) + 10;
        QueueEnqueue (event);
        return;
    }
    else if (!event->seq)
    {
        fpeer->our_seq = 0;
        PeerPacketSend (fpeer, event->pak);
        PacketD (event->pak);
        event->pak = NULL;
    }
    else if (event->seq != fpeer->our_seq)
    {
        event->due = time (NULL) + 10;
        QueueEnqueue (event);
        return;
    }
    else if (event->pak)
    {
        Connection *ffile;
        struct stat finfo;
        
        PeerPacketSend (fpeer, event->pak);
        PacketD (event->pak);
        event->pak = NULL;
        QueueEnqueue (event);
        
        ffile = ConnectionClone (fpeer, TYPE_FILE);
        fpeer->assoc = ffile;

        if (stat (event->info, &finfo))
        {
            rc = errno;
            M_printf (i18n (2071, "Couldn't stat file %s: %s (%d)\n"),
                     event->info, strerror (rc), rc);
        }
        ffile->len = finfo.st_size;

        ffile->sok = open (event->info, O_RDONLY);
        if (ffile->sok == -1)
        {
            int rc = errno;
            M_printf ("%s " COLCONTACT "%10s" COLNONE " ", s_now, cont->nick);
            M_printf (i18n (2083, "Cannot open file %s: %s (%d).\n"),
                     event->info, strerror (rc), rc);
            TCPClose (fpeer);
            ConnectionClose (ffile);
            ConnectionClose (fpeer);
            return;
        }
        ffile->close = &PeerFileIODispatchClose;
        return;
    }
    else if (!fpeer->assoc || fpeer->assoc->connect != CONNECT_OK)
    {
        event->attempts++;
        event->due = time (NULL) + 3;
        QueueEnqueue (event);
        return;
    }
    else
    {
        int len = 0;

        pak = PeerPacketC (fpeer, 6);
        len = read (fpeer->assoc->sok, pak->data + 1, 2048);
        if (len == -1)
        {
            len = errno;
            M_printf ("%s " COLCONTACT "%10s" COLNONE " ", s_now, cont->nick);
            M_printf (i18n (2086, "Error while reading file %s: %s (%d).\n"),
                     event->info, strerror (len), len);
            TCPClose (fpeer);
        }
        else
        {
            pak->len += len;
            fpeer->assoc->done += len;
            fpeer->error = &PeerFileError;
            PeerPacketSend (fpeer, pak);
            PacketD (pak);

            if (len == 2048)
            {
                if (fpeer->assoc->len)
                    R_setpromptf ("[" COLCONTACT "%d %02d%%" COLNONE "] " COLSERVER "%s" COLNONE "",
                                  fpeer->assoc->done, (int)((100.0 * fpeer->assoc->done) / fpeer->assoc->len),
                                  i18n (1040, "mICQ> "));
                else
                    R_setpromptf ("[" COLCONTACT "%d" COLNONE "] " COLSERVER "%s" COLNONE "",
                                  fpeer->assoc->done, i18n (1040, "mICQ> "));
                event->attempts = 0;
                QueueEnqueue (event);
                return;
            }

            R_resetprompt ();
            M_printf ("%s " COLCONTACT "%10s" COLNONE " ", s_now, cont->nick);
            M_printf (i18n (2087, "Finished sending file %s.\n"), event->info);
            ConnectionClose (fpeer->assoc);
            fpeer->our_seq++;
            event2 = QueueDequeue (fpeer, QUEUE_PEER_FILE, fpeer->our_seq);
            if (event2)
            {
                QueueEnqueue (event2);
                QueueRetry (fpeer, QUEUE_PEER_FILE, fpeer->uin);
                return;
            }
            else
            {
                M_printf ("%s " COLCONTACT "%10s" COLNONE " ", s_now, cont->nick);
                M_printf (i18n (2088, "Finished sending all %d files.\n"), fpeer->our_seq - 1);
                ConnectionClose (fpeer);
            }
        }
    }
    free (event->info);
    free (event);
}
