/* $Id$ */

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
    UWORD  tabs;        /* type of tab completion */
    
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
    char  *auto_ffc;

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
    unsigned  type:4;
    unsigned  flags:4;
    UBYTE     version;
    UDWORD    uin;
    UDWORD    status;
    char     *server;
    UDWORD    port;
    char     *passwd;
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
#define FLAG_QUIET      4096
/*      FLAG_S5
 *      FLAG_S5_USE
 */

#define SFLAG_BEEP          1
#define SFLAG_CMD           2
#define SFLAG_ON_BEEP       4
#define SFLAG_ON_CMD        8
#define SFLAG_OFF_BEEP     16
#define SFLAG_OFF_CMD      32

#define TYPEF_ANY_SERVER    1  /* any server connection  */
#define TYPEF_SERVER_OLD    2  /* " && ver == 5          */
#define TYPEF_SERVER        4  /* " && var > 6           */
#define TYPEF_ANY_PEER      8  /* any peer connection    */
#define TYPEF_ANY_DIRECT   16  /* " && established       */
#define TYPEF_ANY_LISTEN   32  /* " && listening         */
#define TYPEF_ANY_MSG      64  /* " && for messages      */
#define TYPEF_ANY_FILE    128  /* " && for file transfer */
#define TYPEF_ANY_CHAT    256  /* " && for chat          */
#define TYPEF_FILE        512  /* any file io            */

#define TYPE_SERVER_OLD   (TYPEF_ANY_SERVER | TYPEF_SERVER_OLD)   /* v5 server connection             */
#define TYPE_SERVER       (TYPEF_ANY_SERVER | TYPEF_SERVER)       /* v7/v8 server connection          */
#define TYPE_LISTEN       (TYPEF_ANY_PEER | TYPEF_ANY_LISTEN)     /* listener for direct message conn */
#define TYPE_DIRECT       (TYPEF_ANY_PEER | TYPEF_ANY_DIRECT)     /* direct connection                */
#define TYPE_FILE         TYPEF_FILE

#define CONN_AUTOLOGIN   1
#define CONN_WIZARD      2

#define TABS_SIMPLE      1
#define TABS_CYCLE       2
#define TABS_CYCLEALL    3

#define ASSERT_LISTEN(s)      (assert (s), assert ((s)->type == TYPE_LISTEN))
#define ASSERT_DIRECT(s)      (assert (s), assert ((s)->type == TYPE_DIRECT))
#define ASSERT_DIRECT_FILE(s) (assert (s), assert ((s)->type == TYPE_FILE || (s)->type == TYPE_DIRECT))
#define ASSERT_FILE(s)        (assert (s), assert ((s)->type == TYPE_FILE))
#define ASSERT_SERVER(s)      (assert (s), assert ((s)->type == TYPE_SERVER))

#endif

