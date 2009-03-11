/* $Id$ */

#ifndef CLIMM_PACKET_H
#define CLIMM_PACKET_H

#define PacketMaxData 1024 * 8

struct Packet_s
{
    UDWORD   id;
    UWORD    len;
    UBYTE    ver;
    UDWORD   cmd;
    UDWORD   ref;
    UDWORD   flags;

    UWORD    rpos, wpos, tpos;
    UBYTE    data[PacketMaxData];
};

struct Cap_s
{
    UBYTE id;
    UBYTE len;
    const char *cap;
    const char *name;
    char *var;
};

typedef enum {
  CAP_NONE = 0, CAP_AIM_VOICE, CAP_AIM_SFILE, CAP_ISICQ, CAP_AIM_IMIMAGE,
  CAP_AIM_BUDICON, CAP_AIM_STOCKS, CAP_AIM_GETFILE, CAP_SRVRELAY, CAP_AIM_GAMES,
  CAP_AIM_SBUD, CAP_AIM_INTER, CAP_AVATAR,
  CAP_UTF8, CAP_RTFMSGS, CAP_IS_2001, CAP_STR_2001,
  CAP_STR_2002, CAP_AIM_CHAT, CAP_TYPING, CAP_XTRAZ,
  CAP_TRILL_CRYPT, CAP_TRILL_2, CAP_LICQ, CAP_LICQNEW, CAP_SIM, CAP_SIMNEW,
  CAP_MACICQ, CAP_CLIMM, CAP_MICQ, CAP_KXICQ, CAP_KOPETE,
  CAP_IMSECURE, CAP_ARQ, CAP_MIRANDA, CAP_QIP, CAP_IM2,
  CAP_UTF8ii,
  CAP_WIERD1, CAP_WIERD3, CAP_WIERD4, CAP_WIERD5, CAP_WIERD7,
  CAP_11, CAP_12,
  CAP_MAX = 63
} cap_id_t;

#define CAP_GID_UTF8    "{0946134E-4C7F-11D1-8222-444553540000}"

#define HAS_CAP(caps,cap) (((caps)[(cap) / 32]) & (1UL << ((cap) % 32)))
#define SET_CAP(caps,cap) ((caps)[(cap) / 32]) |= (1UL << ((cap) % 32))
#define CLR_CAP(caps,cap) ((caps)[(cap) / 32]) &= ~(1UL << ((cap) % 32))

Packet *PacketC        (DEBUG0PARAM);
Packet *PacketCreate   (str_t str DEBUGPARAM);
Packet *PacketClone    (const Packet *pak DEBUGPARAM);
void    PacketD        (Packet *pak DEBUGPARAM);

#define PacketC()       PacketC(DEBUG0ARGS)
#define PacketCreate(s) PacketCreate(s DEBUGARGS)
#define PacketClone(p)  PacketClone(p DEBUGARGS)
#define PacketD(p)      PacketD(p DEBUGARGS)

void        PacketWrite1      (      Packet *pak,           UBYTE  data);
void        PacketWrite2      (      Packet *pak,           UWORD  data);
void        PacketWriteB2     (      Packet *pak,           UWORD  data);
void        PacketWrite4      (      Packet *pak,           UDWORD data);
void        PacketWriteB4     (      Packet *pak,           UDWORD data);
void        PacketWriteCap    (      Packet *pak,           Cap *cap);
void        PacketWriteCapID  (      Packet *pak,           UBYTE id);
void        PacketWriteData   (      Packet *pak,           const char *data, UWORD len);
void        PacketWriteStr    (      Packet *pak,           const char *data);
void        PacketWriteLNTS   (      Packet *pak,           const char *data);
void        PacketWriteLNTS2  (      Packet *pak,           str_t data);
void        PacketWriteDLStr  (      Packet *pak,           const char *data);
void        PacketWriteLLNTS  (      Packet *pak,           const char *data);
void        PacketWriteCont   (      Packet *pak, Contact *cont);
void        PacketWriteTLV    (      Packet *pak, UDWORD type);
void        PacketWriteTLVDone(      Packet *pak);
void        PacketWriteLen    (      Packet *pak);
void        PacketWriteLenDone(      Packet *pak);
void        PacketWriteBLenDone(     Packet *pak);
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
void        PacketReadData    (      Packet *pak, str_t str, UWORD len);
strc_t      PacketReadB2Str   (      Packet *pak, str_t str);
strc_t      PacketReadL2Str   (      Packet *pak, str_t str);
strc_t      PacketReadL4Str   (      Packet *pak, str_t str);
strc_t      PacketReadUIN     (      Packet *pak);
Contact    *PacketReadCont    (      Packet *pak, Server *serv);

UBYTE       PacketReadAt1     (const Packet *pak, UWORD at);
UWORD       PacketReadAt2     (const Packet *pak, UWORD at);
UWORD       PacketReadAtB2    (const Packet *pak, UWORD at);
UDWORD      PacketReadAt4     (const Packet *pak, UWORD at);
UDWORD      PacketReadAtB4    (const Packet *pak, UWORD at);
void        PacketReadAtData  (const Packet *pak, UWORD at, str_t str, UWORD len);
strc_t      PacketReadAtL2Str (const Packet *pak, UWORD at, str_t str);

UWORD       PacketWritePos    (const Packet *pak);
UWORD       PacketReadPos     (const Packet *pak);
int         PacketReadLeft    (const Packet *pak);

Cap        *PacketCap         (UBYTE id);

#define PacketWriteTLV2(pak,tlv,data)        do { PacketWriteB2 (pak, tlv); PacketWriteB2   (pak, 2);   PacketWriteB2 (pak, data);        } while (0)
#define PacketWriteTLV4(pak,tlv,data)        do { PacketWriteB2 (pak, tlv); PacketWriteB2   (pak, 4);   PacketWriteB4 (pak, data);        } while (0)
#define PacketWriteTLVData(pak,tlv,data,len) do { PacketWriteB2 (pak, tlv); PacketWriteB2   (pak, len); PacketWriteData (pak, data, len); } while (0)
#define PacketWriteTLVStr(pak,tlv,data)      do { PacketWriteTLV (pak, tlv); PacketWriteStr (pak, data);PacketWriteTLVDone (pak);         } while (0)
#define PacketWriteStrB(pak,str)             do { PacketWriteB2 (pak, strlen (str)); PacketWriteStr (pak, str);                           } while (0)
#define PacketWriteBLen(pak) PacketWriteLen(pak)

#endif /* CLIMM_PACKET_H */
