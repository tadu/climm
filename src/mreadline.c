/* $Id$ */

/*****************************************************
 * mreadline - small line editing and history code
 * Copyright (C) 1998 Sergey Shkonda (serg@bcs.zp.ua)
 * This file may be distributed under version 2 of the GPL licence.
 * Originally placed in the public domain by Sergey Shkonda Nov 27, 1998
 *****************************************************/

#include "micq.h"

#ifdef USE_MREADLINE

#include "mreadline.h"
#include "util_ui.h"
#include "util.h"
#include "cmd_user.h"
#include "tabs.h"
#include "conv.h"
#include "contact.h"
#include "preferences.h"
#include "util_str.h"

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
static int istat = 0;

static int curpos;
static int curlen;
#ifdef ENABLE_UTF8
static int bytepos;
static int bytelen;
#else
#define bytepos curpos
#define bytelen curlen
#define charbytes 1
#endif

#ifndef ANSI_TERM
static char bsbuf[HISTORY_LINE_LEN];
#endif

void R_init (void)
{
    int k;
    static int inited = 0;
    
#ifndef ANSI_TERM
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
    curpos = curlen = 0;
    bytepos = bytelen = 0;
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
    int mypos, mylen;
    static char *t = NULL;
    static UDWORD size = 0;
#ifdef ANSI_TERM
    int scr, off;
#endif

    assert (pos >= 0);
    
    if (pos == curpos)
        return;
        
    if (!t)
        t = strdup ("");
    *t = '\0';
#ifdef WIP
    fprintf (stderr, "Goto: from %d to %d (with %d)", curpos, pos, Get_Max_Screen_Width ());
#endif

#ifdef ANSI_TERM
    scr = Get_Max_Screen_Width ();
    off = M_pos ();
    while ((off + pos) / scr < (off + curpos) / scr)
    {
        t = s_cat (t, &size, ESC "[A");
        curpos -= scr;
    }
    if (curpos < 0)
    {
        t = s_catf (t, &size, ESC "[%dC", -curpos);
        curpos = 0;
    }
    if (pos < curpos)
        t = s_catf (t, &size, ESC "[%dD", curpos - pos);
#else
    while (pos + 10 <= curpos)
    {
        t = s_cat (t, &size, "\b\b\b\b\b\b\b\b\b\b");
        curpos -= 10;
    }
    if (pos < curpos)
    {
        t = s_catf (t, &size, "%.*s", curpos - pos, "\b\b\b\b\b\b\b\b\b\b");
        curpos = pos;
    }
#endif

    if (curpos < pos)
    {
#ifdef ENABLE_UTF8
        if ((prG->enc_loc & ~ENC_AUTO) == ENC_UTF8)
        {
            mypos = s_offset (s, curpos);
            mylen = s_offset (s, pos) - mypos;
        }
        else
#endif
        {
            mypos = curpos;
            mylen = pos - curpos;
        }
        t = s_catf (t, &size, "%.*s", mylen, s + mypos);
    }
    curpos = pos;
#ifdef ENABLE_UTF8
    bytepos = s_offset (s, curpos);
#endif
#ifdef WIP
    fprintf (stderr, "byte %d '%s' out of '%s'\n", bytepos, t, s);
#endif
    printf ("%s", t);
}

void R_rlap (const char *s, const char *add, BOOL clear)
{
    int len;
    
#ifdef ENABLE_UTF8
    len = (prG->enc_loc & ~ENC_AUTO) == ENC_UTF8 ? s_strlen (s) : strlen (s);
#else
    len = strlen (s);
#endif
#ifdef ANSI_TERM
    printf ("%s%s%s%s", add, s, clear ? ESC "[J" : "",
            (M_pos () + curpos) % Get_Max_Screen_Width() == 0 ? " \b" : "");

    curpos += len;
    R_goto (curpos - len);

#else
    printf ("%s%s%s%.*s", add, s, clear ? " \b" : "", len, bsbuf);
#endif
}


