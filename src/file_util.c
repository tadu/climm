/* $Id$ */
/* Copyright: This file may be distributed under version 2 of the GPL licence. */

#include "micq.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_TERMIOS_H
#include <termios.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include "buildmark.h"
#include "util_ui.h"
#include "file_util.h"
#include "tabs.h"
#include "contact.h"
#include "tcp.h"
#include "util.h"
#include "conv.h"
#include "cmd_user.h"
#include "cmd_pkt_cmd_v5.h"
#include "cmd_pkt_cmd_v5_util.h"
#include "preferences.h"
#include "util_io.h"
#include "contact.h"
#include "remote.h"
#include "cmd_pkt_v8.h"
#include "session.h"
#include "util_str.h"


/****/

#define WAND1 " \xb7 \xb7 "
#define WAND2 "\xb7 o \xb7"
#define WAND3 " \xb7 \\ "
#define WAND4 "    \\"

void Initialize_RC_File ()
{
    char pwd1[20], pwd2[20], input[200];
    Connection *conn, *connt;
#ifdef ENABLE_REMOTECONTROL
    Connection *conns;
#endif
    char *passwd;
    char *t;
    UDWORD uin;
    long tmpuin;
    
    prG->away_time = default_away_time;

    M_print ("\n");
    M_print (i18n (1793, "No valid UIN found. The setup wizard will guide you through the process of setting one up.\n"));
    M_print (i18n (1794, "If you already have an UIN, please enter it. Otherwise, enter 0, and I will request one for you.\n"));
    M_printf ("%s ", i18n (1618, "UIN:"));
    fflush (stdout);
    M_fdnreadln (stdin, input, sizeof (input));
    tmpuin = 0;
    sscanf (input, "%ld", &tmpuin);
    uin = tmpuin;

    M_print ("\n");
    if (uin)
        M_printf (i18n (1781, "Your password for UIN %ld:\n"), uin);
    else
        M_print (i18n (1782, "You need a password for your new UIN.\n"));
    memset (pwd1, 0, sizeof (pwd1));
    while (!pwd1[0])
    {
        M_printf ("%s ", i18n (1795, "Password:"));
        fflush (stdout);
        Echo_Off ();
        M_fdnreadln (stdin, pwd1, sizeof (pwd1));
        Echo_On ();
        M_print ("\n");
        if (uin)
            continue;

        M_print (i18n (1783, "To prevent typos, please enter your password again.\n"));
        M_printf ("%s ", i18n (1795, "Password:"));
        fflush (stdout);
        memset (pwd2, 0, sizeof (pwd2));
        Echo_Off ();
        M_fdnreadln (stdin, pwd2, sizeof (pwd2));
        Echo_On ();
        M_print ("\n");
        if (strcmp (pwd1, pwd2))
        {
            M_printf ("\n%s\n", i18n (1093, "Passwords did not match - please try again."));
            pwd1[0] = '\0';
        }
    }
#ifdef ENABLE_UTF8
    passwd = strdup (c_out (pwd1));
#else
    passwd = strdup (pwd1);
#endif

    prG->s5Use = 0;
    prG->s5Port = 0;

    M_print ("\n");
    M_print (i18n (1784, "If you are firewalled, you may need to use a SOCKS5 server. If you do, please enter its hostname or IP address. Otherwise, or if unsure, just press return.\n"));
    M_printf ("%s ", i18n (1094, "SOCKS5 server:"));
    fflush (stdout);
    M_fdnreadln (stdin, input, sizeof (input));
    if (strlen (input) > 1)
    {
        if ((t = strchr (input, ':')))
        {
            prG->s5Port = atoi (t + 1);
            *t = '\0';
            prG->s5Host = strdup (input);
        }
        else
        {
            prG->s5Host = strdup (input);
            M_print (i18n (1786, "I also need the port the socks server listens on. If unsure, press return for the default port.\n"));
            M_printf ("%s ", i18n (1095, "SOCKS5 port:"));
            fflush (stdout);
            M_fdnreadln (stdin, input, sizeof (input));
            sscanf (input, "%hu", &prG->s5Port);
        }
        if (!prG->s5Port)
            prG->s5Port = 1080;

        prG->s5Use = 1;
        prG->s5Auth = 0;
        prG->s5Pass = NULL;
        prG->s5Name = NULL;

        M_print ("\n");
        M_print (i18n (1787, "You probably need to authenticate yourself to the socks server. If so, you need to enter the user name the administrator of the socks server gave you. Otherwise, just press return.\n"));
        M_printf ("%s ", i18n (1096, "SOCKS5 user name:"));
        fflush (stdout);
        M_fdnreadln (stdin, input, sizeof (input));
        if (strlen (input) > 1)
        {
            prG->s5Auth = 1;
            prG->s5Name = strdup (input);
            M_print (i18n (1788, "Now I also need the password for this user.\n"));
            M_printf ("%s ", i18n (1097, "SOCKS5 password:"));
            fflush (stdout);
            M_fdnreadln (stdin, input, sizeof (input));
            prG->s5Pass = strdup (input);
        }
    }

    M_print ("\n");

    if (!uin)
    {
        M_print (i18n (1796, "Setup wizard finished. Please wait until registration has finished.\n"));
        conn = SrvRegisterUIN (NULL, passwd);
        conn->flags = CONN_WIZARD;
        conn->open = &ConnectionInitServer;
    }
    else
    {
        M_print (i18n (1791, "Setup wizard finished. Congratulations!\n"));
        conn = ConnectionC ();
        conn->open = &ConnectionInitServer;
        conn->spref = PreferencesConnectionC ();
        
        conn->spref->type = TYPE_SERVER;
        conn->spref->flags = CONN_AUTOLOGIN | CONN_WIZARD;
        conn->spref->server = strdup ("login.icq.com");
        conn->spref->port = 5190;
        conn->spref->status = STATUS_ONLINE;
        conn->spref->version = 8;
        conn->spref->uin = uin;
#ifdef __BEOS__
        conn->spref->passwd = strdup (passwd);
#endif
        
        conn->server  = strdup ("login.icq.com");
        conn->port    = 5190;
        conn->type    = TYPE_SERVER;
        conn->flags   = CONN_AUTOLOGIN | CONN_WIZARD;
        conn->ver     = 8;
        conn->uin     = uin;
        conn->passwd  = strdup (passwd);
    }
    
#ifdef ENABLE_PEER2PEER
    connt = ConnectionC ();
    connt->open = &ConnectionInitPeer;
    connt->spref = PreferencesConnectionC ();

    connt->parent = conn;
    conn->assoc = connt;
    connt->spref->type = TYPE_MSGLISTEN;
    connt->spref->flags = CONN_AUTOLOGIN;
    connt->spref->status = prG->s5Use ? 2 : TCP_OK_FLAG;
    connt->type  = connt->spref->type;
    connt->flags = connt->spref->flags;
    connt->status = connt->spref->status;
    connt->spref->version = 8;
    connt->ver = 8;
#endif

#ifdef ENABLE_REMOTECONTROL
    conns = ConnectionC ();
    conns->open = &RemoteOpen;
    conns->spref = PreferencesConnectionC ();
    
    conns->parent = NULL;
    conns->spref->type = TYPE_REMOTE;
    conns->spref->flags = CONN_AUTOLOGIN;
    conns->spref->server = strdup ("remote-control");
    conns->type  = conns->spref->type;
    conns->flags = conns->spref->flags;
    conns->server = strdup (conns->spref->server);
#endif

    prG->status = STATUS_ONLINE;
    prG->tabs = TABS_SIMPLE;
    prG->flags = FLAG_LOG | FLAG_LOG_ONOFF | FLAG_DELBS;
    prG->flags |= FLAG_AUTOSAVE | FLAG_AUTOFINGER;
#ifdef ANSI_TERM
    prG->flags |= FLAG_COLOR;
#endif
    prG->auto_dnd  = strdup (i18n (1929, "User is dnd [Auto-Message]"));
    prG->auto_away = strdup (i18n (1010, "User is away [Auto-Message]"));
    prG->auto_na   = strdup (i18n (1011, "User is not available [Auto-Message]"));
    prG->auto_occ  = strdup (i18n (1012, "User is occupied [Auto-Message]"));
    prG->auto_inv  = strdup (i18n (1013, "User is offline"));
    prG->auto_ffc  = strdup (i18n (2055, "User is ffc and wants to chat about everything."));
    prG->logplace  = strdup ("history" _OS_PATHSEPSTR);
    prG->chat      = 49;

    conn->contacts = ContactGroupFind (0, conn, s_sprintf ("contacts-icq8-%ld", uin), 1);
#ifdef ENABLE_UTF8
    ContactFind (conn->contacts, 0, 82274703, "R\xc3\xbc" "diger Kuhlmann", 1);
#else
    ContactFind (conn->contacts, 0, 82274703, "R\xfc" "diger Kuhlmann", 1);
#endif
    ContactFind (conn->contacts, 0, 82274703, "mICQ maintainer", 1);
    ContactFind (conn->contacts, 0, 82274703, "Tadu", 1);

    if (uin)
        Save_RC ();
    
    free (passwd);
}

