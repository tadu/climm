
#include "micq.h"
#include "sendmsg.h"
#include "util.h"
#include "util_ui.h"
#include "network.h"
#include "cmd_user.h"
#include "cmd_pkt_server.h"
#include "icq_response.h"
#include "server.h"
#include "contact.h"
#include <string.h>
#include <stdio.h>

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

static int Reconnect_Count = 0;

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
void CmdPktSrvRead (SOK_T sok)
{
    srv_net_icq_pak pak;
    int s;

    s = SOCKREAD (sok, &pak.head.ver, sizeof (pak) - 2);
    if (s < 0)
        return;

    if (uiG.Verbose & 4)
    {
        Time_Stamp ();
        M_print (" \x1b«" COLSERV "");
        M_print (i18n (774, "Incoming packet:"));
        M_print (" %04x %08x:%08x %04x (%s)" COLNONE "\n",
                 Chars_2_Word (pak.head.ver), Chars_2_DW (pak.head.session),
                 Chars_2_DW (pak.head.seq), Chars_2_Word (pak.head.cmd),
                 CmdPktSrvName (Chars_2_Word (pak.head.cmd)));
#if ICQ_VER == 5
        Hex_Dump (pak.head.ver, 3);                       M_print ("\n");
        Hex_Dump (pak.head.ver + 3, 6);                   M_print ("\n");
        Hex_Dump (pak.head.ver + 9, 12);      if (s > 21) M_print ("\n");
        Hex_Dump (pak.head.ver + 21, s - 21);
#else
        Hex_Dump (pak.head.ver, s);
#endif
        M_print ("\x1b»\n");
    }
    if (Chars_2_DW (pak.head.session) != ssG.our_session)
    {
        if (uiG.Verbose)
        {
            M_print (i18n (606, "Got a bad session ID %08X with CMD %04X ignored.\n"),
                     Chars_2_DW (pak.head.session), Chars_2_Word (pak.head.cmd));
        }
        return;
    }
    if ((Chars_2_Word (pak.head.cmd) != SRV_NEW_UIN)
        && (Is_Repeat_Packet (Chars_2_Word (pak.head.seq))))
    {
        if (Chars_2_Word (pak.head.seq))
        {
            if (Chars_2_Word (pak.head.cmd) != SRV_ACK) /* ACKs don't matter */
            {
                if (uiG.Verbose)
                {
                    M_print (i18n (67, "\nIgnored a message cmd  %04x\n"),
                             Chars_2_Word (pak.head.cmd));
                }
                ack_srv (sok, Chars_2_DW (pak.head.seq));       /* LAGGGGG!! */
                return;
            }
        }
    }
    if (Chars_2_Word (pak.head.cmd) != SRV_ACK)
    {
        ssG.serv_mess[Chars_2_Word (pak.head.seq2)] = TRUE;
        Got_SEQ (Chars_2_DW (pak.head.seq));
        ack_srv (sok, Chars_2_DW (pak.head.seq));
        ssG.real_packs_recv++;
    }
    CmdPktSrvProcess (sok, pak.head.check + DATA_OFFSET, s - (sizeof (pak.head) - 2),
                     Chars_2_Word (pak.head.cmd), Chars_2_Word (pak.head.ver),
                     Chars_2_DW (pak.head.seq), Chars_2_DW (pak.head.UIN));
}

/*
 * Process the given server packet
 */
void CmdPktSrvProcess (SOK_T sok, UBYTE * data, int len, UWORD cmd,
                       UWORD ver, UDWORD seq, UDWORD uin)
{
    jump_srv_t *t;
    SIMPLE_MESSAGE_PTR s_mesg;
    static int loginmsg = 0;
    int i;
    
    for (t = jump; t->cmd; t++)
    {
        if (cmd == t->cmd && t->f)
        {
            t->f (sok, data, len, cmd, ver, seq, uin);
            return;
        }
    }

    switch (cmd)
    {
        case SRV_META_USER:
            Meta_User (sok, data, len, uin);
            break;
        case SRV_NEW_UIN:
            M_print (i18n (639, "The new UIN is %ld!\n"), uin);
            break;
        case SRV_UPDATE_FAIL:
            M_print (i18n (640, "Failed to update info.\n"));
            break;
        case SRV_UPDATE_SUCCESS:
            M_print (i18n (641, "User info successfully updated.\n"));
            break;
        case SRV_LOGIN_REPLY:
            Time_Stamp ();
            M_print (" " MAGENTA BOLD "%10lu" COLNONE " %s\n", uin, i18n (50, "Login successful!"));
            snd_login_1 (sok);
            snd_contact_list (sok);
            snd_invis_list (sok);
            snd_vis_list (sok);
            uiG.Current_Status = ssG.set_status;
            if (loginmsg++)
                break;

            Time_Stamp ();
            M_print (" " MAGENTA BOLD "%10s" COLNONE " %s: %u.%u.%u.%u\n", ContactFindName (uin), i18n (642, "IP"),
#if ICQ_VER == 0x0002
                     data[4], data[5], data[6], data[7]);
#elif ICQ_VER == 0x0004
                     data[0], data[1], data[2], data[3]);
#else
                     data[12], data[13], data[14], data[15]);
