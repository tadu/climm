/* $Id$ */

/*********************************************
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
#include <fcntl.h>
#include <util_ui.h>
#ifdef _WIN32
#include <io.h>
#define S_IRUSR          _S_IREAD
#define S_IWUSR          _S_IWRITE
#else
#include <netinet/in.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
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

/*
 * Return a string describing the status.
 * Result must be free()d.
 */
char *UtilStatus (UDWORD status)
{
    char buf[200];
    
    if (status == STATUS_OFFLINE)
        return (strdup (i18n (1969, "offline")));
 
    if (status & STATUSF_INV)
        snprintf (buf, sizeof (buf), "%s-", i18n (1975, "invisible"));
    else
        buf[0] = '\0';
    
    if (status & STATUSF_DND)
        snprintf (buf + strlen (buf), sizeof(buf) - strlen (buf), "%s", i18n (1971, "do not disturb"));
    else if (status & STATUSF_OCC)
        snprintf (buf + strlen (buf), sizeof(buf) - strlen (buf), "%s", i18n (1973, "occupied"));
    else if (status & STATUSF_NA)
        snprintf (buf + strlen (buf), sizeof(buf) - strlen (buf), "%s", i18n (1974, "not available"));
    else if (status & STATUSF_AWAY)
        snprintf (buf + strlen (buf), sizeof(buf) - strlen (buf), "%s", i18n (1972, "away"));
    else if (status & STATUSF_FFC)
        snprintf (buf + strlen (buf), sizeof(buf) - strlen (buf), "%s", i18n (1976, "free for chat"));
    else
        snprintf (buf + strlen (buf), sizeof(buf) - strlen (buf), "%s", i18n (1970, "online"));
    
    if (prG->verbose)
        snprintf (buf + strlen (buf), sizeof(buf) - strlen (buf), " %08lx", status);
    
    return strdup (buf);
}

/********************************************
returns a string describing the status
*********************************************/
const char *Convert_Status_2_Str (UDWORD status)
{
    if (STATUS_OFFLINE == (status | STATUSF_INV))       /* this because -1 & 0xFFFF is not -1 */
        return i18n (1969, "offline");

    if (status & STATUSF_INV)
        return i18n (1975, "invisible");
    if (status & STATUSF_DND)
        return i18n (1971, "do not disturb");
    if (status & STATUSF_OCC)
        return i18n (1973, "occupied");
    if (status & STATUSF_NA)
        return i18n (1974, "not available");
    if (status & STATUSF_AWAY)
        return i18n (1972, "away");
    if (status & STATUSF_FFC)
        return i18n (1976, "free for chat");
    return i18n (1970, "online");
}

/********************************************
prints out the status of new_status as a string
if possible otherwise as a hex number
*********************************************/
void Print_Status (UDWORD status)
{
    if (status != STATUS_OFFLINE && (status & STATUSF_INV))
        M_print ("%s-", i18n (1975, "invisible"));
    M_print (Convert_Status_2_Str (status & ~STATUSF_INV));
    if (prG->verbose)
        M_print (" %08x", status);
}

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
    sess->sok = UtilIOConnectUDP (sess->server, sess->port);
    if ((sess->sok == -1) || (sess->sok == 0))
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
                M_print (i18n (1760, "\nYour new UIN is %s%ld%s!\n"), COLSERVER, sess->uin, COLNONE);
                return;
            }
        }
        CmdPktCmdRegNewUser (sess, sess->passwd);
    }
}


void Print_IP (UDWORD uin)
{
    Contact *cont;

    if (!(cont = ContactFind (uin)) || (cont->outside_ip == -1L))
    {
        M_print (i18n (1761, "unknown"));
        return;
    }
    
    M_print (UtilIOIP (cont->outside_ip));
}

/*
 * Gets the TCP port of the specified UIN
 */
UDWORD Get_Port (UDWORD uin)
{
    Contact *cont;
    
    if (!(cont = ContactFind (uin)))
        return -1L;

    return cont->port;
}

/********************************************
Converts an intel endian character sequence to
a UDWORD
*********************************************/
UDWORD Chars_2_DW (UBYTE * buf)
{
    UDWORD i;

    i = buf[3];
    i <<= 8;
    i += buf[2];
    i <<= 8;
    i += buf[1];
    i <<= 8;
    i += buf[0];

    return i;
}

