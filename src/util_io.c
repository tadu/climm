/*
 * Assorted helper functions for doing I/O.
 *
 * Copyright: This file may be distributed under version 2 of the GPL licence.
 *
 * $Id$
 */

#include "micq.h"
#include <errno.h>
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#include <fcntl.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <assert.h>
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#if HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
#if !HAVE_DECL_H_ERRNO
extern int h_errno;
#endif
#include <signal.h>
#include <stdarg.h>
#include "preferences.h"
#include "util_ui.h"
#include "util_io.h"
#include "util_ssl.h"
#include "conv.h"
#include "util.h"
#include "contact.h"
#include "session.h"
#include "packet.h"

#ifndef HAVE_HSTRERROR
const char *hstrerror (int rc)
{
    return "";
}
#endif

#ifndef HAVE_GETHOSTBYNAME
struct hostent *gethostbyname(const char *name)
{
    return NULL;
}
#endif

#define BACKLOG 10

static void UtilIOTOConn (Event *event);
static void UtilIOConnectCallback (Connection *conn);

/*
 * Connects to hostname on port port
 * hostname can be FQDN or IP
 */
void UtilIOConnectUDP (Connection *conn)
{
/* SOCKS5 stuff begin */
    int res;
    char buf[64];
    struct sockaddr_in s5sin;
    int s5Sok;
    unsigned short s5OurPort;
/* SOCKS5 stuff end */

    int conct;
    socklen_t length;
    struct sockaddr_in sin;     /* used to store inet addr stuff */
    struct hostent *host_struct;        /* used in DNS lookup */

    errno = 0;
    conn->sok = socket (AF_INET, SOCK_DGRAM, 0);      /* create the unconnected socket */

    if (conn->sok < 0)
    {
        if (prG->verbose || conn->type & TYPEF_ANY_SERVER)
        {
            M_print (i18n (1055, "Socket creation failed"));
            M_print (".\n");
        }
        conn->sok = -1;
        return;
    }
    if (prG->verbose || conn->type & TYPEF_ANY_SERVER)
        M_print (i18n (1056, "Socket created, attempting to connect.\n"));

    if (prG->s5Use)
    {
        sin.sin_addr.s_addr = INADDR_ANY;
        sin.sin_family = AF_INET;
        sin.sin_port = 0;

        if (bind (conn->sok, (struct sockaddr *) &sin, sizeof (struct sockaddr)) < 0)
        {
            if (prG->verbose || conn->type & TYPEF_ANY_SERVER)
                M_print (i18n (1637, "Can't bind socket to free port\n"));
            conn->sok = -1;
            return;
        }

        length = sizeof (sin);
        getsockname (conn->sok, (struct sockaddr *) &sin, &length);
        s5OurPort = ntohs (sin.sin_port);

        s5sin.sin_addr.s_addr = inet_addr (prG->s5Host);
        if (s5sin.sin_addr.s_addr == (unsigned long) -1)        /* name isn't n.n.n.n so must be DNS */
        {
            host_struct = gethostbyname (prG->s5Host);
            if (!host_struct)
            {
                if (prG->verbose || conn->type & TYPEF_ANY_SERVER)
                {
                    M_printf (i18n (1596, "[SOCKS] Can't find hostname %s: %s."), prG->s5Host, hstrerror (h_errno));
                    M_print ("\n");
                }
                conn->sok = -1;
                return;
            }
            s5sin.sin_addr = *((struct in_addr *) host_struct->h_addr);
        }
        s5sin.sin_family = AF_INET;     /* we're using the inet not appletalk */
        s5sin.sin_port = htons (prG->s5Port);        /* port */
        s5Sok = socket (AF_INET, SOCK_STREAM, 0);       /* create the unconnected socket */
        if (s5Sok < 0)
        {
            if (prG->verbose || conn->type & TYPEF_ANY_SERVER)
                M_print (i18n (1597, "[SOCKS] Socket creation failed\n"));
            conn->sok = -1;
            return;
        }
        conct = connect (s5Sok, (struct sockaddr *) &s5sin, sizeof (s5sin));
        if (conct == -1)        /* did we connect ? */
        {
            if (prG->verbose || conn->type & TYPEF_ANY_SERVER)
            {
                M_print (i18n (1598, "[SOCKS] Connection request refused"));
                M_print (".\n");
            }
            conn->sok = -1;
            return;
        }
        buf[0] = 5;             /* protocol version */
        buf[1] = 1;             /* number of methods */
        if (!*prG->s5Name || !*prG->s5Pass || !prG->s5Auth)
            buf[2] = 0;         /* no authorization required */
        else
            buf[2] = 2;         /* method username/password */
        send (s5Sok, buf, 3, 0);
        res = recv (s5Sok, buf, 2, 0);
        if (strlen (prG->s5Name) && strlen (prG->s5Pass) && prG->s5Auth)
        {
            if (res != 2 || buf[0] != 5 || buf[1] != 2) /* username/password authentication */
            {
                if (prG->verbose || conn->type & TYPEF_ANY_SERVER)
                {
                    M_print (i18n (1599, "[SOCKS] Authentication method incorrect"));
                    M_print (".\n");
                }
                sockclose (s5Sok);
                conn->sok = -1;
                return;
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
                if (prG->verbose || conn->type & TYPEF_ANY_SERVER)
                {
                    M_print (i18n (1600, "[SOCKS] Authorization failure"));
                    M_print (".\n");
                }
                sockclose (s5Sok);
                conn->sok = -1;
                return;
            }
        }
        else
        {
            if (res != 2 || buf[0] != 5 || buf[1] != 0) /* no authentication required */
            {
                if (prG->verbose || conn->type & TYPEF_ANY_SERVER)
                {
                    M_print (i18n (1599, "[SOCKS] Authentication method incorrect"));
                    M_print (".\n");
                }
                sockclose (s5Sok);
                conn->sok = -1;
                return;
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
            if (prG->verbose || conn->type & TYPEF_ANY_SERVER)
            {
                M_print (i18n (1601, "[SOCKS] General SOCKS server failure"));
                M_print (".\n");
            }
            sockclose (s5Sok);
            conn->sok = -1;
            return;
        }
    }

    sin.sin_addr.s_addr = inet_addr (conn->server);
    if (sin.sin_addr.s_addr + 1 == 0)      /* name isn't n.n.n.n so must be DNS */
    {
        host_struct = gethostbyname (conn->server);
        if (!host_struct)
        {
            if (prG->verbose || conn->type & TYPEF_ANY_SERVER)
            {
                M_printf (i18n (1948, "Can't find hostname %s: %s."), conn->server, hstrerror (h_errno));
                M_print ("\n");
            }
            conn->sok = -1;
            return;
        }
        sin.sin_addr = *((struct in_addr *) host_struct->h_addr);
    }
    conn->ip = ntohl (sin.sin_addr.s_addr);
    sin.sin_family = AF_INET;
    sin.sin_port = htons (conn->port);

    if (prG->s5Use)
    {
        memcpy (&sin.sin_addr.s_addr, &buf[4], 4);

        sin.sin_family = AF_INET;
        memcpy (&sin.sin_port, &buf[8], 2);
    }

    conct = connect (conn->sok, (struct sockaddr *) &sin, sizeof (sin));

    if (conct == -1)            /* did we connect ? */
    {
        if (prG->verbose || conn->type & TYPEF_ANY_SERVER)
            M_printf (i18n (1966, " Connection refused on port %ld at %s\n"), conn->port, conn->server);
        conn->sok = -1;
        return;
    }

    length = sizeof (sin);
    getsockname (conn->sok, (struct sockaddr *) &sin, &length);
    conn->our_local_ip = ntohl (sin.sin_addr.s_addr);

    if (prG->verbose || conn->type & TYPEF_ANY_SERVER)
        M_printf (i18n (1053, "Connected to %s, waiting for response\n"), conn->server);
}

#ifdef __AMIGA__
#define CONN_CHECK_EXTRA   if (rc == 11) return;  /* UAE sucks */
#else
#define CONN_CHECK_EXTRA   
#endif
#define CONN_FAIL(s)  { const char *t = s;      \
                        if (t) if (prG->verbose || conn->type & TYPEF_ANY_SERVER) \
                            M_printf    ("%s [%d]\n", t, __LINE__);  \
                        EventD (QueueDequeue (conn, QUEUE_CON_TIMEOUT, conn->ip)); \
                        if (conn->sok > 0)          \
                          sockclose (conn->sok);     \
                        conn->sok = -1;               \
                        conn->connect += 2;            \
                        conn->dispatch = conn->utilio;  \
                        conn->dispatch (conn);           \
                        return; }