static void R_process_input_backspace (void)
{
#ifdef ENABLE_UTF8
    int charbytes;
#endif

    if (!curpos)
        return;
    
#ifdef ENABLE_UTF8
    if ((prG->enc_loc & ~ENC_AUTO) == ENC_UTF8 && s[bytepos - 1] & 0x80)
    {
        charbytes = 0;
        do {
            charbytes++;
        } while ((s[bytepos - charbytes] & 0xc0) != 0xc0);
    }
    else
        charbytes = 1;
    bytelen -= charbytes;
#endif

    curlen--;
    R_goto (curpos - 1);
    memmove (s + bytepos, s + bytepos + charbytes, bytelen - bytepos + 1);
    R_rlap (s + bytepos, "", TRUE);
}

static void R_process_input_delete (void)
{
#ifdef ENABLE_UTF8
    int charbytes;
#endif

    if (curpos >= curlen)
        return;

#ifdef ENABLE_UTF8
    if ((prG->enc_loc & ~ENC_AUTO) == ENC_UTF8 && s[bytepos] & 0x80)
    {
        charbytes = 0;
        do {
            charbytes++;
        } while ((s[bytepos + charbytes] & 0xc0) == 0x80);
    }
    else
        charbytes = 1;
    bytelen -= charbytes;
#endif

    curlen--;

    memmove (s + bytepos, s + bytepos + charbytes, bytelen - bytepos + 1);
    R_rlap (s + bytepos, "", TRUE);
}

/* tab completion

Originally, when the user pressed the tab key, mICQ would cycle through all
contacts that messages had been sent to or received from.  This is now
called simple tab completion.  (set tabs simple in micqrc)

Now, there's the new cycling tab completion that makes the tab key work
just like in a couple of popular IRC clients.  Each press of the tab key
will cycle through the nicks in the current contact list that begin with the
word that the cursor is standing on or immediately after on the command
line.  This is called cycle tab completion.

set tabs cycle in micqrc will make mICQ search only online contacts, or
set tabs cycleall to have mICQ search the entire contact list, including
offline contacts. */

static int tabstate = 0;
/* set to 1 on first tab, reset to 0 on any other key in R_process_input */

static char tabword[20];
/* word that the cursor was on or immediately after at first tab */

static int tabwlen;
/* length of tabword */

static Contact *tabcont;
/* current contact in contact list cycle */

static char *tabwstart;
static char *tabwend;
/* position of tabword on the command line, needed to handle nicks
containing spaces */

void R_process_input_tab (void)
{
    UDWORD uin;
    const char *msgcmd = CmdUserLookupName ("msg");
    int nicklen = 0;
    int gotmatch = 0;

/* FIXME: */

    if (prG->tabs == TABS_SIMPLE)
    {
        if (strncmp (s, msgcmd, strlen (s) < strlen (msgcmd) ? strlen (s) : strlen (msgcmd)))
        {
            M_print ("\a");
            return;
        }
        if (strchr (s, ' ') && strchr (s, ' ') - s < strlen (s) - 1
            && s[strlen (s) - 1] != ' ')
        {
            M_print ("\a");
            return;
        }

        if ((uin = TabGetNext ()))
            sprintf (s, "%s %s ", msgcmd, ContactFindName (uin));
        else
            sprintf (s, "%s ", msgcmd);

        R_remprompt ();
        curlen = curpos = strlen (s);
    }
    else
    {
        if (tabstate == 0)
        {
            tabcont = ContactStart ();
            tabwstart = s + curpos;
            if (*tabwstart == ' ' && tabwstart > s) tabwstart --;
            while (*tabwstart != ' ' && tabwstart >= s) tabwstart --;
            tabwstart ++;
            if (!(tabwend = strchr (tabwstart, ' ')))
                tabwend = s + curlen;
            tabwlen = sizeof (tabword) < tabwend - tabwstart ? sizeof (tabword) : tabwend - tabwstart;
            strncpy (tabword, tabwstart, tabwlen);
        }
        else
            tabcont = ContactHasNext (tabcont) ? ContactNext (tabcont) : ContactStart ();
        if (prG->tabs == TABS_CYCLE)
            while (tabcont->status == STATUS_OFFLINE && ContactHasNext (tabcont))
                tabcont = ContactNext (tabcont);
        while (!gotmatch)
        {
            while (!gotmatch && ContactHasNext (tabcont))
            {
                nicklen = strlen(tabcont->nick);
                if (((prG->tabs == TABS_CYCLE && tabcont->status != STATUS_OFFLINE) || prG->tabs == TABS_CYCLEALL)
                    && nicklen >= tabwlen && !strncasecmp (tabword, tabcont->nick, tabwlen))
                    gotmatch = 1;
                else
                    tabcont = ContactNext (tabcont);
            }
            if (!gotmatch)
            {
                if (tabstate == 0)
                {
                    M_print ("\a");
                    return;
                }
                else
                    tabcont = ContactStart ();
            }
        }
        memmove (tabwstart + nicklen, tabwend, strlen (tabwend) + 1);
        tabwend = tabwstart + nicklen;
        strncpy (tabwstart, tabcont->nick, nicklen);
        R_remprompt ();
        curlen = strlen (s);
        curpos = tabwstart - s + nicklen;
        tabstate = 1;
    }
}

