
#ifndef MICQ_OLDICQ_COMPAT
#define MICQ_OLDICQ_COMPAT

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

#define META_SET_GENERAL_INFO_v5 1001
#define META_SET_GENERAL_INFO    1002
#define META_SET_WORK_INFO       1011 /* */
#define META_SET_MORE_INFO       1021
#define META_SET_ABOUT_INFO      1030
#define META_INFO_SECURE         1060 /* ?? */
#define META_SET_PASS            1070
#define META_REQ_INFO_v5         1200
#define META_REQ_INFO            1232
#define META_SEARCH_WP           1331
#define META_SEARCH_PERSINFO     1375
#define META_SEARCH_EMAIL        1395
#define META_SEARCH_RANDOM       1870
#define META_SET_RANDOM          1880
#define META_SET_WEB_PRESENCE    2000 /* */
#define META_SEND_SMS            5250

void Auto_Reply (Connection *conn, Contact *cont);
#define IREP_HASAUTHFLAG 1
void Meta_User (Connection *conn, Contact *cont, Packet *pak);
void Display_Rand_User (Connection *conn, Packet *pak);
void Recv_Message (Connection *conn, Packet *pak);
void Display_Info_Reply (Contact *cont, Packet *pak, UBYTE flags);
void Display_Ext_Info_Reply (Connection *conn, Packet *pak);

#endif
