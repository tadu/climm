
#define UIN_DELIMS ":|/" /* add space if you don't want your nick names to have spaces */
#define END_MSG_STR "."
#define CANCEL_MSG_STR "#"
#define W_SEPERATOR MESSCOL "============================================" NOCOL "\n"

#define CU_DEFAULT 1
#define CU_USER    2

typedef int (jump_f)(Session *sess, char *args, int data, int status);
#define JUMP_F(f) int f (Session *sess, char *args, int data, int status)

struct jumpstr {
    jump_f *f;
    const char *defname;
    const char *name;
    int unidle;
    int data;
};

typedef struct jumpstr jump_t;

jump_t *CmdUserTable (void);
jump_t *CmdUserLookup (const char *command, int flags);
const char *CmdUserLookupName (const char *command);

void CmdUser (Session *sess, const char *command);
void CmdUserInput (Session *sess, int *idle_val, int *idle_flag);


extern UDWORD last_uin;
