/*
 * Line editing code.
 *
 * mICQ Copyright (C) © 2001,2002,2003,2004 Rüdiger Kuhlmann
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
 * In addition, as a special exception permission is granted to link the
 * code of this release of mICQ with the OpenSSL project's "OpenSSL"
 * library, and distribute the linked executables.  You must obey the GNU
 * General Public License in all respects for all of the code used other
 * than "OpenSSL".  If you modify this file, you may extend this exception
 * to your version of the file, but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your version
 * of this file.
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

#include <stdarg.h>
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
#if HAVE_WCHAR_H
/* glibc sucks */
#define __USE_XOPEN 1
#include <wchar.h>
#endif
#if HAVE_WCTYPE_H
#include <wctype.h>
#else
#include <ctype.h>
#endif
#ifndef WEOF
#define WEOF (wchar_tt)-1
#endif

#undef DEBUG_RL

#if defined(_WIN32) && !defined(__CYGWIN__)
#define ANSI_CLEAR ""
#else
#define ANSI_CLEAR ESC "[J"
#endif


/* We'd like to use wint_t and wchar_t to store the input line,
   however not all C implementations store the unicode codepoint
   in wint_t/wchar_t, which kills the possibility to enter an
   arbitrary codepoint into the line. Thus, we end up using UDWORDS instead. */
#define wint_tt  UDWORD
#define wchar_tt UDWORD
#define rl_ucs_at(str,pos) *(wint_tt *)((str)->txt + sizeof (wint_tt) * (pos))

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
static void rl_insert_basic (wchar_tt ucs, const char *display, UDWORD len, UDWORD collen);
static void rl_analyze_ucs (wchar_tt ucs, const char **display, UWORD *columns);
static void rl_insert (wchar_tt ucs);
static int  rl_delete (void);
static int  rl_left (UDWORD i);
static int  rl_right (UDWORD i);
static const Contact *rl_tab_getnext (strc_t common);
static const Contact *rl_tab_getprev (strc_t common);
static void rl_tab_accept (void);
static void rl_tab_cancel (void);
static void rl_key_tab (void);
static void rl_key_shifttab (void);
static void rl_key_insert (wchar_tt ucs);
static void rl_key_left (void);
static void rl_key_right (void);
static void rl_key_delete (void);
static void rl_key_backspace (void);
static void rl_key_delete_backward_word (void);
static void rl_key_backward_word (void);
static void rl_key_forward_word (void);
static void rl_key_end (void);
static void rl_key_cut (void);
static void rl_key_kill (void);
static void rl_linecompress (str_t line, UDWORD from, UDWORD to);
static void rl_lineexpand (char *hist);
static void rl_historyback (void);
static void rl_historyforward (void);
static void rl_historyadd (void);

#if defined(SIGWINCH)
static volatile int rl_columns_cur = 0;
#endif
volatile UBYTE rl_signal = 0;

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

static int    rl_tab_state = 0;  /* 0 = OFF 1 = Out 2 = Inc 3 = Online 4 = Offline |8 = second try -1 = disallow */
static UDWORD rl_tab_index = 0;  /* index in list */
static UDWORD rl_tab_len   = 0;  /* number of codepoints tabbed in */
static UDWORD rl_tab_pos   = 0;  /* start of word tabbed in */
static UDWORD rl_tab_common = 0; /* number of given codepoints */
static str_s  rl_colon      = { NULL, 0, 0 };
static str_s  rl_coloff     = { NULL, 0, 0 };
static const Contact *rl_tab_cont = NULL;
static const ContactAlias *rl_tab_alias = NULL;

static str_s  rl_prompt   = { NULL, 0, 0 };
static UWORD  rl_prompt_len = 0;
static int    rl_prompt_stat = 0;
static time_t rl_prompt_time = 0;

typedef struct rl_autoexpandv_s {
  struct rl_autoexpandv_s *next;
  char *command;
  char *replace;
} rl_autoexpandv_t;
      
static rl_autoexpandv_t *rl_ae = NULL;

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

#ifdef DEBUG_RL
/*
 * Debugging function
 */
static void rl_dump_line (void)
{
    int i, bp;
    fprintf (stderr, "colpos %ld ucspos %ld bytepos %ld\n", rl_colpos, rl_ucspos, rl_bytepos);
    for (i = bp = 0; i < rl_ucscol.len; i++)
    {
        char *p = strdup (rl_display.txt + bp);
        p[rl_ucsbytes.txt[i]] = 0;
        fprintf (stderr, " %08x %d/%d %s",
                 rl_ucs_at (&rl_ucs, i), rl_ucscol.txt[i], rl_ucsbytes.txt[i],
                 s_qquote (p));
        bp += rl_ucsbytes.txt[i];
        free (p);
    }
    fprintf (stderr, "\n");
}
#else
#define rl_dump_line()
#endif

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
        rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
        rl_printf (i18n (2524, "Can't restore terminal modes.\n"));
    }
    else
        tty_saved = 0;
