/* $Id$ */

#ifndef MICQ_USER_H
#define MICQ_USER_H

#define END_MSG_STR    "."
#define CANCEL_MSG_STR "#"
#define W_SEPARATOR COLQUOTE, "============================================", COLNONE, "\n"

#define CU_DEFAULT 1
#define CU_USER    2

typedef int (jump_f)(const char *args, UDWORD data, UDWORD status);
#define JUMP_F(f) int f (const char *args, UDWORD data, UDWORD status)

struct jumpstr {
    jump_f *f;
    const char *defname;
    char *name;
    int unidle;
    int data;
};

typedef struct jumpstr jump_t;

struct aliasstr {
    char *name;
    char *expansion;
    struct aliasstr *next;
};

typedef struct aliasstr alias_t;
	

jump_t *CmdUserTable (void);
jump_t *CmdUserLookup (const char *command, int flags);
const char *CmdUserLookupName (const char *command);

alias_t *CmdUserAliases (void);

void CmdUser (const char *command);
void CmdUserInput (time_t *idle_val, UBYTE *idle_flag);

#endif /* MICQ_USER_H */
