/*
 * Assorted helper functions for doing I/O.
 *
 * Copyright: various.
 *
 * $Id$
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <assert.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include "micq.h"
#include "preferences.h"
#include "util_ui.h"




/*
 * Connects to hostname on port port
 * hostname can be FQDN or IP
 * write out messages to the FD aux
 */
SOK_T UtilIOConnectUDP (char *hostname, int port)
{
/* SOCKS5 stuff begin */
    int res;
    char buf[64];
    struct sockaddr_in s5sin;
    int s5Sok;
    unsigned short s5OurPort;
    unsigned long s5IP;
/* SOCKS5 stuff end */

    int conct, length;
    int sok;
    struct sockaddr_in sin;     /* used to store inet addr stuff */
    struct hostent *host_struct;        /* used in DNS lookup */

    sok = socket (AF_INET, SOCK_DGRAM, 0);      /* create the unconnected socket */

    if (sok == -1)
    {
        perror (i18n (55, "Socket creation failed"));
        exit (1);
    }
    if (prG->verbose)
    {
        M_print (i18n (56, "Socket created attempting to connect\n"));
    }

    if (prG->s5Use)
    {
        sin.sin_addr.s_addr = INADDR_ANY;
        sin.sin_family = AF_INET;
        sin.sin_port = 0;

        if (bind (sok, (struct sockaddr *) &sin, sizeof (struct sockaddr)) < 0)
        {
            M_print (i18n (819, "Can't bind socket to free port\n"));
            return -1;
        }

        length = sizeof (sin);
        getsockname (sok, (struct sockaddr *) &sin, &length);
        s5OurPort = ntohs (sin.sin_port);

        s5sin.sin_addr.s_addr = inet_addr (prG->s5Host);
        if (s5sin.sin_addr.s_addr == (unsigned long) -1)        /* name isn't n.n.n.n so must be DNS */
        {
            host_struct = gethostbyname (prG->s5Host);
            if (!host_struct)
            {
#ifdef HAVE_HSTRERROR
                M_print (i18n (596, "[SOCKS] Can't find hostname %s: %s."), prG->s5Host, hstrerror (h_errno));
#else
                M_print (i18n (57, "[SOCKS] Can't find hostname %s."), prG->s5Host);
#endif
                M_print ("\n");
                return -1;
            }
            s5sin.sin_addr = *((struct in_addr *) host_struct->h_addr);
        }
        s5IP = ntohl (s5sin.sin_addr.s_addr);
        s5sin.sin_family = AF_INET;     /* we're using the inet not appletalk */
        s5sin.sin_port = htons (prG->s5Port);        /* port */
        s5Sok = socket (AF_INET, SOCK_STREAM, 0);       /* create the unconnected socket */
        if (s5Sok == -1)
        {
            M_print (i18n (597, "[SOCKS] Socket creation failed\n"));
            return -1;
        }
        conct = connect (s5Sok, (struct sockaddr *) &s5sin, sizeof (s5sin));
        if (conct == -1)        /* did we connect ? */
        {
            M_print (i18n (598, "[SOCKS] Connection refused\n"));
            return -1;
        }
        buf[0] = 5;             /* protocol version */
        buf[1] = 1;             /* number of methods */
        if (!strlen (prG->s5Name) || !strlen (prG->s5Pass) || !prG->s5Auth)
            buf[2] = 0;         /* no authorization required */
        else
            buf[2] = 2;         /* method username/password */
        send (s5Sok, buf, 3, 0);
        res = recv (s5Sok, buf, 2, 0);
        if (strlen (prG->s5Name) && strlen (prG->s5Pass) && prG->s5Auth)
        {
            if (res != 2 || buf[0] != 5 || buf[1] != 2) /* username/password authentication */
            {
                M_print (i18n (599, "[SOCKS] Authentication method incorrect\n"));
                close (s5Sok);
                return -1;
            }
            buf[0] = 1;         /* version of subnegotiation */
            buf[1] = strlen (prG->s5Name);
            memcpy (&buf[2], prG->s5Name, buf[1]);
            buf[2 + buf[1]] = strlen (prG->s5Pass);
            memcpy (&buf[3 + buf[1]], prG->s5Pass, buf[2 + buf[1]]);
            send (s5Sok, buf, buf[1] + buf[2 + buf[1]] + 3, 0);
            res = recv (s5Sok, buf, 2, 0);
            if (res != 2 || buf[0] != 1 || buf[1] != 0)
            {
                M_print (i18n (600, "[SOCKS] Authorization failure\n"));
                close (s5Sok);
                return -1;
            }
        }
        else
        {
            if (res != 2 || buf[0] != 5 || buf[1] != 0) /* no authentication required */
            {
                M_print (i18n (599, "[SOCKS] Authentication method incorrect\n"));
                close (s5Sok);
                return -1;
            }
        }
        buf[0] = 5;             /* protocol version */
        buf[1] = 3;             /* command UDP associate */
        buf[2] = 0;             /* reserved */
        buf[3] = 1;             /* address type IP v4 */
        buf[4] = (char) 0;
        buf[5] = (char) 0;
        buf[6] = (char) 0;
        buf[7] = (char) 0;
        *(unsigned short *) &buf[8] = htons (s5OurPort);
/*     memcpy(&buf[8], &s5OurPort, 2); */
        send (s5Sok, buf, 10, 0);
        res = recv (s5Sok, buf, 10, 0);
        if (res != 10 || buf[0] != 5 || buf[1] != 0)
        {
            M_print (i18n (601, "[SOCKS] General SOCKS server failure\n"));
            close (s5Sok);
            return -1;
        }
    }

    sin.sin_addr.s_addr = inet_addr (hostname);
    if (sin.sin_addr.s_addr == -1)      /* name isn't n.n.n.n so must be DNS */
    {
        host_struct = gethostbyname (hostname);
        if (!host_struct)
        {
            if (prG->verbose)
            {
#ifdef HAVE_HSTRERROR
                M_print (i18n (58, "Can't find hostname %s: %s."), hostname, hstrerror (h_errno));
#else
                M_print (i18n (772, "Can't find hostname %s."), hostname);
#endif
                M_print ("\n");
            }
            return -1;
        }
        sin.sin_addr = *((struct in_addr *) host_struct->h_addr);
    }
    sin.sin_family = AF_INET;
    sin.sin_port = htons (port);

    if (prG->s5Use)
    {
        prG->s5DestIP = ntohl (sin.sin_addr.s_addr);
        memcpy (&sin.sin_addr.s_addr, &buf[4], 4);

        sin.sin_family = AF_INET;
        prG->s5DestPort = port;
        memcpy (&sin.sin_port, &buf[8], 2);
    }

    conct = connect (sok, (struct sockaddr *) &sin, sizeof (sin));

    if (conct == -1)            /* did we connect ? */
    {
        if (prG->verbose)
        {
            M_print (i18n (54, " Conection Refused on port %d at %s\n"), port, hostname);
            perror ("connect");
        }
        return -1;
    }

    length = sizeof (sin);
    getsockname (sok, (struct sockaddr *) &sin, &length);
    if (prG->sess)
        prG->sess->our_local_ip = ntohl (sin.sin_addr.s_addr);

    if (prG->verbose)
    {
        M_print (i18n (53, "Connected to %s, waiting for response\n"), hostname);
    }

    return sok;
}

