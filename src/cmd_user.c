/*
 * Handles commands the user sends.
 *
 * Copyright: This file may be distributed under version 2 of the GPL licence.
 *
 * alias stuff GPL >= v2
 *
 * $Id$
 */

#include "micq.h"
#include "cmd_user.h"
#include "util.h"
#include "util_ui.h"
#include "util_io.h"
#include "util_table.h"
#include "util_extra.h"
#include "cmd_pkt_cmd_v5.h"
#include "cmd_pkt_cmd_v5_util.h"
#include "cmd_pkt_v8.h"
#include "cmd_pkt_v8_flap.h"
#include "cmd_pkt_v8_snac.h"
#include "im_icq8.h"
#include "preferences.h"
#include "packet.h"
#include "tabs.h"
#include "session.h"
#include "tcp.h"
#include "icq_response.h"
#include "conv.h"
#include "file_util.h"
#include "buildmark.h"
#include "contact.h"
#include "server.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>

static jump_f
    CmdUserChange, CmdUserRandom, CmdUserHelp, CmdUserInfo, CmdUserTrans,
    CmdUserAuto, CmdUserAlias, CmdUserUnalias, CmdUserMessage,
    CmdUserResend, CmdUserPeek, CmdUserAsSession,
    CmdUserVerbose, CmdUserRandomSet, CmdUserIgnoreStatus, CmdUserSMS,
    CmdUserStatusDetail, CmdUserStatusWide,
    CmdUserSound, CmdUserSoundOnline, CmdUserRegister, CmdUserStatusMeta,
    CmdUserSoundOffline, CmdUserAutoaway, CmdUserSet, CmdUserClear,
    CmdUserTogIgnore, CmdUserTogInvis, CmdUserTogVisible, CmdUserAdd, CmdUserRemove,
    CmdUserAuth, CmdUserURL, CmdUserSave, CmdUserTabs, CmdUserLast,
    CmdUserUptime, CmdUserOldSearch, CmdUserSearch, CmdUserUpdate, CmdUserPass,
    CmdUserOther, CmdUserAbout, CmdUserQuit, CmdUserPeer, CmdUserConn,
    CmdUserContact, CmdUserAnyMess, CmdUserGetAuto, CmdUserOpt;

static void CmdUserProcess (const char *command, time_t *idle_val, UBYTE *idle_flag);

/* 1 = do not apply idle stuff next time           v
   2 = count this line as being idle               v */
static jump_t jump[] = {
    { &CmdUserRandom,        "rand",         NULL, 0,   0 },
    { &CmdUserRandomSet,     "setr",         NULL, 0,   0 },
    { &CmdUserHelp,          "help",         NULL, 0,   0 },
    { &CmdUserInfo,          "f",            NULL, 0,   0 },
    { &CmdUserInfo,          "finger",       NULL, 0,   0 },
    { &CmdUserInfo,          "info",         NULL, 0,   0 },
    { &CmdUserInfo,          "rinfo",        NULL, 0,   1 },
    { &CmdUserTrans,         "lang",         NULL, 0,   0 },
    { &CmdUserTrans,         "trans",        NULL, 0,   0 },
    { &CmdUserAuto,          "auto",         NULL, 0,   0 },
    { &CmdUserAlias,         "alias",        NULL, 0,   0 },
    { &CmdUserUnalias,       "unalias",      NULL, 0,   0 },
    { &CmdUserAnyMess,       "message",      NULL, 0,   0 },
    { &CmdUserMessage,       "msg",          NULL, 0,   1 },
    { &CmdUserMessage,       "r",            NULL, 0,   2 },
    { &CmdUserMessage,       "a",            NULL, 0,   4 },
    { &CmdUserGetAuto,       "getauto",      NULL, 0,   0 },
    { &CmdUserResend,        "resend",       NULL, 0,   0 },
    { &CmdUserVerbose,       "verbose",      NULL, 0,   0 },
    { &CmdUserIgnoreStatus,  "i",            NULL, 0,   0 },
    { &CmdUserStatusDetail,  "status",       NULL, 2,  10 },
    { &CmdUserStatusDetail,  "ww",           NULL, 2,   2 },
    { &CmdUserStatusDetail,  "ee",           NULL, 2,   3 },
    { &CmdUserStatusDetail,  "w",            NULL, 2,   4 },
    { &CmdUserStatusDetail,  "e",            NULL, 2,   5 },
    { &CmdUserStatusDetail,  "wwg",          NULL, 2,  34 },
    { &CmdUserStatusDetail,  "eeg",          NULL, 2,  35 },
    { &CmdUserStatusDetail,  "wg",           NULL, 2,  36 },
    { &CmdUserStatusDetail,  "eg",           NULL, 2,  37 },
    { &CmdUserStatusDetail,  "s",            NULL, 2,  30 },
    { &CmdUserStatusDetail,  "s-any",        NULL, 2,   0 },
    { &CmdUserStatusMeta,    "ss",           NULL, 2,   1 },
    { &CmdUserStatusMeta,    "meta",         NULL, 2,   0 },
    { &CmdUserStatusWide,    "wide",         NULL, 2,   1 },
    { &CmdUserStatusWide,    "ewide",        NULL, 2,   0 },
    { &CmdUserSet,           "set",          NULL, 0,   0 },
    { &CmdUserOpt,           "opt",          NULL, 0,   0 },
    { &CmdUserOpt,           "optglobal",    NULL, 0, COF_GLOBAL  },
    { &CmdUserOpt,           "optgroup",     NULL, 0, COF_GROUP   },
    { &CmdUserOpt,           "optcontact",   NULL, 0, COF_CONTACT },
    { &CmdUserSound,         "sound",        NULL, 2,   0 },
    { &CmdUserSoundOnline,   "soundonline",  NULL, 2,   0 },
    { &CmdUserSoundOffline,  "soundoffline", NULL, 2,   0 },
    { &CmdUserAutoaway,      "autoaway",     NULL, 2,   0 },
    { &CmdUserChange,        "change",       NULL, 1,  -1 },
    { &CmdUserChange,        "online",       NULL, 1, STATUS_ONLINE },
    { &CmdUserChange,        "away",         NULL, 1, STATUS_AWAY },
    { &CmdUserChange,        "na",           NULL, 1, STATUS_NA   },
    { &CmdUserChange,        "occ",          NULL, 1, STATUS_OCC  },
    { &CmdUserChange,        "dnd",          NULL, 1, STATUS_DND  },
    { &CmdUserChange,        "ffc",          NULL, 1, STATUS_FFC  },
    { &CmdUserChange,        "inv",          NULL, 1, STATUS_INV  },
    { &CmdUserClear,         "clear",        NULL, 2,   0 },
    { &CmdUserTogIgnore,     "togig",        NULL, 0,   0 },
    { &CmdUserTogVisible,    "togvis",       NULL, 0,   0 },
    { &CmdUserTogInvis,      "toginv",       NULL, 0,   0 },
    { &CmdUserAdd,           "add",          NULL, 0,   0 },
    { &CmdUserAdd,           "addalias",     NULL, 0,   1 },
    { &CmdUserAdd,           "addgroup",     NULL, 0,   2 },
    { &CmdUserRemove,        "rem",          NULL, 0,   0 },
    { &CmdUserRemove,        "remalias",     NULL, 0,   1 },
    { &CmdUserRemove,        "remgroup",     NULL, 0,   2 },
    { &CmdUserRegister,      "reg",          NULL, 0,   0 },
    { &CmdUserAuth,          "auth",         NULL, 0,   0 },
    { &CmdUserURL,           "url",          NULL, 0,   0 },
    { &CmdUserSave,          "save",         NULL, 0,   0 },
    { &CmdUserTabs,          "tabs",         NULL, 0,   0 },
    { &CmdUserLast,          "last",         NULL, 0,   0 },
    { &CmdUserUptime,        "uptime",       NULL, 0,   0 },
    { &CmdUserPeer,          "peer",         NULL, 0,   0 },
    { &CmdUserPeer,          "tcp",          NULL, 0,   0 },
    { &CmdUserPeer,          "file",         NULL, 0,   4 },
    { &CmdUserQuit,          "q",            NULL, 0,   0 },
    { &CmdUserQuit,          "quit",         NULL, 0,   0 },
    { &CmdUserQuit,          "exit",         NULL, 0,   0 },
    { &CmdUserPass,          "pass",         NULL, 0,   0 },
    { &CmdUserSMS,           "sms",          NULL, 0,   0 },
    { &CmdUserPeek,          "peek",         NULL, 0,   0 },
    { &CmdUserAsSession,     "as",           NULL, 0,   0 },
    { &CmdUserContact,       "contact",      NULL, 0,   0 },
    { &CmdUserContact,       "contactshow",  NULL, 0,   1 },
    { &CmdUserContact,       "contactdiff",  NULL, 0,   2 },
    { &CmdUserContact,       "contactadd",   NULL, 0,   3 },
    { &CmdUserContact,       "contactdl",    NULL, 0,   1 },

    { &CmdUserOldSearch,     "oldsearch",    NULL, 0,   0 },
    { &CmdUserSearch,        "search",       NULL, 0,   0 },
    { &CmdUserUpdate,        "update",       NULL, 0,   0 },
    { &CmdUserOther,         "other",        NULL, 0,   0 },
    { &CmdUserAbout,         "about",        NULL, 0,   0 },
    { &CmdUserConn,          "conn",         NULL, 0,   0 },

    { NULL, NULL, NULL, 0 }
};

static alias_t *aliases = NULL;

static Connection *currconn = NULL;

/* Have an opened server connection ready. */
#define OPENCONN Connection *conn = currconn; \
    if (!conn)                     currconn = conn = ConnectionFind (TYPEF_ANY_SERVER, 0, NULL); \
    if (!conn || ~conn->connect & CONNECT_OK) \
    { M_print (i18n (1931, "Current session is closed. Try another or open this one.\n")); return 0; }

/* Try to have any server connection ready. */
#define ANYCONN Connection *conn = currconn; \
    if (!conn) currconn = conn = ConnectionFind (TYPEF_ANY_SERVER, 0, NULL);

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
jump_t *CmdUserLookup (const char *cmd, int flags)
{
    jump_t *j;
    for (j = CmdUserTable (); j->f; j++)
        if (   ((flags & CU_DEFAULT) && j->defname && !strcasecmp (cmd, j->defname))
            || ((flags & CU_USER)    && j->name    && !strcasecmp (cmd, j->name)))
            return j;
    return NULL;
}

/*
 * Looks up just the current command name.
 */
