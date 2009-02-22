/*
 * Handles commands the user sends.
 *
 * Copyright: This file may be distributed under version 2 of the GPL licence.
 *
 * history & find command Copyright Sebastian Felis
 *
 * $Id$
 */

#include "climm.h"

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>                      /* fopen parameter */
#include <sys/stat.h>                   /* fopen parameter */
#include <errno.h>

#include "cmd_user.h"
#include "util.h"
#include "util_ui.h"
#include "util_io.h"
#include "util_rl.h"
#include "util_table.h"
#include "util_parse.h"
#include "oscar_base.h"
#include "oscar_snac.h"
#include "oscar_service.h"
#include "oscar_contact.h"
#include "oscar_icbm.h"
#include "oscar_location.h"
#include "oscar_bos.h"
#include "oscar_roster.h"
#include "oscar_oldicq.h"
#include "im_icq8.h"
#include "preferences.h"
#include "packet.h"
#include "util_tabs.h"
#include "connection.h"
#include "oscar_dc.h"
#include "im_response.h"
#include "conv.h"
#include "oscar_dc_file.h"
#include "file_util.h"
#include "buildmark.h"
#include "contact.h"
#include "im_request.h"
#include "util_tcl.h"
#include "util_ssl.h"
#include "util_alias.h"
#include "util_otr.h"
#include "jabber_base.h"

#define MAX_STR_BUF 256                 /* buffer length for history */
#define DEFAULT_HISTORY_COUNT 10        /* count of last messages of history */
#define MEMALIGN(n)  (((n) & -128)+128) /* align of 2^7 blocks for malloc()
                                           in CmdUserFind() */

static jump_f
    CmdUserChange, CmdUserRandom, CmdUserHelp, CmdUserInfo, CmdUserTrans,
    CmdUserAuto, CmdUserAlias, CmdUserUnalias, CmdUserMessage,
    CmdUserResend, CmdUserPeek, CmdUserAsSession, CmdUserVerbose,
    CmdUserRandomSet, CmdUserIgnoreStatus, CmdUserSMS, CmdUserStatusDetail,
    CmdUserStatusWide, CmdUserSound, CmdUserSoundOnline, CmdUserRegister,
    CmdUserStatusMeta, CmdUserSoundOffline, CmdUserAutoaway, CmdUserSet,
    CmdUserClear, CmdUserTogIgnore, CmdUserTogInvis, CmdUserTogVisible,
    CmdUserAdd, CmdUserRemove, CmdUserAuth, CmdUserURL, CmdUserSave,
    CmdUserTabs, CmdUserLast, CmdUserHistory, CmdUserFind, CmdUserUptime,
    CmdUserOldSearch, CmdUserSearch, CmdUserUpdate, CmdUserPass,
    CmdUserOther, CmdUserAbout, CmdUserQuit, CmdUserConn, CmdUserContact,
    CmdUserAnyMess, CmdUserGetAuto, CmdUserOpt, CmdUserPrompt, CmdUserGmail;

#ifdef ENABLE_PEER2PEER
static jump_f CmdUserPeer;
#endif

#ifdef ENABLE_DEBUG
static jump_f CmdUserListQueue;
#endif

#ifdef ENABLE_OTR
static jump_f CmdUserOtr;
#endif

static void CmdUserProcess (const char *command, time_t *idle_val, idleflag_t *idle_flag);

/* 1 = do not apply idle stuff next time     v
   2 = count this line as being idle         v */
static jump_t jump[] = {
    { &CmdUserRandom,        "rand",         0,   0 },
    { &CmdUserRandomSet,     "setr",         0,   0 },
    { &CmdUserHelp,          "help",         0,   0 },
    { &CmdUserInfo,          "f",            0,   0 },
    { &CmdUserInfo,          "finger",       0,   0 },
    { &CmdUserInfo,          "info",         0,   0 },
    { &CmdUserInfo,          "rinfo",        0,   1 },
    { &CmdUserTrans,         "lang",         0,   0 },
    { &CmdUserTrans,         "trans",        0,   0 },
    { &CmdUserAuto,          "auto",         0,   0 },
    { &CmdUserAlias,         "alias",        0,   0 },
    { &CmdUserUnalias,       "unalias",      0,   0 },
    { &CmdUserAnyMess,       "_msg",         0,   0 },
    { &CmdUserMessage,       "msg",          0,   1 },
    { &CmdUserMessage,       "chat",         0,   8 },
    { &CmdUserGetAuto,       "getauto",      0,   0 },
    { &CmdUserResend,        "resend",       0,   0 },
    { &CmdUserVerbose,       "verbose",      0,   0 },
    { &CmdUserIgnoreStatus,  "i",            0,   0 },
    { &CmdUserStatusDetail,  "status",       2,     2     + 8           + 512 },
    { &CmdUserStatusDetail,  "s",            2,     2 + 4 + 8 + 16      + 512 },
    { &CmdUserStatusDetail,  "ww",           2,     2 },
    { &CmdUserStatusDetail,  "ee",           2, 1 + 2 },
    { &CmdUserStatusDetail,  "w",            2,         4 },
    { &CmdUserStatusDetail,  "e",            2, 1     + 4 },
    { &CmdUserStatusDetail,  "wwg",          2,     2              + 32 },
    { &CmdUserStatusDetail,  "eeg",          2, 1 + 2              + 32 },
    { &CmdUserStatusDetail,  "wg",           2,         4          + 32 },
    { &CmdUserStatusDetail,  "eg",           2, 1     + 4          + 32 },
    { &CmdUserStatusDetail,  "wwv",          2,     2                   + 64 },
    { &CmdUserStatusDetail,  "eev",          2, 1 + 2                   + 64 },
    { &CmdUserStatusDetail,  "wv",           2,         4               + 64 },
    { &CmdUserStatusDetail,  "ev",           2, 1     + 4               + 64 },
    { &CmdUserStatusDetail,  "wwgv",         2,     2              + 32 + 64 },
    { &CmdUserStatusDetail,  "eegv",         2, 1 + 2              + 32 + 64 },
    { &CmdUserStatusDetail,  "wgv",          2,         4          + 32 + 64 },
    { &CmdUserStatusDetail,  "egv",          2, 1     + 4          + 32 + 64 },
    { &CmdUserStatusDetail,  "_s",           2,   -1 },
    { &CmdUserStatusMeta,    "ss",           2,   1 },
    { &CmdUserStatusMeta,    "meta",         2,   0 },
    { &CmdUserStatusWide,    "wide",         2,   1 },
    { &CmdUserStatusWide,    "ewide",        2,   0 },
    { &CmdUserSet,           "set",          0,   0 },
    { &CmdUserOpt,           "opt",          0,   0 },
    { &CmdUserOpt,           "optglobal",    0, COF_GLOBAL },
    { &CmdUserOpt,           "optserv",      0, COF_SERVER },
    { &CmdUserOpt,           "optconnection",0, COF_SERVER },
    { &CmdUserOpt,           "optgroup",     0, COF_GROUP },
    { &CmdUserOpt,           "optcontact",   0, COF_CONTACT },
    { &CmdUserSound,         "sound",        2,   0 },
    { &CmdUserSoundOnline,   "soundonline",  2,   0 },
    { &CmdUserSoundOffline,  "soundoffline", 2,   0 },
    { &CmdUserAutoaway,      "autoaway",     2,   0 },
    { &CmdUserChange,        "change",       1, -1          },
    { &CmdUserChange,        "online",       1, ims_online  },
    { &CmdUserChange,        "away",         1, ims_away    },
    { &CmdUserChange,        "na",           1, ims_na      },
    { &CmdUserChange,        "occ",          1, ims_occ     },
    { &CmdUserChange,        "dnd",          1, ims_dnd     },
    { &CmdUserChange,        "ffc",          1, ims_ffc     },
    { &CmdUserChange,        "inv",          1, ims_inv     },
    { &CmdUserClear,         "clear",        2,   0 },
    { &CmdUserTogIgnore,     "togig",        0,   0 },
    { &CmdUserTogVisible,    "togvis",       0,   0 },
    { &CmdUserTogInvis,      "toginv",       0,   0 },
    { &CmdUserAdd,           "add",          0,   0 },
    { &CmdUserAdd,           "addalias",     0,   1 },
    { &CmdUserAdd,           "addgroup",     0,   2 },
    { &CmdUserRemove,        "rem",          0,   0 },
    { &CmdUserRemove,        "remalias",     0,   1 },
    { &CmdUserRemove,        "remcont",      0,   2 },
    { &CmdUserRemove,        "remgroup",     0,   4 },
    { &CmdUserRegister,      "reg",          0,   0 },
    { &CmdUserAuth,          "auth",         0,   0 },
    { &CmdUserURL,           "url",          0,   0 },
    { &CmdUserSave,          "save",         0,   0 },
    { &CmdUserTabs,          "tabs",         0,   0 },
    { &CmdUserLast,          "last",         0,   0 },
    { &CmdUserUptime,        "uptime",       2,   0 },
    { &CmdUserHistory,       "h",            2,   0 },
    { &CmdUserHistory,       "history",      2,   0 },
    { &CmdUserHistory,       "historyd",     2,   1 },
    { &CmdUserFind,          "find",         2,   0 },
    { &CmdUserFind,          "finds",        2,   1 },
    { &CmdUserQuit,          "q",            0,   1 },
    { &CmdUserQuit,          "quit",         0,   1 },
    { &CmdUserQuit,          "x",            0,   2 },
    { &CmdUserQuit,          "exit",         0,   1 },
    { &CmdUserPass,          "pass",         0,   0 },
    { &CmdUserPrompt,        "prompt",       0,   0 },
    { &CmdUserSMS,           "sms",          0,   0 },
    { &CmdUserPeek,          "peek",         0,   0 },
    { &CmdUserPeek,          "peekall",      0,   1 },
    { &CmdUserGetAuto,       "peek2",        0,   MSGF_GETAUTO | MSG_GET_AWAY },
    { &CmdUserAsSession,     "as",           0,   0 },
    { &CmdUserContact,       "contact",      0,   0 },
                                                                
    { &CmdUserOldSearch,     "oldsearch",    0,   0 },
    { &CmdUserSearch,        "search",       0,   0 },
    { &CmdUserUpdate,        "update",       0,   0 },
    { &CmdUserOther,         "other",        0,   0 },
    { &CmdUserAbout,         "about",        0,   0 },
    { &CmdUserConn,          "conn",         0,   0 },
    { &CmdUserConn,          "login",        0, 202 },
#ifdef ENABLE_XMPP
    { &CmdUserGmail,         "gmail",        0,   0 },
#endif
#ifdef ENABLE_PEER2PEER
    { &CmdUserPeer,          "peer",         0,   0 },
    { &CmdUserPeer,          "tcp",          0,   0 },
    { &CmdUserPeer,          "file",         0,   4 },
    { &CmdUserPeer,          "accept",       0,  22 },
#endif
#ifdef ENABLE_TCL
    { &CmdUserTclScript,     "tclscript",    0,   0 },
    { &CmdUserTclScript,     "tcl",          0,   1 },
#endif
#ifdef ENABLE_DEBUG
    { &CmdUserListQueue,     "_queue",       0,   0 },
#endif
#ifdef ENABLE_OTR
    { &CmdUserOtr,           "otr",          0,   0 },
#endif
    { NULL, NULL, 0, 0 }
};

/* Have an opened server connection ready. */
#define OPENCONN \
    if (!uiG.conn) uiG.conn = ServerNr (0); \
    if (!uiG.conn || ~uiG.conn->conn->connect & CONNECT_OK) \
    { rl_print (i18n (1931, "Current session is closed. Try another or open this one.\n")); return 0; } \

/* Try to have any server connection ready. */
#define ANYCONN if (!uiG.conn) uiG.conn = ServerNr (0); \
    if (!uiG.conn) { rl_print (i18n (2397, "No server session found.\n")); return 0; }

/*
 * Returns a pointer to the jump table.
 */
jump_t *CmdUserTable (void)
{
    return jump;
}

/*
 * Looks up an entry in the jump table.
 */
jump_t *CmdUserLookup (const char *cmd)
{
    jump_t *j;
    for (j = CmdUserTable (); j->f; j++)
        if (!strcasecmp (cmd, j->name))
            return j;
    return NULL;
}

/*
 * Change status.
 */
static JUMP_F(CmdUserChange)
{
    char *arg1 = NULL;
    status_t sdata = data;
    ContactGroup *cg = NULL;
    Contact *cont;
    int i;
    ANYCONN;

    if (data + 1 == 0)
    {
        if (!s_parseint (&args, &data))
        {
            rl_print (i18n (1703, "Status modes:\n"));
            rl_printf ("  %-20s %d\n", i18n (1921, "Online"),         ims_online);
            rl_printf ("  %-20s %d\n", i18n (1923, "Away"),           ims_away);
            rl_printf ("  %-20s %d\n", i18n (1922, "Do not disturb"), ims_dnd);
            rl_printf ("  %-20s %d\n", i18n (1924, "Not Available"),  ims_na);
            rl_printf ("  %-20s %d\n", i18n (1927, "Free for chat"),  ims_ffc);
            rl_printf ("  %-20s %d\n", i18n (1925, "Occupied"),       ims_occ);
            rl_printf ("  %-20s %d\n", i18n (1926, "Invisible"),      ims_inv);
            return 0;
        }
        sdata = IcqToStatus (data);
    }

    if (s_parsekey (&args, "for"))
    {
        if (!(cg = s_parselist (&args, uiG.conn)))
            return 0;

        if (!(arg1 = s_parserem (&args)))
            arg1 = "";

        for (i = 0; (cont = ContactIndex (cg, i)); i++)
            IMSetStatus (NULL, cont, sdata, arg1);
    }
    else
    {
        OptSetStr (&prG->copts, CO_TAUTODND,  NULL);
        OptSetStr (&prG->copts, CO_TAUTOOCC,  NULL);
        OptSetStr (&prG->copts, CO_TAUTONA,   NULL);
        OptSetStr (&prG->copts, CO_TAUTOAWAY, NULL);
        OptSetStr (&prG->copts, CO_TAUTOFFC,  NULL);
        OptSetStr (&prG->copts, CO_TAUTOINV,  NULL);

        if (!(arg1 = s_parserem (&args)))
            arg1 = "";

        switch (ContactClearInv (sdata))
        {
            case imr_dnd:  OptSetStr (&prG->copts, CO_TAUTODND,  arg1); break;
            case imr_occ:  OptSetStr (&prG->copts, CO_TAUTOOCC,  arg1); break;
            case imr_na:   OptSetStr (&prG->copts, CO_TAUTONA,   arg1); break;
            case imr_away: OptSetStr (&prG->copts, CO_TAUTOAWAY, arg1); break;
            case imr_ffc:  OptSetStr (&prG->copts, CO_TAUTOFFC,  arg1); break;
            case imr_offline: break;
            case imr_online:
                if (sdata == ims_inv)
                           OptSetStr (&prG->copts, CO_TAUTOINV,  arg1); break;
        }
        IMSetStatus (uiG.conn, NULL, sdata, arg1);
    }
    return 0;
}

/*
 * Finds random user.
 */
static JUMP_F(CmdUserRandom)
{
    UDWORD arg1 = 0;
    OPENCONN;
    
    if (!s_parseint (&args, &arg1))
    {
        rl_print (i18n (1704, "Groups:\n"));
        rl_printf ("  %2d %s\n",  1, i18n (1705, "General"));
        rl_printf ("  %2d %s\n",  2, i18n (1706, "Romance"));
        rl_printf ("  %2d %s\n",  3, i18n (1707, "Games"));
        rl_printf ("  %2d %s\n",  4, i18n (1708, "Students"));
        rl_printf ("  %2d %s\n",  6, i18n (1709, "20 something"));
        rl_printf ("  %2d %s\n",  7, i18n (1710, "30 something"));
        rl_printf ("  %2d %s\n",  8, i18n (1711, "40 something"));
        rl_printf ("  %2d %s\n",  9, i18n (1712, "50+"));
        rl_printf ("  %2d %s\n", 10, i18n (1713, "Seeking women"));
        rl_printf ("  %2d %s\n", 11, i18n (1714, "Seeking men"));
        rl_printf ("  %2d %s\n", 49, i18n (1715, "climm"));
    }
    else
        IMCliInfo (uiG.conn, NULL, arg1);
    return 0;
}

/*
 * Sets the random user group.
 */
static JUMP_F(CmdUserRandomSet)
{
    UDWORD arg1 = 0;
    OPENCONN;
    
    if (!s_parseint (&args, &arg1))
    {
        rl_print (i18n (1704, "Groups:\n"));
        rl_printf ("  %2d %s\n", uiG.conn->pref_version > 6 ? 0 : -1, i18n (1716, "None"));
        rl_printf ("  %2d %s\n",  1, i18n (1705, "General"));
        rl_printf ("  %2d %s\n",  2, i18n (1706, "Romance"));
        rl_printf ("  %2d %s\n",  3, i18n (1707, "Games"));
        rl_printf ("  %2d %s\n",  4, i18n (1708, "Students"));
        rl_printf ("  %2d %s\n",  6, i18n (1709, "20 something"));
        rl_printf ("  %2d %s\n",  7, i18n (1710, "30 something"));
        rl_printf ("  %2d %s\n",  8, i18n (1711, "40 something"));
        rl_printf ("  %2d %s\n",  9, i18n (1712, "50+"));
        rl_printf ("  %2d %s\n", 10, i18n (1713, "Seeking women"));
        rl_printf ("  %2d %s\n", 11, i18n (1714, "Seeking men"));
        rl_printf ("  %2d %s\n", 49, i18n (1715, "climm"));
    }
    else
    {
        if (uiG.conn->type == TYPE_SERVER)
            SnacCliSetrandom (uiG.conn, arg1);
    }
    return 0;
}

/*
 * Displays help.
 */
