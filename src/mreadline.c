/* $Id$ */

/*****************************************************
 * Copyright (C) 1998 Sergey Shkonda (serg@bcs.zp.ua)
 * This file may be distributed under version 2 of the GPL licence.
 * Originally placed in the public domain by Sergey Shkonda Nov 27, 1998
 *****************************************************/

#include "micq.h"

#ifdef USE_MREADLINE

#include "mreadline.h"
#include "util_ui.h"
#include "util_rl.h"
#include "util.h"
#include "cmd_user.h"
#include "tabs.h"
#include "conv.h"
#include "contact.h"
#include "session.h"
#include "preferences.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <assert.h>

static int CharCount = 0;       /* number of characters printed on line. */
static int IndentCount = 0;

#define LOGOS 10
static const char *logos[LOGOS] = { NULL };
static UBYTE logoc = 0;
static UBYTE first = 0;

void M_logo (const char *logo)
{
    if (logoc != LOGOS)
        logos[logoc++] = logo;
    first = 2;
}

static const char *M_getlogo (void)
{
    UBYTE i;
    const char *logo;

    if (!logoc)
        return first ? "         " : "";
    logo = logos[0];
    logoc--;
    for (i = 0; i < logoc; i++)
        logos[i] = logos[i + 1];
    return ConvTo (logo, prG->enc_loc)->txt;
}

void M_logo_clear ()
{
    first = 1;
    puts ("");
    while (logoc)
        puts (M_getlogo ());
    M_print ("\r");
    first = 0;
}

#define chardiff(aa,bb)  (ENC(enc_loc) == ENC_UTF8 ? s_strnlen ((bb), (aa) - (bb)) : (aa) - (bb))

#define USECOLORINVCHAR if (!colinvchar) colinvchar = ((prG->flags & FLAG_COLOR) ? ContactPrefStr (NULL, CO_COLORINVCHAR) : "")

/*
 * Print a string to the output, interpreting color and indenting codes.
 */
