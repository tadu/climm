/* $ Id: $ */

#define CLI_HELLO 1

#define FLAP_VER_MAJOR       5
#define FLAP_VER_MINOR      15
#define FLAP_VER_LESSER      1
#define FLAP_VER_BUILD    3638
#define FLAP_VER_SUBBUILD   85

void FlapCliHello (Session *sess);
void FlapCliIdent (Session *sess);
void FlapCliCookie (Session *sess, const char *cookie, UWORD len);
void FlapCliGoodbye (Session *sess);
void FlapCliKeepalive (Session *sess);

void SrvCallBackFlap (struct Event *event);

Packet *FlapC (UBYTE channel);
void    FlapSend (Session *sess, Packet *pak);
void    FlapPrint (Packet *pak);
void    FlapSave (Packet *pak, BOOL in);
