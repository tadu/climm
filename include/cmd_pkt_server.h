

typedef void (jump_srv_f)(SOK_T sok, UBYTE *data, int len, UWORD cmd, UWORD ver, UDWORD seq, UDWORD uin);
#define JUMP_SRV_F(f) void f (SOK_T sok, UBYTE *data, int len, UWORD cmd, \
                              UWORD ver, UDWORD seq, UDWORD uin)

struct jumpsrvstr {
    int cmd;
    jump_srv_f *f;
    const char *cmdname;
};

typedef struct jumpsrvstr jump_srv_t;

const char *CmdPktSrvName (int cmd);
void CmdPktSrvRead (SOK_T sok);
JUMP_SRV_F(CmdPktSrvProcess);
