
/*
 * Helper functions of various sources for cmd_pkt_cmd_v5.c.
 */

#include "micq.h"
#include "packet.h"
#include "cmd_pkt_cmd_v5.h"
#include "cmd_pkt_cmd_v5_util.h"
#include "util_ui.h"
#include "util.h"
#include "conv.h"
#include "cmd_pkt_server.h"
#include "contact.h"
#include "sendmsg.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <netinet/in.h> /* for htonl, htons */

static void Gen_Checksum (UBYTE * buf, UDWORD len);
static UDWORD Scramble_cc (UDWORD cc);
static void Wrinkle (void *buf, size_t len);

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
 *
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
Packet *PacketCv5 (Session *sess, UWORD cmd)
{
    Packet *pak = PacketC ();
    BOOL  iss2  = cmd != CMD_KEEP_ALIVE && cmd != CMD_SEND_TEXT_CODE && cmd != CMD_ACK;
    UWORD seq   = cmd != CMD_ACK ? sess->seq_num  : 0;
    UWORD seq2  = iss2 ? sess->seq_num2 : 0;

    assert (pak);
    assert (sess);
    assert (sess->ver == 5);

    pak->cmd = cmd;
    pak->ver = sess->ver;
    pak->id  = (seq2 << 16) + seq;

    PacketWrite2 (pak, sess->ver);  /* 5 */
    PacketWrite4 (pak, 0);
    PacketWrite4 (pak, sess->uin);
    PacketWrite4 (pak, sess->our_session);
    PacketWrite2 (pak, cmd);
    PacketWrite2 (pak, seq);
    PacketWrite2 (pak, seq2);
    PacketWrite4 (pak, 0);     /* reserved for check code */

    return pak;
}

/*
 * Finalize a v5 packet and enqueue for sending.
 */
