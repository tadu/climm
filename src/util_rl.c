
/*
 * Line editing code.
 *
 * mICQ Copyright (C) © 2001,2002,2003 Rüdiger Kuhlmann
 *
 * mICQ is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 dated June, 1991.
 *
 * mICQ is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * $Id$
 */

#include "micq.h"
#include "util_rl.h"
#include "tabs.h"
#include "conv.h"
#include "contact.h"
#include "cmd_user.h"
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

#if HAVE_TCGETATTR
static struct termios tty_attr;
static struct termios tty_saved_attr;
static int tty_saved = 0;
#endif

#if defined(SIGTSTP) && defined(SIGCONT)
static RETSIGTYPE tty_stop_handler (int);
static RETSIGTYPE tty_cont_handler (int);
#endif
static RETSIGTYPE tty_int_handler (int);

/* static void rl_dump_line (void); */
static int  rl_getcolumns (void);
static void rl_syncpos (UDWORD pos);
static void rl_goto (UDWORD pos);
static void rl_recheck (BOOL clear);
static void rl_insert_basic (UWORD ucs, const char *display, UDWORD len, UDWORD collen);
static void rl_analyze_ucs (UWORD ucs, const char **display, UWORD *columns);
static void rl_insert (UWORD ucs);
static int rl_delete (void);
static int rl_left (UDWORD i);
static int rl_right (UDWORD i);
static const Contact *rl_tab_getnext (strc_t common);
static void rl_tab_accept (void);
static void rl_tab_cancel (void);
static void rl_key_tab (void);
static void rl_key_insert (UWORD ucs);
static void rl_key_left (void);
static void rl_key_right (void);
static void rl_key_delete (void);
static void rl_key_backspace (void);
static void rl_key_end (void);
static void rl_key_cut (void);
static void rl_key_kill (void);
static void rl_linecompress (str_t line, UDWORD from, UDWORD to);
static void rl_lineexpand (char *hist);
static void rl_historyback (void);
static void rl_historyforward (void);
static void rl_historyadd (void);
static void rl_checkcolumns (void);

static volatile int rl_interrupted = 0;
static volatile int rl_columns_cur = 0;

UDWORD rl_columns = 0;
static UDWORD rl_colpos, rl_ucspos, rl_bytepos;
static int rl_stat = 0, rl_inputdone;

static str_s rl_ucs      = { NULL, 0, 0 };
static str_s rl_ucscol   = { NULL, 0, 0 };
static str_s rl_ucsbytes = { NULL, 0, 0 };
static str_s rl_display  = { NULL, 0, 0 };

static str_s rl_input    = { NULL, 0, 0 };
static str_s rl_operate  = { NULL, 0, 0 };
static str_s rl_temp     = { NULL, 0, 0 };

#define RL_HISTORY_LINES 100
static char *rl_yank = NULL;
static char *rl_history[RL_HISTORY_LINES + 1];
static int   rl_history_pos = 0;

static int    rl_tab_state = 0;  /* 0 = OFF 1 = Out 2 = Inc 3 = Online 4 = Offline */
static UDWORD rl_tab_index = 0;  /* index in list */
static UDWORD rl_tab_len   = 0;  /* number of codepoints tabbed in */
static UDWORD rl_tab_pos   = 0;  /* start of word tabbed in */
static UDWORD rl_tab_common = 0; /* number of given codepoints */
static str_s  rl_colon      = { NULL, 0, 0 };
static str_s  rl_coloff     = { NULL, 0, 0 };
static const Contact *rl_tab_cont = NULL;

static str_s  rl_prompt   = { NULL, 0, 0 };
static UWORD  rl_prompt_len = 0;
static int    rl_prompt_stat = 0;
static time_t rl_prompt_time = 0;

/*
 * Initialize read line code
 */
void ReadLineInit ()
{
    int i;
    for (i = 0; i <= RL_HISTORY_LINES; i++)
        rl_history[i] = NULL;
    rl_columns = rl_getcolumns ();
    s_init (&rl_ucs,      "", 128);
    s_init (&rl_ucscol,   "", 128);
    s_init (&rl_ucsbytes, "", 128);
    s_init (&rl_input,    "", 128);
    s_init (&rl_display,  "", 128);
    rl_colpos = rl_ucspos = rl_bytepos = 0;
    ReadLineTtySet ();
    atexit (ReadLineTtyUnset);
#if defined(SIGTSTP) && defined(SIGCONT)
    signal (SIGTSTP, &tty_stop_handler);
    signal (SIGCONT, &tty_cont_handler);
#endif
#ifndef __amigaos__
    signal (SIGINT, &tty_int_handler);
#else
    signal (SIGINT, SIG_IGN);
#endif
    ReadLinePromptReset ();
    ConvTo ("", ENC_UCS2BE);
}

/*
 * Debugging function
 */ /*
static void rl_dump_line (void)
{
    int i, bp;
    fprintf (stderr, "colpos %d ucspos %d bytepos %d\n", rl_colpos, rl_ucspos, rl_bytepos);
    for (i = bp = 0; i < rl_ucscol.len; i++)
    {
        fprintf (stderr, "ucs %02x%02x col %d bytes %d string '%s'\n",
                 rl_ucs.txt[2 * i], rl_ucs.txt[2 * i + 1], rl_ucscol.txt[i], rl_ucsbytes.txt[i],
                 s_mquote (s_sprintf ("%.*s", rl_ucsbytes.txt[i], rl_display.txt + bp), COLNONE, 0));
        bp += rl_ucsbytes.txt[i];
    }
} */

