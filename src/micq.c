/* $Id$ */

#include "micq.h"
#include "util_ui.h"
#include "file_util.h"
#include "util.h"
#include "buildmark.h"
#include "sendmsg.h"
#include "network.h"
#include "cmd_user.h"
#include "cmd_pkt_server.h"
#include "icq_response.h"
#include "server.h"

#include "tcp.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#include <winsock2.h>
#else
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#ifndef __BEOS__
#include <arpa/inet.h>
#endif
#include <netdb.h>
#include <sys/wait.h>
#endif

#ifdef __BEOS__
#include "beos.h"
#endif


#define BACKLOG 10 /* james */

extern int h_error;

user_interface_state uiG;
session_state        ssG;
socks5_state         s5G;

/*** auto away values ***/
static int idle_val = 0;
static int idle_flag = 0;

struct msg_queue *queue = NULL;
#ifdef TCP_COMM
struct msg_queue *tcp_rq = NULL, *tcp_sq = NULL;
#endif

void init_global_defaults () {
  /* Initialize User Interface global state */
  uiG.Num_Contacts = 0;
  uiG.Contact_List = FALSE;
  uiG.Verbose      = FALSE;
  uiG.Sound = SOUND_ON;         /* Beeps on by default */
  uiG.SoundOnline = SOUND_OFF;  /* sound for ppl coming online */
  uiG.SoundOffline= SOUND_OFF;  /* sound for ppl going offline */
  uiG.Sound_Str[0]         = 0;
  uiG.Sound_Str_Online[0]  = 0;
  uiG.Sound_Str_Offline[0] = 0;
  uiG.Current_Status = STATUS_OFFLINE;
  uiG.LogLevel = 1;
  uiG.LogPlace = NULL;
  uiG.auto_resp = FALSE;
  uiG.auto_rep_str_na[0] = 0;
  uiG.auto_rep_str_away[0] = 0;
  uiG.auto_rep_str_occ[0] = 0;
  uiG.auto_rep_str_inv[0] = 0;
  uiG.auto_rep_str_dnd[0] = 0;
  uiG.last_uin_prompt = FALSE;
  uiG.line_break_type = 0;
  uiG.del_is_bs = TRUE;
  uiG.Max_Screen_Width  = 0;

  uiG.Russian     = FALSE;
  uiG.JapaneseEUC = FALSE;
  uiG.Color       = TRUE;
  uiG.Funny       = FALSE;
  uiG.Hermit      = FALSE;
  uiG.MicqStartTime = time (NULL);
#ifdef MSGEXEC
  uiG.receive_script[0] = 0;
#endif

  /* Initialize ICQ session global state    */
  ssG.seq_num = 1;  /* current sequence number */
  ssG.our_ip = 0x0100007f; /* Intel-ism??? why little-endian? */
  ssG.our_port = 0;
  ssG.our_outside_ip = 0x0100007f;
  ssG.Quit = FALSE;
  ssG.passwd[0] = 0;
  ssG.server[0] = 0;
  ssG.Done_Login = FALSE;
  ssG.real_packs_sent = 0;
  ssG.real_packs_recv = 0;
  ssG.Packets_Sent = 0;
  ssG.Packets_Recv = 0;
  ssG.last_message_sent = NULL;
  ssG.last_recv_uin  = 0;

  /* Initialize SOCKS5 global state         */
  s5G.s5Host[0] = 0;
  s5G.s5Name[0] = 0;
  s5G.s5Pass[0] = 0;
}


#ifdef TCP_COMM
/* TCP: init function james -- initializes TCP socket for incoming
 * connections Set port to 0 for random port
 */
SOK_T Init_TCP (int port, FD_T aux)
{
    int sok;
    int length;
    struct sockaddr_in sin;

    /* Create the socket */
    sok = socket (AF_INET, SOCK_STREAM, 0);
    if (sok == -1)
    {
        perror ("Error creating TCP socket.");
        exit (1);
    }
    else if (uiG.Verbose)
    {
        M_fdprint (aux, "Created TCP socket.\n");
    }

    /* Bind the socket to a port */
    sin.sin_family = AF_INET;
    sin.sin_port = htons (port);
    sin.sin_addr.s_addr = INADDR_ANY;   /* my ip */
   
    if (bind (sok, (struct sockaddr*)&sin, sizeof (struct sockaddr)) < 0)
    {
        perror ("Can't bind socket to free port.");
        return (-1);
    }

    /* Listen on socket */
    if (listen (sok, BACKLOG) < 0)
    {
        perror ("Unable to listen on TCP socket");
        return (-1);
    }

    /* Get the port used -- needs to be sent in login packet to ICQ server */
    length = sizeof (struct sockaddr);
    getsockname (sok, (struct sockaddr *) &sin, &length);
    ssG.our_port = ntohs (sin.sin_port);
    M_fdprint (aux, i18n (777, "The port that will be used for tcp (partially implemented) is %d\n"),
               ssG.our_port);

    return sok;
}
#endif

