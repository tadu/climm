/* $Id$ */
/* Copyright ?*/

#include "micq.h"
#include "mreadline.h"
#include "util_ui.h"
#include "util_io.h"
#include "util.h"
#include "contact.h"
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
#include <signal.h>


#ifdef _WIN32
#include <io.h>
#define S_IRUSR        _S_IREAD
#define S_IWUSR        _S_IWRITE
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
#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

static BOOL No_Prompt = FALSE;

#ifdef HAVE_TCGETATTR
struct termios sattr;
#endif

/***************************************************************
Turns keybord echo off for the password
****************************************************************/
SDWORD Echo_Off (void)
{
#ifdef HAVE_TCGETATTR
    struct termios attr;

    if (tcgetattr (STDIN_FILENO, &attr) != 0)
        return (-1);
    
    memcpy (&sattr, &attr, sizeof (sattr));
    
    attr.c_lflag &= ~(ECHO);
    if (tcsetattr (STDIN_FILENO, TCSAFLUSH, &attr) != 0)
        return (-2);
#endif
    return 0;
}


/***************************************************************
Turns keybord echo back on after the password
****************************************************************/
SDWORD Echo_On (void)
{
#ifdef HAVE_TCGETATTR
    if (tcsetattr (STDIN_FILENO, TCSAFLUSH, &sattr) != 0)
        return (-1);
#endif
    return 0;
}

static volatile UDWORD scrwd = 0;
static RETSIGTYPE micq_sigwinch_handler (int a)
{
    struct winsize ws;

    scrwd = 0;
    ioctl (STDIN, TIOCGWINSZ, &ws);
    scrwd = ws.ws_col;
};

UWORD Get_Max_Screen_Width ()
{
    UDWORD scrwdtmp = scrwd;

    if (scrwdtmp)
        return scrwdtmp;

    micq_sigwinch_handler (0);
    if ((scrwdtmp = scrwd))
    {
        if (signal (SIGWINCH, &micq_sigwinch_handler) == SIG_ERR)
            scrwd = 0;
        return scrwdtmp;
    }
    if (prG->screen)
        return prG->screen;
    return 80;                  /* a reasonable screen width default. */
}

#ifdef WORD_WRAP
static int CharCount = 0;       /* number of characters printed on line. */
static int IndentCount = 0;

static void M_prints (const char *str)
{
    const char *p, *s, *t;
    int i;
    int sw = Get_Max_Screen_Width () - IndentCount;

    for (; *str; str++)
    {
        for (p = s = str; *p; p++)
        {
            if (strchr ("\n\r\t\a\x1b", *p))
            {
                if (s != str)
                    p = s;
                break;
            }
            if (strchr ("-.,_:;!?/ ", *p))
            {
                t = p + 1;
                if (t - str <= sw - CharCount)
                    s = t;
                else
                {
                    if (s != str)
                        p = s;
                    else
                        p = t;
                    break;
                }
            }
        }
        if (p != str)           /* Print out (block of) word(s) */
        {
            while (p - str > sw)
            {
                printf ("%.*s%*s", sw - CharCount, str, IndentCount, "");
                str += sw - CharCount;
                CharCount = 0;
            }
            if (p - str > sw - CharCount)
            {
                printf ("\n%*s", IndentCount, "");
                CharCount = 0;
            }
            printf ("%.*s", p - str, str);
            CharCount += p - str;
            str = p;
        }
        switch (*str)           /* Take care of specials */
        {
            case '\n':
                printf ("\n%*s", IndentCount, "");
                CharCount = 0;
                break;
            case '\r':
                putchar ('\r');
                if (str[1] != '\n' && IndentCount)
                {
                    printf ("\x1b[%dD", IndentCount);
                }
                CharCount = 0;
                break;
            case '\t':
                i = TAB_STOP - (CharCount % TAB_STOP);
                if (CharCount + i > sw)
                {
                    printf ("\n%*s", IndentCount, "");
                    CharCount = 0;
                }
                else
                {
                    printf ("%*s", i, "");
                    CharCount += i;
                }
                break;
            case '\a':
                if (prG->sound & SFLAG_CMD)
                    ExecScript (prG->sound_cmd, 0, 0, NULL);
                else if (prG->sound & SFLAG_BEEP)
                    printf ("\a");
                break;
            case '\x1b':
                switch (*++p)
                {
                    case '<':
                        switch (prG->flags & (FLAG_LIBR_BR | FLAG_LIBR_INT))
                        {
                            case FLAG_LIBR_BR:
                                printf ("\n");
                                CharCount = 0;
                                break;
                            case FLAG_LIBR_INT:
                                IndentCount = CharCount;
                                sw -= IndentCount;
                                CharCount = 0;
                                break;
                            case FLAG_LIBR_BR | FLAG_LIBR_INT:
                                s = strstr (str, "\x1b»");
                                if (s && s - str - 2 > sw - CharCount)
                                {
                                    printf ("\n");
                                    CharCount = 0;
                                    break;
                                }
                                break;
                        }
                        str++;
                        break;
                    case '«':
                        IndentCount = CharCount;
                        sw -= IndentCount;
                        CharCount = 0;
                        str++;
                        break;
                    case '»':
                        CharCount += IndentCount;
                        sw += IndentCount;
                        IndentCount = 0;
                        str++;
                        break;
                    case '>':
                        switch (prG->flags & (FLAG_LIBR_BR | FLAG_LIBR_INT))
                        {
                            case FLAG_LIBR_INT:
                                CharCount += IndentCount;
                                sw += IndentCount;
                                IndentCount = 0;
                                break;
                        }
                        str++;
                        break;
                    case COLCHR:
                        if (!(prG->flags & FLAG_COLOR))
                        {
                            str += 2;
                            break;
                        }
                        switch (*++p)
                        {
                            case '0':
                                printf ("%s", NOCOL);
                                break;
                            case '1':
                                printf ("%s", SERVCOL);
                                break;
                            case '2':
                                printf ("%s", CLIENTCOL);
                                break;
                            case '3':
                                printf ("%s", MESSCOL);
                                break;
                            case '4':
                                printf ("%s", CONTACTCOL);
                                break;
                            case '5':
                                printf ("%s", SENTCOL);
                                break;
                            case '6':
                                printf ("%s", ACKCOL);
                                break;
                            case '7':
                                printf ("%s", ERRCOL);
                                break;
                            default:
                                str--;
                        }
                        str += 2;
                        break;
                    default:
                        s = strchr (p, 'm');
                        if (s)
                        {
                            if (prG->flags & FLAG_COLOR)
                                printf ("%.*s", s - str + 1, str);
                            str = s;
                        }
                        break;
                }
                break;
            default:
                str--;
        }
    }
}