#endif
}

/*
 * Change tty mode for mICQ
 */
void ReadLineTtySet (void)
{
#if HAVE_TCGETATTR
    if (tcgetattr (STDIN_FILENO, &tty_attr) != 0)
    {
        rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
        rl_printf (i18n (2525, "Can't read terminal modes.\n"));
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
        rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
        rl_printf (i18n (2526, "Can't modify terminal modes.\n"));
        return;
    }
    tty_attr.c_cc[VMIN] = 1;
    if (tcsetattr (STDIN_FILENO, TCSAFLUSH, &tty_attr) != 0)
    {
        rl_printf ("%s%s%s ", COLERROR, i18n (1619, "Warning:"), COLNONE);
        rl_printf (i18n (2526, "Can't modify terminal modes.\n"));
    }
#endif
}

#if defined(SIGWINCH)
/*
 * Handle a window resize
 */
static RETSIGTYPE tty_sigwinch_handler (int i)
{
    rl_signal |= 16;
}
#endif

#if defined(SIGTSTP) && defined(SIGCONT)
/*
 * Handle a ^Z terminal stop signal
 */
static RETSIGTYPE tty_stop_handler (int i)
{
    rl_signal |= 8;
}

/*
 * Handle continuation after a ^Z terminal stop
 */
static RETSIGTYPE tty_cont_handler (int i)
{
    rl_signal |= 4;
    signal (SIGTSTP, &tty_stop_handler);
    signal (SIGCONT, &tty_cont_handler);
}
#endif

/*
 * Handle a ^C interrupt signal - push line into history
 * or exit after two consecutive ^C
 */
static RETSIGTYPE tty_int_handler (int i)
{
    signal (SIGINT, SIG_IGN);

    if (rl_signal & 1)
    {
        printf (" %s:-X%s\n", COLERROR, COLNONE);
        fflush (stdout);
        exit (1);
    }
    rl_signal |= 3;
    signal (SIGINT, &tty_int_handler);
}

/*
 * Really handle signals
 */
void ReadLineHandleSig (void)
{
    UBYTE sig;
    
    while ((sig = (rl_signal & 30)))
    {
        rl_signal &= 1;
        if (sig & 2)
        {
            s_init (&rl_operate, "", 0);
            rl_key_end ();
            printf ("%s %s^C%s\n", rl_operate.txt, COLERROR, COLNONE);
            rl_print ("\r");
            rl_prompt_stat = 0;
            rl_historyadd ();
            rl_tab_state = 0;
            s_init (&rl_input, "", 0);
            CmdUserInterrupt ();
            ReadLinePromptReset ();
            ReadLinePrompt ();
        }
#if defined(SIGTSTP) && defined(SIGCONT)
        if (sig & 4)
        {
            ReadLineTtySet ();
            rl_print ("\r");
            ReadLinePrompt ();
        }
        if (sig & 8)
        {
            int gpos;

            s_init (&rl_operate, "", 0);
            gpos = rl_colpos;
            rl_key_end ();
            rl_colpos = gpos;
            printf ("%s", rl_operate.txt);
            printf (" %s^Z%s", COLERROR, COLNONE);
            ReadLineTtyUnset ();
            signal (SIGTSTP, SIG_DFL);
            raise (SIGTSTP);
        }
#endif
#if defined(SIGWINCH)
        if (sig & 16)
        {
            if (rl_columns != rl_getcolumns ())
            {
                ReadLinePromptHide ();
                rl_columns = rl_getcolumns ();
            }
        }
#endif
    }
}

void ReadLineAllowTab (UBYTE onoff)
{
    if (!onoff == (rl_tab_state == -1))
        return;

    switch (rl_tab_state)
    {
        case -1:
            rl_tab_state = 0;
            break;
        default:
            rl_tab_cancel ();
        case 0:
            rl_tab_state = -1;
    }
}

/*
 * Fetch the number of columns of this terminal
 */
