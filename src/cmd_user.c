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
#include "util_str.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>

static jump_f
    CmdUserChange, CmdUserRandom, CmdUserHelp, CmdUserInfo, CmdUserTrans,
    CmdUserAuto, CmdUserAlter, CmdUserAlias, CmdUserUnalias, CmdUserMessage,
    CmdUserMessageNG, CmdUserResend, CmdUserPeek,
    CmdUserVerbose, CmdUserRandomSet, CmdUserIgnoreStatus, CmdUserSMS,
    CmdUserStatusDetail, CmdUserStatusWide, CmdUserStatusShort,
    CmdUserSound, CmdUserSoundOnline, CmdUserRegister, CmdUserStatusMeta,
    CmdUserSoundOffline, CmdUserAutoaway, CmdUserSet, CmdUserClear,
    CmdUserTogIgnore, CmdUserTogInvis, CmdUserTogVisible, CmdUserAdd, CmdUserRemove,
    CmdUserAuth, CmdUserURL, CmdUserSave, CmdUserTabs, CmdUserLast,
    CmdUserUptime, CmdUserOldSearch, CmdUserSearch, CmdUserUpdate, CmdUserPass,
    CmdUserOther, CmdUserAbout, CmdUserQuit, CmdUserPeer, CmdUserConn,
    CmdUserContact, CmdUserAnyMess, CmdUserGetAuto;

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
    { &CmdUserAlter,         "alter",        NULL, 0,   0 },
    { &CmdUserAlias,         "alias",        NULL, 0,   0 },
    { &CmdUserUnalias,       "unalias",      NULL, 0,   0 },
    { &CmdUserAnyMess,       "message",      NULL, 0,   0 },
    { &CmdUserMessageNG,     "msg",          NULL, 0,   1 },
    { &CmdUserMessageNG,     "r",            NULL, 0,   2 },
    { &CmdUserMessageNG,     "a",            NULL, 0,   4 },
    { &CmdUserGetAuto,       "getauto",      NULL, 0,   0 },
    { &CmdUserMessage,       "msg-old",      NULL, 0,   1 },
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
    { &CmdUserStatusShort,   "w-old",        NULL, 2,   1 },
    { &CmdUserStatusShort,   "e-old",        NULL, 2,   0 },
    { &CmdUserStatusWide,    "wide",         NULL, 2,   1 },
    { &CmdUserStatusWide,    "ewide",        NULL, 2,   0 },
    { &CmdUserSet,           "set",          NULL, 0,   0 },
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
        if      (data & STATUSF_DND)  s_repl (&prG->auto_dnd,  arg1);
        else if (data & STATUSF_OCC)  s_repl (&prG->auto_occ,  arg1);
        else if (data & STATUSF_NA)   s_repl (&prG->auto_na,   arg1);
        else if (data & STATUSF_AWAY) s_repl (&prG->auto_away, arg1);
        else if (data & STATUSF_FFC)  s_repl (&prG->auto_ffc,  arg1);
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
    char *arg1 = NULL;
    char what;

    s_parse (&args, &arg1);

    if (!arg1) what = 0;
    else if (!strcasecmp (arg1, i18n (1447, "Client"))   || !strcasecmp (arg1, "Client"))   what = 1;
    else if (!strcasecmp (arg1, i18n (1448, "Message"))  || !strcasecmp (arg1, "Message"))  what = 2;
    else if (!strcasecmp (arg1, i18n (1449, "User"))     || !strcasecmp (arg1, "User"))     what = 3;
    else if (!strcasecmp (arg1, i18n (1450, "Account"))  || !strcasecmp (arg1, "Account"))  what = 4;
    else if (!strcasecmp (arg1, i18n (2171, "Advanced")) || !strcasecmp (arg1, "Advanced")) what = 5;
    else what = 0;

    if (!what)
    {
        const char *fmt = i18n (2184, "%s%-10s%s - %s\n");
        M_printf ("%s\n", i18n (1442, "Please select one of the help topics below."));
        M_printf (fmt, COLCLIENT, i18n (1448, "Message"), COLNONE,
                 i18n (1446, "Commands relating to sending messages."));
        M_printf (fmt, COLCLIENT, i18n (1447, "Client"), COLNONE,
                 i18n (1443, "Commands relating to mICQ displays and configuration."));
        M_printf (fmt, COLCLIENT, i18n (1449, "User"), COLNONE,
                 i18n (1444, "Commands relating to finding and seeing other users."));
        M_printf (fmt, COLCLIENT, i18n (1450, "Account"), COLNONE,
                 i18n (1445, "Commands relating to your ICQ account."));
        M_printf (fmt, COLCLIENT, i18n (2171, "Advanced"), COLNONE,
                 i18n (2172, "Commands for advanced features."));
    }
    else if (what == 1)
    {
        M_printf (COLMESSAGE "%s [<nr>]" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("verbose"),
                  i18n (1418, "Set the verbosity level, or display verbosity level."));
        M_printf (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("clear"),
                  i18n (1419, "Clears the screen."));
        M_printf (COLMESSAGE "%s [%s|%s|event]" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("sound"), i18n (1085, "on"), i18n (1086, "off"),
                  i18n (1420, "Switches beeping when receiving new messages on or off, or using the event script."));
        M_printf (COLMESSAGE "%s [<timeout>]" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("autoaway"),
                  i18n (1767, "Toggles auto cycling to away/not available."));
        M_printf (COLMESSAGE "%s [on|off]" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("auto"),
                  i18n (1424, "Set whether autoreplying when not online, or displays setting."));
        M_printf (COLMESSAGE "%s <status> <message>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("auto"),
                  i18n (1425, "Sets the message to send as an auto reply for the status."));
        M_printf (COLMESSAGE "%s [<alias> <expansion>]" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("alias"),
                  i18n (2300, "Set an alias or list current aliases."));
        M_printf (COLMESSAGE "%s <alias>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("unalias"),
                  i18n (2301, "Delete an alias."));
        M_printf (COLMESSAGE "%s [<lang|nr>...]" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("trans"), 
                  i18n (1800, "Change the working language to <lang> or display string <nr>."));  
        M_printf (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("uptime"),
                  i18n (1719, "Shows how long mICQ has been running and some statistics."));
        M_printf (COLMESSAGE "%s <option> <value>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("set"),
                  i18n (2044, "Set, clear or display an <option>: hermit, delbs, log, logonoff, auto, uinprompt, autosave, autofinger, linebreak, tabs, silent."));
        M_printf (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("save"),
                  i18n (2036, "Save current preferences to disc."));
        M_printf (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("q"),
                  i18n (1422, "Logs off and quits."));
        M_printf ("  " COLCLIENT "" COLINDENT "%s" COLEXDENT "" COLNONE "\n",
                  i18n (1717, "'!' as the first character of a command will execute a shell command (e.g. '!ls', '!dir', '!mkdir temp')"));
    }
    else if (what == 2)
    {
        M_printf (COLMESSAGE "%s <contacts> [<message>]" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("msg"),
                  i18n (1409, "Sends a message to <contacts>."));
        M_printf (COLMESSAGE "%s <message>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("a"),
                  i18n (1412, "Sends a message to the last person you sent a message to."));
        M_printf (COLMESSAGE "%s <message>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("r"),
                  i18n (1414, "Replies to the last person to send you a message."));
        M_printf (COLMESSAGE "%s <contacts> <url> <message>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("url"),
                  i18n (1410, "Sends a url and message to <contacts>."));
        M_printf (COLMESSAGE "%s <nick|cell> <message>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("sms"),
                  i18n (2039, "Sends a message to a (<nick>'s) <cell> phone."));
        M_printf (COLMESSAGE "%s [auto|away|na|dnd|occ|ffc] [<contacts>]" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("getauto"),
                  i18n (2304, "Get automatic reply from all <contacts> for current status, away, not available, do not disturb, occupied, or free for chat."));
        M_printf (COLMESSAGE "%s [grant|deny|req|add] <contacts>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("auth"),
                  i18n (2313, "Grant, deny, request or acknowledge authorization from/to <contacts> to you/them to their/your contact list."));
        M_printf (COLMESSAGE "%s <contacts>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("resend"),
                  i18n (1770, "Resend your last message to <contacts>."));
        M_printf (COLMESSAGE "%s [<contacts>]" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("last"),
                  i18n (1403, "Displays the last message received from <contacts> or from everyone."));
        M_printf (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("tabs"),
                  i18n (1098, "Display a list of nicknames that you can tab through.")); 
        M_printf ("  " COLCLIENT "" COLINDENT "%s" COLEXDENT "" COLNONE "\n",
                  i18n (1720, "<contacts> is a comma seperated list of UINs and nick names of users."));
        M_printf ("  " COLCLIENT "" COLINDENT "%s" COLEXDENT "" COLNONE "\n",
                  i18n (1721, "Sending a blank message will put the client into multiline mode.\nUse . on a line by itself to end message.\nUse # on a line by itself to cancel the message."));
    }
    else if (what == 3)
    {
        M_printf (COLMESSAGE "%s [<nr>]" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("rand"),
                  i18n (1415, "Finds a random user in interest group <nr> or lists the groups."));
        M_printf (COLMESSAGE "%s <uin|nick>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("s"),
                  i18n (1400, "Shows locally stored info on <uin> or <nick>, or on yourself."));
        M_printf (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("e"),
                  i18n (1407, "Displays the current status of online people on your contact list."));
        M_printf (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("w"),
                  i18n (1416, "Displays the current status of everyone on your contact list."));
        M_printf (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("ee"),
                  i18n (2177, "Displays verbose the current status of online people on your contact list."));
        M_printf (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("ww"),
                  i18n (2176, "Displays verbose the current status of everyone on your contact list."));
        M_printf (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("eg"),
                  i18n (2307, "Displays the current status of online people on your contact list, sorted by contact group."));
        M_printf (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("wg"),
                  i18n (2308, "Displays the current status of everyone on your contact list, sorted by contact group."));
        M_printf (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("eeg"),
                  i18n (2309, "Displays verbose the current status of online people on your contact list, sorted by contact group."));
        M_printf (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("wwg"),
                  i18n (2310, "Displays verbose the current status of everyone on your contact list, sorted by contact group."));
        M_printf (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("ewide"),
                  i18n (2042, "Displays a list of online people on your contact list in a screen wide format.")); 
        M_printf (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("wide"),
                  i18n (1801, "Displays a list of people on your contact list in a screen wide format.")); 
        M_printf (COLMESSAGE "%s <uin|nick>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("finger"),
                  i18n (1430, "Displays general info on <uin> or <nick>."));
        M_printf (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("i"),
                  i18n (1405, "Lists ignored nicks/uins."));
        M_printf (COLMESSAGE "%s [<email>|<nick>|<first> <last>]" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("search"),
                  i18n (1429, "Searches for an ICQ user."));
        M_printf (COLMESSAGE "%s <group> [<contacts>]" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("addgroup"),
                  i18n (1428, "Adds all contacts in <contacts> to contact group <group>."));
        M_printf (COLMESSAGE "%s <uin|nick> [<new nick>]" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("addalias"),
                  i18n (2311, "Adds <uin> as <new nick>, or add alias <new nick> for <uin> or <nick>."));
        M_printf (COLMESSAGE "%s [all] group <contacts>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("remgroup"),
                  i18n (2043, "Remove all contacts in <contacts> from contact group <group>."));
        M_printf (COLMESSAGE "%s [all] <contacts>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("remalias"),
                  i18n (2312, "Remove all aliases in <contacts>, removes contact if 'all' is given."));
        M_printf (COLMESSAGE "%s <contacts>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("togig"),
                  i18n (1404, "Toggles ignoring/unignoring the <contacts>."));
        M_printf (COLMESSAGE "%s <contacts>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("toginv"),
                  i18n (2045, "Toggles your visibility to <contacts> when you're online."));
        M_printf (COLMESSAGE "%s <contacts>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("togvis"),
                  i18n (1406, "Toggles your visibility to <contacts> when you're invisible."));
    }
    else if (what == 4)
    {
        M_printf (COLMESSAGE "%s <password>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("reg"),
                  i18n (1426, "Creates a new UIN with the specified password."));
        M_printf (COLMESSAGE "%s <password>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("pass"),
                  i18n (1408, "Changes your password to <password>."));
        M_printf (COLMESSAGE "%s <status> [<away-message>]" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("change"),
                  i18n (1427, "Changes your status to the status number, or list the available modes."));
        M_printf (COLMESSAGE "%s|%s|%s|%s|%s|%s|%s [<away-message>]" COLNONE "\n\t" COLINDENT "%s\n%s\n%s\n%s\n%s\n%s\n%s" COLEXDENT "\n", 
                  CmdUserLookupName ("online"),
                  CmdUserLookupName ("away"),
                  CmdUserLookupName ("na"),
                  CmdUserLookupName ("occ"),
                  CmdUserLookupName ("dnd"),
                  CmdUserLookupName ("ffc"),
                  CmdUserLookupName ("inv"),
                  i18n (1431, "Set status to \"online\"."),
                  i18n (1432, "Set status to \"away\"."),
                  i18n (1433, "Set status to \"not available\"."),
                  i18n (1434, "Set status to \"occupied\"."),
                  i18n (1435, "Set status to \"do not disturb\"."),
                  i18n (1436, "Set status to \"free for chat\"."),
                  i18n (1437, "Set status to \"invisible\"."));
        M_printf (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("update"),
                  i18n (1438, "Updates your basic info (email, nickname, etc.)."));
        M_printf (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("other"),
                  i18n (1401, "Updates more user info like age and sex."));
        M_printf (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("about"),
                  i18n (1402, "Updates your about user info."));
        M_printf (COLMESSAGE "%s <nr>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n", 
                  CmdUserLookupName ("setr"),
                  i18n (1439, "Sets your random user group."));
    }
    else if (what == 5)
    {
        M_printf (i18n (2314, "These are advanced commands. Be sure to have read the manual pages for complete information.\n"));
        M_printf (COLMESSAGE "%s [show|load|save|set|get|rget] <contacts>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("meta"),
                  i18n (2305, "Handle meta data of contacts."));
        M_printf (COLMESSAGE "%s open|close|off <uin|nick>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("peer"),
                  i18n (2037, "Open or close a peer-to-peer connection, or disable using peer-to-peer connections for <uin> or <nick>."));
        M_printf (COLMESSAGE "%s file <contacts> <file> <description>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("peer"),
                  i18n (2179, "Send all <contacts> a single file."));
        M_printf (COLMESSAGE "%s files <uin|nick> <file1> <as1> ... <fileN> <asN> <description>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("peer"),
                  i18n (2180, "Send <uin> or <nick> several files."));
        M_printf (COLMESSAGE "%s accept <uin|nick> [<id>]" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("peer"),
                  i18n (2319, "Accept an incoming file transfer from <uin> or <nick>."));
        M_printf (COLMESSAGE "%s open|login [<nr>]" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("conn"),
                  i18n (2038, "Opens connection number <nr>, or the first server connection."));
        M_printf (COLMESSAGE "%s close|remove <nr>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("conn"),
                  i18n (2181, "Closes or removes connection number <nr>."));
        M_printf (COLMESSAGE "%s select [<nr>]" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("conn"),
                  i18n (2182, "Select server connection number <nr> as current server connection."));
        M_printf (COLMESSAGE "%s [show|diff|import]" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("contact"),
                  i18n (2306, "Request server side contact list and show all or new contacts or import."));
        M_printf (COLMESSAGE "%s <contacts>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                  CmdUserLookupName ("peek"),
                  i18n (2183, "Check all <contacts> whether they are offline or invisible."));
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
#ifdef ENABLE_UTF8
        if (*arg1 == '\xc3' && arg1[1] == '\xb3')
#else
        if (*arg1 == '\xf3')
#endif
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
    Contact *cont = NULL, *contr = NULL;
    char *arg1 = NULL, *arg2 = NULL;
    UDWORD i;
    OPENCONN;
    
    if (conn->ver < 6)
    {
        M_print (i18n (2013, "This command is v8 only.\n"));
        return 0;
    }
    if (s_parsenick (&args, &cont, &contr, conn))
    {
        CONTACT_GENERAL (contr);
        arg2 = args;
        if ((!contr->meta_general->cellular || !contr->meta_general->cellular[0])
            && s_parseint (&arg2, &i) && s_parse (&args, &arg1))
        {
            s_repl (&contr->meta_general->cellular, arg1);
            contr->updated = 0;
        }
        arg1 = contr->meta_general->cellular;
    }
    if ((!arg1 || !arg1[0]) && !s_parse (&args, &arg1))
    {
        M_print (i18n (2014, "No number given.\n"));
        return 0;
    }
    if (arg1[0] != '+' || arg1[1] == '0')
    {
        M_printf (i18n (2250, "Number '%s' is not of the format +<countrycode><cellprovider><number>.\n"), arg1);
        if (contr && contr->meta_general)
            s_repl (&contr->meta_general->cellular, NULL);
        return 0;
    }
    arg1 = strdup (arg1);
    if (!s_parserem (&args, &arg2))
        M_print (i18n (2015, "No message given.\n"));
    else
        SnacCliSendsms (conn, arg1, arg2);
    free (arg1);
    return 0;
}

