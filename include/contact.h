
typedef struct
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
   UDWORD session_id;
   SOK_T sok;
   int sokflag; /* -1 = failed 0 = none
                 * 1 = initiating outside IP 2 = initiating real IP
                 * 3 = established 10 = ok
                 */
#endif
   UDWORD last_time; /* last time online or when came online */
   UWORD TCP_version;
   UBYTE connection_type;
   char *version;
   char *LastMessage;
   char nick[20];
} Contact, *ContactP;

Contact *ContactAdd (UDWORD uin, const char *nick);
Contact *ContactFind (UDWORD uin);
const char *ContactFindNick (UDWORD uin);
char *ContactFindName (UDWORD uin);
UDWORD ContactFindByNick (const char *nick);
Contact *ContactStart ();
Contact *ContactNext (Contact *cont);
BOOL ContactHasNext (Contact *cont);
