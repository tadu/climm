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


#define      ADD_ALTER(a,b)      else if (!strcasecmp (tmp, a))    \
                                       CmdUser (NULL, UtilFill ("¶alter quiet " #b " %s", strtok (NULL, " \n\t")))
#define      ADD_CMD(a,b)        else if (!strcasecmp (tmp, a))        \
                                       { prG->b = strtok (NULL, "\n");\
                                         if (!prG->b) prG->b = ""; \
                                         while (*prG->b == ' ' || *prG->b == '\t') prG->b++; \
                                         prG->b = strdup (prG->b);     \
                                         } else if (0)

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

static char *M_strdup (char *src)
{
    return strdup (src ? src : "");
}

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
        sess->flags |= CONN_WIZARD;
    }
    else
    {
        M_print (i18n (1791, "Setup wizard finished. Congratulations!\n"));
        sess = SessionC ();
        assert (sess);
        sess->spref = PreferencesSessionC ();
        assert (sess->spref);
        
        sess->spref->type = TYPE_SERVER;
        sess->spref->flags = CONN_AUTOLOGIN;
        sess->spref->server = strdup ("login.icq.com");
        sess->spref->port = 5190;
        sess->spref->status = STATUS_ONLINE;
        sess->spref->version = 8;
        sess->spref->uin = uin;
        
        sess->server  = strdup ("login.icq.com");
        sess->port    = 5190;
        sess->type    = TYPE_SERVER;
        sess->flags   = CONN_AUTOLOGIN;
        sess->ver     = 8;
        sess->uin     = uin;
        sess->passwd  = strdup (passwd);
    }
    
    sesst = SessionC ();
    assert (sesst);
    sesst->spref = PreferencesSessionC ();
    assert (sesst->spref);

    sesst->assoc = sess;
    sess->assoc = sesst;
    sesst->spref->type = TYPE_LISTEN;
    sesst->spref->flags = CONN_AUTOLOGIN;
    sesst->type = sesst->spref->type;
    sesst->flags = sesst->spref->flags;
    sesst->spref->version = 8;
    sesst->ver = 8;

    prG->status = STATUS_ONLINE;
    prG->tabs = TABS_SIMPLE;
#ifdef ANSI_TERM
    prG->flags = FLAG_COLOR | FLAG_LOG | FLAG_LOG_ONOFF | FLAG_DELBS;
#else
    prG->flags =              FLAG_LOG | FLAG_LOG_ONOFF | FLAG_DELBS;
#endif
    prG->auto_dnd  = strdup (i18n (1929, "User is DND [Auto-Message]"));
    prG->auto_away = strdup (i18n (1010, "User is Away [Auto-Message]"));
    prG->auto_na   = strdup (i18n (1011, "User is not available [Auto-Message]"));
    prG->auto_occ  = strdup (i18n (1012, "User is Occupied [Auto-Message]"));
    prG->auto_inv  = strdup (i18n (1013, "User is offline"));
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

void Read_RC_File (FILE *rcf)
{
    char buf[450];
    char *tmp;
    char *p;
    Contact *cont = NULL;
    Session *oldsess = NULL, *sess = NULL;
    int i, section, dep = 0;
    UDWORD uin;
    UWORD flags;
    char *tab_nick_spool[TAB_SLOTS];
    int spooled_tab_nicks;

    prG->away_time = default_away_time;
    prG->tabs = TABS_SIMPLE;

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
                M_print (COLERR "%s" COLNONE " ", i18n (1619, "Warning:"));
                M_print (i18n (1659, "Unknown section %s in configuration file."), buf);
                M_print ("\n");
                section = -1;
            }
            continue;
        }
        switch (section)
        {
            case -1:
                M_print (COLERR "%s" COLNONE " ", i18n (1619, "Warning:"));
                M_print (i18n (1675, "Ignored line:"));
                M_print (" %s\n", buf);
                break;
            case 0:
                tmp = strtok (buf, " ");
                if (!strcasecmp (tmp, "ReceiveScript"))
                {
#ifdef MSGEXEC
                    prG->event_cmd = M_strdup (strtok (NULL, "\n"));
#else
                    printf (i18n (1817, "Warning: ReceiveScript feature not enabled!\n"));
#endif
                }
                else if (!strcasecmp (tmp, "s5_use"))
                {
                    prG->s5Use = atoi (strtok (NULL, " \n\t"));
                }
                else if (!strcasecmp (tmp, "s5_host"))
                {
                    prG->s5Host = M_strdup (strtok (NULL, "\n\t"));
                }
                else if (!strcasecmp (tmp, "s5_port"))
                {
                    prG->s5Port = atoi (strtok (NULL, " \n\t"));
                }
                else if (!strcasecmp (tmp, "s5_auth"))
                {
                    prG->s5Auth = atoi (strtok (NULL, " \n\t"));
                }
                else if (!strcasecmp (tmp, "s5_name"))
                {
                    prG->s5Name = M_strdup (strtok (NULL, "\n\t"));
                }
                else if (!strcasecmp (tmp, "s5_pass"))
                {
                    prG->s5Pass = M_strdup (strtok (NULL, "\n\t"));
                }
                else if (!strcasecmp (tmp, "verbose"))
                {
                    if (!prG->verbose)
                        prG->verbose = atoi (strtok (NULL, "\n"));
                }
                else if (!strcasecmp (tmp, "logplace"))
                {
                    if (!prG->logplace) /* don't overwrite command line arg */
                        prG->logplace = strdup (strtok (NULL, " \t\n"));
                }
                else if (!strcasecmp (tmp, "LineBreakType"))
                {
                    i = atoi (strtok (NULL, " \n\t"));
                    prG->flags &= ~FLAG_LIBR_BR & ~FLAG_LIBR_INT;
                    if (!i || i == 3)
                        prG->flags |= FLAG_LIBR_BR;
                    if (i & 2)
                        prG->flags |= FLAG_LIBR_INT;
                }
                else if (!strcasecmp (tmp, "auto"))
                {
                    tmp = strtok (NULL, " \t\n");
                    
                    if (!tmp || !strcasecmp (tmp, "on"))
                        prG->flags |= FLAG_AUTOREPLY;
                    else if (!strcasecmp (tmp, "off"))
                        prG->flags &= ~FLAG_AUTOREPLY;
                    ADD_CMD ("away", auto_away);
                    ADD_CMD ("na",   auto_na);
                    ADD_CMD ("dnd",  auto_dnd);
                    ADD_CMD ("occ",  auto_occ);
                    ADD_CMD ("inv",  auto_inv);
                    else
                        prG->flags |= FLAG_AUTOREPLY;
                }
                else if (!strcasecmp (tmp, "Sound"))
                {
                    tmp = strtok (NULL, "\n\t");
                    if (!tmp)
                    {
                        prG->sound |= SFLAG_BEEP;
                        dep |= 1;
                        continue;
                    }
                    prG->sound &= ~SFLAG_BEEP & ~SFLAG_CMD;
                    if (!strcasecmp (tmp, "on"))
                        prG->sound |= SFLAG_BEEP;
                    else if (!strcasecmp (tmp, "off")) ;
                    else
                    {
                        prG->sound |= SFLAG_CMD;
                        prG->sound_cmd = strdup (tmp);
                    }
                }
                else if (!strcasecmp (tmp, "SoundOnline"))
                {
                    tmp = strtok (NULL, "\n\t");
                    if (!tmp)
                    {
                        prG->sound |= SFLAG_ON_BEEP;
                        dep |= 1;
                        continue;
                    }
                    prG->sound &= ~SFLAG_ON_BEEP & ~SFLAG_ON_CMD;
                    if (!strcasecmp (tmp, "on"))
                        prG->sound |= SFLAG_ON_BEEP;
                    else if (!strcasecmp (tmp, "off")) ;
                    else
                    {
                        prG->sound |= SFLAG_ON_CMD;
                        prG->sound_on_cmd = strdup (tmp);
                    }
                }
                else if (!strcasecmp (tmp, "SoundOffline"))
                {
                    tmp = strtok (NULL, "\n\t");
                    if (!tmp)
                    {
                        prG->sound |= SFLAG_OFF_BEEP;
                        dep |= 1;
                        continue;
                    }
                    prG->sound &= ~SFLAG_OFF_BEEP & ~SFLAG_OFF_CMD;
                    if (!strcasecmp (tmp, "on"))
                        prG->sound |= SFLAG_OFF_BEEP;
                    else if (!strcasecmp (tmp, "off")) ;
                    else
                    {
                        prG->sound |= SFLAG_OFF_CMD;
                        prG->sound_on_cmd = strdup (tmp);
                    }
                }
                else if (!strcasecmp (tmp, "Auto_away"))
                {
                    prG->away_time = atoi (strtok (NULL, " \n\t"));
                }
                else if (!strcasecmp (tmp, "Screen_width"))
                {
                    prG->screen = atoi (strtok (NULL, " \n\t"));
                }
                else if (!strcasecmp (tmp, "Tab"))
                {
                    if (spooled_tab_nicks < TAB_SLOTS)
                        tab_nick_spool[spooled_tab_nicks++] = M_strdup (strtok (NULL, "\n\t"));
                }
                else if (!strcasecmp (tmp, "set"))
                {
                    int which = 0;
                    tmp = strtok (NULL, " \t\n");
                    if (!tmp)
                        continue;
                    if (!strcasecmp (tmp, "color"))
                        which = FLAG_COLOR;
                    else if (!strcasecmp (tmp, "hermit"))
                        which = FLAG_HERMIT;
                    else if (!strcasecmp (tmp, "delbs"))
                        which = FLAG_DELBS;
                    else if (!strcasecmp (tmp, "russian"))
                        which = FLAG_CONVRUSS;
                    else if (!strcasecmp (tmp, "japanese"))
                        which = FLAG_CONVEUC;
                    else if (!strcasecmp (tmp, "funny"))
                        which = FLAG_FUNNY;
                    else if (!strcasecmp (tmp, "log"))
                        which = FLAG_LOG;
                    else if (!strcasecmp (tmp, "loglevel"))
                        which = -1;
                    else if (!strcasecmp (tmp, "logonoff"))
                        which = FLAG_LOG_ONOFF;
                    else if (!strcasecmp (tmp, "auto"))
                        which = FLAG_AUTOREPLY;
                    else if (!strcasecmp (tmp, "uinprompt"))
                        which = FLAG_UINPROMPT;
                    else if (!strcasecmp (tmp, "linebreak"))
                        which = -2;
                    else if (!strcasecmp (tmp, "tabs"))
                        which = -3;
                    else
                        continue;
                    if (which > 0)
                    {
                        tmp = strtok (NULL, " \t\n");
                        if (!tmp || !strcasecmp (tmp, "on"))
                            prG->flags |= which;
                        else
                            prG->flags &= ~which;
                    }
                    else if (which == -1)
                    {
                        tmp = strtok (NULL, " \t\n");
                        if (!tmp)
                            continue;
                        i = atoi (tmp);
                        prG->flags &= ~FLAG_LOG & ~FLAG_LOG_ONOFF;
                        if (i)
                            prG->flags |= FLAG_LOG;
                        if (i & 2)
                            prG->flags |= FLAG_LOG_ONOFF;
                    }
                    else if (which == -2)
                    {
                        tmp = strtok (NULL, " \t\n");
                        prG->flags &= ~FLAG_LIBR_BR & ~FLAG_LIBR_INT;
                        if (!strcasecmp (tmp, "break"))
                            prG->flags |= FLAG_LIBR_BR;
                        else if (!strcasecmp (tmp, "simple"))
                            ;
                        else if (!strcasecmp (tmp, "indent"))
                            prG->flags |= FLAG_LIBR_INT;
                        else if (!strcasecmp (tmp, "smart"))
                            prG->flags |= FLAG_LIBR_BR | FLAG_LIBR_INT;
                    }
                    else if (which == -3)
                    {
                        tmp = strtok (NULL, " \t\n");
                        prG->tabs = TABS_SIMPLE;
                        if (!strcasecmp (tmp, "cycle"))
                            prG->tabs = TABS_CYCLE;
                        else if (!strcasecmp (tmp, "cycleall"))
                            prG->tabs = TABS_CYCLEALL;
                    }
                }
                else
                {
                    M_print (COLERR "%s" COLNONE " ", i18n (1619, "Warning:"));
                    M_print (i18n (1188, "Unrecognized command in rc file '%s', ignored."), tmp);
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

                if (isdigit (*p))
                {
                    tmp = strtok (p, " ");
                    if (!tmp)
                        continue;
                    uin = atoi (tmp);
                    tmp = strtok (NULL, "");
                    if (!tmp)
                        continue;
                    if (ContactFind (uin))
                        flags |= CONT_ALIAS;
                }
                else
                {
                    if (!cont)     /* ignore noise */
                        continue;
                    uin = cont->uin;
                    tmp = strtok (NULL, "");
                    flags |= CONT_ALIAS;
                }
                
                
                if (!(cont = ContactAdd (uin, tmp)))
                {
                    M_print (COLERR "%s" COLNONE " %s\n", i18n (1619, "Warning:"),
                             i18n (1620, "maximal number of contacts reached. Ask a wizard to enlarge me!"));
                    section = -1;
                    break;
                }
                cont->flags = flags;
                if (uin == -1)
                    cont->uin = (cont - 1)->uin;
                if (flags & CONT_ALIAS)
                    cont->flags = ContactFind (uin)->flags | CONT_ALIAS;
                if (prG->verbose > 2)
                    M_print ("%ld = %s\n", cont->uin, cont->nick);
                break;
            case 2:
                tmp = strtok (buf, " ");
                if (!strcasecmp (tmp, "alter"))
                {
                    CmdUser (UtilFill ("¶alter quiet %s", strtok (NULL, "\n")));
                }
                else
                {
                    M_print (COLERR "%s" COLNONE " ", i18n (1619, "Warning:"));
                    M_print (i18n (1188, "Unrecognized command in rc file '%s', ignored."), tmp);
                    M_print ("\n");
                }
                break;
            case 3:
                tmp = strtok (buf, " ");
                if (!strcasecmp (tmp, "type"))
                {
                    tmp = strtok (NULL, " ");
                    if (!tmp)
                        continue;
                    if (!strcasecmp (tmp, "server"))
                    {
                        sess->spref->type =
                            (sess->spref->version ? (sess->spref->version > 6 
                               ? TYPE_SERVER : TYPE_SERVER_OLD) : 0);
                        sess->spref->flags = 0;
                    }
                    else if (!strcasecmp (tmp, "peer"))
                    {
                        sess->spref->type = TYPE_LISTEN;
                        sess->spref->flags = 0;
                        if (oldsess->spref->type == TYPE_SERVER || oldsess->spref->type == TYPE_SERVER_OLD)
                        {
                            oldsess->assoc = sess;
                            sess->assoc = oldsess;
                        }
                    }
                    else 
                        continue;
                    tmp = strtok (NULL, " ");
                    if (!tmp)
                        continue;
                    if (!strcasecmp (tmp, "auto"))
                        sess->spref->flags |= CONN_AUTOLOGIN;
                }
                else if (!strcasecmp (tmp, "version"))
                {
                    sess->spref->version = atoi (strtok (NULL, " \n\t"));
                    if (!sess->spref->type)
                    {
                        if (sess->spref->version > 6)
                            sess->spref->type = TYPE_SERVER;
                        else
                            sess->spref->type = TYPE_SERVER_OLD;
                    }
                }
                else if (!strcasecmp (tmp, "server"))
                {
                    sess->spref->server = M_strdup (strtok (NULL, " \n\t"));
                }
                else if (!strcasecmp (tmp, "port"))
                {
                    sess->spref->port = atoi (strtok (NULL, " \n\t"));
                }
                else if (!strcasecmp (tmp, "uin"))
                {
                    sess->spref->uin = atoi (strtok (NULL, " \n\t"));
                }
                else if (!strcasecmp (tmp, "password"))
                {
                    sess->spref->passwd = M_strdup (strtok (NULL, "\n\t"));
                }
                else if (!strcasecmp (tmp, "status"))
                {
                    sess->spref->status = atoi (strtok (NULL, " \n\t"));
                }
                else
                    printf ("Bad line in section 3: %s\n", buf);
        }
    }
    
    /* now tab the nicks we may have spooled earlier */
    for (i = 0; i < spooled_tab_nicks; i++)
    {
        TabAddUIN (ContactFindByNick (tab_nick_spool[i]));
        free (tab_nick_spool[i]);
    }

    if (!prG->auto_dnd)
        prG->auto_dnd  = strdup (i18n (1929, "User is DND [Auto-Message]"));
    if (!prG->auto_away)
        prG->auto_away = strdup (i18n (1010, "User is Away [Auto-Message]"));
    if (!prG->auto_na)
        prG->auto_na   = strdup (i18n (1011, "User is not available [Auto-Message]"));
    if (!prG->auto_occ)
        prG->auto_occ  = strdup (i18n (1012, "User is Occupied [Auto-Message]"));
    if (!prG->auto_inv)
        prG->auto_inv  = strdup (i18n (1013, "User is offline"));

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
        if (!ss->spref || (ss->spref->type != TYPE_SERVER && ss->spref->type != TYPE_SERVER_OLD && ss->spref->type != TYPE_LISTEN)
            || (!ss->spref->uin && ss->spref->type == TYPE_SERVER)
            || (ss->assoc && !ss->assoc->spref->uin && ss->spref->type == TYPE_LISTEN))
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
    fprintf (rcf, "verbose %d\n\n", prG->verbose);


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


    fprintf (rcf, "logplace %s      # the file or (dstinct files in) dir to log to\n",
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
    fprintf (rcf, "receivescript %s\n\n", prG->event_cmd ? prG->event_cmd : "");

    fprintf (rcf, "\n# automatic responses\n");
    fprintf (rcf, "auto away %s\n", prG->auto_away);
    fprintf (rcf, "auto na   %s\n", prG->auto_na);
    fprintf (rcf, "auto dnd  %s\n", prG->auto_dnd);
    fprintf (rcf, "auto occ  %s\n", prG->auto_occ);
    fprintf (rcf, "auto inv  %s\n", prG->auto_inv);

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

int Add_User (Session *sess, UDWORD uin, char *name)
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

