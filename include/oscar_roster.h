
#ifndef CLIMM_OSCAR_ROSTER_H
#define CLIMM_OSCAR_ROSTER_H

#include "oscar_tlv.h"

typedef enum {
  roster_normal = 0,
  roster_group = 1,
  roster_visible = 2,
  roster_invisible = 3,
  roster_visibility = 4,
  roster_presence = 5,
  roster_icqtic = 9,
  roster_ignore = 14,
  roster_lastupd = 15,
  roster_noncont = 16,
  roster_wierd17 = 17,
  roster_importt = 19,
  roster_icon = 20,
  roster_wierd25 = 25,
  roster_wierd29 = 29
} roster_t;

jump_snac_f SnacSrvReplylists, SnacSrvReplyroster, SnacSrvUpdateack,
    SnacSrvRosterok, SnacSrvAuthreq, SnacSrvAuthreply, SnacSrvAddedyou,
    SnacSrvRosterupdate, SnacSrvAddstart, SnacSrvAddend, SnacSrvRosteradd, SnacSrvRosterdelete;

typedef struct RosterEntry_s RosterEntry;
typedef struct Roster_s Roster;

struct RosterEntry_s {
  RosterEntry *next;
  char  *name;
  char  *nick;
  TLV   *tlv;
  UWORD  tag;
  UWORD  id;
  UWORD  type;
  unsigned short int reqauth:1;
};

struct Roster_s {
  RosterEntry *generic;
  RosterEntry *groups;
  RosterEntry *normal;
  RosterEntry *visible;
  RosterEntry *invisible;
  RosterEntry *ignore;
  time_t import;
  char *ICQTIC;
  char *delname;
  UDWORD delid;
  UDWORD deltag;
};

Roster *OscarRosterC (void);
void    OscarRosterD (Roster *roster);


UDWORD SnacCliCheckroster (Server *serv);
void SnacCliRosterentryadd (Server *serv, const char *name, UWORD tag, UWORD id, UWORD type, UWORD tlv, void *data, UWORD len);
void SnacCliRosteraddgroup (Server *serv, ContactGroup *cg, int mode);
void SnacCliRosteraddcontact (Server *serv, Contact *cont, int mode);
void SnacCliRosterbulkadd (Server *serv, ContactGroup *cs);
void SnacCliRosterupdategroup (Server *serv, ContactGroup *cg, int mode);
void SnacCliRosterupdatecontact (Server *serv, Contact *cont, int mode);
void SnacCliRosterdeletegroup (Server *serv, ContactGroup *cg, int mode);
void SnacCliRosterdeletecontact (Server *serv, Contact *cont, int mode);
void SnacCliRosterentrydelete (Server *serv, RosterEntry *entry);
void SnacCliRosterdelete (Server *serv, const char *name, UWORD tag, UWORD id, roster_t type);
void SnacCliSetvisibility (Server *serv, char value, char islogin);
void SnacCliSetlastupdate (Server *serv);
void SnacCliAddstart (Server *serv);
void SnacCliAddend (Server *serv);
void SnacCliGrantauth (Server *serv, Contact *cont);
void SnacCliReqauth (Server *serv, Contact *cont, const char *msg);
void SnacCliAuthorize (Server *serv, Contact *cont, BOOL accept, const char *msg);

#define SnacCliReqlists(serv)     SnacSend (serv, SnacC (serv, 19, 2, 0, 0))
#define SnacCliReqroster(serv)    SnacSend (serv, SnacC (serv, 19, 4, 0, 0))
#define SnacCliRosterack(serv)    SnacSend (serv, SnacC (serv, 19, 7, 0, 0))

#endif