const char *CmdUserLookupName (const char *cmd)
{
    jump_t *j;

    j = CmdUserLookup (cmd, CU_DEFAULT);
    if (!j)
        j = CmdUserLookup (cmd, CU_USER);
    if (!j)
        return "";
    if (j->name)
        return j->name;
    return j->defname;
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
alias_t *CmdUserLookupAlias (const char *name)
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
alias_t *CmdUserSetAlias (const char *name, const char *expansion)
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
int CmdUserRemoveAlias (const char *name)
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
    OPENCONN;

    if (data == -1)
    {
        if (!s_parseint (&args, &data))
        {
            M_print (i18n (1703, "Status modes:\n"));
            M_printf ("  %-20s %d\n", i18n (1921, "Online"),         STATUS_ONLINE);
            M_printf ("  %-20s %d\n", i18n (1923, "Away"),           STATUS_AWAY);
            M_printf ("  %-20s %d\n", i18n (1922, "Do not disturb"), STATUS_DND);
            M_printf ("  %-20s %d\n", i18n (1924, "Not Available"),  STATUS_NA);
            M_printf ("  %-20s %d\n", i18n (1927, "Free for chat"),  STATUS_FFC);
            M_printf ("  %-20s %d\n", i18n (1925, "Occupied"),       STATUS_OCC);
            M_printf ("  %-20s %d\n", i18n (1926, "Invisible"),      STATUS_INV);
            return 0;
        }
    }

    if (s_parserem (&args, &arg1))
    {
        if      (data & STATUSF_DND)  ContactOptionsSetStr (&prG->copts, CO_AUTODND,  arg1);
        else if (data & STATUSF_OCC)  ContactOptionsSetStr (&prG->copts, CO_AUTOOCC,  arg1);
        else if (data & STATUSF_NA)   ContactOptionsSetStr (&prG->copts, CO_AUTONA,   arg1);
        else if (data & STATUSF_AWAY) ContactOptionsSetStr (&prG->copts, CO_AUTOAWAY, arg1);
        else if (data & STATUSF_FFC)  ContactOptionsSetStr (&prG->copts, CO_AUTOFFC,  arg1);
        else if (data & STATUSF_INV)  ContactOptionsSetStr (&prG->copts, CO_AUTOINV,  arg1);
    }

    if (conn->ver > 6)
        SnacCliSetstatus (conn, data, 1);
    else
    {
        CmdPktCmdStatusChange (conn, data);
        M_printf ("%s %s\n", s_now, s_status (conn->status));
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
        M_print (i18n (1704, "Groups:\n"));
        M_printf ("  %2d %s\n",  1, i18n (1705, "General"));
        M_printf ("  %2d %s\n",  2, i18n (1706, "Romance"));
        M_printf ("  %2d %s\n",  3, i18n (1707, "Games"));
        M_printf ("  %2d %s\n",  4, i18n (1708, "Students"));
        M_printf ("  %2d %s\n",  6, i18n (1709, "20 something"));
        M_printf ("  %2d %s\n",  7, i18n (1710, "30 something"));
        M_printf ("  %2d %s\n",  8, i18n (1711, "40 something"));
        M_printf ("  %2d %s\n",  9, i18n (1712, "50+"));
        M_printf ("  %2d %s\n", 10, i18n (1713, "Seeking women"));
        M_printf ("  %2d %s\n", 11, i18n (1714, "Seeking men"));
        M_printf ("  %2d %s\n", 49, i18n (1715, "mICQ"));
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
        M_print (i18n (1704, "Groups:\n"));
        M_printf ("  %2d %s\n", conn->ver > 6 ? 0 : -1, i18n (1716, "None"));
        M_printf ("  %2d %s\n",  1, i18n (1705, "General"));
        M_printf ("  %2d %s\n",  2, i18n (1706, "Romance"));
        M_printf ("  %2d %s\n",  3, i18n (1707, "Games"));
        M_printf ("  %2d %s\n",  4, i18n (1708, "Students"));
        M_printf ("  %2d %s\n",  6, i18n (1709, "20 something"));
        M_printf ("  %2d %s\n",  7, i18n (1710, "30 something"));
        M_printf ("  %2d %s\n",  8, i18n (1711, "40 something"));
        M_printf ("  %2d %s\n",  9, i18n (1712, "50+"));
        M_printf ("  %2d %s\n", 10, i18n (1713, "Seeking women"));
        M_printf ("  %2d %s\n", 11, i18n (1714, "Seeking men"));
        M_printf ("  %2d %s\n", 49, i18n (1715, "mICQ"));
    }
    else
    {
        if (conn->ver > 6)
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
    strc_t par;
    char what;

    if (!s_parse (&args, &par)) what = 0;
    else if (!strcasecmp (par->txt, i18n (1447, "Client"))   || !strcasecmp (par->txt, "Client"))   what = 1;
    else if (!strcasecmp (par->txt, i18n (1448, "Message"))  || !strcasecmp (par->txt, "Message"))  what = 2;
    else if (!strcasecmp (par->txt, i18n (1449, "User"))     || !strcasecmp (par->txt, "User"))     what = 3;
    else if (!strcasecmp (par->txt, i18n (1450, "Account"))  || !strcasecmp (par->txt, "Account"))  what = 4;
    else if (!strcasecmp (par->txt, i18n (2171, "Advanced")) || !strcasecmp (par->txt, "Advanced")) what = 5;
    else what = 0;

    if (!what)
    {
        const char *fmt = i18n (2184, "%s%-10s%s - %s\n");
        M_printf ("%s\n", i18n (1442, "Please select one of the help topics below."));
        M_printf (fmt, COLQUOTE, i18n (1448, "Message"), COLNONE,
                 i18n (1446, "Commands relating to sending messages."));
        M_printf (fmt, COLQUOTE, i18n (1447, "Client"), COLNONE,
                 i18n (1443, "Commands relating to mICQ displays and configuration."));
        M_printf (fmt, COLQUOTE, i18n (1449, "User"), COLNONE,
                 i18n (1444, "Commands relating to finding and seeing other users."));
        M_printf (fmt, COLQUOTE, i18n (1450, "Account"), COLNONE,
                 i18n (1445, "Commands relating to your ICQ account."));
        M_printf (fmt, COLQUOTE, i18n (2171, "Advanced"), COLNONE,
                 i18n (2172, "Commands for advanced features."));
    }
    else if (what == 1)
    {
#define CMD_USER_HELP(syn,des) M_printf ("%s" syn "%s\n\t" COLINDENT "%s" COLEXDENT "\n", COLQUOTE, COLNONE, des);
#define CMD_USER_HELP3(syn,d,e,f) M_printf ("%s" syn "%s\n\t" COLINDENT "%s" COLEXDENT "\n", COLQUOTE, COLNONE, d, e, f);
#define CMD_USER_HELP7(syn,a,b,c,d,e,f,g) M_printf ("%s" syn "%s\n\t" COLINDENT "%s\n%s\n%s\n%s\n%s\n%s\n%s" COLEXDENT "\n", COLQUOTE, COLNONE, a,b,c,d,e,f,g);

        CMD_USER_HELP  ("verbose [<nr>]", i18n (1418, "Set the verbosity level, or display verbosity level."));
        CMD_USER_HELP  ("clear", i18n (1419, "Clears the screen."));
        CMD_USER_HELP3 ("sound [%s|%s|event]", i18n (1085, "on"), i18n (1086, "off"),
                        i18n (1420, "Switches beeping when receiving new messages on or off, or using the event script."));
        CMD_USER_HELP  ("autoaway [<timeout>]", i18n (1767, "Toggles auto cycling to away/not available."));
        CMD_USER_HELP  ("auto [on|off]", i18n (1424, "Set whether autoreplying when not online, or displays setting."));
        CMD_USER_HELP  ("auto <status> <message>", i18n (1425, "Sets the message to send as an auto reply for the status."));
        CMD_USER_HELP  ("alias [<alias> [<expansion>]]", i18n (2300, "Set an alias or list current aliases."));
        CMD_USER_HELP  ("unalias <alias>", i18n (2301, "Delete an alias."));
        CMD_USER_HELP  ("trans [<lang|nr>...]", i18n (1800, "Change the working language to <lang> or display string <nr>."));
        CMD_USER_HELP  ("uptime", i18n (1719, "Shows how long mICQ has been running and some statistics."));
        CMD_USER_HELP  ("set <option> <value>", i18n (2044, "Set, clear or display an <option>: hermit, delbs, log, logonoff, auto, uinprompt, autosave, autofinger, linebreak, tabs, silent."));
        CMD_USER_HELP  ("opt [[<contact>|<contact group>] <option> <value>]", i18n (9999, "Set an option for a contact group, a contact or global.\n"));
        CMD_USER_HELP  ("save", i18n (2036, "Save current preferences to disc."));
        CMD_USER_HELP  ("q", i18n (1422, "Logs off and quits."));
        CMD_USER_HELP  ("  ", i18n (1717, "'!' as the first character of a command will execute a shell command (e.g. '!ls', '!dir', '!mkdir temp')"));
    }
    else if (what == 2)
    {
        CMD_USER_HELP  ("msg <contacts> [<message>]", i18n (1409, "Sends a message to <contacts>."));
        CMD_USER_HELP  ("a <message>", i18n (1412, "Sends a message to the last person you sent a message to."));
        CMD_USER_HELP  ("r <message>", i18n (1414, "Replies to the last person to send you a message."));
        CMD_USER_HELP  ("url <contacts> <url> <message>", i18n (1410, "Sends a url and message to <contacts>."));
        CMD_USER_HELP  ("sms <nick|cell> <message>", i18n (2039, "Sends a message to a (<nick>'s) <cell> phone."));
        CMD_USER_HELP  ("getauto [auto|away|na|dnd|occ|ffc] [<contacts>]", i18n (2304, "Get automatic reply from all <contacts> for current status, away, not available, do not disturb, occupied, or free for chat."));
        CMD_USER_HELP  ("auth [grant|deny|req|add] <contacts>", i18n (2313, "Grant, deny, request or acknowledge authorization from/to <contacts> to you/them to their/your contact list."));
        CMD_USER_HELP  ("resend <contacts>", i18n (1770, "Resend your last message to <contacts>."));
        CMD_USER_HELP  ("last [<contacts>]", i18n (1403, "Displays the last message received from <contacts> or from everyone."));
        CMD_USER_HELP  ("tabs", i18n (1098, "Display a list of nicknames that you can tab through.")); 
        CMD_USER_HELP  ("  ", i18n (1720, "<contacts> is a comma separated list of UINs and nick names of users."));
        CMD_USER_HELP  ("  ", i18n (1721, "Sending a blank message will put the client into multiline mode.\nUse . on a line by itself to end message.\nUse # on a line by itself to cancel the message."));
    }
    else if (what == 3)
    {
        CMD_USER_HELP  ("rand [<nr>]", i18n (1415, "Finds a random user in interest group <nr> or lists the groups."));
        CMD_USER_HELP  ("s <uin|nick>", i18n (1400, "Shows locally stored info on <uin> or <nick>, or on yourself."));
        CMD_USER_HELP  ("e", i18n (1407, "Displays the current status of online people on your contact list."));
        CMD_USER_HELP  ("w", i18n (1416, "Displays the current status of everyone on your contact list."));
        CMD_USER_HELP  ("ee", i18n (2177, "Displays verbosely the current status of online people on your contact list."));
        CMD_USER_HELP  ("ww", i18n (2176, "Displays verbosely the current status of everyone on your contact list."));
        CMD_USER_HELP  ("eg", i18n (2307, "Displays the current status of online people on your contact list, sorted by contact group."));
        CMD_USER_HELP  ("wg", i18n (2308, "Displays the current status of everyone on your contact list, sorted by contact group."));
        CMD_USER_HELP  ("eeg", i18n (2309, "Displays verbosely the current status of online people on your contact list, sorted by contact group."));
        CMD_USER_HELP  ("wwg", i18n (2310, "Displays verbosely the current status of everyone on your contact list, sorted by contact group."));
        CMD_USER_HELP  ("ewide", i18n (2042, "Displays a list of online people on your contact list in a screen wide format.")); 
        CMD_USER_HELP  ("wide", i18n (1801, "Displays a list of people on your contact list in a screen wide format.")); 
        CMD_USER_HELP  ("finger <uin|nick>", i18n (1430, "Displays general info on <uin> or <nick>."));
        CMD_USER_HELP  ("i", i18n (1405, "Lists ignored nicks/uins."));
        CMD_USER_HELP  ("search [<email>|<nick>|<first> <last>]", i18n (1429, "Searches for an ICQ user."));
        CMD_USER_HELP  ("addgroup <group> [<contacts>]", i18n (1428, "Adds all contacts in <contacts> to contact group <group>."));
        CMD_USER_HELP  ("addalias <uin|nick> [<new nick>]", i18n (2311, "Adds <uin> as <new nick>, or add alias <new nick> for <uin> or <nick>."));
        CMD_USER_HELP  ("remgroup [all] group <contacts>", i18n (2043, "Remove all contacts in <contacts> from contact group <group>."));
        CMD_USER_HELP  ("remalias [all] <contacts>", i18n (2312, "Remove all aliases in <contacts>, removes contact if 'all' is given."));
        CMD_USER_HELP  ("togig <contacts>", i18n (1404, "Toggles ignoring/unignoring the <contacts>."));
        CMD_USER_HELP  ("toginv <contacts>", i18n (2045, "Toggles your visibility to <contacts> when you're online."));
        CMD_USER_HELP  ("togvis <contacts>", i18n (1406, "Toggles your visibility to <contacts> when you're invisible."));
    }
    else if (what == 4)
    {
        CMD_USER_HELP  ("reg <password>", i18n (1426, "Creates a new UIN with the specified password."));
        CMD_USER_HELP  ("pass <password>", i18n (1408, "Changes your password to <password>."));
        CMD_USER_HELP  ("change <status> [<away-message>]", i18n (1427, "Changes your status to the status number, or list the available modes."));
        CMD_USER_HELP7 ("online|away|na|occ|dnd|ffc|inv [<away-message>]", i18n (1431, "Set status to \"online\"."),
                  i18n (1432, "Set status to \"away\"."), i18n (1433, "Set status to \"not available\"."),
                  i18n (1434, "Set status to \"occupied\"."), i18n (1435, "Set status to \"do not disturb\"."),
                  i18n (1436, "Set status to \"free for chat\"."), i18n (1437, "Set status to \"invisible\"."));
        CMD_USER_HELP  ("update", i18n (1438, "Updates your basic info (email, nickname, etc.)."));
        CMD_USER_HELP  ("other", i18n (1401, "Updates more user info like age and sex."));
        CMD_USER_HELP  ("about", i18n (1402, "Updates your about user info."));
        CMD_USER_HELP  ("setr <nr>", i18n (1439, "Sets your random user group."));
    }
    else if (what == 5)
    {
        M_printf (i18n (2314, "These are advanced commands. Be sure to have read the manual pages for complete information.\n"));
        CMD_USER_HELP  ("meta [show|load|save|set|get|rget] <contacts>", i18n (2305, "Handle meta data of contacts."));
        CMD_USER_HELP  ("peer open|close|off <uin|nick>", i18n (2037, "Open or close a peer-to-peer connection, or disable using peer-to-peer connections for <uin> or <nick>."));
        CMD_USER_HELP  ("peer file <contacts> <file> <description>", i18n (2179, "Send all <contacts> a single file."));
        CMD_USER_HELP  ("peer files <uin|nick> <file1> <as1> ... <fileN> <asN> <description>", i18n (2180, "Send <uin> or <nick> several files."));
        CMD_USER_HELP  ("peer accept <uin|nick> [<id>]", i18n (2319, "Accept an incoming file transfer from <uin> or <nick>."));
        CMD_USER_HELP  ("conn open|login [<nr>]", i18n (2038, "Opens connection number <nr>, or the first server connection."));
        CMD_USER_HELP  ("conn close|remove <nr>", i18n (2181, "Closes or removes connection number <nr>."));
        CMD_USER_HELP  ("conn select [<nr>]", i18n (2182, "Select server connection number <nr> as current server connection."));
        CMD_USER_HELP  ("contact [show|diff|import]", i18n (2306, "Request server side contact list and show all or new contacts or import."));
        CMD_USER_HELP  ("peek <contacts>", i18n (2183, "Check all <contacts> whether they are offline or invisible."));
    }
    return 0;
}

/*
 * Sets a new password.
 */
static JUMP_F(CmdUserPass)
{
    char *arg1 = NULL;
    OPENCONN;
    
    if (!s_parserem (&args, &arg1))
        M_print (i18n (2012, "No password given.\n"));
    else
    {
        if (*arg1 == '\xc3' && arg1[1] == '\xb3')
        {
            M_printf (i18n (2198, "Unsuitable password '%s' - may not start with byte 0xf3.\n"), arg1);
            return 0;
        }
        if (conn->ver < 6)
            CmdPktCmdMetaPass (conn, arg1);
        else
            SnacCliMetasetpass (conn, arg1);
        conn->passwd = strdup (arg1);
        if (conn->spref->passwd && strlen (conn->spref->passwd))
        {
            M_print (i18n (2122, "Note: You need to 'save' to write new password to disc.\n"));
            conn->spref->passwd = strdup (arg1);
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
    
    if (conn->ver < 6)
    {
        M_print (i18n (2013, "This command is v8 only.\n"));
        return 0;
    }
    if (s_parsenick (&args, &cont, conn))
    {
        CONTACT_GENERAL (cont);
        cell = args;
        if ((!cont->meta_general->cellular || !cont->meta_general->cellular[0])
            && s_parseint (&cell, &i) && s_parse (&args, &par))
        {
            s_repl (&cont->meta_general->cellular, par->txt);
            cont->updated = 0;
        }
        cell = cont->meta_general->cellular;
    }
    if (!cell || !cell)
    {
        if (!s_parse (&args, &par))
        {
            M_print (i18n (2014, "No number given.\n"));
            return 0;
        }
        cell = par->txt;
    }
    if (cell[0] != '+' || cell[1] == '0')
    {
        M_printf (i18n (2250, "Number '%s' is not of the format +<countrycode><cellprovider><number>.\n"), cell);
        if (cont && cont->meta_general)
            s_repl (&cont->meta_general->cellular, NULL);
        return 0;
    }
    cellv = strdup (cell);
    if (!s_parserem (&args, &text))
        M_print (i18n (2015, "No message given.\n"));
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
    Contact *cont = NULL;
    OPENCONN;

    while (*args || data)
    {
        if (!data && !s_parsenick_s (&args, &cont, MULTI_SEP, conn))
        {
            M_printf (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
            return 0;
        }
        if (*args == ',')
            args++;
        if (data)
        {
            cont = ContactUIN (conn, uiG.last_rcvd_uin);
            if (!cont)
                return 0;
        }
        IMCliInfo (conn, cont, 0);
    }
    return 0;
}

/*
 * Peeks whether a user is really offline.
 */
static JUMP_F(CmdUserPeek)
{
    Contact *cont = NULL;
    OPENCONN;
    
    if (conn->ver < 6)
    {
        M_print (i18n (2013, "This command is v8 only.\n"));
        return 0;
    }
    
    while (*args)
    {
        if (!s_parsenick_s (&args, &cont, MULTI_SEP, conn))
        {
            M_printf (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
            return 0;
        }
        if (*args == ',')
            args++;

        SnacCliSendmsg2 (conn, cont, ExtraSet (NULL, EXTRA_MESSAGE, MSG_GET_PEEK, ""));
    }
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
        UDWORD i, l = 0;

        if (s_parseint (&args, &i))
        {
            M_printf ("%3ld:%s\n", i, i18n (i, i18n (1078, "No translation available.")));
            one = 1;
            continue;
        }
        if (s_parse (&args, &par))
        {
            if (!strcmp (par->txt, "all"))
            {
                for (i = l = 1000; i - l < 100; i++)
                {
                    p = i18n (i, NULL);
                    if (p)
                    {
                        l = i;
                        M_printf ("%3ld:%s\n", i, p);
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
                    M_printf ("%s\n", t);
                    free (t);
                    continue;
                }
                i = i18nOpen (par->txt);
                if (i == -1)
                    M_printf (i18n (2316, "Translation %s not found.\n"), par->txt);
                else
                    M_printf (i18n (2318, "No translation (%s) loaded (%ld entries).\n"), par->txt, i);
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
            M_printf (i18n (-1, s),
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
    Contact *cont;
    int one = 0;
    ANYCONN;

    if (!data)
    {
        const char *argst = args;
        strc_t par;
        if (!s_parse (&args, &par))     data = 0;
        else if (!strcmp (par->txt, "auto")) data = 0;
        else if (!strcmp (par->txt, "away")) data = MSGF_GETAUTO | MSG_GET_AWAY;
        else if (!strcmp (par->txt, "na"))   data = MSGF_GETAUTO | MSG_GET_NA;
        else if (!strcmp (par->txt, "dnd"))  data = MSGF_GETAUTO | MSG_GET_DND;
        else if (!strcmp (par->txt, "ffc"))  data = MSGF_GETAUTO | MSG_GET_FFC;
        else if (!strcmp (par->txt, "occ"))  data = MSGF_GETAUTO | MSG_GET_OCC;
#ifdef WIP
        else if (!strcmp (par->txt, "ver"))  data = MSGF_GETAUTO | MSG_GET_VER;
#endif
        else args = argst;
    }
    while (s_parsenick_s (&args, &cont, MULTI_SEP, conn))
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
        IMCliMsg (conn, cont, ExtraSet (NULL, EXTRA_MESSAGE, cdata, ""));
        if (*args == ',')
            args++;
    }
    if (*args)
        M_printf (i18n (1845, "Nick %s unknown.\n"), args);
    if (data || *args || one)
        return 0;
    M_print (i18n (2056, "getauto [auto] <nick> - Get the auto-response from the contact.\n"));
    M_print (i18n (2057, "getauto away   <nick> - Get the auto-response for away from the contact.\n"));
    M_print (i18n (2058, "getauto na     <nick> - Get the auto-response for not available from the contact.\n"));
    M_print (i18n (2059, "getauto dnd    <nick> - Get the auto-response for do not disturb from the contact.\n"));
    M_print (i18n (2060, "getauto ffc    <nick> - Get the auto-response for free for chat from the contact.\n"));
    return 0;
}

/*
 * Manually handles peer-to-peer (TCP) connections.
 */
static JUMP_F(CmdUserPeer)
{
#ifdef ENABLE_PEER2PEER
    Contact *cont = NULL;
    Connection *list;
    UDWORD seq = 0;
    ANYCONN;

    if (!conn || !(list = conn->assoc))
    {
        M_print (i18n (2011, "You do not have a listening peer-to-peer connection.\n"));
        return 0;
    }

    if (!data)
    {
        strc_t par;
        if (!s_parse (&args, &par)) data = 0;
        else if (!strcmp (par->txt, "open"))   data = 1;
        else if (!strcmp (par->txt, "close"))  data = 2;
        else if (!strcmp (par->txt, "reset"))  data = 2;
        else if (!strcmp (par->txt, "off"))    data = 3;
        else if (!strcmp (par->txt, "file"))   data = 4;
        else if (!strcmp (par->txt, "files"))  data = 5;
        else if (!strcmp (par->txt, "accept")) data = 6;
        else if (!strcmp (par->txt, "deny"))   data = 7;
        else if (!strcmp (par->txt, "auto"))   data = 10;
        else if (!strcmp (par->txt, "away"))   data = 11;
        else if (!strcmp (par->txt, "na"))     data = 12;
        else if (!strcmp (par->txt, "dnd"))    data = 13;
        else if (!strcmp (par->txt, "ffc"))    data = 14;
    }
    while (data && s_parsenick_s (&args, &cont, MULTI_SEP, conn))
    {
        switch (data)
        {
            case 1:
                if (!TCPDirectOpen  (list, cont))
                    M_printf (i18n (2142, "Direct connection with %s not possible.\n"), cont->nick);
                break;
            case 2:
                TCPDirectClose (list, cont);
                break;
            case 3:
                TCPDirectOff   (list, cont);
                break;
            case 4:
                {
                    strc_t par;
                    const char *files[1], *ass[1];
                    char *des = NULL, *file;
                    
                    if (!s_parse (&args, &par))
                    {
                        M_print (i18n (2158, "No file name given.\n"));
                        return 0;
                    }
                    files[0] = file = strdup (par->txt);
                    if (!s_parserem (&args, &des))
                        des = file;
                    des = strdup (des);

                    ass[0] = (strchr (file, '/')) ? strrchr (file, '/') + 1 : file;
                    
                    if (!TCPSendFiles (list, cont, des, files, ass, 1))
                        M_printf (i18n (2142, "Direct connection with %s not possible.\n"), cont->nick);
                    
                    free (file);
                    free (des);
                }
                return 0;
            case 5:
                {
                    char *files[10], *ass[10], *des = NULL;
                    const char *as;
                    int count;
                    
                    for (count = 0; count < 10; count++)
                    {
                        strc_t par;
                        if (!s_parse (&args, &par))
                        {
                            des = strdup (i18n (2159, "Some files."));
                            break;
                        }
                        files[count] = des = strdup (par->txt);
                        if (!s_parse (&args, &par))
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
                        M_print (i18n (2158, "No file name given.\n"));
                        return 0;
                    }
                    if (!TCPSendFiles (list, cont, des, (const char **)files, (const char **)ass, count))
                        M_printf (i18n (2142, "Direct connection with %s not possible.\n"), cont->nick);
                    
                    while (count--)
                    {
                       free (ass[count]);
                       free (files[count]);
                    }
                    free (des);
                }
                return 0;
            case 6:
                {
                    Event *event, *revent = NULL;

                    s_parseint (&args, &seq);
                    if (!(event = QueueDequeue2 (conn, QUEUE_ACKNOWLEDGE, seq, cont->uin)) || !(revent = event->rel) ||
                        !(revent = QueueDequeue2 (event->rel->conn, event->rel->type, event->rel->seq, event->rel->uin)))
                    {
                        M_printf (i18n (2258, "No pending incoming file transfer request for %s with (sequence %ld) found.\n"),
                                  cont->nick, seq);
                    }
                    else
                        revent->callback (revent);
                    EventD (event);
                }
                break;
            case 7:
                {
                    Event *event, *revent = NULL;

                    s_parseint (&args, &seq);
                    if (!(event = QueueDequeue2 (conn, QUEUE_ACKNOWLEDGE, seq, cont->uin)) || !(revent = event->rel) ||
                        !(revent = QueueDequeue2 (event->rel->conn, event->rel->type, event->rel->seq, event->rel->uin)))
                    {
                        M_printf (i18n (2258, "No pending incoming file transfer request for %s with (sequence %ld) found.\n"),
                                  cont->nick, seq);
                    }
                    else
                        event->callback (revent);
                    EventD (event);
                }
                break;
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
                M_printf (i18n (2260, "Please try the getauto command instead.\n"));
                return 0;
        }
        if (*args == ',')
            args++;
    }
    if (*args)
        M_printf (i18n (1845, "Nick %s unknown.\n"), args);
    if (data || *args)
        return 0;
    M_print (i18n (1846, "Opens and closes direct (peer to peer) connections:\n"));
    M_print (i18n (1847, "peer open  <nick> - Opens direct connection.\n"));
    M_print (i18n (1848, "peer close <nick> - Closes/resets direct connection(s).\n"));
    M_print (i18n (1870, "peer off   <nick> - Closes direct connection(s) and don't try it again.\n"));
    M_print (i18n (2160, "peer file  <nick> <file> [<description>]\n"));
    M_print (i18n (2110, "peer files <nick> <file1> <as1> ... [<description>]\n"));
    M_print (i18n (2111, "                  - Send file1 as as1, ..., with description.\n"));
    M_print (i18n (2112, "                  - as = '/': strip path, as = '.': as is\n"));
    M_print (i18n (2320, "peer accept <nick> [<id>]\n                  - accept an incoming file transfer.\n"));
#else
    M_print (i18n (1866, "This version of mICQ is compiled without direct connection (peer to peer) support.\n"));
#endif
    return 0;
}

/*
 * Changes automatic reply messages.
 */
static JUMP_F(CmdUserAuto)
{
    strc_t par;

    if (!s_parse (&args, &par))
    {
        M_printf (i18n (1724, "Automatic replies are %s.\n"),
                 prG->flags & FLAG_AUTOREPLY ? i18n (1085, "on") : i18n (1086, "off"));
        return 0;
    }

    if      (!strcasecmp (par->txt, "on")  || !strcasecmp (par->txt, i18n (1085, "on")))
    {
        prG->flags |= FLAG_AUTOREPLY;
        M_printf (i18n (1724, "Automatic replies are %s.\n"), i18n (1085, "on"));
        return 0;
    }
    else if (!strcasecmp (par->txt, "off") || !strcasecmp (par->txt, i18n (1086, "off")))
    {
        prG->flags &= ~FLAG_AUTOREPLY;
        M_printf (i18n (1724, "Automatic replies are %s.\n"), i18n (1086, "off"));
        return 0;
    }
    M_printf (i18n (9999, "Auto reply messages are now contact options, see the 'opt' command.\n"));

    return 0;
}

/*
 * Add an alias.
 */

static JUMP_F(CmdUserAlias)
{
    strc_t name;
    char *exp = NULL, *nname;

    if (!s_parse_s (&args, &name, " \t\r\n="))
    {
        alias_t *node;

        for (node = aliases; node; node = node->next)
            M_printf ("alias %s %s\n", node->name, node->expansion);
        
        return 0;
    }
    
    if ((exp = strpbrk (name->txt, " \t\r\n")))
    {
        M_printf (i18n (2303, "Invalid character 0x%x in alias name.\n"), *exp);
        return 0;
    }

    if (*args == '=')
        args++;
    
    nname = strdup (name->txt);
    
    if (s_parserem (&args, &exp))
        CmdUserSetAlias (nname, exp);
    else
    {
        alias_t *node;

        for (node = aliases; node; node = node->next)
            if (!strcasecmp (node->name, nname))
            {
                M_printf ("alias %s %s\n", node->name, node->expansion);
                free (nname);
                return 0;
            }
        M_print (i18n (2297, "Alias to what?\n"));
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

    if (!s_parse (&args, &par))
        M_print (i18n (2298, "Remove which alias?\n"));
    else if (!CmdUserRemoveAlias (par->txt))
        M_print (i18n (2299, "Alias doesn't exist.\n"));
    
    return 0;
}

/*
 * Resend your last message
 */
static JUMP_F (CmdUserResend)
{
    Contact *cont = NULL;
    OPENCONN;

    if (!uiG.last_message_sent) 
    {
        M_print (i18n (1771, "You haven't sent a message to anyone yet!\n"));
        return 0;
    }

    if (!s_parsenick_s (&args, &cont, MULTI_SEP, conn))
    {
        if (*args)
            M_printf (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
        else
            M_print (i18n (1676, "Need uin/nick to send to.\n"));
        return 0;
    }
    
    while (1)
    {
        IMCliMsg (conn, cont, ExtraSet (NULL, EXTRA_MESSAGE, uiG.last_message_sent_type, uiG.last_message_sent));
        uiG.last_sent_uin = cont->uin;

        if (*args == ',')
            args++;
        if (!s_parsenick_s (&args, &cont, MULTI_SEP, conn))
        {
            if (*args)
                M_printf (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
            return 0;
        }
    }
}

/*
 * Send an instant message of any kind.
 */
static JUMP_F (CmdUserAnyMess)
{
    Contact *cont;
    strc_t par;
    UDWORD i, f = 0;
    static str_s t;
    ANYCONN;

    if (!(data & 3))
    {
        if (!s_parse (&args, &par))
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
    s_init (&t, "", 100);

    if (!s_parsenick (&args, &cont, conn))
        return 0;
    
    if (!s_parse (&args, &par))
        return 0;
    
    s_catf (&t, "%s", par->txt);

    while (s_parse (&args, &par))
        s_catf (&t, "%c%s", Conv0xFE, par->txt);
        
    if (data & 1)
    {
#ifdef ENABLE_PEER2PEER
        if (!conn->assoc || !TCPDirectOpen  (conn->assoc, cont))
        {
            M_printf (i18n (2142, "Direct connection with %s not possible.\n"), cont->nick);
            return 0;
        }
        TCPSendMsg (conn->assoc, cont, t.txt, data >> 2);
    }
#endif
    else
    {
        if (conn->type != TYPE_SERVER)
            CmdPktCmdSendMessage (conn, cont, t.txt, data >> 2);
        else if (f != 2)
            SnacCliSendmsg (conn, cont, t.txt, data >> 2, f);
        else
            SnacCliSendmsg2 (conn, cont, ExtraSet (ExtraSet (NULL, EXTRA_FORCE, 1, NULL), EXTRA_MESSAGE, data >> 2, t.txt));
    }
    return 0;
}

/*
 * Send an instant message.
 */
static JUMP_F (CmdUserMessage)
{
    static str_s t;
    static UDWORD uinlist[20] = { 0 };
    char *arg1 = NULL;
    UDWORD i;
    Contact *cont = NULL;
    OPENCONN;

    if (!status)
    {
        switch (data)
        {
            case 1:
                if (!*args)
                {
                    M_print (i18n (2235, "No nick name given.\n"));
                    return 0;
                }
                for (i = 0; *args; args++)
                {
                    if (!s_parsenick_s (&args, &cont, MULTI_SEP, conn))
                    {
                        M_printf (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
                        return 0;
                    }
                    uinlist[i++] = cont->uin;
                    if (*args != ',')
                        break;
                }
                uinlist[i] = 0;
                break;
            case 2:
                if (!uiG.last_rcvd_uin)
                {
                    M_print (i18n (1741, "Must receive a message first.\n"));
                    return 0;
                }
                uinlist[0] = uiG.last_rcvd_uin;
                uinlist[1] = 0;
                break;
            case 4:
                if (!uiG.last_sent_uin)
                {
                    M_print (i18n (1742, "Must write a message first.\n"));
                    return 0;
                }
                uinlist[0] = uiG.last_sent_uin;
                uinlist[1] = 0;
                break;
            default:
                assert (0);
        }
        if (!s_parserem (&args, &arg1))
            arg1 = NULL;
        if (arg1 && (arg1[strlen (arg1) - 1] != '\\'))
        {
            s_repl (&uiG.last_message_sent, arg1);
            uiG.last_message_sent_type = MSG_NORM;
            for (i = 0; uinlist[i]; i++)
            {
                if (!(cont = ContactUIN (conn, uinlist[i])))
                    continue;
                IMCliMsg (conn, cont, ExtraSet (NULL, EXTRA_MESSAGE, MSG_NORM, arg1));
                uiG.last_sent_uin = cont->uin;
                TabAddOut (cont);
            }
            return 0;
        }
        if (!uinlist[1])
            M_printf (i18n (2131, "Composing message to %s%s%s:\n"), COLCONTACT, ContactUIN (conn, uinlist[0])->nick, COLNONE);
        else
            M_printf (i18n (2131, "Composing message to %s%s%s:\n"), COLQUOTE, i18n (2220, "several"), COLNONE);
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
    else
    {
        if (!strcmp (args, END_MSG_STR))
        {
            arg1 = t.txt + t.len - 1;
            while (*t.txt && strchr ("\r\n\t ", *arg1))
                *arg1-- = '\0';
            s_repl (&uiG.last_message_sent, t.txt);
            uiG.last_message_sent_type = MSG_NORM;
            for (i = 0; uinlist[i]; i++)
            {
                if (!(cont = ContactUIN (conn, uinlist[i])))
                    continue;
                IMCliMsg (conn, cont, ExtraSet (NULL, EXTRA_MESSAGE, MSG_NORM, t.txt));
                uiG.last_sent_uin = cont->uin;
                TabAddOut (cont);
            }
            return 0;
        }
        else if (!strcmp (args, CANCEL_MSG_STR))
        {
            M_print (i18n (1038, "Message canceled.\n"));
            return 0;
        }
        else
        {
            s_cat (&t, args);
            s_catc (&t, '\r');
            s_catc (&t, '\n');
        }
    }
    R_setprompt (i18n (1041, "msg> "));
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
        M_printf (i18n (2115, "'%s' is not an integer.\n"), args);
        return 0;
    }
    M_printf (i18n (1060, "Verbosity level is %ld.\n"), prG->verbose);
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

static int __tuin, __lenuin, __lennick, __lenstat, __lenid, __totallen, __l;

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
    if (__lennick > uiG.nick_len)
        uiG.nick_len = __lennick;
    while (__tuin)
        __lenuin++, __tuin /= 10;
    __totallen = 1 + __lennick + 1 + __lenstat + 3 + __lenid + 2;
    if (prG->verbose)
        __totallen += 29;
    if (data & 2)
        __totallen += 2 + 3 + 1 + 1 + __lenuin + 19;

}

static void __showcontact (Connection *conn, Contact *cont, UWORD data)
{
    Connection *peer;
    ContactAlias *alias;
    char *stat, *ver = NULL, *ver2 = NULL;
    const char *ul = "";
    char tbuf[100];
    
    peer = (conn && conn->assoc) ? ConnectionFind (TYPE_MSGDIRECT, cont->uin, conn->assoc) : NULL;
    
    stat = strdup (s_sprintf ("(%s)", s_status (cont->status)));
    if (cont->version)
        ver  = strdup (s_sprintf ("[%s]", cont->version));
    if (prG->verbose && cont->dc)
        ver2 = strdup (s_sprintf (" <%08x:%08x:%08x>", (unsigned int)cont->dc->id1,
                                   (unsigned int)cont->dc->id2, (unsigned int)cont->dc->id3));

    if (cont->seen_time != -1L && data & 2)
        strftime (tbuf, sizeof (tbuf), " %Y-%m-%d %H:%M:%S", localtime (&cont->seen_time));
    else if (data & 2)
        strcpy (tbuf, "                    ");
    else
        tbuf[0] = '\0';
    
#ifdef CONFIG_UNDERLINE
    if (!(++__l % 5))
        ul = ESC "[4m";
#endif
    if (data & 2)
        M_printf ("%s%s%c%c%c%1.1d%c%s%s %*ld", COLSERVER, ul,
             !cont->group                        ? '#' : ' ',
             ContactPrefVal (cont,  CO_INTIMATE) ? '*' :
              ContactPrefVal (cont, CO_HIDEFROM) ? '-' : ' ',
             ContactPrefVal (cont,  CO_IGNORE)   ? '^' : ' ',
             cont->dc ? cont->dc->version : 0,
             peer ? (
              peer->connect & CONNECT_OK         ? '&' :
              peer->connect & CONNECT_FAIL       ? '|' :
              peer->connect & CONNECT_MASK       ? ':' : '.' ) :
              cont->dc && cont->dc->version && cont->dc->port && ~cont->dc->port &&
              cont->dc->ip_rem && ~cont->dc->ip_rem ? '^' : ' ',
             COLNONE, ul, (int)__lenuin, cont->uin);

    M_printf ("%s%s%c%s%s%-*s%s%s %s%s%-*s%s%s %-*s%s%s%s\n",
             COLSERVER, ul, data & 2                       ? ' ' :
             !cont->group                       ? '#' :
             ContactPrefVal (cont, CO_INTIMATE) ? '*' :
             ContactPrefVal (cont, CO_HIDEFROM) ? '-' :
             ContactPrefVal (cont, CO_IGNORE)   ? '^' :
             !peer                              ? ' ' :
             peer->connect & CONNECT_OK         ? '&' :
             peer->connect & CONNECT_FAIL       ? '|' :
             peer->connect & CONNECT_MASK       ? ':' : '.' ,
             COLCONTACT, ul, __lennick + s_delta (cont->nick), cont->nick,
             COLNONE, ul, COLQUOTE, ul, __lenstat + 2 + s_delta (stat), stat,
             COLNONE, ul, __lenid + 2 + s_delta (ver ? ver : ""), ver ? ver : "",
             ver2 ? ver2 : "", tbuf, COLNONE);

    for (alias = cont->alias; alias && (data & 2); alias = alias->more)
    {
        M_printf ("%s%s+     %*ld", COLSERVER, ul, (int)__lenuin, cont->uin);
        M_printf ("%s%s %s%s%-*s%s%s %s%s%-*s%s%s %-*s%s%s%s\n",
                  COLSERVER, ul, COLCONTACT, ul, __lennick + s_delta (alias->alias), alias->alias,
                  COLNONE, ul, COLQUOTE, ul, __lenstat + 2 + s_delta (stat), stat,
                  COLNONE, ul, __lenid + 2 + s_delta (ver ? ver : ""), ver ? ver : "",
                  ver2 ? ver2 : "", tbuf, COLNONE);
    }
    free (stat);
    s_free (ver);
    s_free (ver2);
}

/*
 * Shows the contact list in a very detailed way.
 *
 * data & 32: sort by groups
 * data & 16: show _only_ own status
 * data & 8: show only given nick
 * data & 4: show own status
 * data & 2: be verbose
 * data & 1: do not show offline
 */
static JUMP_F(CmdUserStatusDetail)
{
    ContactGroup *cg = NULL, *tcg = NULL;
    int i, j, k;
    Contact *cont = NULL;
    strc_t par;
    UDWORD stati[] = { 0xfffffffe, STATUS_OFFLINE, STATUS_DND,    STATUS_OCC, STATUS_NA,
                                   STATUS_AWAY,    STATUS_ONLINE, STATUS_FFC, STATUSF_BIRTH };
    ANYCONN;

    if (!data)
        s_parseint (&args, &data);

    if (~data & 16)
    {
        const char *argst = args;
        if (s_parse (&argst, &par))
            if ((tcg = cg = ContactGroupFind (conn, 0, par->txt)))
                args = argst;
    }

    if ((data & 8) && !cg && (!conn || !s_parsenick (&args, &cont, conn)) && *args)
    {
        M_printf (i18n (1700, "%s is not a valid user in your list.\n"), args);
        return 0;
    }

    if (cont)
    {
        char *t1, *t2;
        UBYTE id;

        __initcheck ();
        __checkcontact (cont, data);
        __donecheck (data);
        __showcontact (conn, cont, data);

        if (cont->dc)
        {
            M_printf ("    %-15s %s / %s:%ld\n    %s %d    %s (%d)    %s %08lx\n",
                  i18n (1642, "IP:"), t1 = strdup (s_ip (cont->dc->ip_rem)),
                  t2 = strdup (s_ip (cont->dc->ip_loc)), cont->dc->port,
                  i18n (1453, "TCP version:"), cont->dc->version,
                  cont->dc->type == 4 ? i18n (1493, "Peer-to-Peer")
                    : i18n (1494, "Server Only"), cont->dc->type,
                  i18n (2026, "TCP cookie:"), cont->dc->cookie);
            free (t1);
            free (t2);
        }
        for (i = id = 0; id < CAP_MAX; id++)
            if (cont->caps & (1 << id))
            {
                Cap *cap = PacketCap (id);
                if (i++)
                    M_print (", ");
                else
                    M_printf ("    %s", i18n (2192, "Capabilities: "));
                M_print (cap->name);
                if (cap->name[4] == 'U' && cap->name[5] == 'N')
                {
                    M_print (": ");
                    M_print (s_dump (cap->cap, 16));
                }
            }
        if (i)
            M_print ("\n");
        return 0;
    }

    if (data & 32)
    {
        if (!(tcg = ContactGroupC (conn, 0, NULL)))
        {
            M_print (i18n (2118, "Out of memory.\n"));
            return 0;
        }
        cg = conn->contacts;
        for (j = 0; (cont = ContactIndex (cg, j)); j++)
            if (!ContactFind (tcg, 0, cont->uin, NULL))
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
            M_printf ("%s %s%10lu%s ", s_now, COLCONTACT, conn->uin, COLNONE);
            M_printf (i18n (2211, "Your status is %s.\n"), s_status (conn->status));
        }
        if (data & 16)
            return 0;
    }

    for (k = -1; (k == -1) ? (tcg ? (cg = tcg) : cg) : (cg = ContactGroupIndex (k)); k++)
    {
        if (k != -1 && (cg == conn->contacts || cg == tcg))
            continue;
        if (cg->serv != conn)
            continue;
#ifdef CONFIG_UNDERLINE
        __l = 0;
#endif
        M_print (COLQUOTE);
        if (cg != tcg && cg != conn->contacts)
        {
            if (__totallen > c_strlen (cg->name) + 1)
            {
                for (i = j = (__totallen - c_strlen (cg->name) - 1) / 2; i >= 20; i -= 20)
                    M_print ("====================");
                M_printf ("%.*s", (int)i, "====================");
                M_printf (" %s%s%s ", COLCONTACT, cg->name, COLQUOTE);
                for (i = __totallen - j - c_strlen (cg->name) - 2; i >= 20; i -= 20)
                    M_print ("====================");
            }
            else
                M_printf (" %s%s%s ", COLCONTACT, cg->name, COLQUOTE);
        }
        else
            for (i = __totallen; i >= 20; i -= 20)
                M_print ("====================");
        M_printf ("%.*s%s\n", (int)i, "====================", COLNONE);

        for (i = (data & 1 ? 2 : 0); i < 9; i++)
        {
            status = stati[i];
            for (j = 0; (cont = ContactIndex (cg, j)); j++)
            {
                if (__status (cont) != status)
                    continue;
                __showcontact (conn, cont, data);
            }
        }
        if (~data & 32)
            break;
    }
    if (data & 32)
        ContactGroupD (tcg);
    M_print (COLQUOTE);
    for (i = __totallen; i >= 20; i -= 20)
        M_print ("====================");
    M_printf ("%.*s%s\n", (int)i, "====================", COLNONE);
    return 0;
}

/*
 * Show meta information remembered from a contact.
 */
static JUMP_F(CmdUserStatusMeta)
{
    Contact *cont;
    strc_t par;
    ANYCONN;

    if (!data)
    {
        if (!s_parse (&args, &par))     data = 0;
        else if (!strcmp (par->txt, "show")) data = 1;
        else if (!strcmp (par->txt, "load")) data = 2;
        else if (!strcmp (par->txt, "save")) data = 3;
        else if (!strcmp (par->txt, "set"))  data = 4;
        else if (!strcmp (par->txt, "get"))  data = 5;
        else if (!strcmp (par->txt, "rget")) data = 6;
        
        if (!data)
        {
            M_printf (i18n (2333, "%s [show|load|save|set|get|rget] <contacts> - handle meta data for contacts.\n"), CmdUserLookupName ("meta"));
            M_print  (i18n (2334, "  show - show current known meta data\n"));
            M_print  (i18n (2335, "  load - load from file and show meta data\n"));
            M_print  (i18n (2336, "  save - save meta data to file\n"));
            M_print  (i18n (2337, "  set  - upload own meta data to server\n"));
            M_print  (i18n (2338, "  get  - query server for meta data\n"));
            M_print  (i18n (2339, "  rget - query server for last sender's meta data\n"));
            return 0;
        }
    }

    while (*args)
    {
        if (data == 6)
            cont = ContactUIN (conn, uiG.last_rcvd_uin);
        else if (data != 4 && conn && !s_parsenick (&args, &cont, conn) && *args)
        {
            M_printf (i18n (1700, "%s is not a valid user in your list.\n"), args);
            return 0;
        }
        
        switch (data)
        {
            case 1:
                UtilUIDisplayMeta (cont);
                if (*args == ',')
                    args++;
                continue;
            case 2:
                if (ContactMetaLoad (cont))
                    UtilUIDisplayMeta (cont);
                else
                    M_printf (i18n (2247, "Couldn't load meta data for '%s' (%ld).\n"),
                              cont->nick, cont->uin);
                if (*args == ',')
                    args++;
                continue;
            case 3:
                if (ContactMetaSave (cont))
                    M_printf (i18n (2248, "Saved meta data for '%s' (%ld).\n"),
                              cont->nick, cont->uin);
                else
                    M_printf (i18n (2249, "Couldn't save meta data for '%s' (%ld).\n"),
                              cont->nick, cont->uin);
                if (*args == ',')
                    args++;
                continue;
            case 4:
                cont = ContactUIN (conn, conn->uin);
                if (!cont)
                    return 0;
                if (conn->ver > 6)
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
            case 5:
            case 6:
                IMCliInfo (conn, cont, 0);
                if (*args == ',')
                    args++;
                if (data == 6)
                    return 0;
                continue;
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

    M_printf ("%s%s%s%s%s\n", W_SEPARATOR, i18n (1062, "Users ignored:"));
    cg = conn->contacts;
    for (i = 0; (cont = ContactIndex (cg, i)); i++)
    {
        if (ContactPrefVal (cont, CO_IGNORE))
        {
            if (ContactPrefVal (cont, CO_INTIMATE))
                M_printf ("%s*%s", COLSERVER, COLNONE);
            else if (ContactPrefVal (cont, CO_HIDEFROM))
                M_printf ("%s~%s", COLSERVER, COLNONE);
            else
                M_print (" ");

            M_printf ("%s%-20s\t%s(%s)%s\n", COLCONTACT, cont->nick,
                      COLQUOTE, s_status (cont->status), COLNONE);
        }
    }
    M_printf ("%s%s%s%s", W_SEPARATOR);
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
        M_print (i18n (2118, "Out of memory.\n"));
        return 0;
    }
    
    if (!(cgon = ContactGroupC (conn, 0, "")))
    {
        if (data)
            ContactGroupD (cgon);
        M_print (i18n (2118, "Out of memory.\n"));
        return 0;
    }

    for (i = 0; (cont = ContactIndex (cg, i)); i++)
    {
        if (cont->status == STATUS_OFFLINE)
        {
            if (data)
            {
                ContactAdd (cgoff, cont);
                if (s_strlen (cont->nick) > lennick)
                    lennick = s_strlen (cont->nick);
            }
        }
        else
        {
            ContactAdd (cgon, cont);
            if (s_strlen (cont->nick) > lennick)
                lennick = s_strlen (cont->nick);
        }
    }

    columns = Get_Max_Screen_Width () / (lennick + 3);
    if (columns < 1)
        columns = 1;

    if (data)
    {
        colleft = (Get_Max_Screen_Width () - s_strlen (i18n (1653, "Offline"))) / 2 - 1;
        M_print (COLQUOTE);
        for (i = 0; i < colleft; i++)
            M_print ("=");
        M_printf (" %s%s%s ", COLCLIENT, i18n (1653, "Offline"), COLQUOTE);
        colright = Get_Max_Screen_Width () - i - s_strlen (i18n (1653, "Offline")) - 2;
        for (i = 0; i < colright; i++)
            M_print ("=");
        M_print (COLNONE);
        for (i = 0; (cont = ContactIndex (cgoff, i)); i++)
        {
            if (!(i % columns))
                M_print ("\n");
            M_printf ("  %s%-*s%s ", COLCONTACT, lennick + s_delta (cont->nick), cont->nick, COLNONE);
        }
        M_print ("\n");
        ContactGroupD (cgoff);
    }

    cont = ContactUIN (conn, conn->uin);
    M_printf ("%s%ld%s %s", COLCONTACT, cont->uin, COLNONE, COLQUOTE);
    colleft = (Get_Max_Screen_Width () - s_strlen (i18n (1654, "Online"))) / 2 - s_strlen (s_sprintf ("%ld", conn->uin)) - 2;
    for (i = 0; i < colleft; i++)
        M_print ("=");
    M_printf (" %s%s%s ", COLCLIENT, i18n (1654, "Online"), COLQUOTE);
    i += 3 + s_strlen (i18n (1654, "Online")) + s_strlen (s_sprintf ("%ld", conn->uin));
    colright = Get_Max_Screen_Width () - i - s_strlen (s_status (conn->status)) - 3;
    for (i = 0; i < colright; i++)
        M_print ("=");
    M_printf (" %s(%s)%s", COLQUOTE, s_status (conn->status), COLNONE);
    for (i = 0; (cont = ContactIndex (cgon, i)); i++)
    {
        char ind = s_status (cont->status)[0];
        if ((cont->status & 0xffff) == STATUS_ONLINE)
            ind = ' ';

        if (!(i % columns))
            M_print ("\n");
        M_printf ("%c %s%-*s%s ", ind, COLCONTACT, lennick + s_delta (cont->nick), cont->nick, COLNONE);
    }
    M_printf ("\n%s", COLQUOTE);
    colleft = Get_Max_Screen_Width ();
    for (i = 0; i < colleft; i++)
        M_print ("=");
    M_printf ("%s\n", COLNONE);
    ContactGroupD (cgon);

    return 0;
}

/*
 * Toggles sound or changes sound command.
 */
static JUMP_F(CmdUserSound)
{
    strc_t par;
    
    if (s_parse (&args, &par))
    {
        if (!strcasecmp (par->txt, "on") || !strcasecmp (par->txt, i18n (1085, "on")) || !strcasecmp (par->txt, "beep"))
           prG->sound = SFLAG_BEEP;
        else if (strcasecmp (par->txt, "off") && strcasecmp (par->txt, i18n (1086, "off")))
           prG->sound = SFLAG_EVENT;
        else
           prG->sound = 0;
    }
    if (prG->sound == SFLAG_BEEP)
        M_printf (i18n (2252, "A beep is generated by %sbeep%sing.\n"), COLSERVER, COLNONE);
    else if (prG->sound == SFLAG_EVENT)
        M_printf (i18n (2253, "A beep is generated by running the %sevent%s script.\n"), COLSERVER, COLNONE);
    else
        M_print (i18n (2254, "A beep is never generated.\n"));
    return 0;
}

/*
 * Toggles soundonline or changes soundonline command.
 */
static JUMP_F(CmdUserSoundOnline)
{
    M_printf (i18n (2255, "A beep for oncoming contacts is always generated by running the %sevent%s script.\n"), COLSERVER, COLNONE);
    return 0;
}

/*
 * Toggles soundoffine or changes soundoffline command.
 */
static JUMP_F(CmdUserSoundOffline)
{
    M_printf (i18n (2256, "A beep for offgoing contacts is always generated by running the %sevent%s script.\n"), COLSERVER, COLNONE);
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
    else if (s_parse (&args, &par))
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
    M_printf (i18n (1766, "Changing status to away/not available after idling %s%ld%s seconds.\n"),
              COLQUOTE, prG->away_time, COLNONE);
    return 0;
}

/*
 * Toggles simple options.
 */
static JUMP_F(CmdUserSet)
{
    int quiet = 0, setstatus = 0;
    strc_t par;
    const char *str = "";
    
    while (!data)
    {
        if (!s_parse (&args, &par))                    break;
        else if (!strcasecmp (par->txt, "color"))      { data = FLAG_COLOR;      str = i18n (2133, "Color is %s%s%s.\n"); }
        else if (!strcasecmp (par->txt, "colour"))     { data = FLAG_COLOR;      str = i18n (2133, "Color is %s%s%s.\n"); }
        else if (!strcasecmp (par->txt, "delbs"))      { data = FLAG_DELBS;      str = i18n (2262, "Interpreting a delete character as backspace is %s%s%s.\n"); }
        else if (!strcasecmp (par->txt, "funny"))      { data = FLAG_FUNNY;      str = i18n (2134, "Funny messages are %s%s%s.\n"); }
        else if (!strcasecmp (par->txt, "auto"))       { data = FLAG_AUTOREPLY;  str = i18n (2265, "Automatic replies are %s%s%s.\n"); }
        else if (!strcasecmp (par->txt, "uinprompt"))  { data = FLAG_UINPROMPT;  str = i18n (2266, "Having the last nick in the prompt is %s%s%s.\n"); }
        else if (!strcasecmp (par->txt, "autosave"))   { data = FLAG_AUTOSAVE;   str = i18n (2267, "Automatic saves are %s%s%s.\n"); }
        else if (!strcasecmp (par->txt, "autofinger")) { data = FLAG_AUTOFINGER; str = i18n (2268, "Automatic fingering of new UINs is %s%s%s.\n"); }
        else if (!strcasecmp (par->txt, "linebreak"))  data = -1;
        else if (!strcasecmp (par->txt, "tabs"))       data = -2;
        else if (!strcasecmp (par->txt, "webaware"))   { data = FLAG_WEBAWARE; setstatus = 1; }
        else if (!strcasecmp (par->txt, "hideip"))     { data = FLAG_HIDEIP;   setstatus = 1; }
        else if (!strcasecmp (par->txt, "dcauth"))     { data = FLAG_DC_AUTH;  setstatus = 1; }
        else if (!strcasecmp (par->txt, "dccont"))     { data = FLAG_DC_CONT;  setstatus = 1; }
        else if (!strcasecmp (par->txt, "quiet"))
        {
            quiet = 1;
            continue;
        }
        break;
    }
    
    if (data && !s_parse (&args, &par))
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
                M_printf (str, COLQUOTE, prG->flags & data ? i18n (1085, "on") : i18n (1086, "off"), COLNONE);
            if (setstatus && currconn && currconn->type == TYPE_SERVER)
                SnacCliSetstatus (currconn, currconn->status & 0xffff, 3);
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
                M_printf (i18n (2288, "Indentation style is %s%s%s.\n"), COLQUOTE,
                          ~prG->flags & FLAG_LIBR_BR ? (~prG->flags & FLAG_LIBR_INT ? i18n (2270, "simple") : i18n (2271, "indent")) :
                          ~prG->flags & FLAG_LIBR_INT ? i18n (2269, "break") : i18n (2272, "smart"), COLNONE);
            break;
        case -2:
            if (par)
            {
                if (!strcasecmp (par->txt, "cycle") || !strcasecmp (par->txt, i18n (2273, "cycle")))
                    prG->tabs = TABS_CYCLE;
                else if (!strcasecmp (par->txt, "cycleall") || !strcasecmp (par->txt, i18n (2274, "cycleall")))
                    prG->tabs = TABS_CYCLEALL;
                else if (!strcasecmp (par->txt, "simple") || !strcasecmp (par->txt, i18n (2270, "simple")))
                    prG->tabs = TABS_SIMPLE;
                else
                    data = 0;
            }
            if (!quiet)
                M_printf (i18n (2275, "Tab style is %s%s%s.\n"), COLQUOTE,
                          prG->tabs == TABS_CYCLE ? i18n (2273, "cycle") :
                          prG->tabs == TABS_CYCLEALL ? i18n (2274, "cycleall") : i18n (2270, "simple"), COLNONE);
            break;
    }
    if (!data)
    {
        M_printf (i18n (1820, "%s <option> [on|off|<value>] - control simple options.\n"), CmdUserLookupName ("set"));
        M_print (i18n (1822, "    color:      use colored text output.\n"));
        M_print (i18n (2277, "    hermit:     ignore all non-contacts.\n"));
        M_print (i18n (2278, "    delbs:      interpret delete characters as backspace.\n"));
        M_print (i18n (1815, "    funny:      use funny messages for output.\n"));
        M_print (i18n (2281, "    auto:       send auto-replies.\n"));
        M_print (i18n (2282, "    uinprompt:  have the last nick in the prompt.\n"));
        M_print (i18n (2283, "    autosave:   automatically save the micqrc.\n"));
        M_print (i18n (2284, "    autofinger: automatically finger new UINs.\n"));
        M_print (i18n (2285, "    linebreak:  style for line-breaking messages: simple, break, indent, smart.\n"));
        M_print (i18n (2286, "    tabs:       style for tab-handling: simple, cycle, cycleall.\n"));
    }
    return 0;
}

/*
 *
 */
static JUMP_F(CmdUserOpt)
{
    ContactGroup *cg = NULL;
    Contact *cont = NULL;
    const char *optname = NULL, *res = NULL, *optobj = NULL;
    char *coptname, *coptobj, *col;
    ContactOptions *copts = NULL;
    int i;
    UWORD flag = 0, val;
    strc_t par;
    ANYCONN;

    if (!data && s_parsekey (&args, "global"))
    {
        copts = &prG->copts;
        data = COF_GLOBAL;
        optobj = "";
        coptobj = strdup ("");
    }
    else if ((!data || data == COF_GROUP) && s_parsecg (&args, &cg, conn))
    {
        copts = &cg->copts;
        data = COF_GROUP;
        optobj = cg->name;
        coptobj = strdup (s_cquote (optobj, COLQUOTE));
    }
    else if ((!data || data == COF_CONTACT) && s_parsenick (&args, &cont, conn))
    {
        copts = &cont->copts;
        data = COF_CONTACT;
        optobj = cont->nick;
        coptobj = strdup (s_cquote (optobj, COLQUOTE));
    }
    else if (data == COF_GLOBAL)
    {
        copts = &prG->copts;
        optobj = "";
        coptobj = strdup ("");
    }
    else
    {
        if (s_parse (&args, &par))
        {
            switch (data)
            {
                case COF_GROUP:
                    M_printf (i18n (9999, "%s is not a contact group.\n"), s_cquote (par->txt, COLQUOTE));
                    break;
                case COF_CONTACT:
                    M_printf (i18n (9999, "%s is not a contact.\n"), s_cquote (par->txt, COLQUOTE));
                    break;
                default:
                    M_printf (i18n (9999, "%s is neither a contact, nor a contact group, nor the %sglobal%s keyword.\n"),
                              s_cquote (par->txt, COLQUOTE), COLQUOTE, COLNONE);
            }
        }
        else
        {
            if (!data)
                M_printf (i18n (9999, "opt - short hand for optglobal, optgroup or optcontact.\n"));
            if (!data || data == COF_GLOBAL)
                M_printf (i18n (9999, "optglobal            [<option> [<value>]] - set global option.\n"));
            if (!data || data == COF_GROUP)
                M_printf (i18n (9999, "optgroup   <group>   [<option> [<value>]] - set contact group option.\n"));
            if (!data || data == COF_CONTACT)
                M_printf (i18n (9999, "optcontact <contact> [<option> [<value>]] - set contact option.\n"));
        }
        return 0;
    }
    
    if (!*args)
    {
        M_printf (data == COF_CONTACT ? i18n (9999, "Options for contact %s:\n") :
                  data == COF_GROUP   ? i18n (9999, "Options for contact group %s:\n")
                                      : i18n (9999, "Global options:\n"),
                  coptobj);
        
        for (i = 0; (optname = ContactOptionsList[i].name); i++)
        {
            UWORD flag = ContactOptionsList[i].flag;
            
            if (~flag & data)
                continue;
            
            switch (flag & (COF_BOOL | COF_NUMERIC | COF_STRING | COF_COLOR))
            {
                case COF_BOOL:
                    if (data == COF_CONTACT)
                        M_printf ("    %-15s  %s%-15s%s  (%s %s%s%s)\n", optname, COLQUOTE,
                                  ContactOptionsGetVal (copts, flag, &val)
                                    ? val ? i18n (1085, "on") : i18n (1086, "off")
                                    : i18n (9999, "undefined"), COLNONE, i18n (9999, "effectivly"), COLQUOTE,
                                  ContactPrefVal (cont, flag)
                                    ? i18n (1085, "on") : i18n (1086, "off"), COLNONE);
                    else
                        M_printf ("    %-15s  %s%s%s\n", optname, COLQUOTE,
                                  ContactOptionsGetVal (copts, flag, &val)
                                    ? val  ? i18n (1085, "on") : i18n (1086, "off")
                                    : i18n (9999, "undefined"), COLNONE);
                    break;
                case COF_NUMERIC:
                    if (data == COF_CONTACT)
                        M_printf ("    %-15s  %s%-15s%s  (%s %s%d%s)\n", optname, COLQUOTE,
                                  ContactOptionsGetVal (copts, flag, &val)
                                    ? s_sprintf ("%d", val)
                                    : i18n (9999, "undefined"), COLNONE, i18n (9999, "effectivly"), COLQUOTE,
                                  ContactPrefVal (cont, flag), COLNONE);
                    else
                        M_printf ("    %-15s  %s%s%s\n", optname, COLQUOTE,
                                  ContactOptionsGetVal (copts, flag, &val)
                                    ? s_sprintf ("%d", val)
                                    : i18n (9999, "undefined"), COLNONE);
                    break;
                case COF_STRING:
                    if (data == COF_CONTACT)
                        M_printf ("    %-15s  %s%-15s%s  (%s %s%s%s)\n", optname, COLQUOTE,
                                  ContactOptionsGetStr (copts, flag, &res)
                                    ? res : i18n (9999, "undefined"), COLNONE, i18n (9999, "effectivly"), COLQUOTE,
                                  ContactPrefStr (cont, flag), COLNONE);
                    else
                        M_printf ("    %-15s  %s%s%s\n", optname, COLQUOTE,
                                  ContactOptionsGetStr (copts, flag, &res)
                                    ? res : i18n (9999, "undefined"), COLNONE);
                    break;
                case COF_COLOR:
                    if (data == COF_CONTACT)
                        M_printf ("    %-15s  %s%-15s%s  (%s %s%s%s)\n", optname, COLQUOTE,
                                  ContactOptionsGetStr (copts, flag, &res)
                                    ? ContactOptionsS2C (res) : i18n (9999, "undefined"), COLNONE,
                                  i18n (9999, "effectivly"), COLQUOTE,
                                  ContactOptionsS2C (ContactPrefStr (cont, flag)), COLNONE);
                    else
                        M_printf ("    %-15s  %s%s%s\n", optname, COLQUOTE,
                                  ContactOptionsGetStr (copts, flag, &res)
                                    ? ContactOptionsS2C (res) : i18n (9999, "undefined"), COLNONE);
                    break;
            }
        }
        free (coptobj);
        return 0;
    }
    
    while (*args)
    {
        if (!s_parse (&args, &par))
            break;
        
        optname = NULL;
        for (i = 0; ContactOptionsList[i].name; i++)
            if (ContactOptionsList[i].flag & data && !strcmp (par->txt, ContactOptionsList[i].name))
            {
                optname = ContactOptionsList[i].name;
                flag = ContactOptionsList[i].flag;
                break;
            }
        
        if (!optname)
        {
            M_printf (i18n (9999, "Unknown option %s.\n"), s_cquote (par->txt, COLQUOTE));
            continue;
        }
        coptname = strdup (s_cquote (optname, COLQUOTE));
        
        if (!s_parse (&args, &par))
        {
            const char *res = NULL;
            UWORD val;

            if (!ContactOptionsGetVal (copts, flag, &val) || ((flag & (COF_STRING | COF_COLOR)) && !ContactOptionsGetStr (copts, flag, &res)))
                M_printf (data == COF_CONTACT ? i18n (9999, "Option %s for contact %s is undefined.\n") :
                          data == COF_GROUP   ? i18n (9999, "Option %s for contact group %s is undefined.\n")
                                              : i18n (9999, "Option %s%s has no global value.\n"),
                          coptname, coptobj);
            else if (flag & (COF_BOOL | COF_NUMERIC))
                M_printf (data == COF_CONTACT ? i18n (9999, "Option %s for contact %s is %s%s%s.\n") :
                          data == COF_GROUP   ? i18n (9999, "Option %s for contact group %s is %s%s%s.\n")
                                              : i18n (9999, "Option %s%s is globally %s%s%s.\n"),
                          coptname, coptobj, COLQUOTE, flag & COF_NUMERIC ? s_sprintf ("%d", val)
                          : val ? i18n (1085, "on") : i18n (1086, "off"), COLNONE);
            else
                M_printf (data == COF_CONTACT ? i18n (9999, "Option %s for contact %s is %s.\n") :
                          data == COF_GROUP   ? i18n (9999, "Option %s for contact group %s is %s.\n")
                                              : i18n (9999, "Option %s%s is globally %s.\n"),
                          coptname, coptobj, s_cquote (flag & COF_STRING  ? res : ContactOptionsS2C (res), COLQUOTE));
            return 0;
        }
        
        if (!strcasecmp (par->txt, "undef"))
        {
            ContactOptionsUndef (copts, flag);
            M_printf (data == COF_CONTACT ? i18n (9999, "Undefining option %s for contact %s.\n") :
                      data == COF_GROUP   ? i18n (9999, "Undefining option %s for contact group %s.\n")
                                          : i18n (9999, "Undefining global option %s%s.\n"),
                      coptname, coptobj);
        }
        else if (flag & COF_STRING)
        {
            ContactOptionsSetStr (copts, flag, par->txt);
            M_printf (data == COF_CONTACT ? i18n (9999, "Setting option %s for contact %s to %s.\n") :
                      data == COF_GROUP   ? i18n (9999, "Setting option %s for contact group %s to %s.\n")
                                          : i18n (9999, "Setting option %s%s globally to %s.\n"),
                      coptname, coptobj, s_cquote (par->txt, COLQUOTE));
        }
        else if (flag & COF_COLOR)
        {
            res = ContactOptionsC2S (col = strdup (par->txt));
            ContactOptionsSetStr (copts, flag, res);
            M_printf (data == COF_CONTACT ? i18n (9999, "Setting option %s for contact %s to %s.\n") :
                      data == COF_GROUP   ? i18n (9999, "Setting option %s for contact group %s to %s.\n")
                                          : i18n (9999, "Setting option %s%s' globally to %s.\n"),
                      coptname, coptobj, s_cquote (col, COLQUOTE));
            free (col);
        }
        else if (flag & COF_NUMERIC)
        {
            ContactOptionsSetVal (copts, flag, atoi (par->txt));
            M_printf (data == COF_CONTACT ? i18n (9999, "Setting option %s for contact %s to %s%d%s.\n") :
                      data == COF_GROUP   ? i18n (9999, "Setting option %s for contact group %s to %s%d%s.\n")
                                          : i18n (9999, "Setting option %s%s globally to %s%d%s.\n"),
                      coptname, coptobj, COLQUOTE, atoi (par->txt), COLNONE);
        }
        else if (!strcasecmp (par->txt, "on")  || !strcasecmp (par->txt, i18n (1085, "on")))
        {
            ContactOptionsSetVal (copts, flag, 1);
            M_printf (data == COF_CONTACT ? i18n (9999, "Setting option %s for contact %s.\n") :
                      data == COF_GROUP   ? i18n (9999, "Setting option %s for contact group %s.\n")
                                          : i18n (9999, "Setting option %s%s globally.\n"),
                      coptname, coptobj);
        }
        else if (!strcasecmp (par->txt, "off") || !strcasecmp (par->txt, i18n (1086, "off")))
        {
            ContactOptionsSetVal (copts, flag, 0);
            M_printf (data == COF_CONTACT ? i18n (9999, "Clearing option %s for contact %s.\n") :
                      data == COF_GROUP   ? i18n (9999, "Clearing option %s for contact group %s.\n")
                                          : i18n (9999, "Clearing option %s%s globally.\n"),
                      coptname, coptobj);
        }
        else
        {
            M_printf (i18n (9999, "Invalid value %s for boolean option %s.\n"), s_cquote (par->txt, COLQUOTE), coptname);
            continue;
        }
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
    R_clrscr ();
    return 0;
}


/*
 * Registers a new user.
 */
static JUMP_F(CmdUserRegister)
{
    if (strlen (args))
    {
        ANYCONN;

        if (!conn)
            SrvRegisterUIN (NULL, args);
        else if (conn->type & TYPEF_SERVER_OLD)
            CmdPktCmdRegNewUser (conn, args);     /* TODO */
        else
            SrvRegisterUIN (conn, args);
    }
    return 0;
}

/*
 * Toggles ignoring a user.
 */
static JUMP_F(CmdUserTogIgnore)
{
    Contact *cont = NULL;
    OPENCONN;

    while (*args)
    {
        if (!s_parsenick_s (&args, &cont, MULTI_SEP, conn))
        {
            M_printf (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
            return 0;
        }

        if (ContactPrefVal (cont, CO_IGNORE))
        {
            ContactOptionsSetVal (&cont->copts, CO_IGNORE, 0);
            M_printf (i18n (1666, "Unignored %s.\n"), cont->nick);
        }
        else
        {
            ContactOptionsSetVal (&cont->copts, CO_IGNORE, 1);
            M_printf (i18n (1667, "Ignoring %s.\n"), cont->nick);
        }
        if (*args == ',')
            args++;
    }
    return 0;
}

/*
 * Toggles beeing invisible to a user.
 */
static JUMP_F(CmdUserTogInvis)
{
    Contact *cont = NULL;
    OPENCONN;

    while (*args)
    {
        if (!s_parsenick_s (&args, &cont, MULTI_SEP, conn))
        {
            M_printf (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
            return 0;
        }

        if (ContactPrefVal (cont, CO_HIDEFROM))
        {
            ContactOptionsSetVal (&cont->copts, CO_HIDEFROM, 0);
            if (conn->ver > 6)
                SnacCliReminvis (conn, cont);
            else
                CmdPktCmdUpdateList (conn, cont, INV_LIST_UPDATE, FALSE);
            M_printf (i18n (2020, "Being visible to %s.\n"), cont->nick);
        }
        else
        {
            ContactOptionsSetVal (&cont->copts, CO_INTIMATE, 0);
            ContactOptionsSetVal (&cont->copts, CO_HIDEFROM, 1);
            if (conn->ver > 6)
                SnacCliAddinvis (conn, cont);
            else
                CmdPktCmdUpdateList (conn, cont, INV_LIST_UPDATE, TRUE);
            M_printf (i18n (2021, "Being invisible to %s.\n"), cont->nick);
        }
        if (*args == ',')
            args++;
    }
    if (conn->ver < 6)
    {
        CmdPktCmdContactList (conn);
        CmdPktCmdInvisList (conn);
        CmdPktCmdVisList (conn);
        CmdPktCmdStatusChange (conn, conn->status);
    }
    return 0;
}

/*
 * Toggles visibility to a user.
 */
static JUMP_F(CmdUserTogVisible)
{
    Contact *cont = NULL;
    OPENCONN;

    while (*args)
    {
        if (!s_parsenick (&args, &cont, conn))
        {
            M_printf (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
            return 0;
        }

        if (ContactPrefVal (cont, CO_INTIMATE))
        {
            ContactOptionsSetVal (&cont->copts, CO_INTIMATE, 0);
            if (conn->ver > 6)
                SnacCliRemvisible (conn, cont);
            else
                CmdPktCmdUpdateList (conn, cont, VIS_LIST_UPDATE, FALSE);
            M_printf (i18n (1670, "Normal visible to %s now.\n"), cont->nick);
        }
        else
        {
            ContactOptionsSetVal (&cont->copts, CO_HIDEFROM, 0);
            ContactOptionsSetVal (&cont->copts, CO_INTIMATE, 1);
            if (conn-> ver > 6)
                SnacCliAddvisible (conn, cont);
            else
                CmdPktCmdUpdateList (conn, cont, VIS_LIST_UPDATE, TRUE);
            M_printf (i18n (1671, "Always visible to %s now.\n"), cont->nick);
        }
        if (*args == ',')
            args++;
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
    ContactGroup *cg = NULL;
    Contact *cont = NULL, *cont2;
    char *cmd;
    strc_t par;
    OPENCONN;

    if (data != 1)
    {
        const char *argst = args;
        if (s_parse (&argst, &par))
        {
            if ((cg = ContactGroupFind (conn, 0, par->txt)))
                args = argst;
            else if (data == 2)
            {
                if ((cg = ContactGroupFind (conn, 0, par->txt)))
                {
                    M_printf (i18n (9999, "Contact group '%s' already exists\n"), par->txt);
                    args = argst;
                }
                if ((cg = ContactGroupC (conn, 0, par->txt)))
                {
                    M_printf (i18n (2245, "Added contact group '%s'.\n"), par->txt);
                    args = argst;
                }
                else
                {
                    M_print (i18n (2118, "Out of memory.\n"));
                    return 0;
                }
            }

        }
        else if (data == 2)
        {
            M_print (i18n (2240, "No contact group given.\n"));
            return 0;
        }
    }
    
    if (cg)
    {
        while (*args)
        {
            while (s_parsenick_s (&args, &cont, MULTI_SEP, conn))
            {
                if (!cont->group)
                    M_printf (i18n (1061, "'%s' not recognized as a nick name.\n"), cont->nick);
                else if (!ContactFind (cg, 0, cont->uin, 0))
                {
                    if (ContactAdd (cg, cont))
                        M_printf (i18n (2241, "Added '%s' to contact group '%s'.\n"), cont->nick, cg->name);
                    else
                        M_print (i18n (2118, "Out of memory.\n"));
                }
                else
                    M_printf (i18n (2244, "Contact group '%s' already has contact '%s' (%ld).\n"),
                              cg->name, cont->nick, cont->uin);
                if (*args == ',')
                    args++;
            }
            if (s_parse (&args, &par) && *par->txt)
                M_printf (i18n (1061, "'%s' not recognized as a nick name.\n"), par->txt);
        }
        return 0;
    }

    if (!s_parsenick (&args, &cont, conn))
    {
        M_printf (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
        return 0;
    }

    if (!s_parserem (&args, &cmd))
    {
        M_print (i18n (2116, "No new nick name given.\n"));
        return 0;
    }

    if (cmd)
    {
        if (!cont->group)
        {
            M_printf (i18n (2117, "%ld added as %s.\n"), cont->uin, cmd);
            M_print (i18n (1754, "Note: You need to 'save' to write new contact list to disc.\n"));
            if (c_strlen (cmd) > uiG.nick_len)
                uiG.nick_len = c_strlen (cmd);
            ContactFindCreate (conn->contacts, 0, cont->uin, cmd);
            if (conn->ver > 6)
                SnacCliAddcontact (conn, cont);
            else
                CmdPktCmdContactList (conn);
        }
        else
        {
            if ((cont2 = ContactFind (conn->contacts, 0, cont->uin, cmd)))
                M_printf (i18n (2146, "'%s' is already an alias for '%s' (%ld).\n"),
                         cont2->nick, cont->nick, cont->uin);
            else if ((cont2 = ContactFind (conn->contacts, 0, 0, cmd)))
                M_printf (i18n (2147, "'%s' (%ld) is already used as a nick.\n"),
                         cont2->nick, cont2->uin);
            else
            {
                if (!(cont2 = ContactFindCreate (conn->contacts, 0, cont->uin, cmd)))
                    M_print (i18n (2118, "Out of memory.\n"));
                else
                {
                    M_printf (i18n (2148, "Added '%s' as an alias for '%s' (%ld).\n"),
                             cont2->nick, cont->nick, cont->uin);
                    M_print (i18n (1754, "Note: You need to 'save' to write new contact list to disc.\n"));
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
    ContactGroup *cg = NULL;
    Contact *cont = NULL;
    UDWORD uin;
    strc_t par;
    char *alias;
    const char *argst;
    UBYTE all = 0;
    OPENCONN;
    
    if (data != 1)
    {
        argst = args;
        if (s_parse (&argst, &par))
            if ((cg = ContactGroupFind (conn, 0, par->txt)))
                args = argst;
        if (data == 2 && !cg)
        {
            M_print (i18n (2240, "No contact group given.\n"));
            return 0;
        }
    }

    if (s_parsekey (&argst, "all"))
        all = 2;
    else if (cg && data == 2)
        all = 1;
    
    if (all && cg && cg->serv->contacts != cg)
    {
        ContactGroupD (cg);
        M_print (i18n (1754, "Note: You need to 'save' to write new contact list to disc.\n"));
        return 0;
    }

    while (*args)
    {
        if (!s_parsenick_s (&args, &cont, MULTI_SEP, conn))
        {
            M_printf (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
            break;
        }
        if (*args == ',')
            args++;
        
        if (cg)
        {
            if (ContactRem (cg, cont))
                M_printf (i18n (2243, "Removed contact '%s' from group '%s'.\n"),
                          cont->nick, cg->name);
            else
                M_printf (i18n (2246, "Contact '%s' is not in group '%s'.\n"),
                          cont->nick, cg->name);
        }
        else
        {
            if (!cont->group)
                continue;

            if (all || !cont->alias)
            {
                if (conn->ver > 6)
                    SnacCliRemcontact (conn, cont);
                else
                    CmdPktCmdContactList (conn);
            }

            alias = strdup (cont->nick);
            uin = cont->uin;
            
            if (all || !cont->alias)
            {
                ContactD (cont);
                M_printf (i18n (2150, "Removed contact '%s' (%ld).\n"),
                          alias, uin);
            }
            else
            {
                ContactRemAlias (cont, alias);
                M_printf (i18n (2149, "Removed alias '%s' for '%s' (%ld).\n"),
                         alias, cont->nick, uin);
            }
            free (alias);
        }
    }

    M_print (i18n (1754, "Note: You need to 'save' to write new contact list to disc.\n"));
    return 0;
}

/*
 * Authorize another user to add you to the contact list.
 */
static JUMP_F(CmdUserAuth)
{
    strc_t par;
    char *cmd = NULL, *msg = NULL;
    const char *argsb;
    Contact *cont = NULL;
    OPENCONN;

    argsb = args;
    if (!s_parse (&args, &par))
    {
        M_print (i18n (2119, "auth [grant] <nick>    - grant authorization.\n"));
        M_print (i18n (2120, "auth deny <nick> <msg> - refuse authorization.\n"));
        M_print (i18n (2121, "auth req  <nick> <msg> - request authorization.\n"));
        M_print (i18n (2145, "auth add  <nick>       - authorized add.\n"));
        return 0;
    }
    cmd = strdup (par->txt);

    if (s_parsenick (&args, &cont, conn))
    {
        s_parserem (&args, &msg);
        if (!strcmp (cmd, "req"))
        {
            if (!msg)         /* FIXME: let it untranslated? */
                msg = "Please authorize my request and add me to your Contact List\n";
#ifdef WIP
            if (conn->type == TYPE_SERVER && conn->ver >= 8)
                SnacCliReqauth (conn, cont, msg);
            else
#endif
            if (conn->type == TYPE_SERVER)
                SnacCliSendmsg (conn, cont, msg, MSG_AUTH_REQ, 0);
            else
                CmdPktCmdSendMessage (conn, cont, msg, MSG_AUTH_REQ);
            free (cmd);
            return 0;
        }
        else if (!strcmp (cmd, "deny"))
        {
            if (!msg)         /* FIXME: let it untranslated? */
                msg = "Authorization refused\n";
#ifdef WIP
            if (conn->type == TYPE_SERVER && conn->ver >= 8)
                SnacCliAuthorize (conn, cont, 0, msg);
            else
#endif
            if (conn->type == TYPE_SERVER)
                SnacCliSendmsg (conn, cont, msg, MSG_AUTH_DENY, 0);
            else
                CmdPktCmdSendMessage (conn, cont, msg, MSG_AUTH_DENY);
            free (cmd);
            return 0;
        }
        else if (!strcmp (cmd, "add"))
        {
#ifdef WIP
            if (conn->type == TYPE_SERVER && conn->ver >= 8)
                SnacCliGrantauth (conn, cont);
            else
#endif
            if (conn->type == TYPE_SERVER)
                SnacCliSendmsg (conn, cont, "\x03", MSG_AUTH_ADDED, 0);
            else
                CmdPktCmdSendMessage (conn, cont, "\x03", MSG_AUTH_ADDED);
            free (cmd);
            return 0;
        }
    }
    if ((strcmp (cmd, "grant") && !s_parsenick (&argsb, &cont, conn)) || !cont)
    {
        M_printf (i18n (1061, "'%s' not recognized as a nick name.\n"),
                 strcmp (cmd, "grant") && strcmp (cmd, "req") && strcmp (cmd, "deny") ? argsb : args);
        free (cmd);
        return 0;
    }

#ifdef WIP
    if (conn->type == TYPE_SERVER && conn->ver >= 8)
        SnacCliAuthorize (conn, cont, 1, NULL);
    else
#endif
    if (conn->type == TYPE_SERVER)
        SnacCliSendmsg (conn, cont, "\x03", MSG_AUTH_GRANT, 0);
    else
        CmdPktCmdSendMessage (conn, cont, "\x03", MSG_AUTH_GRANT);
    return 0;
}

/*
 * Save user preferences.
 */
static JUMP_F(CmdUserSave)
{
    int i = Save_RC ();
    if (i == -1)
        M_print (i18n (1679, "Sorry saving your personal reply messages went wrong!\n"));
    else
        M_print (i18n (1680, "Your personal settings have been saved!\n"));
    return 0;
}

/*
 * Send an URL message.
 */
static JUMP_F(CmdUserURL)
{
    char *url, *msg;
    const char *cmsg;
    strc_t par;
    Contact *cont = NULL;
    OPENCONN;

    if (!s_parsenick (&args, &cont, conn))
    {
        M_printf (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
        return 0;
    }

    if (!s_parse (&args, &par))
    {
        M_print (i18n (1678, "Need URL please.\n"));
        return 0;
    }

    url = strdup (par->txt);

    if (!s_parserem (&args, &msg))
        msg = "";

    cmsg = s_sprintf ("%s%c%s", msg, Conv0xFE, url);
    s_repl (&uiG.last_message_sent, cmsg);
    uiG.last_message_sent_type = MSG_URL;
    uiG.last_sent_uin = cont->uin;

    IMCliMsg (conn, cont, ExtraSet (NULL, EXTRA_MESSAGE, MSG_URL, cmsg));

    free (url);
    return 0;
}

/*
 * Shows the user in your tab list.
 */
static JUMP_F(CmdUserTabs)
{
    int i;
    Contact *cont;
    ANYCONN;

    for (i = 0; TabGetOut (i); i++)
        ;
    M_printf (i18n (1681, "Last %d people you talked to:\n"), i);
    for (i = 0; (cont = TabGetOut (i)); i++)
    {
        M_printf ("    %s", cont->nick);
        M_printf (" %s(%s)%s\n", COLQUOTE, s_status (cont->status), COLNONE);
    }
    for (i = 0; TabGetIn (i); i++)
        ;
    M_printf (i18n (9999, "Last %d people that talked to you:\n"), i);
    for (i = 0; (cont = TabGetIn (i)); i++)
    {
        M_printf ("    %s", cont->nick);
        M_printf (" %s(%s)%s\n", COLQUOTE, s_status (cont->status), COLNONE);
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

    if (!s_parsenick (&args, &cont, conn))
    {
        HistShow (conn, NULL);

/*        cg = conn->contacts;
        M_print (i18n (1682, "You have received messages from:\n"));
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
            if (cont->last_message)
                M_printf ("  " COLCONTACT "%s" COLNONE " %s " COLQUOTE "%s" COLNONE "\n",
                         cont->nick, s_time (&cont->last_time), cont->last_message);*/
        return 0;
    }

    do
    {
/*        if (cont->last_message)
        {
            M_printf (i18n (2106, "Last message from %s%s%s at %s:\n"),
                     COLCONTACT, cont->nick, COLNONE, s_time (&cont->last_time));
            M_printf (COLQUOTE "%s" COLNONE "\n", cont->last_message);
        }
        else
        {
            M_printf (i18n (2107, "No messages received from %s%s%s.\n"),
                     COLCONTACT, cont->nick, COLNONE);
        }*/
        HistShow (conn, cont);
        if (*args == ',')
            args++;
    }
    while (s_parsenick (&args, &cont, conn));
    return 0;
}

/*
 * Shows mICQ's uptime.
 */
static JUMP_F(CmdUserUptime)
{
    double TimeDiff = difftime (time (NULL), uiG.start_time);
    Connection *conn;
    int Days, Hours, Minutes, Seconds, i;
    int pak_sent = 0, pak_rcvd = 0, real_pak_sent = 0, real_pak_rcvd = 0;

    Seconds = (int) TimeDiff % 60;
    TimeDiff = TimeDiff / 60.0;
    Minutes = (int) TimeDiff % 60;
    TimeDiff = TimeDiff / 60.0;
    Hours = (int) TimeDiff % 24;
    TimeDiff = TimeDiff / 24.0;
    Days = TimeDiff;

    M_printf ("%s ", i18n (1687, "mICQ has been running for"));
    if (Days != 0)
        M_printf ("%s%02d%s %s, ", COLQUOTE, Days, COLNONE, i18n (1688, "days"));
    if (Hours != 0)
        M_printf ("%s%02d%s %s, ", COLQUOTE, Hours, COLNONE, i18n (1689, "hours"));
    if (Minutes != 0)
        M_printf ("%s%02d%s %s, ", COLQUOTE, Minutes, COLNONE, i18n (1690, "minutes"));
    M_printf ("%s%02d%s %s.\n", COLQUOTE, Seconds, COLQUOTE, i18n (1691, "seconds"));

    M_print (i18n (1746, " nr type         sent/received packets/unique packets\n"));
    for (i = 0; (conn = ConnectionNr (i)); i++)
    {
        M_printf ("%3d %-12s %7ld %7ld %7ld %7ld\n",
                 i + 1, ConnectionType (conn), conn->stat_pak_sent, conn->stat_pak_rcvd,
                 conn->stat_real_pak_sent, conn->stat_real_pak_rcvd);
        pak_sent += conn->stat_pak_sent;
        pak_rcvd += conn->stat_pak_rcvd;
        real_pak_sent += conn->stat_real_pak_sent;
        real_pak_rcvd += conn->stat_real_pak_rcvd;
    }
    M_printf ("    %-12s %7d %7d %7d %7d\n",
             i18n (1747, "total"), pak_sent, pak_rcvd,
             real_pak_sent, real_pak_rcvd);
    M_printf (i18n (2073, "Memory usage: %ld packets processing.\n"), uiG.packets);
    return 0;
}

/*
 * List connections, and open/close them.
 */
static JUMP_F(CmdUserConn)
{
    int i;
    strc_t par;
    Connection *conn;

    if (!s_parse (&args, &par))
    {
        M_print (i18n (1887, "Connections:"));
        M_print ("\n  " COLINDENT);
        
        for (i = 0; (conn = ConnectionNr (i)); i++)
        {
            Contact *cont;
            
            cont = ContactUIN (conn, conn->uin);

            M_printf (i18n (2093, "%02d %-15s version %d for %s (%lx), at %s:%ld %s\n"),
                     i + 1, ConnectionType (conn), conn->ver, cont ? cont->nick : "", conn->status,
                     conn->server ? conn->server : s_ip (conn->ip), conn->port,
                     conn->connect & CONNECT_FAIL ? i18n (1497, "failed") :
                     conn->connect & CONNECT_OK   ? i18n (1934, "connected") :
                     conn->connect & CONNECT_MASK ? i18n (1911, "connecting") : i18n (1912, "closed"));
            if (prG->verbose)
            {
                char *t1, *t2, *t3;
                M_printf (i18n (1935, "    type %d socket %d ip %s (%d) on [%s,%s] id %lx/%x/%x\n"),
                     conn->type, conn->sok, t1 = strdup (s_ip (conn->ip)),
                     conn->connect, t2 = strdup (s_ip (conn->our_local_ip)),
                     t3 = strdup (s_ip (conn->our_outside_ip)),
                     conn->our_session, conn->our_seq, conn->our_seq2);
                M_printf (i18n (2081, "    at %p parent %p assoc %p\n"), conn, conn->parent, conn->assoc);
                free (t1);
                free (t2);
                free (t3);
            }
        } 
        M_print (COLEXDENT "\r");
    }
    else if (!strcmp (par->txt, "login") || !strcmp (par->txt, "open"))
    {
        UDWORD i = 0, j;

        if (!s_parseint (&args, &i))
            for (j = 0; (conn = ConnectionNr (j)); j++)
                if (conn->type & TYPEF_SERVER)
                {
                    i = j + 1;
                    break;
                }

        conn = ConnectionNr (i - 1);
        if (!conn)
        {
            M_printf (i18n (1894, "There is no connection number %ld.\n"), i);
            return 0;
        }
        if (conn->connect & CONNECT_OK)
        {
            M_printf (i18n (1891, "Connection %ld is already open.\n"), i);
            return 0;
        }
        if (!conn->open)
        {
            M_printf (i18n (2194, "Don't know how to open this connection type.\n"));
            return 0;
        }
        conn->open (conn);
    }
    else if (!strcmp (par->txt, "select"))
    {
        UDWORD i = 0;

        if (!s_parseint (&args, &i))
            i = ConnectionFindNr (currconn) + 1;

        conn = ConnectionNr (i - 1);
        if (!conn && !(conn = ConnectionFind (TYPEF_SERVER, i, NULL)))
        {
            M_printf (i18n (1894, "There is no connection number %ld.\n"), i);
            return 0;
        }
        if (~conn->type & TYPEF_SERVER)
        {
            M_printf (i18n (2098, "Connection %ld is not a server connection.\n"), i);
            return 0;
        }
#if 0
        if (~conn->connect & CONNECT_OK)
        {
            M_printf (i18n (2096, "Connection %ld is not open.\n"), i);
            return 0;
        }
#endif
        currconn = conn;
        M_printf (i18n (2099, "Selected connection %ld (version %d, UIN %ld) as current connection.\n"),
                 i, conn->ver, conn->uin);
    }
    else if (!strcmp (par->txt, "remove") || !strcmp (par->txt, "delete"))
    {
        UDWORD i = 0;

        s_parseint (&args, &i);

        conn = ConnectionNr (i - 1);
        if (!conn)
        {
            M_printf (i18n (1894, "There is no connection number %ld.\n"), i);
            return 0;
        }
        if (conn->spref)
        {
            M_printf (i18n (2102, "Connection %ld is a configured connection.\n"), i);
            return 0;
        }
        M_printf (i18n (2101, "Removing connection %ld and its dependents completely.\n"), i);
        ConnectionClose (conn);
    }
    else if (!strcmp (par->txt, "close") || !strcmp (par->txt, "logoff"))
    {
        UDWORD i = 0;

        s_parseint (&args, &i);

        conn = ConnectionNr (i - 1);
        if (!conn)
        {
            M_printf (i18n (1894, "There is no connection number %ld.\n"), i);
            return 0;
        }
        if (conn->close)
        {
            M_printf (i18n (2185, "Closing connection %ld.\n"), i);
            conn->close (conn);
        }
        else
        {
            M_printf (i18n (2101, "Removing connection %ld and its dependents completely.\n"), i);
            ConnectionClose (conn);
        }
    }
    else
    {
        M_print (i18n (1892, "conn               List available connections.\n"));
        M_print (i18n (2094, "conn login         Open first server connection.\n"));
        M_print (i18n (1893, "conn login <nr>    Open connection <nr>.\n"));
        M_print (i18n (2156, "conn close <nr>    Close connection <nr>.\n"));
        M_print (i18n (2095, "conn remove <nr>   Remove connection <nr>.\n"));
        M_print (i18n (2097, "conn select <nr>   Select connection <nr> as server connection.\n"));
        M_print (i18n (2100, "conn select <uin>  Select connection with UIN <uin> as server connection.\n"));
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
        M_print (i18n (2326, "Server side contact list only supported for ICQ v8.\n"));
        return 0;
    }

    if (!data)
    {
        if (!s_parse (&args, &par))                  data = 0;
        else if (!strcasecmp (par->txt, "show"))     data = IMROSTER_SHOW;
        else if (!strcasecmp (par->txt, "diff"))     data = IMROSTER_DIFF;
        else if (!strcasecmp (par->txt, "add"))      data = IMROSTER_DOWNLOAD;
        else if (!strcasecmp (par->txt, "download")) data = IMROSTER_DOWNLOAD;
        else if (!strcasecmp (par->txt, "import"))   data = IMROSTER_IMPORT;
        else if (!strcasecmp (par->txt, "sync"))     data = IMROSTER_SYNC;
        else if (!strcasecmp (par->txt, "export"))   data = IMROSTER_EXPORT;
        else if (!strcasecmp (par->txt, "upload"))   data = IMROSTER_UPLOAD;
        else                                         data = 0;
    }
    if (data)
        IMRoster (conn, data);
    else
    {
        M_print (i18n (2103, "contact show    Show server based contact list.\n"));
        M_print (i18n (2104, "contact diff    Show server based contacts not on local contact list.\n"));
        M_print (i18n (2321, "contact add     Add server based contact list to local contact list.\n"));
        M_print (i18n (2105, "contact import  Import server based contact list as local contact list.\n"));
        M_print (i18n (2322, "contact sync    Import server based contact list if appropriate.\n"));
        M_print (i18n (2323, "contact export  Export local contact list to server based list.\n"));
        M_print (i18n (2324, "contact upload  Add local contacts to server based contact list.\n"));
    }
    return 0;
}

/*
 * Exits mICQ.
 */
static JUMP_F(CmdUserQuit)
{
    uiG.quit = TRUE;
    return 0;
}


/*
 * Runs a command using temporarely a different current connection
 */
static JUMP_F(CmdUserAsSession)
{
    Connection *saveconn, *tmpconn;
    UDWORD quiet = 0, nr;
    
    saveconn = currconn;
    
    if (s_parsekey (&args, "quiet"))
        quiet = 1;

    if (!s_parseint (&args, &nr))
    {
        /* syntax error */
        return 0;
    }

    tmpconn = ConnectionNr (nr - 1);
    if (!tmpconn && !(tmpconn = ConnectionFind (TYPEF_SERVER, nr, NULL)))
    {
        if (!quiet)
            M_printf (i18n (1894, "There is no connection number %ld.\n"), nr);
        return 0;
    }
    if (~tmpconn->type & TYPEF_SERVER)
    {
        if (!quiet)
            M_printf (i18n (2098, "Connection %ld is not a server connection.\n"), nr);
        return 0;
    }
    currconn = tmpconn;
    CmdUser (args);
    currconn = saveconn;
    return 0;
}


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
                if (conn->type == TYPE_SERVER_OLD)
                    CmdPktCmdSearchUser (conn, args, "", "", "");
                else
                    SnacCliSearchbymail (conn, args);
                
                return 0;
            }
            R_setpromptf ("%s ", i18n (1655, "Enter the user's e-mail address:"));
            return status = 101;
        case 101:
            email = strdup ((char *) args);
            R_setpromptf ("%s ", i18n (1656, "Enter the user's nick name:"));
            return ++status;
        case 102:
            nick = strdup ((char *) args);
            R_setpromptf ("%s ", i18n (1657, "Enter the user's first name:"));
            return ++status;
        case 103:
            first = strdup ((char *) args);
            R_setpromptf ("%s ", i18n (1658, "Enter the user's last name:"));
            return ++status;
        case 104:
            last = strdup ((char *) args);
            if (conn->type == TYPE_SERVER_OLD)
                CmdPktCmdSearchUser (conn, email, nick, first, last);
            else
                SnacCliSearchbypersinf (conn, email, nick, first, last);
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
    static MetaWP wp = { 0 };
    strc_t par;
    char *arg1 = NULL;
    OPENCONN;

    if (!strcmp (args, "."))
        status += 400;

    switch (status)
    {
        case 0:
            if (!s_parse (&args, &par))
            {
                M_print (i18n (1960, "Enter data to search user for. Enter '.' to start the search.\n"));
                R_setpromptf ("%s ", i18n (1656, "Enter the user's nick name:"));
                return 200;
            }
            arg1 = strdup (par->txt);
            if (s_parse (&args, &par))
            {
                if (conn->ver > 6)
                    SnacCliSearchbypersinf (conn, NULL, NULL, arg1, par->txt);
                else
                    CmdPktCmdSearchUser (conn, NULL, NULL, arg1, par->txt);
                free (arg1);
                return 0;
            }
            if (strchr (arg1, '@'))
            {
                if (conn->type == TYPE_SERVER_OLD)
                    CmdPktCmdSearchUser (conn, arg1, "", "", "");
                else
                    SnacCliSearchbymail (conn, arg1);
            }
            else
            {
                if (conn->type == TYPE_SERVER_OLD)
                    CmdPktCmdSearchUser (conn, NULL, arg1, NULL, NULL);
                else
                    SnacCliSearchbypersinf (conn, NULL, arg1, NULL, NULL);
            }
            free (arg1);
            return 0;
        case 200:
            wp.nick = strdup (args);
            R_setpromptf ("%s ", i18n (1657, "Enter the user's first name:"));
            return ++status;
        case 201:
            wp.first = strdup (args);
            R_setpromptf ("%s ", i18n (1658, "Enter the user's last name:"));
            return ++status;
        case 202:
            wp.last = strdup (args);
            R_setpromptf ("%s ", i18n (1655, "Enter the user's e-mail address:"));
            return ++status;
        case 203:
            wp.email = strdup (args);
            R_setpromptf ("%s ", i18n (1604, "Should the users be online?"));
            return ++status;
/* A few more could be added here, but we're gonna make this
 the last one -KK */
        case 204:
            if (strcasecmp (args, i18n (1028, "NO")) && strcasecmp (args, i18n (1027, "YES")))
            {
                M_printf ("%s\n", i18n (1029, "Please enter YES or NO!"));
                R_setpromptf ("%s ", i18n (1604, "Should the users be online?"));
                return status;
            }
            wp.online = strcasecmp (args, i18n (1027, "YES")) ? FALSE : TRUE;
            R_setpromptf ("%s ", i18n (1936, "Enter min age (18-22,23-29,30-39,40-49,50-59,60-120):"));
            return ++status;
        case 205:
            wp.minage = atoi (args);
            R_setpromptf ("%s ", i18n (1937, "Enter max age (22,29,39,49,59,120):"));
            return ++status;
        case 206:
            wp.maxage = atoi (args);
            R_setprompt (i18n (1938, "Enter sex:"));
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
            R_setpromptf ("%s ", i18n (1534, "Enter a language by number or L for a list:"));
            return ++status;
        case 208:
            temp = atoi (args);
            if ((0 == temp) && (toupper (args[0]) == 'L'))
            {
                TablePrintLang ();
                R_setpromptf ("%s ", i18n (1534, "Enter a language by number or L for a list:"));
            }
            else
            {
                wp.language = temp;
                status++;
                R_setpromptf ("%s ", i18n (1939, "Enter a city:"));
            }
            return status;
        case 209:
            wp.city = strdup ((char *) args);
            R_setpromptf ("%s ", i18n (1602, "Enter a state:"));
            return ++status;
        case 210:
            wp.state = strdup ((char *) args);
            R_setpromptf ("%s ", i18n (1578, "Enter country's phone ID number:"));
            return ++status;
        case 211:
            wp.country = atoi ((char *) args);
            R_setpromptf ("%s ", i18n (1579, "Enter company: "));
            return ++status;
        case 212:
            wp.company = strdup ((char *) args);
            R_setpromptf ("%s ", i18n (1587, "Enter department: "));
            return ++status;
        case 213:
            wp.department = strdup ((char *) args);
            R_setpromptf ("%s ", i18n (1603, "Enter position: "));
            return ++status;
        case 214:
            wp.position = strdup ((char *) args);
        default:
            if (conn->ver > 6)
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
    
    if (!(cont = ContactUIN (conn, conn->uin)))
        return 0;
    if (!(user = CONTACT_GENERAL(cont)))
        return 0;

    switch (status)
    {
        case 0:
            R_setpromptf ("%s ", i18n (1553, "Enter Your New Nickname:"));
            return 300;
        case 300:
            s_repl (&user->nick, args);
            R_setpromptf ("%s ", i18n (1554, "Enter your new first name:"));
            return ++status;
        case 301:
            s_repl (&user->first, args);
            R_setpromptf ("%s ", i18n (1555, "Enter your new last name:"));
            return ++status;
        case 302:
            s_repl (&user->last, args);
            s_repl (&user->email, NULL);
            R_setpromptf ("%s ", i18n (1556, "Enter your new email address:"));
            return ++status;
        case 303:
            if (!user->email && args)
                user->email = strdup (args);
            else if (args && *args)
            {
                MetaEmail **em = &cont->meta_email;
                while (*em)
                    em = &(*em)->meta_email;
                *em = calloc (1, sizeof (MetaEmail));
                if (!*em)
                    args = NULL;
                else
                    s_repl (&(*em)->email, args);
            }
            else
            {
                R_setpromptf ("%s ", i18n (1544, "Enter new city:"));
                status += 3;
                return status;
            }
            R_setpromptf ("%s ", i18n (1556, "Enter your new email address:"));
            return status;
        case 306:
            user->city = strdup (args);
            R_setpromptf ("%s ", i18n (1545, "Enter new state:"));
            return ++status;
        case 307:
            user->state = strdup (args);
            R_setpromptf ("%s ", i18n (1546, "Enter new phone number:"));
            return ++status;
        case 308:
            user->phone = strdup (args);
            R_setpromptf ("%s ", i18n (1547, "Enter new fax number:"));
            return ++status;
        case 309:
            user->fax = strdup (args);
            R_setpromptf ("%s ", i18n (1548, "Enter new street address:"));
            return ++status;
        case 310:
            user->street = strdup (args);
            R_setpromptf ("%s ", i18n (1549, "Enter new cellular number:"));
            return ++status;
        case 311:
            user->cellular = strdup (args);
            R_setpromptf ("%s ", i18n (1550, "Enter new zip code (must be numeric):"));
            return ++status;
        case 312:
            user->zip = strdup (args);
            R_setpromptf ("%s ", i18n (1551, "Enter your country's phone ID number:"));
            return ++status;
        case 313:
            user->country = atoi (args);
            R_setpromptf ("%s ", i18n (1552, "Enter your time zone (+/- 0-12):"));
            return ++status;
        case 314:
            user->tz = atoi (args);
            user->tz *= 2;
            R_setpromptf ("%s ", i18n (1557, "Do you want to require Mirabilis users to request your authorization? (YES/NO)"));
            return ++status;
        case 315:
            if (strcasecmp (args, i18n (1028, "NO")) && strcasecmp (args, i18n (1027, "YES")))
            {
                M_printf ("%s\n", i18n (1029, "Please enter YES or NO!"));
                R_setpromptf ("%s ", i18n (1557, "Do you want to require Mirabilis users to request your authorization? (YES/NO)"));
                return status;
            }
            user->auth = strcasecmp (args, i18n (1027, "YES")) ? FALSE : TRUE;
            R_setpromptf ("%s ", i18n (1605, "Do you want your status to be available on the web? (YES/NO)"));
            return ++status;
        case 316:
            if (strcasecmp (args, i18n (1028, "NO")) && strcasecmp (args, i18n (1027, "YES")))
            {
                M_printf ("%s\n", i18n (1029, "Please enter YES or NO!"));
                R_setpromptf ("%s ", i18n (1605, "Do you want your status to be available on the web? (YES/NO)"));
                return status;
            }
            user->webaware = strcasecmp (args, i18n (1027, "YES")) ? FALSE : TRUE;
            R_setpromptf ("%s ", i18n (1857, "Do you want to hide your IP from other contacts? (YES/NO)"));
            return ++status;
        case 317:
            if (strcasecmp (args, i18n (1028, "NO")) && strcasecmp (args, i18n (1027, "YES")))
            {
                M_printf ("%s\n", i18n (1029, "Please enter YES or NO!"));
                R_setpromptf ("%s ", i18n (1857, "Do you want to hide your IP from other contacts? (YES/NO)"));
                return status;
            }
            user->hideip = strcasecmp (args, i18n (1027, "YES")) ? FALSE : TRUE;
            R_setpromptf ("%s ", i18n (1622, "Do you want to apply these changes? (YES/NO)"));
            return ++status;
        case 318:
            if (strcasecmp (args, i18n (1028, "NO")) && strcasecmp (args, i18n (1027, "YES")))
            {
                M_printf ("%s\n", i18n (1029, "Please enter YES or NO!"));
                R_setpromptf ("%s ", i18n (1622, "Do you want to apply these changes? (YES/NO)"));
                return status;
            }

            if (!strcasecmp (args, i18n (1027, "YES")))
            {
                if (conn->ver > 6)
                    SnacCliMetasetgeneral (conn, cont);
                else
                    CmdPktCmdMetaGeneral (conn, cont);
            }
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
    
    if (!(cont = ContactUIN (conn, conn->uin)))
        return 0;
    if (!(more = CONTACT_MORE(cont)))
        return 0;

    switch (status)
    {
        case 0:
            R_setpromptf ("%s ", i18n (1535, "Enter new age:"));
            return 400;
        case 400:
            more->age = atoi (args);
            R_setpromptf ("%s ", i18n (1536, "Enter new sex:"));
            return ++status;
        case 401:
            if (!strncasecmp (args, i18n (1528, "female"), 1))
                more->sex = 1;
            else if (!strncasecmp (args, i18n (1529, "male"), 1))
                more->sex = 2;
            else
                more->sex = 0;
            R_setpromptf ("%s ", i18n (1537, "Enter new homepage:"));
            return ++status;
        case 402:
            s_repl (&more->homepage, args);
            R_setpromptf ("%s ", i18n (1538, "Enter new year of birth (4 digits):"));
            return ++status;
        case 403:
            more->year = atoi (args);
            R_setpromptf ("%s ", i18n (1539, "Enter new month of birth:"));
            return ++status;
        case 404:
            more->month = atoi (args);
            R_setpromptf ("%s ", i18n (1540, "Enter new day of birth:"));
            return ++status;
        case 405:
            more->day = atoi (args);
            R_setpromptf ("%s ", i18n (1534, "Enter a language by number or L for a list:"));
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
            R_setpromptf ("%s ", i18n (1534, "Enter a language by number or L for a list:"));
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
            R_setpromptf ("%s ", i18n (1534, "Enter a language by number or L for a list:"));
            return status;
        case 408:
            temp = atoi (args);
            if (!temp && (toupper (args[0]) == 'L'))
            {
                TablePrintLang ();
                R_setpromptf ("%s ", i18n (1534, "Enter a language by number or L for a list:"));
                return status;
            }
            more->lang3 = temp;
            if (conn->ver > 6)
                SnacCliMetasetmore (conn, cont);
            else
                CmdPktCmdMetaMore (conn, cont);
            return 0;
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

    if (status > 100)
    {
        msg[offset] = 0;
        if (!strcmp (args, END_MSG_STR))
        {
            if (conn->ver > 6)
                SnacCliMetasetabout (conn, msg);
            else
                CmdPktCmdMetaAbout (conn, msg);
            return 0;
        }
        else if (!strcmp (args, CANCEL_MSG_STR))
        {
            return 0;
        }
        else
        {
            if (offset + strlen (args) < 450)
            {
                strcat (msg, args);
                strcat (msg, "\r\n");
                offset += strlen (args) + 2;
            }
            else
            {
                if (conn->ver > 6)
                    SnacCliMetasetabout (conn, msg);
                else
                    CmdPktCmdMetaAbout (conn, msg);
                return 0;
            }
        }
    }
    else
    {
        if (s_parserem (&args, &arg))
        {
            if (conn->ver > 6)
                SnacCliMetasetabout (conn, arg);
            else
                CmdPktCmdMetaAbout (conn, arg);
            return 0;
        }
        offset = 0;
    }
    R_setprompt (i18n (1541, "About> "));
    return 400;
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
void CmdUserInput (time_t *idle_val, UBYTE *idle_flag)
{
    CmdUserProcess (NULL, idle_val, idle_flag);
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
        M_print (i18n (2302, "Too many levels of alias expansion; probably an infinite loop.\n"));
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

/*
 * Process one line of command, get it if necessary.
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

    if (command)
    {
        snprintf (buf, sizeof (buf), "%s", command);
    }
    else
    {
        memset (buf, 0, 1024);
        R_getline (buf, 1024);
        M_print ("\r");             /* reset char printed count for dumb terminals */
        buf[1023] = 0;              /* be safe */
    }

    if (R_isinterrupted ())
        status = 0;
    
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
                R_pause ();
#ifdef SHELL_COMMANDS
                if (((unsigned char)buf[1] < 31) || (buf[1] & 128))
                    M_printf (i18n (1660, "Invalid Command: %s\n"), &buf[1]);
                else
                    system (&buf[1]);
#else
                M_print (i18n (1661, "Shell commands have been disabled.\n"));
#endif
                R_resume ();
                if (!command)
                    R_resetprompt ();
                return;
            }
            args = buf;
            if (!s_parse (&args, &par))
            {
                if (!command)
                    R_resetprompt ();
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
                    R_resetprompt ();
                return;
            }

            else if (!strcasecmp (cmd, "ver"))
            {
                M_print (BuildVersion ());
            }
            else
            {
                char *argsd;
                jump_t *j = (jump_t *)NULL;
                int is_alias = FALSE;
                
                cmd = strdup (cmd);
                argsd = strdup (args);

                if (*cmd != '\xb6')
                {
                    if (*argsd && CmdUserProcessAlias (cmd, argsd + 1, &idle_save, idle_flag))
                        is_alias = TRUE;
                    else
                        j = CmdUserLookup (cmd, CU_USER);
                }
                if (!is_alias && !j)
                    j = CmdUserLookup (*cmd == '\xb6' ? cmd + 1 : cmd, CU_DEFAULT);

                if (j)
                {
                    if (j->unidle == 2)
                        *idle_val = idle_save;
                    else if (j->unidle == 1)
                        *idle_flag = 0;
                    status = j->f (argsd, j->data, 0);
                    sticky = j->f;
                }
                else
                {
                    if (!is_alias)
                    {
                        M_printf (i18n (9999, "Unknown command %s, type /help for help.\n"),
                                  s_mquote (cmd, COLQUOTE, 0));
                    }
                }
                free (cmd);
                free (argsd);
            }
        }
    }
    if (!status && !uiG.quit && !command)
        R_resetprompt ();
}
