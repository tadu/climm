
#include "micq.h"
#include "cmd_pkt_v8_tlv.h"
#include "util_ui.h"

TLV *TLVRead (Packet *pak)
{
    TLV *tlv = calloc (1, sizeof (TLV));
    char *p;
    int typ, len, val;

    while (PacketReadLeft (pak) >= 4)
    {
        typ = PacketReadB2 (pak);
        len = PacketReadB2 (pak);
        if (PacketReadLeft (pak) < len)
        {
            M_print (i18n (897, "Incomplete TLV %d, len %d of %d - ignoring.\n"), typ, PacketReadLeft (pak), len);
            return tlv;
        }
        printf ("TLV %d len %d\n", typ, len);
        switch (typ)
        {
            case 1:
                p = malloc (len + 1);
                PacketReadData (pak, p, len);
                p[len] = '\0';
                tlv->uin = atoi (p);
                free (p);
                break;
            case 4:
                p = malloc (len + 1);
                PacketReadData (pak, p, len);
                p[len]= '\0';
                tlv->URL = p;
                break;
            case 5:
                p = malloc (len + 1);
                PacketReadData (pak, p, len);
                p[len] = '\0';
                tlv->server = p;
                break;
            case 6:
                p = malloc (len + 1);
                PacketReadData (pak, p, len);
                p[len] = '\0';
                tlv->cookie = p;
                tlv->cookielen = len;
                break;
            case 8:
                tlv->error = (len == 2 ? PacketReadB2 (pak) : (len == 4 ? PacketReadB4 (pak) : (pak->rpos += len), -1));
                break;
            default:
                if (len == 2)
                {
                    val = PacketReadB2 (pak);
                    M_print (i18n (900, "Unknown TLV %d, value %d %4x\n"), tlv, val, val);
                }
                else if (len == 4)
                {
                    val = PacketReadB4 (pak);
                    M_print (i18n (901, "Unknown TLV %d, value %d %8x\n"), tlv, val, val);
                }
                else
                {
                    pak->rpos -= 2;
                    M_print (i18n (902, "Unknown TLV %d, value %s\n"), tlv, PacketReadStrB (pak));
                }
        }
    }
    return tlv;
}

