
/*
 * Helper functions of various sources for cmd_pkt_cmd_v5.c.
 *
 * Copyright: This file may be distributed under version 2 of the GPL licence.
 *
 * $Id$
 */

#include "micq.h"
#include "packet.h"
#include "connection.h"
#include "cmd_pkt_cmd_v5.h"
#include "cmd_pkt_cmd_v5_util.h"
#include "util_ui.h"
#include "util.h"
#include "util_syntax.h"
#include "conv.h"
#include "cmd_pkt_server.h"
#include "preferences.h"
#include "contact.h"
#include "server.h"
#include "icq_response.h"
#include "util_io.h"
#include <assert.h>
#include <errno.h>
#if HAVE_NETINET_IN_H
#include <netinet/in.h> /* for htonl, htons */
#endif
#if HAVE_WINSOCK2_H
#include <winsock2.h>
#endif

static UDWORD Gen_Checksum (const Packet *pak);
static UDWORD Scramble_cc (UDWORD cc);
static Packet *Wrinkle (const Packet *pak);
static void CallBackClosev5 (Connection *conn);

typedef struct { UWORD cmd; const char *name; } jump_cmd_t;
#define jump_el(x) { x, #x },

static jump_cmd_t jump[] = {
    jump_el(CMD_ACK)              jump_el(CMD_SEND_MESSAGE)   jump_el(CMD_TCP_REQUEST)
    jump_el(CMD_LOGIN)            jump_el(CMD_REG_NEW_USER)   jump_el(CMD_CONTACT_LIST)
    jump_el(CMD_SEARCH_UIN)       jump_el(CMD_SEARCH_USER)    jump_el(CMD_KEEP_ALIVE)
    jump_el(CMD_SEND_TEXT_CODE)   jump_el(CMD_ACK_MESSAGES)   jump_el(CMD_LOGIN_1)
    jump_el(CMD_MSG_TO_NEW_USER)  jump_el(CMD_INFO_REQ)       jump_el(CMD_EXT_INFO_REQ)
    jump_el(CMD_CHANGE_PW)        jump_el(CMD_NEW_USER_INFO)  jump_el(CMD_UPDATE_EXT_INFO)
    jump_el(CMD_QUERY_SERVERS)    jump_el(CMD_QUERY_ADDONS)   jump_el(CMD_STATUS_CHANGE)
    jump_el(CMD_NEW_USER_1)       jump_el(CMD_UPDATE_INFO)    jump_el(CMD_AUTH_UPDATE)
    jump_el(CMD_KEEP_ALIVE2)      jump_el(CMD_LOGIN_2)        jump_el(CMD_ADD_TO_LIST)
    jump_el(CMD_RAND_SET)         jump_el(CMD_RAND_SEARCH)    jump_el(CMD_META_USER)
    jump_el(CMD_INVIS_LIST)       jump_el(CMD_VIS_LIST)       jump_el(CMD_UPDATE_LIST)
    { 0, "" }
};

/*
 * Return the name of a command given by number
 */
const char *CmdPktCmdName (UWORD cmd)
{
    char buf[10];
    jump_cmd_t *t;

    for (t = jump; t->cmd; t++)
        if (t->cmd == cmd)
            return t->name;
    snprintf (buf, 9, "0x%04x", cmd);
    buf[9] = '\0';
    return strdup (buf);
}

/*
 * Create a v5 packet and fill in the header.
 */
