/* $Id$ */
/* Copyright: This file may be distributed under version 2 of the GPL licence. */

#include "climm.h"
#include <stdarg.h>
#include <assert.h>
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
#include "util_alias.h"
#include "tabs.h"
#include "contact.h"
#include "tcp.h"
#include "util.h"
#include "conv.h"
#include "cmd_user.h"
#include "oldicq_base.h"
#include "preferences.h"
#include "util_io.h"
#include "remote.h"
#include "oscar_base.h"
#include "oldicq_compat.h"
#include "connection.h"
#include "util_parse.h"
#include "util_rl.h"
#include "msn_base.h"
#include "jabber_base.h"

/****/
#define USER_PROMPT "%4c%b%p%d://%1c%b%n%3c/%2c%b%s%8c %t%7c%b>%6c%r%7c%b<%6c%b%a%7c%b>"
/*#define USER_PROMPT "%n@%p:%s %t>%r<%a>"*/
#define USER_PROMPT_STRFTIME "%c"

#define WAND1 " \xb7 \xb7 "
#define WAND2 "\xb7 o \xb7"
#define WAND3 " \xb7 \\ "
#define WAND4 "    \\"

Connection *PrefNewConnection (UDWORD servertype, const char *user, const char *passwd)
{
    Connection *conn = NULL;
    Contact *cont;
    
    if (servertype == TYPE_SERVER)
    {
        conn = ConnectionC (servertype);
        conn->open = &ConnectionInitServer;
        
        conn->flags |= CONN_AUTOLOGIN;
        conn->pref_server = strdup ("login.icq.com");
        conn->pref_port = 5190;
        conn->pref_status = ims_online;
        conn->version = 8;
        s_repl (&conn->screen, user);
        conn->uin = atoi (user);
        conn->pref_passwd = passwd ? strdup (passwd) : NULL;
        
        conn->server  = strdup (conn->pref_server);
        conn->port    = conn->pref_port;
        conn->passwd  = passwd ? strdup (passwd) : NULL;
        conn->status  = ims_online;

        conn->contacts = ContactGroupC (conn, 0, s_sprintf ("contacts-icq8-%s", user));
        OptSetVal (&conn->contacts->copts, CO_IGNORE, 0);
        
        cont = ContactUIN (conn, 82274703);
        ContactCreate (conn, cont);
        ContactAddAlias (cont, "climm");
        ContactAddAlias (cont, "Tadu");
        OptSetStr (&cont->copts, CO_COLORINCOMING, OptC2S ("red bold"));
        OptSetStr (&cont->copts, CO_COLORMESSAGE, OptC2S ("red bold"));
        rl_print ("\n");
        rl_printf (i18n (2381, "I'll add the author of climm to your contact list for your convenience. Don't abuse this opportunity - please use the help command and make a serious attempt to read the man pages and the FAQ before asking questions.\n"));
        rl_print ("\n");
    }
#ifdef ENABLE_XMPP
    else if (servertype == TYPE_XMPP_SERVER)
    {
        const char *serverpart = strchr (user, '@') + 1;
    
        conn = ConnectionC (servertype);
        conn->open = &ConnectionInitXMPPServer;
        
        conn->flags |= CONN_AUTOLOGIN;
        if (!strcmp (serverpart, "gmail.com") || !strcmp (serverpart, "gmail.com"))
            conn->pref_server = strdup ("talk.google.com");
        else
            conn->pref_server = strdup (serverpart);
        conn->pref_port = 5222;
        conn->pref_status = ims_online;
        conn->version = 8;
        s_repl (&conn->screen, user);
        conn->uin = 0;
        conn->pref_passwd = passwd ? strdup (passwd) : NULL;
        
        conn->server  = strdup (conn->pref_server);
        conn->port    = conn->pref_port;
        conn->passwd  = passwd ? strdup (passwd) : NULL;
        conn->status  = ims_online;

        conn->contacts = ContactGroupC (conn, 0, s_sprintf ("contacts-xmpp-%s", user));
        OptSetVal (&conn->contacts->copts, CO_IGNORE, 0);
        
        cont = ContactScreen (conn, "RKuhlmann@gmail.com");
        ContactCreate (conn, cont);
        ContactAddAlias (cont, "climm");
        ContactAddAlias (cont, "Tadu");
        OptSetStr (&cont->copts, CO_COLORINCOMING, OptC2S ("red bold"));
        OptSetStr (&cont->copts, CO_COLORMESSAGE, OptC2S ("red bold"));
        rl_print ("\n");
        rl_printf (i18n (2381, "I'll add the author of climm to your contact list for your convenience. Don't abuse this opportunity - please use the help command and make a serious attempt to read the man pages and the FAQ before asking questions.\n"));
        rl_print ("\n");
    }
#endif
#ifdef ENABLE_MSN
    else if (servertype == TYPE_MSN_SERVER)
    {
        conn = ConnectionC (servertype);
        conn->open = &ConnectionInitMSNServer;
        
        conn->flags |= CONN_AUTOLOGIN;
        conn->pref_server = strdup (strchr (user, '@') + 1);
        conn->pref_port = 42;
        conn->pref_status = ims_online;
        conn->version = 8;
        s_repl (&conn->screen, user);
        conn->uin = 0;
        conn->pref_passwd = passwd ? strdup (passwd) : NULL;
        
        conn->server  = strdup (conn->pref_server);
        conn->port    = conn->pref_port;
        conn->passwd  = passwd ? strdup (passwd) : NULL;
        conn->status  = ims_online;

        conn->contacts = ContactGroupC (conn, 0, s_sprintf ("contacts-msn-%s", user));
        OptSetVal (&conn->contacts->copts, CO_IGNORE, 0);
    }
#endif
    return conn;
}