#define CONN_FAIL_RC(s) { int rc = errno;                  \
                          if (prG->verbose || conn->type & TYPEF_ANY_SERVER) \
                          M_print (i18n (1949, "failed:\n"));\
                          CONN_FAIL (s_sprintf  ("%s: %s (%d).", s, strerror (rc), rc)) }
#define CONN_CHECK(s) { if (rc == -1) { rc = errno;            \
                          if (rc == EAGAIN) return;             \
                          CONN_CHECK_EXTRA                       \
                          CONN_FAIL (s_sprintf  ("%s: %s (%d).", s, strerror (rc), rc)) } }
#define CONN_OK         { conn->connect++;                          \
                          EventD (QueueDequeue (conn, QUEUE_CON_TIMEOUT, conn->ip)); \
                          conn->dispatch = conn->utilio;               \
                          conn->dispatch (conn);                        \
                          return; }

/*
 * Connect to conn->server, or conn->ip, or opens port for listening
 *
 * Usage: conn->dispatch will be called with conn->connect++ if ok,
 * conn->connect+=2 if fail.
 */
#undef UtilIOConnectTCP
void UtilIOConnectTCP (Connection *conn DEBUGPARAM)
{
    int rc, rce;
    socklen_t length;
    struct sockaddr_in sin;
    struct hostent *host;
    char *origserver = NULL;
    UDWORD origport = 0, origip = 0;
    
    conn->utilio   = conn->dispatch;

    Debug (DEB_IO, "UtilIOConnectCallback: %x", conn->connect);

    errno = 0;
    conn->sok = socket (AF_INET, SOCK_STREAM, 0);
    if (conn->sok < 0)
        CONN_FAIL_RC (i18n (1638, "Couldn't create socket"));
#if HAVE_FCNTL
    rc = fcntl (conn->sok, F_GETFL, 0);
    if (rc != -1)
        rc = fcntl (conn->sok, F_SETFL, rc | O_NONBLOCK);
#elif defined(HAVE_IOCTLSOCKET)
    origip = 1;
    rc = ioctlsocket (conn->sok, FIONBIO, &origip);
#endif
    if (rc == -1)
        CONN_FAIL_RC (i18n (1950, "Couldn't set socket nonblocking"));
    if (conn->server || conn->ip || prG->s5Use)
    {
        if (prG->s5Use)
        {
            origserver = conn->server;
            origip     = conn->ip;
            origport   = conn->port;
            conn->server = prG->s5Host;
            conn->port   = prG->s5Port;
            conn->ip     = -1;
        }

        sin.sin_family = AF_INET;
        sin.sin_port = htons (conn->port);

        if (conn->server)
            conn->ip = htonl (inet_addr (conn->server));
        if (conn->ip + 1 == 0 && conn->server)
        {
            host = gethostbyname (conn->server);
            if (!host)
            {
                rc = h_errno;
                CONN_FAIL (s_sprintf (i18n (1951, "Can't find hostname %s: %s (%d)."), conn->server, hstrerror (rc), rc));
            }
            sin.sin_addr = *((struct in_addr *) host->h_addr);
            conn->ip = ntohl (sin.sin_addr.s_addr);
        }
        sin.sin_addr.s_addr = htonl (conn->ip);
        
        if (prG->s5Use)
        {
            conn->server = origserver;
            conn->port   = origport;
            conn->ip     = origip;
        }

        rc = connect (conn->sok, (struct sockaddr *) &sin, sizeof (struct sockaddr));
        rce = rc < 0 ? errno : 0;

        length = sizeof (struct sockaddr);
        getsockname (conn->sok, (struct sockaddr *) &sin, &length);
        conn->our_local_ip = ntohl (sin.sin_addr.s_addr);
        if (conn->assoc && (conn->type == TYPE_SERVER))
            conn->assoc->our_local_ip = conn->our_local_ip;

        if (rc >= 0)
        {
            M_print ("");
            if (prG->verbose || conn->type & TYPEF_ANY_SERVER)
                if (M_pos () > 0)
                     M_print (i18n (1634, "ok.\n"));
            if (prG->s5Use)
            {
                QueueEnqueueData (conn, QUEUE_CON_TIMEOUT, conn->ip,
                                  time (NULL) + 10, NULL,
                                  conn->cont, NULL, &UtilIOTOConn);
                conn->dispatch = &UtilIOConnectCallback;
                conn->connect |= CONNECT_SOCKS_ADD;
                UtilIOConnectCallback (conn);
                return;
            }
            CONN_OK
        }

#ifdef __AMIGA__
        if (rce == EINPROGRESS || rce == 115) /* UAE sucks */
#elif defined(EINPROGRESS)
        if (rce == EINPROGRESS)
#elif defined(WSAEINPROGRESS)
        if (!rce || rce == WSAEINPROGRESS)
#else
        rce = 1;
        if (0)
#endif
        {
            M_print ("");
            if (prG->verbose || conn->type & TYPEF_ANY_SERVER)
                if (M_pos () > 0)
                    M_print ("\n");
            QueueEnqueueData (conn, QUEUE_CON_TIMEOUT, conn->ip,
                              time (NULL) + 10, NULL,
                              conn->cont, NULL, &UtilIOTOConn);
            conn->utilio   = conn->dispatch;
            conn->dispatch = &UtilIOConnectCallback;
            conn->connect |= CONNECT_SELECT_W | CONNECT_SELECT_X;
            return;
        }
        else
        {
            errno = rce;
            CONN_FAIL_RC (i18n (1952, "Couldn't open connection"));
        }
    }
    else
    {
        sin.sin_family = AF_INET;
        sin.sin_port = htons (conn->port);
        sin.sin_addr.s_addr = INADDR_ANY;

        if (bind (conn->sok, (struct sockaddr*)&sin, sizeof (struct sockaddr)) < 0)
        {
#if defined(EADDRINUSE)
            while ((rc = errno) == EADDRINUSE && conn->port)
            {
                rc = 0;
                sin.sin_port = htons (++conn->port);
                if (bind (conn->sok, (struct sockaddr*)&sin, sizeof (struct sockaddr)) == 0)
                    break;
                rc = errno;
            }
#endif
            if (rc)
            {
                errno = rc;
                CONN_FAIL_RC (i18n (1953, "couldn't bind socket to free port"));
            }
        }

        if (listen (conn->sok, BACKLOG) < 0)
            CONN_FAIL_RC (i18n (1954, "unable to listen on socket"));

        length = sizeof (struct sockaddr);
        getsockname (conn->sok, (struct sockaddr *) &sin, &length);
        conn->port = ntohs (sin.sin_port);
        s_repl (&conn->server, "localhost");
        if (prG->verbose || conn->type == TYPE_MSGLISTEN)
            if (M_pos () > 0)
                M_print (i18n (1634, "ok.\n"));
        CONN_OK
    }
}

