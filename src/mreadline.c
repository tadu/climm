/* $Id$ */

/*****************************************************
 * mreadline - small line editing and history code
 * Copyright (C) 1998 Sergey Shkonda (serg@bcs.zp.ua)
 *
 * This software is provided AS IS to be used in
 * whatever way you see fit and is placed in the
 * public domain.
 *
 * Author : Sergey Shkonda Nov 27, 1998
 * Changes:
 * * Lalo Martins (lalo@webcom.com) Feb 26, 1999
 *   added tab completion (added get_tab() and changed
 *   R_process_input()
 * * Lalo Martins (lalo@webcom.com) Feb 26, 1999
 *   added more editing commands: delete (as VEOF),
 *   ^A, ^E, ^K, ^U and ^Y, all in R_process_input()
 *****************************************************/

#include "micq.h"

#ifdef USE_MREADLINE

#include "mreadline.h"
#include "util_ui.h"
#include "util.h"
#include "cmd_user.h"
#include "tabs.h"
#include "contact.h"
#include "preferences.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <assert.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_TERMIOS_H
#include <termios.h>
#endif
#ifndef _POSIX_VDISABLE
#define _POSIX_VDISABLE 0
#endif

#define HISTORY_LINES 10
#define HISTORY_LINE_LEN 1024

static struct termios t_attr;
static void tty_prepare (void);
static void tty_restore (void);

static void R_process_input_backspace (void);
static void R_process_input_delete (void);

static RETSIGTYPE micq_ttystop_handler (int);
static RETSIGTYPE micq_cont_handler (int);

static char *history[HISTORY_LINES + 1];
static int history_cur = 0;
static char s[HISTORY_LINE_LEN];
static char y[HISTORY_LINE_LEN];
static int cpos = 0;
static int clen = 0;
static int istat = 0;

#ifndef ANSI_COLOR
static char bsbuf[HISTORY_LINE_LEN];
#endif

void R_init (void)
{
    int k;
    static int inited = 0;
    
#ifndef ANSI_COLOR
    for (k = 0; k < HISTORY_LINE_LEN; k++)
        bsbuf[k] = '\b';
#endif

    if (inited)
        return;
    for (k = 0; k < HISTORY_LINES + 1; k++)
    {
        history[k] = (char *) malloc (HISTORY_LINE_LEN);
        history[k][0] = 0;
    }
    s[0] = 0;
    inited = 1;
    signal (SIGTSTP, &micq_ttystop_handler);
    signal (SIGCONT, &micq_cont_handler);
    tty_prepare ();
    atexit (tty_restore);
}

static RETSIGTYPE micq_ttystop_handler (int a)
{
    tty_restore ();
    signal (SIGTSTP, SIG_DFL);
    raise (SIGTSTP);
}

static RETSIGTYPE micq_cont_handler (int a)
{
    tty_prepare ();
    R_redraw ();
    signal (SIGTSTP, &micq_ttystop_handler);
    signal (SIGCONT, &micq_cont_handler);
}

void R_pause (void)
{
    tty_restore ();
}

void R_resume (void)
{
    tty_prepare ();
}

/*
 * Moves cursor to different position in input line.
 */
void R_goto (int pos)
{
#ifdef ANSI_COLOR
    int scr, off;
#endif

    assert (pos >= 0);
    
    if (pos == cpos)
        return;
        
#ifdef ANSI_COLOR
    scr = Get_Max_Screen_Width ();
    off = M_pos ();
    while ((off + pos) / scr < (off + cpos) / scr)
    {
        printf (ESC "[A");
        cpos -= scr;
    }
    if (cpos < 0)
    {
        printf (ESC "[%dC", -cpos);
        cpos = 0;
    }
    if (pos < cpos)
        printf (ESC "[%dD", cpos - pos);
#else
    if (pos < cpos)
    {
        printf ("%.*s", cpos - pos, bsbuf);
        cpos = pos;
    }
#endif

    if (cpos < pos)
        printf ("%.*s", pos - cpos, s + cpos);
    cpos = pos;
}

void R_rlap (const char *s, const char *add, BOOL clear)
{
#ifdef ANSI_COLOR
    char buf[20];
    int pos;

    snprintf (buf, sizeof (buf), ESC "[%dD", strlen (s));
    printf ("%s%s%s%s", add, s, clear ? ESC "[J" : "",
            (M_pos () + cpos) % Get_Max_Screen_Width() == 0 ? " \b" : "");
    pos = cpos;
    cpos += strlen (s);
    R_goto (pos);
#else
    printf ("%s%s%s%.*s", add, s, clear ? " \b" : "", strlen (s), bsbuf);
#endif
}


