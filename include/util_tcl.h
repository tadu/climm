#ifdef ENABLE_TCL
#ifndef MICQ_UTIL_TCL_H
#define MICQ_UTIL_TCL_H

typedef struct tcl_hook_st {
    char *filter;
    char *cmd;
    struct tcl_hook_st *next;
} tcl_hook_s, *tcl_hook_p;

typedef enum {TCL_FILE, TCL_CMD} Tcl_type;
typedef struct tcl_pref_st {
    Tcl_type type;
    char *file;
    struct tcl_pref_st *next;
} tcl_pref_s, *tcl_pref_p;

typedef void (tcl_callback)(const char *s);
#define TCL_CALLBACK(f) void f (const char *s)

#define TCL_COMMAND(f) int f (ClientData cd, Tcl_Interp *interp, int argc, const char *argv[])

void TCLInit ();
void TCLPrefAppend (Tcl_type type, char *file);
void TCLEvent (const char *type, const char *data);
void TCLMessage (Contact *from, const char *text);

#include "cmd_user.h"
jump_f CmdUserTclScript;

#endif /* MICQ_UTIL_TCL_H */
#endif /* ENABLE_TCL */
