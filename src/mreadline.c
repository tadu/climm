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
#include "session.h"
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
#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifndef _POSIX_VDISABLE
#define _POSIX_VDISABLE 0
#endif
#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#if HAVE_CONIO_H
#include <conio.h>
#endif
#ifdef HAVE_TCGETATTR
struct termios sattr;
#endif

#define HISTORY_LINES 10
#define HISTORY_LINE_LEN 1024

#if HAVE_TCGETATTR
static struct termios t_attr;
static struct termios saved_attr;
#endif
static int attrs_saved = 0;
static void tty_prepare (void);
static void tty_restore (void);

static void R_process_input_backspace (void);
static void R_process_input_delete (void);

#if defined(SIGTSTP) && defined(SIGCONT)
static RETSIGTYPE micq_ttystop_handler (int);
static RETSIGTYPE micq_cont_handler (int);
#endif
static RETSIGTYPE micq_int_handler (int);

static char *history[HISTORY_LINES + 1];
static int history_cur = 0;
static char s[HISTORY_LINE_LEN];
static char y[HISTORY_LINE_LEN];
static int istat = 0;

static int curpos;
static int curlen;
static int bytepos;
static int bytelen;

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
#if defined(SIGTSTP) && defined(SIGCONT)
    signal (SIGTSTP, &micq_ttystop_handler);
    signal (SIGCONT, &micq_cont_handler);
#endif
#ifndef __amigaos__
    signal (SIGINT, &micq_int_handler);
#else
    signal (SIGINT, SIG_IGN);
#endif
    tty_prepare ();
    atexit (tty_restore);
    R_resetprompt ();
}

/*
 * Clear the screen.
 */
