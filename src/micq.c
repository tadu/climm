/* $Id$ */
/* Copyright
 * This file may be distributed under version 2 of the GPL licence.
 */


#include "micq.h"

#include <limits.h>
#include <fcntl.h>
#include <stdarg.h>
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

#include "util_ui.h"
#include "file_util.h"
#include "util.h"
#include "util_rl.h"
#include "conv.h"
#include "buildmark.h"
#include "cmd_user.h"
#include "icq_response.h"
#include "preferences.h"
#include "server.h"
#include "contact.h"
#include "connection.h"
#include "tcp.h"
#include "tabs.h"
#include "msg_queue.h"
#include "util_io.h"
#include "oscar_snac.h"
#include "oscar_service.h"
#include "oldicq_compat.h"
#include "oldicq_client.h"
#include "util_tcl.h"
#include "util_ssl.h"
#include "os.h"

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

static void Idle_Check (Connection *conn);

user_interface_state uiG = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
Preferences *prG;

static void Idle_Check (Connection *conn)
{
    int saver = -1;
    time_t now;
    UDWORD delta, new = 0xffffffffL;

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
        rl_printf ("%s ", s_now);
        rl_printf (i18n (1064, "Automatically changed status to %s.\n"), s_status (new));
    }
    return;
}

/*
 * Parse command line and initialize everything
 */
