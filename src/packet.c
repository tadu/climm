
/*
 * Assemble outgoing and dissect incoming packets.
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
 *
 * Note: alle read strings need to be free()ed.
 */

#include "micq.h"
#include "packet.h"
#include "preferences.h"
#include "util_ui.h"
#include "util_str.h"
#include "buildmark.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define cap_id   "\x82\x22\x44\x45\x53\x54\x00\x00"
#define cap_none "\x00\x00\x00\x00\x00\x00\x00\x00"
#define cap_str  "\xbc\xd2\x00\x04\xac\x96\xdd\x96"

#define cap_mid  "\x4c\x7f\x11\xd1"
#define cap_mstr "\x4f\xe9\xd3\x11"
#define cap_aim  "\x09\x46\x13"

static Cap caps[CAP_MAX] =
{
    { CAP_NONE,        16, cap_none cap_none,                   "CAP_NONE"        },
    { CAP_AIM_VOICE,   16, cap_aim "\x41" cap_mid cap_id,       "CAP_AIM_VOICE"   },
    { CAP_AIM_SFILE,   16, cap_aim "\x43" cap_mid cap_id,       "CAP_AIM_SFILE"   },
    { CAP_ISICQ,       16, cap_aim "\x44" cap_mid cap_id,       "CAP_ISICQ"       },
    { CAP_AIM_IMIMAGE, 16, cap_aim "\x45" cap_mid cap_id,       "CAP_AIM_IMIMAGE" },
    { CAP_AIM_BUDICON, 16, cap_aim "\x46" cap_mid cap_id,       "CAP_AIM_BUDICON" },
    { CAP_AIM_STOCKS,  16, cap_aim "\x47" cap_mid cap_id,       "CAP_AIM_STOCKS"  },
    { CAP_AIM_GETFILE, 16, cap_aim "\x48" cap_mid cap_id,       "CAP_AIM_GETFILE" },
    { CAP_SRVRELAY,    16, cap_aim "\x49" cap_mid cap_id,       "CAP_SRVRELAY"    },
    { CAP_AIM_GAMES,   16, cap_aim "\x4a" cap_mid cap_id,       "CAP_AIM_GAMES"   },
    { CAP_AIM_SBUD,    16, cap_aim "\x4b" cap_mid cap_id,       "CAP_AIM_SBUD"    },
    { CAP_UTF8,        16, cap_aim "\x4e" cap_mid cap_id,       "CAP_UTF8"        },
    { CAP_RTFMSGS,     16, "\x97\xb1\x27\x51\x24\x3c\x43\x34\xad\x22\xd6\xab\xf7\x3f\x14\x92", "CAP_RTFMSGS"     },
    { CAP_IS_2001,     16, "\x2e\x7a\x64\x75\xfa\xdf\x4d\xc8\x88\x6f\xea\x35\x95\xfd\xb6\xdf", "CAP_IS_2001"     },
    { CAP_STR_2001,    16, "\xa0\xe9\x3f\x37" cap_mstr cap_str, "CAP_STR_2001"    },
    { CAP_STR_2002,    16, "\x10\xcf\x40\xd1" cap_mstr cap_str, "CAP_STR_2002"    },
    { CAP_AIM_CHAT,    16, "\x74\x8f\x24\x20\x62\x87\x11\xd1" cap_id,                          "CAP_AIM_CHAT"    },
    { CAP_IS_WEB,      16, "\x56\x3f\xc8\x09\x0b\x6f\x41\xbd\x9f\x79\x42\x26\x09\xdf\xa2\xf3", "CAP_IS_WEB"      },
    { CAP_TRILL_CRYPT, 16, "\xf2\xe7\xc7\xf4\xfe\xad\x4d\xfb\xb2\x35\x36\x79\x8b\xdf\x00\x00", "CAP_TRILL_CRYPT" },
    { CAP_TRILL_2,     16, "\x97\xb1\x27\x51\x24\x3c\x43\x34\xad\x22\xd6\xab\xf7\x3f\x14\x09", "CAP_TRILL_2"     },
    { CAP_LICQ,        16, "\x09\x49\x13\x49" cap_mid cap_id,   "CAP_LICQ"        },
    { CAP_SIM,         15, "\x97\xb1\x27\x51\x24\x3c\x43\x34\xad\x22\xd6\xab\xf7\x3f\x14\x48", "CAP_SIM"         },
    { CAP_MACICQ,      16, "\xdd\x16\xf2\x02\x84\xe6\x11\xd4\x90\xdb\x00\x10\x4b\x9b\x4b\x7d", "CAP_MACICQ"      },
    { CAP_MICQ,        12, "mICQ \xa9 R.K. \x00\x00\x00\x00",   "CAP_MICQ"        },
    { CAP_KXICQ,       16, "\x09\x49\x13\x44" cap_mid cap_id,   "CAP_KXICQ"},
    { 0, 0, NULL, NULL }
};

