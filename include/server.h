/* $Id$ */

#ifndef MICQ_IM_CLI_H
#define MICQ_IM_CLI_H

void Auto_Reply (Connection *conn, UDWORD uin);
void icq_sendmsg (Connection *conn, UDWORD uin, const char *text, UDWORD msg_type);
void icq_sendurl (Connection *conn, UDWORD uin, const char *description, const char *url);

#endif /* MICQ_IM_CLI_H */