static JUMP_F(CmdUserHelp)
{
    struct { const char *category; const char *keyword; const char *help; const char *cmds; const char *warn; } myhelp[] = {
      { _i18n (1448, "Message"), "message",
        _i18n (1446, "Commands relating to sending messages."),
        "msg, a, r, url, sms, chat, getauto, auth, resend, last, h = history, historyd, find, finds, tabs", NULL },
      { _i18n (2541, "Status"), "status",
        _i18n (2542, "Commands to change your status."),
        "login, online, away, na, occ, dnd, ffc, inv, change", NULL },
      { _i18n (1449, "User"), "user",
        _i18n (1444, "Commands relating to seeing and finding other users."),
        "f = finger, ss, i, s, e, ee, eg, eeg, ev, eev, egv, eegv, w, ..., wwgv, ewide, wide, search, rand", NULL },
      { _i18n (2543, "Contacts"), "contacts",
        _i18n (2544, "Commands to modify your contact list."),
        "add, rem, togig, toginv, togvis, addgroup, addalias, remgroup, remalias", NULL },
      { _i18n (1450, "Account"), "account",
        _i18n (1445, "Commands relating to your ICQ account."),
        "pass, update, other, about, setr, reg", NULL },
      { _i18n (1447, "Client"), "client",
        _i18n (1443, "Commands relating to climm displays and configuration."),
        "verbose, clear, sound, prompt, autoaway, auto, alias, unalias, lang, uptime, set, opt, optcontact, optgroup, optconnection, optglobal, save, q = quit = exit, x, !", NULL },
      { _i18n (2171, "Advanced"), "advanced",
        _i18n (2172, "Advanced commands."),
#ifdef ENABLE_XMPP
        "meta, conn, peer, file, accept, contact, peek, peek2, peekall, as, gmail",
#else
        "meta, conn, peer, file, accept, contact, peek, peek2, peekall, as",
#endif
        _i18n (2314, "These are advanced commands. Be sure to have read the manual pages for complete information.\n") },
#ifdef ENABLE_TCL
      { _i18n (2342, "Scripting"), "scripting",
        _i18n (2343, "Advanced commands for scripting."),
        "tclscript, tcl",
        _i18n (2314, "These are advanced commands. Be sure to have read the manual pages for complete information.\n") },
#endif
      { "0000:", "--none--", NULL, NULL, NULL },
      { "0000:", "all", NULL, "message status user contacts account client advanced scripting", NULL },
/* trans(=lang) info(=finger) status(>(ee?|ww?)v?g?) rinfo(>info) soundonline(>event) soundoffline(>event) oldsearch(>search) tcp(=peer) */
/* _s _msg _queue */

      { NULL }
    };

    strc_t par;
    char *newargs = NULL;
    int i;

    if (!(par = s_parse_s (&args, " \t\r\n,")))
    {
        rl_printf ("%s\n", i18n (1442, "Please select one of the help topics below."));
        for (i = 0; myhelp[i].cmds; i++)
        {
            rl_printf ("\n");
            rl_printf (i18n (2184, "%s%-14s%s - %s\n"), COLQUOTE,
                       i18n (-1, myhelp[i].category), COLNONE, myhelp[i].cmds);
            if (myhelp[i].help)
                rl_printf (i18n (2545, "     %s\n"), i18n (-1, myhelp[i].help));
            if (myhelp[i].warn)
                rl_printf ("%s", i18n (-1, myhelp[i].warn));
        }
        return 0;
    }

    do {
        for (i = 0; myhelp[i].category; i++)
        {
            if (myhelp[i].cmds && (!strcasecmp (par->txt, myhelp[i].keyword) || !strcasecmp (par->txt, i18n (-1, myhelp[i].category))))
            {
                char *line = malloc (strlen (args) + strlen (myhelp[i].cmds) + 2);
                sprintf (line, "%s %s", args, myhelp[i].cmds);
                s_free (newargs);
                args = newargs = line;
                i = -1;
                par = s_parse_s (&args, " \t\r\n,");
            }
        }
        /* Message */
        if (!strcasecmp (par->txt, "msg"))
        {
            CMD_USER_HELP  ("msg <contacts> [<message>]", i18n (1409, "Sends a message to <contacts>."));
            CMD_USER_HELP  ("  ", i18n (1721, "Sending a blank message will put the client into multiline mode.\nUse . on a line by itself to end message.\nUse # on a line by itself to cancel the message."));
        }
        else if (!strcasecmp (par->txt, "a"))
            CMD_USER_HELP  ("a <message>", i18n (1412, "Sends a message to the last person you sent a message to."));
        else if (!strcasecmp (par->txt, "r"))
            CMD_USER_HELP  ("r <message>", i18n (1414, "Replies to the last person to send you a message."));
        else if (!strcasecmp (par->txt, "url"))
            CMD_USER_HELP  ("url <contacts> <url> <message>", i18n (1410, "Sends a url and message to <contacts>."));
        else if (!strcasecmp (par->txt, "sms"))
            CMD_USER_HELP  ("sms <nick|cell> <message>", i18n (2039, "Sends a message to a (<nick>'s) <cell> phone."));
        else if (!strcasecmp (par->txt, "chat"))
            CMD_USER_HELP  ("chat <contacts> [<message>]", i18n (2569, "Send <message> and further messages to <contacts> until canceled."));
        else if (!strcasecmp (par->txt, "getauto"))
            CMD_USER_HELP  ("getauto [auto|away|na|dnd|occ|ffc] [<contacts>]", i18n (2304, "Get automatic reply from all <contacts> for current status, away, not available, do not disturb, occupied, or free for chat."));
        else if (!strcasecmp (par->txt, "auth"))
            CMD_USER_HELP  ("auth [grant|deny|req|add] <contacts>", i18n (2313, "Grant, deny, request or acknowledge authorization from/to <contacts> to you/them to their/your contact list."));
        else if (!strcasecmp (par->txt, "resend"))
            CMD_USER_HELP  ("resend <contacts>", i18n (1770, "Resend your last message to <contacts>."));
        else if (!strcasecmp (par->txt, "last"))
            CMD_USER_HELP  ("last [<contacts>]", i18n (1403, "Displays the last message received from <contacts> or from everyone."));
        else if (!strcasecmp (par->txt, "h"))
            CMD_USER_HELP  ("h <contact> [<last> [<count>]]", i18n (2383, "View your last messages of <contact>. Type 'h' as short command."));
        else if (!strcasecmp (par->txt, "history"))
            CMD_USER_HELP  ("history <contact> [<last> [<count>]]", i18n (2383, "View your last messages of <contact>. Type 'h' as short command."));
        else if (!strcasecmp (par->txt, "historyd"))
            CMD_USER_HELP  ("historyd <contact|*> [<date> [<count>]]", i18n (2395, "View your messages of <contact> or all contacts (for *) since <date>."));
        else if (!strcasecmp (par->txt, "find"))
            CMD_USER_HELP  ("find <contact> <pattern>", i18n (2384, "Case insensitive search of <pattern> in log file of <contact>."));
        else if (!strcasecmp (par->txt, "finds"))
            CMD_USER_HELP  ("finds <contact> <pattern>", i18n (2391, "Case sensitive search of <pattern> in log file of <contact>."));
        else if (!strcasecmp (par->txt, "tabs"))
            CMD_USER_HELP  ("tabs", i18n (1098, "Display a list of nicknames that you can tab through.")); 
        /* Status */
        else if (!strcasecmp (par->txt, "login"))
            CMD_USER_HELP  ("login", "= conn login");
        else if (!strcasecmp (par->txt, "online"))
            CMD_USER_HELP  ("online [for <contacts>] [<away-message>]", i18n (1431, "Set status to \"online\"."));
        else if (!strcasecmp (par->txt, "away"))
            CMD_USER_HELP  ("away [for <contacts>] [<away-message>]", i18n (1432, "Set status to \"away\"."));
        else if (!strcasecmp (par->txt, "na"))
            CMD_USER_HELP  ("na [for <contacts>] [<away-message>]", i18n (1433, "Set status to \"not available\"."));
        else if (!strcasecmp (par->txt, "occ"))
            CMD_USER_HELP  ("occ [for <contacts>] [<away-message>]", i18n (1434, "Set status to \"occupied\"."));
        else if (!strcasecmp (par->txt, "dnd"))
            CMD_USER_HELP  ("dnd [for <contacts>] [<away-message>]", i18n (1435, "Set status to \"do not disturb\"."));
        else if (!strcasecmp (par->txt, "ffc"))
            CMD_USER_HELP  ("ffc [for <contacts>] [<away-message>]", i18n (1436, "Set status to \"free for chat\"."));
        else if (!strcasecmp (par->txt, "inv"))
            CMD_USER_HELP  ("inv [for <contacts>] [<away-message>]", i18n (1437, "Set status to \"invisible\"."));
        else if (!strcasecmp (par->txt, "change"))
            CMD_USER_HELP  ("change <status> [for <contacts>] [<away-message>]", i18n (1427, "Changes your status to the status number, or list the available modes."));
        /* User */
        else if (!strcasecmp (par->txt, "f"))
            CMD_USER_HELP  ("f <uin|nick>", i18n (1430, "Displays general info on <uin> or <nick>."));
        else if (!strcasecmp (par->txt, "finger"))
            CMD_USER_HELP  ("finger <uin|nick>", i18n (1430, "Displays general info on <uin> or <nick>."));
        else if (!strcasecmp (par->txt, "ss"))
            CMD_USER_HELP  ("ss <contacts>", i18n (2547, "Displays saved meta data about <contacts>."));
        else if (!strcasecmp (par->txt, "i"))
            CMD_USER_HELP  ("i", i18n (1405, "Lists ignored nicks/uins."));
        else if (!strcasecmp (par->txt, "s"))
            CMD_USER_HELP  ("s <uin|nick>", i18n (1400, "Shows locally stored info on <uin> or <nick>, or on yourself."));
        else if (!strcasecmp (par->txt, "e"))
            CMD_USER_HELP  ("e", i18n (1407, "Displays the current status of online people on your contact list."));
        else if (!strcasecmp (par->txt, "ee"))
            CMD_USER_HELP  ("ee", i18n (2177, "Displays verbosely the current status of online people on your contact list."));
        else if (!strcasecmp (par->txt, "eg"))
            CMD_USER_HELP  ("eg", i18n (2307, "Displays the current status of online people on your contact list, sorted by contact group."));
        else if (!strcasecmp (par->txt, "eeg"))
            CMD_USER_HELP  ("eeg", i18n (2309, "Displays verbosely the current status of online people on your contact list, sorted by contact group."));
        else if (!strcasecmp (par->txt, "ev"))
            CMD_USER_HELP  ("ev", i18n (2548, "Displays the current status of online people on your contact list, including otherwise hidden contacts."));
        else if (!strcasecmp (par->txt, "eev"))
            CMD_USER_HELP  ("eev", i18n (2549, "Displays verbosely the current status of online people on your contact list, including otherwise hidden contacts."));
        else if (!strcasecmp (par->txt, "egv"))
            CMD_USER_HELP  ("egv", i18n (2550, "Displays the current status of online people on your contact list, sorted by contact group, including otherwise hidden contacts."));
        else if (!strcasecmp (par->txt, "eegv"))
            CMD_USER_HELP  ("eegv", i18n (2551, "Displays verbosely the current status of online people on your contact list, sorted by contact group, including otherwise hidden contacts."));
        else if (!strcasecmp (par->txt, "w"))
            CMD_USER_HELP  ("w", i18n (1416, "Displays the current status of everyone on your contact list."));
        else if (!strcasecmp (par->txt, "ww"))
            CMD_USER_HELP  ("ww", i18n (2176, "Displays verbosely the current status of everyone on your contact list."));
        else if (!strcasecmp (par->txt, "wg"))
            CMD_USER_HELP  ("wg", i18n (2308, "Displays the current status of everyone on your contact list, sorted by contact group."));
        else if (!strcasecmp (par->txt, "wwg"))
            CMD_USER_HELP  ("wwg", i18n (2310, "Displays verbosely the current status of everyone on your contact list, sorted by contact group."));
        else if (!strcasecmp (par->txt, "wv"))
            CMD_USER_HELP  ("wv", i18n (2552, "Displays the current status of everyone on your contact list, including otherwise hidden contacts."));
        else if (!strcasecmp (par->txt, "wwv"))
            CMD_USER_HELP  ("wwv", i18n (2553, "Displays verbosely the current status of everyone on your contact list, including otherwise hidden contacts."));
        else if (!strcasecmp (par->txt, "wgv"))
            CMD_USER_HELP  ("wgv", i18n (2554, "Displays the current status of everyone on your contact list, sorted by contact group, including otherwise hidden contacts."));
        else if (!strcasecmp (par->txt, "wwgv"))
            CMD_USER_HELP  ("wwgv", i18n (2555, "Displays verbosely the current status of everyone on your contact list, sorted by contact group, including otherwise hidden contacts."));
        else if (!strcasecmp (par->txt, "ewide"))
            CMD_USER_HELP  ("ewide", i18n (2042, "Displays a list of online people on your contact list in a screen wide format.")); 
        else if (!strcasecmp (par->txt, "wide"))
            CMD_USER_HELP  ("wide", i18n (1801, "Displays a list of people on your contact list in a screen wide format.")); 
        else if (!strcasecmp (par->txt, "search"))
            CMD_USER_HELP  ("search [<email>|<nick>|<first> <last>]", i18n (1429, "Searches for an ICQ user."));
        else if (!strcasecmp (par->txt, "rand"))
            CMD_USER_HELP  ("rand [<nr>]", i18n (1415, "Finds a random user in interest group <nr> or lists the groups."));
        /* Contacts */
        else if (!strcasecmp (par->txt, "add"))
            CMD_USER_HELP  ("add [<uin|nick> [<new nick>]|<group> [<contacts>]]", i18n (2556, "Add contact, alias, or contact to group."));
        else if (!strcasecmp (par->txt, "rem"))
            CMD_USER_HELP  ("rem [<group>] <contacts>", i18n (2557, "Remove alias, or contact from group or contact list."));
        else if (!strcasecmp (par->txt, "togig"))
            CMD_USER_HELP  ("togig <contacts>", i18n (1404, "Toggles ignoring/unignoring the <contacts>."));
        else if (!strcasecmp (par->txt, "toginv"))
            CMD_USER_HELP  ("toginv <contacts>", i18n (2045, "Toggles your visibility to <contacts> when you're online."));
        else if (!strcasecmp (par->txt, "togvis"))
            CMD_USER_HELP  ("togvis <contacts>", i18n (1406, "Toggles your visibility to <contacts> when you're invisible."));
        else if (!strcasecmp (par->txt, "addgroup"))
            CMD_USER_HELP  ("addgroup <group> [<contacts>]", i18n (1428, "Adds all contacts in <contacts> to contact group <group>."));
        else if (!strcasecmp (par->txt, "addalias"))
            CMD_USER_HELP  ("addalias <uin|nick> [<new nick>]", i18n (2311, "Adds <uin> as <new nick>, or add alias <new nick> for <uin> or <nick>."));
        else if (!strcasecmp (par->txt, "remgroup"))
            CMD_USER_HELP  ("remgroup [all] group <contacts>", i18n (2043, "Remove all contacts in <contacts> from contact group <group>."));
        else if (!strcasecmp (par->txt, "remalias"))
            CMD_USER_HELP  ("remalias [all] <contacts>", i18n (2312, "Remove all aliases in <contacts>, removes contact if 'all' is given."));
        /* Account */
        else if (!strcasecmp (par->txt, "pass"))
            CMD_USER_HELP  ("pass <password>", i18n (1408, "Changes your password to <password>."));
        else if (!strcasecmp (par->txt, "update"))
            CMD_USER_HELP  ("update", i18n (1438, "Updates your basic info (email, nickname, etc.)."));
        else if (!strcasecmp (par->txt, "other"))
            CMD_USER_HELP  ("other", i18n (1401, "Updates more user info like age and sex."));
        else if (!strcasecmp (par->txt, "about"))
            CMD_USER_HELP  ("about", i18n (1402, "Updates your about user info."));
        else if (!strcasecmp (par->txt, "setr"))
            CMD_USER_HELP  ("setr <nr>", i18n (1439, "Sets your random user group."));
        else if (!strcasecmp (par->txt, "reg"))
            CMD_USER_HELP  ("reg <password>", i18n (1426, "Creates a new UIN with the specified password."));
        /* Client */
        else if (!strcasecmp (par->txt, "verbose"))
            CMD_USER_HELP  ("verbose [<nr>]", i18n (1418, "Set the verbosity level, or display verbosity level."));
        else if (!strcasecmp (par->txt, "clear"))
            CMD_USER_HELP  ("clear", i18n (1419, "Clears the screen."));
        else if (!strcasecmp (par->txt, "prompt"))
            CMD_USER_HELP ("prompt [<user defined prompt>]", i18n (2629, "Set or show user defined prompt. See manual for details."));
        else if (!strcasecmp (par->txt, "sound"))
            CMD_USER_HELP3 ("sound [%s|%s|event]", i18n (1085, "on"), i18n (1086, "off"),
                            i18n (1420, "Switches beeping when receiving new messages on or off, or using the event script."));
        else if (!strcasecmp (par->txt, "autoaway"))
            CMD_USER_HELP  ("autoaway [<timeout>]", i18n (1767, "Toggles auto cycling to away/not available."));
        else if (!strcasecmp (par->txt, "auto"))
        {
            CMD_USER_HELP  ("auto [on|off]", i18n (1424, "Set whether autoreplying when not online, or displays setting."));
            CMD_USER_HELP  ("auto <status> <message>", i18n (1425, "Sets the message to send as an auto reply for the status."));
        }
        else if (!strcasecmp (par->txt, "alias"))
            CMD_USER_HELP  ("alias [auto[expand]] [<alias> [<expansion>]]", i18n (2300, "Set an alias or list current aliases."));
        else if (!strcasecmp (par->txt, "unalias"))
            CMD_USER_HELP  ("unalias <alias>", i18n (2301, "Delete an alias."));
        else if (!strcasecmp (par->txt, "lang"))
            CMD_USER_HELP  ("lang [<lang|nr>...]", i18n (1800, "Change the working language (and encoding) to <lang> or display string <nr>."));
        else if (!strcasecmp (par->txt, "uptime"))
            CMD_USER_HELP  ("uptime", i18n (1719, "Shows how long climm has been running and some statistics."));
        else if (!strcasecmp (par->txt, "set"))
            CMD_USER_HELP  ("set <option> <value>", i18n (2044, "Set, clear or display an <option>: color, delbs, auto, prompt, autosave, autofinger, linebreak, funny."));
        else if (!strcasecmp (par->txt, "opt"))
            CMD_USER_HELP  ("opt [<contact>|<contact group>|connection|global [<option> [<value>]]]", i18n (2398, "Set or display options for a contact group, a contact or global.\n"));
        else if (!strcasecmp (par->txt, "optcontact"))
            CMD_USER_HELP  ("optcontact <contact> [<option> [<value>]]", i18n (2558, "Set or display options for a contact.\n"));
        else if (!strcasecmp (par->txt, "optgroup"))
            CMD_USER_HELP  ("optgroup <contact group> [<option> [<value>]]", i18n (2559, "Set or display options for a contact group.\n"));
        else if (!strcasecmp (par->txt, "optconnection"))
            CMD_USER_HELP  ("optconnection [<option> [<value>]]", i18n (2563, "Set or display connection options.\n"));
        else if (!strcasecmp (par->txt, "optglobal"))
            CMD_USER_HELP  ("optglobal [<option> [<value>]]", i18n (2560, "Set or display global options.\n"));
        else if (!strcasecmp (par->txt, "save"))
            CMD_USER_HELP  ("save", i18n (2036, "Save current preferences to disc."));
        else if (!strcasecmp (par->txt, "q"))
            CMD_USER_HELP  ("q [<msg>]", i18n (1422, "Logs off and quits."));
        else if (!strcasecmp (par->txt, "quit"))
            CMD_USER_HELP  ("quit [<msg>]", i18n (1422, "Logs off and quits."));
        else if (!strcasecmp (par->txt, "exit"))
            CMD_USER_HELP  ("exit [<msg>]", i18n (1422, "Logs off and quits."));
        else if (!strcasecmp (par->txt, "x"))
            CMD_USER_HELP  ("x [<msg>]", i18n (2561, "Logs off and quits without saving."));
        else if (!strcasecmp (par->txt, "!"))
            CMD_USER_HELP  ("! <cmd>", i18n (1717, "Execute shell command <cmd> (e.g. '!ls', '!dir', '!mkdir temp')."));
        /* Advanced */
        else if (!strcasecmp (par->txt, "meta"))
            CMD_USER_HELP  ("meta [show|load|save|set|get|rget] <contacts>", i18n (2305, "Handle meta data of contacts."));
        else if (!strcasecmp (par->txt, "conn"))
        {
            CMD_USER_HELP  ("conn open|login [<nr|uin>]", i18n (2038, "Opens connection number <nr> or connection for uin <uin>, or the first server connection."));
            CMD_USER_HELP  ("conn close|remove <nr>", i18n (2181, "Closes or removes connection number <nr>."));
            CMD_USER_HELP  ("conn select [<nr|uin>]", i18n (2182, "Select server connection number <nr> or connection for uin <uin> as current server connection."));
        }
        else if (!strcasecmp (par->txt, "peer"))
        {
            CMD_USER_HELP  ("peer open|close|off <uin|nick>...", i18n (2037, "Open or close a peer-to-peer connection, or disable using peer-to-peer connections for <uin> or <nick>."));
            CMD_USER_HELP  ("peer file <contacts> <file> <description>", i18n (2179, "Send all <contacts> a single file."));
            CMD_USER_HELP  ("peer files <uin|nick> <file1> <as1> ... <fileN> <asN> <description>", i18n (2180, "Send <uin> or <nick> several files."));
            CMD_USER_HELP  ("peer accept [<uin|nick>] [<id>]", i18n (2319, "Accept an incoming file transfer from <uin> or <nick>."));
            CMD_USER_HELP  ("peer deny [<uin|nick>] [<id>] [<reason>]", i18n (2369, "Refuse an incoming file transfer from <uin> or <nick>."));
#ifdef ENABLE_SSL
            CMD_USER_HELP  ("peer ssl <uin|nick>...", i18n (2399, "Request SSL connection."));
#endif
        }
        else if (!strcasecmp (par->txt, "file"))
            CMD_USER_HELP  ("file", "= peer file");
        else if (!strcasecmp (par->txt, "accept"))
            CMD_USER_HELP  ("accept", "= accept file");
        else if (!strcasecmp (par->txt, "contact"))
            CMD_USER_HELP  ("contact [show|diff|download|import|upload|delete]", i18n (2306, "Request server side contact list and show all or new contacts or import."));
        else if (!strcasecmp (par->txt, "peek"))
            CMD_USER_HELP  ("peek [all] [<contacts>]", i18n (2183, "Check all <contacts> whether they are offline or invisible."));
        else if (!strcasecmp (par->txt, "peek2"))
            CMD_USER_HELP  ("peek2 [<contacts>]", "= getauto away <contacts>");
        else if (!strcasecmp (par->txt, "peekall"))
            CMD_USER_HELP  ("peekall [<contacts>]", "= peek all <contacts>");
        else if (!strcasecmp (par->txt, "as"))
            CMD_USER_HELP  ("as <nr|uin> <cmd>", i18n (2562, "Execute <cmd> with connection <nr> or connection for uin <uin> as current server connection."));
#ifdef ENABLE_XMPP
        else if (!strcasecmp (par->txt, "gmail"))
            CMD_USER_HELP  ("gmail [<date>] [<query>]|more", i18n (2712, "Query emails matching <query> since <date>, or continue search."));
#endif
#ifdef ENABLE_TCL
        /* Scripting */
        else if (!strcasecmp (par->txt, "tclscript"))
            CMD_USER_HELP  ("tclscript <file>", i18n (2344, "Execute Tcl script from <file>."));
        else if (!strcasecmp (par->txt, "tcl"))
            CMD_USER_HELP  ("tcl <string>", i18n (2345, "Execute Tcl script in <string>."));
#endif
        else if (strcmp (par->txt, "=") && strcmp (par->txt, "..."))
            rl_printf (i18n (2546, "No help available for '%s'.\n"), par->txt);
    } while ((par = s_parse_s (&args, " \t\r\n,")));
    s_free (newargs);
    return 0;
}

#ifdef ENABLE_OTR
/*
 * OTR commands
 */
