/* $Id$ */

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
#ifndef __BEOS__
#include <arpa/inet.h>
#endif
#endif

/****/

static char *fill (const char *fmt, const char *in);

#define      ADD_ALTER(a,b)      else if (!strcasecmp (tmp, a))    \
                                       CmdUser (sess, fill ("¶alter quiet " #b " %s", strtok (NULL, " \n\t")))
#define      ADD_CMD_D(a,b)      else if (!strcasecmp (tmp, a))        \
                                       { prG->b = strtok (NULL, "\n");\
                                         if (!prG->b) prG->b = ""; \
                                         while (*prG->b == ' ' || *prG->b == '\t') prG->b++; \
                                         prG->b = strdup (prG->b);     \
                                         dep = 1; } else if (0)
#define      ADD_CMD(a,b)        else if (!strcasecmp (tmp, a))        \
                                       { prG->b = strtok (NULL, "\n");\
                                         if (!prG->b) prG->b = ""; \
                                         while (*prG->b == ' ' || *prG->b == '\t') prG->b++; \
                                         prG->b = strdup (prG->b);     \
                                         } else if (0)

char *fill (const char *fmt, const char *in)
{
    char buf[1024];
    snprintf (buf, sizeof (buf), fmt, in);
    return strdup (buf);
}

/************************************************************************
 Copies src string into dest.  If src is NULL dest becomes ""
*************************************************************************/
static void M_strcpy (char *dest, char *src)
{
    if (src)
        strcpy (dest, src);
    else
        *dest = '\0';
}

static char *M_strdup (char *src)
{
    return strdup (src ? src : "");
}

void Initalize_RC_File (Session *sess)
{
    FD_T rcf;
    char pwd1[20], pwd2[20], input[200];
    sess->server = "icq1.mirabilis.com";
    sess->server_port = 4000;

    sess->away_time = default_away_time;

    M_print ("%s ", i18n (88, "Enter UIN or 0 for new UIN:"));
    fflush (stdout);
    scanf ("%ld", &sess->uin);
  password_entry:
    M_print ("%s ", i18n (63, "Enter password:"));
    fflush (stdout);
    memset (pwd1, 0, sizeof (pwd1));
    Echo_Off ();
    M_fdnreadln (STDIN, pwd1, sizeof (pwd1));
    Echo_On ();
    if (sess->uin == 0)
    {
        if (!pwd1[0])
        {
            M_print ("\n%s\n", i18n (91, "Must enter password!"));
            goto password_entry;
        }
        M_print ("\n%s ", i18n (92, "Reenter password to verify:"));
        fflush (stdout);
        memset (pwd2, 0, sizeof (pwd2));
        Echo_Off ();
        M_fdnreadln (STDIN, pwd2, sizeof (pwd2));
        Echo_On ();
        if (strcmp (pwd1, pwd2))
        {
            M_print ("\n%s\n", i18n (93, "Passwords did not match - please reenter."));
            goto password_entry;
        }
        sess->passwd = strdup (pwd1);
        Init_New_User (sess);
    }

/* SOCKS5 stuff begin */
    M_print ("\n%s ", i18n (94, "SOCKS5 server hostname or IP (type 0 if you don't want to use this):"));
    fflush (stdout);
    scanf ("%190s", input);
    if (strlen (input) > 1)
    {
        prG->s5Host = strdup (input);
        prG->s5Use = 1;
        M_print ("%s ", i18n (95, "SOCKS5 port (in general 1080):"));
        fflush (stdout);
        scanf ("%hu", &prG->s5Port);

        M_print ("%s ", i18n (96, "SOCKS5 username (type 0 if you don't need authentification):"));
        fflush (stdout);
        scanf ("%190s", input);
        if (strlen (input) > 1)
        {
            prG->s5Name = strdup (input);
            prG->s5Auth = 1;
            M_print ("%s ", i18n (97, "SOCKS5 password:"));
            fflush (stdout);
            scanf ("%190s", input);
            prG->s5Pass = strdup (input);
        }
        else
        {
            prG->s5Auth = 0;
        }
    }
    else
        prG->s5Use = 0;
/* SOCKS5 stuff end */

    sess->set_status = STATUS_ONLINE;

    ContactAdd (11290140, "mICQ author (dead)");
    ContactAdd (99798577, "Rico \"mc\" Glöckner");
    ContactAdd (-99798577, "mICQ maintainer");
    ContactAdd (82274703, "Rüdiger Kuhlmann");
    ContactAdd (-82274703, "mICQ developer");

    uiG.Current_Status = STATUS_ONLINE;

    rcf = open (prG->rcfile, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (rcf == -1)
    {
        perror ("Error creating config file ");
        exit (1);
    }
    close (rcf);

    if (Save_RC (sess) == -1)
    {
        perror ("Error creating config file ");
        exit (1);
    }
}

void Read_RC_File (Session *sess, FD_T rcf)
{
    char buf[450];
    char *tmp;
    char *p;
    Contact *cont;
    int i, section, dep = 0;
    UDWORD uin;
    char *tab_nick_spool[TAB_SLOTS];
    int spooled_tab_nicks;

    sess->passwd = NULL;
    sess->uin = 0;
    sess->away_time = default_away_time;

    spooled_tab_nicks = 0;
    for (section = 0; !M_fdnreadln (rcf, buf, sizeof (buf)); )
    {
        if (!buf[0] || (buf[0] == '#'))
            continue;
        if (buf[0] == '[')
        {
            if (!strcasecmp (buf, "[Contacts]"))
                section = 1;
            else if (!strcasecmp (buf, "[Strings]"))
                section = 2;
            else
            {
                M_print (COLERR "%s" COLNONE " ", i18n (733, "Warning:"));
                M_print (i18n (659, "Unknown section %s in configuration file."), buf);
                M_print ("\n");
                section = -1;
            }
            continue;
        }
        switch (section)
        {
            case -1:
                M_print (COLERR "%s" COLNONE " ", i18n (733, "Warning:"));
                M_print (i18n (675, "Ignored line:"));
                M_print (" %s\n", buf);
                break;
            case 0:
                tmp = strtok (buf, " ");
                if (!strcasecmp (tmp, "Server"))
                {
                    sess->server = M_strdup (strtok (NULL, " \n\t"));
                }
                else if (!strcasecmp (tmp, "Password"))
                {
                    sess->passwd = M_strdup (strtok (NULL, "\n\t"));
                }
                else if (!strcasecmp (tmp, "ReceiveScript"))
                {
#ifdef MSGEXEC
                    prG->event_cmd = M_strdup (strtok (NULL, "\n"));
#else
                    printf (i18n (817, "Warning: ReceiveScript feature not enabled!\n"));
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
                    if (prG->verbose == (unsigned)-2)
                        prG->verbose = atoi (strtok (NULL, "\n"));
                }
                else if (!strcasecmp (tmp, "Russian"))
                {
                    prG->flags |= FLAG_CONVRUSS;
                    dep = 1;
                }
                else if (!strcasecmp (tmp, "JapaneseEUC"))
                {
                    prG->flags |= FLAG_CONVEUC;
                    dep = 1;
                }
                else if (!strcasecmp (tmp, "Hermit"))
                {
                    prG->flags |= FLAG_HERMIT;
                    dep = 1;
                }
                else if (!strcasecmp (tmp, "logplace"))
                {
                    if (!prG->logplace) /* don't overwrite command line arg */
                        prG->logplace = strdup (strtok (NULL, " \t\n"));
                }
                else if (!strcasecmp (tmp, "LogType"))
                {
                    char *home = getenv ("HOME");
                    if (!home) home = "";
                    i = atoi (strtok (NULL, " \n\t"));
                    prG->flags &= ~FLAG_LOG & ~FLAG_LOG_ONOFF;
                    switch (i)
                    {
                        case 1:
                            prG->logplace = malloc (strlen (home) + 10);
                            strcpy (prG->logplace, home);
                            strcpy (prG->logplace, "/micq_log");
                            break;
                        case 3:
                            prG->flags |= FLAG_LOG_ONOFF;
                        case 2:
                            prG->flags |= FLAG_LOG;
                            prG->logplace = malloc (strlen (home) + 10);
                            strcpy (prG->logplace, home);
                            strcpy (prG->logplace, "/micq.log/");
                    }
                    dep = 1;
                }
                else if (!strcasecmp (tmp, "No_Log"))
                {
                    prG->flags &= ~FLAG_LOG & ~FLAG_LOG_ONOFF;
                    dep = 1;
                }
                else if (!strcasecmp (tmp, "No_Color"))
                {
                    prG->flags &= ~FLAG_COLOR;
                    dep = 1;
                }
                else if (!strcasecmp (tmp, "Last_UIN_Prompt"))
                {
                    prG->flags |= FLAG_UINPROMPT;
                    dep = 1;
                }
                else if (!strcasecmp (tmp, "Del_is_Del"))
                {
                    prG->flags &= ~FLAG_DELBS;
                    dep = 1;
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
                else if (!strcasecmp (tmp, "UIN"))
                {
                    sess->uin = atoi (strtok (NULL, " \n\t"));
                }
                else if (!strcasecmp (tmp, "port"))
                {
                    sess->server_port = atoi (strtok (NULL, " \n\t"));
                }
                else if (!strcasecmp (tmp, "Status"))
                {
                    sess->set_status = atoi (strtok (NULL, " \n\t"));
                }
                ADD_CMD_D ("auto_rep_str_away", auto_away);
                ADD_CMD_D ("auto_rep_str_na",   auto_na);
                ADD_CMD_D ("auto_rep_str_dnd",  auto_dnd);
                ADD_CMD_D ("auto_rep_str_occ",  auto_occ);
                ADD_CMD_D ("auto_rep_str_inv",  auto_inv);
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
                else if (!strcasecmp (tmp, "LogDir"))
                {
                    char *tmp = strtok (NULL, "\n");
                    if (!tmp) tmp = "";
                    if (*tmp && tmp[strlen (tmp) - 1] == '/')
                    {
                        prG->logplace = strdup (tmp);
                    }
                    else
                    {
                        prG->logplace = malloc (strlen (tmp) + 2);
                        strcpy (prG->logplace, tmp);
                        strcat (prG->logplace, "/");
                    }
                    prG->flags |= FLAG_LOG;
                    dep = 1;
                }
                else if (!strcasecmp (tmp, "Sound"))
                {
                    tmp = strtok (NULL, "\n\t");
                    if (!tmp)
                    {
                        prG->sound |= SFLAG_BEEP;
                        dep = 1;
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
                else if (!strcasecmp (tmp, "No_Sound"))
                {
                    prG->sound &= ~SFLAG_BEEP & ~SFLAG_CMD;
                    dep = 1;
                }
                else if (!strcasecmp (tmp, "SoundOnline"))
                {
                    tmp = strtok (NULL, "\n\t");
                    if (!tmp)
                    {
                        prG->sound |= SFLAG_ON_BEEP;
                        dep = 1;
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
                else if (!strcasecmp (tmp, "No_SoundOnline"))
                {
                    prG->sound &= ~SFLAG_ON_BEEP & ~SFLAG_ON_CMD;
                    dep = 1;
                }
                else if (!strcasecmp (tmp, "SoundOffline"))
                {
                    tmp = strtok (NULL, "\n\t");
                    if (!tmp)
                    {
                        prG->sound |= SFLAG_OFF_BEEP;
                        dep = 1;
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
                else if (!strcasecmp (tmp, "No_SoundOffline"))
                {
                    prG->sound &= ~SFLAG_OFF_BEEP & ~SFLAG_OFF_CMD;
                    dep = 1;
                }
                else if (!strcasecmp (tmp, "Auto_away"))
                {
                    sess->away_time = atoi (strtok (NULL, " \n\t"));
                }
                else if (!strcasecmp (tmp, "Screen_width"))
                {
                    prG->screen = atoi (strtok (NULL, " \n\t"));
                }
                ADD_ALTER ("clear_cmd", clear);
                ADD_ALTER ("message_cmd", msg);
                ADD_ALTER ("info_cmd", info);
                ADD_ALTER ("rand_cmd", rand);
                ADD_ALTER ("color_cmd", color);
                ADD_ALTER ("sound_cmd", sound);
                ADD_ALTER ("quit_cmd", q);
                ADD_ALTER ("reply_cmd", r);
                ADD_ALTER ("again_cmd", a);
                ADD_ALTER ("list_cmd", w);
                ADD_ALTER ("away_cmd", away);
                ADD_ALTER ("na_cmd", na);
                ADD_ALTER ("dnd_cmd", dnd);
                ADD_ALTER ("togig_cmd", togig);
                ADD_ALTER ("iglist_cmd", i);
                ADD_ALTER ("online_cmd", online);
                ADD_ALTER ("occ_cmd", occ);
                ADD_ALTER ("ffc_cmd", ffc);
                ADD_ALTER ("inv_cmd", inv);
                ADD_ALTER ("status_cmd", status);
                ADD_ALTER ("auth_cmd", auth);
                ADD_ALTER ("auto_cmd", auto);
                ADD_ALTER ("change_cmd", change);
                ADD_ALTER ("add_cmd", add);
                ADD_ALTER ("togvis_cmd", togvis);
                ADD_ALTER ("search_cmd", search);
                ADD_ALTER ("save_cmd", save);
                ADD_ALTER ("alter_cmd", alter);
                ADD_ALTER ("online_list_cmd", e);
                ADD_ALTER ("msga_cmd", msga);
                ADD_ALTER ("update_cmd", update);
                ADD_ALTER ("url_cmd", url);
                ADD_ALTER ("about_cmd", about);
                else if (!strcasecmp (tmp, "Tab"))
                {
                    if (spooled_tab_nicks < TAB_SLOTS)
                        tab_nick_spool[spooled_tab_nicks++] = M_strdup (strtok (NULL, "\n\t"));
                }
                else if (!strcasecmp (tmp, "Contacts"))
                {
                    section = 1;
                    dep = 1;
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
                    else if (!strcasecmp (tmp, "autologin"))
                        which = FLAG_AUTOLOGIN;
                    else if (!strcasecmp (tmp, "linebreak"))
                        which = -2;
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
                    else
                    {
                        tmp = strtok (NULL, " \t\n");
                        printf ("type: %s\n", tmp);
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
                }
                else if (!strcasecmp (tmp, "logging"))
                {
                    dep = 1;
                    tmp = strtok (NULL, " \t\n");
                    if (tmp)
                    {
                        i = atoi (tmp);
                        prG->flags &= ~FLAG_LOG & ~FLAG_LOG_ONOFF;
                        if (i)
                            prG->flags |= FLAG_LOG;
                        if (i & 2)
                            prG->flags |= FLAG_LOG_ONOFF;
                        tmp = strtok (NULL, "\n");
                        if (tmp)
                        {
                            prG->logplace = strdup (tmp);
                            if (!strlen (prG->logplace))
                                prG->logplace = NULL;
                        }
                    }
                }
                else
                {
                    M_print (COLERR "%s" COLNONE " ", i18n (733, "Warning:"));
                    M_print (i18n (188, "Unrecognized command in rc file '%s', ignored."), tmp);
                    M_print ("\n");
                }
                break;
            case 1:
                p = buf;

                while (*p == ' ')
                    p++;

                if (!*p || *p == '#' )
                    continue;

                if (isdigit ((int) *p))
                    i = 0;
                else if (*p == '*')
                {
                    i = 1;
                    for (p++; *p == ' '; p++) ;
                }
                else if (*p == '~')
                {
                    i = 2;
                    for (p++; *p == ' '; p++) ;
                }
                else
                    i = 3;
                
                if (i == 3)
                {
                    uin = -1;
                    tmp = p;
                }
                else
                {
                    uin = atoi (strtok (p, " "));
                    tmp = strtok (NULL, "");
                }
                
                if (!(cont = ContactAdd (uin, tmp)))
                {
                    M_print (COLERR "%s" COLNONE " %s\n", i18n (733, "Warning:"),
                             i18n (732, "maximal number of contacts reached. Ask a wizard to enlarge me!"));
                    section = -1;
                    break;
                }
                
                if (i == 1)
                {
                    cont->vis_list = TRUE;
                }
                else if (i == 2)
                {
                    cont->invis_list = TRUE;
                }
                else if (i == 3)
                {
                    strncpy (cont->nick, (cont - 1)->nick, 20);
                }

                if (prG->verbose > 2)
                    M_print ("%ld = %s\n", cont->uin, cont->nick);
                break;
            case 2:
                tmp = strtok (buf, " ");
                if (!strcasecmp (tmp, "alter"))
                {
                    CmdUser (sess, fill ("¶alter quiet %s", strtok (NULL, "\n")));
                }
                else
                {
                    M_print (COLERR "%s" COLNONE " ", i18n (733, "Warning:"));
                    M_print (i18n (188, "Unrecognized command in rc file '%s', ignored."), tmp);
                    M_print ("\n");
                }
                break;
        }
    }

    /* now tab the nicks we may have spooled earlier */
    for (i = 0; i < spooled_tab_nicks; i++)
    {
        TabAddUIN (ContactFindByNick (tab_nick_spool[i]));
        free (tab_nick_spool[i]);
    }

    if (!prG->auto_dnd)
        prG->auto_dnd  = strdup (i18n (9, "User is DND [Auto-Message]"));
    if (!prG->auto_away)
        prG->auto_away = strdup (i18n (10, "User is Away [Auto-Message]"));
    if (!prG->auto_na)
        prG->auto_na   = strdup (i18n (11, "User is not available [Auto-Message]"));
    if (!prG->auto_occ)
        prG->auto_occ  = strdup (i18n (12, "User is Occupied [Auto-Message]"));
    if (!prG->auto_inv)
        prG->auto_inv  = strdup (i18n (13, "User is offline"));

    if (prG->flags & FLAG_LOG && !prG->logplace)
    {
        prG->logplace = malloc (strlen (GetUserBaseDir ()) + 10);
        strcpy (prG->logplace, GetUserBaseDir ());
        strcat (prG->logplace, "history/");
    }

    if (prG->verbose)
    {
        M_print (i18n (189, "UIN = %ld\n"),    sess->uin);
        M_print (i18n (190, "port = %ld\n"),   sess->server_port);
        M_print (i18n (191, "passwd = %s\n"),  sess->passwd);
        M_print (i18n (192, "server = %s\n"),  sess->server);
        M_print (i18n (193, "status = %ld\n"), sess->set_status);
        M_print (i18n (196, "Message_cmd = %s\n"), CmdUserLookupName ("msg"));
        M_print ("flags: %08x\n", prG->flags);
    }
    if (sess->uin == 0)
    {
        fprintf (stderr, "Bad .micqrc file.  No UIN found aborting.\a\n");
        exit (1);
    }
    if (dep)
        M_print (i18n (818, "Warning: Deprecated syntax found in rc file!\n    Please update or \"save\" the rc file and check for changes.\n"));
}

/************************************************
 *   This function should save your auto reply messages in the rc file.
 *   NOTE: the code isn't really neat yet, I hope to change that soon.
 *   Added on 6-20-98 by Fryslan
 ***********************************************/
int Save_RC (Session *sess)
{
    FD_T rcf;
    time_t t;
    int k;
    Contact *cont;

    rcf = open (prG->rcfile, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (rcf == -1)
        return -1;
    M_fdprint (rcf, "# This file was generated by Micq of %s %s\n", __TIME__, __DATE__);
    t = time (NULL);
    M_fdprint (rcf, "# This file was generated on %s", ctime (&t));
    M_fdprint (rcf, "# Micq version " MICQ_VERSION "\n");
    M_fdprint (rcf, "\n\nUIN %d\n", sess->uin);
    M_fdprint (rcf, "# Remove the entire below line if you want to be prompted for your password.\n");
    M_fdprint (rcf, "Password %s\n", sess->passwd);
    M_fdprint (rcf, "\n#Partial list of status you can use here.  (more from the change command )\n");
    M_fdprint (rcf, "#    0 Online\n");
    M_fdprint (rcf, "#   32 Free for Chat\n");
    M_fdprint (rcf, "#  256 Invisible\n");
    M_fdprint (rcf, "Status %d\n", uiG.Current_Status);
    M_fdprint (rcf, "\nServer %s\n", "icq1.mirabilis.com");
    M_fdprint (rcf, "Port %d\n", 4000);
    M_fdprint (rcf, "\nverbose %d\n", prG->verbose);

    M_fdprint (rcf, "\n# Support for SOCKS5 server\n");
    M_fdprint (rcf, "s5_use %d\n", prG->s5Use);
    if (!prG->s5Host)
        M_fdprint (rcf, "s5_host [none]\n");
    else
        M_fdprint (rcf, "s5_host %s\n", prG->s5Host);
    M_fdprint (rcf, "s5_port %d\n", prG->s5Port);
    M_fdprint (rcf, "# If you need authentification, put 1 for s5_auth and fill your name/password\n");
    M_fdprint (rcf, "s5_auth %d\n", prG->s5Auth);
    if (!prG->s5Name)
        M_fdprint (rcf, "s5_name [none]\n");
    else
        M_fdprint (rcf, "s5_name %s\n", prG->s5Name);
    if (!prG->s5Pass)
        M_fdprint (rcf, "s5_pass [none]\n");
    else
        M_fdprint (rcf, "s5_pass %s\n", prG->s5Pass);

    M_fdprint (rcf, "\n#in seconds\nAuto_Away %d\n", sess->away_time);
    M_fdprint (rcf, "\n#For dumb terminals that don't wrap set this.");
    M_fdprint (rcf, "\nScreen_Width %d\n", prG->screen);


    M_fdprint (rcf, "# Set some simple options.\n");
    M_fdprint (rcf, "set delbs     %s # if a DEL char is supposed to be backspace\n",
                    prG->flags & FLAG_DELBS     ? "on " : "off");
    M_fdprint (rcf, "set russian   %s # if you want russian koi8-r/u <-> cp1251 character conversion\n",
                    prG->flags & FLAG_CONVRUSS  ? "on " : "off");
    M_fdprint (rcf, "set japanese  %s # if you want japanese Shift-JIS <-> EUC character conversion\n",
                    prG->flags & FLAG_CONVEUC   ? "on " : "off");
    M_fdprint (rcf, "set funny     %s # if you want funny messages\n",
                    prG->flags & FLAG_FUNNY     ? "on " : "off");
    M_fdprint (rcf, "set color     %s # if you want colored messages\n",
                    prG->flags & FLAG_COLOR     ? "on " : "off");
    M_fdprint (rcf, "set hermit    %s # if you want messages from people on your contact list ONLY\n",
                    prG->flags & FLAG_HERMIT    ? "on " : "off");
    M_fdprint (rcf, "set log       %s # if you want to log messages\n",
                    prG->flags & FLAG_LOG       ? "on " : "off");
    M_fdprint (rcf, "set logonoff  %s # if you also want to log online/offline events\n",
                    prG->flags & FLAG_LOG_ONOFF ? "on " : "off");
    M_fdprint (rcf, "set auto      %s # if automatic responses are to be sent\n",
                    prG->flags & FLAG_AUTOREPLY ? "on " : "off");
    M_fdprint (rcf, "set uinprompt %s # if the prompt should contain the last uin a message was received from\n",
                    prG->flags & FLAG_UINPROMPT ? "on " : "off");
    M_fdprint (rcf, "set autologin %s # if you want to login automatically\n",
                    prG->flags & FLAG_DELBS     ? "on " : "off");
    M_fdprint (rcf, "set linebreak %s # the line break type to be used (simple, break, indent, smart)\n\n",
                    prG->flags & FLAG_LIBR_INT 
                    ? prG->flags & FLAG_LIBR_BR ? "smart " : "indent"
                    : prG->flags & FLAG_LIBR_BR ? "break " : "simple");


    M_fdprint (rcf, "logplace %s      # the file or (dstinct files in) dir to log to\n",
                    prG->logplace ? prG->logplace : "");


    M_fdprint (rcf, "# Define to a program which is executed to play sound when a message is received.\n");
    M_fdprint (rcf, "sound %s\n\n", prG->sound & SFLAG_BEEP ? "on" :
                                    prG->sound & SFLAG_CMD && prG->sound_cmd ? prG->sound_cmd : "off");

    M_fdprint (rcf, "# Execute this cmd when a user comes online in your contacts.\n");
    M_fdprint (rcf, "soundonline %s\n\n", prG->sound & SFLAG_ON_BEEP ? "on" :
                                          prG->sound & SFLAG_ON_CMD && prG->sound_on_cmd ? 
                                          prG->sound_on_cmd : "off");

    M_fdprint (rcf, "# Execute this cmd when a user goes offline in your contacts.\n");
    M_fdprint (rcf, "soundoffline %s\n\n", prG->sound & SFLAG_OFF_BEEP ? "on" :
                                           prG->sound & SFLAG_OFF_CMD && prG->sound_off_cmd ?
                                           prG->sound_off_cmd : "off");
    M_fdprint (rcf, "receivescript %s\n\n", prG->event_cmd ? prG->event_cmd : "");

    M_fdprint (rcf, "\n# automatic responses\n");
    M_fdprint (rcf, "auto away %s\n", prG->auto_away);
    M_fdprint (rcf, "auto na   %s\n", prG->auto_na);
    M_fdprint (rcf, "auto dnd  %s\n", prG->auto_dnd);
    M_fdprint (rcf, "auto occ  %s\n", prG->auto_occ);
    M_fdprint (rcf, "auto inv  %s\n", prG->auto_inv);

    M_fdprint (rcf, "\n# The strings section - runtime redefinable strings.\n");
    M_fdprint (rcf, "# The alter command redefines command names.\n");
    M_fdprint (rcf, "[Strings]\n");
    {
        jump_t *f;
        for (f = CmdUserTable (); f->f; f++)
            if (f->name && strcmp (f->name, f->defname))
                M_fdprint (rcf, "alter %s %s\n", f->defname, f->name);
    }

    M_fdprint (rcf, "\n# The contact list section.\n");
    M_fdprint (rcf, "#  Use * in front of the number of anyone you want to see you while you're invisble.\n");
    M_fdprint (rcf, "#  Use ~ in front of the number of anyone you want to always see you as offline.\n");
    M_fdprint (rcf, "#  People in the second group won't show up in your list.\n");
    M_fdprint (rcf, "[Contacts]\n");
    /* adding contacts to the rc file. */
    /* we start counting at zero in the index. */

    for (cont = ContactStart (); ContactHasNext (cont); cont = ContactNext (cont))
    {
        if (!(cont->uin & 0x80000000L))
        {
            Contact *cont2;
            M_fdprint (rcf, cont->vis_list ? "*" : cont->invis_list ? "~" : " ");
            M_fdprint (rcf, "%9d %s\n", cont->uin, cont->nick);
            k = 0;
            for (cont2 = ContactStart (); ContactHasNext (cont2); cont2 = ContactNext (cont2))
            {
                if (cont2->uin == - cont->uin)
                {
                    if (k)
                        M_fdprint (rcf, " ");
                    M_fdprint (rcf, "%s", cont2->nick);
                    k++;
                }
            }
            if (k)
                M_fdprint (rcf, "\n");
        }
    }
    M_fdprint (rcf, "\n");
    return close (rcf);
}

int Add_User (Session *sess, UDWORD uin, char *name)
{
    FD_T rcf;

    if (!uin)
        return 0;

    if (ContactFind (uin))
            return 0;

    rcf = open (prG->rcfile, O_RDWR | O_APPEND);
    if (rcf == -1)
        return 0;
    M_fdprint (rcf, "%d %s\n", uin, name);
    close (rcf);

    ContactAdd (uin, name);

    CmdPktCmdContactList (sess);
    return 1;
}
