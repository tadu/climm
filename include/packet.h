
#ifndef MICQ_PACKET_H
#define MICQ_PACKET_H

#define PacketMaxData 4000

typedef struct
{
    UDWORD   id;
    UWORD    len;
    UBYTE    ver;
    UDWORD   cmd;

    UBYTE    rpos, wpos;
    UBYTE    socks[10];
    UBYTE    data[PacketMaxData];
} Packet;

Packet *PacketC        (void);
Packet *PacketClone    (const Packet *pak);

void        PacketWrite1    (      Packet *pak,           UBYTE  data);
void        PacketWrite2    (      Packet *pak,           UWORD  data);
void        PacketWrite4    (      Packet *pak,           UDWORD data);
void        PacketWriteStr  (      Packet *pak,           const char *data);
void      PacketWriteStrCUW (      Packet *pak,           const char *data);
UWORD       PacketWritePos  (const Packet *pak);
void        PacketWriteAt1  (      Packet *pak, UWORD at, UBYTE  data);
void        PacketWriteAt2  (      Packet *pak, UWORD at, UWORD  data);
void        PacketWriteAt4  (      Packet *pak, UWORD at, UDWORD data);
UBYTE       PacketRead1     (      Packet *pak);
UWORD       PacketRead2     (      Packet *pak);
UDWORD      PacketRead4     (      Packet *pak);
const char *PacketReadStr   (      Packet *pak);
UWORD       PacketReadPos   (const Packet *pak);
UBYTE       PacketReadAt1   (const Packet *pak, UWORD at);
UWORD       PacketReadAt2   (const Packet *pak, UWORD at);
UDWORD      PacketReadAt4   (const Packet *pak, UWORD at);
const char *PacketReadAtStr (const Packet *pak, UWORD at);

#endif MICQ_PACKET_H
