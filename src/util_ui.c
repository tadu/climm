/* $Id$ */
/* Copyright: This file may be distributed under version 2 of the GPL licence. */

#include "micq.h"
#include "mreadline.h"
#include "util_ui.h"
#include "util_io.h"
#include "util.h"
#include "conv.h"
#include "contact.h"
#include "session.h"
#include "preferences.h"
#include "buildmark.h"
#include "util_str.h"
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

#ifdef ENABLE_UTF8
#define chardiff(aa,bb)  (ENC(enc_loc) == ENC_UTF8 ? s_strnlen ((bb), (aa) - (bb)) : (aa) - (bb))
#define charoff(str,off) (ENC(enc_loc) == ENC_UTF8 ? s_offset ((str), (off))      : (off))
#else
#define chardiff(aa,bb)  ((aa) - (bb))
#define charoff(str,off) (off)
#endif

/*
 * Print a string to the output, interpreting color and indenting codes.
 */
void M_print (const char *org)
{
    const char *test, *save, *temp, *str;
    int i;
    int sw = Get_Max_Screen_Width () - IndentCount;
    
#ifdef ENABLE_UTF8
    char *fstr = strdup (ConvFromUTF8 (org, prG->enc_loc));
    str = fstr;
#else
    str = org;
#endif

    R_remprompt ();
    for (; *str; str++)
    {
        for (test = save = str; *test; test++)
        {
            if (strchr ("\n\r\t\a\x1b", *test)) /* special character reached - emit text till last saved position */
            {
                if (save != str)
                    test = save;
                break;
            }
            if (strchr ("-.,_:;!?/ ", *test)) /* punctuation found - save position after it */
            {
                temp = test + 1;
                if (chardiff (temp, str) <= sw - CharCount)
                    save = temp;
                else
                {
                    if (save != str)
                        test = save;
                    else
                        test = temp;
                    break;
                }
            }
        }
        if (test != str)           /* Print out (block of) word(s) from str to test*/
        {
            while (chardiff (test, str) > sw) /* word is longer than line, print till end of line */
            {
                printf ("%.*s%*s", (int) charoff (str, sw - CharCount), str, IndentCount, "");
                str += charoff (str, sw - CharCount);
                CharCount = 0;
            }
            if (chardiff (test, str) > sw - CharCount) /* remainder doesn't fit anymore => linebreak */
            {
                printf ("\n%*s", IndentCount, "");
                CharCount = 0;
            }
            printf ("%.*s", test - str, str);
            CharCount += chardiff (test, str);
            str = test;
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
                switch (*++test)
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
                                save = strstr (str, "\x1b»");
                                if (save && chardiff (save, str) - 2 > sw - CharCount)
                                {
                                    printf ("\n");
                                    CharCount = 0;
                                    break;
                                }
                                break;
                        }
                        str++;
                        break;
                    case 'v':
                        IndentCount = CharCount;
                        sw -= IndentCount;
                        CharCount = 0;
                        str++;
                        break;
                    case '^':
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
                        test++;
                        if (*test >= '0' && *test <= '0' + CXCOUNT)
                        {
                            if (prG->colors[*test - '0'])
                                printf ("%s", prG->colors[*test - '0']);
                            else
                                /* FIXME */;
                            str++;
                        }
                        str++;
                        break;
                    default:
                        save = strchr (test, 'm');
                        if (save)
                        {
                            if (prG->flags & FLAG_COLOR)
                                printf ("%.*s", (int)(save - str + 1), str);
                            str = save;
                        }
                        break;
                }
                break;
            default:
                str--;
        }
    }
#ifdef ENABLE_UTF8
    free (fstr);
#endif
}

/*
 * Print a formatted string to the output, interpreting color and indenting
 * codes.
 * Note: does not use the same static buffer as s_sprintf().
 */
void M_printf (const char *str, ...)
{
    va_list args;
    char buf[4048];

    va_start (args, str);
    vsnprintf (buf, sizeof (buf), str, args);
    M_print (buf);
    va_end (args);
}

#else

/*
 * Print a string to the output, interpreting color and indenting codes.
 */
void M_print (char *str)
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
M_printf with colors.
***************************************************************/
void M_printf (char *str, ...)
{
    va_list args;
    char buf[2048];
    char *str1, *str2;

    va_start (args, str);
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
    if (level & DEB_CONNECT)     return "Connect";
    return "unknown";
}

/*
 * Output a given string iff the debug level is appropriate.
 */
BOOL Debug (UDWORD level, const char *str, ...)
{
    va_list args;
    char buf[2048], c;

    if (!(prG->verbose & level) && level)
        return 0;

    va_start (args, str);
    vsnprintf (buf, sizeof (buf), str, args);
    va_end (args);

    M_print ("");
    if ((c = M_pos ()))
        M_print ("\n");
    M_printf ("%s %s%7.7s%s %s", s_now, COLDEBUG, DebugStr (level & prG->verbose), COLNONE, buf);

    if (!c)
        M_print ("\n");

    return 1;
}
