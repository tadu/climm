/* $Id$ */

/*
 * This file is the general dumpster for functions
 * that better be rewritten.

********************************************
**********************************************
This is a file of general utility functions useful
for programming in general and icq in specific

Author : Matthew Smith April 23, 1998
Contributors :  airog (crabbkw@rose-hulman.edu) May 13, 1998
Copyright: This file may be distributed under version 2 of the GPL licence.


Changes :
  6-18-98 Added support for saving auto reply messages. Fryslan
 
**********************************************
**********************************************/
#include "micq.h"
#include "util.h"
#include "cmd_pkt_cmd_v5.h"
#include "cmd_pkt_v8_snac.h"
#include "contact.h"
#include "session.h"
#include "util_io.h"
#include "preferences.h"
#include "packet.h"
#include "util_str.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <util_ui.h>
#ifdef _WIN32
#include <io.h>
#define S_IRUSR          _S_IREAD
#define S_IWUSR          _S_IWRITE
#endif
#include <netinet/in.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_TERMIOS_H
#include <termios.h>
#endif
#include "mreadline.h"

#ifdef _WIN32
struct timeval
{
    long tv_sec;
    long tv_usec;
};
#endif

static char username[L_cuserid + SYS_NMLN] = "";

/**********************************************
 * Returns at most MSGID_LENGTH characters of a
 * message, possibly using ellipsis.
 **********************************************/

char *MsgEllipsis (const char *msg)
{
    static char buff[MSGID_LENGTH + 2];
    int screen_width, msgid_length;

    screen_width = Get_Max_Screen_Width ();
    if (screen_width < 10)
        screen_width = 10;
    msgid_length = screen_width - (strlen ("##:##:## .......... " MSGSENTSTR) % screen_width);
    if (msgid_length < 5)
        msgid_length += screen_width;
    if (msgid_length > MSGID_LENGTH)
        msgid_length = MSGID_LENGTH;

    if (strchr (msg, '\n') || strchr (msg, '\r'))
    {
        const char *p;
        char *q;
        int i;
        for (p = msg, q = buff, i = 0; *p && i <= MSGID_LENGTH; i++, q++, p++)
        {
            if (*p == '\r' || *p == '\n')
            {
                *(q++) = '¶';
                break;
            }
            *q = *p;
        }
        *q = '\0';
        msg = buff;
    }


    if (strlen (msg) <= msgid_length)
    {
        strncpy (buff, msg, sizeof (buff) - 1);
        return buff;
    }
    if (buff != msg)
        strncpy (buff, msg, msgid_length - 3);
    buff[msgid_length - 3] = '\0';
    strcat (buff, "...");
    return buff;
}

/**************************************************
Automates the process of creating a new user.
***************************************************/
void Init_New_User (Session *sess)
{
    Packet *pak;
    struct timeval tv;
#ifdef _WIN32
    int i;
    WSADATA wsaData;
    FD_SET readfds;
#else
    fd_set readfds;
#endif

    sess->ver = 5;
#ifdef _WIN32
    i = WSAStartup (0x0101, &wsaData);
    if (i != 0)
    {
        perror (i18n (1624, "Sorry, can't initialize Windows Sockets..."));
        exit (1);
    }
#endif
    M_print (i18n (1756, "\nCreating Connection...\n"));
    UtilIOConnectUDP (sess);
    if (sess->sok == -1)
    {
        M_print (i18n (1757, "Couldn't establish connection.\n"));
        exit (1);
    }
    M_print (i18n (1758, "Sending Request...\n"));
    CmdPktCmdRegNewUser (sess, sess->passwd);
    for (;;)
    {
#ifdef _WIN32
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
#else
        tv.tv_sec = 3;
        tv.tv_usec = 500000;
#endif

        FD_ZERO (&readfds);
        FD_SET (sess->sok, &readfds);

        /* don't care about writefds and exceptfds: */
        select (sess->sok + 1, &readfds, NULL, NULL, &tv);
        M_print (i18n (1759, "Waiting for response....\n"));
        if (FD_ISSET (sess->sok, &readfds))
        {
            pak = UtilIOReceiveUDP (sess);
            if (!pak)
                continue;
            if (PacketReadAt2 (pak, 7) == SRV_NEW_UIN)
            {
                sess->uin = PacketReadAt4 (pak, 13);
                M_printf (i18n (1760, "\nYour new UIN is %s%ld%s!\n"), COLSERVER, sess->uin, COLNONE);
                return;
            }
        }
        CmdPktCmdRegNewUser (sess, sess->passwd);
    }
}

#define LOG_MAX_PATH 255
#define DSCSIZ 192 /* Maximum length of log file descriptor lines. */

/*************************************************************************
 *        Function: putlog
 *        Purpose: Log the event provided to the log with a time stamp.
 *        Andrew Frolov dron@ilm.net
 *        6-20-98 Added names to the logs. Fryslan
 *************************************************************************/
