/*
 * Handles commands the user sends.
 *
 * Copyright: This file may be distributed under version 2 of the GPL licence.
 *
 * alias stuff GPL >= v2 Copyright Per Olofsson
 * history & find command Copyright Sebastian Felis
 *
 * $Id$
 */

#include "micq.h"

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
#include "oldicq_compat.h"
#include "oldicq_client.h"
#include "oscar_base.h"
#include "oscar_snac.h"
#include "oscar_service.h"
#include "oscar_contact.h"
#include "oscar_icbm.h"
#include "oscar_bos.h"
#include "oscar_roster.h"
#include "oscar_oldicq.h"
#include "im_icq8.h"
#include "preferences.h"
#include "packet.h"
#include "tabs.h"
#include "connection.h"
#include "tcp.h"
#include "icq_response.h"
#include "conv.h"
#include "peer_file.h"
#include "file_util.h"
#include "buildmark.h"
#include "contact.h"
#include "server.h"
#include "util_tcl.h"
#include "util_ssl.h"

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
    CmdUserAnyMess, CmdUserGetAuto, CmdUserOpt;

#ifdef ENABLE_PEER2PEER
static jump_f CmdUserPeer;
#endif

#ifdef ENABLE_DEBUG
static jump_f CmdUserListQueue;
#endif
    
static void CmdUserProcess (const char *command, time_t *idle_val, UBYTE *idle_flag);

static alias_t *CmdUserLookupAlias (const char *name);
static alias_t *CmdUserSetAlias (const char *name, const char *expansion);
static int CmdUserRemoveAlias (const char *name);

/* 1 = do not apply idle stuff next time           v
   2 = count this line as being idle               v */
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
    { &CmdUserMessage,       "r",            0,   2 },
    { &CmdUserMessage,       "a",            0,   4 },
    { &CmdUserGetAuto,       "getauto",      0,   0 },
    { &CmdUserResend,        "resend",       0,   0 },
    { &CmdUserVerbose,       "verbose",      0,   0 },
    { &CmdUserIgnoreStatus,  "i",            0,   0 },
    { &CmdUserStatusDetail,  "status",       2,     2     + 8 },
    { &CmdUserStatusDetail,  "ww",           2,     2 },
    { &CmdUserStatusDetail,  "ee",           2, 1 + 2 },
    { &CmdUserStatusDetail,  "w",            2,         4 },
    { &CmdUserStatusDetail,  "e",            2, 1     + 4 },
    { &CmdUserStatusDetail,  "wwg",          2,     2              + 32 },
    { &CmdUserStatusDetail,  "eeg",          2, 1 + 2              + 32 },
    { &CmdUserStatusDetail,  "wg",           2,         4          + 32 },
    { &CmdUserStatusDetail,  "eg",           2, 1     + 4          + 32 },
    { &CmdUserStatusDetail,  "s",            2,     2 + 4 + 8 + 16 },
    { &CmdUserStatusDetail,  "wwv",          2,     2                   + 64 },
    { &CmdUserStatusDetail,  "eev",          2, 1 + 2                   + 64 },
    { &CmdUserStatusDetail,  "wv",           2,         4               + 64 },
    { &CmdUserStatusDetail,  "ev",           2, 1     + 4               + 64 },
    { &CmdUserStatusDetail,  "wwgv",         2,     2              + 32 + 64 },
    { &CmdUserStatusDetail,  "eegv",         2, 1 + 2              + 32 + 64 },
    { &CmdUserStatusDetail,  "wgv",          2,         4          + 32 + 64 },
    { &CmdUserStatusDetail,  "egv",          2, 1     + 4          + 32 + 64 },
    { &CmdUserStatusDetail,  "sv",           2,     2 + 4 + 8 + 16      + 64 },
    { &CmdUserStatusDetail,  "_s",           2,   0 },
    { &CmdUserStatusMeta,    "ss",           2,   1 },
    { &CmdUserStatusMeta,    "meta",         2,   0 },
    { &CmdUserStatusWide,    "wide",         2,   1 },
    { &CmdUserStatusWide,    "ewide",        2,   0 },
    { &CmdUserSet,           "set",          0,   0 },
    { &CmdUserOpt,           "opt",          0,   0 },
    { &CmdUserOpt,           "optglobal",    0, COF_GLOBAL  },
    { &CmdUserOpt,           "optconnection",0, COF_GLOBAL | COF_GROUP },
    { &CmdUserOpt,           "optgroup",     0, COF_GROUP   },
    { &CmdUserOpt,           "optcontact",   0, COF_CONTACT },
    { &CmdUserSound,         "sound",        2,   0 },
    { &CmdUserSoundOnline,   "soundonline",  2,   0 },
    { &CmdUserSoundOffline,  "soundoffline", 2,   0 },
    { &CmdUserAutoaway,      "autoaway",     2,   0 },
    { &CmdUserChange,        "change",       1,  -1 },
    { &CmdUserChange,        "online",       1, STATUS_ONLINE },
    { &CmdUserChange,        "away",         1, STATUS_AWAY },
    { &CmdUserChange,        "na",           1, STATUS_NA   },
    { &CmdUserChange,        "occ",          1, STATUS_OCC  },
    { &CmdUserChange,        "dnd",          1, STATUS_DND  },
    { &CmdUserChange,        "ffc",          1, STATUS_FFC  },
    { &CmdUserChange,        "inv",          1, STATUS_INV  },
    { &CmdUserClear,         "clear",        2,   0 },
    { &CmdUserTogIgnore,     "togig",        0,   0 },
    { &CmdUserTogVisible,    "togvis",       0,   0 },
    { &CmdUserTogInvis,      "toginv",       0,   0 },
    { &CmdUserAdd,           "add",          0,   0 },
    { &CmdUserAdd,           "addalias",     0,   1 },
    { &CmdUserAdd,           "addgroup",     0,   2 },
    { &CmdUserRemove,        "rem",          0,   0 },
    { &CmdUserRemove,        "remalias",     0,   1 },
    { &CmdUserRemove,        "remgroup",     0,   2 },
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
    { &CmdUserSMS,           "sms",          0,   0 },
    { &CmdUserPeek,          "peek",         0,   0 },
    { &CmdUserAsSession,     "as",           0,   0 },
    { &CmdUserContact,       "contact",      0,   0 },
                                                                
    { &CmdUserOldSearch,     "oldsearch",    0,   0 },
    { &CmdUserSearch,        "search",       0,   0 },
    { &CmdUserUpdate,        "update",       0,   0 },
    { &CmdUserOther,         "other",        0,   0 },
    { &CmdUserAbout,         "about",        0,   0 },
    { &CmdUserConn,          "conn",         0,   0 },
    { &CmdUserConn,          "login",        0,   2 },
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
    { NULL, NULL, 0, 0 }
};

static alias_t *aliases = NULL;

static Connection *conn = NULL;

/* Have an opened server connection ready. */
#define OPENCONN \
    if (!conn) conn = ConnectionFind (TYPEF_ANY_SERVER, 0, NULL); \
    if (!conn || ~conn->connect & CONNECT_OK) \
    { rl_print (i18n (1931, "Current session is closed. Try another or open this one.\n")); return 0; } \

/* Try to have any server connection ready. */
#define ANYCONN if (!conn) conn = ConnectionFind (TYPEF_ANY_SERVER, 0, NULL); \
    if (!conn) { rl_print (i18n (2397, "No server session found.\n")); return 0; }

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
 * Returns the alias list.
 */
alias_t *CmdUserAliases (void)
{
    return aliases;
}

/*
 * Looks up an alias.
 */
static alias_t *CmdUserLookupAlias (const char *name)
{
    alias_t *node;

    for (node = aliases; node; node = node->next)
        if (!strcasecmp (name, node->name))
            break;

    return node;
}

/*
 * Sets an alias.
 */
static alias_t *CmdUserSetAlias (const char *name, const char *expansion)
{
    alias_t *alias;

    alias = CmdUserLookupAlias (name);

    if (!alias)
    {
        alias = calloc (1, sizeof (alias_t));
        
        if (aliases)
        {
            alias_t *node;
            
            for (node = aliases; node->next; node = node->next)
                ;

            node->next = alias;
        }
        else
            aliases = alias;
    }

    s_repl (&alias->name, name);
    s_repl (&alias->expansion, expansion);

    return alias;
}

/*
 * Removes an alias.
 */
static int CmdUserRemoveAlias (const char *name)
{
    alias_t *node, *prev_node = NULL;

    for (node = aliases; node; node = node->next)
    {
        if (!strcasecmp (name, node->name))
            break;
        prev_node = node;
    }

    if (node)
    {
        if (prev_node)
            prev_node->next = node->next;
        else
            aliases = node->next;
        free (node->name);
        free (node->expansion);
        free (node);
        return TRUE;
    }
    else
        return FALSE;
}

/*
 * Change status.
 */
