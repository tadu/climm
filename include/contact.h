/* $Id$ */

#ifndef MICQ_CONTACT_H
#define MICQ_CONTACT_H

#include "contactopts.h"

typedef struct ContactMetaGeneral_s  MetaGeneral;
typedef struct ContactMetaWork_s     MetaWork;
typedef struct ContactMetaMore_s     MetaMore;
typedef struct ContactMetaObsolete_s MetaObsolete;
typedef struct ContactDC_s           ContactDC;
typedef struct ContactMeta_s         ContactMeta;

struct ContactMetaGeneral_s
{
    char  *nick,   *first,  *last,
          *email,  *city,   *state,
          *phone,  *fax,    *zip,
          *street, *cellular;
    UWORD  country;
    SBYTE  tz;
    BOOL   auth, webaware, hideip;
};

struct ContactMetaWork_s
{
    char  *wcity,    *wstate,   *wphone,
          *wfax,     *waddress, *wzip,
          *wcompany, *wdepart,  *wposition,
          *whomepage;
    UWORD  wcountry, woccupation;
};

struct ContactMetaMore_s
{
    char  *homepage;
    UWORD  age, year, unknown;
    UBYTE  sex, month, day, lang1, lang2, lang3;
};

struct ContactMetaObsolete_s
{
    char  *description;
    UWORD  unknown;
    UBYTE  given, empty;
};

struct ContactDC_s
{
    time_t id1, id2, id3;
    UDWORD ip_loc, ip_rem, port, cookie;
    UWORD  version;
    UBYTE  type;
};

struct ContactGroup_s
{
    ContactGroup  *more;
    Contact       *contacts[32];
    Connection    *serv;
    char          *name;
    ContactOptions copts;
    UWORD          id;
    UBYTE          used;
};

struct ContactAlias_s
{
    ContactAlias *more;
    char         *alias;
};

struct ContactMeta_s
{
    ContactMeta *next;
    UWORD data;
    char *text;
};

struct Contact_s
{
    ContactGroup *group;
    char  *nick;
    UDWORD uin;
    UDWORD status;
    UDWORD oldflags;
    ContactOptions copts;
    UDWORD caps;
    UWORD  id;
    UBYTE  v1, v2, v3, v4;

    char  *version;
    char  *last_message;
    time_t last_time;
    
    ContactAlias *alias; /* alias of this entry */

    UWORD  updated; /* which meta_* has been updated */

    ContactDC       *dc;
    MetaGeneral     *meta_general;
    MetaWork        *meta_work;
    MetaMore        *meta_more;
    char            *meta_about;
    MetaObsolete    *meta_obsolete;
    ContactMeta     *meta_email, *meta_interest, *meta_background, *meta_affiliation;
};


ContactGroup *ContactGroupC       (Connection *serv, UWORD id, const char *name);
ContactGroup *ContactGroupFind    (Connection *serv, UWORD id, const char *name);
ContactGroup *ContactGroupIndex   (int i);
UWORD         ContactGroupID      (ContactGroup *group);
UDWORD        ContactGroupCount   (ContactGroup *group);
void          ContactGroupD       (ContactGroup *group);

/* NULL ContactGroup accesses global list */
Contact      *ContactIndex        (ContactGroup *group, int i);
Contact      *ContactUIN          (Connection *conn, UDWORD uin);
Contact      *ContactFind         (ContactGroup *group, UWORD id, UDWORD uin, const char *nick);
Contact      *ContactFindCreate   (ContactGroup *group, UWORD id, UDWORD uin, const char *nick);
BOOL          ContactAdd          (ContactGroup *group, Contact *cont);
BOOL          ContactHas          (ContactGroup *group, Contact *cont);
BOOL          ContactRem          (ContactGroup *group, Contact *cont);
void          ContactD            (Contact *cont);

BOOL          ContactAddAlias     (Contact *cont, const char *nick);
BOOL          ContactRemAlias     (Contact *cont, const char *nick);

UWORD         ContactID           (Contact *cont);
void          ContactSetCap       (Contact *cont, Cap *cap);
void          ContactSetVersion   (Contact *cont);

void          ContactMetaD        (ContactMeta *m);
void          ContactMetaAdd      (ContactMeta **m, UWORD val, const char *text);
BOOL          ContactMetaSave     (Contact *cont);
BOOL          ContactMetaLoad     (Contact *cont);

const char   *ContactPrefStr      (Contact *cont, UDWORD flag);
val_t         ContactPrefVal      (Contact *cont, UDWORD flag);

#define CONTACT_GENERAL(cont)  ((cont)->meta_general  ? (cont)->meta_general  : ((cont)->meta_general  = calloc (1, sizeof (MetaGeneral))))
#define CONTACT_WORK(cont)     ((cont)->meta_work     ? (cont)->meta_work     : ((cont)->meta_work     = calloc (1, sizeof (MetaWork))))
#define CONTACT_MORE(cont)     ((cont)->meta_more     ? (cont)->meta_more     : ((cont)->meta_more     = calloc (1, sizeof (MetaMore))))
#define CONTACT_OBSOLETE(cont) ((cont)->meta_obsolete ? (cont)->meta_obsolete : ((cont)->meta_obsolete = calloc (1, sizeof (MetaObsolete))))
#define CONTACT_DC(cont)       ((cont)->dc            ? (cont)->dc            : ((cont)->dc            = calloc (1, sizeof (ContactDC))))

#define CONT_UTF8(cont,mt) (((cont)->caps & (1 << CAP_UTF8)) && (((mt) == 1) || ((cont)->caps & (1 << CAP_MICQ))))

#define CONT_SEENAUTO  32UL /* has seen auto response. */
#define CONT_ISEDITED  64UL /* meta data was edited by hand. */

#define CONT_ISSBL    128UL /* contact has been added to server based list */
#define CONT_REQAUTH  256UL /* contact requires authorization */
#define CONT_TMPSBL   512UL /* contact tagged for SBL operation */

#define UPF_GENERAL_A   0x0001
#define UPF_GENERAL_B   0x0002
#define UPF_WORK        0x0004
#define UPF_MORE        0x0008
#define UPF_ABOUT       0x0010
#define UPF_EMAIL       0x0020
#define UPF_INTEREST    0x0040
#define UPF_BACKGROUND  0x0080
#define UPF_AFFILIATION 0x0100
#define UPF_OBSOLETE    0x0200
#define UPF_GENERAL_C   0x0400
#define UPF_GENERAL_E   0x0800

#define UPF_DISC        0x1000
#define UPF_SERVER      0x2000
#define UPF_AUTOFINGER  0x4000

#define UP_ALL          (UPF_DISC | UPF_SERVER)
#define UP_INFO         (UPF_GENERAL_A  | UPF_GENERAL_B | UPF_WORK | UPF_MORE \
                         | UPF_ABOUT    | UPF_INTEREST  | UPF_BACKGROUND  \
                         | UPF_EMAIL    | UPF_OBSOLETE  | UPF_AFFILIATION)

#endif /* MICQ_CONTACT_H */
