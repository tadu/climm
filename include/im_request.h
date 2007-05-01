/* $Id$ */

#ifndef MICQ_IM_CLI_H
#define MICQ_IM_CLI_H

UBYTE IMCliMsg (Connection *conn, Contact *cont, Opt *opt);
void IMSetStatus (Connection *conn, Contact *cont, status_t status, const char *msg);
UBYTE IMCliReMsg (Connection *conn, Contact *cont, Opt *opt); /* no log */
void IMCliInfo (Connection *conn, Contact *cont, int group);

#endif /* MICQ_IM_CLI_H */
