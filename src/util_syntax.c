/*
 * Dump packet according to syntax
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
#include <string.h>
#include <assert.h>
#include "packet.h"
#include "conv.h"
#include "util_str.h"
#include "preferences.h"

#ifdef ENABLE_UTF8
#define c_pin(x) ConvToUTF8 (x, ENC_LATIN1, -1, 0)
#else
#define c_pin(x) x
#endif

static const char *syntable[] = {
    "s1x2s",   "W-",
    "s1x3s",   "W-",
    "s1x6s",   "",
    "s1x7s",   "",
    "s1x8s",   "W-",
    "s1x10s",  "",
    "s1x14s",  "",
    "s1x15s",  "uWWt[12gp2p]-",
    "s1x19s",  "Wt-",
    "s1x23s",  "D-",
    "s1x24s",  "D-",
    "s1x30s",  "t[12gp2p]-",
    "s2x3s",   "t-",
    "s2x4s",   "t[5C-]-",
    "s3x1s",   "WWu-",
    "s3x2s",   "",
    "s3x3s",   "t-",
    "s3x4s",   "u-",
    "s3x5s",   "u-",
    "s3x10s",  "u-",
    "s3x11s",  "uWWt[13C-][12gp2p]-",
    "s3x12s",  "uDt-",
    "s4x1s",   "W",
    "s4x2s",   "W-",
    "s4x4s",   "",
    "s4x5s",   "W-",
    "s4x6s",   "DDW[1ut[2t[257WW]-]-][2ut[5WDDCt[10001(wCwdbw)wwdddwwwLDDS]-]-][4ut[5DWL]-]",
    "s4x7s",   "DDW[1uWWt[2t[257WW]-]-][2uWWt[5WDDCt[10001(wCwdbw)wwdddwwwLDDS]-]-][4uWWt[5DWL]-]",
    "s4x11s",  "DDwuW(wCwdbw)wwdddwwwLDD",
    "s4x12s",  "DDwu",
    "s9x2s",   "",
    "s9x3s",   "t-",
    "s9x5s",   "u-",
    "s9x6s",   "u-",
    "s9x7s",   "u-",
    "s9x8s",   "u-",
    "s11x2s",  "W",
    "s19x2s",  "",
    "s19x3s",  "t-",
    "s19x4s",  "",
    "s19x5s",  "DW",
    "s19x6s",  "bWgs19x6cs",
    "s19x6cs", "BWWW<t-)gs19x6cs",
    "s19x7s",  "",
    "s19x8s",  ")uWWWWt-",
    "s19x9s",  "BWWWWt-",
    "s19x10s", "uWWWWt-",
    "s19x14s", "W",
    "s19x15s", "DW",
    "s19x17s", "",
    "s19x18s", "",
    "s19x20s", "uD",
    "s19x24s", "uBW",
    "s19x25s", "uB",
    "s19x27s", "ubBW",
    "s19x28s", "u",
    "s21x1s",  "Wt-",
    "s21x2s",  "t[1wdww]-",
    "s21x3s",  "t[1wdw,w.[2010w,b.[270bwLb]][65dwbbbbwL]]-",
    "s23x1s",  "Wt[33DdWDDDD]-",
    "s23x4s",  "t[1DDDDDDDDDDLDDW]-",
    "s23x5s",  "t-",
    "p2p",     "DDbWDWWWWDDDW",
    "peer",    "b[1bw][2gpeemsg][3dddddddd][255wwdwddDDbddddS]",
    "peemsg",  "dwwwdddw,wwL.[1DDS][26ggreet]",
    "greet",   "wddddwSdddwbdSdd",
    "v5sp",    "wbdwwwdd",
    "v5cp",    "wdddwwwd",
    NULL,      NULL
};

char *PacketDump (Packet *pak, const char *syntax)
{
    Packet *p = NULL;
    str_s t = { NULL };
    UDWORD size, nr, len, val, i, mem1, mem2, oldrpos;
    const char *f, *l, *last;
    char *sub, lev, *tmp;
    
    assert (pak);
    assert (syntax);
    
    oldrpos = pak->rpos;

    nr = size = mem1 = mem2 = 0;
    s_init (&t, "", 100);
    
    for (l = f = syntax; *f && pak->len > pak->rpos; f++)
    {
        last = l;
        l = f;
        switch (*f)
        {
            case 'b':
                nr = PacketRead1 (pak);
                s_cat  (&t, s_dumpnd (pak->data + pak->rpos - 1, 1));
                s_catf (&t, " " COLDEBUG "BYTE     0x%02lx = %03lu" COLNONE "\n", nr, nr);
                continue;
            case 'W':
                if (pak->len < pak->rpos + 2) break;
                s_cat  (&t, s_dumpnd (pak->data + pak->rpos, 2));
                nr = PacketReadB2 (pak);
                s_catf (&t, " " COLDEBUG "WORD.B   0x%04lx = %05lu" COLNONE "\n", nr, nr);
                continue;
            case 'w':
                if (pak->len < pak->rpos + 2) break;
                s_cat  (&t, s_dumpnd (pak->data + pak->rpos, 2));
                nr = PacketRead2 (pak);
                s_catf (&t, " " COLDEBUG "WORD.L   0x%04lx = %05lu" COLNONE "\n", nr, nr);
                continue;
            case 'D':
                if (pak->len < pak->rpos + 4) break;
                s_cat  (&t, s_dumpnd (pak->data + pak->rpos, 4));
                nr = PacketReadB4 (pak);
                s_catf (&t, " " COLDEBUG "DWORD.B  0x%08lx = %010lu" COLNONE "\n", nr, nr);
                continue;
            case 'd':
                if (pak->len < pak->rpos + 4) break;
                s_cat  (&t, s_dumpnd (pak->data + pak->rpos, 4));
                nr = PacketRead4 (pak);
                s_catf (&t, " " COLDEBUG "DWORD.L  0x%08lx = %010lu" COLNONE "\n", nr, nr);
                continue;
            case 'C':
                if (pak->len < pak->rpos + 16) break;
                s_cat  (&t, s_dumpnd (pak->data + pak->rpos, 16));
                s_catf (&t, " " COLDEBUG "%s" COLNONE "\n", PacketReadCap (pak)->name);
                continue;
            case 'u':
                nr = PacketReadAt1 (pak, pak->rpos);
                s_cat  (&t, s_dumpnd (pak->data + pak->rpos, nr + 1));
                s_catf (&t, " " COLDEBUG "UIN      %ld" COLNONE "\n", PacketReadUIN (pak));
                continue;
            case 'B':
                nr = PacketReadAtB2 (pak, pak->rpos);
                if (pak->len < pak->rpos + nr + 2) break;
                s_cat  (&t, s_dumpnd (pak->data + pak->rpos, nr + 2));
                s_catf (&t, " " COLDEBUG "BStr     '%s'" COLNONE "\n", c_pin (tmp = PacketReadStrB (pak)));
                free (tmp);
                continue;
            case 'L':
                nr = PacketReadAt2 (pak, pak->rpos);
                if (pak->len < pak->rpos + nr + 2) break;
                s_cat  (&t, s_dumpnd (pak->data + pak->rpos, nr + 2));
                s_catf (&t, " " COLDEBUG "LNTS     '%s'" COLNONE "\n", c_pin (tmp = PacketReadLNTS (pak)));
                free (tmp);
                continue;
            case 'S':
                nr = PacketReadAt4 (pak, pak->rpos);
                if (pak->len < pak->rpos + nr + 4) break;
                s_cat  (&t, s_dumpnd (pak->data + pak->rpos, nr + 4));
                s_catf (&t, " " COLDEBUG "DLStr    '%s'" COLNONE "\n", c_pin (tmp = PacketReadDLStr (pak)));
                free (tmp);
                continue;
            case '-':
                l = last;
                f = l - 1;
                continue;
            case ',':
                mem1 = nr;
                continue;
            case ';':
                mem2 = nr;
                continue;
            case '.':
                nr = mem1;
                continue;
            case ':':
                nr = mem2;
                continue;
            case 'g':
                for (i = 0; syntable[i]; i += 2)
                    if (!strncmp (syntable[i], f + 1, strlen (syntable[i])))
                    {
                        l = f = syntable[i + 1] - 1;
                        i = 0;
                        break;
                    }
                if (!i)
                    continue;
                break;
            case 't':
                nr  = PacketReadAtB2 (pak, pak->rpos);
                len = PacketReadAtB2 (pak, pak->rpos + 2);
                if (pak->len < pak->rpos + len + 4) break;
                sub = NULL;
                for (f++; *f == '['; )
                {
                    char *t = NULL;
                    for (val = 0, f++; strchr ("0123456789", *f); f++)
                        val = 10 * val + *f - '0';
                    if (nr == val)
                        sub = t = strdup (f);
                    for (lev = 1; *f && lev; f++)
                    {
                        if (strchr ("<[]", *f))
                            lev += (*f == ']' ? -1 : 1);
                        t++;
                    }
                    if (nr == val)
                    {
                        *t = '\0';
                        if (t != sub)
                            *--t = '\0';
                    }
                }
                f--;
                    
                if (len == 2)
                {
                    val = PacketReadAtB2 (pak, pak->rpos + 4);
                    s_cat  (&t, s_dumpnd (pak->data + pak->rpos, len + 4));
                    s_catf (&t, " " COLDEBUG "TLV (%2lx) 0x%04lx = %05lu" COLNONE "\n", nr, val, val);
                }
                else if (len == 4)
                {
                    val = PacketReadAtB4 (pak, pak->rpos + 4);
                    s_cat  (&t, s_dumpnd (pak->data + pak->rpos, len + 4));
                    s_catf (&t, " " COLDEBUG "TLV (%2lx) 0x%08lx = %010lu" COLNONE "\n", nr, val, val);
                }
                else if (sub)
                {
                    Packet *p;
                    
                    p = PacketCreate (pak->data + pak->rpos + 4, len);
                    
                    s_cat  (&t, s_dumpnd (pak->data + pak->rpos, 4));
                    s_catf (&t, " " COLDEBUG "TLV (%2lx) \"%s\"" COLNONE "\n", nr, sub);
                    s_cat  (&t, s_ind (tmp = PacketDump (p, sub)));
                    
                    PacketD (p);
                    p = NULL;
                    free (sub);
                    free (tmp);
                }
                else
                {
                    s_cat  (&t, s_dumpnd (pak->data + pak->rpos, 4));
                    s_catf (&t, " " COLDEBUG "TLV (%2lx)" COLNONE "\n", nr);
                    s_cat  (&t, s_ind (s_dump (pak->data + pak->rpos + 4, len)));
                }
                pak->rpos += len + 4;
                continue;
            case '(':
            case '<':
                if (*f == '<')
                    len = PacketReadAtB2 (pak, pak->rpos);
                else
                    len = PacketReadAt2 (pak, pak->rpos);
                if (pak->len < pak->rpos + 2 + len) break;
                s_cat  (&t, s_dumpnd (pak->data + pak->rpos, 2));
                s_catf (&t, " " COLDEBUG "DWORD.%c  \"%s\"" COLNONE "\n", *f == '<' ? 'B' : 'L', f);
                p = PacketCreate (pak->data + pak->rpos + 2, len);
                pak->rpos += len + 2;
                if (*(tmp = PacketDump (p, ++f)))
                    s_cat  (&t, s_ind (tmp));
                PacketD (p);
                free (tmp);
                p = NULL;
                for (lev = 1; *f && lev; f++)
                {
                    if (strchr ("<()", *f))
                        lev += (*f == ')' ? -1 : 1);
                }
                f--;
                continue;
            case ')':
                break;
            case '[':
                for (val = 0, f++; strchr ("0123456789", *f); f++)
                    val = 10 * val + *f - '0';
                if (nr == val)
                    f--;
                else
                {
                    for (lev = 1; *f && lev; f++)
                    {
                        if (strchr ("<[]", *f))
                            lev += (*f == ']' ? -1 : 1);
                    }
                    if (*f != '<')
                        f--;
                }
                continue;
            case ']':
                f++;
                while (*f == '<' || *f == '[')
                {
                    for (lev = 1; *f && lev; f++)
                    {
                        if (strchr ("<[]", *f))
                            lev += (*f == ']' ? -1 : 1);
                    }
                }
                f--;
                continue;
            default:
                break;
        }
        break;
    }
    if (pak->len > pak->rpos)
        s_cat (&t, s_dump (pak->data + pak->rpos, pak->len - pak->rpos));
    pak->rpos = oldrpos;
    return t.txt;
}

