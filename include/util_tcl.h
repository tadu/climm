#ifndef CLIMM_UTIL_TCL_H
#define CLIMM_UTIL_TCL_H

#ifdef ENABLE_TCL
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
void TCLEvent (Contact *from, const char *type, const char *data);
void TCLMessage (Contact *from, const char *text);

#include "im_response.h"
int cb_status_tcl (Contact *cont, parentmode_t pm, change_t ch, const char *text);
int cb_int_msg_tcl (Contact *cont, parentmode_t pm, time_t stamp, fat_int_msg_t *msg);
int cb_srv_msg_tcl (Contact *cont, parentmode_t pm, time_t stamp, fat_srv_msg_t *msg);

#include "cmd_user.h"
jump_f CmdUserTclScript;

#endif
#endif /* CLIMM_UTIL_TCL_H */
