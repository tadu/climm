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

#define MICQ_ICON_1 "   " GREEN "_" SGR0 "     "
#define MICQ_ICON_2 " " GREEN "_/ \\_" SGR0 "   "
#define MICQ_ICON_3 GREEN "/ \\ / \\" SGR0 "  "
#define MICQ_ICON_4 RED ">--" YELLOW "o" GREEN "--<" SGR0 "  "
#define MICQ_ICON_5 RED "\\_" RED "/" BLUE " \\" GREEN "_/" SGR0 "  "
#define MICQ_ICON_6 "  " BLUE "\\" UL "m" BLUE BOLD "/" BLUE "CQ" SGR0 "  "
#define MICQ_ICON_7 "         "

user_interface_state uiG = { 0 };
Preferences *prG;

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
        M_printf ("%s ", s_now);
        M_printf (i18n (1064, "Automatically changed status to %s.\n"), s_status (new));
    }
    return;
}

/******************************
Main function connects gets UIN
and passwd and logins in and sits
in a loop waiting for server responses.
******************************/
int main (int argc, char *argv[])
{
    int i, rc;
#ifdef _WIN32
    WSADATA wsaData;
#endif
    Connection *conn;
    
    const char *arg_v, *arg_f, *arg_l, *arg_i, *arg_b;
    UDWORD arg_h = 0, arg_vv = 0, arg_c = 0;

    srand (time (NULL));
    uiG.start_time = time (NULL);
    setbuf (stdout, NULL);
    tzset ();

    arg_v = arg_f = arg_l = arg_i = arg_b = NULL;
    for (i = 1; i < argc; i++)
    {
        if (argv[i][0] != '-')
        {
            ;
        }
        else if ((argv[i][1] == 'v') || !strcmp (argv[i], "--verbose"))
        {
            arg_v = argv[i] + 2;
            arg_vv = (*arg_v ? atol (arg_v) : prG->verbose + 1);
        }
        else if ((argv[i][1] == 'b') || !strcmp (argv[i], "--basedir"))
        {
            if (argv[i][2])
                arg_b = &argv[i][2];
            else
                arg_b = argv[++i];
        }
        else if ((argv[i][1] == 'f') || !strcmp (argv[i], "--config"))
        {
            if (argv[i][2])
                arg_f = &argv[i][2];
            else
                arg_f = argv[++i];
        }
        else if ((argv[i][1] == 'l') || !strcmp (argv[i], "--logplace"))
        {
            if (argv[i][2])
                arg_l = &argv[i][2];
            else
                arg_l = argv[++i];
        }
        else if ((argv[i][1] == 'i') || !strcmp (argv[i], "--i18n") || !strcmp (argv[i], "--locale"))
        {
            if (argv[i][2])
                arg_i = &argv[i][2];
            else
                arg_i = argv[++i];
        }
        else if ((argv[i][1] == '?') || (argv[i][1] == 'h') ||
                 !strcmp (argv[i], "--help") || !strcmp (argv[i], "--version"))
            arg_h++;
        else if ((argv[i][1] == 'c') || !strcmp (argv[i], "--nocolor"))
            arg_c++;
            
    }

    prG = PreferencesC ();
    prG->verbose  = arg_vv;
    prG->rcfile   = arg_f ? strdup (arg_f) : NULL;
    prG->logplace = arg_l ? strdup (arg_l) : NULL;
    prG->flags |= arg_c ? 0 : FLAG_COLOR;
    
    prG->enc_loc = ENC_AUTO;
    i18nInit (&prG->locale, &prG->enc_loc, arg_i);
    
    rc = arg_h ? 0 : PrefLoad (prG);

    i = i18nOpen (prG->locale, prG->enc_loc);
    
    if (prG->enc_loc == ENC_AUTO)
        prG->enc_loc = ENC_AUTO | ENC_LATIN1;
    
    if (prG->enc_rem == ENC_AUTO)
    {
        switch (prG->enc_loc & ~ENC_AUTO)
        {
            case ENC_EUC:     prG->enc_rem = ENC_SJIS;    break;
            case ENC_SJIS:    prG->enc_rem = ENC_SJIS;    break;
            case ENC_KOI8:    prG->enc_rem = ENC_WIN1251; break;
            case ENC_WIN1251: prG->enc_rem = ENC_WIN1251; break;
            default:          prG->enc_rem = ENC_LATIN1;
        }
    }
    
    if (arg_v)
        prG->verbose = arg_vv;
    
    M_logo (MICQ_ICON_1 "\n" MICQ_ICON_2);
    M_logo (MICQ_ICON_3);
    M_logo (MICQ_ICON_4);
    M_logo (MICQ_ICON_5);
    M_logo (MICQ_ICON_6);
    M_logo (MICQ_ICON_7);

    M_print (BuildVersion ());
    M_print (BuildAttribution ());
    M_print (i18n (1612, "This program was made without any help from Mirabilis or their consent.\n"));
    M_print (i18n (1613, "No reverse engineering or decompilation of any Mirabilis code took place to make this program.\n"));

    M_logo_clear ();

    if (arg_h)
    {
        M_print  (i18n (1607, "Usage: micq [-h] [-v[level]] [-f <rc-file>] [-l <logfile>]\n"));
        M_print  (i18n (2199, "  -h, --help     gives this help text\n"));
        M_print  (i18n (2200, "  -v, --verbose  set (or increase) verbosity (mostly for debugging)\n"));
        M_printf (i18n (2201, "  -b, --basedir  use given BASE dir (default: %s)\n"), "~/.micq/");
        M_printf (i18n (2202, "  -f, --config   use given configuration file (default: %s)\n"), "BASE micqrc");
        M_printf (i18n (2203, "  -l, --logplace use given log file/dir (default: %s)\n"), "BASE history/");
        M_print  (i18n (2204, "  -i, --i1" "8n     use given locale (default: auto-detected)\n"));
        M_print  (i18n (2205, "  -c, --nocolor  disable colors\n"));
        exit (0);
    }

    if (!rc && arg_l)
    {
        M_printf (i18n (1864, "Can't open rcfile %s."), prG->rcfile);
        exit (20);
    }

    if (i == -1)
        M_print ("Couldn't load internationalization. Maybe you want to do some translation work?\n");
    else if (i)
        M_printf (i18n (1081, "Successfully loaded en translation (%d entries).\n"), i);
    else
        M_print ("No internationalization requested.\n");

    if (ENC(enc_loc) == ENC_UTF8)
#ifdef ENABLE_UTF8
        M_print (i18n (2209, "Detected UTF-8 encoding.\n"));
#else
        M_print (i18n (2208, "Detected UTF-8 encoding, however, this mICQ was compiled without UTF-8 support.\n"));
#endif

    if (!rc)
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

    R_init ();

    for (i = 0; (conn = ConnectionNr (i)); i++)
        if (conn->flags & CONN_AUTOLOGIN && conn->open)
            conn->open (conn);

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
