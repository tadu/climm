

typedef void (jump_srv_f)(Session *sess, Packet *pak, UWORD cmd, UWORD ver, UDWORD seq, UDWORD uin);
#define JUMP_SRV_F(f) void f (Session *sess, Packet *pak, UWORD cmd, UWORD ver, UDWORD seq, UDWORD uin)

struct jumpsrvstr {
    int cmd;
    jump_srv_f *f;
    const char *cmdname;
};

typedef struct jumpsrvstr jump_srv_t;

const char *CmdPktSrvName (int cmd);
void CmdPktSrvRead (Session *sess);
JUMP_SRV_F(CmdPktSrvProcess);