/********************************************
Converts an intel endian character sequence to
a UWORD
*********************************************/
UWORD Chars_2_Word (UBYTE * buf)
{
    UWORD i;

    i = buf[1];
    i <<= 8;
    i += buf[0];

    return i;
}

/*************************************************************************
 *        Function: log_event
 *        Purpose: Log the event provided to the log with a time stamp.
 *        Andrew Frolov dron@ilm.net
 *        6-20-98 Added names to the logs. Fryslan
 *************************************************************************/
int log_event (UDWORD uin, int type, char *str, ...)
{
    char symbuf[256], symbuf2[256];           /* holds the path for a sym link */
    FILE *msgfd;
    va_list args;
    int k;
    char buf[2048];             /* this should big enough - This holds the message to be logged */
    char buffer[256], *b, *home;
    time_t timeval;
    struct stat statbuf;

    if (!(prG->flags & FLAG_LOG))
        return 0;

    if (!(prG->flags & FLAG_LOG_ONOFF) && type == LOG_ONLINE)
        return 0;

    if (!prG->logplace)
    {
        prG->logplace = malloc (strlen (PrefUserDir ()) + 10);
        strcpy (prG->logplace, PrefUserDir ());
        strcat (prG->logplace, "history/");
    }

    timeval = time (0);
    va_start (args, str);
    snprintf (buf, sizeof (buf), "\n%-24.24s ", ctime (&timeval));
    vsprintf (&buf[strlen (buf)], str, args);
    va_end (args);

    if (prG->logplace[0] == '~' && prG->logplace[1] == '/' && (home = getenv ("HOME")))
        snprintf (buffer, sizeof (buffer), "%s%s", home, prG->logplace + 1);
    else
        snprintf (buffer, sizeof (buffer), "%s", prG->logplace);
    
    if (buffer[strlen (buffer) - 1] == '/')
    {
        if (stat (buffer, &statbuf) == -1)
        {
            if (errno == ENOENT)
                mkdir (buffer, 0700);
            else
                return -1;
        }
        snprintf (buffer + strlen (buffer), sizeof (buffer) - strlen (buffer), "%ld.log", uin);

#if HAVE_SYMLINK
        if (ContactFindNick (uin))
        {
            snprintf (symbuf, sizeof (symbuf), "%snick-%s.log", buffer, ContactFindNick (uin));
            snprintf (symbuf2, sizeof (symbuf2), "%ld.log", uin);
            for (b = symbuf + strlen (buffer); (b = strchr (b, '/')); )
                *b = '_';
            symlink  (symbuf2, symbuf);
        }
#endif
    }

    if (!(msgfd = fopen (buffer, "a")))
    {
        fprintf (stderr, "\nCouldn't open %s for logging\n", buffer);
        return -1;
    }

    if (strlen (buf))
    {
        fwrite ("<", 1, 1, msgfd);
        k = fwrite (buf, 1, strlen (buf), msgfd);
        if (k != strlen (buf))
        {
            perror ("\nLog file write error\n");
            return -1;
        }
        fwrite (">\n", 1, 2, msgfd);
    }
    fclose (msgfd);

#if HAVE_CHMOD
    chmod (buffer, 0600);
#endif
    return (0);
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

    assert (len >= 0);

    for (i = 0; i < ((len + 15) & ~15); i++)
    {
        if (i < len)
            M_print ("%02x ", buf[i]);
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
                    M_print ("%c", buf[i - j]);
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
        M_print (i18n (1584, "Warning! Script command '%s' failed with exit value %d\n"),
                 script, rc);
    }
    free (cmd);
    free (mydata);
    free (who);
}

const char *UtilFill (const char *fmt, ...)
{
    char buf[1024];
    va_list args;

    va_start (args, fmt);
    vsnprintf (buf, sizeof (buf), fmt, args);
    va_end (args);

    return strdup (buf);
}

UDWORD UtilCheckUIN (Session *sess, UDWORD uin)
{
    if (!ContactFind (uin))
    {
        Contact *cont;
        
        cont = ContactAdd (uin, ContactFindName (uin));
        if (cont)
            cont->flags |= CONT_TEMPORARY;
    }
    return uin;
}