static str_s packetstr[] =
{
    { NULL, 0, 0 },
    { NULL, 0, 0 },
    { NULL, 0, 0 },
    { NULL, 0, 0 },
    { "<invalidlen>", 12, 0 },
    { "", 0, 0 },
};

static int packetstrind = 0;

#define PACKETMAXSTR 4
#define PACKETSTRINVALID 4
#define PACKETSTREMPTY 5

Packet *PacketC (void)
{
    Packet *pak;
    
    pak = calloc (1, sizeof (Packet));
    assert (pak);

    Debug (DEB_PACKET, "<--- %p new", pak);
    uiG.packets++;

    return pak;
}

Packet *PacketCreate (const void *data, UDWORD len)
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

void PacketWriteCapID (Packet *pak, UBYTE id)
{
    UBYTE i;

    assert (pak);
    assert (id < CAP_MAX);
    
    if (caps[id].id == id)
    {
        if (id == CAP_MICQ)
        {
            PacketWriteData (pak, (const char *)caps[id].cap, 12);
            PacketWriteB4   (pak, BuildVersionNum);
        }
        else
            PacketWriteData (pak, (const char *)caps[id].cap, 16);
        return;
    }

    for (i = 0; i < CAP_MAX; i++)
        if (caps[i].id == id)
            break;
    
    i %= CAP_MAX;
    PacketWriteData (pak, (const char *)caps[i].cap, 16);
}

void PacketWriteCap (Packet *pak, Cap *cap)
{
    assert (pak);
    assert (cap);

    PacketWriteData (pak, (const char *)(cap->var ? cap->var : cap->cap), 16);
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
    if (data)
        PacketWriteData (pak, data, strlen (data));
}

void PacketWriteLNTS (Packet *pak, const char *data)
{
    data = data ? data : "";

    assert (pak);
    assert (pak->wpos + 3 + strlen (data) < PacketMaxData);
    
    PacketWrite2 (pak, strlen (data) + 1);
    PacketWriteData (pak, data, strlen (data));
    PacketWrite1 (pak, 0);
}

void PacketWriteDLStr (Packet *pak, const char *data)
{
    data = data ? data : "";

    assert (pak);
    assert (pak->wpos + 4 + strlen (data) < PacketMaxData);
    
    PacketWrite4 (pak, strlen (data));
    PacketWriteData (pak, data, strlen (data));
}

void PacketWriteLLNTS (Packet *pak, const char *data)
{
    data = data ? data : "";

    assert (pak);
    assert (pak->wpos + 5 + strlen (data) < PacketMaxData);

    PacketWrite2 (pak, strlen (data) + 3);
    PacketWrite2 (pak, strlen (data) + 1);
    PacketWriteData (pak, data, strlen (data));
    PacketWrite1 (pak, 0);
}

