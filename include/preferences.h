
#ifndef MICQ_PREFERENCES_H
#define MICQ_PREFERENCES_H

struct Preferences_s
{
    char  *rcfile;      /* the preference file to load */
    BOOL   rcisdef;     /* whether this is the default location */

    SWORD  verbose;     /* verbosity to use on startup */
    UWORD  sound;       /* flags for sound output */
    UDWORD status;      /* status to use when logging in */
    UWORD  screen;      /* manual maximum screen width; 0 = auto */
    UWORD  flags;       /* flags for output */
    UDWORD away_time;   /* time after which to be away automatically; 0 = disable */
    
    char  *logplace;

    char  *sound_cmd;
    char  *sound_on_cmd;
    char  *sound_off_cmd;
    char  *event_cmd;
    
    char  *auto_na;
    char  *auto_away;
    char  *auto_occ;
    char  *auto_inv;
    char  *auto_dnd;

    BOOL   s5Use;
    char  *s5Host;
    UWORD  s5Port;
    BOOL   s5Auth;
    char  *s5Name;
    char  *s5Pass;

    UDWORD s5DestIP;    /* this doesn't belong here %TODO% */
    UDWORD s5DestPort;  /* neither this %TODO%             */
    Session *sess;      /* get rid of this ASAP  %TODO% */

    /* Much more stuff to go here - %TODO% */
};

struct PreferencesSession_s
{
    UBYTE   type:4;
    UBYTE   flags:4;
    UBYTE   version;
    UDWORD  uin;
    UDWORD  status;
    char   *server;
    UDWORD  port;
    char   *passwd;
};

Preferences *PreferencesC (void);
PreferencesSession *PreferencesSessionC (void);

const char *PrefUserDir ();
void PrefLoad (Preferences *pref);

#define VERB_PACK         4
#define VERB_PACK_DEBUG   8
#define VERB_QUEUE       16

#define FLAG_DELBS         1
#define FLAG_CONVRUSS      2
#define FLAG_CONVEUC       4
#define FLAG_FUNNY         8
#define FLAG_COLOR        16
#define FLAG_HERMIT       32
#define FLAG_LOG          64
#define FLAG_LOG_ONOFF   128
#define FLAG_AUTOREPLY   256
#define FLAG_UINPROMPT   512
#define FLAG_LIBR_BR    1024 /* 0, 3: posssible line break before message */
#define FLAG_LIBR_INT   2048 /* 2, 3: indent if appropriate */
/*      FLAG_S5
 *      FLAG_S5_USE
 */

#define SFLAG_BEEP        1
#define SFLAG_CMD         2
#define SFLAG_ON_BEEP     4
#define SFLAG_ON_CMD      8
#define SFLAG_OFF_BEEP   16
#define SFLAG_OFF_CMD    32

#define TYPE_SERVER_OLD   1
#define TYPE_SERVER       2
#define TYPE_PEER         3
#define TYPE_DIRECT       4

#define CONN_AUTOLOGIN   1
#define CONN_WIZARD      2

#define ASSERT_PEER(s)   (assert (s), assert (s->type == TYPE_PEER))
#define ASSERT_DIRECT(s) (assert (s), assert (s->type == TYPE_DIRECT))

#endif

