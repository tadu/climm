
/*
 * Assemble outgoing and dissect incoming packets.
 *
 * This file is Copyright © Rüdiger Kuhlmann; it may be distributed under
 * version 2 of the GPL licence.
 *
 * $Id$
 *
 * Note: alle read strings need to be free()ed.
 */

#include "micq.h"
#include "packet.h"
#include "conv.h"
#include "util_ui.h"
#include "util_str.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

Packet *PacketC (void)
{
    Packet *pak;
    
    pak = calloc (1, sizeof (Packet));
    assert (pak);

    Debug (DEB_PACKET, "<--- %p new", pak);
    uiG.packets++;

    return pak;
}

Packet *PacketCreate (const char *data, UDWORD len)
{
    Packet *newpak;
    
    newpak = calloc (1, sizeof (Packet));
    assert (newpak);
    
    memcpy (newpak->data, data, len);
    newpak->len = len;
    
    Debug (DEB_PACKET, "<-+- %p create", newpak);
    uiG.packets++;

    return newpak;
}

void PacketD (Packet *pak)
{
    Debug (DEB_PACKET, "---> %p free", pak);
    uiG.packets--;
    free (pak);
}

Packet *PacketClone (const Packet *pak)
{
    Packet *newpak;
    
    newpak = malloc (sizeof (Packet));
    assert (newpak);
    
    memcpy (newpak, pak, sizeof (Packet));
    newpak->rpos = 0;
    
    Debug (DEB_PACKET, "<-+- %p clone %p", newpak, pak);
    uiG.packets++;

    return newpak;
}

void PacketWrite1 (Packet *pak, UBYTE data)
{
    assert (pak);
    assert (pak->wpos < PacketMaxData);

    pak->data[pak->wpos++] = data;
    if (pak->wpos > pak->len)
        pak->len = pak->wpos;
}

void PacketWrite2 (Packet *pak, UWORD data)
{
    assert (pak);
    assert (pak->wpos + 1 < PacketMaxData);

    pak->data[pak->wpos++] = data & 0xff;  data >>= 8;
    pak->data[pak->wpos++] = data;
    if (pak->wpos > pak->len)
        pak->len = pak->wpos;
}

void PacketWriteB2 (Packet *pak, UWORD data)
{
    assert (pak);
    assert (pak->wpos + 1 < PacketMaxData);

    pak->data[pak->wpos++] = data >> 8;
    pak->data[pak->wpos++] = data & 0xff;
    if (pak->wpos > pak->len)
        pak->len = pak->wpos;
}

void PacketWrite4 (Packet *pak, UDWORD data)
{
    assert (pak);
    assert (pak->wpos + 3 < PacketMaxData);

    pak->data[pak->wpos++] = data & 0xff;  data >>= 8;
    pak->data[pak->wpos++] = data & 0xff;  data >>= 8;
    pak->data[pak->wpos++] = data & 0xff;  data >>= 8;
    pak->data[pak->wpos++] = data;
    if (pak->wpos > pak->len)
        pak->len = pak->wpos;
}

void PacketWriteB4 (Packet *pak, UDWORD data)
{
    assert (pak);
    assert (pak->wpos + 3 < PacketMaxData);

    pak->data[pak->wpos++] =  data >> 24;
    pak->data[pak->wpos++] = (data >> 16) & 0xff;
    pak->data[pak->wpos++] = (data >>  8) & 0xff;
    pak->data[pak->wpos++] =  data        & 0xff;
    if (pak->wpos > pak->len)
        pak->len = pak->wpos;
}

void PacketWriteData (Packet *pak, const char *data, UWORD len)
{
    assert (pak);
    assert (pak->wpos + len < PacketMaxData);

    memcpy (pak->data + pak->wpos, data, len);
    pak->wpos += len;
    if (pak->wpos > pak->len)
        pak->len = pak->wpos;
}

void PacketWriteStr(Packet *pak, const char *data)
{
    char *buf = strdup (data);
    ConvUnixWin (buf);
    PacketWriteData (pak, buf, strlen (data));
}

void PacketWriteLNTS (Packet *pak, const char *data)
{
    char *buf = strdup (data ? data : "");

    assert (pak);
    assert (buf);
    assert (pak->wpos + 3 + strlen (buf) < PacketMaxData);
    
    ConvUnixWin (buf);
    PacketWrite2 (pak, strlen (buf) + 1);
    PacketWriteData (pak, buf, strlen (buf));
    PacketWrite1 (pak, 0);
    
    free (buf);
}