size_t SOCKREAD (Session *sess, void *ptr, size_t len)
{
    size_t sz;

    sz = sockread (sess->sok, ptr, len);
    sess->stat_pak_rcvd++;

/* SOCKS5 stuff begin */
    if (prG->s5Use)
    {
        sz -= 10;
        memcpy (ptr, ptr + 10, sz);
    }
/* SOCKS5 stuff end */

    return sz;
}

/*
 * Send a given packet to the session's socket.
 * Use socks if requested.
 */
void UtilIOSend (Session *sess, Packet *pak)
{
    size_t s5len = 0;
    UBYTE *body = NULL, *data = pak->data;

    if (prG->s5Use)
    {
        s5len = 10;
        body = malloc (pak->len + s5len);

        assert (body);
        
        body[0] = 0;
        body[1] = 0;
        body[2] = 0;
        body[3] = 1;
        *(UDWORD *) &body[4] = htonl (prG->s5DestIP);
        *(UWORD  *) &body[8] = htons (prG->s5DestPort);
        memcpy (body + s5len, data, pak->len);
        data = body;
    }
    sockwrite (sess->sok, data, pak->len + s5len);
    if (body)
        free (body);
    sess->stat_pak_sent++;
}

/*
 * Receive a single packet via UDP.
 * Note this won't work for TCP - UDP packets are always received in one part.
 */
Packet *UtilIORecvUDP (Session *sess)
{
    Packet *pak;
    int s5len;
    
    s5len = prG->s5Use ? 10 : 0;
    pak = PacketC ();
    
    pak->len = sockread (sess->sok, prG->s5Use ? pak->socks : pak->data, sizeof (pak->data) + s5len);
    
    if (pak->len <= 4 + s5len)
    {
        PacketD (pak);
        return NULL;
    }
    pak->len -= s5len;
    
    return pak;
}

/**************************************************************
Same as M_print but for FD_T's
***************************************************************/
void M_fdprint (FD_T fd, const char *str, ...)
{
    va_list args;
    int k;
    char buf[2048];

    va_start (args, str);
    vsnprintf (buf, sizeof (buf), str, args);
    k = write (fd, buf, strlen (buf));
    if (k != strlen (buf))
    {
        perror (str);
        exit (10);
    }
    va_end (args);
}

/***********************************************************
Reads a line of input from the file descriptor fd into buf
an entire line is read but no more than len bytes are 
actually stored
************************************************************/
int M_fdnreadln (FILE *fd, char *buf, size_t len)
{
    if (!fgets (buf, len, fd))
        return -1;
    
    if (buf[strlen(buf) - 1] == '\n')
        buf[strlen(buf) - 1] = '\0';
    if (buf[strlen(buf) - 1] == '\r')
        buf[strlen(buf) - 1] = '\0';
    return 0;
}