/**************************************************************
M_print with colors.
***************************************************************/
void M_print (const char *str, ...)
{
    va_list args;
    char buf[2048];

    R_print ();
    va_start (args, str);
#ifndef CURSES_UI
    vsnprintf (buf, sizeof (buf), str, args);
    M_prints (buf);
#else
#error No curses support included yet.
#error You must add it yourself.
#endif
    va_end (args);
}

#else

/************************************************************
Prints the preformatted string to stdout.
Plays sounds if appropriate.
************************************************************/
static void M_prints (char *str)
{
    int i, temp;
    static int chars_printed = 0;

    for (i = 0; str[i] != 0; i++)
    {
        if (str[i] != '\a')
        {
            if (str[i] == '\n')
            {
                printf ("\n");
                chars_printed = 0;
            }
            else if (str[i] == '\r')
            {
                printf ("\r");
                chars_printed = 0;
            }
            else if (str[i] == '\t')
            {
                temp = (chars_printed % TAB_STOP);
                /*chars_printed += TAB_STOP - temp; */
                temp = TAB_STOP - temp;
                for (; temp != 0; temp--)
                {
                    M_prints (" ");
                }
            }
            else if ((str[i] >= 32) || (str[i] < 0))
            {
                printf ("%c", str[i]);
                chars_printed++;
                if ((Get_Max_Screen_Width () != 0) && (chars_printed > Get_Max_Screen_Width ()))
                {
                    printf ("\n");
                    chars_printed = 0;
                }
            }
        }
        else if (prG->sound & SFLAG_CMD)
            ExecScript (prG->sound_cmd, 0, 0, NULL);
        else if (prG->sound & SFLAG_BEEP)
            printf ("\a");
    }
}

/**************************************************************
M_print with colors.
***************************************************************/
void M_print (char *str, ...)
{
    va_list args;
    char buf[2048];
    char *str1, *str2;

    va_start (args, str);
#ifndef CURSES_UI
    vsnprintf (buf, sizeof (buf), str, args);
    str2 = buf;
    while ((str1 = strchr (str2, '\x1b')) != NULL)
    {
        str1[0] = 0;
        M_prints (str2);
        str1[0] = 0x1B;
        str2 = strchr (str1, 'm');
        if (str2)
        {
            str2[0] = 0;
            if (prG->flags & FLAG_COLOR)
                printf ("%sm", str1);
            str2++;
        }
        else
            str2 = str1 + 1;
    }
    M_prints (str2);
#else
#error No curses support included yet.
#error You must add it yourself.
#endif
    va_end (args);
}
#endif

/*
 * Returns current horizontal position
 */
int M_pos ()
{
    return CharCount;
}