#define CONNS_FAIL_RC(s) { int rc = errno;            \
                           const char *t = s_sprintf ("%s: %s (%d).", s, strerror (rc), rc); \
                           M_print (i18n (1949, "failed:\n"));  \
                           M_printf ("%s [%d]\n", t, __LINE__);  \
                           if (conn->sok > 0)             \
                               sockclose (conn->sok);      \
                           conn->sok = -1;                  \
                           conn->connect = 0;                \
                           return; }

#if ENABLE_REMOTECONTROL
void UtilIOConnectF (Connection *conn)
{
    int rc;
    
    if (!conn->server)
        return;

    unlink (conn->server);
    if (mkfifo (conn->server, 0600) < 0)
        CONNS_FAIL_RC (i18n (2226, "Couldn't create FIFO"));

#if defined(O_NONBLOCK)
    if ((conn->sok = open (conn->server, O_RDWR /*ONLY*/ | O_NONBLOCK)) < 0)
#else
    if ((conn->sok = open (conn->server, O_RDWR)) < 0)
#endif
        CONNS_FAIL_RC (i18n (2227, "Couldn't open FIFO"));

#if HAVE_FCNTL
    rc = fcntl (conn->sok, F_GETFL, 0);
    if (rc != -1)
        rc = fcntl (conn->sok, F_SETFL, rc | O_NONBLOCK);
#elif defined(HAVE_IOCTLSOCKET)
    origip = 1;
    rc = ioctlsocket(conn->sok, FIONBIO, &origip);
#endif
    if (rc == -1)
        CONNS_FAIL_RC (i18n (2228, "Couldn't set FIFO nonblocking"));

    if (M_pos () > 0)
        M_print (i18n (1634, "ok.\n"));

    conn->connect = CONNECT_OK;
}
#endif