void PacketWriteDLStr (Packet *pak, const char *data)
{
    char *buf = strdup (data ? data : "");

    assert (pak);
    assert (buf);
    assert (pak->wpos + 4 + strlen (buf) < PacketMaxData);
    
    ConvUnixWin (buf);
    PacketWrite4 (pak, strlen (buf));
    PacketWriteData (pak, buf, strlen (buf));
    
    free (buf);
}

void PacketWriteLLNTS (Packet *pak, const char *data)
{
    char *buf = strdup (data ? data : "");

    assert (pak);
    assert (buf);
    assert (pak->wpos + 5 + strlen (buf) < PacketMaxData);

    ConvUnixWin (buf);
    PacketWrite2 (pak, strlen (buf) + 3);
    PacketWrite2 (pak, strlen (buf) + 1);
    PacketWriteData (pak, buf, strlen (buf));
    PacketWrite1 (pak, 0);
    free (buf);
}

void PacketWriteUIN (Packet *pak, UDWORD uin)
{
    char str[15];
    
    snprintf (str, sizeof (str), "%ld", uin);
    PacketWrite1 (pak, strlen (str));
    PacketWriteData (pak, str, strlen (str));
}

void PacketWriteTLV (Packet *pak, UDWORD type)
{
    UWORD pos;
    
    PacketWriteB2 (pak, type);
    pos = pak->wpos;
    PacketWriteB2 (pak, pak->tpos); /* will be length */
    pak->tpos = pos;
}

void PacketWriteTLVDone (Packet *pak)
{
    UWORD pos;
    
    pos = PacketReadAtB2 (pak, pak->tpos);
    PacketWriteAtB2 (pak, pak->tpos, pak->wpos - pak->tpos - 2);
    pak->tpos = pos;
}

void PacketWriteLen (Packet *pak)
{
    UWORD pos;
    
    pos = pak->wpos;
    PacketWriteB2 (pak, pak->tpos); /* will be length */
    pak->tpos = pos;
}

void PacketWriteLenDone (Packet *pak)
{
    UWORD pos;
    
    pos = PacketReadAtB2 (pak, pak->tpos);
    PacketWriteAt2 (pak, pak->tpos, pak->wpos - pak->tpos - 2);
    pak->tpos = pos;
}

void PacketWriteLen4 (Packet *pak)
{
    UWORD pos;
    
    pos = pak->wpos;
    PacketWrite4 (pak, pak->tpos); /* will be length */
    pak->tpos = pos;
}

void PacketWriteLen4Done (Packet *pak)
{
    UWORD pos;
    
    pos = PacketReadAt4 (pak, pak->tpos);
    PacketWriteAt4 (pak, pak->tpos, pak->wpos - pak->tpos - 4);
    pak->tpos = pos;
}

UWORD PacketWritePos (const Packet *pak)
{
    return pak->wpos;
}

void PacketWriteAt1 (Packet *pak, UWORD at, UBYTE data)
{
    assert (pak);
    assert (at < PacketMaxData);

    pak->data[at++] = data;
    if (at > pak->len)
        pak->len = at;
}

void PacketWriteAt2 (Packet *pak, UWORD at, UWORD data)
{
    assert (pak);
    assert (at + 1 < PacketMaxData);

    pak->data[at++] = data & 0xff;  data >>= 8;
    pak->data[at++] = data;
    if (at > pak->len)
        pak->len = at;
}

void PacketWriteAtB2 (Packet *pak, UWORD at, UWORD data)
{
    assert (pak);
    assert (at + 1 < PacketMaxData);

    pak->data[at++] = data >> 8;
    pak->data[at++] = data & 0xff;
    if (at > pak->len)
        pak->len = at;
}

void PacketWriteAt4 (Packet *pak, UWORD at, UDWORD data)
{
    assert (pak);
    assert (at + 3 < PacketMaxData);

    pak->data[at++] = data & 0xff;  data >>= 8;
    pak->data[at++] = data & 0xff;  data >>= 8;
    pak->data[at++] = data & 0xff;  data >>= 8;
    pak->data[at++] = data;
    if (at > pak->len)
        pak->len = at;
}

void PacketWriteAtB4 (Packet *pak, UWORD at, UDWORD data)
{
    assert (pak);
    assert (at + 3 < PacketMaxData);

    pak->data[at++] =  data >> 24;
    pak->data[at++] = (data >> 16) & 0xff;
    pak->data[at++] = (data >>  8) & 0xff;
    pak->data[at++] =  data        & 0xff;
    if (at > pak->len)
        pak->len = at;
}