/* i18n (59, " ") i18n */

/*
 * Connects to hostname on port port
 * hostname can be FQDN or IP
 * write out messages to the FD aux
 */
SOK_T Connect_Remote (char *hostname, int port, FD_T aux)
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
    if (uiG.Verbose)
    {
        M_fdprint (aux, i18n (56, "Socket created attempting to connect\n"));
    }

    if (s5G.s5Use)
    {
        sin.sin_addr.s_addr = INADDR_ANY;
        sin.sin_family = AF_INET;
        sin.sin_port = 0;

        if (bind (sok, (struct sockaddr *) &sin, sizeof (struct sockaddr)) < 0)
        {
            M_fdprint (aux, "Can't bind socket to free port\n");
            return -1;
        }

        length = sizeof (sin);
        getsockname (sok, (struct sockaddr *) &sin, &length);
        s5OurPort = ntohs (sin.sin_port);

        s5sin.sin_addr.s_addr = inet_addr (s5G.s5Host);
        if (s5sin.sin_addr.s_addr == (unsigned long) -1)        /* name isn't n.n.n.n so must be DNS */
        {
            host_struct = gethostbyname (s5G.s5Host);
            if (!host_struct)
            {
#ifdef HAVE_HSTRERROR
                M_print (i18n (596, "[SOCKS] Can't find hostname %s: %s."), s5G.s5Host, hstrerror (h_errno));
#else
                M_print (i18n (57, "[SOCKS] Can't find hostname %s."), s5G.s5Host);
#endif
                M_print ("\n");
                return -1;
            }
            s5sin.sin_addr = *((struct in_addr *) host_struct->h_addr);
        }
        s5IP = ntohl (s5sin.sin_addr.s_addr);
        s5sin.sin_family = AF_INET;     /* we're using the inet not appletalk */
        s5sin.sin_port = htons (s5G.s5Port);        /* port */
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
        if (!strlen (s5G.s5Name) || !strlen (s5G.s5Pass) || !s5G.s5Auth)
            buf[2] = 0;         /* no authorization required */
        else
            buf[2] = 2;         /* method username/password */
        send (s5Sok, buf, 3, 0);
        res = recv (s5Sok, buf, 2, 0);
        if (strlen (s5G.s5Name) && strlen (s5G.s5Pass) && s5G.s5Auth)
        {
            if (res != 2 || buf[0] != 5 || buf[1] != 2) /* username/password authentication */
            {
                M_print (i18n (599, "[SOCKS] Authentication method incorrect\n"));
                close (s5Sok);
                return -1;
            }
            buf[0] = 1;         /* version of subnegotiation */
            buf[1] = strlen (s5G.s5Name);
            memcpy (&buf[2], s5G.s5Name, buf[1]);
            buf[2 + buf[1]] = strlen (s5G.s5Pass);
            memcpy (&buf[3 + buf[1]], s5G.s5Pass, buf[2 + buf[1]]);
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
            if (uiG.Verbose)
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
    sin.sin_family = AF_INET;   /* we're using the inet not appletalk */
    sin.sin_port = htons (port);

    if (s5G.s5Use)
    {
        s5G.s5DestIP = ntohl (sin.sin_addr.s_addr);
        memcpy (&sin.sin_addr.s_addr, &buf[4], 4);

        sin.sin_family = AF_INET;       /* we're using the inet not appletalk */
        s5G.s5DestPort = port;
        memcpy (&sin.sin_port, &buf[8], 2);
    }

    conct = connect (sok, (struct sockaddr *) &sin, sizeof (sin));

    if (conct == -1)            /* did we connect ? */
    {
        if (uiG.Verbose)
        {
            M_fdprint (aux, i18n (54, " Conection Refused on port %d at %s\n"), port, hostname);
            perror ("connect");
        }
        return -1;
    }

    length = sizeof (sin);
    getsockname (sok, (struct sockaddr *) &sin, &length);
    ssG.our_ip = ntohl (sin.sin_addr.s_addr);

    if (uiG.Verbose)
    {
        M_fdprint (aux, i18n (53, "Connected to %s, waiting for response\n"), hostname);
    }

    return sok;
}

/**********************************************
Verifies that we are in the correct endian
***********************************************/
void Check_Endian (void)
{
    int i;
    char passwd[10];

    passwd[0] = 1;
    passwd[1] = 0;
    passwd[2] = 0;
    passwd[3] = 0;
    passwd[4] = 0;
    passwd[5] = 0;
    passwd[6] = 0;
    passwd[7] = 0;
    passwd[8] = 0;
    passwd[9] = 0;
    i = *(UDWORD *) passwd;
    if (i == 1)
    {
        M_print (i18n (65, "Using intel byte ordering."));
    }
    else
    {
        M_print (i18n (66, "Using motorola byte ordering."));
    }
    M_print ("\n");
}

/******************************
Idle checking function
added by Warn Kitchen 1/23/99
******************************/
void Idle_Check (SOK_T sok)
{
    int tm;

    if (ssG.away_time == 0)
        return;
    tm = (time (NULL) - idle_val);
    if ((uiG.Current_Status == STATUS_AWAY || uiG.Current_Status == STATUS_NA)
        && tm < ssG.away_time && idle_flag == 1)
    {
        icq_change_status (sok, STATUS_ONLINE);
        R_undraw ();
        Time_Stamp ();
        M_print (" %s ", i18n (64, "Auto-Changed status to"));
        Print_Status (uiG.Current_Status);
        M_print ("\n");
        R_redraw ();
        idle_flag = 0;
        return;
    }
    if ((uiG.Current_Status == STATUS_AWAY) && (tm >= (ssG.away_time * 2)) && (idle_flag == 1))
    {
        icq_change_status (sok, STATUS_NA);
        R_undraw ();
        Time_Stamp ();
        M_print (" %s ", i18n (64, "Auto-Changed status to"));
        Print_Status (uiG.Current_Status);
        M_print ("\n");
        R_redraw ();
        return;
    }
    if (uiG.Current_Status != STATUS_ONLINE && uiG.Current_Status != STATUS_FREE_CHAT)
    {
        return;
    }
    if (tm >= ssG.away_time)
    {
        icq_change_status (sok, STATUS_AWAY);
        R_undraw ();
        Time_Stamp ();
        M_print (" %s ", i18n (64, "Auto-Changed status to"));
        Print_Status (uiG.Current_Status);
        M_print ("\n");
        R_redraw ();
        idle_flag = 1;
    }
    return;
}

void Usage ()
{
    M_print (i18n (607, "Usage: micq [-v|-V] [-f|-F <rc-file>] [-l|-L <logfile>] [-?|-h]\n"));
    M_print (i18n (608, "        -v   Turn on verbose Mode (useful for Debugging only)\n"));
    M_print (i18n (609, "        -f   specifies an alternate Config File (default: ~/.micqrc)\n"));
    M_print (i18n (610, "        -l   specifies an alternate logfile resp. logdir\n"));
    M_print (i18n (611, "        -?   gives this help screen\n\n"));
    exit (0);
}

/******************************
Main function connects gets UIN
and passwd and logins in and sits
in a loop waiting for server responses.
******************************/
int main (int argc, char *argv[])
{
    int sok;
#ifdef TCP_COMM
    int tcpSok;
#endif
    int i, j;
    int next;
    int time_delay = 120;
#ifdef _WIN32
    WSADATA wsaData;
#endif

    init_global_defaults ();

    i = i18nOpen ("!");

    setbuf (stdout, NULL);      /* Don't buffer stdout */
    M_print (BuildVersion ());

    M_print (i18n (612, "This program was made without any help from Mirabilis or their consent.\n"));
    M_print (i18n (613, "No reverse engineering or decompilation of any Mirabilis code took place to make this program.\n"));

    if (i == -1)
        M_print ("Couldn't load internationalization.\n");
    else if (i)
        M_print (i18n (81, "Successfully loaded en translation (%d entries).\n"), i);
    else
        M_print ("No internationalization requested.\n");

    /* and now we save the time that Micq was started, so that uptime will be
       able to appear semi intelligent.                                                                          */
    uiG.MicqStartTime = time (NULL);
    /* end of aaron */

    Set_rcfile (NULL);
    if (argc > 1)
    {
        for (i = 1; i < argc; i++)
        {
            if (argv[i][0] != '-')
            {
                ;
            }
            else if ((argv[i][1] == 'v') || (argv[i][1] == 'V'))
            {
                j = atol (argv[i] + 2);
                uiG.Verbose = (j ? j : uiG.Verbose + 1);
            }
            else if ((argv[i][1] == 'f') || (argv[i][1] == 'F'))
            {
                i++;            /* skip the argument to f */
                Set_rcfile (argv[i]);
                M_print (i18n (614, "Using config file \"%s\"\n"), argv[i]);
            }
            else if ((argv[i][1] == 'l') || (argv[i][1] == 'L'))
            {
                i++;
                M_print (i18n (615, "Logging to \"%s\"\n"), argv[i]);
                uiG.LogPlace = argv[i];
            }
            else if ((argv[i][1] == '?') || (argv[i][1] == 'h'))
            {
                Usage ();
                /* not reached */
            }
        }
    }

    Get_Config_Info ();
    srand (time (NULL));
    if (!strcmp (ssG.passwd, ""))
    {
        M_print ("%s ", i18n (63, "Enter password:"));
        Echo_Off ();
        M_fdnreadln (STDIN, ssG.passwd, sizeof (ssG.passwd));
        Echo_On ();
    }
    memset (ssG.serv_mess, FALSE, 1024);

#ifdef __BEOS__
    Be_Start ();
    M_print (i18n (616, "Started BeOS InputThread\n\r"));
#endif

    Initialize_Msg_Queue ();
    Check_Endian ();


#ifdef _WIN32
    i = WSAStartup (0x0101, &wsaData);
    if (i != 0)
    {
/* FUNNY: "Windows Sockets broken blame Bill -" */
        perror (i18n (718, "Sorry, can't initialize Windows Sockets..."));
        exit (1);
    }
#endif

/*** TCP: tcp init ***/
#ifdef TCP_COMM
    tcpSok = Init_TCP (TCP_PORT, STDERR);
#endif

    sok = Connect_Remote (ssG.server, ssG.remote_port, STDERR);

#ifdef __BEOS__
    if (sok == -1)
#else
    if ((sok == -1) || (sok == 0))
#endif
    {
        M_print (i18n (52, "Couldn't establish connection\n"));
        exit (1);
    }
    Login (sok, ssG.UIN, &ssG.passwd[0], ssG.our_ip, ssG.our_port, ssG.set_status);
    next = time (NULL);
    idle_val = time (NULL);
    next += 120;
    ssG.next_resend = 10;
    R_init ();
    Prompt ();
    while (!ssG.Quit)
    {
        Idle_Check (sok);
#if _WIN32 || defined(__BEOS__)
        M_set_timeout (0, 100000);
#else
        M_set_timeout (2, 500000);
#endif

        M_select_init ();
        M_Add_rsocket (sok);
#ifndef _WIN32
        M_Add_rsocket (STDIN);
#endif

/*** TCP: add tcp socket into select queue ***/
#ifdef TCP_COMM
        M_Add_rsocket (tcpSok);
        for (i = 0; i < uiG.Num_Contacts; i++)
        {
            if (uiG.Contacts[i].sok > 0)
            {
                M_Add_rsocket (uiG.Contacts[i].sok);
            }
        }
#endif


        M_select ();

        if (M_Is_Set (sok))
            CmdPktSrvRead (sok);
/*** TCP: on receiving a connection ***/
#ifdef TCP_COMM
        if (M_Is_Set (tcpSok))
            New_TCP (tcpSok);
        for (i = 0; i < uiG.Num_Contacts; i++)
        {
            if (uiG.Contacts[i].sok > 0)
            {
                if (M_Is_Set (uiG.Contacts[i].sok))
                {
                    /* Are we waiting for an init packet
                     * ... or is init already finished? */
                    if (uiG.Contacts[i].session_id > 0)
                    {
                        /* session id is non-zero only during init */
                        Recv_TCP_Init (uiG.Contacts[i].sok);
                    }
                    else
                    {
                        Handle_TCP_Comm (uiG.Contacts[i].uin);
                    }
                }
            }
        }
#endif


#if _WIN32
        if (_kbhit ())          /* sorry, this is a bit ugly...   [UH] */
#else
#ifdef __BEOS__
        if (Be_TextReady ())
#else
        if (M_Is_Set (STDIN))
#endif
#endif
        {
/*         idle_val = time( NULL );*/
            if (R_process_input ())
                CmdUserInput (sok, &idle_val, &idle_flag);
        }

        if (time (NULL) > next)
        {
            next = time (NULL) + time_delay;
            Keep_Alive (sok);
        }

        if (time (NULL) > ssG.next_resend)
        {
            Do_Resend (sok);
        }
/*** TCP: Resend stuff ***/
#ifdef TCP_COMM
        if (time (NULL) > ssG.next_tcp_resend)
        {
            Do_TCP_Resend (sok);
        }
#endif

#if HAVE_FORK
        while (waitpid (-1, NULL, WNOHANG) > 0);        /* clean up child processes */
#endif
    }

#ifdef __BEOS__
    Be_Stop ();
#endif

    Quit_ICQ (sok);
    return 0;
}
