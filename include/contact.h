
#ifndef MICQ_CONTACT_H
#define MICQ_CONTACT_H

struct Contact_t;

#include "tcp.h"

struct Contact_t
{
   UDWORD uin;
   UDWORD status;
   BOOL   invis_list;
   BOOL   vis_list;
   BOOL   not_in_list;
   UBYTE  current_ip[4];
   UBYTE  other_ip[4];
   UDWORD port;
#ifdef TCP_COMM
   tcpsock_t sok, sok_chat, sok_file;
#endif
   UDWORD last_time; /* last time online or when came online */
   UWORD TCP_version;
   UBYTE connection_type;
   char *version;
   char *LastMessage;
   char nick[20];
};

typedef struct Contact_t Contact;

Contact *ContactAdd (UDWORD uin, const char *nick);
Contact *ContactFind (UDWORD uin);
const char *ContactFindNick (UDWORD uin);
char *ContactFindName (UDWORD uin);
UDWORD ContactFindByNick (const char *nick);
Contact *ContactStart ();
Contact *ContactNext (Contact *cont);
BOOL ContactHasNext (Contact *cont);

#endif
