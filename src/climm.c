/* $Id: climm.c 2299 2007-05-29 22:57:38Z kuhlmann $ */
/* Copyright
 * This file may be distributed under version 2 of the GPL licence.
 */


#include "climm.h"

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

#include "file_util.h"
#include "util_rl.h"
#include "conv.h"
#include "buildmark.h"
#include "preferences.h"
#include "im_request.h"
#include "contact.h"
#include "connection.h"
#include "oscar_dc.h"
#include "util_tabs.h"
#include "util_io.h"
#include "oscar_base.h"
#include "util_tcl.h"
#include "util_ssl.h"
#include "util_otr.h"
#include "util_alias.h"
#include "cmd_user.h"
#include "util_parse.h"
#include "os.h"

#define CLIMM_ICON_1 "   " GREEN "_" SGR0 "     "
#define CLIMM_ICON_2 " " GREEN "_/ \\_" SGR0 "   "
#define CLIMM_ICON_3 GREEN "/ \\ / \\" SGR0 "  "
#define CLIMM_ICON_4 RED ">--" YELLOW "o" GREEN "--<" SGR0 "  "
#define CLIMM_ICON_5 RED "\\_" RED "/" BLUE " \\" GREEN "_/" SGR0 "  "
#define CLIMM_ICON_6 " " BLUE "c" BOLD "l" SGR0 BLUE UL "i" BLUE "/" BLUE "mm" SGR0 "  "
#define CLIMM_ICON_7 "          "

#define CLIMM_ICON_NOCOLOR_1 "   _     "
#define CLIMM_ICON_NOCOLOR_2 " _/ \\_   "
#define CLIMM_ICON_NOCOLOR_3 "/ \\ / \\  "
#define CLIMM_ICON_NOCOLOR_4 ">--o--<  "
#define CLIMM_ICON_NOCOLOR_5 "\\_/ \\_/  "
#define CLIMM_ICON_NOCOLOR_6 " cli/mm  "
#define CLIMM_ICON_NOCOLOR_7 "         "

/*
      |
cli=o=O
    | |
    O=o=mm
    |

cli
  O=o==
  | |
==o=O
    |mm

    |
==o=O
  | |
  O=o==
  |

CLI  /
  +-+
  | |
  +-+
 /   MM
*/

static void Idle_Check (Server *serv);

user_interface_state uiG = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
Preferences *prG;

