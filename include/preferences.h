/* $Id$ */

#ifndef MICQ_PREFERENCES_H
#define MICQ_PREFERENCES_H

struct Preferences_s
{
    char  *s5Host;      /* socks 5 settings */
    char  *s5Name;
    char  *s5Pass;
    UWORD  s5Port;
    BOOL   s5Use;
    BOOL   s5Auth;

    char  *locale;      /* the used locale */
    char  *locale_full; /* ... verbatim as derived from ENV/--1i8n */
    UBYTE  enc_rem;     /* the (assumed) remote encoding */
    UBYTE  enc_loc;     /* the local character encoding */
    
    UDWORD verbose;     /* verbosity to use on startup */
    UWORD  sound;       /* how to beep */
    UDWORD status;      /* status to use when logging in */
    UWORD  screen;      /* manual maximum screen width; 0 = auto */
    UDWORD flags;       /* flags for output */
    UDWORD away_time;   /* time after which to be away automatically; 0 = disable */
    UWORD  tabs;        /* type of tab completion */
    
    char  *basedir;     /* the base dir where micqrc etc. reside in */
    char  *rcfile;      /* the preference file to load */
    char  *logplace;    /* where to log to */
    char  *logname;     /* which name to use for logging */

    char  *event_cmd;   /* the command to execute for events */
    
    char  *auto_na;
    char  *auto_away;
    char  *auto_occ;
    char  *auto_inv;
    char  *auto_dnd;
    char  *auto_ffc;
    
    char  *colors[CXCOUNT];
    UBYTE  scheme;
    SBYTE  chat;

    /* Much more stuff to go here - %TODO% */
};

struct PreferencesConnection_s
{
    UDWORD    type;
    UBYTE     flags;
    UBYTE     version;
    UDWORD    uin;
    UDWORD    status;
    char     *server;
    UDWORD    port;
    char     *passwd;
};

Preferences           *PreferencesC (void);
PreferencesConnection *PreferencesConnectionC (void);

BOOL        PrefLoad (Preferences *pref);
void        PrefSetColorScheme (Preferences *pref, UBYTE scheme);

#define PrefUserDir(pref) (pref->basedir ? pref->basedir : PrefUserDirReal (pref))
#define PrefLogName(pref) (pref->logname ? pref->logname : PrefLogNameReal (pref))

const char *PrefUserDirReal (Preferences *pref);
const char *PrefLogNameReal (Preferences *pref);

#define FLAG_DELBS      (1 <<  0)
#define FLAG_CONVRUSS   (1 <<  1)
#define FLAG_CONVEUC    (1 <<  2)
#define FLAG_FUNNY      (1 <<  3)
#define FLAG_COLOR      (1 <<  4)
#define FLAG_HERMIT     (1 <<  5)
#define FLAG_LOG        (1 <<  6)
#define FLAG_LOG_ONOFF  (1 <<  7)
#define FLAG_AUTOREPLY  (1 <<  8)
#define FLAG_UINPROMPT  (1 <<  9)
#define FLAG_LIBR_BR    (1 << 10) /* 0, 3: possible line break before message */
#define FLAG_LIBR_INT   (1 << 11) /* 2, 3: indent if appropriate */
#define FLAG_QUIET      (1 << 12)
#define FLAG_ULTRAQUIET (1 << 13)
#define FLAG_AUTOSAVE   (1 << 14)
#define FLAG_AUTOFINGER (1 << 15)
#define FLAG_WEBAWARE   (1 << 16)
#define FLAG_HIDEIP     (1 << 17)
#define FLAG_DC_AUTH    (1 << 18)
#define FLAG_DC_CONT    (1 << 19)
/*      FLAG_S5
 *      FLAG_S5_USE
 */

#define SFLAG_BEEP          1
#define SFLAG_EVENT         2

#define CONN_AUTOLOGIN   1
#define CONN_WIZARD      2

#define TABS_SIMPLE      1
#define TABS_CYCLE       2
#define TABS_CYCLEALL    3

#define ENC(enc_x) (prG->enc_x & ~ENC_AUTO)

#endif /* MICQ_PREFERENCES_H */