static void Init (int argc, char *argv[])
{
#ifdef _WIN32
    WSADATA wsaData;
#endif
    Connection *conn;
    Event *loginevent = NULL;
    const char *arg_v, *arg_f, *arg_l, *arg_i, *arg_b, *arg_s, *arg_u, *arg_p;
    UDWORD loaded, uingiven = 0, arg_h = 0, arg_vv = 0, arg_c = 0, arg_ss = 0;
    str_s arg_C = { NULL, 0, 0 };
    val_t res;
    UBYTE save_conv_error;
    char **targv;
    int i, j;

    srand (time (NULL));
    uiG.start_time = time (NULL);
    setbuf (stdout, NULL);
    tzset ();

    targv = calloc (argc + 2, sizeof (char *));
    arg_v = arg_f = arg_l = arg_i = arg_b = arg_s = arg_u = arg_p = NULL;
    for (i = 1, j = 0; i < argc; i++)
    {
        if (   !strcmp (argv[i], "-?") || !strcmp (argv[i], "-h")
            || !strcmp (argv[i], "--help") || !strcmp (argv[i], "--version"))
            arg_h++;
        else if (   !strcmp (argv[i], "-c") || !strcmp (argv[i], "--nocolor")
                 || !strcmp (argv[i], "--nocolour"))
            arg_c++;
        else if (!strcmp (argv[i], "-i") || !strcmp (argv[i], "--i18n") || !strcmp (argv[i], "--locale"))
            arg_i = argv[++i];
        else if (!strcmp (argv[i], "-l") || !strcmp (argv[i], "--logplace"))
            arg_l = argv[++i];
        else if (!strcmp (argv[i], "-b") || !strcmp (argv[i], "--basedir"))
            arg_b = argv[++i];
        else if (!strcmp (argv[i], "-f") || !strcmp (argv[i], "--config"))
            arg_f = argv[++i];
        else if (!strncmp (argv[i], "-v", 2) || !strcmp (argv[i], "--verbose"))
        {
            arg_v = argv[i] + ((argv[i][1] == '-') ? 9 : 2);
            if (*arg_v)
                arg_vv = atoll (arg_v) | 0x80000000UL;
            else if (argv[i + 1])
            {
                char *t;
                UDWORD n = strtol (argv[i + 1], &t, 0);
                if (*t)
                    arg_vv++;
                else
                    i++, arg_vv = n | 0x80000000UL;
            }
        }
        else if (!strcmp (argv[i], "-u") || !strcmp (argv[i], "--uin"))
        {
            if (argv[i + 1])
            {
                targv[j++] = argv[i++];
                targv[j++] = argv[i];
                uingiven = 1;
            }
        }
        else if (    !strcmp (argv[i], "-p") || !strcmp (argv[i], "--passwd")
                  || !strcmp (argv[i], "-s") || !strcmp (argv[i], "--status"))
        {
            if (argv[i + 1])
            {
                targv[j++] = argv[i++];
                targv[j++] = argv[i];
            }
        }
        else
        {
            if (argv[i + 1] && (!strcmp (argv[i], "-C") || !strcmp (argv[i], "--cmd")))
                i++;
            targv[j++] = argv[i];
        }
    }

    ConvInit ();
    save_conv_error = conv_error;

    prG = PreferencesC ();
    prG->verbose  = arg_vv;
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
    i18nInit (arg_i);
    PreferencesInit (prG);
    ReadLineInit ();
    
    loaded = arg_h ? 0 : PrefLoad (prG);
    prG->verbose &= ~0x80000000UL;
    i = i18nOpen (prG->locale);

    if (prG->enc_loc == ENC_AUTO)
        prG->enc_loc = ENC_FAUTO | ENC_ASCII;
    
    if (!OptGetVal (&prG->copts, CO_ENCODING, &res))
    {
        switch (prG->enc_loc & ~ENC_FAUTO)
        {
            case ENC_EUC:     OptSetVal (&prG->copts, CO_ENCODING, ENC_SJIS); break;
            case ENC_SJIS:    OptSetVal (&prG->copts, CO_ENCODING, ENC_SJIS); break;
            case ENC_KOI8:    OptSetVal (&prG->copts, CO_ENCODING, ENC_WIN1251); break;
            case ENC_WIN1251: OptSetVal (&prG->copts, CO_ENCODING, ENC_WIN1251); break;
            default:          OptSetVal (&prG->copts, CO_ENCODING, ENC_LATIN1);
        }
    }
    
    if (arg_c)
        prG->flags &= ~FLAG_COLOR;
    
    if (prG->flags & FLAG_COLOR)
    {
        rl_logo (MICQ_ICON_1 "\n" MICQ_ICON_2);
        rl_logo (MICQ_ICON_3);
        rl_logo (MICQ_ICON_4);
        rl_logo (MICQ_ICON_5);
        rl_logo (MICQ_ICON_6);
        rl_logo (MICQ_ICON_7);
    }
    else
    {
        rl_logo (MICQ_ICON_NOCOLOR_1 "\n" MICQ_ICON_NOCOLOR_2);
        rl_logo (MICQ_ICON_NOCOLOR_3);
        rl_logo (MICQ_ICON_NOCOLOR_4);
        rl_logo (MICQ_ICON_NOCOLOR_5);
        rl_logo (MICQ_ICON_NOCOLOR_6);
        rl_logo (MICQ_ICON_NOCOLOR_7);
    }

    rl_print (BuildVersion ());
    rl_print (BuildAttribution ());
    rl_print (i18n (1612, "This program was made without any help from Mirabilis or their consent.\n"));
    rl_print (i18n (1613, "No reverse engineering or decompilation of any Mirabilis code took place to make this program.\n"));

    rl_logo_clear ();

    if (arg_h)
    {
        rl_print  (i18n (2492, "Usage: micq [-h] [-c] [-b <basedir>] [-i <locale>] [-v[<level>]] [-l <logplace>]\n"));
        rl_print  (i18n (2493, "            [[-u <UIN>] [-p <passwd>] [-s <status>] [[-C] <command>]...]...\n"));
        rl_print  (i18n (2199, "  -h, --help     gives this help text\n"));
        rl_print  (i18n (2205, "  -c, --nocolor  disable colors\n"));
        rl_printf (i18n (2201, "  -b, --basedir  use given BASE dir (default: %s)\n"), "$HOME" _OS_PATHSEPSTR ".micq" _OS_PATHSEPSTR);
        rl_print  (i18n (2204, "  -i, --i1" "8n     use given locale (default: auto-detected)\n"));
        rl_print  (i18n (2200, "  -v, --verbose  set (or increase) verbosity (mostly for debugging)\n"));
        rl_printf (i18n (2203, "  -l, --logplace use given log file/dir (default: %s)\n"), "BASE" _OS_PATHSEPSTR "history" _OS_PATHSEPSTR);
        rl_print  (i18n (2494, "  -u, --uin      login with this UIN instead of those configured\n"));
        rl_print  (i18n (2495, "  -p, --passwd   ... and override password\n"));
        rl_print  (i18n (2496, "  -s, --status   ... and override status\n"));
        rl_print  (i18n (2497, "  -C, --cmd      ... and execute mICQ command\n"));
        exit (0);
    }
    
    if (conv_error)
    {
        rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
        rl_printf (i18n (2498, "Encoding %s%s%s is not supported by this mICQ.\n"), COLQUOTE, ConvEncName (conv_error), COLNONE);
        if (save_conv_error)
        {
            rl_print  (i18n (2499, "Please recompile using the '--enable-iconvrepl' configure option.\n"));
            if (conv_error <= prG->enc_loc)
            {
                rl_print  ("Warning: ");
                rl_printf ("Encoding %s is not supported by this mICQ.\n", ConvEncName (conv_error));
                rl_print  ("Please recompile using the '--enable-iconvrepl' configure option.\n");
            }
        }
        conv_error = 0;
    }
    
    if (arg_i)
    {
        if (!*arg_i || !strcasecmp (arg_i, "C") || !strcasecmp (arg_i, "POSIX"))
        {
            rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
            rl_printf (i18n (2500, "Manual locale setting %s ignored - use a real locale instead.\n"), s_qquote (arg_i));
        }
    }
    else
    {
        if (!prG->locale_orig)
        {
            rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
            rl_printf (i18n (2501, "Your locale is unset. Please use the %s--i1" "8n%s command line option or set one of the environment variables %sLC_ALL%s, %sLC_MESSAGES%s, or %sLANG%s to your current locale.\n"),
                      COLQUOTE, COLNONE, COLQUOTE, COLNONE, COLQUOTE, COLNONE, COLQUOTE, COLNONE);
        }
        else if (!*prG->locale_orig || !strcasecmp (prG->locale_orig, "C") || !strcasecmp (prG->locale_orig, "POSIX"))
        {
            rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
            rl_printf (i18n (2502, "Locale setting %s ignored - use a real locale instead.\n"), s_qquote (prG->locale_orig));
        }
        if (prG->locale_full && prG->locale_broken)
        {
            rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
            rl_printf (i18n (2503, "Your system doesn't know the %s locale - try %siconv --list%s.\n"),
                      s_qquote (prG->locale_full), COLQUOTE, COLNONE);
        }
    }

    if (!loaded && arg_l)
    {
        rl_printf (i18n (2504, "Could not open configuration file %s%s%s."), COLQUOTE, prG->rcfile, COLNONE);
        exit (20);
    }

    if (arg_f)
    {
        rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
        rl_printf (i18n (2505, "Deprecated option -f used. Please use the similar -b instead.\n"));
    }

    rl_printf (i18n (2506, "This console uses encoding %s.\n"), s_qquote (ConvEncName (prG->enc_loc)));
    if (i == -1)
    {
        rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
        rl_printf (i18n (2507, "Translation %s%s%s not found. Would you like to translate mICQ into your language?\n"),
                  COLQUOTE, s_qquote (prG->locale), COLNONE);
    }
    else if (i)
        rl_printf (i18n (2508, "English (%s) translation loaded (%s%d%s entries).\n"),
                  s_qquote (prG->locale), COLQUOTE, i, COLNONE);
    else
        rl_printf ("No translation requested. You live in nowhereland, eh?\n");

#ifdef _WIN32
    if (WSAStartup (0x0101, &wsaData))
    {
        perror (i18n (2509, "WSAStartup() for WinSocks 1.1 failed"));
        exit (1);
    }
#endif

    if (!loaded)
        Initialize_RC_File ();
    TabInit ();

#ifdef ENABLE_SSL
    if (SSLInit ())
        rl_printf (i18n (2371, "SSL init failed.\n"));
#endif

#ifdef ENABLE_TCL
    TCLInit ();
#endif

    if (uingiven)
    {
        targv[j++] = "-u";
        targv[j++] = "";
    }

    for (i = 0; (conn = ConnectionNr (i)); i++)
        if (conn->open && conn->flags & CONN_AUTOLOGIN)
            if (conn->type & TYPEF_ANY_SERVER)
                conn->status = conn->pref_status;

    conn = NULL; //ConnectionFind (TYPEF_ANY_SERVER, NULL, NULL);
    s_init (&arg_C, "", 0);
    arg_u = arg_p = arg_s = NULL;
    
    for (i = 0; i < j; i++)
    {
        if      (!strcmp (targv[i], "-u") || !strcmp (targv[i], "--uin"))
        {
             if (arg_u)
                 if (!(conn = ConnectionFindUIN (TYPEF_ANY_SERVER, atoll (arg_u))))
                     conn = PrefNewConnection (atoll (arg_u), arg_p);
             if (conn)
             {
                 if (arg_s)
                     conn->status = arg_ss;
                 if (arg_p)
                     s_repl (&conn->passwd, arg_p);
                 if (uingiven && arg_u && (loginevent = conn->open (conn)))
                     QueueEnqueueDep (conn, QUEUE_MICQ_COMMAND, 0, loginevent, NULL, conn->cont,
                                      OptSetVals (NULL, CO_MICQCOMMAND, arg_C.len ? arg_C.txt : "eg", 0),
                                      &CmdUserCallbackTodo);
             }
             
             arg_u = targv[++i];
             if (arg_u && !atoll (arg_u))
                 arg_u = NULL;
             arg_p = arg_s = NULL;
             s_init (&arg_C, "", 0);
        }
        else if (!strcmp (targv[i], "-p") || !strcmp (targv[i], "--passwd"))
            arg_p = targv[++i];
        else if (!strcmp (targv[i], "-s") || !strcmp (targv[i], "--status"))
        {
            if ((arg_s = targv[++i]))
            {
                if (!strncmp (arg_s, "inv", 3))
                    arg_ss = STATUS_INV;
                else if (!strcmp (arg_s, "dnd"))
                    arg_ss = STATUS_DND;
                else if (!strcmp (arg_s, "occ"))
                    arg_ss = STATUS_OCC;
                else if (!strcmp (arg_s, "na"))
                    arg_ss = STATUS_NA;
                else if (!strcmp (arg_s, "away"))
                    arg_ss = STATUS_AWAY;
                else if (!strcmp (arg_s, "ffc"))
                    arg_ss = STATUS_FFC;
                else if (!strncmp (arg_s, "off", 3))
                    arg_ss = STATUS_OFFLINE;
                else
                    arg_ss = atoll (arg_s);
            }
        }
        else
        {
            s_catc (&arg_C, ' ');
            s_cat (&arg_C, s_quote (targv[i]));
        }
    }
    
    if (!uingiven)
    {
        for (i = 0; (conn = ConnectionNr (i)); i++)
            if (conn->open && conn->flags & CONN_AUTOLOGIN)
            {
                if (conn->type & TYPEF_ANY_SERVER)
                {
                    if (conn->status != STATUS_OFFLINE && (loginevent = conn->open (conn)))
                         QueueEnqueueDep (conn, QUEUE_MICQ_COMMAND, 0, loginevent, NULL, conn->cont,
                                          OptSetVals (NULL, CO_MICQCOMMAND, arg_C.len ? arg_C.txt : "eg", 0),
                                          &CmdUserCallbackTodo);
                }
                else
                    conn->open (conn);
            }
    }
    s_done (&arg_C);
    free (targv);
}