/*
 * Read a character of input and process it.
 */
int R_process_input (void)
{
    char ch;
    int k;

    if (!read (STDIN_FILENO, &ch, 1))
        return 0;
#ifdef __BEOS__
    if (ch == (char)0x80)
        return 0;
#endif
    if (!istat)
    {
        if (prG->tabs != TABS_SIMPLE && ch != '\t')
            tabstate = 0;
        if ((ch > 0 && ch < ' ') || ch == 127 || !ch)
        {
            switch (ch)
            {
                case 1:        /* ^A */
                    R_goto (0);
                    break;
                case 5:        /* ^E */
                    R_goto (curlen);
                    break;
                case 8:        /* ^H = \b */
                    R_process_input_backspace ();
                    return 0;
                case 11:       /* ^K */
                    curlen = curpos;
                    bytelen = bytepos;
                    s[bytepos] = '\0';
                    printf (ESC "[J");
                    break;
                case '\n':
                case '\r':
                    s[bytelen] = 0;
                    R_goto (curlen);
                    curpos = curlen = 0;
                    bytepos = bytelen = 0;
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
                    R_remprompt ();
                    system ("clear");
                    break;
                case '\t':
                    R_process_input_tab ();
                    break;
                case 25:       /* ^Y */
                    R_remprompt ();
                    strcpy (s, y);
                    bytelen = bytepos = strlen (s);
#ifdef ENABLE_UTF8
                    curlen = curpos = ((prG->enc_loc & ~ENC_AUTO) == ENC_UTF8) ? s_strlen (s) : bytelen;
#endif
                    break;
#ifdef ANSI_TERM
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
                        if (curlen)
                        {
                            R_process_input_delete ();
                            return 0;
                        }
                        strcpy (s, "q");
                        printf ("q\n");
                        return 1;
                    }
                    if (ch == t_attr.c_cc[VKILL] && t_attr.c_cc[VERASE] != _POSIX_VDISABLE)
                    {
                        R_remprompt ();
                        strcpy (y, s);
                        s[0] = '\0';
                        curpos = curlen = 0;
                        bytepos = bytelen = 0;
                        return 0;
                    }
#ifdef    VREPRINT
                    if (ch == t_attr.c_cc[VREPRINT] && t_attr.c_cc[VERASE] != _POSIX_VDISABLE)
                    {
                        R_remprompt ();
                        return 0;
                    }
#endif /* VREPRINT */
            }
        }
        else if (bytelen + 1 < HISTORY_LINE_LEN)
        {
            static char buf[7] = "\0\0\0\0\0\0";
            static char todo = 0;

#ifdef ENABLE_UTF8
            int charbytes;
            
            if ((prG->enc_loc & ~ENC_AUTO) == ENC_UTF8 && ch & 0x80)
            {
                if (ch & 0x40)
                {
                    buf[0] = ch;
                    buf[1] = '\0';
                    if (~ch & 0x20)
                        todo = 1;
                    else if (~ch & 0x10)
                        todo = 2;
                    else if (~ch & 0x08)
                        todo = 3;
                    else if (~ch & 0x04)
                        todo = 4;
                    else
                        todo = 5;
                    return 0;
                }
                else if (--todo)
                {
                    strncat (buf, &ch, 1);
                    return 0;
                }
            }
            else
                buf[0] = '\0';
            
            strncat (buf, &ch, 1);
            charbytes = strlen (buf);
#else
            buf[0] = ch;
            buf[1] = '\0';
#endif

            bytelen += charbytes;
            curlen += 1;
            curpos += 1;
            memmove (s + bytepos + charbytes, s + bytepos, bytelen - bytepos);
            memmove (s + bytepos, buf, charbytes);
            bytepos += charbytes;
            s[bytelen] = 0;
            R_rlap (s + bytepos, buf, FALSE);
            buf[0] = '\0';
            todo = 0;
        }
        return 0;
    }