static JUMP_F(CmdUserChange)
{
    char *arg1 = NULL;
    ANYCONN;

    if (data + 1 == 0)
    {
        if (!s_parseint (&args, &data))
        {
            rl_print (i18n (1703, "Status modes:\n"));
            rl_printf ("  %-20s %d\n", i18n (1921, "Online"),         STATUS_ONLINE);
            rl_printf ("  %-20s %d\n", i18n (1923, "Away"),           STATUS_AWAY);
            rl_printf ("  %-20s %d\n", i18n (1922, "Do not disturb"), STATUS_DND);
            rl_printf ("  %-20s %d\n", i18n (1924, "Not Available"),  STATUS_NA);
            rl_printf ("  %-20s %d\n", i18n (1927, "Free for chat"),  STATUS_FFC);
            rl_printf ("  %-20s %d\n", i18n (1925, "Occupied"),       STATUS_OCC);
            rl_printf ("  %-20s %d\n", i18n (1926, "Invisible"),      STATUS_INV);
            return 0;
        }
    }

    if (!(arg1 = s_parserem (&args)))
        arg1 = "";

    if      (data & STATUSF_DND)  OptSetStr (&prG->copts, CO_TAUTODND,  arg1);
    else if (data & STATUSF_OCC)  OptSetStr (&prG->copts, CO_TAUTOOCC,  arg1);
    else if (data & STATUSF_NA)   OptSetStr (&prG->copts, CO_TAUTONA,   arg1);
    else if (data & STATUSF_AWAY) OptSetStr (&prG->copts, CO_TAUTOAWAY, arg1);
    else if (data & STATUSF_FFC)  OptSetStr (&prG->copts, CO_TAUTOFFC,  arg1);
    else if (data & STATUSF_INV)  OptSetStr (&prG->copts, CO_TAUTOINV,  arg1);

    if (~conn->connect & CONNECT_OK)
        conn->status = data;
    else if (conn->type == TYPE_SERVER)
        SnacCliSetstatus (conn, data, 1);
    else
    {
        CmdPktCmdStatusChange (conn, data);
        rl_printf ("%s %s\n", s_now, s_status (conn->status));
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
        rl_printf ("  %2d %s\n", 49, i18n (1715, "mICQ"));
    }
    else
        IMCliInfo (conn, NULL, arg1);
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
        rl_printf ("  %2d %s\n", conn->version > 6 ? 0 : -1, i18n (1716, "None"));
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
        rl_printf ("  %2d %s\n", 49, i18n (1715, "mICQ"));
    }
    else
    {
        if (conn->type == TYPE_SERVER)
            SnacCliSetrandom (conn, arg1);
        else
            CmdPktCmdRandSet (conn, arg1);
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
        "msg, a, r, url, sms, getauto, auth, resend, last, h = history, historyd, find, finds, tabs", NULL },
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
        _i18n (1443, "Commands relating to mICQ displays and configuration."),
        "verbose, clear, sound, autoaway, auto, alias, unalias, lang, uptime, set, opt, optcontact, optgroup, optconnection, optglobal, save, q = quit = exit, x, !", NULL },
      { _i18n (2171, "Advanced"), "advanced",
        _i18n (2172, "Advanced commands."),
        "meta, conn, peer, file, accept, contact, peek, as",
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
                rl_printf (i18n (-1, myhelp[i].warn));
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
            CMD_USER_HELP  ("historyd <contact> [<date> [<count>]]", i18n (2395, "View your messages of <contact> since <date>."));
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
            CMD_USER_HELP  ("online [<away-message>]", i18n (1431, "Set status to \"online\"."));
        else if (!strcasecmp (par->txt, "away"))
            CMD_USER_HELP  ("away [<away-message>]", i18n (1432, "Set status to \"away\"."));
        else if (!strcasecmp (par->txt, "na"))
            CMD_USER_HELP  ("na [<away-message>]", i18n (1433, "Set status to \"not available\"."));
        else if (!strcasecmp (par->txt, "occ"))
            CMD_USER_HELP  ("occ [<away-message>]", i18n (1434, "Set status to \"occupied\"."));
        else if (!strcasecmp (par->txt, "dnd"))
            CMD_USER_HELP  ("dnd [<away-message>]", i18n (1435, "Set status to \"do not disturb\"."));
        else if (!strcasecmp (par->txt, "ffc"))
            CMD_USER_HELP  ("ffc [<away-message>]", i18n (1436, "Set status to \"free for chat\"."));
        else if (!strcasecmp (par->txt, "inv"))
            CMD_USER_HELP  ("inv [<away-message>]", i18n (1437, "Set status to \"invisible\"."));
        else if (!strcasecmp (par->txt, "change"))
            CMD_USER_HELP  ("change <status> [<away-message>]", i18n (1427, "Changes your status to the status number, or list the available modes."));
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
            CMD_USER_HELP  ("alias [<alias> [<expansion>]]", i18n (2300, "Set an alias or list current aliases."));
        else if (!strcasecmp (par->txt, "unalias"))
            CMD_USER_HELP  ("unalias <alias>", i18n (2301, "Delete an alias."));
        else if (!strcasecmp (par->txt, "lang"))
            CMD_USER_HELP  ("lang [<lang|nr>...]", i18n (1800, "Change the working language (and encoding) to <lang> or display string <nr>."));
        else if (!strcasecmp (par->txt, "uptime"))
            CMD_USER_HELP  ("uptime", i18n (1719, "Shows how long mICQ has been running and some statistics."));
        else if (!strcasecmp (par->txt, "set"))
            CMD_USER_HELP  ("set <option> <value>", i18n (2044, "Set, clear or display an <option>: color, delbs, auto, uinprompt, autosave, autofinger, linebreak, funny."));
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
            CMD_USER_HELP  ("q", i18n (1422, "Logs off and quits."));
        else if (!strcasecmp (par->txt, "quit"))
            CMD_USER_HELP  ("quit", i18n (1422, "Logs off and quits."));
        else if (!strcasecmp (par->txt, "exit"))
            CMD_USER_HELP  ("exit", i18n (1422, "Logs off and quits."));
        else if (!strcasecmp (par->txt, "x"))
            CMD_USER_HELP  ("x", i18n (2561, "Logs off and quits without saving."));
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
            CMD_USER_HELP  ("contact [show|diff|download|upload]", i18n (2306, "Request server side contact list and show all or new contacts or import."));
        else if (!strcasecmp (par->txt, "peek"))
            CMD_USER_HELP  ("peek <contacts>", i18n (2183, "Check all <contacts> whether they are offline or invisible."));
        else if (!strcasecmp (par->txt, "as"))
            CMD_USER_HELP  ("as <nr|uin> <cmd>", i18n (2562, "Execute <cmd> with connection <nr> or connection for uin <uin> as current server connection."));
#ifdef ENABLE_TCL
        /* Scripting */
        else if (!strcasecmp (par->txt, "tclscript"))
            CMD_USER_HELP  ("tclscript <file>", i18n (2344, "Execute Tcl script from <file>."));
        else if (!strcasecmp (par->txt, "tcl"))
            CMD_USER_HELP  ("tcl <string>", i18n (2345, "Execute Tcl script in <string>."));
#endif
        else if (strcmp (par->txt, "=") && strcmp (par->txt, "..."))
            rl_printf (i18n (2546, "No help available for '%s'\n"), par->txt);
    } while ((par = s_parse_s (&args, " \t\r\n,")));
    s_free (newargs);
    return 0;
}

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
        if (conn->type == TYPE_SERVER)
            SnacCliMetasetpass (conn, arg1);
        else
            CmdPktCmdMetaPass (conn, arg1);
        conn->passwd = strdup (arg1);
        if (conn->pref_passwd && strlen (conn->pref_passwd))
        {
            rl_print (i18n (2122, "Note: You need to 'save' to write new password to disc.\n"));
            conn->pref_passwd = strdup (arg1);
        }
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
    
    if (conn->type != TYPE_SERVER)
    {
        rl_print (i18n (2013, "This command is v8 only.\n"));
        return 0;
    }
    if ((cont = s_parsenick (&args, conn)))
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
        SnacCliSendsms (conn, cellv, text);
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
            IMCliInfo (conn, cont, 0);
        return 0;
    }

    if ((cg = s_parselistrem (&args, conn)))
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
            IMCliInfo (conn, cont, 0);
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
    
    if (conn->type != TYPE_SERVER)
    {
        rl_print (i18n (2013, "This command is v8 only.\n"));
        return 0;
    }
    
    if ((cg = s_parselistrem (&args, conn)))
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
            SnacCliSendmsg2 (conn, cont, OptSetVals (NULL, CO_MSGTYPE, MSG_GET_PEEK, CO_MSGTEXT, "", 0));
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
            rl_printf ("%3ld:%s\n", j, i18n (j, i18n (1078, "No translation available.")));
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
                    rl_printf (i18n (2400, "No translation (%s) loaded (%d entries).\n"), par->txt, i);
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
            
            /* i18n (1079, "Translation (%s, %s) from %s, last modified on %s by %s, for mICQ %d.%d.%d%s.\n") */
            rl_printf (i18n (-1, s),
                     i18n (1001, "<lang>"), i18n (1002, "<lang_cc>"), i18n (1004, "<translation authors>"),
                     i18n (1006, "<last edit date>"), i18n (1005, "<last editor>"),
                     v1, v2, v3, v4 ? s_sprintf (".%ld", v4) : "");
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

    if ((cg = s_parselistrem (&args, conn)))
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
        {
            int cdata;
            
            one = 1;
            if (!(cdata = data))
            {
                if      (cont->status == STATUS_OFFLINE) continue;
                else if (cont->status  & STATUSF_DND)    cdata = MSGF_GETAUTO | MSG_GET_DND;
                else if (cont->status  & STATUSF_OCC)    cdata = MSGF_GETAUTO | MSG_GET_OCC;
                else if (cont->status  & STATUSF_NA)     cdata = MSGF_GETAUTO | MSG_GET_NA;
                else if (cont->status  & STATUSF_AWAY)   cdata = MSGF_GETAUTO | MSG_GET_AWAY;
                else if (cont->status  & STATUSF_FFC)    cdata = MSGF_GETAUTO | MSG_GET_FFC;
                else continue;
            }
            IMCliMsg (conn, cont, OptSetVals (NULL, CO_MSGTYPE, cdata, 0));
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
    Connection *list;
    UDWORD seq;
    char *reason;
    int i;
    ANYCONN;

    if (!conn || !(list = conn->assoc))
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
            rl_print (i18n (2378, "peer ssl <nick>   - initiate SSL handshake."));
#endif
            return 0;
        }
    }
    if ((cg = s_parselist (&args, conn)) || (data & 16))
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
                    PeerFileUser (seq, NULL, reason, conn);
                else
                    for (i = 0; (cont = ContactIndex (cg, i)); i++)
                        PeerFileUser (seq, cont, reason, conn);
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
    char *exp = NULL, *nname;

    if (!(name = s_parse_s (&args, " \t\r\n=")))
    {
        alias_t *node;

        for (node = aliases; node; node = node->next)
            rl_printf ("alias %s %s\n", node->name, node->expansion);
        
        return 0;
    }
    
    if ((exp = strpbrk (name->txt, " \t\r\n")))
    {
        rl_printf (i18n (2303, "Invalid character 0x%x in alias name.\n"), *exp);
        return 0;
    }

    if (*args == '=')
        args++;
    
    nname = strdup (name->txt);
    
    if ((exp = s_parserem (&args)))
        CmdUserSetAlias (nname, exp);
    else
    {
        alias_t *node;

        for (node = aliases; node; node = node->next)
            if (!strcasecmp (node->name, nname))
            {
                rl_printf ("alias %s %s\n", node->name, node->expansion);
                free (nname);
                return 0;
            }
        rl_print (i18n (2297, "Alias to what?\n"));
    }
    free (nname);
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
    else if (!CmdUserRemoveAlias (par->txt))
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

    if ((cg = s_parselistrem (&args, conn)))
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
        {
            IMCliMsg (conn, cont, OptSetVals (NULL, CO_MSGTYPE, uiG.last_message_sent_type, CO_MSGTEXT, uiG.last_message_sent, 0));
            uiG.last_sent = cont;
        }
    else
        IMCliMsg (conn, uiG.last_sent, OptSetVals (NULL, CO_MSGTYPE, uiG.last_message_sent_type, CO_MSGTEXT, uiG.last_message_sent, 0));
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

    if (!(cont = s_parsenick (&args, conn)))
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
        if (!conn->assoc || !TCPDirectOpen  (conn->assoc, cont))
        {
            rl_printf (i18n (2142, "Direct connection with %s not possible.\n"), cont->nick);
            return 0;
        }
        PeerSendMsg (conn->assoc, cont, OptSetVals (NULL, CO_MSGTYPE, data >> 2, CO_MSGTEXT, t.txt, 0));
