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
#include "util_ui.h"
#include "preferences.h"
#include "packet.h"
#include "cmd_user.h"
#include "conv.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif
#include <fcntl.h>
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_TERMIOS_H
#include <termios.h>
#endif
#if HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
#include "mreadline.h"

/**************************************************
Automates the process of creating a new user.
***************************************************/
void Init_New_User (Connection *conn)
{
    Packet *pak;
    struct timeval tv;
    fd_set readfds;

#ifdef _WIN32
    int rc;
    WSADATA wsaData;

    if ((rc = WSAStartup (0x0101, &wsaData)))
    {
        perror (i18n (1624, "Sorry, can't initialize Windows Sockets..."));
        exit (1);
    }
#endif

    conn->ver = 5;
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
int putlog (Connection *conn, time_t stamp, Contact *cont, 
            UDWORD status, enum logtype level, UWORD type, const char *log)
{
    char buffer[LOG_MAX_PATH + 1];                   /* path to the logfile */
#if HAVE_SYMLINK
    char symbuf[LOG_MAX_PATH + 1];                  /* path of a sym link */
#endif
    char *target = buffer;                         /* Target of the sym link */
    const char *username = PrefLogName (prG);
    FILE *logfile;
    int fd;
    char *mylog; /* Buffer to compute log entry */
    str_s t = { NULL, 0, 0 };
    size_t lcnt;
    char *pos, *indic;
    time_t now;
    struct tm *utctime;

    if (!cont)
        return 0;

    switch (level)
    {
        case LOG_MISC:    indic = "--"; break;
        case LOG_SENT:    indic = "->"; if (!ContactPrefVal (cont, CO_LOGMESS)) return 0; break;
        case LOG_AUTO:    indic = "=>"; if (!ContactPrefVal (cont, CO_LOGMESS)) return 0; break;
        case LOG_RECVD:   indic = "<-"; if (!ContactPrefVal (cont, CO_LOGMESS)) return 0; break;
        case LOG_CHANGE:  indic = "::"; if (!ContactPrefVal (cont, CO_LOGCHANGE)) return 0; break;
        case LOG_REPORT:  indic = ":!"; if (!ContactPrefVal (cont, CO_LOGCHANGE)) return 0; break;
        case LOG_GUESS:   indic = ":?"; if (!ContactPrefVal (cont, CO_LOGCHANGE)) return 0; break;
        case LOG_ONLINE:  indic = ":+"; if (!ContactPrefVal (cont, CO_LOGONOFF)) return 0; break;
        case LOG_OFFLINE: indic = ":-"; if (!ContactPrefVal (cont, CO_LOGONOFF)) return 0; break;
        case LOG_ACK:     indic = "<#"; if (!ContactPrefVal (cont, CO_LOGMESS)) return 0; break;
        case LOG_ADDED:   indic = "<*"; if (!ContactPrefVal (cont, CO_LOGMESS)) return 0; break;
        case LOG_LIST:    indic = "*>"; break;
        case LOG_EVENT:   indic = "<="; break;
        default:          indic = "=="; break;
    }
    
    now = time (NULL);

    for (lcnt = 0, pos = mylog = strdup (ConvCrush0xFE (log)); *pos; lcnt += *pos++ == '\n')
        if (pos[0] == '\r' && pos[1] == '\n')
            *pos = ' ';

    utctime = gmtime (&now);

    s_init (&t, "", 100);
    s_catf (&t, "# %04d%02d%02d%02d%02d%02d%s%.0ld ",
        utctime->tm_year + 1900, utctime->tm_mon + 1, 
        utctime->tm_mday, utctime->tm_hour, utctime->tm_min, 
        utctime->tm_sec, stamp != -1 ? "/" : "", 
        stamp != NOW ? stamp - now : 0L);

    pos = strchr (cont->nick, ' ') ? "\"" : "";
    
    switch (conn->type)
    {
        case TYPE_SERVER_OLD:
            s_catf (&t, "[icq5:%lu]!%s %s %s%s%s[icq5:%lu+%lX]", 
                conn->uin, username, indic, pos, cont->uin ? cont->nick : "", pos, cont->uin, status);
            break;
        case TYPE_SERVER:
            s_catf (&t, "[icq8:%lu]!%s %s %s%s%s[icq8:%lu+%lX]", 
                conn->uin, username, indic, pos, cont->uin ? cont->nick : "", pos, cont->uin, status);
            break;
        case TYPE_MSGLISTEN:
        case TYPE_MSGDIRECT:
            s_catf (&t, "%s %s %s%s%s[tcp:%lu+%lX]",
                      username, indic, pos, cont->uin ? cont->nick : "", pos, cont->uin, status);
            break;
        default:
            s_catf (&t, "%s %s %s%s%s[tcp:%lu+%lX]",
                      username, indic, pos, cont->uin ? cont->nick : "", pos, cont->uin, status);
            break;
    }

    if ((lcnt += *mylog != '\0') != 0)
        s_catf (&t, " +%u", (unsigned)lcnt);

    if (type != 0xFFFF && type != MSG_NORM)
        s_catf (&t, " (%u)", type);
    
    s_catc (&t, '\n');

    if (*mylog)
    {
        s_cat (&t, mylog);
        s_catc (&t, '\n');
    }
    free (mylog);

    if (!prG->logplace)
        prG->logplace = "history" _OS_PATHSEPSTR;

    /* Check for '/' below doesn't work for empty strings. */
    assert (*prG->logplace != '\0');

    snprintf (buffer, sizeof (buffer), s_realpath (prG->logplace));
    target += strlen (buffer);
    
    if (target[-1] == _OS_PATHSEP)
    {
        if (mkdir (buffer, S_IRWXU) == -1 && errno != EEXIST)
        {
            s_done (&t);
            return -1;
        }

        snprintf (target, buffer + sizeof (buffer) - target, "%lu.log", cont->uin);

#if HAVE_SYMLINK
        if (cont->uin)
        {
            char *b = target - buffer + symbuf;

            strncpy (symbuf, buffer, target - buffer);
            snprintf (b, symbuf + sizeof (symbuf) - b, "nick-%s.log", cont->nick);

            while ((b = strchr (b, _OS_PATHSEP)) != NULL)
                *b = '_';
            symlink (target, symbuf);
        }
#endif
    }

    if ((fd = open (buffer, O_CREAT | O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR))
        == -1)
    {
        fprintf (stderr, "\nCouldn't open %s for logging\n", buffer);
        s_done (&t);
        return -1;
    }
    if (!(logfile = fdopen (fd, "a")))
    {
        fprintf (stderr, "\nCouldn't open %s for logging\n", buffer);
        s_done (&t);
        close (fd);
        return -1;
    }

    /* Write the log entry as a single chunk to cope with concurrent writes 
     * of multiple mICQ instances.
     */
    fputs (t.txt, logfile);
    s_done (&t);

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
    mytype = (type == 1 ? "msg" : type == 2 ? "on" : type == 3 ? "off" : 
              type == 4 ? "beep" : type == 5 ? "status" : "other");
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