void R_clrscr (void)
{
#ifdef ANSI_TERM
    printf ("\x1b[H\x1b[J");
#else
    printf ("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
#endif
}

static int tabstate = 0;
/* set to 1 on first tab, reset to 0 on any other key in R_process_input */

#if defined(SIGTSTP) && defined(SIGCONT)
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
#endif

volatile static UBYTE interrupted = 0;

UBYTE R_isinterrupted (void)
{
    UBYTE is = interrupted;
    interrupted = 0;
    return is;
}

static RETSIGTYPE micq_int_handler (int a)
{
    int k;
    R_remprompt ();
    s[bytelen] = 0;
    history_cur = 0;
    TabReset ();
    strcpy (history[0], s);
    if (strcmp (s, history[1]) && *s)
        for (k = HISTORY_LINES; k; k--)
            strcpy (history[k], history[k - 1]);
    R_goto (curlen);
    printf ("\n");
    curpos = curlen = 0;
    bytepos = bytelen = 0;
    tabstate = 0;
    s[0] = 0;
    if (interrupted & 1)
        exit (1);
    interrupted = 3;
    R_resetprompt ();
    R_prompt ();
    signal (SIGINT, &micq_int_handler);
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
    static str_s t;
#ifdef ANSI_TERM
    int scr, off;
#endif

    assert (pos >= 0);
    
    if (pos == curpos)
        return;

    s_init (&t, "", 100);

#ifdef ANSI_TERM
    scr = Get_Max_Screen_Width ();
    off = M_pos ();
    while ((off + pos) / scr < (off + curpos) / scr)
    {
        s_catc (&t, ESCC);
        s_catc (&t, '[');
        s_catc (&t, 'A');
        curpos -= scr;
    }
    if (curpos < 0)
    {
        s_catf (&t, ESC "[%dC", -curpos);
        curpos = 0;
    }
    if (pos < curpos)
        s_catf (&t, ESC "[%dD", curpos - pos);
#else
    while (pos + 10 <= curpos)
    {
        s_cat (&t, "\b\b\b\b\b\b\b\b\b\b");
        curpos -= 10;
    }
    while (pos < curpos)
    {
        s_catc (&t, '\b');
        curpos--;
    }
#endif

    if (curpos < pos)
    {
        if (ENC(enc_loc) == ENC_UTF8)
        {
            mypos = s_offset (s, curpos);
            mylen = s_offset (s, pos) - mypos;
        }
        else
        {
            mypos = curpos;
            mylen = pos - curpos;
        }
        s_catf (&t, "%.*s", mylen, s + mypos);
    }
    curpos = pos;
    bytepos = c_offset (s, curpos);
    printf ("%s", t.txt);
}

void R_rlap (const char *s, const char *add, BOOL clear)
{
    int len;
    
    len = (ENC(enc_loc) == ENC_UTF8) ? s_strlen (s) : strlen (s);
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
    int charbytes;

    if (!curpos)
        return;
    
    if (ENC(enc_loc) == ENC_UTF8 && s[bytepos - 1] & 0x80)
    {
        charbytes = 0;
        do {
            charbytes++;
        } while ((s[bytepos - charbytes] & 0xc0) != 0xc0);
    }
    else
        charbytes = 1;
    bytelen -= charbytes;

    curlen--;
    R_goto (curpos - 1);
    memmove (s + bytepos, s + bytepos + charbytes, bytelen - bytepos + 1);
    R_rlap (s + bytepos, "", TRUE);
}

static void R_process_input_delete (void)
{
    int charbytes;

    if (curpos >= curlen)
        return;

    if (ENC(enc_loc) == ENC_UTF8 && s[bytepos] & 0x80)
    {
        charbytes = 0;
        do {
            charbytes++;
        } while ((s[bytepos + charbytes] & 0xc0) == 0x80);
    }
    else
        charbytes = 1;
    bytelen -= charbytes;

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

static char tabword[20];
/* word that the cursor was on or immediately after at first tab */

static int tabwlen;
/* length of tabword */

static int      tabconti;
/* current contact in contact list cycle */

static char *tabwstart;
static char *tabwend;
/* position of tabword on the command line, needed to handle nicks
containing spaces */

void R_process_input_tab (void)
{
    UDWORD uin;
    Contact *cont, *tabcont;
    const char *msgcmd;
    int nicklen = 0;
    int gotmatch = 0;

    msgcmd = CmdUserLookupName ("r");
    if (msgcmd && *msgcmd && !strncmp (s, msgcmd, strlen (msgcmd)) &&
         bytelen > strlen (msgcmd) && s[strlen (msgcmd)] == ' ')
        return;
    msgcmd = CmdUserLookupName ("a");
    if (msgcmd && *msgcmd && !strncmp (s, msgcmd, strlen (msgcmd)) &&
         bytelen > strlen (msgcmd) && s[strlen (msgcmd)] == ' ')
        return;
    msgcmd = CmdUserLookupName ("msg");

    if (bytelen < strlen (msgcmd) &&
        !strncmp (s, msgcmd, bytelen < strlen (msgcmd) ? bytelen : strlen (msgcmd)))
    {
        snprintf (s, sizeof (s), "%s ", msgcmd);
        bytepos = bytelen = strlen (s);
        curpos = curlen = c_strlen (s);
    }

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
        {
            cont = ContactFind (NULL, 0, uin, NULL);
            snprintf (s, sizeof (s), "%s %s ", msgcmd,
                      cont ? ConvTo (cont->nick, prG->enc_loc)->txt
                           : s_sprintf ("%ld", uin));
        }
        else
            snprintf (s, sizeof (s), "%s ", msgcmd);

        R_remprompt ();
        bytelen = bytepos = strlen (s);
        curlen = curpos = c_strlen (s);
    }
    else
    {
        if (tabstate == 0)
        {
            tabcont = ContactIndex (0, tabconti = 0);
            tabwstart = s + bytepos;
            if (*tabwstart == ' ' && tabwstart > s) tabwstart --;
            while (*tabwstart != ' ' && tabwstart >= s) tabwstart --;
            tabwstart ++;
            if (!(tabwend = strchr (tabwstart, ' ')))
            {
                tabwend = s + bytelen;
                *tabwend = '\0';
            }
            tabwlen = sizeof (tabword) < tabwend - tabwstart ? sizeof (tabword) : tabwend - tabwstart;
            snprintf (tabword, sizeof (tabword), "%.*s", (int)(tabwend - tabwstart), tabwstart);
        }
        else
            tabcont = ContactIndex (0, ++tabconti);
        if (prG->tabs == TABS_CYCLE)
            while (tabcont && tabcont->status == STATUS_OFFLINE)
                tabcont = ContactIndex (0, ++tabconti);
        if (!tabcont)
             tabcont = ContactIndex (0, tabconti = 0);
        while (!gotmatch)
        {
            while (!gotmatch && tabcont)
            {
                nicklen = strlen (tabcont->nick);
                if (((prG->tabs == TABS_CYCLE && tabcont->status != STATUS_OFFLINE) || prG->tabs == TABS_CYCLEALL)
                    && nicklen >= tabwlen && !strncasecmp (tabword, ConvTo (tabcont->nick, prG->enc_loc)->txt, tabwlen)
                    && (tabwlen > 0) && tabcont->group)
                    gotmatch = 1;
                else
                    tabcont = ContactIndex (NULL, ++tabconti);
            }
            if (!gotmatch)
            {
                if (tabstate == 0)
                {
                    M_print ("\a");
                    return;
                }
                else
                    tabcont = ContactIndex (NULL, tabstate = tabconti = 0);
            }
        }
        *tabwstart = '\0';
        nicklen = strlen (ConvTo (tabcont->nick, prG->enc_loc)->txt);
        memmove (s, s_sprintf ("%s%s%s", s, ConvTo (tabcont->nick, prG->enc_loc)->txt, tabwend), HISTORY_LINE_LEN);
        tabwend = tabwstart + nicklen;
        R_remprompt ();
        bytelen = strlen (s);
        bytepos = tabwstart - s + nicklen;
        curlen = c_strlen (s);
        curpos = ENC(enc_loc) ? s_strnlen (s, bytepos) : bytepos;
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
#ifdef ANSI_TERM
    static UDWORD inp = 0;
#endif

#if INPUT_BY_GETCH
    ch = getch ();
#else
    k = read (STDIN_FILENO, &ch, 1);
    if (!k || k == -1)
        return 0;
#endif
    interrupted &= ~1;
    if (!istat)
    {
        if (prG->tabs != TABS_SIMPLE && ch != '\t')
            tabstate = 0;
        if ((ch > 0 && ch < ' ') || ch == (char)127 || !ch)
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
                    R_clrscr ();
                    break;
                case '\t':
                    R_process_input_tab ();
                    break;
                case 16:       /* ^P */
                    istat = 10;
                    break;
                case 25:       /* ^Y */
                    R_remprompt ();
                    strcpy (s, y);
                    bytelen = bytepos = strlen (s);
                    curlen = curpos = (ENC(enc_loc) == ENC_UTF8) ? s_strlen (s) : bytelen;
                    break;
#ifdef ANSI_TERM
                case 27:       /* ESC */
                    istat = 1;
                    break;
#endif
                case 127:      /* DEL */
                    if (prG->flags & FLAG_DELBS)
                        R_process_input_backspace ();
                    else
                        R_process_input_delete ();
                    return 0;
                default:
#if HAVE_TCGETATTR
#if defined(VERASE)
                    if (ch == t_attr.c_cc[VERASE] && t_attr.c_cc[VERASE] != _POSIX_VDISABLE)
                    {
                        if (prG->flags & FLAG_DELBS)
                            R_process_input_backspace ();
                        else
                            R_process_input_delete ();
                        return 0;
                    }
#endif
#if defined(VEOF) && defined(VERASE)
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
#endif
#if defined(VKILL) && defined(VERASE)
                    if (ch == t_attr.c_cc[VKILL] && t_attr.c_cc[VERASE] != _POSIX_VDISABLE)
                    {
                        R_remprompt ();
                        strcpy (y, s);
                        s[0] = '\0';
                        curpos = curlen = 0;
                        bytepos = bytelen = 0;
                        return 0;
                    }
#endif
#if defined(VREPRINT) && defined(VERASE)
                    if (ch == t_attr.c_cc[VREPRINT] && t_attr.c_cc[VERASE] != _POSIX_VDISABLE)
                    {
                        R_remprompt ();
                        return 0;
                    }
#endif
#endif
                    /* silence warning */ ;
            }
        }
        else if (ENC(enc_loc) == ENC_LATIN1 && ch == (char)0x9b)
            istat = 2;
        else if (bytelen + 1 < HISTORY_LINE_LEN)
        {
            static char buf[7] = "\0\0\0\0\0\0";
            static char todo = 0;

            int charbytes;
            
            if (ENC(enc_loc) == ENC_UTF8 && ch & 0x80)
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
            curlen += 1;
            curpos += 1;

            bytelen += charbytes;
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
            if (ch == 'u' || ch == 'U')
                istat = 10;
            else if (ch == '[' || ch == 'O')
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
                        curpos = curlen = ENC(enc_loc) == ENC_UTF8 ? s_strlen (s) : bytepos;
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
                case 'H':
                    R_goto (0);
                    break;
                case 'F':
                    R_goto (curlen);
                    break;
                case '3':      /* ESC [ 3 ~ = Delete */
                    istat = 3;
                    break;
                case '1':
                case '7':
                    istat = 4; /* ESC [ 7 ~ = ESC [ 1 ~ = Home */
                    break;
                case '4':
                case '8':
                    istat = 5; /* ESC [ 8 ~ = ESC [ 4 ~ = End */
                    break;
                default:
                    printf ("\a");
            }
            break;
        case 3:                /* Del Key */
            istat = 0;
            if (ch == '~')
                R_process_input_delete ();
            else
                printf ("\a");
            break;
        case 4:
            istat = 0;
            if (ch == '~')
                R_goto (0);
            else
                printf ("\a");
            break;
        case 5:
            istat = 0;
            if (ch == '~')
                R_goto (curlen);
            else
                printf ("\a");
            break;
        case 10:
            istat++;
#ifndef WIP
            if (ENC(enc_loc) != ENC_UTF8)
                istat = 0;
            else
#endif
            if (ch >= '0' && ch <= '9')
                inp = ch - '0';
            else if (ch >= 'a' && ch <= 'f')
                inp = ch - 'a' + 10;
            else if (ch >= 'A' && ch <= 'F')
                inp = ch - 'A' + 10;
            else
                istat = 0;
            break;
        case 11:
            istat++;
            inp <<= 4;
            if (ch >= '0' && ch <= '9')
                inp |= ch - '0';
            else if (ch >= 'a' && ch <= 'f')
                inp |= ch - 'a' + 10;
            else if (ch >= 'A' && ch <= 'F')
                inp |= ch - 'A' + 10;
            else istat = 0;
            break;
        case 12:
            istat++;
            inp <<= 4;
            if (ch >= '0' && ch <= '9')
                inp |= ch - '0';
            else if (ch >= 'a' && ch <= 'f')
                inp |= ch - 'a' + 10;
            else if (ch >= 'A' && ch <= 'F')
                inp |= ch - 'A' + 10;
            else istat = 0;
            break;
        case 13:
            istat++;
            inp <<= 4;
            if (ch >= '0' && ch <= '9')
                inp |= ch - '0';
            else if (ch >= 'a' && ch <= 'f')
                inp |= ch - 'a' + 10;
            else if (ch >= 'A' && ch <= 'F')
                inp |= ch - 'A' + 10;
            else istat = 0;
            if (!istat)
                return 0;
            
            {
                const char *add = ConvUTF8 (inp);
                int charbytes = strlen (add);
            
                curlen += 1;
                curpos += 1;

                bytelen += charbytes;
                memmove (s + bytepos + charbytes, s + bytepos, bytelen - bytepos);
                memmove (s + bytepos, add, charbytes);
                bytepos += charbytes;
                s[bytelen] = 0;
                R_rlap (s + bytepos, add, FALSE);
            }
            istat = 0;
            break;
        default:
            istat = 0;
    }
#endif
    return 0;
}