#define PrefParse(x)          switch (1) { case 1: if (!s_parse    (&args, &x)) { M_printf (i18n (2123, "%sSyntax error%s: Too few arguments: '%s'\n"), COLERROR, COLNONE, buf); continue; }}
#define PrefParseInt(i)       switch (1) { case 1: if (!s_parseint (&args, &i)) { M_printf (i18n (2124, "%sSyntax error%s: Not an integer: '%s'\n"), COLERROR, COLNONE, buf); continue; }}
#define ERROR continue;

/*
 * Reads in a configuration file.
 */
void Read_RC_File (FILE *rcf)
{
    char buf[450];
    char *tmp = NULL, *tmp2 = NULL, *cmd = NULL;
    char *p, *args;
    Contact *cont = NULL, *lastcont = NULL;
    Connection *oldconn = NULL, *conn = NULL;
#ifdef ENABLE_REMOTECONTROL
    Connection *conns = NULL;
#endif
    ContactGroup *cg = NULL;
    int section, dep = 0;
    UDWORD uin, i;
    UWORD flags;
    UBYTE enc = ENC_LATIN1, format = 0;
    char *tab_nick_spool[TAB_SLOTS];
    int spooled_tab_nicks;

    spooled_tab_nicks = 0;
    for (section = 0; !M_fdnreadln (rcf, buf, sizeof (buf)); )
    {
        if (!buf[0] || (buf[0] == '#'))
            continue;
        if (buf[0] == '[')
        {
            if (!strcasecmp (buf, "[General]"))
                section = 0;
            else if (!strcasecmp (buf, "[Contacts]"))
                section = 1;
            else if (!strcasecmp (buf, "[Strings]"))
                section = 2;
            else if (!strcasecmp (buf, "[Connection]"))
            {
                section = 3;
                oldconn = conn;
                conn = ConnectionC ();
                conn->spref = PreferencesConnectionC ();
            }
            else if (!strcasecmp (buf, "[Group]"))
            {
                section = 4;
                cg = ContactGroupFind (0, (Connection *)(-1), "", 1);
                cg->serv = NULL;
                for (i = 0; (conn = ConnectionNr (i)); i++)
                    if (conn->flags & CONN_AUTOLOGIN)
                    {
                        cg->serv = conn;
                        break;
                    }
            }
            else
            {
                M_printf (COLERROR "%s" COLNONE " ", i18n (1619, "Warning:"));
                M_printf (i18n (1659, "Unknown section %s in configuration file."), ConvToUTF8 (buf, enc));
                M_print ("\n");
                section = -1;
            }
            continue;
        }
        args = buf;
        switch (section)
        {
            case -1:
                M_printf (COLERROR "%s" COLNONE " ", i18n (1619, "Warning:"));
                M_print  (i18n (1675, "Ignored line:"));
                M_printf (" %s\n", ConvToUTF8 (buf, enc));
                break;
            case 0:
                if (!s_parse (&args, &cmd))
                    continue;

                if (!strcasecmp (cmd, "encoding"))
                {
                    int which, what;
                    PrefParse (cmd);
                    
                    if (!strcasecmp (cmd, "remote"))
                        which = 1;
                    else if (!strcasecmp (cmd, "local"))
                        which = 2;
                    else if (!strcasecmp (cmd, "file"))
                        which = 3;
                    else
                        ERROR;
                    
                    PrefParse (cmd);
                    if (!strcasecmp (cmd, "auto"))
                        continue;
                    else if (!strcasecmp (cmd, "utf8"))
                    {   what = ConvEnc ("utf-8");        dep = 1; }
                    else if (!strcasecmp (cmd, "latin1"))
                    {   what = ConvEnc ("iso-8859-1");   dep = 1; }
                    else if (!strcasecmp (cmd, "latin9"))
                    {   what = ConvEnc ("iso-8859-15");  dep = 1; }
                    else if (!strcasecmp (cmd, "koi8"))
                    {   what = ConvEnc ("koi8-u");       dep = 1; }
                    else if (!strcasecmp (cmd, "win1251"))
                    {   what = ConvEnc ("windows-1251"); dep = 1; }
                    else if (!strcasecmp (cmd, "euc"))
                    {   what = ConvEnc ("euc-jp");       dep = 1; }
                    else if (!strcasecmp (cmd, "sjis"))
                    {   what = ConvEnc ("shift-jis");    dep = 1; }
                    else
                        what = ConvEnc (cmd);

                    if (!what)
                    {
                        M_printf (COLERROR "%s" COLNONE " ", i18n (2251, "Error:"));
                        M_printf (i18n (2216, "This mICQ doesn't know the '%s' encoding.\n"), cmd);
                        ERROR;
                    }
                    if (what & ENC_AUTO)
                    {
                        if ((which == 3 && (what ^ prG->enc_loc) & ~ENC_AUTO)
                            || (which == 2 && (what ^ enc) & ~ENC_AUTO))
                            M_printf (COLERROR "%s" COLNONE " ", i18n (2251, "Error:"));
                        else
                            M_printf (COLERROR "%s" COLNONE " ", i18n (1619, "Warning:"));
                        M_printf (i18n (2217, "This mICQ can't convert between '%s' and unicode.\n"), cmd);
                        what &= ~ENC_AUTO;
                    }
                    if (which == 1)
                        prG->enc_rem = what;
                    else if (which == 2)
                        prG->enc_loc = what;
                    else
                        enc = what;
                }
                else if (!strcasecmp (cmd, "format"))
                {
                    PrefParseInt (i);
                    format = i;
                    if (format != 1)
                        return;
                }
                else if (!strcasecmp (cmd, "receivescript") || !strcasecmp (cmd, "event"))
                {
                    if (!strcasecmp (cmd, "receivescript"))
                        dep |= 1;
                    if (!s_parse (&args, &tmp))
                    {
                        dep |= 1;
                        prG->event_cmd = NULL;
                        continue;
                    }
                    if (!strcmp (tmp, "off") || !strcmp (tmp, i18n (1086, "off")) || !*tmp)
                        prG->event_cmd = NULL;
                    else
                    {
                        prG->event_cmd = strdup (tmp);
#ifndef MSGEXEC
                        M_printf (COLERROR "%s" COLNONE " ", i18n (1619, "Warning:"));
                        M_printf (i18n (1817, "The event scripting feature is disabled.\n"));
#endif
                    }
                }
                else if (!strcasecmp (cmd, "s5_use"))
                {
                    PrefParseInt (i);
                    prG->s5Use = i;
                }
                else if (!strcasecmp (cmd, "s5_host"))
                {
                    PrefParse (tmp);
                    prG->s5Host = strdup (tmp);
                }
                else if (!strcasecmp (cmd, "s5_port"))
                {
                    PrefParseInt (i);
                    prG->s5Port = i;
                }
                else if (!strcasecmp (cmd, "s5_auth"))
                {
                    PrefParseInt (i);
                    prG->s5Auth = i;
                }
                else if (!strcasecmp (cmd, "s5_name"))
                {
                    PrefParse (tmp);
                    prG->s5Name = strdup (tmp);
                }
                else if (!strcasecmp (cmd, "s5_pass"))
                {
                    PrefParse (tmp);
                    prG->s5Pass = strdup (tmp);
                }
                else if (!strcasecmp (cmd, "verbose"))
                {
                    PrefParseInt (i);
                    if (!prG->verbose)
                        prG->verbose = i;
                }
                else if (!strcasecmp (cmd, "logplace"))
                {
                    PrefParse (tmp);
                    if (!prG->logplace) /* don't overwrite command line arg */
                        prG->logplace = strdup (tmp);
                }
                else if (!strcasecmp (cmd, "chat"))
                {
                    PrefParseInt (i);
                    prG->chat = i;
                }
                else if (!strcasecmp (cmd, "color") || !strcasecmp (cmd, "colour"))
                {
                    if (!s_parseint (&args, &i))
                    {
                        PrefParse (tmp);
                        
                        if      (!strcasecmp (tmp, "none"))     i = 0;
                        else if (!strcasecmp (tmp, "server"))   i = 1;
                        else if (!strcasecmp (tmp, "client"))   i = 2;
                        else if (!strcasecmp (tmp, "message"))  i = 3;
                        else if (!strcasecmp (tmp, "contact"))  i = 4;
                        else if (!strcasecmp (tmp, "sent"))     i = 5;
                        else if (!strcasecmp (tmp, "ack"))      i = 6;
                        else if (!strcasecmp (tmp, "error"))    i = 7;
                        else if (!strcasecmp (tmp, "incoming")) i = 8;
                        else if (!strcasecmp (tmp, "debug"))    i = 9;
                        else if (!strcasecmp (tmp, "scheme"))   i = -1;
                        else continue;
                    }
                    
                    if (i == -1)
                    {
                        PrefParseInt (i);
                        PrefSetColorScheme (prG, i);
                    }
                    else
                    {
                        char buf[200], *c;

                        if (i >= CXCOUNT)
                            continue;
                        buf[0] = '\0';

                        while (s_parse (&args, &cmd))
                        {
                            if      (!strcasecmp (cmd, "black"))   c = BLACK;
                            else if (!strcasecmp (cmd, "red"))     c = RED;
                            else if (!strcasecmp (cmd, "green"))   c = GREEN;
                            else if (!strcasecmp (cmd, "yellow"))  c = YELLOW;
                            else if (!strcasecmp (cmd, "blue"))    c = BLUE;
                            else if (!strcasecmp (cmd, "magenta")) c = MAGENTA;
                            else if (!strcasecmp (cmd, "cyan"))    c = CYAN;
                            else if (!strcasecmp (cmd, "white"))   c = WHITE;
                            else if (!strcasecmp (cmd, "none"))    c = SGR0;
                            else if (!strcasecmp (cmd, "bold"))    c = BOLD;
                            else c = cmd;
                            
                            snprintf (buf + strlen (buf), sizeof (buf) - strlen (buf), "%s", c);
                        }
                        prG->scheme = -1;
                        s_repl (&prG->colors[i], buf);
                    }
                }
                else if (!strcasecmp (cmd, "linebreaktype"))
                {
                    PrefParseInt (i);

                    prG->flags &= ~FLAG_LIBR_BR & ~FLAG_LIBR_INT;
                    if (!i || i == 3)
                        prG->flags |= FLAG_LIBR_BR;
                    if (i & 2)
                        prG->flags |= FLAG_LIBR_INT;
                }
                else if (!strcasecmp (cmd, "auto"))
                {
                    if (!s_parse (&args, &tmp))
                    {
                        dep = 1;
                        prG->flags |= FLAG_AUTOREPLY;
                        continue;
                    }
                    
                    if (!strcasecmp (tmp, "on"))
                        prG->flags |= FLAG_AUTOREPLY;
                    else if (!strcasecmp (tmp, "off"))
                        prG->flags &= ~FLAG_AUTOREPLY;
                    else if (!strcasecmp (tmp, "away"))
                    {
                        PrefParse (tmp);
                        s_repl (&prG->auto_away, ConvToUTF8 (tmp, enc));
                    }
                    else if (!strcasecmp (tmp, "na"))
                    {
                        PrefParse (tmp);
                        s_repl (&prG->auto_na, ConvToUTF8 (tmp, enc));
                    }
                    else if (!strcasecmp (tmp, "dnd"))
                    {
                        PrefParse (tmp);
                        s_repl (&prG->auto_dnd, ConvToUTF8 (tmp, enc));
                    }
                    else if (!strcasecmp (tmp, "occ"))
                    {
                        PrefParse (tmp);
                        s_repl (&prG->auto_occ, ConvToUTF8 (tmp, enc));
                    }
                    else if (!strcasecmp (tmp, "inv"))
                    {
                        PrefParse (tmp);
                        s_repl (&prG->auto_inv, ConvToUTF8 (tmp, enc));
                    }
                    else if (!strcasecmp (tmp, "ffc"))
                    {
                        PrefParse (tmp);
                        s_repl (&prG->auto_ffc, ConvToUTF8 (tmp, enc));
                    }
                    else
                        ERROR;
                }
                else if (!strcasecmp (cmd, "sound"))
                {
                    if (!s_parse (&args, &tmp))
                    {
                        prG->sound = SFLAG_BEEP;
                        dep |= 1;
                        continue;
                    }
                    if (!strcasecmp (tmp, "on") || !strcasecmp (tmp, "beep"))
                        prG->sound = SFLAG_BEEP;
                    else if (!strcasecmp (tmp, "event"))
                        prG->sound = SFLAG_EVENT;
                    else
                        prG->sound = 0;
                }
                else if (!strcasecmp (cmd, "soundonline"))
                    dep |= 1;
                else if (!strcasecmp (cmd, "soundoffline"))
                    dep |= 1;
                else if (!strcasecmp (cmd, "auto_away"))
                {
                    PrefParseInt (i);
                    prG->away_time = i;
                }
                else if (!strcasecmp (cmd, "screen_width"))
                {
                    PrefParseInt (i);
                    prG->screen = i;
                }
                else if (!strcasecmp (cmd, "tab"))
                {
                    PrefParse (tmp);
                    if (spooled_tab_nicks < TAB_SLOTS)
                        tab_nick_spool[spooled_tab_nicks++] = strdup (ConvToUTF8 (tmp, enc));
                }
                else if (!strcasecmp (cmd, "set"))
                {
                    int which = 0;
                    
                    PrefParse (cmd);

                    if (!strcasecmp (cmd, "color") || !strcasecmp (cmd, "colour"))
                        which = FLAG_COLOR;
                    else if (!strcasecmp (cmd, "hermit"))
                        which = FLAG_HERMIT;
                    else if (!strcasecmp (cmd, "delbs"))
                        which = FLAG_DELBS;
                    else if (!strcasecmp (cmd, "russian"))
                        which = FLAG_CONVRUSS;
                    else if (!strcasecmp (cmd, "japanese"))
                        which = FLAG_CONVEUC;
                    else if (!strcasecmp (cmd, "funny"))
                        which = FLAG_FUNNY;
                    else if (!strcasecmp (cmd, "log"))
                        which = FLAG_LOG;
                    else if (!strcasecmp (cmd, "loglevel"))
                        which = -1;
                    else if (!strcasecmp (cmd, "logonoff"))
                        which = FLAG_LOG_ONOFF;
                    else if (!strcasecmp (cmd, "auto"))
                        which = FLAG_AUTOREPLY;
                    else if (!strcasecmp (cmd, "uinprompt"))
                        which = FLAG_UINPROMPT;
                    else if (!strcasecmp (cmd, "autosave"))
                        which = FLAG_AUTOSAVE;
                    else if (!strcasecmp (cmd, "autofinger"))
                        which = FLAG_AUTOFINGER;
                    else if (!strcasecmp (cmd, "linebreak"))
                        which = -2;
                    else if (!strcasecmp (cmd, "tabs"))
                        which = -3;
                    else if (!strcasecmp (cmd, "silent"))
                        which = -4;
                    else
                        ERROR;
                    if (which > 0)
                    {
                        PrefParse (cmd);
                        if (!strcasecmp (cmd, "on"))
                        {
                            if (which == FLAG_CONVRUSS)
                            {
                                dep = 1;
                                prG->enc_rem = ENC_WIN1251;
                            }
                            else if (which == FLAG_CONVEUC)
                            {
                                dep = 1;
                                prG->enc_rem = ENC_SJIS;
#ifndef ENABLE_ICONV
                                if (prG->enc_loc == ENC_UTF8)
                                {
                                    M_printf (COLERROR "%s" COLNONE " ", i18n (1619, "Warning:"));
                                    M_print (i18n (2215, "This mICQ can't convert between SJIS or EUC and unicode.\n"));
                                }
#endif
                            }
                            prG->flags |= which;
                        }
                        else if (!strcasecmp (cmd, "off"))
                            prG->flags &= ~which;
                        else
                            ERROR;
                    }
                    else if (which == -1)
                    {
                        PrefParseInt (i);

                        prG->flags &= ~FLAG_LOG & ~FLAG_LOG_ONOFF;
                        if (i)
                            prG->flags |= FLAG_LOG;
                        if (i & 2)
                            prG->flags |= FLAG_LOG_ONOFF;
                        dep = 1;
                    }
                    else if (which == -2)
                    {
                        PrefParse (cmd);

                        prG->flags &= ~FLAG_LIBR_BR & ~FLAG_LIBR_INT;
                        if (!strcasecmp (cmd, "break"))
                            prG->flags |= FLAG_LIBR_BR;
                        else if (!strcasecmp (cmd, "simple"))
                            ;
                        else if (!strcasecmp (cmd, "indent"))
                            prG->flags |= FLAG_LIBR_INT;
                        else if (!strcasecmp (cmd, "smart"))
                            prG->flags |= FLAG_LIBR_BR | FLAG_LIBR_INT;
                        else
                            ERROR;
                    }
                    else if (which == -3)
                    {
                        PrefParse (cmd);

                        prG->tabs = TABS_SIMPLE;
                        if (!strcasecmp (cmd, "cycle"))
                            prG->tabs = TABS_CYCLE;
                        else if (!strcasecmp (cmd, "cycleall"))
                            prG->tabs = TABS_CYCLEALL;
                        else if (strcasecmp (cmd, "simple"))
                            dep = 1;
                    }
                    else if (which == -4)
                    {
                        PrefParse (cmd);
                        
                        prG->flags &= ~FLAG_QUIET & ~FLAG_ULTRAQUIET;
                        if (!strcasecmp (cmd, "on"))
                            prG->flags |= FLAG_QUIET;
                        else if (!strcasecmp (cmd, "complete"))
                            prG->flags |= FLAG_QUIET | FLAG_ULTRAQUIET;
                        else if (strcasecmp (cmd, "off"))
                            dep = 1;
                    }
                }
                else
                {
                    M_printf (COLERROR "%s" COLNONE " ", i18n (1619, "Warning:"));
                    M_printf (i18n (1188, "Unrecognized command in rc file '%s', ignored."), ConvToUTF8 (cmd, enc));
                    M_print ("\n");
                }
                break;
            case 1:
                flags = 0;
                
                for (p = buf; *p && *p != '#'; p++)
                {
                    if (!*p || *p == '#' || isdigit ((int) *p))
                        break;

                    switch (*p)
                    {
                        case '*':
                            flags |= CONT_INTIMATE;
                            flags &= ~CONT_HIDEFROM;
                            continue;
                        case '^':
                            flags |= CONT_IGNORE;
                            continue;
                        case '~':
                            flags |= CONT_HIDEFROM;
                            flags &= ~CONT_INTIMATE;
                            continue;
                        case ' ':
                        case '-':
                            continue;
                    }
                    break;
                }

                if (!*p || *p == '#' )
                    continue;
                
                args = p;

                if (isdigit (*p))
                {
                    PrefParseInt (uin);
                    PrefParse (cmd);
                }
                else
                {
                    if (!lastcont)     /* ignore noise */
                        continue;
                    uin = lastcont->uin;
                    PrefParse (cmd);
                }
                
                
                if (!(cont = ContactFind (NULL, 0, uin, ConvToUTF8 (cmd, enc), 1)))
                {
                    M_printf (COLERROR "%s" COLNONE " %s\n", i18n (1619, "Warning:"),
                             i18n (1620, "maximal number of contacts reached. Ask a wizard to enlarge me!"));
                    section = -1;
                    break;
                }
                if (~cont->flags & CONT_ALIAS)
                    cont->flags = flags;
                if (prG->verbose > 2)
                    M_printf ("%ld = %s %lx | %p\n", cont->uin, cont->nick, cont->flags, cont);
                break;
            case 2:
                PrefParse (cmd);

                if (!strcasecmp (cmd, "alter"))
                {
                    PrefParse (tmp);
                    tmp = strdup (s_quote (ConvToUTF8 (tmp, enc)));
                    PrefParse (tmp2);
                    tmp2 = strdup (s_quote (ConvToUTF8 (tmp2, enc)));
                    
                    CmdUser (cmd = strdup (s_sprintf ("\xb6" "alter quiet %s %s", tmp, tmp2)));

                    free (cmd);
                    free (tmp);
                    free (tmp2);
                }
                else if (!strcasecmp (cmd, "alias"))
                {
                    PrefParse (tmp);
                    tmp = strdup (s_quote (ConvToUTF8 (tmp, enc)));
                    PrefParse (tmp2);
                    tmp2 = strdup (ConvToUTF8 (tmp2, enc));

                    CmdUser (cmd = strdup (s_sprintf ("\xb6" "alias %s %s", tmp, tmp2)));
                    
                    free (cmd);
                    free (tmp);
                    free (tmp2);
                }
                else
                {
                    M_printf (COLERROR "%s" COLNONE " ", i18n (1619, "Warning:"));
                    M_printf (i18n (1188, "Unrecognized command in rc file '%s', ignored."), ConvToUTF8 (cmd, enc));
                    M_print ("\n");
                }
                break;
            case 3:
                PrefParse (cmd);

                if (!strcasecmp (cmd, "type"))
                {
                    PrefParse (cmd);

                    if (!strcasecmp (cmd, "server"))
                    {
                        conn->spref->type =
                            (conn->spref->version ? (conn->spref->version > 6 
                               ? TYPE_SERVER : TYPE_SERVER_OLD) : conn->spref->type);
                        conn->spref->flags = 0;
                        dep |= 1;
                    }
                    else if (!strcasecmp (cmd, "icq8"))
                    {
                        conn->spref->type = TYPE_SERVER;
                        conn->spref->flags = 0;
                    }
                    else if (!strcasecmp (cmd, "icq5"))
                    {
                        conn->spref->type = TYPE_SERVER_OLD;
                        conn->spref->flags = 0;
                    }
                    else if (!strcasecmp (cmd, "peer"))
                    {
                        conn->spref->type = TYPE_MSGLISTEN;
                        conn->spref->flags = 0;
                        if (oldconn->spref->type == TYPE_SERVER || oldconn->spref->type == TYPE_SERVER_OLD)
                        {
                            oldconn->assoc = conn;
                            conn->parent = oldconn;
                        }
                        conn->spref->status = TCP_OK_FLAG;
                    }
                    else if (!strcasecmp (cmd, "remote"))
                    {
                        conn->spref->type = TYPE_REMOTE;
                        conn->spref->flags = 0;
                    }
                    else
                        ERROR;
                    if (s_parse (&args, &cmd))
                    {
                        if (!strcasecmp (cmd, "auto"))
                            conn->spref->flags |= CONN_AUTOLOGIN;
                    }
                }
                else if (!strcasecmp (cmd, "version"))
                {
                    PrefParseInt (i);

                    conn->spref->version = i;
                    if (!conn->spref->type)
                    {
                        if (conn->spref->version > 6)
                            conn->spref->type = TYPE_SERVER;
                        else
                            conn->spref->type = TYPE_SERVER_OLD;
                    }
                }
                else if (!strcasecmp (cmd, "server"))
                {
                    PrefParse (tmp);
                    s_repl (&conn->spref->server, tmp);
                }
                else if (!strcasecmp (cmd, "port"))
                {
                    PrefParseInt (i);
                    conn->spref->port = i;
                }
                else if (!strcasecmp (cmd, "uin"))
                {
                    PrefParseInt (i);
                    conn->spref->uin = i;
                }
                else if (!strcasecmp (cmd, "password"))
                {
                    PrefParse (tmp);
                    s_repl (&conn->spref->passwd, ConvToUTF8 (tmp, enc));
                }
                else if (!strcasecmp (cmd, "status"))
                {
                    PrefParseInt (i);
                    conn->spref->status = i;
                }
                else
                {
                    M_printf (COLERROR "%s" COLNONE " ", i18n (1619, "Warning:"));
                    M_printf (i18n (1188, "Unrecognized command in rc file '%s', ignored."), cmd);
                    M_print ("\n");
                }
                break;
            case 4: /* contact groups */
                PrefParse (cmd);
                if (!strcasecmp (cmd, "label") && !cg->used)
                {
                    PrefParse (tmp);
                    s_repl (&cg->name, ConvToUTF8 (tmp, enc));
                    if (!strncmp (cg->name, "contacts-", 9))
                    {
                        UWORD type = 0;
                        UDWORD uin = 0;
                        
                        if      (!strncmp (cg->name + 9, "icq5-", 5)) type = TYPE_SERVER_OLD;
                        else if (!strncmp (cg->name + 9, "icq8-", 5)) type = TYPE_SERVER;
                        uin = atoi (cg->name + 14);
                        
                        for (i = 0; (conn = ConnectionNr (i)); i++)
                            if (conn->spref && conn->spref->type == type && conn->spref->uin == uin)
                            {
                                cg->serv = conn;
                                cg->serv->contacts = cg;
                                break;
                            }
                    }
                    else if (!cg->serv)
                    {
                        for (i = 0; (conn = ConnectionNr (i)); i++)
                            if (conn->spref && conn->spref->type & TYPEF_SERVER)
                            {
                                cg->serv = conn;
                                break;
                            }
                    }
                }
                else if (!strcasecmp (cmd, "id") && !cg->used)
                {
                    PrefParseInt (i);
                    cg->id = i;
                }
                else if (!strcasecmp (cmd, "server") && !cg->used)
                {
                    UWORD type = 0;
                    UDWORD uin = 0;
                    
                    PrefParse (tmp);
                    if      (!strcasecmp (tmp, "icq5")) type = TYPE_SERVER_OLD;
                    else if (!strcasecmp (tmp, "icq8")) type = TYPE_SERVER;
                    
                    PrefParseInt (uin);
                    
                    for (i = 0; (conn = ConnectionNr (i)); i++)
                        if (conn->type == type && conn->uin == uin)
                        {
                            if (cg->serv->contacts == cg)
                                cg->serv->contacts = NULL;
                            cg->serv = conn;
                            cg->serv->contacts = cg;
                            break;
                        }
                }
                else if (!strcasecmp (cmd, "entry"))
                {
                    UDWORD uin;
                    
                    PrefParseInt (i);
                    PrefParseInt (uin);
                    
                    ContactFind (cg, i, uin, NULL, 1);
                }
                else
                {
                    M_printf (COLERROR "%s" COLNONE " ", i18n (1619, "Warning:"));
                    M_printf (i18n (1188, "Unrecognized command in rc file '%s', ignored."), ConvToUTF8 (cmd, enc));
                    M_print ("\n");
                }
                break;
        }
    }
    
    /* now tab the nicks we may have spooled earlier */
    for (i = 0; i < spooled_tab_nicks; i++)
    {
        Contact *cont = ContactFind (NULL, 0, 0, tab_nick_spool[i], 1);
        if (cont)
            TabAddUIN (cont->uin);
        free (tab_nick_spool[i]);
    }

    if (!prG->auto_dnd)
        prG->auto_dnd  = strdup (i18n (1929, "User is dnd [Auto-Message]"));
    if (!prG->auto_away)
        prG->auto_away = strdup (i18n (1010, "User is away [Auto-Message]"));
    if (!prG->auto_na)
        prG->auto_na   = strdup (i18n (1011, "User is not available [Auto-Message]"));
    if (!prG->auto_occ)
        prG->auto_occ  = strdup (i18n (1012, "User is occupied [Auto-Message]"));
    if (!prG->auto_inv)
        prG->auto_inv  = strdup (i18n (1013, "User is offline"));
    if (!prG->auto_ffc)
        prG->auto_ffc  = strdup (i18n (2055, "User is ffc and wants to chat about everything."));

    if (prG->flags & FLAG_LOG && !prG->logplace)
        prG->logplace = strdup ("history" _OS_PATHSEPSTR);
    
    if (!prG->chat)
        prG->chat = 49;

    for (i = 0; (conn = ConnectionNr (i)); i++)
    {
        assert (conn->spref);

        conn->port   = conn->spref->port;
        s_repl (&conn->server, conn->spref->server);
        s_repl (&conn->passwd, conn->spref->passwd);
        conn->status = conn->spref->status;
        conn->uin    = conn->spref->uin;
        conn->ver    = conn->spref->version;
        conn->type   = conn->spref->type;
        conn->flags  = conn->spref->flags;
        if (prG->s5Use && conn->type & TYPEF_SERVER && !conn->ver)
            conn->ver = 2;
        switch (conn->type)
        {
            case TYPE_SERVER:
                conn->open = &ConnectionInitServer;
                break;
            case TYPE_SERVER_OLD:
                conn->open = &ConnectionInitServerV5;
                break;
#ifdef ENABLE_PEER2PEER
            case TYPE_MSGLISTEN:
                conn->open = &ConnectionInitPeer;
                break;
#endif
#ifdef ENABLE_REMOTECONTROL
            case TYPE_REMOTE:
                conn->open = &RemoteOpen;
                conns = conn;
                break;
#endif
            default:
                conn->open = NULL;
                break;
        }
        if (!conn->contacts && conn->type & TYPEF_SERVER)
        {
            conn->contacts = cg = ContactGroupFind (0, conn, s_sprintf ("contacts-%s-%ld",
                                conn->type == TYPE_SERVER ? "icq8" : "icq5", conn->uin), 1);
            for (i = 0; (cont = ContactIndex (NULL, i)); i++)
                ContactFind (cg, 0, cont->uin, cont->nick, 1);
            dep = 1;
        }
    }

#ifdef ENABLE_REMOTECONTROL
    if (!conns)
    {
        conns = ConnectionC ();
        conns->open = &RemoteOpen;
        conns->spref = PreferencesConnectionC ();
        conns->spref->server = strdup ("remote-control");
        conns->parent = NULL;
        conns->spref->type = TYPE_REMOTE;
        conns->type  = conns->spref->type;
        conns->server = strdup (conns->spref->server);
        dep = 1;
    }
#endif
                           
    if (dep || !format)
        M_print (i18n (1818, "Warning: Deprecated syntax found in rc file!\n    Please update or \"save\" the rc file and check for changes.\n"));
    fclose (rcf);
}

