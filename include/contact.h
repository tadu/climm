/* $Id$ */

#ifndef MICQ_CONTACT_H
#define MICQ_CONTACT_H

#define CONT_IGNORE     1UL /* ignore contact. */
#define CONT_HIDEFROM   2UL /* always pretend to be offline. */
#define CONT_INTIMATE   4UL /* can see even if invisible. */

#define CONT_TEMPORARY  8UL /* no status display for this contact. */
#define CONT_ALIAS     16UL /* is an alias entry. */
#define CONT_SEENAUTO  32UL /* has seen auto response. */

struct Contact_s
{
   /* basic data*/

   char  *nick;
   UDWORD uin;
   UDWORD status;
   UDWORD flags;
   
   /* connection data */

   UDWORD local_ip;
   UDWORD outside_ip;
   UDWORD port;
   UWORD  TCP_version;
   UBYTE  connection_type;
   UDWORD cookie;
   time_t id1, id2, id3;
   UWORD  caps;

   /* statistic data */

   char  *version;
   char  *last_message;
   time_t last_time;

   time_t seen_time;
   time_t seen_micq_time;
};

Contact    *ContactAdd (UDWORD uin, const char *nick);
void        ContactRem (Contact *cont);
Contact    *ContactFind (UDWORD uin);
const char *ContactFindNick (UDWORD uin);
const char *ContactFindName (UDWORD uin);
Contact    *ContactFindAlias (UDWORD uin, const char *nick);
Contact    *ContactFindContact (const char *nick);
UDWORD      ContactFindByNick (const char *nick);
Contact    *ContactStart ();
Contact    *ContactNext (Contact *cont);
BOOL        ContactHasNext (Contact *cont);

#endif
