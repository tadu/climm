/* $Id$ */

typedef struct { UWORD tag; UWORD len; UDWORD nr; char *str; } TLV;

TLV  *TLVRead (Packet *pak, UDWORD TLVlen);
UWORD TLVGet  (TLV *tlv, UWORD nr);
void  TLVDone (TLV *tlv, UWORD nr);
void  TLVD    (TLV *tlv);
