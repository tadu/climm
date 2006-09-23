
/* Copyright: This file may be distributed under version 2 of the GPL licence.
 * $Id$
 */

#include "micq.h"
#include "oldicq_base.h"
#include "oldicq_compat.h"
#include "oldicq_client.h"
#include "oldicq_server.h"
#include "oldicq_util.h"
#include "util.h"
#include "util_ui.h"
#include "cmd_user.h"
#include "icq_response.h"
#include "preferences.h"
#include "server.h"
#include "connection.h"
#include "packet.h"
#include "contact.h"
#include "util_io.h"
#include "util_syntax.h"
#include "oscar_base.h"
#include "conv.h"
#include <assert.h>

static void CmdPktSrvCallBackKeepAlive (Event *event);
static BOOL Is_Repeat_Packet (UWORD this_seq);
static void Got_SEQ (UWORD this_seq);

static jump_srv_f CmdPktSrvMulti, CmdPktSrvAck;

static jump_srv_t jump[] = {
    { SRV_ACK,                &CmdPktSrvAck,   "SRV_ACK"                },
    { SRV_GO_AWAY,            NULL,            "SRV_GO_AWAY"            },
    { SRV_NEW_UIN,            NULL,            "SRV_NEW_UIN"            },
    { SRV_LOGIN_REPLY,        NULL,            "SRV_LOGIN_REPLY"        },
    { SRV_BAD_PASS,           NULL,            "SRV_BAD_PASS"           },
    { SRV_BAD_PASSWORD,       NULL,            "SRV_BAD_PASSWORD"       },
    { SRV_USER_ONLINE,        NULL,            "SRV_USER_ONLINE"        },
    { SRV_USER_OFFLINE,       NULL,            "SRV_USER_OFFLINE"       },
    { SRV_QUERY,              NULL,            "SRV_QUERY"              },
    { SRV_USER_FOUND,         NULL,            "SRV_USER_FOUND"         },
    { SRV_END_OF_SEARCH,      NULL,            "SRV_END_OF_SEARCH"      },
    { SRV_NEW_USER,           NULL,            "SRV_NEW_USER"           },
    { SRV_UPDATE_EXT,         NULL,            "SRV_UPDATE_EXT"         },
    { SRV_RECV_MESSAGE,       NULL,            "SRV_RECV_MESSAGE"       },
    { SRV_X2,                 NULL,            "SRV_X2"                 },
    { SRV_NOT_CONNECTED,      NULL,            "SRV_NOT_CONNECTED"      },
    { SRV_TRY_AGAIN,          NULL,            "SRV_TRY_AGAIN"          },
    { SRV_SYS_DELIVERED_MESS, NULL,            "SRV_SYS_DELIVERED_MESS" },
    { SRV_INFO_REPLY,         NULL,            "SRV_INFO_REPLY"         },
    { SRV_EXT_INFO_REPLY,     NULL,            "SRV_EXT_INFO_REPLY"     },
    { SRV_TCP_REQUEST,        NULL,            "SRV_TCP_REQUEST"        },
    { SRV_STATUS_UPDATE,      NULL,            "SRV_STATUS_UPDATE"      },
    { SRV_SYSTEM_MESSAGE,     NULL,            "SRV_SYSTEM_MESSAGE"     },
    { SRV_UPDATE_SUCCESS,     NULL,            "SRV_UPDATE_SUCCESS"     },
    { SRV_UPDATE_FAIL,        NULL,            "SRV_UPDATE_FAIL"        },
    { SRV_AUTH_UPDATE,        NULL,            "SRV_AUTH_UPDATE"        },
    { SRV_MULTI_PACKET,       &CmdPktSrvMulti, "SRV_MULTI_PACKET"       },
    { SRV_X1,                 NULL,            "SRV_X1"                 },
    { SRV_RAND_USER,          NULL,            "SRV_RAND_USER"          },
    { SRV_META_USER,          NULL,            "SRV_META_USER"          },
    { 0, NULL, "" }
};

static UWORD recv_packs[MAX_SEQ_DEPTH];
static UWORD start = 0, end = 0;

static BOOL Is_Repeat_Packet (UWORD this_seq)
{
    UWORD i;

    assert (end <= MAX_SEQ_DEPTH);
    for (i = start; i != end; i++)
    {
        if (i > MAX_SEQ_DEPTH)
        {
            i = -1;
            continue;
        }
        if (recv_packs[i] == this_seq)
        {
            if (prG->verbose & DEB_PROTOCOL)
                rl_printf (i18n (1623, "Double packet %04x.\n"), this_seq);
            return TRUE;
        }
    }
    return FALSE;
}

