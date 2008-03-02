
#ifndef CLIMM_OSCAR_OLDICQ_H
#define CLIMM_OSCAR_OLDICQ_H

jump_snac_f SnacSrvToicqerr, SnacSrvFromicqsrv;

typedef struct
{
    char *nick, *first, *last, *email;
    UWORD minage, maxage;
    UBYTE sex, language;
    char *city, *state;
    UWORD country;
    char *company, *department, *position;
    BOOL online;
} MetaWP;

typedef enum {
  META_SET_GENERAL_INFO_v5 = 1001,
  META_SET_GENERAL_INFO    = 1002,
  META_SET_WORK_INFO       = 1011,
  META_SET_MORE_INFO       = 1021,
  META_SET_ABOUT_INFO      = 1030,
  META_SET_EXT_EMAIL       = 1035,
  META_SET_INTERESTS       = 1040,
  META_SET_AFFILIATIONS    = 1050,
  META_INFO_SECURE         = 1060,
  META_SET_PASS            = 1070,
  META_SET_HPCAT           = 1090,
  META_REQ_INFO_v5         = 1200,
  META_REQ_INFO_FULL       = 1202,
  META_REQ_INFO_SHORT      = 1210,
  META_UNREGISTER          = 1220,
  META_REQ_INFO            = 1232,
  META_SEARCH_DETAIL_P     = 1301,
  META_SEARCH_UIN_P        = 1311,
  META_SEARCH_EMAIL_P      = 1321,
  META_SEARCH_WP           = 1331,
  META_SEARCH_DETAIL_WC    = 1341,
  META_SEARCH_EMAIL_WC     = 1351,
  META_SEARCH_WP_WC        = 1361,
  META_SEARCH_PERSINFO     = 1375,
  META_SEARCH_UIN_TLV      = 1385,
  META_SEARCH_EMAIL        = 1395,
  META_SEARCH_RANDOM       = 1870,
  META_SET_RANDOM          = 1880,
  META_SET_WEB_PRESENCE    = 2000,
  META_GET_XML             = 2200,
  META_SEND_REG_STATS      = 2725,
  META_SEND_SHORTCUT       = 2735,
  META_SAVE_INFO           = 3130,
  META_SEND_SMS            = 5250,
  META_SPAM_REPORT         = 8200
} meta_cmd;


void SnacCliReqofflinemsgs (Connection *serv);
void SnacCliAckofflinemsgs (Connection *serv);
void SnacCliMetasetgeneral (Connection *serv, Contact *cont);
void SnacCliMetasetabout (Connection *serv, const char *text);
void SnacCliMetasetmore (Connection *serv, Contact *cont);
void SnacCliMetasetpass (Connection *serv, const char *newpass);
UDWORD SnacCliMetareqmoreinfo (Connection *serv, Contact *cont);
UDWORD SnacCliMetareqinfo (Connection *serv, Contact *cont);
void SnacCliSearchbypersinf (Connection *serv, const char *email, const char *nick, const char *name, const char *surname);
void SnacCliSearchbymail (Connection *serv, const char *email);
UDWORD SnacCliSearchrandom (Connection *serv, UWORD group);
void SnacCliSetrandom (Connection *serv, UWORD group);
void SnacCliSearchwp (Connection *serv, const MetaWP *wp);
void SnacCliSendsms (Connection *serv, const char *target, const char *text);

void Auto_Reply (Connection *conn, Contact *cont);
#define IREP_HASAUTHFLAG 1
void Meta_User (Connection *conn, Contact *cont, Packet *pak);
void Display_Rand_User (Connection *conn, Packet *pak);
void Recv_Message (Connection *conn, Packet *pak);
void Display_Info_Reply (Contact *cont, Packet *pak, UBYTE flags);
void Display_Ext_Info_Reply (Connection *conn, Packet *pak);

#endif
