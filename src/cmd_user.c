/*
 * Handles commands the user sends.
 *
 * Copyright: This file may be distributed under version 2 of the GPL licence.
 *
 * $Id$
 */

#include "micq.h"
#include "cmd_user.h"
#include "util.h"
#include "util_ui.h"
#include "util_io.h"
#include "util_table.h"
#include "cmd_pkt_cmd_v5.h"
#include "cmd_pkt_cmd_v5_util.h"
#include "cmd_pkt_v8.h"
#include "cmd_pkt_v8_snac.h"
#include "preferences.h"
#include "tabs.h"
#include "session.h"
#include "tcp.h"
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
    CmdUserAuto, CmdUserAlter, CmdUserMessage, CmdUserResend, CmdUserPeek,
    CmdUserVerbose, CmdUserRandomSet, CmdUserIgnoreStatus, CmdUserSMS,
    CmdUserStatusDetail, CmdUserStatusWide, CmdUserStatusShort,
    CmdUserSound, CmdUserSoundOnline, CmdUserRegister,
    CmdUserSoundOffline, CmdUserAutoaway, CmdUserSet, CmdUserClear,
    CmdUserTogIgnore, CmdUserTogInvis, CmdUserTogVisible, CmdUserAdd, CmdUserRem, CmdUserRInfo,
    CmdUserAuth, CmdUserURL, CmdUserSave, CmdUserTabs, CmdUserLast,
    CmdUserUptime, CmdUserOldSearch, CmdUserSearch, CmdUserUpdate, CmdUserPass,
    CmdUserOther, CmdUserAbout, CmdUserQuit, CmdUserTCP, CmdUserConn,
    CmdUserContact;

static void CmdUserProcess (const char *command, time_t *idle_val, UBYTE *idle_flag);

/* 1 = do not apply idle stuff next time           v
   2 = count this line as being idle               v */