void PacketEnqueuev5 (Packet *pak, Session *sess)
{
    UDWORD cmd = pak->cmd;
    BOOL iss2 = cmd != CMD_KEEP_ALIVE && cmd != CMD_SEND_TEXT_CODE && cmd != CMD_ACK;

    assert (pak->bytes > 0x18);
    assert (sess);
    assert (sess->ver == 5);

    sess->last_cmd[sess->seq_num & 0x3ff] = cmd;
    if (iss2)           sess->seq_num2++;

    if (cmd != CMD_ACK)
    {
        sess->real_packs_sent++;
        sess->seq_num++;

        QueueEnqueueData (queue, sess, pak->id, QUEUE_TYPE_UDP_RESEND,
                          0, time (NULL) + (sess->sok >= 0 ? 10 : 0), -1,
                          (UBYTE *)pak, NULL, &UDPCallBackResend);
    }
    else
    {
        PacketSendv5 (pak, sess);
        Debug (64, "%p --> %s", pak, i18n (860, "freeing (is-ack) packet"));
        free (pak);
    }
    if (uiG.Verbose & 4)
    {
        Time_Stamp ();
        M_print (" \x1b«" COLCLIENT "");
        M_print (i18n (775, "Outgoing packet:"));
        M_print (" %04x %08x:%08x %04x (%s)" COLNONE "\n",
                 PacketReadAt2 (pak, CMD_v5_OFF_VER), PacketReadAt4 (pak, CMD_v5_OFF_SESS),
                 PacketReadAt4 (pak, CMD_v5_OFF_SEQ), PacketReadAt2 (pak, CMD_v5_OFF_SEQ2),
                 CmdPktSrvName (PacketReadAt2 (pak, CMD_v5_OFF_CMD)));
        Hex_Dump (&pak->data, pak->bytes);
        M_print ("\x1b»\n");
    }
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

void Gen_Checksum (UBYTE * buf, UDWORD len)
{
    UDWORD cc, cc2;
    UDWORD r1, r2;

    cc = buf[8];
    cc <<= 8;
    cc += buf[4];
    cc <<= 8;
    cc += buf[2];
    cc <<= 8;
    cc += buf[6];

    r1 = rand () % (len - 0x18);
    r1 += 0x18;
    r2 = rand () & 0xff;

    cc2 = r1;
    cc2 <<= 8;
    cc2 += buf[r1];
    cc2 <<= 8;
    cc2 += r2;
    cc2 <<= 8;
    cc2 += table[r2];
    cc2 ^= 0xff00ff;

    DW_2_Chars (&buf[20], cc ^ cc2);
}

UDWORD Scramble_cc (UDWORD cc)
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

void Wrinkle (void *buf, size_t len)
{
    UDWORD checkcode;
    UDWORD code1, code2, code3;
    UDWORD pos;
    UDWORD data;

    Gen_Checksum (buf, len);
    checkcode = Chars_2_DW (&((UBYTE *) buf)[20]);
    code1 = len * 0x68656c6cL;
    code2 = code1 + checkcode;
    pos = 0xa;
    for (; pos < (len); pos += 4)
    {
        code3 = code2 + table[pos & 0xFF];
        data = Chars_2_DW (&((UBYTE *) buf)[pos]);
        data ^= code3;
        DW_2_Chars (&((UBYTE *) buf)[pos], data);
    }
    checkcode = Scramble_cc (checkcode);
    DW_2_Chars (&((UBYTE *) buf)[20], checkcode);
}

/*
 * Actually send a v5 packet.
 */
void PacketSendv5 (Packet *pak, Session *sess)
{
    UBYTE s5len = (s5G.s5Use ? 10 : 0);
    UBYTE *body = malloc (pak->bytes + s5len + 3);
    UBYTE *data = body + s5len;
    
    assert (pak);
    assert (sess);
    assert (body);

    Debug (64, "--- %p %s", pak, i18n (858, "sending packet"));

    memcpy (data, pak->data, pak->bytes);
    Wrinkle (data, pak->bytes);
    if (s5G.s5Use)
    {
        body[0] = 0;
        body[1] = 0;
        body[2] = 0;
        body[3] = 1;
        *(UDWORD *) &body[4] = htonl (s5G.s5DestIP);
        *(UWORD  *) &body[8] = htons (s5G.s5DestPort);
        data = body;
    }
    sockwrite (sess->sok, data, pak->bytes + s5len);
    sess->Packets_Sent++;
}

/*
 * Callback function that handles UDP resends.
 */
void UDPCallBackResend (struct Event *event)
{
    Packet *pak = (Packet *)event->body;

    UWORD  cmd     = PacketReadAt2 (pak, CMD_v5_OFF_CMD);
    UDWORD session = PacketReadAt4 (pak, CMD_v5_OFF_SESS);

    event->attempts++;

    if (session != event->sess->our_session)
    {
        M_print (i18n (856, "Discarded a %04x (%s) packet from old session %08x (current: %08x).\n"),
                 cmd, CmdPktSrvName (cmd),
                 session, event->sess->our_session);

        Debug (64, "--> %p (^%p ^-%p) %s", event->body, event, event->info, i18n (861, "freeing (old) packet"));
        free (event->body);
        if (event->info)
            free (event->info);
        free (event);
    }
    else if (event->attempts <= MAX_RETRY_ATTEMPTS)
    {
        if (uiG.Verbose & 32)
        {
            M_print (i18n (826, "Resending message %04x (%s) sequence %04x (attempt #%d, len %d).\n"),
                     cmd, CmdPktCmdName (cmd),
                     event->seq >> 16, event->attempts, event->len);
        }
        PacketSendv5 (pak, event->sess);
        event->due = time (NULL) + 10;
        QueueEnqueue (queue, event);
    }
    else
    {
        if (cmd == CMD_SEND_MESSAGE)
        {
            UWORD  type = PacketReadAt2 (pak, CMD_v5_OFF_PARAM + 4);
            UDWORD tuin = PacketReadAt4 (pak, CMD_v5_OFF_PARAM);
            char  *data = &pak->data[CMD_v5_OFF_PARAM + 8];

            M_print ("\n");
            M_print (i18n (830, "Discarding message to %s after %d send attempts.  Message content:\n"),
                     ContactFindName (tuin), event->attempts - 1);

            if (type == URL_MESS || type == MRURL_MESS)
            {
                char *tmp = strchr (data, '\xFE');
                if (tmp != NULL)
                {
                    char url_desc[1024], url_data[1024];
                    
                    *tmp = 0;
                    ConvUnixWin (data);
                    strcpy (url_desc, data);
                    tmp++;
                    data = tmp;
                    ConvUnixWin (data);
                    strcpy (url_data, data);

                    M_print (i18n (628, " Description: " COLMESS "%s" COLNONE "\n"), url_desc);
                    M_print (i18n (629, " URL:         " COLMESS "%s" COLNONE), url_data);
                }
            }
            else if (type == NORM_MESS || type == MRNORM_MESS)
            {
                ConvUnixWin (data);
                M_print (COLMESS "%s", data);
                M_print (COLNONE " ");
            }
        }
        else
        {
            M_print (i18n (825, "Discarded a %04x (%s) packet"), cmd, CmdPktSrvName (cmd));
            if (cmd == CMD_LOGIN || cmd == CMD_KEEP_ALIVE)
            {
                M_print (i18n (632, "\n\aConnection unstable. Exiting...."));
                event->sess->Quit = TRUE;
            }
        }
        M_print ("\n");

        Debug (64, "--> %p (^%p ^-%p) %s", event->body, event, event->info, i18n (862, "freeing (disc) packet"));
        free (event->body);
        if (event->info)
            free (event->info);
        free (event);
    }
}
