
#ifndef MICQ_CONTACT_H
#define MICQ_CONTACT_H

#define CONT_IGNORE     1UL /* Ignore contact. */
#define CONT_HIDEFROM   2UL /* Always pretend to be offline. */
#define CONT_INTIMATE   4UL /* Can see even if invisible. */
#define CONT_TEMPORARY  8UL /* No status display for this contact. */
#define CONT_ALIAS     16UL /* Is an alias entry (not yet used). */

struct Contact_s
{
   UDWORD uin;
   UDWORD status;
   UDWORD flags;
   UDWORD local_ip;   /* host byte order */
   UDWORD outside_ip;
   UDWORD port;
   UDWORD last_time; /* last time online or when came online */
   UWORD  TCP_version;
   UBYTE  connection_type;
   UDWORD cookie;
   char  *version;
   char  *LastMessage;
   char   nick[20];
};

Contact    *ContactAdd (UDWORD uin, const char *nick);
void        ContactRem (UDWORD uin);
Contact    *ContactFind (UDWORD uin);
const char *ContactFindNick (UDWORD uin);
char       *ContactFindName (UDWORD uin);
UDWORD      ContactFindByNick (const char *nick);
Contact    *ContactStart ();
Contact    *ContactNext (Contact *cont);
BOOL        ContactHasNext (Contact *cont);

#endif