static jump_t jump[] = {
    { &CmdUserRandom,        "rand",         NULL, 0,   0 },
    { &CmdUserRandomSet,     "setr",         NULL, 0,   0 },
    { &CmdUserHelp,          "help",         NULL, 0,   0 },
    { &CmdUserInfo,          "info",         NULL, 0,   0 },
    { &CmdUserTrans,         "lang",         NULL, 0,   0 },
    { &CmdUserTrans,         "trans",        NULL, 0,   0 },
    { &CmdUserAuto,          "auto",         NULL, 0,   0 },
    { &CmdUserAlter,         "alter",        NULL, 0,   0 },
    { &CmdUserMessage,       "msg",          NULL, 0,   1 },
    { &CmdUserMessage,       "r",            NULL, 0,   2 },
    { &CmdUserMessage,       "a",            NULL, 0,   4 },
    { &CmdUserMessage,       "msga",         NULL, 0,   8 },
    { &CmdUserResend,        "resend",       NULL, 0,   0 },
    { &CmdUserVerbose,       "verbose",      NULL, 0,   0 },
    { &CmdUserIgnoreStatus,  "i",            NULL, 0,   0 },
    { &CmdUserStatusDetail,  "status",       NULL, 2,  10 },
    { &CmdUserStatusDetail,  "ww",           NULL, 2,   2 },
    { &CmdUserStatusDetail,  "ee",           NULL, 2,   3 },
    { &CmdUserStatusDetail,  "w",            NULL, 2,   4 },
    { &CmdUserStatusDetail,  "e",            NULL, 2,   5 },
    { &CmdUserStatusDetail,  "s",            NULL, 2,  30 },
    { &CmdUserStatusDetail,  "s-any",        NULL, 2,   0 },
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
    { &CmdUserRem,           "rem",          NULL, 0,   0 },
    { &CmdUserRegister,      "reg",          NULL, 0,   0 },
    { &CmdUserRInfo,         "rinfo",        NULL, 0,   0 },
    { &CmdUserAuth,          "auth",         NULL, 0,   0 },
    { &CmdUserURL,           "url",          NULL, 0,   0 },
    { &CmdUserSave,          "save",         NULL, 0,   0 },
    { &CmdUserTabs,          "tabs",         NULL, 0,   0 },
    { &CmdUserLast,          "last",         NULL, 0,   0 },
    { &CmdUserUptime,        "uptime",       NULL, 0,   0 },
    { &CmdUserTCP,           "peer",         NULL, 0,   0 },
    { &CmdUserTCP,           "tcp",          NULL, 0,   0 },
    { &CmdUserQuit,          "q",            NULL, 0,   0 },
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

static Session *currsess = NULL;

#define SESSION Session *sess = currsess; if (!sess && !(currsess = sess = SessionFind (TYPEF_ANY_SERVER, 0, NULL))) \
    { M_print (i18n (1931, "Couldn't find server connection.\n")); return 0; }

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
 * Change status.
 */
static JUMP_F(CmdUserChange)
{
    char *arg1 = NULL;
    SESSION;

    if (data == -1)
    {
        if (!UtilUIParseInt (&args, &data))
        {
            M_print (i18n (1703, "Status modes:\n"));
            M_print ("  %-20s %d\n", i18n (1921, "Online"),         STATUS_ONLINE);
            M_print ("  %-20s %d\n", i18n (1923, "Away"),           STATUS_AWAY);
            M_print ("  %-20s %d\n", i18n (1922, "Do not disturb"), STATUS_DND);
            M_print ("  %-20s %d\n", i18n (1924, "Not Available"),  STATUS_NA);
            M_print ("  %-20s %d\n", i18n (1927, "Free for chat"),  STATUS_FFC);
            M_print ("  %-20s %d\n", i18n (1925, "Occupied"),       STATUS_OCC);
            M_print ("  %-20s %d\n", i18n (1926, "Invisible"),      STATUS_INV);
            return 0;
        }
    }
    if (UtilUIParseRemainder (&args, &arg1))
    {
        if      (data & STATUSF_DND) { if (prG->auto_dnd)  free (prG->auto_dnd);  prG->auto_dnd  = strdup (arg1); }
        else if (data & STATUSF_OCC) { if (prG->auto_occ)  free (prG->auto_occ);  prG->auto_occ  = strdup (arg1); }
        else if (data & STATUSF_NA)  { if (prG->auto_na)   free (prG->auto_na);   prG->auto_na   = strdup (arg1); }
        else if (data & STATUSF_AWAY){ if (prG->auto_away) free (prG->auto_away); prG->auto_away = strdup (arg1); }
        else if (data & STATUSF_FFC) { if (prG->auto_ffc)  free (prG->auto_ffc);  prG->auto_ffc  = strdup (arg1); }
    }

    if (sess->ver > 6)
        SnacCliSetstatus (sess, data, 1);
    else
    {
        CmdPktCmdStatusChange (sess, data);
        Time_Stamp ();
        M_print (" ");
        Print_Status (sess->status);
        M_print ("\n");
    }
    return 0;
}

/*
 * Finds random user.
 */
static JUMP_F(CmdUserRandom)
{
    UDWORD arg1 = 0;
    SESSION;
    
    if (!UtilUIParseInt (&args, &arg1))
    {
        M_print (i18n (1704, "Groups:\n"));
        M_print ("  %2d %s\n",  1, i18n (1705, "General"));
        M_print ("  %2d %s\n",  2, i18n (1706, "Romance"));
        M_print ("  %2d %s\n",  3, i18n (1707, "Games"));
        M_print ("  %2d %s\n",  4, i18n (1708, "Students"));
        M_print ("  %2d %s\n",  6, i18n (1709, "20 something"));
        M_print ("  %2d %s\n",  7, i18n (1710, "30 something"));
        M_print ("  %2d %s\n",  8, i18n (1711, "40 something"));
        M_print ("  %2d %s\n",  9, i18n (1712, "50+"));
        M_print ("  %2d %s\n", 10, i18n (1713, "Man chat requesting women"));
        M_print ("  %2d %s\n", 11, i18n (1714, "Woman chat requesting men"));
        M_print ("  %2d %s\n", 49, i18n (1715, "mICQ"));
    }
    else
    {
        if (sess->ver > 6)
            SnacCliSearchrandom (sess, arg1);
        else
            CmdPktCmdRandSearch (sess, arg1);
    }
    return 0;
}

/*
 * Sets the random user group.
 */
static JUMP_F(CmdUserRandomSet)
{
    UDWORD arg1 = 0;
    SESSION;
    
    if (!UtilUIParseInt (&args, &arg1))
    {
        M_print (i18n (1704, "Groups:\n"));
        M_print ("  %2d %s\n", sess->ver > 6 ? 0 : -1, i18n (1716, "None"));
        M_print ("  %2d %s\n",  1, i18n (1705, "General"));
        M_print ("  %2d %s\n",  2, i18n (1706, "Romance"));
        M_print ("  %2d %s\n",  3, i18n (1707, "Games"));
        M_print ("  %2d %s\n",  4, i18n (1708, "Students"));
        M_print ("  %2d %s\n",  6, i18n (1709, "20 something"));
        M_print ("  %2d %s\n",  7, i18n (1710, "30 something"));
        M_print ("  %2d %s\n",  8, i18n (1711, "40 something"));
        M_print ("  %2d %s\n",  9, i18n (1712, "50+"));
        M_print ("  %2d %s\n", 10, i18n (1713, "Man chat requesting women"));
        M_print ("  %2d %s\n", 11, i18n (1714, "Woman chat requesting men"));
        M_print ("  %2d %s\n", 49, i18n (1715, "mICQ"));
    }
    else
    {
        if (sess->ver > 6)
            SnacCliSetrandom (sess, arg1);
        else
            CmdPktCmdRandSet (sess, arg1);
    }
    return 0;
}

/*
 * Displays help.
 */

/* FIXME: update help strings */

static JUMP_F(CmdUserHelp)
{
    char *arg1 = NULL;

    UtilUIParse (&args, &arg1);

    if (arg1 && !strcasecmp (arg1, i18n (1447, "Client")))
    {
        M_print (COLMESSAGE "%s [<nr>]" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("verbose"),
                 i18n (1418, "Set the verbosity level, or display verbosity level."));
        M_print (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("clear"),
                 i18n (1419, "Clears the screen."));
        M_print (COLMESSAGE "%s [%s|%s|<command>]" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("sound"), i18n (1085, "on"), i18n (1086, "off"),
                 i18n (1420, "Switches beeping when receiving new messages on or off, sets a command for this event or displays current state of this option."));
        M_print (COLMESSAGE "%s [%s|%s|<command>]" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("soundonline"), i18n (1085, "on"), i18n (1086, "off"),
                 i18n (2040, "Switches beeping for incoming contacts on or off, sets a command for this event or displays current state of this option."));
        M_print (COLMESSAGE "%s [%s|%s|<command>]" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("soundoffline"), i18n (1085, "on"), i18n (1086, "off"),
                 i18n (2041, "Switches beeping for offgoing contacts on or off, sets a command for this event or displays current state of this option."));
        M_print (COLMESSAGE "%s [<timeout>]" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("autoaway"),
                 i18n (1767, "Toggles auto cycling to away/not available."));
        M_print (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("q"),
                 i18n (1422, "Logs off and quits."));
        M_print (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("auto"),
                 i18n (1423, "Displays your autoreply status."));
        M_print (COLMESSAGE "%s [on|off]" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("auto"),
                 i18n (1424, "Toggles sending messages when your status is DND, NA, etc."));
        M_print (COLMESSAGE "%s <status> <message>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("auto"),
                 i18n (1425, "Sets the message to send as an auto reply for the status."));
        M_print (COLMESSAGE "%s <old cmd> <new cmd>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("alter"),
                 i18n (1417, "This command allows you to alter your command set on the fly."));
        M_print (COLMESSAGE "%s <lang>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("trans"), 
                 i18n (1800, "Change the working language to <lang>."));  
        M_print (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("uptime"), 
                 i18n (2035, "Show uptime and some statistics."));  
        M_print (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("save"),
                 i18n (2036, "Save current preferences to disc."));
        M_print (COLMESSAGE "%s open|close|off uin|nick" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("peer"),
                 i18n (2037, "Open or close a peer-to-peer connection, or disable using peer-to-peer connections for uin|nick."));
        M_print (COLMESSAGE "%s open|login <nr>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("conn"),
                 i18n (2038, "Opens connection number nr."));
        M_print (COLMESSAGE "%s color|funny|quiet <on|off>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("set"),
                 i18n (2044, "Set or clear an option."));
        M_print ("  " COLCLIENT "" COLINDENT "%s" COLEXDENT "" COLNONE "\n",
                 i18n (1717, "! as the first character of a command will execute a shell command (e.g. \"!ls\"  \"!dir\" \"!mkdir temp\")"));
    }
    else if (arg1 && !strcasecmp (arg1, i18n (1448, "Message")))
    {
        M_print (COLMESSAGE "%s <uin|nick>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("auth"),
                 i18n (1413, "Authorize uin or nick to add you to their list."));
        M_print (COLMESSAGE "%s <uin|nick> [<message>]" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("msg"),
                 i18n (1409, "Sends a message to uin or nick."));
        M_print (COLMESSAGE "%s <uin|nick> <url> <message>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("url"),
                 i18n (1410, "Sends a url and message to uin."));
        M_print (COLMESSAGE "%s <tel> <message>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("sms"),
                 i18n (2039, "Sends a message to a cell phone."));
        M_print (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("msga"),
                 i18n (1411, "Sends a multiline message to everyone on your list."));
        M_print (COLMESSAGE "%s <message>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("a"),
                 i18n (1412, "Sends a message to the last person you sent a message to."));
        M_print (COLMESSAGE "%s <message>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("r"),
                 i18n (1414, "Replys to the last person to send you a message."));
        M_print (COLMESSAGE "%s [<contact>]" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("last"),
                 i18n (1403, "Displays the last message received from <nick>, or a list of who has send you at least one message."));
        M_print (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("tabs"),
                 i18n (1098, "Display a list of nicknames that you can tab through.")); 
        M_print (COLMESSAGE "%s <uin>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("resend"),
                 i18n (1770, "Resend your last message to a new uin."));
        M_print (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("uptime"),
                 i18n (1719, "Shows how long mICQ has been running."));
        M_print ("  " COLCLIENT "" COLINDENT "%s" COLEXDENT "" COLNONE "\n",
                 i18n (1720, "uin can be either a number or the nickname of the user."));
        M_print ("  " COLCLIENT "" COLINDENT "%s" COLEXDENT "" COLNONE "\n",
                 i18n (1721, "Sending a blank message will put the client into multiline mode.\nUse . on a line by itself to end message.\nUse # on a line by itself to cancel the message."));
    }
    else if (arg1 && !strcasecmp (arg1, i18n (1449, "User")))
    {
        M_print (COLMESSAGE "%s [<nr>]" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("rand"),
                 i18n (1415, "Finds a random user in the specified group or lists the groups."));
        M_print (COLMESSAGE "%s <user>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("status"),
                 i18n (1400, "Shows locally stored info on user."));
        M_print (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("s"),
                 i18n (1769, "Displays your current online status."));
        M_print (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("w"),
                 i18n (1416, "Displays the current status of everyone on your contact list."));
        M_print (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("e"),
                 i18n (1407, "Displays the current status of online people on your contact list."));
        M_print (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("wide"),
                 i18n (1801, "Displays a list of people on your contact list in a screen wide format.")); 
        M_print (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("ewide"),
                 i18n (2042, "Displays a list of online people on your contact list in a screen wide format.")); 
        M_print (COLMESSAGE "%s <uin>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("info"),
                 i18n (1430, "Displays general info on uin."));
        M_print (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("i"),
                 i18n (1405, "Lists ignored nicks/uins."));
        M_print (COLMESSAGE "%s [<email>|<nick>|<first> <last>]" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("search"),
                 i18n (1429, "Searches for a ICQ user."));
        M_print (COLMESSAGE "%s <uin> [<nick>]" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("add"),
                 i18n (1428, "Adds the uin number to your contact list with nickname, or renames it."));
        M_print (COLMESSAGE "%s <uin|nick>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("rem"),
                 i18n (2043, "Remove contact from your contact list."));
        M_print (COLMESSAGE "%s <nick>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("togig"),
                 i18n (1404, "Toggles ignoring/unignoring nick."));
        M_print (COLMESSAGE "%s <nick>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("toginv"),
                 i18n (2045, "Toggles your visibility to a user when you're online."));
        M_print (COLMESSAGE "%s <nick>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("togvis"),
                 i18n (1406, "Toggles your visibility to a user when you're invisible."));
    }
    else if (arg1 && !strcasecmp (arg1, i18n (1450, "Account")))
    {
        M_print (COLMESSAGE "%s <status>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("change"),
                 i18n (1427, "Changes your status to the status number. Without a number it lists the available modes."));
        M_print (COLMESSAGE "%s <secret>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("pass"),
                 i18n (1408, "Changes your password to secret."));
        M_print (COLMESSAGE "%s <password>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("reg"),
                 i18n (1426, "Creates a new UIN with the specified password."));
        M_print (COLMESSAGE "%s|%s|%s|%s|%s|%s|%s" COLNONE "\n\t" COLINDENT "%s\n%s\n%s\n%s\n%s\n%s\n%s" COLEXDENT "\n", 
                 CmdUserLookupName ("online"),
                 CmdUserLookupName ("away"),
                 CmdUserLookupName ("na"),
                 CmdUserLookupName ("occ"),
                 CmdUserLookupName ("dnd"),
                 CmdUserLookupName ("ffc"),
                 CmdUserLookupName ("inv"),
                 i18n (1431, "Change status to Online."),
                 i18n (1432, "Mark as \"away\"."),
                 i18n (1433, "Mark as \"not available\"."),
                 i18n (1434, "Mark as \"occupied\"."),
                 i18n (1435, "Mark as \"do not disturb\"."),
                 i18n (1436, "Mark as \"free for chat\"."),
                 i18n (1437, "Mark as \"invisible\"."));
        M_print (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("update"),
                 i18n (1438, "Updates your basic info (email, nickname, etc.)."));
        M_print (COLMESSAGE "other" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 i18n (1401, "Updates more user info like age and sex."));
        M_print (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 CmdUserLookupName ("about"),
                 i18n (1402, "Updates your about user info."));
        M_print (COLMESSAGE "%s <nr>" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n", 
                 CmdUserLookupName ("setr"),
                 i18n (1439, "Sets your random user group."));
    }
    else
    {
        M_print (COLCLIENT "%s\n", i18n (1442, "Please select one of the help topics below."));
        M_print ("%s\t-\t%s\n", i18n (1447, "Client"),
                 i18n (1443, "Commands relating to mICQ displays and configuration."));
        M_print ("%s\t-\t%s\n", i18n (1448, "Message"),
                 i18n (1446, "Commands relating to sending messages."));
        M_print ("%s\t-\t%s\n", i18n (1449, "User"),
                 i18n (1444, "Commands relating to finding other users."));
        M_print ("%s\t-\t%s\n", i18n (1450, "Account"),
                 i18n (1445, "Commands relating to your ICQ account."));
    }
    return 0;
}

/*
 * Sets a new password.
 */
static JUMP_F(CmdUserPass)
{
    char *arg1 = NULL;
    SESSION;
    
    if (!UtilUIParseRemainder (&args, &arg1))
        M_print (i18n (2012, "No password given.\n"));
    else
    {
        if (sess->ver < 6)
            CmdPktCmdMetaPass (sess, arg1);
        else
            SnacCliMetasetpass (sess, arg1);
        sess->passwd = strdup (arg1);
        if (sess->spref->passwd && strlen (sess->spref->passwd))
        {
            M_print (i18n (2122, " Note: You need to 'save' to write new password to disc.\n"));
            sess->spref->passwd = strdup (arg1);
        }
    }
    return 0;
}

/*
 * Sends an SMS message.
 */
static JUMP_F(CmdUserSMS)
{
    char *arg1 = NULL, *arg2 = NULL;
    SESSION;
    
    if (sess->ver < 6)
    {
        M_print (i18n (2013, "This command is v8 only.\n"));
        return 0;
    }
    if (!UtilUIParse (&args, &arg1))
        M_print (i18n (2014, "No number given.\n"));
    else
    {
        arg1 = strdup (arg1);
        if (!UtilUIParseRemainder (&args, &arg2))
            M_print (i18n (2015, "No message given.\n"));
        else
            SnacCliSendsms (sess, arg1, arg2);
        free (arg1);
    }
    return 0;
}

/*
 * Queries basic info of a user
 */
static JUMP_F(CmdUserInfo)
{
    Contact *cont = NULL, *contr = NULL;
    SESSION;

    if (!UtilUIParseNick (&args, &cont, &contr, sess))
    {
        M_print (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
        return 0;
    }
    M_print (i18n (1672, "%s's IP address is "), cont->nick);
    Print_IP (cont->uin); /* FIXME: static buffer helper instead of printer */
    M_print ("\t");

    if (contr->port != (UWORD) 0xffff)
        M_print (i18n (1673, "The port is %d.\n"), contr->port);
    else
        M_print (i18n (1674, "The port is unknown.\n"));

    M_print (i18n (1765, "%s has UIN %d."), cont->nick, cont->uin);
    M_print ("\n");

    if (sess->ver > 6)
        SnacCliMetareqinfo (sess, cont->uin);
    else
        CmdPktCmdMetaReqInfo (sess, cont->uin);

    return 0;
}

/*
 * Peeks whether a user is really offline.
 */
static JUMP_F(CmdUserPeek)
{
    Contact *cont = NULL;
    SESSION;
    
    if (sess->ver < 6)
    {
        M_print (i18n (2013, "This command is v8 only.\n"));
        return 0;
    }

    if (!UtilUIParseNick (&args, &cont, NULL, sess))
        M_print (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
    else
        SnacCliSendmsg (sess, cont->uin, "", 0xe8);
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

        if (UtilUIParseInt (&args, &i))
        {
            M_print ("%3d:%s\n", i, i18n (i, i18n (1078, "No translation available.")));
            one = 1;
            continue;
        }
        if (UtilUIParse (&args, &arg1))
        {
            if (!strcmp (arg1, "all"))
            {
                for (i = l = 1000; i - l < 100; i++)
                {
                    p = i18n (i, NULL);
                    if (p)
                    {
                        l = i;
                        M_print ("%3d:%s\n", i, p);
                    }
                }
            }
            else
            {
                if (!strcmp (arg1, ".") || !strcmp (arg1, "none") || !strcmp (arg1, "unload"))
                {
                    t = strdup (i18n (1089, "Unloaded translation."));
                    i18nClose ();
                    M_print ("%s\n", t);
                    free (t);
                    continue;
                }
                i = i18nOpen (arg1);
                if (i == -1)
                    M_print (i18n (1080, "Couldn't load \"%s\" internationalization.\n"), arg1);
                else if (i)
                    M_print (i18n (1081, "Successfully loaded en translation (%d entries).\n"), i);
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

            arg1 = strdup (i18n (1003, "0"));
            for (t = arg1; *t; t++)
                if (*t == '-')
                    *t = ' ';
            UtilUIParseInt (&arg1, &v1);
            UtilUIParseInt (&arg1, &v2);
            UtilUIParseInt (&arg1, &v3);
            UtilUIParseInt (&arg1, &v4);
            
            /* i18n (1079, "Translation (%s, %s) from %s, last modified on %s by %s, for mICQ %d.%d.%d%s.\n") */
            M_print (i18n (-1, "1079:No translation; using compiled-in strings.�"),
                     i18n (1001, "<lang>"), i18n (1002, "<lang_cc>"), i18n (1004, "<translation authors>"),
                     i18n (1006, "<last edit date>"), i18n (1005, "<last editor>"),
                     v1, v2, v3, v4 ? t = (char *)UtilFill (".%d", v4) : "");
            if (t)
                free (t);
            return 0;
        }
    }
}

/*
 * Manually handles peer-to-peer (TCP) connections.
 */
static JUMP_F(CmdUserTCP)
{
#ifdef ENABLE_PEER2PEER
    char *arg1 = NULL;
    Contact *cont = NULL;
    Session *list;
    SESSION;

    while (1)
    {
        if (!(list = sess->assoc))
        {
            M_print (i18n (2011, "You do not have a listening peer-to-peer connection.\n"));
            return 0;
        }

        if (!UtilUIParse (&args, &arg1))
            break;
        
        arg1 = strdup (arg1);

        if (!UtilUIParseNick (&args, &cont, NULL, sess))
        {
            M_print (i18n (1845, "Nick %s unknown.\n"), args);
            free (arg1);
            return 0;
        }

        if (!strcmp (arg1, "open"))
        {
            if (!TCPDirectOpen  (list, cont->uin))
                M_print (i18n (2142, "Direct connection with %s not possible.\n"), cont->nick);
        }
        else if (!strcmp (arg1, "close"))
            TCPDirectClose (list, cont->uin);
        else if (!strcmp (arg1, "reset"))
            TCPDirectClose (list, cont->uin);
        else if (!strcmp (arg1, "off"))
            TCPDirectOff   (list, cont->uin);
        else if (!strcmp (arg1, "file"))
        {
            char *files[10], *ass[10], *des = NULL, *as;
            int count;
            
            for (count = 0; count < 10; count++)
            {
                if (!UtilUIParse (&args, &des))
                {
                    des = strdup ("no descriptione, sir.");
                    break;
                }
                files[count] = des = strdup (des);
                if (!UtilUIParse (&args, &as))
                    break;
                if (*as == '/' && !*(as + 1))
                    as = (strchr (des, '/')) ? strrchr (des, '/') + 1 : des;
                if (*as == '.' && !*(as + 1))
                    as = des;
                ass[count] = as = strdup (as);
            }
            TCPSendFiles (list, cont->uin, des, files, ass, count);
            while (count--)
            {
               free (ass[count]);
               free (files[count]);
            }
            free (des);
        }
#ifdef WIP
        else if (!strcmp (arg1, "ver"))
            TCPGetAuto     (list, cont->uin, TCP_MSG_GET_VER);
#endif
        else if (!strcmp (arg1, "auto"))
            TCPGetAuto     (list, cont->uin, 0);
        else if (!strcmp (arg1, "away"))
            TCPGetAuto     (list, cont->uin, TCP_MSG_GET_AWAY);
        else if (!strcmp (arg1, "na"))
            TCPGetAuto     (list, cont->uin, TCP_MSG_GET_NA);
        else if (!strcmp (arg1, "dnd"))
            TCPGetAuto     (list, cont->uin, TCP_MSG_GET_DND);
        else if (!strcmp (arg1, "ffc"))
            TCPGetAuto     (list, cont->uin, TCP_MSG_GET_FFC);
        else
            break;
        free (arg1);
        return 0;
    }
    M_print (i18n (1846, "Opens and closes TCP connections:\n"));
    M_print (i18n (1847, "    open  <nick> - Opens TCP connection.\n"));
    M_print (i18n (1848, "    close <nick> - Closes/resets TCP connection(s).\n"));
    M_print (i18n (1870, "    off   <nick> - Closes TCP connection(s) and don't try TCP again.\n"));
    M_print (i18n (2056, "    auto  <nick> - Get the auto-response from the contact.\n"));
    M_print (i18n (2057, "    away  <nick> - Get the auto-response for away from the contact.\n"));
    M_print (i18n (2058, "    na    <nick> - Get the auto-response for not available from the contact.\n"));
    M_print (i18n (2059, "    dnd   <nick> - Get the auto-response for do not disturb from the contact.\n"));
    M_print (i18n (2060, "    ffc   <nick> - Get the auto-response for free for chat from the contact.\n"));
    M_print (i18n (2110, "    file  <nick> <file1> <as1> ... <description>\n"));
    M_print (i18n (2111, "                 - Send file1 as as1, ..., with description.\n"));
    M_print (i18n (2112, "                 - as = '/': strip path, as = '.': as is\n"));
#else
    M_print (i18n (1866, "This version of mICQ is compiled without TCP support.\n"));
#endif
    return 0;
}

/*
 * Changes automatic reply messages.
 */
static JUMP_F(CmdUserAuto)
{
    char *arg1 = NULL, *arg2 = NULL;

    if (!UtilUIParse (&args, &arg1))
    {
        M_print (i18n (1724, "Automatic replies are %s.\n"),
                 prG->flags & FLAG_AUTOREPLY ? i18n (1085, "on") : i18n (1086, "off"));
        M_print ("%30s %s\n", i18n (1727, "The \"do not disturb\" message is:"), prG->auto_dnd);
        M_print ("%30s %s\n", i18n (1728, "The \"away\" message is:"),           prG->auto_away);
        M_print ("%30s %s\n", i18n (1729, "The \"not available\" message is:"),  prG->auto_na);
        M_print ("%30s %s\n", i18n (1730, "The \"occupied\" message is:"),       prG->auto_occ);
        M_print ("%30s %s\n", i18n (1731, "The \"invisible\" message is:"),      prG->auto_inv);
        M_print ("%30s %s\n", i18n (2054, "The \"free for chat\" message is:"),  prG->auto_ffc);
        return 0;
    }

    if      (!strcasecmp (arg1, "on")  || !strcasecmp (arg1, i18n (1085, "on")))
    {
        prG->flags |= FLAG_AUTOREPLY;
        M_print (i18n (1724, "Automatic replies are %s.\n"), i18n (1085, "on"));
        return 0;
    }
    else if (!strcasecmp (arg1, "off") || !strcasecmp (arg1, i18n (1086, "off")))
    {
        prG->flags &= ~FLAG_AUTOREPLY;
        M_print (i18n (1724, "Automatic replies are %s.\n"), i18n (1086, "off"));
        return 0;
    }
    
    arg1 = strdup (arg1);

    if (!UtilUIParseRemainder (&args, &arg2))
    {
        M_print (i18n (1735, "Must give a message.\n"));
        free (arg1);
        return 0;
    }

    if      (!strcasecmp (arg1, "dnd")  || !strcasecmp (arg1, CmdUserLookupName ("dnd")))
    {
        if (prG->auto_dnd)  free (prG->auto_dnd);  prG->auto_dnd  = strdup (arg2);
    }
    else if (!strcasecmp (arg1, "away") || !strcasecmp (arg1, CmdUserLookupName ("away")))
    {
        if (prG->auto_away) free (prG->auto_away); prG->auto_away = strdup (arg2);
    }
    else if (!strcasecmp (arg1, "na")   || !strcasecmp (arg1, CmdUserLookupName ("na")))
    {
        if (prG->auto_na)   free (prG->auto_na);   prG->auto_na   = strdup (arg2);
    }
    else if (!strcasecmp (arg1, "occ")  || !strcasecmp (arg1, CmdUserLookupName ("occ")))
    {
        if (prG->auto_occ)  free (prG->auto_occ);  prG->auto_occ  = strdup (arg2);
    }
    else if (!strcasecmp (arg1, "inv")  || !strcasecmp (arg1, CmdUserLookupName ("inv")))
    {
        if (prG->auto_inv)  free (prG->auto_inv);  prG->auto_inv  = strdup (arg2);
    }
    else if (!strcasecmp (arg1, "ffc")  || !strcasecmp (arg1, CmdUserLookupName ("ffc")))
    {
        if (prG->auto_ffc)  free (prG->auto_ffc);  prG->auto_ffc  = strdup (arg2);
    }
    else
        M_print (i18n (2113, "Unknown status '%s'.\n"), arg1);
    free (arg1);
    return 0;
}

/*
 * Relabels commands.
 */

/* FIXME: write an alias system. */

static JUMP_F(CmdUserAlter)
{
    char *arg1 = NULL, *arg2 = NULL;
    jump_t *j;
    int quiet = 0;

    UtilUIParse (&args, &arg1);

    if (arg1 && !strcasecmp ("quiet", arg1))
    {
        quiet = 1;
        UtilUIParse (&args, &arg1);
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
        M_print (i18n (2114, "The command name '%s' does not exist.\n"), arg1);
        return 0;
    }
    
    if (UtilUIParse (&args, &arg2))
    {
        if (CmdUserLookup (arg2, CU_USER))
        {
            if (!quiet)
                M_print (i18n (1768, "The command name '%s' is already being used.\n"), arg2);
            return 0;
        }
        else
        {
            if (j->name)
                free ((char *) j->name);
            j->name = strdup (arg2);
        }
    }
    
    if (!quiet)
    {
        if (j->name)
            M_print (i18n (1763, "The command '%s' has been renamed to '%s'."), j->defname, j->name);
        else
            M_print (i18n (1764, "The command '%s' is unchanged."), j->defname);
        M_print ("\n");
    }
    return 0;
}

/*
 * Resend your last message
 */
static JUMP_F (CmdUserResend)
{
    Contact *cont = NULL;
    SESSION;

    if (!uiG.last_message_sent) 
    {
        M_print (i18n (1771, "You haven't sent a message to anyone yet!\n"));
        return 0;
    }

    if (!UtilUIParseNick (&args, &cont, NULL, sess))
    {
        if (*args)
            M_print (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
        else
            M_print (i18n (1676, "Need uin/nick to send to.\n"));
        return 0;
    }
    
    while (1)
    {
        icq_sendmsg (sess, uiG.last_sent_uin = cont->uin,
                     uiG.last_message_sent, uiG.last_message_sent_type);
        if (!UtilUIParseNick (&args, &cont, NULL, sess))
        {
            if (*args)
                M_print (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
            return 0;
        }
    }
}

/*
 * Send an instant message.
 */
static JUMP_F (CmdUserMessage)
{
    static UDWORD multi_uin;
    static int offset = 0;
    static char msg[1024];
    char *arg1 = NULL, *temp;
    UDWORD uin = 0;
    Contact *cont = NULL;
    SESSION;

    if (status)
    {
        arg1 = args;
        msg[offset] = 0;
        if (strcmp (arg1, END_MSG_STR) == 0)
        {
            msg[offset - 1] = msg[offset - 2] = 0;
            if (multi_uin == -1)
            {
                char *temp;
                
                for (cont = ContactStart (); ContactHasNext (cont); cont = ContactNext (cont))
                {
                    temp = strdup (msg);
                    icq_sendmsg (sess, cont->uin, temp, MSG_NORM | MSGF_MASS);
                    free (temp);
                }
            }
            else
            {
                icq_sendmsg (sess, multi_uin, msg, MSG_NORM);
                uiG.last_sent_uin = multi_uin;
            }
            return 0;
        }
        else if (strcmp (arg1, CANCEL_MSG_STR) == 0)
        {
            M_print (i18n (1038, "Message canceled\n"));
            return 0;
        }
        else
        {
            int diff, first = 1;

            while (offset + strlen (arg1) + 2 > 450)
            {
                M_print (i18n (1037, "Message sent before last line buffer is full\n"));
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
                if (multi_uin == -1)
                {
                    for (cont = ContactStart (); ContactHasNext (cont); cont = ContactNext (cont))
                    {
                        temp = strdup (msg);
                        icq_sendmsg (sess, cont->uin, temp, MSG_NORM | MSGF_MASS);
                        free (temp);
                    }
                }
                else
                {
                    temp = strdup (msg);
                    icq_sendmsg (sess, multi_uin, temp, MSG_NORM);
                    free (temp);
                    uiG.last_sent_uin = multi_uin;
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
                if (!UtilUIParseNick (&args, &cont, NULL, sess))
                {
                    M_print (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
                    return 0;
                }
                uin = cont->uin;
                UtilUIParseRemainder (&args, &arg1);
                break;
            case 2:
                if (!uiG.last_rcvd_uin)
                {
                    M_print (i18n (1741, "Must receive a message first\n"));
                    return 0;
                }
                uin = uiG.last_rcvd_uin;
                UtilUIParseRemainder (&args, &arg1);
                break;
            case 4:
                if (!uiG.last_sent_uin)
                {
                    M_print (i18n (1742, "Must write one message first\n"));
                    return 0;
                }
                uin = uiG.last_sent_uin;
                UtilUIParseRemainder (&args, &arg1);
                break;
            case 8:
                uin = -1;
                UtilUIParseRemainder (&args, &arg1);
                break;
            default:
                assert (0);
        }
        if (data != 8)
        {
            uiG.last_sent_uin = uin;
            TabAddUIN (uin);
        }
        if (arg1)
        {
            if (data == 8)
            {
                char *temp;
                
                for (cont = ContactStart (); ContactHasNext (cont); cont = ContactNext (cont))
                {
                    temp = strdup (arg1);
                    icq_sendmsg (sess, cont->uin, temp, MSG_NORM | MSGF_MASS);
                    free (temp);
                }
            }
            else
            {
                icq_sendmsg (sess, uin, arg1, MSG_NORM);
                uiG.last_sent_uin = uin;
            }
            return 0;
        }
        multi_uin = uin;
        if (uin == -1)
            M_print (i18n (2130, "Composing message to %sall%s:\n"), COLCONTACT, COLNONE);
        else if (ContactFindNick (uin))
            M_print (i18n (2131, "Composing message to %s%s%s:\n"), COLCONTACT, ContactFindNick (uin), COLNONE);
        else
            M_print (i18n (2132, "Composing message to %s%d%s:\n"), COLCONTACT, uin, COLNONE);
        offset = 0;
        status = data;
    }
    if (status == 8)
        R_setprompt (i18n (1042, "msg all> "));
    else
        R_setprompt (i18n (1041, "msg> "));
    return status;
}

/*
 * Changes verbosity.
 */
static JUMP_F(CmdUserVerbose)
{
    UDWORD i = 0;

    if (UtilUIParseInt (&args, &i))
        prG->verbose = i;
    else if (*args)
    {
        M_print (i18n (2115, "'%s' is not an integer.\n"), args);
        return 0;
    }
    M_print (i18n (1060, "Verbosity level is %d.\n"), prG->verbose = i);
    return 0;
}

static UDWORD __status (Contact *cont)
{
    if (cont->flags   & CONT_IGNORE)     return -2;
    if (cont->flags   & CONT_TEMPORARY)  return -2;
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
 * data & 16: show _only_ own status
 * data & 8: show only given nick
 * data & 4: show own status
 * data & 2: be verbose
 * data & 1: do not show offline
 */
static JUMP_F(CmdUserStatusDetail)
{
    UDWORD uin = 0, tuin = 0, i, lenuin = 0, lennick = 0, lenstat = 0, lenid = 0, totallen = 0;
    Contact *cont = NULL, *contr = NULL;
    Session *peer;
    UDWORD stati[] = { -2, STATUS_OFFLINE, STATUS_DND,    STATUS_OCC, STATUS_NA,
                           STATUS_AWAY,    STATUS_ONLINE, STATUS_FFC, STATUSF_BIRTH };
    SESSION;

    if (!data)
        UtilUIParseInt (&args, &data);

    if ((data & 8) && !UtilUIParseNick (&args, &cont, NULL, sess) && *args)
    {
        M_print (i18n (1700, "%s is not a valid user in your list.\n"), args);
        return 0;
    }

    if (cont)
        uin = cont->uin;

    for (cont = ContactStart (); ContactHasNext (cont); cont = ContactNext (cont))
    {
        if (uin && cont->uin != uin)
            continue;
        if (cont->flags & CONT_ALIAS && ~data & 2)
            continue;
        if (cont->uin > tuin)
            tuin = cont->uin;
        if (strlen (cont->nick) > lennick)
            lennick = strlen (cont->nick);
        if (cont->flags & CONT_ALIAS)
            continue;
        if (strlen (UtilStatus (cont->status)) > lenstat)
            lenstat = strlen (UtilStatus (cont->status));
        if (cont->version && strlen (cont->version) > lenid)
            lenid = strlen (cont->version);
    }

    while (tuin)
        lenuin++, tuin /= 10;
    totallen = 1 + lennick + 1 + lenstat + 2 + 1 + lenid + 2;
    if (data & 2)
        totallen += 2 + 3 + 1 + 1 + lenuin + 24;

    if (data & 4 && !uin)
    {
        Time_Stamp ();
        M_print (" " MAGENTA BOLD "%10lu" COLNONE " ", sess->uin);
        M_print (i18n (1071, "Your status is "));
        Print_Status (sess->status);
        M_print ("\n");
        if (data & 16)
            return 0;
    }
    if (!uin)
    {
        M_print (COLMESSAGE);
        for (i = totallen; i >= 20; i -= 20)
            M_print ("====================");
        M_print ("%.*s" COLNONE "\n", i, "====================");
    }
    for (i = (data & 1 ? 2 : 0); i < 9; i++)
    {
        status = stati[i];
        for (cont = ContactStart (); ContactHasNext (cont); cont = ContactNext (cont))
        {
            char statbuf[100], verbuf[100];
            
            contr = ContactFind (cont->uin);
            if (!contr)
                continue;

            if (uin && cont->uin != uin)
                continue;

            if (__status (contr) != status)
                continue;
            if (cont->flags & CONT_ALIAS && (~data & 2 || data & 1))
                continue;

            peer = sess->assoc ? SessionFind (TYPE_MSGDIRECT, cont->uin, sess->assoc) : NULL;

            snprintf (statbuf, sizeof (statbuf), "(%s)", UtilStatus (contr->status));
            if (contr->version)
                snprintf (verbuf,  sizeof (verbuf), "[%s]", contr->version);
            else
                verbuf[0] = '\0';

            if (data & 2)
                M_print (COLSERVER "%c%c%c%1.1d%c" COLNONE " %*ld",
                     cont->flags & CONT_ALIAS      ? '+' :
                     contr->flags & CONT_TEMPORARY ? '#' : ' ',
                     contr->flags & CONT_INTIMATE  ? '*' :
                      contr->flags & CONT_HIDEFROM ? '-' : ' ',
                     contr->flags & CONT_IGNORE    ? '^' : ' ',
                     contr->TCP_version,
                     peer ? (
                      peer->connect & CONNECT_OK   ? '&' :
                      peer->connect & CONNECT_FAIL ? '|' :
                      peer->connect & CONNECT_MASK ? ':' : '.' ) :
                      contr->TCP_version && contr->port &&
                      contr->outside_ip && ~contr->outside_ip ? '^' : ' ',
                     lenuin, cont->uin);

            M_print (COLSERVER "%c" COLCONTACT "%-*s" COLNONE " " COLMESSAGE "%-*s" COLNONE " %-*s %s",
                     data & 2                      ? ' ' :
                     cont->flags  & CONT_ALIAS     ? '+' :
                     contr->flags & CONT_TEMPORARY ? '#' :
                     contr->flags & CONT_INTIMATE  ? '*' :
                     contr->flags & CONT_HIDEFROM  ? '-' :
                     contr->flags & CONT_IGNORE    ? '^' :
                     !peer                         ? ' ' :
                     peer->connect & CONNECT_OK    ? '&' :
                     peer->connect & CONNECT_FAIL  ? '|' :
                     peer->connect & CONNECT_MASK  ? ':' : '.' ,
                     lennick, cont->nick, lenstat + 2, statbuf, lenid + 2, verbuf,
                     contr->seen_time != -1L && data & 2 ? ctime ((time_t *) &contr->seen_time) : "\n");
            
        }
    }
    if (uin)
    {
        if (!contr)
            return 0;

        M_print ("%-15s %s/%s:%d\n", i18n (1441, "IP:"), UtilIOIP (contr->outside_ip), UtilIOIP (contr->local_ip), contr->port);
        M_print ("%-15s %d\n", i18n (1453, "TCP version:"), contr->TCP_version);
        M_print ("%-15s %s (%d)\n", i18n (1454, "Connection:"),
                 contr->connection_type == 4 ? i18n (1493, "Peer-to-Peer") : i18n (1494, "Server Only"),
                 contr->connection_type);
        M_print ("%-15s %08x\n", i18n (2026, "TCP cookie:"), contr->cookie);
        return 0;
    }
    M_print (COLMESSAGE);
    for (i = totallen; i >= 20; i -= 20)
        M_print ("====================");
    M_print ("%.*s" COLNONE "\n", i, "====================");
    return 0;
}

/*
 * Shows the ignored user on the contact list.
 */
static JUMP_F(CmdUserIgnoreStatus)
{
    Contact *cont;

    M_print ("%s%s\n", W_SEPERATOR, i18n (1062, "Users ignored:"));
    /*  Sorts thru all ignored users */
    for (cont = ContactStart (); ContactHasNext (cont); cont = ContactNext (cont))
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

                M_print (COLCONTACT "%-20s\t" COLMESSAGE "(", cont->nick);
                Print_Status (cont->status);
                M_print (")" COLNONE "\n");
            }
        }
    }
    M_print (W_SEPERATOR);
    return 0;
}


/*
 * Displays the contact list in a wide format, similar to the ls command.
 */
static JUMP_F(CmdUserStatusWide)
{
    Contact **Online;           /* definitely won't need more; could    */
    Contact **Offline = NULL;   /* probably get away with less.    */
    int MaxLen = 0;             /* legnth of longest contact name */
    int i;
    int OnIdx = 0;              /* for inserting and tells us how many there are */
    int OffIdx = 0;             /* for inserting and tells us how many there are */
    int NumCols;                /* number of columns to display on screen        */
    int StatusLen;
    Contact *cont = NULL;
    char *stat;
    SESSION;

    if (data)
    {
        if ((Offline = (Contact **) malloc (MAX_CONTACTS * sizeof (Contact *))) == NULL)
        {
            M_print (i18n (1652, "Insuffificient memory to display a wide Contact List.\n"));
            return 0;
        }
    }

    if ((Online = (Contact **) malloc (MAX_CONTACTS * sizeof (Contact *))) == NULL)
    {
        M_print (i18n (1652, "Insuffificient memory to display a wide Contact List.\n"));
        return 0;
    }

    /* Filter the contact list into two lists -- online and offline. Also
       find the longest name in the list -- this is used to determine how
       many columns will fit on the screen. */
    for (cont = ContactStart (); ContactHasNext (cont); cont = ContactNext (cont))
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
        M_print (COLCLIENT "%s" COLMESSAGE, i18n (1653, "Offline"));
        for (i += strlen (i18n (1653, "Offline")); i < Get_Max_Screen_Width (); i++)
        {
            M_print ("=");
        }
        M_print (COLNONE "\n");
        for (i = 0; i < OffIdx; i++)
        {
            M_print (COLCONTACT "  %-*s" COLNONE, MaxLen + 2, Offline[i]->nick);
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

    stat = UtilStatus (sess->status);
    StatusLen = strlen (i18n (1071, "Your status is ")) + strlen (stat) + 12;
    for (i = 0; 2 * i + StatusLen + strlen (i18n (1654, "Online")) < Get_Max_Screen_Width (); i++)
    {
        M_print ("=");
    }
    M_print (COLCLIENT "%s" COLMESSAGE, i18n (1654, "Online"));
    for (i += strlen (i18n (1654, "Online")); i + StatusLen < Get_Max_Screen_Width (); i++)
    {
        M_print ("=");
    }

   /* Print our status */
    M_print (" " MAGENTA BOLD "%10lu" COLNONE " %s%s",
             sess->uin, i18n (1071, "Your status is "), stat);
    
    M_print (COLNONE "\n");
    for (i = 0; i < OnIdx; i++)
    {
        const char *status;
        char weird = 'W';       /* for weird statuses that are reported as hex */

        status = Convert_Status_2_Str (Online[i]->status);
        status = status ? status : &weird;
        if ((Online[i]->status & 0xfff) == STATUS_ONLINE)
        {
            status = " ";
        }
        M_print (COLNONE "%c " COLCONTACT "%-*s" COLNONE,
                 *status, MaxLen + 2, Online[i]->nick);
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
    free (stat);
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
    Contact *cont;
    SESSION;

    M_print (W_SEPERATOR);
    Time_Stamp ();
    M_print (" " MAGENTA BOLD "%10lu" COLNONE " ", sess->uin);
    M_print (i18n (1071, "Your status is "));
    Print_Status (sess->status);
    M_print ("\n");

    if (data)
    {
        M_print ("%s%s\n", W_SEPERATOR, i18n (1072, "Users offline:"));
        for (cont = ContactStart (); ContactHasNext (cont); cont = ContactNext (cont))
        {
            if (!(cont->flags & CONT_ALIAS))
            {
                if (!(cont->flags & CONT_IGNORE))
                {
                    if (cont->status == STATUS_OFFLINE)
                    {
                        if (cont->flags & CONT_INTIMATE)
                            M_print (COLSERVER "*" COLNONE);
                        else
                            M_print (" ");

                        M_print (COLCONTACT "%-20s\t" COLMESSAGE "(", cont->nick);
                        Print_Status (cont->status);
                        M_print (")" COLNONE "\n");
                    }
                }
            }
        }
    }

    M_print ("%s%s\n", W_SEPERATOR, i18n (1073, "Users online:"));
    for (cont = ContactStart (); ContactHasNext (cont); cont = ContactNext (cont))
    {
        if (!(cont->flags & CONT_ALIAS))
        {
            if (!(cont->flags & CONT_IGNORE))
            {
                if (cont->status != STATUS_OFFLINE)
                {
                    if (cont->flags & CONT_INTIMATE)
                        M_print (COLSERVER "*" COLNONE);
                    else
                        M_print (" ");

                    M_print (COLCONTACT "%-20s\t" COLMESSAGE "(", cont->nick);
                    Print_Status (cont->status);
                    M_print (")" COLNONE);
                    if (cont->version)
                       M_print (" [%s]", cont->version);
                    if (cont->status & STATUSF_BIRTH)
                       M_print (" (%s)", i18n (2033, "born today"));
                    M_print ("\n");
                }
            }
        }
    }
    M_print (W_SEPERATOR);
    return 0;
}

/*
 * Toggles sound or changes sound command.
 */
static JUMP_F(CmdUserSound)
{
    if (strlen (args))
    {
        prG->sound &= ~SFLAG_BEEP & ~SFLAG_CMD;
        if (!strcasecmp (args, "on") || !strcasecmp (args, i18n (1085, "on")))
           prG->sound |= SFLAG_BEEP;
        else if (strcasecmp (args, "off") && strcasecmp (args, i18n (1086, "off")))
        {
           prG->sound |= SFLAG_CMD;
           prG->sound_cmd = strdup (args);
        }
    }
    if (prG->sound & SFLAG_BEEP)
        M_print ("%s " COLSERVER "%s" COLNONE ".\n", i18n (1084, "Sound"), i18n (1085, "on"));
    else if (prG->sound & SFLAG_CMD)
        M_print ("%s " COLSERVER "%s" COLNONE ".\n", i18n (1083, "Sound cmd"), prG->sound_cmd);
    else
        M_print ("%s " COLSERVER "%s" COLNONE ".\n", i18n (1084, "Sound"), i18n (1086, "off"));
    return 0;
}

/*
 * Toggles soundonline or changes soundonline command.
 */
static JUMP_F(CmdUserSoundOnline)
{
    if (strlen (args))
    {
        prG->sound &= ~SFLAG_ON_BEEP & ~SFLAG_ON_CMD;
        if (!strcasecmp (args, "on") || !strcasecmp (args, i18n (1085, "on")))
           prG->sound |= SFLAG_ON_BEEP;
        else if (strcasecmp (args, "off") && strcasecmp (args, i18n (1086, "off")))
        {
           prG->sound |= SFLAG_ON_CMD;
           prG->sound_on_cmd = strdup (args);
        }
    }
    if (prG->sound & SFLAG_ON_BEEP)
        M_print ("%s " COLSERVER "%s" COLNONE ".\n", i18n (1804, "SoundOnline"), i18n (1085, "on"));
    else if (prG->sound & SFLAG_ON_CMD)
        M_print ("%s " COLSERVER "%s" COLNONE ".\n", i18n (1802, "SoundOnline cmd"), prG->sound_on_cmd);
    else
        M_print ("%s " COLSERVER "%s" COLNONE ".\n", i18n (1804, "SoundOnline"), i18n (1086, "off"));
    return 0;
}

/*
 * Toggles soundoffine or changes soundoffline command.
 */
static JUMP_F(CmdUserSoundOffline)
{
    if (strlen (args))
    {
        prG->sound &= ~SFLAG_OFF_BEEP & ~SFLAG_OFF_CMD;
        if (!strcasecmp (args, "on") || !strcasecmp (args, i18n (1085, "on")))
           prG->sound |= SFLAG_OFF_BEEP;
        else if (strcasecmp (args, "off") && strcasecmp (args, i18n (1086, "off")))
        {
           prG->sound |= SFLAG_OFF_CMD;
           prG->sound_off_cmd = strdup (args);
        }
    }
    if (prG->sound & SFLAG_OFF_BEEP)
        M_print ("%s " COLSERVER "%s" COLNONE ".\n", i18n (1805, "SoundOffline"), i18n (1085, "on"));
    else if (prG->sound & SFLAG_OFF_CMD)
        M_print ("%s " COLSERVER "%s" COLNONE ".\n", i18n (1803, "SoundOffline cmd"), prG->sound_off_cmd);
    else
        M_print ("%s " COLSERVER "%s" COLNONE ".\n", i18n (1805, "SoundOffline"), i18n (1086, "off"));
    return 0;
}

/*
 * Toggles autoaway or sets autoaway time.
 */
static JUMP_F(CmdUserAutoaway) 
{
    char *arg1 = NULL;
    UDWORD i = 0;

    if (UtilUIParseInt (&args, &i))
    {
        if (prG->away_time)
            uiG.away_time_prev = prG->away_time;
        prG->away_time = i;
    }
    else if (UtilUIParse (&args, &arg1))
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
    M_print (i18n (1766, "Changing status to away resp. not available after idling %s%d%s seconds.\n"),
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
    
    if (!UtilUIParse (&args, &arg1) || !strcmp (arg1, "help") || !strcmp (arg1, "?"))
    {
        M_print (i18n (1820, "%s <option> [on|off] - control simple options.\n"), CmdUserLookupName ("set"));
        M_print (i18n (1822, "    color: use colored text output.\n"));
        M_print (i18n (1815, "    funny: use funny messages for output.\n"));
        M_print (i18n (2018, "    quiet: be quiet about status changes.\n"));
    }
    else if (!strcmp (arg1, "color"))
    {
        if (UtilUIParse (&args, &arg1))
        {
            if (!strcmp (arg1, "on") || !strcmp (arg1, i18n (1085, "on")))
            {
                prG->flags |= FLAG_COLOR;
            }
            else if (!strcmp (arg1, "off") || !strcmp (arg1, i18n (1086, "off")))
            {
                prG->flags &= ~FLAG_COLOR;
            }
        }

        if (!quiet)
        {
            if (prG->flags & FLAG_COLOR)
                M_print (i18n (2133, "Color is %s%s%s.\n"),
                         COLMESSAGE, i18n (1085, "on"), COLNONE);
            else
                M_print (i18n (2133, "Color is %s%s%s.\n"),
                         COLMESSAGE, i18n (1086, "off"), COLNONE);
        }
    }
    else if (!strcmp (arg1, "funny"))
    {
        if (UtilUIParse (&args, &arg1))
        {
            if (!strcmp (arg1, "on") || !strcmp (arg1, i18n (1085, "on")))
            {
                prG->flags |= FLAG_FUNNY;
            }
            else if (!strcmp (arg1, "off") || !strcmp (arg1, i18n (1086, "off")))
            {
                prG->flags &= ~FLAG_FUNNY;
            }
        }
        
        if (!quiet)
        {
            if (prG->flags & FLAG_FUNNY)
                M_print (i18n (2134, "Funny messages are %s%s%s.\n"),
                         COLMESSAGE, i18n (1085, "on"), COLNONE);
            else
                M_print (i18n (2134, "Funny messages are %s%s%s.\n"),
                         COLMESSAGE, i18n (1086, "off"), COLNONE);
        }
    }
    else if (!strcmp (arg1, "quiet"))
    {
        if (UtilUIParse (&args, &arg1))
        {
            if (!strcmp (arg1, "on") || !strcmp (arg1, i18n (1085, "on")))
            {
                prG->flags |= FLAG_QUIET;
            }
            else if (!strcmp (arg1, "off") || !strcmp (arg1, i18n (1086, "off")))
            {
                prG->flags &= ~FLAG_QUIET;
            }
        }
        
        if (!quiet)
        {
            if (prG->flags & FLAG_QUIET)
                M_print (i18n (2135, "Quiet output is %s%s%s.\n"),
                         COLMESSAGE, i18n (1085, "on"), COLNONE);
            else
                M_print (i18n (2135, "Quiet output is %s%s%s.\n"),
                         COLMESSAGE, i18n (1086, "off"), COLNONE);
        }
    }
    else
        M_print (i18n (1816, "Unknown option %s to set command.\n"), arg1);
    return 0;
}

/*
 * Clears the screen.
 */
static JUMP_F(CmdUserClear)
{
    clrscr ();
    return 0;
}


/*
 * Registers a new user.
 */
static JUMP_F(CmdUserRegister)
{
    if (strlen (args))
    {
        Session *sess;
        if ((sess = SessionFind (TYPE_SERVER, 0, NULL)))
            SrvRegisterUIN (sess, args);
        else if ((sess = SessionFind (TYPE_SERVER_OLD, 0, NULL)))
            CmdPktCmdRegNewUser (sess, args);     /* TODO */
        else
            SrvRegisterUIN (NULL, args);
    }
    return 0;
}

/*
 * Toggles ignoring a user.
 */
static JUMP_F(CmdUserTogIgnore)
{
    Contact *cont = NULL, *contr = NULL;
    SESSION;

    if (!UtilUIParseNick (&args, &cont, &contr, sess))
    {
        M_print (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
        return 0;
    }

    if (contr->flags & CONT_IGNORE)
    {
        contr->flags &= ~CONT_IGNORE;
        M_print (i18n (1666, "Unignored %s."), cont->nick);
    }
    else
    {
        contr->flags |= CONT_IGNORE;
        M_print (i18n (1667, "Ignoring %s."), cont->nick);
    }
    M_print ("\n");
    return 0;
}

/*
 * Toggles beeing invisible to a user.
 */
static JUMP_F(CmdUserTogInvis)
{
    Contact *cont = NULL, *contr = NULL;
    SESSION;

    if (!UtilUIParseNick (&args, &cont, &contr, sess))
    {
        M_print (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
        return 0;
    }

    if (contr->flags & CONT_HIDEFROM)
    {
        contr->flags &= ~CONT_HIDEFROM;
        if (sess->ver > 6)
            SnacCliReminvis (sess, cont->uin);
        else
            CmdPktCmdUpdateList (sess, cont->uin, INV_LIST_UPDATE, FALSE);
        M_print (i18n (2020, "Being visible to %s."), cont->nick);
    }
    else
    {
        contr->flags |= CONT_HIDEFROM;
        contr->flags &= ~CONT_INTIMATE;
        if (sess->ver > 6)
            SnacCliAddinvis (sess, cont->uin);
        else
            CmdPktCmdUpdateList (sess, cont->uin, INV_LIST_UPDATE, TRUE);
        M_print (i18n (2021, "Being invisible to %s."), cont->nick);
    }
    if (sess->ver < 6)
    {
        CmdPktCmdContactList (sess);
        CmdPktCmdInvisList (sess);
        CmdPktCmdVisList (sess);
        CmdPktCmdStatusChange (sess, sess->status);
    }
    M_print ("\n");
    return 0;
}

/*
 * Toggles visibility to a user.
 */
static JUMP_F(CmdUserTogVisible)
{
    Contact *cont = NULL, *contr = NULL;
    SESSION;

    if (!UtilUIParseNick (&args, &cont, &contr, sess))
    {
        M_print (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
        return 0;
    }

    if (contr->flags & CONT_INTIMATE)
    {
        contr->flags &= ~CONT_INTIMATE;
        if (sess->ver > 6)
            SnacCliRemvisible (sess, cont->uin);
        else
            CmdPktCmdUpdateList (sess, cont->uin, VIS_LIST_UPDATE, FALSE);
        M_print (i18n (1670, "Normal visible to %s now."), cont->nick);
    }
    else
    {
        contr->flags |= CONT_INTIMATE;
        contr->flags &= ~CONT_HIDEFROM;
        if (sess-> ver > 6)
            SnacCliAddvisible (sess, cont->uin);
        else
            CmdPktCmdUpdateList (sess, cont->uin, VIS_LIST_UPDATE, TRUE);
        M_print (i18n (1671, "Always visible to %s now."), cont->nick);
    }

    M_print ("\n");
    return 0;
}

/*
 * Add a user to your contact list.
 */
static JUMP_F(CmdUserAdd)
{
    Contact *cont = NULL, *cont2;
    char *arg1;
    SESSION;

    if (!UtilUIParseNick (&args, &cont, NULL, sess))
    {
        M_print (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
        return 0;
    }

    if (!UtilUIParseRemainder (&args, &arg1))
    {
        M_print (i18n (2116, "No new nick name given.\n"));
        return 0;
    }

    if (cont->flags & CONT_TEMPORARY)
    {
        M_print (i18n (2117, "%d added as %s.\n"), cont->uin, arg1);
        if (!Add_User (sess, cont->uin, arg1))
            M_print (i18n (1754, " Note: You need to 'save' to write new contact list to disc.\n"));
        if (sess->ver > 6)
            SnacCliAddcontact (sess, cont->uin);
        else
            CmdPktCmdContactList (sess);
        cont->flags &= ~CONT_TEMPORARY;
        strncpy (cont->nick, arg1, 18);
        cont->nick[19] = '\0';
    }
    else
    {
        if ((cont2 = ContactFindAlias (cont->uin, arg1)))
        {
            M_print (i18n (2146, "'%s' is already an alias for '%s' (%d).\n"),
                     cont2->nick, cont->nick, cont->uin);
        }
        else if ((cont2 = ContactFindContact (arg1)))
        {
            M_print (i18n (2147, "'%s' (%d) is already used as a nick.\n"),
                     cont2->nick, cont2->uin);
        }
        else
        {
            if (!(cont2 = ContactAdd (cont->uin, arg1)))
            {
                M_print (i18n (2118, "Out of memory.\n"));
            }
            else
            {
                M_print (i18n (2148, "Added '%s' as an alias for '%s' (%d).\n"),
                         cont2->nick, cont->nick, cont->uin);
                M_print (i18n (1754, " Note: You need to 'save' to write new contact list to disc.\n"));
            }
        }
    }

    return 0;
}

/*
 * Remove a user from your contact list.
 */
static JUMP_F(CmdUserRem)
{
    Contact *cont = NULL;
    UDWORD uin;
    char *alias;
    SESSION;

    if (!UtilUIParseNick (&args, &cont, NULL, sess))
    {
        M_print (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
        return 0;
    }
    
    alias = strdup (cont->nick);
    uin = cont->uin;
    
    ContactRem (cont);
    
    if ((cont = ContactFind (uin)))
    {
        M_print (i18n (2149, "Removed alias '%s' for '%s' (%d).\n"),
                 alias, cont->nick, uin);
    }
    else
    {
        if (sess->ver > 6)
            SnacCliRemcontact (sess, cont->uin);
        else
            CmdPktCmdContactList (sess);
        M_print (i18n (2150, "Removed contact '%s' (%d)."),
                 alias, uin);
    }

    M_print (i18n (1754, " Note: You need to 'save' to write new contact list to disc.\n"));
    return 0;
}

/*
 * Basic information on last user a message was received from.
 */
static JUMP_F(CmdUserRInfo)
{
    Contact *cont;
    SESSION;

    cont = ContactFind (uiG.last_rcvd_uin);
    if (!cont)
    {
        M_print (i18n (2151, "%d is not on your contact list anymore.\n"),
                 uiG.last_rcvd_uin);
        return 0;
    }
    
    M_print (i18n (1672, "%s's IP address is "), cont->nick);
    Print_IP (uiG.last_rcvd_uin);
    M_print ("\t");

    if (cont->port != (UWORD) 0xffff)
        M_print (i18n (1673, "The port is %d.\n"), cont->port);
    else
        M_print (i18n (1674, "The port is unknown.\n"));
    
    if (sess->ver > 6)
        SnacCliMetareqinfo (sess, uiG.last_rcvd_uin);
    else
        CmdPktCmdMetaReqInfo (sess, uiG.last_rcvd_uin);
    return 0;
}

/*
 * Authorize another user to add you to the contact list.
 */
static JUMP_F(CmdUserAuth)
{
    char *cmd = NULL, *argsb, *msg = NULL;
    Contact *cont = NULL;
    SESSION;

    argsb = args;
    if (!UtilUIParse (&args, &cmd))
    {
        M_print (i18n (2119, "auth [grant] <nick>    - grant authorization.\n"));
        M_print (i18n (2120, "auth deny <nick> <msg> - refuse authorization.\n"));
        M_print (i18n (2121, "auth req  <nick> <msg> - request authorization.\n"));
        M_print (i18n (2145, "auth add  <nick>       - authorized add.\n"));
        return 0;
    }
    cmd = strdup (cmd);

    if (UtilUIParseNick (&args, &cont, NULL, sess))
    {
        UtilUIParse (&args, &msg);
        if (!strcmp (cmd, "req"))
        {
/* FIXME: v8 packets seem to cause trouble */
            if (!msg)         /* FIXME: let it untranslated? */
                msg = "Please authorize my request and add me to your Contact List\n";
/*            if (sess->type == TYPE_SERVER && sess->ver >= 8)
                SnacCliReqauth (sess, cont->uin, msg);
            else */ if (sess->type == TYPE_SERVER)
                SnacCliSendmsg (sess, cont->uin, msg, MSG_AUTH_REQ);
            else
                CmdPktCmdSendMessage (sess, cont->uin, msg, MSG_AUTH_REQ);
            free (cmd);
            return 0;
        }
        else if (!strcmp (cmd, "deny"))
        {
            if (!msg)         /* FIXME: let it untranslated? */
                msg = "Authorization refused\n";
/*            if (sess->type == TYPE_SERVER && sess->ver >= 8)
                SnacCliAuthorize (sess, cont->uin, 0, msg);
            else */ if (sess->type == TYPE_SERVER)
                SnacCliSendmsg (sess, cont->uin, "\x03", MSG_AUTH_DENY);
            else
                CmdPktCmdSendMessage (sess, cont->uin, "\x03", MSG_AUTH_DENY);
            free (cmd);
            return 0;
        }
        else if (!strcmp (cmd, "add"))
        {
/*            if (sess->type == TYPE_SERVER && sess->ver >= 8)
                SnacCliGrantauth (sess, cont->uin);
            else */ if (sess->type == TYPE_SERVER)
                SnacCliSendmsg (sess, cont->uin, "\x03", MSG_AUTH_ADDED);
            else
                CmdPktCmdSendMessage (sess, cont->uin, "\x03", MSG_AUTH_ADDED);
            free (cmd);
            return 0;
        }
    }
    if ((strcmp (cmd, "grant") && !UtilUIParseNick (&argsb, &cont, NULL, sess)) || !cont)
    {
        M_print (i18n (1061, "'%s' not recognized as a nick name.\n"),
                 strcmp (cmd, "grant") && strcmp (cmd, "req") && strcmp (cmd, "deny") ? argsb : args);
        free (cmd);
        return 0;
    }
        
/*    if (sess->type == TYPE_SERVER && sess->ver >= 8)
        SnacCliAuthorize (sess, cont->uin, 1, NULL);
    else */ if (sess->type == TYPE_SERVER)
        SnacCliSendmsg (sess, cont->uin, "\x03", MSG_AUTH_GRANT);
    else
        CmdPktCmdSendMessage (sess, cont->uin, "\x03", MSG_AUTH_GRANT);
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
    Contact *cont = NULL;
    SESSION;

    if (!UtilUIParseNick (&args, &cont, NULL, sess))
    {
        M_print (i18n (1061, "'%s' not recognized as a nick name.\n"), args);
        return 0;
    }

    if (!UtilUIParse (&args, &url))
    {
        M_print (i18n (1678, "Need URL please.\n"));
        return 0;
    }

    url = strdup (url);

    if (!UtilUIParseRemainder (&args, &msg))
        msg = "";

    uiG.last_sent_uin = cont->uin;
    icq_sendurl (sess, cont->uin, url, msg);
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
    M_print (i18n (1681, "Last %d people you talked to:\n"), i);
    for (TabReset (); TabHasNext ();)
    {
        UDWORD uin = TabGetNext ();
        Contact *cont;
        M_print ("    %s", ContactFindName (uin));
        cont = ContactFind (uin);
        if (cont)
        {
            M_print (COLMESSAGE " (");
            Print_Status (cont->status);
            M_print (")" COLNONE);
        }
        M_print ("\n");
    }
    return 0;
}

/*
 * Displays the last message received from the given nickname.
 */
static JUMP_F(CmdUserLast)
{
    Contact *cont = NULL, *contr = NULL;
    SESSION;

    if (!UtilUIParseNick (&args, &cont, &contr, sess))
    {
        M_print (i18n (1682, "You have received messages from:\n"));
        for (cont = ContactStart (); ContactHasNext (cont); cont = ContactNext (cont))
            if (cont->last_message)
                M_print ("  " COLCONTACT "%s" COLNONE " %s " COLMESSAGE "%s" COLNONE "\n",
                         cont->nick, UtilUITime (&cont->last_time), cont->last_message);
    }
    else
    {
        if (contr->last_message)
        {
            M_print (i18n (2106, "Last message from %s%s%s at %s:\n"),
                     COLCONTACT, cont->nick, COLNONE, UtilUITime (&contr->last_time));
            M_print (COLMESSAGE "%s" COLNONE "\n", cont->last_message);
        }
        else
        {
            M_print (i18n (2107, "No messages received from %s%s%s.\n"),
                     COLCONTACT, cont->nick, COLNONE);
        }
    }
    return 0;
}

/*
 * Shows mICQ's uptime.
 */
static JUMP_F(CmdUserUptime)
{
    double TimeDiff = difftime (time (NULL), uiG.start_time);
    Session *sess;
    int Days, Hours, Minutes, Seconds, i;
    int pak_sent = 0, pak_rcvd = 0, real_pak_sent = 0, real_pak_rcvd = 0;

    Seconds = (int) TimeDiff % 60;
    TimeDiff = TimeDiff / 60.0;
    Minutes = (int) TimeDiff % 60;
    TimeDiff = TimeDiff / 60.0;
    Hours = (int) TimeDiff % 24;
    TimeDiff = TimeDiff / 24.0;
    Days = TimeDiff;

    M_print ("%s ", i18n (1687, "mICQ has been running for"));
    if (Days != 0)
        M_print (COLMESSAGE "%02d" COLNONE " %s, ", Days, i18n (1688, "days"));
    if (Hours != 0)
        M_print (COLMESSAGE "%02d" COLNONE " %s, ", Hours, i18n (1689, "hours"));
    if (Minutes != 0)
        M_print (COLMESSAGE "%02d" COLNONE " %s, ", Minutes, i18n (1690, "minutes"));
    M_print (COLMESSAGE "%02d" COLNONE " %s.\n", Seconds, i18n (1691, "seconds"));
/*    M_print ("%s " COLMESSAGE "%d" COLNONE " / %d\n", i18n (1692, "Contacts:"), uiG.Num_Contacts, MAX_CONTACTS); */

    M_print (i18n (1746, " nr type         sent/received packets/unique packets\n"));
    for (i = 0; (sess = SessionNr (i)); i++)
    {
        M_print ("%3d %-12s %7d %7d %7d %7d\n",
                 i + 1, SessionType (sess), sess->stat_pak_sent, sess->stat_pak_rcvd,
                 sess->stat_real_pak_sent, sess->stat_real_pak_rcvd);
        pak_sent += sess->stat_pak_sent;
        pak_rcvd += sess->stat_pak_rcvd;
        real_pak_sent += sess->stat_real_pak_sent;
        real_pak_rcvd += sess->stat_real_pak_rcvd;
    }
    M_print ("    %-12s %7d %7d %7d %7d\n",
             i18n (1747, "total"), pak_sent, pak_rcvd,
             real_pak_sent, real_pak_rcvd);
    M_print (i18n (2073, "Memory usage: %d packets processing.\n"), uiG.packets);
    return 0;
}

/*
 * List connections, and open/close them.
 */
static JUMP_F(CmdUserConn)
{
    char *arg1 = NULL;
    int i;
    Session *sess;

    if (!UtilUIParse (&args, &arg1))
    {
        M_print (i18n (1887, "Connections:"));
        M_print ("\n  " COLINDENT);
        
        for (i = 0; (sess = SessionNr (i)); i++)
        {
            M_print (i18n (2093, "%02d %-12s version %d for %s (%x), at %s:%d %s\n"),
                     i + 1, SessionType (sess), sess->ver, ContactFindName (sess->uin), sess->status,
                     sess->server ? sess->server : UtilIOIP (sess->ip), sess->port,
                     sess->connect & CONNECT_FAIL ? i18n (1497, "failed") :
                     sess->connect & CONNECT_OK   ? i18n (1934, "connected") :
                     sess->connect & CONNECT_MASK ? i18n (1911, "connecting") : i18n (1912, "closed"));
            if (prG->verbose)
            {
                M_print (i18n (1935, "    type %d socket %d ip %s (%d) on [%s,%s] id %x/%x/%x\n"),
                     sess->type, sess->sok, UtilIOIP (sess->ip),
                     sess->connect, UtilIOIP (sess->our_local_ip), UtilIOIP (sess->our_outside_ip),
                     sess->our_session, sess->our_seq, sess->our_seq2);
                M_print (i18n (2081, "    at %p parent %p assoc %p\n"), sess, sess->parent, sess->assoc);
            }
        } 
        M_print (COLEXDENT "\r");
    }
    else if (!strcmp (arg1, "login") || !strcmp (arg1, "open"))
    {
        UDWORD i = 0, j;

        if (!UtilUIParseInt (&args, &i))
            for (j = 0; (sess = SessionNr (j)); j++)
                if (sess->type & TYPEF_SERVER)
                {
                    i = j + 1;
                    break;
                }

        sess = SessionNr (i - 1);
        if (!sess)
        {
            M_print (i18n (1894, "There is no connection number %d.\n"), i);
            return 0;
        }
        if (sess->connect & CONNECT_OK)
        {
            M_print (i18n (1891, "Connection %d is already open.\n"), i);
            return 0;
        }

        SessionInit (sess);
    }
    else if (!strcmp (arg1, "select"))
    {
        UDWORD i = 0;

        if (!UtilUIParseInt (&args, &i))
            i = SessionFindNr (currsess) + 1;

        sess = SessionNr (i - 1);
        if (!sess && !(sess = SessionFind (TYPEF_SERVER, i, NULL)))
        {
            M_print (i18n (1894, "There is no connection number %d.\n"), i);
            return 0;
        }
        if (~sess->type & TYPEF_SERVER)
        {
            M_print (i18n (2098, "Connection %d is not a server connection.\n"), i);
            return 0;
        }
        if (~sess->connect & CONNECT_OK)
        {
            M_print (i18n (2096, "Connection %d is not open.\n"), i);
            return 0;
        }
        currsess = sess;
        M_print (i18n (2099, "Selected connection %d (version %d, UIN %d) as current connection.\n"),
                 i, sess->ver, sess->uin);
    }
    else if (!strcmp (arg1, "remove") || !strcmp (arg1, "delete"))
    {
        UDWORD i = 0;

        UtilUIParseInt (&args, &i);

        sess = SessionNr (i - 1);
        if (!sess)
        {
            M_print (i18n (1894, "There is no connection number %d.\n"), i);
            return 0;
        }
        if (sess->spref)
        {
            M_print (i18n (2102, "Connection %d is a configured connection.\n"), i);
            return 0;
        }
        M_print (i18n (2101, "Removing connection %d and its dependands completely.\n"), i);
        SessionClose (sess);
    }
    else
    {
        M_print (i18n (1892, "conn               List available connections.\n"));
        M_print (i18n (2094, "conn login         Open first server connection.\n"));
        M_print (i18n (1893, "conn login <nr>    Open connection <nr>.\n"));
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
    SESSION;

    if (sess->type != TYPE_SERVER)
        return 0;

    if (!data)
    {
        if (!UtilUIParse (&args, &tmp))       data = 0;
        else if (!strcasecmp (tmp, "show"))   data = 1;
        else if (!strcasecmp (tmp, "diff"))   data = 2;
        else if (!strcasecmp (tmp, "add"))    data = 3;
        else if (!strcasecmp (tmp, "import")) data = 3;
        else                                  data = 0;
    }
    if (data)
    {
        QueueEnqueueData (sess, 0, QUEUE_REQUEST_ROSTER, data, 0x7fffffffL, NULL, NULL, NULL);
        SnacCliReqroster (sess);
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
    SESSION;

    switch (status)
    {
        case 0:
            if (*args)
            {
                if (sess->type == TYPE_SERVER_OLD)
                    CmdPktCmdSearchUser (sess, args, "", "", "");
                else
                    SnacCliSearchbymail (sess, args);
                
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
            if (sess->type == TYPE_SERVER_OLD)
                CmdPktCmdSearchUser (sess, email, nick, first, last);
            else
                SnacCliSearchbypersinf (sess, email, nick, first, last);
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
    SESSION;

    if (!strcmp (args, "."))
        status += 400;

    switch (status)
    {
        case 0:
            if (!UtilUIParse (&args, &arg1))
            {
                M_print (i18n (1960, "Enter data to search user for. Enter '.' to start the search.\n"));
                R_setpromptf ("%s ", i18n (1656, "Enter the user's nick name:"));
                return 200;
            }
            arg1 = strdup (arg1);
            if (UtilUIParse (&args, &arg2))
            {
                if (sess->ver > 6)
                    SnacCliSearchbypersinf (sess, NULL, NULL, arg1, arg2);
                else
                    CmdPktCmdSearchUser (sess, NULL, NULL, arg1, arg2);
                free (arg1);
                return 0;
            }
            if (strchr (arg1, '@'))
            {
                if (sess->type == TYPE_SERVER_OLD)
                    CmdPktCmdSearchUser (sess, arg1, "", "", "");
                else
                    SnacCliSearchbymail (sess, arg1);
            }
            else
            {
                if (sess->type == TYPE_SERVER_OLD)
                    CmdPktCmdSearchUser (sess, NULL, arg1, NULL, NULL);
                else
                    SnacCliSearchbypersinf (sess, NULL, arg1, NULL, NULL);
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
                M_print ("%s\n", i18n (1029, "Please enter YES or NO!"));
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
            if (sess->ver > 6)
            {
                if (status > 250 && status <= 403)
                    SnacCliSearchbypersinf (sess, wp.email, wp.nick, wp.first, wp.last);
                else
                    SnacCliSearchwp (sess, &wp);
            }
            else
            {
                if (status > 250 && status <= 403)
                    CmdPktCmdSearchUser (sess, wp.email, wp.nick, wp.first, wp.last);
                else
                    CmdPktCmdMetaSearchWP (sess, &wp);
            }

            if (wp.nick)       free (wp.nick);       wp.nick = NULL;
            if (wp.last)       free (wp.last);       wp.last = NULL;
            if (wp.first)      free (wp.first);      wp.first = NULL;
            if (wp.email)      free (wp.email);      wp.email = NULL;
            if (wp.company)    free (wp.company);    wp.company = NULL;
            if (wp.department) free (wp.department); wp.department = NULL;
            if (wp.position)   free (wp.position);   wp.position = NULL;
            if (wp.city)       free (wp.city);       wp.city = NULL;
            if (wp.state)      free (wp.state);      wp.state = NULL;
            return 0;
    }
    return 0;
}

/*
 * Update your basic user information.
 */
static JUMP_F(CmdUserUpdate)
{
    static MetaGeneral user;
    SESSION;

    switch (status)
    {
        case 0:
            R_setpromptf ("%s ", i18n (1553, "Enter Your New Nickname:"));
            return 300;
        case 300:
            user.nick = strdup ((char *) args);
            R_setpromptf ("%s ", i18n (1554, "Enter your new first name:"));
            return ++status;
        case 301:
            user.first = strdup ((char *) args);
            R_setpromptf ("%s ", i18n (1555, "Enter your new last name:"));
            return ++status;
        case 302:
            user.last = strdup ((char *) args);
            R_setpromptf ("%s ", i18n (1556, "Enter your new email address:"));
            return ++status;
        case 303:
            user.email = strdup ((char *) args);
            if (sess->ver > 6)
            {
                R_setpromptf ("%s ", i18n (1544, "Enter new city:"));
                status += 3;
                return status;
            }
            R_setpromptf ("%s ", i18n (1542, "Enter other email address:"));
            return ++status;
        case 304:
            user.email2 = strdup ((char *) args);
            R_setpromptf ("%s ", i18n (1543, "Enter old email address:"));
            return ++status;
        case 305:
            user.email3 = strdup ((char *) args);
            R_setpromptf ("%s ", i18n (1544, "Enter new city:"));
            return ++status;
        case 306:
            user.city = strdup ((char *) args);
            R_setpromptf ("%s ", i18n (1545, "Enter new state:"));
            return ++status;
        case 307:
            user.state = strdup ((char *) args);
            R_setpromptf ("%s ", i18n (1546, "Enter new phone number:"));
            return ++status;
        case 308:
            user.phone = strdup ((char *) args);
            R_setpromptf ("%s ", i18n (1547, "Enter new fax number:"));
            return ++status;
        case 309:
            user.fax = strdup ((char *) args);
            R_setpromptf ("%s ", i18n (1548, "Enter new street address:"));
            return ++status;
        case 310:
            user.street = strdup ((char *) args);
            R_setpromptf ("%s ", i18n (1549, "Enter new cellular number:"));
            return ++status;
        case 311:
            user.cellular = strdup ((char *) args);
            R_setpromptf ("%s ", i18n (1550, "Enter new zip code (must be numeric):"));
            return ++status;
        case 312:
            user.zip = atoi ((char *) args);
            R_setpromptf ("%s ", i18n (1551, "Enter your country's phone ID number:"));
            return ++status;
        case 313:
            user.country = atoi ((char *) args);
            R_setpromptf ("%s ", i18n (1552, "Enter your time zone (+/- 0-12):"));
            return ++status;
        case 314:
            user.tz = atoi ((char *) args);
            user.tz *= 2;
            R_setpromptf ("%s ", i18n (1557, "Do you want to require Mirabilis users to request your authorization? (YES/NO)"));
            return ++status;
        case 315:
            if (strcasecmp (args, i18n (1028, "NO")) && strcasecmp (args, i18n (1027, "YES")))
            {
                M_print ("%s\n", i18n (1029, "Please enter YES or NO!"));
                R_setpromptf ("%s ", i18n (1557, "Do you want to require Mirabilis users to request your authorization? (YES/NO)"));
                return status;
            }
            user.auth = strcasecmp (args, i18n (1027, "YES")) ? FALSE : TRUE;
            R_setpromptf ("%s ", i18n (1605, "Do you want your status to be available on the web? (YES/NO)"));
            return ++status;
        case 316:
            if (strcasecmp (args, i18n (1028, "NO")) && strcasecmp (args, i18n (1027, "YES")))
            {
                M_print ("%s\n", i18n (1029, "Please enter YES or NO!"));
                R_setpromptf ("%s ", i18n (1605, "Do you want your status to be available on the web? (YES/NO)"));
                return status;
            }
            user.webaware = strcasecmp (args, i18n (1027, "YES")) ? FALSE : TRUE;
            R_setpromptf ("%s ", i18n (1857, "Do you want to hide your IP from other contacts? (YES/NO)"));
            return ++status;
        case 317:
            if (strcasecmp (args, i18n (1028, "NO")) && strcasecmp (args, i18n (1027, "YES")))
            {
                M_print ("%s\n", i18n (1029, "Please enter YES or NO!"));
                R_setpromptf ("%s ", i18n (1857, "Do you want to hide your IP from other contacts? (YES/NO)"));
                return status;
            }
            user.hideip = strcasecmp (args, i18n (1027, "YES")) ? FALSE : TRUE;
            R_setpromptf ("%s ", i18n (1622, "Do you want to apply these changes? (YES/NO)"));
            return ++status;
        case 318:
            if (strcasecmp (args, i18n (1028, "NO")) && strcasecmp (args, i18n (1027, "YES")))
            {
                M_print ("%s\n", i18n (1029, "Please enter YES or NO!"));
                R_setpromptf ("%s ", i18n (1622, "Do you want to apply these changes? (YES/NO)"));
                return status;
            }

            if (!strcasecmp (args, i18n (1027, "YES")))
            {
                if (sess->ver > 6)
                    SnacCliMetasetgeneral (sess, &user);
                else
                    CmdPktCmdMetaGeneral (sess, &user);
            }

            free (user.nick);
            free (user.last);
            free (user.first);
            free (user.email);
            return 0;
    }
    return 0;
}

/*
 * Update additional information.
 */
static JUMP_F(CmdUserOther)
{
    static MetaMore other;
    int temp;
    SESSION;

    switch (status)
    {
        case 0:
            R_setpromptf ("%s ", i18n (1535, "Enter new age:"));
            return 400;
        case 400:
            other.age = atoi (args);
            R_setpromptf ("%s ", i18n (1536, "Enter new sex:"));
            return ++status;
        case 401:
            if (!strncasecmp (args, i18n (1528, "female"), 1))
            {
                other.sex = 1;
            }
            else if (!strncasecmp (args, i18n (1529, "male"), 1))
            {
                other.sex = 2;
            }
            else
            {
                other.sex = 0;
            }
            R_setpromptf ("%s ", i18n (1537, "Enter new homepage:"));
            return ++status;
        case 402:
            other.hp = strdup (args);
            R_setpromptf ("%s ", i18n (1538, "Enter new year of birth (4 digits):"));
            return ++status;
        case 403:
            other.year = atoi (args);
            R_setpromptf ("%s ", i18n (1539, "Enter new month of birth:"));
            return ++status;
        case 404:
            other.month = atoi (args);
            R_setpromptf ("%s ", i18n (1540, "Enter new day of birth:"));
            return ++status;
        case 405:
            other.day = atoi (args);
            R_setpromptf ("%s ", i18n (1534, "Enter a language by number or L for a list:"));
            return ++status;
        case 406:
            temp = atoi (args);
            if ((0 == temp) && (toupper (args[0]) == 'L'))
            {
                TablePrintLang ();
            }
            else
            {
                other.lang1 = temp;
                status++;
            }
            R_setpromptf ("%s ", i18n (1534, "Enter a language by number or L for a list:"));
            return status;
        case 407:
            temp = atoi (args);
            if ((0 == temp) && (toupper (args[0]) == 'L'))
            {
                TablePrintLang ();
            }
            else
            {
                other.lang2 = temp;
                status++;
            }
            R_setpromptf ("%s ", i18n (1534, "Enter a language by number or L for a list:"));
            return status;
        case 408:
            temp = atoi (args);
            if ((0 == temp) && (toupper (args[0]) == 'L'))
            {
                TablePrintLang ();
                R_setpromptf ("%s ", i18n (1534, "Enter a language by number or L for a list:"));
                return status;
            }
            other.lang3 = temp;
            if (sess->ver > 6)
                SnacCliMetasetmore (sess, &other);
            else
                CmdPktCmdMetaMore (sess, &other);
            free (other.hp);
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
    SESSION;

    if (status > 100)
    {
        msg[offset] = 0;
        if (!strcmp (args, END_MSG_STR))
        {
            if (sess->ver > 6)
                SnacCliMetasetabout (sess, msg);
            else
                CmdPktCmdMetaAbout (sess, msg);
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
                if (sess->ver > 6)
                    SnacCliMetasetabout (sess, msg);
                else
                    CmdPktCmdMetaAbout (sess, msg);
                return 0;
            }
        }
    }
    else
    {
        if (args && strlen (args))
        {
            if (sess->ver > 6)
                SnacCliMetasetabout (sess, args);
            else
                CmdPktCmdMetaAbout (sess, args);
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
 * Process one line of command, get it if necessary.
 */
void CmdUserProcess (const char *command, time_t *idle_val, UBYTE *idle_flag)
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
                    M_print (i18n (1660, "Invalid Command: %s\n"), &buf[1]);
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
            if (!UtilUIParse (&args, &cmd))
            {
                if (!command)
                    R_resetprompt ();
                return;
            }

            /* skip all non-alphanumeric chars on the beginning
             * to accept IRC like commands starting with a /
             * or talker like commands starting with a .
             * or whatever */
            if (!isalnum (*cmd))
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
                
                cmd = strdup (cmd);
                if (!UtilUIParseRemainder (&args, &argsd))
                    argsd = "";
                argsd = strdup (argsd);

                if (*cmd != '�')
                    j = CmdUserLookup (cmd, CU_USER);
                if (!j)
                    j = CmdUserLookup (*cmd == '�' ? cmd + 1 : cmd, CU_DEFAULT);

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
                    M_print (i18n (1036, "Unknown command %s, type /help for help."), cmd);
                    M_print ("\n");
                }
                free (cmd);
                free (argsd);
            }
        }
    }
    if (!status && !uiG.quit && !command)
        R_resetprompt ();
}