/************************************************
 *   This function should save your auto reply messages in the rc file.
 *   NOTE: the code isn't really neat yet, I hope to change that soon.
 *   Added on 6-20-98 by Fryslan
 ***********************************************/
int Save_RC ()
{
    FILE *rcf;
    time_t t;
    int i, k, l;
    Contact *cont;
    Connection *ss;
    ContactGroup *cg;

    M_printf (i18n (2048, "Saving preferences to %s.\n"), prG->rcfile);
    rcf = fopen (prG->rcfile, "w");
    if (!rcf)
    {
        int rc = errno;
        if (rc == ENOENT)
        {
            char *tmp = strdup (PrefUserDir (prG));
            if (tmp[strlen (tmp) - 1] == '/')
                tmp[strlen (tmp) - 1] = '\0';
            M_printf (i18n (2047, "Creating directory %s.\n"), tmp);
            k = mkdir (tmp, 0700);
            if (!k)
                rcf = fopen (prG->rcfile, "w");
            if (!rcf)
                rc = errno;
        }
        if (!rcf)
            M_printf (i18n (1872, "failed: %s (%d)\n"), strerror (rc), rc);
    }
    if (!rcf)
        return -1;
    fprintf (rcf, "# This file was generated by mICQ " MICQ_VERSION " of %s %s\n", __TIME__, __DATE__);
    t = time (NULL);
    fprintf (rcf, "# This file was generated on %s\n\n", ctime (&t));
    
    fprintf (rcf, "# Character encodings: auto, iso-8859-1, koi8-u, ...\n");
#ifdef ENABLE_UTF8
    fprintf (rcf, "encoding file utf-8\n");
#else
    fprintf (rcf, "encoding file %s\n", s_quote (ConvEncName (prG->enc_loc)));
#endif
    fprintf (rcf, "%sencoding local %s%s\n",
             prG->enc_loc & ENC_AUTO ? "#" : "", s_quote (ConvEncName (prG->enc_loc)),
             prG->enc_loc & ENC_AUTO ? "" : " # please set your locale correctly instead");
    fprintf (rcf, "%sencoding remote %s\n",
             prG->enc_rem & ENC_AUTO ? "#" : "", s_quote (ConvEncName (prG->enc_rem)));
    fprintf (rcf, "format 1\n\n");

    for (k = 0; (ss = ConnectionNr (k)); k++)
    {
        if (!ss->spref || (!ss->spref->uin && ss->spref->type == TYPE_SERVER)
            || (ss->spref->type != TYPE_SERVER && ss->spref->type != TYPE_SERVER_OLD
                && ss->spref->type != TYPE_MSGLISTEN && ss->spref->type != TYPE_REMOTE)
            || (ss->spref->type == TYPE_MSGLISTEN && ss->parent && !ss->parent->spref->uin))
            continue;

        fprintf (rcf, "[Connection]\n");
        fprintf (rcf, "type %s%s\n",  ss->spref->type == TYPE_SERVER     ? "icq8" :
                                      ss->spref->type == TYPE_SERVER_OLD ? "icq5" :
                                      ss->spref->type == TYPE_MSGLISTEN  ? "peer" : "remote",
                                      ss->spref->flags & CONN_AUTOLOGIN ? " auto" : "");
        fprintf (rcf, "version %d\n", ss->spref->version);
        if (ss->spref->server)
            fprintf (rcf, "server %s\n",   s_quote (ss->spref->server));
        if (ss->spref->port)
            fprintf (rcf, "port %ld\n",    ss->spref->port);
        if (ss->spref->uin)
            fprintf (rcf, "uin %ld\n",     ss->spref->uin);
        if (ss->spref->passwd)
            fprintf (rcf, "password %s\n", s_quote (ss->spref->passwd));
        else if (!k)
            fprintf (rcf, "# password\n");
        if (ss->spref->status || !k)
            fprintf (rcf, "status %ld\n",  ss->spref->status);
        fprintf (rcf, "\n");
    }

    fprintf (rcf, "\n[General]\n# Support for SOCKS5 server\n");
    fprintf (rcf, "s5_use %d\n", prG->s5Use);
    if (!prG->s5Host)
        fprintf (rcf, "s5_host \"[none]\"\n");
    else
        fprintf (rcf, "s5_host %s\n", s_quote (prG->s5Host));
    fprintf (rcf, "s5_port %d\n", prG->s5Port);
    fprintf (rcf, "# If you need authentication, put 1 for s5_auth and fill your name/password\n");
    fprintf (rcf, "s5_auth %d\n", prG->s5Auth);
    if (!prG->s5Name)
        fprintf (rcf, "s5_name \"[none]\"\n");
    else
        fprintf (rcf, "s5_name %s\n", s_quote (prG->s5Name));
    if (!prG->s5Pass)
        fprintf (rcf, "s5_pass \"[none]\"\n");
    else
        fprintf (rcf, "s5_pass %s\n", s_quote (prG->s5Pass));

    fprintf (rcf, "\n#in seconds\nauto_away %ld\n", prG->away_time);
    fprintf (rcf, "\n#For dumb terminals that don't wrap set this.");
    fprintf (rcf, "\nScreen_Width %d\n", prG->screen);
    fprintf (rcf, "verbose %ld\n\n", prG->verbose);


    fprintf (rcf, "# Set some simple options.\n");
    fprintf (rcf, "set delbs      %s # if a DEL char is supposed to be backspace\n",
                    prG->flags & FLAG_DELBS     ? "on " : "off");
    fprintf (rcf, "set funny      %s # if you want funny messages\n",
                    prG->flags & FLAG_FUNNY     ? "on " : "off");
    fprintf (rcf, "set color      %s # if you want colored messages\n",
                    prG->flags & FLAG_COLOR     ? "on " : "off");
    fprintf (rcf, "set hermit     %s # if you want messages from people on your contact list ONLY\n",
                    prG->flags & FLAG_HERMIT    ? "on " : "off");
    fprintf (rcf, "set log        %s # if you want to log messages\n",
                    prG->flags & FLAG_LOG       ? "on " : "off");
    fprintf (rcf, "set logonoff   %s # if you also want to log online/offline events\n",
                    prG->flags & FLAG_LOG_ONOFF ? "on " : "off");
    fprintf (rcf, "set auto       %s # if automatic responses are to be sent\n",
                    prG->flags & FLAG_AUTOREPLY ? "on " : "off");
    fprintf (rcf, "set uinprompt  %s # if the prompt should contain the last uin a message was received from\n",
                    prG->flags & FLAG_UINPROMPT ? "on " : "off");
    fprintf (rcf, "set autosave   %s # whether the micqrc should be automatically saved on exit\n",
                    prG->flags & FLAG_AUTOSAVE ? "on " : "off");
    fprintf (rcf, "set autofinger %s # whether new UINs should be fingered automatically\n",
                    prG->flags & FLAG_AUTOFINGER ? "on " : "off");
    fprintf (rcf, "set linebreak  %s # the line break type to be used (simple, break, indent, smart)\n",
                    prG->flags & FLAG_LIBR_INT 
                    ? prG->flags & FLAG_LIBR_BR ? "smart " : "indent"
                    : prG->flags & FLAG_LIBR_BR ? "break " : "simple");
    fprintf (rcf, "set silent     %s # what stuff to hide (on, off, complete)\n",
                    prG->flags & FLAG_ULTRAQUIET ? "complete" :
                    prG->flags & FLAG_QUIET      ? "on" : "off");
    fprintf (rcf, "set tabs       %s # type of tab completion (simple, cycle, cycleall)\n\n",
                    prG->tabs == TABS_SIMPLE ? "simple" :
                    prG->tabs == TABS_CYCLE ? "cycle" : "cycleall");

    fprintf (rcf, "# Colors. color scheme 0|1|2|3 or color <use> <color>\n");
    {
        char *t, *c;

        for (i = 0; i < CXCOUNT; i++)
        {
            switch (i)
            {
                case 0: c = "none    "; break;
                case 1: c = "server  "; break;
                case 2: c = "client  "; break;
                case 3: c = "message "; break;
                case 4: c = "contact "; break;
                case 5: c = "sent    "; break;
                case 6: c = "ack     "; break;
                case 7: c = "error   "; break;
                case 8: c = "incoming"; break;
                case 9: c = "debug   "; break;
                default: c = ""; assert (0);
            }
            fprintf (rcf, "color %s", c);
            for (t = prG->colors[i]; *t; t += l)
            {
                if      (!strncmp (BLACK,   t, l = strlen (BLACK)))   c = "black";
                else if (!strncmp (RED,     t, l = strlen (RED)))     c = "red";
                else if (!strncmp (GREEN,   t, l = strlen (GREEN)))   c = "green";
                else if (!strncmp (YELLOW,  t, l = strlen (YELLOW)))  c = "yellow";
                else if (!strncmp (BLUE,    t, l = strlen (BLUE)))    c = "blue";
                else if (!strncmp (MAGENTA, t, l = strlen (MAGENTA))) c = "magenta";
                else if (!strncmp (CYAN,    t, l = strlen (CYAN)))    c = "cyan";
                else if (!strncmp (WHITE,   t, l = strlen (WHITE)))   c = "white";
                else if (!strncmp (SGR0,    t, l = strlen (SGR0)))    c = "none";
                else if (!strncmp (BOLD,    t, l = strlen (BOLD)))    c = "bold";
                else c = t, l = strlen (t);
                fprintf (rcf, " %s", s_quote (c));
            }
            fprintf (rcf, "\n");
        }
    }
    if (prG->scheme != (UBYTE)-1)
        fprintf (rcf, "color scheme   %d\n\n", prG->scheme);
    else
        fprintf (rcf, "# color scheme <0,1,2,3>\n\n");
    
    fprintf (rcf, "chat %d          # random chat group; -1 to disable, 49 for mICQ\n\n",
                  prG->chat);

    fprintf (rcf, "logplace %s      # the file or (distinct files in) dir to log to\n\n",
                  prG->logplace ? s_quote (prG->logplace) : "");

    fprintf (rcf, "sound %s # on=beep,off,event\n",
                  prG->sound & SFLAG_BEEP  ? "beep " :
                  prG->sound & SFLAG_EVENT ? "event" : "off  ");

    fprintf (rcf, "event %s\n\n", prG->event_cmd && *prG->event_cmd ? s_quote (prG->event_cmd) : "off");

    fprintf (rcf, "\n# automatic responses\n");
    fprintf (rcf, "auto away %s\n", s_quote (prG->auto_away));
    fprintf (rcf, "auto na   %s\n", s_quote (prG->auto_na));
    fprintf (rcf, "auto dnd  %s\n", s_quote (prG->auto_dnd));
    fprintf (rcf, "auto occ  %s\n", s_quote (prG->auto_occ));
    fprintf (rcf, "auto inv  %s\n", s_quote (prG->auto_inv));
    fprintf (rcf, "auto ffc  %s\n", s_quote (prG->auto_ffc));

    fprintf (rcf, "\n# The strings section - runtime redefinable strings.\n");
    fprintf (rcf, "# The alter command redefines command names.\n");
    fprintf (rcf, "[Strings]\n");
    {
        jump_t *f;
        for (f = CmdUserTable (); f->f; f++)
            if (f->name && strcmp (f->name, f->defname))
                fprintf (rcf, "alter %s %s\n", f->defname, s_quote (f->name));
    }
    {
        alias_t *node;
        for (node = CmdUserAliases (); node; node = node->next)
        {
            fprintf (rcf, "alias %s", s_quote (node->name));
            fprintf (rcf, " %s\n", s_quote (node->expansion));
        }
    }

    fprintf (rcf, "\n# Contact groups.");
    for (k = 0; (cg = ContactGroupIndex (k)); k++)
    {
        fprintf (rcf, "\n[Group]\n");
        if (cg->serv)
            fprintf (rcf, "server %s %ld\n", cg->serv->type == TYPE_SERVER ? "icq8" : "icq5", cg->serv->uin);
        else
            fprintf (rcf, "#server <icq5|icq8> <uin>\n");
        fprintf (rcf, "label %s\n", s_quote (cg->name));
        fprintf (rcf, "id %d\n", cg->id);
        while (cg)
        {
            for (i = 0; i < cg->used; i++)
                fprintf (rcf, "entry 0 %ld\n", cg->contacts[i]->uin);
            cg = cg->more;
        }
    }

    fprintf (rcf, "\n# The contact list section.\n");
    fprintf (rcf, "#  Use * in front of the number of anyone you want to see you while you're invisible.\n");
    fprintf (rcf, "#  Use ~ in front of the number of anyone you want to always see you as offline.\n");
    fprintf (rcf, "#  Use ^ in front of the number of anyone you want to ignore.\n");
    fprintf (rcf, "[Contacts]\n");

    for (i = 0; (cont = ContactIndex (0, i)); i++)
    {
        if (!(cont->flags & (CONT_TEMPORARY | CONT_ALIAS)))
        {
            Contact *cont2;
            if (cont->flags & CONT_INTIMATE) fprintf (rcf, "*"); else fprintf (rcf, " ");
            if (cont->flags & CONT_HIDEFROM) fprintf (rcf, "~"); else fprintf (rcf, " ");
            if (cont->flags & CONT_IGNORE)   fprintf (rcf, "^"); else fprintf (rcf, " ");
            fprintf (rcf, "%9ld %s\n", cont->uin, s_quote (cont->nick));
            for (cont2 = cont->alias; cont2; cont2 = cont2->alias)
                fprintf (rcf, "   %9ld %s\n", cont->uin, s_quote (cont2->nick));
        }
    }
    fprintf (rcf, "\n");
    return fclose (rcf) ? -1 : 0;
}

/*
 * Writes a hex dump of buf to a file.
 */
void fHexDump (FILE *f, void *buffer, size_t len)
{
    int i, j;
    unsigned char *buf = buffer;

    if (!len)
        return;

    assert (len > 0);

    for (i = 0; i < ((len + 15) & ~15); i++)
    {
        if (i < len)
            fprintf (f, "%02x ", buf[i]);
        else
            fprintf (f, "   ");
        if ((i & 15) == 15)
        {
            fprintf (f, "  ");
            for (j = 15; j >= 0; j--)
            {
                if (i - j >= len)
                    break;
                if ((buf[i - j] & 0x7f) > 31)
                    fprintf (f, "%c", buf[i - j]);
                else
                    fprintf (f, ".");
                if (((i - j) & 3) == 3)
                    fprintf (f, " ");
            }
            fprintf (f, "\n");
            if (i > len)
                return;
        }
        else if (i < len && (i & 7) == 7)
            fprintf (f, "- ");
        else if ((i & 3) == 3)
            fprintf (f, "  ");
    }
}

