
#ifndef MICQ_CONTACT_H
#define MICQ_CONTACT_H

struct Contact_s
{
   UDWORD uin;
   UDWORD status;
   BOOL   invis_list;
   BOOL   vis_list;
   BOOL   not_in_list;
   UDWORD local_ip;   /* host byte order */
   UDWORD outside_ip;
   UDWORD port;
   UDWORD last_time; /* last time online or when came online */
   UWORD  TCP_version;
   UBYTE  connection_type;
   char  *version;
   char  *LastMessage;
   char   nick[20];
};

Contact    *ContactAdd (UDWORD uin, const char *nick);
Contact    *ContactFind (UDWORD uin);
Contact    *ContactFindM (UDWORD uin);
const char *ContactFindNick (UDWORD uin);
char       *ContactFindName (UDWORD uin);
UDWORD      ContactFindByNick (const char *nick);
Contact    *ContactStart ();
Contact    *ContactNext (Contact *cont);
BOOL        ContactHasNext (Contact *cont);

#endif