UBYTE PacketRead1 (Packet *pak)
{
    assert (pak);
    
    if (pak->rpos + 1 > PacketMaxData)
        return 0;

    return pak->data[pak->rpos++];
}

UWORD PacketRead2 (Packet *pak)
{
    UWORD data;

    assert (pak);

    if (pak->rpos + 2 > PacketMaxData)
        return 0;

    data  = pak->data[pak->rpos++];
    data |= pak->data[pak->rpos++] << 8;
    return data;
}

UWORD PacketReadB2 (Packet *pak)
{
    UWORD data;

    assert (pak);

    if (pak->rpos + 2 > PacketMaxData)
        return 0;

    data  = pak->data[pak->rpos++] << 8;
    data |= pak->data[pak->rpos++];
    return data;
}

UDWORD PacketRead4 (Packet *pak)
{
    UDWORD data;

    assert (pak);

    if (pak->rpos + 4 > PacketMaxData)
        return 0;

    data  = pak->data[pak->rpos++];
    data |= pak->data[pak->rpos++] << 8;
    data |= pak->data[pak->rpos++] << 16;
    data |= pak->data[pak->rpos++] << 24;
    return data;
}

UDWORD PacketReadB4 (Packet *pak)
{
    UDWORD data;

    assert (pak);

    if (pak->rpos + 4 > PacketMaxData)
        return 0;

    data  = pak->data[pak->rpos++] << 24;
    data |= pak->data[pak->rpos++] << 16;
    data |= pak->data[pak->rpos++] << 8;
    data |= pak->data[pak->rpos++];
    return data;
}

void PacketReadData (Packet *pak, char *buf, UWORD len)
{
    assert (pak);
    
    if (pak->rpos + len > PacketMaxData)
        return;
    
    if (buf)
        memcpy (buf, pak->data + pak->rpos, len);

    pak->rpos += len;
}

char *PacketReadStrB (Packet *pak)
{
    UWORD len;
    char *str, *t;
    
    len = PacketReadB2 (pak);

    if (pak->rpos + len >= PacketMaxData)
        return "<invalidlen>";

    str = malloc (len + 1);
    assert (str);
    
    PacketReadData (pak, str, len);
    str[len] = '\0';
    
    for (t = str; t - str < len; t+= strlen (t) + 1)
        ConvWinUnix (t);

    return str;
}

char *PacketReadLNTS (Packet *pak)
{
    UWORD len;
    UBYTE *str;
    
    len = PacketRead2 (pak);

    if (pak->rpos + len >= PacketMaxData)
        str = "<invalidlen>";
    if (!len)
        return strdup ("");

    str = malloc (len);
    assert (str);

    PacketReadData (pak, str, len);
    str[len - 1] = '\0';
    ConvWinUnix (str);

    return str;
}

char *PacketReadDLStr (Packet *pak)
{
    UWORD len;
    UBYTE *str;
    
    len = PacketRead4 (pak);

    if (pak->rpos + len >= PacketMaxData)
        return "<invalidlen>";

    str = malloc (len + 1);
    assert (str);
    
    PacketReadData (pak, str, len);
    str[len] = '\0';
    ConvWinUnix (str);

    return str;
}

UDWORD PacketReadUIN (Packet *pak)
{
    UBYTE len = PacketRead1 (pak);
    char *str = malloc (len + 1);
    UDWORD uin;

    PacketReadData (pak, str, len);
    str[len] = '\0';
    uin = atoi (str);
    free (str);
    return uin;
}

UWORD PacketReadPos (const Packet *pak)
{
    return pak->rpos;
}

UBYTE PacketReadAt1 (const Packet *pak, UWORD at)
{
    assert (pak);
    
    if (at + 1 > PacketMaxData)
        return 0;

    return pak->data[at];
}

UWORD PacketReadAt2 (const Packet *pak, UWORD at)
{
    UWORD data;

    assert (pak);

    if (at + 2 > PacketMaxData)
        return 0;

    data  = pak->data[at++];
    data |= pak->data[at] << 8;
    return data;
}

UWORD PacketReadAtB2 (const Packet *pak, UWORD at)
{
    UWORD data;

    assert (pak);

    if (at + 2 > PacketMaxData)
        return 0;

    data  = pak->data[at++] << 8;
    data |= pak->data[at];
    return data;
}