static void Got_SEQ (UWORD this_seq)
{
    recv_packs[end++] = this_seq;
    if (end > MAX_SEQ_DEPTH)
        end = 0;

    if (end == start)
        start++;

    if (start > MAX_SEQ_DEPTH)
        start = 0;
}

/*
 * Returns the name of the server packet.
 */
const char *CmdPktSrvName (int cmd)
{
    char buf[10];
    jump_srv_t *t;
    
    for (t = jump; t->cmd; t++)
        if (cmd == t->cmd)
            return t->cmdname;
    snprintf (buf, 9, "0x%04x", cmd);
    buf[9] = '\0';
    return strdup (buf);
}

/*
 * Reads from given socket and processes server packet received.
 */
void CmdPktSrvRead (Connection *conn)
{
    Packet *pak;
    char *f;
    UDWORD session, uin, id;
    UWORD cmd, seq, seq2;
#if ICQ_VER != 5
    int s;
#endif

    pak = UtilIOReceiveUDP (conn);
    if (!pak)
        return;

    pak->ver = PacketRead2 (pak);
               PacketRead1 (pak); /* zero */
    session  = PacketRead4 (pak);
    cmd      = PacketRead2 (pak);
    seq      = PacketRead2 (pak);
    seq2     = PacketRead2 (pak);
    uin      = PacketRead4 (pak);
               PacketRead4 (pak); /* check */
    id = seq2 << 16 | seq;
#if ICQ_VER != 5
    s = pak->len;
#endif
    
    if (prG->verbose & DEB_PACK5DATA)
    {
        UDWORD rpos = pak->rpos;
        rl_printf ("%s " COLINDENT "%s", s_now, COLSERVER);
        rl_print  (i18n (1774, "Incoming packet:"));
        rl_printf (" %04x %08lx:%04x%04x %04x (%s)%s\n",
                 pak->ver, session, seq2, seq, cmd, CmdPktSrvName (cmd), COLNONE);
#if ICQ_VER == 5
        pak->rpos = 0;
        rl_print  (f = PacketDump (pak, "gv5sp", COLDEBUG, COLNONE));
        free (f);
        pak->rpos = rpos;
#else
        rl_print  (s_dump (pak->data, s));
#endif
        rl_print  (COLEXDENT "\r");
    }
    if (pak->len < 21)
    {
        if (prG->verbose & DEB_PROTOCOL)
            rl_print (i18n (1867, "Received a malformed (too short) packet - ignored.\n"));
        return;
    }
    if (session != conn->our_session)
    {
        if (prG->verbose & DEB_PROTOCOL)
        {
            rl_printf (i18n (1606, "Received a bad session ID %08lx (correct: %08lx) with cmd %04x ignored.\n"),
                     session, conn->our_session, cmd);
        }
        return;
    }
    if (cmd != SRV_NEW_UIN && Is_Repeat_Packet (seq))
    {
        if (seq && cmd != SRV_ACK)
        {
            if (prG->verbose & DEB_PROTOCOL)
            {
                rl_printf (i18n (1032, "debug: double packet #%04lx type %04x (%s)\n"),
                         id, cmd, CmdPktSrvName (cmd));
            }
            CmdPktCmdAck (conn, id);       /* LAGGGGG!! */ 
            return;
        }
    }
    if (cmd != SRV_ACK)
    {
        Got_SEQ (id);
        CmdPktCmdAck (conn, id);
        conn->stat_real_pak_rcvd++;
    }
    CmdPktSrvProcess (conn, ContactUIN (conn, uin), pak, cmd, pak->ver, id);
}

/*
 * Handles sending keep alives regularly
 */
static void CmdPktSrvCallBackKeepAlive (Event *event)
{
    if (event->conn)
    {
        ASSERT_SERVER_OLD (event->conn);
        CmdPktCmdKeepAlive (event->conn);
        event->due = time (NULL) + 120;
        QueueEnqueue (event);
    }
    else
        EventD (event);
}

/*
 * Process the given server packet
 */
