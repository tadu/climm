/* $Id$ */

#include "micq.h"
#include "util_ui.h"
#include "datatype.h"
#include "mreadline.h"
#include "msg_queue.h"
#include "server.h"
#include "sendmsg.h"
#include "util.h"
#include "icq_response.h"
#include "cmd_user.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <limits.h>

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#include <winsock2.h>
#else
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/socket.h>
#ifndef __BEOS__
#include <arpa/inet.h>
#endif
#include <netdb.h>
#include <sys/wait.h>
#include "mreadline.h"
#endif

static void Auto_Reply (SOK_T sok, SIMPLE_MESSAGE_PTR s_mesg);
static void Multi_Packet (SOK_T sok, UBYTE * data);

/*extern BOOL serv_mess[ 1024 ];  used so that we don't get duplicate messages with the same SEQ */
static int Reconnect_Count = 0;


/* TCP: auto response 
 * Returns the appropriate auto-reply string IF user has enabled auto-replys
 * dup's the string to prevent Russian conversion from happening on the
 * original */
char *Get_Auto_Reply ()
{
    char *nullmsg;
    nullmsg = malloc (1);
    *nullmsg = '\0';

    if (uiG.auto_resp == FALSE)
        return nullmsg;

    switch (uiG.Current_Status & 0x1FF)
    {
        case STATUS_OCCUPIED:
            return strdup (uiG.auto_rep_str_occ);
            break;

        case STATUS_AWAY:
            return strdup (uiG.auto_rep_str_away);
            break;

        case STATUS_DND:
            return strdup (uiG.auto_rep_str_dnd);
            break;

        case STATUS_NA:
            return strdup (uiG.auto_rep_str_na);
            break;

        case STATUS_INVISIBLE:
        case STATUS_ONLINE:
        case STATUS_FREE_CHAT:
        default:
            return nullmsg;
    }
}

static void Auto_Reply (SOK_T sok, SIMPLE_MESSAGE_PTR s_mesg)
{
    char *temp;

/* If TCP communication is enabled, don't send auto-reply msgs
   since the client can request one if they want to see it.   */
#ifndef TCP_COMM
/*** TCP: handle auto response ***/
    if (uiG.auto_resp)
    { 
#endif
        /*** !!!!???? What does this if do?  Why is it here ????!!!!! ****/
    if (0xfe != *(((unsigned char *) s_mesg) + sizeof (s_mesg)))
    {
        if (uiG.auto_resp && (uiG.Current_Status != STATUS_ONLINE) && (uiG.Current_Status != STATUS_FREE_CHAT))
        {
            switch (uiG.Current_Status & 0x1ff)
            {
                case STATUS_OCCUPIED:
                    /* Dup the string so the russian translation only happens once */
                    temp = strdup (uiG.auto_rep_str_occ);
                    break;
                case STATUS_AWAY:
                    temp = strdup (uiG.auto_rep_str_away);
                    break;
                case STATUS_DND:
                    temp = strdup (uiG.auto_rep_str_dnd);
                    break;
                case STATUS_INVISIBLE:
                    temp = strdup (uiG.auto_rep_str_inv);
                    break;
                case STATUS_NA:
                    temp = strdup (uiG.auto_rep_str_na);
                    break;
                default:
                    temp = strdup (uiG.auto_rep_str_occ);
                    M_print (i18n (635, "You have encounterd a bug in my code :( I now own you a beer!\nGreetings Fryslan!\n"));
            }

            icq_sendmsg (sok, Chars_2_DW (s_mesg->uin), temp, NORM_MESS);
            free (temp);
/*            M_print (i18n (636, "[ Sent auto-reply message to %d(%d)]\n"), Chars_2_DW (s_mesg->uin),
                     uiG.last_recv_uin);*/

            if (UIN2nick (uiG.last_recv_uin) != NULL)
                log_event (uiG.last_recv_uin, LOG_AUTO_MESS,
                           "Sending an auto-reply message to %s\n", UIN2nick (uiG.last_recv_uin));
            else
                log_event (uiG.last_recv_uin, LOG_AUTO_MESS,
                           "Sending an auto-reply message to %d\n", uiG.last_recv_uin);
        }
    }
#ifndef TCP_COMM
    }
#endif
}