int UtilIOError (Connection *conn)
{
    int rc;
    socklen_t length;

    length = sizeof (int);
#ifdef SO_ERROR
    if (getsockopt (conn->sok, SOL_SOCKET, SO_ERROR, (void *)&rc, &length) < 0)
#endif
        rc = errno;

    return rc;
}

/*
 * Continue connecting.
 */
static void UtilIOConnectCallback (Connection *conn)
{
    int rc, eno = 0, len;
    char buf[60];

    while (1)
    {
        eno = 0;
        rc = 0;
        DebugH (DEB_IO, "UtilIOConnectCallback: %x", conn->connect);
        switch ((eno = conn->connect / CONNECT_SOCKS_ADD) % 7)
        {
            case 0:
                if ((rc = UtilIOError (conn)))
                    CONN_FAIL (s_sprintf ("%s: %s (%d).", i18n (1955, "Connection failed"), strerror (rc), rc));

                conn->connect += CONNECT_SOCKS_ADD;
            case 1:
                if (!prG->s5Use)
                    CONN_OK

                conn->connect += CONNECT_SOCKS_ADD;
                conn->connect |= CONNECT_SELECT_R;
                conn->connect &= ~CONNECT_SELECT_W & ~CONNECT_SELECT_X;
                sockwrite (conn->sok, prG->s5Auth ? "\x05\x02\x02\x00" : "\x05\x01\x00", prG->s5Auth ? 4 : 3);
                return;
            case 2:
                rc = sockread (conn->sok, buf, 2);
                CONN_CHECK (i18n (1601, "[SOCKS] General SOCKS server failure"));
                if (buf[0] != 5 || !(buf[1] == 0 || (buf[1] == 2 && prG->s5Auth)))
                    CONN_FAIL (i18n (1599, "[SOCKS] Authentication method incorrect"));

                conn->connect += CONNECT_SOCKS_ADD;
                if (buf[1] == 2)
                {
                    snprintf (buf, sizeof (buf), "%c%c%s%c%s%n", 1, (char) strlen (prG->s5Name), 
                              prG->s5Name, (char) strlen (prG->s5Pass), prG->s5Pass, &len);
                    sockwrite (conn->sok, buf, len);
                    return;
                }
                conn->connect += CONNECT_SOCKS_ADD;
                continue;
            case 3:
                rc = sockread (conn->sok, buf, 2);
                CONN_CHECK (i18n (1601, "[SOCKS] General SOCKS server failure"));
                if (rc != 2 || buf[1])
                    CONN_FAIL  (i18n (1600, "[SOCKS] Authorization failure"));
                conn->connect += CONNECT_SOCKS_ADD;
            case 4:
                if (conn->server)
                    snprintf (buf, sizeof (buf), "%c%c%c%c%c%s%c%c%n", 5, 1, 0, 3, (char)strlen (conn->server),
                              conn->server, (char)(conn->port >> 8), (char)(conn->port & 255), &len);
                else if (conn->ip)
                    snprintf (buf, sizeof (buf), "%c%c%c%c%c%c%c%c%c%c%n", 5, 1, 0, 1, (char)(conn->ip >> 24),
                              (char)(conn->ip >> 16), (char)(conn->ip >> 8), (char)conn->ip,
                              (char)(conn->port >> 8), (char)(conn->port & 255), &len);
                else
                    snprintf (buf, sizeof (buf), "%c%c%c%c%c%c%c%c%c%c%n", 5, 2, 0, 1, 0,0,0,0,
                              eno & 8 ? 0 : (char)(conn->port >> 8),
                              eno & 8 ? 0 : (char)(conn->port & 255), &len); 
                sockwrite (conn->sok, buf, len);
                conn->connect += CONNECT_SOCKS_ADD;
                return;
            case 5:
                rc = sockread (conn->sok, buf, 10);
                CONN_CHECK (i18n (1601, "[SOCKS] General SOCKS server failure"));
                if (rc != 10 || buf[3] != 1)
                    CONN_FAIL (i18n (1601, "[SOCKS] General SOCKS server failure"));
                if (buf[1] == 4 && conn->port && !(eno & 8))
                {
                    conn->connect &= ~CONNECT_SOCKS;
                    conn->connect |= 8 * CONNECT_SOCKS_ADD;
                    conn->dispatch = conn->utilio;
                    EventD (QueueDequeue (conn, QUEUE_CON_TIMEOUT, conn->ip));
                    UtilIOConnectTCP (conn DEBUGNONE);
                    return;
                }
                if (buf[1])
                    CONN_FAIL (s_sprintf (i18n (1958, "[SOCKS] Connection request refused (%d)"), buf[1]));
                if (!conn->server && !conn->ip)
                {
                    conn->our_outside_ip = ntohl (*(UDWORD *)(&buf[4]));
                    conn->port = ntohs (*(UWORD *)(&buf[8]));
                    if (conn->assoc)
                        conn->assoc->our_local_ip = conn->our_outside_ip;
                }
                conn->connect &= ~CONNECT_SOCKS;
                CONN_OK
            default:
                assert (0);
        }
    }
}