void Initialize_RC_File ()
{
    strc_t line;
    Connection *conn;
#ifdef ENABLE_REMOTECONTROL
    Connection *conns;
#endif
    char *user = NULL;
    char *passwd;
    char *t;
    UDWORD servertype = 0;
    
    prG->away_time = default_away_time;

    rl_print ("\n");
    rl_print (i18n (2612, "No valid user account found. The setup wizard will guide you through the process of setting one up.\n"));
    rl_print (i18n (2613, "You first need to enter a user account you want to use. This climm supports the following chat protocols:\n"));
    rl_print ("  *  ");
    rl_printf (i18n (2614, "%s, enter i.e. %s\n"), "ICQ", "12345678");
#ifdef ENABLE_XMPP
    rl_print ("  *  ");
    rl_printf (i18n (2614, "%s, enter i.e. %s\n"), "XMPP (Jabber, Google Talk)", "example@gmail.com");
#endif
#ifdef ENABLE_MSN
    rl_print ("  *  ");
    rl_printf (i18n (2614, "%s, enter i.e. %s\n"), "MSN", "fool@hotmale.net");
#endif
    while (1)
    {
        servertype = 0;
        while (!servertype)
        {
            rl_printf ("%s ", i18n (2615, "User account ID:"));
            fflush (stdout);
            ReadLineTtyUnset ();
            line = UtilIOReadline (stdin);
            ReadLineTtySet ();
            
            if (!line || !line->len)
            {
                if (!user)
                    continue;
                break;
            }

#ifdef ENABLE_XMPP
            if (is_valid_xmpp_name (line->txt))
                servertype = TYPE_XMPP_SERVER;
            else
#endif
            if (is_valid_icq_name (line->txt))
                servertype = TYPE_SERVER;
            else
#ifdef ENABLE_MSN
            if (is_valid_msn_name (line->txt))
                servertype = TYPE_MSN_SERVER;
            else
#endif
                rl_printf (i18n (2616, "Cannot parse %s as a valid user ID. Try again!\n"), line->txt);
        }
        if (user && !servertype)
            break;

        user = strdup (line->txt);
        
        rl_print ("\n");
        rl_printf (i18n (2617, "Next you can allow climm to store your password for %s account %s. Enter nothing to not save the password.\n"),
            ConnectionServerType (servertype), user);
        
        rl_printf ("%s ", i18n (1795, "Password:"));
        fflush (stdout);
        line = UtilIOReadline (stdin);
        rl_print ("\n");

        passwd = line && line->len ? strdup (c_out (line->txt)) : NULL;
        conn = PrefNewConnection (servertype, user, passwd);
        conn->flags |= CONN_WIZARD;
        free (user);
        s_free (passwd);
        rl_print (i18n (2618, "You may add more user accounts now. Enter nothing to not add more accounts.\n"));
    }

    prG->s5Use = 0;
    prG->s5Port = 0;

    rl_print (i18n (1784, "If you are firewalled, you may need to use a SOCKS5 server. If you do, please enter its hostname or IP address. Otherwise, or if unsure, just press return.\n"));
    rl_printf ("%s ", i18n (1094, "SOCKS5 server:"));
    fflush (stdout);
    ReadLineTtyUnset ();
    line = UtilIOReadline (stdin);
    if (line && line->len)
    {
        if (strchr (line->txt, ':'))
        {
            prG->s5Host = strdup (line->txt);
            t = strchr (prG->s5Host, ':');
            prG->s5Port = atoi (t + 1);
            *t = '\0';
        }
        else
        {
            prG->s5Host = strdup (line->txt);
            rl_print (i18n (1786, "I also need the port the socks server listens on. If unsure, press return for the default port.\n"));
            rl_printf ("%s ", i18n (1095, "SOCKS5 port:"));
            fflush (stdout);
            line = UtilIOReadline (stdin);
            if (line)
                sscanf (line->txt, "%hu", &prG->s5Port);
        }
        if (!prG->s5Port)
            prG->s5Port = 1080;

        prG->s5Use = 1;
        prG->s5Auth = 0;
        prG->s5Pass = NULL;
        prG->s5Name = NULL;

        rl_print ("\n");
        rl_print (i18n (1787, "You probably need to authenticate yourself to the socks server. If so, you need to enter the user name the administrator of the socks server gave you. Otherwise, just press return.\n"));
        rl_printf ("%s ", i18n (1096, "SOCKS5 user name:"));
        fflush (stdout);
        line = UtilIOReadline (stdin);
        if (line && line->len)
        {
            prG->s5Auth = 1;
            prG->s5Name = strdup (line->txt);
            rl_print (i18n (1788, "Now I also need the password for this user.\n"));
            rl_printf ("%s ", i18n (1097, "SOCKS5 password:"));
            fflush (stdout);
            line = UtilIOReadline (stdin);
            if (line && line->len)
                prG->s5Pass = strdup (line->txt);
        }
    }
    rl_print ("\n");
    ReadLineTtySet ();

    rl_print (i18n (1791, "Setup wizard finished. Congratulations!\n"));
    rl_print ("\n");
    
    {
        char *tmp = strdup (PrefUserDir (prG));
        if (tmp[strlen (tmp) - 1] == '/')
            tmp[strlen (tmp) - 1] = '\0';
        mkdir (tmp, 0700);
        free (tmp);
    }
#ifdef ENABLE_REMOTECONTROL
    conns = ConnectionC (TYPE_REMOTE);
    conns->open = &RemoteOpen;
    conns->flags |= CONN_AUTOLOGIN;
    conns->pref_server = strdup ("scripting");
    conns->server = strdup (conns->pref_server);
#endif

    prG->prompt    = strdup (USER_PROMPT);
    prG->prompt_strftime = strdup (USER_PROMPT_STRFTIME);
    prG->logplace  = strdup ("history" _OS_PATHSEPSTR);
    prG->chat      = 49;
    prG->autoupdate = AUTOUPDATE_CURRENT;
    prG->status = ims_online;
    prG->flags = FLAG_DELBS | FLAG_AUTOSAVE;
#ifdef ANSI_TERM
    prG->flags |= FLAG_COLOR;
#endif

    OptSetVal (&prG->copts, CO_LOGMESS,    1);
    OptSetVal (&prG->copts, CO_LOGONOFF,   1);
    OptSetVal (&prG->copts, CO_LOGCHANGE,  1);
    OptSetVal (&prG->copts, CO_SHOWONOFF,  1);
    OptSetVal (&prG->copts, CO_SHOWCHANGE, 1);
    OptSetVal (&prG->copts, CO_AUTOAUTO,   0);
    OptSetVal (&prG->copts, CO_WANTSBL,    1);
    OptSetVal (&prG->copts, CO_PEEKME,     0);
    OptSetVal (&prG->copts, CO_REVEALTIME, 600);

    OptSetStr (&prG->copts, CO_AUTODND,  i18n (1929, "User is dnd [Auto-Message]"));
    OptSetStr (&prG->copts, CO_AUTOAWAY, i18n (1010, "User is away [Auto-Message]"));
    OptSetStr (&prG->copts, CO_AUTONA,   i18n (1011, "User is not available [Auto-Message]"));
    OptSetStr (&prG->copts, CO_AUTOOCC,  i18n (1012, "User is occupied [Auto-Message]"));
    OptSetStr (&prG->copts, CO_AUTOFFC,  i18n (2055, "User is ffc and wants to chat about everything."));

    Save_RC ();
}

#define PrefParse(x)          switch (1) { case 1: if (!(par = s_parse (&args))) { rl_printf (i18n (2123, "%sSyntax error%s: Too few arguments: %s\n"), COLERROR, COLNONE, s_qquote (line->txt)); continue; } x = par->txt; }
#define PrefParseInt(i)       switch (1) { case 1: if (!s_parseint (&args, &i))  { rl_printf (i18n (2124, "%sSyntax error%s: Not an integer: %s\n"), COLERROR, COLNONE, s_qquote (line->txt)); continue; }}
#define ERROR continue;

/*
 * Reads in a configuration file.
 */