/*
 * Queries basic info of a user
 */
static JUMP_F(CmdUserInfo)
{
    Contact *cont = NULL, *contr = NULL;
    OPENCONN;

    while (*args || data)
    {
        if (!data && !s_parsenick_s (&args, &cont, MULTI_SEP, &contr, conn))
        {
            M_printf (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
            return 0;
        }
        if (*args == ',')
            args++;
        if (data)
        {
            contr = ContactUIN (conn, uiG.last_rcvd_uin);
            if (!contr)
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
        if (!s_parsenick_s (&args, &cont, MULTI_SEP, NULL, conn))
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
    char *arg1 = NULL, *t = NULL;
    const char *p = NULL;
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
        if (s_parse (&args, &arg1))
        {
            if (!strcmp (arg1, "all"))
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
                if (!strcmp (arg1, ".") || !strcmp (arg1, "none") || !strcmp (arg1, "unload"))
                {
                    t = strdup (i18n (1089, "Unloaded translation."));
                    i18nClose ();
                    M_printf ("%s\n", t);
                    free (t);
                    continue;
                }
                i = i18nOpen (arg1, prG->enc_loc);
                if (i == -1)
                    M_printf (i18n (1080, "Couldn't load \"%s\" internationalization.\n"), arg1);
                else if (i)
                    M_printf (i18n (1081, "Successfully loaded en translation (%ld entries).\n"), i);
                else
                    M_print ("No internationalization requested.\n");
            }
            one = 1;
        }
        else if (one)
            return 0;
        else
        {
            UDWORD v1 = 0, v2 = 0, v3 = 0, v4 = 0;
            const char *s = "1079:No translation; using compiled-in strings.\n";

            arg1 = strdup (i18n (1003, "0"));
            for (t = arg1; *t; t++)
                if (*t == '-')
                    *t = ' ';
            s_parseint (&arg1, &v1);
            s_parseint (&arg1, &v2);
            s_parseint (&arg1, &v3);
            s_parseint (&arg1, &v4);
            
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
        char *argst = args, *arg1 = NULL;
        if (!s_parse (&args, &arg1))     data = 0;
        else if (!strcmp (arg1, "auto")) data = 0;
        else if (!strcmp (arg1, "away")) data = MSGF_GETAUTO | MSG_GET_AWAY;
        else if (!strcmp (arg1, "na"))   data = MSGF_GETAUTO | MSG_GET_NA;
        else if (!strcmp (arg1, "dnd"))  data = MSGF_GETAUTO | MSG_GET_DND;
        else if (!strcmp (arg1, "ffc"))  data = MSGF_GETAUTO | MSG_GET_FFC;
        else if (!strcmp (arg1, "occ"))  data = MSGF_GETAUTO | MSG_GET_OCC;
#ifdef WIP
        else if (!strcmp (arg1, "ver"))  data = MSGF_GETAUTO | MSG_GET_VER;
#endif
        else args = argst;
    }
    while (s_parsenick_s (&args, &cont, MULTI_SEP, NULL, conn))
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
        char *arg1 = NULL;
        if (!s_parse (&args, &arg1)) data = 0;
        else if (!strcmp (arg1, "open"))   data = 1;
        else if (!strcmp (arg1, "close"))  data = 2;
        else if (!strcmp (arg1, "reset"))  data = 2;
        else if (!strcmp (arg1, "off"))    data = 3;
        else if (!strcmp (arg1, "file"))   data = 4;
        else if (!strcmp (arg1, "files"))  data = 5;
        else if (!strcmp (arg1, "accept")) data = 6;
        else if (!strcmp (arg1, "deny"))   data = 7;
        else if (!strcmp (arg1, "auto"))   data = 10;
        else if (!strcmp (arg1, "away"))   data = 11;
        else if (!strcmp (arg1, "na"))     data = 12;
        else if (!strcmp (arg1, "dnd"))    data = 13;
        else if (!strcmp (arg1, "ffc"))    data = 14;
    }
    while (data && s_parsenick_s (&args, &cont, MULTI_SEP, NULL, conn))
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
                    const char *files[1], *ass[1];
                    char *des = NULL, *file;
                    
                    if (!s_parse (&args, &file))
                    {
                        M_print (i18n (2158, "No file given.\n"));
                        return 0;
                    }
                    files[0] = file = strdup (file);
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
                    char *files[10], *ass[10], *des = NULL, *as;
                    int count;
                    
                    for (count = 0; count < 10; count++)
                    {
                        if (!s_parse (&args, &des))
                        {
                            des = strdup (i18n (2159, "Some files."));
                            break;
                        }
                        files[count] = des = strdup (des);
                        if (!s_parse (&args, &as))
                            break;
                        if (*as == '/' && !*(as + 1))
                            as = (strchr (des, '/')) ? strrchr (des, '/') + 1 : des;
                        if (*as == '.' && !*(as + 1))
                            as = des;
                        ass[count] = as = strdup (as);
                    }
                    if (!count)
                    {
                        free (des);
                        M_print (i18n (2158, "No file given.\n"));
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
    char *arg1 = NULL, *arg2 = NULL;

    if (!s_parse (&args, &arg1))
    {
        M_printf (i18n (1724, "Automatic replies are %s.\n"),
                 prG->flags & FLAG_AUTOREPLY ? i18n (1085, "on") : i18n (1086, "off"));
        M_printf ("%30s %s\n", i18n (1727, "The \"do not disturb\" message is:"), prG->auto_dnd);
        M_printf ("%30s %s\n", i18n (1728, "The \"away\" message is:"),           prG->auto_away);
        M_printf ("%30s %s\n", i18n (1729, "The \"not available\" message is:"),  prG->auto_na);
        M_printf ("%30s %s\n", i18n (1730, "The \"occupied\" message is:"),       prG->auto_occ);
        M_printf ("%30s %s\n", i18n (1731, "The \"invisible\" message is:"),      prG->auto_inv);
        M_printf ("%30s %s\n", i18n (2054, "The \"free for chat\" message is:"),  prG->auto_ffc);
        return 0;
    }

    if      (!strcasecmp (arg1, "on")  || !strcasecmp (arg1, i18n (1085, "on")))
    {
        prG->flags |= FLAG_AUTOREPLY;
        M_printf (i18n (1724, "Automatic replies are %s.\n"), i18n (1085, "on"));
        return 0;
    }
    else if (!strcasecmp (arg1, "off") || !strcasecmp (arg1, i18n (1086, "off")))
    {
        prG->flags &= ~FLAG_AUTOREPLY;
        M_printf (i18n (1724, "Automatic replies are %s.\n"), i18n (1086, "off"));
        return 0;
    }
    
    arg1 = strdup (arg1);

    if (!s_parserem (&args, &arg2))
    {
        M_print (i18n (1735, "Must give a message.\n"));
        free (arg1);
        return 0;
    }

    if      (!strcasecmp (arg1, "dnd")  || !strcasecmp (arg1, CmdUserLookupName ("dnd")))
        s_repl       (&prG->auto_dnd,  arg2);
    else if (!strcasecmp (arg1, "away") || !strcasecmp (arg1, CmdUserLookupName ("away")))
        s_repl       (&prG->auto_away, arg2);
    else if (!strcasecmp (arg1, "na")   || !strcasecmp (arg1, CmdUserLookupName ("na")))
        s_repl       (&prG->auto_na,   arg2);
    else if (!strcasecmp (arg1, "occ")  || !strcasecmp (arg1, CmdUserLookupName ("occ")))
        s_repl       (&prG->auto_occ,  arg2);
    else if (!strcasecmp (arg1, "inv")  || !strcasecmp (arg1, CmdUserLookupName ("inv")))
        s_repl       (&prG->auto_inv,  arg2);
    else if (!strcasecmp (arg1, "ffc")  || !strcasecmp (arg1, CmdUserLookupName ("ffc")))
        s_repl       (&prG->auto_ffc,  arg2);
    else
        M_printf (i18n (2113, "Unknown status '%s'.\n"), arg1);
    free (arg1);
    return 0;
}

/*
 * Relabels commands.
 */

static JUMP_F(CmdUserAlter)
{
    char *arg1 = NULL, *arg2 = NULL;
    jump_t *j;
    int quiet = 0;

    s_parse (&args, &arg1);

    if (arg1 && !strcasecmp ("quiet", arg1))
    {
        quiet = 1;
        s_parse (&args, &arg1);
    }
        
    if (!arg1)
    {
        M_print (i18n (1738, "Need a command to alter.\n"));
        return 0;
    }
    
    j = CmdUserLookup (arg1, CU_DEFAULT);
    if (!j)
        j = CmdUserLookup (arg1, CU_USER);
    if (!j)
    {
        M_printf (i18n (2114, "The command name '%s' does not exist.\n"), arg1);
        return 0;
    }
    
    if (s_parse (&args, &arg2))
    {
        if (CmdUserLookup (arg2, CU_USER))
        {
            if (!quiet)
                M_printf (i18n (1768, "The command name '%s' is already being used.\n"), arg2);
            return 0;
        }
        else
            s_repl (&j->name, arg2);
    }
    
    if (!quiet)
    {
        if (j->name)
            M_printf (i18n (1763, "The command '%s' has been renamed to '%s'."), j->defname, j->name);
        else
            M_printf (i18n (1764, "The command '%s' is unchanged."), j->defname);
        M_print ("\n");
    }
    return 0;
}

/*
 * Add an alias.
 */

static JUMP_F(CmdUserAlias)
{
    char *name = NULL, *exp = NULL;

    s_parse_s (&args, &name, " \t\r\n=");

    if (!name)
    {
        alias_t *node;

        for (node = aliases; node; node = node->next)
            M_printf ("alias %s %s\n", node->name, node->expansion);
        
        return 0;
    }
    
    if ((exp = strpbrk (name, " \t\r\n")))
    {
        M_printf (i18n (2303, "Invalid character 0x%x in alias name.\n"), *exp);
        return 0;
    }

    if (*args == '=')
        args++;
    
    name = strdup (name);
    s_parserem (&args, &exp);

    if (!exp)
    {
        alias_t *node;

        for (node = aliases; node; node = node->next)
            if (!strcasecmp (node->name, name))
            {
                M_printf ("alias %s %s\n", node->name, node->expansion);
                free (name);
                return 0;
            }

        free (name);
        M_print (i18n (2297, "Alias to what?\n"));
        return 0;
    }
    
    CmdUserSetAlias (name, exp);

    free (name);
    return 0;
}

/*
 * Remove an alias.
 */

static JUMP_F(CmdUserUnalias)
{
    char *arg = NULL;

    s_parse (&args, &arg);

    if (!arg)
        M_print (i18n (2298, "Remove which alias?\n"));
    else if (!CmdUserRemoveAlias (arg))
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

    if (!s_parsenick_s (&args, &cont, MULTI_SEP, NULL, conn))
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
        if (!s_parsenick_s (&args, &cont, MULTI_SEP, NULL, conn))
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
    char *arg1 = NULL;
    UDWORD i, f = 0;
    static UDWORD size;
    static char *t = NULL;
    ANYCONN;

    if (!(data & 3))
    {
        if (!s_parse (&args, &arg1))
            return 0;
        if (!strcmp (arg1, "peer"))
            data |= 1;
        else if (!strcmp (arg1, "srv"))
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
    if (!t)
        t = s_catf (t, &size, " ");
    *t = 0;

    if (!s_parsenick (&args, &cont, NULL, conn))
        return 0;
    
    if (!s_parse (&args, &arg1))
        return 0;
    
    t = s_catf (t, &size, "<%s>", arg1);

    while (s_parse (&args, &arg1))
        t = s_catf (t, &size, "%c<%s>", Conv0xFE, arg1);
        
    if (data & 1)
    {
#ifdef ENABLE_PEER2PEER
        if (!conn->assoc || !TCPDirectOpen  (conn->assoc, cont))
        {
            M_printf (i18n (2142, "Direct connection with %s not possible.\n"), cont->nick);
            return 0;
        }
        TCPSendMsg (conn->assoc, cont, t, data >> 2);
    }
#endif
    else
    {
        if (conn->type != TYPE_SERVER)
            CmdPktCmdSendMessage (conn, cont, t, data >> 2);
        else if (f != 2)
            SnacCliSendmsg (conn, cont, t, data >> 2, f);
        else
            SnacCliSendmsg2 (conn, cont, ExtraSet (ExtraSet (NULL, EXTRA_FORCE, 1, NULL), EXTRA_MESSAGE, data >> 2, t));
    }
    return 0;
}

/*
 * Send an instant message.
 */
static JUMP_F (CmdUserMessageNG)
{
    static UDWORD size = 0;
    static char *msg = NULL;
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
                    if (!s_parsenick_s (&args, &cont, MULTI_SEP, NULL, conn))
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
                TabAddUIN (cont->uin);
            }
            return 0;
        }
        if (!uinlist[1])
            M_printf (i18n (2131, "Composing message to %s%s%s:\n"), COLCONTACT, ContactUIN (conn, uinlist[0])->nick, COLNONE);
        else
            M_printf (i18n (2131, "Composing message to %s%s%s:\n"), COLMESSAGE, i18n (2220, "several"), COLNONE);
        msg = s_cat (msg, &size, "");
        *msg = 0;
        if (arg1)
        {
            arg1[strlen (arg1) - 1] = '\0';
            msg = s_cat (msg, &size, arg1);
            msg = s_cat (msg, &size, "\r\n");
        }
        status = 1;
    }
    else
    {
        if (!strcmp (args, END_MSG_STR))
        {
            arg1 = msg + strlen (msg) - 1;
            while (*msg && strchr ("\r\n\t ", *arg1))
                *arg1-- = '\0';
            s_repl (&uiG.last_message_sent, msg);
            uiG.last_message_sent_type = MSG_NORM;
            for (i = 0; uinlist[i]; i++)
            {
                if (!(cont = ContactUIN (conn, uinlist[i])))
                    continue;
                IMCliMsg (conn, cont, ExtraSet (NULL, EXTRA_MESSAGE, MSG_NORM, msg));
                uiG.last_sent_uin = cont->uin;
                TabAddUIN (cont->uin);
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
            msg = s_cat (msg, &size, args);
            msg = s_cat (msg, &size, "\r\n");
        }
    }
    R_setprompt (i18n (1041, "msg> "));
    return status;
}

/*
 * Send an instant message.
 */
static JUMP_F (CmdUserMessage)
{
    static int offset = 0;
    static char msg[1024];
    static UDWORD uinlist[20] = { 0 };
    char *arg1 = NULL;
    UDWORD i;
    Contact *cont = NULL;
    OPENCONN;

    if (status)
    {
        arg1 = args;
        msg[offset] = 0;
        if (strcmp (arg1, END_MSG_STR) == 0)
        {
            msg[offset - 1] = msg[offset - 2] = 0;
            for (i = 0; uinlist[i]; i++)
            {
                icq_sendmsg (conn, uiG.last_sent_uin = uinlist[i], msg, MSG_NORM);
                TabAddUIN (uinlist[i]);
            }
            return 0;
        }
        else if (strcmp (arg1, CANCEL_MSG_STR) == 0)
        {
            M_print (i18n (1038, "Message canceled.\n"));
            return 0;
        }
        else
        {
            int diff, first = 1;

            while (offset + strlen (arg1) + 2 > 450)
            {
                M_print (i18n (1037, "Message partially sent.\n"));
                if (first)
                {
                    diff = 0;
                    first = 0;
                }
                else
                {
                    diff = 450 - offset - 2;
                    while (arg1[diff] != ' ')
                        if (!--diff)
                            break;
                    diff = diff ? diff : 450 - offset - 2;
                    snprintf (msg + offset, diff, "%s\r\n", arg1);
                    arg1 += diff;
                    while (*arg1 == ' ')
                        arg1++;
                }
                for (i = 0; uinlist[i]; i++)
                {
                    icq_sendmsg (conn, uiG.last_sent_uin = uinlist[i], msg, MSG_NORM);
                    TabAddUIN (uinlist[i]);
                }
                msg[0] = '\0';
                offset = 0;
            }
            strcat (msg, arg1);
            strcat (msg, "\r\n");
            offset += strlen (arg1) + 2;
        }
    }
    else
    {
        switch (data)
        {
            case 1:
                i = 0;
                if (!*args)
                {
                    M_print (i18n (2235, "No nick name given.\n"));
                    return 0;
                }
                while (*args)
                {
                    if (!s_parsenick_s (&args, &cont, MULTI_SEP, NULL, conn))
                    {
                        M_printf (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
                        return 0;
                    }
                    uinlist[i++] = cont->uin;
                    if (*args != ',')
                        break;
                    args++;
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
        if (arg1)
        {
            for (i = 0; uinlist[i]; i++)
            {
                icq_sendmsg (conn, uiG.last_sent_uin = uinlist[i], arg1, MSG_NORM);
                TabAddUIN (uinlist[i]);
            }
            return 0;
        }
        if (data == 8)
            M_printf (i18n (2130, "Composing message to %sall%s:\n"), COLCONTACT, COLNONE);
        else if (!uinlist[1])
            M_printf (i18n (2131, "Composing message to %s%s%s:\n"), COLCONTACT, ContactUIN (conn, uinlist[0])->nick, COLNONE);
        else
            M_printf (i18n (2131, "Composing message to %s%s%s:\n"), COLMESSAGE, i18n (2220, "several"), COLNONE);
        offset = 0;
        status = data;
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
    if (cont->flags   & CONT_IGNORE)     return 0xfffffffe;
    if (cont->flags   & CONT_TEMPORARY)  return 0xfffffffe;
    if (cont->status == STATUS_OFFLINE)  return STATUS_OFFLINE;
    if (cont->status  & STATUSF_BIRTH)   return STATUSF_BIRTH;
    if (cont->status  & STATUSF_DND)     return STATUS_DND;
    if (cont->status  & STATUSF_OCC)     return STATUS_OCC;
    if (cont->status  & STATUSF_NA)      return STATUS_NA;
    if (cont->status  & STATUSF_AWAY)    return STATUS_AWAY;
    if (cont->status  & STATUSF_FFC)     return STATUS_FFC;
    return STATUS_ONLINE;
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
    UDWORD uin = 0, tuin = 0;
    int i, j, k;
#ifdef CONFIG_UNDERLINE
    int l;
#endif
    int lenuin = 0, lennick = 0, lenstat = 0, lenid = 0, totallen = 0;
    Contact *cont = NULL, *alias = NULL;
    Connection *peer;
    char *argst, *arg1;
    UDWORD stati[] = { 0xfffffffe, STATUS_OFFLINE, STATUS_DND,    STATUS_OCC, STATUS_NA,
                                   STATUS_AWAY,    STATUS_ONLINE, STATUS_FFC, STATUSF_BIRTH };
    ANYCONN;

    if (!data)
        s_parseint (&args, &data);

    if (~data & 32)
    {
        argst = args;
        if (s_parse (&argst, &arg1))
            if ((tcg = cg = ContactGroupFind (0, conn, arg1, 0)))
                args = argst;
    }

    if ((data & 8) && !cg && (!conn || !s_parsenick (&args, &cont, NULL, conn)) && *args)
    {
        M_printf (i18n (1700, "%s is not a valid user in your list.\n"), args);
        return 0;
    }

    if (cont)
        uin = cont->uin;

    if (data & 32)
    {
        if (!(tcg = ContactGroupFind (0xcafe, conn, "\x01\x02\x03\x04\x05", 1)))
        {
            M_print (i18n (2118, "Out of memory.\n"));
            return 0;
        }
        while ((cont = ContactIndex (tcg, 0)))
            ContactRem (tcg, cont);
        cg = conn->contacts;
        for (j = 0; (cont = ContactIndex (cg, j)); j++)
            if (~cont->flags & CONT_TEMPORARY && ~cont->flags & CONT_ALIAS)
                if(!ContactFind (tcg, 0, cont->uin, NULL, 0))
                    ContactAdd (tcg, cont);
        for (i = 0; (cg = ContactGroupIndex (i)); i++)
            if (cg->serv == conn && cg != conn->contacts && cg != tcg)
                for (j = 0; (cont = ContactIndex (cg, j)); j++)
                    ContactRem (tcg, cont);
        cg = conn->contacts;
    }

    if (!cg)
        cg = conn->contacts;

    for (i = 0; (cont = ContactIndex (tcg && ~data & 32 ? tcg : cg, i)); i++)
    {
        if (uin && cont->uin != uin)
            continue;
        if (cont->flags & (CONT_TEMPORARY | CONT_ALIAS) && ~data & 2)
            continue;
        if (cont->uin > tuin)
            tuin = cont->uin;
        if (s_strlen (cont->nick) > lennick)
            lennick = s_strlen (cont->nick);
        if (cont->flags & CONT_ALIAS)
            continue;
        if (s_strlen (s_status (cont->status)) > lenstat)
            lenstat = s_strlen (s_status (cont->status));
        if (cont->version && s_strlen (cont->version) > lenid)
            lenid = s_strlen (cont->version);
    }

    if (lennick > uiG.nick_len)
        uiG.nick_len = lennick;
    while (tuin)
        lenuin++, tuin /= 10;
    totallen = 1 + lennick + 1 + lenstat + 3 + lenid + 2;
    if (prG->verbose)
        totallen += 29;
    if (data & 2)
        totallen += 2 + 3 + 1 + 1 + lenuin + 19;

    if (data & 4 && !uin)
    {
        if (conn)
        {
            M_printf ("%s " COLCONTACT "%10lu" COLNONE " ", s_now, conn->uin);
            M_printf (i18n (2211, "Your status is %s.\n"), s_status (conn->status));
        }
        if (data & 16)
            return 0;
    }
    for (k = -1; (k == -1) ? (tcg ? (cg = tcg) : cg) : (cg = ContactGroupIndex (k)); k++)
    {
        if (k != -1 && (cg == conn->contacts || cg == tcg))
            continue;
#ifdef CONFIG_UNDERLINE
        l = 0;
#endif
        if (!uin)
        {
            M_print (COLMESSAGE);
            if (cg != tcg && cg != conn->contacts)
            {
                for (i = j = (totallen - c_strlen (cg->name) - 1) / 2; i >= 20; i -= 20)
                    M_print ("====================");
                M_printf ("%.*s", (int)i, "====================");
                M_printf (" " COLCONTACT "%s" COLMESSAGE " ", cg->name);
                for (i = totallen - j - c_strlen (cg->name) - 2; i >= 20; i -= 20)
                    M_print ("====================");
            }
            else
                for (i = totallen; i >= 20; i -= 20)
                    M_print ("====================");
            M_printf ("%.*s" COLNONE "\n", (int)i, "====================");
        }
        for (i = (data & 1 ? 2 : 0); i < 9; i++)
        {
            status = stati[i];
            for (j = 0; (cont = ContactIndex (cg, j)); j++)
            {
                char *stat, *ver = NULL, *ver2 = NULL;
                
                if (uin && cont->uin != uin)
                    continue;
                if (cont->flags & CONT_ALIAS)
                    continue;
                if (__status (cont) != status)
                    continue;

                cont = ContactFind (conn->contacts, 0, cont->uin, NULL, 0);
                if (!cont)
                    continue;

                peer = (conn && conn->assoc) ? ConnectionFind (TYPE_MSGDIRECT, cont->uin, conn->assoc) : NULL;
                
                stat = strdup (s_sprintf ("(%s)", s_status (cont->status)));
                if (cont->version)
                    ver  = strdup (s_sprintf ("[%s]", cont->version));
                if (prG->verbose && cont->dc)
                    ver2 = strdup (s_sprintf (" <%08x:%08x:%08x>", (unsigned int)cont->dc->id1,
                                               (unsigned int)cont->dc->id2, (unsigned int)cont->dc->id3));
                for (alias = cont; alias && (data & 2 || alias == cont); alias = alias->alias)
                {
                    const char *ul = "";
                    char tbuf[100];
                    
                    if (cont->seen_time != -1L && data & 2)
                        strftime (tbuf, sizeof (tbuf), " %Y-%m-%d %H:%M:%S", localtime (&cont->seen_time));
                    else if (data & 2)
                        strcpy (tbuf, "                    ");
                    else
                        tbuf[0] = '\0';
                    
#ifdef CONFIG_UNDERLINE
                    if (!(++l % 5))
                        ul = ESC "[4m";
#endif
                    
                    if (data & 2)
                        M_printf (COLSERVER "%s%c%c%c%1.1d%c" COLNONE "%s %*ld", ul,
                             alias->flags & CONT_ALIAS     ? '+' :
                             cont->flags  & CONT_TEMPORARY ? '#' : ' ',
                             cont->flags  & CONT_INTIMATE  ? '*' :
                              cont->flags & CONT_HIDEFROM  ? '-' : ' ',
                             cont->flags  & CONT_IGNORE    ? '^' : ' ',
                             cont->dc ? cont->dc->version : 0,
                             peer ? (
                              peer->connect & CONNECT_OK   ? '&' :
                              peer->connect & CONNECT_FAIL ? '|' :
                              peer->connect & CONNECT_MASK ? ':' : '.' ) :
                              cont->dc && cont->dc->version && cont->dc->port && ~cont->dc->port &&
                              cont->dc->ip_rem && ~cont->dc->ip_rem ? '^' : ' ',
                             ul, (int)lenuin, cont->uin);

                    M_printf (COLSERVER "%s%c" COLCONTACT "%s%-*s" COLNONE "%s " COLMESSAGE "%s%-*s" COLNONE "%s %-*s%s%s" COLNONE "\n",
                             ul, data & 2                  ? ' ' :
                             alias->flags & CONT_ALIAS     ? '+' :
                             cont->flags  & CONT_TEMPORARY ? '#' :
                             cont->flags  & CONT_INTIMATE  ? '*' :
                             cont->flags  & CONT_HIDEFROM  ? '-' :
                             cont->flags  & CONT_IGNORE    ? '^' :
                             !peer                         ? ' ' :
                             peer->connect & CONNECT_OK    ? '&' :
                             peer->connect & CONNECT_FAIL  ? '|' :
                             peer->connect & CONNECT_MASK  ? ':' : '.' ,
                             ul, lennick + s_delta (alias->nick), alias->nick,
                             ul, ul, lenstat + 2 + s_delta (stat), stat,
                             ul, lenid + 2 + s_delta (ver ? ver : ""), ver ? ver : "",
                             ver2 ? ver2 : "", tbuf);
                }
                free (stat);
                s_free (ver);
                s_free (ver2);
            }
        }
        if (~data & 32)
            break;
    }
    if (data & 32)
        ContactGroupD (tcg);
    if (uin)
    {
        char *t1, *t2;
        UBYTE id;
        if (!(cont = ContactFind (conn->contacts, 0, uin, NULL, 0)))
            return 0;

        if (cont->dc)
        {
            M_printf ("%-15s %s/%s:%ld\n", i18n (1441, "remote IP:"),
                      t1 = strdup (s_ip (cont->dc->ip_rem)),
                      t2 = strdup (s_ip (cont->dc->ip_loc)), cont->dc->port);
            M_printf ("%-15s %d\n", i18n (1453, "TCP version:"), cont->dc->version);
            M_printf ("%-15s %s (%d)\n", i18n (1454, "Connection:"),
                      cont->dc->type == 4 ? i18n (1493, "Peer-to-Peer") : i18n (1494, "Server Only"),
                      cont->dc->type);
            M_printf ("%-15s %08lx\n", i18n (2026, "TCP cookie:"), cont->dc->cookie);
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
                    M_print (i18n (2192, "Capabilities: "));
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
    M_print (COLMESSAGE);
    for (i = totallen; i >= 20; i -= 20)
        M_print ("====================");
    M_printf ("%.*s" COLNONE "\n", (int)i, "====================");
    return 0;
}

/*
 * Show meta information remembered from a contact.
 */
static JUMP_F(CmdUserStatusMeta)
{
    Contact *cont, *contr;
    char *cmd;
    ANYCONN;

    if (!data)
    {
        if (!s_parse (&args, &cmd)) data = 0;
        else if (!strcmp (cmd, "show")) data = 1;
        else if (!strcmp (cmd, "load")) data = 2;
        else if (!strcmp (cmd, "save")) data = 3;
        else if (!strcmp (cmd, "set"))  data = 4;
        else if (!strcmp (cmd, "get"))  data = 5;
        else if (!strcmp (cmd, "rget")) data = 6;
        
        if (!data)
        {
            M_printf ("FIXME: help for meta *\n");
            return 0;
        }
    }

    while (*args)
    {
        if (data == 6)
            contr = ContactUIN (conn, uiG.last_rcvd_uin);
        else if (data != 4 && conn && !s_parsenick (&args, &cont, &contr, conn) && *args)
        {
            M_printf (i18n (1700, "%s is not a valid user in your list.\n"), args);
            return 0;
        }
        
        if (!contr)
            contr = ContactUIN (conn, conn->uin);
        if (!contr)
            return 0;
        
        switch (data)
        {
            case 1:
                UtilUIDisplayMeta (contr);
                if (*args == ',')
                    args++;
                continue;
            case 2:
                if (ContactMetaLoad (contr))
                    UtilUIDisplayMeta (contr);
                else
                    M_printf (i18n (2247, "Couldn't load meta data for '%s' (%ld).\n"),
                              cont->nick, contr->uin);
                if (*args == ',')
                    args++;
                continue;
            case 3:
                if (ContactMetaSave (contr))
                    M_printf (i18n (2248, "Saved meta data for '%s' (%ld).\n"),
                              cont->nick, contr->uin);
                else
                    M_printf (i18n (2249, "Couldn't save meta data for '%s' (%ld).\n"),
                              cont->nick, contr->uin);
                if (*args == ',')
                    args++;
                continue;
            case 4:
                contr = ContactUIN (conn, conn->uin);
                if (!contr)
                    return 0;
                if (conn->ver > 6)
                {
                    SnacCliMetasetgeneral (conn, contr);
                    SnacCliMetasetmore (conn, contr);
                    SnacCliMetasetabout (conn, contr->meta_about);
                }
                else
                {
                    CmdPktCmdMetaGeneral (conn, contr);
                    CmdPktCmdMetaMore (conn, contr);
                    CmdPktCmdMetaAbout (conn, contr->meta_about);
                }
                return 0;
            case 5:
            case 6:
                IMCliInfo (conn, contr, 0);
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

    M_printf ("%s%s\n", W_SEPARATOR, i18n (1062, "Users ignored:"));
    cg = conn->contacts;
    for (i = 0; (cont = ContactIndex (cg, i)); i++)
    {
        if (!(cont->flags & CONT_ALIAS))
        {
            if (cont->flags & CONT_IGNORE)
            {
                if (cont->flags & CONT_INTIMATE)
                    M_print (COLSERVER "*" COLNONE);
                else if (cont->flags & CONT_HIDEFROM)
                    M_print (COLSERVER "~" COLNONE);
                else
                    M_print (" ");

                M_printf (COLCONTACT "%-20s\t" COLMESSAGE "(%s)" COLNONE "\n",
                         cont->nick, s_status (cont->status));
            }
        }
    }
    M_print (W_SEPARATOR);
    return 0;
}


/*
 * Displays the contact list in a wide format, similar to the ls command.
 */
static JUMP_F(CmdUserStatusWide)
{
    Contact **Online;           /* definitely won't need more; could    */
    Contact **Offline = NULL;   /* probably get away with less.    */
    int MaxLen = 0;             /* length of longest contact name */
    int i;
    int OnIdx = 0;              /* for inserting and tells us how many there are */
    int OffIdx = 0;             /* for inserting and tells us how many there are */
    int NumCols;                /* number of columns to display on screen        */
    int StatusLen;
    ContactGroup *cg;
    Contact *cont = NULL;
    char *stat;
    OPENCONN;

    cg = conn->contacts;

    if (data)
    {
        if ((Offline = (Contact **) malloc (MAX_CONTACTS * sizeof (Contact *))) == NULL)
        {
            M_print (i18n (2118, "Out of memory.\n"));
            return 0;
        }
    }

    if ((Online = (Contact **) malloc (MAX_CONTACTS * sizeof (Contact *))) == NULL)
    {
        M_print (i18n (2118, "Out of memory.\n"));
        return 0;
    }

    /* Filter the contact list into two lists -- online and offline. Also
       find the longest name in the list -- this is used to determine how
       many columns will fit on the screen. */
    for (i = 0; (cont = ContactIndex (cg, i)); i++)
    {
        if (!(cont->flags & CONT_ALIAS))
        {
            if (cont->status == STATUS_OFFLINE)
            {
                if (data)
                {
                    Offline[OffIdx++] = cont;
                    if (strlen (cont->nick) > MaxLen)
                        MaxLen = strlen (cont->nick);
                }
            }
            else
            {
                Online[OnIdx++] = cont;
                if (strlen (cont->nick) > MaxLen)
                    MaxLen = strlen (cont->nick);
            }
        }
    }                           /* end for */

    /* This is probably a very ugly way to determine the number of columns
       to use... it's probably specific to my own contact list.            */
    NumCols = Get_Max_Screen_Width () / (MaxLen + 4);
    if (NumCols < 1)
        NumCols = 1;            /* sanity check. :)  */

    if (data)
    {
        /* Fairly simple print routine. We check that we only print the right
           number of columns to the screen.                                    */
        M_print (COLMESSAGE);
        for (i = 0; 2 * i + strlen (i18n (1653, "Offline")) < Get_Max_Screen_Width (); i++)
        {
            M_print ("=");
        }
        M_printf (COLCLIENT "%s" COLMESSAGE, i18n (1653, "Offline"));
        for (i += strlen (i18n (1653, "Offline")); i < Get_Max_Screen_Width (); i++)
        {
            M_print ("=");
        }
        M_print (COLNONE "\n");
        for (i = 0; i < OffIdx; i++)
        {
            M_printf (COLCONTACT "  %-*s" COLNONE, MaxLen + 2, Offline[i]->nick);
            if ((i + 1) % NumCols == 0)
                M_print ("\n");
        }
        if (i % NumCols != 0)
            M_print ("\n");
    }

    /* The user status for Online users is indicated by a one-character
       prefix to the nickname. Unfortunately not all statuses (statusae? :)
       are unique at one character. A better way to encode the information
       is needed. Our own status is shown to the right in the 'Online' headline */
    M_print (COLMESSAGE);

    stat = strdup (s_status (conn->status));
    StatusLen = strlen (i18n (2211, "Your status is %s.\n")) + strlen (stat) + 8;
    for (i = 0; 2 * i + StatusLen + strlen (i18n (1654, "Online")) < Get_Max_Screen_Width (); i++)
    {
        M_print ("=");
    }
    M_printf (COLCLIENT "%s" COLMESSAGE, i18n (1654, "Online"));
    for (i += strlen (i18n (1654, "Online")); i + StatusLen < Get_Max_Screen_Width (); i++)
    {
        M_print ("=");
    }

   /* Print our status */
    M_printf (" " COLCONTACT "%10lu" COLNONE " ", conn->uin);
    M_printf (i18n (2211, "Your status is %s.\n"), stat);
    free (stat);
    
    for (i = 0; i < OnIdx; i++)
    {
        char ind;
        
        ind = s_status (Online[i]->status)[0];

        if ((Online[i]->status & 0xffff) == STATUS_ONLINE)
            ind = ' ';

        M_printf (COLNONE "%c " COLCONTACT "%-*s" COLNONE,
                 ind, MaxLen + 2, Online[i]->nick);
        if ((i + 1) % NumCols == 0)
            M_print ("\n");
    }
    if (i % NumCols != 0)
    {
        M_print ("\n");
    }
    M_print (COLMESSAGE);
    for (i = 0; i < Get_Max_Screen_Width (); i++)
    {
        M_print ("=");
    }
    M_print (COLNONE "\n");
    free (Online);
    if (data)
        free (Offline);
    return 0;
}

/*
 * Display offline and online users on your contact list.
 */
static JUMP_F(CmdUserStatusShort)
{
    ContactGroup *cg;
    Contact *cont;
    int i;
    OPENCONN;

    M_print  (W_SEPARATOR);
    M_printf ("%s " COLCONTACT "%10lu" COLNONE " ", s_now, conn->uin);
    M_printf (i18n (2211, "Your status is %s.\n"), s_status (conn->status));

    cg = conn->contacts;
    if (data)
    {
        M_printf ("%s%s\n", W_SEPARATOR, i18n (1072, "Users offline:"));
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
        {
            if (~cont->flags & CONT_ALIAS && ~cont->flags & CONT_IGNORE)
            {
                if (cont->status == STATUS_OFFLINE)
                {
                    if (cont->flags & CONT_INTIMATE)
                        M_print (COLSERVER "*" COLNONE);
                    else
                        M_print (" ");

                    M_printf (COLCONTACT "%-20s\t" COLMESSAGE "(%s)" COLNONE "\n",
                             cont->nick, s_status (cont->status));
                }
            }
        }
    }

    M_printf ("%s%s\n", W_SEPARATOR, i18n (1073, "Users online:"));
    for (i = 0; (cont = ContactIndex (cg, i)); i++)
    {
        if (~cont->flags & CONT_ALIAS && ~cont->flags & CONT_IGNORE)
        {
            if (cont->status != STATUS_OFFLINE)
            {
                if (cont->flags & CONT_INTIMATE)
                    M_print (COLSERVER "*" COLNONE);
                else
                    M_print (" ");

                M_printf (COLCONTACT "%-20s\t" COLMESSAGE "(%s)" COLNONE "\n",
                         cont->nick, s_status (cont->status));
                if (cont->version)
                   M_printf (" [%s]", cont->version);
                if (cont->status & STATUSF_BIRTH)
                   M_printf (" (%s)", i18n (2033, "born today"));
                M_print ("\n");
            }
        }
    }
    M_print (W_SEPARATOR);
    return 0;
}

/*
 * Toggles sound or changes sound command.
 */
static JUMP_F(CmdUserSound)
{
    if (strlen (args))
    {
        if (!strcasecmp (args, "on") || !strcasecmp (args, i18n (1085, "on")) || !strcasecmp (args, "beep"))
           prG->sound = SFLAG_BEEP;
        else if (strcasecmp (args, "off") && strcasecmp (args, i18n (1086, "off")))
           prG->sound = SFLAG_EVENT;
        else
           prG->sound = 0;
    }
    if (prG->sound == SFLAG_BEEP)
        M_printf (i18n (2252, "A beep is generated by %sbeep%sing.\n"), COLSERVER, COLNONE);
    else if (prG->sound == SFLAG_EVENT)
        M_printf (i18n (2253, "A beep is generated by running the %sevent%s script.\n"), COLSERVER, COLNONE);
    else
    {
        M_print (i18n (2254, "A beep is never generated."));
        M_print ("\n");
    }
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
    char *arg1 = NULL;
    UDWORD i = 0;

    if (s_parseint (&args, &i))
    {
        if (prG->away_time)
            uiG.away_time_prev = prG->away_time;
        prG->away_time = i;
    }
    else if (s_parse (&args, &arg1))
    {
        if      (!strcmp (arg1, i18n (1085, "on"))  || !strcmp (arg1, "on"))
        {
            prG->away_time = uiG.away_time_prev ? uiG.away_time_prev : default_away_time;
        }
        else if (!strcmp (arg1, i18n (1086, "off")) || !strcmp (arg1, "off"))
        {
            if (prG->away_time)
                uiG.away_time_prev = prG->away_time;
            prG->away_time = 0;
        }
    }
    M_printf (i18n (1766, "Changing status to away/not available after idling %s%ld%s seconds.\n"),
             COLMESSAGE, prG->away_time, COLNONE);
    return 0;
}

/*
 * Toggles simple options.
 */
static JUMP_F(CmdUserSet)
{
    int quiet = 0;
    char *arg1 = NULL;
    const char *str = "";
    
    while (!data)
    {
        if (!s_parse (&args, &arg1))               data = 0;
        else if (!strcasecmp (arg1, "color"))      { data = FLAG_COLOR;      str = i18n (2133, "Color is %s%s%s.\n"); }
        else if (!strcasecmp (arg1, "colour"))     { data = FLAG_COLOR;      str = i18n (2133, "Color is %s%s%s.\n"); }
        else if (!strcasecmp (arg1, "hermit"))     { data = FLAG_HERMIT;     str = i18n (2261, "Hermit is %s%s%s.\n"); }
        else if (!strcasecmp (arg1, "delbs"))      { data = FLAG_DELBS;      str = i18n (2262, "Interpreting a delete character as backspace is %s%s%s.\n"); }
        else if (!strcasecmp (arg1, "funny"))      { data = FLAG_FUNNY;      str = i18n (2134, "Funny messages are %s%s%s.\n"); }
        else if (!strcasecmp (arg1, "log"))        { data = FLAG_LOG;        str = i18n (2263, "Logging is %s%s%s.\n"); }
        else if (!strcasecmp (arg1, "logonoff"))   { data = FLAG_LOG_ONOFF;  str = i18n (2264, "Logging of status changes is %s%s%s.\n"); }
        else if (!strcasecmp (arg1, "auto"))       { data = FLAG_AUTOREPLY;  str = i18n (2265, "Automatic replies are %s%s%s.\n"); }
        else if (!strcasecmp (arg1, "uinprompt"))  { data = FLAG_UINPROMPT;  str = i18n (2266, "Having the last nick in the prompt is %s%s%s.\n"); }
        else if (!strcasecmp (arg1, "autosave"))   { data = FLAG_AUTOSAVE;   str = i18n (2267, "Automatic saves are %s%s%s.\n"); }
        else if (!strcasecmp (arg1, "autofinger")) { data = FLAG_AUTOFINGER; str = i18n (2268, "Automatic fingering of new UINs is %s%s%s.\n"); }
        else if (!strcasecmp (arg1, "linebreak"))  data = -1;
        else if (!strcasecmp (arg1, "tabs"))       data = -2;
        else if (!strcasecmp (arg1, "silent"))     data = -3;
        else if (!strcasecmp (arg1, "quiet"))
        {
            quiet = 1;
            continue;
        }
        break;
    }
    
    if (data && !s_parse (&args, &arg1))
        arg1 = "";
    
    switch (data)
    {
        case 0:
            break;
        default:
            if (!strcasecmp (arg1, "on") || !strcmp (arg1, i18n (1085, "on")))
                prG->flags |= data;
            else if (!strcasecmp (arg1, "off") || !strcmp (arg1, i18n (1086, "off")))
                prG->flags &= ~data;
            else if (*arg1)
                data = 0;
            if (!quiet && str)
                M_printf (str, COLMESSAGE, prG->flags & data ? i18n (1085, "on") : i18n (1086, "off"), COLNONE);
            break;
        case -1:
            prG->flags &= ~FLAG_LIBR_BR & ~FLAG_LIBR_INT;
            if (!strcasecmp (arg1, "break") || !strcasecmp (arg1, i18n (2269, "break")))
                prG->flags |= FLAG_LIBR_BR;
            else if (!strcasecmp (arg1, "simple") || !strcasecmp (arg1, i18n (2270, "simple")))
                ;
            else if (!strcasecmp (arg1, "indent") || !strcasecmp (arg1, i18n (2271, "indent")))
                prG->flags |= FLAG_LIBR_INT;
            else if (!strcasecmp (arg1, "smart") || !strcasecmp (arg1, i18n (2272, "smart")))
                prG->flags |= FLAG_LIBR_BR | FLAG_LIBR_INT;
            else if (*arg1)
                data = 0;
            if (!quiet)
                M_printf (i18n (2288, "Indentation style is %s%s%s.\n"), COLMESSAGE,
                          ~prG->flags & FLAG_LIBR_BR ? (~prG->flags & FLAG_LIBR_INT ? i18n (2270, "simple") : i18n (2271, "indent")) :
                          ~prG->flags & FLAG_LIBR_INT ? i18n (2269, "break") : i18n (2272, "smart"), COLNONE);
            break;
        case -2:
            if (!strcasecmp (arg1, "cycle") || !strcasecmp (arg1, i18n (2273, "cycle")))
                prG->tabs = TABS_CYCLE;
            else if (!strcasecmp (arg1, "cycleall") || !strcasecmp (arg1, i18n (2274, "cycleall")))
                prG->tabs = TABS_CYCLEALL;
            else if (!strcasecmp (arg1, "simple") || !strcasecmp (arg1, i18n (2270, "simple")))
                prG->tabs = TABS_SIMPLE;
            else if (*arg1)
                data = 0;
            if (!quiet)
                M_printf (i18n (2275, "Tab style is %s%s%s.\n"), COLMESSAGE,
                          prG->tabs == TABS_CYCLE ? i18n (2273, "cycle") :
                          prG->tabs == TABS_CYCLEALL ? i18n (2274, "cycleall") : i18n (2270, "simple"), COLNONE);
            break;
        case -3:
            prG->flags &= ~FLAG_QUIET & ~FLAG_ULTRAQUIET;
            if (!strcasecmp (arg1, "on") || !strcasecmp (arg1, i18n (1085, "on")))
                prG->flags |= FLAG_QUIET;
            else if (!strcasecmp (arg1, "complete") || !strcasecmp (arg1, i18n (2276, "complete")))
                prG->flags |= FLAG_QUIET | FLAG_ULTRAQUIET;
            else if ((strcasecmp (arg1, "off") || !strcasecmp (arg1, i18n (1086, "off"))) && *arg1)
                data = 0;
            if (!quiet)
                M_printf (i18n (2135, "Quiet output is %s%s%s.\n"), COLMESSAGE,
                          prG->flags & FLAG_ULTRAQUIET ? i18n (2276, "complete") :
                          prG->flags & FLAG_QUIET ? i18n (1085, "on") : i18n (1086, "off"), COLNONE);
            break;
    }
    if (!data)
    {
        M_printf (i18n (1820, "%s <option> [on|off|<value>] - control simple options.\n"), CmdUserLookupName ("set"));
        M_print (i18n (1822, "    color:      use colored text output.\n"));
        M_print (i18n (2277, "    hermit:     ignore all non-contacts.\n"));
        M_print (i18n (2278, "    delbs:      interpret delete characters as backspace.\n"));
        M_print (i18n (1815, "    funny:      use funny messages for output.\n"));
        M_print (i18n (2279, "    log:        do logging.\n"));
        M_print (i18n (2280, "    logonoff:   also log status changes.\n"));
        M_print (i18n (2281, "    auto:       send auto-replies.\n"));
        M_print (i18n (2282, "    uinprompt:  have the last nick in the prompt.\n"));
        M_print (i18n (2283, "    autosave:   automatically save the micqrc.\n"));
        M_print (i18n (2284, "    autofinger: automatically finger new UINs.\n"));
        M_print (i18n (2285, "    linebreak:  style for line-breaking messages: simple, break, indent, smart.\n"));
        M_print (i18n (2286, "    tabs:       style for tab-handling: simple, cycle, cycleall.\n"));
        M_print (i18n (2287, "    silent:     suppress some output: off, on, complete.\n"));
    }
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
    Contact *cont = NULL, *contr = NULL;
    OPENCONN;

    while (*args)
    {
        if (!s_parsenick_s (&args, &cont, MULTI_SEP, &contr, conn))
        {
            M_printf (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
            return 0;
        }

        if (contr->flags & CONT_IGNORE)
        {
            contr->flags &= ~CONT_IGNORE;
            M_printf (i18n (1666, "Unignored %s.\n"), cont->nick);
        }
        else
        {
            contr->flags |= CONT_IGNORE;
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
    Contact *cont = NULL, *contr = NULL;
    OPENCONN;

    while (*args)
    {
        if (!s_parsenick_s (&args, &cont, MULTI_SEP, &contr, conn))
        {
            M_printf (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
            return 0;
        }

        if (contr->flags & CONT_HIDEFROM)
        {
            contr->flags &= ~CONT_HIDEFROM;
            if (conn->ver > 6)
                SnacCliReminvis (conn, cont);
            else
                CmdPktCmdUpdateList (conn, cont, INV_LIST_UPDATE, FALSE);
            M_printf (i18n (2020, "Being visible to %s.\n"), cont->nick);
        }
        else
        {
            contr->flags |= CONT_HIDEFROM;
            contr->flags &= ~CONT_INTIMATE;
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
    Contact *cont = NULL, *contr = NULL;
    OPENCONN;

    while (*args)
    {
        if (!s_parsenick (&args, &cont, &contr, conn))
        {
            M_printf (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
            return 0;
        }

        if (contr->flags & CONT_INTIMATE)
        {
            contr->flags &= ~CONT_INTIMATE;
            if (conn->ver > 6)
                SnacCliRemvisible (conn, cont);
            else
                CmdPktCmdUpdateList (conn, cont, VIS_LIST_UPDATE, FALSE);
            M_printf (i18n (1670, "Normal visible to %s now.\n"), cont->nick);
        }
        else
        {
            contr->flags |= CONT_INTIMATE;
            contr->flags &= ~CONT_HIDEFROM;
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
    char *arg1, *argst;
    OPENCONN;

    if (data != 1)
    {
        argst = args;
        if (s_parse (&argst, &arg1))
        {
            if ((cg = ContactGroupFind (0, conn, arg1, 0)))
                args = argst;
            else if (data == 2)
            {
                if ((cg = ContactGroupFind (0, conn, arg1, 1)))
                {
                    M_printf (i18n (2245, "Added contact group '%s'.\n"), arg1);
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
            while (s_parsenick_s (&args, &cont2, MULTI_SEP, &cont, conn))
            {
                if (cont->flags & CONT_TEMPORARY)
                    M_printf (i18n (1061, "'%s' not recognized as a nick name.\n"), cont->nick);
                else if (!ContactFind (cg, 0, cont->uin, 0, 0))
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
            if (s_parse (&args, &arg1) && strlen (arg1))
                M_printf (i18n (1061, "'%s' not recognized as a nick name.\n"), arg1);
        }
        return 0;
    }

    if (!s_parsenick (&args, &cont, NULL, conn))
    {
        M_printf (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
        return 0;
    }

    if (!s_parserem (&args, &arg1))
    {
        M_print (i18n (2116, "No new nick name given.\n"));
        return 0;
    }

    if (arg1)
    {
        if (cont->flags & CONT_TEMPORARY)
        {
            M_printf (i18n (2117, "%ld added as %s.\n"), cont->uin, arg1);
            M_print (i18n (1754, "Note: You need to 'save' to write new contact list to disc.\n"));
            if (c_strlen (arg1) > uiG.nick_len)
                uiG.nick_len = c_strlen (arg1);
            ContactFind (conn->contacts, 0, cont->uin, cont->nick, 1);
            if (conn->ver > 6)
                SnacCliAddcontact (conn, cont);
            else
                CmdPktCmdContactList (conn);
            s_repl (&cont->nick, arg1);
            cont->flags &= ~CONT_TEMPORARY;
        }
        else
        {
            if ((cont2 = ContactFind (conn->contacts, 0, cont->uin, arg1, 0)))
                M_printf (i18n (2146, "'%s' is already an alias for '%s' (%ld).\n"),
                         cont2->nick, cont->nick, cont->uin);
            else if ((cont2 = ContactFind (conn->contacts, 0, 0, arg1, 0)))
                M_printf (i18n (2147, "'%s' (%ld) is already used as a nick.\n"),
                         cont2->nick, cont2->uin);
            else
            {
                if (!(cont2 = ContactFind (conn->contacts, 0, cont->uin, arg1, 1)))
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
    char *alias, *argst;
    UBYTE all = 0;
    OPENCONN;
    
    if (data != 1)
    {
        argst = args;
        if (s_parse (&argst, &alias))
            if ((cg = ContactGroupFind (0, conn, alias, 0)))
                args = argst;
        if (data == 2 && !cg)
        {
            M_print (i18n (2240, "No contact group given.\n"));
            return 0;
        }
    }

    argst = args;
    if (s_parse (&argst, &alias))
    {
        if (!strcmp (alias, "all"))
        {
            args = argst;
            all = 2;
        }
    }
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
        if (!s_parsenick_s (&args, &cont, MULTI_SEP, NULL, conn))
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
            if (cont->flags & CONT_TEMPORARY)
            {
                M_printf (i18n (2221, "Removed temporary contact '%s' (%ld).\n"),
                          cont->nick, cont->uin);
                ContactRem (conn->contacts, cont);
                continue;
            }

            if (all || !cont->alias)
            {
                if (conn->ver > 6)
                    SnacCliRemcontact (conn, cont);
                else
                    CmdPktCmdContactList (conn);
            }

            alias = strdup (cont->nick);
            uin = cont->uin;
            
            ContactRemAlias (conn->contacts, cont);
            if (all)
            {
                while ((cont = ContactFind (conn->contacts, 0, uin, NULL, 0)))
                    ContactRemAlias (conn->contacts, cont);
            }
            
            if ((cont = ContactFind (conn->contacts, 0, uin, NULL, 0)))
                M_printf (i18n (2149, "Removed alias '%s' for '%s' (%ld).\n"),
                         alias, cont->nick, uin);
            else
                M_printf (i18n (2150, "Removed contact '%s' (%ld).\n"),
                          alias, uin);
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
    char *cmd = NULL, *argsb, *msg = NULL;
    Contact *cont = NULL;
    OPENCONN;

    argsb = args;
    if (!s_parse (&args, &cmd))
    {
        M_print (i18n (2119, "auth [grant] <nick>    - grant authorization.\n"));
        M_print (i18n (2120, "auth deny <nick> <msg> - refuse authorization.\n"));
        M_print (i18n (2121, "auth req  <nick> <msg> - request authorization.\n"));
        M_print (i18n (2145, "auth add  <nick>       - authorized add.\n"));
        return 0;
    }
    cmd = strdup (cmd);

    if (s_parsenick (&args, &cont, NULL, conn))
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
    if ((strcmp (cmd, "grant") && !s_parsenick (&argsb, &cont, NULL, conn)) || !cont)
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
    Contact *cont = NULL;
    OPENCONN;

    if (!s_parsenick (&args, &cont, NULL, conn))
    {
        M_printf (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
        return 0;
    }

    if (!s_parse (&args, &url))
    {
        M_print (i18n (1678, "Need URL please.\n"));
        return 0;
    }

    url = strdup (url);

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

    for (i = 0, TabReset (); TabHasNext (); i++)
        TabGetNext ();
    M_printf (i18n (1681, "Last %d people you talked to:\n"), i);
    for (TabReset (); TabHasNext ();)
    {
        UDWORD uin = TabGetNext ();
        Contact *cont;
        
        cont = ContactFind (NULL, 0, uin, NULL, 1);
        if (!cont)
            continue;
        M_printf ("    %s", cont->nick);
        if (cont)
            M_printf (COLMESSAGE " (%s)" COLNONE, s_status (cont->status));
        M_print ("\n");
    }
    return 0;
}

/*
 * Displays the last message received from the given nickname.
 */
static JUMP_F(CmdUserLast)
{
/*    ContactGroup *cg; */
    Contact *cont = NULL, *contr = NULL;
/*    int i; */
    ANYCONN;

    if (!s_parsenick (&args, &cont, &contr, conn))
    {
        HistShow (conn, NULL);

/*        cg = conn->contacts;
        M_print (i18n (1682, "You have received messages from:\n"));
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
            if (cont->last_message)
                M_printf ("  " COLCONTACT "%s" COLNONE " %s " COLMESSAGE "%s" COLNONE "\n",
                         cont->nick, s_time (&cont->last_time), cont->last_message);*/
        return 0;
    }

    do
    {
/*        if (contr->last_message)
        {
            M_printf (i18n (2106, "Last message from %s%s%s at %s:\n"),
                     COLCONTACT, cont->nick, COLNONE, s_time (&contr->last_time));
            M_printf (COLMESSAGE "%s" COLNONE "\n", cont->last_message);
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
    while (s_parsenick (&args, &cont, &contr, conn));
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
        M_printf (COLMESSAGE "%02d" COLNONE " %s, ", Days, i18n (1688, "days"));
    if (Hours != 0)
        M_printf (COLMESSAGE "%02d" COLNONE " %s, ", Hours, i18n (1689, "hours"));
    if (Minutes != 0)
        M_printf (COLMESSAGE "%02d" COLNONE " %s, ", Minutes, i18n (1690, "minutes"));
    M_printf (COLMESSAGE "%02d" COLNONE " %s.\n", Seconds, i18n (1691, "seconds"));
/*    M_printf ("%s " COLMESSAGE "%d" COLNONE " / %d\n", i18n (1692, "Contacts:"), uiG.Num_Contacts, MAX_CONTACTS); */

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
    char *arg1 = NULL;
    int i;
    Connection *conn;

    if (!s_parse (&args, &arg1))
    {
        M_print (i18n (1887, "Connections:"));
        M_print ("\n  " COLINDENT);
        
        for (i = 0; (conn = ConnectionNr (i)); i++)
        {
            Contact *cont;
            
            cont = ContactUIN (conn, conn->uin);

            M_printf (i18n (2093, "%02d %-12s version %d for %s (%lx), at %s:%ld %s\n"),
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
    else if (!strcmp (arg1, "login") || !strcmp (arg1, "open"))
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
    else if (!strcmp (arg1, "select"))
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
        if (~conn->connect & CONNECT_OK)
        {
            M_printf (i18n (2096, "Connection %ld is not open.\n"), i);
            return 0;
        }
        currconn = conn;
        M_printf (i18n (2099, "Selected connection %ld (version %d, UIN %ld) as current connection.\n"),
                 i, conn->ver, conn->uin);
    }
    else if (!strcmp (arg1, "remove") || !strcmp (arg1, "delete"))
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
    else if (!strcmp (arg1, "close") || !strcmp (arg1, "logoff"))
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
    char *tmp = NULL;
    OPENCONN;

    if (conn->type != TYPE_SERVER)
        return 0;

    if (!data)
    {
        if (!s_parse (&args, &tmp))           data = 0;
        else if (!strcasecmp (tmp, "show"))   data = 1;
        else if (!strcasecmp (tmp, "diff"))   data = 2;
        else if (!strcasecmp (tmp, "add"))    data = 3;
        else if (!strcasecmp (tmp, "import")) data = 3;
        else                                  data = 0;
    }
    if (data)
    {
        QueueEnqueueData (conn, QUEUE_REQUEST_ROSTER, 0, 0x7fffffffL, NULL, data, NULL, NULL);
        SnacCliReqroster (conn);
    }
    else
    {
        M_print (i18n (2103, "contact show    Show server side contact list.\n"));
        M_print (i18n (2104, "contact diff    Show server side contacts not on local contact list.\n"));
        M_print (i18n (2105, "contact import  Add server side contact list to local contact list.\n"));
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
    char *arg1 = NULL, *arg2 = NULL;
    OPENCONN;

    if (!strcmp (args, "."))
        status += 400;

    switch (status)
    {
        case 0:
            if (!s_parse (&args, &arg1))
            {
                M_print (i18n (1960, "Enter data to search user for. Enter '.' to start the search.\n"));
                R_setpromptf ("%s ", i18n (1656, "Enter the user's nick name:"));
                return 200;
            }
            arg1 = strdup (arg1);
            if (s_parse (&args, &arg2))
            {
                if (conn->ver > 6)
                    SnacCliSearchbypersinf (conn, NULL, NULL, arg1, arg2);
                else
                    CmdPktCmdSearchUser (conn, NULL, NULL, arg1, arg2);
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
        if (args && strlen (args))
        {
            if (conn->ver > 6)
                SnacCliMetasetabout (conn, args);
            else
                CmdPktCmdMetaAbout (conn, args);
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
    char *cmd = NULL, *args;

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
            if (!s_parse (&args, &cmd))
            {
                if (!command)
                    R_resetprompt ();
                return;
            }

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
                if (!s_parserem (&args, &argsd))
                    argsd = "";
                argsd = strdup (argsd);

                if (*cmd != '\xb6')
                {
                    if (CmdUserProcessAlias (cmd, argsd, &idle_save, idle_flag))
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
                        M_printf (i18n (1036, "Unknown command %s, type /help for help."), cmd);
                        M_print ("\n");
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
