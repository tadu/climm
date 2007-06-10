/*
 * Dump packet according to syntax
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
#include <assert.h>
#include "util_syntax.h"
#include "packet.h"
#include "conv.h"

#define c_pin(x) ConvFrom (x, ENC_LATIN1)->txt

static const char *syntable[] = {
    "s1x1s",   "WT[8W]",
    "s1x2s",   "W-",
    "s1x3s",   "W-",
    "s1x4s",   "W-",
    "s1x6s",   "",
    "s1x7s",   "W,{WDDDDDDDDb}.{WW{D}}",
    "s1x8s",   "W-",
    "s1x10s",  "",
    "s1x14s",  "",
    "s1x15s",  "uWW{T[12gp2p]}uWW{T[12gp2p]}",
    "s1x19s",  "WT-",
    "s1x23s",  "D-",
    "s1x24s",  "D-",
    "s1x30s",  "T[12gp2p]-",
    "s2x1s",   "WT[8W]",
    "s2x3s",   "T-",
    "s2x4s",   "T[5C-]-",
    "s2x5s",   "Wu",
    "s2x6s",   "uWW{T[1D][6D][12gp2p][10D][15D][3D][5D]}T[1][2][3][4][5C-]-",
    "s3x1s",   "WWu-",
    "s3x2s",   "",
    "s3x3s",   "T-",
    "s3x4s",   "u-",
    "s3x5s",   "u-",
    "s3x10s",  "u-",
    "s3x11s",  "uWWT[13C-][12gp2p]-",
    "s3x12s",  "uDT-",
    "s4x1s",   "W",
    "s4x2s",   "W-",
    "s4x4s",   "",
    "s4x5s",   "W-",
    "s4x6s",   "DDW[1uT[2T[257WW]-]-][2uT[5WDDCT[10001(wCwdbw)wwdddwwwLDDS]-]-][4uT[5DWL]-]",
    "s4x7s",   "DDW[1uWWT[2T[257WW]-]-][2uWWT[5WDDCT[10001(wCwdbw)wwdddwwwLDDS]-]-][4uWWT[5DWL]-]",
    "s4x11s",  "DDwuW(wCwdbw)wwdddwwwLDD",
    "s4x12s",  "DDwu",
    "s9x2s",   "",
    "s9x3s",   "T-",
    "s9x5s",   "u-",
    "s9x6s",   "u-",
    "s9x7s",   "u-",
    "s9x8s",   "u-",
    "s11x2s",  "W",
    "s13x9s",  "T-",
    "s19x2s",  "",
    "s19x3s",  "T-",
    "s19x4s",  "",
    "s19x5s",  "DW",
    "s19x6s",  "bWgs19x6cs",
    "s19x6cs", "BWWW<T-)gs19x6cs",
    "s19x7s",  "",
    "s19x8s",  "BWWW<T-)gs19x8s",
    "s19x9s",  "BWWW<T-)gs19x9s",
    "s19x10s", "uWWWWT-",
    "s19x14s", "W-",
    "s19x15s", "DW",
    "s19x17s", "",
    "s19x18s", "",
    "s19x20s", "uD",
    "s19x24s", "uBW",
    "s19x25s", "uB",
    "s19x27s", "ubBW",
    "s19x28s", "u",
    "s21x1s",  "WT-",
    "s21x2s",  "T[1wdw,w.[2000gs21x2m]]-",
    "s21x2m",  "w[1030L][1232d][3130gmsall]",
    "msall",   "t[600L]-",
    "s21x3s",  "T[1wdw,w.[2010gs21x3m.][65dwbbbbwL]]-",
    "s21x3m.", "w,b.gs21x3m%d",
    "s21x3m200",  "LLLLLLLLLLLwbbw",
    "s21x3m210",  "LLLLLLwLLLwL",
    "s21x3m220",  "wbLwbbbbbwLLww",
    "s21x3m230",  "L",
    "s21x3m235",  "b{bL}",
    "s21x3m240",  "b{wL}",
    "s21x3m250",  "b{wL}b{wL}",
    "s21x3m270",  "bwLb",
    "s23x1s",  "WT[33DdWDDDD]-",
    "s23x2s",  "T-",
    "s23x3s",  "T-",
    "s23x4s",  "T[1DDDDDDDDDDLDDW]-",
    "s23x5s",  "T-",
    "s23x6s",  "T-",
    "s23x7s",  "B",
    "p2p",     "DDbWDWWWWDDDW",
    "peer",    "b[1bw][2gpeemsg][3dddddddd][255wwdwddDDbddddS]",
    "peemsg",  "dwwwdddw,wwL.[1DDS][26ggreet]",
    "greet",   "wddddwSdddwbdSdd",
    "v5sp",    "wbdwwwdd",
    "v5cp",    "wdddwwwd",
    NULL,      NULL
};

char *PacketDump (Packet *pak, const char *syntax, const char *coldebug, const char *colnone)
{
    Packet *p = NULL;
    str_s str = { NULL, 0, 0 };
    UDWORD count[3];
    UDWORD nr, len, val, i, mem1, mem2, oldrpos, clev = -1;
    const char *f, *l, *last;
    char *sub = NULL, lev, *tmp;
    
    assert (pak);
    assert (syntax);
    
    oldrpos = pak->rpos;

    nr = mem1 = mem2 = 0;
    s_init (&str, "", 100);
    
    for (l = f = syntax; *f && pak->len > pak->rpos; f++)
    {
        last = l;
        l = f;
        switch (*f)
        {
            case 'b':
                nr = PacketRead1 (pak);
                s_cat  (&str, s_dumpnd (pak->data + pak->rpos - 1, 1));
                s_catf (&str, " %sBYTE     0x%02lx = %03lu%s\n", coldebug, nr, nr, colnone);
                continue;
            case 'W':
                if (pak->len < pak->rpos + 2) break;
                s_cat  (&str, s_dumpnd (pak->data + pak->rpos, 2));
                nr = PacketReadB2 (pak);
                s_catf (&str, " %sWORD.B   0x%04lx = %05lu%s\n", coldebug, nr, nr, colnone);
                continue;
            case 'w':
                if (pak->len < pak->rpos + 2) break;
                s_cat  (&str, s_dumpnd (pak->data + pak->rpos, 2));
                nr = PacketRead2 (pak);
                s_catf (&str, " %sWORD.L   0x%04lx = %05lu%s\n", coldebug, nr, nr, colnone);
                continue;
            case 'D':
                if (pak->len < pak->rpos + 4) break;
                s_cat  (&str, s_dumpnd (pak->data + pak->rpos, 4));
                nr = PacketReadB4 (pak);
                s_catf (&str, " %sDWORD.B  0x%08lx = %010lu%s\n", coldebug, nr, nr, colnone);
                continue;
            case 'd':
                if (pak->len < pak->rpos + 4) break;
                s_cat  (&str, s_dumpnd (pak->data + pak->rpos, 4));
                nr = PacketRead4 (pak);
                s_catf (&str, " %sDWORD.L  0x%08lx = %010lu%s\n", coldebug, nr, nr, colnone);
                continue;
            case 'C':
                if (pak->len < pak->rpos + 16) break;
                s_cat  (&str, s_dumpnd (pak->data + pak->rpos, 16));
                s_catf (&str, " %s%s%s\n", coldebug, PacketReadCap (pak)->name, colnone);
                continue;
            case 'u':
                nr = PacketReadAt1 (pak, pak->rpos);
                s_cat  (&str, s_dumpnd (pak->data + pak->rpos, nr + 1));
                s_catf (&str, " %sUIN      %s%s\n", coldebug, PacketReadUIN (pak)->txt, colnone);
                continue;
            case 'B':
                nr = PacketReadAtB2 (pak, pak->rpos);
                if (pak->len < pak->rpos + nr + 2) break;
                s_cat  (&str, s_dumpnd (pak->data + pak->rpos, nr + 2));
                s_catf (&str, " %sBStr     '%s'%s\n", coldebug, c_pin (PacketReadB2Str (pak, NULL)), colnone);
                continue;
            case 'L':
                nr = PacketReadAt2 (pak, pak->rpos);
                if (pak->len < pak->rpos + nr + 2) break;
                s_cat  (&str, s_dumpnd (pak->data + pak->rpos, nr + 2));
                s_catf (&str, " %sLNTS     '%s'%s\n", coldebug, c_pin (PacketReadL2Str (pak, NULL)), colnone);
                continue;
            case 'S':
                nr = PacketReadAt4 (pak, pak->rpos);
                if (pak->len < pak->rpos + nr + 4) break;
                s_cat  (&str, s_dumpnd (pak->data + pak->rpos, nr + 4));
                s_catf (&str, " %sDLStr    '%s'%s\n", coldebug, c_pin (PacketReadL4Str (pak, NULL)), colnone);
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
                {
                const char *form = s_sprintf (f + 1, nr);
                for (i = 0; syntable[i]; i += 2)
                    if (!strncmp (syntable[i], form, strlen (syntable[i])))
                    {
                        l = f = syntable[i + 1] - 1;
                        i = 0;
                        break;
                    }
                }
                if (!i)
                    continue;
                break;
            case 'T':
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
                    s_cat  (&str, s_dumpnd (pak->data + pak->rpos, len + 4));
                    s_catf (&str, " %sTLV (%2lx) 0x%04lx = %05lu%s\n", coldebug, nr, val, val, colnone);
                }
                else if (len == 4)
                {
                    val = PacketReadAtB4 (pak, pak->rpos + 4);
                    s_cat  (&str, s_dumpnd (pak->data + pak->rpos, len + 4));
                    s_catf (&str, " %sTLV (%2lx) 0x%08lx = %010lu%s\n", coldebug, nr, val, val, colnone);
                }
                else if (sub)
                {
                    Packet *p;
                    str_s tt = { 0, 0, 0 };
                    
                    tt.txt = (char *)pak->data + pak->rpos + 4;
                    tt.len = len;
                    p = PacketCreate (&tt);
                    
                    s_cat  (&str, s_dumpnd (pak->data + pak->rpos, 4));
                    s_catf (&str, " %sTLV (%2lx) \"%s\"%s\n", coldebug, nr, sub, colnone);
                    s_cat  (&str, s_ind (tmp = PacketDump (p, sub, coldebug, colnone)));
                    
                    PacketD (p);
                    p = NULL;
                    free (sub);
                    free (tmp);
                }
                else
                {
                    s_cat  (&str, s_dumpnd (pak->data + pak->rpos, 4));
                    s_catf (&str, " %sTLV (%2lx)%s\n", coldebug, nr, colnone);
                    s_cat  (&str, s_ind (s_dump (pak->data + pak->rpos + 4, len)));
                }
                pak->rpos += len + 4;
                continue;
            case 't': /* brain dead */
                nr  = PacketReadAt2 (pak, pak->rpos);
                len = PacketReadAt2 (pak, pak->rpos + 2);
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
                    val = PacketReadAt2 (pak, pak->rpos + 4);
                    s_cat  (&str, s_dumpnd (pak->data + pak->rpos, len + 4));
                    s_catf (&str, " %stlv (%2lx) 0x%04lx = %05lu%s\n", coldebug, nr, val, val, colnone);
                }
                else if (len == 4)
                {
                    val = PacketReadAt4 (pak, pak->rpos + 4);
                    s_cat  (&str, s_dumpnd (pak->data + pak->rpos, len + 4));
                    s_catf (&str, " %stlv (%2lx) 0x%08lx = %010lu%s\n", coldebug, nr, val, val, colnone);
                }
                else if (sub)
                {
                    Packet *p;
                    str_s tt = { 0, 0, 0 };
                    
                    tt.txt = (char *)pak->data + pak->rpos + 4;
                    tt.len = len;
                    p = PacketCreate (&tt);
                    
                    s_cat  (&str, s_dumpnd (pak->data + pak->rpos, 4));
                    s_catf (&str, " %stlv (%2lx) \"%s\"%s\n", coldebug, nr, sub, colnone);
                    s_cat  (&str, s_ind (tmp = PacketDump (p, sub, coldebug, colnone)));
                    
                    PacketD (p);
                    p = NULL;
                    free (sub);
                    free (tmp);
                }
                else
                {
                    s_cat  (&str, s_dumpnd (pak->data + pak->rpos, 4));
                    s_catf (&str, " %stlv (%2lx)%s\n", coldebug, nr, colnone);
                    s_cat  (&str, s_ind (s_dump (pak->data + pak->rpos + 4, len)));
                }
                pak->rpos += len + 4;
                continue;
            case '{':
                count[++clev] = nr;
                if (!nr)
                {
                    for (lev = 1; *f && lev; f++)
                    {
                        if (strchr ("{}", *f))
                            lev += (*f == '}' ? -1 : 1);
                    }
                    f--;
                    continue;
                }
                continue;
            case '}':
                count[clev]--;
                if (count[clev] > 0)
                {
                    f--;
                    for (lev = 1; *f && lev; f--)
                    {
                        if (strchr ("{}", *f))
                            lev += (*f == '{' ? -1 : 1);
                    }
                    f++;
                }
                else
                    clev--;
                continue;
            case '(':
            case '<':
                if (*f == '<')
                    len = PacketReadAtB2 (pak, pak->rpos);
                else
                    len = PacketReadAt2 (pak, pak->rpos);
                if (pak->len < pak->rpos + 2 + len) break;
                s_cat  (&str, s_dumpnd (pak->data + pak->rpos, 2));
                s_catf (&str, " %sDWORD.%c  \"%s\"%s\n", coldebug, *f == '<' ? 'B' : 'L', f, colnone);
                {
                    str_s tt = { 0, 0, 0 };
                    tt.txt = (char *)pak->data + pak->rpos + 2;
                    tt.len = len;
                    p = PacketCreate (&tt);
                }
                pak->rpos += len + 2;
                if (*(tmp = PacketDump (p, ++f, coldebug, colnone)))
                    s_cat  (&str, s_ind (tmp));
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
        s_cat (&str, s_dump (pak->data + pak->rpos, pak->len - pak->rpos));
    pak->rpos = oldrpos;
    return str.txt;
}

