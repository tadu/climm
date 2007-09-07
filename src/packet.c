

/*
 * Assemble outgoing and dissect incoming packets.
 *
 * climm Copyright (C) © 2001-2007 Rüdiger Kuhlmann
 *
 * climm is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 dated June, 1991.
 *
 * climm is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * In addition, as a special exception permission is granted to link the
 * code of this release of climm with the OpenSSL project's "OpenSSL"
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
 *
 * Note: alle read strings need to be free()ed.
 */

#include "climm.h"
#include "packet.h"
#include "preferences.h"
#include "util_ui.h"
#include "contact.h"
#include "buildmark.h"
#include <assert.h>

#define cap_id   "\x82\x22\x44\x45\x53\x54\x00\x00"
#define cap_none "\x00\x00\x00\x00\x00\x00\x00\x00"
#define cap_str  "\xbc\xd2\x00\x04\xac\x96\xdd\x96"

#define cap_mid  "\x4c\x7f\x11\xd1"
#define cap_mstr "\x4f\xe9\xd3\x11"
#define cap_aim  "\x09\x46\x13"

static Cap caps[CAP_MAX] =
{
    { CAP_NONE,        16, cap_none cap_none,                   "CAP_NONE",        NULL },
    /* AIM capabilities */
    { CAP_AIM_VOICE,   16, cap_aim "\x41" cap_mid cap_id,       "CAP_AIM_VOICE",   NULL },
    { CAP_AIM_SFILE,   16, cap_aim "\x43" cap_mid cap_id,       "CAP_AIM_SFILE",   NULL },
    { CAP_ISICQ,       16, cap_aim "\x44" cap_mid cap_id,       "CAP_ISICQ",       NULL },
    { CAP_AIM_IMIMAGE, 16, cap_aim "\x45" cap_mid cap_id,       "CAP_AIM_IMIMAGE", NULL },
    { CAP_AIM_BUDICON, 16, cap_aim "\x46" cap_mid cap_id,       "CAP_AIM_BUDICON", NULL },
    { CAP_AIM_STOCKS,  16, cap_aim "\x47" cap_mid cap_id,       "CAP_AIM_STOCKS",  NULL },
    { CAP_AIM_GETFILE, 16, cap_aim "\x48" cap_mid cap_id,       "CAP_AIM_GETFILE", NULL },
    { CAP_SRVRELAY,    16, cap_aim "\x49" cap_mid cap_id,       "CAP_SRVRELAY",    NULL },
    { CAP_AIM_GAMES,   16, cap_aim "\x4a" cap_mid cap_id,       "CAP_AIM_GAMES",   NULL },
    { CAP_AIM_SBUD,    16, cap_aim "\x4b" cap_mid cap_id,       "CAP_AIM_SBUD",    NULL },
    { CAP_AIM_INTER,   16, cap_aim "\x4d" cap_mid cap_id,       "CAP_AIM_INTER",   NULL },
    { CAP_AVATAR,      16, cap_aim "\x4c" cap_mid cap_id,       "CAP_AVATAR",      NULL },
    { CAP_UTF8,        16, cap_aim "\x4e" cap_mid cap_id,       "CAP_UTF8",        NULL },
    /* ICQ capabilities */
    { CAP_RTFMSGS,     16, "\x97\xb1\x27\x51\x24\x3c\x43\x34\xad\x22\xd6\xab\xf7\x3f\x14\x92", "CAP_RTFMSGS",     NULL },
    { CAP_IS_2001,     16, "\x2e\x7a\x64\x75\xfa\xdf\x4d\xc8\x88\x6f\xea\x35\x95\xfd\xb6\xdf", "CAP_IS_2001",     NULL },
    { CAP_STR_2001,    16, "\xa0\xe9\x3f\x37" cap_mstr cap_str, "CAP_STR_2001",    NULL }, /* PSIG_INFO_PLUGIN_s   PMSG_QUERY_INFO_s */
    { CAP_STR_2002,    16, "\x10\xcf\x40\xd1" cap_mstr cap_str, "CAP_STR_2002",    NULL }, /* PSIG_STATUS_PLUGIN_s PMSG_QUERY_STATUS_s */
    { CAP_AIM_CHAT,    16, "\x74\x8f\x24\x20\x62\x87\x11\xd1" cap_id,                          "CAP_AIM_CHAT",    NULL },
    { CAP_TYPING,      16, "\x56\x3f\xc8\x09\x0b\x6f\x41\xbd\x9f\x79\x42\x26\x09\xdf\xa2\xf3", "CAP_TYPING",      NULL },
    { CAP_XTRAZ,       16, "\x1a\x09\x3c\x6c\xd7\xfd\x4e\xc5\x9d\x51\xa6\x47\x4e\x34\xf5\xa0", "CAP_XTRAZ", NULL },
    /* client detection capabilities */
    { CAP_TRILL_CRYPT, 16, "\xf2\xe7\xc7\xf4\xfe\xad\x4d\xfb\xb2\x35\x36\x79\x8b\xdf\x00\x00", "CAP_TRILL_CRYPT", NULL },
    { CAP_TRILL_2,     16, "\x97\xb1\x27\x51\x24\x3c\x43\x34\xad\x22\xd6\xab\xf7\x3f\x14\x09", "CAP_TRILL_2",     NULL },
    { CAP_LICQ,        16, "\x09\x49\x13\x49" cap_mid cap_id,   "CAP_LICQ",        NULL },
    { CAP_LICQNEW,     12, "Licq client \x00\x00\x00\x00",      "CAP_LICQNEW",     NULL },
    { CAP_SIM,         15, "\x97\xb1\x27\x51\x24\x3c\x43\x34\xad\x22\xd6\xab\xf7\x3f\x14\x48", "CAP_SIM",         NULL },
    { CAP_SIMNEW,      12, "SIM client  \x00\x00\x00\x00",      "CAP_SIMNEW",      NULL },
    { CAP_MACICQ,      16, "\xdd\x16\xf2\x02\x84\xe6\x11\xd4\x90\xdb\x00\x10\x4b\x9b\x4b\x7d", "CAP_MACICQ",      NULL },
    { CAP_MICQ,        12, "mICQ \xa9 R.K. \x00\x00\x00\x00",   "CAP_MICQ",        NULL },
    { CAP_CLIMM,       12, "climm\xa9 R.K. \x00\x00\x00\x00",   "CAP_CLIMM",       NULL },
    { CAP_KXICQ,       16, "\x09\x49\x13\x44" cap_mid cap_id,   "CAP_KXICQ",       NULL },
    { CAP_KOPETE,      12, "Kopete ICQ  \x00\x00\x00\x00",      "CAP_KOPETE",      NULL },
    { CAP_IMSECURE,    12, "IMsecureCphr\x00\x00\x00\x00",      "CAP_IMSECURE",    NULL },
    { CAP_ARQ,          9, "&RQinside\x00\x00\x00\x00\x00\x00\x00", "CAP_ARQ",     NULL },
    { CAP_MIRANDA,      8, "MirandaM\x00\x00\x00\x00\x00\x00\x00\x00", "CAP_MIRANDA", NULL },
    { CAP_QIP,         16, "\x56\x3f\xc8\x09\x0b\x6f\x41QIP 2005a", "CAP_QIP",     NULL },
    { CAP_IM2,         16, "\x74\xed\xc3\x36\x44\xdf\x48\x5b\x8b\x1c\x67\x1a\x1f\x86\x09\x9f", "CAP_IM2", NULL },
    /* Unknown capabilities */
    { CAP_UTF8ii,      16, cap_aim "\x4e" cap_mid "\x82\x22\x44\x45\x53\x54ii", "CAP_UTF82", NULL },
    { CAP_WIERD1,      16, "\x17\x8c\x2d\x9b\xda\xa5\x45\xbb\x8d\xdb\xf3\xbd\xbd\x53\xa1\x0a", "CAP_WIERD1", NULL },
    { CAP_WIERD3,      16, "\x67\x36\x15\x15\x61\x2d\x4c\x07\x8f\x3d\xbd\xe6\x40\x8e\xa0\x41", "CAP_WIERD3", NULL },
    { CAP_WIERD4,      16, "\xe3\x62\xc1\xe9\x12\x1a\x4b\x94\xa6\x26\x7a\x74\xde\x24\x27\x0d", "CAP_WIERD4", NULL },
    { CAP_WIERD5,      16, "\xb9\x97\x08\xb5\x3a\x92\x42\x02\xb0\x69\xf1\xe7\x57\xbb\x2e\x17", "CAP_WIERD5", NULL },
    { CAP_WIERD7,      16, "\xb6\x07\x43\x78\xf5\x0c\x4a\xc7\x90\x92\x59\x38\x50\x2d\x05\x91", "CAP_WIERD7", NULL },
    { CAP_11,          16, "\x01\x01\x01\x01\x01\x01\x19\x04\x4a\x16\xed\x79\x2c\xb1\x71\x01", "CAP_11", NULL },
    { CAP_12,          16, "\x02\x02\x02\x02\x02\x02\xb3\xf8\x53\x44\x7f\x0d\x2d\x83\xbd\x76", "CAP_12", NULL },  

    { 0, 0, NULL, NULL, NULL }
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

#undef PacketC
Packet *PacketC (DEBUG0PARAM)
{
    Packet *pak;
    
    pak = calloc (1, sizeof (Packet));
    assert (pak);

    Debug (DEB_PACKET, "<--- %p new", pak);
    uiG.packets++;

    return pak;
}

#undef PacketCreate
Packet *PacketCreate (str_t str DEBUGPARAM)
{
    Packet *newpak;
    
    newpak = calloc (1, sizeof (Packet));
    assert (newpak);
    
    memcpy (newpak->data, str->txt, str->len);
    newpak->len = str->len;
    
    Debug (DEB_PACKET, "<-+- %p create", newpak);
    uiG.packets++;

    return newpak;
}

#undef PacketD
void PacketD (Packet *pak DEBUGPARAM)
{
    Debug (DEB_PACKET, "---> %p free", pak);
    uiG.packets--;
    free (pak);
}

#undef PacketClone
Packet *PacketClone (const Packet *pak DEBUGPARAM)
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
        if (id == CAP_CLIMM || id == CAP_MICQ)
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

    PacketWriteData (pak, (cap->var ? (const char *)cap->var : cap->cap), 16);
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

void PacketWriteLNTS2 (Packet *pak, str_t text)
{
    int len = text ? text->len : 0;
    char *data = text ? text->txt : "";

    assert (pak);
    assert (pak->wpos + 3 + len < PacketMaxData);
    
    PacketWrite2 (pak, len + 1);
    PacketWriteData (pak, data, len);
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

void PacketWriteCont (Packet *pak, Contact *cont)
{
    int len = strlen (cont->screen);
    PacketWrite1 (pak, len);
    PacketWriteData (pak, cont->screen, len);
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
            break;
    if (id == CAP_MAX)
        return &caps[0];
    
    p = malloc (16);
    assert (p);
    memcpy (p, cap, 16);

    caps[id].id = id;
    caps[id].cap = p;
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
        if (str->max > len)
        {
            str->len = len;
            str->txt[len] = '\0';
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
    if (str->txt[len - 1])
        str->txt[str->len = len] = '\0';
    else
        str->len = len - 1;
    return str;
}

strc_t PacketReadL4Str (Packet *pak, str_t str)
{
    size_t len;
    
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

strc_t PacketReadUIN (Packet *pak)
{
    UBYTE len = PacketRead1 (pak);
    str_t str;

    packetstrind %= 4;
    str = &packetstr[packetstrind++];
    s_init (str, "", len + 2);
    PacketReadData (pak, str, len);
    str->txt[len] = '\0';
    return str;
}

Contact *PacketReadCont (Packet *pak, Connection *serv)
{
    UBYTE len = PacketRead1 (pak);
    Contact *cont;
    str_s str = { NULL, 0, 0 };

    PacketReadData (pak, &str, len);
    str.txt[len] = '\0';
    cont = ContactScreen (serv, str.txt);
    s_done (&str);
    return cont;
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
        if (str->max > len)
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
    if (str->txt[len - 1])
        str->txt[str->len = len] = '\0';
    else
        str->len = len - 1;

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