/*
 * Does SOCKS5 handshake for incoming connection
 */
void UtilIOSocksAccept (Connection *conn)
{
    char buf[60];
    int rc;

    rc = sockread (conn->sok, buf, 10);
    CONN_CHECK (i18n (1601, "[SOCKS] General SOCKS server failure"));
    if (rc != 10 || buf[3] != 1)
        CONN_FAIL (i18n (1601, "[SOCKS] General SOCKS server failure"));
}

/*
 * Handles timeout on TCP connect
 */
static void UtilIOTOConn (Event *event)
{
     Connection *conn = event->conn;
     EventD (event);
     if (conn)
#if defined (ETIMEDOUT)
         CONN_FAIL (s_sprintf ("%s: %s (%d).", i18n (1955, "Connection failed"),
                    strerror (ETIMEDOUT), ETIMEDOUT));
#else
         CONN_FAIL (s_sprintf ("%s: connection timed out", i18n (1955, "Connection failed")));
#endif
}

#ifndef ECONNRESET
#define ECONNRESET 0x424242
#endif

/*
 * Receive a packet via TCP.
 */
Packet *UtilIOReceiveTCP (Connection *conn)
{
    int off, len, rc;
    Packet *pak;
    
    if (!(conn->connect & CONNECT_MASK))
        return NULL;
    
    if (!(pak = conn->incoming))
        conn->incoming = pak = PacketC ();
    
    if (conn->type == TYPE_SERVER)
    {
        len = off = 6;
        if (pak->len >= off)
            len = PacketReadAtB2 (pak, 4) + 6;
    }
    else
    {
        len = off = 2;
        if (pak->len >= off)
            len = PacketReadAt2 (pak, 0) + 2;
    }
    while (1)
    {
        errno = 0;
        if (len < 0 || len > PacketMaxData)
        {
            rc = ENOMEM;
            break;
        }
#if defined(SIGPIPE)
        signal (SIGPIPE, SIG_IGN);
#endif
        rc = dc_read (conn, pak->data + pak->len, len - pak->len);
        if (rc <= 0)
        {
            if (!rc)
                rc = UtilIOError (conn);
            else
                rc = errno;
            if (rc == EAGAIN)
                return NULL;
#ifdef __AMIGA__
            if (rc == 11)         /* UAE sucks */
                return NULL;
#endif
            if (!rc)
                rc = ECONNRESET;
            break;
        }
        pak->len += rc;
        if (pak->len < len)
            return NULL;
        if (len == off)
            return NULL;
        conn->incoming = NULL;
        if (off == 2)
        {
            pak->len -= 2;
            memmove (pak->data, pak->data + 2, pak->len);
        }
        return pak;
    }

    if (conn->error && conn->error (conn, rc, CONNERR_READ))
        return NULL;

    PacketD (pak);
    dc_close (conn);
    conn->sok = -1;
    conn->incoming = NULL;

    if ((rc && rc != ECONNRESET) || !conn->reconnect)
    {
        if (prG->verbose || conn->type & TYPEF_ANY_SERVER)
        {
            Contact *cont;
            
            if ((cont = conn->cont))
            {
                M_printf ("%s %s%*s%s ", s_now, COLCONTACT, uiG.nick_len + s_delta (cont->nick), cont->nick, COLNONE);
                M_printf (i18n (1878, "Error while reading from socket: %s (%d)\n"), dc_strerror (rc), rc);
            }
        }
        conn->connect = 0;
    }
    if (conn->reconnect)
        conn->reconnect (conn);
    else
        conn->connect = 0;
    return NULL;
}

