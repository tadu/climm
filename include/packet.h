
#ifndef MICQ_PACKET_H
#define MICQ_PACKET_H

#define PacketMaxData 4000

struct Packet_s
{
    UDWORD   id;
    UWORD    len;
    UBYTE    ver;
    UDWORD   cmd;

    UWORD    rpos, wpos;
    UBYTE    socks[10];
    UBYTE    data[PacketMaxData];
};

Packet *PacketC        (void);
void    PacketD        (Packet *pak);
Packet *PacketClone    (const Packet *pak);

void        PacketWrite1      (      Packet *pak,           UBYTE  data);
void        PacketWrite2      (      Packet *pak,           UWORD  data);
void        PacketWriteB2     (      Packet *pak,           UWORD  data);
void        PacketWrite4      (      Packet *pak,           UDWORD data);
void        PacketWriteB4     (      Packet *pak,           UDWORD data);
void        PacketWriteData   (      Packet *pak,           const char *data, UWORD len);
void        PacketWriteStrB   (      Packet *pak,           const char *data);
void        PacketWriteLNTS   (      Packet *pak,           const char *data);
void        PacketWriteStrCUW (      Packet *pak,           const char *data);
void        PacketWriteUIN    (      Packet *pak, UDWORD uin);
UWORD       PacketWritePos    (const Packet *pak);
void        PacketWriteAt1    (      Packet *pak, UWORD at, UBYTE  data);
void        PacketWriteAt2    (      Packet *pak, UWORD at, UWORD  data);
void        PacketWriteBAt2   (      Packet *pak, UWORD at, UWORD  data);
void        PacketWriteAt4    (      Packet *pak, UWORD at, UDWORD data);
void        PacketWriteBAt4   (      Packet *pak, UWORD at, UDWORD data);
UBYTE       PacketRead1       (      Packet *pak);
UWORD       PacketRead2       (      Packet *pak);
UWORD       PacketReadB2      (      Packet *pak);
UDWORD      PacketRead4       (      Packet *pak);
UDWORD      PacketReadB4      (      Packet *pak);
const char *PacketReadSkip    (      Packet *pak,                       UWORD len);
void        PacketReadData    (      Packet *pak,           char *data, UWORD len);
char       *PacketReadStrB    (      Packet *pak);
const char *PacketReadLNTS    (      Packet *pak);
UDWORD      PacketReadUIN     (      Packet *pak);
UWORD       PacketReadPos     (const Packet *pak);
UBYTE       PacketReadAt1     (const Packet *pak, UWORD at);
UWORD       PacketReadAt2     (const Packet *pak, UWORD at);
UWORD       PacketReadBAt2    (const Packet *pak, UWORD at);
UDWORD      PacketReadAt4     (const Packet *pak, UWORD at);
UDWORD      PacketReadBAt4    (const Packet *pak, UWORD at);
void        PacketReadAtData  (const Packet *pak, UWORD at, char *data, UWORD len);
char       *PacketReadAtStrB  (const Packet *pak, UWORD at);
const char *PacketReadAtLNTS  (const Packet *pak, UWORD at);
int         PacketReadLeft    (const Packet *pak);

#define PacketWriteTLV2(pak,tlv,data)        do { PacketWriteB2 (pak, tlv); PacketWriteB2   (pak, 2);   PacketWriteB2 (pak, data);        } while (0)
#define PacketWriteTLV4(pak,tlv,data)        do { PacketWriteB2 (pak, tlv); PacketWriteB2   (pak, 4);   PacketWriteB4 (pak, data);        } while (0)
#define PacketWriteTLVData(pak,tlv,data,len) do { PacketWriteB2 (pak, tlv); PacketWriteB2   (pak, len); PacketWriteData (pak, data, len); } while (0)
#define PacketWriteTLVStr(pak,tlv,data)      do { PacketWriteB2 (pak, tlv); PacketWriteStrB (pak, data);                                  } while (0)
#define PacketWriteTLVUIN(pak,tlv,data)      do { PacketWriteB2 (pak, tlv); PacketWriteUIN  (pak, data);                                  } while (0)

#endif /* MICQ_PACKET_H */