UDWORD PacketReadAt4 (const Packet *pak, UWORD at)
{
    UDWORD data;

    assert (pak);

    if (at + 4 > PacketMaxData)
        return 0;

    data  = pak->data[at++];
    data |= pak->data[at++] << 8;
    data |= pak->data[at++] << 16;
    data |= pak->data[at] << 24;
    return data;
}

UDWORD PacketReadAtB4 (const Packet *pak, UWORD at)
{
    UDWORD data;

    assert (pak);

    if (at + 4 > PacketMaxData)
        return 0;

    data  = pak->data[at++] << 24;
    data |= pak->data[at++] << 16;
    data |= pak->data[at++] << 8;
    data |= pak->data[at];
    return data;
}

void PacketReadAtData (const Packet *pak, UWORD at, char *buf, UWORD len)
{
    assert (pak);

    if (at + len > PacketMaxData)
        return;

    memcpy (buf, pak->data + at, len);
}

char *PacketReadAtStrB (const Packet *pak, UWORD at)
{
    UWORD len;
    char *str;
    
    len = PacketReadAtB2 (pak, at);

    if (!len)
        return "";
    
    str = malloc (len + 1);
    assert (str);
    
    PacketReadAtData (pak, at + 2, str, len);
    str[len] = '\0';
    ConvWinUnix (str);

    return str;
}

char *PacketReadAtLNTS (Packet *pak, UWORD at)
{
    UWORD len;
    UBYTE *str;
    
    len = PacketReadAt2 (pak, at);

    if (at + 2 + len >= PacketMaxData)
        return "<invalid>";
    if (!len)
        return strdup ("");

    str = malloc (len);
    assert (str);

    PacketReadAtData (pak, at + 2, str, len);
    str[len - 1] = '\0';
    ConvWinUnix (str);
    return str;
}

int PacketReadLeft  (const Packet *pak)
{
    if (pak->rpos > pak->len)
        return 0;
    return pak->len - pak->rpos;
}

const char *PacketDump (Packet *pak, const char *syntax)
{
    Packet *p = NULL;
    UDWORD size, nr, len, val;
    const char *f, *l, *last;
    char *t, *sub, lev;
    
    assert (pak);
    assert (syntax);

    nr = size = 0;
    t = strdup ("");
    
    for (l = f = syntax; *f && pak->len > pak->rpos; f++)
    {
        last = l;
        l = f;
        switch (*f)
        {
            case 'C':
                if (pak->len < pak->rpos + 16) break;
                t = s_cat  (t, &size, s_dumpnd (pak->data + pak->rpos, 16));
                t = s_catf (t, &size, " " COLDEBUG "CAP" COLNONE "\n");
                pak->rpos += 16;
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
            case 'b':
                nr = PacketRead1 (pak);
                t = s_cat  (t, &size, s_dumpnd (pak->data + pak->rpos - 1, 1));
                t = s_catf (t, &size, " " COLDEBUG "BYTE     0x%02x = %03u" COLNONE "\n", nr, nr);
                continue;
            case 'B':
                nr = PacketReadAtB2 (pak, pak->rpos);
                if (pak->len < pak->rpos + nr + 2) break;
                t = s_cat  (t, &size, s_dumpnd (pak->data + pak->rpos, nr + 2));
                t = s_catf (t, &size, " " COLDEBUG "BStr     '%s'" COLNONE "\n", PacketReadStrB (pak));
                continue;
            case 'L':
                nr = PacketReadAt2 (pak, pak->rpos);
                if (pak->len < pak->rpos + nr + 2) break;
                t = s_cat  (t, &size, s_dumpnd (pak->data + pak->rpos, nr + 2));
                t = s_catf (t, &size, " " COLDEBUG "LNTS     '%s'" COLNONE "\n", PacketReadLNTS (pak));
                continue;
            case '-':
                l = last;
                f = l - 1;
                continue;
            case 'u':
                nr = PacketReadAt1 (pak, pak->rpos);
                t = s_cat  (t, &size, s_dumpnd (pak->data + pak->rpos, nr + 1));
                t = s_catf (t, &size, " " COLDEBUG "UIN      %d" COLNONE "\n", PacketReadUIN (pak));
                continue;
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
                M_printf ("Parse: %c in '%s'\n", *f, syntax);
        }
        break;
    }
    t = s_cat (t, &size, s_dump (pak->data + pak->rpos, pak->len - pak->rpos));
    return t;
}

