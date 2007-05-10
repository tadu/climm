/* $Id$ */

#ifndef MICQ_IM_CLI_H
#define MICQ_IM_CLI_H

typedef enum {
  auth_req = 1,
  auth_grant,
  auth_deny,
  auth_add
} auth_t;

UBYTE IMCliMsg (Connection *conn, Contact *cont, Opt *opt);
void  IMSetStatus (Connection *conn, Contact *cont, status_t status, const char *msg);
UBYTE IMCliReMsg  (Connection *conn, Contact *cont, Opt *opt); /* no log */
void  IMCliInfo   (Connection *conn, Contact *cont, int group);
void  IMCliAuth   (Contact *cont, const char *msg, auth_t how);

#endif /* MICQ_IM_CLI_H */