void R_process_input_backspace (void)
{
    if (!cpos)
        return;

    clen--;
    R_goto (cpos - 1);
    memmove (s + cpos, s + cpos + 1, clen - cpos + 1);
    R_rlap (s + cpos, "", TRUE);
}

void R_process_input_delete (void)
{
    if (cpos >= clen)
        return;

    clen--;
    memmove (s + cpos, s + cpos + 1, clen - cpos + 1);
    R_rlap (s + cpos, "", TRUE);
}

void R_process_input_tab (void)
{
    UDWORD uin;
    const char *msgcmd = CmdUserLookupName ("msg");

    if (strncmp (s, msgcmd, strlen (s) < strlen (msgcmd) ? strlen (s) : strlen (msgcmd)))
    {
        M_print ("\a");
        return;
    }
    if (strpbrk (s, UIN_DELIMS) && strpbrk (s, UIN_DELIMS) - s < strlen (s) - 1
        && !strchr (UIN_DELIMS, s[strlen (s) - 1]))
    {
        M_print ("\a");
        return;
    }

    if ((uin = TabGetNext ()))
        sprintf (s, "%s %s/", msgcmd, ContactFindName (uin));
    else
        sprintf (s, "%s ", msgcmd);

    R_print ();
    clen = cpos = strlen (s);
}

int R_process_input (void)
{
    char ch;
    int k;

    if (!read (STDIN_FILENO, &ch, 1))
        return 0;
    if (!istat)
    {
        if ((ch >= 0 && ch < ' ') || ch == 127)
        {
            switch (ch)
            {
                case 1:        /* ^A */
                    R_goto (0);
                    break;
                case 5:        /* ^E */
                    R_goto (clen);
                    break;
                case 8:        /* ^H = \b */
                    R_process_input_backspace ();
                    return 0;
                case 11:       /* ^K, as requested by Bernhard Sadlowski */
                    clen = cpos;
                    s[cpos] = '\0';
                    printf (ESC "[J");
                    break;
                case '\n':
                case '\r':
                    s[clen] = 0;
                    R_goto (clen);
                    cpos = 0;
                    clen = 0;
                    printf ("\n");
                    history_cur = 0;
                    TabReset ();
                    strcpy (history[0], s);
                    if (!s[0])
                        return 1;
                    if (strcmp (s, history[1]))
                        for (k = HISTORY_LINES; k; k--)
                            strcpy (history[k], history[k - 1]);
                    return 1;
                case 12:       /* ^L */
                    R_print ();
                    system ("clear");
                    break;
                case '\t':
                    R_process_input_tab ();
                    break;
                case 25:       /* ^Y */
                    R_print ();
                    strcpy (s, y);
                    clen = cpos = strlen (s);
                    break;
#ifdef ANSI_COLOR
                case 27:       /* ESC */
                    istat = 1;
                    break;
#endif
                default:
                    if (ch == t_attr.c_cc[VERASE] && t_attr.c_cc[VERASE] != _POSIX_VDISABLE)
                    {
                        if (prG->flags & FLAG_DELBS)
                            R_process_input_backspace ();
                        else
                            R_process_input_delete ();
                        return 0;
                    }
                    if (ch == t_attr.c_cc[VEOF] && t_attr.c_cc[VERASE] != _POSIX_VDISABLE)
                    {
                        if (clen)
                        {
                            R_process_input_delete ();
                            return 0;
                        }
                        strcpy (s, "q");
                        printf ("\n");
                        return 1;
                    }
                    if (ch == t_attr.c_cc[VKILL] && t_attr.c_cc[VERASE] != _POSIX_VDISABLE)
                    {
                        R_print ();
                        strcpy (y, s);
                        s[0] = '\0';
                        cpos = clen = 0;
                        return 0;
                    }
#ifdef    VREPRINT
                    if (ch == t_attr.c_cc[VREPRINT] && t_attr.c_cc[VERASE] != _POSIX_VDISABLE)
                    {
                        R_print ();
                        return 0;
                    }
#endif /* VREPRINT */
            }
        }
        else if (clen + 1 < HISTORY_LINE_LEN)
        {
            char buf[2] = "\0\0";
            memmove (s + cpos + 1, s + cpos, clen - cpos + 1);
            s[cpos++] = ch;
            clen++;
            s[clen] = 0;
            buf[0] = ch;
            R_rlap (s + cpos, buf, FALSE);
        }
        return 0;
    }
#ifdef ANSI_COLOR
    switch (istat)
    {
        case 1:                /* ESC */
            if (ch == '[' || ch == 'O')
                istat = 2;
            else
                istat = 0;
            break;
        case 2:                /* CSI */
            istat = 0;
            switch (ch)
            {
                case 'A':      /* Up key */
                case 'B':      /* Down key */
                    if ((ch == 'A' && history_cur >= HISTORY_LINES)
                        || (ch == 'B' && history_cur == 0))
                        break;
                    k = history_cur;
                    strcpy (history[history_cur], s);
                    if (ch == 'A')
                        history_cur++;
                    else
                        history_cur--;
                    if (history[history_cur][0] || history_cur == 0)
                    {
                        R_print ();
                        strcpy (s, history[history_cur]);
                        cpos = clen = strlen (s);
                    }
                    else
                    {
                        history_cur = k;
                    }
                    break;
                case 'C':      /* Right key */
                    if (cpos == clen)
                        break;
                    printf ("%c", s[cpos++]);
                    break;
                case 'D':      /* Left key */
                    if (!cpos)
                        break;
                    R_goto (cpos - 1);
                    break;
                case '3':      /* ESC [ 3 ~ = Delete */
                    istat = 3;
                    break;
                default:
                    printf ("\007");
            }
            break;
        case 3:                /* Del Key */
            istat = 0;
            switch (ch)
            {
                case '~':      /* Del Key */
                    R_process_input_delete ();
                    break;
                default:
                    printf ("\007");
            }
    }
#endif
    return 0;
}