#endif
    }
    else
    {
        if (conn->type != TYPE_SERVER)
            CmdPktCmdSendMessage (conn, cont, t.txt, data >> 2);
        else if (f != 2)
            SnacCliSendmsg (conn, cont, t.txt, data >> 2, f);
        else
            SnacCliSendmsg2 (conn, cont, OptSetVals (NULL, CO_FORCE, 1, CO_MSGTYPE, data >> 2, CO_MSGTEXT, t.txt, 0));
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

        switch (data)
        {
            case 1:
                if (!(cg = s_parselist (&args, conn)))
                    return 0;
                tcg = ContactGroupC (NULL, 0, NULL);
                for (i = 0; (cont = ContactIndex (cg, i)); i++)
                    ContactAdd (tcg, cont);
                cg = tcg;
                break;
            case 2:
                if (!uiG.last_rcvd)
                {
                    rl_print (i18n (1741, "Must receive a message first.\n"));
                    return 0;
                }
                cg = ContactGroupC (NULL, 0, NULL);
                ContactAdd (cg, uiG.last_rcvd);
                break;
            case 4:
                if (!uiG.last_sent)
                {
                    rl_print (i18n (1742, "Must write a message first.\n"));
                    return 0;
                }
                cg = ContactGroupC (NULL, 0, NULL);
                ContactAdd (cg, uiG.last_sent);
                break;
            default:
                assert (0);
        }
        if (!(arg1 = s_parserem (&args)))
            arg1 = NULL;
        if (arg1 && (arg1[strlen (arg1) - 1] != '\\'))
        {
            s_repl (&uiG.last_message_sent, arg1);
            uiG.last_message_sent_type = MSG_NORM;
            for (i = 0; (cont = ContactIndex (cg, i)); i++)
            {
                IMCliMsg (conn, cont, OptSetVals (NULL, CO_MSGTYPE, MSG_NORM, CO_MSGTEXT, arg1, 0));
                uiG.last_sent = cont;
                if (tab)
                    TabAddOut (cont);
            }
            ContactGroupD (cg);
            return 0;
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
        status = 1;
    }
    else if (status == -1 || !strcmp (args, CANCEL_MSG_STR))
    {
        rl_print (i18n (1038, "Message canceled.\n"));
        ReadLinePromptReset ();
        ContactGroupD (cg);
        return 0;
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
            IMCliMsg (conn, cont, OptSetVals (NULL, CO_MSGTYPE, MSG_NORM, CO_MSGTEXT, t.txt, 0));
            uiG.last_sent = cont;
            if (tab)
                TabAddOut (cont);
        }
        ReadLinePromptReset ();
        ContactGroupD (cg);
        return 0;
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
    UDWORD i = 0;

    if (s_parseint (&args, &i))
        prG->verbose = i;
    else if (*args)
    {
        rl_printf (i18n (2115, "'%s' is not an integer.\n"), args);
        return 0;
    }
    rl_printf (i18n (1060, "Verbosity level is %ld.\n"), prG->verbose);
    return 0;
}

static UDWORD __status (Contact *cont)
{
    if (ContactPrefVal (cont, CO_IGNORE))   return 0xfffffffe;
    if (!cont->group)                       return 0xfffffffe;
    if (cont->status == STATUS_OFFLINE)     return STATUS_OFFLINE;
    if (cont->status  & STATUSF_BIRTH)      return STATUSF_BIRTH;
    if (cont->status  & STATUSF_DND)        return STATUS_DND;
    if (cont->status  & STATUSF_OCC)        return STATUS_OCC;
    if (cont->status  & STATUSF_NA)         return STATUS_NA;
    if (cont->status  & STATUSF_AWAY)       return STATUS_AWAY;
    if (cont->status  & STATUSF_FFC)        return STATUS_FFC;
    return STATUS_ONLINE;
}

static UDWORD __tuin, __lenuin, __lennick, __lenstat, __lenid, __totallen, __l;

static void __initcheck (void)
{
    __tuin = __lenuin = __lennick = __lenstat = __lenid = __l = 0;
}

static void __checkcontact (Contact *cont, UWORD data)
{
    ContactAlias *alias;

    if (cont->uin > __tuin)
        __tuin = cont->uin;
    if (s_strlen (cont->nick) > __lennick)
        __lennick = s_strlen (cont->nick);
    if (data & 2)
        for (alias = cont->alias; alias; alias = alias->more)
            if (s_strlen (alias->alias) > __lennick)
                __lennick = s_strlen (alias->alias);
    if (s_strlen (s_status (cont->status)) > __lenstat)
        __lenstat = s_strlen (s_status (cont->status));
    if (cont->version && s_strlen (cont->version) > __lenid)
        __lenid = s_strlen (cont->version);
}

static void __donecheck (UWORD data)
{
    if (__lennick > (UDWORD)uiG.nick_len)
        uiG.nick_len = __lennick;
    while (__tuin)
        __lenuin++, __tuin /= 10;
    __totallen = 1 + __lennick + 1 + __lenstat + 3 + __lenid + 2;
    if (prG->verbose)
        __totallen += 29;
    if (data & 2)
        __totallen += 2 + 3 + 2 + 1 + __lenuin + 19;

}

static void __showcontact (Connection *conn, Contact *cont, UWORD data)
{
    Connection *peer;
    ContactAlias *alias;
    char *stat, *ver = NULL, *ver2 = NULL;
    const char *ul = "";
    time_t tseen;
#ifdef WIP
    time_t tmicq;
#endif
    char tbuf[100];
    
    peer = (conn && conn->assoc) ? ConnectionFind (TYPE_MSGDIRECT, cont, conn->assoc) : NULL;
    
    stat = strdup (s_sprintf ("(%s)", s_status (cont->status)));
    if (cont->version)
        ver  = strdup (s_sprintf ("[%s]", cont->version));
    if (prG->verbose && cont->dc)
        ver2 = strdup (s_sprintf (" <%08x:%08x:%08x>", (unsigned int)cont->dc->id1,
                                   (unsigned int)cont->dc->id2, (unsigned int)cont->dc->id3));
    if (!OptGetVal (&cont->copts, cont->status == STATUS_OFFLINE ? CO_TIMESEEN : CO_TIMEONLINE, &tseen))
        tseen = (time_t)-1;
#ifdef WIP
    if (!OptGetVal (&cont->copts, CO_TIMEMICQ, &tmicq))
        tmicq = (time_t)-1;
#endif

    if (tseen != (time_t)-1 && data & 2)
        strftime (tbuf, sizeof (tbuf), " %Y-%m-%d %H:%M:%S", localtime (&tseen));
    else if (data & 2)
        strcpy (tbuf, "                    ");
    else
        tbuf[0] = '\0';

#ifdef WIP
    if (tmicq != (time_t)-1 && data & 2)
        strftime (tbuf + strlen (tbuf), sizeof (tbuf) - strlen (tbuf),
                  " %Y-%m-%d %H:%M:%S", localtime (&tmicq));
#endif
    
#ifdef CONFIG_UNDERLINE
    if (!(++__l % 5))
        ul = ESC "[4m";
#endif
    if (data & 2)
        rl_printf ("%s%s%c%c%c%2d%c%c%s%s %*ld", COLSERVER, ul,
             !cont->group                        ? '#' : ' ',
             ContactPrefVal (cont,  CO_INTIMATE) ? '*' :
              ContactPrefVal (cont, CO_HIDEFROM) ? '-' : ' ',
             ContactPrefVal (cont,  CO_IGNORE)   ? '^' : ' ',
             cont->dc ? cont->dc->version : 0,
             ContactPrefVal (cont, CO_WANTSBL)  ? 
               (ContactPrefVal (cont, CO_ISSBL) ? 'S' : '.') :
                ContactPrefVal (cont, CO_ISSBL) ? 's' : ' ',
             peer ? (
#ifdef ENABLE_SSL
              peer->connect & CONNECT_OK && peer->ssl_status == SSL_STATUS_OK ? '%' :
#endif
              peer->connect & CONNECT_OK         ? '&' :
              peer->connect & CONNECT_FAIL       ? '|' :
              peer->connect & CONNECT_MASK       ? ':' : '.' ) :
              cont->dc && cont->dc->version && cont->dc->port && ~cont->dc->port &&
              cont->dc->ip_rem && ~cont->dc->ip_rem ? '^' : ' ',
             COLNONE, ul, (int)__lenuin, cont->uin);

    rl_printf ("%s%s%c%s%s%-*s%s%s %s%s%-*s%s%s %-*s%s%s%s\n",
             COLSERVER, ul, data & 2                       ? ' ' :
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
             COLCONTACT, ul, (int)__lennick + s_delta (cont->nick), cont->nick,
             COLNONE, ul, COLQUOTE, ul, (int)__lenstat + 2 + s_delta (stat), stat,
             COLNONE, ul, (int)__lenid + 2 + s_delta (ver ? ver : ""), ver ? ver : "",
             ver2 ? ver2 : "", tbuf, COLNONE);

    for (alias = cont->alias; alias && (data & 2); alias = alias->more)
    {
        rl_printf ("%s%s+      %s%*ld", COLSERVER, ul, COLNONE, (int)__lenuin, cont->uin);
        rl_printf ("%s%s %s%s%-*s%s%s %s%s%-*s%s%s %-*s%s%s%s\n",
                  COLSERVER, ul, COLCONTACT, ul, (int)__lennick + s_delta (alias->alias), alias->alias,
                  COLNONE, ul, COLQUOTE, ul, (int)__lenstat + 2 + s_delta (stat), stat,
                  COLNONE, ul, (int)__lenid + 2 + s_delta (ver ? ver : ""), ver ? ver : "",
                  ver2 ? ver2 : "", tbuf, COLNONE);
    }
    free (stat);
    s_free (ver);
    s_free (ver2);
}

/*
 * Shows the contact list in a very detailed way.
 *
 * data & 64: also show shadowed contacts
 * data & 32: sort by groups
 * data & 16: show _only_ own status
 * data & 8: show only given nicks
 * data & 4: show own status
 * data & 2: be verbose
 * data & 1: do not show offline
 */
