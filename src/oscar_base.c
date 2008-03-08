/*
 * Decodes and creates FLAPs.
 *
 * climm Copyright (C) © 2001-2007 Rüdiger Kuhlmann
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
#include <assert.h>
#include <sys/types.h>
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
#include <fcntl.h>
#include "util_ui.h"
#include "util_io.h"
#include "util_syntax.h"
#include "oscar_base.h"
#include "oscar_tlv.h"
#include "oscar_snac.h"
#include "oscar_register.h"
#include "preferences.h"
#include "connection.h"
#include "packet.h"
#include "contact.h"
#include "conv.h"
#include "oscar_dc.h"

static void FlapChannel1 (Server *serv, Packet *pak);
static void FlapSave (Server *serv, Packet *pak, BOOL in);

void SrvCallBackFlap (Event *event)
{
    Server *serv;

    if (!event->conn)
    {
        EventD (event);
        return;
    }
    
    assert (event->type == QUEUE_FLAP);
    ASSERT_SERVER (event->conn);
    
    serv = Connection2Server (event->conn);

    event->pak->tpos = event->pak->rpos;
    event->pak->cmd = PacketRead1 (event->pak);
    event->pak->id  = PacketReadB2 (event->pak);
                      PacketReadB2 (event->pak);
    
    serv->stat_pak_rcvd++;
    serv->stat_real_pak_rcvd++;
    switch (event->pak->cmd)
    {
        case 1: /* Client login */
            FlapChannel1 (serv, event->pak);
            break;
        case 2: /* SNAC packets */
            SnacCallback (event);
            return;
        case 3: /* Errors */
            break;
        case 4: /* Logoff */
            FlapChannel4 (serv, event->pak);
            break;
        case 5: /* Ping */
            if (PacketReadB4 (event->pak) || PacketReadLeft (event->pak))
                FlapPrint (event->pak);
            break;
        default:
            rl_printf (i18n (1884, "FLAP with unknown channel %ld received.\n"), UD2UL (event->pak->cmd));
    }
    EventD (event);
}

/***********************************************/

static void FlapChannel1 (Server *serv, Packet *pak)
{
    int i;

    if (PacketReadLeft (pak) < 4)
    {
        rl_print (i18n (1881, "FLAP channel 1 out of data.\n"));
        return;
    }
    i = PacketReadB4 (pak);
    switch (i)
    {
        case 1:
            if (PacketReadLeft (pak))
            {
                rl_print (i18n (1882, "FLAP channel 1 cmd 1 extra data:\n"));
                rl_print (s_dump (pak->data + pak->rpos, PacketReadLeft (pak)));
                break;
            }
            if (!serv->uin)
            {
                FlapCliHello (serv);
                SnacCliRegisteruser (serv);
            }
            else if (serv->connect & 8)
            {
                TLV *tlv = serv->tlv;
                
                assert (tlv);
                FlapCliCookie (serv, tlv[6].str.txt, tlv[6].str.len);
                TLVD (tlv);
                tlv = NULL;
            }
#if ENABLE_SSL
            else if (libgcrypt_is_present)
            {
                FlapCliHello (serv);
                SnacCliReqlogin (serv);
            }
#endif
            else
                FlapCliIdent (serv);
            break;
        default:
            rl_printf (i18n (1883, "FLAP channel 1 unknown command %d.\n"), i);
    }
}

