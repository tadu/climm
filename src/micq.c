/* $Id$ */
/* Copyright? */

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
#include "cmd_pkt_v8.h"
#include "cmd_pkt_cmd_v5_util.h"

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
#include <sys/stat.h>
#include <sys/wait.h>
#endif

#ifdef __BEOS__
#include "beos.h"
#endif

user_interface_state uiG;
Session *ssG;
Preferences *prG;
PreferencesSession *psG;

/*** auto away values ***/
static int idle_val = 0;
static int idle_flag = 0;

struct Queue *queue = NULL;

void init_global_defaults () {
  /* Initialize User Interface global state */
  uiG.start_time = time (NULL);
  uiG.last_rcvd_uin = 0;
  uiG.quit = FALSE;
  uiG.last_message_sent = NULL;
  uiG.last_sent_uin  = 0;
  uiG.reconnect_count = 0;
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

    if (prG->away_time == 0 || !(sess->connect & CONNECT_OK))
        return;
    if (!idle_val)
    {
        idle_val = time (NULL);
        return;
    }
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
        idle_val = time (NULL);
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
        idle_val = time (NULL);
    }
    return;
}

void Usage ()
{
    M_print (i18n (607, "Usage: micq [-v|-V] [-f|-F <rc-file>] [-l|-L <logfile>] [-?|-h]\n"));
    M_print (i18n (608, "        -v   Turn on verbose Mode (useful for Debugging only)\n"));
    M_print (i18n (609, "        -f   specifies an alternate Config File (default: ~/.micq/micqrc)\n"));
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
    int i, j, rc;
#ifdef _WIN32
    WSADATA wsaData;
#endif
    Session *sess;

    prG = PreferencesC ();
    psG = PreferencesSessionC ();
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

    { int argverb = 0;
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
                prG->verbose = (strlen (argv[i] + 2) ? j : prG->verbose + 1);
                if (!prG->verbose)
                    argverb = 1;
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
    
    if (argverb) prG->verbose = 0;
    }
    
    ssG = SessionNr (0);
    prG->sess = ssG;
    
    srand (time (NULL));

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
        perror (i18n (624, "Sorry, can't initialize Windows Sockets..."));
        exit (1);
    }
#endif

    for (i = 0; (sess = SessionNr (i)); i++)
        SessionInit (sess);

    R_init ();
    Prompt ();
    while (!uiG.quit)
    {
        if (ssG)
            Idle_Check (ssG);
#if _WIN32 || defined(__BEOS__)
        M_set_timeout (0, 100000);
#else
        M_set_timeout (2, 500000);
#endif

        M_select_init ();
        for (i = 0; (sess = SessionNr (i)); i++)
        {
            if (sess->sok < 0 || !sess->dispatch)
                continue;
            if (sess->connect & CONNECT_SELECT_R)
                M_Add_rsocket (sess->sok);
            if (sess->connect & CONNECT_SELECT_W)
                M_Add_wsocket (sess->sok);
            if (sess->connect & CONNECT_SELECT_X)
                M_Add_xsocket (sess->sok);
        }

#ifndef _WIN32
        M_Add_rsocket (STDIN);
#endif

        R_redraw ();

        rc = M_select ();
        assert (rc >= 0);

        R_undraw ();

        if (__os_has_input)
            if (R_process_input ())
                CmdUserInput (&idle_val, &idle_flag);

        for (i = 0; (sess = SessionNr (i)); i++)
        {
            if (sess->sok < 0 || !sess->dispatch || !M_Is_Set (sess->sok))
                continue;
            sess->dispatch (sess);
        }

        QueueRun (queue);
    }

#ifdef __BEOS__
    Be_Stop ();
#endif

    if (ssG->ver < 6)
        CmdPktCmdSendTextCode (ssG, "B_USER_DISCONNECTED");
    return 0;
}
