/* $Id$ */
/* Copyright: This file may be distributed under version 2 of the GPL licence. */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "micq.h"
#include "buildmark.h"
#include "util_ui.h"
#include "file_util.h"
#include "tabs.h"
#include "contact.h"
#include "tcp.h"
#include "util.h"
#include "cmd_user.h"
#include "cmd_pkt_cmd_v5.h"
#include "preferences.h"
#include "util_io.h"
#include "cmd_pkt_v8.h"
#include "session.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef _WIN32
#include <io.h>
#define S_IRUSR        _S_IREAD
#define S_IWUSR        _S_IWRITE
#else
#include <netinet/in.h>
#include <termios.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#endif

/****/


/************************************************************************
 Copies src string into dest.  If src is NULL dest becomes ""
*************************************************************************/
/*
static void M_strcpy (char *dest, char *src)
{
    if (src)
        strcpy (dest, src);
    else
        *dest = '\0';
}
*/

void Initalize_RC_File ()
{
    char pwd1[20], pwd2[20], input[200];
    Session *sess, *sesst;
    char *passwd, *t;
    UDWORD uin;
    
    prG->away_time = default_away_time;

    M_print ("\n");
    M_print (i18n (1793, "No valid UIN found. The setup wizard will guide you through the process of setting one up.\n"));
    M_print (i18n (1794, "If you already have an UIN, please enter it. Otherwise, enter 0, and I will request one for you.\n"));
    M_print ("%s ", i18n (1618, "UIN:"));
    fflush (stdout);
    M_fdnreadln (stdin, input, sizeof (input));
    uin = 0;
    sscanf (input, "%ld", &uin);

    M_print ("\n");
    if (uin)
        M_print (i18n (1781, "Your password for UIN %d:\n"), uin);
    else
        M_print (i18n (1782, "You need a password for your new UIN.\n"));
    memset (pwd1, 0, sizeof (pwd1));
    while (!pwd1[0])
    {
        M_print ("%s ", i18n (1795, "Password:"));
        fflush (stdout);
        Echo_Off ();
        M_fdnreadln (stdin, pwd1, sizeof (pwd1));
        Echo_On ();
        M_print ("\n");
        if (uin)
            continue;

        M_print (i18n (1783, "To prevent typos, please enter your password again.\n"));
        M_print ("%s ", i18n (1795, "Password:"));
        fflush (stdout);
        memset (pwd2, 0, sizeof (pwd2));
        Echo_Off ();
        M_fdnreadln (stdin, pwd2, sizeof (pwd2));
        Echo_On ();
        M_print ("\n");
        if (strcmp (pwd1, pwd2))
        {
            M_print ("\n%s\n", i18n (1093, "Passwords did not match - please try again."));
            pwd1[0] = '\0';
        }
    }
    passwd = pwd1;

    prG->s5Use = 0;
    prG->s5Port = 0;

    M_print ("\n");
    M_print (i18n (1784, "If you are firewalled, you may need to use a SOCKS5 server. If you do, please enter its hostname or IP address. Otherwise, or if unsure, just press return.\n"));
    M_print ("%s ", i18n (1094, "SOCKS5 server:"));
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
            M_print ("%s ", i18n (1095, "SOCKS5 port:"));
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
        M_print (i18n (1787, "You probably need to authentificate yourself to the socks server. If so, you need to enter the user name the administrator of the socks server gave you. Otherwise, just press return.\n"));
        M_print ("%s ", i18n (1096, "SOCKS5 user name:"));
        fflush (stdout);
        M_fdnreadln (stdin, input, sizeof (input));
        if (strlen (input) > 1)
        {
            prG->s5Auth = 1;
            prG->s5Name = strdup (input);
            M_print (i18n (1788, "Now I also need the password for this user.\n"));
            M_print ("%s ", i18n (1097, "SOCKS5 password:"));
            fflush (stdout);
            M_fdnreadln (stdin, input, sizeof (input));
            prG->s5Pass = strdup (input);
        }
    }

    M_print ("\n");

    if (!uin)
    {
        M_print (i18n (1796, "Setup wizard finished. Please wait until registration has finished.\n"));
        sess = SrvRegisterUIN (NULL, pwd1);
        sess->flags = CONN_WIZARD;
    }
    else
    {
        M_print (i18n (1791, "Setup wizard finished. Congratulations!\n"));
        sess = SessionC ();
        assert (sess);
        sess->spref = PreferencesSessionC ();
        assert (sess->spref);
        
        sess->spref->type = TYPE_SERVER;
        sess->spref->flags = CONN_AUTOLOGIN | CONN_WIZARD;
        sess->spref->server = strdup ("login.icq.com");
        sess->spref->port = 5190;
        sess->spref->status = STATUS_ONLINE;
        sess->spref->version = 8;
        sess->spref->uin = uin;
#ifdef __BEOS__
        sess->spref->passwd = strdup (passwd);
#endif
        
        sess->server  = strdup ("login.icq.com");
        sess->port    = 5190;
        sess->type    = TYPE_SERVER;
        sess->flags   = CONN_AUTOLOGIN | CONN_WIZARD;
        sess->ver     = 8;
        sess->uin     = uin;
        sess->passwd  = strdup (passwd);
    }
    
    sesst = SessionC ();
    assert (sesst);
    sesst->spref = PreferencesSessionC ();
    assert (sesst->spref);

    sesst->parent = sess;
    sess->assoc = sesst;
    sesst->spref->type = TYPE_MSGLISTEN;
    sesst->spref->flags = CONN_AUTOLOGIN;
    sesst->type = sesst->spref->type;
    sesst->flags = sesst->spref->flags;
    sesst->spref->version = 8;
    sesst->ver = 8;
    sesst->status = prG->s5Use ? 2 : TCP_OK_FLAG;

    prG->status = STATUS_ONLINE;
    prG->tabs = TABS_SIMPLE;