void FlapChannel4 (Server *serv, Packet *pak)
{
    TLV *tlv;
    
    tlv = TLVRead (pak, PacketReadLeft (pak), -1);
    if (!tlv[5].str.len)
    {
        rl_printf ("%s " COLINDENT, s_now);
        if (!(serv->connect & CONNECT_OK))
            rl_print (i18n (1895, "Login failed:\n"));
        else
            rl_print (i18n (1896, "Server closed connection:\n"));
        rl_printf (i18n (1048, "Error code: %ld\n"), UD2UL (tlv[9].nr ? tlv[9].nr : tlv[8].nr));
        if (tlv[1].str.len && strcmp (tlv[1].str.txt, serv->screen))
            rl_printf (i18n (2218, "UIN: %s\n"), tlv[1].str.txt);
        if (tlv[4].str.len)
            rl_printf (i18n (1961, "URL: %s\n"), tlv[4].str.txt);
        rl_print (COLEXDENT "\n");
        
        if (tlv[8].nr == 24)
            rl_print (i18n (2328, "You logged in too frequently, please wait 30 minutes before trying again.\n"));
        if (tlv[8].nr == 5)
            s_repl (&serv->passwd, NULL);

        if ((serv->connect & CONNECT_MASK) && serv->sok != -1)
            sockclose (serv->sok);
        serv->connect = 0;
        serv->sok = -1;
        TLVD (tlv);
        tlv = NULL;
    }
    else
    {
        assert (strchr (tlv[5].str.txt, ':'));
        FlapCliGoodbye (serv);

        serv->port = atoi (strchr (tlv[5].str.txt, ':') + 1);
        *strchr (tlv[5].str.txt, ':') = '\0';
        s_repl (&serv->server, tlv[5].str.txt);
        serv->ip = 0;

        rl_printf (i18n (2511, "Redirect to server %s:%s%ld%s... "),
                  s_wordquote (serv->server), COLQUOTE, UD2UL (serv->port), COLNONE);

        serv->connect = 8;
        serv->tlv = tlv;
        UtilIOConnectTCP (Server2Connection (serv));
    }
}

/***********************************************/

void FlapPrint (Packet *pak)
{
    UWORD opos = pak->rpos;
    UBYTE ch;
    UWORD seq, len;

    pak->rpos = 1;
    ch  = PacketRead1 (pak);
    seq = PacketReadB2 (pak);
    len = PacketReadB2 (pak);

    rl_printf (COLEXDENT "%s\n  " COLINDENT "%s %sFLAP  ch %d seq %08x length %04x%s\n",
              COLNONE, s_dumpnd (pak->data, 6), COLDEBUG, ch, seq, len, COLNONE);

    if (ch == 2)
        SnacPrint (pak);
    else
        if (prG->verbose & DEB_PACK8DATA || ~prG->verbose & DEB_PACK8)
            rl_print (s_dump (pak->data + 6, pak->len - 6));

    pak->rpos = opos;
}

static void FlapSave (Server *serv, Packet *pak, BOOL in)
{
    UDWORD oldrpos = pak->rpos;
    UBYTE ch;
    UWORD seq, len;
    str_s str = { NULL, 0, 0 };
    const char *data;
    char *dump;
    
    if (serv->logfd < 0)
    {
        const char *dir, *file;
        dir = s_sprintf ("%sdebug", PrefUserDir (prG));
        mkdir (dir, 0700);
        file = s_sprintf ("%sdebug" _OS_PATHSEPSTR "packets.icq8.%s.%lu", PrefUserDir (prG), serv->screen, time (NULL));
        serv->logfd = open (file, O_WRONLY | O_CREAT | O_APPEND, 0600);
    }
    if (serv->logfd < 0)
        return;

    pak->rpos = 1;
    ch  = PacketRead1 (pak);
    seq = PacketReadB2 (pak);
    len = PacketReadB2 (pak);
    
    data = s_sprintf ("%s %s\n", s_now, in ? "<<<" : ">>>");
    write (serv->logfd, data, strlen (data));
    
    s_init (&str, "", 64);
    s_catf (&str, "%s FLAP  ch %d seq %08x length %04x\n",
            s_dumpnd (pak->data, 6), ch, seq, len);
    
    if (ch == 2)
    {
        UWORD fam, cmd, flag;
        UDWORD ref, len;
        char *syn;
        
        fam  = PacketReadB2 (pak);
        cmd  = PacketReadB2 (pak);
        flag = PacketReadB2 (pak);
        ref  = PacketReadB4 (pak);
        len  = PacketReadAtB2 (pak, pak->rpos);

        s_catf (&str, "%s SNAC (%x,%x) [%s] flags %x ref %lx",
            s_dumpnd (pak->data + 6, flag & 0x8000 ? 10 + len + 2 : 10),
            fam, cmd, SnacName (fam, cmd), flag, UD2UL (ref));

        if (flag & 0x8000)
        {
            s_catf (&str, " extra (%ld)", UD2UL (len));
            pak->rpos += len + 2;
        }
        s_catc (&str, '\n');

        syn = strdup (s_sprintf ("gs%dx%ds", fam, cmd));
        dump = PacketDump (pak, syn, "", "");
        free (syn);
        s_catf (&str, "%s", dump);
        free (dump);
    }
    else
        s_catf (&str, "%s", s_dump (pak->data + 6, pak->len - 6));

    data = s_ind (str.txt);
    s_done (&str);
    write (serv->logfd, data, strlen (data));
    pak->rpos = oldrpos;
}