void CmdPktSrvProcess (Connection *conn, Contact *cont, Packet *pak,
                       UWORD cmd, UWORD ver, UDWORD seq)
{
    jump_srv_t *t;
    static int loginmsg = 0;
    unsigned char ip[4];
    strc_t ctext;
    char *text;
    UWORD wdata;
    UDWORD status, uin;
    
    if (!cont)
        return;
    
    for (t = jump; t->cmd; t++)
    {
        if (cmd == t->cmd && t->f)
        {
            t->f (conn, cont, pak, cmd, ver, seq);
            return;
        }
    }

    switch (cmd)
    {
        case SRV_META_USER:
            Meta_User (conn, cont, pak);
            break;
        case SRV_NEW_UIN:
            rl_printf (i18n (9999, "The new UIN is %s!\n"), cont->screen);
            break;
        case SRV_UPDATE_FAIL:
            rl_print (i18n (1640, "Failed to update info.\n"));
            break;
        case SRV_UPDATE_SUCCESS:
            rl_print (i18n (1641, "User info successfully updated.\n"));
            break;
        case SRV_LOGIN_REPLY:
            rl_printf ("%s %s%s%s %s\n", s_now, COLCONTACT, cont->screen, COLNONE, i18n (1050, "Login successful!"));
            CmdPktCmdLogin1 (conn);
            CmdPktCmdContactList (conn);
            CmdPktCmdInvisList (conn);
            CmdPktCmdVisList (conn);
            conn->status = prG->status;
            conn->connect = CONNECT_OK | CONNECT_SELECT_R;
            uiG.reconnect_count = 0;
            if (loginmsg++)
                break;

#if ICQ_VER == 0x0002
            PacketRead4 (pak);
#elif ICQ_VER != 0x0004
            PacketRead4 (pak);
            PacketRead4 (pak);
            PacketRead4 (pak);
#endif
            ip[0] = PacketRead1 (pak);
            ip[1] = PacketRead1 (pak);
            ip[2] = PacketRead1 (pak);
            ip[3] = PacketRead1 (pak);
            rl_printf ("%s %s%*s%s %s %u.%u.%u.%u\n",
                s_now, COLCONTACT, uiG.nick_len + s_delta (cont->nick), cont->nick,
                COLNONE, i18n (1642, "IP:"), ip[0], ip[1], ip[2], ip[3]);
            QueueEnqueueData (conn, QUEUE_UDP_KEEPALIVE, 0, time (NULL) + 120,
                              NULL, 0, NULL, &CmdPktSrvCallBackKeepAlive);
            break;
        case SRV_RECV_MESSAGE:
            Recv_Message (conn, pak);
            break;
        case SRV_X1:
            if (prG->verbose & DEB_PROTOCOL)
                rl_print (i18n (1643, "Acknowledged SRV_X1 0x021C Done Contact list?\n"));
            CmdUser ("\\e");
            conn->connect |= CONNECT_OK;
            break;
        case SRV_X2:
            if (prG->verbose & DEB_PROTOCOL)
                rl_print (i18n (1644, "Acknowledged SRV_X2 0x00E6 Done old messages?\n"));
            CmdPktCmdAckMessages (conn);
            break;
        case SRV_INFO_REPLY:
            uin = PacketRead4 (pak);
            if (!uin || !(cont = ContactUIN (conn, uin)))
                break;
            rl_printf (i18n (2214, "Info for %s%lu%s:\n"), COLSERVER, uin, COLNONE);
            Display_Info_Reply (cont, pak, IREP_HASAUTHFLAG);
            break;
        case SRV_EXT_INFO_REPLY:
            Display_Ext_Info_Reply (conn, pak);
            break;
        case SRV_USER_OFFLINE:
            uin = PacketRead4 (pak);
            if ((cont = ContactUIN (conn, uin)))
                IMOffline (cont, conn);
            break;
        case SRV_BAD_PASS:
            rl_print (i18n (1645, "You entered an incorrect password.\n"));
            exit (1);
            break;
        case SRV_TRY_AGAIN:
            rl_printf ("%s %s" , s_now, COLSERVER);
            rl_print  (i18n (1646, "Server is busy.\n"));
            uiG.reconnect_count++;
            if (uiG.reconnect_count >= MAX_RECONNECT_ATTEMPTS)
            {
                rl_printf ("%s\n", i18n (1034, "Maximum number of tries reached. Giving up."));
                uiG.quit = TRUE;
                break;
            }
            rl_printf (i18n (1082, "Trying to reconnect... [try %d out of %d]\n"), uiG.reconnect_count, MAX_RECONNECT_ATTEMPTS);
            QueueEnqueueData (conn, /* FIXME: */ 0, 0, time (NULL) + 5, NULL, 0, NULL, &CallBackServerInitV5); 
            break;
        case SRV_USER_ONLINE:
            uin = PacketRead4 (pak);
            cont = ContactUIN (conn, uin);
            if (!cont || !CONTACT_DC (cont))
                return;
            OptSetVal (&cont->copts, CO_TIMESEEN, time (NULL));
            OptSetVal (&cont->copts, CO_TIMEONLINE, time (NULL));
            cont->dc->ip_rem  = PacketRead4 (pak);
            cont->dc->port    = PacketRead4 (pak);
            cont->dc->ip_loc  = PacketRead4 (pak);
            cont->dc->type    = PacketRead1 (pak);
            status            = PacketRead4 (pak);
            cont->dc->version = PacketRead4 (pak);
                                PacketRead4 (pak);
                                PacketRead4 (pak);
                                PacketRead4 (pak);
            cont->dc->id1     = PacketRead4 (pak);
            cont->dc->id2     = PacketRead4 (pak);
            cont->dc->id3     = PacketRead4 (pak);
            ContactSetVersion (cont);
            IMOnline (cont, conn, IcqToStatus (status), IcqToFlags (status), status, NULL);
            break;
        case SRV_STATUS_UPDATE:
            uin = PacketRead4 (pak);
            if ((cont = ContactUIN (conn, uin)))
            {
                status = PacketRead4 (pak);
                IMOnline (cont, conn, IcqToStatus (status), IcqToFlags (status), status, NULL);
            }
            break;
        case SRV_GO_AWAY:
        case SRV_NOT_CONNECTED:
            rl_printf ("%s %s\n", s_now, i18n (1039, "The server claims we're not connected.\n"));
            uiG.reconnect_count++;
            if (uiG.reconnect_count >= MAX_RECONNECT_ATTEMPTS)
            {
                rl_printf ("%s\n", i18n (1034, "Maximum number of tries reached. Giving up."));
                uiG.quit = TRUE;
                break;
            }
            rl_printf (i18n (1082, "Trying to reconnect... [try %d out of %d]\n"), uiG.reconnect_count, MAX_RECONNECT_ATTEMPTS);
            QueueEnqueueData (conn, /* FIXME: */ 0, 0, time (NULL) + 5, NULL, 0, NULL, &CallBackServerInitV5);
            break;
        case SRV_END_OF_SEARCH:
            rl_print (i18n (1045, "Search Done."));
            if (PacketReadLeft (pak) >= 1)
            {
                rl_print ("\t");
                if (PacketRead1 (pak) == 1)
                    rl_print (i18n (1044, "Too many users found."));
                else
                    rl_print (i18n (1043, "All users found."));
            }
            rl_print ("\n");
            break;
        case SRV_USER_FOUND:
            uin = PacketRead4 (pak);
            if (!uin || !(cont = ContactUIN (conn, uin)))
                break;
            rl_printf (i18n (1968, "User found:\n"));
            Display_Info_Reply (cont, pak, IREP_HASAUTHFLAG);
            break;
        case SRV_RAND_USER:
            if (PacketReadLeft (pak) != 37)
            {
                rl_printf ("%s\n", i18n (1495, "No Random User Found"));
                return;
            }

            rl_print (s_dump (pak->data + pak->rpos, pak->len - pak->rpos));

            if (!(cont = ContactUIN (conn, PacketRead4 (pak))))
                return;
            if (!CONTACT_DC (cont))
                return;

            cont->dc->ip_rem  = PacketReadB4 (pak);
            cont->dc->port    = PacketRead4  (pak);
            cont->dc->ip_loc  = PacketReadB4 (pak);
            cont->dc->type    = PacketRead1  (pak);
            status            = PacketRead4  (pak);
            cont->dc->version = PacketRead2  (pak);
            
            cont->status = IcqToStatus (status);
            cont->flags = IcqToFlags (status);
            cont->nativestatus = status;

            rl_printf ("%-15s %s\n", i18n (1440, "Random User:"), cont->screen);
            rl_printf ("%-15s %s:%lu\n", i18n (1441, "remote IP:"), 
                      s_ip (cont->dc->ip_rem), cont->dc->port);
            rl_printf ("%-15s %s\n", i18n (1451, "local  IP:"),  s_ip (cont->dc->ip_loc));
            rl_printf ("%-15s %s\n", i18n (1454, "Connection:"), cont->dc->type == 4
                      ? i18n (1493, "Peer-to-Peer") : i18n (1494, "Server Only"));
            rl_printf ("%-15s %s\n", i18n (1452, "Status:"), s_status (cont->status, cont->nativestatus));
            rl_printf ("%-15s %d\n", i18n (1453, "TCP version:"), cont->dc->version);
        
            CmdPktCmdMetaReqInfo (conn, cont);
            break;
        case SRV_SYS_DELIVERED_MESS:
            uin   = PacketRead4 (pak);
            wdata = PacketRead2 (pak);
            ctext = PacketReadL2Str (pak, NULL);
            
            text = strdup (c_in_to_split (ctext, cont));

            if ((cont = ContactUIN (conn, uin)))
            {
                IMSrvMsg (cont, conn, NOW, OptSetVals (NULL, CO_ORIGIN, CV_ORIGIN_v5,
                          CO_MSGTYPE, wdata, CO_MSGTEXT, text, 0));
                Auto_Reply (conn, cont);
            }
            free (text);
            break;
        case SRV_AUTH_UPDATE:
            break;
        default:               /* commands we dont handle yet */
            rl_printf ("%s %s", s_now, COLCLIENT);
            rl_printf (i18n (1648, "The response was %04x\t"), cmd);
            rl_printf (i18n (1649, "The version was %x\t"), ver);
            rl_printf (i18n (1650, "\nThe SEQ was %04lx\t"), seq);
            rl_printf (i18n (1651, "The size was %d\n"), pak->len - pak->rpos);
            rl_print  (s_dump (pak->data + pak->rpos, pak->len - pak->rpos));
            rl_printf ("%s\n", COLNONE);
            break;
    }
}


