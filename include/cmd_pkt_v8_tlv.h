
typedef struct { UWORD tlv; UWORD len; UDWORD nr; const char *str; } TLV;

TLV *TLVRead (Packet *pak);
void TLVD (TLV *tlv);
Packet *TLVPak (TLV *tlv);