int putlog (Session *sess, time_t stamp, UDWORD uin, 
            UDWORD status, enum logtype level, UWORD type, char *str, ...)
{
    static const char deflogdir[] = "history/";
    char buffer[LOG_MAX_PATH + 1],                   /* path to the logfile */
        symbuf[LOG_MAX_PATH + 1];                     /* path of a sym link */
    char *target = buffer;                        /* Target of the sym link */
    const char *nick = ContactFindNick (uin);
    FILE *logfile;
    int fd;
    va_list args;
    size_t l, lcnt;
    char buf[2048], *home;                   /* Buffer to compute log entry */
    char *pos, *indic;
    time_t now;
    struct tm *utctime;

    if (~prG->flags & FLAG_LOG)
        return 0;

    if (~prG->flags & FLAG_LOG_ONOFF 
        && (level == LOG_ONLINE || level == LOG_CHANGE || level == LOG_OFFLINE))
        return 0;

    switch (level)
    {
        case LOG_MISC:    indic = "--"; break;
        case LOG_SENT:    indic = "->"; break;
        case LOG_AUTO:    indic = "=>"; break;
        case LOG_RECVD:   indic = "<-"; break;
        case LOG_CHANGE:  indic = "::"; break;
        case LOG_REPORT:  indic = ":!"; break;
        case LOG_GUESS:   indic = ":?"; break;
        case LOG_ONLINE:  indic = ":+"; break;
        case LOG_OFFLINE: indic = ":-"; break;
        case LOG_ACK:     indic = "<#"; break;
        case LOG_ADDED:   indic = "<*"; break;
        case LOG_LIST:    indic = "*>"; break;
        case LOG_EVENT:   indic = "<="; break;
        default:          indic = "=="; break;
    }
    
    pos = buf + DSCSIZ;

    now = time (NULL);
    va_start (args, str);
    vsnprintf (pos, sizeof (buf) - DSCSIZ, str, args);
    va_end (args);

    l = strlen (pos);

    for (lcnt = 0; pos < buf + DSCSIZ + l; lcnt += *pos++ == '\n')
        if (pos[0] == '\r' && pos[1] == '\n')
            *pos = ' ';

    utctime = gmtime (&now);

    snprintf (buf, DSCSIZ, "# %04d%02d%02d%02d%02d%02d%s%.0ld ",
        utctime->tm_year + 1900, utctime->tm_mon + 1, 
        utctime->tm_mday, utctime->tm_hour, utctime->tm_min, 
        utctime->tm_sec, stamp != -1 ? "/" : "", 
        stamp != NOW ? stamp - now : 0);

    l = strlen (buf);

    pos = strchr (nick, ' ') ? "\"" : "";
    
    switch (sess->type)
    {
        case TYPE_SERVER_OLD:
            snprintf (buf + l, DSCSIZ - l, "[icq5:%lu]!%s %s %s%s%s[icq5:%lu+%lX]", 
                sess->uin, username, indic, pos, nick ? nick : "", pos, uin, status);
            break;
        case TYPE_SERVER:
            snprintf (buf + l, DSCSIZ - l, "[icq8:%lu]!%s %s %s%s%s[icq8:%lu+%lX]", 
                sess->uin, username, indic, pos, nick ? nick : "", pos, uin, status);
            break;
        case TYPE_MSGLISTEN:
        case TYPE_MSGDIRECT:
            snprintf (buf + l, DSCSIZ - l, "%s %s %s%s%s[tcp:%lu+%lX]", username, 
                indic, pos, nick ? nick : "", pos, uin, status);
            break;
        default:
            snprintf (buf + l, DSCSIZ - l, "%s %s %s%s%s[tcp:%lu+%lX]", username, 
                indic, pos, nick ? nick : "", pos, uin, status);
            break;
    }
    l += strlen (buf + l);

    if (lcnt != 0)
    {
        snprintf (buf + l, DSCSIZ - l, " +%u", lcnt);
        l += strlen (buf + l);
    }

    if (type != 0xFFFF && type != MSG_NORM)
    {
        snprintf (buf + l, DSCSIZ - l, " (%u)", type);
        l += strlen (buf + l);
    }
    buf[l++] = '\n';
    pos = buf + DSCSIZ;
    
    memmove (buf + l, pos, strlen (pos) + 1);

    if (!prG->logplace)
    {
        register const char *userdir = PrefUserDir ();
        prG->logplace = strcat (strcpy (malloc (strlen (userdir) 
            + sizeof (deflogdir)), userdir), deflogdir);
    }

    /* Check for '/' below doesn't work for empty strings. */
    assert (*prG->logplace != '\0');

    if (prG->logplace[0] == '~' && prG->logplace[1] == '/' && (home = getenv ("HOME")))
        snprintf (buffer, sizeof (buffer), "%s%s", home, prG->logplace + 1);
    else
        snprintf (buffer, sizeof (buffer), "%s", prG->logplace);

    target += strlen (buffer);
    
    if (target[-1] == '/')
    {
        if (mkdir (prG->logplace, S_IRWXU) == -1 && errno != EEXIST)
            return -1;

        snprintf (target, buffer + sizeof (buffer) - target, "%lu.log", uin);

#if HAVE_SYMLINK
        if (nick != NULL)
        {
            char *b = target - buffer + symbuf;

            strncpy (symbuf, buffer, target - buffer);
            snprintf (b, symbuf + sizeof (symbuf) - b, "nick-%s.log", nick);

            while ((b = strchr (b, '/')) != NULL)
                *b = '_';
            symlink (target, symbuf);
        }
#endif
    }

    if ((fd = open (buffer, O_CREAT | O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR))
        == -1)
    {
        fprintf (stderr, "\nCouldn't open %s for logging\n", buffer);
        return -1;
    }
    if (!(logfile = fdopen (fd, "a")))
    {
        fprintf (stderr, "\nCouldn't open %s for logging\n", buffer);
        close (fd);
        return -1;
    }

    /* Write the log entry as a single chunk to cope with concurrent writes 
     * of multiple mICQ instances.
     */
    fputs (buf, logfile);

    /* Closes stream and file descriptor. */
    fclose (logfile);

    return 0;
}