/*
 * Splits multi packet into several packets
 */
static JUMP_SRV_F (CmdPktSrvMulti)
{
    int i, num_pack, llen;
    Packet *npak;
    UDWORD session, uin;
    UWORD /*cmd, seq,*/ seq2;
    char *f;

    num_pack = PacketRead1 (pak);

    for (i = 0; i < num_pack; i++)
    {
        llen = PacketRead2 (pak);
        if (pak->rpos + llen > pak->len)
        {
            if (prG->verbose & DEB_PROTOCOL)
                rl_print (i18n (1868, "Got a malformed (to long subpacket) multi-packet - remainder ignored.\n"));
            return;
        }
        
        npak = PacketC ();
        memcpy (npak->data, pak->data + pak->rpos, llen);
        npak->len = llen;
        pak->rpos += llen;
        
        npak->ver = PacketRead2 (npak);
                    PacketRead1 (npak); /* zero */
        session   = PacketRead4 (npak);
        cmd       = PacketRead2 (npak);
        seq       = PacketRead2 (npak);
        seq2      = PacketRead2 (npak);
        uin       = PacketRead4 (npak);
                    PacketRead4 (npak); /* check */

        if (prG->verbose & DEB_PACK5DATA)
        {
            UDWORD rpos = pak->rpos;
            rl_printf ("%s " COLINDENT "%s", s_now, COLSERVER);
            rl_print  (i18n (1823, "Incoming partial packet:"));
            rl_printf (" %04x %08lx:%04x%04lx %04x (%s)%s\n",
                     ver, session, seq2, seq, cmd, CmdPktSrvName (cmd), COLNONE);
#if ICQ_VER == 5
            pak->rpos = 0;
            rl_print  (f = PacketDump (pak, "gv5sp", COLDEBUG, COLNONE));
            free (f);
            pak->rpos = rpos;
#else
            rl_print  (s_dump (pak->data, llen));
#endif
            rl_print (COLEXDENT "\n");
        }

        CmdPktSrvProcess (conn, ContactUIN (conn, uin), npak, cmd, ver, seq << 16 | seq2);
    }
}

