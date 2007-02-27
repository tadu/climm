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

#if ENABLE_XMPP
#define ENABLE_CONT_HIER 1
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

typedef enum {
  imr_online   = 0,
  imr_away     = 1,
  imr_dnd      = 2,
  imr_na       = 3,
  imr_occ      = 4,
  imr_ffc      = 5,
  imr_offline  = 7
} status_noi_t;

typedef enum {
  ims_online   = 0,
  ims_away     = 1,
  ims_dnd      = 2,
  ims_na       = 3,
  ims_occ      = 4,
  ims_ffc      = 5,
  ims_offline  = 7,
  ims_inv      = 8,
  ims_inv_ffc  = ims_inv + ims_ffc,
  ims_inv_away = ims_inv + ims_away,
  ims_inv_na   = ims_inv + ims_na,
  ims_inv_occ  = ims_inv + ims_occ,
  ims_inv_dnd  = ims_inv + ims_dnd
} status_t;

typedef enum {
  imf_none   = 0,
  imf_birth  = 1,
  imf_web    = 2,
  imf_dcauth = 4,
  imf_dccont = 8,
  imf_bw     = 3,
  imf_ab     = 5,
  imf_aw     = 6,
  imf_awb    = 7,
  imf_cb     = 9,
  imf_cw     = 10,
  imf_cwb    = 11,
  imf_ca     = 12,
  imf_cab    = 13,
  imf_caw    = 14,
  imf_cawb   = 15
} statusflag_t;

typedef enum {
  i_idle      = 0,
  i_os        = 1,
  i_want_away = 2,
  i_want_na   = 3
} idleflag_t;

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
/* XMPP */
#define MSG_NORM_SUBJ     0x30

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
    time_t     idle_val;
    UDWORD     idle_msgs;
    char      *idle_uins;
    UWORD    nick_len;   /* this *must* be an int why??*/
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