/*** low-level tty stuff ***/

/*
 * Undo changes to tty mode
 */
void ReadLineTtyUnset (void)
{
#if HAVE_TCGETATTR
    if (!tty_saved)
        return;

    if (tcsetattr (STDIN_FILENO, TCSAFLUSH, &tty_saved_attr) != 0)
    {
        M_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
        M_printf (i18n (9999, "Can't restore terminal modes.\n"));
    }
    else
#endif
        tty_saved = 0;
}

/*
 * Change tty mode for mICQ
 */
void ReadLineTtySet (void)
{
#if HAVE_TCGETATTR
    if (tcgetattr (STDIN_FILENO, &tty_attr) != 0)
    {
        M_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
        M_printf (i18n (9999, "Can't read terminal modes.\n"));
        return;
    }
    if (!tty_saved)
    {
        tty_saved_attr = tty_attr;
        tty_saved = 1;
    }
    tty_attr.c_lflag &= ~(ECHO | ICANON);
    if (tcsetattr (STDIN_FILENO, TCSAFLUSH, &tty_attr) != 0)
    {
        M_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
        M_printf (i18n (9999, "Can't modify terminal modes.\n"));
        return;
    }
    tty_attr.c_cc[VMIN] = 1;
    if (tcsetattr (STDIN_FILENO, TCSAFLUSH, &tty_attr) != 0)
    {
        M_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
        M_printf (i18n (9999, "Can't modify terminal modes.\n"));
    }
#endif
}

#if defined(SIGTSTP) && defined(SIGCONT)
/*
 * Handle a ^Z terminal stop signal
 */
static RETSIGTYPE tty_stop_handler (int i)
{
    rl_key_end ();
    printf (" %s^Z%s", COLERROR, COLNONE);
    ReadLineTtyUnset ();
    signal (SIGTSTP, SIG_DFL);
    raise (SIGTSTP);
}

/*
 * Handle continuation after a ^Z terminal stop
 */
static RETSIGTYPE tty_cont_handler (int i)
{
    ReadLineTtySet ();
    M_print ("\r");
    ReadLinePrompt ();
    signal (SIGTSTP, &tty_stop_handler);
    signal (SIGCONT, &tty_cont_handler);
}
#endif

#if defined(SIGWINCH)
/*
 * Handle a window resize
 */
static RETSIGTYPE tty_sigwinch_handler (int i)
{
    struct winsize ws;

    rl_columns_cur = 0;
    ioctl (STDIN_FILENO, TIOCGWINSZ, &ws);
    rl_columns_cur = ws.ws_col;
}
#endif

/*
 * Handle a ^C interrupt signal - push line into history
 * or exit after two consecutive ^C
 */
static RETSIGTYPE tty_int_handler (int i)
{
    signal (SIGINT, SIG_IGN);

    if (rl_interrupted & 1)
    {
        printf (" %s:-X%s\n", COLERROR, COLNONE);
        fflush (stdout);
        exit (1);
    }
    rl_interrupted = 1;
    CmdUserInterrupt ();
    rl_key_end ();

    printf (" %s^C%s\n", COLERROR, COLNONE);
    M_print ("\r");
    rl_prompt_stat = 0;
    rl_tab_state = 0;
    rl_historyadd ();
    s_init (&rl_input, "", 0);
    ReadLinePromptReset ();
    ReadLinePrompt ();

    signal (SIGINT, &tty_int_handler);
}

/*
 * Fetch the number of columns of this terminal
 */
static int rl_getcolumns ()
{
    int width = rl_columns_cur;

    if (rl_columns_cur)
        return rl_columns_cur;

#if defined(SIGWINCH)
    tty_sigwinch_handler (0);
    if ((width = rl_columns_cur))
    {
        if (signal (SIGWINCH, &tty_sigwinch_handler) == SIG_ERR)
            rl_columns_cur = 0;
        return width;
    }
#endif
    if (prG->screen)
        return prG->screen;
    return 80;                  /* a reasonable screen width default. */
}

/*
 * Recheck whether number of columns changed and cause a redraw
 */
static void rl_checkcolumns ()
{
    UDWORD w;

    if ((w = rl_getcolumns ()) == rl_columns)
        return;
    rl_columns = w;
    ReadLinePromptHide ();
}

/*
 * Clear the screen
 */
