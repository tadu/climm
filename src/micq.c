/* $Id$ */
/* Copyright
 * This file may be distributed under version 2 of the GPL licence.
 */

#include "micq.h"
#include "util_ui.h"
#include "file_util.h"
#include "util.h"
#include "conv.h"
#include "buildmark.h"
#include "cmd_pkt_cmd_v5.h"
#include "network.h"
#include "cmd_user.h"
#include "cmd_pkt_server.h"
#include "icq_response.h"
#include "preferences.h"
#include "server.h"
#include "contact.h"
#include "contactopts.h"
#include "session.h"
#include "tcp.h"
#include "msg_queue.h"
#include "util_io.h"
#include "cmd_pkt_v8.h"
#include "cmd_pkt_v8_flap.h"
#include "cmd_pkt_v8_snac.h"
#include "cmd_pkt_cmd_v5_util.h"
#include "util_str.h"
#include "os.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#if HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
#if HAVE_CONIO_H
#include <conio.h>
#endif

#define MICQ_ICON_1 "   " GREEN "_" SGR0 "     "
#define MICQ_ICON_2 " " GREEN "_/ \\_" SGR0 "   "
#define MICQ_ICON_3 GREEN "/ \\ / \\" SGR0 "  "
#define MICQ_ICON_4 RED ">--" YELLOW "o" GREEN "--<" SGR0 "  "
#define MICQ_ICON_5 RED "\\_" RED "/" BLUE " \\" GREEN "_/" SGR0 "  "
#define MICQ_ICON_6 "  " BLUE "\\" UL "m" BLUE BOLD "/" BLUE "CQ" SGR0 "  "
#define MICQ_ICON_7 "         "

#define MICQ_ICON_NOCOLOR_1 "   _     "
#define MICQ_ICON_NOCOLOR_2 " _/ \\_   "
#define MICQ_ICON_NOCOLOR_3 "/ \\ / \\  "
#define MICQ_ICON_NOCOLOR_4 ">--o--<  "
#define MICQ_ICON_NOCOLOR_5 "\\_/ \\_/  "
#define MICQ_ICON_NOCOLOR_6 "  \\m/CQ  "
#define MICQ_ICON_NOCOLOR_7 "         "

user_interface_state uiG = { 0 };
Preferences *prG;