static void Multi_Packet (SOK_T sok, UBYTE * data)
{
    int num_pack, i;
    int len;
    UBYTE *j;
    srv_net_icq_pak pak;
    num_pack = (unsigned char) data[0];
    j = data;
    j++;

    for (i = 0; i < num_pack; i++)
    {
        len = Chars_2_Word (j);
/*       pak = *( srv_net_icq_pak *) j ;*/
        memcpy (&pak, j, sizeof (pak));
        j += 2;
#if 0
        M_print (i18n (637, "\nPacket Number %d\n"), i);
        M_print ("%s %04X\n", i18n (46, "Length"), len);
        M_print (COMMAND_STR " %04X\n", Chars_2_Word (pak.head.cmd));
        M_print ("%s %04X\n", i18n (48, "SEQ"), Chars_2_Word (pak.head.seq));
        M_print ("%s %04X\n", i18n (49, "Ver"), Chars_2_Word (pak.head.ver));
#endif
        Kill_Prompt ();
        Server_Response (sok, pak.data, (len + 2) - sizeof (pak.head),
                         Chars_2_Word (pak.head.cmd), Chars_2_Word (pak.head.ver),
                         Chars_2_DW (pak.head.seq), Chars_2_DW (pak.head.UIN));
        j += len;
    }
}

void Server_Response (SOK_T sok, UBYTE * data, UDWORD len, UWORD cmd, UWORD ver, UDWORD seq, UDWORD uin)
{
    SIMPLE_MESSAGE_PTR s_mesg;
    static int loginmsg = 0;
    int i;

    switch (cmd)
    {
        case SRV_ACK:
            if (uiG.Verbose)
            {
                R_undraw ();
/*                M_print (i18n (51, "The server acknowledged the %04x command."),
                         ssG.last_cmd[seq >> 16]);
                M_print (i18n (638, "\nThe SEQ was %04X\n"), seq);*/
                if (len)
                {
                    M_print ("%s %s %d\n", i18n (47, "Extra Data"), i18n (46, "Length"), len);
                }
                R_redraw ();
            }
            Check_Queue (seq, queue);
            break;
        case SRV_META_USER:
            R_undraw ();
            Meta_User (sok, data, len, uin);
            R_redraw ();
            break;
        case SRV_MULTI_PACKET:
/*      printf( "\n" );
      Hex_Dump( data, len );*/
            Multi_Packet (sok, data);
            break;
        case SRV_NEW_UIN:
            R_undraw ();
            M_print (i18n (639, "The new UIN is %ld!\n"), uin);
            R_redraw ();
            break;
        case SRV_UPDATE_FAIL:
            R_undraw ();
            M_print (i18n (640, "Failed to update info.\n"));
            R_redraw ();
            break;
        case SRV_UPDATE_SUCCESS:
            R_undraw ();
            M_print (i18n (641, "User info successfully updated.\n"));
            R_redraw ();
            break;
        case SRV_LOGIN_REPLY:
/*      ssG.UIN = Chars_2_DW( &pak.data[0] ); */
            R_undraw ();
            Time_Stamp ();
            M_print (" " MAGENTA BOLD "%10lu" COLNONE " %s\n", uin, i18n (50, "Login successful!"));
            snd_login_1 (sok);
            snd_contact_list (sok);
            snd_invis_list (sok);
            snd_vis_list (sok);
            uiG.Current_Status = ssG.set_status;
            if (loginmsg++)
            {
                R_redraw ();
                break;
            }
            Time_Stamp ();
            M_print (" " MAGENTA BOLD "%10s" COLNONE " %s: %u.%u.%u.%u\n", UIN2Name (uin), i18n (642, "IP"),
#if ICQ_VER == 0x0002
                     data[4], data[5], data[6], data[7]);
#elif ICQ_VER == 0x0004
                     data[0], data[1], data[2], data[3]);
#else
                     data[12], data[13], data[14], data[15]);
#endif
            R_redraw ();
/*      icq_change_status( sok, ssG.set_status );*/
/*      Prompt();*/
            break;
        case SRV_RECV_MESSAGE:
            R_undraw ();
            Recv_Message (sok, data);
            R_redraw ();
            break;
        case SRV_X1:
            R_undraw ();
            if (uiG.Verbose)
            {
                M_print (i18n (643, "Acknowleged SRV_X1 0x021C Done Contact list?\n"));
            }
            CmdUser (sok, "¶e");
            R_redraw ();
            ssG.Done_Login = TRUE;
            break;
        case SRV_X2:
/*      Kill_Prompt();*/
            if (uiG.Verbose)
            {
                R_undraw ();
                M_print (i18n (644, "Acknowleged SRV_X2 0x00E6 Done old messages?\n"));
                R_redraw ();
            }
            snd_got_messages (sok);
            break;
        case SRV_INFO_REPLY:
            R_undraw ();
            Display_Info_Reply (sok, data);
            M_print ("\n");
            R_redraw ();
            break;
        case SRV_EXT_INFO_REPLY:
            R_undraw ();
            Display_Ext_Info_Reply (sok, data);
            M_print ("\n");
            R_redraw ();
            break;
        case SRV_USER_OFFLINE:
            R_undraw ();
            User_Offline (sok, data);
            R_redraw ();
            break;
        case SRV_BAD_PASS:
            R_undraw ();
            M_print (i18n (645, COLMESS "You entered an incorrect password." COLNONE "\n"));
            exit (1);
            break;
        case SRV_TRY_AGAIN:
            R_undraw ();
            M_print (i18n (646, COLMESS "Server is busy please try again.\nTrying again...\n"));
#if HAVE_FORK
            i = fork();
#else
            i = -1;
#endif
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
            R_redraw ();
            break;
        case SRV_USER_ONLINE:
            User_Online (sok, data);
            break;
        case SRV_STATUS_UPDATE:
            R_undraw ();
            Status_Update (sok, data);
            /*M_print( "\n" ); */
            R_redraw ();
            break;
        case SRV_GO_AWAY:
        case SRV_NOT_CONNECTED:
            R_undraw ();
            Time_Stamp ();
            M_print (" ");
            Reconnect_Count++;
            if (Reconnect_Count >= MAX_RECONNECT_ATTEMPTS)
            {
                M_print ("%s\n", i18n (34, "Maximum number of tries reached. Giving up."));
                ssG.Quit = TRUE;
                R_redraw ();
                break;
            }
            M_print ("%s\n", i18n (39, "Server has forced us to disconnect.  This may be because of network lag."));
            M_print (i18n (82, "Trying to reconnect... [try %d out of %d]"), Reconnect_Count, MAX_RECONNECT_ATTEMPTS);
#if HAVE_FORK
            i = fork();
#else
            i = -1;
#endif
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
            R_undraw ();
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
            R_redraw ();
            break;
        case SRV_USER_FOUND:
            R_undraw ();
            Display_Search_Reply (sok, data);
            M_print ("\n");
            R_redraw ();
            break;
        case SRV_RAND_USER:
            R_undraw ();
            Display_Rand_User (sok, data, len);
            M_print ("\n");
            R_redraw ();
            break;
        case SRV_SYS_DELIVERED_MESS:
        /***  There are 2 places we get messages!! */
        /*** Don't edit here unless you're sure you know the difference */
        /*** Edit Do_Msg() in icq_response.c so you handle all messages */
            R_undraw ();
            s_mesg = (SIMPLE_MESSAGE_PTR) data;
            if (!((NULL == UIN2Contact (Chars_2_DW (s_mesg->uin))) && (uiG.Hermit)))
            {
                uiG.last_recv_uin = Chars_2_DW (s_mesg->uin);
                Time_Stamp ();
                M_print ("\a " CYAN BOLD "%10s" COLNONE " ", UIN2Name (Chars_2_DW (s_mesg->uin)));
                if (uiG.Verbose)
                    M_print (i18n (647, " Type = %04x\t"), Chars_2_Word (s_mesg->type));
                Do_Msg (sok, Chars_2_Word (s_mesg->type), Chars_2_Word (s_mesg->len),
                        s_mesg->len + 2, uiG.last_recv_uin);
                Auto_Reply (sok, s_mesg);
            }
            R_redraw ();
            break;
        case SRV_AUTH_UPDATE:
            break;
        default:               /* commands we dont handle yet */
            R_undraw ();
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
            R_redraw ();
            break;
    }
}

/* i18n (32, " ") i18n (33, " ") i18n */