Packet *FlapC (UBYTE channel)
{
    Packet *pak;
    
    pak = PacketC ();
    
    assert (pak);
    
    PacketWrite1 (pak, 0x2a);
    PacketWrite1 (pak, pak->cmd = channel);
    PacketWrite4 (pak, 0);
    
    return pak;
}

void FlapSend (Server *serv, Packet *pak)
{
    if (!serv->our_seq)
        serv->our_seq = rand () & 0x7fff;
    serv->our_seq++;
    serv->our_seq &= 0x7fff;

    PacketWriteAtB2 (pak, 2, pak->id = serv->our_seq);
    PacketWriteAtB2 (pak, 4, pak->len - 6);
    
    if (prG->verbose & DEB_PACK8)
    {
        rl_printf ("%s " COLINDENT "%s%s ", s_now, COLCLIENT, i18n (1903, "Outgoing v8 server packet:"));
        FlapPrint (pak);
        rl_print (COLEXDENT "\r");
    }
    
    if (ConnectionPrefVal (serv, CO_LOGSTREAM))
        FlapSave (serv, pak, FALSE);
    
    serv->stat_pak_sent++;
    serv->stat_real_pak_sent++;
    
    sockwrite (serv->sok, pak->data, pak->len);
    PacketD (pak);
}

/***********************************************/

static char *_encryptpw (const char *pw)
{
    char *cpw = strdup (pw), *p;
    const char *tb = "\xf3\x26\x81\xc4\x39\x86\xdb\x92"
                     "\x71\xa3\xb9\xe6\x53\x7a\x95\x7c";
    int i = 0;
    for (p = cpw; *p; p++, i++)
        *p ^= tb[i % 16];
    return cpw;
}

    
void FlapCliIdent (Server *serv)
{
    Packet *pak;
    char *f;

    assert (serv->passwd);
    assert (*serv->passwd);

    pak = FlapC (1);
    PacketWriteB4 (pak, CLI_HELLO);
    PacketWriteTLVStr  (pak, 1, serv->screen);
    PacketWriteTLVData (pak, 2, f = _encryptpw (serv->passwd), strlen (serv->passwd));
    PacketWriteTLVStr  (pak, 3, "ICQ Inc. - Product of ICQ (TM).2003b.5.37.1.3728.85");
    PacketWriteTLV2    (pak, 22, 266);
    PacketWriteTLV2    (pak, 23, FLAP_VER_MAJOR);
    PacketWriteTLV2    (pak, 24, FLAP_VER_MINOR);
    PacketWriteTLV2    (pak, 25, FLAP_VER_LESSER);
    PacketWriteTLV2    (pak, 26, FLAP_VER_BUILD);
    PacketWriteTLV4    (pak, 20, FLAP_VER_SUBBUILD);
    PacketWriteTLVStr  (pak, 15, "de");  /* en */
    PacketWriteTLVStr  (pak, 14, "DE");  /* en */
    FlapSend (serv, pak);
    free (f);
}

void FlapCliCookie (Server *serv, const char *cookie, UWORD len)
{
    Packet *pak;

    pak = FlapC (1);
    PacketWriteB4 (pak, CLI_HELLO);
    PacketWriteTLVData (pak, 6, cookie, len);
    FlapSend (serv, pak);
}