/*
 * Return and delete the inputted line.
 */
void R_getline (char *buf, int len)
{
    str_s str = { s, len, 0 };
    strncpy (buf, ConvFrom (&str, prG->enc_loc)->txt, len);
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
    prlast = time (NULL);
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
 * Formats, sets and displays the prompt.
 */
void R_settimepromptf (const char *prompt, ...)
{
    va_list args;
    char buf[2048];
    
    if (prlast == time (NULL))
        return;
    
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
    Contact *cont;
    if (prG->flags & FLAG_UINPROMPT && uiG.last_sent_uin
        && (cont = ContactFind (NULL, 0, uiG.last_sent_uin, NULL)))
        R_setpromptf ("%s[%s]%s ", COLSERVER, cont->nick, COLNONE);
    else
        R_setpromptf ("%s%s%s", COLSERVER, i18n (1040, "mICQ> "), COLNONE);
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
    int bpos = bytepos;
    
    if (prstat != 1 && prstat != 3)
        return;
    pos = curpos;
    prstat = 2;
    R_goto (0);
    curpos = pos;
    bytepos = bpos;
    M_print ("\r");             /* for tab stop reasons */
#ifdef ANSI_TERM
    printf (ESC "[J");
#else
    printf ("%.*s", curpos, bsbuf);
#endif
    prstat = 2;
}


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

/***************************************************************
Turns keyboard echo off for the password
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
Turns keyboard echo back on after the password
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

#if defined(SIGWINCH)
static RETSIGTYPE micq_sigwinch_handler (int a)
{
    struct winsize ws;

    scrwd = 0;
    ioctl (STDIN_FILENO, TIOCGWINSZ, &ws);
    scrwd = ws.ws_col;
}
#endif

UWORD Get_Max_Screen_Width ()
{
    UDWORD scrwdtmp = scrwd;

    if (scrwdtmp)
        return scrwdtmp;

#if defined(SIGWINCH)
    micq_sigwinch_handler (0);
    if ((scrwdtmp = scrwd))
    {
        if (signal (SIGWINCH, &micq_sigwinch_handler) == SIG_ERR)
            scrwd = 0;
        return scrwdtmp;
    }
#endif
    if (prG->screen)
        return prG->screen;
    return 80;                  /* a reasonable screen width default. */
}

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

