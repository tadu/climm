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

#define      ADD_ALTER(a,b)      else if (!strcasecmp (tmp, a))   \
                                       CmdUser (sess, fill ("¶alter quiet " #b " %s", strtok (NULL, " \n\t")))
#define      ADD_CMD(a,b)        else if (!strcasecmp (tmp, a))     \
                                 { char *stb;                        \
                                   stb = strtok (NULL, "\n\t");       \
                                   if (!stb) stb = "";                 \
                                   snprintf (b, sizeof (b), "%s", stb); } else if (0) 

static char rcfile[256];
static int rcfiledef = 0;

char *fill (const char *fmt, const char *in)
{
    char buf[1024];
    snprintf (buf, sizeof (buf), fmt, in);
    return strdup (buf);
}

void Set_rcfile (const char *name)
{
    char *path = 0;
    char *home;

    if (!name || !strlen (name))
    {
        if (rcfiledef == 2)
            return;

#ifdef _WIN32
        path = ".\\";
#endif

#ifdef __amigaos__
        path = "PROGDIR:";
#endif

        home = getenv ("HOME");
        if (home || !path)
        {
            if (!home)
                home = "";
            path = malloc (strlen (home) + 2 + 6);
            strcpy (path, home);
            if (path[strlen (path) - 1] != '/')
                strcat (path, "/");
            if (!name)
                strcat (path, ".micq/");
        }
        strcpy (rcfile, path);
        if (name)
            strcat (rcfile, ".micqrc");
        else
            strcat (rcfile, "micqrc");
        rcfiledef = 1;
    }
    else
    {
        strncpy (rcfile, name, 256);
        rcfile[255] = 0;
        rcfiledef = 2;
    }
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

static void Initalize_RC_File (Session *sess)
{
    FD_T rcf;
    char passwd2[sizeof (sess->passwd)];
    strcpy (sess->server, "icq1.mirabilis.com");
    sess->remote_port = 4000;

    sess->away_time = default_away_time;

    M_print ("%s ", i18n (88, "Enter UIN or 0 for new UIN:"));
    fflush (stdout);
    scanf ("%ld", &sess->uin);
  password_entry:
    M_print ("%s ", i18n (63, "Enter password:"));
    fflush (stdout);
    memset (sess->passwd, 0, sizeof (sess->passwd));
    Echo_Off ();
    M_fdnreadln (STDIN, sess->passwd, sizeof (sess->passwd));
    Echo_On ();
    if (sess->uin == 0)
    {
        if (0 == sess->passwd[0])
        {
            M_print ("\n%s\n", i18n (91, "Must enter password!"));
            goto password_entry;
        }
        M_print ("\n%s ", i18n (92, "Reenter password to verify:"));
        fflush (stdout);
        memset (passwd2, 0, sizeof (passwd2));
        Echo_Off ();
        M_fdnreadln (STDIN, passwd2, sizeof (sess->passwd));
        Echo_On ();
        if (strcmp (sess->passwd, passwd2))
        {
            M_print ("\n%s\n", i18n (93, "Passwords did not match - please reenter."));
            goto password_entry;
        }
        Init_New_User (sess);
    }

/* SOCKS5 stuff begin */
    M_print ("\n%s ", i18n (94, "SOCKS5 server hostname or IP (type 0 if you don't want to use this):"));
    fflush (stdout);
    scanf ("%s", s5G.s5Host);
    if (strlen (s5G.s5Host) > 1)
    {
        s5G.s5Use = 1;
        M_print ("%s ", i18n (95, "SOCKS5 port (in general 1080):"));
        fflush (stdout);
        scanf ("%hu", &s5G.s5Port);

        M_print ("%s ", i18n (96, "SOCKS5 username (type 0 if you don't need authentification):"));
        fflush (stdout);
        scanf ("%s", s5G.s5Name);
        if (strlen (s5G.s5Name) > 1)
        {
            s5G.s5Auth = 1;
            M_print ("%s ", i18n (97, "SOCKS5 password:"));
            fflush (stdout);
            scanf ("%s", s5G.s5Pass);
        }
        else
        {
            s5G.s5Auth = 0;
            strcpy (s5G.s5Name, "0");
            strcpy (s5G.s5Pass, "0");
        }
    }
    else
        s5G.s5Use = 0;
/* SOCKS5 stuff end */

    sess->set_status = STATUS_ONLINE;

    ContactAdd (11290140, "mICQ author (dead)");
    ContactAdd (99798577, "Rico \"mc\" Glöckner");
    ContactAdd (-99798577, "mICQ maintainer");
    ContactAdd (82274703, "Rüdiger Kuhlmann");
    ContactAdd (-82274703, "mICQ developer");

    uiG.Current_Status = STATUS_ONLINE;

    rcf = open (rcfile, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
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

static void Read_RC_File (Session *sess, FD_T rcf)
{
    char buf[450];
    char *tmp;
    char *p;
    Contact *cont;
    int i, section, dep = 0;
    UDWORD uin;
    char *tab_nick_spool[TAB_SLOTS];
    int spooled_tab_nicks;

    sess->passwd[0] = 0;
    sess->uin = 0;
    sess->away_time = default_away_time;

#ifdef MSGEXEC
    uiG.receive_script[0] = '\0';
#endif

/* SOCKS5 stuff begin */
    s5G.s5Use = 0;
    s5G.s5Host[0] = '\0';
    s5G.s5Port = 0;
    s5G.s5Auth = 0;
    s5G.s5Name[0] = '\0';
    s5G.s5Pass[0] = '\0';
/* SOCKS5 stuff end */

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
                    strcpy (sess->server, strtok (NULL, " \n\t"));
                }
                else if (!strcasecmp (tmp, "Password"))
                {
                    strcpy (sess->passwd, strtok (NULL, "\n\t"));
                }
                else if (!strcasecmp (tmp, "ReceiveScript"))
                {
#ifdef MSGEXEC
                    strcpy (uiG.receive_script, strtok (NULL, "\n\t"));
#else
                    printf (i18n (817, "Warning: ReceiveScript Feature not enabled!\n"));
#endif
                }
                else if (!strcasecmp (tmp, "s5_use"))
                {
                    s5G.s5Use = atoi (strtok (NULL, " \n\t"));
                }
                else if (!strcasecmp (tmp, "s5_host"))
                {
                    M_strcpy (s5G.s5Host, strtok (NULL, "\n\t"));
                }
                else if (!strcasecmp (tmp, "s5_port"))
                {
                    s5G.s5Port = atoi (strtok (NULL, " \n\t"));
                }
                else if (!strcasecmp (tmp, "s5_auth"))
                {
                    s5G.s5Auth = atoi (strtok (NULL, " \n\t"));
                }
                else if (!strcasecmp (tmp, "s5_name"))
                {
                    M_strcpy (s5G.s5Name, strtok (NULL, "\n\t"));
                }
                else if (!strcasecmp (tmp, "s5_pass"))
                {
                    M_strcpy (s5G.s5Pass, strtok (NULL, "\n\t"));
                }
                else if (!strcasecmp (tmp, "Russian"))
                {
                    uiG.Russian = TRUE;
                }
                else if (!strcasecmp (tmp, "JapaneseEUC"))
                {
                    uiG.JapaneseEUC = TRUE;
                }
                else if (!strcasecmp (tmp, "Hermit"))
                {
                    uiG.Hermit = TRUE;
                }
                else if (!strcasecmp (tmp, "LogType"))
                {
                    char *home = getenv ("HOME");
                    if (!home) home = "";
                    uiG.LogLevel = atoi (strtok (NULL, " \n\t"));
                    switch (uiG.LogLevel)
                    {
                        case 1:
                            uiG.LogPlace = malloc (strlen (home) + 10);
                            strcpy (uiG.LogPlace, home);
                            strcpy (uiG.LogPlace, "/micq_log");
                            break;
                        case 3:
                        case 2:
                            uiG.LogLevel--;
                            uiG.LogPlace = malloc (strlen (home) + 10);
                            strcpy (uiG.LogPlace, home);
                            strcpy (uiG.LogPlace, "/micq.log/");
                    }
                    dep = 1;
                }
                else if (!strcasecmp (tmp, "No_Log"))
                {
                    uiG.LogLevel = 0;
                    dep = 1;
                }
                else if (!strcasecmp (tmp, "No_Color"))
                {
                    uiG.Color = FALSE;
                    dep = 1;
                }
                else if (!strcasecmp (tmp, "Last_UIN_Prompt"))
                {
                    uiG.last_uin_prompt = TRUE;
                }
                else if (!strcasecmp (tmp, "Del_is_Del"))
                {
                    uiG.del_is_bs = FALSE;
                }
                else if (!strcasecmp (tmp, "LineBreakType"))
                {
                    uiG.line_break_type = atoi (strtok (NULL, " \n\t"));
                }
                else if (!strcasecmp (tmp, "UIN"))
                {
                    sess->uin = atoi (strtok (NULL, " \n\t"));
                }
                else if (!strcasecmp (tmp, "port"))
                {
                    sess->remote_port = atoi (strtok (NULL, " \n\t"));
                }
                else if (!strcasecmp (tmp, "Status"))
                {
                    sess->set_status = atoi (strtok (NULL, " \n\t"));
                }
                else if (!strcasecmp (tmp, "Auto"))
                {
                    uiG.auto_resp = TRUE;
                }
                ADD_CMD ("auto_rep_str_away", uiG.auto_rep_str_away);
                ADD_CMD ("auto_rep_str_na", uiG.auto_rep_str_na);
                ADD_CMD ("auto_rep_str_dnd", uiG.auto_rep_str_dnd);
                ADD_CMD ("auto_rep_str_occ", uiG.auto_rep_str_occ);
                ADD_CMD ("auto_rep_str_inv", uiG.auto_rep_str_inv);
                else if (!strcasecmp (tmp, "LogDir"))
                {
                    char *tmp = strtok (NULL, "\n");
                    if (!tmp) tmp = "";
                    if (*tmp && tmp[strlen (tmp) - 1] == '/')
                    {
                        uiG.LogPlace = strdup (tmp);
                    }
                    else
                    {
                        uiG.LogPlace = malloc (strlen (tmp) + 2);
                        strcpy (uiG.LogPlace, tmp);
                        strcat (uiG.LogPlace, "/");
                    }
                    uiG.LogLevel |= 1;
                    dep = 1;
                }
                else if (!strcasecmp (tmp, "Sound"))
                {
                    M_strcpy (uiG.Sound_Str, strtok (NULL, "\n\t"));
                    if (uiG.Sound_Str[0])
                    {
                        uiG.Sound = SOUND_CMD;
                    }
                    else
                    {
                        uiG.Sound = SOUND_ON;
                    }
                }
                else if (!strcasecmp (tmp, "No_Sound"))
                {
                    uiG.Sound = SOUND_OFF;
                    uiG.Sound_Str[0] = '\0';
                }
                else if (!strcasecmp (tmp, "SoundOnline"))
                {
                    M_strcpy (uiG.Sound_Str_Online, strtok (NULL, "\n\t"));
                    if (uiG.Sound_Str_Online[0])
                    {
                        uiG.SoundOnline = SOUND_CMD;
                    }
                    else 
                    {
                        uiG.SoundOnline = SOUND_ON;
                    }
                }
                else if (!strcasecmp (tmp, "No_SoundOnline"))
                {
                    uiG.SoundOnline = SOUND_OFF;
                    uiG.Sound_Str_Online[0] = '\0';
                }
                else if (!strcasecmp (tmp, "SoundOffline"))
                {       
                    M_strcpy (uiG.Sound_Str_Offline, strtok (NULL, "\n\t"));
                    if (uiG.Sound_Str_Offline[0])
                    {
                        uiG.SoundOffline = SOUND_CMD;
                    }
                    else
                    {
                        uiG.SoundOffline = SOUND_ON;
                    }
                }
                else if (!strcasecmp (tmp, "No_SoundOffline"))
                {
                    uiG.SoundOffline = SOUND_OFF;
                    uiG.Sound_Str_Offline[0] = '\0';
                }
                else if (!strcasecmp (tmp, "Auto_away"))
                {
                    sess->away_time = atoi (strtok (NULL, " \n\t"));
                }
                else if (!strcasecmp (tmp, "Screen_width"))
                {
                    uiG.Max_Screen_Width = atoi (strtok (NULL, " \n\t"));
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
                        tab_nick_spool[spooled_tab_nicks++] = strdup (strtok (NULL, "\n\t"));
                }
                else if (!strcasecmp (tmp, "Contacts"))
                {
                    section = 1;
                }
                else if (!strcasecmp (tmp, "set"))
                {
                    CmdUser (sess, fill ("@set quiet %s", strtok (NULL, "\n")));
                }
                else if (!strcasecmp (tmp, "logging"))
                {
                    tmp = strtok (NULL, " \t\n");
                    if (tmp)
                    {
                        uiG.LogLevel = atoi (tmp);
                        tmp = strtok (NULL, "\n");
                        if (tmp)
                        {
                            uiG.LogPlace = strdup (tmp);
                            if (!strlen (uiG.LogPlace))
                                uiG.LogPlace = NULL;
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

                if (uiG.Verbose > 2)
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

    if (!*uiG.auto_rep_str_dnd)
        strcpy (uiG.auto_rep_str_dnd, i18n (9, "User is DND [Auto-Message]"));
    if (!*uiG.auto_rep_str_away)
        strcpy (uiG.auto_rep_str_away, i18n (10, "User is Away [Auto-Message]"));
    if (!*uiG.auto_rep_str_na)
        strcpy (uiG.auto_rep_str_na, i18n (11, "User is not available [Auto-Message]"));
    if (!*uiG.auto_rep_str_occ)
        strcpy (uiG.auto_rep_str_occ, i18n (12, "User is Occupied [Auto-Message]"));
    if (!*uiG.auto_rep_str_inv)
        strcpy (uiG.auto_rep_str_inv, i18n (13, "User is offline"));

    if (uiG.LogLevel && !uiG.LogPlace)
    {
        uiG.LogPlace = malloc (strlen (GetUserBaseDir ()) + 10);
        strcpy (uiG.LogPlace, GetUserBaseDir ());
        strcat (uiG.LogPlace, "history/");
    }

    if (uiG.Verbose)
    {
        M_print (i18n (189, "UIN = %ld\n"), sess->uin);
        M_print (i18n (190, "port = %ld\n"), sess->remote_port);
        M_print (i18n (191, "passwd = %s\n"), sess->passwd);
        M_print (i18n (192, "server = %s\n"), sess->server);
        M_print (i18n (193, "status = %ld\n"), sess->set_status);
        M_print (i18n (196, "Message_cmd = %s\n"), CmdUserLookupName ("msg"));
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

    rcf = open (rcfile, O_WRONLY | O_CREAT | O_TRUNC, 0600);
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

/* SOCKS5 stuff begin */
    M_fdprint (rcf, "\n# Support for SOCKS5 server\n");
    M_fdprint (rcf, "s5_use %d\n", s5G.s5Use);
    if (NULL == s5G.s5Host)
        M_fdprint (rcf, "s5_host [none]\n");
    else
        M_fdprint (rcf, "s5_host %s\n", s5G.s5Host);
    M_fdprint (rcf, "s5_port %d\n", s5G.s5Port);
    M_fdprint (rcf, "# If you need authentification, put 1 for s5_auth and fill your name/password\n");
    M_fdprint (rcf, "s5_auth %d\n", s5G.s5Auth);
    if (NULL == s5G.s5Name)
        M_fdprint (rcf, "s5_name [none]\n");
    else
        M_fdprint (rcf, "s5_name %s\n", s5G.s5Name);
    if (NULL == s5G.s5Pass)
        M_fdprint (rcf, "s5_pass [none]\n");
    else
        M_fdprint (rcf, "s5_pass %s\n", s5G.s5Pass);
/* SOCKS5 stuff end */

    M_fdprint (rcf, "\n#If you want messages from people on your list ONLY uncomment below.\n");
    if (!uiG.Hermit)
        M_fdprint (rcf, "#Hermit\n\n");
    else
        M_fdprint (rcf, "Hermit\n\n");

    M_fdprint (rcf, "# Define to a program which is executed to play sound when a message is received.\n");
    M_fdprint (rcf, "%sSound %s\n%sNo_Sound\n\n", uiG.Sound == SOUND_OFF ? "#" : "", 
                    uiG.Sound_Str, uiG.Sound == SOUND_OFF ? "" : "#");

    M_fdprint (rcf, "# Execute this cmd when a user comes online in your contacts.\n");
    M_fdprint (rcf, "%sSoundOnline %s\n%sNo_SoundOnline\n\n", uiG.SoundOnline == SOUND_OFF ? "#" : "",
                    uiG.Sound_Str_Online, uiG.SoundOnline == SOUND_OFF ? "" : "#");

    M_fdprint (rcf, "# Execute this cmd when a user goes offline in your contacts.\n");
    M_fdprint (rcf, "%sSoundOffline %s\n%sNo_SoundOffline\n\n", uiG.SoundOffline == SOUND_OFF ? "#" : "",
                    uiG.Sound_Str_Offline, uiG.SoundOffline == SOUND_OFF ? "" : "#");

    M_fdprint (rcf, "# Set some simple options.\n");
    M_fdprint (rcf, "set color %s\n", uiG.Color ? "on" : "off");
    M_fdprint (rcf, "set funny %s\n\n", uiG.Funny ? "on" : "off");

    M_fdprint (rcf, "# This defines the loglevel and the location of the logfile(s).\n");
    M_fdprint (rcf, "# logging <level> <location>\n");
    M_fdprint (rcf, "#     level != 0 means enable loggin\n");
    M_fdprint (rcf, "#     level  & 2 means suppress on/offline changes\n");
    M_fdprint (rcf, "#     location   file to log into, or directory to log log into seperate files for each UIN\n");
    M_fdprint (rcf, "logging %d %s\n", uiG.LogLevel, uiG.LogPlace ? uiG.LogPlace : "");

    if (uiG.del_is_bs)
        M_fdprint (rcf, "#Del_is_Del\n");
    else
        M_fdprint (rcf, "Del_is_Del\n");
    if (uiG.last_uin_prompt)
        M_fdprint (rcf, "Last_UIN_Prompt\n");
    else
        M_fdprint (rcf, "#Last_UIN_Prompt\n");
    M_fdprint (rcf, "# 0 = Line break before message\n");
    M_fdprint (rcf, "# 1 = Just word wrap message as usual\n");
    M_fdprint (rcf, "# 2 = Indent message\n");
    M_fdprint (rcf, "# 3 = Line break before message unless message fits on current line\n");
    M_fdprint (rcf, "LineBreakType %d\n", uiG.line_break_type);
    if (uiG.Russian)
        M_fdprint (rcf, "\nRussian\n#if you want KOI8-R/U to CP1251 Cyrillic translation uncomment the above line.\n");
    else
        M_fdprint (rcf, "\n#Russian\n#if you want KOI8-R/U to CP1251 Cyrillic translation uncomment the above line.\n");
    if (uiG.JapaneseEUC)
    {
        M_fdprint (rcf, "\nJapaneseEUC\n#if you want Shift-JIS <-> EUC Japanese translation uncomment the above line.\n");
    }
    else
    {
        M_fdprint (rcf, "\n#JapaneseEUC\n#if you want Shift-JIS <-> EUC Japanese translation uncomment the above line.\n");
    }
    if (uiG.auto_resp)
        M_fdprint (rcf, "\n#Automatic responses on.\nAuto\n");
    else
        M_fdprint (rcf, "\n#Automatic responses off.\n#Auto\n");

    M_fdprint (rcf, "\n#in seconds\nAuto_Away %d\n", sess->away_time);
    M_fdprint (rcf, "\n#For dumb terminals that don't wrap set this.");
    M_fdprint (rcf, "\nScreen_Width %d\n", uiG.Max_Screen_Width);

    M_fdprint (rcf, "\n#Now auto response messages\n");
    M_fdprint (rcf, "auto_rep_str_away %s\n", uiG.auto_rep_str_away);
    M_fdprint (rcf, "auto_rep_str_na %s\n", uiG.auto_rep_str_na);
    M_fdprint (rcf, "auto_rep_str_dnd %s\n", uiG.auto_rep_str_dnd);
    M_fdprint (rcf, "auto_rep_str_occ %s\n", uiG.auto_rep_str_occ);
    M_fdprint (rcf, "auto_rep_str_inv %s\n", uiG.auto_rep_str_inv);

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

/*******************************************************
Gets config info from the rc file in the users home 
directory.
********************************************************/
void Get_Unix_Config_Info (Session *sess)
{
    FD_T rcf;

    rcf = open (rcfile, O_RDONLY);
    if (rcf == -1 && rcfiledef == 1)
    {
        char *tmp = strdup (rcfile);
        Set_rcfile ("");
        M_print (i18n (819, "Can't find rc file %s - using old location %s\n"),
                 tmp, rcfile);
        rcf = open (rcfile, O_RDONLY);
        Set_rcfile (NULL);
    }
    if (rcf == -1)
    {
        if (errno == ENOENT)    /* file not found */
        {
            Initalize_RC_File (sess);
        }
        else
        {
            perror ("Error reading config file exiting ");
            exit (1);
        }
    }
    else
    {
        Read_RC_File (sess, rcf);
    }
}

int Add_User (Session *sess, UDWORD uin, char *name)
{
    FD_T rcf;

    if (!uin)
        return 0;

    if (ContactFind (uin))
            return 0;

    rcf = open (rcfile, O_RDWR | O_APPEND);
    if (rcf == -1)
        return 0;
    M_fdprint (rcf, "%d %s\n", uin, name);
    close (rcf);

    ContactAdd (uin, name);

    CmdPktCmdContactList (sess);
    return 1;
}