Packet *PacketCv5 (Connection *conn, UWORD cmd)
{
    Packet *pak = PacketC ();
    BOOL  iss2  = cmd != CMD_KEEP_ALIVE && cmd != CMD_SEND_TEXT_CODE && cmd != CMD_ACK;
    UWORD seq   = cmd != CMD_ACK ? conn->our_seq  : 0;
    UWORD seq2  = iss2 ? conn->our_seq2 : 0;

    assert (pak);
    assert (conn);
    assert (conn->version == 5);

    pak->cmd = cmd;
    pak->ver = conn->version;
    pak->id  = (seq2 << 16) + seq;
    pak->ref = seq;

    PacketWrite2 (pak, conn->version);  /* 5 */
    PacketWrite4 (pak, 0);
    PacketWrite4 (pak, conn->uin);
    PacketWrite4 (pak, conn->our_session);
    PacketWrite2 (pak, cmd);
    PacketWrite2 (pak, seq);
    PacketWrite2 (pak, seq2);
    PacketWrite4 (pak, 0);     /* reserved for check code */

    return pak;
}

/*
 * Finalize a v5 packet and enqueue for sending.
 */
void PacketEnqueuev5 (Packet *pak, Connection *conn)
{
    UDWORD cmd = pak->cmd;
    BOOL iss2 = cmd != CMD_KEEP_ALIVE && cmd != CMD_SEND_TEXT_CODE && cmd != CMD_ACK;

    assert (pak->len > 0x18);
    assert (conn);
    assert (conn->version == 5);

    if (iss2)           conn->our_seq2++;

    PacketSendv5 (pak, conn);

    if (prG->verbose & DEB_PACK5DATA)
    {
        char *f;
        pak->rpos = 0;
        M_printf ("%s " COLINDENT "%s", s_now, COLCLIENT);
        M_print  (i18n (1775, "Outgoing packet:"));
        M_printf (" %04x %08lx:%08lx %04x (%s) @%p%s\n",
                 PacketReadAt2 (pak, CMD_v5_OFF_VER), PacketReadAt4 (pak, CMD_v5_OFF_SESS),
                 PacketReadAt4 (pak, CMD_v5_OFF_SEQ), PacketReadAt2 (pak, CMD_v5_OFF_SEQ2),
                 CmdPktCmdName (PacketReadAt2 (pak, CMD_v5_OFF_CMD)), pak, COLNONE);
        M_print  (f = PacketDump (pak, "gv5cp", COLDEBUG, COLNONE));
        free (f);
        M_print  (COLEXDENT "\r");
    }

    if (cmd != CMD_ACK)
    {
        conn->stat_real_pak_sent++;
        conn->our_seq++;

        QueueEnqueueData (conn, QUEUE_UDP_RESEND, pak->id, time (NULL) + 10,
                          pak, 0, NULL, &UDPCallBackResend);
    }
    else
        PacketD (pak);
}

Event *ConnectionInitServerV5 (Connection *conn)
{
    if (conn->version < 5)
    {
        M_print (i18n (1869, "Protocol versions less than 5 are not supported anymore.\n"));
        return NULL;
    }
    
    conn->close = &CallBackClosev5;
    conn->cont = ContactUIN (conn, conn->uin);
    if (!conn->server || !*conn->server)
        s_repl (&conn->server, "icq.icq.com");
    if (!conn->port)
        conn->port = 4000;
    if (!conn->passwd || !*conn->passwd)
    {
        strc_t pwd;
        M_printf ("%s ", i18n (1063, "Enter password:"));
        Echo_Off ();
        pwd = UtilIOReadline (stdin);
        Echo_On ();
        conn->passwd = strdup (pwd ? pwd->txt : "");
    }
    QueueEnqueueData (conn, /* FIXME: */ 0, 0, time (NULL), NULL, 0, NULL, &CallBackServerInitV5);
    return NULL;
}

static void CallBackClosev5 (Connection *conn)
{
    conn->connect = 0;
    if (conn->sok == -1)
        return;
    CmdPktCmdSendTextCode (conn, "B_USER_DISCONNECTED");
    sockclose (conn->sok);
    conn->sok = -1;
}

