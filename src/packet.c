
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

    Debug (64, "<-- %p %s", pak, i18n (863, "creating new packet"));

    return pak;
}

void PacketD (Packet *pak)
{
    Debug (64, "--> %p %s", pak, i18n (860, "freeing packet"));
    free (pak);
}

Packet *PacketClone (const Packet *pak)
{
    Packet *newpak;
    
    newpak = malloc (sizeof (Packet));
    assert (newpak);
    
    memcpy (newpak, pak, sizeof (Packet));
    newpak->rpos = 0;
    
    Debug (64, "<+- %p %s %p", newpak, i18n (865, "cloning existing packet"), pak);

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

void PacketWriteStrB(Packet *pak, const char *data)
{
    PacketWriteB2 (pak, strlen (data));
    PacketWriteData (pak, data, strlen (data));
}

void PacketWriteStrN (Packet *pak, const char *data)
{
    assert (pak);
    assert (pak->wpos + strlen (data) < PacketMaxData);

    PacketWrite2 (pak, strlen (data) + 1);
    strcpy (&pak->data[pak->wpos], data);
    pak->wpos += strlen (data);
    PacketWrite1 (pak, 0);
}

void PacketWriteStrCUW (Packet *pak, const char *data)
{
    char *tmp = malloc (strlen (data) + 2);
    
    strcpy (tmp, data);
    ConvUnixWin (tmp);
    PacketWriteStrN (pak, tmp);
    free (tmp);
}

void PacketWriteUIN (Packet *pak, UDWORD uin)
{
    char str[15];
    
    snprintf (str, sizeof (str), "%ld", uin);
    PacketWrite1 (pak, strlen (str));
    PacketWriteData (pak, str, strlen (str));
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

void PacketWriteBAt2 (Packet *pak, UWORD at, UWORD data)
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

void PacketWriteBAt4 (Packet *pak, UWORD at, UDWORD data)
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
    assert (pak->rpos < PacketMaxData);

    return pak->data[pak->rpos++];
}

UWORD PacketRead2 (Packet *pak)
{
    UWORD data;

    assert (pak);
    assert (pak->rpos + 1 < PacketMaxData);

    data  = pak->data[pak->rpos++];
    data |= pak->data[pak->rpos++] << 8;
    return data;
}

UWORD PacketReadB2 (Packet *pak)
{
    UWORD data;

    assert (pak);
    assert (pak->rpos + 1 < PacketMaxData);

    data  = pak->data[pak->rpos++] << 8;
    data |= pak->data[pak->rpos++];
    return data;
}

UDWORD PacketRead4 (Packet *pak)
{
    UDWORD data;

    assert (pak);
    assert (pak->rpos + 3 < PacketMaxData);

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
    assert (pak->rpos + 3 < PacketMaxData);

    data  = pak->data[pak->rpos++] << 24;
    data |= pak->data[pak->rpos++] << 16;
    data |= pak->data[pak->rpos++] << 8;
    data |= pak->data[pak->rpos++];
    return data;
}

void PacketReadData (Packet *pak, char *buf, UWORD len)
{
    assert (pak);
    assert (pak->rpos + len < PacketMaxData);
    
    memcpy (buf, pak->data + pak->rpos, len);
    pak->rpos += len;
}

const char *PacketReadStrB (Packet *pak)
{
    UWORD len;
    char *str;
    
    assert (pak);
    assert (pak->rpos + 1 < PacketMaxData);
    
    len = PacketReadB2 (pak);
    str = malloc (len + 1);
    
    assert (str);
    assert (pak->rpos + len < PacketMaxData);

    memcpy (str, pak->data + pak->rpos, len);
    str[len] = '\0';
    
    pak->rpos += len;

    return str;
}

const char *PacketReadStrN (Packet *pak)
{
    UWORD len;
    
    assert (pak);
    assert (pak->rpos + 1 < PacketMaxData);
    
    len  = pak->data[pak->rpos++];
    len += pak->data[pak->rpos++] << 8;
    
    assert (pak->rpos + len < PacketMaxData);
    assert (!pak->data[pak->rpos + len]);

    pak->rpos += len;

    return &pak->data[pak->rpos - len];
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
    assert (at < PacketMaxData);

    return pak->data[at];
}

UWORD PacketReadAt2 (const Packet *pak, UWORD at)
{
    UWORD data;

    assert (pak);
    assert (at + 1 < PacketMaxData);

    data  = pak->data[at++];
    data |= pak->data[at] << 8;
    return data;
}

UWORD PacketReadBAt2 (const Packet *pak, UWORD at)
{
    UWORD data;

    assert (pak);
    assert (at + 1 < PacketMaxData);

    data  = pak->data[at++] << 8;
    data |= pak->data[at];
    return data;
}

UDWORD PacketReadAt4 (const Packet *pak, UWORD at)
{
    UDWORD data;

    assert (pak);
    assert (at + 3 < PacketMaxData);

    data  = pak->data[at++];
    data |= pak->data[at++] << 8;
    data |= pak->data[at++] << 16;
    data |= pak->data[at] << 24;
    return data;
}

UDWORD PacketReadBAt4 (const Packet *pak, UWORD at)
{
    UDWORD data;

    assert (pak);
    assert (at + 3 < PacketMaxData);

    data  = pak->data[at++] << 24;
    data |= pak->data[at++] << 16;
    data |= pak->data[at++] << 8;
    data |= pak->data[at];
    return data;
}

void PacketReadAtData (const Packet *pak, UWORD at, char *buf, UWORD len)
{
    assert (pak);
    assert (at + len < PacketMaxData);
    
    memcpy (buf, pak->data + at, len);
}

const char *PacketReadAtStrB (const Packet *pak, UWORD at)
{
    UWORD len;
    char *str;
    
    assert (pak);
    assert (at + 1 < PacketMaxData);
    
    len  = pak->data[at++] << 8;
    len += pak->data[at++];
    str = malloc (len + 1);
    
    assert (len);
    assert (at + len < PacketMaxData);

    memcpy (str, pak->data + at, len);
    str[len] = '\0';
    
    return str;
}

const char *PacketReadAtStrN (const Packet *pak, UWORD at)
{
    UWORD len;
    
    assert (pak);
    assert (at + 1 < PacketMaxData);
    
    len  = pak->data[at++];
    len += pak->data[at++] << 8;
    
    assert (at + len < PacketMaxData);
    assert (!pak->data[at + len]);

    return &pak->data[at];
}

int PacketReadLeft  (const Packet *pak)
{
    return pak->len - pak->rpos;
}
