/* $Id$ */
/* Copyright: This file may be distributed under version 2 of the GPL licence. */

#include "micq.h"
#include "mreadline.h"
#include "util_ui.h"
#include "util_io.h"
#include "util.h"
#include "contact.h"
#include "session.h"
#include "preferences.h"
#include "buildmark.h"
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
#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#ifdef HAVE_TCGETATTR
struct termios sattr;
#endif

static const char *DebugStr (UDWORD level);

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
    ioctl (STDIN_FILENO, TIOCGWINSZ, &ws);
    scrwd = ws.ws_col;
}

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
            printf ("%.*s", (int)(p - str), str);
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
                        p++;
                        if (*p >= '0' && *p <= '0' + CXCOUNT)
                        {
                            if (prG->colors[*p - '0'])
                                printf ("%s", prG->colors[*p - '0']);
                            else
                                /* FIXME */;
                            str++;
                        }
                        str++;
                        break;
                    default:
                        s = strchr (p, 'm');
                        if (s)
                        {
                            if (prG->flags & FLAG_COLOR)
                                printf ("%.*s", (int)(s - str + 1), str);
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

    R_remprompt ();
    va_start (args, str);
#ifndef CURSES_UI
    vsnprintf (buf, sizeof (buf), str, args);
    M_prints (buf);
#else
#error No curses support included yet. You must add it yourself.
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
        str1[0] = '\x1b';
        if (strchr ("©<>«»", str1[1]))
        {
            char c = str1[2];
            str1[2] = 0;
            M_prints (str1);
            str1[2] = c;
            str2 = str1 + 2;
        }
        else
        {
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
    }
    M_prints (str2);
#else
#error No curses support included yet. You must add it yourself.
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

/*
 * Returns string describing debug level
 */
static const char *DebugStr (UDWORD level)
{
    if (level & DEB_PACKET)      return "Packet ";
    if (level & DEB_QUEUE)       return "Queue  ";
    if (level & DEB_PACK5DATA)   return "v5 data";
    if (level & DEB_PACK8)       return "v8 pack";
    if (level & DEB_PACK8DATA)   return "v8 data";
    if (level & DEB_PACK8SAVE)   return "v8 save";
    if (level & DEB_PACKTCP)     return "TCPpack";
    if (level & DEB_PACKTCPDATA) return "TCPdata";
    if (level & DEB_PACKTCPSAVE) return "TCPsave";
    if (level & DEB_PROTOCOL)    return "Protocl";
    if (level & DEB_TCP)         return "TCP HS ";
    if (level & DEB_IO)          return "  I/O  ";
    if (level & DEB_SESSION)     return "Session";
    return "unknown";
}

/*
 * Output a given string iff the debug level is appropriate.
 */
BOOL Debug (UDWORD level, const char *str, ...)
{
    va_list args;
    char buf[2048], buf2[3072], c;

    if (!(prG->verbose & level) && level)
        return 0;

    va_start (args, str);
    vsnprintf (buf, sizeof (buf), str, args);
    va_end (args);

    M_print ("");
    if ((c = M_pos ()))
        M_print ("\n");
/*    M_print ("Debug: ");   */
    Time_Stamp ();
    snprintf (buf2, sizeof (buf2), " %s%7.7s%s %s", YELLOW, DebugStr (level & prG->verbose), COLNONE, buf);

    M_print ("%s", buf2);
    if (!c)
        M_print ("\n");

    return 1;
}

/*
 * Output the current time. Add µs for high enough debug level.
 */
void Time_Stamp (void)
{
    struct timeval tv;
    struct tm *thetime;

#ifdef HAVE_GETTIMEOFDAY
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    gettimeofday (&tv, NULL);
#else
    tv.tv_usec = 0;
    tv.tv_sec = time (NULL);
#endif
    thetime = localtime (&tv.tv_sec);

    M_print ("%.02d:%.02d:%.02d", thetime->tm_hour, thetime->tm_min, thetime->tm_sec);
    
    if (prG->verbose > 7)
        M_print (".%.06d", tv.tv_usec);
    else if (prG->verbose > 1)
        M_print (".%.03d", tv.tv_usec / 1000);
}

/*
 * Returns static string describing given time.
 */
char *UtilUITime (time_t *t)
{
    static char buf[20];
    struct tm *thetime;
    
    thetime = localtime (t);
    snprintf (buf, sizeof (buf), "%.02d:%.02d:%.02d", thetime->tm_hour, thetime->tm_min, thetime->tm_sec);
    
/*  if (prG->verbose > 7)
        snprintf (buf + strlen (buf), sizeof (buf) - strlen (buf), ".%.06d", tv.tv_usec);
    else if (prG->verbose > 1)
        snprintf (buf + strlen (buf), sizeof (buf) - strlen (buf), ".%.03d", tv.tv_usec / 1000); */
    
    return buf;
}

/*
 * Try to find a parameter in the string.
 * String pointer is advanced to point after the parsed argument.
 * Result must NOT be free()d.
 */
BOOL UtilUIParse (char **input, char **parsed)
{
    static char *t = NULL;
    char *p = *input;
    int s = 0;
    
    while (*p && strchr (" \t\r\n", *p))
        p++;
    
    *input = p;
    if (!*p)
    {
        *parsed = NULL;
        return FALSE;
    }
    if (t)
        free (t);
    *parsed = t = malloc (strlen (p));
    
    if (*p == '"')
    {
        s = 1;
        p++;
    }
    while (*p)
    {
        if (*p == '\\' && *(p + 1))
        {
            p++;
            *(t++) = *(p++);
            continue;
        }
        if (*p == '"' && s)
        {
            *t = '\0';
            *input = p;
            return TRUE;
        }
        *(t++) = *(p++);
    }
    *t = '\0';
    *input = p;
    return TRUE;
}

/*
 * Try to find a nick name or uin in the string.
 * String pointer is advanced to point after the parsed argument.
 */
BOOL UtilUIParseNick (char **input, Contact **parsed)
{
    Contact *r;
    char *p = *input, *t;
    int max, l, ll;
    
    while (*p && strchr (" \t\r\n", *p))
        p++;
    
    *input = p;
    if (!*p)
    {
        *parsed = NULL;
        return FALSE;
    }
    
    if (*p == '"')
    {
        t = NULL;
        if (UtilUIParse (&p, &t))
        {
            *parsed = ContactFindContact (t);
            *input = p;
            return TRUE;
        }
    }
    max = 0;
    ll = strlen (p);
    for (r = ContactStart (); ContactHasNext (r); r = ContactNext (r))
    {
        l = strlen (r->nick);
        if (l > max && l <= ll && (!p[l] || strchr (" \t\r\n", p[l])) && !strncmp (p, r->nick, l))
        {
            *parsed = r;
            max = strlen (r->nick);
        }
    }
    if (max)
    {
        *input = p + max;
        return TRUE;
    }
    return FALSE;
}

/*
 * Try to find a number.
 * String pointer is advanced to point after the parsed argument.
 */
BOOL UtilUIParseInt (char **input, UDWORD *parsed)
{
    char *p = *input;
    UDWORD nr;
    
    while (*p && strchr (" \t\r\n", *p))
        p++;
    
    nr = 0;
    while (*p && *p >= '0' && *p <= '9')
    {
        nr = nr * 10 + (*p - '0');
        p++;
    }
    if (*p && !strchr (" \t\r\n", *p))
    {
        *parsed = 0;
        return FALSE;
    }
    *input = p;
    *parsed = nr;
    return TRUE;
}

/*
 * Inform that a user went online
 */
void UtilUIUserOnline (Session *sess, Contact *cont, UDWORD status)
{
    UDWORD old;

    cont->seen_time = time (NULL);
    UtilUISetVersion (cont);

    if (status == cont->status)
        return;
    
    old = cont->status;
    cont->status = status;
    cont->flags &= ~CONT_SEENAUTO;

    log_event (cont->uin, LOG_ONLINE, "User logged on %s (%08lx)\n", ContactFindName (cont->uin), status);
 
    if ((cont->flags & (CONT_TEMPORARY | CONT_IGNORE)) || (prG->flags & FLAG_QUIET) || !(sess->connect & CONNECT_OK))
        return;

    if (~old)
    {
        if (prG->sound & SFLAG_ON_CMD)
            ExecScript (prG->sound_on_cmd, cont->uin, 0, NULL);
        else if (prG->sound & SFLAG_ON_BEEP)
            printf ("\a");
    }
    Time_Stamp ();
    M_print (" " COLCONTACT "%10s" COLNONE " %s ",
             ContactFindName (cont->uin),
             ~old ? i18n (1035, "changed status to") : i18n (1031, "logged on"));

    if (!~old)
        M_print ("(");
    Print_Status (status);
    if (!~old)
        M_print (")");
    if (cont->version && !~old)
        M_print (" [%s]", cont->version);
    if ((status & STATUSF_BIRTH) && (!(old & STATUSF_BIRTH) || !~old))
        M_print (" (%s)", i18n (2033, "born today"));
    M_print (".\n");

    if (prG->verbose && !~old)
    {
        M_print ("%-15s %s\n", i18n (1441, "IP:"), UtilIOIP (cont->outside_ip));
        M_print ("%-15s %s\n", i18n (1451, "IP2:"), UtilIOIP (cont->local_ip));
        M_print ("%-15s %d\n", i18n (1453, "TCP version:"), cont->TCP_version);
        M_print ("%-15s %s\n", i18n (1454, "Connection:"),
                 cont->connection_type == 4 ? i18n (1493, "Peer-to-Peer") : i18n (1494, "Server Only"));
    }
}

/*
 * Inform that a user went offline
 */
void UtilUIUserOffline (Session *sess, Contact *cont)
{
    log_event (cont->uin, LOG_ONLINE, "User logged off %s\n", ContactFindName (cont->uin));

    cont->status = STATUS_OFFLINE;
    cont->seen_time = time (NULL);

    if ((cont->flags & (CONT_TEMPORARY | CONT_IGNORE)) || (prG->flags & FLAG_QUIET))
        return;

    if (prG->sound & SFLAG_OFF_CMD)
        ExecScript (prG->sound_off_cmd, cont->uin, 0, NULL);
    else if (prG->sound & SFLAG_OFF_BEEP)
        printf ("\a");
 
    Time_Stamp ();
    M_print (" " COLCONTACT "%10s" COLNONE " %s\n",
             ContactFindName (cont->uin), i18n (1030, "logged off."));
}

/*
 * Guess the contacts client from time stamps.
 */
void UtilUISetVersion (Contact *cont)
{
    char buf[100];
    char *new = NULL;
    unsigned int ver = cont->id1 & 0xffff, ssl = 0;
    signed char v1 = 0, v2 = 0, v3 = 0, v4 = 0;

    if ((cont->id1 & 0xff7f0000) == BUILD_LICQ && ver > 1000)
    {
        new = "licq";
        if (cont->id1 & BUILD_SSL)
            ssl = 1;
        v1 = ver / 1000;
        v2 = (ver / 10) % 100;
        v3 = ver % 10;
        v4 = 0;
    }
#ifdef WIP
    else if ((cont->id1 & 0xff7f0000) == BUILD_MICQ || (cont->id1 & 0xff7f0000) == BUILD_LICQ)
    {
        new = "mICQ";
        v1 = ver / 10000;
        v2 = (ver / 100) % 100;
        v3 = (ver / 10) % 10;
        v4 = ver % 10;
        if (ver >= 489 && cont->id2)
            cont->id1 = BUILD_MICQ;
    }
#endif
    else if (cont->id1 == cont->id2 && cont->id2 == cont->id3 && cont->id1 == 0xffffffff)
        new = "vICQ/GAIM(?)";

    if ((cont->id1 & 0xffff0000) == 0xffff0000)
    {
        v1 = (cont->id2 & 0x7f000000) >> 24;
        v2 = (cont->id2 &   0xff0000) >> 16;
        v3 = (cont->id2 &     0xff00) >> 8;
        v4 =  cont->id2 &       0xff;
        switch (cont->id1)
        {
            case BUILD_MIRANDA:
                if (cont->id2 <= 0x00010202 && cont->TCP_version >= 8)
                    cont->TCP_version = 7;
                new = "Miranda";
                break;
            case BUILD_STRICQ:
                new = "StrICQ";
                break;
            case BUILD_MICQ:
                cont->seen_micq_time = time (NULL);
                new = "mICQ";
                break;
            case BUILD_YSM:
                new = "YSM";
                if (v1 < 0 || v2 < 0 || v3 < 0 || v4 < 0)
                    v1 = v2 = v3 = v4 = 0;
                break;
            default:
                snprintf (buf, sizeof (buf), "%08lx", cont->id1);
                new = buf;
        }
    }
    
    if (new)
    {
        if (new != buf)
            strcpy (buf, new);
        if (v1 || v2 || v3 || v4)
        {
            strcat (buf, " ");
                          sprintf (buf + strlen (buf), "%d.%d", v1, v2);
            if (v3 || v4) sprintf (buf + strlen (buf), ".%d", v3);
            if (v4)       sprintf (buf + strlen (buf), ".%d", v4);
        }
        if (ssl) strcat (buf, "/SSL");
    }
    else
        buf[0] = '\0';

    if (prG->verbose)
        sprintf (buf + strlen (buf), " <%08x:%08x:%08x>", (unsigned int)cont->id1,
                 (unsigned int)cont->id2, (unsigned int)cont->id3);

    if (cont->version) free (cont->version);
    cont->version = strlen (buf) ? strdup (buf) : NULL;
}

