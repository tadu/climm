/* $Id$ */
/* Copyright
 * This file may be distributed under version 2 of the GPL licence.
 */

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
#include "session.h"
#include "tcp.h"
#include "msg_queue.h"
#include "util_io.h"
#include "cmd_pkt_v8.h"
#include "cmd_pkt_v8_flap.h"
#include "cmd_pkt_v8_snac.h"
#include "cmd_pkt_cmd_v5_util.h"
#include "util_str.h"

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

user_interface_state uiG;
Preferences *prG;
PreferencesConnection *psG;

void init_global_defaults () {
  /* Initialize User Interface global state */
  uiG.start_time = time (NULL);
  uiG.last_rcvd_uin = 0;
  uiG.quit = FALSE;
  uiG.last_message_sent = NULL;
  uiG.away_time_prev = 0;
  uiG.last_sent_uin  = 0;
  uiG.reconnect_count = 0;
  uiG.idle_val = 0;
  uiG.idle_flag = 0;
  uiG.idle_msgs = 0;
  uiG.idle_uins = NULL;
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
        M_print (i18n (1065, "Using intel byte ordering."));
    }
    else
    {
        M_print (i18n (1066, "Using motorola byte ordering."));
    }
    M_print ("\n");
}

void Idle_Check (Connection *conn)
{
    int delta;
    UDWORD new = 0xffffffffL;

    if (~conn->type & TYPEF_ANY_SERVER)
        return;

    if ((conn->status & (STATUSF_DND | STATUSF_OCC | STATUSF_FFC))
        || !(conn->connect & CONNECT_OK))
    {
        uiG.idle_val = 0;
        return;
    }
    if (!prG->away_time && !uiG.idle_flag)
        return;
    if (!uiG.idle_val)
        uiG.idle_val = time (NULL);

    delta = (time (NULL) - uiG.idle_val);
    if (uiG.idle_flag)
    {
        if (conn->status & STATUSF_NA)
        {
            if (delta < prG->away_time || !prG->away_time)
            {
                new = (conn->status & STATUSF_INV) | STATUS_ONLINE;
                uiG.idle_flag = 0;
                uiG.idle_val = 0;
            }
        }
        else if (conn->status & STATUSF_AWAY)
        {
            if (delta >= 2 * prG->away_time)
                new = (conn->status & STATUSF_INV) | STATUS_NA;
            else if (delta < prG->away_time || !prG->away_time)
            {
                new = (conn->status & STATUSF_INV) | STATUS_ONLINE;
                uiG.idle_flag = 0;
                uiG.idle_val = 0;
            }
        }
    }
    else if (delta >= prG->away_time && !(conn->status & (STATUSF_AWAY | STATUSF_NA)))
    {
        new = (conn->status & STATUSF_INV) | STATUS_AWAY;
        uiG.idle_flag = 1;
        uiG.idle_msgs = 0;
    }
    if (new != 0xffffffffL && new != conn->status)
    {
        if (conn->type == TYPE_SERVER)
            SnacCliSetstatus (conn, new, 1);
        else
            CmdPktCmdStatusChange (conn, new);
        conn->status = new;
        M_printf ("%s %s %s\n", s_now, i18n (1064, "Auto-Changed status to"), s_status (new));
    }
    return;
}