static JUMP_F(CmdUserOtr)
{
    Contact *cont = NULL;
    strc_t earg;

    if (!libotr_is_present)
    {
        rl_print (i18n (2634, "Install libOTR 3.0.0 or newer and enjoy off-the-record encrypted messages!\n"));
        return 0;
    }

    if (!data)
    {
        if      (s_parsekey (&args, "list"))   data = OTR_CMD_LIST;
        else if (s_parsekey (&args, "start"))  data = OTR_CMD_START;
        else if (s_parsekey (&args, "stop"))   data = OTR_CMD_STOP;
        else if (s_parsekey (&args, "trust"))  data = OTR_CMD_TRUST;
        else if (s_parsekey (&args, "genkey")) data = OTR_CMD_GENKEY;
        else if (s_parsekey (&args, "keys"))   data = OTR_CMD_KEYS;
        else                                   data = 0;
    }
    
    switch (data)
    {
        case 0:
            rl_print (i18n (2635, "otr                           - Off-the-Record Messaging\n"));
            rl_print (i18n (2636, "otr list [<contact>]          - List OTR connection states\n"));
            rl_print (i18n (2637, "otr start|stop <contact>      - Start/Stop OTR session\n"));
            rl_print (i18n (2638, "otr trust <contact> [<trust>] - Set/Show trust for active fingerprint\n"));
            rl_print (i18n (2639, "otr genkey                    - Generate private key\n"));
            rl_print (i18n (2640, "otr keys                      - List private keys\n"));
            break;
        case OTR_CMD_LIST:
            cont = s_parsenick (&args, uiG.conn);
            if (!cont && (earg = s_parse (&args)))
                rl_printf (i18n (1061, "'%s' not recognized as a nick name.\n"), earg->txt);
            else
                OTRContext (cont);
            break;
        case OTR_CMD_START:
        case OTR_CMD_STOP:
            cont = s_parsenick (&args, uiG.conn);
            if (cont)
                OTRStart (cont, data == OTR_CMD_START ? 1 : 0);
            else
                rl_printf (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
            break;
        case OTR_CMD_TRUST:
            cont = s_parsenick (&args, uiG.conn);
            if (!cont)
            {
                rl_printf (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
                break;
            }
            if (s_parsekey (&args, "trust") || s_parsekey (&args, "verify"))
                OTRSetTrust (cont, 2);
            else if (s_parsekey (&args, "mistrust") || s_parsekey (&args, "distrust") || s_parsekey (&args, "falsify"))
                OTRSetTrust (cont, 1);
            else if (s_parse (&args))
                rl_printf (i18n (2687, "Trust must be trust, verify, or mistrust, distrust, falsify.\n"));
            else
                OTRSetTrust (cont, 0);
            break;
        case OTR_CMD_GENKEY:
            OTRGenKey ();
            break;
        case OTR_CMD_KEYS:
            OTRListKeys ();
    }
    return 0;
}
#endif /* ENABLE_OTR */

/*
 * Sets a new password.
 */
static JUMP_F(CmdUserPass)
{
    char *arg1 = NULL;
    OPENCONN;
    
    if (!(arg1 = s_parserem (&args)))
        rl_print (i18n (2012, "No password given.\n"));
    else
    {
        if (*arg1 == '\xc3' && arg1[1] == '\xb3')
        {
            rl_printf (i18n (2198, "Unsuitable password '%s' - may not start with byte 0xf3.\n"), arg1);
            return 0;
        }
        if (uiG.conn->type == TYPE_SERVER)
            SnacCliMetasetpass (uiG.conn, arg1);
        s_repl (&uiG.conn->passwd, arg1);
        if (uiG.conn->pref_passwd && *uiG.conn->pref_passwd)
        {
            rl_print (i18n (2122, "Note: You need to 'save' to write new password to disc.\n"));
            uiG.conn->pref_passwd = strdup (arg1);
        }
    }
    return 0;
}

/*
 * Sets a new user prompt.
 */
static JUMP_F(CmdUserPrompt)
{
    char *arg1 = NULL;
    
    if (!(arg1 = s_parserem (&args)))
    {
        if (prG->prompt != NULL)
            rl_print (s_sprintf ("%s\n", prG->prompt));
    }
    else
    {
        if (prG->prompt != NULL)
            free (prG->prompt);
        prG->prompt = strdup (arg1);
        rl_print (i18n (2633, "Note: You need to 'save' to write new user prompt to disc.\n"));
    }
    return 0;
}

/*
 * Sends an SMS message.
 */
static JUMP_F(CmdUserSMS)
{
    Contact *cont = NULL;
    const char *cell = NULL;
    strc_t par;
    char *cellv, *text;
    UDWORD i;
    OPENCONN;
    
    if (uiG.conn->type != TYPE_SERVER)
    {
        rl_print (i18n (2013, "This command is v8 only.\n"));
        return 0;
    }
    if ((cont = s_parsenick (&args, uiG.conn)))
    {
        CONTACT_GENERAL (cont);
        cell = args;
        if ((!cont->meta_general->cellular || !cont->meta_general->cellular[0])
            && s_parseint (&cell, &i) && (par = s_parse (&args)))
        {
            s_repl (&cont->meta_general->cellular, par->txt);
            cont->updated = 0;
        }
        cell = cont->meta_general->cellular;
    }
    if (!cell || !cell)
    {
        if (!(par = s_parse (&args)))
        {
            rl_print (i18n (2014, "No number given.\n"));
            return 0;
        }
        cell = par->txt;
    }
    if (cell[0] != '+' || cell[1] == '0')
    {
        rl_printf (i18n (2250, "Number '%s' is not of the format +<countrycode><cellprovider><number>.\n"), cell);
        if (cont && cont->meta_general)
            s_repl (&cont->meta_general->cellular, NULL);
        return 0;
    }
    cellv = strdup (cell);
    if (!(text = s_parserem (&args)))
        rl_print (i18n (2015, "No message given.\n"));
    else
        SnacCliSendsms (uiG.conn, cellv, text);
    free (cellv);
    return 0;
}

/*
 * Queries basic info of a user
 */
static JUMP_F(CmdUserInfo)
{
    ContactGroup *cg;
    Contact *cont;
    int i;
    OPENCONN;
    
    if (data)
    {
        if ((cont = uiG.last_rcvd))
            IMCliInfo (cont->serv, cont, 0);
        return 0;
    }

    if ((cg = s_parselistrem (&args, uiG.conn)))
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
            IMCliInfo (cont->serv, cont, 0);
    return 0;
}

/*
 * Peeks whether a user is really offline.
 */
static JUMP_F(CmdUserPeek)
{
    ContactGroup *cg;
    Contact *cont;
    int i;
    OPENCONN;
    
    if (uiG.conn->type != TYPE_SERVER)
    {
        rl_print (i18n (2013, "This command is v8 only.\n"));
        return 0;
    }
    
    if ((cg = s_parselistrem (&args, uiG.conn)))
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
            SnacCliRequserinfo (uiG.conn, cont, -2);
    
    if (data || s_parsekey (&args, "all"))
        for (i = 0, cg = uiG.conn->contacts; (cont = ContactIndex (cg, i)); i++)
            if (ContactPrefVal (cont, CO_PEEKME) && (cont->status == ims_offline))
                SnacCliRequserinfo (uiG.conn, cont, -1);

    if ((cg = s_parselistrem (&args, uiG.conn)))
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
            SnacCliRequserinfo (uiG.conn, cont, -2);
    
    return 0;
}

/*
 * Gives information about internationalization and translates
 * strings by number.
 */
static JUMP_F(CmdUserTrans)
{
    strc_t par;
    char *t = NULL;
    const char *p = NULL, *arg2;
    int one = 0;

    while (1)
    {
        UDWORD j;
        int i, l = 0;

        if (s_parseint (&args, &j))
        {
            rl_printf ("%3ld:%s\n", UD2UL (j), i18n (j, i18n (1078, "No translation available.")));
            one = 1;
            continue;
        }
        if ((par = s_parse (&args)))
        {
            if (!strcmp (par->txt, "all"))
            {
                for (i = l = 1000; i - l < 100; i++)
                {
                    p = i18n (i, NULL);
                    if (p)
                    {
                        l = i;
                        rl_printf ("%3d:%s\n", i, p);
                    }
                }
            }
            else
            {
                if (!strcmp (par->txt, "en_US.US-ASCII") || !strcmp (par->txt, ".")
                    || !strcmp (par->txt, "none") || !strcmp (par->txt, "unload"))
                {
                    t = strdup (i18n (1089, "Unloaded translation."));
                    i18nClose ();
                    rl_printf ("%s\n", t);
                    free (t);
                    continue;
                }
                i = i18nOpen (par->txt);
                if (i == -1)
                    rl_printf (i18n (2316, "Translation %s not found.\n"), par->txt);
                else
                    rl_printf (i18n (2508, "No translation (%s) loaded (%s%d%s entries).\n"),
                        s_qquote (prG->locale), COLQUOTE, i, COLNONE);
            }
            one = 1;
        }
        else if (one)
            return 0;
        else
        {
            UDWORD v1 = 0, v2 = 0, v3 = 0, v4 = 0;
            char *arg1;
            const char *s = "1079:No translation; using compiled-in strings.\n";

            arg1 = strdup (i18n (1003, "0"));
            for (t = arg1; *t; t++)
                if (*t == '-')
                    *t = ' ';
            arg2 = arg1;
            s_parseint (&arg2, &v1);
            s_parseint (&arg2, &v2);
            s_parseint (&arg2, &v3);
            s_parseint (&arg2, &v4);
            
            /* i18n (1079, "Translation (%s, %s) from %s, last modified on %s by %s, for climm %d.%d.%d%s.\n") */
            rl_printf (i18n (-1, s),
                     i18n (1001, "<lang>"), i18n (1002, "<lang_cc>"), i18n (1004, "<translation authors>"),
                     i18n (1006, "<last edit date>"), i18n (1005, "<last editor>"),
                     v1, v2, v3, v4 ? s_sprintf (".%ld", UD2UL (v4)) : "");
            return 0;
        }
    }
}

/*
 * Request auto away/.. strings.
 */
static JUMP_F(CmdUserGetAuto)
{
    ContactGroup *cg;
    Contact *cont;
    int i;
    int one = 0;
    ANYCONN;

    if (!data)
    {
        if      (s_parsekey (&args, "auto")) data = 0;
        else if (s_parsekey (&args, "away")) data = MSGF_GETAUTO | MSG_GET_AWAY;
        else if (s_parsekey (&args, "na"))   data = MSGF_GETAUTO | MSG_GET_NA;
        else if (s_parsekey (&args, "dnd"))  data = MSGF_GETAUTO | MSG_GET_DND;
        else if (s_parsekey (&args, "ffc"))  data = MSGF_GETAUTO | MSG_GET_FFC;
        else if (s_parsekey (&args, "occ"))  data = MSGF_GETAUTO | MSG_GET_OCC;
#ifdef WIP
        else if (s_parsekey (&args, "ver"))  data = MSGF_GETAUTO | MSG_GET_VER;
#endif
    }

    if ((cg = s_parselistrem (&args, uiG.conn)))
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
        {
            int cdata;
            
            one = 1;
            if (!(cdata = data))
            {
                switch (ContactClearInv (cont->status))
                {
                    case imr_offline: continue;
                    case imr_dnd:     cdata = MSGF_GETAUTO | MSG_GET_DND;  break;
                    case imr_occ:     cdata = MSGF_GETAUTO | MSG_GET_OCC;  break;
                    case imr_na:      cdata = MSGF_GETAUTO | MSG_GET_NA;   break;
                    case imr_away:    cdata = MSGF_GETAUTO | MSG_GET_AWAY; break;
                    case imr_ffc:     cdata = MSGF_GETAUTO | MSG_GET_FFC;  break;
                    default:          continue;
                }
            }
            IMCliMsg (cont, cdata, "\xff", NULL);
            SnacCliRequserinfo (uiG.conn, cont, 3);
        }

    if (data || one)
        return 0;
    rl_print (i18n (2056, "getauto [auto] <nick> - Get the auto-response from the contact.\n"));
    rl_print (i18n (2057, "getauto away   <nick> - Get the auto-response for away from the contact.\n"));
    rl_print (i18n (2058, "getauto na     <nick> - Get the auto-response for not available from the contact.\n"));
    rl_print (i18n (2059, "getauto dnd    <nick> - Get the auto-response for do not disturb from the contact.\n"));
    rl_print (i18n (2060, "getauto ffc    <nick> - Get the auto-response for free for chat from the contact.\n"));
    return 0;
}

#ifdef ENABLE_PEER2PEER
/*
 * Manually handles peer-to-peer (TCP) connections.
 */
static JUMP_F(CmdUserPeer)
{
    ContactGroup *cg;
    Contact *cont;
    Connection *list = NULL;
    UDWORD seq;
    char *reason;
    int i;
    ANYCONN;

#ifdef CLIMM_XMPP_FILE_TRANSFER
    if (uiG.conn->type != TYPE_XMPP_SERVER)
#endif
        if (!uiG.conn || !(list = uiG.conn->oscar_dc))
        {
            rl_print (i18n (2011, "You do not have a listening peer-to-peer connection.\n"));
            return 0;
        }

    if (!data)
    {
        if      (s_parsekey (&args, "open"))   data = 1;
        else if (s_parsekey (&args, "close"))  data = 2;
        else if (s_parsekey (&args, "reset"))  data = 2;
        else if (s_parsekey (&args, "off"))    data = 3;
        else if (s_parsekey (&args, "file"))   data = 4;
        else if (s_parsekey (&args, "files"))  data = 5;
        else if (s_parsekey (&args, "accept")) data = 16 + 6;
        else if (s_parsekey (&args, "deny"))   data = 16 + 7;
#ifdef ENABLE_SSL
        else if (s_parsekey (&args, "ssl"))    data = 8;
#endif
        else if (s_parsekey (&args, "auto"))   data = 10;
        else if (s_parsekey (&args, "away"))   data = 11;
        else if (s_parsekey (&args, "na"))     data = 12;
        else if (s_parsekey (&args, "dnd"))    data = 13;
        else if (s_parsekey (&args, "ffc"))    data = 14;
        else
        {
            rl_print (i18n (1846, "Opens and closes direct (peer to peer) connections:\n"));
            rl_print (i18n (1847, "peer open  <nick> - Opens direct connection.\n"));
            rl_print (i18n (1848, "peer close <nick> - Closes/resets direct connection(s).\n"));
            rl_print (i18n (1870, "peer off   <nick> - Closes direct connection(s) and don't try it again.\n"));
            rl_print (i18n (2160, "peer file  <nick> <file> [<description>]\n"));
            rl_print (i18n (2110, "peer files <nick> <file1> <as1> ... [<description>]\n"));
            rl_print (i18n (2111, "                  - Send file1 as as1, ..., with description.\n"));
            rl_print (i18n (2112, "                  - as = '/': strip path, as = '.': as is\n"));
            rl_print (i18n (2320, "peer accept <nick> [<id>]\n                  - accept an incoming file transfer.\n"));
            rl_print (i18n (2368, "peer deny <nick> [<id>] [<reason>]\n                  - deny an incoming file transfer.\n"));
#ifdef ENABLE_SSL
            rl_print (i18n (2378, "peer ssl <nick>   - initiate SSL handshake.\n"));
#endif
            return 0;
        }
    }
    if ((cg = s_parselist (&args, uiG.conn)) || (data & 16))
    {
        data &= 15;
        switch (data)
        {
            case 1:
                for (i = 0; (cont = ContactIndex (cg, i)); i++)
                    if (!TCPDirectOpen  (list, cont))
                        rl_printf (i18n (2142, "Direct connection with %s not possible.\n"), cont->nick);
                break;
            case 2:
                for (i = 0; (cont = ContactIndex (cg, i)); i++)
                    TCPDirectClose (list, cont);
                break;
            case 3:
                for (i = 0; (cont = ContactIndex (cg, i)); i++)
                    TCPDirectOff   (list, cont);
                break;
            case 4:
                {
                    strc_t par;
                    const char *files[1], *ass[1];
                    char *des = NULL, *file;
                    
                    if (!(par = s_parse (&args)))
                    {
                        rl_print (i18n (2158, "No file name given.\n"));
                        break;
                    }
                    files[0] = file = strdup (par->txt);
                    if (!(des = s_parserem (&args)))
                        des = file;
                    des = strdup (des);

                    ass[0] = (strchr (file, '/')) ? strrchr (file, '/') + 1 : file;
                    
                    for (i = 0; (cont = ContactIndex (cg, i)); i++)
                        if (!TCPSendFiles (list, cont, des, files, ass, 1))
                            rl_printf (i18n (2142, "Direct connection with %s not possible.\n"), cont->nick);
                    
                    free (file);
                    free (des);
                }
                break;
            case 5:
                {
                    char *files[10], *ass[10], *des = NULL;
                    const char *as;
                    int count;
                    
                    for (count = 0; count < 10; count++)
                    {
                        strc_t par;
                        if (!(par = s_parse (&args)))
                        {
                            des = strdup (i18n (2159, "Some files."));
                            break;
                        }
                        files[count] = des = strdup (par->txt);
                        if (!(par = s_parse (&args)))
                            break;
                        as = par->txt;
                        if (*as == '/' && !*(as + 1))
                            as = (strchr (des, '/')) ? strrchr (des, '/') + 1 : des;
                        if (*as == '.' && !*(as + 1))
                            as = des;
                        ass[count] = strdup (as);
                    }
                    if (!count)
                    {
                        free (des);
                        rl_print (i18n (2158, "No file name given.\n"));
                        break;
                    }
                    for (i = 0; (cont = ContactIndex (cg, i)); i++)
                        if (!TCPSendFiles (list, cont, des, (const char **)files, (const char **)ass, count))
                            rl_printf (i18n (2142, "Direct connection with %s not possible.\n"), cont->nick);
                    
                    while (count--)
                    {
                       free (ass[count]);
                       free (files[count]);
                    }
                    free (des);
                }
                break;
            case 6:
            case 7:
                s_parseint (&args, &seq);
                if (data == 6)
                    reason = NULL;
                else if (!(reason = s_parserem (&args)))
                    reason = "refused";

                if (!cg || !ContactIndex (cg, 0))
                    PeerFileUser (seq, NULL, reason, uiG.conn);
                else
                    for (i = 0; (cont = ContactIndex (cg, i)); i++)
                        PeerFileUser (seq, cont, reason, uiG.conn);
                break;
#ifdef ENABLE_SSL
            case 8:
                for (i = 0; (cont = ContactIndex (cg, i)); i++)
                    TCPSendSSLReq (list, cont);
                break;
#endif
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
                rl_printf (i18n (2260, "Please try the getauto command instead.\n"));
        }
    }
    return 0;
}
#endif /* ENABLE_PEER2PEER */

/*
 * Obsolete.
 */
static JUMP_F(CmdUserAuto)
{
    rl_printf (i18n (2401, "Auto reply messages are now contact options, see the %s command.\n"), s_wordquote ("opt"));
    rl_printf (i18n (2402, "The global auto reply flag is handled by the %s command.\n"), s_wordquote ("set"));
    return 0;
}

/*
 * Add an alias.
 */

static JUMP_F(CmdUserAlias)
{
    strc_t name;
    char *exp = NULL;
    char autoexpand = FALSE;
    const alias_t *node;

    if (s_parsekey (&args, "autoexpand") || s_parsekey (&args, "auto"))
        autoexpand = TRUE;
    
    if (!(name = s_parse_s (&args, " \t\r\n=")))
    {
        for (node = AliasList (); node; node = node->next)
            rl_printf ("alias %s%s %s\n", node->autoexpand ? "autoexpand " : "           ",
                s_cquote (node->command, COLQUOTE), node->replace);
        return 0;
    }
    
    if ((exp = strpbrk (name->txt, " \t\r\n")))
    {
        rl_printf (i18n (2303, "Invalid character 0x%x in alias name.\n"), *exp);
        return 0;
    }

    if (*args == '=')
        args++;
    
    if ((exp = s_parserem (&args)))
        AliasAdd (name->txt, exp, autoexpand);
    else
    {
        for (node = AliasList (); node; node = node->next)
            if (!strcasecmp (node->command, name->txt))
            {
                rl_printf ("alias %s%s %s\n", node->autoexpand ? "autoexpand " : "           ",
                    s_cquote (node->command, COLQUOTE), node->replace);
                return 0;
            }
        rl_print (i18n (2297, "Alias to what?\n"));
    }
    return 0;
}

/*
 * Remove an alias.
 */

static JUMP_F(CmdUserUnalias)
{
    strc_t par;

    if (!(par = s_parse (&args)))
        rl_print (i18n (2298, "Remove which alias?\n"));
    else if (!AliasRemove (par->txt))
        rl_print (i18n (2299, "Alias doesn't exist.\n"));
    
    return 0;
}

/*
 * Resend your last message
 */
static JUMP_F (CmdUserResend)
{
    ContactGroup *cg;
    Contact *cont;
    int i;
    OPENCONN;

    if (!uiG.last_message_sent) 
    {
        rl_print (i18n (1771, "You haven't sent a message to anyone yet!\n"));
        return 0;
    }

    if ((cg = s_parselistrem (&args, uiG.conn)))
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
        {
            IMCliMsg (cont, uiG.last_message_sent_type, uiG.last_message_sent, NULL);
            uiG.last_sent = cont;
        }
    else
        IMCliMsg (uiG.last_sent, uiG.last_message_sent_type, uiG.last_message_sent, NULL);
    return 0;
}

/*
 * Send an instant message of any kind.
 */
static JUMP_F (CmdUserAnyMess)
{
    Contact *cont;
    strc_t par;
    UDWORD i, f = 0;
    static str_s t = { NULL, 0, 0 };
    ANYCONN;

    if (!(data & 3))
    {
        if (!(par = s_parse (&args)))
            return 0;
        if (!strcmp (par->txt, "peer"))
            data |= 1;
        else if (!strcmp (par->txt, "srv"))
        {
            if (!s_parseint (&args, &f))
                return 0;
            data |= 2;
        }
        else
            return 0;
    }
    if (!(data & ~3))
    {
        if (!s_parseint (&args, &i))
            return 0;
        data |= i << 2;
    }

    if (!(cont = s_parsenick (&args, uiG.conn)))
        return 0;
    
    if (!(par = s_parse (&args)))
        return 0;
    
    s_init (&t, "", 0);
    s_catf (&t, "%s", par->txt);

    while ((par = s_parse (&args)))
        s_catf (&t, "%c%s", Conv0xFE, par->txt);
        
    if (data & 1)
    {
#ifdef ENABLE_PEER2PEER
        if (!uiG.conn->oscar_dc || !TCPDirectOpen  (uiG.conn->oscar_dc, cont))
        {
            rl_printf (i18n (2142, "Direct connection with %s not possible.\n"), cont->nick);
            return 0;
        }
        PeerSendMsg (uiG.conn->oscar_dc, cont, data >> 2, t.txt);
#endif
    }
    else
    {
        if (uiG.conn->type == TYPE_SERVER)
        {
            Message *msg = MsgC ();
            UBYTE ret;
            msg->cont = cont;
            msg->send_message = strdup (t.txt);
            msg->type = data >> 2;
            msg->force = 1;
            ret = SnacCliSendmsg (cont->serv, cont, f, msg);
            if (!RET_IS_OK (ret))
                MsgD (msg);
        }
    }
    return 0;
}

/*
 * Send an instant message.
 */
static JUMP_F (CmdUserMessage)
{
    static str_s t;
    static ContactGroup *cg = NULL;
    static int tab = 0;
    ContactGroup *tcg;
    Contact *cont = NULL;
    char *arg1 = NULL;
    UDWORD i;
    OPENCONN;

    if (!status)
    {
        tab = 1;
        if (s_parsekey (&args, "notab"))
            tab = 0;

        if (!(cg = s_parseanylist (&args, uiG.conn)))
            return 0;
        tcg = ContactGroupC (NULL, 0, NULL);
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
            ContactAdd (tcg, cont);
        cg = tcg;

        arg1 = s_parserem (&args);
        if (arg1 && (arg1[strlen (arg1) - 1] != '\\'))
        {
            s_repl (&uiG.last_message_sent, arg1);
            uiG.last_message_sent_type = MSG_NORM;
            for (i = 0; (cont = ContactIndex (cg, i)); i++)
            {
                IMCliMsg (cont, MSG_NORM, arg1, NULL);
                uiG.last_sent = cont;
                if (tab)
                {
                    TabAddOut (cont);
                    OptSetVal (&cont->copts, CO_TALKEDTO, 1);
                }
            }
            if (data != 8)
            {
                ContactGroupD (cg);
                return 0;
            }
            arg1 = NULL;
        }
        if (!ContactIndex (cg, 1))
            rl_printf (i18n (2131, "Composing message to %s%s%s:\n"), COLCONTACT, ContactIndex (cg, 0)->nick, COLNONE);
        else
            rl_printf (i18n (2131, "Composing message to %s%s%s:\n"), COLQUOTE, i18n (2220, "several"), COLNONE);
        s_init (&t, "", 100);
        if (arg1)
        {
            arg1[strlen (arg1) - 1] = '\0';
            s_cat (&t, arg1);
            s_catc (&t, '\r');
            s_catc (&t, '\n');
        }
        status = 1 + (data & 8);
    }
    else if (status == -1 || !strcmp (args, CANCEL_MSG_STR))
    {
        rl_print (i18n (1038, "Message canceled.\n"));
        if ((status & 8) && *t.txt)
        {
            if (!ContactIndex (cg, 1))
                rl_printf (i18n (2570, "Continuing chat with %s%s%s:\n"), COLCONTACT, ContactIndex (cg, 0)->nick, COLNONE);
            else
                rl_printf (i18n (2570, "Continuing chat with %s%s%s:\n"), COLQUOTE, i18n (2220, "several"), COLNONE);
            s_init (&t, "", 100);
        }
        else
        {
            ReadLinePromptReset ();
            ContactGroupD (cg);
            return 0;
        }
    }
    else if (!strcmp (args, END_MSG_STR))
    {
        arg1 = t.txt + t.len - 1;
        while (*t.txt && strchr ("\r\n\t ", *arg1))
            *arg1-- = '\0';
        s_repl (&uiG.last_message_sent, t.txt);
        uiG.last_message_sent_type = MSG_NORM;
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
        {
            IMCliMsg (cont, MSG_NORM, t.txt, NULL);
            uiG.last_sent = cont;
            if (tab)
            {
                TabAddOut (cont);
                OptSetVal (&cont->copts, CO_TALKEDTO, 1);
            }
        }
        if (status & 8)
        {
            if (!ContactIndex (cg, 1))
                rl_printf (i18n (2570, "Continuing chat with %s%s%s:\n"), COLCONTACT, ContactIndex (cg, 0)->nick, COLNONE);
            else
                rl_printf (i18n (2570, "Continuing chat with %s%s%s:\n"), COLQUOTE, i18n (2220, "several"), COLNONE);
            s_init (&t, "", 100);
        }
        else
        {
            ReadLinePromptReset ();
            ContactGroupD (cg);
            return 0;
        }
    }
    else
    {
        s_cat (&t, args);
        s_catc (&t, '\r');
        s_catc (&t, '\n');
    }
    ReadLinePromptSet (i18n (2403, "msg>"));
    return status;
}

/*
 * Changes verbosity.
 */
static JUMP_F(CmdUserVerbose)
{
    UDWORD i = 0, j;

    while (args && *args)
    {
        if      (s_parsekey (&args, "sane"))
            i |= DEB_PROTOCOL | DEB_CONNECT | DEB_EVENT
                | DEB_PACK5DATA | DEB_PACK8 | DEB_PACK8DATA
                | DEB_PACKTCP | DEB_PACKTCPDATA 
                | DEB_TCP | DEB_IO | DEB_SSL;
        else if (s_parsekey (&args, "xmpp"))
            i |= DEB_XMPPIN | DEB_XMPPOUT | DEB_XMPPOTHER;
        else if (s_parseint (&args, &j))
            i |= j;
#if WIP
        else if (s_parsekey (&args, "SSL"))     i |= DEB_SSL;
        else if (s_parsekey (&args, "PRO"))     i |= DEB_PROTOCOL;
        else if (s_parsekey (&args, "PAK"))     i |= DEB_PACKET;
        else if (s_parsekey (&args, "QUE"))     i |= DEB_QUEUE;
        else if (s_parsekey (&args, "CON"))     i |= DEB_CONNECT;
        else if (s_parsekey (&args, "EVE"))     i |= DEB_EVENT;
        else if (s_parsekey (&args, "EXT"))     i |= DEB_EXTRA;
        else if (s_parsekey (&args, "CTC"))     i |= DEB_CONTACT;
        else if (s_parsekey (&args, "OPT"))     i |= DEB_OPTS;
        else if (s_parsekey (&args, "5PD"))     i |= DEB_PACK5DATA;
        else if (s_parsekey (&args, "8PK"))     i |= DEB_PACK8;
        else if (s_parsekey (&args, "8PD"))     i |= DEB_PACK8DATA;
        else if (s_parsekey (&args, "TCP"))     i |= DEB_TCP;
        else if (s_parsekey (&args, "I/O"))     i |= DEB_IO;
        else if (s_parsekey (&args, "TPK"))     i |= DEB_PACKTCP;
        else if (s_parsekey (&args, "TPD"))     i |= DEB_PACKTCPDATA;
        else if (strlen (args) == 3)
        {
            rl_printf ("### numeric, or keywords: SSL PRO PAK QUE CON EVE EXT CTC OPT 5PD 8PK 8PD 8PS TCP I/O TPK TPD TPS\n");
            return 0;
        }
#endif
        else if (*args)
        {
            rl_printf (i18n (2115, "'%s' is not an integer.\n"), args);
            return 0;
        }
        prG->verbose = i;
    }
    rl_printf (i18n (1060, "Verbosity level is %ld.\n"), UD2UL (prG->verbose));
    return 0;
}

typedef enum { ss_none, ss_off, ss_dnd, ss_occ, ss_na, ss_away, ss_on, ss_ffc, ss_birth } sortstate_t;

static sortstate_t __status (Contact *cont)
{
    if (ContactPrefVal (cont, CO_IGNORE))  return ss_none;
    if (!cont->group)                      return ss_none;
    if (cont->status == ims_offline)       return ss_off;
    if (cont->flags & imf_birth)           return ss_birth;
    switch (ContactClearInv (cont->status))
    {
        case imr_dnd:    return ss_dnd;
        case imr_occ:    return ss_occ;
        case imr_na:     return ss_na;
        case imr_away:   return ss_away;
        case imr_ffc:    return ss_ffc;
        case imr_offline:
        case imr_online: return ss_on;
    }
    return ss_on;
}

static UWORD __lenscreen, __lenstat, __lenid, __lenid2, __totallen, __l;

static void __initcheck (void)
{
    __lenscreen = __lenstat = __lenid = __lenid2 = __l = 0;
}

static void __checkcontact (Contact *cont, UWORD data)
{
    ContactAlias *alias;
    UWORD width;

    ReadLineAnalyzeWidth (cont->screen, &width);
    if (width > __lenscreen)
        __lenscreen = width;
    
    ReadLineAnalyzeWidth (cont->nick, &width);
    if (width > uiG.nick_len && ~data & 128)
        uiG.nick_len = width;
    
    if (data & 2)
        for (alias = cont->alias; alias; alias = alias->more)
            ReadLinePrintCont (alias->alias, "");

    ReadLineAnalyzeWidth (s_status (cont->status, cont->nativestatus), &width);
    if (width > __lenstat)
        __lenstat = width;
    if (cont->version)
    {
        ReadLineAnalyzeWidth (cont->version, &width);
        if (width > __lenid)
            __lenid = width;
    }
    if (cont->serv->type == TYPE_SERVER)
        __lenid2 = 29;
#if ENABLE_CONT_HIER
    for (cont = cont->firstchild; cont; cont = cont->next)
        __checkcontact (cont, data | 128);
#endif
}

static void __donecheck (UWORD data)
{
    __lenstat += 2;
    __lenid += 2;
    __totallen = 1 + uiG.nick_len + 1 + __lenstat + 1 + __lenid + __lenid2;
    if (data & 2)
        __totallen += 2 + 3 + 2 + 2 + __lenscreen + 19;
}

static void __checkgroup (ContactGroup *cg, UWORD data)
{
   Contact *cont;
   int i;

   __initcheck ();
   data = data ? 2 : 0;
   for (i = 0; cg && (cont = ContactIndex (cg, i)); i++)
       __checkcontact (cont, data);
  __donecheck (data);
}

static void __showcontact (Contact *cont, UWORD data)
{
    Connection *peer;
    ContactAlias *alias;
    char *stat, *ver = NULL, *ver2 = NULL;
    const char *ul = "", *nul = COLNONE;
    time_t tseen;
    val_t vseen;
#ifdef WIP
    time_t tclimm;
    val_t vclimm;
#endif
    char tbuf[100];
#ifdef CONFIG_UNDERLINE
    static char *non_ul = "";
    if (!non_ul || !*non_ul)
        non_ul = strdup (s_sprintf ("%s%s", COLNONE, ESC "[4m"));
#endif
    
    peer = (cont->serv && cont->serv->oscar_dc) ? ServerFindChild (cont->serv, cont, TYPE_MSGDIRECT) : NULL;
    
    stat = strdup (s_sprintf ("(%s)", s_status (cont->status, cont->nativestatus)));
    if (cont->version)
        ver  = strdup (s_sprintf ("[%s]", cont->version));
    if (prG->verbose && cont->dc)
        ver2 = strdup (s_sprintf (" <%08x:%08x:%08x>", (unsigned int)cont->dc->id1,
                                   (unsigned int)cont->dc->id2, (unsigned int)cont->dc->id3));
    if (!OptGetVal (&cont->copts, cont->status == ims_offline ? CO_TIMESEEN : CO_TIMEONLINE, &vseen))
        vseen = -1;
    tseen = vseen;
#ifdef WIP
    if (!OptGetVal (&cont->copts, CO_TIMECLIMM, &vclimm))
        vclimm = -1;
    tclimm = vclimm;
#endif

    if (tseen != (time_t)-1 && data & 2)
        strftime (tbuf, sizeof (tbuf), " %Y-%m-%d %H:%M:%S", localtime (&tseen));
    else if (data & 2)
        strcpy (tbuf, "                    ");
    else
        tbuf[0] = '\0';

#ifdef WIP
    if (tclimm != (time_t)-1 && data & 2)
        strftime (tbuf + strlen (tbuf), sizeof (tbuf) - strlen (tbuf),
                  " %Y-%m-%d %H:%M:%S", localtime (&tclimm));
#endif
    
#ifdef CONFIG_UNDERLINE
    if (!(++__l % 5))
        ul = ESC "[4m", nul = none_ul;
    else
        ul = "", nul = COLNONE;
#endif
    if (data & 2)
    {
#if ENABLE_CONT_HIER
        if (data & 128)
            rl_printf ("%s        %s", ul, ReadLinePrintWidth (cont->screen, "", nul, &__lenscreen));
        else
#endif
          rl_printf ("%s%s%c%c%c%2d%c%c%s %s", COLSERVER, ul,
             !cont->group                        ? '#' : ' ',
             ContactPrefVal (cont,  CO_INTIMATE) ? '*' :
              ContactPrefVal (cont, CO_HIDEFROM) ? '-' : ' ',
             ContactPrefVal (cont,  CO_IGNORE)   ? '^' : ' ',
             cont->dc ? cont->dc->version : 0,
             ContactPrefVal (cont, CO_WANTSBL)  ? 
               (ContactPrefVal (cont, CO_ISSBL) ? 
                 (cont->oldflags & CONT_REQAUTH ? 'T' : 'S') : '.') :
                ContactPrefVal (cont, CO_ISSBL) ?
                 (cont->oldflags & CONT_REQAUTH ? 't' : 's') : ' ',
             peer ? (
#ifdef ENABLE_SSL
              peer->connect & CONNECT_OK && peer->ssl_status == SSL_STATUS_OK ? '%' :
#endif
              peer->connect & CONNECT_OK         ? '&' :
              peer->connect & CONNECT_FAIL       ? '|' :
              peer->connect & CONNECT_MASK       ? ':' : '.' ) :
              cont->dc && cont->dc->version && cont->dc->port && ~cont->dc->port &&
              cont->dc->ip_rem && ~cont->dc->ip_rem ? '^' : ' ',
             nul,
             ReadLinePrintWidth (cont->screen, ul, nul, &__lenscreen));
    }

#if ENABLE_CONT_HIER
    if (data & 128)
      rl_printf ("%s%s%c%s%s ",
             COLNONE, ul,
             data & 2                           ? ' ' :
             !cont->group                       ? '#' :
             ContactPrefVal (cont, CO_INTIMATE) ? '*' :
             ContactPrefVal (cont, CO_HIDEFROM) ? '-' :
             ContactPrefVal (cont, CO_IGNORE)   ? '^' :
             !peer                              ? ' ' :
#ifdef ENABLE_SSL
             peer->connect & CONNECT_OK && peer->ssl_status == SSL_STATUS_OK ? '%' :
#endif
             peer->connect & CONNECT_OK         ? '&' :
             peer->connect & CONNECT_FAIL       ? '|' :
             peer->connect & CONNECT_MASK       ? ':' : '.' ,
             "",
             ReadLinePrintWidth (cont->nick, ul, nul, &uiG.nick_len));
    else
#endif
      rl_printf ("%s%s%c%s%s ",
             COLSERVER, ul,
             data & 2                           ? ' ' :
             !cont->group                       ? '#' :
             ContactPrefVal (cont, CO_INTIMATE) ? '*' :
             ContactPrefVal (cont, CO_HIDEFROM) ? '-' :
             ContactPrefVal (cont, CO_IGNORE)   ? '^' :
             !peer                              ? ' ' :
#ifdef ENABLE_SSL
             peer->connect & CONNECT_OK && peer->ssl_status == SSL_STATUS_OK ? '%' :
#endif
             peer->connect & CONNECT_OK         ? '&' :
             peer->connect & CONNECT_FAIL       ? '|' :
             peer->connect & CONNECT_MASK       ? ':' : '.' ,
             COLCONTACT,
             ReadLinePrintWidth (cont->nick, ul, nul, &uiG.nick_len));

    rl_printf ("%s%s ", COLQUOTE, ReadLinePrintWidth (stat, ul, nul, &__lenstat));
    rl_printf ("%s%s%s%s\n",
             ReadLinePrintWidth (ver ? ver : "", ul, "", &__lenid),
             ver2 ? ver2 : "", tbuf, COLNONE);

    for (alias = cont->alias; alias && (data & 2); alias = alias->more)
    {
#ifdef CONFIG_UNDERLINE
            if (!(++__l % 5))
                ul = ESC "[4m", nul = none_ul;
            else
                ul = "", nul = COLNONE;
#endif
        rl_printf ("%s%s+       %s", COLSERVER, ul, ReadLinePrintWidth (cont->screen, COLNONE, nul, &__lenscreen));
        rl_printf ("%s%s %s%s ",
                  COLSERVER, ul, COLCONTACT,
                  ReadLinePrintWidth (alias->alias, ul, nul, &uiG.nick_len));
        rl_printf ("%s%s ", COLQUOTE, ReadLinePrintWidth (stat, ul, nul, &__lenstat));
        rl_printf ("%s%s%s%s\n",
                  ReadLinePrintWidth (ver ? ver : "", "", "", &__lenid),
                  ver2 ? ver2 : "", tbuf, COLNONE);
    }
    free (stat);
    s_free (ver);
    s_free (ver2);
    if (cont->cap_string && *cont->cap_string && data  & 8)
    {
        rl_printf ("    Cap: %s%s", COLMSGINDENT, cont->cap_string);
        rl_print  ("\n");
    }
    if (cont->status_message && *cont->status_message)
    {
        rl_printf ("    %s%s%s", COLQUOTE, (data & 8) ? COLMSGINDENT : COLSINGLE, cont->status_message);
        rl_print  ("\n");
    }
#if ENABLE_CONT_HIER
    if (prG->verbose)
        for (cont = cont->firstchild; cont; cont = cont->next)
            __showcontact (cont, data | 128);
#endif
}

static void __showgroupverbose (ContactGroup *cg, UWORD data)
{
    Contact *cont;
    int i, j;
    char *t1, *t2;
    UBYTE id;

    __checkgroup (cg, data);
    
    for (i = 0; (cont = ContactIndex (cg, i)); i++)
    {
        char tbuf[100];
        val_t vseen, vonl;
        time_t tseen, tonl;
#ifdef WIP
        val_t vclimm;
        time_t tclimm;
#endif
        __showcontact (cont, data);
        if (cont->dc)
        {
            rl_printf ("    %-15s %s / %s:%ld\n    %s %d    %s (%d)    %s %08lx\n",
                  i18n (1642, "IP:"), t1 = strdup (s_ip (cont->dc->ip_rem)),
                  t2 = strdup (s_ip (cont->dc->ip_loc)), UD2UL (cont->dc->port),
                  i18n (1453, "TCP version:"), cont->dc->version,
                  cont->dc->type == 4 ? i18n (1493, "Peer-to-Peer")
                    : i18n (1494, "Server Only"), cont->dc->type,
                  i18n (2026, "TCP cookie:"), UD2UL (cont->dc->cookie));
            free (t1);
            free (t2);
        }

        if (!OptGetVal (&cont->copts, CO_TIMESEEN, &vseen))
            vseen = -1;
        tseen = vseen;
        if (!OptGetVal (&cont->copts, CO_TIMEONLINE, &vonl))
            vonl = -1;
        tonl = vonl;
#ifdef WIP
        if (!OptGetVal (&cont->copts, CO_TIMECLIMM, &vclimm))
            vclimm = -1;
        tclimm = vclimm;
#endif
        if (tseen != (time_t)-1)
        {
            strftime (tbuf, sizeof (tbuf), " %Y-%m-%d %H:%M:%S", localtime (&tseen));
            rl_printf ("    %-15s %s", i18n (2691, "Last seen:"), tbuf);
        }
        if (tonl != (time_t)-1)
        {
            strftime (tbuf, sizeof (tbuf), " %Y-%m-%d %H:%M:%S", localtime (&tonl));
            rl_printf ("    %-15s %s", i18n (2692, "Online since:"), tbuf);
        }
        if (tseen != (time_t)-1 || tonl != (time_t)-1)
            rl_print ("\n");
#ifdef WIP
        if (tclimm != (time_t)-1)
        {
            strftime (tbuf, sizeof (tbuf), " %Y-%m-%d %H:%M:%S", localtime (&tclimm));
            rl_printf ("    %-15s %s\n", i18n (2693, "Using climm:"), tbuf);
        }
#endif

        if (cont->ids)
        {
            ContactIDs *ids;
            rl_printf ("    %s %u", i18n (2577, "SBL ids:"), cont->group ? cont->group->id : 0);
            for (ids = cont->ids; ids; ids = ids->next)
                rl_printf (", %u: %u/%u/%u", ids->type, ids->id, ids->tag, ids->issbl);
            rl_printf ("\n");
            
        }
        if (cont->group && cont->group != uiG.conn->contacts && cont->group->name)
            rl_printf ("    %s %s \n", i18n (2536, "Group:"), cont->group->name);
        for (j = id = 0; id < CAP_MAX; id++)
            if (HAS_CAP (cont->caps, id))
            {
                Cap *cap = PacketCap (id);
                if (j++)
                    rl_print (", ");
                else
                    rl_printf ("    %s ", i18n (2192, "Capabilities:"));
                rl_print (cap->name);
                if (cap->name[4] == 'U' && cap->name[5] == 'N')
                {
                    rl_print (": ");
                    rl_print (s_dump ((const UBYTE *)cap->cap, 16));
                }
            }
        if (j)
            rl_print ("\n");
    }
}

int __sort_group (Contact *a, Contact *b, int mode)
{
    if (!a)
        return -1;
    if (!b)
        return 1;
    if (__status (a) > __status (b))
        return -1;
    if (__status (a) < __status (b))
        return 1;
    if (!a->nick)
        return -1;
    if (!b->nick)
        return 1;
    return strcasecmp (b->nick, a->nick);
}

void __center (const char *text, int is_shadow)
{
    Contact *cont = NULL;
    int i, j;
    rl_print (COLQUOTE);
    if (text)
    {
        i = 0;
        if (__totallen > c_strlen (text) + 1)
        {
            for (i = j = (__totallen - c_strlen (text) - 1) / 2; i >= 20; i -= 20)
                rl_print ("====================");
            rl_printf ("%.*s", (int)i, "====================");
            rl_printf (" %s%s%s ", is_shadow ? COLQUOTE : COLCONTACT, text, COLQUOTE);
            for (i = __totallen - j - c_strlen (text) - 2; i >= 20; i -= 20)
                rl_print ("====================");
        }
        else
            rl_printf (" %s%s%s ", is_shadow ? COLQUOTE : COLCONTACT, text, COLQUOTE);
    }
    else
        for (i = __totallen; i >= 20; i -= 20)
            rl_print ("====================");
    rl_printf ("%.*s%s\n", i, "====================", COLNONE);
}

static void __showgroupshort (ContactGroup *cg, ContactGroup *dcg, ContactGroup *lcg, UWORD data)
{
    ContactGroup *tcg;
    Contact *cont;
    int is_shadow, j;
    val_t val;
    
#ifdef CONFIG_UNDERLINE
    __l = 0;
#endif
    tcg = ContactGroupC (NULL, 0, NULL);
    assert (tcg);
    for (j = 0; (cont = ContactIndex (cg, j)); j++)
        if ((!lcg || ContactHas (lcg, cont)) && !ContactHas (tcg, cont) && (cg == cont->serv->contacts || cont->group == lcg))
            ContactAdd (tcg, cont);
    ContactGroupSub (tcg, dcg);
    if (data & 32 && !ContactIndex (tcg, 0) && (!lcg || cg != lcg->serv->contacts))
    {
        ContactGroupD (tcg);
        return;
    }
    is_shadow = ~data & 64 && data & 32 && ContactGroupPrefVal (lcg, CO_SHADOW);
    __center (lcg && lcg != lcg->serv->contacts ? lcg->name : NULL, is_shadow);
    if (!is_shadow)
    {
        ContactGroupSort (tcg, __sort_group, 0);
        for (j = 0; (cont = ContactIndex (tcg, j)); j++)
        {
            if (data & 1 && __status (cont) < ss_dnd)
                continue;
            if (~data & 64 && OptGetVal (&cont->copts, CO_SHADOW, &val) && val)
                continue;
            __showcontact (cont, data);
        }
    }
    ContactGroupD (tcg);
}

/*
 * Shows the contact list in a very detailed way.
 *
 * data & 256: also include all server connections
 * data & 64: also show shadowed contacts
 * data & 32: sort by groups
 * data & 16: show _only_ own status
 * data & 8: show only given nicks overly verbose
 * data & 4: show own status if no further group argument is given
 * data & 2: be verbose
 * data & 1: do not show offline
 */
static JUMP_F(CmdUserStatusDetail)
{
    Server *serv;
    ContactGroup *cg = NULL, *wcg = NULL, *dcg = NULL;
    Contact *cont = NULL;
    const char *targs;
    int i, k, f;
    ANYCONN;

    if (data == (UDWORD)-1)
        s_parseint (&args, &data);
    
    do
    {
        f = 0;
        if (data & 512)
            f = 0;
        else if (s_parsekey (&args, "multi"))
            f = 1, data |= 256;
        else if (s_parsekey (&args, "shadow"))
            f = 1, data |= 64;
        else if (s_parsekey (&args, "group"))
            f = 1, data |= 32;
        else if (s_parsekey (&args, "mine"))
            f = 1, data |= 4;
        else if (s_parsekey (&args, "long"))
            f = 1, data |= 2;
        else if (s_parsekey (&args, "online"))
            f = 1, data |= 1;
    } while (f);
    
    if (data & 8)
    {
        cg = s_parselist_s (&args, 1, 1, uiG.conn);
        if (cg)
        {
            __showgroupverbose (cg, data & 10);
            return 0;
        }
    }

    if (data & 32)
    {
        targs = args;
        wcg = s_parselist_s (&targs, 1, 1, uiG.conn);
        if (wcg)
            __checkgroup (wcg, data & 2);
        else
            data |= 1024;
        while ((cg = s_parsecg_s (&args, DEFAULT_SEP, 1, uiG.conn)))
        {
            if (!dcg)
            {
                f = 1;
                dcg = ContactGroupC (NULL, 0, NULL);
            }
            __showgroupshort (cg, dcg, cg, data | 64);
            ContactGroupAdd (dcg, cg);
        }
    }
    
    for (i = 0; (serv = ServerNr (i)); i++)
    {
        if (~data & 256)
            serv = uiG.conn;

        if (data & (16 | 4) && !f)
        {
            if (serv)
            {
                UWORD nick_len = uiG.nick_len;
                rl_log_for (serv->screen, COLCONTACT);
                uiG.nick_len = nick_len;
                if (~serv->conn->connect & CONNECT_OK)
                    rl_printf (i18n (2405, "Your status is %s (%s).\n"),
                        i18n (1969, "offline"), s_status (serv->status, serv->nativestatus));
                else
                    rl_printf (i18n (2211, "Your status is %s.\n"), s_status (serv->status, serv->nativestatus));
            }
            if (data & 16)
                return 0;
        }
        
        wcg = s_parselist_s (&args, 1, 1, serv);
        if (!wcg && !f)
            wcg = serv->contacts;
        
        if (wcg)
        {
            if (~data & 32 || data & 1024)
                __checkgroup (wcg, data & 2);

            if (data & 32 && ContactIndex (wcg, 0))
                for (k = 0; (cg = ContactGroupIndex (k)); k++)
                {
                    if (cg == serv->contacts || cg->serv != serv)
                        continue;
                    __showgroupshort (wcg, dcg, cg, data);
                    if (!dcg)
                        dcg = ContactGroupC (NULL, 0, NULL);
                    ContactGroupAdd (dcg, cg);
                }
            __showgroupshort (wcg, dcg, NULL, data);
        }
        __center (NULL, 0);
        if (~data & 256)
            return 0;
    }
    return 0;
}

/*
 * Show meta information remembered from a contact.
 */
static JUMP_F(CmdUserStatusMeta)
{
    ContactGroup *cg = NULL;
    Contact *cont;
    int i;
    ANYCONN;

    if (!data)
    {
        if      (s_parsekey (&args, "show")) data = 1;
        else if (s_parsekey (&args, "load")) data = 2;
        else if (s_parsekey (&args, "save")) data = 3;
        else if (s_parsekey (&args, "set"))  data = 4;
        else if (s_parsekey (&args, "get"))  data = 5;
        else if (s_parsekey (&args, "rget")) data = 6;
        else
        {
            rl_printf (i18n (2333, "%s [show|load|save|set|get|rget] <contacts> - handle meta data for contacts.\n"), "meta");
            rl_print  (i18n (2334, "  show - show current known meta data\n"));
            rl_print  (i18n (2335, "  load - load from file and show meta data\n"));
            rl_print  (i18n (2336, "  save - save meta data to file\n"));
            rl_print  (i18n (2337, "  set  - upload own meta data to server\n"));
            rl_print  (i18n (2338, "  get  - query server for meta data\n"));
            rl_print  (i18n (2339, "  rget - query server for last sender's meta data\n"));
            return 0;
        }
    }
    
    if ((data == 4) || (cg = s_parselistrem (&args, uiG.conn)) || (data == 6))
    {
        switch (data)
        {
            case 1:
                for (i = 0; (cont = ContactIndex (cg, i)); i++)
                    UtilUIDisplayMeta (cont);
                break;
            case 2:
                for (i = 0; (cont = ContactIndex (cg, i)); i++)
                {
                    if (ContactMetaLoad (cont))
                        UtilUIDisplayMeta (cont);
                    else
                        rl_printf (i18n (2587, "Couldn't load meta data for %s (%s).\n"),
                                  s_wordquote (cont->nick), cont->screen);
                }
                break;
            case 3:
                for (i = 0; (cont = ContactIndex (cg, i)); i++)
                {
                    if (ContactMetaSave (cont))
                        rl_printf (i18n (2588, "Saved meta data for '%s' (%s).\n"),
                                  cont->nick, cont->screen);
                    else
                        rl_printf (i18n (2589, "Couldn't save meta data for '%s' (%s).\n"),
                                  cont->nick, cont->screen);
                }
                break;
            case 4:
                if (!(cont = uiG.conn->conn->cont))
                    return 0;
                if (uiG.conn->type == TYPE_SERVER)
                {
                    SnacCliMetasetgeneral (uiG.conn, cont);
                    SnacCliMetasetmore (uiG.conn, cont);
                    SnacCliMetasetabout (uiG.conn, cont->meta_about);
                }
                return 0;
            case 6:
                IMCliInfo (uiG.last_rcvd->serv, uiG.last_rcvd, 0);
                if (!cg)
                    break;
            case 5:
                for (i = 0; (cont = ContactIndex (cg, i)); i++)
                    IMCliInfo (cont->serv, cont, 0);
                data = 5;
                break;
        }
    }
    return 0;
}

/*
 * Shows the ignored user on the contact list.
 */
static JUMP_F(CmdUserIgnoreStatus)
{
    ContactGroup *cg;
    Contact *cont;
    int i;
    ANYCONN;

    rl_printf ("%s%s%s%s%s\n", W_SEPARATOR, i18n (1062, "Users ignored:"));
    cg = uiG.conn->contacts;
    for (i = 0; (cont = ContactIndex (cg, i)); i++)
    {
        if (ContactPrefVal (cont, CO_IGNORE))
        {
            if (ContactPrefVal (cont, CO_INTIMATE))
                rl_printf ("%s*%s", COLSERVER, COLNONE);
            else if (ContactPrefVal (cont, CO_HIDEFROM))
                rl_printf ("%s~%s", COLSERVER, COLNONE);
            else
                rl_print (" ");

            rl_printf ("%s%-20s\t%s(%s)%s\n", COLCONTACT, cont->nick,
                      COLQUOTE, s_status (cont->status, cont->nativestatus), COLNONE);
        }
    }
    rl_printf ("%s%s%s%s", W_SEPARATOR);
    return 0;
}


/*
 * Displays the contact list in a wide format, similar to the ls command.
 */
static JUMP_F(CmdUserStatusWide)
{
    ContactGroup *cg, *cgon, *cgoff = NULL;
    int columns, colleft, colright, i;
    UWORD lennick = 0, width, width2;
    Contact *cont;
    const char *on = i18n (1654, "Online");
    OPENCONN;

    cg = uiG.conn->contacts;

    if (data && !(cgoff = ContactGroupC (uiG.conn, 0, "")))
    {
        rl_print (i18n (2118, "Out of memory.\n"));
        return 0;
    }
    
    if (!(cgon = ContactGroupC (uiG.conn, 0, "")))
    {
        if (data)
            ContactGroupD (cgon);
        rl_print (i18n (2118, "Out of memory.\n"));
        return 0;
    }

    for (i = 0; (cont = ContactIndex (cg, i)); i++)
    {
        if (cont->status == ims_offline)
        {
            if (data)
            {
                ContactAdd (cgoff, cont);
                ReadLineAnalyzeWidth (cont->nick, &width);
                if (width > lennick)
                    lennick = width;
            }
        }
        else
        {
            ContactAdd (cgon, cont);
            ReadLineAnalyzeWidth (cont->nick, &width);
            if (width > lennick)
                lennick = width;
        }
    }

    columns = rl_columns / (lennick + 3);
    if (columns < 1)
        columns = 1;
    lennick = rl_columns / columns;
    lennick = lennick > 3 ? lennick - 3 : 1;

    if (data)
    {
        const char *off = i18n (1653, "Offline");
        ReadLineAnalyzeWidth (off, &width);
        colleft = (rl_columns - width) / 2 - 1;
        rl_print (COLQUOTE);
        for (i = 0; i < colleft; i++)
            rl_print ("=");
        rl_printf (" %s%s%s ", COLCLIENT, off, COLQUOTE);
        colright = rl_columns - i - width - 2;
        for (i = 0; i < colright; i++)
            rl_print ("=");
        rl_print (COLNONE);
        for (i = 0; (cont = ContactIndex (cgoff, i)); i++)
        {
            if (!(i % columns))
                rl_print ("\n");
            rl_printf ("  %s ", ReadLinePrintWidth (cont->nick, COLCONTACT, COLNONE, &lennick));
        }
        rl_print ("\n");
        ContactGroupD (cgoff);
    }

    cont = uiG.conn->conn->cont;
    rl_printf ("%s%s%s %s", COLCONTACT, cont->screen, COLNONE, COLQUOTE);
    ReadLineAnalyzeWidth (on, &width);
    colleft = (rl_columns - width) / 2 - 2;
    ReadLineAnalyzeWidth (cont->screen, &width2);
    colleft -= width2;
    for (i = 0; i < colleft; i++)
        rl_print ("=");
    rl_printf (" %s%s%s ", COLCLIENT, on, COLQUOTE);
    i += 3 + width + width2;
    
    on = s_status (uiG.conn->status, uiG.conn->nativestatus);
    ReadLineAnalyzeWidth (on, &width);
    colright = rl_columns - i - width - 3;
    for (i = 0; i < colright; i++)
        rl_print ("=");
    rl_printf (" %s(%s)%s", COLQUOTE, on, COLNONE);
    for (i = 0; (cont = ContactIndex (cgon, i)); i++)
    {
        const char *flags = i18n (2611, " adnof__iiiiiiii");

        if (!(i % columns))
            rl_print ("\n");

        rl_printf ("%c %s ", flags[cont->status], ReadLinePrintWidth (cont->nick, COLCONTACT, COLNONE, &lennick));
    }
    rl_printf ("\n%s", COLQUOTE);
    colleft = rl_columns;
    for (i = 0; i < colleft; i++)
        rl_print ("=");
    rl_printf ("%s\n", COLNONE);
    ContactGroupD (cgon);

    return 0;
}

/*
 * Obsolete.
 */
static JUMP_F(CmdUserSound)
{
    rl_printf (i18n (2407, "This flag is handled by the %s command.\n"), s_wordquote ("set"));
    return 0;
}

/*
 * Toggles soundonline or changes soundonline command.
 */
static JUMP_F(CmdUserSoundOnline)
{
    rl_printf (i18n (2255, "A beep for oncoming contacts is always generated by running the %sevent%s script.\n"), COLSERVER, COLNONE);
    return 0;
}

/*
 * Toggles soundoffine or changes soundoffline command.
 */
static JUMP_F(CmdUserSoundOffline)
{
    rl_printf (i18n (2256, "A beep for offgoing contacts is always generated by running the %sevent%s script.\n"), COLSERVER, COLNONE);
    return 0;
}

/*
 * Toggles autoaway or sets autoaway time.
 */
static JUMP_F(CmdUserAutoaway) 
{
    strc_t par;
    UDWORD i = 0;

    if (s_parseint (&args, &i))
    {
        if (prG->away_time)
            uiG.away_time_prev = prG->away_time;
        prG->away_time = i;
    }
    else if ((par = s_parse (&args)))
    {
        if      (!strcmp (par->txt, i18n (1085, "on"))  || !strcmp (par->txt, "on"))
        {
            prG->away_time = uiG.away_time_prev ? uiG.away_time_prev : default_away_time;
        }
        else if (!strcmp (par->txt, i18n (1086, "off")) || !strcmp (par->txt, "off"))
        {
            if (prG->away_time)
                uiG.away_time_prev = prG->away_time;
            prG->away_time = 0;
        }
    }
    rl_printf (i18n (1766, "Changing status to away/not available after idling %s%ld%s seconds.\n"),
              COLQUOTE, UD2UL (prG->away_time), COLNONE);
    return 0;
}

/*
 * Toggles simple options.
 */
static JUMP_F(CmdUserSet)
{
    int quiet = 0;
    strc_t par = NULL;
    const char *str = "";
    
    while (!data)
    {
        if (!(par = s_parse (&args)))                    break;
        else if (!strcasecmp (par->txt, "color"))      { data = FLAG_COLOR;      str = i18n (2133, "Color is %s%s%s.\n"); }
        else if (!strcasecmp (par->txt, "colour"))     { data = FLAG_COLOR;      str = i18n (2133, "Color is %s%s%s.\n"); }
        else if (!strcasecmp (par->txt, "delbs"))      { data = FLAG_DELBS;      str = i18n (2262, "Interpreting a delete character as backspace is %s%s%s.\n"); }
        else if (!strcasecmp (par->txt, "funny"))      { data = FLAG_FUNNY;      str = i18n (2134, "Funny messages are %s%s%s.\n"); }
        else if (!strcasecmp (par->txt, "auto"))       { data = FLAG_AUTOREPLY;  str = i18n (2265, "Automatic replies are %s%s%s.\n"); }
        else if (!strcasecmp (par->txt, "prompt"))     data = -3; 
        else if (!strcasecmp (par->txt, "autosave"))   { data = FLAG_AUTOSAVE;   str = i18n (2267, "Automatic saves are %s%s%s.\n"); }
        else if (!strcasecmp (par->txt, "autofinger")) { data = FLAG_AUTOFINGER; str = i18n (2268, "Automatic fingering of new UINs is %s%s%s.\n"); }
        else if (!strcasecmp (par->txt, "linebreak"))  data = -1;
        else if (!strcasecmp (par->txt, "sound"))      data = -2;
        else if (!strcasecmp (par->txt, "quiet"))
        {
            quiet = 1;
            continue;
        }
        break;
    }
    
    if (data && !(par = s_parse (&args)))
        par = NULL;
    
    switch (data)
    {
        case 0:
            break;
        default:
            if (par)
            {
                if (!strcasecmp (par->txt, "on") || !strcmp (par->txt, i18n (1085, "on")))
                    prG->flags |= data;
                else if (!strcasecmp (par->txt, "off") || !strcmp (par->txt, i18n (1086, "off")))
                    prG->flags &= ~data;
                else
                    data = 0;
            }
            if (!quiet && str)
                rl_printf (str, COLQUOTE, prG->flags & data ? i18n (1085, "on") : i18n (1086, "off"), COLNONE);
            break;
        case -1:
            if (par)
            {
                UDWORD flags = prG->flags & ~FLAG_LIBR_BR & ~FLAG_LIBR_INT;
                if (!strcasecmp (par->txt, "break") || !strcasecmp (par->txt, i18n (2269, "break")))
                    prG->flags = flags | FLAG_LIBR_BR;
                else if (!strcasecmp (par->txt, "simple") || !strcasecmp (par->txt, i18n (2270, "simple")))
                    prG->flags = flags;
                else if (!strcasecmp (par->txt, "indent") || !strcasecmp (par->txt, i18n (2271, "indent")))
                    prG->flags = flags | FLAG_LIBR_INT;
                else if (!strcasecmp (par->txt, "smart") || !strcasecmp (par->txt, i18n (2272, "smart")))
                    prG->flags = flags | FLAG_LIBR_BR | FLAG_LIBR_INT;
                else
                    data = 0;
            }
            if (!quiet)
                rl_printf (i18n (2288, "Indentation style is %s%s%s.\n"), COLQUOTE,
                          ~prG->flags & FLAG_LIBR_BR ? (~prG->flags & FLAG_LIBR_INT ? i18n (2270, "simple") : i18n (2271, "indent")) :
                          ~prG->flags & FLAG_LIBR_INT ? i18n (2269, "break") : i18n (2272, "smart"), COLNONE);
            break;
        case -2:
            if (par)
            {
                if (!strcasecmp (par->txt, "on") || !strcasecmp (par->txt, i18n (1085, "on")) || !strcasecmp (par->txt, "beep"))
                   prG->sound = SFLAG_BEEP;
                else if (strcasecmp (par->txt, "off") && strcasecmp (par->txt, i18n (1086, "off")))
                   prG->sound = SFLAG_EVENT;
                else
                   prG->sound = 0;
            }
            if (!quiet)
            {
                if (prG->sound == SFLAG_BEEP)
                    rl_printf (i18n (2252, "A beep is generated by %sbeep%sing.\n"), COLSERVER, COLNONE);
                else if (prG->sound == SFLAG_EVENT)
                    rl_printf (i18n (2253, "A beep is generated by running the %sevent%s script.\n"), COLSERVER, COLNONE);
                else
                    rl_print (i18n (2254, "A beep is never generated.\n"));
            }
            break;  
        case -3:
            if (par)
            {
                UDWORD flags = prG->flags & ~FLAG_USERPROMPT & ~FLAG_UINPROMPT;
                if (!strcasecmp (par->txt, "user") || !strcasecmp (par->txt, i18n (2630, "user")))
                    prG->flags = flags | FLAG_USERPROMPT;
                else if (!strcasecmp (par->txt, "uin") || !strcasecmp (par->txt, i18n (2631, "uin")))
                    prG->flags = flags | FLAG_UINPROMPT;
                else if (!strcasecmp (par->txt, "simple") || !strcasecmp (par->txt, i18n (2632, "simple")))
                    prG->flags = flags;
                else
                    data = 0;
            }
            if (!quiet)
            {
                rl_printf (i18n (2266, "Type of the prompt is %s%s%s.\n"), COLQUOTE,
                            prG->flags & FLAG_USERPROMPT ? i18n (2630, "user") :
                            prG->flags & FLAG_UINPROMPT ? i18n (2631, "uin") : i18n (2632, "simple"),
                            COLNONE);
            }
            break;  
    }
    if (!data)
    {
        rl_printf (i18n (1820, "%s <option> [on|off|<value>] - control simple options.\n"), "set");
        rl_print (i18n (1822, "    color:      use colored text output.\n"));
        rl_print (i18n (2278, "    delbs:      interpret delete characters as backspace.\n"));
        rl_print (i18n (1815, "    funny:      use funny messages for output.\n"));
        rl_print (i18n (2281, "    auto:       send auto-replies.\n"));
        rl_print (i18n (2282, "    prompt:     type of the prompt: user, uin, simple\n"));
        rl_print (i18n (2283, "    autosave:   automatically save the climmrc.\n"));
        rl_print (i18n (2284, "    autofinger: automatically finger new UINs.\n"));
        rl_print (i18n (2285, "    linebreak:  style for line-breaking messages: simple, break, indent, smart.\n"));
        rl_print (i18n (2540, "    sound:      how to beep: off, on = beep, event.\n"));
    }
    return 0;
}

/*
 *
 */
static JUMP_F(CmdUserOpt)
{
    const char *colquote = COLQUOTE, *colnone = COLNONE;
    ContactGroup *cg = NULL;
    Contact *cont = NULL;
    const char *optname = NULL, *res = NULL, *optobj = NULL;
    char *coptname, *coptobj, *col;
    Opt *copts = NULL;
    Server *connl = NULL;
    int i;
    UDWORD flag = 0;
    val_t val;
    strc_t par;
    ANYCONN;

    if (!data && s_parsekey (&args, "global"))
    {
        copts = &prG->copts;
        data = COF_GLOBAL;
        optobj = "";
        coptobj = strdup (optobj);
    }
    else if (!data && s_parsekey (&args, "connection"))
    {
        copts = &uiG.conn->copts;
        data = COF_SERVER;
        optobj = uiG.conn->screen;
        coptobj = strdup (optobj);
        connl = uiG.conn;
    }
    else if ((!data || data == COF_GROUP) && (cg = s_parsecg (&args, uiG.conn)))
    {
        copts = &cg->copts;
        data = COF_GROUP;
        optobj = cg->name;
        coptobj = strdup (s_qquote (optobj));
    }
    else if ((!data || data == COF_CONTACT) && (cont = s_parsenick (&args, uiG.conn)))
    {
        copts = &cont->copts;
        data = COF_CONTACT;
        optobj = cont->nick;
        coptobj = strdup (s_qquote (optobj));
    }
    else if (data == COF_SERVER)
    {
        copts = &uiG.conn->copts;
        data = COF_SERVER;
        optobj = uiG.conn->screen;
        coptobj = strdup (optobj);
        connl = uiG.conn;
    }
    else if (data == COF_GLOBAL)
    {
        copts = &prG->copts;
        optobj = "";
        coptobj = strdup ("");
    }
    else
    {
        if ((par = s_parse (&args)))
        {
            switch (data)
            {
                case COF_SERVER:
                    rl_printf (i18n (2408, "%s is not a contact group.\n"), s_qquote (par->txt));
                    break;
                case COF_CONTACT:
                    rl_printf (i18n (2409, "%s is not a contact.\n"), s_qquote (par->txt));
                    break;
                default:
                    rl_printf (i18n (2410, "%s is neither a contact, nor a contact group, nor the %sglobal%s keyword.\n"),
                              s_qquote (par->txt), colquote, colnone);
            }
        }
        else
        {
            if (!data)
                rl_printf (i18n (2411, "opt - short hand for optglobal, optconnection, optgroup or optcontact.\n"));
            if (!data || data == COF_GLOBAL)
                rl_printf (i18n (2412, "optglobal            [<option> [<value>]] - set global option.\n"));
            if (!data || data == (COF_GLOBAL | COF_GROUP))
                rl_printf (i18n (2413, "optconnection        {<option> [<value>]] - set connection option.\n"));
            if (!data || data == COF_GROUP)
                rl_printf (i18n (2414, "optgroup   <group>   [<option> [<value>]] - set contact group option.\n"));
            if (!data || data == COF_CONTACT)
                rl_printf (i18n (2415, "optcontact <contact> [<option> [<value>]] - set contact option.\n"));
        }
        return 0;
    }
    
    if (!*args)
    {
        rl_printf (data & COF_CONTACT ? i18n (2416, "Options for contact %s:\n") :
                   data & COF_GROUP   ? i18n (2417, "Options for contact group %s:\n") :
                   data & COF_SERVER  ? i18n (2713, "Options for server %s:\n")
                                      : i18n (2418, "Global options:\n"),
                   coptobj);
        
        for (i = 0; (optname = OptList[i].name); i++)
        {
            flag = OptList[i].flag;
            if (~flag & data)
                continue;
            
            switch (flag & (COF_BOOL | COF_NUMERIC | COF_STRING | COF_COLOR))
            {
                case COF_BOOL:
                    if (data == COF_CONTACT)
                        rl_printf ("    %-15s  %s%-15s%s  (%s %s%s%s)\n", optname, colquote,
                                  OptGetVal (copts, flag, &val)
                                    ? val ? i18n (1085, "on") : i18n (1086, "off")
                                    : i18n (2419, "undefined"), colnone, i18n (2420, "effectivly"), colquote,
                                  ContactPrefVal (cont, flag)
                                    ? i18n (1085, "on") : i18n (1086, "off"), colnone);
                    else
                        rl_printf ("    %-15s  %s%s%s\n", optname, colquote,
                                  OptGetVal (copts, flag, &val)
                                    ? val  ? i18n (1085, "on") : i18n (1086, "off")
                                    : i18n (2419, "undefined"), colnone);
                    break;
                case COF_NUMERIC:
                    if (data == COF_CONTACT)
                        rl_printf ("    %-15s  %s%-15s%s  (%s %s%ld%s)\n", optname, colquote,
                                  OptGetVal (copts, flag, &val)
                                    ? s_sprintf ("%ld", UD2UL (val))
                                    : i18n (2419, "undefined"), colnone, i18n (2420, "effectivly"), colquote,
                                  UD2UL (ContactPrefVal (cont, flag)), colnone);
                    else
                        rl_printf ("    %-15s  %s%s%s\n", optname, colquote,
                                  OptGetVal (copts, flag, &val)
                                    ? s_sprintf ("%ld", UD2UL (val))
                                    : i18n (2419, "undefined"), colnone);
                    break;
                case COF_STRING:
                    if (data == COF_CONTACT)
                        rl_printf ("    %-15s  %s%-15s%s  (%s %s%s%s)\n", optname, colquote,
                                  OptGetStr (copts, flag, &res)
                                    ? res : i18n (2419, "undefined"), colnone, i18n (2420, "effectivly"), colquote,
                                  ContactPrefStr (cont, flag), colnone);
                    else
                        rl_printf ("    %-15s  %s%s%s\n", optname, colquote,
                                  OptGetStr (copts, flag, &res)
                                    ? res : i18n (2419, "undefined"), colnone);
                    break;
                case COF_COLOR:
                    if (data == COF_CONTACT)
                        rl_printf ("    %-15s  %s%-15s%s  (%s %s%s%s)\n", optname, colquote,
                                  OptGetStr (copts, flag, &res)
                                    ? OptS2C (res) : i18n (2419, "undefined"), colnone,
                                  i18n (2420, "effectivly"), colquote,
                                  OptS2C (ContactPrefStr (cont, flag)), colnone);
                    else
                        rl_printf ("    %-15s  %s%s%s\n", optname, colquote,
                                  OptGetStr (copts, flag, &res)
                                    ? OptS2C (res) : i18n (2419, "undefined"), colnone);
                    break;
            }
        }
        free (coptobj);
        return 0;
    }
    
    while (*args)
    {
        if (!(par = s_parse (&args)))
            break;
        
        optname = NULL;
        for (i = 0; OptList[i].name; i++)
            if (OptList[i].flag & data && !strcmp (par->txt, OptList[i].name))
            {
                optname = OptList[i].name;
                flag = OptList[i].flag;
                break;
            }
        
        if (!optname)
        {
            rl_printf (i18n (2421, "Unknown option %s.\n"), s_qquote (par->txt));
            continue;
        }
        coptname = strdup (s_qquote (optname));
        
        if (s_parsekey (&args, "undef") || s_parsekey (&args, "delete")
            || s_parsekey (&args, "remove") || s_parsekey (&args, "erase"))
        {
            if (flag & COF_STRING)
                OptSetStr (copts, flag, NULL);
            else
                OptUndef (copts, flag);
            rl_printf (data & COF_CONTACT ? i18n (2431, "Undefining option %s for contact %s.\n") :
                       data & COF_GROUP   ? i18n (2432, "Undefining option %s for contact group %s.\n") :
                       data & COF_SERVER  ? i18n (2714, "Undefining option %s for server %s.\n")
                                          : i18n (2433, "Undefining global option %s%s.\n"),
                      coptname, coptobj);
        }
        else if (!(par = s_parse (&args)))
        {
            const char *res = NULL;
            val_t val;

            if (!OptGetVal (copts, flag, &val) || ((flag & (COF_STRING | COF_COLOR)) && !OptGetStr (copts, flag, &res)))
                rl_printf (data & COF_CONTACT ? i18n (2422, "Option %s for contact %s is undefined.\n") :
                           data & COF_GROUP   ? i18n (2423, "Option %s for contact group %s is undefined.\n") :
                           data & COF_SERVER  ? i18n (2715, "Option %s for server %s is undefined.\n")
                                              : i18n (2424, "Option %s%s has no global value.\n"),
                          coptname, coptobj);
            else if (flag & (COF_BOOL | COF_NUMERIC))
                rl_printf (data & COF_CONTACT ? i18n (2425, "Option %s for contact %s is %s%s%s.\n") :
                           data & COF_GROUP   ? i18n (2426, "Option %s for contact group %s is %s%s%s.\n") :
                           data & COF_SERVER  ? i18n (2716, "Option %s for server %s is %s%s%s.\n")
                                              : i18n (2427, "Option %s%s is globally %s%s%s.\n"),
                          coptname, coptobj, colquote, flag & COF_NUMERIC ? s_sprintf ("%ld", UD2UL (val))
                          : val ? i18n (1085, "on") : i18n (1086, "off"), colnone);
            else
                rl_printf (data & COF_CONTACT ? i18n (2428, "Option %s for contact %s is %s.\n") :
                           data & COF_GROUP   ? i18n (2429, "Option %s for contact group %s is %s.\n") :
                           data & COF_SERVER  ? i18n (2717, "Option %s for server %s is %s.\n")
                                              : i18n (2430, "Option %s%s is globally %s.\n"),
                          coptname, coptobj, s_qquote (flag & COF_STRING  ? res : OptS2C (res)));
            free (coptname);
            break;
        }
        else if (flag & COF_STRING)
        {
            res = par->txt;
            if (flag == CO_ENCODINGSTR)
            {
                UWORD enc = ConvEnc (par->txt) & ~ENC_FLAGS;
                OptSetVal (copts, CO_ENCODING, enc);
                res = ConvEncName (enc);
            }
            OptSetStr (copts, flag, res);
            rl_printf (data & COF_CONTACT ? i18n (2434, "Setting option %s for contact %s to %s.\n") :
                       data & COF_GROUP   ? i18n (2435, "Setting option %s for contact group %s to %s.\n") :
                       data & COF_SERVER  ? i18n (2718, "Setting option %s for server %s to %s.\n")
                                          : i18n (2436, "Setting option %s%s globally to %s.\n"),
                      coptname, coptobj, s_qquote (res));
        }
        else if (flag & COF_COLOR)
        {
            res = OptC2S (col = strdup (par->txt));
            OptSetStr (copts, flag, res);
            rl_printf (data & COF_CONTACT ? i18n (2434, "Setting option %s for contact %s to %s.\n") :
                       data & COF_GROUP   ? i18n (2435, "Setting option %s for contact group %s to %s.\n") :
                       data & COF_SERVER  ? i18n (2718, "Setting option %s for server %s to %s.\n")
                                          : i18n (2436, "Setting option %s%s globally to %s.\n"),
                      coptname, coptobj, s_qquote (col));
            free (col);
        }
        else if (flag & COF_NUMERIC)
        {
            if (flag == CO_CSCHEME)
                OptImport (copts, PrefSetColorScheme (val));
            OptSetVal (copts, flag, atoi (par->txt));
            rl_printf (data & COF_CONTACT ? i18n (2437, "Setting option %s for contact %s to %s%d%s.\n") :
                       data & COF_GROUP   ? i18n (2438, "Setting option %s for contact group %s to %s%d%s.\n") :
                       data & COF_SERVER  ? i18n (2719, "Setting option %s for server %s to %s%d%s.\n")
                                          : i18n (2439, "Setting option %s%s globally to %s%d%s.\n"),
                      coptname, coptobj, colquote, atoi (par->txt), colnone);
        }
        else if (!strcasecmp (par->txt, "on")  || !strcasecmp (par->txt, i18n (1085, "on")))
        {
            OptSetVal (copts, flag, 1);
            rl_printf (data & COF_CONTACT ? i18n (2440, "Setting option %s for contact %s.\n") :
                       data & COF_GROUP   ? i18n (2441, "Setting option %s for contact group %s.\n") :
                       data & COF_SERVER  ? i18n (2720, "Setting option %s for server %s.\n")
                                          : i18n (2442, "Setting option %s%s globally.\n"),
                      coptname, coptobj);
        }
        else if (!strcasecmp (par->txt, "off") || !strcasecmp (par->txt, i18n (1086, "off")))
        {
            OptSetVal (copts, flag, 0);
            rl_printf (data & COF_CONTACT ? i18n (2443, "Clearing option %s for contact %s.\n") :
                       data & COF_GROUP   ? i18n (2444, "Clearing option %s for contact group %s.\n") :
                       data & COF_SERVER  ? i18n (2721, "Clearing option %s for server %s.\n")
                                          : i18n (2445, "Clearing option %s%s globally.\n"),
                      coptname, coptobj);
        }
        else
        {
            rl_printf (i18n (2446, "Invalid value %s for boolean option %s.\n"), s_qquote (par->txt), coptname);
            free (coptname);
            continue;
        }
        if (flag & COF_GROUP && ~flag & COF_CONTACT && data == COF_GROUP && connl)
            SnacCliSetstatus (uiG.conn, uiG.conn->status, 3);
        free (coptname);
    }
    free (coptobj);
    return 0;
}

/*
 * Clears the screen.
 */
static JUMP_F(CmdUserClear)
{
    ReadLineClrScr ();
    return 0;
}


/*
 * Registers a new user.
 */
static JUMP_F(CmdUserRegister)
{
    strc_t par;

    if ((par = s_parse (&args)))
    {
        ANYCONN;

        if (!uiG.conn || uiG.conn->type == TYPE_SERVER)
        {
            Server *newc = SrvRegisterUIN (uiG.conn, par->txt);
            ConnectionInitServer (newc);
        }
    }
    else
        rl_print (i18n (2447, "No new password given.\n"));
    return 0;
}

/*
 * Toggles ignoring a user.
 */
static JUMP_F(CmdUserTogIgnore)
{
    ContactGroup *cg;
    Contact *cont;
    int i;
    OPENCONN;

    if ((cg = s_parselistrem (&args, uiG.conn)))
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
        {
            if (ContactPrefVal (cont, CO_IGNORE))
            {
                OptSetVal (&cont->copts, CO_IGNORE, 0);
                rl_printf (i18n (1666, "Unignored %s.\n"), cont->nick);
            }
            else
            {
                OptSetVal (&cont->copts, CO_IGNORE, 1);
                rl_printf (i18n (1667, "Ignoring %s.\n"), cont->nick);
            }
        }
    return 0;
}

/*
 * Toggles beeing invisible to a user.
 */
static JUMP_F(CmdUserTogInvis)
{
    ContactGroup *cg;
    Contact *cont;
    int i, j;
    OPENCONN;

    if ((cg = s_parselistrem (&args, uiG.conn)))
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
        {
            if (ContactPrefVal (cont, CO_HIDEFROM))
            {
                OptSetVal (&cont->copts, CO_HIDEFROM, 0);
                if (uiG.conn->type == TYPE_SERVER)
                {
                    ContactIDs *cid;
                    if (!ContactIsInv (uiG.conn->status))
                        SnacCliReminvis (uiG.conn, cont);
                    if ((cid = ContactIDHas (cont, roster_invisible)) && cid->issbl)
                        SnacCliRosterdelete (cont->serv, cont->screen, cid->tag, cid->id, roster_invisible);
                }
                rl_printf (i18n (2020, "Being visible to %s.\n"), cont->nick);
            }
            else
            {
                j = ContactPrefVal (cont, CO_INTIMATE);
                OptSetVal (&cont->copts, CO_INTIMATE, 0);
                OptSetVal (&cont->copts, CO_HIDEFROM, 1);
                if (uiG.conn->type == TYPE_SERVER)
                {
                    ContactIDs *cid;
                    SnacCliAddinvis (uiG.conn, cont);
                    if (j || ContactIsInv (uiG.conn->status))
                        SnacCliSetstatus (uiG.conn, uiG.conn->status, 3);
                    if ((cid = ContactIDHas (cont, roster_visible)) && cid->issbl)
                        SnacCliRosterdelete (cont->serv, cont->screen, cid->tag, cid->id, roster_visible);
                    SnacCliRosterentryadd (cont->serv, cont->screen, 0, ContactIDGet (cont, roster_invisible), roster_invisible, 0, NULL, 0);
                }
                rl_printf (i18n (2021, "Being invisible to %s.\n"), cont->nick);
            }
        }
    return 0;
}

/*
 * Toggles visibility to a user.
 */
static JUMP_F(CmdUserTogVisible)
{
    ContactGroup *cg;
    Contact *cont;
    Event *event;
    int i, j;
    OPENCONN;

    if ((cg = s_parselistrem (&args, uiG.conn)))
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
        {
            event = QueueDequeue2 (uiG.conn->conn, QUEUE_TOGVIS, 0, cont);
            if (event)
                EventD (event);
            if (ContactPrefVal (cont, CO_INTIMATE))
            {
                OptSetVal (&cont->copts, CO_INTIMATE, 0);
                if (uiG.conn->type == TYPE_SERVER)
                {
                    ContactIDs *cid;
                    if (ContactIsInv (uiG.conn->status))
                        SnacCliRemvisible (uiG.conn, cont);
                    if ((cid = ContactIDHas (cont, roster_visible)) && cid->issbl)
                        SnacCliRosterdelete (cont->serv, cont->screen, cid->tag, cid->id, roster_visible);
                }
                rl_printf (i18n (1670, "Normal visible to %s now.\n"), cont->nick);
            }
            else
            {
                j = ContactPrefVal (cont, CO_HIDEFROM);
                OptSetVal (&cont->copts, CO_HIDEFROM, 0);
                OptSetVal (&cont->copts, CO_INTIMATE, 1);
                if (uiG.conn->type == TYPE_SERVER)
                {
                    ContactIDs *cid;
                    SnacCliAddvisible (uiG.conn, cont);
                    if ((cid = ContactIDHas (cont, roster_invisible)) && cid->issbl)
                        SnacCliRosterdelete (cont->serv, cont->screen, cid->tag, cid->id, roster_invisible);
                    SnacCliRosterentryadd (cont->serv, cont->screen, 0, ContactIDGet (cont, roster_visible), roster_visible, 0, NULL, 0);
                    if (j || !ContactIsInv (uiG.conn->status))
                        SnacCliSetstatus (uiG.conn, uiG.conn->status, 3);
                }
                rl_printf (i18n (1671, "Always visible to %s now.\n"), cont->nick);
            }
        }
    return 0;
}

/*
 * Add a user to your contact list.
 *
 * 2 contact group
 * 1 alias
 * 0 auto
 */
static JUMP_F(CmdUserAdd)
{
    ContactGroup *cg = NULL, *acg;
    Contact *cont = NULL, *cont2;
    char *cmd;
    strc_t par;
    int i, rc;
    OPENCONN;

    if (data == 2)
    {
        if ((cg = s_parsecg (&args, uiG.conn)))
        {
            rl_printf (i18n (2448, "Contact group '%s' already exists.\n"), cg->name);
            return 0;
        }
        if (!(par = s_parse (&args)))
        {
            rl_print (i18n (2240, "No contact group given.\n"));
            return 0;
        }
        if ((cg = ContactGroupC (uiG.conn, 0, par->txt)))
        {
            rl_printf (i18n (2245, "Added contact group '%s'.\n"), par->txt);
            if (uiG.conn->type == TYPE_SERVER)
                SnacCliRosteraddgroup (uiG.conn, cg, 3);
        }
        else
        {
            rl_print (i18n (2118, "Out of memory.\n"));
            return 0;
        }
    }
    
    if (!data)
        (cg = s_parsecg (&args, uiG.conn));

    if (cg)
    {
        if ((acg = s_parselistrem (&args, uiG.conn)))
        {
            for (i = 0; (cont = ContactIndex (acg, i)); i++)
            {
                if (!cont->group)
                {
                    ContactCreate (uiG.conn, cont);
                    if (uiG.conn->type == TYPE_SERVER && ContactPrefVal (cont, CO_WANTSBL))
                        SnacCliRosteraddcontact (uiG.conn, cont, 3);
                    rl_printf (i18n (2590, "%s added as %s.\n"), cont->screen, cont->nick);
                }
                else if (ContactHas (cg, cont))
                {
                    if (uiG.conn->type == TYPE_SERVER)
                        SnacCliRostermovecontact (cont->serv, cont, cg, 3);
                    if (cont->group != cg)
                    {
                        rl_printf (i18n (2449, "Primary contact group for contact '%s' is now '%s'.\n"),
                                   cont->nick, cg->name);
                        cont->group = cg;
                    }
                    else
                        rl_printf (i18n (2591, "Contact group '%s' already has contact '%s' (%s).\n"),
                                   cg->name, cont->nick, cont->screen);
                }
                else if (ContactAdd (cg, cont))
                {
                    rl_printf (i18n (2241, "Added '%s' to contact group '%s'.\n"), cont->nick, cg->name);
                    rl_printf (i18n (2449, "Primary contact group for contact '%s' is now '%s'.\n"),
                               cont->nick, cg->name);
                    if (uiG.conn->type == TYPE_SERVER)
                        SnacCliRostermovecontact (cont->serv, cont, cg, 3);
                    cont->group = cg;
                }
                else
                    rl_print (i18n (2118, "Out of memory.\n"));
            }
            rl_print (i18n (1754, "Note: You need to 'save' to write new contact list to disc.\n"));
        }
        return 0;
    }

    if (!(cont = s_parsenick (&args, uiG.conn)))
    {
        rl_printf (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
        return 0;
    }

    if (!(cmd = s_parserem (&args)) || !*cmd)
    {
        rl_print (i18n (2116, "No new nick name given.\n"));
        return 0;
    }

    i = strlen (cmd);
    while (i > 1 && strchr (" \r\n\t", cmd[i-1]))
        cmd[i---1] = 0;
    if (!cont->group)
    {
        if ((cont2 = ContactFind (uiG.conn, cmd)))
            rl_printf (i18n (2593, "'%s' (%s) is already used as a nick.\n"),
                     cmd, cont2->screen);
        else
        {
            rl_printf (i18n (2590, "%s added as %s.\n"), cont->screen, cmd);
            rl_print (i18n (1754, "Note: You need to 'save' to write new contact list to disc.\n"));
            if (c_strlen (cmd) > (UDWORD)uiG.nick_len)
                uiG.nick_len = c_strlen (cmd);
            ContactCreate (uiG.conn, cont);
            ContactAddAlias (cont, cmd);
            if (uiG.conn->type == TYPE_SERVER && ContactPrefVal (cont, CO_WANTSBL))
                SnacCliRosteraddcontact (uiG.conn, cont, 3);
        }
    }
    else
    {
        if (ContactFindAlias (cont, cmd))
            rl_printf (i18n (2592, "'%s' is already an alias for '%s' (%s).\n"),
                     cmd, cont->nick, cont->screen);
        else if ((cont2 = ContactFind (uiG.conn, cmd)))
            rl_printf (i18n (2593, "'%s' (%s) is already used as a nick.\n"),
                     cmd, cont2->screen);
        else
        {
            rc = ContactAddAlias (cont, cmd);
            if (!rc)
                return 0;
            if (rc == 2 && uiG.conn->type == TYPE_SERVER && ContactPrefVal (cont, CO_ISSBL))
                SnacCliRosterupdatecontact (uiG.conn, cont, 3);
            else if (rc == 2 && uiG.conn->type == TYPE_SERVER && ContactPrefVal (cont, CO_WANTSBL))
                SnacCliRosteraddcontact (uiG.conn, cont, 3);
            rl_printf (i18n (2594, "Added '%s' as an alias for '%s' (%s).\n"),
                     cmd, cont->nick, cont->screen);
            rl_print (i18n (1754, "Note: You need to 'save' to write new contact list to disc.\n"));
        }
    }

    return 0;
}

/*
 * Remove a user from your contact list.
 *
 * 4 remgroup <g> <c>... - remove contacts from group
 * 4 remgroup <g>        - remove empty contact group
 * 4 remgroup all <g>    - remove all contacts from contact group, then remove group
 * 2 remcont <c>...      - remove contacts
 * 1 remalias <c>...     - remove mentioned alias from contact
 * 0 rem <g> <c>...      - remove contacts from group
 * 0 rem <g>             - remove empty contact group
 * 0 rem all <g>         - remove all contacts from group
 * 0 rem <c>...          - remove contacts or remove alias from contact
 * 0 rem all <c>...      - remove contacts
 *
 * .... this is a total mess.
 */
static JUMP_F(CmdUserRemove)
{
    ContactGroup *cg = NULL, *acg, *ncg;
    Contact *cont = NULL;
    const char *oldalias;
    char *screen;
    char *alias;
    UBYTE all = 0;
    UBYTE did = 0;
    int i, rc;
    OPENCONN;
    
    if (!(data & 3))
    {
        if (s_parsekey (&args, "all"))
            all = 1;
        cg = s_parsecg (&args, uiG.conn);
    }
    s_parsekey (&args, "");
    
    if (cg && (all || (!args && !ContactIndex (cg, 0))))
    {
        if (cg->serv->contacts == cg)
            return 0;
        while ((cont = ContactIndex (cg, 0)))
        {
            did = 1;
            ncg = ContactGroupFor (cont, cg);
            if (uiG.conn->type == TYPE_SERVER && cont->group == cg)
                SnacCliRostermovecontact (cont->serv, cont, ncg, 3);
            ContactRem (cg, cont);
            ContactAdd (ncg, cont);
            if (data != 4)
                rl_printf (i18n (2243, "Removed contact '%s' from group '%s'.\n"),
                    cont->nick, cg->name);
        }
        if (data == 4 || (data == 0 && !all))
        {
            SnacCliRosterdeletegroup (uiG.conn, cg, 3);
            ContactGroupD (cg);
            did = 1;
        }
    }
    else if (data == 4 || cg)
    {
        if (!cg)
        {
            rl_print (i18n (2240, "No contact group given.\n"));
            return 0;
        }
        acg = s_parselistrem (&args, uiG.conn);
        if (!acg)
            return 0;
        for (i = 0; (cont = ContactIndex (acg, i)); i++)
        {
            if (ContactHas (cg, cont))
            {
                ncg = ContactGroupFor (cont, cg);
                if (uiG.conn->type == TYPE_SERVER && cont->group == cg)
                    SnacCliRostermovecontact (uiG.conn, cont, ncg, 3);
                ContactRem (cg, cont);
                ContactAdd (ncg, cont);
                did = 1;
                rl_printf (i18n (2243, "Removed contact '%s' from group '%s'.\n"),
                          cont->nick, cg->name);
            }
            else
                rl_printf (i18n (2246, "Contact '%s' is not in group '%s'.\n"),
                          cont->nick, cg->name);
        }
    }
    else if (data == 2 || all)
    {
        acg = s_parselistrem (&args, uiG.conn);
        if (!acg)
            return 0;
        for (i = 0; (cont = ContactIndex (acg, i)); i++)
        {
            if (!cont->group)
                continue;

            if (uiG.conn->type == TYPE_SERVER && ContactPrefVal (cont, CO_ISSBL))
                SnacCliRosterdeletecontact (uiG.conn, cont, 1);

            alias = strdup (cont->nick);
            screen = strdup (cont->screen);
            
            ContactD (cont);
            rl_printf (i18n (2595, "Removed contact '%s' (%s).\n"), alias, screen);

            s_free (alias);
            s_free (screen);
            did = 1;
        }
    }
    else
    {
        while ((cont = s_parsenick_s (&args, MULTI_SEP, 0, uiG.conn, &oldalias)))
        {
            if (!cont->group)
                continue;

            alias = strdup (oldalias);
            screen = strdup (cont->screen);

            if (!cont->alias && !data)
            {
                if (uiG.conn->type == TYPE_SERVER && ContactPrefVal (cont, CO_ISSBL))
                    SnacCliRosterdeletecontact (uiG.conn, cont, 1);
                ContactD (cont);
                rl_printf (i18n (2595, "Removed contact '%s' (%s).\n"),
                          alias, screen);
            }
            else
            {
                rc = ContactRemAlias (cont, alias);
                if (rc == 2 && uiG.conn->type == TYPE_SERVER && ContactPrefVal (cont, CO_ISSBL))
                    SnacCliRosterupdatecontact (uiG.conn, cont, 3);
                else if (rc == 2 && uiG.conn->type == TYPE_SERVER && ContactPrefVal (cont, CO_WANTSBL))
                    SnacCliRosteraddcontact (uiG.conn, cont, 3);
                rl_printf (i18n (2596, "Removed alias '%s' for '%s' (%s).\n"),
                         alias, cont->nick, screen);
            }
            s_free (alias);
            s_free (screen);
            did = 1;
        }
    }
    if (did)
        rl_print (i18n (1754, "Note: You need to 'save' to write new contact list to disc.\n"));
    return 0;
}

/*
 * Authorize another user to add you to the contact list.
 */
static JUMP_F(CmdUserAuth)
{
    ContactGroup *cg;
    Contact *cont;
    int i;
    char *msg;
    auth_t how;
    OPENCONN;

    if (!data)
    {
        if      (s_parsekey (&args, "deny"))  data = 2;
        else if (s_parsekey (&args, "req"))   data = 3;
        else if (s_parsekey (&args, "add"))   data = 4;
        else if (s_parsekey (&args, "grant")) data = 5;
        else if (*args)                       data = 5;
        else                                  data = 0;
    }
    if (!data)
    {
        rl_print (i18n (2119, "auth [grant] <contacts>    - grant authorization.\n"));
        rl_print (i18n (2120, "auth deny <contacts> <msg> - refuse authorization.\n"));
        rl_print (i18n (2121, "auth req  <contacts> <msg> - request authorization.\n"));
        rl_print (i18n (2145, "auth add  <contacts>       - authorized add.\n"));
        return 0;
    }
    if (!(cg = s_parselist (&args, uiG.conn)))
        return 0;
    msg = s_parserem (&args);

    switch (data)
    {
        case 2: how = auth_deny; break;
        case 3: how = auth_req;  break;
        case 4: how = auth_add;  break;
        default:
        case 5: how = auth_grant;
    }

    for (i = 0; (cont = ContactIndex (cg, i)); i++)
        IMCliAuth (cont, msg, how);
    return 0;
}

/*
 * Save user preferences.
 */
static JUMP_F(CmdUserSave)
{
    int i = Save_RC ();
    if (i == -1)
        rl_print (i18n (1679, "Sorry saving your personal reply messages went wrong!\n"));
    else
        rl_print (i18n (1680, "Your personal settings have been saved!\n"));
    return 0;
}

/*
 * Send an URL message.
 */
static JUMP_F(CmdUserURL)
{
    ContactGroup *cg;
    Contact *cont;
    char *url, *msg;
    const char *cmsg;
    strc_t par;
    int i;
    OPENCONN;

    if (!(cg = s_parselist (&args, uiG.conn)))
        return 0;
    
    if (!(par = s_parse (&args)))
    {
        rl_print (i18n (1678, "No URL given.\n"));
        return 0;
    }

    url = strdup (par->txt);

    if (!(msg = s_parserem (&args)))
        msg = "";

    cmsg = s_sprintf ("%s%c%s", msg, Conv0xFE, url);
    s_repl (&uiG.last_message_sent, cmsg);
    uiG.last_message_sent_type = MSG_URL;

    for (i = 0; (cont = ContactIndex (cg, i)); i++)
    {
        IMCliMsg (cont, MSG_URL, cmsg, NULL);
        uiG.last_sent = cont;
    }

    free (url);
    return 0;
}

/*
 * Shows the user in your tab list.
 */
static JUMP_F(CmdUserTabs)
{
    int i;
    const Contact *cont;
    time_t when;
    ANYCONN;

    for (i = 0; TabGet (i); i++)
        ;
    rl_printf (i18n (2450, "Last %d people you talked with:\n"), i);
    for (i = 0; (cont = TabGet (i)); i++)
    {
        when = TabTime (i);
        rl_printf ("%s    %s", s_time (&when), cont->nick);
        rl_printf (" %s(%s)%s\n", COLQUOTE, s_status (cont->status, cont->nativestatus), COLNONE);
    }
    return 0;
}

/*
 * Displays the last message received from the given nickname.
 */
static JUMP_F(CmdUserLast)
{
/*    ContactGroup *cg; */
    Contact *cont = NULL;
/*    int i; */
    ANYCONN;

    if (!(cont = s_parsenick (&args, uiG.conn)))
    {
        HistShow (NULL);

/*        cg = uiG.conn->contacts;
        rl_print (i18n (1682, "You have received messages from:\n"));
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
            if (cont->last_message)
                rl_printf ("  " COLCONTACT "%s" COLNONE " %s " COLQUOTE "%s" COLNONE "\n",
                         cont->nick, s_time (&cont->last_time), cont->last_message);*/
        return 0;
    }

    do
    {
/*        if (cont->last_message)
        {
            rl_printf (i18n (2106, "Last message from %s%s%s at %s:\n"),
                     COLCONTACT, cont->nick, COLNONE, s_time (&cont->last_time));
            rl_printf (COLQUOTE "%s" COLNONE "\n", cont->last_message);
        }
        else
        {
            rl_printf (i18n (2107, "No messages received from %s%s%s.\n"),
                     COLCONTACT, cont->nick, COLNONE);
        }*/
        HistShow (cont);
        if (*args == ',')
            args++;
    }
    while ((cont = s_parsenick (&args, uiG.conn)));
    return 0;
}

/*
 * Displays the history received from the given nickname.
 *
 * This function reads the log file of a given contact. To print the last
 * n messages, read all messages and save the last n file position of
 * these messages. Reset position to the first requested message and print
 * them all.
 *
 * Only the 'real' log files are read, not the symlink file like putlog().
 *
 * Comand parameter:
 *      h[istory] <contact> [<min> [<count>]]
 *
 *      <contact> Nick or UIN
 *      <min>     Last messages of log file. If <min> is negative, count from
 *                begining. -1 means the first message, -13 the 13th message
 *                of logfile. If <min> is zero dump all messages. Default is
 *                DEFAULT_HISTORY_COUNT.
 *      <num>     Number of shown message. <contact> -1 20 list the first
 *                20 messages.  Default is <min> (if <min> is negative default is
 *                DEFAULT_HISTORY_COUNT).
 *
 * Comand parameter:
 *      historyd  <contact|*> <date> [<count>]
 *
 *      <contact|*> Nick or UIN or all contacts
 *      <date>    Earliest date of shown messages. <date> has to be in
 *                ISO 8601 format (CCYY-MM-DD or CCYY-MM-DDThh:mm:ss).
 *      <count>   Count of shown message. Default is DEFAULT_HISTORY_COUNT.
 */
static JUMP_F(CmdUserHistory)
{
    Contact *ra_cont = NULL, *cont = NULL;
    int msgCount;
    int msgMin, msgNum, msgLines;
    time_t time, htime = (time_t)0;
    struct tm stamp, hstamp;
    const char *timeformat;
    strc_t par, p, line;

    FILE *logfile;
    long *fposArray = NULL, fposLen, len;
    char buffer[LOG_MAX_PATH + 1], *target = buffer;
    const char *linep;
    int fposCur;
    UDWORD parsetmp;

    int ra_i, ra_first = 0, ra_mode = 0,
        ra_msgMin = DEFAULT_HISTORY_COUNT,
        ra_msgNum = DEFAULT_HISTORY_COUNT;

    ANYCONN;

    if (data == 1 && s_parsekey (&args, "*"))
        ra_mode = 1;
    else if (!(ra_cont = s_parsenick (&args, uiG.conn)))
    /*
     * check for nick/uin in contacts. If first parameter isn't a
     * contact then exit.
     * parse history parameters min and num
     */
    {
        if ((par = s_parse (&args)))
            rl_printf (i18n (1061, "'%s' not recognized as a nick name.\n"), par->txt);
        else if (data == 1)
            rl_print(i18n (2711, "historyd <contact|*> [<date> [<count>]] -- Show history of <contact> or all contacts (for *) since <date>.\n"));
        else
            rl_print (i18n (2451, "history <contact> [<last> [<count>]] -- Show history of <contact>.\n"));
        return 0;
    }

    if (data == 1)
        memset (&hstamp, 0, sizeof (struct tm));
    
    if (data == 1 && (par = s_parse_s (&args, MULTI_SEP)))
    {
        if ((strlen (par->txt) == 10 &&
             sscanf (par->txt, "%4d-%2d-%2d",
                     &hstamp.tm_year, &hstamp.tm_mon, &hstamp.tm_mday) == 3) ||
            (strlen (par->txt) == 19 &&
             sscanf (par->txt, "%4d-%2d-%2dT%2d:%2d:%2d",
                     &hstamp.tm_year, &hstamp.tm_mon, &hstamp.tm_mday,
                     &hstamp.tm_hour, &hstamp.tm_min, &hstamp.tm_sec) == 6))
        {
            hstamp.tm_mon--;
            hstamp.tm_year -= 1900;
            htime = timelocal (&hstamp);
            ra_msgMin = 0;
        }
        else
        {
            rl_printf (i18n (2396, "Parameter '%s' has a wrong date format, it has to be ISO 8601 compliant. Try '2004-01-31' or '2004-01-31T23:12:05'.\n"), par->txt);
            return 0;
        }
    }

    if (data != 1 && s_parseint (&args, &parsetmp))
        ra_msgMin = (SDWORD)parsetmp;

    if (ra_msgMin >= 0)
        ra_msgNum = ra_msgMin;
    
    if (s_parseint (&args, &parsetmp))
        ra_msgNum = parsetmp;

    /*
     *  correct parameters min and num parameters
     */
    if (ra_msgMin == 0 && data != 1)
        ra_msgNum = 0;
    if (ra_msgNum < 0)
        ra_msgNum = -ra_msgNum;

    for (ra_i = 0; ra_i >= 0; ra_i++)
    {
        if (ra_mode)
        {
            cont = ContactIndex(NULL, ra_i);
            if (!cont)
                break;
            ra_first = 1;
        }
        else
        {
            if (cont)
                break;
            cont = ra_cont;
        }
            
        msgMin = ra_msgMin;
        msgNum = ra_msgNum;

        /* reset to default */
        msgLines = 0;
        fposArray = NULL;
        fposLen = 0;
        target = buffer;
        fposCur = 0;
        
        /*
         * get filename of logfile and open it for reading.
         */
        if (!prG->logplace || !*prG->logplace)
            prG->logplace = "history" _OS_PATHSEPSTR;

        snprintf (buffer, sizeof (buffer), "%s", s_realpath (prG->logplace));
        target += strlen (buffer);

        if (target[-1] == _OS_PATHSEP)
            snprintf (target, buffer + sizeof (buffer) - target, "%s.log", cont->screen);

        /* try to open logfile for reading (no symlink file) */
        if (!(logfile = fopen (buffer, "r")))
        {
            rl_printf (i18n (2385, "Couldn't open %s for reading: %s (%d).\n"),
                      buffer, strerror (errno), errno);
            continue;
        }

        memset (&stamp, 0, sizeof (struct tm));

        /* Get array of last file positions, reset memory to zeros */
        if (msgMin > 0)
        {
            fposLen = msgMin;
            fposArray = (long *) malloc (fposLen * sizeof (long));
            if (!fposArray)
                msgMin = 0;
        }

        /*
         * count messages only if history should dump only the last
         * n message. Save the last n = msgMin file position of each
         * message in fposArray[].
         */
        msgCount = 0;
        if (msgMin > 0)
        {
            while ((line = UtilIOReadline (logfile)))
            {
                linep = line->txt;
                if (*linep == '#')
                {
                    /* get len to correct the file position to the
                       beginning of line */
                    len = (long)strlen (linep) + 1;
                    linep++;
                    if (!(p = s_parse (&linep)))
                        continue;
                    if (!(p = s_parse (&linep)))
                        continue;
                    if (!(p = s_parse (&linep)))
                        continue;
                    if ((p->txt[0] != '<' || p->txt[1] != '-') &&
                        (p->txt[0] != '-' || p->txt[1] != '>'))
                        continue;
                    
                    msgCount++;
                    /* save corrected file position */
                    fposArray[fposCur] = ftell (logfile) - len;
                    if (++fposCur == fposLen)
                        fposCur = 0;
                }
            }
            /* reset file to the requested position. If message
               count smaller than msgMin, set file position to the first
               message. */
            if (msgCount == 0)
            {
                s_free ((char *)fposArray);
                continue;
            }
            if (msgCount < msgMin)
            {
                fposCur = 0;
                fposLen = msgCount;
            }
            fseek (logfile, fposArray[fposCur], SEEK_SET);
        }


        /*
         * if msgMin negativ, count from the beginning (-1 for first message).
         */
        if (msgMin > 0)
        {
            if (msgCount < msgMin)
            {
                msgNum = msgCount;
                msgCount = 0;
                msgMin = 1;
            }
            else
            {
                msgMin   = msgCount - msgMin + 1;
                msgCount = msgMin - 1;
            }
        }
        else
            msgMin   = -msgMin;

        timeformat = s_sprintf ("%s%s%s%s", COLDEBUG, i18n (2393, "%Y-%m-%d"),
                                COLNONE, i18n (2394, " %I:%M %p"));

        /*
         * dump message.
         * If file positions of messages are available (fposArray != NULL) and
         * log entry isn't a message entry, jump to next message. If fseek()
         * fails, remove position array.
         */
        while ((line = UtilIOReadline (logfile)))
        {
            linep = line->txt;
            if (msgLines && (msgLines > 0 || *linep != '#'))
            {
                rl_printf ("%s%s\n", COLNONE, linep);
                msgLines--;
            }
            else if (*linep == '#')
            {
                msgLines = 0;
                /* evaluate requested history scope
                   if all lines are dumped, exit */
                if (msgNum > 0 && msgCount + 1 == msgMin + msgNum)
                    break;

                if (fposArray)
                {
                    len = strlen (linep) + 1;
                    /* jump to next message according to fposArray[] */
                    if (ftell (logfile) - len != fposArray[fposCur])
                    {
                        if (!fseek (logfile, fposArray[fposCur], SEEK_SET))
                        {
                            free (fposArray);
                            fposArray = NULL;
                        }
                        continue;
                    }
                    if (++fposCur == fposLen)
                        fposCur = 0;
                }
                linep++;

                /* extract time from sent and received messages */
                if (!(p = s_parse (&linep)))
                    continue;
                sscanf (p->txt, "%4d%2d%2d%2d%2d%2d",
                        &stamp.tm_year, &stamp.tm_mon, &stamp.tm_mday,
                        &stamp.tm_hour, &stamp.tm_min, &stamp.tm_sec);

                if (!(p = s_parse (&linep)))
                    continue;

                if (!(p = s_parse (&linep)))
                    continue;
                if ((p->txt[0] != '<' || p->txt[1] != '-') &&
                    (p->txt[0] != '-' || p->txt[1] != '>'))
                    continue;
                msgCount++;
                
                if (msgCount < msgMin && data != 1)
                    continue;

                /* convert local time */
                stamp.tm_mon--;
                stamp.tm_year -= 1900;
                time = timegm (&stamp);

                if (time != -1)
                {
                    localtime_r (&time, &stamp);
                    /* history date command */
                    if (data == 1 && difftime (time, htime) < 0)
                    {
                        msgMin = msgCount + 1;
                        msgLines = 0;
                        continue;
                    }
                }

                if (ra_first)
                {
                    rl_printf("%s>>> %s%s\n", COLQUOTE, COLCONTACT, cont->nick);
                    ra_first = 0;
                }
                rl_printf ("%s %s[%4d] ", s_strftime (&time, timeformat, 0), COLNONE, msgCount);
                msgLines = -1;

                if (*p->txt == '<')
                    rl_printf ("%s<- ", COLINCOMING);
                else
                    rl_printf ("%s-> ", COLSENT);

                linep = strrchr (linep, '+');
                if (!linep)
                    continue;
                linep++;
                /* parse number of lines of message */
                if (s_parseint (&linep, &parsetmp))
                    msgLines = (SDWORD)parsetmp;
                if (msgLines <= 0)
                    rl_printf ("%s\n", COLNONE);
            }
        }
        fclose (logfile);
        s_free ((char *)fposArray);
    }
    return 0;
}

/*
 * Search a pattern in messages of a given contact.
 *
 * Open log file of the given contact and read messages into allocated buffer
 * *msg (and *msgLower for case insensitive search). To find the pattern
 * in the message the function strstr() is used. If a pattern is found dump
 * the hole message with time, message number and highlighted pattern within
 * the message.
 * Comand 'find' search for a case insensitive pattern, 'finds' for case
 * sensitive pattern (parameter data is 1).
 *
 * Comand parameter:
 *      find <contact> <pattern>
 *      finds <contact> <pattern>
 *
 *      <contact> Nick or UIN
 *      pattern   Pattern of search. All leaded and followed whitespaces will
 *                be deleted. Whitespaces could be protected by a double
 *                quotation marks.
 *
 * ToDo: replace case lowering function tolower() of strings with other
 *       function to include international special characters.
 */
static JUMP_F(CmdUserFind)
{
    Contact *cont = NULL;

    char *pattern, *p = NULL, c;
    char *msg = NULL, *m, *msgLower = NULL, *mL;
    UDWORD size = 0;
    int n, msgCount, matchCount = 0, patternLen, len;
    time_t time;
    struct tm stamp;
    const char *timeformat;
    BOOL isMsg = FALSE, isIncoming = FALSE, doRead = TRUE;
    strc_t par, line = NULL;

    FILE *logfile;
    char buffer[LOG_MAX_PATH + 1], *target = buffer;
    const char *linep;
    int fd;

    ANYCONN;

    /*
     * parse history parameters and remove all white spaces around
     * the pattern. For case insensitive, lower the pattern.
     */
    if (!(cont = s_parsenick (&args, uiG.conn)))
    {
        if ((par = s_parse (&args)))
            rl_printf (i18n (1061, "'%s' not recognized as a nick name.\n"), par->txt);
        else
        {
            rl_print (i18n (2388, "find <contact> <pattern> -- Search <pattern> case insensitive in history of <contact>.\n"));
            rl_print (i18n (2389, "finds <contact> <pattern> -- Search <pattern> case sensitive in history of <contact>.\n"));
        }
        return 0;
    }

    if (!(pattern = s_parserem (&args)) || !*pattern)
    {
        rl_printf (i18n (2390, "No or empty pattern given.\n"));
        return 0;
    }
    patternLen = strlen (pattern);
    pattern = strdup (pattern);

    /* find is case insensitive, lower pattern */
    if (!data)
        for (p = pattern; *p; p++)
            *p = tolower (*p);

    /*
     * get filename of logfile and open it for reading.
     */
    if (!prG->logplace || !*prG->logplace)
        prG->logplace = "history" _OS_PATHSEPSTR;

    snprintf (buffer, sizeof (buffer), "%s", s_realpath (prG->logplace));
    target += strlen (buffer);

    if (target[-1] == _OS_PATHSEP)
        snprintf (target, buffer + sizeof (buffer) - target, "%s.log", cont->screen);

    if ((fd = open (buffer, O_RDONLY, S_IRUSR)) == -1)
    {
        rl_printf (i18n (2385, "Couldn't open %s for reading: %s (%d).\n"),
                   buffer, strerror (errno), errno);
        free (pattern);
        return 0;
    }
    if (!(logfile = fopen (buffer, "r")))
    {
        rl_printf (i18n (2385, "Couldn't open %s for reading: %s (%d).\n"),
                  buffer, strerror (errno), errno);
        free (pattern);
        close (fd);
        return 0;
    }

    timeformat = s_sprintf ("%s%s%s%s", COLDEBUG, i18n (2393, "%Y-%m-%d"),
                            COLNONE, i18n (2394, " %I:%M %p"));

    /*
     * read message for message into *msg and dump it if pattern is found.
     */
    msgCount = 0;
    doRead = TRUE;
    while (!doRead || (line = UtilIOReadline (logfile)))
    {
        doRead = TRUE;
        if (!line)
            break;

        linep = line->txt;
        isMsg = FALSE;
        if (*linep == '#')
        {
            linep++;
            n = 0;
            while ((par = s_parse (&linep)))
            {
                switch (n)
                {
                    case 0:
                        sscanf (par->txt, "%4d%2d%2d%2d%2d%2d",
                                &stamp.tm_year, &stamp.tm_mon, &stamp.tm_mday,
                                &stamp.tm_hour, &stamp.tm_min, &stamp.tm_sec);
                        break;
                    case 2:
                        if ((par->txt[0] == '<' && par->txt[1] == '-') ||
                            (par->txt[0] == '-' && par->txt[1] == '>'))
                        {
                            msgCount++;
                            isMsg = TRUE;
                            isIncoming = (*par->txt == '<');
                        }
                        break;
                }
                if (++n > 2)
                    break;
            }
        }

        if (!isMsg)
            continue;

        if (msg)
            *msg = '\0';
        while ((line = UtilIOReadline (logfile)))
        {
            linep = line->txt;
            if (*linep == '#')
                break;

            if (!msg)
            {
                msg = (char*) malloc (size = MEMALIGN (strlen (linep) + 2));
                *msg = '\0';
                if (!data)
                    msgLower = (char*) malloc (size);
            }
            len = strlen (msg);
            if (size < len + strlen (linep) + 2)
            {
                msg = realloc (msg, size = MEMALIGN (len + strlen (linep) + 2));
                if (!data)
                    msgLower = realloc (msgLower, size);
            }
            if (len)
                msg[len++] = '\n';
            strcpy (&msg[len], linep);
        }
        doRead = FALSE;

        if (!msg)
            continue;

        /* lower message for case insensitive search */
        if (!data)
        {
            m = msg;
            mL = msgLower;
            while (*m)
            {
                *mL++ = (char)tolower (*m);
                m++;
            }
            *mL = '\0';
        }
        else
            msgLower = msg;

        /*
         * if pattern found, print message and highlight pattern,
         * otherwise read next log message
         */
        if (!(p = strstr (msgLower, pattern)))
            continue;

        matchCount++;

        /* convert local time */
        stamp.tm_mon--;
        stamp.tm_year -= 1900;
        time = timegm (&stamp);

        if (time != -1)
            localtime_r (&time, &stamp);

        rl_printf ("%s %s[%4d] ", s_strftime (&time, timeformat, 0), COLNONE, msgCount);

        if (isIncoming)
            rl_printf ("%s<- ", COLINCOMING);
        else
            rl_printf ("%s-> ", COLSENT);

        m = msg;
        mL = msgLower;
        do
        {
            /* print message before pattern */
            len = p - mL;
            c = m[len];
            m[len] = '\0';
            rl_printf ("%s%s", COLNONE, m);

            /* print pattern colored */
            m[len] = c;
            m += len;
            c = m[patternLen];
            m[patternLen] = '\0';
            rl_printf ("%s%s", COLQUOTE, m);

            /* search next pattern in message */
            m[patternLen] = c;
            mL = p + patternLen;
            m += patternLen;
            p = strstr (mL, pattern);
        } while (p);
        rl_printf ("%s%s\n", COLNONE, m);
    } /* next log line */

    rl_printf (i18n (2386, "Found %s%d%s matches.\n"), COLQUOTE, matchCount, COLNONE);

    close (fd);

    if (msg)
        free (msg);
    if (!data)
        free (msgLower);
    free (pattern);

    return 0;
}

/*
 * Shows climm's uptime.
 */
static JUMP_F(CmdUserUptime)
{
    double TimeDiff = difftime (time (NULL), uiG.start_time);
    Connection *connl;
    int Days, Hours, Minutes, Seconds, i;
    int pak_sent = 0, pak_rcvd = 0, real_pak_sent = 0, real_pak_rcvd = 0;

    Seconds = (int) TimeDiff % 60;
    TimeDiff = TimeDiff / 60.0;
    Minutes = (int) TimeDiff % 60;
    TimeDiff = TimeDiff / 60.0;
    Hours = (int) TimeDiff % 24;
    TimeDiff = TimeDiff / 24.0;
    Days = TimeDiff;

    rl_printf ("%s ", i18n (1687, "climm has been running for"));
    if (Days != 0)
        rl_printf ("%s%02d%s %s, ", COLQUOTE, Days, COLNONE, i18n (1688, "days"));
    if (Hours != 0)
        rl_printf ("%s%02d%s %s, ", COLQUOTE, Hours, COLNONE, i18n (1689, "hours"));
    if (Minutes != 0)
        rl_printf ("%s%02d%s %s, ", COLQUOTE, Minutes, COLNONE, i18n (1690, "minutes"));
    rl_printf ("%s%02d%s %s.\n", COLQUOTE, Seconds, COLNONE, i18n (1691, "seconds"));

    rl_print (i18n (1746, " nr type         sent/received packets/unique packets\n"));
    for (i = 0; (connl = ConnectionNr (i)); i++) /* FIXME */
    {
        rl_printf ("%3d %-12s %7ld %7ld %7ld %7ld\n",
                 i + 1, ConnectionStrType (connl), UD2UL (connl->stat_pak_sent), UD2UL (connl->stat_pak_rcvd),
                 UD2UL (connl->stat_real_pak_sent), UD2UL (connl->stat_real_pak_rcvd));
        pak_sent += connl->stat_pak_sent;
        pak_rcvd += connl->stat_pak_rcvd;
        real_pak_sent += connl->stat_real_pak_sent;
        real_pak_rcvd += connl->stat_real_pak_rcvd;
    }
    rl_printf ("    %-12s %7d %7d %7d %7d\n",
             i18n (1747, "total"), pak_sent, pak_rcvd,
             real_pak_sent, real_pak_rcvd);
    rl_printf (i18n (2073, "Memory usage: %ld packets processing.\n"), UD2UL (uiG.packets));
    return 0;
}

/*
 * List connections, and open/close them.
 */
static JUMP_F(CmdUserConn)
{
    int i, ok = 0;
    UDWORD nr = 0;
    strc_t par;
    Server *servl = NULL;
    Connection *connl = NULL;
    const char *targs;

    if (!data)
    {
        if      (s_parsekey (&args, "login"))   data = 202;
        else if (s_parsekey (&args, "open"))    data = 102;
        else if (s_parsekey (&args, "select"))  data = 203;
        else if (s_parsekey (&args, "delete"))  data = 104;
        else if (s_parsekey (&args, "remove"))  data = 104;
        else if (s_parsekey (&args, "discard")) data = 204;
        else if (s_parsekey (&args, "close"))   data = 105;
        else if (s_parsekey (&args, "logoff"))  data = 205;
        else if (s_parserem (&args))            data = 0;
        else                                    data = 1;
    }
    if (data > 200)
    {
        targs = args;
        if (s_parseint (&targs, &nr) && (servl = ServerNr (nr - 1)))
            args = targs;
        else if ((par = s_parse (&args)))
        {
            servl = ServerFindScreen (TYPEF_ANY_SERVER, par->txt);
            if (!servl && s_parserem (&args))
            {
                if (nr)
                    rl_printf (i18n (2598, "There is no connection number %ld and no connection for UIN %s.\n"), UD2UL (nr), par->txt);
                else
                    rl_printf (i18n (2599, "There is no connection for %s.\n"), par->txt);
                return 0;
            }
            if (!servl)
                args = targs;
        }
        if (!servl)
        {
            if (ServerFindNr (uiG.conn) != (UDWORD)-1)
                servl = uiG.conn;
            else
                servl = ServerNr (0);
            if (!servl)
            {
                rl_printf (i18n (2600, "No connection selected.\n"));
                return 0;
            }
        }
    }
    else if (data > 100)
    {
        if (!s_parseint (&args, &nr))
            rl_printf (i18n (1894, "There is no connection number %ld.\n"), UD2UL (0));
        else if (!(connl = ConnectionNr (nr - 1)))
            rl_printf (i18n (1894, "There is no connection number %ld.\n"), UD2UL (nr));
        else
            ok = 1;
        if (!ok)
            return 0;
    }
     
    switch (data)
    {
        case 0:
            rl_print (i18n (1892, "conn                    List available connections.\n"));
            rl_print (i18n (2094, "conn login [pass]       Log in to current/first server connection.\n"));
            rl_print (i18n (1893, "conn login <nr> [pass]  Log in to server connection <nr>.\n"));
            rl_print (i18n (2722, "conn logoff <nr>        Log off from server connection <nr>.\n"));
            rl_print (i18n (2723, "conn discard <nr>       Discard server connection <nr>.\n"));
            rl_print (i18n (2724, "conn open <nr>          Open connection <nr>.\n"));
            rl_print (i18n (2156, "conn close <nr>         Close connection <nr>.\n"));
            rl_print (i18n (2095, "conn remove <nr>        Remove connection <nr>.\n"));
            rl_print (i18n (2097, "conn select <nr>        Select connection <nr> as server connection.\n"));
            rl_print (i18n (2100, "conn select <uin>       Select connection with UIN <uin> as server connection.\n"));
            break;

        case 1:
            rl_print (i18n (1887, "Connections:"));
            rl_print ("\n  " COLINDENT);
            
            for (i = 0; (servl = ServerNr (i)); i++)
            {
                Contact *cont = servl->conn->cont;
                rl_printf (i18n (2597, "%02d %-15s version %d%s for %s (%s), at %s:%ld %s\n"),
                          i + 1, ServerStrType (servl), servl->pref_version,
#ifdef ENABLE_SSL
                          servl->conn->ssl_status == SSL_STATUS_OK ? " SSL" : "",
#else
                          "",
#endif
                          cont ? cont->nick : "", ContactStatusStr (servl->status),
                          servl->conn->server ? servl->conn->server : s_ip (servl->conn->ip), UD2UL (servl->conn->port),
                          servl->conn->connect & CONNECT_FAIL ? i18n (1497, "failed") :
                          servl->conn->connect & CONNECT_OK   ? i18n (1934, "connected") :
                          servl->conn->connect & CONNECT_MASK ? i18n (1911, "connecting") : i18n (1912, "closed"));
                if (prG->verbose)
                {
                    char *t1, *t2, *t3;
                    rl_printf (i18n (1935, "    type %d socket %d ip %s (%d) on [%s,%s] id %lx/%x/%x\n"),
                         servl->type, servl->conn->sok, t1 = strdup (s_ip (servl->conn->ip)),
                         servl->conn->connect, t2 = strdup (s_ip (servl->conn->our_local_ip)),
                         t3 = strdup (s_ip (servl->conn->our_outside_ip)),
                         UD2UL (servl->conn->our_session), servl->conn->our_seq, servl->oscar_snac_seq);
#ifdef ENABLE_SSL
                    rl_printf (i18n (2453, "    at %p parent %p assoc %p ssl %d\n"), servl, servl->conn->serv, servl->oscar_dc, servl->conn->ssl_status);
#else
                    rl_printf (i18n (2081, "    at %p parent %p assoc %p\n"), servl, servl->conn->serv, servl->oscar_dc);
#endif
                    rl_printf (i18n (2454, "    open %p reconn %p close %p err %p dispatch %p\n"),
                              servl->c_open, servl->conn->reconnect, servl->conn->close, servl->conn->error, servl->conn->dispatch);
                    free (t1);
                    free (t2);
                    free (t3);
                }
            }
            rl_print (COLEXDENT "\r");
            rl_print (i18n (1887, "Connections:"));
            rl_print ("\n  " COLINDENT);
            for (i = 0; (connl = ConnectionNr (i)); i++)
            {
                Contact *cont = connl->cont;
                rl_printf (i18n (2597, "%02d %-15s version %d%s for %s (%s), at %s:%ld %s\n"),
                          i + 1, ConnectionStrType (connl), connl->version,
#ifdef ENABLE_SSL
                          connl->ssl_status == SSL_STATUS_OK ? " SSL" : "",
#else
                          "",
#endif
                          cont ? cont->nick : "", connl->serv ? ContactStatusStr (connl->serv->status) : "",
                          connl->server ? connl->server : s_ip (connl->ip), UD2UL (connl->port),
                          connl->connect & CONNECT_FAIL ? i18n (1497, "failed") :
                          connl->connect & CONNECT_OK   ? i18n (1934, "connected") :
                          connl->connect & CONNECT_MASK ? i18n (1911, "connecting") : i18n (1912, "closed"));
                if (prG->verbose)
                {
                    char *t1, *t2, *t3;
                    rl_printf (i18n (1935, "    type %d socket %d ip %s (%d) on [%s,%s] id %lx/%x/%x\n"),
                         connl->type, connl->sok, t1 = strdup (s_ip (connl->ip)),
                         connl->connect, t2 = strdup (s_ip (connl->our_local_ip)),
                         t3 = strdup (s_ip (connl->our_outside_ip)),
                         UD2UL (connl->our_session), connl->our_seq, connl->serv ? connl->serv->oscar_snac_seq : 0);
#ifdef ENABLE_SSL
                    rl_printf (i18n (2453, "    at %p parent %p assoc %p ssl %d\n"), connl, connl->serv, connl->oscar_file, connl->ssl_status);
#else
                    rl_printf (i18n (2081, "    at %p parent %p assoc %p\n"), connl, connl->serv, connl->oscar_file);
#endif
                    rl_printf (i18n (2454, "    open %p reconn %p close %p err %p dispatch %p\n"),
                              connl->c_open, connl->reconnect, connl->close, connl->error, connl->dispatch);
                    free (t1);
                    free (t2);
                    free (t3);
                }
            } 
            rl_print (COLEXDENT "\r");
            break;
            
        case 102:
            if (connl->connect & CONNECT_OK)
                rl_printf (i18n (2725, "Connection #%ld is already open.\n"), UD2UL (nr));
            else if (!connl->c_open)
                rl_printf (i18n (2726, "Don't know how to open connection type %s for #%ld.\n"),
                    ConnectionStrType (connl), UD2UL (nr));
            else
                connl->c_open (connl);
            break;

        case 202:
            if ((targs = s_parserem (&args)))
                s_repl (&servl->passwd, targs);
            if (servl->conn->connect & CONNECT_OK)
                rl_printf (i18n (2601, "Connection for %s is already open.\n"), servl->screen);
            else if (!servl->c_open)
                rl_printf (i18n (2602, "Don't know how to open connection type %s for %s.\n"),
                    ServerStrType (servl), servl->screen);
            else if (!servl->passwd || !*servl->passwd)
                rl_printf (i18n (2688, "No password given for %s.\n"), servl->screen);
            else
            {
                Event *loginevent = servl->c_open (servl);
                if (loginevent)
                    QueueEnqueueDep (servl->conn, QUEUE_CLIMM_COMMAND, 0, loginevent, NULL, servl->conn->cont,
                                     OptSetVals (NULL, CO_CLIMMCOMMAND, "eg", 0), &CmdUserCallbackTodo);
                                                                                                              
            }
            break;

        case 203:
            uiG.conn = servl;
            rl_printf (i18n (2603, "Selected connection %ld (version %d, UIN %s) as current connection.\n"),
                       UD2UL (nr), servl->pref_version, servl->screen);
            break;
        
        case 104:
            if (connl->serv && connl->serv->conn == connl)
                rl_printf (i18n (2727, "Connection #%ld is a server's main i/o connection.\n"), UD2UL (nr));
            else
            {
                rl_printf (i18n (2101, "Removing connection %ld and its dependents completely.\n"), UD2UL (nr));
                ConnectionD (connl);
            }
            break;
        
        case 204:
            rl_printf (i18n (2728, "Discarding server connection %ld completely.\n"), UD2UL (nr));
            ServerD (servl);
            break;
        
        case 105:
            if (connl->serv && connl->serv->conn == connl)
            {
                rl_printf (i18n (2729, "Logging of from connection %ld.\n"), UD2UL (nr));
                connl->close (connl);
            }
            else if (connl->close)
            {
                rl_printf (i18n (2185, "Closing connection %ld.\n"), UD2UL (nr));
                connl->close (connl);
            }
            else
            {
                rl_printf (i18n (2101, "Removing connection %ld and its dependents completely.\n"), UD2UL (nr));
                ConnectionD (connl);
            }
            break;

        case 205:
            if (servl->conn && servl->conn->close)
            {
                rl_printf (i18n (2729, "Logging of from connection %ld.\n"), UD2UL (nr));
                servl->conn->close (servl->conn);
            }
    }
    return 0;
}

#ifdef ENABLE_XMPP
/*
 * Search GoogleMail
 */
static JUMP_F(CmdUserGmail)
{
    UDWORD isince;
    time_t since;
    strc_t par;
    const char *q;

    if (uiG.conn->type != TYPE_XMPP_SERVER)
        return 0;
    
    if (s_parseint (&args, &isince))
        since = isince;
    else if (s_parsekey (&args, "more"))
        since = 1;
    else if (strchr ("0123456789", *args) && (par = s_parse (&args)))
    {
        struct tm stamp;
        memset (&stamp, 0, sizeof (struct tm));
        if ((strlen (par->txt) == 10 &&
             sscanf (par->txt, "%4d-%2d-%2d",
                     &stamp.tm_year, &stamp.tm_mon, &stamp.tm_mday) == 3) ||
            (strlen (par->txt) == 19 &&
             sscanf (par->txt, "%4d-%2d-%2dT%2d:%2d:%2d",
                     &stamp.tm_year, &stamp.tm_mon, &stamp.tm_mday,
                     &stamp.tm_hour, &stamp.tm_min, &stamp.tm_sec) == 6))
        {
            stamp.tm_mon--;
            stamp.tm_year -= 1900;
            stamp.tm_isdst = -1;
            since = timelocal (&stamp);
            if (since < 10)
                since = 0;
        }
        else
        {
            rl_printf (i18n (2396, "Parameter '%s' has a wrong date format, it has to be ISO 8601 compliant. Try '2004-01-31' or '2004-01-31T23:12:05'.\n"), par->txt);
            return 0;
        }
    }
    else
        since = 0;
    q = s_parserem (&args);
    XMPPGoogleMail (uiG.conn, since, q ? q : "");
    return 0;
}
#endif

/*
 * Download Contact List
 */
static JUMP_F(CmdUserContact)
{
    strc_t par;
    UDWORD i;
    OPENCONN;

    if (uiG.conn->type != TYPE_SERVER)
    {
        rl_print (i18n (2326, "Server side contact list only supported for ICQ v8.\n"));
        return 0;
    }

    if (!data)
    {
        if (!(par = s_parse (&args)))                  data = 0;
        else if (!strcasecmp (par->txt, "activate"))
        {
            SnacCliRosterack (uiG.conn);
            return 0;
        }
        else if (!strcasecmp (par->txt, "security") && s_parseint (&args, &i))
        {
            SnacCliSetvisibility (uiG.conn, i, 0);
            return 0;
        }
        else if (!strcasecmp (par->txt, "show"))     data = IMROSTER_SHOW;
        else if (!strcasecmp (par->txt, "diff"))     data = IMROSTER_DIFF;
        else if (!strcasecmp (par->txt, "add"))      data = IMROSTER_DOWNLOAD;
        else if (!strcasecmp (par->txt, "download")) data = IMROSTER_DOWNLOAD;
        else if (!strcasecmp (par->txt, "import"))   data = IMROSTER_IMPORT;
        else if (!strcasecmp (par->txt, "sync"))     data = IMROSTER_SYNC;
        else if (!strcasecmp (par->txt, "export"))   data = IMROSTER_EXPORT;
        else if (!strcasecmp (par->txt, "upload"))   data = IMROSTER_UPLOAD;
        else if (!strcasecmp (par->txt, "delete"))
        {
            ContactGroup *cg;
            Contact *cont;
            const char *name;
            
            if ((cg = s_parsecg (&args, uiG.conn)))
                SnacCliRosterdeletegroup (uiG.conn, cg, 3);
            else if ((cont = s_parsenick (&args, uiG.conn)))
                SnacCliRosterdeletecontact (uiG.conn, cont, 3);
            else if ((name = s_parserem (&args)))
                IMDeleteID (uiG.conn, 0, 0, name);
            return 0;
        }
        else if (!strcasecmp (par->txt, "delid"))
        {
            UDWORD tag, id;
            if (s_parseint (&args, &tag) && s_parseint (&args, &id))
                IMDeleteID (uiG.conn, tag, id, NULL);
            return 0;
        }
        else                                         data = 0;
    }
    if (data)
        IMRoster (uiG.conn, data);
    else
    {
        rl_print (i18n (2103, "contact show    Show server based contact list.\n"));
        rl_print (i18n (2104, "contact diff    Show server based contacts not on local contact list.\n"));
        rl_print (i18n (2321, "contact add     Add server based contact list to local contact list.\n"));
        rl_print (i18n (2576, "contact delete  Delete server based contact list entry.\n"));
        rl_print (i18n (2105, "contact import  Import server based contact list as local contact list.\n"));
/*        rl_print (i18n (2322, "contact sync    Import server based contact list if appropriate.\n"));
        rl_print (i18n (2323, "contact export  Export local contact list to server based list.\n"));*/
        rl_print (i18n (2324, "contact upload  Add local contacts to server based contact list.\n"));
        rl_print (i18n (2579, "contact delid   Delete server based contact list entry (experts only).\n"));
    }
    return 0;
}

/*
 * Exits climm.
 */
static JUMP_F(CmdUserQuit)
{
    Contact *cont;
    char *arg1 = NULL;
    int i;
    
    if (!(arg1 = s_parserem (&args)))
        arg1 = NULL;
                
    if (arg1)
    {
        for (i = 0; (cont = ContactIndex (NULL, i)); i++)
            if (cont->group && cont->group->serv && cont->status != ims_offline && ContactPrefVal (cont, CO_TALKEDTO))
                IMCliMsg (cont, CO_MSGTYPE, arg1, NULL);
    }

    uiG.quit = data;
    return 0;
}


/*
 * Runs a command using temporarely a different current connection
 */
static JUMP_F(CmdUserAsSession)
{
    Server *saveconn, *tmpconn;
    UDWORD quiet = 0, nr = 0;
    const char *targs;
    strc_t par;
    
    saveconn = uiG.conn;
    
    if (s_parsekey (&args, "quiet"))
        quiet = 1;

    targs = args;

    if (s_parseint (&targs, &nr) && (tmpconn = ServerNr (nr - 1)))
        args = targs;
    else if ((par = s_parse (&args)))
    {
        tmpconn = ServerFindScreen (TYPEF_ANY_SERVER, par->txt);
        if (!tmpconn && !quiet)
        {
            if (nr)
                rl_printf (i18n (2598, "There is no connection number %ld and no connection for UIN %s.\n"), UD2UL (nr), par->txt);
            else
                rl_printf (i18n (2599, "There is no connection for %s.\n"), par->txt);
        }
    }
    else
    {
        if (ServerFindNr (uiG.conn))
            tmpconn = uiG.conn;
        else
            tmpconn = ServerNr (0);
        if (!tmpconn && !quiet)
            rl_printf (i18n (2600, "No connection selected.\n"));
    }
    if (!tmpconn)
        return 0;

    if (~tmpconn->type & TYPEF_ANY_SERVER)
    {
        if (!quiet)
            rl_printf (i18n (2098, "Connection %ld is not a server connection.\n"), UD2UL (nr));
        return 0;
    }
    uiG.conn = tmpconn;
    CmdUser (args);
    uiG.conn = saveconn;
    return 0;
}

#ifdef ENABLE_DEBUG
static JUMP_F(CmdUserListQueue)
{
    QueuePrint ();
    return 0;
}
#endif

/******************************************************************************
 *
 * Now the user command function requiring a call back.
 *
 ******************************************************************************/

/*
 * Search for a user.
 */
static JUMP_F(CmdUserOldSearch)
{
    static char *email, *nick, *first, *last;
    OPENCONN;

    switch (status)
    {
        case 0:
            if (*args)
            {
                if (uiG.conn->type == TYPE_SERVER)
                    SnacCliSearchbymail (uiG.conn, args);
                
                return 0;
            }
            ReadLinePromptSet (i18n (1655, "Enter the user's e-mail address:"));
            return status = 101;
        case 101:
            email = strdup ((char *) args);
            ReadLinePromptSet (i18n (1656, "Enter the user's nick name:"));
            return ++status;
        case 102:
            nick = strdup ((char *) args);
            ReadLinePromptSet (i18n (1657, "Enter the user's first name:"));
            return ++status;
        case 103:
            first = strdup ((char *) args);
            ReadLinePromptSet (i18n (1658, "Enter the user's last name:"));
            return ++status;
        case 104:
            last = strdup ((char *) args);
            if (uiG.conn->type == TYPE_SERVER)
                SnacCliSearchbypersinf (uiG.conn, email, nick, first, last);
        case -1:
            ReadLinePromptReset ();
            free (email);
            free (nick);
            free (first);
            free (last);
            return 0;
    }
    return 0;
}

/*
 * Do a whitepage search.
 */
static JUMP_F(CmdUserSearch)
{
    int temp;
    static MetaWP wp = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    strc_t par;
    char *arg1 = NULL;
    OPENCONN;

    if (!strcmp (args, "."))
        status += 400;

    switch (status)
    {
        case 0:
            if (!(par = s_parse (&args)))
            {
                rl_print (i18n (1960, "Enter data to search user for. Enter '.' to start the search.\n"));
                ReadLinePromptSet (i18n (1656, "Enter the user's nick name:"));
                return 200;
            }
            arg1 = strdup (par->txt);
            if ((par = s_parse (&args)))
            {
                if (uiG.conn->type == TYPE_SERVER)
                    SnacCliSearchbypersinf (uiG.conn, NULL, NULL, arg1, par->txt);
            }
            else if (strchr (arg1, '@'))
            {
                if (uiG.conn->type == TYPE_SERVER)
                    SnacCliSearchbymail (uiG.conn, arg1);
            }
            else
            {
                if (uiG.conn->type == TYPE_SERVER)
                    SnacCliSearchbypersinf (uiG.conn, NULL, arg1, NULL, NULL);
            }
            free (arg1);
            return 0;
        case 200:
            wp.nick = strdup (args);
            ReadLinePromptSet (i18n (1657, "Enter the user's first name:"));
            return ++status;
        case 201:
            wp.first = strdup (args);
            ReadLinePromptSet (i18n (1658, "Enter the user's last name:"));
            return ++status;
        case 202:
            wp.last = strdup (args);
            ReadLinePromptSet (i18n (1655, "Enter the user's e-mail address:"));
            return ++status;
        case 203:
            wp.email = strdup (args);
            ReadLinePromptSet (i18n (1604, "Should the users be online?"));
            return ++status;
/* A few more could be added here, but we're gonna make this
 the last one -KK */
        case 204:
            if (strcasecmp (args, i18n (1028, "NO")) && strcasecmp (args, i18n (1027, "YES")))
            {
                rl_printf ("%s\n", i18n (1029, "Please enter YES or NO!"));
                ReadLinePromptSet (i18n (1604, "Should the users be online?"));
                return status;
            }
            wp.online = strcasecmp (args, i18n (1027, "YES")) ? FALSE : TRUE;
            ReadLinePromptSet (i18n (1936, "Enter min age (18-22,23-29,30-39,40-49,50-59,60-120):"));
            return ++status;
        case 205:
            wp.minage = atoi (args);
            ReadLinePromptSet (i18n (1937, "Enter max age (22,29,39,49,59,120):"));
            return ++status;
        case 206:
            wp.maxage = atoi (args);
            ReadLinePromptSet (i18n (1938, "Enter sex:"));
            return ++status;
        case 207:
            if (!strncasecmp (args, i18n (1528, "female"), 1))
            {
                wp.sex = 1;
            }
            else if (!strncasecmp (args, i18n (1529, "male"), 1))
            {
                wp.sex = 2;
            }
            else
            {
                wp.sex = 0;
            }
            ReadLinePromptSet (i18n (1534, "Enter a language by number or L for a list:"));
            return ++status;
        case 208:
            temp = atoi (args);
            if ((0 == temp) && (toupper (args[0]) == 'L'))
            {
                TablePrintLang ();
                ReadLinePromptSet (i18n (1534, "Enter a language by number or L for a list:"));
            }
            else
            {
                wp.language = temp;
                status++;
                ReadLinePromptSet (i18n (1939, "Enter a city:"));
            }
            return status;
        case 209:
            wp.city = strdup ((char *) args);
            ReadLinePromptSet (i18n (1602, "Enter a state:"));
            return ++status;
        case 210:
            wp.state = strdup ((char *) args);
            ReadLinePromptSet (i18n (1578, "Enter country's phone ID number:"));
            return ++status;
        case 211:
            wp.country = atoi ((char *) args);
            ReadLinePromptSet (i18n (1579, "Enter company: "));
            return ++status;
        case 212:
            wp.company = strdup ((char *) args);
            ReadLinePromptSet (i18n (1587, "Enter department: "));
            return ++status;
        case 213:
            wp.department = strdup ((char *) args);
            ReadLinePromptSet (i18n (1603, "Enter position: "));
            return ++status;
        case 214:
            wp.position = strdup ((char *) args);
        default:
            if (uiG.conn->type == TYPE_SERVER)
            {
                if (status > 250 && status <= 403)
                    SnacCliSearchbypersinf (uiG.conn, wp.email, wp.nick, wp.first, wp.last);
                else
                    SnacCliSearchwp (uiG.conn, &wp);
            }
        case -1:
            s_repl (&wp.nick,  NULL);
            s_repl (&wp.last,  NULL);
            s_repl (&wp.first, NULL);
            s_repl (&wp.email, NULL);
            s_repl (&wp.company,    NULL);
            s_repl (&wp.department, NULL);
            s_repl (&wp.position,   NULL);
            s_repl (&wp.city,  NULL);
            s_repl (&wp.state, NULL);
            return 0;
    }
}

/*
 * Update your basic user information.
 */
static JUMP_F(CmdUserUpdate)
{
    Contact *cont;
    MetaGeneral *user;
    OPENCONN;
    
    if (!(cont = uiG.conn->conn->cont))
        return 0;
    if (!(user = CONTACT_GENERAL(cont)))
        return 0;

    switch (status)
    {
        case 0:
            ReadLinePromptSet (i18n (1553, "Enter your new nickname:"));
            return 300;
        case 300:
            s_repl (&user->nick, args);
            ReadLinePromptSet (i18n (1554, "Enter your new first name:"));
            return ++status;
        case 301:
            s_repl (&user->first, args);
            ReadLinePromptSet (i18n (1555, "Enter your new last name:"));
            return ++status;
        case 302:
            s_repl (&user->last, args);
            s_repl (&user->email, NULL);
            ContactMetaD (cont->meta_email);
            cont->meta_email = NULL;
            ReadLinePromptSet (i18n (1556, "Enter your new email address:"));
            return ++status;
        case 303:
            if (!user->email && args && *args)
                user->email = strdup (args);
            else if (args && *args)
                ContactMetaAdd (&cont->meta_email, 0, args);
            else
            {
                ReadLinePromptSet (i18n (1544, "Enter new city:"));
                status += 3;
                return status;
            }
            ReadLinePromptSet (i18n (1556, "Enter your new email address:"));
            return status;
        case 306:
            user->city = strdup (args);
            ReadLinePromptSet (i18n (1545, "Enter new state:"));
            return ++status;
        case 307:
            user->state = strdup (args);
            ReadLinePromptSet (i18n (1546, "Enter new phone number:"));
            return ++status;
        case 308:
            user->phone = strdup (args);
            ReadLinePromptSet (i18n (1547, "Enter new fax number:"));
            return ++status;
        case 309:
            user->fax = strdup (args);
            ReadLinePromptSet (i18n (1548, "Enter new street address:"));
            return ++status;
        case 310:
            user->street = strdup (args);
            ReadLinePromptSet (i18n (1549, "Enter new cellular number:"));
            return ++status;
        case 311:
            user->cellular = strdup (args);
            ReadLinePromptSet (i18n (1550, "Enter new zip code (must be numeric):"));
            return ++status;
        case 312:
            user->zip = strdup (args);
            ReadLinePromptSet (i18n (1551, "Enter your country's phone ID number:"));
            return ++status;
        case 313:
            user->country = atoi (args);
            ReadLinePromptSet (i18n (1552, "Enter your time zone (+/- 0-12):"));
            return ++status;
        case 314:
            user->tz = atoi (args);
            user->tz *= 2;
            ReadLinePromptSet (i18n (1557, "Do you want to require Mirabilis users to request your authorization? (YES/NO)"));
            return ++status;
        case 315:
            if (strcasecmp (args, i18n (1028, "NO")) && strcasecmp (args, i18n (1027, "YES")))
            {
                rl_printf ("%s\n", i18n (1029, "Please enter YES or NO!"));
                ReadLinePromptSet (i18n (1557, "Do you want to require Mirabilis users to request your authorization? (YES/NO)"));
                return status;
            }
            user->auth = strcasecmp (args, i18n (1027, "YES")) ? FALSE : TRUE;
            ReadLinePromptSet (i18n (1605, "Do you want your status to be available on the web? (YES/NO)"));
            return ++status;
        case 316:
            if (strcasecmp (args, i18n (1028, "NO")) && strcasecmp (args, i18n (1027, "YES")))
            {
                rl_printf ("%s\n", i18n (1029, "Please enter YES or NO!"));
                ReadLinePromptSet (i18n (1605, "Do you want your status to be available on the web? (YES/NO)"));
                return status;
            }
            user->webaware = strcasecmp (args, i18n (1027, "YES")) ? FALSE : TRUE;
#if 0
            ReadLinePromptSet (i18n (1857, "Do you want to hide your IP from other contacts? (YES/NO)"));
            return ++status;
        case 317:
            if (strcasecmp (args, i18n (1028, "NO")) && strcasecmp (args, i18n (1027, "YES")))
            {
                rl_printf ("%s\n", i18n (1029, "Please enter YES or NO!"));
                ReadLinePromptSet (i18n (1857, "Do you want to hide your IP from other contacts? (YES/NO)"));
                return status;
            }
            user->hideip = strcasecmp (args, i18n (1027, "YES")) ? FALSE : TRUE;
#else
            ++status;
#endif
            ReadLinePromptSet (i18n (1622, "Do you want to apply these changes? (YES/NO)"));
            return ++status;
        case 318:
            if (strcasecmp (args, i18n (1028, "NO")) && strcasecmp (args, i18n (1027, "YES")))
            {
                rl_printf ("%s\n", i18n (1029, "Please enter YES or NO!"));
                ReadLinePromptSet (i18n (1622, "Do you want to apply these changes? (YES/NO)"));
                return status;
            }

            if (!strcasecmp (args, i18n (1027, "YES")))
            {
                if (uiG.conn->type == TYPE_SERVER)
                    SnacCliMetasetgeneral (uiG.conn, cont);
            }
        case -1:
            break;
    }
    return 0;
}

/*
 * Update additional information.
 */
static JUMP_F(CmdUserOther)
{
    Contact *cont;
    MetaMore *more;
    int temp;
    OPENCONN;
    
    if (!(cont = uiG.conn->conn->cont))
        return 0;
    if (!(more = CONTACT_MORE(cont)))
        return 0;

    switch (status)
    {
        case 0:
            ReadLinePromptSet (i18n (1535, "Enter new age:"));
            return 400;
        case 400:
            more->age = atoi (args);
            ReadLinePromptSet (i18n (1536, "Enter new sex:"));
            return ++status;
        case 401:
            if (!strncasecmp (args, i18n (1528, "female"), 1))
                more->sex = 1;
            else if (!strncasecmp (args, i18n (1529, "male"), 1))
                more->sex = 2;
            else
                more->sex = 0;
            ReadLinePromptSet (i18n (1537, "Enter new homepage:"));
            return ++status;
        case 402:
            s_repl (&more->homepage, args);
            ReadLinePromptSet (i18n (1538, "Enter new year of birth (4 digits):"));
            return ++status;
        case 403:
            more->year = atoi (args);
            ReadLinePromptSet (i18n (1539, "Enter new month of birth:"));
            return ++status;
        case 404:
            more->month = atoi (args);
            ReadLinePromptSet (i18n (1540, "Enter new day of birth:"));
            return ++status;
        case 405:
            more->day = atoi (args);
            ReadLinePromptSet (i18n (1534, "Enter a language by number or L for a list:"));
            return ++status;
        case 406:
            temp = atoi (args);
            if (!temp && (toupper (args[0]) == 'L'))
                TablePrintLang ();
            else
            {
                more->lang1 = temp;
                status++;
            }
            ReadLinePromptSet (i18n (1534, "Enter a language by number or L for a list:"));
            return status;
        case 407:
            temp = atoi (args);
            if (!temp && (toupper (args[0]) == 'L'))
                TablePrintLang ();
            else
            {
                more->lang2 = temp;
                status++;
            }
            ReadLinePromptSet (i18n (1534, "Enter a language by number or L for a list:"));
            return status;
        case 408:
            temp = atoi (args);
            if (!temp && (toupper (args[0]) == 'L'))
            {
                TablePrintLang ();
                ReadLinePromptSet (i18n (1534, "Enter a language by number or L for a list:"));
                return status;
            }
            more->lang3 = temp;
            if (uiG.conn->type == TYPE_SERVER)
                SnacCliMetasetmore (uiG.conn, cont);
        case -1:
            break;
    }
    return 0;
}

/*
 * Update about information.
 */
static JUMP_F(CmdUserAbout)
{
    static int offset = 0;
    static char msg[1024];
    char *arg;
    OPENCONN;

    switch (status)
    {
        case 400:
            msg[offset] = 0;
            if (!strcmp (args, END_MSG_STR))
            {
                if (uiG.conn->type == TYPE_SERVER)
                    SnacCliMetasetabout (uiG.conn, msg);
            }
            else if (strcmp (args, CANCEL_MSG_STR))
            {
                if (offset + strlen (args) < 450)
                {
                    strcat (msg, args);
                    strcat (msg, "\r\n");
                    offset += strlen (args) + 2;
                    break;
                }
                else
                {
                    if (uiG.conn->type == TYPE_SERVER)
                        SnacCliMetasetabout (uiG.conn, msg);
                }
            }
        case -1:
            return 0;
        case 0:
            if ((arg = s_parserem (&args)))
            {
                if (uiG.conn->type == TYPE_SERVER)
                    SnacCliMetasetabout (uiG.conn, arg);
                return 0;
            }
            offset = 0;
    }
    ReadLinePromptSet (i18n (2455, "About>"));
    return 400;
}

/*
 * executes queued commands
 */
void CmdUserCallbackTodo (Event *event)
{
    Server *tconn;
    const char *args;
    strc_t par;
    
    if (event && event->conn && event->conn->serv && event->conn == event->conn->serv->conn
        && OptGetStr (event->opt, CO_CLIMMCOMMAND, &args))
    {
        tconn = uiG.conn;
        uiG.conn = event->conn->serv;
        while ((par = s_parse (&args)))
        {
            char *cmd = strdup (par->txt);
            CmdUser (cmd);
            s_free (cmd);
        }
        uiG.conn = tconn;
    }
    EventD (event);
}

/*
 * Process one command, but ignore idle times.
 */
void CmdUser (const char *command)
{
    time_t a;
    idleflag_t b;
    CmdUserProcess (command, &a, &b);
}

/*
 * Get one line of input and process it.
 */
void CmdUserInput (strc_t line)
{
    idleflag_t i = i_idle;
    CmdUserProcess (line->txt, &uiG.idle_val, uiG.conn ? &uiG.conn->idle_flag : &i);
}

static UBYTE isinterrupted = 0;

void CmdUserInterrupt (void)
{
    isinterrupted = 1;
}

/*
 * Process one line of command.
 */
static void CmdUserProcess (const char *command, time_t *idle_val, idleflag_t *idle_flag)
{
    char *cmd = NULL;
    const char *args, *argst;
    strc_t par;

    static jump_f *sticky = (jump_f *)NULL;
    static int status = 0;

    time_t idle_save;
    idle_save = *idle_val;
    *idle_val = time (NULL);

    rl_print ("\r");

    if (!uiG.conn || !(ServerFindNr (uiG.conn) + 1))
        uiG.conn = ServerNr (0);

    if (isinterrupted)
    {
        if (status)
            status = (*sticky)(command, 0, -1);
        isinterrupted = 0;
    }
    
    if (status)
    {
        status = (*sticky)(command, 0, status);
    }
    else
    {
        args = command;
        if (*args)
        {
            if ('!' == args[0])
            {
                size_t rc;
                ReadLineTtyUnset ();
#ifdef SHELL_COMMANDS
                if (((unsigned char)args[1] < 31) || (args[1] & 128))
                    rl_printf (i18n (1660, "Invalid Command: %s\n"), args + 1);
                else
                    rc = system (args + 1);
#else
                rl_print (i18n (1661, "Shell commands have been disabled.\n"));
#endif
                ReadLineTtySet ();
                if (!command)
                    ReadLinePromptReset ();
                return;
            }
            argst = AliasExpand (args, 0, 0);
            args = argst ? argst : args;
            if (!(par = s_parse (&args)))
            {
                if (!command)
                    ReadLinePromptReset ();
                return;
            }
            cmd = par->txt;

            /* skip all non-alphanumeric chars on the beginning
             * to accept IRC like commands starting with a /
             * or talker like commands starting with a .
             * or whatever */
            if (strchr ("/.\\", *cmd))
                cmd++;

            if (!*cmd)
            {
                if (!command)
                    ReadLinePromptReset ();
                return;
            }

            else if (!strcasecmp (cmd, "ver"))
            {
                rl_print (BuildVersion ());
                rl_print (BuildVersionText);
                rl_print ("\n");
            }
            else
            {
                char *argsd;
                jump_t *j = (jump_t *)NULL;
                
                cmd = strdup (cmd);
                argsd = strdup (args);

                if ((j = CmdUserLookup (cmd)))
                {
                    if (j->unidle == 2)
                        *idle_val = idle_save;
                    else if (j->unidle == 1)
                    {
                        *idle_flag = i_idle;
                        *idle_val = 0;
                    }
                    status = j->f (argsd, j->data, 0);
                    sticky = j->f;
                }
                else
                    rl_printf (i18n (2456, "Unknown command %s, type %shelp%s for help.\n"),
                              s_qquote (cmd), COLQUOTE, COLNONE);
                free (cmd);
                free (argsd);
            }
        }
    }
    if (!status && !uiG.quit)
        ReadLinePromptReset ();
    ReadLineAllowTab (!status);
}
