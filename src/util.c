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
#include "sendmsg.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <util_ui.h>
#ifdef _WIN32
#include <io.h>
#define S_IRUSR          _S_IREAD
#define S_IWUSR          _S_IWRITE
#else
#include <sys/time.h>
#include <netinet/in.h>
#ifndef __BEOS__
#include <arpa/inet.h>
#endif
#endif
#ifdef UNIX
#include <unistd.h>
#include <termios.h>
#include "mreadline.h"
#endif

#ifdef _WIN32
typedef struct
{
    long tv_sec;
    long tv_usec;
}
timeval;
#endif

static char *Log_Dir = NULL;
static BOOL Normal_Log = TRUE;

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
Prints a informative string to the screen.
describing the command
*********************************************/
void Print_CMD (UWORD cmd)
{
    switch (cmd)
    {
        case CMD_KEEP_ALIVE:
            M_print (i18n (743, "Keep Alive"));
            break;
        case CMD_KEEP_ALIVE2:
            M_print (i18n (744, "Secondary Keep Alive"));
            break;
        case CMD_CONT_LIST:
            M_print (i18n (745, "Contact List"));
            break;
        case CMD_INVIS_LIST:
            M_print (i18n (746, "Invisible List"));
            break;
        case CMD_VIS_LIST:
            M_print (i18n (747, "Visible List"));
            break;
        case CMD_RAND_SEARCH:
            M_print (i18n (748, "Random Search"));
            break;
        case CMD_RAND_SET:
            M_print (i18n (749, "Set Random"));
            break;
        case CMD_ACK_MESSAGES:
            M_print (i18n (750, "Delete Server Messages"));
            break;
        case CMD_LOGIN_1:
            M_print (i18n (751, "Finish Login"));
            break;
        case CMD_LOGIN:
            M_print (i18n (752, "Login"));
            break;
        case CMD_SENDM:
            M_print (i18n (753, "Send Message"));
            break;
        case CMD_INFO_REQ:
            M_print (i18n (754, "Info Request"));
            break;
        case CMD_EXT_INFO_REQ:
            M_print (i18n (755, "Extended Info Request"));
            break;
        default:
            M_print ("%04X", cmd);
            break;
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
        if (Verbose)
            M_print (" %06X", (UWORD) (new_status >> 8));
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

char *MsgEllipsis (char *msg)
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
        char *p, *q;
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
        return msg;
    if (buff != msg)
        strncpy (buff, msg, msgid_length - 3);
    buff[msgid_length - 3] = '\0';
    strcat (buff, "...");
    return buff;
}

/**********************************************
 * Returns the nick of a UIN if we know it else
 * it will return Unknown UIN
 **********************************************/
char *UIN2nick (UDWORD uin)
{
    int i;

    for (i = 0; i < Num_Contacts; i++)
    {
        if (Contacts[i].uin == uin)
            return Contacts[i].nick;
    }
    return NULL;
}

/**********************************************
 * Returns the nick of a UIN if we know it else
 * it will return UIN
 **********************************************/
char *UIN2Name (UDWORD uin)
{
    int i;
    char buff[100];

    for (i = 0; i < Num_Contacts; i++)
    {
        if (Contacts[i].uin == uin)
            return Contacts[i].nick;
    }
    snprintf (buff, 98, "%lu", uin);
    return strdup (buff);
}

/**********************************************
Prints the name of a user or their UIN if name
is not known.
***********************************************/
int Print_UIN_Name (UDWORD uin)
{
    int i;

    for (i = 0; i < Num_Contacts; i++)
    {
        if (Contacts[i].uin == uin)
        {
            M_print (COLCONTACT "%s" COLNONE, Contacts[i].nick);
            return i;
        }
    }
    M_print (COLCLIENT "%lu" COLNONE, uin);
    return -1;
}

/**********************************************
Prints the name of a user or their UIN if name
is not known, but use exactly 8 chars if possible.
***********************************************/
int Print_UIN_Name_10 (UDWORD uin)
{
    int i;

    for (i = 0; i < Num_Contacts; i++)
    {
        if (Contacts[i].uin == uin)
        {
            M_print (COLCONTACT "%10s" COLNONE, Contacts[i].nick);
            return i;
        }
    }

    M_print (COLCLIENT "%8lu" COLNONE, uin);
    return -1;
}

/**********************************************
Returns the contact list with uin
***********************************************/
CONTACT_PTR UIN2Contact (UDWORD uin)
{
    int i;

    for (i = 0; i < Num_Contacts; i++)
    {
        if (Contacts[i].uin == uin)
            break;
    }

    if (i == Num_Contacts)
    {
        return (CONTACT_PTR) NULL;
    }
    else
    {
        return &Contacts[i];
    }
}

/*********************************************
Converts a nick name into a uin from the contact
list.
**********************************************/
UDWORD nick2uin (char *nick)
{
    int i;
    BOOL non_numeric = FALSE;

    /*cut off whitespace at the end (i.e. \t or space */
    i = strlen (nick) - 1;
    while (isspace (nick[i]))
        i--;
    nick[i + 1] = '\0';


    for (i = 0; i < Num_Contacts; i++)
    {
        if (!strncasecmp (nick, Contacts[i].nick, 19))
        {
            if ((SDWORD) Contacts[i].uin > 0)
                return Contacts[i].uin;
            else
                return -Contacts[i].uin;        /* alias */
        }
    }
    for (i = 0; i < strlen (nick); i++)
    {
        if (!isdigit ((int) nick[i]))
        {
            non_numeric = TRUE;
            break;
        }
    }
    if (non_numeric)
        return -1;              /* not found and not a number */
    else
        return atoi (nick);
}

/**************************************************
Automates the process of creating a new user.
***************************************************/
void Init_New_User (void)
{
    SOK_T sok;
    srv_net_icq_pak pak;
    int s;
    struct timeval tv;
#ifdef _WIN32
    int i;
    WSADATA wsaData;
    FD_SET readfds;
#else
    fd_set readfds;
#endif

#ifdef _WIN32
    i = WSAStartup (0x0101, &wsaData);
    if (i != 0)
    {
#ifdef FUNNY_MSGS
        perror ("Windows Sockets broken blame Bill -");
#else
        perror ("Sorry, can't initialize Windows Sockets...");
#endif
        exit (1);
    }
#endif
    M_print (i18n (756, "\nCreating Connection...\n"));
    sok = Connect_Remote (server, remote_port, STDERR);
    if ((sok == -1) || (sok == 0))
    {
        M_print (i18n (757, "Couldn't establish connection\n"));
        exit (1);
    }
    M_print (i18n (758, "Sending Request...\n"));
    reg_new_user (sok, passwd);
    for (;;)
    {
#ifdef UNIX
        tv.tv_sec = 3;
        tv.tv_usec = 500000;
#else
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
#endif

        FD_ZERO (&readfds);
        FD_SET (sok, &readfds);

        /* don't care about writefds and exceptfds: */
        select (sok + 1, &readfds, NULL, NULL, &tv);
        M_print (i18n (759, "Waiting for response....\n"));
        if (FD_ISSET (sok, &readfds))
        {
            s = SOCKREAD (sok, &pak.head.ver, sizeof (pak) - 2);
            if (Chars_2_Word (pak.head.cmd) == SRV_NEW_UIN)
            {
                UIN = Chars_2_DW (pak.head.UIN);
                M_print (i18n (760, "\nYour new UIN is %s%ld%s!\n"), COLSERV, UIN, COLNONE);
                return;
            }
            else
            {
/*                Hex_Dump( &pak.head.ver, s);*/
            }
        }
        reg_new_user (sok, passwd);
    }
}


void Print_IP (UDWORD uin)
{
    int i;
#if 0
    struct in_addr sin;
#endif

    for (i = 0; i < Num_Contacts; i++)
    {
        if (Contacts[i].uin == uin)
        {
            if (*(UDWORD *) Contacts[i].current_ip != -1L)
            {
                M_print ("%d.%d.%d.%d", Contacts[i].current_ip[0],
                         Contacts[i].current_ip[1], Contacts[i].current_ip[2],
                         Contacts[i].current_ip[3]);
            }
            else
            {
                M_print (i18n (761, "unknown"));
            }
            return;
        }
    }
    M_print (i18n (761, "unknown"));
}

/************************************************
Gets the TCP port of the specified UIN
************************************************/
UDWORD Get_Port (UDWORD uin)
{
    int i;

    for (i = 0; i < Num_Contacts; i++)
    {
        if (Contacts[i].uin == uin)
        {
            return Contacts[i].port;
        }
    }
    return -1L;
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

/********************************************
Converts a UDWORD to
an intel endian character sequence 
*********************************************/
void DW_2_Chars (UBYTE * buf, UDWORD num)
{
    buf[3] = (unsigned char) ((num) >> 24) & 0x000000FF;
    buf[2] = (unsigned char) ((num) >> 16) & 0x000000FF;
    buf[1] = (unsigned char) ((num) >> 8) & 0x000000FF;
    buf[0] = (unsigned char) (num) & 0x000000FF;
}

/********************************************
Converts a UWORD to
an intel endian character sequence 
*********************************************/
void Word_2_Chars (UBYTE * buf, UWORD num)
{
    buf[1] = (unsigned char) (((unsigned) num) >> 8) & 0x00FF;
    buf[0] = (unsigned char) ((unsigned) num) & 0x00FF;
}

BOOL Log_Dir_Normal (void)
{
    return Normal_Log;
}

/************************************************************************
Sets the Log directory to newpath 
if newpath is null it will set it to a reasonable default
if newpath doesn't end with a valid directory seperator one is added.
the path used is returned.
*************************************************************************/
const char *Set_Log_Dir (const char *newpath)
{
    if (NULL == newpath)
    {
        char *path = 0;
        char *home;
        Normal_Log = TRUE;
#ifdef _WIN32
        path = ".\\";
#endif

#ifdef __amigaos__
        path = "PROGDIR:";
#endif

#ifdef UNIX
        home = getenv ("HOME");
        if (home || !path)
        {
            if (!home)
                home = "";
            path = malloc (strlen (home) + 2);
            strcpy (path, home);
            if (path[strlen (path) - 1] != '/')
                strcat (path, "/");
        }
#endif

        Log_Dir = path;
        return path;
    }
    else
    {
#ifdef _WIN32
        char sep = '\\';
#else
        char sep = '/';
#endif
        Normal_Log = FALSE;
        if (newpath[strlen (newpath) - 1] != sep)
        {
            Log_Dir = malloc (strlen (newpath) + 2);
            sprintf (Log_Dir, "%s%c", newpath, sep);
        }
        else
        {
            Log_Dir = strdup (newpath);
        }
        return Log_Dir;
    }
}

/************************************************************************
Returns the current directory used for logging.  If none has been 
specified it sets it tobe a reasonable default (~/)
*************************************************************************/
const char *Get_Log_Dir (void)
{
    if (Log_Dir)
        return Log_Dir;
    return Set_Log_Dir (NULL);
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
    const char *path;
/*    char *home; */
    struct stat statbuf;

    if (!LogType)
        return 0;

    if ((3 == LogType) && (LOG_ONLINE == type))
        return 0;

    timeval = time (0);
    va_start (args, str);
    sprintf (buf, "\n%-24.24s ", ctime (&timeval));
    vsprintf (&buf[strlen (buf)], str, args);

    path = Get_Log_Dir ();
    strcpy (buffer, path);

    switch (LogType)
    {
        case 1:
            strcat (buffer, "micq_log");
            break;
        case 2:
        case 3:
        default:
            strcat (buffer, "micq.log");
            if (-1 == stat (buffer, &statbuf))
            {
                if (errno == ENOENT)
                {
                    mkdir (buffer, 0700);
                }
                else
                {
                    return -1;
                }
            }
#ifdef _WIN32
            strcat (buffer, "\\");
#else
            strcat (buffer, "/");
#endif
            strcpy (symbuf, buffer);
            sprintf (&buffer[strlen (buffer)], "%ld.log", uin);

#ifdef UNIX
            if (NULL != UIN2nick (uin))
            {
                sprintf (&symbuf[strlen (symbuf)], "%s.log", UIN2nick (uin));
                symlink (buffer, symbuf);
            }
#endif


            break;
    }
    if ((msgfd = fopen (buffer, "a")) == (FILE *) NULL)
    {
        fprintf (stderr, "\nCouldn't open %s for logging\n", buffer);
        return (-1);
    }
/*     if ( ! strcasecmp(UIN2nick(uin),"Unknow UIN"))
         fprintf(msgfd, "\n%-24.24s %s %ld\n%s\n", ctime(&timeval), desc, uin, msg);
     else
         fprintf(msgfd, "\n%-24.24s %s %s\n%s\n", ctime(&timeval), desc, UIN2nick(uin), msg);*/

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
    va_end (args);

    fclose (msgfd);
#ifdef UNIX
    chmod (buffer, 0600);
#endif
    return (0);
}

/*************************************************
 clears the screen 
**************************************************/
void clrscr (void)
{
#ifdef UNIX
    system ("clear");
#else
#ifdef _WIN32
    system ("cls");
#else
    int x;
    char newline = '\n';

    for (x = 0; x <= 25; x++)
        M_print ("%c", newline);
#endif
#endif
}

/************************************************************
Displays a hex dump of buf on the screen.
*************************************************************/
void Hex_Dump (void *buffer, size_t len)
{
    int i;
    int j;
    int k;
    char *buf;

    buf = buffer;
    assert (len > 0);
    if ((len > 1000))
    {
        M_print (i18n (762, "Ack!!!!!!  %d\a\n"), len);
        return;
    }
    for (i = 0; i < len; i++)
    {
        M_print ("%02x ", (unsigned char) buf[i]);
        if ((i & 15) == 15)
        {
            M_print ("  ");
            for (j = 15; j >= 0; j--)
            {
                if (buf[i - j] > 31)
                    M_print ("%c", buf[i - j]);
                else
                    M_print (".");
                if (((i - j) & 3) == 3)
                    M_print (" ");
            }
            M_print ("\n");
        }
        else if ((i & 7) == 7)
            M_print ("- ");
        else if ((i & 3) == 3)
            M_print ("  ");
    }
    for (k = i % 16; k < 16; k++)
    {
        M_print ("    ");
        if ((k & 7) == 7)
            M_print ("  ");
        else if ((k & 3) == 3)
            M_print ("  ");
    }
    for (j = i % 16; j > 0; j--)
    {
        if (buf[i - j] > 31)
            M_print ("%c", buf[i - j]);
        else
            M_print (".");
        if (((i - j) & 3) == 3)
            M_print (" ");
    }
}
