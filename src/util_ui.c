/* $Id$ */

#include "micq.h"
#include "mreadline.h"
#include "util_ui.h"
#include "util.h"
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
/* Max_Screen_Width set to zero in micq.c */


/***************************************************************
Turns keybord echo off for the password
****************************************************************/
SDWORD Echo_Off (void)
{
#ifdef HAVE_TCGETATTR
    struct termios attr;        /* used for getting and setting terminal
                                   attributes */

    /* Now turn off echo */
    if (tcgetattr (STDIN_FILENO, &attr) != 0)
        return (-1);
    /* Start by getting current attributes.  This call copies
       all of the terminal paramters into attr */

    attr.c_lflag &= ~(ECHO);
    /* Turn off echo flag.  NOTE: We are careful not to modify any
       bits except ECHO */
    if (tcsetattr (STDIN_FILENO, TCSAFLUSH, &attr) != 0)
        return (-2);
    /* Wait for all of the data to be printed. */
    /* Set all of the terminal parameters from the (slightly)
       modified struct termios */
    /* Discard any characters that have been typed but not yet read */
#endif
    return 0;
}


/***************************************************************
Turns keybord echo back on after the password
****************************************************************/
SDWORD Echo_On (void)
{
#ifdef HAVE_TCGETATTR
    struct termios attr;        /* used for getting and setting terminal
                                   attributes */

    if (tcgetattr (STDIN_FILENO, &attr) != 0)
        return (-1);

    attr.c_lflag |= ECHO;
    if (tcsetattr (STDIN_FILENO, TCSANOW, &attr) != 0)
        return (-1);
#endif
    return 0;
}

/**************************************************************
Same as M_print but for FD_T's
***************************************************************/
void M_fdprint (FD_T fd, const char *str, ...)
{
    va_list args;
    int k;
    char buf[2048];

    va_start (args, str);
    vsnprintf (buf, sizeof (buf), str, args);
    k = write (fd, buf, strlen (buf));
    if (k != strlen (buf))
    {
        perror (str);
        exit (10);
    }
    va_end (args);
}

/*
 * Open a file for reading.
 */
FD_T M_fdopen (const char *fmt, ...)
{
    va_list args;
    char buf[2048];

    va_start (args, fmt);
    vsnprintf (buf, sizeof (buf), fmt, args);
    va_end (args);

    return open (buf, O_RDONLY);
}

static volatile UDWORD scrwd = 0;
static RETSIGTYPE micq_sigwinch_handler (int a)
{
    struct winsize ws;

    scrwd = 0;
    ioctl (STDOUT, TIOCGWINSZ, &ws);
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
    if (uiG.Max_Screen_Width)
        return uiG.Max_Screen_Width;
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
                if (uiG.Sound == SOUND_CMD)
                    ExecScript (uiG.Sound_Str, 0, 0, NULL);
                else if (uiG.Sound == SOUND_ON)
                    printf ("\a");
                break;
            case '\x1b':
                switch (*++p)
                {
                    case '<':
                        switch (uiG.line_break_type)
                        {
                            case 0:
                                printf ("\n");
                                CharCount = 0;
                                break;
                            case 2:
                                IndentCount = CharCount;
                                sw -= IndentCount;
                                CharCount = 0;
                                break;
                            case 3:
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
                        switch (uiG.line_break_type)
                        {
                            case 2:
                                CharCount += IndentCount;
                                sw += IndentCount;
                                IndentCount = 0;
                                break;
                        }
                        str++;
                        break;
                    case COLCHR:
                        if (!uiG.Color)
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
                            if (uiG.Color)
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
        else if (uiG.Sound == SOUND_CMD)
            ExecScript (uiG.Sound_Str, 0, 0, NULL);
        else if (uiG.Sound == SOUND_ON)
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
            if (uiG.Color)
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

/***********************************************************
Reads a line of input from the file descriptor fd into buf
an entire line is read but no more than len bytes are 
actually stored
************************************************************/
int M_fdnreadln (FD_T fd, char *buf, size_t len)
{
    int i, j;
    char tmp;
    static char buff[20] = "\0";


    assert (buf != NULL);
    assert (len > 0);
    
    tmp = 0;
    len--;
    for (i = -1; (tmp != '\n');)
    {
        if ((i < len) || (i == -1))
        {
            i++;
            j = read (fd, &buf[i], 1);
            tmp = buf[i];
        }
        else
        {
            j = read (fd, &tmp, 1);
        }
        assert (j != -1);
        if (j == 0)
        {
            buf[i] = 0;
            return -1;
        }
    }
    if (i < 1)
    {
        buf[i] = 0;
    }
    else
    {
        if (buf[i - 1] == '\r')
        {
            buf[i - 1] = 0;
        }
        else
        {
            buf[i] = 0;
        }
    }
    return 0;
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
Displays the Micq prompt.  Maybe someday this will be 
configurable
******************************************************/
extern UDWORD last_uin;
void Prompt (void)
{
    static char buff[200];
    if (uiG.last_uin_prompt && last_uin)
    {
        snprintf (buff, sizeof (buff), COLSERV "[%s]" COLNONE " ", UIN2Name (last_uin));
        R_doprompt (buff);
    }
    else
    {
        snprintf (buff, sizeof (buff), COLSERV "%s" COLNONE, i18n (40, "Micq> "));
        R_doprompt (buff);
    }
    No_Prompt = FALSE;
#ifndef USE_MREADLINE
    fflush (stdout);
#endif
}

/*****************************************************
Displays the Micq prompt.  Maybe someday this will be 
configurable
******************************************************/
void Soft_Prompt (void)
{
#if 1
    static char buff[200];
    snprintf (buff, sizeof (buff), COLSERV "%s" COLNONE, i18n (40, "Micq> "));
    R_doprompt (buff);
    No_Prompt = FALSE;
#else
    if (!No_Prompt)
    {
        M_print (COLSERV "%s" COLNONE, i18n (40, "Micq> "));
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
