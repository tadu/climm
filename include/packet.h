/* $Id$ */

#ifndef MICQ_PACKET_H
#define MICQ_PACKET_H

#define PacketMaxData 4096

struct Packet_s
{
    UDWORD   id;
    UWORD    len;
    UBYTE    ver;
    UDWORD   cmd;
    UDWORD   flags;

    UWORD    rpos, wpos, tpos;
    UBYTE    socks[10];
    UBYTE    data[PacketMaxData];
};

typedef struct { UBYTE id; const char *cap; const char *name; } Cap;

#define CAP_NONE     0
#define CAP_ISICQ    1
#define CAP_SRVRELAY 2
#define CAP_RTFMSGS  3
#define CAP_UNK_2001 4
#define CAP_UNK_2002 5
#define CAP_STR_2001 6
#define CAP_STR_2002 7
#define CAP_MICQ     8
                    
Packet *PacketC        (void);
Packet *PacketCreate   (const char *data, UDWORD len);
Packet *PacketClone    (const Packet *pak);
void    PacketD        (Packet *pak);

void        PacketWrite1      (      Packet *pak,           UBYTE  data);
void        PacketWrite2      (      Packet *pak,           UWORD  data);
void        PacketWriteB2     (      Packet *pak,           UWORD  data);
void        PacketWrite4      (      Packet *pak,           UDWORD data);
void        PacketWriteB4     (      Packet *pak,           UDWORD data);
void        PacketWriteCap    (      Packet *pak,           UBYTE id);
void        PacketWriteData   (      Packet *pak,           const char *data, UWORD len);
void        PacketWriteStr    (      Packet *pak,           const char *data);
void        PacketWriteLNTS   (      Packet *pak,           const char *data);
void        PacketWriteDLStr  (      Packet *pak,           const char *data);
void        PacketWriteLLNTS  (      Packet *pak,           const char *data);
void        PacketWriteUIN    (      Packet *pak, UDWORD uin);
void        PacketWriteTLV    (      Packet *pak, UDWORD type);
void        PacketWriteTLVDone(      Packet *pak);
void        PacketWriteLen    (      Packet *pak);
void        PacketWriteLenDone(      Packet *pak);
void        PacketWriteLen4   (      Packet *pak);
void        PacketWriteLen4Done(     Packet *pak);

void        PacketWriteAt1    (      Packet *pak, UWORD at, UBYTE  data);
void        PacketWriteAt2    (      Packet *pak, UWORD at, UWORD  data);
void        PacketWriteAtB2   (      Packet *pak, UWORD at, UWORD  data);
void        PacketWriteAt4    (      Packet *pak, UWORD at, UDWORD data);
void        PacketWriteAtB4   (      Packet *pak, UWORD at, UDWORD data);

UBYTE       PacketRead1       (      Packet *pak);
UWORD       PacketRead2       (      Packet *pak);
UWORD       PacketReadB2      (      Packet *pak);
UDWORD      PacketRead4       (      Packet *pak);
UDWORD      PacketReadB4      (      Packet *pak);
Cap        *PacketReadCap     (      Packet *pak);
void        PacketReadData    (      Packet *pak,           char  *data, UWORD len);
char       *PacketReadStrB    (      Packet *pak);
char       *PacketReadLNTS    (      Packet *pak);
char       *PacketReadDLStr   (      Packet *pak);
UDWORD      PacketReadUIN     (      Packet *pak);

UBYTE       PacketReadAt1     (const Packet *pak, UWORD at);
UWORD       PacketReadAt2     (const Packet *pak, UWORD at);
UWORD       PacketReadAtB2    (const Packet *pak, UWORD at);
UDWORD      PacketReadAt4     (const Packet *pak, UWORD at);
UDWORD      PacketReadAtB4    (const Packet *pak, UWORD at);
void        PacketReadAtData  (const Packet *pak, UWORD at, char  *data, UWORD len);
char       *PacketReadAtStrB  (const Packet *pak, UWORD at);
char       *PacketReadAtLNTS  (      Packet *pak, UWORD at);

UWORD       PacketWritePos    (const Packet *pak);
UWORD       PacketReadPos     (const Packet *pak);
int         PacketReadLeft    (const Packet *pak);

Cap        *PacketCap         (UBYTE id);
const char *PacketDump        (      Packet *pak, const char *syntax);

#define PacketWriteTLV2(pak,tlv,data)        do { PacketWriteB2 (pak, tlv); PacketWriteB2   (pak, 2);   PacketWriteB2 (pak, data);        } while (0)
#define PacketWriteTLV4(pak,tlv,data)        do { PacketWriteB2 (pak, tlv); PacketWriteB2   (pak, 4);   PacketWriteB4 (pak, data);        } while (0)
#define PacketWriteTLVData(pak,tlv,data,len) do { PacketWriteB2 (pak, tlv); PacketWriteB2   (pak, len); PacketWriteData (pak, data, len); } while (0)
#define PacketWriteTLVStr(pak,tlv,data)      do { PacketWriteTLV (pak, tlv); PacketWriteStr (pak, data);PacketWriteTLVDone (pak);         } while (0)
#define PacketWriteTLVUIN(pak,tlv,data)      do { PacketWriteB2 (pak, tlv); PacketWriteUIN  (pak, data);                                  } while (0)
#define PacketWriteStrB(pak,str)             do { PacketWriteB2 (pak, strlen (str)); PacketWriteStr (pak, str);                           } while (0)

#endif /* MICQ_PACKET_H */
