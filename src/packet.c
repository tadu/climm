
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
    Packet *pak = malloc (sizeof (Packet));
    
    assert (pak);
    
    pak->ver = 0;
    pak->id = 0;
    pak->bytes = 0;
    pak->cmd = 0;

    Debug (64, "<-- %p %s", pak, i18n (863, "creating new packet"));

    return pak;
}

Packet *PacketClone (const Packet *pak)
{
    Packet *newpak;
    const UBYTE *p;
    UBYTE *q;
    int i;
    
    newpak = malloc (sizeof (Packet));
    assert (newpak);
    
    newpak->ver   = pak->ver;
    newpak->id    = pak->id;
    newpak->bytes = pak->bytes;
    newpak->cmd   = pak->cmd;
    
    for (i = 0, p = pak->data, q = newpak->data; i < pak->bytes; i++)
        *q++ = *p++;
    
    return newpak;
}

void PacketWrite1 (Packet *pak, UBYTE data)
{
    assert (pak);
    assert (pak->bytes < PacketMaxData);

    pak->data[pak->bytes++] = data;
}

void PacketWrite2 (Packet *pak, UWORD data)
{
    assert (pak);
    assert (pak->bytes + 1 < PacketMaxData);

    pak->data[pak->bytes++] = data & 0xff;  data >>= 8;
    pak->data[pak->bytes++] = data;
}

void PacketWrite4 (Packet *pak, UDWORD data)
{
    assert (pak);
    assert (pak->bytes + 3 < PacketMaxData);

    pak->data[pak->bytes++] = data & 0xff;  data >>= 8;
    pak->data[pak->bytes++] = data & 0xff;  data >>= 8;
    pak->data[pak->bytes++] = data & 0xff;  data >>= 8;
    pak->data[pak->bytes++] = data;
}

void PacketWriteStr (Packet *pak, const char *data)
{
    assert (pak);
    assert (pak->bytes + strlen (data) < PacketMaxData);

    PacketWrite2 (pak, strlen (data) + 1);
    strcpy (&pak->data[pak->bytes], data);
    pak->bytes += strlen (data);
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

void PacketWriteAt1 (Packet *pak, UWORD at, UBYTE data)
{
    assert (pak);
    assert (at < PacketMaxData);

    pak->data[at] = data;
}

void PacketWriteAt2 (Packet *pak, UWORD at, UWORD data)
{
    assert (pak);
    assert (at + 1< PacketMaxData);

    pak->data[at++] = data & 0xff;  data >>= 8;
    pak->data[at]   = data;
}

void PacketWriteAt4 (Packet *pak, UWORD at, UDWORD data)
{
    assert (pak);
    assert (at + 3< PacketMaxData);

    pak->data[at++] = data & 0xff;  data >>= 8;
    pak->data[at++] = data & 0xff;  data >>= 8;
    pak->data[at++] = data & 0xff;  data >>= 8;
    pak->data[at]   = data;
}

UBYTE PacketReadAt1 (const Packet *pak, UWORD at)
{
    assert (pak);

    return pak->data[at];
}

UWORD PacketReadAt2 (const Packet *pak, UWORD at)
{
    UWORD data;

    assert (pak);

    data  = pak->data[at++];
    data |= pak->data[at] << 8;
    return data;
}

UDWORD PacketReadAt4 (const Packet *pak, UWORD at)
{
    UDWORD data;

    assert (pak);

    data  = pak->data[at++];
    data |= pak->data[at++] << 8;
    data |= pak->data[at++] << 16;
    data |= pak->data[at] << 24;
    return data;
}

const char *PacketReadAtStr (const Packet *pak, UWORD at)
{
    return &pak->data[at + 2]; /* ignore size */
}