void Debug (UDWORD level, const char *str, ...)
{
    va_list args;
    char buf[2048];
    char buf2[3072];

    if (!(prG->verbose & level))
        return;

    va_start (args, str);
    vsnprintf (buf, sizeof (buf), str, args);
    va_end (args);

    M_print ("Debug: ");
    Time_Stamp ();
    snprintf (buf2, sizeof (buf2), " [%s] %s\n",
              level == 32 ? "Queue" : 
              level == 64 ? "Packet" : "unknown",
              buf);
    
    M_print ("%s", buf2);
}


/*****************************************************
Disables the printing of the next prompt.
useful for multipacket messages.
******************************************************/
void Kill_Prompt (void)
{
    No_Prompt = TRUE;
}

/*****************************************************
Displays the mICQ prompt.  Maybe someday this will be 
configurable
******************************************************/
void Prompt (void)
{
    static char buff[200];
    printf ("\r" ESC "[J");
    if (prG->flags & FLAG_UINPROMPT && uiG.last_sent_uin)
    {
        snprintf (buff, sizeof (buff), COLSERV "[%s]" COLNONE " ", ContactFindName (uiG.last_sent_uin));
        R_doprompt (buff);
    }
    else
    {
        snprintf (buff, sizeof (buff), COLSERV "%s" COLNONE, i18n (1040, "mICQ> "));
        R_doprompt (buff);
    }
    No_Prompt = FALSE;
#ifndef USE_MREADLINE
    fflush (stdout);
#endif
}

/*****************************************************
Displays the mICQ prompt.  Maybe someday this will be 
configurable
******************************************************/
void Soft_Prompt (void)
{
#if 1
    static char buff[200];
    printf ("\r" ESC "[J");
    snprintf (buff, sizeof (buff), COLSERV "%s" COLNONE, i18n (1040, "mICQ> "));
    R_doprompt (buff);
    No_Prompt = FALSE;
#else
    if (!No_Prompt)
    {
        M_print (COLSERV "%s" COLNONE, i18n (1040, "mICQ> "));
        fflush (stdout);
    }
    else
    {
        No_Prompt = FALSE;
    }
#endif
}

void Time_Stamp (void)
{
    struct tm *thetime;
    time_t p;

    p = time (NULL);
    thetime = localtime (&p);

    M_print ("%.02d:%.02d:%.02d", thetime->tm_hour, thetime->tm_min, thetime->tm_sec);
}

/*
 * Inform that a user went online
 */
void UtilUIUserOnline (Contact *cont, UDWORD status)
{
    if (status == cont->status)
        return;
    if (~cont->status)
    {
        if (prG->sound & SFLAG_ON_CMD)
            ExecScript (prG->sound_on_cmd, cont->uin, 0, NULL);
        else if (prG->sound & SFLAG_ON_BEEP)
            printf ("\a");
    }
    log_event (cont->uin, LOG_ONLINE, "User logged on %s\n", ContactFindName (cont->uin));
 
    Time_Stamp ();
    M_print (" " COLCONTACT "%10s" COLNONE " %s ",
             ContactFindName (cont->uin),
             ~cont->status ? i18n (1035, "changed status to") : i18n (1031, "logged on"));

    if (!~cont->status)
        M_print ("(");
    Print_Status (status);
    if (!~cont->status)
        M_print (")");
    if (cont->version && !~cont->status)
        M_print (" [%s]", cont->version);
    M_print (".\n");

    if (prG->verbose && !~cont->status)
    {
        M_print ("%-15s %s\n", i18n (1441, "IP:"), UtilIOIP (cont->outside_ip));
        M_print ("%-15s %s\n", i18n (1451, "IP2:"), UtilIOIP (cont->local_ip));
        M_print ("%-15s %d\n", i18n (1453, "TCP version:"), cont->TCP_version);
        M_print ("%-15s %s\n", i18n (1454, "Connection:"),
                 cont->connection_type == 4 ? i18n (1493, "Peer-to-Peer") : i18n (1494, "Server Only"));
    }

    cont->status = status;
    cont->last_time = time (NULL);
}

/*
 * Inform that a user went offline
 */
void UtilUIUserOffline (Contact *cont)
{
    if (prG->sound & SFLAG_OFF_CMD)
        ExecScript (prG->sound_off_cmd, cont->uin, 0, NULL);
    else if (prG->sound & SFLAG_OFF_BEEP)
        printf ("\a");
    log_event (cont->uin, LOG_ONLINE, "User logged off %s\n", ContactFindName (cont->uin));
 
    Time_Stamp ();
    M_print (" " COLCONTACT "%10s" COLNONE " %s\n",
             ContactFindName (cont->uin), i18n (1030, "logged off."));
    
    cont->status = STATUS_OFFLINE;
    cont->last_time = time (NULL);
}