#ifdef ANSI_TERM
    prG->flags = FLAG_COLOR | FLAG_LOG | FLAG_LOG_ONOFF | FLAG_DELBS;
#else
    prG->flags =              FLAG_LOG | FLAG_LOG_ONOFF | FLAG_DELBS;
#endif
    prG->auto_dnd  = strdup (i18n (1929, "User is dnd [Auto-Message]"));
    prG->auto_away = strdup (i18n (1010, "User is away [Auto-Message]"));
    prG->auto_na   = strdup (i18n (1011, "User is not available [Auto-Message]"));
    prG->auto_occ  = strdup (i18n (1012, "User is occupied [Auto-Message]"));
    prG->auto_inv  = strdup (i18n (1013, "User is offline"));
    prG->auto_ffc  = strdup (i18n (2055, "User is ffc and wants to chat about everything."));
    prG->logplace = malloc (strlen (PrefUserDir ()) + 10);
    assert (prG->logplace);
    strcpy (prG->logplace, PrefUserDir ());
    strcat (prG->logplace, "history/");

    if (uin)
        Save_RC ();

    ContactAdd (11290140, "mICQ author (dead)");
    ContactAdd (82274703, "Rüdiger Kuhlmann");
    ContactAdd (82274703, "mICQ maintainer");
}

#define PrefParse(x)          switch (1) { case 1: if (!UtilUIParse          (&args, &x)) { M_print (i18n (2123, "%sSyntax error%s: Too few arguments: '%s'\n"), COLERROR, COLNONE, buf); continue; }}
#define PrefParseInt(i)       switch (1) { case 1: if (!UtilUIParseInt       (&args, &i)) { M_print (i18n (2124, "%sSyntax error%s: Not an integer: '%s'\n"), COLERROR, COLNONE, buf); continue; }}
#define PrefParseRemainder(x) switch (1) { case 1: if (!UtilUIParseRemainder (&args, &x)) { M_print (i18n (2123, "%sSyntax error%s: Too few arguments: '%s'\n"), COLERROR, COLNONE, buf); continue; }}

#define ADD_CMD(a,b)     else if (!strcasecmp (tmp, a))       \
                                 { PrefParseRemainder (tmp);   \
                                   prG->b = strdup (tmp); } else if (0)
#define ERROR

/*
 * Reads in a configuration file.
 */