void CallBackServerInitV5 (Event *event)
{
    Connection *conn = event->conn;
    int rc;

    if (!conn)
    {
        EventD (event);
        return;
    }
    ASSERT_SERVER_OLD (conn);

    if (conn->assoc && !(conn->assoc->connect & CONNECT_OK))
    {
        event->due = time (NULL) + 1;
        QueueEnqueue (event);
        return;
    }
    EventD (event);
    
    M_printf (i18n (9999, "Opening v5 connection to %s:%s%ld%s... "),
              s_wordquote (conn->server), COLQUOTE, conn->port, COLNONE);
    
    if (conn->sok < 0)
    {
        UtilIOConnectUDP (conn);

#ifdef __BEOS__
        if (conn->sok == -1)
#else
        if (conn->sok == -1 || conn->sok == 0)
#endif
        {
            rc = errno;
            M_printf (i18n (1872, "failed: %s (%d)\n"), strerror (rc), rc);
            conn->connect = 0;
            conn->sok = -1;
            return;
        }
    }
    M_print (i18n (1877, "ok.\n"));
    conn->our_seq2    = 0;
    conn->connect = 1 | CONNECT_SELECT_R;
    conn->dispatch = &CmdPktSrvRead;
    CmdPktCmdLogin (conn);
}

/**
 ** Wrinkling the packet
 **/

/********************************************************
The following data constitutes fair use for compatibility.
*********************************************************/
static const UBYTE table[] = {
    0x59, 0x60, 0x37, 0x6B, 0x65, 0x62, 0x46, 0x48, 0x53, 0x61, 0x4C, 0x59, 0x60, 0x57, 0x5B, 0x3D,
    0x5E, 0x34, 0x6D, 0x36, 0x50, 0x3F, 0x6F, 0x67, 0x53, 0x61, 0x4C, 0x59, 0x40, 0x47, 0x63, 0x39,
    0x50, 0x5F, 0x5F, 0x3F, 0x6F, 0x47, 0x43, 0x69, 0x48, 0x33, 0x31, 0x64, 0x35, 0x5A, 0x4A, 0x42,
    0x56, 0x40, 0x67, 0x53, 0x41, 0x07, 0x6C, 0x49, 0x58, 0x3B, 0x4D, 0x46, 0x68, 0x43, 0x69, 0x48,
    0x33, 0x31, 0x44, 0x65, 0x62, 0x46, 0x48, 0x53, 0x41, 0x07, 0x6C, 0x69, 0x48, 0x33, 0x51, 0x54,
    0x5D, 0x4E, 0x6C, 0x49, 0x38, 0x4B, 0x55, 0x4A, 0x62, 0x46, 0x48, 0x33, 0x51, 0x34, 0x6D, 0x36,
    0x50, 0x5F, 0x5F, 0x5F, 0x3F, 0x6F, 0x47, 0x63, 0x59, 0x40, 0x67, 0x33, 0x31, 0x64, 0x35, 0x5A,
    0x6A, 0x52, 0x6E, 0x3C, 0x51, 0x34, 0x6D, 0x36, 0x50, 0x5F, 0x5F, 0x3F, 0x4F, 0x37, 0x4B, 0x35,
    0x5A, 0x4A, 0x62, 0x66, 0x58, 0x3B, 0x4D, 0x66, 0x58, 0x5B, 0x5D, 0x4E, 0x6C, 0x49, 0x58, 0x3B,
    0x4D, 0x66, 0x58, 0x3B, 0x4D, 0x46, 0x48, 0x53, 0x61, 0x4C, 0x59, 0x40, 0x67, 0x33, 0x31, 0x64,
    0x55, 0x6A, 0x32, 0x3E, 0x44, 0x45, 0x52, 0x6E, 0x3C, 0x31, 0x64, 0x55, 0x6A, 0x52, 0x4E, 0x6C,
    0x69, 0x48, 0x53, 0x61, 0x4C, 0x39, 0x30, 0x6F, 0x47, 0x63, 0x59, 0x60, 0x57, 0x5B, 0x3D, 0x3E,
    0x64, 0x35, 0x3A, 0x3A, 0x5A, 0x6A, 0x52, 0x4E, 0x6C, 0x69, 0x48, 0x53, 0x61, 0x6C, 0x49, 0x58,
    0x3B, 0x4D, 0x46, 0x68, 0x63, 0x39, 0x50, 0x5F, 0x5F, 0x3F, 0x6F, 0x67, 0x53, 0x41, 0x25, 0x41,
    0x3C, 0x51, 0x54, 0x3D, 0x5E, 0x54, 0x5D, 0x4E, 0x4C, 0x39, 0x50, 0x5F, 0x5F, 0x5F, 0x3F, 0x6F,
    0x47, 0x43, 0x69, 0x48, 0x33, 0x51, 0x54, 0x5D, 0x6E, 0x3C, 0x31, 0x64, 0x35, 0x5A, 0x00, 0x00,
};

