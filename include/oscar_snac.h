/* $Id$ */

#ifndef MICQ_OSCAR_SNAC_H
#define MICQ_OSCAR_SNAC_H

void SnacCallback (Event *event);
const char *SnacName (UWORD fam, UWORD cmd);
void SnacPrint (Packet *pak);

typedef void (jump_snac_f)(Event *);
typedef struct { UWORD fam; UWORD cmd; const char *name; jump_snac_f *f; } SNAC;
#define JUMP_SNAC_F(f) void f (Event *event)
#define SnacSend FlapSend
void SnacSendR (Connection *conn, Packet *pak, jump_snac_f *f, void *data);

Packet *SnacC (Connection *serv, UWORD fam, UWORD cmd, UWORD flags, UDWORD ref);
jump_snac_f SnacSrvUnknown;

#endif
