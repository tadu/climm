
typedef struct TLV_s TLV;

struct TLV_s
{
    UWORD uin;          /* 1 */
    const char *URL;    /* 4 */
    const char *server;
    const char *cookie;
    UWORD cookielen;
    UWORD error;        /* 8 */
};

TLV *TLVRead (Packet *pak);
void TLVD (TLV *tlv);