void ReadLineClrScr ()
{
#ifdef ANSI_TERM
    printf (ESC "[H" ESC "[J");
#else
    printf ("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
#endif
}

/*** read line basics ***/

/*
 * Reset rl_colpos, rl_ucspos, rl_bytepos to correct values for given column
 */
static void rl_syncpos (UDWORD pos)
{
    rl_colpos = rl_ucspos = rl_bytepos = 0;
    while (rl_colpos + rl_ucscol.txt[rl_ucspos] <= pos)
    {
        rl_colpos += rl_ucscol.txt[rl_ucspos];
        rl_bytepos += rl_ucsbytes.txt[rl_ucspos];
        rl_ucspos++;
    }
    assert (rl_colpos == pos);
}

/*
 * Go to given column
 */
static void rl_goto (UDWORD pos)
{
    if (pos == rl_colpos)
        return;

    if (rl_colpos > pos)
    {
#ifdef ANSI_TERM
        int l = ((rl_prompt_len + rl_colpos) / rl_columns) - ((rl_prompt_len + pos) / rl_columns);
        if (l)
            s_catf (&rl_operate, ESC "[%dA", l);
        rl_colpos -= l * rl_columns;
        s_catc (&rl_operate, '\r');
        rl_colpos -= (rl_prompt_len + rl_colpos) % rl_columns;
        if ((int)rl_colpos < 0)
        {
            if (rl_prompt_len)
                s_catf (&rl_operate, ESC "[%uC", rl_prompt_len);
            rl_colpos = 0;
        }
#else
        while (pos < rl_colpos)
        {
            s_catc (&rl_operate, '\b');
            rl_curpos --;
        }
#endif
    }
    rl_syncpos (rl_colpos);
    while (rl_colpos + rl_ucscol.txt[rl_ucspos] <= pos && rl_ucspos < rl_ucscol.len)
    {
        s_catn (&rl_operate, rl_display.txt + rl_bytepos, rl_ucsbytes.txt[rl_ucspos]);
        rl_colpos += rl_ucscol.txt[rl_ucspos];
        rl_bytepos += rl_ucsbytes.txt[rl_ucspos];
        rl_ucspos++;
    }
    assert (rl_colpos == pos);
}

/*
 * Re-check remaining line for multicolumn line break problems
 */
static void rl_recheck (BOOL clear)
{
    int gpos, i;
    
    gpos = rl_colpos;
    
    while (rl_ucspos < rl_ucscol.len)
    {
        if (rl_ucs.txt[2 * rl_ucspos] == -1 && rl_ucs.txt[2 * rl_ucspos + 1] == -1)
        {
            s_delc (&rl_ucs, 2 * rl_ucspos);
            s_delc (&rl_ucs, 2 * rl_ucspos);
            s_delc (&rl_ucscol, rl_ucspos);
            s_delc (&rl_ucscol, rl_ucspos);
            s_delc (&rl_display, rl_bytepos);
        }
        else if (((rl_prompt_len + rl_colpos) % rl_columns)
                 + (UBYTE)rl_ucscol.txt[rl_ucspos] > rl_columns)
        {
            for (i = ((rl_prompt_len + rl_colpos) % rl_columns)
                 + (UBYTE)rl_ucscol.txt[rl_ucspos] - rl_columns; i > 0; i--)
            {
                s_insc (&rl_ucs, 2 * rl_ucspos, -1);
                s_insc (&rl_ucs, 2 * rl_ucspos, -1);
                s_insc (&rl_ucscol, rl_ucspos, 1);
                s_insc (&rl_ucsbytes, rl_ucspos++, 1);
                s_insc (&rl_display, rl_bytepos++, ' ');
                s_catc (&rl_operate, ' ');
                rl_colpos++;
            }
        }
        else
        {
            s_catn (&rl_operate, rl_display.txt + rl_bytepos, rl_ucsbytes.txt[rl_ucspos]);
            rl_bytepos += rl_ucsbytes.txt[rl_ucspos];
            rl_colpos += rl_ucscol.txt[rl_ucspos];
            rl_ucspos++;
        }
    }
#ifdef ANSI_TERM
    s_cat (&rl_operate, " \b");
    if (clear)
        s_cat (&rl_operate, ESC "[J");
#else
    s_cat (&rl_operate, "     \b\b\b\b\b");
#endif
    rl_goto (gpos);
}

/*
 * Insert a character by unicode codepoint, display string in local encoding
 * and its length, and its width in columns
 */
static void rl_insert_basic (UWORD ucs, const char *display, UDWORD len, UDWORD collen)
{
    int i;
    
    if (((rl_prompt_len + rl_colpos) % rl_columns) + collen > rl_columns)
    {
        for (i = ((rl_prompt_len + rl_colpos) % rl_columns) + collen - rl_columns; i > 0; i--)
        {
            s_insc (&rl_ucs, 2 * rl_ucspos, -1);
            s_insc (&rl_ucs, 2 * rl_ucspos, -1);
            s_insc (&rl_ucscol, rl_ucspos, 1);
            s_insc (&rl_ucsbytes, rl_ucspos++, 1);
            s_insc (&rl_display, rl_bytepos++, ' ');
            s_catc (&rl_operate, ' ');
            rl_colpos++;
        }
    }
    
    s_insc (&rl_ucs, 2 * rl_ucspos,  ucs        & 0xff);
    s_insc (&rl_ucs, 2 * rl_ucspos, (ucs / 256) & 0xff);
    s_insc (&rl_ucscol, rl_ucspos, collen);
    s_insc (&rl_ucsbytes, rl_ucspos++, len);
    s_insn (&rl_display, rl_bytepos, display, len);
    rl_colpos += collen;
    rl_bytepos += len;
    s_cat (&rl_operate, display);
}

/*
 * Determine for a character is display string and columns width
 */
static void rl_analyze_ucs (UWORD ucs, const char **display, UWORD *columns)
{
   if (ucs < 32 || ucs == 127) /* control code */
    {
        *display = s_sprintf ("%s%c%s", COLINVCHAR, (ucs - 1 + 'A') & 0x7f, COLNONE);
        *columns = 1;
    }
    else if (ucs >= 128 && ucs < 160) /* more control code */
    {
        *display = s_sprintf ("%s%c%c%c%c%s", COLINVCHAR,
           ((ucs / 4096) & 0xf) <= 9 ? '0' + ((ucs / 4096) & 0xf) : 'a' - 10 + ((ucs / 4096) & 0xf),
           ((ucs /  256) & 0xf) <= 9 ? '0' + ((ucs /  256) & 0xf) : 'a' - 10 + ((ucs /  256) & 0xf),
           ((ucs /   16) & 0xf) <= 9 ? '0' + ((ucs /   16) & 0xf) : 'a' - 10 + ((ucs /   16) & 0xf),
           ( ucs         & 0xf) <= 9 ? '0' + ( ucs         & 0xf) : 'a' - 10 + ( ucs         & 0xf),
           COLNONE);
        *columns = 4;
    }
    else
    {
        *display = ConvTo (ConvUTF8 (ucs), ENC(enc_loc))->txt;
        *columns = 1; /* FIXME: combining marks, double-width characters, undisplayable stuff, ... */
    }
}

/*
 * Insert a given unicode codepoint
 */
static void rl_insert (UWORD ucs)
{
    const char *display;
    UWORD columns;

    rl_analyze_ucs (ucs, &display, &columns);
    rl_insert_basic (ucs, display, strlen (display), columns);
    if (columns && (rl_ucscol.len > rl_ucspos))
        rl_recheck (FALSE);
}

/*
 * Delete a glyph
 */
static int rl_delete (void)
{
    if (rl_ucspos >= rl_ucscol.len)
        return FALSE;
    
    s_deln (&rl_display,  rl_bytepos, rl_ucsbytes.txt[rl_ucspos]);
    s_delc (&rl_ucscol,   rl_ucspos);
    s_delc (&rl_ucsbytes, rl_ucspos);
    s_delc (&rl_ucs,  2 * rl_ucspos);
    s_delc (&rl_ucs,  2 * rl_ucspos);
    
    while (!rl_ucscol.txt[rl_ucspos] && rl_ucspos < rl_ucscol.len)
    {
        s_deln (&rl_display,  rl_bytepos, rl_ucsbytes.txt[rl_ucspos]);
        s_delc (&rl_ucscol,   rl_ucspos);
        s_delc (&rl_ucsbytes, rl_ucspos);
        s_delc (&rl_ucs,  2 * rl_ucspos);
        s_delc (&rl_ucs,  2 * rl_ucspos);
    }
    return TRUE;
}

/*
 * Go an amount of glyphs to the left
 */
static int rl_left (UDWORD i)
{
    int gpos;

    if (!rl_ucspos)
        return FALSE;
    
    gpos = rl_colpos;
    for ( ; i > 0; i--)
    {
        while (rl_ucspos > 0 && (!rl_ucscol.txt[rl_ucspos - 1] || 
               (rl_ucs.txt[2 * rl_ucspos - 2] == -1 && rl_ucs.txt[2 * rl_ucspos - 1] == -1)))
            gpos -= rl_ucscol.txt[--rl_ucspos];
        if (rl_ucspos > 0)
            gpos -= rl_ucscol.txt[--rl_ucspos];
    }
    rl_goto (gpos);
    return TRUE;
}

/*
 * Go an amount of glyphs to the right
 */
static int rl_right (UDWORD i)
{
    int gpos;

    if (rl_ucspos >= rl_ucscol.len)
        return FALSE;
    
    gpos = rl_colpos;
    for ( ; i > 0; i--)
    {
        while (rl_ucspos < rl_ucscol.len && (!rl_ucscol.txt[rl_ucspos] || 
               (rl_ucs.txt[2 * rl_ucspos] != -1 && rl_ucs.txt[2 * rl_ucspos + 1] == -1)))
            gpos += rl_ucscol.txt[rl_ucspos++];
        if (rl_ucspos < rl_ucscol.len)
            gpos += rl_ucscol.txt[rl_ucspos++];
    }
    rl_goto (gpos);
    return TRUE;
}


/*** history processing ***/

/*
 * Compresses part of the current edited line into an UTF8 string
 */
static void rl_linecompress (str_t line, UDWORD from, UDWORD to)
{
    UDWORD i, ucs;
    
    if (to + 1 == 0)
        to = rl_ucscol.len;
    s_init (line, "", 0);
    for (i = from; i < to; i++)
        if ((ucs = ((UBYTE)rl_ucs.txt[2 * i + 1]) | (((UBYTE)rl_ucs.txt[2 * i]) << 8)) != 0x0000ffffL)
            s_cat (line, ConvUTF8 (ucs));
}

/*
 * Expand given UTF8 string into (and replace) editing line
 */
static void rl_lineexpand (char *hist)
{
    str_s str = { NULL, 0, 0 };
    strc_t line;
    UDWORD i;

    s_init (&rl_ucs, "", 0);
    s_init (&rl_ucscol, "", 0);
    s_init (&rl_ucsbytes, "", 0);
    s_init (&rl_display, "", 0);
    rl_colpos = rl_ucspos = rl_bytepos = 0;
    s_init (&str, "", 0);
    line = ConvTo (hist, ENC_UCS2BE);
    s_catn (&str, line->txt, line->len);
    for (i = 0; 2 * i + 1 < str.len; i++)
        rl_insert (((UBYTE)str.txt[2 * i + 1]) | (((UBYTE)str.txt[2 * i]) << 8));
    s_done (&str);
}

/*
 * Go back one line in history
 */
static void rl_historyback ()
{
    if (rl_history_pos == RL_HISTORY_LINES || !rl_history[rl_history_pos + 1])
        return;
    if (!rl_history_pos)
    {
        s_free (rl_history[0]);
        rl_linecompress (&rl_temp, 0, -1);
        rl_history[0] = strdup (rl_temp.txt);
    }
    rl_key_kill ();
    rl_history_pos++;
    rl_lineexpand (rl_history[rl_history_pos]);
}

/*
 * Go forward one line in history
 */
void rl_historyforward ()
{
    if (!rl_history_pos)
        return;
    rl_key_kill ();
    rl_history_pos--;
    rl_lineexpand (rl_history[rl_history_pos]);
}

/*
 * Add current line to history
 */
static void rl_historyadd ()
{
    int i, j;

    rl_linecompress (&rl_temp, 0, -1);
    if (rl_temp.txt[0])
    {
        for (j = 1; j <= RL_HISTORY_LINES; j++)
            if (!rl_history[j] || !strcmp (rl_temp.txt, rl_history[j]))
                break;
        if (j > RL_HISTORY_LINES)
            j = RL_HISTORY_LINES;
        s_free (rl_history[j]);
        s_free (rl_history[0]);
        rl_history[0] = strdup (rl_temp.txt);
        for (i = j; i > 0; i--)
            rl_history[i] = rl_history[i - 1];
        rl_history[0] = NULL;
    }
    s_init (&rl_ucs, "", 0);
    s_init (&rl_ucscol, "", 0);
    s_init (&rl_ucsbytes, "", 0);
    s_init (&rl_display, "", 0);
    rl_colpos = rl_ucspos = rl_bytepos = 0;
    rl_inputdone = 1;
    rl_history_pos = 0;
}

/*** tab handling ***/

/*
 * Fetch next tab contact
 */
static const Contact *rl_tab_getnext (strc_t common)
{
    const Contact *cont;
    
    switch (rl_tab_state)
    {
        case 1:
            while ((cont = TabGetOut (rl_tab_index++)))
                if (!strncasecmp (cont->nick, common->txt, common->len))
                    return cont;
            rl_tab_index = 0;
            rl_tab_state++;
        case 2:
            while ((cont = TabGetIn (rl_tab_index++)))
                if (!strncasecmp (cont->nick, common->txt, common->len))
                    return cont;
            rl_tab_index = 0;
            rl_tab_state++;
        case 3:
            while ((cont = ContactIndex (NULL, rl_tab_index++)))
                if (!strncasecmp (cont->nick, common->txt, common->len)
                    && cont->status != STATUS_OFFLINE && !TabHas (cont))
                    return cont;
            rl_tab_index = 0;
            rl_tab_state++;
        case 4:
            while ((cont = ContactIndex (NULL, rl_tab_index++)))
                if (!strncasecmp (cont->nick, common->txt, common->len)
                    && cont->status == STATUS_OFFLINE && !TabHas (cont))
                    return cont;
            rl_tab_index = 0;
            rl_tab_state = 1;
            while ((cont = TabGetOut (rl_tab_index++)))
                if (!strncasecmp (cont->nick, common->txt, common->len))
                    return cont;
            rl_tab_index = 0;
            rl_tab_state++;
            while ((cont = TabGetIn (rl_tab_index++)))
                if (!strncasecmp (cont->nick, common->txt, common->len))
                    return cont;
            rl_tab_index = 0;
            rl_tab_state++;
            while ((cont = ContactIndex (NULL, rl_tab_index++)))
                if (!strncasecmp (cont->nick, common->txt, common->len)
                    && cont->status != STATUS_OFFLINE && !TabHas (cont))
                    return cont;
            rl_tab_index = 0;
            rl_tab_state++;
            while ((cont = ContactIndex (NULL, rl_tab_index++)))
                if (!strncasecmp (cont->nick, common->txt, common->len)
                    && cont->status == STATUS_OFFLINE && !TabHas (cont))
                    return cont;
            return NULL;
    }
    assert (0);
}

/*
 * Accept the current selected contact
 */
static void rl_tab_accept (void)
{
    str_s inss = { NULL, 0, 0 };
    strc_t ins;
    const char *display;
    UDWORD i;
    UWORD columns;
    
    if (!rl_tab_state)
       return;
    rl_left (rl_tab_common);
    for ( ; rl_tab_len; rl_tab_len--)
        rl_delete ();
    ins = ConvTo (rl_tab_cont->nick, ENC_UCS2BE);
    s_init (&inss, "", 0);
    s_catn (&inss, ins->txt, ins->len);
    for (i = 0; i < inss.len; i += 2)
    {
        UWORD ucs = ((UBYTE)inss.txt[i + 1]) | (((UBYTE)inss.txt[i]) << 8);
        rl_analyze_ucs (ucs, &display, &columns);
        rl_insert_basic (ucs, display, strlen (display), columns);
    }
    rl_tab_state = 0;
    s_done (&inss);
    rl_recheck (TRUE);
}

/*
 * Cancel the current tab contact
 */
static void rl_tab_cancel (void)
{
    str_s inss = { NULL, 0, 0 };
    strc_t ins;
    const char *display;
    UDWORD i;
    UWORD columns;
    
    if (!rl_tab_state)
       return;
    rl_left (rl_tab_common);
    for ( ; rl_tab_len; rl_tab_len--)
        rl_delete ();
    ins = ConvTo (rl_tab_cont->nick, ENC_UCS2BE);
    s_init (&inss, "", 0);
    s_catn (&inss, ins->txt, ins->len);
    for (i = 0; i < rl_tab_common; i++)
    {
        UWORD ucs = ((UBYTE)inss.txt[2 * i + 1]) | (((UBYTE)inss.txt[2 * i]) << 8);
        rl_analyze_ucs (ucs, &display, &columns);
        rl_insert_basic (ucs, display, strlen (display), columns);
    }
    rl_tab_state = 0;
    s_done (&inss);
    rl_recheck (TRUE);
}

/*
 * Handle tab key - start or continue tabbing
 */
static void rl_key_tab (void)
{
    str_s inss = { NULL, 0, 0 };
    strc_t ins;
    const char *display;
    UDWORD i;
    UWORD columns;

    if (!rl_tab_state)
    {
        if (!rl_ucs.len)
        {
            rl_insert ('m');
            rl_insert ('s');
            rl_insert ('g');
            rl_insert (' ');
        }
        for (i = 0; i < rl_ucspos; i++)
        {
            if (rl_ucs.txt[2 * i] == 0 && rl_ucs.txt[2 * i + 1] == ' ')
            {
                rl_tab_index = 0;
                rl_tab_state = 1;
                rl_tab_pos = i + 1;
                rl_linecompress (&rl_temp, rl_tab_pos, rl_ucspos);
                if ((rl_tab_cont = rl_tab_getnext (&rl_temp)))
                {
                    ins = ConvTo (COLQUOTE, ENC(enc_loc));
                    s_init (&rl_colon, "", 0);
                    s_catn (&rl_colon, ins->txt, ins->len);
                    ins = ConvTo (COLNONE, ENC(enc_loc));
                    s_init (&rl_coloff, "", 0);
                    s_catn (&rl_coloff, ins->txt, ins->len);

                    while (rl_ucspos > rl_tab_pos)
                        rl_left (1), rl_tab_len++;
                    rl_tab_common = rl_tab_len;
                    break;
                }
            }
        }
        if (!rl_tab_state)
        {
            printf ("\a");
            return;
        }
    }
    else
    {
        rl_linecompress (&rl_temp, rl_tab_pos, rl_ucspos);
        rl_tab_cont = rl_tab_getnext (&rl_temp);
        rl_left (rl_tab_common);
    }
    for ( ; rl_tab_len; rl_tab_len--)
        rl_delete ();
    if (!rl_tab_cont)
    {
        printf ("\a");
        rl_tab_state = 0;
        rl_recheck (TRUE);
        return;
    }
    ins = ConvTo (rl_tab_cont->nick, ENC_UCS2BE);
    s_init (&inss, "", 0);
    s_catn (&inss, ins->txt, ins->len);
    for (i = 0; i < inss.len; i += 2)
    {
        UWORD ucs = ((UBYTE)inss.txt[i + 1]) | (((UBYTE)inss.txt[i]) << 8);
        rl_analyze_ucs (ucs, &display, &columns);
        rl_insert_basic (ucs, s_sprintf ("%s%s%s", rl_colon.txt, display, rl_coloff.txt),
                         strlen (display) + rl_colon.len + rl_coloff.len, columns);
        rl_tab_len++;
    }
    rl_left (rl_tab_len - rl_tab_common);
    s_done (&inss);
    rl_recheck (TRUE);
}

/*** key handlers ***/

/*
 * Handle key to insert itself
 */
static void rl_key_insert (UWORD ucs)
{
    if (rl_tab_state && rl_tab_common)
    {
        rl_tab_cancel ();
        rl_insert (ucs);
        rl_key_tab ();
    }
    else
    {
        if (rl_tab_state)
        {
            rl_tab_accept ();
            rl_insert (' ');
        }
        rl_insert (ucs);
    }
}

/*
 * Handle left arrow key
 */
static void rl_key_left (void)
{
    if (!rl_ucspos)
        return;

    if (rl_tab_state)
    {
        if (rl_tab_common)
            rl_tab_common--;
        else
            rl_tab_cancel();
    }
    rl_left (1);
}

/*
 * Handle right arrow key
 */
static void rl_key_right (void)
{
    if (rl_ucspos >= rl_ucscol.len)
        return;

    if (rl_tab_state)
    {
        if (rl_tab_common < rl_tab_len)
            rl_tab_common++;
        else
            rl_tab_accept();
    }
    rl_right (1);
}

/*
 * Handle delete key
 */
static void rl_key_delete (void)
{
    if (rl_tab_state)
        rl_tab_cancel ();
    rl_delete ();
    rl_recheck (TRUE);
}

/*
 * Handle backspace key
 */
static void rl_key_backspace (void)
{
    if (rl_tab_state)
        rl_tab_cancel ();
    if (rl_left (1))
        rl_key_delete ();
}

/*
 * Handle end key
 */
static void rl_key_end (void)
{
    int gpos;

    if (rl_tab_state)
        rl_tab_accept ();

    if (rl_ucspos >= rl_ucscol.len)
        return;
    
    gpos = rl_colpos;
    while (rl_ucspos < rl_ucscol.len)
        gpos += rl_ucscol.txt[rl_ucspos++];
    rl_goto (gpos);
}

/*
 * Handle key to cut and discard line after cursor
 */
static void rl_key_cut (void)
{
#ifdef ANSI_TERM
    while (rl_delete ())
        ;
    rl_recheck (TRUE);
#else
    while (rl_delete ())
        rl_recheck;
#endif
    rl_tab_state = 0;
}

/*
 * Handle kill character
 */
static void rl_key_kill (void)
{
    rl_goto (0);
    rl_linecompress (&rl_temp, 0, -1);
    s_repl (&rl_yank, rl_temp.txt);
    rl_key_cut ();
}

/*** read line input processor ***/

/*
 * Process one byte of input
 */
str_t ReadLine (UBYTE newbyte)
{
    strc_t input, inputucs;
    static UWORD ucsesc;
    UWORD ucs;

    rl_checkcolumns ();
    s_catc (&rl_input, newbyte);
    
    input = ConvFrom (&rl_input, prG->enc_loc);
    if (input->txt[0] == CHAR_INCOMPLETE)
    {
        if (strcmp (rl_input.txt, ConvTo (input->txt, prG->enc_loc)->txt))
            return NULL;
    }
    
    rl_inputdone = 0;
    rl_interrupted = 0;
    inputucs = ConvTo (input->txt, ENC_UCS2BE);
    s_init (&rl_input, "", 0);
    s_init (&rl_operate, "", 0);
    
    ReadLinePrompt ();
    ucs = ((UBYTE)inputucs->txt[1])  | (((UBYTE)inputucs->txt[0]) << 8);
    
    switch (rl_stat)
    {
        case 0:
#if HAVE_TCGETATTR
#if defined(VERASE)
            if (ucs == tty_attr.c_cc[VERASE] && tty_attr.c_cc[VERASE] != _POSIX_VDISABLE)
                rl_key_backspace ();
#endif
#if defined(VEOF)
            else if (ucs == tty_attr.c_cc[VEOF] && tty_attr.c_cc[VEOF] != _POSIX_VDISABLE)
            {
                rl_tab_cancel ();
                if (rl_ucscol.len)
                {
                    rl_key_left ();
                    rl_key_delete ();
                }
                else
                {
                    rl_insert ('q');
                    rl_historyadd ();
                }
            }
#endif
#if defined(VKILL)
            else if (ucs == tty_attr.c_cc[VKILL] && tty_attr.c_cc[VKILL] != _POSIX_VDISABLE)
                rl_key_kill ();
#endif
#if defined(VREPRINT)
            else if (ucs == tty_attr.c_cc[VREPRINT] && tty_attr.c_cc[VREPRINT] != _POSIX_VDISABLE)
                ReadLinePromptHide ();
#endif
#endif
            else switch (ucs)
            {
                case 1:              /* ^A */
                    rl_tab_cancel ();
                    rl_goto (0);
                    break;
                case 5:              /* ^E */
                    rl_key_end ();
                    break;
                case 8:              /* ^H = \b */
                    rl_key_backspace ();
                    break;
                case 9:              /* ^I = \t */
                    rl_key_tab ();
                    break;
                case 11:             /* ^K */
                    rl_key_cut ();
                    break;
                case 12:             /* ^L */
                    ReadLineClrScr ();
                    break;
                case '\r':
                case '\n':
                    rl_tab_accept ();
                    rl_key_end ();
                    rl_historyadd ();
                    break;
                case 25:             /* ^Y */
                    rl_tab_state = 0;
                    rl_goto (0);
                    rl_lineexpand (rl_yank);
                    break;
                case 27:             /* ^[ = ESC */
#ifdef ANSI_TERM
                    rl_stat = 1;
#endif
                    break;
                case 32:             /*   = SPACE */
                    if (rl_tab_state)
                        rl_tab_accept ();
                    rl_insert (' ');
                    break;
                case 127:            /* DEL */
                    if (prG->flags & FLAG_DELBS)
                        rl_key_backspace ();
                    else
                        rl_key_delete ();
                    break;
                case 0x9b:           /* CSI */
#ifdef ANSI_TERM
                    if (ENC(enc_loc) == ENC_LATIN1)
                    {
                        rl_stat = 2;
                        break;
                    }
#else
                    printf ("\a");
                    break;
#endif
                    /* fall-through */
                default:
                    rl_key_insert (ucs);
            }
            break;

#ifdef ANSI_TERM

        case 1: /* state 1: ESC was pressed */
            if (ucs == 'u' || ucs == 'U')
                rl_stat = 10;
            else if (ucs == '[' || ucs == 'O')
                rl_stat = 2;
            else
            {
                printf ("\a");
                rl_stat = 0;
            }
            break;
            
        case 2: /* state 2: CSI was typed */
            rl_stat = 0;
            switch (ucs)
            {
                case 'A':            /* up */
                    rl_tab_cancel ();
                    rl_historyback ();
                    break;
                case 'B':            /* down */
                    rl_tab_cancel ();
                    rl_historyforward ();
                    break;
                case 'C':            /* right */
                    rl_key_right ();
                    break;
                case 'D':            /* left */
                    rl_key_left ();
                    break;
                case 'H':            /* home */
                    rl_tab_cancel ();
                    rl_goto (0);
                    break;
                case 'F':            /* end */
                    rl_key_end ();
                    break;
                case '3':            /* + ~ = delete */
                    rl_stat = 3;
                    break;
                case '1':            /* + ~ = home */
                case '7':            /* + ~ = home */
                    rl_stat = 4;
                    break;
                case '4':            /* + ~ = end */
                case '8':            /* + ~ = end */
                    rl_stat = 5;
                    break;
                default:
                    printf ("\a");
            }
            break;
        
        case 3: /* state 3: incomplete delete key */
            rl_stat = 0;
            if (ucs == '~')
                rl_key_delete ();
            else
                printf ("\a");
            break;
        
        case 4: /* state 4: incomplete home key */
            rl_stat = 0;
            if (ucs == '~')
            {
                rl_tab_cancel ();
                rl_goto (0);
            }
            else
                printf ("\a");
            break;
        
        case 5: /* state 5: incomplete end key */
            rl_stat = 0;
            if (ucs == '~')
                rl_key_end ();
            else
                printf ("\a");
            break;
        
        case 10: /* state 10: unicode sequence to be entered */
             rl_stat++;
             if (ucs >= '0' && ucs <= '9')
                 ucsesc = ucs - '0';
             else if (ucs >= 'a' && ucs <= 'f')
                 ucsesc = ucs - 'a' + 10;
             else if (ucs >= 'A' && ucs <= 'F')
                 ucsesc = ucs - 'A' + 10;
             else
             {
                 rl_stat = 0;
                 printf ("\a");
             }
             break;
         case 11:
             rl_stat++;
             ucsesc <<= 4;
             if (ucs >= '0' && ucs <= '9')
                 ucsesc |= ucs - '0';
             else if (ucs >= 'a' && ucs <= 'f')
                 ucsesc |= ucs - 'a' + 10;
             else if (ucs >= 'A' && ucs <= 'F')
                 ucsesc |= ucs - 'A' + 10;
             else
             {
                 rl_stat = 0;
                 printf ("\a");
             }
             break;
         case 12:
             rl_stat++;
             ucsesc <<= 4;
             if (ucs >= '0' && ucs <= '9')
                 ucsesc |= ucs - '0';
             else if (ucs >= 'a' && ucs <= 'f')
                 ucsesc |= ucs - 'a' + 10;
             else if (ucs >= 'A' && ucs <= 'F')
                 ucsesc |= ucs - 'A' + 10;
             else
             {
                 rl_stat = 0;
                 printf ("\a");
             }
             break;
         case 13:
             rl_stat = 0;
             ucsesc <<= 4;
             if (ucs >= '0' && ucs <= '9')
                 ucsesc |= ucs - '0';
             else if (ucs >= 'a' && ucs <= 'f')
                 ucsesc |= ucs - 'a' + 10;
             else if (ucs >= 'A' && ucs <= 'F')
                 ucsesc |= ucs - 'A' + 10;
             else
             {
                 printf ("\a");
                 break;
             }
             rl_key_insert (ucsesc);
             break;
    }
#endif
    
    if (rl_operate.len)
        printf ("%s", rl_operate.txt);
    
    if (!rl_inputdone)
        return NULL;

    printf ("\n");
    return &rl_temp;
}

/*** prompt management ***/

/*
 * Shows the prompt
 */
void ReadLinePrompt ()
{
    int gpos = rl_colpos;

    if (rl_prompt_stat == 1)
        return;
    s_init (&rl_operate, "", 0);
    if (rl_prompt_stat == 2)
        rl_goto (0);
    if (rl_prompt_stat == 2)
    {
        s_catc (&rl_operate, '\r');
#ifdef ANSI_TERM
        s_cat (&rl_operate, ESC "[J");
#endif
        rl_prompt_stat = 0;
    }
    printf ("%s", rl_operate.txt);
    if (rl_prompt_stat == 0)
    {
        M_print ("\r");
        M_print (rl_prompt.txt);
        rl_prompt_len = M_pos ();
        rl_prompt_stat = 1;
        rl_colpos = 0;
    }
    s_init (&rl_operate, "", 0);
    rl_recheck (TRUE);
    rl_goto (gpos);
    printf ("%s", rl_operate.txt);
}

/*
 * Hides the prompt
 */
void ReadLinePromptHide ()
{
    int pos = rl_colpos;
    rl_checkcolumns ();
    if (rl_prompt_stat == 0)
        return;
    s_init (&rl_operate, "", 0);
    rl_goto (0);
    s_catc (&rl_operate, '\r');
#ifdef ANSI_TERM
    s_cat (&rl_operate, ESC "[J");
#endif
    printf ("%s", rl_operate.txt);
    rl_prompt_stat = 0;
    rl_colpos = pos;
}

/*
 * Sets the prompt
 */
void ReadLinePromptSet (const char *prompt)
{
    rl_prompt_time = time (NULL);
    s_init (&rl_prompt, COLSERVER, 0);
    s_cat  (&rl_prompt, ConvTo (prompt, ENC(enc_loc))->txt);
    s_cat  (&rl_prompt, COLNONE);
    s_catc (&rl_prompt, ' ');
    if (rl_prompt_stat != 0)
        rl_prompt_stat = 2;
}

/*
 * Updates prompt - ignores too frequent changes
 */
void ReadLinePromptUpdate (const char *prompt)
{
    if (rl_prompt_time == time (NULL))
        return;
    ReadLinePromptSet (prompt);
}

/*
 * Resets prompt to default prompt
 */
void ReadLinePromptReset (void)
{
    Contact *cont;
    if (prG->flags & FLAG_UINPROMPT && uiG.last_sent_uin &&
        (cont = ContactFind (NULL, 0, uiG.last_sent_uin, NULL)))
        ReadLinePromptSet (s_sprintf ("[%s]", cont->nick));
    else
        ReadLinePromptSet (i18n (9999, "mICQ>"));
}