static int rl_getcolumns ()
{
#if defined(SIGWINCH)
    struct winsize ws;
    int width = rl_columns_cur;

    rl_columns_cur = 0;
    ioctl (STDIN_FILENO, TIOCGWINSZ, &ws);
    rl_columns_cur = ws.ws_col;
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
 * Clear the screen
 */
void ReadLineClrScr ()
{
#ifdef ANSI_TERM
    printf (ESC "[H" ANSI_CLEAR);
#else
    printf ("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
#endif
    rl_prompt_stat = 0;
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
    if (!((rl_prompt_len + rl_colpos) % rl_columns))
    {
        if (rl_ucspos < rl_ucscol.len)
            s_catn (&rl_operate, rl_display.txt + rl_bytepos, rl_ucsbytes.txt[rl_ucspos]);
        else
            s_catc (&rl_operate, ' ');
        s_catc (&rl_operate, '\r');
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
        if (rl_ucs_at (&rl_ucs, rl_ucspos) == WEOF)
        {
            s_deln (&rl_ucs, sizeof (wint_tt) * rl_ucspos, sizeof (wint_tt));
            s_delc (&rl_ucsbytes, rl_ucspos);
            s_delc (&rl_ucscol, rl_ucspos);
            s_delc (&rl_display, rl_bytepos);
        }
        else if (((rl_prompt_len + rl_colpos) % rl_columns)
                 + (UBYTE)rl_ucscol.txt[rl_ucspos] > rl_columns)
        {
            for (i = (rl_columns - ((rl_prompt_len + rl_colpos) % rl_columns)); i > 0; i--)
            {
                wint_tt weof = WEOF;
                s_insn (&rl_ucs, sizeof (wint_tt) * rl_ucspos, (const char *)&weof, sizeof (wint_tt));
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
        s_cat (&rl_operate, ANSI_CLEAR);
#else
    s_cat (&rl_operate, "     \b\b\b\b\b");
#endif
    rl_goto (gpos);
}

/*
 * Insert a character by unicode codepoint, display string in local encoding
 * and its length, and its width in columns
 */
static void rl_insert_basic (wchar_tt ucs, const char *display, UDWORD len, UDWORD collen)
{
    int i;
    
    if (((rl_prompt_len + rl_colpos) % rl_columns) + collen > rl_columns)
    {
        for (i = (rl_columns - ((rl_prompt_len + rl_colpos) % rl_columns)); i > 0; i--)
        {
            wint_tt weof = WEOF;
            s_insn (&rl_ucs, sizeof (wint_tt) * rl_ucspos, (const char *)&weof, sizeof (wint_tt));
            s_insc (&rl_ucscol, rl_ucspos, 1);
            s_insc (&rl_ucsbytes, rl_ucspos++, 1);
            s_insc (&rl_display, rl_bytepos++, ' ');
            s_catc (&rl_operate, ' ');
            rl_colpos++;
        }
    }
    
    s_insn (&rl_ucs, sizeof (wint_tt) * rl_ucspos, (const char *)&ucs, sizeof (wint_tt));
    s_insc (&rl_ucscol, rl_ucspos, collen);
    s_insc (&rl_ucsbytes, rl_ucspos++, len);
    s_insn (&rl_display, rl_bytepos, display, len);
    rl_colpos += collen;
    rl_bytepos += len;
    s_cat (&rl_operate, display);
}

#if HAVE_WCWIDTH
/*
 * Determine width from wcwidth()
 */
static int rl_wcwidth (wint_tt ucs, const char *display)
{
    wchar_t wc[20]; /* NOT wchar_tt */
    int i, l, b, w;
    
    if (ucs != CHAR_NOT_AVAILABLE && *display == CHAR_NOT_AVAILABLE)
        return -1;

    if ((l = mbstowcs (wc, display, 20)) <= 0)
        return -1;
    
    for (i = w = 0; i < l; i++)
        if (!iswprint (wc[i]) || ((b = wcwidth (wc[i])) < 0))
            return -1;
        else
            w += b;
    return w;
}
#endif

#define rl_hex(u) ((u) & 15) <= 9 ? '0' + (char)((u) & 15) : 'a' - 10 + (char)((u) & 15)

/*
 * Determine for a character is display string and columns width
 */
static void rl_analyze_ucs (wint_tt ucs, const char **display, UWORD *columns)
{
   char *utf = NULL;
#if HAVE_WCWIDTH
   int width;
#endif
  
   utf = strdup (ConvUTF8 (ucs));
   *display = ConvTo (utf, ENC(enc_loc))->txt;

    if (ucs < 32 || ucs == 127 || ucs == 173) /* control code */
    {
        *display = s_sprintf ("%s^%c%s", COLINVCHAR, (char)((ucs - 1 + 'A') & 0x7f), COLNONE);
        *columns = 2;
    }
    else if (ucs < 127)
        *columns = 1;
#if HAVE_WCWIDTH
    else if (!prG->locale_broken && (width = rl_wcwidth (ucs, *display)) != -1)
        *columns = 256 | width;
#else
    else if (ucs >= 160) /* no way I'll hard-code double-width or combining marks here */
        *columns = 257;
#endif
    else if (prG->locale_broken && !strcmp (ConvFrom (ConvTo (utf, ENC(enc_loc)), ENC(enc_loc))->txt, utf))
        *columns = 257;
    else if (!(ucs & 0xffff0000)) /* more control code, or unknown */
    {
        *display = s_sprintf ("%s\\u%c%c%c%c%s", COLINVCHAR,
           rl_hex (ucs / 4096), rl_hex (ucs /  256),
           rl_hex (ucs /   16), rl_hex (ucs),
           COLNONE);
        *columns = 6;
    }
    else /* unknown stellar planes */
    {
        *display = s_sprintf ("%s\\U%c%c%c%c%c%c%c%c%s", COLINVCHAR,
           rl_hex (ucs / 268435456), rl_hex (ucs /  16777216),
           rl_hex (ucs /   1048576), rl_hex (ucs /     65536),
           rl_hex (ucs /      4096), rl_hex (ucs /       256),
           rl_hex (ucs /        16), rl_hex (ucs),
           COLNONE);
        *columns = 10;
    }
    free (utf);
}

/*
 * Determine width and correct display for given (UTF8) string
 */
strc_t ReadLineAnalyzeWidth (const char *text, UWORD *width)
{
    static str_s str = { NULL, 0, 0 };
    wchar_tt ucs;
    UWORD twidth, swidth = 0;
    const char *dis;
    int off = 0;
    str_s in;
    
    in.txt = (char *)text;
    in.len = strlen (text);
    in.max = 0;
    s_init (&str, "", 100);
    
    for (off = 0; off < in.len; )
    {
        ucs = ConvGetUTF8 (&in, &off);
        rl_analyze_ucs (ucs, &dis, &twidth);
        swidth += twidth & 0xff;
        s_cat (&str, twidth & 0x100 ? ConvUTF8 (ucs) : dis);
    }
    *width = swidth;
    return &str;
}

const char *ReadLinePrintWidth (const char *text, const char *left, const char *right, UWORD *width)
{
    UWORD pwidth = *width, twidth;
    strc_t txt;
    
    txt = ReadLineAnalyzeWidth (text, &twidth);
    if (twidth > pwidth)
        *width = pwidth = twidth;
    return s_sprintf ("%s%s%s%*.*s", left, txt->txt, right, pwidth - twidth, pwidth - twidth, "");
}


/*
 * Insert a given unicode codepoint
 */
static void rl_insert (wint_tt ucs)
{
    const char *display;
    UWORD columns;

    rl_analyze_ucs (ucs, &display, &columns);
    rl_insert_basic (ucs, display, strlen (display), columns & 0xff);
    if (columns && ((rl_ucscol.len > rl_ucspos) || !((rl_prompt_len + rl_colpos) % rl_columns)))
        rl_recheck (FALSE);
}

/*
 * Delete a glyph
 */
static int rl_delete (void)
{
    if (rl_ucspos >= rl_ucscol.len)
        return FALSE;
    
    while (rl_ucs_at (&rl_ucs, rl_ucspos) == WEOF)
    {
        s_deln (&rl_display,  rl_bytepos, rl_ucsbytes.txt[rl_ucspos]);
        s_delc (&rl_ucscol,   rl_ucspos);
        s_delc (&rl_ucsbytes, rl_ucspos);
        s_deln (&rl_ucs, sizeof (wint_tt) * rl_ucspos, sizeof (wint_tt));
    }

    s_deln (&rl_display,  rl_bytepos, rl_ucsbytes.txt[rl_ucspos]);
    s_delc (&rl_ucscol,   rl_ucspos);
    s_delc (&rl_ucsbytes, rl_ucspos);
    s_deln (&rl_ucs, sizeof (wint_tt) * rl_ucspos, sizeof (wint_tt));
    
    while (!rl_ucscol.txt[rl_ucspos] && rl_ucspos < rl_ucscol.len)
    {
        s_deln (&rl_display,  rl_bytepos, rl_ucsbytes.txt[rl_ucspos]);
        s_delc (&rl_ucscol,   rl_ucspos);
        s_delc (&rl_ucsbytes, rl_ucspos);
        s_deln (&rl_ucs, sizeof (wint_tt) * rl_ucspos, sizeof (wint_tt));
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
               (rl_ucs_at (&rl_ucs, rl_ucspos - 1) == WEOF)))
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
        if (rl_ucspos < rl_ucscol.len)
            gpos += rl_ucscol.txt[rl_ucspos++];
        while (rl_ucspos < rl_ucscol.len && (!rl_ucscol.txt[rl_ucspos] || 
               (rl_ucs_at (&rl_ucs, rl_ucspos) == WEOF)))
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
    UDWORD i;
    wint_tt ucs;
    
    if (to == (UDWORD)-1)
        to = rl_ucscol.len;
    s_init (line, "", 0);
    for (i = from; i < to; i++)
    {
        ucs = rl_ucs_at (&rl_ucs, i);
#if DEBUG_RL
        fprintf (stderr, "ucs %x\n", ucs);
#endif
        if (ucs != WEOF)
            s_cat (line, ConvUTF8 (ucs));
    }
#if DEBUG_RL
    fprintf (stderr, "compress %s\n", s_qquote (line->txt));
#endif
}

/*
 * Expand given UTF8 string into (and replace) editing line
 */
static void rl_lineexpand (char *hist)
{
    str_s str = { NULL, 0, 0 };
    int off;

    s_init (&rl_ucs, "", 0);
    s_init (&rl_ucscol, "", 0);
    s_init (&rl_ucsbytes, "", 0);
    s_init (&rl_display, "", 0);
    rl_colpos = rl_ucspos = rl_bytepos = 0;
    
    str.txt = hist;
    str.len = strlen (str.txt);
    for (off = 0; off < str.len; )
        rl_insert (ConvGetUTF8 (&str, &off));
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
 * ALSO FINISHES LINE PROCESSING
 */
static void rl_historyadd ()
{
    int i, j;

    rl_linecompress (&rl_temp, 0, -1);
    if (rl_tab_state >= 0 && rl_temp.txt[0])
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
    
    rl_tab_state &= ~8;
    rl_tab_alias = NULL;
    while (1)
    {
        switch (rl_tab_state & 7)
        {
            case 1:
                while ((cont = TabGet (rl_tab_index++)))
                    if (!strncasecmp (cont->nick, common->txt, common->len))
                        return cont;
                rl_tab_index = 0;
                rl_tab_state++;
            case 2:
                while ((cont = ContactIndex (NULL, rl_tab_index++)))
                    if (cont->status != STATUS_OFFLINE && !TabHas (cont))
                    {
                        if (!strncasecmp (cont->nick, common->txt, common->len))
                            return cont;
                        for (rl_tab_alias = cont->alias; rl_tab_alias; rl_tab_alias = rl_tab_alias->more)
                            if (!strncasecmp (rl_tab_alias->alias, common->txt, common->len))
                                return cont;
                    }
                
                rl_tab_index = 0;
                rl_tab_state++;
            case 3:
                while ((cont = ContactIndex (NULL, rl_tab_index++)))
                    if (cont->status == STATUS_OFFLINE && !TabHas (cont))
                    {
                        if (!strncasecmp (cont->nick, common->txt, common->len))
                            return cont;
                        for (rl_tab_alias = cont->alias; rl_tab_alias; rl_tab_alias = rl_tab_alias->more)
                            if (!strncasecmp (rl_tab_alias->alias, common->txt, common->len))
                                return cont;
                    }
                if (rl_tab_state & 8)
                    return NULL;
                rl_tab_index = 0;
                rl_tab_state = 9;
                continue;
        }
        assert (0);
    }
}

/*
 * Fetch previous tab contact
 */
static const Contact *rl_tab_getprev (strc_t common)
{
    const Contact *cont;
    
    rl_tab_index--;
    rl_tab_state &= ~8;
    rl_tab_alias = NULL;
    while (1)
    {
        switch (rl_tab_state & 7)
        {
            case 3:
                while (rl_tab_index && (cont = ContactIndex (NULL, --rl_tab_index)))
                    if (cont->status == STATUS_OFFLINE && !TabHas (cont))
                    {
                        rl_tab_index++;
                        if (!strncasecmp (cont->nick, common->txt, common->len))
                            return cont;
                        for (rl_tab_alias = cont->alias; rl_tab_alias; rl_tab_alias = rl_tab_alias->more)
                            if (!strncasecmp (rl_tab_alias->alias, common->txt, common->len))
                                return cont;
                        rl_tab_index--;
                    }
                rl_tab_index = 0;
                while (ContactIndex (NULL, rl_tab_index))
                    rl_tab_index++;
                rl_tab_state--;
            case 2:
                while (rl_tab_index && (cont = ContactIndex (NULL, --rl_tab_index)))
                    if (cont->status != STATUS_OFFLINE && !TabHas (cont))
                    {
                        rl_tab_index++;
                        if (!strncasecmp (cont->nick, common->txt, common->len))
                            return cont;
                        for (rl_tab_alias = cont->alias; rl_tab_alias; rl_tab_alias = rl_tab_alias->more)
                            if (!strncasecmp (rl_tab_alias->alias, common->txt, common->len))
                                return cont;
                        rl_tab_index--;
                    }
                rl_tab_index = 0;
                while (TabGet (rl_tab_index))
                    rl_tab_index++;
                rl_tab_state--;
            case 1:
                while (rl_tab_index && (cont = TabGet (--rl_tab_index)))
                    if (!strncasecmp (cont->nick, common->txt, common->len))
                    {
                        rl_tab_index++;
                        return cont;
                    }
                if (rl_tab_state & 8)
                    return NULL;
                rl_tab_index = 0;
                while (ContactIndex (NULL, rl_tab_index))
                    rl_tab_index++;
                rl_tab_state = 8 + 3;
                continue;
        }
        assert (0);
    }
}

/*
 * Accept the current selected contact
 */
static void rl_tab_accept (void)
{
    str_s str = { NULL, 0, 0 };
    const char *display;
    int off;
    UWORD columns;
    
    if (rl_tab_state <= 0)
       return;
    rl_left (rl_tab_common);
    for ( ; rl_tab_len; rl_tab_len--)
        rl_delete ();
    str.txt = rl_tab_alias ? rl_tab_alias->alias : rl_tab_cont->nick;
    str.len = strlen (str.txt);
    for (off = 0; off < str.len; )
    {
        wint_tt ucs = ConvGetUTF8 (&str, &off);
        rl_analyze_ucs (ucs, &display, &columns);
        rl_insert_basic (ucs, display, strlen (display), columns & 0xff);
    }
    rl_tab_state = 0;
    rl_recheck (TRUE);
}

/*
 * Cancel the current tab contact
 */
static void rl_tab_cancel (void)
{
    str_s str = { NULL, 0, 0 };
    const char *display;
    int i, off;
    UWORD columns;
    
    if (rl_tab_state <= 0)
       return;
    rl_left (rl_tab_common);
    for ( ; rl_tab_len; rl_tab_len--)
        rl_delete ();
    str.txt = rl_tab_alias ? rl_tab_alias->alias : rl_tab_cont->nick;
    str.len = strlen (str.txt);
    for (off = i = 0; off < str.len && i < rl_tab_common; i++)
    {
        wint_tt ucs = ConvGetUTF8 (&str, &off);
        rl_analyze_ucs (ucs, &display, &columns);
        rl_insert_basic (ucs, display, strlen (display), columns & 0xff);
    }
    rl_tab_state = 0;
    rl_recheck (TRUE);
}

/*
 * Handle tab key - start or continue tabbing
 */
static void rl_key_tab (void)
{
    str_s str = { NULL, 0, 0 };
    strc_t ins;
    const char *display;
    int i, off;
    UWORD columns;

    if (rl_tab_state == -1)
    {
        rl_insert (9);
        return;
    }

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
            if (rl_ucs_at (&rl_ucs, i) == ' ')
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

    str.txt = rl_tab_alias ? rl_tab_alias->alias : rl_tab_cont->nick;
    str.len = strlen (str.txt);
    for (off = 0; off < str.len; )
    {
        wint_tt ucs = ConvGetUTF8 (&str, &off);
        rl_analyze_ucs (ucs, &display, &columns);
        rl_insert_basic (ucs, s_sprintf ("%s%s%s", rl_colon.txt, display, rl_coloff.txt),
                         strlen (display) + rl_colon.len + rl_coloff.len, columns & 0xff);
        rl_tab_len++;
    }
    rl_left (rl_tab_len - rl_tab_common);
    rl_recheck (TRUE);
}

/*
 * Handle tab key - continue tabbing backwards
 */
static void rl_key_shifttab (void)
{
    str_s str = { NULL, 0, 0 };
    const char *display;
    int off;
    UWORD columns;

    if (rl_tab_state <= 0)
    {
        printf ("\a");
        return;
    }

    rl_linecompress (&rl_temp, rl_tab_pos, rl_ucspos);
    rl_tab_cont = rl_tab_getprev (&rl_temp);
    rl_left (rl_tab_common);

    for ( ; rl_tab_len; rl_tab_len--)
        rl_delete ();
    if (!rl_tab_cont)
    {
        printf ("\a");
        rl_tab_state = 0;
        rl_recheck (TRUE);
        return;
    }

    str.txt = rl_tab_alias ? rl_tab_alias->alias : rl_tab_cont->nick;
    str.len = strlen (str.txt);
    for (off = 0; off < str.len; )
    {
        wint_tt ucs = ConvGetUTF8 (&str, &off);
        rl_analyze_ucs (ucs, &display, &columns);
        rl_insert_basic (ucs, s_sprintf ("%s%s%s", rl_colon.txt, display, rl_coloff.txt),
                         strlen (display) + rl_colon.len + rl_coloff.len, columns & 0xff);
        rl_tab_len++;
    }
    rl_left (rl_tab_len - rl_tab_common);
    rl_recheck (TRUE);
}

/*** key handlers ***/

/*
 * Handle key to insert itself
 */
static void rl_key_insert (wchar_tt ucs)
{
    if (rl_tab_state > 0 && rl_tab_common)
    {
        rl_tab_cancel ();
        rl_insert (ucs);
        rl_key_tab ();
    }
    else
    {
        if (rl_tab_state > 0)
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

    if (rl_tab_state > 0)
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

    if (rl_tab_state > 0)
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
    if (rl_tab_state > 0)
        rl_tab_cancel ();
    rl_delete ();
    rl_recheck (TRUE);
}

/*
 * Handle backspace key
 */
static void rl_key_backspace (void)
{
    if (rl_tab_state > 0)
        rl_tab_cancel ();
    if (rl_left (1))
        rl_key_delete ();
}

/*
 * Handle delete backward-word
 */
static void rl_key_delete_backward_word (void)
{
    if (!rl_ucspos)
        return;

    rl_key_left ();
    if (rl_ucspos > 0 && !iswalnum (rl_ucs_at (&rl_ucs, rl_ucspos)))
        while (rl_ucspos > 0 && !iswalnum (rl_ucs_at (&rl_ucs, rl_ucspos)))
        {
            rl_key_delete ();
            rl_key_left ();
        }
    if (!rl_ucspos)
    {
        rl_key_delete ();
        return;
    }
    while (rl_ucspos > 0 && iswalnum (rl_ucs_at (&rl_ucs, rl_ucspos)))
    {
        rl_key_delete ();
        rl_key_left ();
    }
    
    if (!iswalnum (rl_ucs_at (&rl_ucs, rl_ucspos)))
        rl_key_right ();
    else
        rl_key_delete ();
}

/*
 * Handle backward-word
 */
static void rl_key_backward_word (void)
{
    if (rl_ucspos > 0)
        rl_key_left ();
    if (rl_ucspos > 0 && !iswalnum (rl_ucs_at (&rl_ucs, rl_ucspos)))
        while (rl_ucspos > 0 && !iswalnum (rl_ucs_at (&rl_ucs, rl_ucspos)))
            rl_key_left ();
    while (rl_ucspos > 0 && iswalnum (rl_ucs_at (&rl_ucs, rl_ucspos)))
        rl_key_left ();
    if (!iswalnum (rl_ucs_at (&rl_ucs, rl_ucspos)))
        rl_key_right ();
}

/*
 * Handle forward-word
 */
static void rl_key_forward_word (void)
{
    if (rl_ucspos < rl_ucscol.len)
        rl_key_right ();
    if (rl_ucspos < rl_ucscol.len && !iswalnum (rl_ucs_at (&rl_ucs, rl_ucspos)))
        while (rl_ucspos < rl_ucscol.len && !iswalnum (rl_ucs_at (&rl_ucs, rl_ucspos)))
            rl_key_right ();
    while (rl_ucspos < rl_ucscol.len && iswalnum (rl_ucs_at (&rl_ucs, rl_ucspos)))
        rl_key_right ();
}

/*
 * Handle end key
 */
static void rl_key_end (void)
{
    int gpos;

    if (rl_tab_state > 0)
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
    rl_tab_state = 0;
    rl_tab_len = 0;
    rl_key_cut ();
}

/*** read line auto expander ***/

const rl_autoexpand_t *ReadLineListAutoExpand (void)
{
    return (const rl_autoexpand_t *) rl_ae;
}

void ReadLineAutoExpand (const char *command, const char *replace)
{
    rl_autoexpandv_t *e;
    
    for (e = rl_ae; e; e = e->next)
    {
        if (!strcmp (command, e->command))
        {
            s_repl (&e->replace, replace);
            return;
        }
    }
    e = malloc (sizeof (rl_autoexpandv_t));
    e->next = rl_ae;
    e->command = strdup (command);
    e->replace = strdup (replace);
    rl_ae = e;
}

static void rl_checkautoexpand (void)
{
    rl_autoexpandv_t *e;
    static str_s rl_exp = { NULL, 0, 0 };
    char *p, *q;
    
    if (!rl_bytepos)
        return;
    
    rl_linecompress (&rl_temp, 0, -1);
        
    for (e = rl_ae; e; e = e->next)
        if (!strncmp (rl_temp.txt, e->command, rl_bytepos) && strlen (e->command) == rl_bytepos)
            break;

    if (!e)
        return;
    
    s_init (&rl_exp, "", 0);
    for (p = e->replace; *p && (q = strchr (p, '%')); p = q)
    {
        s_catn (&rl_exp, p, q - p);
        if (q[1] == 'r')
        {
            if (uiG.last_rcvd)
                s_cat (&rl_exp, uiG.last_rcvd->nick ? uiG.last_rcvd->nick : s_sprintf ("%lu", uiG.last_rcvd->uin));
            q += 2;
        }
        else if (q[1] == 'a')
        {
            if (uiG.last_sent)
                s_cat (&rl_exp, uiG.last_sent->nick ? uiG.last_sent->nick : s_sprintf ("%lu", uiG.last_sent->uin));
            q += 2;
        }
        else
            q++;
    }
    s_cat (&rl_exp, p);
    if (rl_temp.txt[rl_bytepos])
    {
        s_cat (&rl_exp, " ");
        s_cat (&rl_exp, rl_temp.txt + rl_bytepos);
    }
    rl_tab_state = 0;
    rl_goto (0);
    rl_lineexpand (rl_exp.txt);
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

    ReadLineHandleSig ();
    s_catc (&rl_input, newbyte);
    
    input = ConvFrom (&rl_input, prG->enc_loc);
    if (input->txt[0] == CHAR_INCOMPLETE)
    {
        if (strcmp (rl_input.txt, ConvTo (input->txt, prG->enc_loc)->txt))
            return NULL;
    }
    
    rl_inputdone = 0;
    rl_signal &= ~1;

    inputucs = ConvTo (input->txt, ENC_UCS2BE);
    ucs = ((UBYTE)inputucs->txt[1])  | (((UBYTE)inputucs->txt[0]) << 8);

    s_init (&rl_input, "", 0);
    s_init (&rl_operate, "", 0);
    
    rl_dump_line ();
    ReadLinePrompt ();
    
    switch (rl_stat)
    {
        case 0:
            if (0) ;
#if HAVE_TCGETATTR
#if defined(VERASE)
            else if (ucs == tty_attr.c_cc[VERASE] && tty_attr.c_cc[VERASE] != _POSIX_VDISABLE)
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
                case 23:             /* ^W */
                    rl_key_delete_backward_word ();
                    break;
                case 25:             /* ^Y */
                    if (rl_yank)
                    {
                        rl_tab_state = 0;
                        rl_goto (0);
                        rl_lineexpand (rl_yank);
                    }
                    break;
                case 27:             /* ^[ = ESC */
#ifdef ANSI_TERM
                    rl_stat = 1;
#endif
                    break;
                case 32:             /*   = SPACE */
                    if (rl_tab_state > 0)
                        rl_tab_accept ();
                    else if (rl_tab_state == 0)
                        rl_checkautoexpand ();
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
            rl_stat = 0;
            if (ucs == 'u' || ucs == 'U')
                rl_stat = 10;
            else if (ucs == '[' || ucs == 'O')
                rl_stat = 2;
            else if (ucs == 'b')
                rl_key_backward_word ();
            else if (ucs == 'f')
                rl_key_forward_word ();
            else if (ucs == 127)
                rl_key_delete_backward_word ();
            else
                printf ("\a");
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
                case 'Z':            /* shift tab */
                    rl_key_shifttab ();
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
    
#if DEBUG_RL
    fprintf (stderr, "oper: %s\n", s_qquote (rl_operate.txt));
#endif
    rl_dump_line ();
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
#if DEBUG_RL
    fprintf (stderr, "killoper: %s\n", s_qquote (rl_operate.txt));
#endif
    s_init (&rl_operate, "", 0);
    if (rl_prompt_stat == 2)
        rl_goto (0);
    if (rl_prompt_stat == 2)
    {
        s_catc (&rl_operate, '\r');
#ifdef ANSI_TERM
        s_cat (&rl_operate, ANSI_CLEAR);
#endif
        rl_prompt_stat = 0;
    }
#if DEBUG_RL
    fprintf (stderr, "oper(rm): %s\n", s_qquote (rl_operate.txt));
#endif
    printf ("%s", rl_operate.txt);
    if (rl_prompt_stat == 0)
    {
        rl_print ("\r");
        rl_print (rl_prompt.txt);
        rl_prompt_len = rl_pos ();
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
    ReadLineHandleSig ();
    if (rl_prompt_stat == 0)
        return;
    s_init (&rl_operate, "", 0);
    rl_goto (0);
    s_catc (&rl_operate, '\r');
#ifdef ANSI_TERM
    s_cat (&rl_operate, ANSI_CLEAR);
#endif
    printf ("%s", rl_operate.txt);
    rl_prompt_stat = 0;
    rl_colpos = pos;
    rl_print ("\r");
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
    if (prG->flags & FLAG_UINPROMPT && (cont = uiG.last_sent))
        ReadLinePromptSet (s_sprintf ("[%s]", cont->nick));
    else
        ReadLinePromptSet (i18n (2467, "mICQ>"));
}
