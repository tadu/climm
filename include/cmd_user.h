/* $Id$ */

#ifndef MICQ_USER_H
#define MICQ_USER_H

#define END_MSG_STR    "."
#define CANCEL_MSG_STR "#"
#define W_SEPARATOR COLQUOTE, "============================================", COLNONE, "\n"

typedef int (jump_f)(const char *args, UDWORD data, int status);
#define JUMP_F(f) int f (const char *args, UDWORD data, int status)

struct jumpstr {
    jump_f *f;
    const char *name;
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
jump_t *CmdUserLookup (const char *command);

alias_t *CmdUserAliases (void);

void CmdUser (const char *command);
void CmdUserInput (strc_t line);
void CmdUserInterrupt (void);

#define CMD_USER_HELP(syn,des) M_printf ("%s" syn "%s\n\t" COLINDENT "%s" COLEXDENT "\n", COLQUOTE, COLNONE, des);
#define CMD_USER_HELP3(syn,d,e,f) M_printf ("%s" syn "%s\n\t" COLINDENT "%s" COLEXDENT "\n", COLQUOTE, COLNONE, d, e, f);
#define CMD_USER_HELP7(syn,a,b,c,d,e,f,g) M_printf ("%s" syn "%s\n\t" COLINDENT "%s\n%s\n%s\n%s\n%s\n%s\n%s" COLEXDENT "\n", COLQUOTE, COLNONE, a,b,c,d,e,f,g);

#endif /* MICQ_USER_H */