/*
 * SRV_ACK - Remove acknowledged packet from queue.
 */
static JUMP_SRV_F (CmdPktSrvAck)
{
    Event *event = QueueDequeue (conn, QUEUE_UDP_RESEND, seq);
    UDWORD ccmd;

    if (!event)
        return;

    if (pak->rpos < pak->len && (prG->verbose & DEB_PROTOCOL))
    {
        rl_printf ("%s %s %d\n", i18n (1047, "Extra Data"), i18n (1046, "Length"), pak->len - pak->rpos);
    }
    
    ccmd = PacketReadAt2 (event->pak, CMD_v5_OFF_CMD);

    DebugH (DEB_QUEUE, STR_DOT STR_DOT STR_DOT STR_DOT " ack type %04lx (%s) seq %04lx",
                      ccmd, CmdPktCmdName (ccmd), event->seq >> 16);

    if (ccmd == CMD_SEND_MESSAGE)
    {
        Contact *cont;
        
        if (!(cont = ContactUIN (conn, PacketReadAt4 (event->pak, CMD_v5_OFF_PARAM))))
            return;

        IMIntMsg (cont, conn, NOW, ims_offline, INT_MSGACK_V5,
                  c_in_to_split (PacketReadAtL2Str (event->pak, 30, NULL), cont), NULL);
    }
    
    PacketD (pak);
    EventD (event);
}
