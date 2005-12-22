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

#ifndef HAVE_TIMEGM
#define HAVE_TIMEGM 1
#define timegm portable_timegm
time_t portable_timegm (struct tm *tm);
#endif

#ifndef HAVE_TIMELOCAL
#define timelocal mktime
#endif

#ifndef HAVE_LOCALTIME_R
#define HAVE_LOCALTIME_R 1
#define localtime_r(t, s)      memcpy(s, localtime(t), sizeof(struct tm))
#endif

#ifndef _REENTRANT
#define _REENTRANT 1
#endif

#ifndef __attribute__
#if !defined(__GNUC__) || __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 7)
  #define __attribute__(x)
#endif
#endif

#define ENABLE_DEBUG 1

struct Queue_s;
struct Event_s;
struct Contact_s;
struct Packet_s;
struct Connection_s;
struct Preferences_s;
struct ConnectionPreferences_s;
struct Cap_s;
struct Extra_s;

typedef struct Queue_s        Queue;
typedef struct Event_s        Event;
typedef struct Opt_s          Opt;
typedef struct Contact_s      Contact;
typedef struct ContactGroup_s ContactGroup;
typedef struct ContactAlias_s ContactAlias;
typedef struct Packet_s       Packet;
typedef struct Connection_s   Connection;
typedef struct Preferences_s  Preferences;
typedef struct Cap_s          Cap;
typedef struct Extra_s        Extra;

typedef void MSN_Handle;

#include "datatype.h"
#include <stdlib.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include "autopackage_prefix.h"

#if ENABLE_DEBUG && HAVE_VARIADIC_MACRO
#define DEBUGARGS    , __FILE__, __LINE__
#define DEBUGPARAM   , const char *debugfile, int debugline
#define DEBUG0ARGS   __FILE__, __LINE__
#define DEBUG0PARAM  const char *debugfile, int debugline
#define DEBUGFOR     , debugfile, debugline
#define DEBUGNONE    , "", 0
#define Debug(l,f,...) DebugReal (l, f " {%s:%d}<<{%s:%d}" , ## __VA_ARGS__, \
                                  __FILE__, __LINE__, debugfile, debugline)
#define DebugH(l,f,...) DebugReal (l, f " {%s:%d}<|" , ## __VA_ARGS__, \
                                   __FILE__, __LINE__)
#else
#define DEBUGARGS
#define DEBUGPARAM
#define DEBUG0ARGS
#define DEBUG0PARAM  void
#define DEBUGFOR
#define DEBUGNONE
#define DebugH DebugReal
#define Debug DebugReal
#endif

#include "msg_queue.h"
#include "mreadline.h"
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
#define STATUSF_ICQINV       0x00000100
#define STATUSF_ICQDND       0x00000002
#define STATUSF_ICQOCC       0x00000010
#define STATUSF_ICQNA        0x00000004
#define STATUSF_ICQAWAY      0x00000001
#define STATUSF_ICQFFC       0x00000020
#define STATUSF_ICQBIRTH     0x00080000
#define STATUSF_ICQWEBAWARE  0x00010000
#define STATUSF_ICQIP        0x00020000
#define STATUSF_ICQDC_AUTH   0x10000000
#define STATUSF_ICQDC_CONT   0x20000000

#define STATUS_ICQOFFLINE    0xffffffff
#define STATUS_ICQINV         STATUSF_ICQINV
#define STATUS_ICQDND        (STATUSF_ICQDND | STATUSF_ICQOCC | STATUSF_ICQAWAY)
#define STATUS_ICQOCC        (STATUSF_ICQOCC | STATUSF_ICQAWAY)
#define STATUS_ICQNA         (STATUSF_ICQNA  | STATUSF_ICQAWAY)
#define STATUS_ICQAWAY        STATUSF_ICQAWAY
#define STATUS_ICQFFC         STATUSF_ICQFFC
#define STATUS_ICQONLINE     0x00000000

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

#define LOG_MAX_PATH 255
#define DSCSIZ 192 /* Maximum length of log file descriptor lines. */

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
    Contact *last_rcvd;
    Contact *last_sent;
    char    *last_message_sent;
    UDWORD   last_message_sent_type;
    time_t   start_time;
    UDWORD   away_time_prev;
    UBYTE    reconnect_count;
    BOOL     quit;
    UDWORD   packets, events;
    time_t   idle_val;
    UDWORD   idle_msgs;
    UBYTE    idle_flag;
    char    *idle_uins;
    int      nick_len;   /* this *must* be an int */
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
#include "util_str.h"
