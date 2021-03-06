/* $Id$ */

#ifndef CLIMM_IM_CLI_H
#define CLIMM_IM_CLI_H

typedef enum {
    auth_req = 1,
    auth_grant,
    auth_deny,
    auth_add
} auth_t;

struct Message_s {
    Contact *cont;
    char *send_message;
    char *plain_message;

    UDWORD type;
    UDWORD origin;
    UDWORD trans;

    UDWORD maxsize; /* to handle too long messages */
    UBYTE maxenc;

    int otrinjected:1;
    int otrencrypted:2;
    int force:1;
};


Message *MsgC (void);
void     MsgD (Message *msg);

UBYTE  IMCliMsg    (Contact *cont, UDWORD type, const char *msg, Opt *opt);
void   IMSetStatus (Server *serv, Contact *cont, status_t status, const char *msg);
UBYTE  IMCliReMsg  (Contact *cont, Message *msg); /* no log */
void   IMCliInfo   (Server *serv, Contact *cont, int group);
void   IMCliAuth   (Contact *cont, const char *msg, auth_t how);
Event *IMLogin     (Server *serv);
void   IMCallBackReconn (Connection *conn);
void   IMConnOpen  (Connection *conn);
void   IMLogout    (Server *serv);

#endif /* CLIMM_IM_CLI_H */
