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
#include "cmd_user.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
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

/**************************************************
Automates the process of creating a new user.
***************************************************/
void Init_New_User (Connection *conn)
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

    conn->ver = 5;
#ifdef _WIN32
    i = WSAStartup (0x0101, &wsaData);
    if (i != 0)
    {
        perror (i18n (1624, "Sorry, can't initialize Windows Sockets..."));
        exit (1);
    }
#endif
    M_print (i18n (1756, "\nCreating Connection...\n"));
    UtilIOConnectUDP (conn);
    if (conn->sok == -1)
    {
        M_print (i18n (1757, "Couldn't establish connection.\n"));
        exit (1);
    }
    M_print (i18n (1758, "Sending Request...\n"));
    CmdPktCmdRegNewUser (conn, conn->passwd);
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
        FD_SET (conn->sok, &readfds);

        /* don't care about writefds and exceptfds: */
        select (conn->sok + 1, &readfds, NULL, NULL, &tv);
        M_print (i18n (1759, "Waiting for response....\n"));
        if (FD_ISSET (conn->sok, &readfds))
        {
            pak = UtilIOReceiveUDP (conn);
            if (!pak)
                continue;
            if (PacketReadAt2 (pak, 7) == SRV_NEW_UIN)
            {
                conn->uin = PacketReadAt4 (pak, 13);
                M_printf (i18n (1760, "\nYour new UIN is %s%ld%s!\n"), COLSERVER, conn->uin, COLNONE);
                return;
            }
        }
        CmdPktCmdRegNewUser (conn, conn->passwd);
    }
}

#define LOG_MAX_PATH 255
#define DSCSIZ 192 /* Maximum length of log file descriptor lines. */

/*
 * Log the event provided to the log with a time stamp.
 */
int putlog (Connection *conn, time_t stamp, UDWORD uin, 
            UDWORD status, enum logtype level, UWORD type, const char *log)
{
    char buffer[LOG_MAX_PATH + 1],                   /* path to the logfile */
        symbuf[LOG_MAX_PATH + 1];                     /* path of a sym link */
    char *target = buffer;                        /* Target of the sym link */
    Contact *cont = ContactUIN (conn, uin);
    const char *username = PrefLogName (prG);
    FILE *logfile;
    int fd;
    char *t = NULL, *mylog; /* Buffer to compute log entry */
    UDWORD size = 0;
    size_t lcnt;
    char *pos, *indic;
    time_t now;
    struct tm *utctime;

    if (!cont)
        return 0;

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
    
    now = time (NULL);

    for (lcnt = 1, pos = mylog = strdup (log); *pos; lcnt += *pos++ == '\n')
        if (pos[0] == '\r' && pos[1] == '\n')
            *pos = ' ';

    utctime = gmtime (&now);

    t = s_catf (t, &size, "# %04d%02d%02d%02d%02d%02d%s%.0ld ",
        utctime->tm_year + 1900, utctime->tm_mon + 1, 
        utctime->tm_mday, utctime->tm_hour, utctime->tm_min, 
        utctime->tm_sec, stamp != -1 ? "/" : "", 
        stamp != NOW ? stamp - now : 0);

    pos = strchr (cont->nick, ' ') ? "\"" : "";
    
    switch (conn->type)
    {
        case TYPE_SERVER_OLD:
            t = s_catf (t, &size, "[icq5:%lu]!%s %s %s%s%s[icq5:%lu+%lX]", 
                conn->uin, username, indic, pos, cont->uin ? cont->nick : "", pos, uin, status);
            break;
        case TYPE_SERVER:
            t = s_catf (t, &size, "[icq8:%lu]!%s %s %s%s%s[icq8:%lu+%lX]", 
                conn->uin, username, indic, pos, cont->uin ? cont->nick : "", pos, uin, status);
            break;
        case TYPE_MSGLISTEN:
        case TYPE_MSGDIRECT:
            t = s_catf (t, &size, "%s %s %s%s%s[tcp:%lu+%lX]",
                      username, indic, pos, cont->uin ? cont->nick : "", pos, uin, status);
            break;
        default:
            t = s_catf (t, &size, "%s %s %s%s%s[tcp:%lu+%lX]",
                      username, indic, pos, cont->uin ? cont->nick : "", pos, uin, status);
            break;
    }

    if ((lcnt += *mylog != '\0') != 0)
        t = s_catf (t, &size, " +%u", lcnt);

    if (type != 0xFFFF && type != MSG_NORM)
        t = s_catf (t, &size, " (%u)", type);
    
    t = s_cat (t, &size, "\n");

    if (*mylog)
    {
        t = s_cat (t, &size, mylog);
        t = s_cat (t, &size, "\n");
    }
    free (mylog);

    if (!prG->logplace)
        prG->logplace = "history/";

    /* Check for '/' below doesn't work for empty strings. */
    assert (*prG->logplace != '\0');

    snprintf (buffer, sizeof (buffer), s_realpath (prG->logplace));
    target += strlen (buffer);
    
    if (target[-1] == '/')
    {
        if (mkdir (buffer, S_IRWXU) == -1 && errno != EEXIST)
            return -1;

        snprintf (target, buffer + sizeof (buffer) - target, "%lu.log", uin);

#if HAVE_SYMLINK
        if (cont->uin)
        {
            char *b = target - buffer + symbuf;

            strncpy (symbuf, buffer, target - buffer);
            snprintf (b, symbuf + sizeof (symbuf) - b, "nick-%s.log", cont->nick);

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
        free (t);
        return -1;
    }
    if (!(logfile = fdopen (fd, "a")))
    {
        fprintf (stderr, "\nCouldn't open %s for logging\n", buffer);
        free (t);
        close (fd);
        return -1;
    }

    /* Write the log entry as a single chunk to cope with concurrent writes 
     * of multiple mICQ instances.
     */
    fputs (t, logfile);
    free (t);

    /* Closes stream and file descriptor. */
    fclose (logfile);

    return 0;
}

/*
 * Executes a program and feeds some shell-proof information data into it
 */
void EventExec (Contact *cont, const char *script, UBYTE type, UDWORD msgtype, const char *text)
{
    static int rc;
    char *mytext, *mynick, *myscript, *tmp;
    const char *mytype, *cmd;

    mytext = strdup (text ? text : "");
    mynick = strdup (cont ? cont->nick : "");
    mytype = (type == 1 ? "msg" : type == 2 ? "on" : type == 3 ? "off" : "other");
    myscript = strdup (s_realpath (script));

    for (tmp = mytext; *tmp; tmp++)
        if (*tmp == '\'' || *tmp == '\\')
            *tmp = '"';
    for (tmp = mynick; *tmp; tmp++)
        if (*tmp == '\'' || *tmp == '\\')
            *tmp = '"';

    cmd = s_sprintf ("%s icq %ld '%s' global %s %ld '%s'",
                     myscript, cont ? cont->uin : 0, mynick, mytype, msgtype, mytext);

    rc = system (cmd);
    if (rc)
        M_printf (i18n (2222, "Script command '%s' failed: %s (%d).\n"),
                 myscript, strerror (rc), rc);
    free (mynick);
    free (mytext);
    free (myscript);
}
