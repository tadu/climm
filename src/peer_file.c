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
#include "util.h"
#include "tcp.h"

/*
 * Create a new file listener unless one already exists.
 */
Session *PeerFileCreate (Session *serv)
{
    Session *flist;
    
    ASSERT_ANY_SERVER(serv);
    assert (serv->assoc);
    assert (serv->assoc->ver >= 6);
    assert (serv->assoc->ver <= 8);
    
    if ((flist = SessionFind (TYPE_FILELISTEN, 0, serv)))
        return flist;
    
    if (prG->verbose)
        M_print (i18n (9999, "Opening file listener connection at localhost:%d... "), serv->assoc->spref->port);

    flist = SessionClone (serv->assoc, TYPE_FILELISTEN);
    if (!flist)
        return NULL;

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
    
    UtilIOConnectTCP (flist);
    
    return flist;
}

/*
 * Handles an incoming file request.
 */
BOOL PeerFileRequested (Session *peer, const char *files, UDWORD bytes)
{
    Session *flist, *fpeer;
    Contact *cont;

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
    
    fpeer = SessionClone (flist, TYPE_FILEDIRECT);
    if (!fpeer)
        return 0;

    fpeer->port     = 0;
    fpeer->ip       = 0;
    fpeer->connect  = 0;
    fpeer->server   = NULL;
    fpeer->uin      = peer->uin;
    fpeer->our_seq3 = bytes;
    
    return 1;
}

/*
 * Checks the file request response.
 */
BOOL PeerFileAccept (Session *peer, UWORD status, UDWORD port)
{
    Session *flist, *fpeer;
    
    flist = PeerFileCreate (peer->parent->parent);
    fpeer = SessionFind (TYPE_FILEDIRECT, peer->uin, flist);
    
    ASSERT_MSGDIRECT(peer);
    ASSERT_FILELISTEN(flist);
    ASSERT_FILEDIRECT(fpeer);
    
    if (!flist || !fpeer || !port || status)
    {
        if (fpeer)
            TCPClose (fpeer);

        return 0;
    }

    if (prG->verbose)
        M_print (i18n (2068, "Opening file transfer connection at %s:%d... \n"),
                 fpeer->server = strdup (UtilIOIP (fpeer->ip)), fpeer->port);

    fpeer->connect  = 0;
    fpeer->type     = TYPE_FILEDIRECT;
    fpeer->flags    = 0;
    fpeer->our_seq  = 0;
    fpeer->port     = port;
    fpeer->ip       = peer->ip;
    fpeer->server   = NULL;
    
    TCPDispatchConn (fpeer);
    
    return 1;
}

#define FAIL(x) { err = x; break; }
#define PeerFileClose TCPClose

/*
 * Dispatches incoming packets on the file transfer connection.
 */
void PeerFileDispatch (Session *fpeer)
{
    Packet *pak;
    UDWORD err = 0;
    
    ASSERT_FILEDIRECT (fpeer);
    
    if (!(pak = UtilIOReceiveTCP (fpeer)))
        return;

    if (prG->verbose & DEB_PACKTCP)
        TCPPrint (pak, fpeer, FALSE);

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
            
            M_print ("Incoming initialization: %d files with together %d bytes @ %x from %s.\n",
                     nr, len, speed, name);
            
            pak = PacketC ();
            PacketWrite1 (pak, 1);
            PacketWrite4 (pak, 64);
            PacketWriteLNTS (pak, "my Nick0");
            TCPSendPacket (pak, fpeer);
            
            free (name);
            return;
        
        case 1:
            speed= PacketRead4 (pak); /* SPEED */
            name = PacketReadLNTS (pak); /* NICK  */
            PacketD (pak);
            
            M_print ("Files accepted @ %x by %s.\n", speed, name);
            
            fpeer->our_seq = 1;
            QueueRetry (fpeer->uin, QUEUE_PEER_FILE);
            
            free (name);
            return;
            
        case 2:
                   PacketRead1 (pak); /* EMPTY */
            name = strdup (PacketReadLNTS (pak));
            text = PacketReadLNTS (pak);
            len  = PacketRead4 (pak);
                   PacketRead4 (pak); /* EMPTY */
                   PacketRead4 (pak); /* SPEED */
            off  = 0;
            PacketD (pak);
            
            {
                Session *ffile = SessionClone (fpeer, TYPE_FILE);
                char buf[200], *p;
                int pos = 0;
                struct stat finfo;

                assert (ffile);
                snprintf (buf, sizeof (buf), "%s/files/%ld/%n%s", PrefUserDir (), fpeer->uin, &pos, name);
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
                    M_print (i18n (9999, "Cannot open file %s: %s (%d).\n"),
                             buf, strerror (rc), rc);
                    TCPClose (fpeer);
                }
                fpeer->connect &= ~1;

                M_print ("Starting receiving %s (%s), len %d as %s\n",
                         name, text, len, buf);
            }
            pak = PacketC ();
            PacketWrite1 (pak, 3);
            PacketWrite4 (pak, off);
            PacketWrite4 (pak, 0);
            PacketWrite4 (pak, 64);
            PacketWrite4 (pak, 1);
            TCPSendPacket (pak, fpeer);
            
            free (name);
            return;

        case 3:
            off = PacketRead4 (pak);
                  PacketRead4 (pak); /* EMPTY */
                  PacketRead4 (pak); /* SPEED */
            nr  = PacketRead4 (pak); /* NR */
            PacketD (pak);
            
            M_print ("Sending file %d at offset %d.\n",
                     nr, off);
            
            err = lseek (fpeer->assoc->sok, off, SEEK_SET);
            if (err == -1)
            {
                err = errno;
                M_print (i18n (9999, "Error while seeking to offset %d: %s (%d).\n"),
                         off, strerror (err), err);
                TCPClose (fpeer);
                return;
            }
            fpeer->connect |= 1;
            
            QueueRetry (fpeer->uin, QUEUE_PEER_FILE);
            return;
            
        case 4:
            M_print ("File transfer aborted by peer (%d).\n",
                     PacketRead1 (pak));
            PacketD (pak);
            PeerFileClose (fpeer);
            return;

        case 5:
            M_print ("Ignoring speed change to %d.\n",
                     PacketRead1 (pak));
            PacketD (pak);
            return;

        case 6:
            write (fpeer->assoc->sok, pak->data + 1, pak->len - 1);
            PacketD (pak);
            return;
        default:
            M_print ("Error - unknown packet.\n");
            Hex_Dump (pak->data, pak->len);
            PacketD (pak);
            PeerFileClose (fpeer);
    }
    if ((prG->verbose & DEB_TCP) && err)
    {
        Time_Stamp ();
        M_print (" %s: %d\n", i18n (2029, "Protocol error on peer-to-peer connection"), err);
        PeerFileClose (fpeer);
    }
}