#if ENABLE_REMOTECONTROL
/*
 * Receives a line from a FIFO socket.
 */
Packet *UtilIOReceiveF (Connection *conn)
{
    int rc;
    Packet *pak;
    UBYTE *end, *beg;
    
    if (!(conn->connect & CONNECT_MASK))
        return NULL;
    
    if (!(pak = conn->incoming))
        conn->incoming = pak = PacketC ();
    
    while (1)
    {
        errno = 0;
#if defined(SIGPIPE)
        signal (SIGPIPE, SIG_IGN);
#endif
        rc = sockread (conn->sok, pak->data + pak->len, sizeof (pak->data) - pak->len);
        if (rc <= 0)
        {
            rc = errno;
            if (rc == EAGAIN)
                rc = 0;
            if (rc)
                break;
        }
        pak->len += rc;
        pak->data[pak->len] = '\0';
        if (!(beg = end = (UBYTE *)strpbrk ((const char *)pak->data, "\r\n")))
            return NULL;
        while (*end && strchr ("\r\n\t ", *end))
            end++;
        *beg = '\0';
        if (*end)
        {
            conn->incoming = PacketC ();
            conn->incoming->len = pak->len - (end - pak->data);
            memcpy (conn->incoming->data, end, conn->incoming->len);
        }
        else
            conn->incoming = NULL;
        pak->len = beg - pak->data;
        return pak;
    }

    if (conn->error && conn->error (conn, rc, CONNERR_READ))
        return NULL;

    PacketD (pak);
    sockclose (conn->sok);
    conn->sok = -1;
    conn->incoming = NULL;

    if (!conn->reconnect)
    {
        if (prG->verbose)
        {
            Contact *cont;
            if ((cont = conn->cont))
            {
                M_printf ("%s %s%*s%s ", s_now, COLCONTACT, uiG.nick_len + s_delta (cont->nick), cont->nick, COLNONE);
                M_printf (i18n (1878, "Error while reading from socket: %s (%d)\n"), strerror (rc), rc);
            }
        }
        conn->connect = 0;
    }
    else
        conn->reconnect (conn);
    return NULL;
}
#endif

