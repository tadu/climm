/* $Id$ */

/*********************************************
**********************************************
This is a file of general utility functions useful
for programming in general and icq in specific

This software is provided AS IS to be used in
whatever way you see fit and is placed in the
public domain.

Author : Matthew Smith April 23, 1998
Contributors :  airog (crabbkw@rose-hulman.edu) May 13, 1998


Changes :
  6-18-98 Added support for saving auto reply messages. Fryslan
 
**********************************************
**********************************************/
#include "micq.h"
#include "util.h"
#include "cmd_pkt_cmd_v5.h"
#include "contact.h"
#include "util_io.h"
#include "preferences.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <util_ui.h>
#ifdef _WIN32
#include <io.h>
#define S_IRUSR          _S_IREAD
#define S_IWUSR          _S_IWRITE
#else
#include <netinet/in.h>
#ifndef __BEOS__
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
typedef struct
{
    long tv_sec;
    long tv_usec;
}
timeval;
#endif

/********************************************
returns a string describing the status or
a NULL if no such string exists
*********************************************/
const char *Convert_Status_2_Str (UDWORD status)
{
    if (STATUS_OFFLINE == status)       /* this because -1 & 0xFFFF is not -1 */
    {
        return i18n (8, "Offline");
    }

    switch (status & 0x1ff)
    {
        case STATUS_ONLINE:
            return i18n (1, "Online");
        case STATUS_DND_99:
        case STATUS_DND:
            return i18n (2, "Do not disturb");
        case STATUS_AWAY:
            return i18n (3, "Away");
        case STATUS_OCCUPIED_MAC:
        case STATUS_OCCUPIED:
            return i18n (5, "Occupied");
        case STATUS_NA:
        case STATUS_NA_99:
            return i18n (4, "Not Available");
        case STATUS_INVISIBLE:
            return i18n (6, "Invisible");
        case STATUS_FREE_CHAT:
            return i18n (7, "Free for chat");
        default:
            return NULL;
    }
}

/********************************************
prints out the status of new_status as a string
if possible otherwise as a hex number
*********************************************/
void Print_Status (UDWORD new_status)
{
    BOOL inv = FALSE;
    
    if (STATUS_OFFLINE != new_status)
    {
        if (new_status & STATUS_INVISIBLE)
        {
            inv = TRUE;
            new_status = new_status & (~STATUS_INVISIBLE);
        }
    }
    if (Convert_Status_2_Str (new_status))
    {
        if (inv)
        {
            M_print ("%s-%s", i18n (6, "Invisible"), Convert_Status_2_Str (new_status));
            new_status = new_status | (STATUS_INVISIBLE);
        }
        else
        {
            M_print ("%s", Convert_Status_2_Str (new_status));
        }
        if (prG->verbose)
            M_print (" %08x", new_status);
    }
    else
    {
        if (inv)
            new_status = new_status | (STATUS_INVISIBLE);
        M_print ("%08lX", new_status);
    }
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
        perror (i18n (718, "Sorry, can't initialize Windows Sockets..."));
        exit (1);
    }
#endif
    M_print (i18n (756, "\nCreating Connection...\n"));
    sess->sok = UtilIOConnectUDP (sess->server, sess->server_port, STDERR);
    if ((sess->sok == -1) || (sess->sok == 0))
    {
        M_print (i18n (757, "Couldn't establish connection.\n"));
        exit (1);
    }
    M_print (i18n (758, "Sending Request...\n"));
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
        M_print (i18n (759, "Waiting for response....\n"));
        if (FD_ISSET (sess->sok, &readfds))
        {
            pak = UtilIORecvUDP (sess);
            if (!pak)
                continue;
            if (PacketReadAt2 (pak, 7) == SRV_NEW_UIN)
            {
                sess->uin = PacketReadAt4 (pak, 13);
                M_print (i18n (760, "\nYour new UIN is %s%ld%s!\n"), COLSERV, sess->uin, COLNONE);
                return;
            }
        }
        CmdPktCmdRegNewUser (sess, sess->passwd);
    }
}


void Print_IP (UDWORD uin)
{
    Contact *cont;

    if (!(cont = ContactFind (uin)) || (*(UDWORD *)(&cont->current_ip) == -1L))
    {
        M_print (i18n (761, "unknown"));
        return;
    }
    
    M_print ("%d.%d.%d.%d", cont->current_ip[0], cont->current_ip[1],
                            cont->current_ip[2], cont->current_ip[3]);
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
    char symbuf[256];           /* holds the path for a sym link */
    FILE *msgfd;
    va_list args;
    int k;
    char buf[2048];             /* this should big enough - This holds the message to be logged */
    char buffer[256];
    time_t timeval;
    struct stat statbuf;

    if (!(prG->flags & FLAG_LOG))
        return 0;

    if ((prG->flags & FLAG_LOG_ONOFF) && (LOG_ONLINE == type))
        return 0;

    if (!prG->logplace)
    {
        prG->logplace = malloc (strlen (PrefUserDir ()) + 10);
        strcpy (prG->logplace, PrefUserDir ());
        strcat (prG->logplace, "history/");
    }

    timeval = time (0);
    va_start (args, str);
    sprintf (buf, "\n%-24.24s ", ctime (&timeval));
    vsprintf (&buf[strlen (buf)], str, args);
    va_end (args);

    if (prG->logplace[strlen (prG->logplace) - 1] == '/')
    {
        if (stat (prG->logplace, &statbuf) == -1)
        {
            if (errno == ENOENT)
                mkdir (prG->logplace, 0700);
            else
                return -1;
        }
        sprintf (buffer, "%s%ld.log", prG->logplace, uin);

#if HAVE_SYMLINK
        if (ContactFindNick (uin))
        {
            sprintf (symbuf, "%s%s.log", prG->logplace, ContactFindNick (uin));
            symlink (buffer, symbuf);
        }
#endif
    }
    else
        strcpy (buffer, prG->logplace);

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
#ifdef ANSI_COLOR
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
                    return;
                if ((buf[i - j] & 0x7f) > 31)
                    M_print ("%c", buf[i - j]);
                else
                    M_print (".");
                if (((i - j) & 3) == 3)
                    M_print (" ");
            }
            if (i > len)
                return;
            M_print ("\n");
            if (i > 1000)
            {
                M_print ("...");
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
    who = ContactFindName (uin);

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
        M_print (i18n (584, "Warning! Script command '%s' failed with exit value %d\n"),
                 script, rc);
    }
    free (cmd);
    free (who);
    free (mydata);
}

