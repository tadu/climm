/*********************************************
 * ($Id$)
 *
 * Header file for ICQ protocol structres and
 * constants
 *
 * This software is provided AS IS to be used in
 * whatever way you see fit and is placed in the
 * public domain.
 * 
 * Author : Matthew Smith April 19, 1998
 * Contributors : Lalo Martins Febr 26, 1999
 * 
 * Changes :
 *  4-21-98 Increase the size of data associtated
 *           with the packets to enable longer messages. mds
 *  4-22-98 Added function prototypes and extern variables mds
 *  4-22-98 Added SRV_GO_AWAY code for bad passwords etc.
 *  (I assume Matt did a lot of unrecorded changes after these - Lalo)
 *  2-26-99 Added TAB_SLOTS, tab_array, add_tab() and get_tab() (Lalo)
 **********************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if TM_IN_SYS_TIME
  #include <sys/time.h>
  #if TIME_WITH_SYS_TIME
    #include <time.h>
  #endif
#else
  #include <time.h>
  #if TIME_WITH_SYS_TIME
    #include <sys/time.h>
  #endif
#endif
#include <stdio.h>

struct Contact_s;
struct Packet_s;
struct Session_s;
struct Preferences_s;
struct SessionPreferences_s;

typedef struct Contact_s            Contact;
typedef struct Packet_s             Packet;
typedef struct Session_s            Session;
typedef struct Preferences_s        Preferences;
typedef struct PreferencesSession_s PreferencesSession;

#include "micqconfig.h"
#include "datatype.h"
#include <stdlib.h>
#include "mreadline.h"
#include "msg_queue.h"
#include "mselect.h"
#include "color.h"

#define SOUND_ON 1
#define SOUND_OFF 0
#define SOUND_CMD 2

#define default_away_time 600

#define ICQ_VER 5

#define STDIN 0
#define STDOUT 1
#define STDERR 2



#include "icq_v2.h"
#include "icq_v4.h"
#include "icq_v5.h"
#include "icq_tcp.h"

/* test in this order */
#define STATUSF_INV       0x00000100
#define STATUSF_DND       0x00000002
#define STATUSF_OCC       0x00000010
#define STATUSF_NA        0x00000004
#define STATUSF_AWAY      0x00000001
#define STATUSF_FFC       0x00000020
#define STATUSF_BIRTH     0x00080000

#define STATUS_OFFLINE    0xffffffff
#define STATUS_INV         STATUSF_INV
#define STATUS_DND        (STATUSF_DND | STATUSF_OCC | STATUSF_AWAY)
#define STATUS_OCC        (STATUSF_OCC | STATUSF_AWAY)
#define STATUS_NA         (STATUSF_NA  | STATUSF_AWAY)
#define STATUS_AWAY        STATUSF_AWAY
#define STATUS_FFC         STATUSF_FFC
#define STATUS_ONLINE     0x00000000

#define AUTH_REQ_MESS           0x0006
#define AUTH_REF_MESS           0x0007
#define AUTH_OK_MESS            0x0008
#define USER_ADDED_MESS         0x000C
#define URL_MESS                0x0004
#define WEB_MESS                0x000d
#define EMAIL_MESS              0x000e
#define MASS_MESS_MASK          0x8000
#define MRURL_MESS              0x8004
#define NORM_MESS               0x0001
#define MRNORM_MESS             0x8001
#define CONTACT_MESS            0x0013
#define MRCONTACT_MESS          0x8013

#define INV_LIST_UPDATE         0x01
#define VIS_LIST_UPDATE         0x02
#define CONT_LIST_UPDATE        0x00

typedef struct
{
   UBYTE uin[4];
   UBYTE year[2];
   UBYTE month;
   UBYTE day;
   UBYTE hour;
   UBYTE minute;
   UBYTE type[2];
   UBYTE len[2];
} RECV_MESSAGE, *RECV_MESSAGE_PTR;

typedef struct
{
   char *nick;
   char *first;
   char *last;
   char *email;
   BOOL auth;
} USER_INFO_STRUCT, *USER_INFO_PTR;

/*
 * Should you decide to move a global variable from one struct to another,
 * change it manually in the header file, and then in the src directory
 * the following incantation can be handy (assuming you want to move, say,
 * Hermit, from uiG to ss G):
 * perl -pi -e 's/(\W)uiG\.(Hermit)(\W)/$1ss G.$2$3/g;' *.c
 */

/* user interface global state variables */
typedef struct
{
        UDWORD last_rcvd_uin;
        UDWORD last_sent_uin;
        char*  last_message_sent;
        UDWORD last_message_sent_type;
        time_t start_time;
        UDWORD away_time_prev;
        UBYTE  reconnect_count;
        BOOL   quit;
} user_interface_state;

extern user_interface_state uiG;
extern struct Queue *queue;
extern Preferences *prG;

#define LOG_MESS 1
#define LOG_AUTO_MESS 2
#define LOG_ONLINE 3
#define LOG_EVENT 4

#include "i18n.h"