static void Idle_Check (Server *serv)
{
    int saver = -1;
    time_t now;
    UDWORD delta;
    status_t news = ims_offline;
    status_noi_t noinv = ContactClearInv (serv->status);

    if (~serv->type & TYPEF_ANY_SERVER)
        return;

    if ((noinv == imr_dnd || noinv == imr_occ || noinv == imr_ffc)
        || ~serv->connect & CONNECT_OK)
    {
        uiG.idle_val = 0;
        return;
    }
    
    now = time (NULL);

    if (!prG->away_time)
    {
        if (!uiG.idle_val)
            uiG.idle_val = now;

        delta = os_DetermineIdleTime (now, uiG.idle_val);

        if (delta > 10 && uiG.idle_val)
        {
            saver = os_DetectLockedWorkstation();
            
            if (saver >= 0 && saver <= 3)
            {
                switch (saver)
                {
                    case 0: /* no screen saver, not locked */
                        if (noinv != imr_dnd && noinv != imr_occ && noinv != imr_na && noinv != imr_away)
                            return;
                        news = ContactCopyInv (serv->status, imr_online);
                        serv->idle_flag = i_idle;
                        break;
                    case 2: /* locked workstation */
                    case 3:
                        if (noinv == imr_na)
                            return;
                        news = ContactCopyInv (serv->status, imr_na);
                        uiG.idle_msgs = 0;
                        serv->idle_flag = i_os;
                        break;
                    case 1: /* screen saver only */
                        if (noinv == imr_away || noinv == imr_occ || noinv == imr_dnd)
                            return;
                        news = ContactCopyInv (serv->status, imr_away);
                        uiG.idle_msgs = 0;
                        serv->idle_flag = i_os;
                        break;
                }

                uiG.idle_val = 0;
                delta = 0;
                if (news == ims_offline || news == serv->status)
                    return;
            }
        }

        if (serv->idle_flag == i_idle && news == ims_offline)
            return;
    }
    else
        delta = uiG.idle_val ? os_DetermineIdleTime (now, uiG.idle_val) : 0;

    if (!uiG.idle_val)
        uiG.idle_val = now;
    
    if (serv->idle_flag == i_idle)
    {
        if (delta >= prG->away_time && noinv != imr_dnd
            && noinv != imr_occ && noinv != imr_na && noinv != imr_away)
        {
            news = ContactCopyInv (serv->status, imr_away);
            serv->idle_flag = i_want_away;
            uiG.idle_msgs = 0;
        }
    }
    else if (serv->idle_flag != i_os)
    {
        if (delta < prG->away_time || !prG->away_time)
        {
            news = ContactCopyInv (serv->status, imr_online);
            serv->idle_flag = i_idle;
            uiG.idle_val = 0;
        }
        else if (serv->idle_flag == i_want_away && delta >= 2 * prG->away_time)
        {
            news = ContactCopyInv (serv->status, imr_na);
            serv->idle_flag = i_want_na;
        }
    }
    if (news != ims_offline && news != serv->status)
    {
        IMSetStatus (serv, NULL, news, i18n (2605, "Automatic status change."));
        rl_printf ("%s ", s_now);
        rl_printf (i18n (1064, "Automatically changed status to %s.\n"), s_status (news, 0));
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
    Server *serv;
    Connection *conn;
    Event *loginevent = NULL;
    const char *arg_v, *arg_f, *arg_l, *arg_i, *arg_b, *arg_s, *arg_u, *arg_p;
    UDWORD loaded, uingiven = 0, arg_h = 0, arg_vv = 0, arg_c = 0;
    status_t arg_ss = ims_offline;
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
        else if (!strcmp (argv[i], "-u") || !strcmp (argv[i], "--uin") || !strcmp (argv[i], "--user"))
        {
            if (argv[i + 1] && *argv[i + 1])
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
                uingiven = 1;
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
    AliasAdd ("r", "msg %r", TRUE); /*"*/
    AliasAdd ("a", "msg %a", TRUE);
    
    loaded = arg_h ? 0 : PrefLoad (prG);
    prG->verbose &= ~0x80000000UL;
    i = i18nOpen (NULL);

    if (prG->enc_loc == ENC_AUTO)
        prG->enc_loc = ENC_FGUESS | ENC_ASCII;
    
    if (!OptGetVal (&prG->copts, CO_ENCODING, &res))
    {
        UBYTE enc;
        switch (ENC (enc_loc))
        {
            case ENC_EUC:
            case ENC_SJIS:
                enc = ENC_SJIS;
                break;
            case ENC_KOI8:
            case ENC_WIN1251:
                enc = ENC_WIN1251;
                break;
            default:          enc = ENC_LATIN1;
        }
        OptSetVal (&prG->copts, CO_ENCODING, enc);
        OptSetStr (&prG->copts, CO_ENCODINGSTR, ConvEncName (enc));
    }
    
    if (arg_c)
        prG->flags &= ~FLAG_COLOR;
    else if (!arg_h && !loaded)
        prG->flags |= FLAG_COLOR;
    
    if (prG->flags & FLAG_COLOR)
    {
        rl_logo (CLIMM_ICON_1 "\n" CLIMM_ICON_2);
        rl_logo (CLIMM_ICON_3);
        rl_logo (CLIMM_ICON_4);
        rl_logo (CLIMM_ICON_5);
        rl_logo (CLIMM_ICON_6);
        rl_logo (CLIMM_ICON_7);
    }
    else
    {
        rl_logo (CLIMM_ICON_NOCOLOR_1 "\n" CLIMM_ICON_NOCOLOR_2);
        rl_logo (CLIMM_ICON_NOCOLOR_3);
        rl_logo (CLIMM_ICON_NOCOLOR_4);
        rl_logo (CLIMM_ICON_NOCOLOR_5);
        rl_logo (CLIMM_ICON_NOCOLOR_6);
        rl_logo (CLIMM_ICON_NOCOLOR_7);
    }

    rl_print (BuildVersion ());
    rl_print (BuildAttribution ());
    rl_print (i18n (1612, "This program was made without any help from Mirabilis or their consent.\n"));
    rl_print (i18n (1613, "No reverse engineering or decompilation of any Mirabilis code took place to make this program.\n"));

    rl_logo_clear ();

    if (arg_h)
    {
        rl_print  (i18n (2492, "Usage: climm [-h] [-c] [-b <basedir>] [-i <locale>] [-v[<level>]] [-l <logplace>]\n"));
        rl_print  (i18n (2493, "            [[-u <UIN>] [-p <passwd>] [-s <status>] [[-C] <command>]...]...\n"));
        rl_print  (i18n (2199, "  -h, --help     gives this help text\n"));
        rl_print  (i18n (2205, "  -c, --nocolor  disable colors\n"));
        rl_printf (i18n (2201, "  -b, --basedir  use given BASE dir (default: %s)\n"), "$HOME" _OS_PATHSEPSTR ".climm" _OS_PATHSEPSTR);
        rl_print  (i18n (2204, "  -i, --i1" "8n     use given locale (default: auto-detected)\n"));
        rl_print  (i18n (2200, "  -v, --verbose  set (or increase) verbosity (mostly for debugging)\n"));
        rl_printf (i18n (2203, "  -l, --logplace use given log file/dir (default: %s)\n"), "BASE" _OS_PATHSEPSTR "history" _OS_PATHSEPSTR);
        rl_print  (i18n (2494, "  -u, --user     login with this user/UIN instead of those configured\n"));
        rl_print  (i18n (2495, "  -p, --passwd   ... and override password\n"));
        rl_print  (i18n (2496, "  -s, --status   ... and override status\n"));
        rl_print  (i18n (2497, "  -C, --cmd      ... and execute climm command\n"));
        exit (0);
    }
    
    if (conv_error)
    {
        rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
        rl_printf (i18n (2498, "Encoding %s%s%s is not supported by this climm.\n"), COLQUOTE, ConvEncName (conv_error), COLNONE);
        if (save_conv_error)
        {
            rl_print  (i18n (2499, "Please recompile using the '--enable-iconvrepl' configure option.\n"));
            if (conv_error <= prG->enc_loc)
            {
                rl_print  ("Warning: ");
                rl_printf ("Encoding %s is not supported by this climm.\n", ConvEncName (conv_error));
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
            rl_printf (i18n (2502, "Locale setting %s is no real locale and probably not what you want.\n"), s_qquote (prG->locale_orig));
        }
        if (prG->locale_full && prG->locale_broken == TRUE)
        {
            rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
            rl_printf (i18n (2503, "Your system doesn't know the %s locale - try %slocale -a%s.\n"),
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
        rl_printf (i18n (2507, "Translation %s%s%s not found. Would you like to translate climm into your language?\n"),
                  COLQUOTE, s_qquote (prG->locale), COLNONE);
    }
    else if (i)
        rl_printf (i18n (2508, "No translation (%s) loaded (%s%d%s entries).\n"),
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

#ifdef ENABLE_OTR
    OTRInit ();
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
        if (conn->c_open && conn->flags & CONN_AUTOLOGIN)
            conn->status = conn->pref_status;

    serv = NULL;
    s_init (&arg_C, "", 0);
    arg_u = arg_p = arg_s = NULL;
    
    for (i = 0; i < j; i++)
    {
        if      (!strcmp (targv[i], "-u") || !strcmp (targv[i], "--uin") || !strcmp (targv[i], "--user"))
        {
            if (arg_u)
            {
                if (!(serv = ServerFindScreen (0, arg_u)))
                {
                    char *u = strdup (arg_u);
                    if (is_valid_icq_name (u))
                        serv = PrefNewConnection (TYPE_SERVER, u, arg_p);
#ifdef ENABLE_XMPP
                    else if (is_valid_xmpp_name (u))
                        serv = PrefNewConnection (TYPE_XMPP_SERVER, u, arg_p);
#endif
#ifdef ENABLE_MSN
                    else if (is_valid_msn_name (u))
                        serv = PrefNewConnection (TYPE_MSN_SERVER, u, arg_p);
#endif
                    s_free (u);
                }
            }
            else if (!*targv[i+1])
                serv = Connection2Server (ConnectionFind (TYPEF_ANY_SERVER, NULL, NULL));
            if (serv)
            {
                if (arg_s)
                    serv->status = arg_ss;
                if (arg_p)
                    s_repl (&serv->passwd, arg_p);
                if (serv->passwd && *serv->passwd && (!arg_s || arg_ss != ims_offline) && (loginevent = serv->c_open (serv)))
                    QueueEnqueueDep (Server2Connection (serv), QUEUE_CLIMM_COMMAND, 0, loginevent, NULL, serv->cont,
                                     OptSetVals (NULL, CO_CLIMMCOMMAND, arg_C.len ? arg_C.txt : "eg", 0),
                                     &CmdUserCallbackTodo);
            }
            
            arg_u = targv[++i];
            if (arg_u && !atoll (arg_u))
                arg_u = NULL;
            arg_p = arg_s = NULL;
            s_init (&arg_C, "", 0);
            arg_ss = ims_online;
            serv = NULL;
        }
        else if (!strcmp (targv[i], "-p") || !strcmp (targv[i], "--passwd"))
            arg_p = targv[++i];
        else if (!strcmp (targv[i], "-s") || !strcmp (targv[i], "--status"))
        {
            if ((arg_s = targv[++i]))
            {
                if (!ContactStatus (&arg_s, &arg_ss))
                    arg_ss = IcqToStatus (atoll (arg_s));
            }
        }
        else
        {
            s_catc (&arg_C, ' ');
            s_cat (&arg_C, s_quote (targv[i]));
        }
    }
    
    conn = NULL;
    if (!uingiven)
        for (i = 0; (conn = ConnectionNr (i)); i++)
            if (conn->c_open && conn->flags & CONN_AUTOLOGIN && conn->type & TYPEF_ANY_SERVER)
                if (!conn->passwd || !*conn->passwd)
                {
                    strc_t pwd;
                    const char *s = s_sprintf ("%s", s_qquote (conn->screen));
                    rl_printf (i18n (2689, "Enter password for %s account %s: "),
                               s_qquote (ConnectionServerType (conn->type)), s);
                    pwd = UtilIOReadline (stdin);
                    rl_printf ("\n");
                    s_repl (&conn->passwd, pwd ? ConvFrom (pwd, prG->enc_loc)->txt : "");
                }

    if (!uingiven)
    {
        for (i = 0; (conn = ConnectionNr (i)); i++)
            if (conn->c_open && conn->flags & CONN_AUTOLOGIN)
            {
                if (conn->passwd && *conn->passwd && conn->status != ims_offline && (loginevent = conn->c_open (conn)))
                         QueueEnqueueDep (conn, QUEUE_CLIMM_COMMAND, 0, loginevent, NULL, conn->cont,
                                          OptSetVals (NULL, CO_CLIMMCOMMAND, arg_C.len ? arg_C.txt : "eg", 0),
                                          &CmdUserCallbackTodo);
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
    Server *serv;
    int i;
    
    umask (077);
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
            serv = Connection2Server (conn);
            if (conn->connect & CONNECT_OK && serv)
                Idle_Check (serv);
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
            rl_printf (i18n (2498, "Encoding %s%s%s is not supported by this climm.\n"), COLQUOTE, ConvEncName (conv_error), COLNONE);
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
            if (conn->dispatch)
                conn->dispatch (conn);
            else
            {
                printf ("FIXME: avoid spinning.\n");
                conn->connect &= ~(CONNECT_SELECT_R | CONNECT_SELECT_W | CONNECT_SELECT_X);
            }
        }

        QueueRun ();
    }

#ifdef ENABLE_OTR
    OTREnd ();
#endif

    for (i = 0; (conn = ConnectionNr (i)); i++)
        if (conn->close)
            conn->close (conn);
    QueueRun ();
    if (prG->flags & FLAG_AUTOSAVE && uiG.quit == 1)
    {
        int i, j;
        Contact *cont;

        for (i = 0; (serv = ServerNr (i)); i++)
            if (serv->contacts)
                for (j = 0; (cont = ContactIndex (serv->contacts, j)); j++)
                    ContactMetaSave (cont);
        Save_RC ();
    }
    
    return 0;
}
