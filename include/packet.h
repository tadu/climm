
#ifndef MICQ_PACKET_H
#define MICQ_PACKET_H

#define PacketMaxData 4000

typedef struct
{
    UDWORD   id;
    UWORD    bytes;
    UBYTE    ver;
    UDWORD   cmd;
    UBYTE    data[PacketMaxData];
} Packet;

Packet *PacketC        (void);

void PacketWrite1      (Packet *pak, UBYTE  data);
void PacketWrite2      (Packet *pak, UWORD  data);
void PacketWrite4      (Packet *pak, UDWORD data);
void PacketWriteStr    (Packet *pak, const char *data);
void PacketWriteStrCUW (Packet *pak, const char *data);
void PacketWriteAt1    (Packet *pak, UWORD at, UBYTE  data);
void PacketWriteAt2    (Packet *pak, UWORD at, UWORD  data);
void PacketWriteAt4    (Packet *pak, UWORD at, UDWORD data);

UBYTE  PacketReadAt1   (Packet *pak, UWORD at);
UWORD  PacketReadAt2   (Packet *pak, UWORD at);
UDWORD PacketReadAt4   (Packet *pak, UWORD at);

#endif MICQ_PACKET_H