void M_print (const char *org)
{
    const char *test, *save, *temp, *str, *para;
    char *fstr;
    const char *colnone = NULL, *colinvchar = NULL, *col;
    UBYTE isline = 0, ismsg = 0;
    int i;
    static str_s colbuf = { NULL, 0, 0 };
    int sw = rl_columns - IndentCount;
    
    fstr = strdup (ConvTo (org, prG->enc_loc)->txt);
    str = fstr;
    switch (ENC(enc_loc))
    {
        case ENC_UTF8:   para = "\xc2\xb6"; break;
        case ENC_LATIN1:
        case ENC_LATIN9: para = "\xb6";  break;
        default:         para = "P";  break;
    }
    col = colnone = (prG->flags & FLAG_COLOR) ? ContactPrefStr (NULL, CO_COLORNONE) : "";

    if (first)
    {
        sw -= 9;
        if (first == 2)
        {
            if (!CharCount)
                printf ("%s", M_getlogo ());
            first = 1;
        }
    }

    ReadLinePromptHide ();

#ifdef ENABLE_TCL
    if (prG->tclout)
    {
        prG->tclout (fstr);
        free (fstr);
        return;
    }
#endif

    for (; *str; str++)
    {
        for (test = save = str; *test; test++)
        {
            if (!(*test & 0xe0) || (*test == 127)) /* special character reached - emit text till last saved position */
            {
                if (save != str)
                    test = save;
                break;
            }
            else if (strchr ("-.,_:;!?/ ", *test)) /* punctuation found - save position after it */
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
                printf ("%.*s%*s", (int) c_offset (str, sw - CharCount), str, IndentCount, "");
                str += c_offset (str, sw - CharCount);
                if (isline)
                {
                    USECOLORINVCHAR;
                    printf ("%s...%s", colinvchar, col);
                    CharCount = 0;
                    free (fstr);
                    return;
                }
                CharCount = 0;
            }
            if (chardiff (test, str) > sw - CharCount) /* remainder doesn't fit anymore => linebreak */
            {
                if (isline)
                {
                    USECOLORINVCHAR;
                    printf ("%s...%s", colinvchar, col);
                    CharCount = 0;
                    free (fstr);
                    return;
                }
                printf ("\n%s%*s%s", M_getlogo (), IndentCount, "", col);
                CharCount = 0;
            }
            printf ("%.*s", (int)(test - str), str);
            CharCount += chardiff (test, str);
            str = test;
        }
        if (*str != 0x7f && (*str <= 0 || *str >= ' '))
        {
            str--;
            continue;
        }
        if (isline && (*str == '\n' || *str == '\r'))
        {
            if (str[1])
            {
                USECOLORINVCHAR;
                printf ("%s%s..", colinvchar, para);
            }
            printf ("%s", col);
            CharCount = 0;
            free (fstr);
            return;
        }
        if (*str == '\n' || *str == '\r' || (*str == '\t' && !isline))
        {
            if (!str[1] && ismsg)
            {
                printf ("%s\n", colnone);
                CharCount = 0;
                IndentCount = 0;
                free (fstr);
                return;
            }
        }
        else if (ismsg || isline)
        {
            USECOLORINVCHAR;
            printf ("%s%c%s", colinvchar, *str - 1 + 'A', col);
            CharCount++;
            continue;
        }
        switch (*str)           /* Take care of specials */
        {
            case '\b':
                CharCount--;
                printf ("\b");
                break;
            case '\n':
                printf ("\n%s%*s%s", M_getlogo (), IndentCount, "", col);
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
                    printf ("\n%s%*s", M_getlogo (), IndentCount, "");
                    CharCount = 0;
                }
                else
                {
                    printf ("%*s", i, "");
                    CharCount += i;
                }
                break;
            case '\a':
                if (prG->sound == SFLAG_EVENT && prG->event_cmd && *prG->event_cmd)
                    EventExec (NULL, prG->event_cmd, 4, 0, NULL);
                else if (prG->sound == SFLAG_BEEP)
                    printf ("\a");
                break;
            case '\x1b':
                if (isline)
                    break;
                switch (*++test)
                {
                    case '<':
                        ismsg = 1;
                        switch (prG->flags & (FLAG_LIBR_BR | FLAG_LIBR_INT))
                        {
                            case FLAG_LIBR_BR:
                                printf ("\n%s", M_getlogo ());
                                CharCount = 0;
                                break;
                            case FLAG_LIBR_INT:
                                IndentCount = CharCount;
                                sw -= IndentCount;
                                CharCount = 0;
                                break;
                            case FLAG_LIBR_BR | FLAG_LIBR_INT:
                                save = strstr (str, "\x1bv");
                                if (save && chardiff (save, str) - 2 > sw - CharCount)
                                {
                                    printf ("\n%s", M_getlogo ());
                                    CharCount = 0;
                                    break;
                                }
                                break;
                        }
                        str++;
                        break;
                    case 'v':
                        IndentCount += CharCount;
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
                    case '.':
                        isline = 1;
                        sw -= 3;
                        if (sw <= CharCount)
                        {
                            printf ("%s", col);
                            CharCount = IndentCount = 0;
                            free (fstr);
                            return;
                        }
                        str++;
                        break;
                    default:
                        save = strchr (test, 'm');
                        if (save)
                        {
                            while (!strncmp (save, "m" ESC "[", 3) && strchr (save + 1, 'm'))
                                save = strchr (save + 1, 'm');
                            if (prG->flags & FLAG_COLOR)
                            {
                                s_init (&colbuf, "", (int)(save - str) + 1);
                                s_catf (&colbuf, "%.*s", (int)(save - str) + 1, str);
                                printf ("%s", col = colbuf.txt);
                            }
                            else
                                col = "";
                            str = save;
                        }
                        break;
                }
                break;
            default:
                USECOLORINVCHAR;
                printf ("%s%c%s", colinvchar, *str - 1 + 'A', col);
                
        }
    }
    free (fstr);
}

/*
 * Print a formatted string to the output, interpreting color and indenting
 * codes.
 * Note: does not use the same static buffer as s_sprintf().
 */
void M_printf (const char *str, ...)
{
    va_list args;
    char buf[8 * 1024];

    va_start (args, str);
    vsnprintf (buf, sizeof (buf), str, args);
    M_print (buf);
    va_end (args);
}

/*
 * Returns current horizontal position
 */
int M_pos ()
{
    return CharCount;
}


#endif /* USE_MREADLINE */