void FlapCliHello (Server *serv)
{
    Packet *pak;
    
    pak = FlapC (1);
    PacketWriteB4 (pak, CLI_HELLO);
    FlapSend (serv, pak);
}

static void FlapCliGoodbyeConn (Connection *conn)
{
    FlapCliGoodbye (Connection2Server (conn));
}

void FlapCliGoodbye (Server *serv)
{
    Packet *pak;
    
    if (!(serv->connect & CONNECT_MASK))
        return;
    
    pak = FlapC (4);
    FlapSend (serv, pak);
    
    sockclose (serv->sok);
    serv->sok = -1;
    serv->connect = 0;
}

void FlapCliKeepalive (Server *serv)
{
    FlapSend (serv, FlapC (5));
}

jump_conn_f SrvCallBackReceive;
static jump_conn_f SrvCallBackReconn;
static void SrvCallBackTimeout (Event *event);
static void SrvCallBackDoReconn (Event *event);


Event *ConnectionInitServer (Server *serv)
{
    Contact *cont;
    Event *event;
    
    if (!serv->passwd || !*serv->passwd || !serv->port)
        return NULL;
    if (!serv->uin && ~serv->flags & CONN_WIZARD)
        return NULL;
    if (!serv->server || !*serv->server)
        s_repl (&serv->server, "login.icq.com");

    if (serv->sok != -1)
        sockclose (serv->sok);
    serv->sok = -1;
    serv->cont = cont = serv->uin ? ContactUIN (serv, serv->uin) : NULL;
    serv->our_seq  = rand () & 0x7fff;
    serv->connect  = 0;
    serv->dispatch = &SrvCallBackReceive;
    serv->reconnect= &SrvCallBackReconn;
    serv->close    = &FlapCliGoodbyeConn;
    s_repl (&serv->server, serv->pref_server);
    if (serv->status == ims_offline)
        serv->status = serv->pref_status;
    
    if ((event = QueueDequeue2 (Server2Connection (serv), QUEUE_DEP_WAITLOGIN, 0, NULL)))
    {
        event->attempts++;
        event->due = time (NULL) + 10 * event->attempts + 10;
        event->callback = &SrvCallBackTimeout;
        QueueEnqueue (event);
    }
    else
        event = QueueEnqueueData (Server2Connection (serv), QUEUE_DEP_WAITLOGIN, 0, time (NULL) + 12,
                                  NULL, serv->cont, NULL, &SrvCallBackTimeout);

    rl_printf (i18n (2512, "Opening v8 connection to %s:%s%ld%s for %s%s%s... "),
              s_wordquote (serv->server), COLQUOTE, UD2UL (serv->port), COLNONE, COLCONTACT,
              !cont ? i18n (2513, "new UIN") : cont->nick ? cont->nick 
              : cont->screen, COLNONE);

    UtilIOConnectTCP (Server2Connection (serv));
    if (serv->assoc && serv->assoc->c_open)
        serv->assoc->c_open (serv->assoc);
    return event;
}

static void SrvCallBackReconn (Connection *conn)
{
    ContactGroup *cg = conn->contacts;
    Server *serv;
    Event *event;
    Contact *cont;
    int i;

    serv = Connection2Server (conn);

    if (!(cont = serv->cont))
        return;
    
    if (!(event = QueueDequeue2 (Server2Connection (serv), QUEUE_DEP_WAITLOGIN, 0, NULL)))
    {
        ConnectionInitServer (serv);
        return;
    }
    
    conn->connect = 0;
    rl_log_for (cont->nick, COLCONTACT);
    if (event->attempts < 5)
    {
        rl_printf (i18n (2032, "Scheduling v8 reconnect in %d seconds.\n"), 10 << event->attempts);
        event->due = time (NULL) + (10 << event->attempts);
        event->callback = &SrvCallBackDoReconn;
        QueueEnqueue (event);
    }
    else
    {
        rl_print (i18n (2031, "Connecting failed too often, giving up.\n"));
        EventD (event);
    }
    for (i = 0; (cont = ContactIndex (cg, i)); i++)
        cont->status = ims_offline;
}