void PeerFileResend (Event *event)
{
    Contact *cont;
    Session *fpeer = event->sess;
    Packet *pak;
    Event *event2;
    
    if (!fpeer)
    {
        free (event->info);
        free (event);
        return;
    }
    
    ASSERT_FILEDIRECT (fpeer);

    cont = ContactFind (event->uin);

    if (event->attempts >= MAX_RETRY_ATTEMPTS || (!event->pak && !event->seq))
    {
        M_print (i18n (9999, "Dropping file transfer because of timeout: %d %d.\n"),
                 event->attempts, event->seq);
        TCPClose (fpeer);
    }

    if (fpeer->connect & CONNECT_MASK)
    {
        if (fpeer->connect & CONNECT_OK)
        {
            if (!event->seq)
            {
                fpeer->our_seq = 0;
                TCPSendPacket (event->pak, fpeer);
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
                Session *ffile;
                
                TCPSendPacket (event->pak, fpeer);
                PacketD (event->pak);
                event->pak = NULL;
                QueueEnqueue (event);
                
                ffile = SessionClone (fpeer, TYPE_FILE);
                fpeer->assoc = ffile;
                ffile->sok = open (event->info, O_RDONLY);
                if (ffile->sok == -1)
                {
                    int rc = errno;
                    M_print (i18n (9999, "Cannot open file %s: %s (%d).\n"),
                             event->info, strerror (rc), rc);
                    TCPClose (fpeer);
                    SessionClose (fpeer);
                    SessionClose (ffile);
                    return;
                }
                fpeer->connect &= ~1;

                return;
            }
            else if (fpeer->connect & 1)
            {
                int len = 0;

                pak = PacketC ();
                PacketWrite1 (pak, 6);
                len = read (fpeer->assoc->sok, pak->data + 1, 2048);
                if (len == -1)
                {
                    len = errno;
                    M_print (i18n (9999, "Error while reading file %s: %s (%d).\n"),
                             event->info, strerror (len), len);
                    TCPClose (fpeer);
                }
                else
                {
                    pak->len += len;
                    TCPSendPacket (pak, fpeer);
                    PacketD (pak);
                    event->due++;

                    if (len == 2048)
                    {
                        QueueEnqueue (event);
                        return;
                    }
                    M_print (i18n (9999, "Finished sending file %s.\n"), event->info);
                    SessionClose (fpeer->assoc);
                    fpeer->our_seq++;
                    event2 = QueueDequeue (fpeer->our_seq, QUEUE_PEER_FILE);
                    if (event2)
                    {
                        QueueEnqueue (event2);
                        QueueRetry (fpeer->uin, QUEUE_PEER_FILE);
                        return;
                    }
                    else
                    {
                        M_print (i18n (9999, "Finished sending all %d files.\n"), fpeer->our_seq - 1);
                        SessionClose (fpeer);
                    }
                }
            }
            else
            {
                event->attempts++;
                event->due += 3;
                QueueEnqueue (event);
                return;
            }
        }
        else
        {
            if (event->attempts > 1)
            {
                Time_Stamp ();
                M_print (" " COLACK "%10s" COLNONE " %s [%d] %x %s\n",
                         cont->nick, " + ", event->attempts, fpeer->connect, event->info);
            }
            if (!event->seq)
                event->attempts++;
            event->due = time (NULL) + 10;
            QueueEnqueue (event);
            return;
        }
    }
    else
    {
        M_print (i18n (2072, "Dropping file transfer %d (%s) because of closed connection.\n"),
                 event->seq, event->info);
    }
    free (event->info);
    free (event);
}