void Idle_Check (Connection *conn)
{
    int delta, saver = -1;
    time_t now;
    UDWORD new = 0xffffffffL;

    if (~conn->type & TYPEF_ANY_SERVER)
        return;

    if ((conn->status & (STATUSF_DND | STATUSF_OCC | STATUSF_FFC))
        || !(conn->connect & CONNECT_OK))
    {
        uiG.idle_val = 0;
        return;
    }
    
    now = time (NULL);
    delta = uiG.idle_val ? now - uiG.idle_val : 0;

    if (!prG->away_time && delta > 10 && uiG.idle_val)
    {
        saver = os_DetectLockedWorkstation();
        
        if (saver >= 0 && saver <= 3)
        {
            switch (saver)
            {
                case 0: /* no screen saver, not locked */
                    if (!(conn->status & (STATUSF_AWAY | STATUSF_NA)))
                        return;
                    new = (conn->status & STATUSF_INV) | STATUS_ONLINE;
                    break;
                case 2: /* locked workstation */
                case 3:
                    if (conn->status & STATUS_NA)
                        return;
                    new = (conn->status & STATUSF_INV) | STATUS_NA;
                    break;
                case 1: /* screen saver only */
                    if ((conn->status & (STATUSF_AWAY | STATUSF_NA)) == STATUS_AWAY)
                        return;
                    new = (conn->status & STATUSF_INV) | STATUS_AWAY;
                    break;
            }

            uiG.idle_val = 0;
            uiG.idle_flag = 1;
            delta = 0;
        }
    }

    if (!prG->away_time && !uiG.idle_flag)
        return;

    if (!uiG.idle_val)
        uiG.idle_val = now;

    if (uiG.idle_flag & 2)
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
    else if (!uiG.idle_flag && delta >= prG->away_time && !(conn->status & (STATUSF_AWAY | STATUSF_NA)))
    {
        new = (conn->status & STATUSF_INV) | STATUS_AWAY;
        uiG.idle_flag = 2;
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
    UDWORD i, rc;
#ifdef _WIN32
    WSADATA wsaData;
#endif
    Connection *conn;
    
    const char *arg_v, *arg_f, *arg_l, *arg_i, *arg_b;
    UDWORD arg_h = 0, arg_vv = 0, arg_c = 0;
    UWORD res;
    UBYTE save_conv_error;

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
            arg_vv = (*arg_v ? atol (arg_v) : arg_vv + 1);
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
        else if ((argv[i][1] == 'c') || !strcmp (argv[i], "--nocolor") || !strcmp (argv[i], "--nocolour"))
            arg_c++;
            
    }

    ConvInit ();
    save_conv_error = conv_error;

    prG = PreferencesC ();
    prG->verbose  = arg_vv | 0x8000;
    prG->rcfile   = arg_f ? strdup (arg_f) : NULL;
    prG->logplace = arg_l ? strdup (arg_l) : NULL;
    prG->flags |= arg_c ? 0 : FLAG_COLOR;
    if (arg_b && *arg_b != '/' && (*arg_b != '~' || arg_b[1] != '/'))
    {
#ifndef PATH_MAX
#define PATH_MAX 1023
#endif
        char buf[PATH_MAX + 2];
        const char *app, *oldbase;
        buf[0] = '\0';
        getcwd  (buf, PATH_MAX);
        strcat (buf, _OS_PATHSEPSTR);
        app = arg_b[strlen (arg_b) - 1] == _OS_PATHSEP ? "" : _OS_PATHSEPSTR;
        oldbase = arg_b[0] == '.' && arg_b[1] == _OS_PATHSEP ? arg_b += 2, buf : PrefUserDir (prG);
        prG->basedir = strdup (s_sprintf ("%s%s%s", oldbase, arg_b, app));
    }
    else
        prG->basedir = arg_b ? strdup (arg_b[strlen (arg_b) - 1] == _OS_PATHSEP
                               ? arg_b : s_sprintf ("%s" _OS_PATHSEPSTR, arg_b)) : NULL;
    
    prG->enc_loc = ENC_AUTO;
    i18nInit (&prG->locale, &prG->enc_loc, arg_i);
    prG->verbose &= ~0x8000;
    PreferencesInit (prG);
    
    rc = arg_h ? 0 : PrefLoad (prG);

    i = i18nOpen (prG->locale, prG->enc_loc);
    
    if (prG->enc_loc == ENC_AUTO)
        prG->enc_loc = ENC_AUTO | ENC_LATIN1;
    
    if (!ContactOptionsGetVal (&prG->copts, CO_ENCODING, &res))
    {
        switch (prG->enc_loc & ~ENC_AUTO)
        {
            case ENC_EUC:     ContactOptionsSetVal (&prG->copts, CO_ENCODING, ENC_SJIS); break;
            case ENC_SJIS:    ContactOptionsSetVal (&prG->copts, CO_ENCODING, ENC_SJIS); break;
            case ENC_KOI8:    ContactOptionsSetVal (&prG->copts, CO_ENCODING, ENC_WIN1251); break;
            case ENC_WIN1251: ContactOptionsSetVal (&prG->copts, CO_ENCODING, ENC_WIN1251); break;
            default:          ContactOptionsSetVal (&prG->copts, CO_ENCODING, ENC_LATIN1);
        }
    }
    
    if (arg_v)
        prG->verbose = arg_vv;
    if (arg_c)
        prG->flags &= ~FLAG_COLOR;
    
    if (prG->flags & FLAG_COLOR)
    {
        M_logo (MICQ_ICON_1 "\n" MICQ_ICON_2);
        M_logo (MICQ_ICON_3);
        M_logo (MICQ_ICON_4);
        M_logo (MICQ_ICON_5);
        M_logo (MICQ_ICON_6);
        M_logo (MICQ_ICON_7);
    }
    else
    {
        M_logo (MICQ_ICON_NOCOLOR_1 "\n" MICQ_ICON_NOCOLOR_2);
        M_logo (MICQ_ICON_NOCOLOR_3);
        M_logo (MICQ_ICON_NOCOLOR_4);
        M_logo (MICQ_ICON_NOCOLOR_5);
        M_logo (MICQ_ICON_NOCOLOR_6);
        M_logo (MICQ_ICON_NOCOLOR_7);
    }

    M_print (BuildVersion ());
    M_print (BuildAttribution ());
    M_print (i18n (1612, "This program was made without any help from Mirabilis or their consent.\n"));
    M_print (i18n (1613, "No reverse engineering or decompilation of any Mirabilis code took place to make this program.\n"));

    M_logo_clear ();

    if (arg_h)
    {
        M_print  (i18n (1607, "Usage: micq [-h] [-c] [-i <locale>] [-b <basedir>] \n            [-f <configfile>] [-v[<level>]] [-l <logplace>]\n"));
        M_print  (i18n (2199, "  -h, --help     gives this help text\n"));
        M_print  (i18n (2200, "  -v, --verbose  set (or increase) verbosity (mostly for debugging)\n"));
        M_printf (i18n (2201, "  -b, --basedir  use given BASE dir (default: %s)\n"), "$HOME" _OS_PATHSEPSTR ".micq" _OS_PATHSEPSTR);
        M_printf (i18n (2203, "  -l, --logplace use given log file/dir (default: %s)\n"), "BASE" _OS_PATHSEPSTR "history" _OS_PATHSEPSTR);
        M_print  (i18n (2204, "  -i, --i1" "8n     use given locale (default: auto-detected)\n"));
        M_print  (i18n (2205, "  -c, --nocolor  disable colors\n"));
        exit (0);
    }
    
    if (conv_error)
    {
        M_printf (COLERROR "%s" COLNONE " ", i18n (1619, "Warning:"));
        M_printf (i18n (9999, "Encoding %s%s%s is not supported by this mICQ.\n"), COLMESSAGE, ConvEncName (conv_error), COLNONE);
        if (save_conv_error)
        {
            M_print  (i18n (9999, "Please recompile using the '--enable-iconvrepl' configure option.\n"));
            if (conv_error <= prG->enc_loc)
            {
                M_print  ("Warning: ");
                M_printf ("Encoding %s is not supported by this mICQ.\n", ConvEncName (conv_error));
                M_print  ("Please recompile using the '--enable-iconvrepl' configure option.\n");
            }
        }
        conv_error = 0;
    }

    if (!rc && arg_l)
    {
        M_printf (i18n (9999, "Could not open configuration file %s%s%s."), COLMESSAGE, prG->rcfile, COLNONE);
        exit (20);
    }

    if (arg_f)
        M_printf (i18n (9999, "Deprecated option -f used. Please use the similar -b instead.\n"));

    if (i == -1)
        M_printf (i18n (9999, "Translation %s%s%s not found. Would you like to translate mICQ into your language?\n"), COLMESSAGE, prG->locale_full, COLNONE);
    else if (i)
        M_printf (i18n (9999, "English (%s) translation loaded (%s%ld%s entries).\n"), prG->locale_full, COLMESSAGE, i, COLNONE);
    else
        M_print ("No translation requested.\n");

    if (!rc)
        Initialize_RC_File ();

#ifdef _WIN32
    if ((rc = WSAStartup (0x0101, &wsaData)))
    {
/* FUNNY: "Windows Sockets broken blame Bill -" */
        perror (i18n (1624, "Sorry, can't initialize Windows Sockets..."));
        exit (1);
    }
#endif

    R_init ();

    for (i = 0; (conn = ConnectionNr (i)); i++)
        if ((conn->flags & CONN_AUTOLOGIN) && conn->open)
            conn->open (conn);

    while (!uiG.quit)
    {

#if INPUT_BY_POLL
        M_set_timeout (0, 10000);
#else
        if (QueuePeek ())
        {
            int i = QueuePeek ()->due - time (NULL);
            if (i <= 0)
                M_set_timeout (0, 100);
            else if (i <= 2)
                M_set_timeout (i, 0);
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

#if !INPUT_BY_POLL
        M_Add_rsocket (STDIN_FILENO);
#endif

        if (conv_error)
        {
            M_printf (COLERROR "%s" COLNONE " ", i18n (1619, "Warning:"));
            M_printf (i18n (9999, "Encoding %s%s%s is not supported by this mICQ.\n"), COLMESSAGE, ConvEncName (conv_error), COLNONE);
            conv_error = 0;
        }

        R_redraw ();

        rc = M_select ();
        assert (~rc & 0x80000000L);

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
        if (conn->close)
            conn->close (conn);
    QueueRun ();
    if (prG->flags & FLAG_AUTOSAVE)
    {
        int i, j;
        Contact *cont;

        for (i = 0; (conn = ConnectionNr (i)); i++)
            if (conn->contacts)
                for (j = 0; (cont = ContactIndex (conn->contacts, j)); j++)
                    ContactMetaSave (cont);
        Save_RC ();
    }
    
    return 0;
}