int Read_RC_File (FILE *rcf)
{
    char *tmp = NULL, *tmp2 = NULL, *cmd = NULL;
    strc_t line, par;
    const char *args, *p;
    Contact *cont = NULL, *lastcont = NULL;
    Connection *oldconn = NULL, *conn = NULL, *tconn;
#ifdef ENABLE_REMOTECONTROL
    Connection *connr = NULL;
#endif
    ContactGroup *cg = NULL;
    int section, dep = 0;
    UDWORD uin, i, j;
    UWORD flags;
    UBYTE enc = ENC_LATIN1, format = 0;

    for (section = 0; (line = UtilIOReadline (rcf)); )
    {
        if (!line->len || (line->txt[0] == '#'))
            continue;
        args = ConvFrom (line, enc)->txt;
        if (*args == '[')
        {
            if (!strcasecmp (args, "[General]"))
                section = 0;
            else if (!strcasecmp (args, "[Contacts]"))
                section = 1;
            else if (!strcasecmp (args, "[Strings]"))
                section = 2;
            else if (!strcasecmp (args, "[Connection]"))
            {
                section = 3;
                oldconn = conn;
                conn = ConnectionC (0);
            }
            else if (!strcasecmp (args, "[Group]"))
            {
                section = 4;
                cg = ContactGroupC (NULL, 0, NULL);
                OptSetVal (&cg->copts, CO_IGNORE, 0);
                if ((conn = ConnectionFind (TYPEF_SERVER, NULL, NULL)))
                    cg->serv = conn;
                cont = NULL;
            }
            else
            {
                rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
                rl_printf (i18n (1659, "Unknown section '%s' in configuration file."), args);
                rl_print ("\n");
                section = -1;
            }
            continue;
        }
        switch (section)
        {
            case -1:
                rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
                rl_print  (i18n (1675, "Ignored line:"));
                rl_printf (" %s\n", args);
                break;
            case 0:
                if (!(par = s_parse (&args)))
                    continue;
                cmd = par->txt;

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
                    {   what = ConvEnc ("UTF-8");        dep = 1; }
                    else if (!strcasecmp (cmd, "latin1"))
                    {   what = ConvEnc ("ISO-8859-1");   dep = 2; }
                    else if (!strcasecmp (cmd, "latin9"))
                    {   what = ConvEnc ("ISO-8859-15");  dep = 3; }
                    else if (!strcasecmp (cmd, "koi8"))
                    {   what = ConvEnc ("KOI8-U");       dep = 4; }
                    else if (!strcasecmp (cmd, "win1251"))
                    {   what = ConvEnc ("CP-1251");      dep = 5; }
                    else if (!strcasecmp (cmd, "euc"))
                    {   what = ConvEnc ("EUC-JP");       dep = 6; }
                    else if (!strcasecmp (cmd, "sjis"))
                    {   what = ConvEnc ("SHIFT-JIS");    dep = 7; }
                    else
                        what = ConvEnc (cmd);

                    if (!what)
                    {
                        rl_printf ("%s%s%s ", COLERROR, i18n (2251, "Error:"), COLNONE);
                        rl_printf (i18n (2216, "This climm doesn't know the '%s' encoding.\n"), cmd);
                        ERROR;
                    }
                    if (what & ENC_FERR)
                    {
                        if ((what & ~ENC_FLAGS) != ENC_UTF8)
                        {
                            if ((which == 3 && (what ^ prG->enc_loc) & ~ENC_FLAGS)
                                || (which == 2 && (what ^ enc) & ~ENC_FLAGS))
                                rl_printf ("%s%s%s ", COLERROR, i18n (2251, "Error:"), COLNONE);
                            else
                                rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
                            rl_printf (i18n (2217, "This climm can't convert between '%s' and unicode.\n"), cmd);
                        }
                        what &= ~ENC_FLAGS;
                    }
                    if (which == 1)
                    {
                        OptSetVal (&prG->copts, CO_ENCODING, what);
                        OptSetStr (&prG->copts, CO_ENCODINGSTR, ConvEncName (what));
                        dep = 17;
                    }
                    else if (which == 2)
                    {
                        if (ENC (enc_loc) != what)
                            prG->locale_broken = 2;
                        prG->enc_loc = what;
                    }
                    else
                    {
                        if (what != ENC_UTF8)
                            dep = 13;
                        enc = what;
                    }
                }
                else if (!strcasecmp (cmd, "format"))
                {
                    PrefParseInt (i);
                    format = i;
                    if (format > 2)
                    {
                        rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
                        rl_printf (i18n (2457, "Unknown configuration file format version %d.\n"), format);
                        return 0;
                    }
                    if (format == 2)
                        enc = ENC_UTF8;
                }
#ifdef ENABLE_TCL
                else if (!strcasecmp (cmd, "tclscript"))
                {
                    PrefParse (tmp);
                    TCLPrefAppend (TCL_FILE, tmp);
                }
                else if (!strcasecmp (cmd, "tcl"))
                {
                    PrefParse (tmp);
                    TCLPrefAppend (TCL_CMD, tmp);
                }
#endif
                else if (!strcasecmp (cmd, "receivescript") || !strcasecmp (cmd, "event"))
                {
                    if (!strcasecmp (cmd, "receivescript"))
                        dep = 8;
                    if (!(par = s_parse (&args)))
                    {
                        dep = 9;
                        prG->event_cmd = NULL;
                        continue;
                    }
                    tmp = par->txt;
                    if (!strcmp (tmp, "off") || !strcmp (tmp, i18n (1086, "off")) || !*tmp)
                        prG->event_cmd = NULL;
                    else
                    {
                        prG->event_cmd = strdup (tmp);
#ifndef MSGEXEC
                        rl_printf (COLERROR "%s" COLNONE " ", i18n (1619, "Warning:"));
                        rl_printf (i18n (1817, "The event scripting feature is disabled.\n"));
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
                else if (!strcasecmp (cmd, "prompt_strftime"))
                {
                    PrefParse (tmp);
                    prG->prompt_strftime = strdup (tmp);
                }
                else if (!strcasecmp (cmd, "autoupdate"))
                {
                    PrefParseInt (i);
                    prG->autoupdate = i;
                }
                else if (!strcasecmp (cmd, "color") || !strcasecmp (cmd, "colour"))
                {
                    dep = 235;

                    PrefParse (tmp);

                    if (!strcasecmp (tmp, "scheme"))
                    {
                        PrefParseInt (i);
                        OptImport (&prG->copts, PrefSetColorScheme (1 + ((i + 3) % 4)));
                    }
                    else
                        OptImport (&prG->copts, s_sprintf ("color%s %s", tmp, s_quote (args)));
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
                    if (!(par = s_parse (&args)))
                    {
                        dep = 10;
                        prG->flags |= FLAG_AUTOREPLY;
                        continue;
                    }
                    tmp = par->txt;
                    
                    if (!strcasecmp (tmp, "on"))
                        prG->flags |= FLAG_AUTOREPLY;
                    else if (!strcasecmp (tmp, "off"))
                        prG->flags &= ~FLAG_AUTOREPLY;
                    else if (!strcasecmp (tmp, "away"))
                    {
                        PrefParse (tmp);
                        OptSetStr (&prG->copts, CO_AUTOAWAY, tmp);
                        dep = 1;
                    }
                    else if (!strcasecmp (tmp, "na"))
                    {
                        PrefParse (tmp);
                        OptSetStr (&prG->copts, CO_AUTONA, tmp);
                        dep = 1;
                    }
                    else if (!strcasecmp (tmp, "dnd"))
                    {
                        PrefParse (tmp);
                        OptSetStr (&prG->copts, CO_AUTODND, tmp);
                        dep = 1;
                    }
                    else if (!strcasecmp (tmp, "occ"))
                    {
                        PrefParse (tmp);
                        OptSetStr (&prG->copts, CO_AUTOOCC, tmp);
                        dep = 1;
                    }
                    else if (!strcasecmp (tmp, "inv"))
                    {
                        PrefParse (tmp);
                        dep = 1;
                    }
                    else if (!strcasecmp (tmp, "ffc"))
                    {
                        PrefParse (tmp);
                        OptSetStr (&prG->copts, CO_AUTOFFC, tmp);
                        dep = 1;
                    }
                    else
                        ERROR;
                }
                else if (!strcasecmp (cmd, "sound"))
                {
                    if (!(par = s_parse (&args)))
                    {
                        prG->sound = SFLAG_BEEP;
                        dep = 11;
                        continue;
                    }
                    tmp = par->txt;
                    if (!strcasecmp (tmp, "on") || !strcasecmp (tmp, "beep"))
                        prG->sound = SFLAG_BEEP;
                    else if (!strcasecmp (tmp, "event"))
                        prG->sound = SFLAG_EVENT;
                    else
                        prG->sound = 0;
                }
                else if (!strcasecmp (cmd, "soundonline"))
                    dep = 12;
                else if (!strcasecmp (cmd, "soundoffline"))
                    dep = 13;
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
                    dep = 234;
                    PrefParse (tmp);
                    rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
                    rl_printf (i18n (2458, "Can't tab spool %s; type \"%sopt %s tabspool 1%s\" manually.\n"),
                              s_qquote (tmp), COLQUOTE, tmp, COLNONE);
                }
                else if (!strcasecmp (cmd, "set"))
                {
                    int which = 0;
                    
                    PrefParse (cmd);

                    if (!strcasecmp (cmd, "color") || !strcasecmp (cmd, "colour"))
                        which = FLAG_COLOR;
                    else if (!strcasecmp (cmd, "hermit"))
                        which = FLAG_DEP_HERMIT;
                    else if (!strcasecmp (cmd, "delbs"))
                        which = FLAG_DELBS;
                    else if (!strcasecmp (cmd, "russian"))
                        which = FLAG_DEP_CONVRUSS;
                    else if (!strcasecmp (cmd, "japanese"))
                        which = FLAG_DEP_CONVEUC;
                    else if (!strcasecmp (cmd, "funny"))
                        which = FLAG_FUNNY;
                    else if (!strcasecmp (cmd, "log"))
                        which = FLAG_DEP_LOG;
                    else if (!strcasecmp (cmd, "loglevel"))
                        which = -1;
                    else if (!strcasecmp (cmd, "logonoff"))
                        which = FLAG_DEP_LOG_ONOFF;
                    else if (!strcasecmp (cmd, "auto"))
                        which = FLAG_AUTOREPLY;
                    else if (!strcasecmp (cmd, "uinprompt"))
                        which = FLAG_UINPROMPT;
                    else if (!strcasecmp (cmd, "prompt"))
                        which = -9;
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
                    else if (!strcasecmp (cmd, "webaware"))
                        which = -5;
                    else if (!strcasecmp (cmd, "hideip"))
                        which = -6;
                    else if (!strcasecmp (cmd, "dcauth"))
                        which = -7;
                    else if (!strcasecmp (cmd, "dccont"))
                        which = -8;
                    else
                        ERROR;
                    if (which > 0)
                    {
                        PrefParse (cmd);
                        if (!strcasecmp (cmd, "on"))
                        {
                            if (which == FLAG_DEP_CONVRUSS)
                            {
                                OptSetVal (&prG->copts, CO_ENCODING, ENC_WIN1251);
                                OptSetStr (&prG->copts, CO_ENCODINGSTR, ConvEncName (ENC_WIN1251));
                                dep = 14;
                            }
                            else if (which == FLAG_DEP_CONVEUC)
                            {
                                dep = 15;
                                OptSetVal (&prG->copts, CO_ENCODING, ENC_SJIS);
                                OptSetStr (&prG->copts, CO_ENCODINGSTR, ConvEncName (ENC_SJIS));
#ifndef ENABLE_ICONV
                                rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
                                rl_print (i18n (2215, "This climm can't convert between SJIS or EUC and unicode.\n"));
#endif
                            }
                            else if (which == FLAG_DEP_LOG)
                            {
                                dep = 16;
                                OptSetVal (&prG->copts, CO_LOGMESS, 1);
                            }
                            else if (which == FLAG_DEP_LOG_ONOFF)
                            {
                                dep = 17;
                                OptSetVal (&prG->copts, CO_LOGONOFF, 1);
                                OptSetVal (&prG->copts, CO_LOGCHANGE, 1);
                            }
                            else if (which == FLAG_DEP_HERMIT)
                            {
                                dep = 18;
                                OptSetVal (&prG->copts, CO_IGNORE, 1);
                            }
                            else
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

                        if (i)
                        {
                            OptSetVal (&prG->copts, CO_LOGMESS, 1);
                        }
                        if (i & 2)
                        {
                            OptSetVal (&prG->copts, CO_LOGONOFF, 1);
                            OptSetVal (&prG->copts, CO_LOGCHANGE, 1);
                        }
                        dep = 16;
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
                        dep = 8374;
                    }
                    else if (which == -4)
                    {
                        PrefParse (cmd);
                        
                        if (!strcasecmp (cmd, "on"))
                            OptSetVal (&prG->copts, CO_SHOWCHANGE, 0);
                        else if (!strcasecmp (cmd, "complete"))
                        {
                            OptSetVal (&prG->copts, CO_SHOWCHANGE, 0);
                            OptSetVal (&prG->copts, CO_SHOWONOFF, 0);
                        }
                        dep = 18;
                    }
                    else if (which == -5)
                    {
                        OptSetVal (&prG->copts, CO_WEBAWARE, 1);
                        dep = 3247;
                    }
                    else if (which == -6)
                    {
                        OptSetVal (&prG->copts, CO_HIDEIP, 1);
                        dep = 2345;
                    }
                    else if (which == -7)
                    {
                        OptSetVal (&prG->copts, CO_DCAUTH, 1);
                        dep = 3274;
                    }
                    else if (which == -8)
                    {
                        OptSetVal (&prG->copts, CO_DCCONT, 1);
                        dep = 8723;
                    }
                    else if (which == -9)
                    {
                        PrefParse (cmd);

                        prG->flags &= ~FLAG_USERPROMPT & ~FLAG_UINPROMPT;
                        if (!strcasecmp (cmd, "user"))
                            prG->flags |= FLAG_USERPROMPT;
                        else if (!strcasecmp (cmd, "uin"))
                            prG->flags |= FLAG_UINPROMPT;
                        else if (!strcasecmp (cmd, "simple"))
                            ;
                        else
                            ERROR;
                    }
                }
                else if (!strcasecmp (cmd, "options"))
                {
                    if (OptImport (&prG->copts, args))
                        ERROR;
                }
                else
                {
                    rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
                    rl_printf (i18n (1188, "Unrecognized command '%s' in configuration file, ignored."), cmd);
                    rl_print ("\n");
                }
                break;
            case 1:
                if (format > 1)
                    continue;

                flags = 0;
                
                for (p = args; *p && *p != '#'; p++)
                {
                    if (!*p || *p == '#' || isdigit ((int) *p))
                        break;

                    switch (*p)
                    {
                        case '*':
                            flags |= CO_INTIMATE;
                            flags &= ~(CO_HIDEFROM & CO_BOOLMASK);
                            continue;
                        case '^':
                            flags |= CO_IGNORE;
                            continue;
                        case '~':
                            flags |= CO_HIDEFROM;
                            flags &= ~(CO_INTIMATE & CO_BOOLMASK);
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

                if (isdigit ((int)*p))
                {
                    PrefParseInt (uin);
                    PrefParse (cmd);
                }
                else
                {
                    if (!lastcont)     /* ignore noise */
                    {
                        dep = 1;
                        continue;
                    }
                    uin = lastcont->uin;
                    PrefParse (cmd);
                }
                
                for (i = j = 0; (tconn = ConnectionNr (i)); i++)
                {
                    if (~tconn->type & TYPEF_ANY_SERVER)
                        continue;

                    if ((cont = ContactFindUIN (tconn->contacts, uin)))
                    {
                        j = 1;
                        ContactAddAlias (cont, cmd);
                        OptSetVal (&cont->copts, flags, 1); /* FIXME */
                    }
                }
                if (!j)
                {
                    dep = 19;
                    for (i = 0; (tconn = ConnectionNr (i)); i++)
                        if (tconn->type & TYPEF_ANY_SERVER)
                            break;
                    if (!tconn)
                        break;
                    if (!(cont = ContactFindUIN (tconn->contacts, uin)))
                    {
                        rl_printf ("%s%s%s %s\n", COLERROR, i18n (1619, "Warning:"), COLNONE,
                                   i18n (2118, "Out of memory.\n"));
                        section = -1;
                        break;
                    }
                    ContactCreate (tconn, cont);
                    ContactAddAlias (cont, cmd);
                    OptSetVal (&cont->copts, flags, 1); /* FIXME */
                }
                break;
            case 2:
                PrefParse (cmd);

                if (!strcasecmp (cmd, "alter"))
                {
                    PrefParse (tmp);
                    tmp = strdup (tmp);
                    PrefParse (tmp2);
                    tmp2 = strdup (s_quote (tmp2));
                    
                    CmdUser (cmd = strdup (s_sprintf ("\\alias %s %s", tmp2, tmp)));

                    free (cmd);
                    free (tmp);
                    free (tmp2);
                }
                else if (!strcasecmp (cmd, "alias"))
                {
                    char autoexpand = FALSE;
                    if (s_parsekey (&args, "auto") || s_parsekey (&args, "autoexpand"))
                        autoexpand = TRUE;
                    PrefParse (tmp);
                    tmp = strdup (s_quote (tmp));
                    PrefParse (tmp2);
                    tmp2 = strdup (tmp2);

                    CmdUser (cmd = strdup (s_sprintf ("\\alias %s%s%s%s %s",
                        autoexpand ? "autoexpand " : "",
                        *tmp == '"' ? "" : "\"", tmp, *tmp == '"' ? "" : "\"", tmp2)));
                    
                    free (cmd);
                    free (tmp);
                    free (tmp2);
                }
                else if (!strcasecmp (cmd, "prompt"))
                {
                    PrefParse (cmd);
                    prG->prompt = strdup (cmd);
                }
                else
                {
                    rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
                    rl_printf (i18n (1188, "Unrecognized command '%s' in configuration file, ignored."), cmd);
                    rl_print ("\n");
                }
                break;
            case 3:
                PrefParse (cmd);

                if (!strcasecmp (cmd, "type"))
                {
                    PrefParse (cmd);

                    if (!strcasecmp (cmd, "server"))
                    {
                        conn->type =
                            (conn->version ? (conn->version > 6 
                               ? TYPE_SERVER : TYPE_SERVER_OLD) : conn->type);
                        dep = 20;
                    }
                    else if (!(conn->type = ConnectionServerNType (cmd, 0)))
                        ERROR;
                    
                    if (conn->type == TYPE_MSGLISTEN)
                    {
                        conn->pref_status = TCP_OK_FLAG;
                        if (oldconn->type == TYPE_SERVER || oldconn->type == TYPE_SERVER_OLD)
                        {
                            oldconn->assoc = conn;
                            conn->parent = oldconn;
                        }
                        else
                        {
                            rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
                            rl_printf (i18n (2459, "Peer-to-peer connection not associated to server connection, discarding.\n"));
                            conn->type = 0;
                            section = -1;
                        }
                    }

                    if ((par = s_parse (&args)))
                    {
                        if (!strcasecmp (par->txt, "auto"))
                            conn->flags |= CONN_AUTOLOGIN;
                    }
                }
                else if (!strcasecmp (cmd, "version"))
                {
                    PrefParseInt (i);

                    conn->version = i;
                    if (!conn->type)
                    {
                        if (conn->version > 6)
                            conn->type = TYPE_SERVER;
                        else
                            conn->type = TYPE_SERVER_OLD;
                    }
                }
                else if (!strcasecmp (cmd, "server"))
                {
                    PrefParse (tmp);
                    s_repl (&conn->pref_server, tmp);
                }
                else if (!strcasecmp (cmd, "port"))
                {
                    PrefParseInt (i);
                    conn->pref_port = i;
                }
                else if (!strcasecmp (cmd, "uin"))
                {
                    PrefParseInt (i);
                    conn->uin = i;
                    s_repl (&conn->screen, s_sprintf ("%lu", i));
                    dep = 77;
                }
                else if (!strcasecmp (cmd, "screen"))
                {
                    PrefParse (tmp);
                    s_repl (&conn->screen, tmp);
                    conn->uin = IcqIsUIN (tmp);
                }
                else if (!strcasecmp (cmd, "password"))
                {
                    PrefParse (tmp);
                    s_repl (&conn->pref_passwd, tmp);
                }
                else if (!strcasecmp (cmd, "status"))
                {
                    if (!ContactStatus (&args, &conn->pref_status))
                    {
                        PrefParseInt (i);
                        conn->pref_status = IcqToStatus (i);
                    }
                }
                else
                {
                    rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
                    rl_printf (i18n (1188, "Unrecognized command '%s' in configuration file, ignored."), cmd);
                    rl_print ("\n");
                }
                break;
            case 4: /* contact groups */
                if (format > 1) /* contacts are in seperate file now */
                    continue;

                PrefParse (cmd);
                if (!strcasecmp (cmd, "label") && !cg->used)
                {
                    Connection *conn = NULL;
                    PrefParse (tmp);
                    s_repl (&cg->name, tmp);
                    if (!strncmp (cg->name, "contacts-", 9))
                    {
                        UWORD type;
                        if ((type = ConnectionServerNType (cg->name + 9, '-')))
                        {
                            int len = strlen (ConnectionServerType (type));
                            conn = ConnectionFindScreen (type, cg->name + len + 10);
                            if (conn)
                            {
                                if (conn->contacts && conn->contacts != cg) /* Duplicate! */
                                {
                                    ContactGroupD (cg);
                                    cg = conn->contacts;
                                }
                                else
                                {
                                    cg->serv = conn;
                                    cg->serv->contacts = cg;
                                }
                            }
                        }
                    }
                    if (!cg->serv)
                        if ((conn = ConnectionFind (TYPEF_SERVER, NULL, NULL)))
                            cg->serv = conn;
                }
                else if (!strcasecmp (cmd, "id") && !cg->used)
                {
                    PrefParseInt (i);
                    cg->id = i;
                }
                else if (!strcasecmp (cmd, "server") && !cg->used)
                {
                    Connection *conn;
                    UWORD type;
                    
                    PrefParse (tmp);
                    if (!(type = ConnectionServerNType (tmp, 0)))
                        ERROR;
                    
                    PrefParse (tmp);
                    
                    if ((conn = ConnectionFindScreen (type, tmp)))
                        cg->serv = conn;
                }
                else if (!cg->serv)
                {
                    rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
                    rl_printf (i18n (2460, "Contact group %s (id %s%d%s) not associated to server connection.\n"),
                              s_wordquote (cg->name ? cg->name : ""), COLQUOTE, cg->id, COLNONE);
                }
                else if (!strcasecmp (cmd, "entry"))
                {
                    UDWORD uin;
                    
                    PrefParseInt (i);
                    PrefParseInt (uin);
                    
                    if (!(cont = ContactFindUIN (cg->serv->contacts, uin)))
                        break;
                    ContactCreate (cg->serv, cont);
                    if (cont && cg != cg->serv->contacts)
                        ContactAdd (cg, cont);
                }
                else
                {
                    rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
                    rl_printf (i18n (1188, "Unrecognized command '%s' in configuration file, ignored."), cmd);
                    rl_print ("\n");
                }
                break;
        }
    }
    
    if (!prG->logplace)
        prG->logplace = strdup ("history" _OS_PATHSEPSTR);
    
    if (!prG->chat)
        prG->chat = 49;

    if (!prG->prompt)
        prG->prompt = strdup (USER_PROMPT);

    if (!prG->prompt_strftime)
        prG->prompt_strftime = strdup (USER_PROMPT_STRFTIME);

    for (i = 0; (conn = ConnectionNr (i)); i++)
    {
        conn->port   = conn->pref_port;
        s_repl (&conn->server, conn->pref_server);
        s_repl (&conn->passwd, conn->pref_passwd);
        conn->status = conn->pref_status;
        if (prG->s5Use && conn->type & TYPEF_SERVER && !conn->version)
            conn->version = 2;
        switch (conn->type)
        {
            case TYPE_SERVER:
                conn->open = &ConnectionInitServer;
                break;
            case TYPE_SERVER_OLD:
                conn->open = &ConnectionInitServerV5;
                break;
#ifdef ENABLE_MSN
            case TYPE_MSN_SERVER:
                conn->open = &ConnectionInitMSNServer;
                break;
#endif
#ifdef ENABLE_XMPP
            case TYPE_XMPP_SERVER:
                conn->open = &ConnectionInitXMPPServer;
                break;
#endif
#ifdef ENABLE_PEER2PEER
            case TYPE_MSGLISTEN:
                conn->open = &ConnectionInitPeer;
                break;
#endif
#ifdef ENABLE_REMOTECONTROL
            case TYPE_REMOTE:
                conn->open = &RemoteOpen;
                connr = conn;
                break;
#endif
            default:
                conn->type = 0;
                conn->open = NULL;
                break;
        }
        if (format < 2 && !conn->contacts && conn->type & TYPEF_SERVER)
        {
            conn->contacts = cg = ContactGroupC (conn, 0, s_sprintf ("contacts-%s-%s", ConnectionServerType (conn->type), conn->screen));
            for (i = 0; (cont = ContactIndex (NULL, i)); i++)
                ContactCreate (conn, cont);
            dep = 21;
        }
    }

#ifdef ENABLE_REMOTECONTROL
    if (!connr)
    {
        connr = ConnectionC (TYPE_REMOTE);
        connr->open = &RemoteOpen;
        connr->pref_server = strdup ("scripting");
        connr->parent = NULL;
        connr->server = strdup (connr->pref_server);
        dep = 22;
    }
#endif
                           
    if (dep || !format)
    {
        rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
        rl_printf (i18n (2461, "Deprecated syntax found in configuration file '%s'!\n    Please update or \"save\" the configuration file and check for changes.\n"), prG->rcfile);
        rl_printf ("FIXME: dep %d\n", dep);
    }
    fclose (rcf);
    return format;
}


/*
 * Reads in a status file.
 */
void PrefReadStat (FILE *stf)
{
    char *cmd = NULL;
    const char *args;
    Contact *cont = NULL;
    Connection *conn = NULL;
    ContactGroup *cg = NULL;
    int section, dep = 0;
    UDWORD i;
    strc_t par, line;
    UBYTE format = 0;
    UDWORD uinconts = 0;
    Contact *uincont[20];

    for (section = 0; (line = UtilIOReadline (stf)); )
    {
        if (!line->len || (line->txt[0] == '#'))
            continue;
        args = ConvFrom (line, ENC_UTF8)->txt;
        if (format && *args == '[')
        {
            if (!strcasecmp (args, "[Group]"))
            {
                section = 4;
                cg = ContactGroupC (NULL, 0, NULL);
                OptSetVal (&cg->copts, CO_IGNORE, 0);
                if ((conn = ConnectionFind (TYPEF_ANY_SERVER, NULL, NULL)))
                    cg->serv = conn;
            }
            else if (!strcasecmp (args, "[Contacts]"))
            {
                if (format == 1)
                    section = 5;
                else
                    section = 6;
                conn = NULL;
            }
            else
            {
                rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
                rl_printf (i18n (1659, "Unknown section '%s' in configuration file."), args);
                rl_print ("\n");
                section = -1;
            }
            continue;
        }
        switch (section)
        {
            case -1:
                rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
                rl_print  (i18n (1675, "Ignored line:"));
                rl_printf (" %s\n", args);
                break;
            case 0:
                PrefParse (cmd);
                if (!strcasecmp (cmd, "format"))
                {
                    PrefParseInt (i);
                    format = i;
                    if (format > 3)
                    {
                        rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
                        rl_printf (i18n (2457, "Unknown configuration file format version %d.\n"), format);
                        return;
                    }
                }
                else
                {
                    rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
                    rl_printf (i18n (1188, "Unrecognized command '%s' in configuration file, ignored."), cmd);
                    rl_print ("\n");
                }
                break;
            case 4: /* contact groups */
                PrefParse (cmd);
                if (!strcasecmp (cmd, "label") && !cg->used)
                {
                    PrefParse (cmd);
                    s_repl (&cg->name, cmd);
                    if (!strncmp (cg->name, "contacts-", 9))
                    {
                        UWORD type;
                        int len;
                        
                        if (!(type = ConnectionServerNType (cg->name + 9, '-')))
                            break;

                        len = strlen (ConnectionServerType (type));
                        if ((conn = ConnectionFindScreen (type, cg->name + 10 + len)))
                        {
                            if (conn->contacts && conn->contacts != cg)
                            {
                                ContactGroupD (cg);
                                cg = conn->contacts;
                            }
                            else
                            {
                                conn->contacts = cg;
                                cg->serv = conn;
                            }
                        }
                    }
                    if (!cg->serv)
                        cg->serv = ConnectionFind (TYPEF_SERVER, NULL, NULL);
                }
                else if (!strcasecmp (cmd, "id") && !cg->used)
                {
                    PrefParseInt (i);
                    cg->id = i;
                }
                else if (!strcasecmp (cmd, "server") && !cg->used)
                {
                    UWORD type;
                    
                    PrefParse (cmd);
                    if (!(type = ConnectionServerNType (cmd, 0)))
                        ERROR;
                    
                    PrefParse (cmd);
                    if ((conn = ConnectionFindScreen (type, cmd)))
                        cg->serv = conn;
                }
                else if (!strcasecmp (cmd, "options"))
                {
                    if (OptImport (&cg->copts, args))
                        ERROR;
                }
                else if (cg->serv && !strcasecmp (cmd, "entry"))
                {
                    if (format < 3)
                        PrefParseInt (i);
                    PrefParse (cmd);
                    
                    if (!cg->serv->contacts)
                    {
                        cg->serv->contacts = ContactGroupC (NULL, 0, s_sprintf ("contacts-%s-%s", ConnectionServerType (cg->serv->type), cmd));
                        OptSetVal (&cg->serv->contacts->copts, CO_IGNORE, 0);
                        cg->serv->contacts->serv = cg->serv;
                    }
                    
                    if (format == 1)
                        cont = ContactFindUIN (cg->serv->contacts, atol (cmd));
                    else
                        cont = ContactScreen (cg->serv, cmd);
                    
                    if (!cont)
                    {
                        rl_printf ("%s%s%s %s\n", COLERROR, i18n (1619, "Warning:"), COLNONE,
                                   i18n (2118, "Out of memory.\n"));
                        ERROR;
                    }
                    ContactCreate (cg->serv, cont);

                    if (cg != cg->serv->contacts)
                    {
                        ContactAdd (cg, cont);
                        if (cont->group == cg->serv->contacts)
                            cont->group = cg;
                    }

                    while ((par = s_parse (&args)))
                        ContactAddAlias (cont, par->txt);
                }
                else
                {
                    rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
                    rl_printf (i18n (1188, "Unrecognized command '%s' in configuration file, ignored."), cmd);
                    rl_print ("\n");
                }
                break;
            case 5:
                PrefParse (cmd);
                if (!strcasecmp (cmd, "entry"))
                {
                    UDWORD uin;
                    
                    PrefParseInt (uin);
                    PrefParse (cmd);
                    
                    for (i = uinconts = 0; (conn = ConnectionNr (i)); i++)
                    {
                        if (conn->type & TYPEF_ANY_SERVER && conn->contacts && uinconts < 20
                            && (cont = ContactFindUIN (conn->contacts, uin)))
                        {
                            uincont[uinconts++] = cont;
                            ContactAddAlias (cont, cmd);
                        }
                    }
                    while ((par = s_parse (&args)))
                    {
                        for (i = 0; i < uinconts; i++)
                            ContactAddAlias (uincont[i], par->txt);
                    }
                }
                else if (!strcasecmp (cmd, "options"))
                {
                    for (i = 0; i < uinconts; i++)
                        OptImport (&uincont[i]->copts, args);
                }
                else
                {
                    rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
                    rl_printf (i18n (1188, "Unrecognized command '%s' in configuration file, ignored."), cmd);
                    rl_print ("\n");
                }
                break;
            case 6:
                PrefParse (cmd);
                if (!strcasecmp (cmd, "server"))
                {
                    UWORD type;
                    
                    PrefParse (cmd);
                    if (!(type = ConnectionServerNType (cmd, 0)))
                        ERROR;

                    PrefParse (cmd);
                    if (!(conn = ConnectionFindScreen (type, cmd)))
                        ERROR;
                    cont = NULL;
                    if (!conn->contacts)
                    {
                        conn->contacts = ContactGroupC (conn, 0, s_sprintf ("contacts-%s-%s", ConnectionServerType (conn->type), conn->screen));
                        OptSetVal (&conn->contacts->copts, CO_IGNORE, 0);
                        conn->contacts->serv = conn;
                    }
                }
                else if (conn && !strcasecmp (cmd, "entry"))
                {
                    UDWORD id;
                    
                    if (format < 3)
                        PrefParseInt (id);
                    PrefParse (cmd);
                    
                    cont = ContactScreen (conn, cmd);
                    if (!cont)
                        ERROR;
                    ContactCreate (conn, cont);
                    
                    while ((par = s_parse (&args)))
                        ContactAddAlias (cont, par->txt);
                }
                else if (conn && cont && !strcasecmp (cmd, "options"))
                {
                    OptImport (&cont->copts, args);
                }
                else if (conn && !cont && !strcasecmp (cmd, "options"))
                {
                    OptImport (&conn->contacts->copts, args);
                }
                else
                {
                    rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
                    rl_printf (i18n (1188, "Unrecognized command '%s' in configuration file, ignored."), cmd);
                    rl_print ("\n");
                }
                break;
        }
    }
    
    for (i = 0; (conn = ConnectionNr (i)); i++)
    {
        if (conn->type & TYPEF_ANY_SERVER)
        {
            if (!conn->contacts)
            {
                conn->contacts = cg = ContactGroupC (conn, 0, s_sprintf ("contacts-%s-%s", ConnectionServerType (conn->type), conn->screen));
                OptSetVal (&cg->copts, CO_IGNORE, 0);
                dep = 21;
            }
            if (!conn->cont)
                conn->cont = ContactScreen (conn, conn->screen);
        }
    }

    if (dep || !format)
    {
        rl_printf (i18n (1818, "Warning: Deprecated syntax found in configuration file '%s'!\n    Please update or \"save\" the configuration file and check for changes.\n"), prG->statfile);
        rl_printf ("FIXME: dep %d\n", dep);
    }
    fclose (stf);
}

int PrefWriteStatusFile (void)
{
    FILE *stf;
    char *stfn, *stfo;
    time_t t;
    int i, k, rc;
    Contact *cont;
    Connection *ss;
    ContactGroup *cg;

    if (!prG->statfile)
        prG->statfile = strdup (s_sprintf ("%sstatus", PrefUserDir (prG)));
    rl_printf (i18n (2048, "Saving preferences to %s.\n"), prG->statfile);
    stfn = strdup (s_sprintf ("%s.##", prG->statfile));
    stf = fopen (stfn, "w");
    if (!stf)
    {
        int rc = errno;
        if (rc == ENOENT)
        {
            char *tmp = strdup (PrefUserDir (prG));
            if (tmp[strlen (tmp) - 1] == '/')
                tmp[strlen (tmp) - 1] = '\0';
            rl_printf (i18n (2047, "Creating directory %s.\n"), tmp);
            k = mkdir (tmp, 0700);
            if (!k)
                stf = fopen (stfn, "w");
            if (!stf)
                rc = errno;
            free (tmp);
        }
        if (!stf)
            rl_printf (i18n (1872, "failed: %s (%d)\n"), strerror (rc), rc);
    }
    if (!stf)
    {
        s_free (stfn);
        return -1;
    }

    t = time (NULL);
    fprintf (stf, "# This file was generated by climm " CLIMM_VERSION " of %s %s\n", __TIME__, __DATE__);
    fprintf (stf, "# This file was generated on %s\n", ctime (&t));
    fprintf (stf, "format 3\n\n");

    fprintf (stf, "\n# The contact list section.\n");
    for (k = 0; (ss = ConnectionNr (k)); k++)
    {
        if (~ss->type & TYPEF_ANY_SERVER)
            continue;
        if (!ss->flags)
            continue;
        if (!ss->uin && (ss->type & TYPEF_HAVEUIN))
            continue;

        fprintf (stf, "[Contacts]\n");
        fprintf (stf, "server %s %s\n", ConnectionServerType (ss->type), ss->screen);
        fprintf (stf, "%s\n", OptString (&ss->contacts->copts));

        for (i = 0; (cont = ContactIndex (0, i)); i++)
        {
            if (cont->group && cont->group->serv == ss)
            {
                ContactAlias *alias;
                fprintf (stf, "entry %s ", s_quote (cont->screen));
                fprintf (stf, "%s", s_quote (cont->nick));
                for (alias = cont->alias; alias; alias = alias->more)
                    fprintf (stf, " %s", s_quote (alias->alias));
                fprintf (stf, "\n%s", OptString (&cont->copts));
            }
        }
        fprintf (stf, "\n");
    }
    
    fprintf (stf, "# Contact groups.");
    for (k = 1; (cg = ContactGroupIndex (k)); k++)
    {
        if (!cg->serv || cg->serv->contacts == cg)
            continue;

        fprintf (stf, "\n[Group]\n");
        fprintf (stf, "server %s %s\n", ConnectionServerType (cg->serv->type), cg->serv->screen);
        fprintf (stf, "label %s\n", s_quote (cg->name));
        fprintf (stf, "id %d\n", cg->id);
        fprintf (stf, "%s", OptString (&cg->copts));

        for (i = 0; (cont = ContactIndex (cg, i)); i++)
            fprintf (stf, "entry %s\n", s_quote (cont->screen));
    }

    if (fclose (stf))
    {
        s_free (stfn);
        return -1;
    }
    
    stfo = strdup (s_sprintf ("%s.old", prG->statfile));
    rename (prG->statfile, stfo);
    s_free (stfo);
    
    rc = rename (stfn, prG->statfile);
    s_free (stfn);
    return rc;
}

int PrefWriteConfFile (void)
{
    FILE *rcf;
    char *rcfn, *rcfo;
    time_t t;
    int k, rc;
    Connection *ss;
#ifdef ENABLE_TCL
    tcl_pref_p tpref;
#endif

    if (!prG->rcfile)
        prG->rcfile = strdup (s_sprintf ("%sclimmrc", PrefUserDir (prG)));
    rl_printf (i18n (2048, "Saving preferences to %s.\n"), prG->rcfile);
    rcfn = strdup (s_sprintf ("%s.##", prG->rcfile));
    rcf = fopen (rcfn, "w");
    if (!rcf)
    {
        int rc = errno;
        if (rc == ENOENT)
        {
            char *tmp = strdup (PrefUserDir (prG));
            if (tmp[strlen (tmp) - 1] == '/')
                tmp[strlen (tmp) - 1] = '\0';
            rl_printf (i18n (2047, "Creating directory %s.\n"), tmp);
            k = mkdir (tmp, 0700);
            if (!k)
                rcf = fopen (rcfn, "w");
            if (!rcf)
                rc = errno;
            free (tmp);
        }
        if (!rcf)
            rl_printf (i18n (1872, "failed: %s (%d)\n"), strerror (rc), rc);
    }
    if (!rcf)
    {
        s_free (rcfn);
        return -1;
    }

    t = time (NULL);
    fprintf (rcf, "# This file was generated by climm " CLIMM_VERSION " of %s %s\n", __TIME__, __DATE__);
    fprintf (rcf, "# This file was generated on %s\n", ctime (&t));

    fprintf (rcf, "encoding file UTF-8 # do not modify\n");
    fprintf (rcf, "# Local character encodings: auto, ISO-8859-1, KOI8-u, ...\n");
    fprintf (rcf, "# Setting this explicitly will render climm unable to determine character widths.\n");
    fprintf (rcf, "%sencoding local %s%s\n",
             prG->enc_loc & ENC_FLAGS ? "# " : "", s_quote (ConvEncName (prG->enc_loc)),
             prG->enc_loc & ENC_FLAGS ? "" : " # please set your locale correctly instead");
    fprintf (rcf, "format 2\n\n");

    for (k = 0; (ss = ConnectionNr (k)); k++)
    {
        if (!ss->flags)
            continue;
        if (!strcmp (ConnectionServerType (ss->type), "unknown"))
            continue;
        if (ss->type == TYPE_MSGLISTEN && (!ss->parent || !ss->parent->uin))
            continue;

        fprintf (rcf, "[Connection]\n");
        fprintf (rcf, "type %s%s\n",  ConnectionServerType (ss->type),
                                      ss->flags & CONN_AUTOLOGIN ? " auto" : "");
        fprintf (rcf, "version %d\n", ss->version);
        if (ss->pref_server)
            fprintf (rcf, "server %s\n",   s_quote (ss->pref_server));
        if (ss->pref_port && ss->pref_port != -1)
            fprintf (rcf, "port %ld\n",    ss->pref_port);
        if (ss->type & TYPEF_ANY_SERVER)
            fprintf (rcf, "screen %s\n",   s_quote (ss->screen));
        if (ss->pref_passwd)
            fprintf (rcf, "password %s\n", s_quote (ss->pref_passwd));
        else if (!k)
            fprintf (rcf, "# password\n");
        if (ss->pref_status || !k)
            fprintf (rcf, "status %s\n",  ContactStatusStr (ss->pref_status));
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

#ifdef ENABLE_TCL
    tpref = prG->tclscript;
    while (tpref)
    {
        fprintf (rcf, "%s \"%s\"\n", tpref->type == TCL_FILE ? "tclscript" : "tcl",
                        tpref->file);
        tpref = tpref->next;
    }
    fprintf (rcf, "\n");
#endif

    fprintf (rcf, "# Set some simple options.\n");
    fprintf (rcf, "set delbs      %s # if a DEL char is supposed to be backspace\n",
                    prG->flags & FLAG_DELBS     ? "on " : "off");
    fprintf (rcf, "set funny      %s # if you want funny messages\n",
                    prG->flags & FLAG_FUNNY     ? "on " : "off");
    fprintf (rcf, "set color      %s # if you want colored messages\n",
                    prG->flags & FLAG_COLOR     ? "on " : "off");
    fprintf (rcf, "set auto       %s # if automatic responses are to be sent\n",
                    prG->flags & FLAG_AUTOREPLY ? "on " : "off");
    fprintf (rcf, "set prompt  %s # type of prompt (user, uin, simple)\n",
                    prG->flags & FLAG_USERPROMPT ? "user " : 
                    prG->flags & FLAG_UINPROMPT ? "uin " : "simple");
    fprintf (rcf, "set autosave   %s # whether the climmrc should be automatically saved on exit\n",
                    prG->flags & FLAG_AUTOSAVE ? "on " : "off");
    fprintf (rcf, "set autofinger %s # whether new UINs should be fingered automatically\n",
                    prG->flags & FLAG_AUTOFINGER ? "on " : "off");
    fprintf (rcf, "set linebreak  %s # the line break type to be used (simple, break, indent, smart)\n",
                    prG->flags & FLAG_LIBR_INT 
                    ? prG->flags & FLAG_LIBR_BR ? "smart " : "indent"
                    : prG->flags & FLAG_LIBR_BR ? "break " : "simple");
    fprintf (rcf, "\n");
    
    fprintf (rcf, "%s\n\n", OptString (&prG->copts));

    fprintf (rcf, "chat %d          # random chat group; -1 to disable, 49 for climm\n",
                  prG->chat);

    fprintf (rcf, "autoupdate %d    # level of automatic preference updates\n\n",
                  prG->autoupdate);

    fprintf (rcf, "logplace %s      # the file or (distinct files in) dir to log to\n\n",
                  prG->logplace ? s_quote (prG->logplace) : "");

    fprintf (rcf, "sound %s # on=beep,off,event\n",
                  prG->sound & SFLAG_BEEP  ? "beep " :
                  prG->sound & SFLAG_EVENT ? "event" : "off  ");

    fprintf (rcf, "event %s\n\n", prG->event_cmd && *prG->event_cmd ? s_quote (prG->event_cmd) : "off");

    fprintf (rcf, "prompt_strftime \"%s\"          # format for time prompt option %%T. man strftime\n",
                   prG->prompt_strftime);

    fprintf (rcf, "\n# The strings section - runtime redefinable strings.\n");
    fprintf (rcf, "[Strings]\n");
    fprintf (rcf, "# The user prompt.\n");

    fprintf (rcf, "prompt %s          # user prompt. for prompt template see manual\n",
                   s_quote (prG->prompt));

    fprintf (rcf, "# The aliases.\n");
    {
        const alias_t *node;
        for (node = AliasList (); node; node = node->next)
        {
            const char *tmp = s_quote (node->command);
            fprintf (rcf, "alias %s%s%s%s", node->autoexpand ? "autoexpand " : "",
                *tmp == '"' ? "" : "\"", tmp, *tmp == '"' ? "" : "\"");
            fprintf (rcf, " %s\n", s_quote (node->replace));
        }
    }
    fprintf (rcf, "\n");

    if (fclose (rcf))
    {
        s_free (rcfn);
        return -1;
    }
    
    rcfo = strdup (s_sprintf ("%s.old", prG->rcfile));
    rename (prG->rcfile, rcfo);
    s_free (rcfo);
    
    rc = rename (rcfn, prG->rcfile);
    s_free (rcfn);
    return rc;
}

int Save_RC ()
{
    return PrefWriteConfFile () | PrefWriteStatusFile ();
}