#endif
            break;
        case SRV_RECV_MESSAGE:
            Recv_Message (sok, data);
            break;
        case SRV_X1:
            if (uiG.Verbose)
            {
                M_print (i18n (643, "Acknowleged SRV_X1 0x021C Done Contact list?\n"));
            }
            CmdUser (sok, "¶e");
            ssG.Done_Login = TRUE;
            break;
        case SRV_X2:
            if (uiG.Verbose)
                M_print (i18n (644, "Acknowleged SRV_X2 0x00E6 Done old messages?\n"));
            snd_got_messages (sok);
            break;
        case SRV_INFO_REPLY:
            Display_Info_Reply (sok, data);
            M_print ("\n");
            break;
        case SRV_EXT_INFO_REPLY:
            Display_Ext_Info_Reply (sok, data);
            M_print ("\n");
            break;
        case SRV_USER_OFFLINE:
            User_Offline (sok, data);
            break;
        case SRV_BAD_PASS:
            M_print (i18n (645, COLMESS "You entered an incorrect password." COLNONE "\n"));
            exit (1);
            break;
        case SRV_TRY_AGAIN:
            M_print (i18n (646, COLMESS "Server is busy please try again.\nTrying again...\n"));
#if HAVE_FORK
            i = fork();
#else
            i = -1;
#endif
            ssG.our_session = rand();
            if (i < 0)
            {
                sleep (2);
                Login (sok, ssG.UIN, &ssG.passwd[0], ssG.our_ip, ssG.our_port, ssG.set_status);
            }
            else if (!i)
            {
                sleep (5);
                Login (sok, ssG.UIN, &ssG.passwd[0], ssG.our_ip, ssG.our_port, ssG.set_status);
                _exit (0);
            }
            break;
        case SRV_USER_ONLINE:
            User_Online (sok, data);
            break;
        case SRV_STATUS_UPDATE:
            Status_Update (sok, data);
            break;
        case SRV_GO_AWAY:
        case SRV_NOT_CONNECTED:
            Time_Stamp ();
            M_print (" ");
            Reconnect_Count++;
            if (Reconnect_Count >= MAX_RECONNECT_ATTEMPTS)
            {
                M_print ("%s\n", i18n (34, "Maximum number of tries reached. Giving up."));
                ssG.Quit = TRUE;
                break;
            }
            M_print ("%s\n", i18n (39, "Server has forced us to disconnect.  This may be because of network lag."));
            M_print (i18n (82, "Trying to reconnect... [try %d out of %d]"), Reconnect_Count, MAX_RECONNECT_ATTEMPTS);
#if HAVE_FORK
            i = fork();
#else
            i = -1;
