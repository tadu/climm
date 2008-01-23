
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
    SnacSrvRosterupdate, SnacSrvAddstart, SnacSrvAddend;

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


UDWORD SnacCliCheckroster (Connection *serv);
void SnacCliRosteradd (Connection *serv, ContactGroup *cg, Contact *cont);
void SnacCliRosterbulkadd (Connection *serv, ContactGroup *cs);
void SnacCliRosterentryadd (Connection *serv, const char *name, UWORD tag, UWORD id, UWORD type, UWORD tlv, void *data, UWORD len);
void SnacCliRosterupdate (Connection *serv, ContactGroup *cg, Contact *cont);
void SnacCliSetvisibility (Connection *serv, char value, char islogin);
void SnacCliSetlastupdate (Connection *serv);
void SnacCliRosterdeletegroup (Connection *serv, ContactGroup *cg);
void SnacCliRosterdeletecontact (Connection *serv, Contact *cont);
void SnacCliRosterentrydelete (Connection *serv, RosterEntry *entry);
void SnacCliRosterdelete (Connection *serv, const char *name, UWORD tag, UWORD id, roster_t type);
void SnacCliAddstart (Connection *serv);
void SnacCliAddend (Connection *serv);
void SnacCliGrantauth (Connection *serv, Contact *cont);
void SnacCliReqauth (Connection *serv, Contact *cont, const char *msg);
void SnacCliAuthorize (Connection *serv, Contact *cont, BOOL accept, const char *msg);

#define SnacCliReqlists(serv)     SnacSend (serv, SnacC (serv, 19, 2, 0, 0))
#define SnacCliReqroster(serv)    SnacSend (serv, SnacC (serv, 19, 4, 0, 0))
#define SnacCliRosterack(serv)    SnacSend (serv, SnacC (serv, 19, 7, 0, 0))

#endif