static JUMP_F(CmdUserStatusDetail)
{
    ContactGroup *cg = NULL, *tcg = NULL;
    Contact *cont = NULL;
    int i, j, k;
    UDWORD stati[] = { 0xfffffffe, STATUS_OFFLINE, STATUS_DND,    STATUS_OCC, STATUS_NA,
                                   STATUS_AWAY,    STATUS_ONLINE, STATUS_FFC, STATUSF_BIRTH };
    ANYCONN;

    if (!data)
        s_parseint (&args, &data);
    
    if (~data & 16 && !(cg = s_parsecg (&args, conn)))
        tcg = cg = NULL;

    if ((data & 8) && !cg)
        tcg = cg = s_parselistrem (&args, conn);

    if (tcg)
    {
        char *t1, *t2;
        UBYTE id;

        __initcheck ();
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
            __checkcontact (cont, data);
        __donecheck (data);
        
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
        {
            __showcontact (conn, cont, data);

            if (cont->dc)
            {
                rl_printf ("    %-15s %s / %s:%ld\n    %s %d    %s (%d)    %s %08lx\n",
                      i18n (1642, "IP:"), t1 = strdup (s_ip (cont->dc->ip_rem)),
                      t2 = strdup (s_ip (cont->dc->ip_loc)), cont->dc->port,
                      i18n (1453, "TCP version:"), cont->dc->version,
                      cont->dc->type == 4 ? i18n (1493, "Peer-to-Peer")
                        : i18n (1494, "Server Only"), cont->dc->type,
                      i18n (2026, "TCP cookie:"), cont->dc->cookie);
                free (t1);
                free (t2);
            }
            if (cont->group && cont->group != conn->contacts && cont->group->name)
                rl_printf ("    %s %s\n", i18n (2536, "Group:"), cont->group->name);
            for (j = id = 0; id < CAP_MAX; id++)
                if (cont->caps & (1 << id))
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
        return 0;
    }

    if (data & 32)
    {
        if (!(tcg = ContactGroupC (conn, 0, NULL)))
        {
            rl_print (i18n (2118, "Out of memory.\n"));
            return 0;
        }
        cg = conn->contacts;
        for (j = 0; (cont = ContactIndex (cg, j)); j++)
            if (!ContactHas (tcg, cont))
                ContactAdd (tcg, cont);
        for (i = 0; (cg = ContactGroupIndex (i)); i++)
            if (cg->serv == conn && cg != conn->contacts && cg != tcg)
                for (j = 0; (cont = ContactIndex (cg, j)); j++)
                    ContactRem (tcg, cont);
        cg = conn->contacts;
    }

    if (!cg)
        cg = conn->contacts;

    __initcheck ();
    for (i = 0; (cont = ContactIndex (tcg && ~data & 32 ? tcg : cg, i)); i++)
        __checkcontact (cont, data);
    __donecheck (data);

    if (data & 4)
    {
        if (conn)
        {
            rl_printf ("%s %s%10lu%s ", s_now, COLCONTACT, conn->uin, COLNONE);
            if (~conn->connect & CONNECT_OK)
                rl_printf (i18n (2405, "Your status is %s (%s).\n"),
                    i18n (1969, "offline"), s_status (conn->status));
            else
                rl_printf (i18n (2211, "Your status is %s.\n"), s_status (conn->status));
        }
        if (data & 16)
            return 0;
    }

    for (k = -1; (k == -1) ? (tcg ? (cg = tcg) : cg) : (cg = ContactGroupIndex (k)); k++)
    {
        char is_shadow;
        if (k != -1 && (cg == conn->contacts || cg == tcg))
            continue;
        if (cg->serv != conn)
            continue;
#ifdef CONFIG_UNDERLINE
        __l = 0;
#endif
        is_shadow = ~data & 64 && data & 32 && ContactGroupPrefVal (cg, CO_SHADOW);
        rl_print (COLQUOTE);
        if (cg != tcg && cg != conn->contacts && cg->name)
        {
            if (__totallen > c_strlen (cg->name) + 1)
            {
                for (i = j = (__totallen - c_strlen (cg->name) - 1) / 2; i >= 20; i -= 20)
                    rl_print ("====================");
                rl_printf ("%.*s", (int)i, "====================");
                rl_printf (" %s%s%s ", is_shadow ? COLQUOTE : COLCONTACT, cg->name, COLQUOTE);
                for (i = __totallen - j - c_strlen (cg->name) - 2; i >= 20; i -= 20)
                    rl_print ("====================");
            }
            else
                rl_printf (" %s%s%s ", is_shadow ? COLQUOTE : COLCONTACT, cg->name, COLQUOTE);
        }
        else
            for (i = __totallen; i >= 20; i -= 20)
                rl_print ("====================");
        rl_printf ("%.*s%s\n", (int)i, "====================", COLNONE);

        if (!is_shadow)
        {
            val_t val;
            char is_set;
            is_set = OptGetVal (&cg->copts, CO_SHADOW, &val);
            OptSetVal (&cg->copts, CO_SHADOW, 0);
            for (i = (data & 1 ? 2 : 0); i < 9; i++)
            {
                status = stati[i];
                for (j = 0; (cont = ContactIndex (cg, j)); j++)
                {
                    if (__status (cont) != status)
                        continue;
                    if (data & 64 || !ContactPrefVal (cont, CO_SHADOW))
                        __showcontact (conn, cont, data);
                }
            }
            if (is_set)
                OptSetVal (&cg->copts, CO_SHADOW, val);
            else
                OptUndef (&cg->copts, CO_SHADOW);
        }
        if (~data & 32)
            break;
    }
    if (tcg)
        ContactGroupD (tcg);
    rl_print (COLQUOTE);
    for (i = __totallen; i >= 20; i -= 20)
        rl_print ("====================");
    rl_printf ("%.*s%s\n", (int)i, "====================", COLNONE);
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
    
    if ((data == 4) || (cg = s_parselistrem (&args, conn)) || (data == 6))
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
                        rl_printf (i18n (2406, "Couldn't load meta data for %s (%ld).\n"),
                                  s_wordquote (cont->nick), cont->uin);
                }
                break;
            case 3:
                for (i = 0; (cont = ContactIndex (cg, i)); i++)
                {
                    if (ContactMetaSave (cont))
                        rl_printf (i18n (2248, "Saved meta data for '%s' (%ld).\n"),
                                  cont->nick, cont->uin);
                    else
                        rl_printf (i18n (2249, "Couldn't save meta data for '%s' (%ld).\n"),
                                  cont->nick, cont->uin);
                }
                break;
            case 4:
                if (!(cont = conn->cont))
                    return 0;
                if (conn->type == TYPE_SERVER)
                {
                    SnacCliMetasetgeneral (conn, cont);
                    SnacCliMetasetmore (conn, cont);
                    SnacCliMetasetabout (conn, cont->meta_about);
                }
                else
                {
                    CmdPktCmdMetaGeneral (conn, cont);
                    CmdPktCmdMetaMore (conn, cont);
                    CmdPktCmdMetaAbout (conn, cont->meta_about);
                }
                return 0;
            case 6:
                IMCliInfo (conn, uiG.last_rcvd, 0);
                if (!cg)
                    break;
            case 5:
                for (i = 0; (cont = ContactIndex (cg, i)); i++)
                    IMCliInfo (conn, cont, 0);
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
    cg = conn->contacts;
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
                      COLQUOTE, s_status (cont->status), COLNONE);
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
    int lennick = 0, columns, colleft, colright, i;
    Contact *cont;
    OPENCONN;

    cg = conn->contacts;

    if (data && !(cgoff = ContactGroupC (conn, 0, "")))
    {
        rl_print (i18n (2118, "Out of memory.\n"));
        return 0;
    }
    
    if (!(cgon = ContactGroupC (conn, 0, "")))
    {
        if (data)
            ContactGroupD (cgon);
        rl_print (i18n (2118, "Out of memory.\n"));
        return 0;
    }

    for (i = 0; (cont = ContactIndex (cg, i)); i++)
    {
        if (cont->status == STATUS_OFFLINE)
        {
            if (data)
            {
                ContactAdd (cgoff, cont);
                if (s_strlen (cont->nick) > (UDWORD)lennick)
                    lennick = s_strlen (cont->nick);
            }
        }
        else
        {
            ContactAdd (cgon, cont);
            if (s_strlen (cont->nick) > (UDWORD)lennick)
                lennick = s_strlen (cont->nick);
        }
    }

    columns = rl_columns / (lennick + 3);
    if (columns < 1)
        columns = 1;

    if (data)
    {
        colleft = (rl_columns - s_strlen (i18n (1653, "Offline"))) / 2 - 1;
        rl_print (COLQUOTE);
        for (i = 0; i < colleft; i++)
            rl_print ("=");
        rl_printf (" %s%s%s ", COLCLIENT, i18n (1653, "Offline"), COLQUOTE);
        colright = rl_columns - i - s_strlen (i18n (1653, "Offline")) - 2;
        for (i = 0; i < colright; i++)
            rl_print ("=");
        rl_print (COLNONE);
        for (i = 0; (cont = ContactIndex (cgoff, i)); i++)
        {
            if (!(i % columns))
                rl_print ("\n");
            rl_printf ("  %s%-*s%s ", COLCONTACT, lennick + s_delta (cont->nick), cont->nick, COLNONE);
        }
        rl_print ("\n");
        ContactGroupD (cgoff);
    }

    cont = conn->cont;
    rl_printf ("%s%ld%s %s", COLCONTACT, cont->uin, COLNONE, COLQUOTE);
    colleft = (rl_columns - s_strlen (i18n (1654, "Online"))) / 2 - s_strlen (s_sprintf ("%ld", cont->uin)) - 2;
    for (i = 0; i < colleft; i++)
        rl_print ("=");
    rl_printf (" %s%s%s ", COLCLIENT, i18n (1654, "Online"), COLQUOTE);
    i += 3 + s_strlen (i18n (1654, "Online")) + s_strlen (s_sprintf ("%ld", cont->uin));
    colright = rl_columns - i - s_strlen (s_status (conn->status)) - 3;
    for (i = 0; i < colright; i++)
        rl_print ("=");
    rl_printf (" %s(%s)%s", COLQUOTE, s_status (conn->status), COLNONE);
    for (i = 0; (cont = ContactIndex (cgon, i)); i++)
    {
        char ind = s_status (cont->status)[0];
        if ((cont->status & 0xffff) == STATUS_ONLINE)
            ind = ' ';

        if (!(i % columns))
            rl_print ("\n");
        rl_printf ("%c %s%-*s%s ", ind, COLCONTACT, lennick + s_delta (cont->nick), cont->nick, COLNONE);
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
              COLQUOTE, prG->away_time, COLNONE);
    return 0;
}

/*
 * Toggles simple options.
 */
