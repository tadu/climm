/* $ Id: $ */

typedef struct { UWORD tlv; UWORD len; UDWORD nr; const char *str; } TLV;

TLV  *TLVRead (Packet *pak, UDWORD TLVlen);
UWORD TLVGet  (TLV *tlv, UWORD nr);
void  TLVD    (TLV *tlv);
Packet *TLVPak (TLV *tlv);