#endif
            ssG.our_session = rand ();
            if (i < 0)
            {
                sleep (2);
                Login (sok, ssG.UIN, &ssG.passwd[0], ssG.our_ip, ssG.our_port, ssG.set_status);
            }
            else if (!i)
            {
                sleep (5);
                Login (sok, ssG.UIN, &ssG.passwd[0], ssG.our_ip, ssG.our_port, ssG.set_status);
                _exit (0);
            }
            M_print ("\n");
            break;
        case SRV_END_OF_SEARCH:
            M_print (i18n (45, "Search Done."));
            if (len >= 1)
            {
                M_print ("\t");
                if (data[0] == 1)
                {
                    M_print (i18n (44, "Too many users found."));
                }
                else
                {
                    M_print (i18n (43, "All users found."));
                }
            }
            M_print ("\n");
            break;
        case SRV_USER_FOUND:
            Display_Search_Reply (sok, data);
            M_print ("\n");
            break;
        case SRV_RAND_USER:
            Display_Rand_User (sok, data, len);
            M_print ("\n");
            break;
        case SRV_SYS_DELIVERED_MESS:
        /***  There are 2 places we get messages!! */
        /*** Don't edit here unless you're sure you know the difference */
        /*** Edit Do_Msg() in icq_response.c so you handle all messages */
            s_mesg = (SIMPLE_MESSAGE_PTR) data;
            if (!((NULL == ContactFind (Chars_2_DW (s_mesg->uin))) && (uiG.Hermit)))
            {
                uiG.last_recv_uin = Chars_2_DW (s_mesg->uin);
                Time_Stamp ();
                M_print ("\a " CYAN BOLD "%10s" COLNONE " ", ContactFindName (Chars_2_DW (s_mesg->uin)));
                if (uiG.Verbose)
                    M_print (i18n (647, " Type = %04x\t"), Chars_2_Word (s_mesg->type));
                Do_Msg (sok, Chars_2_Word (s_mesg->type), Chars_2_Word (s_mesg->len),
                        s_mesg->len + 2, uiG.last_recv_uin, 0);
                Auto_Reply (sok, s_mesg);
            }
            break;
        case SRV_AUTH_UPDATE:
            break;
        default:               /* commands we dont handle yet */
            Time_Stamp ();
            M_print (i18n (648, " " COLCLIENT "The response was %04X\t"), cmd);
            M_print (i18n (649, "The version was %X\t"), ver);
            M_print (i18n (650, "\nThe SEQ was %04X\t"), seq);
            M_print (i18n (651, "The size was %d\n"), len);
            if (uiG.Verbose)
            {
                if (len > 0)
                    Hex_Dump (data, len);
            }
            M_print (COLNONE "\n");
            break;
    }
}


/*
 * Splits multi packet into several packets
 */
JUMP_SRV_F (CmdPktSrvMulti)
{
    int num_pack, i;
    int llen;
    UBYTE *j;
    srv_net_icq_pak pak;
    num_pack = (unsigned char) data[0];
    j = data;
    j++;

    for (i = 0; i < num_pack; i++)
    {
        llen = Chars_2_Word (j);
        memcpy (&pak, j, sizeof (pak));
        j += 2;

        if (uiG.Verbose & 4)
        {
            Time_Stamp ();
            M_print (" \x1b«" COLSERV "");
            M_print (i18n (823, "Incoming partial packet:"));
            M_print (" %04x %08x:%08x %04x (%s)" COLNONE "\n",
                     Chars_2_Word (pak.head.ver), Chars_2_DW (pak.head.session),
                     Chars_2_DW (pak.head.seq), Chars_2_Word (pak.head.cmd),
                     CmdPktSrvName (Chars_2_Word (pak.head.cmd)));
#if ICQ_VER == 5
            Hex_Dump (pak.head.ver, 3);                          M_print ("\n");
            Hex_Dump (pak.head.ver + 3, 6);                      M_print ("\n");
            Hex_Dump (pak.head.ver + 9, 12);      if (llen > 21) M_print ("\n");
            Hex_Dump (pak.head.ver + 21, llen - 21);
#else
            Hex_Dump (pak.head.ver, llen);
#endif
            M_print ("\x1b»\n");
        }

        Kill_Prompt ();
        CmdPktSrvProcess (sok, pak.data, (llen + 2) - sizeof (pak.head),
                         Chars_2_Word (pak.head.cmd), Chars_2_Word (pak.head.ver),
                         Chars_2_DW (pak.head.seq), Chars_2_DW (pak.head.UIN));
        j += llen;
    }
}

/*
 * SRV_ACK - Remove acknowledged packet from queue.
 */
JUMP_SRV_F (CmdPktSrvAck)
{
    struct Event *event = QueueDequeue (queue, seq, QUEUE_TYPE_UDP_RESEND);
    UDWORD ccmd;

    if (!event)
    {
        if (uiG.Verbose)
        {
            /* complain */
        }
        return;
    }

    if (len && uiG.Verbose)
    {
        M_print ("%s %s %d\n", i18n (47, "Extra Data"), i18n (46, "Length"), len);
    }
    
    if (!event)
        return;

    ccmd = Chars_2_Word (&event->body[CMD_OFFSET]);

    if (uiG.Verbose & 32)
    {
        M_print (i18n (824, "Acknowledged packet type %04x (%s) sequence %04x removed from queue.\n"),
                 ccmd, CmdPktSrvName (ccmd), event->seq >> 16);
    }
    if (ccmd == CMD_SENDM)
    {
        Time_Stamp ();
        M_print (" " COLACK "%10s" COLNONE " %s%s\n",
                 ContactFindName (Chars_2_DW (&event->body[PAK_DATA_OFFSET])),
                 MSGACKSTR, MsgEllipsis (&event->body[32]));
    }
    if (event->info)
        free (event->info);
    free (event->body);
    free (event);
}
