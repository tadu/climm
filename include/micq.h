/* $Id$ */

#ifndef MICQ_H
#define MICQ_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#elif defined(__WIN32)
#include "config.h.win32"
#endif

#include "micqconfig.h"

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
#ifdef PREFER_PORTABLE_SNPRINTF
  #include <snprintf.h>
#endif
#ifdef PREFER_PORTABLE_MEMMOVE
  #include <memmove.h>
#endif
#ifndef __attribute__
#if !defined(__GNUC__) || __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 7)
  #define __attribute__(x)
#endif
#endif

struct Queue_s;
struct Event_s;
struct Contact_s;
struct Packet_s;
struct Connection_s;
struct Preferences_s;
struct ConnectionPreferences_s;
struct Cap_s;
struct Extra_s;

typedef struct Queue_s                 Queue;
typedef struct Event_s                 Event;
typedef struct Contact_s               Contact;
typedef struct ContactGroup_s          ContactGroup;
typedef struct Packet_s                Packet;
typedef struct Connection_s            Connection;
typedef struct Preferences_s           Preferences;
typedef struct PreferencesConnection_s PreferencesConnection;
typedef struct Cap_s                   Cap;
typedef struct Extra_s                 Extra;

#include "datatype.h"
#include <stdlib.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
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
#define STATUSF_WEBAWARE  0x00010000
#define STATUSF_IP        0x00020000
#define STATUSF_DC_AUTH   0x10000000
#define STATUSF_DC_CONT   0x20000000

#define STATUS_OFFLINE    0xffffffff
#define STATUS_INV         STATUSF_INV
#define STATUS_DND        (STATUSF_DND | STATUSF_OCC | STATUSF_AWAY)
#define STATUS_OCC        (STATUSF_OCC | STATUSF_AWAY)
#define STATUS_NA         (STATUSF_NA  | STATUSF_AWAY)
#define STATUS_AWAY        STATUSF_AWAY
#define STATUS_FFC         STATUSF_FFC
#define STATUS_ONLINE     0x00000000

#define NOW -1

#define MSGF_MASS         0x8000
#define MSGF_GETAUTO      0x0300

#define MSG_AUTO          0x00
#define MSG_NORM          0x01
#define MSG_CHAT          0x02
#define MSG_FILE          0x03
#define MSG_URL           0x04
#define MSG_AUTH_REQ      0x06
#define MSG_AUTH_DENY     0x07
#define MSG_AUTH_GRANT    0x08
#define MSG_AUTH_ADDED    0x0c
#define MSG_WEB           0x0d
#define MSG_EMAIL         0x0e
#define MSG_CONTACT       0x13
#define MSG_EXTENDED      0x1a
#define MSG_GET_AWAY      0xe8
#define MSG_GET_OCC       0xe9
#define MSG_GET_NA        0xea
#define MSG_GET_DND       0xeb
#define MSG_GET_FFC       0xec
/* mICQ extension */
#define MSG_GET_PEEK      0xf0
#define MSG_GET_VER       0xf4
/* Licq extension */
#define MSG_SSL_CLOSE     0xee
#define MSG_SSL_OPEN      0xef

#define INV_LIST_UPDATE         0x01
#define VIS_LIST_UPDATE         0x02
#define CONT_LIST_UPDATE        0x00

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
        UDWORD packets;
        time_t idle_val;
        UDWORD idle_msgs;
        UBYTE  idle_flag;
        char * idle_uins;
        int    nick_len;   /* this *must* be an int */
} user_interface_state;

extern user_interface_state uiG;
extern Preferences *prG;

enum logtype
{
    LOG_MISC,    /* miscellaneous */
    LOG_SENT,    /* sent message */
    LOG_AUTO,    /* automatic reply */
    LOG_RECVD,   /* received message */
    LOG_CHANGE,  /* status change */
    LOG_REPORT,  /* status report */
    LOG_GUESS,   /* status guess */
    LOG_ONLINE,  /* went online */
    LOG_OFFLINE, /* went offline */
    LOG_ACK,     /* message acknowedgment */
    LOG_ADDED,   /* added to contact list by someone */
    LOG_LIST,    /* added someone to the contact list */
    LOG_EVENT    /* other event */
};

#endif /* MICQ_H */

#include "i18n.h"
