
/*
 * Dump packet according to syntax
 */

#include "micq.h"
#include <string.h>
#include <assert.h>
#include "packet.h"
#include "conv.h"
#include "util_str.h"
#include "preferences.h"

#undef  c_in
#define c_in(x) ConvToUTF8 (x, ENC_LATIN1)

static const char *syntable[] = {
    "s1x2",   "W-",
    "s1x3",   "W-",
    "s1x6",   "",
    "s1x7",   "",
    "s1x8",   "W-",
    "s1x10",  "",
    "s1x14",  "",
    "s1x15",  "uWWt[12gp2p]-",
    "s1x19",  "Wt-",
    "s1x23",  "D-",
    "s1x24",  "D-",
    "s1x30",  "t[12gp2p]-",
    "s2x3",   "t-",
    "s2x4",   "t[5C-]-",
    "s3x1",   "W",
    "s3x2",   "",
    "s3x3",   "t-",
    "s3x4",   "u-",
    "s3x5",   "u-",
    "s3x11",  "uWWt[13C-][12gp2p]-",
    "s3x12",  "uDt-",
    "s4x1",   "W",
    "s4x2",   "W-",
    "s4x4",   "",
    "s4x5",   "W-",
    "s4x6",   "DDW[1uWWt[2t[257D]-]-][2uWWt[5WDDCt[10001(wCwdbw)WWDDDWWWLDD]-]-][4uWWt[5DWL]-]",
    "s4x7",   "DDW[1uWWt[2t[257D]-]-][2uWWt[5WDDCt[10001(wCwdbw)wwdddwwwLDD]-]-][4uWWt[5DWL]-]",
    "s4x11",  "DDwuW(wCwdbw)wwdddwwwLDD",
    "s4x12",  "DDwu",
    "s9x2",   "",
    "s9x3",   "t-",
    "s9x5",   "u-",
    "s9x6",   "u-",
    "s9x7",   "u-",
    "s9x8",   "u-",
    "s11x2",  "W",
    "s19x2",  "",
    "s19x3",  "t-",
    "s19x4",  "",
    "s19x5",  "DW",
    "s19x6",  "",
    "s19x7",  "",
    "s19x8",  "uWWWWt-",
    "s19x9",  "BWWWWt-",
    "s19x10", "uWWWWt-",
    "s19x14", "W",
    "s19x15", "DW",
    "s19x17", "",
    "s19x18", "",
    "s19x20", "uD",
    "s19x24", "uBW",
    "s19x25", "uB",
    "s19x27", "ubBW",
    "s19x28", "u",
    "s21x1",  "Wt-",
    "s21x2",  "t[1wdww]-",
    "s21x3",  "t[1wDw,w.[2010w,b.[270bwLb]]]-",
    "s23x1",  "Wt[33DdWDDDD]-",
    "s23x4",  "t[1DDDDDDDDDDLDDW]-",
    "s23x5",  "t-",
    "p2p",    "DDbWDWWWWDDDW",
    "peer",   "b[1bw][2gpeemsg][3dddddddd][255wwdwddDDbddddS]",
    "peemsg", "dwwwdddw,wwL.[1DDS][26ggreet]",
    "greet",  "wddddwSdddwbdSdd",
    NULL,    NULL
};