/*
 * Send packet via TCP. Consumes packet.
 */
BOOL UtilIOSendTCP (Connection *conn, Packet *pak)
{
    UBYTE *data;
    int rc, bytessend = 0;

    data = (void *) &pak->data;
    
    if (conn->outgoing)
        return FALSE;
    conn->outgoing = pak;

    while (1)
    {
        errno = 0;

        for ( ; pak->len > pak->rpos; pak->rpos += bytessend)
        {
            bytessend = dc_write (conn, data + pak->rpos, pak->len - pak->rpos);
            if (bytessend <= 0)
                break;
        }
        if (bytessend <= 0)
            break;
        
        PacketD (pak);
        conn->outgoing = NULL;
        conn->stat_pak_sent++;
        return TRUE;
    }
    
    rc = errno;

    if (conn->error && conn->error (conn, rc, CONNERR_WRITE))
        return TRUE;

    conn->outgoing = NULL;
    PacketD (pak);
    dc_close (conn);
    conn->sok = -1;

    if ((rc && rc != ECONNRESET) || !conn->reconnect)
    {
        if (prG->verbose || conn->type & TYPEF_ANY_SERVER)
        {
            Contact *cont;
            
            if ((cont = conn->cont))
            {
                M_printf ("%s %s%*s%s ", s_now, COLCONTACT, uiG.nick_len + s_delta (cont->nick), cont->nick, COLNONE);
                M_printf (i18n (1878, "Error while reading from socket: %s (%d)\n"), dc_strerror (rc), rc);
            }
        }
        conn->connect = 0;
    }
    if (conn->reconnect)
        conn->reconnect (conn);
    else
        conn->connect = 0;
    return FALSE;
}

