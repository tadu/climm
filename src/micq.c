/* $Id$ */

#include "micq.h"
#include "util_ui.h"
#include "file_util.h"
#include "util.h"
#include "buildmark.h"
#include "cmd_pkt_cmd_v5.h"
#include "network.h"
#include "cmd_user.h"
#include "cmd_pkt_server.h"
#include "icq_response.h"
#include "preferences.h"
#include "server.h"
#include "contact.h"
#include "tcp.h"
#include "msg_queue.h"
#include "util_io.h"

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

extern int h_error;

user_interface_state uiG;
Session ssG;
Preferences *prG;

/*** auto away values ***/
static int idle_val = 0;
static int idle_flag = 0;

struct Queue *queue = NULL;

void init_global_defaults () {
  /* Initialize User Interface global state */
  ssG.status = STATUS_OFFLINE;
  uiG.start_time = time (NULL);
  uiG.last_rcvd_uin = 0;
  uiG.quit = FALSE;
  uiG.last_message_sent = NULL;
  uiG.last_sent_uin  = 0;
  uiG.reconnect_count = 0;

  prG->verbose = -2;

  /* Initialize ICQ session global state    */
  ssG.our_seq2 = 0;  /* current sequence number */
  ssG.our_local_ip = 0x0100007f; /* Intel-ism??? why little-endian? */
  ssG.our_port = 0;
  ssG.our_outside_ip = 0x0100007f;
  ssG.passwd = NULL;
  ssG.server = NULL;
  ssG.connect = 0;
  ssG.stat_pak_rcvd = 0;
  ssG.stat_pak_sent = 0;
  ssG.stat_real_pak_sent = 0;
  ssG.stat_real_pak_rcvd = 0;
  ssG.sok = -1;
}