static const char *M_getlogo ()
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

#define USECOLOR(c)     col = ((prG->flags & FLAG_COLOR) ? ContactPrefStr (NULL, CO_COLORNONE + c) : "")
#define USECOLORINVCHAR (colinvchar ? colinvchar : colinvchar = ((prG->flags & FLAG_COLOR) ? ContactPrefStr (NULL, CO_COLORINVCHAR) : ""))

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
    static str_s colbuf = { NULL };
    int sw = Get_Max_Screen_Width () - IndentCount;
    
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

    R_remprompt ();
    for (; *str; str++)
    {
        for (test = save = str; *test; test++)
        {
            if (!(*test & 0xe0)) /* special character reached - emit text till last saved position */
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
                printf ("%.*s%*s", (int) c_offset (str, sw - CharCount), str, IndentCount, "");
                str += c_offset (str, sw - CharCount);
                if (isline)
                {
                    printf ("%s...%s", USECOLORINVCHAR, col);
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
                    printf ("%s...%s\n", USECOLORINVCHAR, col);
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
                printf ("%s%s..", USECOLORINVCHAR, para);
            printf ("%s\n", col);
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
            printf ("%s%c%s", USECOLORINVCHAR, *str - 1 + 'A', col);
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
                            printf ("%s\n", col);
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
                            while (!strncmp (save, "m\e[", 3) && strchr (save + 1, 'm'))
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
                printf ("%s%c%s", USECOLORINVCHAR, *str - 1 + 'A', col);
                
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