static void SrvCallBackDoReconn (Event *event)
{
    if (event->conn && event->conn->type == TYPE_SERVER)
    {
        QueueEnqueue (event);
        ConnectionInitServer (Connection2Server (event->conn));
    }
    else
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
    ASSERT_SERVER (conn);
    
    if (conn->connect & CONNECT_MASK && ~conn->connect & CONNECT_OK)
    {
        if (conn->connect == event->seq)
        {
            rl_print (i18n (1885, "Connection v8 timed out.\n"));
            conn->connect = 0;
            sockclose (conn->sok);
            conn->sok = -1;
            QueueEnqueue (event);
            SrvCallBackReconn (conn);
        }
        else
        {
            event->due = time (NULL) + 10 + 10 * event->attempts;
            conn->connect |= CONNECT_SELECT_R;
            event->seq = conn->connect;
            QueueEnqueue (event);
        }
    }
    else
        EventD (event);
}

void SrvCallBackReceive (Connection *conn)
{
    Packet *pak;

    if (~conn->connect & CONNECT_OK)
    {
        switch (conn->connect & 7)
        {
            case 0:
                if (conn->assoc && (~conn->assoc->connect & CONNECT_OK) && (conn->assoc->flags & CONN_AUTOLOGIN))
                {
                    rl_printf ("FIXME: avoiding deadlock\n");
                    conn->connect &= ~CONNECT_SELECT_R;
                }
                else
            case 1:
            case 5:
                /* fall-through */
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
        DebugH (DEB_PROTOCOL, "Incoming packet is not a FLAP: id is %d.\n", PacketRead1 (pak));
        return;
    }
    
    if (prG->verbose & DEB_PACK8)
    {
        rl_printf ("%s " COLINDENT "%s%s ",
                 s_now, COLSERVER, i18n (1033, "Incoming v8 server packet:"));
        FlapPrint (pak);
        rl_print (COLEXDENT "\r");
    }
    if (ConnectionPrefVal (Connection2Server (conn), CO_LOGSTREAM))
        FlapSave (Connection2Server (conn), pak, TRUE);
    
    QueueEnqueueData (conn, QUEUE_FLAP, pak->id, time (NULL),
                      pak, 0, NULL, &SrvCallBackFlap);
    pak = NULL;
}

Server *SrvRegisterUIN (Server *serv, const char *pass)
{
    Server *news;
#ifdef ENABLE_PEER2PEER
    Connection *newl;
#endif

    assert (pass);
    assert (*pass);
    
    if (!(news = Connection2Server (ConnectionC (TYPE_SERVER))))
        return NULL;

#ifdef ENABLE_PEER2PEER
    if (!(newl = ServerChild (news, NULL, TYPE_MSGLISTEN)))
    {
        ConnectionD (Server2Connection (news));
        return NULL;
    }
    news->assoc = newl;
    newl->c_open = &ConnectionInitPeer;
    if (serv && serv->assoc)
    {
        newl->version = serv->assoc->version;
        newl->status = newl->pref_status = serv->assoc->pref_status;
        newl->flags = serv->assoc->flags & ~CONN_CONFIGURED;
    }
    else
    {
        newl->version = 8;
        newl->status = newl->pref_status = prG->s5Use ? 2 : TCP_OK_FLAG;
    }
    newl->flags &= ~CONN_AUTOLOGIN;
#endif

    if (serv)
    {
        assert (serv->type == TYPE_SERVER);
        
        news->flags   = serv->flags & ~CONN_CONFIGURED;
        news->version = serv->version;
        news->pref_status  = ims_online;
        news->pref_server  = strdup (serv->pref_server);
        news->pref_port    = serv->pref_port;
        news->pref_passwd  = strdup (pass);
    }
    else
    {
        news->version = 8;
        news->pref_status  = ims_online;
        news->pref_server  = strdup ("login.icq.com");
        news->pref_port    = 5190;
        news->pref_passwd  = strdup (pass);
        news->flags |= CONN_AUTOLOGIN;
    }
    news->uin = 0;
    s_repl (&news->screen, "");
    news->server = strdup (news->pref_server);
    news->port = news->pref_port;
    news->passwd = strdup (pass);
    news->c_open = &ConnectionInitServer;
    return news;
}

/* test in this order */
#define STATUSF_ICQINV       0x00000100
#define STATUSF_ICQDND       0x00000002
#define STATUSF_ICQOCC       0x00000010
#define STATUSF_ICQNA        0x00000004
#define STATUSF_ICQAWAY      0x00000001
#define STATUSF_ICQFFC       0x00000020

#define STATUS_ICQOFFLINE    0xffffffff
#define STATUS_ICQINV         STATUSF_ICQINV
#define STATUS_ICQDND        (STATUSF_ICQDND | STATUSF_ICQOCC | STATUSF_ICQAWAY)
#define STATUS_ICQOCC        (STATUSF_ICQOCC | STATUSF_ICQAWAY)
#define STATUS_ICQNA         (STATUSF_ICQNA  | STATUSF_ICQAWAY)
#define STATUS_ICQAWAY        STATUSF_ICQAWAY
#define STATUS_ICQFFC         STATUSF_ICQFFC
#define STATUS_ICQONLINE     0x00000000

status_t IcqToStatus (UDWORD status)
{
    status_t tmp;

    if (status == (UWORD)STATUS_ICQOFFLINE)
        return ims_offline;
    tmp = (status & STATUSF_ICQINV) ? ims_inv : ims_online;
    if (status & STATUSF_ICQDND)
        return ContactCopyInv (tmp, imr_dnd);
    if (status & STATUSF_ICQOCC)
        return ContactCopyInv (tmp, imr_occ);
    if (status & STATUSF_ICQNA)
        return ContactCopyInv (tmp, imr_na);
    if (status & STATUSF_ICQAWAY)
        return ContactCopyInv (tmp, imr_away);
    if (status & STATUSF_ICQFFC)
        return ContactCopyInv (tmp, imr_ffc);
    return tmp;
}

UDWORD IcqFromStatus (status_t status)
{
    UDWORD isinv = ContactIsInv (status) ? STATUSF_ICQINV : 0;
    switch (ContactClearInv (status))
    {
        case imr_offline:  return STATUS_ICQOFFLINE;
        default:
        case imr_online:   return STATUS_ICQONLINE | isinv;
        case imr_ffc:      return STATUS_ICQFFC    | isinv;
        case imr_away:     return STATUS_ICQAWAY   | isinv;
        case imr_na:       return STATUS_ICQNA     | isinv;
        case imr_occ:      return STATUS_ICQOCC    | isinv;
        case imr_dnd:      return STATUS_ICQDND    | isinv;
    }
}

#define STATUSF_ICQBIRTH     0x00080000
#define STATUSF_ICQWEBAWARE  0x00010000
#define STATUSF_ICQIP        0x00020000
#define STATUSF_ICQDC_AUTH   0x10000000
#define STATUSF_ICQDC_CONT   0x20000000

statusflag_t IcqToFlags (UDWORD status)
{
    statusflag_t flags = imf_none;
    if (status & STATUSF_ICQBIRTH)
        flags |= imf_birth;
    if (status & STATUSF_ICQWEBAWARE)
        flags |= imf_web;
    if (status & STATUSF_ICQDC_AUTH)
        flags |= imf_dcauth;
    if (status & STATUSF_ICQDC_CONT)
        flags |= imf_dccont;
    return flags;
}

UDWORD IcqFromFlags (statusflag_t flags)
{
    UDWORD status = 0;
    if (flags & imf_birth)
        status |= STATUSF_ICQBIRTH;
    if (flags & imf_web)
        status |= STATUSF_ICQWEBAWARE;
    if (flags & imf_dcauth)
        status |= STATUSF_ICQDC_AUTH;
    if (flags & imf_dccont)
        status |= STATUSF_ICQDC_CONT;
    return status;
}

UDWORD IcqIsUIN (const char *screen)
{
    UDWORD uin = 0;
    while (*screen)
    {
        if (*screen < '0' || *screen > '9')
            return 0;
        uin *= 10;
        uin += *screen - '0';
        screen++;
    }
    return uin;
}
