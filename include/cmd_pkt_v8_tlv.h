/* $Id$ */

#include "util_str.h"

#ifndef MICQ_ICQV8_TLV_H
#define MICQ_ICQV8_TLV_H

typedef struct { UWORD tag; UDWORD nr; str_s str; } TLV;

TLV  *TLVRead (Packet *pak, UDWORD TLVlen);
UWORD TLVGet  (TLV *tlv, UWORD nr);
void  TLVDone (TLV *tlv, UWORD nr);
void  TLVD    (TLV *tlv);

#endif /* MICQ_ICQV8_TLV_H */
