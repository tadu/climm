/*
 * Reads remaining TLVs from a SNAC. Unfortunately, not all packets are just TLVs.
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
 */

#include "micq.h"
#include "cmd_pkt_v8_tlv.h"
#include "util_ui.h"
#include "packet.h"
#include "util_str.h"
#include <string.h>
#include <assert.h>

#define __maxTLV  25
#define __minTLV  16

/*
 * Read in given amount of data and parse as TLVs.
 *
 * The resulting array of TLV does not have pointers into the given Packet.
 */
TLV *TLVRead (Packet *pak, UDWORD TLVlen)
{
    TLV *tlv = calloc (__maxTLV, sizeof (TLV));
    int typ, len, pos, i = __minTLV, n, max = __maxTLV;

    assert (tlv);

    while (TLVlen >= 4)
    {
        typ = PacketReadB2 (pak);
        len = PacketReadB2 (pak);
        if (TLVlen < len)
        {
            M_printf (i18n (1897, "Incomplete TLV %d, len %ld of %d - ignoring.\n"), typ, TLVlen, len);
            return tlv;
        }
        if (typ >= __minTLV || tlv[typ].str.len)
            n = i++;
        else
            n = typ;

        if (i + 1 == max)
        {
            TLV *tlvn;

            max += 5;
            tlvn = calloc (max, sizeof (TLV));
            
            assert (tlvn);
            
            memcpy (tlvn, tlv, (max - 5) * sizeof (TLV));
            free (tlv);

            tlv = tlvn;
        }

        pos = PacketReadPos (pak);
        if (len == 2)
            tlv[n].nr = PacketReadAtB2 (pak, pos);
        if (len == 4)
            tlv[n].nr = PacketReadAtB4 (pak, pos);
        
        PacketReadData (pak, &tlv[n].str, len);
        if (tlv[n].str.max < len)
            return tlv;
        
        tlv[n].str.txt[len] = '\0';
        tlv[n].tag = typ;
        
        TLVlen -= 2 + 2 + len;
    }
    return tlv;
}

/*
 * Search for a given TLV.
 */
UWORD TLVGet (TLV *tlv, UWORD nr)
{
    UWORD i;

    for (i = 0; i < __minTLV; i++)
        if (tlv[i].tag == nr)
            return i;
    for ( ; tlv[i].tag; i++)
        if (tlv[i].tag == nr)
            return i;

    return (UWORD)-1;
}

/*
 * Free and remove a TLV from the array.
 *
 * Another TLV with the same number will be moved into this slot if it
 * exists.
 */
void TLVDone (TLV *tlv, UWORD nr)
{
    int i;
    s_done (&tlv[nr].str);
    tlv[nr].tag = 0;
    if (nr < __minTLV)
    {
        for (i = __minTLV; tlv[i].tag; i++)
            if (tlv[i].tag == nr)
                break;
        if (!tlv[i].tag)
            return;
        tlv[nr] = tlv[i];
        nr = i;
    }
    i = nr + 1;
    while (tlv[i].tag)
        i++;
    if (i == nr + 1)
        return;
    i--;
    tlv[nr] = tlv[i];
    tlv[i].tag = 0;
    tlv[i].str.txt = NULL;
    tlv[i].str.len = 0;
    tlv[i].str.max = 0;
}

/*
 * Free the array of TLV.
 */
void TLVD (TLV *tlv)
{
    int i;
    for (i = 0; i < __minTLV; i++)
        s_done (&tlv[i].str);
    for ( ; tlv[i].tag; i++)
        s_done (&tlv[i].str);
    free (tlv);
}