void Usage ()
{
    M_print (i18n (1607, "Usage: micq [-v|-V] [-f|-F <rc-file>] [-l|-L <logfile>] [-?|-h]\n"));
    M_print (i18n (1608, "        -v   Turn on verbose Mode (useful for Debugging only)\n"));
    M_print (i18n (1609, "        -f   specifies an alternate Config File (default: ~/.micq/micqrc)\n"));
    M_print (i18n (1610, "        -l   specifies an alternate logfile resp. logdir\n"));
    M_print (i18n (1611, "        -?   gives this help screen\n\n"));
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
    Connection *conn;

    prG = PreferencesC ();
    psG = PreferencesConnectionC ();
    init_global_defaults ();

    i = i18nOpen ("!");

    setbuf (stdout, NULL);      /* Don't buffer stdout */

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
                prG->verbose = (argv[i][2] ? j : prG->verbose + 1);
                if (!prG->verbose)
                    argverb = 1;
            }
            else if ((argv[i][1] == 'f') || (argv[i][1] == 'F'))
            {
                if (argv[i][2])
                    prG->rcfile = &argv[i][2];
                else
                    prG->rcfile = argv[++i];
                M_printf (i18n (1614, "Using config file \"%s\"\n"), prG->rcfile);
            }
            else if ((argv[i][1] == 'l') || (argv[i][1] == 'L'))
            {
                if (argv[i][2])
                    prG->logplace = &argv[i][2];
                else
                    prG->logplace = argv[++i];
                M_printf (i18n (1615, "Logging to \"%s\"\n"), prG->logplace);
            }
            else if ((argv[i][1] == '?') || (argv[i][1] == 'h'))
            {
                Usage ();
                /* not reached */
            }
        }
    }

    PrefLoad (prG);
    if (argverb) prG->verbose = 0;
    }
    
    M_print (BuildVersion ());

    M_print (i18n (1612, "This program was made without any help from Mirabilis or their consent.\n"));
    M_print (i18n (1613, "No reverse engineering or decompilation of any Mirabilis code took place to make this program.\n"));

    if (i == -1)
        M_print ("Couldn't load internationalization.\n");
    else if (i)
        M_printf (i18n (1081, "Successfully loaded en translation (%d entries).\n"), i);
    else
        M_print ("No internationalization requested.\n");

    srand (time (NULL));

    Check_Endian ();

    if (!ConnectionNr (0))
        Initalize_RC_File ();

#ifdef _WIN32
    i = WSAStartup (0x0101, &wsaData);
    if (i != 0)
    {
/* FUNNY: "Windows Sockets broken blame Bill -" */
        perror (i18n (1624, "Sorry, can't initialize Windows Sockets..."));
        exit (1);
    }
#endif

    for (i = 0; (conn = ConnectionNr (i)); i++)
        if (conn->flags & CONN_AUTOLOGIN && conn->open)
            conn->open (conn);

    R_init ();
    R_resetprompt ();
    while (!uiG.quit)
    {

#if _WIN32 || defined(__BEOS__)
        M_set_timeout (0, 1000);
#else
        if (QueuePeek ())
        {
            i = QueuePeek ()->due - time (NULL);
            if (i <= 0)
                M_set_timeout (0, 100);
            else
                M_set_timeout (2, 500000);
        }
        else
            M_set_timeout (2, 500000);
#endif

        M_select_init ();
        for (i = 0; (conn = ConnectionNr (i)); i++)
        {
            if (conn->connect & CONNECT_OK && conn->type & TYPEF_ANY_SERVER)
                Idle_Check (conn);
            if (conn->sok < 0 || !conn->dispatch)
                continue;
            if (conn->connect & CONNECT_SELECT_R)
                M_Add_rsocket (conn->sok);
            if (conn->connect & CONNECT_SELECT_W)
                M_Add_wsocket (conn->sok);
            if (conn->connect & CONNECT_SELECT_X)
                M_Add_xsocket (conn->sok);
        }

#ifndef _WIN32
        M_Add_rsocket (STDIN_FILENO);
#endif

        R_redraw ();

        rc = M_select ();
        assert (rc >= 0);

        R_undraw ();

        if (__os_has_input)
            if (R_process_input ())
                CmdUserInput (&uiG.idle_val, &uiG.idle_flag);

        for (i = 0; (conn = ConnectionNr (i)); i++)
        {
            if (conn->sok < 0 || !conn->dispatch || !M_Is_Set (conn->sok))
                continue;
            conn->dispatch (conn);
        }

        QueueRun ();
    }

    for (i = 0; (conn = ConnectionNr (i)); i++)
    {
        if (conn->close)
            conn->close (conn);
    }
    QueueRun ();
    
    return 0;
}
