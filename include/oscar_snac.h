/* $Id$ */

#ifndef CLIMM_OSCAR_SNAC_H
#define CLIMM_OSCAR_SNAC_H

void SnacCallback (Event *event);
const char *SnacName (UWORD fam, UWORD cmd);
void SnacPrint (Packet *pak);

typedef void (jump_snac_f)(Event *);
typedef struct { UWORD fam; UWORD cmd; const char *name; jump_snac_f *f; } SNAC;
#define JUMP_SNAC_F(f) void f (Event *event)
#define SnacSend FlapSend
void SnacSendR (Server *serv, Packet *pak, jump_snac_f *f, void *data);

Packet *SnacC (Server *serv, UWORD fam, UWORD cmd, UWORD flags, UDWORD ref);
jump_snac_f SnacSrvUnknown;

#endif