void PacketWriteUIN (Packet *pak, UDWORD uin)
{
    const char *str;
    
    str = s_sprintf ("%ld", uin);
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

void PacketWriteBLenDone (Packet *pak)
{
    UWORD pos;
    
    pos = PacketReadAtB2 (pak, pak->tpos);
    PacketWriteAtB2 (pak, pak->tpos, pak->wpos - pak->tpos - 2);
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

Cap *PacketReadCap (Packet *pak)
{
    const UBYTE *cap;
    UBYTE id;
    char *p;
    
    assert (pak);
    
    if (pak->rpos + 16 > PacketMaxData)
        return &caps[0];

    cap = pak->data + pak->rpos;
    pak->rpos += 16;

    for (id = 0; id < CAP_MAX; id++)
        if (caps[id].cap)
        {
#ifdef HAVE_MEMCMP
            if (!memcmp (cap, caps[id].cap, caps[id].len))
            {
                if (caps[id].len != 16)
                {
                    s_free (caps[id].var);
                    caps[id].var = malloc (16);
                    memcpy (caps[id].var, cap, 16);
                }
                return &caps[id];
            }
#else
            {
                const UBYTE *p, *q;
                int i;
                for (p = cap, q = caps[id].cap, i = 0; i < caps[id].len; i++)
                    if (*p++ != *q++)
                        break;
                    else
                        if (i + 1 == caps[id].len)
                        {
                            if (caps[id].len != 16)
                            {
                                s_free ((char *)caps[id].var);
                                caps[id].var = malloc (16);
                                memcpy (caps[id].var, cap, 16);
                            }
                            return &caps[id];
                        }
            }
#endif
        }
        else
            if (id == CAP_MAX)
                return &caps[0];
            else
                break;
    
    p = malloc (16);
    assert (p);
    memcpy (p, cap, 16);

    caps[id].id = id;
    caps[id].cap = (const UBYTE *)p;
    caps[id].len = 16;
    caps[id].name = strdup (s_sprintf ("CAP_UNK_%d", id));
    return &caps[id];
}

void PacketReadData (Packet *pak, str_t str, UWORD len)
{
    assert (pak);
    
    if (pak->rpos + len > PacketMaxData)
    {
        if (str)
        {
            str->len = 0;
            if (str->max)
                *str->txt = '\0';
        }
        return;
    }
    if (str)
    {
        s_init (str, "", len + 1);
        if (str->max >= len + 1)
        {
            str->len = len;
            memcpy (str->txt, pak->data + pak->rpos, len);
        }
    }
    pak->rpos += len;
}

strc_t PacketReadB2Str (Packet *pak, str_t str)
{
    unsigned int len;
    
    len = PacketReadB2 (pak);
    if (pak->rpos + len >= PacketMaxData)
    {
        if (!str)
            return &packetstr[PACKETSTRINVALID];
        s_init (str, packetstr[PACKETSTRINVALID].txt, 0);
        return str;
    }

    if (!str)
    {
        packetstrind %= 4;
        str = &packetstr[packetstrind++];
    }

    s_init (str, "", len + 2);
    assert (str->max >= len + 2);
    
    PacketReadData (pak, str, len);
    str->txt[str->len = len] = '\0';
    
    return str;
}

strc_t PacketReadL2Str (Packet *pak, str_t str)
{
    unsigned int len;
    
    len = PacketRead2 (pak);
    if (pak->rpos + len >= PacketMaxData)
    {
        if (!str)
            return &packetstr[PACKETSTRINVALID];
        s_init (str, packetstr[PACKETSTRINVALID].txt, 0);
        return str;
    }
    if (!len)
    {
        if (!str)
            return &packetstr[PACKETSTREMPTY];
        s_init (str, "", 0);
        return str;
    }
    
    if (!str)
    {
        packetstrind %= 4;
        str = &packetstr[packetstrind++];
    }

    s_init (str, "", len + 2);
    assert (str->max >= len + 2);

    PacketReadData (pak, str, len);
    str->txt[str->len = len - 1] = '\0';

    return str;
}

strc_t PacketReadL4Str (Packet *pak, str_t str)
{
    UWORD len;
    
    len = PacketRead4 (pak);
    if (pak->rpos + len >= PacketMaxData)
    {
        if (!str)
            return &packetstr[PACKETSTRINVALID];
        s_init (str, packetstr[PACKETSTRINVALID].txt, 0);
        return str;
    }

    if (!str)
    {
        packetstrind %= 4;
        str = &packetstr[packetstrind++];
    }

    s_init (str, "", len + 2);
    assert (str->max >= len + 2);
    
    PacketReadData (pak, str, len);
    str->txt[str->len = len] = '\0';

    return str;
}

UDWORD PacketReadUIN (Packet *pak)
{
    UBYTE len = PacketRead1 (pak);
    UDWORD uin;
    str_s str = { NULL };

    PacketReadData (pak, &str, len);
    str.txt[len] = '\0';
    uin = atoi (str.txt);
    s_done (&str);
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

void PacketReadAtData (const Packet *pak, UWORD at, str_t str, UWORD len)
{
    assert (pak);

    if (at + len > PacketMaxData)
    {
        if (str)
        {
            str->len = 0;
            if (str->max)
                *str->txt = '\0';
        }
        return;
    }
    if (str)
    {
        s_init (str, "", len + 1);
        if (str->max >= len + 1)
            memcpy (str->txt, pak->data + at, len);
    }
}

strc_t PacketReadAtL2Str (const Packet *pak, UWORD at, str_t str)
{
    unsigned int len;
    
    len = PacketReadAt2 (pak, at);
    if (at + 2 + len >= PacketMaxData)
    {
        if (!str)
            return &packetstr[PACKETSTRINVALID];
        s_init (str, packetstr[PACKETSTRINVALID].txt, 0);
        return str;
    }
    if (!len)
    {
        if (!str)
            return &packetstr[PACKETSTREMPTY];
        s_init (str, "", 0);
        return str;
    }

    if (!str)
    {
        packetstrind %= 4;
        str = &packetstr[packetstrind++];
    }

    s_init (str, "", len + 2);
    assert (str->max >= len + 2);

    PacketReadAtData (pak, at + 2, str, len);
    str->txt[str->len = len - 1] = '\0';

    return str;
}

int PacketReadLeft  (const Packet *pak)
{
    if (pak->rpos > pak->len)
        return 0;
    return pak->len - pak->rpos;
}

Cap *PacketCap (UBYTE id)
{
    if (id >= CAP_MAX)
        return &caps[0];
    return &caps[id];
}