static UDWORD Gen_Checksum (const Packet *pak)
{
    UDWORD cc, cc2;
    UDWORD r1, r2;

    cc = pak->data[8];
    cc <<= 8;
    cc += pak->data[4];
    cc <<= 8;
    cc += pak->data[2];
    cc <<= 8;
    cc += pak->data[6];

    r1 = rand () % (pak->len - 24);
    r1 += 24;
    r2 = rand () & 0xff;

    cc2 = r1;
    cc2 <<= 8;
    cc2 += pak->data[r1];
    cc2 <<= 8;
    cc2 += r2;
    cc2 <<= 8;
    cc2 += table[r2];
    cc2 ^= 0xff00ff;

    return cc ^ cc2;
}

static UDWORD Scramble_cc (UDWORD cc)
{
    UDWORD a[6];

    a[1] = cc & 0x0000001F;
    a[2] = cc & 0x03E003E0;
    a[3] = cc & 0xF8000400;
    a[4] = cc & 0x0000F800;
    a[5] = cc & 0x041F0000;

    a[1] <<= 0x0C;
    a[2] <<= 0x01;
    a[3] >>= 0x0A;
    a[4] <<= 0x10;
    a[5] >>= 0x0F;

    return a[1] + a[2] + a[3] + a[4] + a[5];
}

static Packet *Wrinkle (const Packet *opak)
{
    UDWORD code, pos, checkcode;
    Packet *pak;
    
    pak = PacketClone (opak);

    checkcode = Gen_Checksum (opak);
    code = opak->len * 0x68656c6cL + checkcode;
    PacketWriteAt4 (pak, 20, checkcode);

    for (pos = 10; pos < opak->len; pos += 4)
    {
        PacketWriteAt4 (pak, pos,
            PacketReadAt4 (opak, pos) ^ (code + table[pos & 0xFF]));
    }

    checkcode = Scramble_cc (checkcode);
    PacketWriteAt4 (pak, 20, checkcode);
    pak->len = opak->len;
    return pak;
}

/*
 * Actually send a v5 packet.
 */
void PacketSendv5 (const Packet *pak, Connection *conn)
{
    Packet *cpak;
    
    assert (pak);
    assert (conn);

    DebugH (DEB_PACKET, "---- %p sent", pak);

    cpak = Wrinkle (pak);
    UtilIOSendUDP (conn, cpak);
    PacketD (cpak);
}

void Auto_Reply (Connection *conn, Contact *cont)
{
    const char *temp;

    if (!(prG->flags & FLAG_AUTOREPLY) || !cont)
        return;

          if (conn->status & STATUSF_DND)
         temp = ContactPrefStr (cont, CO_AUTODND);
     else if (conn->status & STATUSF_OCC)
         temp = ContactPrefStr (cont, CO_AUTOOCC);
     else if (conn->status & STATUSF_NA)
         temp = ContactPrefStr (cont, CO_AUTONA);
     else if (conn->status & STATUSF_AWAY)
         temp = ContactPrefStr (cont, CO_AUTOAWAY);
     else if (conn->status & STATUSF_INV)
         temp = ContactPrefStr (cont, CO_AUTOINV);
     else
         return;

    IMCliMsg (conn, cont, OptSetVals (NULL, CO_MSGTYPE, MSG_AUTO, CO_MSGTEXT, temp, 0));
}

