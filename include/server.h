/* $Id$ */

#ifndef MICQ_IM_CLI_H
#define MICQ_IM_CLI_H

void icq_sendmsg (Connection *conn, UDWORD uin, const char *text, UDWORD msg_type);
UBYTE IMCliMsg (Connection *conn, Contact *cont, Extra *extra);
void IMCliInfo (Connection *conn, Contact *cont, int group);

#endif /* MICQ_IM_CLI_H */