const char *PacketDump (Packet *pak, const char *syntax)
{
    Packet *p = NULL;
    UDWORD size, nr, len, val, i, mem1, mem2;
    const char *f, *l, *last;
    char *t, *sub, lev, *tmp;
    
    assert (pak);
    assert (syntax);

    nr = size = mem1 = mem2 = 0;
    t = strdup ("");
    
    for (l = f = syntax; *f && pak->len > pak->rpos; f++)
    {
        last = l;
        l = f;
        switch (*f)
        {
            case 'b':
                nr = PacketRead1 (pak);
                t = s_cat  (t, &size, s_dumpnd (pak->data + pak->rpos - 1, 1));
                t = s_catf (t, &size, " " COLDEBUG "BYTE     0x%02x = %03u" COLNONE "\n", nr, nr);
                continue;
            case 'W':
                if (pak->len < pak->rpos + 2) break;
                t = s_cat  (t, &size, s_dumpnd (pak->data + pak->rpos, 2));
                nr = PacketReadB2 (pak);
                t = s_catf (t, &size, " " COLDEBUG "WORD.B   0x%04x = %05u" COLNONE "\n", nr, nr);
                continue;
            case 'w':
                if (pak->len < pak->rpos + 2) break;
                t = s_cat  (t, &size, s_dumpnd (pak->data + pak->rpos, 2));
                nr = PacketRead2 (pak);
                t = s_catf (t, &size, " " COLDEBUG "WORD.L   0x%04x = %05u" COLNONE "\n", nr, nr);
                continue;
            case 'D':
                if (pak->len < pak->rpos + 4) break;
                t = s_cat  (t, &size, s_dumpnd (pak->data + pak->rpos, 4));
                nr = PacketReadB4 (pak);
                t = s_catf (t, &size, " " COLDEBUG "DWORD.B  0x%08x = %010u" COLNONE "\n", nr, nr);
                continue;
            case 'd':
                if (pak->len < pak->rpos + 4) break;
                t = s_cat  (t, &size, s_dumpnd (pak->data + pak->rpos, 4));
                nr = PacketRead4 (pak);
                t = s_catf (t, &size, " " COLDEBUG "DWORD.L  0x%08x = %010u" COLNONE "\n", nr, nr);
                continue;
            case 'C':
                if (pak->len < pak->rpos + 16) break;
                t = s_cat  (t, &size, s_dumpnd (pak->data + pak->rpos, 16));
                t = s_catf (t, &size, " " COLDEBUG "%s" COLNONE "\n", PacketReadCap (pak)->name);
                continue;
            case 'u':
                nr = PacketReadAt1 (pak, pak->rpos);
                t = s_cat  (t, &size, s_dumpnd (pak->data + pak->rpos, nr + 1));
                t = s_catf (t, &size, " " COLDEBUG "UIN      %d" COLNONE "\n", PacketReadUIN (pak));
                continue;
            case 'B':
                nr = PacketReadAtB2 (pak, pak->rpos);
                if (pak->len < pak->rpos + nr + 2) break;
                t = s_cat  (t, &size, s_dumpnd (pak->data + pak->rpos, nr + 2));
                t = s_catf (t, &size, " " COLDEBUG "BStr     '%s'" COLNONE "\n", c_in (tmp = PacketReadStrB (pak)));
                free (tmp);
                continue;
            case 'L':
                nr = PacketReadAt2 (pak, pak->rpos);
                if (pak->len < pak->rpos + nr + 2) break;
                t = s_cat  (t, &size, s_dumpnd (pak->data + pak->rpos, nr + 2));
                t = s_catf (t, &size, " " COLDEBUG "LNTS     '%s'" COLNONE "\n", c_in (tmp = PacketReadLNTS (pak)));
                free (tmp);
                continue;
            case 'S':
                nr = PacketReadAt4 (pak, pak->rpos);
                if (pak->len < pak->rpos + nr + 4) break;
                t = s_cat  (t, &size, s_dumpnd (pak->data + pak->rpos, nr + 4));
                t = s_catf (t, &size, " " COLDEBUG "DLStr    '%s'" COLNONE "\n", c_in (tmp = PacketReadDLStr (pak)));
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
                    t = s_cat  (t, &size, s_dumpnd (pak->data + pak->rpos, len + 4));
                    t = s_catf (t, &size, " " COLDEBUG "TLV (% 2x) 0x%04x = %05u" COLNONE "\n", nr, val, val);
                }
                else if (len == 4)
                {
                    val = PacketReadAtB4 (pak, pak->rpos + 4);
                    t = s_cat  (t, &size, s_dumpnd (pak->data + pak->rpos, len + 4));
                    t = s_catf (t, &size, " " COLDEBUG "TLV (% 2x) 0x%08x = %010u" COLNONE "\n", nr, val, val);
                }
                else if (sub)
                {
                    Packet *p;
                    
                    p = PacketCreate (pak->data + pak->rpos + 4, len);
                    
                    t = s_cat  (t, &size, s_dumpnd (pak->data + pak->rpos, 4));
                    t = s_catf (t, &size, " " COLDEBUG "TLV (% 2x) \"%s\"" COLNONE "\n", nr, sub);
                    t = s_cat  (t, &size, s_ind (PacketDump (p, sub)));
                    
                    PacketD (p);
                    p = NULL;
                    free (sub);
                }
                else
                {
                    t = s_cat  (t, &size, s_dumpnd (pak->data + pak->rpos, 4));
                    t = s_catf (t, &size, " " COLDEBUG "TLV (% 2x)" COLNONE "\n", nr);
                    t = s_cat  (t, &size, s_ind (s_dump (pak->data + pak->rpos + 4, len)));
                }
                pak->rpos += len + 4;
                continue;
            case '(':
                len = PacketReadAt2 (pak, pak->rpos);
                if (pak->len < pak->rpos + 2 + len) break;
                t = s_cat  (t, &size, s_dumpnd (pak->data + pak->rpos, 2));
                t = s_catf (t, &size, " " COLDEBUG "DWORD.L  \"%s\"" COLNONE "\n", f);
                p = PacketCreate (pak->data + pak->rpos + 2, len);
                pak->rpos += len + 2;
                t = s_cat  (t, &size, s_ind (PacketDump (p, ++f)));
                for (lev = 1; *f && lev; f++)
                {
                    if (strchr ("()", *f))
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
    t = s_cat (t, &size, s_dump (pak->data + pak->rpos, pak->len - pak->rpos));
    return t;
}