/*
 * Send a given packet to the session's socket.
 * Use socks if requested. UDP only.
 */
void UtilIOSendUDP (Connection *conn, Packet *pak)
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
        *(UDWORD *) &body[4] = htonl (conn->ip);
        *(UWORD  *) &body[8] = htons (conn->port);
        memcpy (body + s5len, data, pak->len);
        data = body;
    }
    sockwrite (conn->sok, data, pak->len + s5len);
    if (body)
        free (body);
    conn->stat_pak_sent++;
}

/*
 * Receive a single packet via UDP.
 * Note this won't work for TCP - UDP packets are always received in one part.
 */
Packet *UtilIOReceiveUDP (Connection *conn)
{
    Packet *pak;
    int s5len;
    
    s5len = prG->s5Use ? 10 : 0;
    pak = PacketC ();
    
    pak->len = sockread (conn->sok, prG->s5Use ? pak->socks : pak->data, sizeof (pak->data) + s5len);
    
    if (pak->len <= 4 + s5len || pak->len == (UWORD)-1)
    {
        PacketD (pak);
        return NULL;
    }
    pak->len -= s5len;
    
    return pak;
}

/*
 * Read a complete line from a fd.
 *
 * Returned string may not be free()d.
 */
strc_t UtilIOReadline (FILE *fd)
{
    static str_s str;
    char *p;
    
    s_init (&str, "", 100);
    while (1)
    {
        str.txt[str.max - 2] = 0;
        if (!fgets (str.txt + str.len, str.max - str.len, fd))
        {
            str.txt[str.len] = '\0';
            if (!str.len)
                return NULL;
            break;
        }
        str.txt[str.max - 1] = '\0';
        str.len = strlen (str.txt);
        if (!str.txt[str.max - 2])
            break;
        s_blow (&str, 100);
    }
    if ((p = strpbrk (str.txt, "\r\n")))
    {
        *p = 0;
        str.len = strlen (str.txt);
    }
    return &str;
}