#ifdef ANSI_TERM
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
                    if (history[history_cur][0] || !history_cur)
                    {
                        R_remprompt ();
                        strcpy (s, history[history_cur]);
                        bytepos = bytelen = strlen (s);
#ifdef ENABLE_UTF8
                        curpos = curlen = (prG->enc_loc & ~ENC_AUTO) == ENC_UTF8 ? s_strlen (s) : bytepos;
#endif
                    }
                    else
                        history_cur = k;
                    break;
                case 'C':      /* Right key */
                    if (curpos == curlen)
                        break;
                    R_goto (curpos + 1);
                    break;
                case 'D':      /* Left key */
                    if (!curpos)
                        break;
                    R_goto (curpos - 1);
                    break;
                case '3':      /* ESC [ 3 ~ = Delete */
                    istat = 3;
                    break;
                default:
                    printf ("\a");
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
                    printf ("\a");
            }
    }
#endif
    return 0;
}

/*
 * Return and delete the inputted line.
 */
void R_getline (char *buf, int len)
{
#ifdef ENABLE_UTF8
    strncpy (buf, ConvToUTF8 (s, prG->enc_loc), len);
#else
    strncpy (buf, s, len);
#endif
    buf[len - 1] = '\0';
    s[0] = 0;
}

static char *curprompt = NULL;
static int prstat = 0;
static time_t prlast = 0;
/* 0 = prompt printed
 * 1 = prompt printed, but "virtually" removed
 * 2 = prompt removed
 * 3 = prompt printed, but "virtually" removed, and modified
 */

/*
 * Sets and displays the prompt.
 */
void R_setprompt (const char *prompt)
{
    time_t now = time (NULL);

    if (now == prlast)
        return;
    prlast = now;
    s_repl (&curprompt, prompt);
    R_undraw ();
    prstat = 3;
}

/*
 * Formats, sets and displays the prompt.
 */
void R_setpromptf (const char *prompt, ...)
{
    va_list args;
    char buf[2048];
    
    va_start (args, prompt);
    vsnprintf (buf, sizeof (buf), prompt, args);
    va_end (args);

    R_setprompt (buf);
}

/*
 * Resets the prompt to the standard one.
 */
void R_resetprompt (void)
{
    prlast = 0;
    if (prG->flags & FLAG_UINPROMPT && uiG.last_sent_uin)
        R_setpromptf (COLSERVER "[%s]" COLNONE " ", ContactFindName (uiG.last_sent_uin));
    else
        R_setpromptf (COLSERVER "%s" COLNONE, i18n (1040, "mICQ> "));
}

/*
 * Re-displays prompt.
 */
void R_prompt (void)
{
    int pos = curpos;
    if (prstat != 2)
    {
        prstat = 1;
        R_remprompt ();
    }
    if (curprompt)
        M_print (curprompt);
    prstat = 0;
    printf ("%s", s);
    curpos = curlen;
    R_goto (pos);
}

/*
 * Virtually hides the prompt.
 */
void R_undraw ()
{
    if (!prstat)
        prstat = 1;
}

/*
 * Virtually re-displays prompt.
 */
void R_redraw ()
{
    if (prstat == 1)
    {
        prstat = 0;
        return;
    }
    R_prompt ();
}

/*
 * Physically removes virtually removed prompt.
 */
void R_remprompt ()
{
    int pos;
    
    if (prstat != 1 && prstat != 3)
        return;
    pos = curpos;
    prstat = 2;
    R_goto (0);
    curpos = pos;
    M_print ("\r");             /* for tab stop reasons */
    printf (ESC "[J");
    prstat = 2;
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