void Read_RC_File (FILE *rcf)
{
    char buf[450];
    char *tmp = NULL, *cmd = NULL;
    char *p, *args;
    Contact *cont = NULL, *lastcont = NULL;
    Session *oldsess = NULL, *sess = NULL;
    int section, dep = 0;
    UDWORD uin, i;
    UWORD flags;
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
                oldsess = sess;
                sess = SessionC ();
                sess->spref = PreferencesSessionC ();
            }
            else
            {
                M_print (COLERROR "%s" COLNONE " ", i18n (1619, "Warning:"));
                M_print (i18n (1659, "Unknown section %s in configuration file."), buf);
                M_print ("\n");
                section = -1;
            }
            continue;
        }
        args = buf;
        switch (section)
        {
            case -1:
                M_print (COLERROR "%s" COLNONE " ", i18n (1619, "Warning:"));
                M_print (i18n (1675, "Ignored line:"));
                M_print (" %s\n", buf);
                break;
            case 0:
                if (!UtilUIParse (&args, &cmd))
                    continue;

                if (!strcasecmp (cmd, "receivescript"))
                {
                    if (!UtilUIParseRemainder (&args, &tmp))
                    {
                        dep = 1;
                        continue;
                    }
                    prG->event_cmd = strdup (tmp);
#ifndef MSGEXEC
                    printf (i18n (1817, "Warning: ReceiveScript feature not enabled!\n"));
#endif
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
                else if (!strcasecmp (cmd, "color"))
                {
                    if (!UtilUIParseInt (&args, &i))
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
                        if (prG->colors[i])
                            free (prG->colors[i]);
                        buf[0] = '\0';

                        while (UtilUIParse (&args, &cmd))
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
                        prG->colors[i] = strdup (buf);
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
                    if (!UtilUIParse (&args, &tmp))
                    {
                        dep = 1;
                        prG->flags |= FLAG_AUTOREPLY;
                        continue;
                    }
                    
                    if (!strcasecmp (tmp, "on"))
                        prG->flags |= FLAG_AUTOREPLY;
                    else if (!strcasecmp (tmp, "off"))
                        prG->flags &= ~FLAG_AUTOREPLY;
                    ADD_CMD ("away", auto_away);
                    ADD_CMD ("na",   auto_na);
                    ADD_CMD ("dnd",  auto_dnd);
                    ADD_CMD ("occ",  auto_occ);
                    ADD_CMD ("inv",  auto_inv);
                    ADD_CMD ("ffc",  auto_ffc);
                    else
                        ERROR;
                }
                else if (!strcasecmp (cmd, "sound"))
                {
                    if (!UtilUIParseRemainder (&args, &tmp))
                    {
                        prG->sound |= SFLAG_BEEP;
                        dep |= 1;
                        continue;
                    }
                    prG->sound &= ~SFLAG_BEEP & ~SFLAG_CMD;
                    if (!strcasecmp (tmp, "on"))
                        prG->sound |= SFLAG_BEEP;
                    else if (strcasecmp (tmp, "off"))
                    {
                        prG->sound |= SFLAG_CMD;
                        prG->sound_cmd = strdup (cmd);
                    }
                }
                else if (!strcasecmp (cmd, "soundonline"))
                {
                    if (!UtilUIParseRemainder (&args, &tmp))
                    {
                        prG->sound |= SFLAG_ON_BEEP;
                        dep |= 1;
                        continue;
                    }
                    prG->sound &= ~SFLAG_ON_BEEP & ~SFLAG_ON_CMD;
                    if (!strcasecmp (tmp, "on"))
                        prG->sound |= SFLAG_ON_BEEP;
                    else if (strcasecmp (tmp, "off"))
                    {
                        prG->sound |= SFLAG_ON_CMD;
                        prG->sound_on_cmd = strdup (cmd);
                    }
                }
                else if (!strcasecmp (cmd, "soundoffline"))
                {
                    if (!UtilUIParseRemainder (&args, &tmp))
                    {
                        prG->sound |= SFLAG_OFF_BEEP;
                        dep |= 1;
                        continue;
                    }
                    prG->sound &= ~SFLAG_OFF_BEEP & ~SFLAG_OFF_CMD;
                    if (!strcasecmp (tmp, "on"))
                        prG->sound |= SFLAG_OFF_BEEP;
                    else if (strcasecmp (tmp, "off"))
                    {
                        prG->sound |= SFLAG_OFF_CMD;
                        prG->sound_on_cmd = strdup (cmd);
                    }
                }
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
                    PrefParseRemainder (tmp);
                    if (spooled_tab_nicks < TAB_SLOTS)
                        tab_nick_spool[spooled_tab_nicks++] = strdup (tmp);
                }
                else if (!strcasecmp (cmd, "set"))
                {
                    int which = 0;
                    
                    PrefParse (cmd);

                    if (!strcasecmp (cmd, "color"))
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
                    else if (!strcasecmp (cmd, "linebreak"))
                        which = -2;
                    else if (!strcasecmp (cmd, "tabs"))
                        which = -3;
                    else
                        continue;
                    if (which > 0)
                    {
                        PrefParse (cmd);
                        if (!strcasecmp (cmd, "on"))
                            prG->flags |= which;
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
                }
                else
                {
                    M_print (COLERROR "%s" COLNONE " ", i18n (1619, "Warning:"));
                    M_print (i18n (1188, "Unrecognized command in rc file '%s', ignored."), cmd);
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
                    PrefParseRemainder (cmd);
                }
                else
                {
                    if (!lastcont)     /* ignore noise */
                        continue;
                    uin = lastcont->uin;
                    PrefParseRemainder (cmd);
                }
                
                
                if (!(cont = ContactAdd (uin, cmd)))
                {
                    M_print (COLERROR "%s" COLNONE " %s\n", i18n (1619, "Warning:"),
                             i18n (1620, "maximal number of contacts reached. Ask a wizard to enlarge me!"));
                    section = -1;
                    break;
                }
                if (~cont->flags & CONT_ALIAS)
                    cont->flags = flags;
                if (prG->verbose > 2)
                    M_print ("%ld = %s %x | %p\n", cont->uin, cont->nick, cont->flags, cont);
                break;
            case 2:
                PrefParse (cmd);

                if (!strcasecmp (cmd, "alter"))
                {
                    PrefParseRemainder (tmp);
                    CmdUser (UtilFill ("¶alter quiet %s", tmp));
                }
                else
                {
                    M_print (COLERROR "%s" COLNONE " ", i18n (1619, "Warning:"));
                    M_print (i18n (1188, "Unrecognized command in rc file '%s', ignored."), cmd);
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
                        sess->spref->type =
                            (sess->spref->version ? (sess->spref->version > 6 
                               ? TYPE_SERVER : TYPE_SERVER_OLD) : 0);
                        sess->spref->flags = 0;
                    }
                    else if (!strcasecmp (cmd, "peer"))
                    {
                        sess->spref->type = TYPE_MSGLISTEN;
                        sess->spref->flags = 0;
                        if (oldsess->spref->type == TYPE_SERVER || oldsess->spref->type == TYPE_SERVER_OLD)
                        {
                            oldsess->assoc = sess;
                            sess->parent = oldsess;
                        }
                        sess->spref->status = TCP_OK_FLAG;
                    }
                    else 
                        ERROR;
                    if (UtilUIParse (&args, &cmd))
                    {
                        if (!strcasecmp (cmd, "auto"))
                            sess->spref->flags |= CONN_AUTOLOGIN;
                    }
                }
                else if (!strcasecmp (cmd, "version"))
                {
                    PrefParseInt (i);

                    sess->spref->version = i;
                    if (!sess->spref->type)
                    {
                        if (sess->spref->version > 6)
                            sess->spref->type = TYPE_SERVER;
                        else
                            sess->spref->type = TYPE_SERVER_OLD;
                    }
                }
                else if (!strcasecmp (cmd, "server"))
                {
                    PrefParse (tmp);
                    sess->spref->server = strdup (tmp);
                }
                else if (!strcasecmp (cmd, "port"))
                {
                    PrefParseInt (i);
                    sess->spref->port = i;
                }
                else if (!strcasecmp (cmd, "uin"))
                {
                    PrefParseInt (i);
                    sess->spref->uin = i;
                }
                else if (!strcasecmp (cmd, "password"))
                {
                    PrefParse (tmp);
                    sess->spref->passwd = strdup (tmp);
                }
                else if (!strcasecmp (cmd, "status"))
                {
                    PrefParseInt (i);
                    sess->spref->status = i;
                }
                else
                {
                    M_print (COLERROR "%s" COLNONE " ", i18n (1619, "Warning:"));
                    M_print (i18n (1188, "Unrecognized command in rc file '%s', ignored."), cmd);
                    M_print ("\n");
                }
        }
    }
    
    /* now tab the nicks we may have spooled earlier */
    for (i = 0; i < spooled_tab_nicks; i++)
    {
        TabAddUIN (ContactFindByNick (tab_nick_spool[i]));
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
    {
        prG->logplace = malloc (strlen (PrefUserDir ()) + 10);
        strcpy (prG->logplace, PrefUserDir ());
        strcat (prG->logplace, "history/");
    }

    for (i = 0; (sess = SessionNr (i)); i++)
    {
        assert (sess->spref);

        sess->port   = sess->spref->port;
        sess->server = sess->spref->server;
        sess->passwd = sess->spref->passwd;
        sess->status = sess->spref->status;
        sess->uin    = sess->spref->uin;
        sess->ver    = sess->spref->version;
        sess->type   = sess->spref->type;
        sess->flags  = sess->spref->flags;
        if (prG->s5Use && sess->type & TYPEF_SERVER)
            sess->type = sess->spref->type = 2;
        if (sess->spref->type == TYPE_SERVER || sess->spref->type == TYPE_SERVER_OLD)
            oldsess = sess;
    }

    if (prG->verbose && oldsess)
    {
        M_print (i18n (1189, "UIN = %ld\n"),    oldsess->spref->uin);
        M_print (i18n (1190, "port = %ld\n"),   oldsess->spref->port);
        M_print (i18n (1191, "passwd = %s\n"),  oldsess->spref->passwd ? oldsess->spref->passwd : "[none]");
        M_print (i18n (1192, "server = %s\n"),  oldsess->spref->server ? oldsess->spref->server : "[none]");
        M_print (i18n (1193, "status = %ld\n"), oldsess->spref->status);
        M_print (i18n (1196, "Message_cmd = %s\n"), CmdUserLookupName ("msg"));
        M_print ("flags: %08x\n", prG->flags);
    }
    if (dep)
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
    int k;
    Contact *cont;
    Session *ss;

    M_print (i18n (2048, "Saving preferences to %s.\n"), prG->rcfile);
    rcf = fopen (prG->rcfile, "w");
    if (!rcf)
    {
        int rc = errno;
        if (rc == ENOENT)
        {
            char *tmp = strdup (PrefUserDir ());
            if (tmp[strlen (tmp) - 1] == '/')
                tmp[strlen (tmp) - 1] = '\0';
            M_print (i18n (2047, "Creating directory %s.\n"), tmp);
            k = mkdir (tmp, 0700);
            if (!k)
                rcf = fopen (prG->rcfile, "w");
            if (!rcf)
                rc = errno;
        }
        if (!rcf)
            M_print (i18n (1872, "failed: %s (%d)\n"), strerror (rc), rc);
    }
    if (!rcf)
        return -1;
    fprintf (rcf, "# This file was generated by mICQ " MICQ_VERSION " of %s %s\n", __TIME__, __DATE__);
    t = time (NULL);
    fprintf (rcf, "# This file was generated on %s", ctime (&t));
    fprintf (rcf, "\n");
    
    for (k = 0; (ss = SessionNr (k)); k++)
    {
        if (!ss->spref || (ss->spref->type != TYPE_SERVER && ss->spref->type != TYPE_SERVER_OLD && ss->spref->type != TYPE_MSGLISTEN)
            || (!ss->spref->uin && ss->spref->type == TYPE_SERVER)
            || (ss->spref->type == TYPE_MSGLISTEN && ss->parent && !ss->parent->spref->uin))
            continue;

        fprintf (rcf, "[Connection]\n");
        fprintf (rcf, "type %s%s\n",  ss->spref->type == TYPE_SERVER || ss->spref->type == TYPE_SERVER_OLD ? "server" : "peer",
                                        ss->spref->flags & CONN_AUTOLOGIN ? " auto" : "");
        fprintf (rcf, "version %d\n", ss->spref->version);
        if (ss->spref->server)
            fprintf (rcf, "server %s\n",  ss->spref->server);
        if (ss->spref->port)
            fprintf (rcf, "port %ld\n",    ss->spref->port);
        if (ss->spref->uin)
            fprintf (rcf, "uin %ld\n",     ss->spref->uin);
        if (ss->spref->passwd)
            fprintf (rcf, "password %s\n", ss->spref->passwd);
        else if (!k)
            fprintf (rcf, "# password\n");
        if (ss->spref->status || !k)
            fprintf (rcf, "status %ld\n",  ss->spref->status);
        fprintf (rcf, "\n");
    }

    fprintf (rcf, "\n[General]\n# Support for SOCKS5 server\n");
    fprintf (rcf, "s5_use %d\n", prG->s5Use);
    if (!prG->s5Host)
        fprintf (rcf, "s5_host [none]\n");
    else
        fprintf (rcf, "s5_host %s\n", prG->s5Host);
    fprintf (rcf, "s5_port %d\n", prG->s5Port);
    fprintf (rcf, "# If you need authentification, put 1 for s5_auth and fill your name/password\n");
    fprintf (rcf, "s5_auth %d\n", prG->s5Auth);
    if (!prG->s5Name)
        fprintf (rcf, "s5_name [none]\n");
    else
        fprintf (rcf, "s5_name %s\n", prG->s5Name);
    if (!prG->s5Pass)
        fprintf (rcf, "s5_pass [none]\n");
    else
        fprintf (rcf, "s5_pass %s\n", prG->s5Pass);

    fprintf (rcf, "\n#in seconds\nauto_away %ld\n", prG->away_time);
    fprintf (rcf, "\n#For dumb terminals that don't wrap set this.");
    fprintf (rcf, "\nScreen_Width %d\n", prG->screen);
    fprintf (rcf, "verbose %ld\n\n", prG->verbose);


    fprintf (rcf, "# Set some simple options.\n");
    fprintf (rcf, "set delbs     %s # if a DEL char is supposed to be backspace\n",
                    prG->flags & FLAG_DELBS     ? "on " : "off");
    fprintf (rcf, "set russian   %s # if you want russian koi8-r/u <-> cp1251 character conversion\n",
                    prG->flags & FLAG_CONVRUSS  ? "on " : "off");
    fprintf (rcf, "set japanese  %s # if you want japanese Shift-JIS <-> EUC character conversion\n",
                    prG->flags & FLAG_CONVEUC   ? "on " : "off");
    fprintf (rcf, "set funny     %s # if you want funny messages\n",
                    prG->flags & FLAG_FUNNY     ? "on " : "off");
    fprintf (rcf, "set color     %s # if you want colored messages\n",
                    prG->flags & FLAG_COLOR     ? "on " : "off");
    fprintf (rcf, "set hermit    %s # if you want messages from people on your contact list ONLY\n",
                    prG->flags & FLAG_HERMIT    ? "on " : "off");
    fprintf (rcf, "set log       %s # if you want to log messages\n",
                    prG->flags & FLAG_LOG       ? "on " : "off");
    fprintf (rcf, "set logonoff  %s # if you also want to log online/offline events\n",
                    prG->flags & FLAG_LOG_ONOFF ? "on " : "off");
    fprintf (rcf, "set auto      %s # if automatic responses are to be sent\n",
                    prG->flags & FLAG_AUTOREPLY ? "on " : "off");
    fprintf (rcf, "set uinprompt %s # if the prompt should contain the last uin a message was received from\n",
                    prG->flags & FLAG_UINPROMPT ? "on " : "off");
    fprintf (rcf, "set linebreak %s # the line break type to be used (simple, break, indent, smart)\n",
                    prG->flags & FLAG_LIBR_INT 
                    ? prG->flags & FLAG_LIBR_BR ? "smart " : "indent"
                    : prG->flags & FLAG_LIBR_BR ? "break " : "simple");
    fprintf (rcf, "set tabs %s # type of tab completion (simple, cycle, cycleall)\n\n",
                    prG->tabs == TABS_SIMPLE ? "simple" :
                    prG->tabs == TABS_CYCLE ? "cycle" : "cycleall");


    fprintf (rcf, "# Colors. color scheme 0|1|2|3 or color <use> <color>");
    {
        int i, l;
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
                default: c = ""; assert (0);
            }
            fprintf (rcf, "\ncolor %s", c);
            for (t = strdup (prG->colors[i]); *t; t += l)
            {
                if      (!strncmp (BLACK,   t, l = strlen (BLACK)))   c = "black  ";
                else if (!strncmp (RED,     t, l = strlen (RED)))     c = "red    ";
                else if (!strncmp (GREEN,   t, l = strlen (GREEN)))   c = "green  ";
                else if (!strncmp (YELLOW,  t, l = strlen (YELLOW)))  c = "yellow ";
                else if (!strncmp (BLUE,    t, l = strlen (BLUE)))    c = "blue   ";
                else if (!strncmp (MAGENTA, t, l = strlen (MAGENTA))) c = "magenta";
                else if (!strncmp (CYAN,    t, l = strlen (CYAN)))    c = "cyan   ";
                else if (!strncmp (WHITE,   t, l = strlen (WHITE)))   c = "white  ";
                else if (!strncmp (SGR0,    t, l = strlen (SGR0)))    c = "none   ";
                else if (!strncmp (BOLD,    t, l = strlen (BOLD)))    c = "bold   ";
                else c = t, l = strlen (t);
                fprintf (rcf, " %s", c);
            }
        }
    }
    if (prG->scheme != (UBYTE)-1)
        fprintf (rcf, "\ncolor scheme   %d", prG->scheme);

    fprintf (rcf, "\n\nlogplace %s      # the file or (dstinct files in) dir to log to\n",
                    prG->logplace ? prG->logplace : "");

    fprintf (rcf, "# Define to a program which is executed to play sound when a message is received.\n");
    fprintf (rcf, "sound %s\n\n", prG->sound & SFLAG_BEEP ? "on" :
                                    prG->sound & SFLAG_CMD && prG->sound_cmd ? prG->sound_cmd : "off");

    fprintf (rcf, "# Execute this cmd when a user comes online in your contacts.\n");
    fprintf (rcf, "soundonline %s\n\n", prG->sound & SFLAG_ON_BEEP ? "on" :
                                          prG->sound & SFLAG_ON_CMD && prG->sound_on_cmd ? 
                                          prG->sound_on_cmd : "off");

    fprintf (rcf, "# Execute this cmd when a user goes offline in your contacts.\n");
    fprintf (rcf, "soundoffline %s\n\n", prG->sound & SFLAG_OFF_BEEP ? "on" :
                                           prG->sound & SFLAG_OFF_CMD && prG->sound_off_cmd ?
                                           prG->sound_off_cmd : "off");
    if (prG->event_cmd)
         fprintf (rcf, "receivescript %s\n\n", prG->event_cmd);
    else
         fprintf (rcf, "#receivescript\n\n");

    fprintf (rcf, "\n# automatic responses\n");
    fprintf (rcf, "auto away %s\n", prG->auto_away);
    fprintf (rcf, "auto na   %s\n", prG->auto_na);
    fprintf (rcf, "auto dnd  %s\n", prG->auto_dnd);
    fprintf (rcf, "auto occ  %s\n", prG->auto_occ);
    fprintf (rcf, "auto inv  %s\n", prG->auto_inv);
    fprintf (rcf, "auto ffc  %s\n", prG->auto_ffc);

    fprintf (rcf, "\n# The strings section - runtime redefinable strings.\n");
    fprintf (rcf, "# The alter command redefines command names.\n");
    fprintf (rcf, "[Strings]\n");
    {
        jump_t *f;
        for (f = CmdUserTable (); f->f; f++)
            if (f->name && strcmp (f->name, f->defname))
                fprintf (rcf, "alter %s %s\n", f->defname, f->name);
    }

    fprintf (rcf, "\n# The contact list section.\n");
    fprintf (rcf, "#  Use * in front of the number of anyone you want to see you while you're invisible.\n");
    fprintf (rcf, "#  Use ~ in front of the number of anyone you want to always see you as offline.\n");
    fprintf (rcf, "#  Use ^ in front of the number of anyone you want to ignore.\n");
    fprintf (rcf, "[Contacts]\n");

    for (cont = ContactStart (); ContactHasNext (cont); cont = ContactNext (cont))
    {
        if (!(cont->flags & (CONT_TEMPORARY | CONT_ALIAS)))
        {
            Contact *cont2;
            if (cont->flags & CONT_INTIMATE) fprintf (rcf, "*"); else fprintf (rcf, " ");
            if (cont->flags & CONT_HIDEFROM) fprintf (rcf, "~"); else fprintf (rcf, " ");
            if (cont->flags & CONT_IGNORE)   fprintf (rcf, "^"); else fprintf (rcf, " ");
            fprintf (rcf, "%9ld %s\n", cont->uin, cont->nick);
            for (cont2 = ContactStart (); ContactHasNext (cont2); cont2 = ContactNext (cont2))
            {
                if (cont2->uin == cont->uin && cont2->flags & CONT_ALIAS)
                    fprintf (rcf, "   %9ld %s\n", cont->uin, cont2->nick);
            }
        }
    }
    fprintf (rcf, "\n");
    return fclose (rcf) ? -1 : 0;
}

int Add_User (Session *sess, UDWORD uin, const char *name)
{
    FILE *rcf;

    rcf = fopen (prG->rcfile, "a");
    if (!rcf)
        return 0;
    fprintf (rcf, "   %ld %s\n", uin, name);
    fclose (rcf);
    return 1;
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

    assert (len >= 0);

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

