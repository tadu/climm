
/* Copyright: This file may be distributed under version 2 of the GPL licence.
 * $Id$
 */

#include "micq.h"
#include "cmd_pkt_cmd_v5.h"
#include "cmd_pkt_cmd_v5_util.h"
#include "util.h"
#include "util_ui.h"
#include "network.h"
#include "cmd_user.h"
#include "cmd_pkt_server.h"
#include "icq_response.h"
#include "preferences.h"
#include "server.h"
#include "session.h"
#include "packet.h"
#include "contact.h"
#include "util_io.h"
#include "util_str.h"
#include "util_syntax.h"
#include "conv.h"
#include <string.h>
#include <stdio.h>

static void CmdPktSrvCallBackKeepAlive (Event *event);

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
    int s;
    UDWORD session, uin, id;
    UWORD cmd, seq, seq2;

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
    s = pak->len;
    
    if (prG->verbose & DEB_PACK5DATA)
    {
        M_printf ("%s " COLINDENT COLSERVER "", s_now);
        M_print  (i18n (1774, "Incoming packet:"));
        M_printf (" %04x %08x:%04x%04x %04x (%s)" COLNONE "\n",
                 pak->ver, session, seq2, seq, cmd, CmdPktSrvName (cmd));
#if ICQ_VER == 5
        M_print  (PacketDump (pak, "gv5sp"));
#else
        M_print  (s_dump (pak->data, s));
#endif
        M_print  (COLEXDENT "\r");
    }
    if (pak->len < 21)
    {
        if (prG->verbose & DEB_PROTOCOL)
            M_print (i18n (1867, "Got a malformed (too short) packet - ignored.\n"));
        return;
    }
    if (session != conn->our_session)
    {
        if (prG->verbose & DEB_PROTOCOL)
        {
            M_printf (i18n (1606, "Got a bad session ID %08x (correct: %08x) with cmd %04x ignored.\n"),
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
                M_printf (i18n (1032, "debug: Doppeltes Packet #%04x vom Typ %04x (%s)\n"),
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
    CmdPktSrvProcess (conn, pak, cmd, pak->ver, id, uin);
}

/*
 * Handles sending keep alives regularly
 */
static void CmdPktSrvCallBackKeepAlive (Event *event)
{
    if (event->conn)
    {
        CmdPktCmdKeepAlive (event->conn);
        event->due = time (NULL) + 120;
        QueueEnqueue (event);
    }
    else
        free (event);
}

/*
 * Process the given server packet
 */
void CmdPktSrvProcess (Connection *conn, Packet *pak, UWORD cmd,
                       UWORD ver, UDWORD seq, UDWORD uin)
{
    jump_srv_t *t;
    static int loginmsg = 0;
    unsigned char ip[4];
    char *text, *ctext;
    UWORD wdata;
    Contact *cont;
    UDWORD status;
    
    for (t = jump; t->cmd; t++)
    {
        if (cmd == t->cmd && t->f)
        {
            t->f (conn, pak, cmd, ver, seq, uin);
            return;
        }
    }

    switch (cmd)
    {
        case SRV_META_USER:
            Meta_User (conn, uin, pak);
            break;
        case SRV_NEW_UIN:
            M_printf (i18n (1639, "The new UIN is %ld!\n"), uin);
            break;
        case SRV_UPDATE_FAIL:
            M_print (i18n (1640, "Failed to update info.\n"));
            break;
        case SRV_UPDATE_SUCCESS:
            M_print (i18n (1641, "User info successfully updated.\n"));
            break;
        case SRV_LOGIN_REPLY:
            M_printf ("%s " COLCONTACT "%10lu" COLNONE " %s\n", s_now, uin, i18n (1050, "Login successful!"));
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
            M_printf ("%s " COLCONTACT "%*s" COLNONE " %s: %u.%u.%u.%u\n",
                s_now, uiG.nick_len + s_delta (ContactFindName (uin)), ContactFindName (uin), i18n (1642, "IP"),
                ip[0], ip[1], ip[2], ip[3]);
            QueueEnqueueData (conn, QUEUE_UDP_KEEPALIVE, 0, 0, time (NULL) + 120,
                              NULL, NULL, &CmdPktSrvCallBackKeepAlive);
            break;
        case SRV_RECV_MESSAGE:
            Recv_Message (conn, pak);
            break;
        case SRV_X1:
            if (prG->verbose & DEB_PROTOCOL)
                M_print (i18n (1643, "Acknowledged SRV_X1 0x021C Done Contact list?\n"));
            CmdUser ("¶e");
            conn->connect |= CONNECT_OK;
            break;
        case SRV_X2:
            if (prG->verbose & DEB_PROTOCOL)
                M_print (i18n (1644, "Acknowleged SRV_X2 0x00E6 Done old messages?\n"));
            CmdPktCmdAckMessages (conn);
            break;
        case SRV_INFO_REPLY:
            Display_Info_Reply (conn, pak, i18n (2214, "Info for %s%lu%s:\n"), IREP_HASAUTHFLAG);
            break;
        case SRV_EXT_INFO_REPLY:
            Display_Ext_Info_Reply (conn, pak, i18n (1967, "More Info for"));
            M_print ("\n");
            break;
        case SRV_USER_OFFLINE:
            uin = PacketRead4 (pak);
            if ((cont = ContactByUIN (uin, 1)))
                IMOffline (cont, conn);
            break;
        case SRV_BAD_PASS:
            M_print (i18n (1645, "You entered an incorrect password.\n"));
            exit (1);
            break;
        case SRV_TRY_AGAIN:
            M_printf ("%s " COLMESSAGE, s_now);
            M_print  (i18n (1646, "Server is busy.\n"));
            uiG.reconnect_count++;
            if (uiG.reconnect_count >= MAX_RECONNECT_ATTEMPTS)
            {
                M_printf ("%s\n", i18n (1034, "Maximum number of tries reached. Giving up."));
                uiG.quit = TRUE;
                break;
            }
            M_printf (i18n (1082, "Trying to reconnect... [try %d out of %d]\n"), uiG.reconnect_count, MAX_RECONNECT_ATTEMPTS);
            QueueEnqueueData (conn, /* FIXME: */ 0, 0, 0, time (NULL) + 5, NULL, NULL, &CallBackServerInitV5); 
            break;
        case SRV_USER_ONLINE:
            uin = PacketRead4 (pak);
            cont = ContactByUIN (uin, 1);
            if (!cont)
                return;
            cont->seen_time = time (NULL);
            cont->outside_ip      = PacketRead4 (pak);
            cont->port            = PacketRead4 (pak);
            cont->local_ip        = PacketRead4 (pak);
            cont->connection_type = PacketRead1 (pak);
            status                = PacketRead4 (pak);
            cont->TCP_version     = PacketRead4 (pak);
                                    PacketRead4 (pak);
                                    PacketRead4 (pak);
                                    PacketRead4 (pak);
            cont->id1             = PacketRead4 (pak);
            cont->id2             = PacketRead4 (pak);
            cont->id3             = PacketRead4 (pak);
            ContactSetVersion (cont);
            IMOnline (cont, conn, status);
            break;
        case SRV_STATUS_UPDATE:
            uin = PacketRead4 (pak);
            if ((cont = ContactByUIN (uin, 1)))
                IMOnline (cont, conn, PacketRead4 (pak));
            break;
        case SRV_GO_AWAY:
        case SRV_NOT_CONNECTED:
            M_printf ("%s %s\n", s_now, i18n (1039, "The server claims we're not connected.\n"));
            uiG.reconnect_count++;
            if (uiG.reconnect_count >= MAX_RECONNECT_ATTEMPTS)
            {
                M_printf ("%s\n", i18n (1034, "Maximum number of tries reached. Giving up."));
                uiG.quit = TRUE;
                break;
            }
            M_printf (i18n (1082, "Trying to reconnect... [try %d out of %d]\n"), uiG.reconnect_count, MAX_RECONNECT_ATTEMPTS);
            QueueEnqueueData (conn, /* FIXME: */ 0, 0, 0, time (NULL) + 5, NULL, NULL, &CallBackServerInitV5);
            break;
        case SRV_END_OF_SEARCH:
            M_print (i18n (1045, "Search Done."));
            if (PacketReadLeft (pak) >= 1)
            {
                M_print ("\t");
                if (PacketRead1 (pak) == 1)
                    M_print (i18n (1044, "Too many users found."));
                else
                    M_print (i18n (1043, "All users found."));
            }
            M_print ("\n");
            break;
        case SRV_USER_FOUND:
            Display_Info_Reply (conn, pak, i18n (1968, "User found:\n"), IREP_HASAUTHFLAG);
            M_print ("\n");
            break;
        case SRV_RAND_USER:
            Display_Rand_User (conn, pak);
            M_print ("\n");
            break;
        case SRV_SYS_DELIVERED_MESS:
            uin   = PacketRead4 (pak);
            wdata = PacketRead2 (pak);
            ctext = PacketReadLNTS (pak);
            
            text = strdup (c_in (ctext));
            free (ctext);

            if ((cont = ContactByUIN (uin, 1)))
            {
                IMSrvMsg (cont, conn, NOW, STATUS_OFFLINE, wdata, text, 0);
                Auto_Reply (conn, uin);
            }
            free (text);
            break;
        case SRV_AUTH_UPDATE:
            break;
        default:               /* commands we dont handle yet */
            M_printf ("%s " COLCLIENT, s_now);
            M_printf (i18n (1648, "The response was %04X\t"), cmd);
            M_printf (i18n (1649, "The version was %X\t"), ver);
            M_printf (i18n (1650, "\nThe SEQ was %04X\t"), seq);
            M_printf (i18n (1651, "The size was %d\n"), pak->len - pak->rpos);
            M_print  (s_dump (pak->data + pak->rpos, pak->len - pak->rpos));
            M_print  (COLNONE "\n");
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
    UDWORD session /*,uin*/;
    UWORD /*cmd, seq,*/ seq2;

    num_pack = PacketRead1 (pak);

    for (i = 0; i < num_pack; i++)
    {
        llen = PacketRead2 (pak);
        if (pak->rpos + llen > pak->len)
        {
            if (prG->verbose & DEB_PROTOCOL)
                M_print (i18n (1868, "Got a malformed (to long subpacket) multi-packet - remainder ignored.\n"));
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
            M_printf ("%s " COLINDENT COLSERVER "", s_now);
            M_print  (i18n (1823, "Incoming partial packet:"));
            M_printf (" %04x %08x:%04x%04x %04x (%s)" COLNONE "\n",
                     ver, session, seq2, seq, cmd, CmdPktSrvName (cmd));
#if ICQ_VER == 5
            M_print  (PacketDump (pak, "gv5sp"));
#else
            M_print  (s_dump (pak->data, llen));
#endif
            M_print (COLEXDENT "\n");
        }

        CmdPktSrvProcess (conn, npak, cmd, ver, seq << 16 | seq2, uin);
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
        M_printf ("%s %s %d\n", i18n (1047, "Extra Data"), i18n (1046, "Length"), pak->len - pak->rpos);
    }
    
    ccmd = PacketReadAt2 (event->pak, CMD_v5_OFF_CMD);

    Debug (DEB_QUEUE, "···· ack type %04x (%s) seq %04x",
                      ccmd, CmdPktCmdName (ccmd), event->seq >> 16);

    if (ccmd == CMD_SEND_MESSAGE)
    {
        char *tmp;
        M_printf ("%s " COLACK "%*s" COLNONE " %s" COLSINGLE "%s\n",
                  s_now, uiG.nick_len + s_delta (ContactFindName (PacketReadAt4 (event->pak, CMD_v5_OFF_PARAM))),
                  ContactFindName (PacketReadAt4 (event->pak, CMD_v5_OFF_PARAM)),
                  MSGACKSTR, c_in (tmp = PacketReadAtLNTS (event->pak, 30)));
        free (tmp);
    }
    
    PacketD (pak);
    s_free (event->info);
    free (event);
}