/******************************
Main function connects gets UIN
and passwd and logins in and sits
in a loop waiting for server responses.
******************************/
int main (int argc, char *argv[])
{
    Connection *conn;
    int i;
    

    Init (argc, argv);
    while (!uiG.quit)
    {

#if INPUT_BY_POLL
        UtilIOSelectInit (0, 1000);
#else
        i = QueueTime ();
        if (!i)
            UtilIOSelectInit (0, 100);
        else if (i <= 2)
            UtilIOSelectInit (i, 0);
        else
            UtilIOSelectInit (2, 500000);
        UtilIOSelectAdd (STDIN_FILENO, READFDS);
#endif

        for (i = 0; (conn = ConnectionNr (i)); i++)
        {
            if (conn->connect & CONNECT_OK && conn->type & TYPEF_ANY_SERVER)
                Idle_Check (conn);
            if (conn->sok < 0 || !conn->dispatch)
                continue;
            if (conn->connect & CONNECT_SELECT_R)
                UtilIOSelectAdd (conn->sok, READFDS);
            if (conn->connect & CONNECT_SELECT_W)
                UtilIOSelectAdd (conn->sok, WRITEFDS);
            if (conn->connect & CONNECT_SELECT_X)
                UtilIOSelectAdd (conn->sok, EXCEPTFDS);
        }


        if (conv_error)
        {
            rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
            rl_printf (i18n (2498, "Encoding %s%s%s is not supported by this mICQ.\n"), COLQUOTE, ConvEncName (conv_error), COLNONE);
            conv_error = 0;
        }
        
        if (rl_signal)
        {
            ReadLineHandleSig ();
            CmdUser ("");
        }

        ReadLinePrompt ();

        UtilIOSelect ();

        if (__os_has_input)
        {
            strc_t input;
            char newbyte;
#if INPUT_BY_GETCH
            newbyte = getch ();
#else
            int k;
            k = read (STDIN_FILENO, &newbyte, 1);
            if (k && k != -1)
#endif
            if ((input = ReadLine (newbyte)))
                CmdUserInput (input);
        }
        

        for (i = 0; (conn = ConnectionNr (i)); i++)
        {
            if (conn->sok < 0 || !conn->dispatch
                || !UtilIOSelectIs (conn->sok, READFDS | WRITEFDS | EXCEPTFDS))
                continue;
            conn->dispatch (conn);
        }

        QueueRun ();
    }

    for (i = 0; (conn = ConnectionNr (i)); i++)
        if (conn->close)
            conn->close (conn);
    QueueRun ();
    if (prG->flags & FLAG_AUTOSAVE && uiG.quit == 1)
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
