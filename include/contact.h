/* $Id$ */

#ifndef MICQ_CONTACT_H
#define MICQ_CONTACT_H

typedef struct ContactMetaGeneral_s  MetaGeneral;
typedef struct ContactMetaWork_s     MetaWork;
typedef struct ContactMetaMore_s     MetaMore;
typedef struct ContactMetaEmail_s    MetaEmail;
typedef struct ContactMetaObsolete_s MetaObsolete;
typedef struct ContactGroup_s        ContactGroup;
typedef struct ContactDC_s           ContactDC;

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

struct ContactMetaEmail_s
{
    MetaEmail *meta_email;
    char      *email;
    UBYTE      auth;
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
    UDWORD ip_loc, ip_rem, port;
    UWORD  version, cookie;
    UBYTE  type;
};

struct ContactGroup_s
{
    ContactGroup *more;
    Connection   *serv;
    char         *name;
    UDWORD        uins[32];
    UWORD         id;
    UBYTE         used;
};

struct Contact_s
{
    char  *nick;
    UWORD  id;
    UDWORD uin;
    UDWORD status;
    UDWORD flags;
    UBYTE  v1, v2, v3, v4;
    UDWORD caps;

    char  *version;
    char  *last_message;
    time_t last_time, seen_time, seen_micq_time;

    UWORD  updated; /* which meta_* has been updated */

    ContactDC       *dc;
    MetaGeneral     *meta_general;
    MetaWork        *meta_work;
    MetaMore        *meta_more;
    char            *meta_about;
    MetaEmail       *meta_email;
    MetaObsolete    *meta_obsolete;
    Extra           *meta_interest, *meta_background, *meta_affiliation;
};

Contact    *ContactAdd (UDWORD uin, const char *nick);
void        ContactRem (Contact *cont);

ContactGroup *ContactGroupFind (UWORD id, Connection *serv, const char *name, BOOL create);
BOOL          ContactGroupAdd (ContactGroup *group, Contact *cont);
BOOL          ContactGroupRem (ContactGroup *group, Contact *cont);

Contact    *ContactByUIN (UDWORD uin, BOOL create);
Contact    *ContactByNick (const char *nick, BOOL create);
Contact    *ContactFindAlias (UDWORD uin, const char *nick);

Contact    *ContactStart ();
Contact    *ContactNext (Contact *cont);
BOOL        ContactHasNext (Contact *cont);

void        ContactSetCap (Contact *cont, Cap *cap);
void        ContactSetVersion (Contact *cont);

#define CONTACT_GENERAL(cont)     ((cont)->meta_general     ? (cont)->meta_general     : ((cont)->meta_general     = calloc (1, sizeof (MetaGeneral))))
#define CONTACT_WORK(cont)        ((cont)->meta_work        ? (cont)->meta_work        : ((cont)->meta_work        = calloc (1, sizeof (MetaWork))))
#define CONTACT_MORE(cont)        ((cont)->meta_more        ? (cont)->meta_more        : ((cont)->meta_more        = calloc (1, sizeof (MetaMore))))
#define CONTACT_EMAIL(cont)       ((cont)->meta_email       ? (cont)->meta_email       : ((cont)->meta_email       = calloc (1, sizeof (MetaEmail))))
#define CONTACT_LIST(listpp)      (*(listpp)                ? *(listpp)                : (*(listpp)                = calloc (1, sizeof (Extra))))
#define CONTACT_OBSOLETE(cont)    ((cont)->meta_obsolete    ? (cont)->meta_obsolete    : ((cont)->meta_obsolete    = calloc (1, sizeof (MetaObsolete))))
#define CONTACT_DC(cont)          ((cont)->dc               ? (cont)->dc               : ((cont)->dc               = calloc (1, sizeof (ContactDC))))

#define CONT_UTF8(cont) ((cont->caps & (1 << CAP_UTF8)) && (prG->enc_loc != ENC_EUC))

#define CONT_IGNORE     1UL /* ignore contact. */
#define CONT_HIDEFROM   2UL /* always pretend to be offline. */
#define CONT_INTIMATE   4UL /* can see even if invisible. */

#define CONT_TEMPORARY  8UL /* no status display for this contact. */
#define CONT_ALIAS     16UL /* is an alias entry. */
#define CONT_SEENAUTO  32UL /* has seen auto response. */

#define UPF_GENERAL_A   0x001
#define UPF_GENERAL_B   0x002
#define UPF_WORK        0x004
#define UPF_MORE        0x008
#define UPF_ABOUT       0x010
#define UPF_EMAIL       0x020
#define UPF_INTEREST    0x040
#define UPF_BACKGROUND  0x080
#define UPF_AFFILIATION 0x100
#define UPF_OBSOLETE    0x200

#define UP_INFO         0x3ff

#endif /* MICQ_CONTACT_H */