void init_log (void)
{
    const char *me = getenv ("LOGNAME");
    char *hostname = username;
#ifndef HAVE_GETHOSTNAME
    struct utsname name;
#endif

    if (me != NULL)
    {
        strncpy (username, me, sizeof (username));
        username[sizeof (username) - 1] = '\0';
        hostname += strlen (username);
    }

    hostname++;
#ifdef HAVE_GETHOSTNAME
    if (gethostname (hostname, username + sizeof (username) - hostname) == 0)
        hostname[-1] = '@';
#else
    if (uname (&name) == 0)
    {
        strncpy (hostname, name.nodename, 
            username + sizeof (username) - hostname);
        username[sizeof (username) - 1] = '\0';
        hostname[-1] = '@';
    }
#endif

    tzset ();
}

/*************************************************
 clears the screen 
**************************************************/
void clrscr (void)
{
#ifdef ANSI_TERM
    printf ("\x1b[H\x1b[J");
#else
    M_print ("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
#endif
}

/*
 * Displays a hex dump of buf on the screen.
 */
void Hex_Dump (void *buffer, size_t len)
{
    int i, j;
    unsigned char *buf = buffer;

    if (!len)
        return;

    assert (len > 0);

    for (i = 0; i < ((len + 15) & ~15); i++)
    {
        if (i < len)
            M_printf ("%02x ", buf[i]);
        else
            M_print ("   ");
        if ((i & 15) == 15)
        {
            M_print ("  ");
            for (j = 15; j >= 0; j--)
            {
                if (i - j >= len)
                    break;
                if ((buf[i - j] & 0x7f) > 31)
                    M_printf ("%c", buf[i - j]);
                else
                    M_print (".");
                if (((i - j) & 3) == 3)
                    M_print (" ");
            }
            M_print ("\n");
            if (i > len)
                return;
            if (i > 1000)
            {
                M_print ("...\n");
                return;
            }
        }
        else if (i < len && (i & 7) == 7)
            M_print ("- ");
        else if ((i & 3) == 3)
            M_print ("  ");
    }
}

/*
 * Executes a program and feeds some shell-proof information data into it
 */
void ExecScript (char *script, UDWORD uin, long num, char *data)
{
    int cmdlen, rc;
    char *mydata, *cmd, *tmp, *who;

    mydata = strdup (data ? data : "");
    who = strdup (ContactFindName (uin));

    for (tmp = mydata; *tmp; tmp++)
        if (*tmp == '\'')
            *tmp = '"';
    for (tmp = who; *tmp; tmp++)
        if (*tmp == '\'')
            *tmp = '"';

    cmdlen = strlen (script) + strlen (mydata) + strlen (who) + 20;
    cmd = (char *) malloc (cmdlen);

    if (data)
        snprintf (cmd, cmdlen, "%s '%s' %ld '%s'", script, who, num, mydata);
    else if (uin)
        snprintf (cmd, cmdlen, "%s '%s'", script, who);
    else
        snprintf (cmd, cmdlen, "%s", script);
    rc = system (cmd);
    if (rc)
    {
        M_printf (i18n (1584, "Warning! Script command '%s' failed with exit value %d\n"),
                 script, rc);
    }
    free (cmd);
    free (mydata);
    free (who);
}

UDWORD UtilCheckUIN (Session *sess, UDWORD uin)
{
    if (!ContactFind (uin))
        ContactAdd (uin, NULL);

    return uin;
}
