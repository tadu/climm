/* $Id$ */

#ifndef CLIMM_OSCAR_TLV_H
#define CLIMM_OSCAR_TLV_H

typedef struct { UWORD tag; UDWORD nr; str_s str; } TLV;

TLV  *TLVRead (Packet *pak, UDWORD TLVlen, UDWORD TLVCount);
UWORD TLVGet  (TLV *tlv, UWORD nr);
void  TLVDone (TLV *tlv, UWORD nr);
void  TLVD    (TLV *tlv);

#endif
