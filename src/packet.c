
/*
 * Assemble outgoing and dissect incoming packets.
 *
 * This file is Copyright © Rüdiger Kuhlmann; it may be distributed under
 * version 2 of the GPL licence.
 *
 * $Id$
 */

#include "micq.h"
#include "packet.h"
#include "conv.h"
#include "util_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

Packet *PacketC (void)
{
    Packet *pak;
    
    pak = calloc (1, sizeof (Packet));
    assert (pak);

    Debug (DEB_PACKET, "<-- %p %s", pak, i18n (1863, "creating new packet"));

    return pak;
}

void PacketD (Packet *pak)
{
    Debug (DEB_PACKET, "--> %p %s", pak, i18n (1946, "freeing packet"));
    free (pak);
}

Packet *PacketClone (const Packet *pak)
{
    Packet *newpak;
    
    newpak = malloc (sizeof (Packet));
    assert (newpak);
    
    memcpy (newpak, pak, sizeof (Packet));
    newpak->rpos = 0;
    
    Debug (DEB_PACKET, "<+- %p %s %p", newpak, i18n (1865, "cloning existing packet"), pak);

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

void PacketWriteDLNTS (Packet *pak, const char *data)
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
    PacketWriteAt4 (pak, pak->tpos, pak->wpos - pak->tpos - 2);
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

    str = malloc (len + 1);
    assert (str);
    
    PacketReadData (pak, str, len);
    str[len] = '\0';
    
    for (t = str; t - str < len; t+= strlen (t) + 1)
        ConvWinUnix (t);

    return str;
}

const char *PacketReadLNTS (Packet *pak)
{
    UWORD len;
    UBYTE *str;
    
    len = PacketRead2 (pak);
    str = pak->data + pak->rpos;

    if (!len)
        return "";
    if (str [len - 1])
        return "<invalid>";

    PacketWriteAt2 (pak, pak->rpos - 2, 0); /* clear string to prevent double recoding */
    PacketReadData (pak, NULL, len);
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

const char *PacketReadAtLNTS (Packet *pak, UWORD at)
{
    UWORD len;
    UBYTE *str;
    
    len = PacketReadAt2 (pak, at);
    str = pak->data + at + 2;

    if (!len)
        return "";
    if (str [len - 1])
        return "<invalid>";

    PacketWriteAt2 (pak, at - 2, 0);
    ConvWinUnix (str);
    return str;
}

int PacketReadLeft  (const Packet *pak)
{
    return pak->len - pak->rpos;
}