/**********************************************
Verifies that we are in the correct endian
***********************************************/
void Check_Endian (void)
{
    int i;
    char check[10];

    check[0] = 1;
    check[1] = 0;
    check[2] = 0;
    check[3] = 0;
    check[4] = 0;
    check[5] = 0;
    check[6] = 0;
    check[7] = 0;
    check[8] = 0;
    check[9] = 0;
    i = *(UDWORD *) check;
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
void Idle_Check (Session *sess)
{
    int tm;

    if (prG->away_time == 0)
        return;
    tm = (time (NULL) - idle_val);
    if ((sess->status == STATUS_AWAY || sess->status == STATUS_NA)
        && tm < prG->away_time && idle_flag == 1)
    {
        CmdPktCmdStatusChange (sess, STATUS_ONLINE);
        Time_Stamp ();
        M_print (" %s ", i18n (64, "Auto-Changed status to"));
        Print_Status (sess->status);
        M_print ("\n");
        idle_flag = 0;
        return;
    }
    if ((sess->status == STATUS_AWAY) && (tm >= (prG->away_time * 2)) && (idle_flag == 1))
    {
        CmdPktCmdStatusChange (sess, STATUS_NA);
        Time_Stamp ();
        M_print (" %s ", i18n (64, "Auto-Changed status to"));
        Print_Status (sess->status);
        M_print ("\n");
        return;
    }
    if (sess->status != STATUS_ONLINE && sess->status != STATUS_FREE_CHAT)
    {
        return;
    }
    if (tm >= prG->away_time)
    {
        CmdPktCmdStatusChange (sess, STATUS_AWAY);
        Time_Stamp ();
        M_print (" %s ", i18n (64, "Auto-Changed status to"));
        Print_Status (sess->status);
        M_print ("\n");
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

void CallBackLoginUDP (struct Event *event)
{
    Session *sess = event->sess;

    free (event);
    if (sess->sok < 0)
    {
        sess->sok = UtilIOConnectUDP (sess->server, sess->server_port, STDERR);

#ifdef __BEOS__
        if (sess->sok == -1)
#else
        if (sess->sok == -1 || sess->sok == 0)
#endif
        {
            M_print (i18n (52, "Couldn't establish connection for this session.\n"));
            sess->sok = -1;
            return;
        }
    }
    sess->our_seq2    = 0;
    CmdPktCmdLogin (sess);
}

/******************************
Main function connects gets UIN
and passwd and logins in and sits
in a loop waiting for server responses.
******************************/
int main (int argc, char *argv[])
{
    int i, j, rc;
    int next;
    int time_delay = 120;
#ifdef _WIN32
    WSADATA wsaData;
#endif

    prG = PreferencesC ();
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

    prG->sess = &ssG;
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
                prG->verbose = (j ? j : prG->verbose + 1);
            }
            else if ((argv[i][1] == 'f') || (argv[i][1] == 'F'))
            {
                i++;            /* skip the argument to f */
                prG->rcfile = argv[i];
                M_print (i18n (614, "Using config file \"%s\"\n"), argv[i]);
            }
            else if ((argv[i][1] == 'l') || (argv[i][1] == 'L'))
            {
                i++;
                M_print (i18n (615, "Logging to \"%s\"\n"), argv[i]);
                prG->logplace = argv[i];
            }
            else if ((argv[i][1] == '?') || (argv[i][1] == 'h'))
            {
                Usage ();
                /* not reached */
            }
        }
    }

    QueueInit (&queue);
    PrefLoad (prG);
    
    srand (time (NULL));
    if (!strcmp (ssG.passwd, ""))
    {
        char pwd[20];
        pwd[0] = '\0';
        M_print ("%s ", i18n (63, "Enter password:"));
        Echo_Off ();
        M_fdnreadln (STDIN, pwd, sizeof (pwd));
        Echo_On ();
        ssG.passwd = strdup (pwd);
    }

#ifdef __BEOS__
    Be_Start ();
    M_print (i18n (616, "Started BeOS InputThread\n\r"));
#endif

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

#ifdef TCP_COMM
    /* trigger TCP "login" (listening) */
    QueueEnqueueData (queue, &ssG, 0, 0, 0, time (NULL), NULL, NULL, &CallBackLoginTCP);
#endif

    /* trigger UDP login */
    QueueEnqueueData (queue, &ssG, 0, 0, 0, time (NULL), NULL, NULL, &CallBackLoginUDP);

#ifdef ICQv8
    QueueEnqueueData (queue, &ssG, 0, 0, 0, time (NULL), NULL, NULL, &CallBackLoginV8);
#endif

    next = time (NULL) + 120;
    idle_val = time (NULL);
    R_init ();
    Prompt ();
    while (!uiG.quit)
    {
        Idle_Check (&ssG);
#if _WIN32 || defined(__BEOS__)
        M_set_timeout (0, 100000);
#else
        M_set_timeout (2, 500000);
#endif

        M_select_init ();
        if (ssG.sok > 0)
            M_Add_rsocket (ssG.sok);
#ifndef _WIN32
        M_Add_rsocket (STDIN);
#endif

#ifdef TCP_COMM
        TCPAddSockets (&ssG);
#endif
        R_redraw ();

        rc = M_select ();
        assert (rc >= 0);

#if _WIN32
        if (_kbhit ())          /* sorry, this is a bit ugly...   [UH] */
#else
#ifdef __BEOS__
        if (Be_TextReady ())
#else
        if (M_Is_Set (STDIN))
#endif
#endif
            if (R_process_input ())
                CmdUserInput (&ssG, &idle_val, &idle_flag);

        R_undraw ();

        if (ssG.sok > 0 && M_Is_Set (ssG.sok))
            CmdPktSrvRead (&ssG);

#ifdef TCP_COMM
        TCPDispatch (&ssG);
#endif


        if (time (NULL) > next)
        {
            next = time (NULL) + time_delay;
            CmdPktCmdKeepAlive (&ssG);
        }

        QueueRun (queue);

#if HAVE_FORK
        while (waitpid (-1, NULL, WNOHANG) > 0);        /* clean up child processes */
#endif
    }

#ifdef __BEOS__
    Be_Stop ();
#endif

    CmdPktCmdSendTextCode (&ssG, "B_USER_DISCONNECTED");
    return 0;
}