void R_getline (char *buf, int len)
{
    strncpy (buf, s, len);
    s[0] = 0;
}

static const char *curprompt = NULL;
static int prstat = 0;
/* 0 = prompt da 1 = prompt kann entfern werden 2 = prompt entfernt */


void R_setprompt (const char *prompt)
{
    if (curprompt)
        free ((char *)curprompt);
    curprompt = strdup (prompt);
}

void R_prompt (void)
{
    int pos = cpos;
    prstat = 2;
    if (curprompt)
        M_print (curprompt);
    prstat = 0;
    printf ("%s", s);
    cpos = clen;
    R_goto (pos);
}

void R_undraw ()
{
    prstat = 1;
}

void R_redraw ()
{
    if (prstat == 1)
    {
        prstat = 0;
        return;
    }
    prstat = 0;
    R_print ();
    R_prompt ();
}

void R_show ()
{
    prstat = 2;
}

void R_print ()
{
    int pos;
    
    if (prstat != 1)
        return;
    pos = cpos;
    prstat = 2;
    R_goto (0);
    cpos = pos;
    M_print ("\r");             /* for tab stop reasons */
    printf (ESC "[J");
}

void R_doprompt (const char *prompt)
{
    R_setprompt (prompt);
}

void R_dopromptf (const char *prompt, ...)
{
    va_list args;
    char buf[2048];
    
    va_start (args, prompt);
    vsnprintf (buf, sizeof (buf), prompt, args);
    va_end (args);

    R_setprompt (buf);
}

static struct termios saved_attr;
static int attrs_saved = 0;

static void tty_restore (void)
{
    if (!attrs_saved)
        return;
#if HAVE_TCGETATTR
    if (tcsetattr (STDIN_FILENO, TCSAFLUSH, &saved_attr) != 0)
        perror ("can't restore tty modes");
    else
#endif
        attrs_saved = 0;
}

static void tty_prepare (void)
{
    istat = 0;
#if HAVE_TCGETATTR
    if (tcgetattr (STDIN_FILENO, &t_attr) != 0)
        return;
    if (!attrs_saved)
    {
        saved_attr = t_attr;
        attrs_saved = 1;
    }

    t_attr.c_lflag &= ~(ECHO | ICANON);
    t_attr.c_cc[VMIN] = 1;
    if (tcsetattr (STDIN_FILENO, TCSAFLUSH, &t_attr) != 0)
        perror ("can't change tty modes");
#endif
}


#endif /* USE_MREADLINE */