static JUMP_F(CmdUserSet)
{
    int quiet = 0;
    strc_t par;
    const char *str = "";
    
    while (!data)
    {
        if (!(par = s_parse (&args)))                    break;
        else if (!strcasecmp (par->txt, "color"))      { data = FLAG_COLOR;      str = i18n (2133, "Color is %s%s%s.\n"); }
        else if (!strcasecmp (par->txt, "colour"))     { data = FLAG_COLOR;      str = i18n (2133, "Color is %s%s%s.\n"); }
        else if (!strcasecmp (par->txt, "delbs"))      { data = FLAG_DELBS;      str = i18n (2262, "Interpreting a delete character as backspace is %s%s%s.\n"); }
        else if (!strcasecmp (par->txt, "funny"))      { data = FLAG_FUNNY;      str = i18n (2134, "Funny messages are %s%s%s.\n"); }
        else if (!strcasecmp (par->txt, "auto"))       { data = FLAG_AUTOREPLY;  str = i18n (2265, "Automatic replies are %s%s%s.\n"); }
        else if (!strcasecmp (par->txt, "uinprompt"))  { data = FLAG_UINPROMPT;  str = i18n (2266, "Having the last nick in the prompt is %s%s%s.\n"); }
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
    }
    if (!data)
    {
        rl_printf (i18n (1820, "%s <option> [on|off|<value>] - control simple options.\n"), "set");
        rl_print (i18n (1822, "    color:      use colored text output.\n"));
        rl_print (i18n (2278, "    delbs:      interpret delete characters as backspace.\n"));
        rl_print (i18n (1815, "    funny:      use funny messages for output.\n"));
        rl_print (i18n (2281, "    auto:       send auto-replies.\n"));
        rl_print (i18n (2282, "    uinprompt:  have the last nick in the prompt.\n"));
        rl_print (i18n (2283, "    autosave:   automatically save the micqrc.\n"));
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
    Connection *connl = NULL;
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
    else if (!data && s_parsekey (&args, "connection") && conn->contacts)
    {
        copts = &conn->contacts->copts;
        data = COF_GROUP;
        optobj = conn->contacts->name;
        coptobj = strdup (optobj);
        connl = conn;
    }
    else if ((!data || data == COF_GROUP) && (cg = s_parsecg (&args, conn)))
    {
        copts = &cg->copts;
        data = COF_GROUP;
        optobj = cg->name;
        coptobj = strdup (s_qquote (optobj));
    }
    else if ((!data || data == COF_CONTACT) && (cont = s_parsenick (&args, conn)))
    {
        copts = &cont->copts;
        data = COF_CONTACT;
        optobj = cont->nick;
        coptobj = strdup (s_qquote (optobj));
    }
    else if (data == (COF_GLOBAL | COF_GROUP))
    {
        copts = &conn->contacts->copts;
        data = COF_GROUP;
        optobj = conn->contacts->name;
        coptobj = strdup (optobj);
        connl = conn;
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
                case COF_GROUP:
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
                   data & COF_GROUP   ? i18n (2417, "Options for contact group %s:\n")
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
                                    ? s_sprintf ("%ld", val)
                                    : i18n (2419, "undefined"), colnone, i18n (2420, "effectivly"), colquote,
                                  ContactPrefVal (cont, flag), colnone);
                    else
                        rl_printf ("    %-15s  %s%s%s\n", optname, colquote,
                                  OptGetVal (copts, flag, &val)
                                    ? s_sprintf ("%ld", val)
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
        
        if (!(par = s_parse (&args)))
        {
            const char *res = NULL;
            val_t val;

            if (!OptGetVal (copts, flag, &val) || ((flag & (COF_STRING | COF_COLOR)) && !OptGetStr (copts, flag, &res)))
                rl_printf (data & COF_CONTACT ? i18n (2422, "Option %s for contact %s is undefined.\n") :
                           data & COF_GROUP   ? i18n (2423, "Option %s for contact group %s is undefined.\n")
                                              : i18n (2424, "Option %s%s has no global value.\n"),
                          coptname, coptobj);
            else if (flag & (COF_BOOL | COF_NUMERIC))
                rl_printf (data & COF_CONTACT ? i18n (2425, "Option %s for contact %s is %s%s%s.\n") :
                           data & COF_GROUP   ? i18n (2426, "Option %s for contact group %s is %s%s%s.\n")
                                              : i18n (2427, "Option %s%s is globally %s%s%s.\n"),
                          coptname, coptobj, colquote, flag & COF_NUMERIC ? s_sprintf ("%ld", val)
                          : val ? i18n (1085, "on") : i18n (1086, "off"), colnone);
            else
                rl_printf (data & COF_CONTACT ? i18n (2428, "Option %s for contact %s is %s.\n") :
                           data & COF_GROUP   ? i18n (2429, "Option %s for contact group %s is %s.\n")
                                              : i18n (2430, "Option %s%s is globally %s.\n"),
                          coptname, coptobj, s_qquote (flag & COF_STRING  ? res : OptS2C (res)));
            return 0;
        }
        
        if (!strcasecmp (par->txt, "undef"))
        {
            if (flag & COF_STRING)
                OptSetStr (copts, flag, NULL);
            else
                OptUndef (copts, flag);
            rl_printf (data & COF_CONTACT ? i18n (2431, "Undefining option %s for contact %s.\n") :
                       data & COF_GROUP   ? i18n (2432, "Undefining option %s for contact group %s.\n")
                                          : i18n (2433, "Undefining global option %s%s.\n"),
                      coptname, coptobj);
        }
        else if (flag & COF_STRING)
        {
            res = par->txt;
            if (flag == CO_ENCODINGSTR)
            {
                UWORD enc = ConvEnc (par->txt) & ~ENC_FAUTO;
                OptSetVal (copts, CO_ENCODING, enc);
                res = ConvEncName (enc);
            }
            OptSetStr (copts, flag, res);
            rl_printf (data & COF_CONTACT ? i18n (2434, "Setting option %s for contact %s to %s.\n") :
                       data & COF_GROUP   ? i18n (2435, "Setting option %s for contact group %s to %s.\n")
                                          : i18n (2436, "Setting option %s%s globally to %s.\n"),
                      coptname, coptobj, s_qquote (res));
        }
        else if (flag & COF_COLOR)
        {
            res = OptC2S (col = strdup (par->txt));
            OptSetStr (copts, flag, res);
            rl_printf (data & COF_CONTACT ? i18n (2434, "Setting option %s for contact %s to %s.\n") :
                       data & COF_GROUP   ? i18n (2435, "Setting option %s for contact group %s to %s.\n")
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
                       data & COF_GROUP   ? i18n (2438, "Setting option %s for contact group %s to %s%d%s.\n")
                                          : i18n (2439, "Setting option %s%s globally to %s%d%s.\n"),
                      coptname, coptobj, colquote, atoi (par->txt), colnone);
        }
        else if (!strcasecmp (par->txt, "on")  || !strcasecmp (par->txt, i18n (1085, "on")))
        {
            OptSetVal (copts, flag, 1);
            rl_printf (data & COF_CONTACT ? i18n (2440, "Setting option %s for contact %s.\n") :
                       data & COF_GROUP   ? i18n (2441, "Setting option %s for contact group %s.\n")
                                          : i18n (2442, "Setting option %s%s globally.\n"),
                      coptname, coptobj);
        }
        else if (!strcasecmp (par->txt, "off") || !strcasecmp (par->txt, i18n (1086, "off")))
        {
            OptSetVal (copts, flag, 0);
            rl_printf (data & COF_CONTACT ? i18n (2443, "Clearing option %s for contact %s.\n") :
                       data & COF_GROUP   ? i18n (2444, "Clearing option %s for contact group %s.\n")
                                          : i18n (2445, "Clearing option %s%s globally.\n"),
                      coptname, coptobj);
        }
        else
        {
            rl_printf (i18n (2446, "Invalid value %s for boolean option %s.\n"), s_qquote (par->txt), coptname);
            continue;
        }
        if (flag & COF_GROUP && ~flag & COF_CONTACT && data == COF_GROUP && connl)
            SnacCliSetstatus (conn, conn->status & 0xffff, 3);
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

        if (!conn || conn->type == TYPE_SERVER)
        {
            Connection *newc = SrvRegisterUIN (conn, par->txt);
            ConnectionInitServer (newc);
        }
        else
            CmdPktCmdRegNewUser (conn, par->txt);     /* TODO */
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

    if ((cg = s_parselistrem (&args, conn)))
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
    int i;
    OPENCONN;

    if ((cg = s_parselistrem (&args, conn)))
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
        {
            if (ContactPrefVal (cont, CO_HIDEFROM))
            {
                OptSetVal (&cont->copts, CO_HIDEFROM, 0);
                if (conn->type == TYPE_SERVER)
                    SnacCliReminvis (conn, cont);
                else
                    CmdPktCmdUpdateList (conn, cont, INV_LIST_UPDATE, FALSE);
                rl_printf (i18n (2020, "Being visible to %s.\n"), cont->nick);
            }
            else
            {
                OptSetVal (&cont->copts, CO_INTIMATE, 0);
                OptSetVal (&cont->copts, CO_HIDEFROM, 1);
                if (conn->type == TYPE_SERVER)
                    SnacCliAddinvis (conn, cont);
                else
                    CmdPktCmdUpdateList (conn, cont, INV_LIST_UPDATE, TRUE);
                rl_printf (i18n (2021, "Being invisible to %s.\n"), cont->nick);
            }
            if (conn->type != TYPE_SERVER)
            {
                CmdPktCmdContactList (conn);
                CmdPktCmdInvisList (conn);
                CmdPktCmdVisList (conn);
                CmdPktCmdStatusChange (conn, conn->status);
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
    int i;
    OPENCONN;

    if ((cg = s_parselistrem (&args, conn)))
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
        {
            if (ContactPrefVal (cont, CO_INTIMATE))
            {
                OptSetVal (&cont->copts, CO_INTIMATE, 0);
                if (conn->type == TYPE_SERVER)
                    SnacCliRemvisible (conn, cont);
                else
                    CmdPktCmdUpdateList (conn, cont, VIS_LIST_UPDATE, FALSE);
                rl_printf (i18n (1670, "Normal visible to %s now.\n"), cont->nick);
            }
            else
            {
                OptSetVal (&cont->copts, CO_HIDEFROM, 0);
                OptSetVal (&cont->copts, CO_INTIMATE, 1);
                if (conn->type == TYPE_SERVER)
                    SnacCliAddvisible (conn, cont);
                else
                    CmdPktCmdUpdateList (conn, cont, VIS_LIST_UPDATE, TRUE);
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
    int i;
    OPENCONN;

    if (!data)
        (cg = s_parsecg (&args, conn));

    if (data == 2)
    {
        if (cg)
        {
            rl_printf (i18n (2448, "Contact group '%s' already exists\n"), cg->name);
            return 0;
        }
        if (!(cg = s_parsecg (&args, conn)))
        {
            if (!(par = s_parse (&args)))
            {
                rl_print (i18n (2240, "No contact group given.\n"));
                return 0;
            }
            if ((cg = ContactGroupC (conn, 0, par->txt)))
                rl_printf (i18n (2245, "Added contact group '%s'.\n"), par->txt);
            else
            {
                rl_print (i18n (2118, "Out of memory.\n"));
                return 0;
            }
        }
    }
    
    if (cg)
    {
        if ((acg = s_parselistrem (&args, conn)))
        {
            for (i = 0; (cont = ContactIndex (acg, i)); i++)
            {
                if (!cont->group)
                {
                    ContactFindCreate (conn->contacts, 0, cont->uin, s_sprintf ("%ld", cont->uin));
                    if (conn->type == TYPE_SERVER)
                        SnacCliAddcontact (conn, cont, NULL);
                    else
                        CmdPktCmdContactList (conn);
                    rl_printf (i18n (2117, "%ld added as %s.\n"), cont->uin, cont->nick);
                }
                if (ContactHas (cg, cont))
                {
                    if (cont->group != cg)
                    {
                        rl_printf (i18n (2449, "Primary contact group for contact '%s' is now '%s'.\n"),
                                   cont->nick, cg->name);
                        cont->group = cg;
                    }
                    else
                    {
                        rl_printf (i18n (2244, "Contact group '%s' already has contact '%s' (%ld).\n"),
                                   cg->name, cont->nick, cont->uin);
                    }
                }
                else if (ContactAdd (cg, cont))
                {
                    rl_printf (i18n (2241, "Added '%s' to contact group '%s'.\n"), cont->nick, cg->name);
                    rl_printf (i18n (2449, "Primary contact group for contact '%s' is now '%s'.\n"),
                               cont->nick, cg->name);
                    cont->group = cg;
                }
                else
                    rl_print (i18n (2118, "Out of memory.\n"));
            }
            rl_print (i18n (1754, "Note: You need to 'save' to write new contact list to disc.\n"));
        }
        return 0;
    }

    if (!(cont = s_parsenick (&args, conn)))
    {
        rl_printf (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
        return 0;
    }

    if (!(cmd = s_parserem (&args)))
    {
        rl_print (i18n (2116, "No new nick name given.\n"));
        return 0;
    }

    if (cmd)
    {
        if (!cont->group)
        {
            rl_printf (i18n (2117, "%ld added as %s.\n"), cont->uin, cmd);
            rl_print (i18n (1754, "Note: You need to 'save' to write new contact list to disc.\n"));
            if (c_strlen (cmd) > (UDWORD)uiG.nick_len)
                uiG.nick_len = c_strlen (cmd);
            ContactFindCreate (conn->contacts, 0, cont->uin, cmd);
            if (conn->type == TYPE_SERVER)
                SnacCliAddcontact (conn, cont, NULL);
            else
                CmdPktCmdContactList (conn);
        }
        else
        {
            if ((cont2 = ContactFind (conn->contacts, 0, cont->uin, cmd)))
                rl_printf (i18n (2146, "'%s' is already an alias for '%s' (%ld).\n"),
                         cont2->nick, cont->nick, cont->uin);
            else if ((cont2 = ContactFind (conn->contacts, 0, 0, cmd)))
                rl_printf (i18n (2147, "'%s' (%ld) is already used as a nick.\n"),
                         cont2->nick, cont2->uin);
            else
            {
                if (!(cont2 = ContactFindCreate (conn->contacts, 0, cont->uin, cmd)))
                    rl_print (i18n (2118, "Out of memory.\n"));
                else
                {
                    rl_printf (i18n (2148, "Added '%s' as an alias for '%s' (%ld).\n"),
                             cont2->nick, cont->nick, cont->uin);
                    rl_print (i18n (1754, "Note: You need to 'save' to write new contact list to disc.\n"));
                }
            }
        }
    }
    return 0;
}

/*
 * Remove a user from your contact list.
 *
 * 2 contact group
 * 1 alias
 * 0 auto
 */
static JUMP_F(CmdUserRemove)
{
    ContactGroup *cg = NULL, *acg;
    Contact *cont = NULL;
    UDWORD uin;
    char *alias;
    UBYTE all = 0;
    int i;
    OPENCONN;
    
    if (data != 1 && !(cg = s_parsecg (&args, conn)) && data == 2)
    {
        rl_print (i18n (2240, "No contact group given.\n"));
        return 0;
    }

    if (s_parsekey (&args, "all"))
        all = 2;
    else if (cg && data == 2)
        all = 1;
    
    if (all && cg && cg->serv->contacts != cg)
        return 0;

    if ((acg = s_parselistrem (&args, conn)))
    {
        for (i = 0; (cont = ContactIndex (acg, i)); i++)
        {
            if (cg)
            {
                if (ContactRem (cg, cont))
                    rl_printf (i18n (2243, "Removed contact '%s' from group '%s'.\n"),
                              cont->nick, cg->name);
                else
                    rl_printf (i18n (2246, "Contact '%s' is not in group '%s'.\n"),
                              cont->nick, cg->name);
            }
            else
            {
                if (!cont->group)
                    continue;

                if (all || !cont->alias)
                {
                    if (conn->type == TYPE_SERVER)
                        SnacCliRemcontact (conn, cont, NULL);
                    else
                        CmdPktCmdContactList (conn);
                }

                alias = strdup (cont->nick);
                uin = cont->uin;
                
                if (all || !cont->alias)
                {
                    ContactD (cont);
                    rl_printf (i18n (2150, "Removed contact '%s' (%ld).\n"),
                              alias, uin);
                }
                else
                {
                    ContactRemAlias (cont, alias);
                    rl_printf (i18n (2149, "Removed alias '%s' for '%s' (%ld).\n"),
                             alias, cont->nick, uin);
                }
                free (alias);
            }
        }
        rl_print (i18n (1754, "Note: You need to 'save' to write new contact list to disc.\n"));
    }
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
    char *msg = NULL;
    OPENCONN;

    if (!data)
    {
        if      (s_parsekey (&args, "deny"))  data = 2;
        else if (s_parsekey (&args, "req"))   data = 3;
        else if (s_parsekey (&args, "add"))   data = 4;
        else if (s_parsekey (&args, "grant")) data = 5;
        else                                  data = 5;
    }
    if (!*args && !data)
    {
        rl_print (i18n (2119, "auth [grant] <contacts>    - grant authorization.\n"));
        rl_print (i18n (2120, "auth deny <contacts> <msg> - refuse authorization.\n"));
        rl_print (i18n (2121, "auth req  <contacts> <msg> - request authorization.\n"));
        rl_print (i18n (2145, "auth add  <contacts>       - authorized add.\n"));
        return 0;
    }
    if (!(cg = s_parselist (&args, conn)))
        return 0;
    if (data & 2 && !(msg = s_parserem (&args)))
        msg = NULL;
    for (i = 0; (cont = ContactIndex (cg, i)); i++)
    {
        switch (data)
        {
            case 2:
                if (!msg)         /* FIXME: let it untranslated? */
                    msg = "Authorization refused\n";
#if 0
                if (conn->type == TYPE_SERVER && conn->version >= 8)
                    SnacCliAuthorize (conn, cont, 0, msg);
                else
#endif
                if (conn->type == TYPE_SERVER)
                    SnacCliSendmsg (conn, cont, msg, MSG_AUTH_DENY, 0);
                else
                    CmdPktCmdSendMessage (conn, cont, msg, MSG_AUTH_DENY);
                break;
            case 3:
                if (!msg)         /* FIXME: let it untranslated? */
                    msg = "Please authorize my request and add me to your Contact List\n";
#if 0
                if (conn->type == TYPE_SERVER && conn->version >= 8)
                    SnacCliReqauth (conn, cont, msg);
                else
#endif
                if (conn->type == TYPE_SERVER)
                    SnacCliSendmsg (conn, cont, msg, MSG_AUTH_REQ, 0);
                else
                    CmdPktCmdSendMessage (conn, cont, msg, MSG_AUTH_REQ);
                break;
            case 4:
#if 0
                if (conn->type == TYPE_SERVER && conn->version >= 8)
                    SnacCliGrantauth (conn, cont);
                else
#endif
                if (conn->type == TYPE_SERVER)
                    SnacCliSendmsg (conn, cont, "\x03", MSG_AUTH_ADDED, 0);
                else
                    CmdPktCmdSendMessage (conn, cont, "\x03", MSG_AUTH_ADDED);
                break;
            case 5:
#if 0
                if (conn->type == TYPE_SERVER && conn->version >= 8)
                    SnacCliAuthorize (conn, cont, 1, NULL);
                else
#endif
                if (conn->type == TYPE_SERVER)
                    SnacCliSendmsg (conn, cont, "\x03", MSG_AUTH_GRANT, 0);
                else
                    CmdPktCmdSendMessage (conn, cont, "\x03", MSG_AUTH_GRANT);
        }
    }
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

    if (!(cg = s_parselist (&args, conn)))
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
        IMCliMsg (conn, cont, OptSetVals (NULL, CO_MSGTYPE, MSG_URL, CO_MSGTEXT, cmsg, 0));
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
        rl_printf (" %s(%s)%s\n", COLQUOTE, s_status (cont->status), COLNONE);
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

    if (!(cont = s_parsenick (&args, conn)))
    {
        HistShow (NULL);

/*        cg = conn->contacts;
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
    while ((cont = s_parsenick (&args, conn)));
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
 *      historyd  <contact> <date> [<count>]
 *
 *      <contact> Nick or UIN
 *      <date>    Earliest date of shown messages. <date> has to be in
 *                ISO 8601 format (CCYY-MM-DD or CCYY-MM-DDThh:mm:ss).
 *      <count>   Count of shown message. Default is DEFAULT_HISTORY_COUNT.
 */
static JUMP_F(CmdUserHistory)
{
    Contact *cont = NULL;

    int n, msgCount;
    int msgMin = DEFAULT_HISTORY_COUNT, msgNum = DEFAULT_HISTORY_COUNT;
    time_t time, htime = (time_t)0;
    struct tm stamp, hstamp;
    char timestr[MAX_STR_BUF];
    const char *timeformat;
    BOOL isMsg = FALSE, isEmptyMsg = FALSE;
    strc_t par, p, line;

    FILE *logfile;
    long *fposArray = NULL, fposLen = 0, len;
    char buffer[LOG_MAX_PATH + 1], *target = buffer;
    const char *linep;
    int fposCur = 0, fd;

    ANYCONN;

    /*
     * check for nick/uin in contacts. If first parameter isn't a
     * contact then exit.
     * parse history parameters min and num
     */
    if (!(cont = s_parsenick (&args, conn)))
    {
        if ((par = s_parse (&args)))
            rl_printf (i18n (1061, "'%s' not recognized as a nick name.\n"), par->txt);
        else
            rl_print (i18n (2451, "history <contact> [<last> [<count>]] -- Show history of <contact>.\n"));
        return 0;
    }

    n = 0;
    if (data == 1)
        memset (&hstamp, 0, sizeof (struct tm));
    while ((par = s_parse_s (&args, MULTI_SEP)))
    {
        switch (n)
        {
            case 0:
                /* scan history date */
                if (data == 1)
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
                        msgMin = 0;
                    }
                    else
                    {
                        rl_printf (i18n (2396, "Parameter '%s' has a wrong date format, it has to be ISO 8601 compliant. Try '2004-31-01' or '2004-31-01T23:12:05'.\n"), par->txt);
                        return 0;
                    }
                }
                else
                {
                    if (!sscanf (par->txt, "%d", &msgMin))
                        msgMin = DEFAULT_HISTORY_COUNT;
                }
                break;
            case 1:
                if (!sscanf (par->txt, "%d", &msgNum))
                    msgNum = (msgMin < 0) ? (DEFAULT_HISTORY_COUNT) : (msgMin);
        }
        n++;
    }

    /*
     * get filename of logfile and open it for reading.
     */
    if (!prG->logplace || !*prG->logplace)
        prG->logplace = "history" _OS_PATHSEPSTR;

    snprintf (buffer, sizeof (buffer), s_realpath (prG->logplace));
    target += strlen (buffer);

    if (target[-1] == _OS_PATHSEP)
        snprintf (target, buffer + sizeof (buffer) - target, "%lu.log", cont->uin);

    /* try to open logfile for reading (no symlink file) */
    if ((fd = open (buffer, O_RDONLY, S_IRUSR)) == -1)
    {
        rl_printf (i18n (2385, "Couldn't open %s for reading: %s (%d).\n"),
                  buffer, strerror (errno), errno);
        return 0;
    }
    if (!(logfile = fopen (buffer, "r")))
    {
        rl_printf (i18n (2385, "Couldn't open %s for reading: %s (%d).\n"),
                  buffer, strerror (errno), errno);
        close (fd);
        return 0;
    }

    /*
     *  correct parameters min and num parameters
     */
    if (n == 1)
        msgNum = (msgMin > 0) ? msgMin : (DEFAULT_HISTORY_COUNT);
    if (msgMin == 0 && data != 1)
        msgNum = 0;
    if (msgNum < 0)
        msgNum = -msgNum;

    memset (&stamp, 0, sizeof (struct tm));

    /* Get array of last file positions, reset memory to zeros */
    if (msgMin > 0)
    {
        fposLen = msgMin;
        fposArray = (long *) malloc (fposLen * sizeof (long));
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
                n = 0;
                while ((p = s_parse (&linep)))
                {
                    switch (n)
                    {
                        case 2:
                            if ((p->txt[0] == '<' && p->txt[1] == '-') ||
                                (p->txt[0] == '-' && p->txt[1] == '>'))
                            {
                                msgCount++;

                                /* save corrected file position */
                                fposArray[fposCur] = ftell (logfile) - len;
                                if (++fposCur == fposLen)
                                    fposCur = 0;
                            }
                            break;
                    }
                    if (++n > 2) break;
                }
            }
        }
        /* reset file to the requested position. If message
           count smaller than msgMin, set file position to the first
           message. */
        if (msgCount == 0)
        {
            close (fd);
            free (fposArray);
            return 0;
        }
        if (msgCount < msgMin)
            fposCur = 0;
        fseek (logfile, fposArray[fposCur], SEEK_SET);
    }


    /*
     * if msgMin negativ, count from the beginning (-1 for first message).
     */
    if (msgMin > 0)
    {
        if (msgCount < msgMin)
        {
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
        if (*linep == '#')
        {
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
                else
                    if (++fposCur == fposLen)
                        fposCur = 0;
            }

            linep++;
            n = 0;
            isMsg = FALSE;

            /* extract time form sent and received messages */
            while ((p = s_parse (&linep)))
            {
                switch (n)
                {
                    case 0:
                        if (msgCount > msgMin-2)
                            sscanf (p->txt, "%4d%2d%2d%2d%2d%2d",
                                    &stamp.tm_year, &stamp.tm_mon, &stamp.tm_mday,
                                    &stamp.tm_hour, &stamp.tm_min, &stamp.tm_sec);
                        break;
                    case 2:
                        if ((p->txt[0] == '<' && p->txt[1] == '-') ||
                            (p->txt[0] == '-' && p->txt[1] == '>'))
                        {
                            msgCount++;
                            /* evaluate requested history scope
                               if all lines are dumped, exit */
                            if (msgCount < msgMin && data != 1)
                                break;
                            if (msgNum > 0 &&
                                msgCount + 1 > msgMin + msgNum)
                            {
                                close (fd);
                                free (fposArray);
                                return 0;
                            }

                            if (isEmptyMsg)
                                rl_printf ("%s\n", COLNONE);

                            /* convert local time */
                            stamp.tm_mon--;
                            stamp.tm_year -= 1900;
                            time = timegm (&stamp);

                            if (time != -1)
                            {
                                localtime_r (&time, &stamp);
                                /* history date command */
                                if (data == 1 &&
                                    difftime (time, htime) < 0)
                                {
                                    msgMin = msgCount + 1;
                                    break;
                                }
                            }

                            isMsg = TRUE;
                            isEmptyMsg = TRUE;

                            strftime (timestr, MAX_STR_BUF, timeformat, &stamp);
                            rl_printf ("%s %s[%4d] ", timestr, COLNONE, msgCount);

                            if (*p->txt == '<')
                                rl_printf ("%s<- ", COLINCOMING);
                            else
                                rl_printf ("%s-> ", COLSENT);
                        }
                        break;
                }
                if (++n > 2)
                    break;
            }
        }
        else
        {
            if (isMsg == TRUE)
            {
                rl_printf ("%s%s\n", COLNONE, linep);
                isEmptyMsg = FALSE;
            }
        }
    }

    if (isEmptyMsg)
        rl_printf ("%s\n", COLNONE);

    close (fd);
    free (fposArray);

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
    char timestr[MAX_STR_BUF];
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
    if (!(cont = s_parsenick (&args, conn)))
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

    snprintf (buffer, sizeof (buffer), s_realpath (prG->logplace));
    target += strlen (buffer);

    if (target[-1] == _OS_PATHSEP)
        snprintf (target, buffer + sizeof (buffer) - target, "%lu.log", cont->uin);

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
                msg = (char*) malloc (size = MEMALIGN (strlen (linep) + 1));
                *msg = '\0';
                if (!data)
                    msgLower = (char*) malloc (size);
            }
            len = strlen (msg);
            if (size < len + strlen (linep))
            {
                msg = realloc (msg, size += MEMALIGN (strlen (linep) + 1));
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

        strftime (timestr, MAX_STR_BUF, timeformat, &stamp);
        rl_printf ("%s %s[%4d] ", timestr, COLNONE, msgCount);

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
 * Shows mICQ's uptime.
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

    rl_printf ("%s ", i18n (1687, "mICQ has been running for"));
    if (Days != 0)
        rl_printf ("%s%02d%s %s, ", COLQUOTE, Days, COLNONE, i18n (1688, "days"));
    if (Hours != 0)
        rl_printf ("%s%02d%s %s, ", COLQUOTE, Hours, COLNONE, i18n (1689, "hours"));
    if (Minutes != 0)
        rl_printf ("%s%02d%s %s, ", COLQUOTE, Minutes, COLNONE, i18n (1690, "minutes"));
    rl_printf ("%s%02d%s %s.\n", COLQUOTE, Seconds, COLQUOTE, i18n (1691, "seconds"));

    rl_print (i18n (1746, " nr type         sent/received packets/unique packets\n"));
    for (i = 0; (connl = ConnectionNr (i)); i++)
    {
        rl_printf ("%3d %-12s %7ld %7ld %7ld %7ld\n",
                 i + 1, ConnectionType (connl), connl->stat_pak_sent, connl->stat_pak_rcvd,
                 connl->stat_real_pak_sent, connl->stat_real_pak_rcvd);
        pak_sent += connl->stat_pak_sent;
        pak_rcvd += connl->stat_pak_rcvd;
        real_pak_sent += connl->stat_real_pak_sent;
        real_pak_rcvd += connl->stat_real_pak_rcvd;
    }
    rl_printf ("    %-12s %7d %7d %7d %7d\n",
             i18n (1747, "total"), pak_sent, pak_rcvd,
             real_pak_sent, real_pak_rcvd);
    rl_printf (i18n (2073, "Memory usage: %ld packets processing.\n"), uiG.packets);
    return 0;
}

/*
 * List connections, and open/close them.
 */
static JUMP_F(CmdUserConn)
{
    int i;
    UDWORD nr;
    strc_t par;
    Connection *connl;

    if (!data)
    {
        if (!(par = s_parse (&args)))          data = 1;
        else if (!strcmp (par->txt, "login"))  data = 2;
        else if (!strcmp (par->txt, "open"))   data = 2;
        else if (!strcmp (par->txt, "select")) data = 3;
        else if (!strcmp (par->txt, "delete")) data = 4;
        else if (!strcmp (par->txt, "remove")) data = 4;
        else if (!strcmp (par->txt, "close"))  data = 5;
        else if (!strcmp (par->txt, "logoff")) data = 5;
        else                                   data = 0;
    }
     
    switch (data)
    {
        case 0:
            rl_print (i18n (1892, "conn               List available connections.\n"));
            rl_print (i18n (2094, "conn login         Open current/first server connection.\n"));
            rl_print (i18n (1893, "conn login <nr>    Open connection <nr>.\n"));
            rl_print (i18n (2156, "conn close <nr>    Close connection <nr>.\n"));
            rl_print (i18n (2095, "conn remove <nr>   Remove connection <nr>.\n"));
            rl_print (i18n (2097, "conn select <nr>   Select connection <nr> as server connection.\n"));
            rl_print (i18n (2100, "conn select <uin>  Select connection with UIN <uin> as server connection.\n"));
            break;

        case 1:
            rl_print (i18n (1887, "Connections:"));
            rl_print ("\n  " COLINDENT);
            
            for (i = 0; (connl = ConnectionNr (i)); i++)
            {
                Contact *cont = connl->cont;

                rl_printf (i18n (2452, "%02d %-15s version %d%s for %s (%lx), at %s:%ld %s\n"),
                          i + 1, ConnectionType (connl), connl->version,
#ifdef ENABLE_SSL
                          connl->ssl_status == SSL_STATUS_OK ? " SSL" : "",
#else
                          "",
#endif
                          cont ? cont->nick : "", connl->status,
                          connl->server ? connl->server : s_ip (connl->ip), connl->port,
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
                         connl->our_session, connl->our_seq, connl->our_seq2);
#ifdef ENABLE_SSL
                    rl_printf (i18n (2453, "    at %p parent %p assoc %p ssl %d\n"), connl, connl->parent, connl->assoc, connl->ssl_status);
#else
                    rl_printf (i18n (2081, "    at %p parent %p assoc %p\n"), connl, connl->parent, connl->assoc);
#endif
                    rl_printf (i18n (2454, "    open %p reconn %p close %p err %p dispatch %p\n"),
                              connl->open, connl->reconnect, connl->close, connl->error, connl->dispatch);
                    free (t1);
                    free (t2);
                    free (t3);
                }
            } 
            rl_print (COLEXDENT "\r");
            break;
            
        case 2:
            if (!s_parseint (&args, &nr))
            {
                if (ConnectionFindNr (conn))
                    nr = ConnectionFindNr (conn) + 1;
                else
                    nr = 1 + ConnectionFindNr (ConnectionFind (TYPEF_SERVER, NULL, 0));
            }

            connl = ConnectionNr (nr - 1);
            if (!connl && !(connl = ConnectionFindUIN (TYPEF_SERVER, nr)))
            {
                rl_printf (i18n (1894, "There is no connection number %ld.\n"), nr);
                return 0;
            }
            if (connl->connect & CONNECT_OK)
            {
                rl_printf (i18n (1891, "Connection %ld is already open.\n"), nr);
                return 0;
            }
            if (!connl->open)
            {
                rl_printf (i18n (2194, "Don't know how to open this connection type.\n"));
                return 0;
            }
            connl->open (connl);

        case 3:
            if (!s_parseint (&args, &nr))
                nr = ConnectionFindNr (conn) + 1;

            connl = ConnectionNr (nr - 1);
            if (!connl && !(connl = ConnectionFindUIN (TYPEF_SERVER, nr)))
            {
                rl_printf (i18n (1894, "There is no connection number %ld.\n"), nr);
                return 0;
            }
            if (~connl->type & TYPEF_SERVER)
            {
                rl_printf (i18n (2098, "Connection %ld is not a server connection.\n"), nr);
                return 0;
            }
            conn = connl;
            rl_printf (i18n (2099, "Selected connection %ld (version %d, UIN %ld) as current connection.\n"),
                      nr, connl->version, connl->uin);
            break;
        
        case 4:
            if (!s_parseint (&args, &nr))
                nr = 0;

            connl = ConnectionNr (nr - 1);
            if (!connl)
            {
                rl_printf (i18n (1894, "There is no connection number %ld.\n"), nr);
                return 0;
            }
            if (connl->flags & CONN_CONFIGURED)
            {
                rl_printf (i18n (2102, "Connection %ld is a configured connection.\n"), nr);
                return 0;
            }
            rl_printf (i18n (2101, "Removing connection %ld and its dependents completely.\n"), nr);
            ConnectionD (connl);
            break;
        
        case 5:
            if (!s_parseint (&args, &nr))
                nr = 0;

            connl = ConnectionNr (nr - 1);
            if (!connl)
            {
                rl_printf (i18n (1894, "There is no connection number %ld.\n"), nr);
                return 0;
            }
            if (connl->close)
            {
                rl_printf (i18n (2185, "Closing connection %ld.\n"), nr);
                connl->close (connl);
            }
            else
            {
                rl_printf (i18n (2101, "Removing connection %ld and its dependents completely.\n"), nr);
                ConnectionD (connl);
            }
    }
    return 0;
}

/*
 * Download Contact List
 */
static JUMP_F(CmdUserContact)
{
    strc_t par;
    OPENCONN;

    if (conn->type != TYPE_SERVER)
    {
        rl_print (i18n (2326, "Server side contact list only supported for ICQ v8.\n"));
        return 0;
    }

    if (!data)
    {
        if (!(par = s_parse (&args)))                  data = 0;
        else if (!strcasecmp (par->txt, "activate"))
        {
            SnacCliRosterack (conn);
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
        else if (!strcasecmp (par->txt, "delid"))
        {
            UDWORD tag, id;
            if (s_parseint (&args, &tag) && s_parseint (&args, &id))
                IMDeleteID (conn, tag, id);
            return 0;
        }
        else                                         data = 0;
    }
    if (data)
        IMRoster (conn, data);
    else
    {
        rl_print (i18n (2103, "contact show    Show server based contact list.\n"));
        rl_print (i18n (2104, "contact diff    Show server based contacts not on local contact list.\n"));
        rl_print (i18n (2321, "contact add     Add server based contact list to local contact list.\n"));
        rl_print (i18n (2105, "contact import  Import server based contact list as local contact list.\n"));
        rl_print (i18n (2322, "contact sync    Import server based contact list if appropriate.\n"));
        rl_print (i18n (2323, "contact export  Export local contact list to server based list.\n"));
        rl_print (i18n (2324, "contact upload  Add local contacts to server based contact list.\n"));
    }
    return 0;
}

/*
 * Exits mICQ.
 */
static JUMP_F(CmdUserQuit)
{
    uiG.quit = data;
    return 0;
}


/*
 * Runs a command using temporarely a different current connection
 */
static JUMP_F(CmdUserAsSession)
{
    Connection *saveconn, *tmpconn;
    UDWORD quiet = 0, nr;
    
    saveconn = conn;
    
    if (s_parsekey (&args, "quiet"))
        quiet = 1;

    if (!s_parseint (&args, &nr))
    {
        /* syntax error */
        return 0;
    }

    tmpconn = ConnectionNr (nr - 1);
    if (!tmpconn && !(tmpconn = ConnectionFindUIN (TYPEF_SERVER, nr)))
    {
        if (!quiet)
            rl_printf (i18n (1894, "There is no connection number %ld.\n"), nr);
        return 0;
    }
    if (~tmpconn->type & TYPEF_SERVER)
    {
        if (!quiet)
            rl_printf (i18n (2098, "Connection %ld is not a server connection.\n"), nr);
        return 0;
    }
    conn = tmpconn;
    CmdUser (args);
    conn = saveconn;
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
                if (conn->type == TYPE_SERVER)
                    SnacCliSearchbymail (conn, args);
                else
                    CmdPktCmdSearchUser (conn, args, "", "", "");
                
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
            if (conn->type == TYPE_SERVER)
                SnacCliSearchbypersinf (conn, email, nick, first, last);
            else
                CmdPktCmdSearchUser (conn, email, nick, first, last);
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
                if (conn->type == TYPE_SERVER)
                    SnacCliSearchbypersinf (conn, NULL, NULL, arg1, par->txt);
                else
                    CmdPktCmdSearchUser (conn, NULL, NULL, arg1, par->txt);
            }
            else if (strchr (arg1, '@'))
            {
                if (conn->type == TYPE_SERVER)
                    SnacCliSearchbymail (conn, arg1);
                else
                    CmdPktCmdSearchUser (conn, arg1, "", "", "");
            }
            else
            {
                if (conn->type == TYPE_SERVER)
                    SnacCliSearchbypersinf (conn, NULL, arg1, NULL, NULL);
                else
                    CmdPktCmdSearchUser (conn, NULL, arg1, NULL, NULL);
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
            if (conn->type == TYPE_SERVER)
            {
                if (status > 250 && status <= 403)
                    SnacCliSearchbypersinf (conn, wp.email, wp.nick, wp.first, wp.last);
                else
                    SnacCliSearchwp (conn, &wp);
            }
            else
            {
                if (status > 250 && status <= 403)
                    CmdPktCmdSearchUser (conn, wp.email, wp.nick, wp.first, wp.last);
                else
                    CmdPktCmdMetaSearchWP (conn, &wp);
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
    
    if (!(cont = conn->cont))
        return 0;
    if (!(user = CONTACT_GENERAL(cont)))
        return 0;

    switch (status)
    {
        case 0:
            ReadLinePromptSet (i18n (1553, "Enter Your New Nickname:"));
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
            ReadLinePromptSet (i18n (1556, "Enter your new email address:"));
            return ++status;
        case 303:
            if (!user->email && args)
            {
                user->email = strdup (args);
                ContactMetaD (cont->meta_email);
                cont->meta_email = NULL;
            }
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
                if (conn->type == TYPE_SERVER)
                    SnacCliMetasetgeneral (conn, cont);
                else
                    CmdPktCmdMetaGeneral (conn, cont);
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
    
    if (!(cont = conn->cont))
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
            if (conn->type == TYPE_SERVER)
                SnacCliMetasetmore (conn, cont);
            else
                CmdPktCmdMetaMore (conn, cont);
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
                if (conn->type == TYPE_SERVER)
                    SnacCliMetasetabout (conn, msg);
                else
                    CmdPktCmdMetaAbout (conn, msg);
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
                    if (conn->type == TYPE_SERVER)
                        SnacCliMetasetabout (conn, msg);
                    else
                        CmdPktCmdMetaAbout (conn, msg);
                }
            }
        case -1:
            return 0;
        case 0:
            if ((arg = s_parserem (&args)))
            {
                if (conn->type == TYPE_SERVER)
                    SnacCliMetasetabout (conn, arg);
                else
                    CmdPktCmdMetaAbout (conn, arg);
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
    Connection *tconn;
    const char *args;
    strc_t par;
    
    if (event && event->conn && event->conn->type & TYPEF_ANY_SERVER
        && OptGetStr (event->opt, CO_MICQCOMMAND, &args))
    {
        tconn = conn;
        conn = event->conn;
        while ((par = s_parse (&args)))
            CmdUser (par->txt);
        conn = tconn;
    }
    EventD (event);
}

/*
 * Process one command, but ignore idle times.
 */
void CmdUser (const char *command)
{
    time_t a;
    UBYTE  b;
    CmdUserProcess (command, &a, &b);
}

/*
 * Get one line of input and process it.
 */
void CmdUserInput (strc_t line)
{
    CmdUserProcess (line->txt, &uiG.idle_val, &uiG.idle_flag);
}

/*
 * Process an alias.
 */
static int CmdUserProcessAlias (const char *cmd, const char *argsd,
                                time_t *idle_val, UBYTE *idle_flag)
{
    alias_t *alias;
    static int recurs_level = 0;

    if (recurs_level > 10)
    {
        rl_print (i18n (2302, "Too many levels of alias expansion; probably an infinite loop.\n"));
        return TRUE;
    }
    
    alias = CmdUserLookupAlias (cmd);

    if (alias)
    {
        char *cmdline = malloc (strlen (alias->expansion)
                                + strlen (argsd) + 2);

        if (strstr (alias->expansion, "%s"))
        {
            char *exp, *ptr;

            exp = strdup (alias->expansion);
            ptr = strstr (exp, "%s");
            *ptr = '\0';
            
            cmdline = strdup (s_sprintf ("%s%s%s", exp, argsd, ptr + 2));
            free (exp);
        }
        else
            cmdline = strdup (s_sprintf ("%s %s", alias->expansion, argsd));

        recurs_level++;
        CmdUserProcess (cmdline, idle_val, idle_flag);
        recurs_level--;
        
        free (cmdline);

        return TRUE;
    }
    else
        return FALSE;
}

static UBYTE isinterrupted = 0;

void CmdUserInterrupt (void)
{
    isinterrupted = 1;
}

/*
 * Process one line of command.
 */
static void CmdUserProcess (const char *command, time_t *idle_val, UBYTE *idle_flag)
{
    char buf[1024];    /* This is hopefully enough */
    char *cmd = NULL;
    const char *args;
    strc_t par;

    static jump_f *sticky = (jump_f *)NULL;
    static int status = 0;

    time_t idle_save;
    idle_save = *idle_val;
    *idle_val = time (NULL);

    snprintf (buf, sizeof (buf), "%s", command);
    rl_print ("\r");
    buf[1023] = 0;

    if (!conn || !(ConnectionFindNr (conn) + 1))
        conn = ConnectionFind (TYPEF_ANY_SERVER, 0, NULL);

    if (isinterrupted)
    {
        if (status)
            status = (*sticky)(buf, 0, -1);
        isinterrupted = 0;
    }
    
    if (status)
    {
        status = (*sticky)(buf, 0, status);
    }
    else
    {
        if (buf[0] != 0)
        {
            if ('!' == buf[0])
            {
                ReadLineTtyUnset ();
#ifdef SHELL_COMMANDS
                if (((unsigned char)buf[1] < 31) || (buf[1] & 128))
                    rl_printf (i18n (1660, "Invalid Command: %s\n"), &buf[1]);
                else
                    system (&buf[1]);
#else
                rl_print (i18n (1661, "Shell commands have been disabled.\n"));
#endif
                ReadLineTtySet ();
                if (!command)
                    ReadLinePromptReset ();
                return;
            }
            args = buf;
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
            if (strchr ("/.", *cmd))
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
            }
            else
            {
                char *argsd;
                jump_t *j = (jump_t *)NULL;
                
                cmd = strdup (cmd);
                argsd = strdup (args);

                if (*cmd != '\\' &&
                    CmdUserProcessAlias (cmd, *argsd ? argsd + 1 : "", &idle_save, idle_flag))
                    ;
                else if ((j = CmdUserLookup (*cmd == '\\' ? cmd + 1 : cmd)))
                {
                    if (j->unidle == 2)
                        *idle_val = idle_save;
                    else if (j->unidle == 1)
                        *idle_flag = 0;
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
