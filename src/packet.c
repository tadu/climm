
/*
 * Assemble outgoing and dissect incoming packets.
 *
 * This file is © Rüdiger Kuhlmann; it may be distributed under the BSD
 * licence (without the advertising clause) or version 2 of the GPL.
 */

#include "micq.h"
#include "packet.h"
#include "conv.h"
#include "util_ui.h"
#include <assert.h>

Packet *PacketC (void)
{
    Packet *pak;
    
    pak = calloc (1, sizeof (Packet));
    assert (pak);

    Debug (64, "<-- %p %s", pak, i18n (863, "creating new packet"));

    return pak;
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

void PacketWriteStr (Packet *pak, const char *data)
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
    PacketWriteStr (pak, tmp);
    free (tmp);
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

const char *PacketReadStr (Packet *pak)
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

const char *PacketReadAtStr (const Packet *pak, UWORD at)
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