/*
 * Callback function that handles UDP resends.
 */
void UDPCallBackResend (Event *event)
{
    Connection *conn = event->conn;
    Packet *pak = event->pak;
    UWORD  cmd;
    UDWORD session;

    assert (pak);

    if (!event->conn)
    {
        EventD (event);
        return;
    }
    ASSERT_SERVER_OLD (conn);

    cmd     = PacketReadAt2 (pak, CMD_v5_OFF_CMD);
    session = PacketReadAt4 (pak, CMD_v5_OFF_SESS);

    event->attempts++;

    if (session != conn->our_session)
    {
        M_printf (i18n (1856, "Discarded a %04x (%s) packet from old session %08lx (current: %08lx).\n"),
                 cmd, CmdPktSrvName (cmd),
                 session, conn->our_session);
        EventD (event);
    }
    else if (event->attempts <= MAX_RETRY_ATTEMPTS)
    {
        if (prG->verbose & 32)
        {
            M_printf (i18n (1826, "Resending message %04x (%s) sequence %04lx (attempt #%ld, len %d).\n"),
                     cmd, CmdPktCmdName (cmd),
                     event->seq >> 16, event->attempts, pak->len);
        }
        PacketSendv5 (pak, conn);
        event->due = time (NULL) + 10;
        QueueEnqueue (event);
    }
    else
    {
        if (cmd == CMD_SEND_MESSAGE)
        {
            UWORD  type = PacketReadAt2 (pak, CMD_v5_OFF_PARAM + 4);
            UDWORD tuin = PacketReadAt4 (pak, CMD_v5_OFF_PARAM);
            Contact *cont = ContactUIN (conn, tuin);
            char *data = (char *) &pak->data[CMD_v5_OFF_PARAM + 8];

            M_print ("\n");
            M_printf (i18n (1830, "Discarding message to %s after %ld send attempts.  Message content:\n"),
                      cont ? cont->nick : s_sprintf ("%ld", tuin), event->attempts - 1);

            if ((type & ~MSGF_MASS) == MSG_URL)
            {
                char *tmp = strchr (data, '\xFE');
                if (tmp)
                {
                    char *url_desc;
                    const char *url_data;
                    str_s cdata = { NULL, 0, 0 };
                    
                    cdata.txt = data;
                    cdata.len = strlen (data);
                    *tmp++ = 0;
                    url_desc = strdup (ConvFromCont (&cdata, cont));
                    cdata.txt = tmp;
                    cdata.len = strlen (tmp);
                    url_data = ConvFromCont (&cdata, cont);

                    M_printf (i18n (2128, " Description: %s%s%s\n"), COLMESSAGE, url_desc, COLNONE);
                    M_printf (i18n (2129, "         URL: %s%s%s\n"), COLMESSAGE, url_data, COLNONE);
                    
                    free (url_desc);
                }
            }
            else if ((type & ~MSGF_MASS) == MSG_NORM)
            {
                str_s cdata = { NULL, 0, 0 };
                cdata.txt = data;
                cdata.len = strlen (data);
                M_printf ("%s%s%s ", COLMESSAGE, ConvFromCont (&cdata, cont), COLNONE);
            }
        }
        else
        {
            M_printf (i18n (1825, "Discarded a %04x (%s) packet"), cmd, CmdPktSrvName (cmd));
            if (cmd == CMD_LOGIN || cmd == CMD_KEEP_ALIVE)
            {
                M_print ("\a");
                M_print (i18n (1632, "\nConnection unstable. Exiting...."));
                uiG.quit = TRUE;
            }
        }
        M_print ("\n");
        EventD (event);
    }
}
