/*
 * Reads remaining TLVs from a SNAC. Unfortunately, not all packets are just TLVs.
 *
 * This file is Copyright © Rüdiger Kuhlmann; it may be distributed under
 * version 2 of the GPL licence.
 *
 * $Id$
 */

#include "micq.h"
#include "cmd_pkt_v8_tlv.h"
#include "util_ui.h"
#include <string.h>

TLV *TLVRead (Packet *pak)
{
    TLV *tlv = calloc (20, sizeof (TLV));
    char *p;
    int typ, len, pos, i = 16, n;

    while (PacketReadLeft (pak) >= 4 && i < 20)
    {
        typ = PacketReadB2 (pak);
        len = PacketReadB2 (pak);
        if (PacketReadLeft (pak) < len)
        {
            M_print (i18n (1897, "Incomplete TLV %d, len %d of %d - ignoring.\n"), typ, PacketReadLeft (pak), len);
            return tlv;
        }
        if (typ >= 16 || tlv[typ].len)
            n = i++;
        else
            n = typ;

        p = malloc (len + 1);
        pos = PacketReadPos (pak);
        PacketReadData (pak, p, len);
        p[len] = '\0';

        tlv[n].tlv = typ;
        tlv[n].len = len;
        tlv[n].str = p;
        
        if (len == 2)
            tlv[n].nr = PacketReadBAt2 (pak, pos);
        if (len == 4)
            tlv[n].nr = PacketReadBAt4 (pak, pos);
    }
    return tlv;
}

void TLVD (TLV *tlv)
{
    int i;
    for (i = 0; i < 20; i++)
        if (tlv[i].str)
            free ((char *)tlv[i].str);
    free (tlv);
}

Packet *TLVPak (TLV *tlv)
{
    Packet *pak;
    
    pak = PacketC ();
    pak->len = tlv->len;
    pak->cmd = tlv->tlv;
    pak->id  = tlv->nr;
    memcpy (pak->data, tlv->str, tlv->len);
    return pak;
}
