
/*
 * Handles commands the user sends.
 *
 * Copyright ?
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
    CmdUserAuto, CmdUserAlter, CmdUserMessage, CmdUserResend,
    CmdUserVerbose, CmdUserRandomSet, CmdUserIgnoreStatus,
    CmdUserStatusDetail, CmdUserStatusWide, CmdUserStatusShort,
    CmdUserStatusSelf, CmdUserSound, CmdUserSoundOnline, CmdUserRegister,
    CmdUserSoundOffline, CmdUserAutoaway, CmdUserSet, CmdUserClear,
    CmdUserTogIgnore, CmdUserTogVisible, CmdUserAdd, CmdUserRem, CmdUserRInfo,
    CmdUserAuth, CmdUserURL, CmdUserSave, CmdUserTabs, CmdUserLast,
    CmdUserUptime, CmdUserSearch, CmdUserWpSearch, CmdUserUpdate,
    CmdUserOther, CmdUserAbout, CmdUserQuit, CmdUserTCP, CmdUserConn;

static void CmdUserProcess (const char *command, int *idle_val, int *idle_flag);

static jump_t jump[] = {
    { &CmdUserRandom,        "rand",         NULL, 0,   0 },
    { &CmdUserRandomSet,     "setr",         NULL, 0,   0 },
    { &CmdUserHelp,          "help",         NULL, 0,   0 },
    { &CmdUserInfo,          "info",         NULL, 0,   0 },
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
    { &CmdUserStatusDetail,  "status",       NULL, 2,   0 },
    { &CmdUserStatusWide,    "wide",         NULL, 2,   1 },
    { &CmdUserStatusWide,    "ewide",        NULL, 2,   0 },
    { &CmdUserStatusShort,   "w",            NULL, 2,   1 },
    { &CmdUserStatusShort,   "e",            NULL, 2,   0 },
    { &CmdUserStatusSelf,    "s",            NULL, 0,   0 },
    { &CmdUserSet,           "set",          NULL, 0,   0 },
    { &CmdUserSound,         "sound",        NULL, 2,   0 },
    { &CmdUserSoundOnline,   "soundonline",  NULL, 2,   0 },
    { &CmdUserSoundOffline,  "soundoffline", NULL, 2,   0 },
    { &CmdUserAutoaway,      "autoaway",     NULL, 2,   0 },
    { &CmdUserChange,        "change",       NULL, 0,  -1 },
    { &CmdUserChange,        "online",       NULL, 1,   0 },
    { &CmdUserChange,        "away",         NULL, 1,   1 },
    { &CmdUserChange,        "na",           NULL, 1,   5 },
    { &CmdUserChange,        "occ",          NULL, 1,  17 },
    { &CmdUserChange,        "dnd",          NULL, 1,  19 },
    { &CmdUserChange,        "ffc",          NULL, 1,  32 },
    { &CmdUserChange,        "inv",          NULL, 1, 256 },
    { &CmdUserClear,         "clear",        NULL, 2,   0 },
    { &CmdUserTogIgnore,     "togig",        NULL, 0,   0 },
    { &CmdUserTogVisible,    "togvis",       NULL, 0,   0 },
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

    { &CmdUserSearch,        "search",       NULL, 0,   0 },
    { &CmdUserWpSearch,      "wpsearch",     NULL, 0,   0 },
    { &CmdUserUpdate,        "update",       NULL, 0,   0 },
    { &CmdUserOther,         "other",        NULL, 0,   0 },
    { &CmdUserAbout,         "about",        NULL, 0,   0 },
    { &CmdUserConn,          "conn",         NULL, 0,   0 },

    { NULL, NULL, NULL, 0 }
};

#define SESSION(type) Session *sess; sess = SessionFind (type, 0); \
    if (!sess) { M_print (i18n (906, "This command operates only on connections of type %s.\n"), #type); return 0; }

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
JUMP_F(CmdUserChange)
{
    char *arg1;
    SESSION(TYPE_SERVER_OLD | TYPE_SERVER);

    arg1 = strtok (args, " \n\r");
    if (data == -1)
    {
        if (arg1 == NULL)
        {
            M_print (i18n (703, COLCLIENT "Status modes: \n"));
            M_print ("  %-20s %d\n", i18n (921, "Online"),         STATUS_ONLINE);
            M_print ("  %-20s %d\n", i18n (923, "Away"),           STATUS_AWAY);
            M_print ("  %-20s %d\n", i18n (922, "Do not disturb"), STATUS_DND);
            M_print ("  %-20s %d\n", i18n (924, "Not Available"),  STATUS_NA);
            M_print ("  %-20s %d\n", i18n (927, "Free for chat"),  STATUS_FREE_CHAT);
            M_print ("  %-20s %d\n", i18n (925, "Occupied"),       STATUS_OCCUPIED);
            M_print ("  %-20s %d",   i18n (926, "Invisible"),      STATUS_INVISIBLE);
            M_print (COLNONE "\n");
            return 0;
        }
        data = atoi (arg1);
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
JUMP_F(CmdUserRandom)
{
    char *arg1;
    SESSION(TYPE_SERVER_OLD);
    
    arg1 = strtok (args, " \n\r");
    if (arg1 == NULL)
    {
        M_print (i18n (704, COLCLIENT "Groups: \n"));
        M_print (i18n (705, "General                    1\n"));
        M_print (i18n (706, "Romance                    2\n"));
        M_print (i18n (707, "Games                      3\n"));
        M_print (i18n (708, "Students                   4\n"));
        M_print (i18n (709, "20 something               6\n"));
        M_print (i18n (710, "30 something               7\n"));
        M_print (i18n (711, "40 something               8\n"));
        M_print (i18n (712, "50+                        9\n"));
        M_print (i18n (713, "Man chat requesting women 10\n"));
        M_print (i18n (714, "Woman chat requesting men 11\n"));
        M_print (i18n (715, "mICQ                      49 (might not work but try it)"));
        M_print (COLNONE "\n");
    }
    else
    {
        CmdPktCmdRandSearch (sess, atoi (arg1));
    }
    return 0;
}

/*
 * Sets the random user group.
 */
JUMP_F(CmdUserRandomSet)
{
    char *arg1;
    SESSION(TYPE_SERVER_OLD);
    
    arg1 = strtok (args, " \n\r");
    if (arg1 == NULL)
    {
        M_print (i18n (704, COLCLIENT "Groups: \n"));
        M_print (i18n (716, "None                      -1\n"));
        M_print (i18n (705, "General                    1\n"));
        M_print (i18n (706, "Romance                    2\n"));
        M_print (i18n (707, "Games                      3\n"));
        M_print (i18n (708, "Students                   4\n"));
        M_print (i18n (709, "20 something               6\n"));
        M_print (i18n (710, "30 something               7\n"));
        M_print (i18n (711, "40 something               8\n"));
        M_print (i18n (712, "50+                        9\n"));
        M_print (i18n (713, "Man chat requesting women 10\n"));
        M_print (i18n (714, "Woman chat requesting men 11\n"));
        M_print (i18n (715, "mICQ                      49 (might not work but try it)"));
        M_print (COLNONE "\n");
    }
    else
    {
        CmdPktCmdRandSet (sess, atoi (arg1));
/*      M_print ("\n" );*/
    }
    return 0;
}

/*
 * Displays help.
 */
JUMP_F(CmdUserHelp)
{
    char *arg1;
    arg1 = strtok (args, " \n\r");

    if (!arg1)
    {
        M_print (COLCLIENT "%s\n", i18n (442, "Please select one of the help topics below."));
        M_print ("%s\t-\t%s\n", i18n (447, "Client"),
                 i18n (443, "Commands relating to micq displays and configuration."));
        M_print ("%s\t-\t%s\n", i18n (448, "Message"),
                 i18n (446, "Commands relating to sending messages."));
        M_print ("%s\t-\t%s\n", i18n (449, "User"),
                 i18n (444, "Commands relating to finding other users."));
        M_print ("%s\t-\t%s\n", i18n (450, "Account"),
                 i18n (445, "Commands relating to your ICQ account."));
    }
    else if (!strcasecmp (arg1, i18n (447, "Client")))
    {
        M_print (COLMESS "verbose <nr>" COLNONE "\n\t\x1b«%s\x1b»\n",
                 i18n (418, "Set the verbosity level (default 0)."));
        M_print (COLMESS "%s" COLNONE "\n\t\x1b«%s\x1b»\n",
                 CmdUserLookupName ("clear"),
                 i18n (419, "Clears the screen."));
        M_print (COLMESS "%s" COLNONE "\n\t\x1b«%s\x1b»\n",
                 CmdUserLookupName ("sound"),
                 i18n (420, "Toggles beeping when recieving new messages."));
        M_print (COLMESS "%s" COLNONE "\n\t\x1b«%s\x1b»\n",
                 CmdUserLookupName ("autoaway"),
                 i18n (767, "Toggles auto cycling to away/not available."));
        M_print (COLMESS "%s" COLNONE "\n\t\x1b«%s\x1b»\n",
                 CmdUserLookupName ("color"),
                 i18n (421, "Toggles displaying colors."));
        M_print (COLMESS "%s" COLNONE "\n\t\x1b«%s\x1b»\n",
                 CmdUserLookupName ("q"),
                 i18n (422, "Logs off and quits."));
        M_print (COLMESS "%s" COLNONE "\n\t\x1b«%s\x1b»\n",
                 CmdUserLookupName ("auto"),
                 i18n (423, "Displays your autoreply status."));
        M_print (COLMESS "%s [on|off]" COLNONE "\n\t\x1b«%s\x1b»\n",
                 CmdUserLookupName ("auto"),
                 i18n (424, "Toggles sending messages when your status is DND, NA, etc."));
        M_print (COLMESS "%s <status> <message>" COLNONE "\n\t\x1b«%s\x1b»\n",
                 CmdUserLookupName ("auto"),
                 i18n (425, "Sets the message to send as an auto reply for the status."));
        M_print (COLMESS "%s <old cmd> <new cmd>" COLNONE "\n\t\x1b«%s\x1b»\n",
                 CmdUserLookupName ("alter"),
                 i18n (417, "This command allows you to alter your command set on the fly."));
        M_print (COLMESS "%s <lang>" COLNONE "\n\t\x1b«%s\x1b»\n", "trans", 
                 i18n (800, "Change the working language to <lang>."));  
        M_print ("  " COLCLIENT "\x1b«%s\x1b»" COLNONE "\n",
                 i18n (717, "! as the first character of a command will execute a shell command (e.g. \"!ls\"  \"!dir\" \"!mkdir temp\")"));
    }
    else if (!strcasecmp (arg1, i18n (448, "Message")))
    {
        M_print (COLMESS "%s <uin>" COLNONE "\n\t\x1b«%s\x1b»\n",
                 CmdUserLookupName ("auth"),
                 i18n (413, "Authorize uin to add you to their list."));
        M_print (COLMESS "%s <uin>/<message>" COLNONE "\n\t\x1b«%s\x1b»\n",
                 CmdUserLookupName ("msg"),
                 i18n (409, "Sends a message to uin."));
        M_print (COLMESS "%s <uin> <url> <message>" COLNONE "\n\t\x1b«%s\x1b»\n",
                 CmdUserLookupName ("url"),
                 i18n (410, "Sends a url and message to uin."));
        M_print (COLMESS "%s" COLNONE "\n\t\x1b«%s\x1b»\n",
                 CmdUserLookupName ("msga"),
                 i18n (411, "Sends a multiline message to everyone on your list."));
        M_print (COLMESS "%s <message>" COLNONE "\n\t\x1b«%s\x1b»\n",
                 CmdUserLookupName ("a"),
                 i18n (412, "Sends a message to the last person you sent a message to."));
        M_print (COLMESS "%s <message>" COLNONE "\n\t\x1b«%s\x1b»\n",
                 CmdUserLookupName ("r"),
                 i18n (414, "Replys to the last person to send you a message."));
        M_print (COLMESS "%s <nick>" COLNONE "\n\t\x1b«%s\x1b»\n", "last",
                 i18n (403, "Displays the last message received from <nick>, or a list of who has send you at least one message."));
        M_print (COLMESS "%s" COLNONE "\n\t\x1b«%s\x1b»\n", "tabs", 
                 i18n (98, "Display a list of nicknames that you can tab through.")); 
        M_print (COLMESS "%s <uin>" COLNONE "\n\t\x1b«%s\x1b»\n",
                 CmdUserLookupName ("resend"),
                 i18n (770, "Resend your last message to a new uin."));
        M_print (COLMESS "%s" COLNONE "\n\t\x1b«%s\x1b»\n", "uptime", 
                 i18n (719, "Shows how long mICQ has been running."));
        M_print ("  " COLCLIENT "\x1b«%s\x1b»" COLNONE "\n",
                 i18n (720, "uin can be either a number or the nickname of the user."));
        M_print ("  " COLCLIENT "\x1b«%s\x1b»" COLNONE "\n",
                 i18n (721, "Sending a blank message will put the client into multiline mode.\nUse . on a line by itself to end message.\nUse # on a line by itself to cancel the message."));
    }
    else if (!strcasecmp (arg1, i18n (449, "User")))
    {
        M_print (COLMESS "%s <nr>" COLNONE "\n\t\x1b«%s\x1b»\n",
                 CmdUserLookupName ("rand"),
                 i18n (415, "Finds a random user in the specified group or lists the groups."));
        M_print (COLMESS "pass <secret>" COLNONE "\n\t\x1b«%s\x1b»\n",
                 i18n (408, "Changes your password to secret."));
        M_print (COLMESS "%s" COLNONE "\n\t\x1b«%s\x1b»\n",
                 CmdUserLookupName ("w"),
                 i18n (416, "Displays the current status of everyone on your contact list."));
        M_print (COLMESS "%s <user>" COLNONE "\n\t\x1b«%s\x1b»\n",
                 CmdUserLookupName ("status"),
                 i18n (400, "Shows locally stored info on user."));
        M_print (COLMESS "%s" COLNONE "\n\t\x1b«%s\x1b»\n",
                 CmdUserLookupName ("s"),
                 i18n (769, "Displays your current online status."));
        M_print (COLMESS "%s" COLNONE "\n\t\x1b«%s\x1b»\n",
                 CmdUserLookupName ("e"),
                 i18n (407, "Displays the current status of online people on your contact list."));
        M_print (COLMESS "%s" COLNONE "\n\t\x1b«%s\x1b»\n", "wide", 
                 i18n (801, "Displays a list of people on your contact list in a screen wide format.")); 
        M_print (COLMESS "%s <uin>" COLNONE "\n\t\x1b«%s\x1b»\n",
                 CmdUserLookupName ("info"),
                 i18n (430, "Displays general info on uin."));
        M_print (COLMESS "%s <nick>" COLNONE "\n\t\x1b«%s\x1b»\n",
                 CmdUserLookupName ("togig"),
                 i18n (404, "Toggles ignoring/unignoring nick."));
        M_print (COLMESS "%s" COLNONE "\n\t\x1b«%s\x1b»\n",
                 CmdUserLookupName ("i"),
                 i18n (405, "Lists ignored nicks/uins."));
        M_print (COLMESS "%s <email>" COLNONE "\n\t\x1b«%s\x1b»\n",
                 CmdUserLookupName ("search"),
                 i18n (429, "Searches for a ICQ user."));
        M_print (COLMESS "%s <uin> <nick>" COLNONE "\n\t\x1b«%s\x1b»\n",
                 CmdUserLookupName ("add"),
                 i18n (428, "Adds the uin number to your contact list with nickname."));
        M_print (COLMESS "%s <nick>" COLNONE "\n\t\x1b«%s\x1b»\n",
                 CmdUserLookupName ("togvis"),
                 i18n (406, "Toggles your visibility to a user when you're invisible."));
    }
    else if (!strcasecmp (arg1, i18n (450, "Account")))
    {
        M_print (COLMESS "%s <status>" COLNONE "\n\t\x1b«%s\x1b»\n",
                 CmdUserLookupName ("change"),
                 i18n (427, "Changes your status to the status number. Without a number it lists the available modes."));
        M_print (COLMESS "reg <password>" COLNONE "\n\t\x1b«%s\x1b»\n",
                 i18n (426, "Creates a new UIN with the specified password."));
        M_print (COLMESS "%s|%s|%s|%s|%s|%s|%s" COLNONE "\n\t\x1b«%s\n%s\n%s\n%s\n%s\n%s\n%s\x1b»\n", 
                 CmdUserLookupName ("online"),
                 CmdUserLookupName ("away"),
                 CmdUserLookupName ("na"),
                 CmdUserLookupName ("occ"),
                 CmdUserLookupName ("dnd"),
                 CmdUserLookupName ("ffc"),
                 CmdUserLookupName ("inv"),
                 i18n (431, "Change status to Online."),
                 /*M_print (COLMESS "%s" COLNONE "\n\t\x1b«%s\x1b»\n", CmdUserLookupName ("online"), */ 
                 i18n (432, "Mark as Away."),
                 /*M_print (COLMESS "%s" COLNONE "\n\t\x1b«%s\x1b»\n", CmdUserLookupName ("away"), */
                 i18n (433, "Mark as Not Available."),
                 /*M_print (COLMESS "%s" COLNONE "\n\t\x1b«%s\x1b»\n", CmdUserLookupName ("na"), */
                 i18n (434, "Mark as Occupied."),
                 /*M_print (COLMESS "%s" COLNONE "\n\t\x1b«%s\x1b»\n", CmdUserLookupName ("dnd"), */
                 i18n (435, "Mark as Do not Disturb."),
                 /*        M_print (COLMESS "%s" COLNONE "\n\t\x1b«%s\x1b»\n", CmdUserLookupName ("ffc"), */ 
                 i18n (436, "Mark as Free for Chat."),
                 /*        M_print (COLMESS "%s" COLNONE "\n\t\x1b«%s\x1b»\n", CmdUserLookupName ("inv"), */
                 i18n (437, "Mark as Invisible."));
        M_print (COLMESS "%s" COLNONE "\n\t\x1b«%s\x1b»\n",
                 CmdUserLookupName ("update"),
                 i18n (438, "Updates your basic info (email, nickname, etc.)."));
        M_print (COLMESS "other" COLNONE "\n\t\x1b«%s\x1b»\n",
                 i18n (401, "Updates other user info like age and sex."));
        M_print (COLMESS "%s" COLNONE "\n\t\x1b«%s\x1b»\n",
                 CmdUserLookupName ("about"),
                 i18n (402, "Updates your about user info."));
        M_print (COLMESS "set <nr>" COLNONE "\n\t\x1b«%s\x1b»\n", 
                 i18n (439, "Sets your random user group."));
    }
    return 0;
}

/*
 * Queries basic info of a user
 */
JUMP_F(CmdUserInfo)
{
    char *arg1;
    UDWORD uin;
    SESSION(TYPE_SERVER | TYPE_SERVER_OLD);

    arg1 = strtok (args, "");
    if (arg1 == NULL)
    {
        M_print (i18n (194, "Need uin to ask for.\n"));
        return 0;
    }
    uin = ContactFindByNick (arg1);
    if (-1 == uin)
    {
        M_print (i18n (61, "%s not recognized as a nick name"), arg1);
        M_print ("\n");
        return 0;
    }
    M_print (i18n (723, "%s's IP address is "), arg1);
    Print_IP (uin);
    if ((UWORD) Get_Port (uin) != (UWORD) 0xffff)
    {
        M_print (i18n (673, "\tThe port is %d\n"), (UWORD) Get_Port (uin));
    }
    else
    {
        M_print (i18n (674, "\tThe port is unknown\n"));
    }
    M_print (i18n (765, "%s has UIN %d."), arg1, uin);
    M_print ("\n");
    if (sess->ver > 6)
        SnacCliMetareqinfo (sess, uin);
    else
        CmdPktCmdMetaReqInfo (sess, uin);
/*   send_ext_info_req( sok, uin );*/
    return 0;
}

/*
 * Gives information about internationalization and translates
 * strings by number.
 */
JUMP_F(CmdUserTrans)
{
    const char *arg1;

    arg1 = strtok (args, " \t");
    if (!arg1)
    {
        M_print (i18n (79, "No translation; using compiled-in strings.\n"));
        return 0;
    }
    for (;arg1; arg1 = strtok (NULL, " \t"))
    {
        char *p;
        int i;
        i = strtol (arg1, &p, 10);
        if (i || !*p)
        {
            if (*p)
                M_print ("%s\n", i18n (87, "Ignoring garbage after number."));
            M_print ("%3d:%s\n", i, i18n (78, "No translation available."));
        }
        else
        {
            if (!strcmp (arg1, "all"))
            {
                const char *p;
                int l = 0;
                for (i = 0; i - l < 100; i++)
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
                    p = strdup (i18n (89, "Unloaded translation."));
                    i18nClose ();
                    M_print ("%s\n", p);
                    free (p);
                    continue;
                }
                i = i18nOpen (arg1);
                if (i == -1)
                    M_print (i18n (80, "Couldn't load \"%s\" internationalization.\n"), arg1);
                else if (i)
                    M_print (i18n (81, "Successfully loaded en translation (%d entries).\n"), i);
                else
                    M_print ("No internationalization requested.\n");
            } 
        }
    }
    return 0;
}

/*
 * Manually handles peer-to-peer (TCP) connections.
 */
JUMP_F(CmdUserTCP)
{
#ifdef TCP_COMM
    char *cmd, *nick;
    UDWORD uin;

    cmd = strtok (args, " \t\n");
    if (cmd)
    {
        nick = strtok (NULL, "");
        if (!nick)
        {
            M_print (i18n (916, "Need a nick or uin.\n"));
            return 0;
        }
        uin = ContactFindByNick (nick);
        if (uin == -1)
        {
            M_print (i18n (845, "Nick %s unknown.\n"), nick);
            return 0;
        }
        if (!strcmp (cmd, "open"))
        {
            SESSION(TYPE_PEER);
            TCPDirectOpen  (sess, uin);
        }
        else if (!strcmp (cmd, "close"))
            TCPDirectClose (      uin);
        else if (!strcmp (cmd, "reset"))
            TCPDirectClose (      uin);
        else if (!strcmp (cmd, "off"))
            TCPDirectOff   (      uin);
    }
    else
    {
        M_print (i18n (846, "Opens and closes TCP connections:\n"));
        M_print (i18n (847, "    open  <nick> - Opens TCP connection.\n"));
        M_print (i18n (848, "    close <nick> - Closes/resets TCP connection(s).\n"));
        M_print (i18n (870, "    off   <nick> - Closes TCP connection(s) and don't try TCP again.\n"));
    }
#else
    M_print (i18n (866, "This version of micq is compiled without TCP support.\n"));
#endif
    return 0;
}

/*
 * Changes automatic reply messages.
 */
JUMP_F(CmdUserAuto)
{
    char *cmd;
    char *arg1;

    cmd = strtok (args, "");
    if (cmd == NULL)
    {
        M_print (i18n (724, "Automatic replies are %s.\n"),
                 prG->flags & FLAG_AUTOREPLY ? i18n (85, "on") : i18n (86, "off"));
        M_print ("%30s %s\n", i18n (727, "The Do not disturb message is:"), prG->auto_dnd);
        M_print ("%30s %s\n", i18n (728, "The Away message is:"),           prG->auto_away);
        M_print ("%30s %s\n", i18n (729, "The Not available message is:"),  prG->auto_na);
        M_print ("%30s %s\n", i18n (730, "The Occupied message is:"),       prG->auto_occ);
        M_print ("%30s %s\n", i18n (731, "The Invisible message is:"),      prG->auto_inv);
        return 0;
    }
    else if (strcasecmp (cmd, "on") == 0)
    {
        prG->flags |= FLAG_AUTOREPLY;
        M_print (i18n (724, "Automatic replies are %s.\n"), i18n (85, "on"));
    }
    else if (strcasecmp (cmd, "off") == 0)
    {
        prG->flags &= ~FLAG_AUTOREPLY;
        M_print (i18n (724, "Automatic replies are %s.\n"), i18n (86, "off"));
    }
    else
    {
        arg1 = strtok (cmd, " ");
        if (arg1 == NULL)
        {
            M_print (i18n (734, "Sorry wrong syntax, can't find a status somewhere.\r\n"));
            return 0;
        }
        if (!strcasecmp (arg1, CmdUserLookupName ("dnd")))
        {
            cmd = strtok (NULL, "");
            if (cmd == NULL)
            {
                M_print (i18n (735, "Must give a message.\n"));
                return 0;
            }
            prG->auto_dnd = strdup (cmd);
        }
        else if (!strcasecmp (arg1, CmdUserLookupName ("away")))
        {
            cmd = strtok (NULL, "");
            if (cmd == NULL)
            {
                M_print (i18n (735, "Must give a message.\n"));
                return 0;
            }
            prG->auto_away = strdup (cmd);
        }
        else if (!strcasecmp (arg1, CmdUserLookupName ("na")))
        {
            cmd = strtok (NULL, "");
            if (cmd == NULL)
            {
                M_print (i18n (735, "Must give a message.\n"));
                return 0;
            }
            prG->auto_na = strdup (cmd);
        }
        else if (!strcasecmp (arg1, CmdUserLookupName ("occ")))
        {
            cmd = strtok (NULL, "");
            if (cmd == NULL)
            {
                M_print (i18n (735, "Must give a message.\n"));
                return 0;
            }
            prG->auto_occ = strdup (cmd);
        }
        else if (!strcasecmp (arg1, CmdUserLookupName ("inv")))
        {
            cmd = strtok (NULL, "");
            if (cmd == NULL)
            {
                M_print (i18n (735, "Must give a message.\n"));
                return 0;
            }
            prG->auto_inv = strdup (cmd);
        }
        else
            M_print (i18n (736, "Sorry wrong syntax. Read tha help man!\n"));
        M_print (i18n (737, "Automatic reply setting\n"));
    }
    return 0;
}

/*
 * Relabels commands.
 */
JUMP_F(CmdUserAlter)
{
    char *cmd;
    jump_t *j;
    int quiet = 0;

    cmd = strtok (args, " ");

    if (cmd && !strcasecmp ("quiet", cmd))
    {
        quiet = 1;
        cmd = strtok (NULL, " ");
    }
        
    if (cmd == NULL)
    {
        M_print (i18n (738, "Need a command to alter!\n"));
        return 0;
    }
    
    j = CmdUserLookup (cmd, CU_DEFAULT);
    if (!j)
        j = CmdUserLookup (cmd, CU_USER);
    if (!j)
    {
        M_print (i18n (722, "Type help to see your current command, because this one you typed wasn't one!"));
        M_print ("\n");
    }
    else
    {
        char *new = strtok (NULL, " ");
        
        if (new)
        {
            if (CmdUserLookup (new, CU_USER))
            {
                if (!quiet)
                {
                    M_print (i18n (768, "The label '%s' is already being used."), new);
                    M_print ("\n");
                }
                return 0;
            }
            else
            {
                if (j->name)
                    free ((char *) j->name);
                j->name = strdup (new);
            }
        }
        
        if (!quiet)
        {
            if (j->name)
                M_print (i18n (763, "The command '%s' has been renamed to '%s'."), j->defname, j->name);
            else
                M_print (i18n (764, "The command '%s' is unchanged."), j->defname);
            M_print ("\n");
        }
    }
    return 0;
}

/*
 * Resend your last message
 */
JUMP_F (CmdUserResend)
{
    UDWORD uin;
    char *arg1;
    SESSION(TYPE_SERVER_OLD);

    arg1 = strtok (args, UIN_DELIMS);
    if (!uiG.last_message_sent) 
    {
        M_print (i18n (771, "You haven't sent a message to anyone yet!\n"));
        return 0;
    }
    if (!arg1)
    {
        M_print (i18n (676, "Need uin to send to.\n"));
        return 0;
    }
    uin = ContactFindByNick (arg1);
    if (uin == -1)
    {
        M_print (i18n (61, "%s not recognized as a nick name"), arg1);
        M_print ("\n");
        return 0;
    }
    icq_sendmsg (sess, uin, uiG.last_message_sent, uiG.last_message_sent_type);
    uiG.last_sent_uin = uin;
    return 0;
}

/*
 * Send an instant message.
 */
JUMP_F (CmdUserMessage)
{
    UDWORD uin;
    static UDWORD multi_uin;
    static int offset = 0;
    static char msg[1024];
    char *arg1, *p;
    int len;
    Contact *cont;
    SESSION(TYPE_SERVER_OLD | TYPE_SERVER);

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
                    icq_sendmsg (sess, cont->uin, temp, MRNORM_MESS);
                    free (temp);
                }
            }
            else
            {
                icq_sendmsg (sess, multi_uin, msg, NORM_MESS);
                uiG.last_sent_uin = multi_uin;
            }
            return 0;
        }
        else if (strcmp (arg1, CANCEL_MSG_STR) == 0)
        {
            M_print (i18n (38, "Message canceled\n"));
            return 0;
        }
        else
        {
            if (offset + strlen (arg1) < 450)
            {
                strcat (msg, arg1);
                strcat (msg, "\r\n");
                offset += strlen (arg1) + 2;
            }
            else
            {
                M_print (i18n (37, "Message sent before last line buffer is full\n"));
                if (multi_uin == -1)
                {
                    char *temp;
                    
                    for (cont = ContactStart (); ContactHasNext (cont); cont = ContactNext (cont))
                    {
                        temp = strdup (msg);
                        icq_sendmsg (sess, cont->uin, temp, MRNORM_MESS);
                        free (temp);
                    }
                }
                else
                {
                    icq_sendmsg (sess, multi_uin, msg, NORM_MESS);
                    uiG.last_sent_uin = multi_uin;
                }
                return 0;
            }
        }
    }
    else
    {
        switch (data)
        {
            case 1:
                arg1 = strtok (p = strdup (args), UIN_DELIMS);
                if (!arg1)
                {
                    M_print (i18n (676, "Need uin to send to.\n"));
                    return 0;
                }
                uin = ContactFindByNick (arg1);
                len = 0;
                while (uin == -1)
                {
                    len += strlen (arg1);
                    free (p);
                    p = strdup (args);
                    arg1 = strtok (p + len + 1, UIN_DELIMS);
                    if (!arg1)
                    {
                        
                        M_print (i18n (61, "%s not recognized as a nick name"), strtok (args, UIN_DELIMS));
                        M_print ("\n");
                        return 0;
                    }
                    uin = ContactFindByNick (p);
                }
                arg1 = strtok (NULL, "");
                break;
            case 2:
                if (!uiG.last_rcvd_uin)
                {
                    M_print (i18n (741, "Must receive a message first\n"));
                    return 0;
                }
                uin = uiG.last_rcvd_uin;
                arg1 = strtok (args, "");
                break;
            case 4:
                if (!uiG.last_sent_uin)
                {
                    M_print (i18n (742, "Must write one message first\n"));
                    return 0;
                }
                uin = uiG.last_sent_uin;
                arg1 = strtok (args, "");
                break;
            case 8:
                uin = -1;
                arg1 = strtok (args, "");
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
                    icq_sendmsg (sess, cont->uin, temp, MRNORM_MESS);
                    free (temp);
                }
            }
            else
            {
                icq_sendmsg (sess, uin, arg1, NORM_MESS);
                uiG.last_sent_uin = uin;
            }
            return 0;
        }
        multi_uin = uin;
        if (uin == -1)
            M_print (i18n (664, "Composing message to " COLCONTACT "all" COLNONE ":\n"));
        else if (ContactFindNick (uin))
            M_print (i18n (739, "Composing message to " COLCONTACT "%s" COLNONE ":\n"), ContactFindNick (uin));
        else
            M_print (i18n (740, "Composing message to " COLCLIENT "%d" COLNONE ":\n"), uin);
        offset = 0;
        status = data;
    }
    if (status == 8)
        R_doprompt (i18n (42, "msg all> "));
    else
        R_doprompt (i18n (41, "msg> "));
    return status;
}

/*
 * Changes verbosity.
 */
JUMP_F(CmdUserVerbose)
{
    char *arg1;

    arg1 = strtok (args, "");
    if (arg1 != NULL)
    {
        prG->verbose = atoi (arg1);
    }
    M_print (i18n (60, "Verbosity level is %d.\n"), prG->verbose);
    return 0;
}

/*
 * Shows the contact list in a very detailed way.
 */
JUMP_F(CmdUserStatusDetail)
{
    UDWORD num;
    Contact *cont;
    char *name = strtok (args, "");
    SESSION(TYPE_SERVER_OLD | TYPE_SERVER);

    if (name)
    {
        num = ContactFindByNick (name);
        if (num == -1)
        {
            M_print (i18n (699, "Must give a valid uin/nickname\n"));
            return 0;
        }
        cont = ContactFind (num);
        if (cont == NULL)
        {
            M_print (i18n (700, "%s is not a valid user in your list.\n"), name);
            return 0;
        }
        if (cont->vis_list)
        {
            M_print ("%s*%s", COLSERV, COLNONE);
        }
        else if (cont->invis_list)
        {
            M_print (COLSERV "-" COLNONE);
        }
        else
        {
            M_print (" ");
        }
        M_print ("%6ld=", cont->uin);
        M_print (COLCONTACT "%-20s\t%s(", cont->nick, COLMESS);
        Print_Status (cont->status);
        M_print (")%s\n", COLNONE);
        if (cont->status == STATUS_OFFLINE)
        {
            if (-1L != cont->last_time)
            {
                M_print (i18n (69, " Last online at %s"), ctime ((time_t *) & cont->last_time));
            }
            else
            {
                M_print (i18n (70, " Last on-line unknown."));
                M_print ("\n");
            }
        }
        else
        {
            if (-1L != cont->last_time)
            {
                M_print (i18n (68, " Online since %s"), ctime ((time_t *) & cont->last_time));
            }
            else
            {
                M_print (i18n (70, " Last on-line unknown."));
                M_print ("\n");
            }
        }
        M_print ("%-15s %s:%d\n", i18n (441, "IP:"), UtilIOIP (cont->outside_ip), cont->port);
        M_print ("%-15s %d\n", i18n (453, "TCP version:"), cont->TCP_version);
        M_print ("%-15s %s\n", i18n (454, "Connection:"),
                 cont->connection_type == 4 ? i18n (493, "Peer-to-Peer") : i18n (494, "Server Only"));
        return 0;
    }
    M_print (W_SEPERATOR);
    Time_Stamp ();
    M_print (" ");
    M_print (i18n (71, "Your status is "));
    Print_Status (sess->status);
    M_print ("\n");
    /*  First loop sorts thru all offline users */
    M_print ("%s%s\n", W_SEPERATOR, i18n (72, "Users offline:"));
    for (cont = ContactStart (); ContactHasNext (cont); cont = ContactNext (cont))
    {
        if ((SDWORD) cont->uin > 0)
        {
            if (!cont->invis_list)
            {
                if (cont->status == STATUS_OFFLINE)
                {
                    if (cont->vis_list)
                    {
                        M_print ("%s*%s", COLSERV, COLNONE);
                    }
                    else
                    {
                        M_print (" ");
                    }
                    M_print ("%8ld=", cont->uin);
                    M_print (COLCONTACT "%-20s\t%s(", cont->nick, COLMESS);
                    Print_Status (cont->status);
                    M_print (")" COLNONE);
                    if (-1L != cont->last_time)
                    {
                        M_print (i18n (69, " Last online at %s"),
                                 ctime ((time_t *) & cont->last_time));
                    }
                    else
                    {
                        M_print (i18n (70, " Last on-line unknown."));
                        M_print ("\n");
                        /* if time is unknow they can't be logged on cause we */
                        /* set the time at login */
                    }
                }
            }
        }
    }
    /* The second loop displays all the online users */
    M_print ("%s%s\n", W_SEPERATOR, i18n (73, "Users online:"));
    for (cont = ContactStart (); ContactHasNext (cont); cont = ContactNext (cont))
    {
        if ((SDWORD) cont->uin > 0)
        {
            if (FALSE == cont->invis_list)
            {
                if (cont->status != STATUS_OFFLINE)
                {
                    if (cont->vis_list)
                    {
                        M_print ("%s*%s", COLSERV, COLNONE);
                    }
                    else
                    {
                        M_print (" ");
                    }
                    M_print ("%8ld=", cont->uin);
                    M_print (COLCONTACT "%-20s\t%s(", cont->nick, COLMESS);
                    Print_Status (cont->status);
                    M_print (")" COLNONE);
                    if (-1L != cont->last_time)
                    {
                        if (cont->status == STATUS_OFFLINE)
                            M_print (i18n (69, " Last online at %s"),
                                     ctime ((time_t *) & cont->last_time));
                        else
                            M_print (i18n (68, " Online since %s"),
                                     ctime ((time_t *) & cont->last_time));
                    }
                    else
                    {
                        M_print (i18n (70, " Last on-line unknown."));
                        M_print ("\n");
                        /* if time is unknow they can't be logged on cause we */
                        /* set the time at login */
                    }
                }
            }
        }
    }
    M_print (W_SEPERATOR);
    return 0;
}

/*
 * Shows the ignored user on the contact list.
 */
JUMP_F(CmdUserIgnoreStatus)
{
    Contact *cont;

    M_print ("%s%s\n", W_SEPERATOR, i18n (62, "Users ignored:"));
    /*  Sorts thru all ignored users */
    for (cont = ContactStart (); ContactHasNext (cont); cont = ContactNext (cont))
    {
        if ((SDWORD) cont->uin > 0)
        {
            if (TRUE == cont->invis_list)
            {
                if (cont->vis_list)
                {
                    M_print (COLSERV "*" COLNONE);
                }
                else
                {
                    M_print (" ");
                }
                M_print (COLCONTACT "%-20s\t" COLMESS "(", cont->nick);
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
JUMP_F(CmdUserStatusWide)
{
    Contact **Online;           /* definitely won't need more; could    */
    Contact **Offline = NULL;   /* probably get away with less.    */
    int MaxLen = 0;             /* legnth of longest contact name */
    int i;
    int OnIdx = 0;              /* for inserting and tells us how many there are */
    int OffIdx = 0;             /* for inserting and tells us how many there are */
    int NumCols;                /* number of columns to display on screen        */
    Contact *cont = NULL;

    if (data)
    {
        if ((Offline = (Contact **) malloc (MAX_CONTACTS * sizeof (Contact *))) == NULL)
        {
            M_print (i18n (652, "Insuffificient memory to display a wide Contact List.\n"));
            return 0;
        }
    }

    if ((Online = (Contact **) malloc (MAX_CONTACTS * sizeof (Contact *))) == NULL)
    {
        M_print (i18n (652, "Insuffificient memory to display a wide Contact List.\n"));
        return 0;
    }

    /* Filter the contact list into two lists -- online and offline. Also
       find the longest name in the list -- this is used to determine how
       many columns will fit on the screen.
    for (cont = ContactStart (); ContactHasNext (cont); cont = ContactNext (cont))                                */
    {
        if ((SDWORD) cont->uin > 0)
        {                       /* Aliases */
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
        M_print (COLMESS);
        for (i = 0; i < (Get_Max_Screen_Width () - strlen (i18n (653, "Offline"))) / 2; i++)
        {
            M_print ("=");
        }
        M_print (COLCLIENT "%s" COLMESS, i18n (653, "Offline"));
        for (i += strlen (i18n (653, "Offline")); i < Get_Max_Screen_Width (); i++)
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
       is needed.                                                            */
    M_print (COLMESS);
    for (i = 0; i < (Get_Max_Screen_Width () - strlen (i18n (654, "Online"))) / 2; i++)
    {
        M_print ("=");
    }
    M_print (COLCLIENT "%s" COLMESS, i18n (654, "Online"));
    for (i += strlen (i18n (654, "Online")); i < Get_Max_Screen_Width (); i++)
    {
        M_print ("=");
    }
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
    M_print (COLMESS);
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
 * Display your personal online status
 */
JUMP_F(CmdUserStatusSelf)
{
    SESSION(TYPE_SERVER_OLD | TYPE_SERVER);
    
    M_print (W_SEPERATOR);
    Time_Stamp ();
    M_print (" " MAGENTA BOLD "%10lu" COLNONE " ", sess->uin);
    M_print (i18n (71, "Your status is "));
    Print_Status (sess->status);
    M_print ("\n");
    M_print (W_SEPERATOR);
    return 0;
}

/*
 * Display offline and online users on your contact list.
 */
JUMP_F(CmdUserStatusShort)
{
    Contact *cont;
    SESSION(TYPE_SERVER_OLD | TYPE_SERVER);

    M_print (W_SEPERATOR);
    Time_Stamp ();
    M_print (" " MAGENTA BOLD "%10lu" COLNONE " ", sess->uin);
    M_print (i18n (71, "Your status is "));
    Print_Status (sess->status);
    M_print ("\n");

    if (data)
    {
        M_print ("%s%s\n", W_SEPERATOR, i18n (72, "Users offline:"));
        for (cont = ContactStart (); ContactHasNext (cont); cont = ContactNext (cont))
        {
            if ((SDWORD) cont->uin > 0)
            {
                if (FALSE == cont->invis_list)
                {
                    if (cont->status == STATUS_OFFLINE)
                    {
                        if (cont->vis_list)
                        {
                            M_print (COLSERV "*" COLNONE);
                        }
                        else
                        {
                            M_print (" ");
                        }
                        M_print (COLCONTACT "%-20s\t" COLMESS "(", cont->nick);
                        Print_Status (cont->status);
                        M_print (")" COLNONE "\n");
                    }
                }
            }
        }
    }

    M_print ("%s%s\n", W_SEPERATOR, i18n (73, "Users online:"));
    for (cont = ContactStart (); ContactHasNext (cont); cont = ContactNext (cont))
    {
        if ((SDWORD) cont->uin > 0)
        {
            if (!cont->invis_list)
            {
                if (cont->status != STATUS_OFFLINE)
                {
                    if (cont->vis_list)
                    {
                        M_print (COLSERV "*" COLNONE);
                    }
                    else
                    {
                        M_print (" ");
                    }
                    M_print (COLCONTACT "%-20s\t" COLMESS "(", cont->nick);
                    Print_Status (cont->status);
                    M_print (")" COLNONE);
                    if (cont->version)
                       M_print (" [%s]", cont->version);
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
JUMP_F(CmdUserSound)
{
    char *arg1;
    
    if ((arg1 = strtok (args, "")))
    {
        prG->sound &= ~SFLAG_BEEP & ~SFLAG_CMD;
        if (!strcasecmp (arg1, i18n (85, "on")))
           prG->sound |= SFLAG_BEEP;
        else if (!strcasecmp (arg1, i18n (86, "off"))) ;
        else
        {
           prG->sound |= SFLAG_CMD;
           prG->sound_cmd = strdup (arg1);
        }
    }
    if (prG->sound & SFLAG_BEEP)
        M_print ("%s " COLSERV "%s" COLNONE ".\n", i18n (84, "Sound"), i18n (85, "on"));
    else if (prG->sound & SFLAG_CMD)
        M_print ("%s " COLSERV "%s" COLNONE ".\n", i18n (83, "Sound cmd"), prG->sound_cmd);
    else
        M_print ("%s " COLSERV "%s" COLNONE ".\n", i18n (84, "Sound"), i18n (86, "off"));
    return 0;
}

/*
 * Toggles soundonline or changes soundonline command.
 */
JUMP_F(CmdUserSoundOnline)
{
    char *arg1;
    
    if ((arg1 = strtok (args, "")))
    {
        prG->sound &= ~SFLAG_ON_BEEP & ~SFLAG_ON_CMD;
        if (!strcasecmp (arg1, i18n (85, "on")))
           prG->sound |= SFLAG_ON_BEEP;
        else if (!strcasecmp (arg1, i18n (86, "off"))) ;
        else
        {
           prG->sound |= SFLAG_ON_CMD;
           prG->sound_on_cmd = strdup (arg1);
        }
    }
    if (prG->sound & SFLAG_ON_BEEP)
        M_print ("%s " COLSERV "%s" COLNONE ".\n", i18n (804, "SoundOnline"), i18n (85, "on"));
    else if (prG->sound & SFLAG_ON_CMD)
        M_print ("%s " COLSERV "%s" COLNONE ".\n", i18n (802, "SoundOnline cmd"), prG->sound_on_cmd);
    else
        M_print ("%s " COLSERV "%s" COLNONE ".\n", i18n (804, "SoundOnline"), i18n (86, "off"));
    return 0;
}

/*
 * Toggles soundoffine or changes soundoffline command.
 */
JUMP_F(CmdUserSoundOffline)
{
    char *arg1;
    
    if ((arg1 = strtok (args, "")))
    {
        prG->sound &= ~SFLAG_OFF_BEEP & ~SFLAG_OFF_CMD;
        if (!strcasecmp (arg1, i18n (85, "on")))
           prG->sound |= SFLAG_OFF_BEEP;
        else if (!strcasecmp (arg1, i18n (86, "off"))) ;
        else
        {
           prG->sound |= SFLAG_OFF_CMD;
           prG->sound_off_cmd = strdup (arg1);
        }
    }
    if (prG->sound & SFLAG_OFF_BEEP)
        M_print ("%s " COLSERV "%s" COLNONE ".\n", i18n (805, "SoundOffline"), i18n (85, "on"));
    else if (prG->sound & SFLAG_OFF_CMD)
        M_print ("%s " COLSERV "%s" COLNONE ".\n", i18n (803, "SoundOffline cmd"), prG->sound_off_cmd);
    else
        M_print ("%s " COLSERV "%s" COLNONE ".\n", i18n (805, "SoundOffline"), i18n (86, "off"));
    return 0;
}

/*
 * Toggles autoaway or sets autoaway time.
 */
JUMP_F(CmdUserAutoaway) 
{
    char *arg1;
    if ((arg1 = strtok (args, ""))) /* assign a value */
    {
        if (prG->away_time == 0 || uiG.away_time_prev == 0 || atoi (arg1) == 0)
        {
            uiG.away_time_prev = prG->away_time;
            prG->away_time = atoi (arg1);
        }
        else
        {
            prG->away_time = atoi (arg1);
        }
    }
    else                            /* toggle */
    {
        if (prG->away_time == 0 && uiG.away_time_prev == 0)
        {
            prG->away_time = default_away_time;
        }
        else if (prG->away_time == 0)
        {
            prG->away_time = uiG.away_time_prev;
            uiG.away_time_prev = 0;
        }
        else
        {
            uiG.away_time_prev = prG->away_time;
            prG->away_time = 0;
        }
    }
    M_print (i18n (766, "Auto_away is " COLMESS "%d" COLNONE ".\n"), prG->away_time);
    return 0;
}

/*
 * Toggles simple options.
 */
JUMP_F(CmdUserSet)
{
    int quiet = 0;
    char *arg1;
    
    arg1 = strtok (args, " \t");
    
    if (arg1 && !strcmp (arg1, "quiet"))
    {
        quiet = 1;
        arg1 = strtok (NULL, " \t");
    }
    
    if (!arg1 || !strcmp (arg1, "help") || !strcmp (arg1, "?"))
    {
        M_print (i18n (820, "%s <option> [on|off] - control simple options.\n"), CmdUserLookupName ("set"));
        M_print (i18n (822, "    color: use colored text output.\n"));
        M_print (i18n (815, "    funny: use funny messages for output.\n"));
    }
    else if (!strcmp (arg1, "color"))
    {
        arg1 = strtok (NULL, "\n");
        if (arg1)
        {
            if (!strcmp (arg1, "on") || !strcmp (arg1, i18n (85, "on")))
            {
                prG->flags |= FLAG_COLOR;
            }
            else if (!strcmp (arg1, "off") || !strcmp (arg1, i18n (86, "off")))
            {
                prG->flags &= ~FLAG_COLOR;
            }
        }

        if (!quiet)
        {
            if (prG->flags & FLAG_COLOR)
                M_print (i18n (195, "Color is " COLMESS "%s" COLNONE ".\n"), i18n (85, "on"));
            else
                M_print (i18n (195, "Color is " COLMESS "%s" COLNONE ".\n"), i18n (86, "off"));
        }
    }
    else if (!strcmp (arg1, "funny"))
    {
        arg1 = strtok (NULL, "\n");
        if (arg1)
        {
            if (!strcmp (arg1, "on") || !strcmp (arg1, i18n (85, "on")))
            {
                prG->flags |= FLAG_FUNNY;
            }
            else if (!strcmp (arg1, "off") || !strcmp (arg1, i18n (86, "off")))
            {
                prG->flags &= ~FLAG_FUNNY;
            }
        }
        
        if (!quiet)
        {
            if (prG->flags & FLAG_FUNNY)
                M_print (i18n (821, "Funny messages are " COLMESS "%s" COLNONE ".\n"), i18n (85, "on"));
            else
                M_print (i18n (821, "Funny messages are " COLMESS "%s" COLNONE ".\n"), i18n (86, "off"));
        }
    }
    else
        M_print (i18n (816, "Unknown option %s to set command.\n"), arg1);
    return 0;
}

/*
 * Clears the screen.
 */
JUMP_F(CmdUserClear)
{
    clrscr ();
    return 0;
}


/*
 * Registers a new user.
 */
JUMP_F(CmdUserRegister)
{
    char *arg1;
    
    arg1 = strtok (args, "");
    if (arg1)
    {
        Session *sess;
        if ((sess = SessionFind (TYPE_SERVER, 0)))
            SrvRegisterUIN (sess, arg1);
        else if ((sess = SessionFind (TYPE_SERVER_OLD, 0)))
            CmdPktCmdRegNewUser (sess, arg1);     /* TODO */
        else
            SrvRegisterUIN (NULL, arg1);
    }
    return 0;
}

/*
 * Toggles ignoring a user.
 */
JUMP_F(CmdUserTogIgnore)
{
    char *arg1;
    Contact *bud;
    UDWORD uin;
    SESSION(TYPE_SERVER_OLD | TYPE_SERVER);

    arg1 = strtok (args, "\n");
    if (!arg1)
    {
        M_print (i18n (668, "You must specify a nick name."));
        M_print ("\n");
        return 0;
    }

    uin = ContactFindByNick (arg1);
    if (uin == -1)
    {
        M_print (i18n (665, "%s not recognized as a nick name."), arg1);
        M_print ("\n");
        return 0;
    }

    bud =  ContactFind (uin);
    if (!bud)
    {
        M_print (i18n (90, "%s is a UIN, not a nick name."), arg1);
        M_print ("\n");
        return 0;
    }

    if (bud->invis_list == TRUE)
    {
        bud->invis_list = FALSE;
        if (sess->ver > 6)
            SnacCliReminvis (sess, uin);
        else
            CmdPktCmdUpdateList (sess, uin, INV_LIST_UPDATE, FALSE);
        M_print (i18n (666, "Unignored %s."), bud->nick);
    }
    else
    {
        bud->vis_list = FALSE;
        bud->invis_list = TRUE;
        if (sess->ver > 6)
            SnacCliAddinvis (sess, uin);
        else
            CmdPktCmdUpdateList (sess, uin, INV_LIST_UPDATE, TRUE);
        M_print (i18n (667, "Ignoring %s."), bud->nick);
    }
    if (sess->ver < 6)
    {
        CmdPktCmdContactList (sess);
        CmdPktCmdInvisList (sess);
        CmdPktCmdVisList (sess);
        CmdPktCmdStatusChange (sess, sess->status);
    }
    M_print ("\n");
/*    Time_Stamp ();
    M_print (" ");
    Print_Status (sess->status);
    M_print ("\n"); */
    return 0;
}

/*
 * Toggles visibility to a user.
 */
JUMP_F(CmdUserTogVisible)
{
    char *arg1;
    Contact *bud;
    UDWORD uin;
    SESSION(TYPE_SERVER_OLD | TYPE_SERVER);

    arg1 = strtok (args, " \n");
    if (!arg1)
    {
        M_print (i18n (668, "You must specify a nick name."));
        M_print ("\n");
        return 0;
    }

    uin = ContactFindByNick (arg1);
    if (uin == -1)
    {
        M_print (i18n (665, "%s not recognized as a nick name."), arg1);
        M_print ("\n");
        return 0;
    }

    bud =  ContactFind (uin);
    if (!bud)
    {
        M_print (i18n (90, "%s is a UIN, not a nick name."), arg1);
        M_print ("\n");
        return 0;
    }

    if (bud->vis_list == TRUE)
    {
        bud->vis_list = FALSE;
        if (sess->ver > 6)
            SnacCliRemvisible (sess, uin);
        else
            CmdPktCmdUpdateList (sess, uin, VIS_LIST_UPDATE, FALSE);
        M_print (i18n (670, "Invisible to %s now."), ContactFindNick (uin));
    }
    else
    {
        bud->vis_list = TRUE;
        bud->invis_list = FALSE;
        if (sess-> ver > 6)
            SnacCliAddvisible (sess, uin);
        else
            CmdPktCmdUpdateList (sess, uin, VIS_LIST_UPDATE, TRUE);
        M_print (i18n (671, "Visible to %s now."), ContactFindNick (uin));
    }

    M_print ("\n");
    return 0;
}

/*
 * Add a user to your contact list.
 */
JUMP_F(CmdUserAdd)
{
    Contact *cont;
    char *arg1, *arg2;
    UDWORD uin;
    SESSION(TYPE_SERVER_OLD | TYPE_SERVER);

    arg1 = strtok (args, " \t");
    if (!arg1)
    {
        M_print (i18n (753, "You must specify UIN and new nick name.\n"));
        return 0;
    }
    
    uin = atoi (arg1);
    arg2 = strtok (NULL, "");
    if (!uin)
    {
        M_print (i18n (748, "No UIN given.\n"));
        return 0;
    }

    if ((cont = ContactFind (uin)))
    {
        if (!arg2)
            arg2 = ContactFindName (uin);
        if (cont->not_in_list)
        {
            M_print (i18n (669, "%s added.\n"), arg1);
            M_print (i18n (754, " Note: You need to 'save' to write new contact list to disc.\n"));
        }
        else
        {
            M_print (i18n (749, "UIN %d renamed to %s.\n"), uin, arg2);
            M_print (i18n (754, " Note: You need to 'save' to write new contact list to disc.\n"));
        }
        cont->not_in_list = 0;
        strncpy (cont->nick, arg2, 18);
        cont->nick[19] = '\0';
    }
    else
    {
        if (!arg2)
            arg2 = ContactFindName (uin);
        if (Add_User (sess, uin, arg2))
            M_print (i18n (669, "%s added.\n"), arg2);
        else
            M_print (i18n (773, "%s could not be added.\n"), arg2);
        ContactAdd (uin, arg2);
        if (sess->ver > 6)
            SnacCliAddcontact (sess, uin);
        else
            CmdPktCmdContactList (sess);
    }
    return 0;
}

/*
 * Remove a user from your contact list.
 */
JUMP_F(CmdUserRem)
{
    char *arg1;
    UDWORD uin;
    SESSION(TYPE_SERVER_OLD | TYPE_SERVER);

    arg1 = strtok (args, " \t");
    if (arg1)
    {
        uin = ContactFindByNick (arg1);
        if (!uin)
            M_print (i18n (750, "Nick %s not in contact list.\n"), arg1);
        else
        {
            ContactRem (uin);
            if (sess->ver > 6)
                SnacCliRemcontact (sess, uin);
            else
                CmdPktCmdContactList (sess);
            M_print (i18n (751, "Nick %s removed."), arg1);
            M_print (i18n (754, " Note: You need to 'save' to write new contact list to disc.\n"), arg1);
        }
    }
    else
        M_print (i18n (752, "No nick given.\n"));
    return 0;
}

/*
 * Basic information on last user a message was received from.
 */
JUMP_F(CmdUserRInfo)
{
    SESSION(TYPE_SERVER | TYPE_SERVER_OLD);

    M_print (i18n (672, "%s's IP address is "), ContactFindName (uiG.last_rcvd_uin));
    Print_IP (uiG.last_rcvd_uin);
    if ((UWORD) Get_Port (uiG.last_rcvd_uin) != (UWORD) 0xffff)
        M_print (i18n (673, "\tThe port is %d\n"), (UWORD) Get_Port (uiG.last_rcvd_uin));
    else
        M_print (i18n (674, "\tThe port is unknown\n"));
    
    if (sess->ver > 6)
        SnacCliMetareqinfo (sess, uiG.last_rcvd_uin);
    else
        CmdPktCmdMetaReqInfo (sess, uiG.last_rcvd_uin);
/*  send_ext_info_req( sok, uiG.last_rcvd_uin );*/
    return 0;
}

/*
 * Authorize another user to add you to the contact list.
 */
JUMP_F(CmdUserAuth)
{
    char *arg1;
    UDWORD uin;
    SESSION(TYPE_SERVER_OLD | TYPE_SERVER);

    arg1 = strtok (args, "");
    if (!arg1)
    {
        M_print (i18n (676, "Need uin to send to.\n"));
    }
    else
    {
        uin = ContactFindByNick (arg1);
        if (-1 == uin)
            M_print (i18n (665, "%s not recognized as a nick name."), arg1);
        else
            if (sess->type & TYPE_SERVER)
                SnacCliSendmsg (sess, uin, "\x03", AUTH_OK_MESS);
            else
                CmdPktCmdSendMessage (sess, uin, "\x03", AUTH_OK_MESS);
    }
    return 0;
}

/*
 * Save user preferences.
 */
JUMP_F(CmdUserSave)
{
    int i = Save_RC ();
    if (i == -1)
        M_print (i18n (679, "Sorry saving your personal reply messages went wrong!\n"));
    else
        M_print (i18n (680, "Your personal settings have been saved!\n"));
    return 0;
}

/*
 * Send an URL message.
 */
JUMP_F(CmdUserURL)
{
    char *arg1, *arg2;
    UDWORD uin;
    SESSION(TYPE_SERVER_OLD | TYPE_SERVER);

    arg1 = strtok (args, " ");
    if (!arg1)
    {
        M_print (i18n (676, "Need uin to send to.\n"));
    }
    else
    {
        uin = ContactFindByNick (arg1);
        if (uin == -1)
        {
            M_print (i18n (677, "%s not recognized as a nick name\n"), arg1);
        }
        else
        {
            arg1 = strtok (NULL, " ");
            uiG.last_sent_uin = uin;
            if (arg1)
            {
                arg2 = strtok (NULL, "");
                if (!arg2)
                    arg2 = "";
                icq_sendurl (sess, uin, arg1, arg2);
            }
            else
            {
                M_print (i18n (678, "Need URL please.\n"));
            }
        }
    }
    return 0;
}

/*
 * Shows the user in your tab list.
 */
JUMP_F(CmdUserTabs)
{
    int i;

    for (i = 0, TabReset (); TabHasNext (); i++)
        TabGetNext ();
    M_print (i18n (681, "Last %d people you talked to:\n"), i);
    for (TabReset (); TabHasNext ();)
    {
        UDWORD uin = TabGetNext ();
        Contact *cont;
        M_print ("    %s", ContactFindName (uin));
        cont = ContactFind (uin);
        if (cont)
        {
            M_print (COLMESS " (now ");
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
JUMP_F(CmdUserLast)
{
    char *arg1;
    UDWORD uin;
    Contact *cont;

    arg1 = strtok (args, "\t\n");
    if (!arg1)
    {
        M_print (i18n (682, "You have received messages from:\n"));
        for (cont = ContactStart (); ContactHasNext (cont); cont = ContactNext (cont))
            if (cont->LastMessage)
                M_print (COLCONTACT "  %s" COLNONE "\n", cont->nick);
    }
    else
    {
        if ((uin = ContactFindByNick (arg1)) == -1)
            M_print (i18n (683, "%s is neither a nick nor a UIN.\n"), arg1);
        else
        {
            if (ContactFind (uin) == NULL)
                M_print (i18n (684, "%s is not on your contact list.\n"), arg1);
            else
            {
                if (ContactFind (uin)->LastMessage != NULL)
                {
                    M_print (i18n (685, "Last message from " COLCONTACT "%s" COLNONE ":\n"),
                             ContactFind (uin)->nick);
                    M_print (COLMESS "%s" COLNONE "\n", ContactFind (uin)->LastMessage);
                }
                else
                {
                    M_print (i18n (686, "No messages received from " COLCONTACT "%s" COLNONE "\n"),
                             ContactFind (uin)->nick);
                }
            }
        }
    }
    return 0;
}

/*
 * Shows micq's uptime.
 */
JUMP_F(CmdUserUptime)
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

    M_print ("%s ", i18n (687, "mICQ has been running for"));
    if (Days != 0)
        M_print (COLMESS "%02d" COLNONE " %s, ", Days, i18n (688, "days"));
    if (Hours != 0)
        M_print (COLMESS "%02d" COLNONE " %s, ", Hours, i18n (689, "hours"));
    if (Minutes != 0)
        M_print (COLMESS "%02d" COLNONE " %s, ", Minutes, i18n (690, "minutes"));
    M_print (COLMESS "%02d" COLNONE " %s.\n", Seconds, i18n (691, "seconds"));
/*    M_print ("%s " COLMESS "%d" COLNONE " / %d\n", i18n (692, "Contacts:"), uiG.Num_Contacts, MAX_CONTACTS); */

/* i18n (693, " ") i18n (694, " ") i18n (695, " ") i18n (697, " ") i18n (698, " ") i18n */

    M_print (i18n (746, " nr type         sent/received packets/unique packets\n"));
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
             i18n (747, "total"), pak_sent, pak_rcvd,
             real_pak_sent, real_pak_rcvd);
    return 0;
}

/*
 * List connections, and open/close them.
 */
JUMP_F(CmdUserConn)
{
    char *arg1;
    int i;
    Session *sess;

    arg1 = strtok (args, " \t\n");
    if (!arg1)
    {
        M_print (i18n (887, "Connections:"));
        M_print ("\n  " ESC "«");
        
        for (i = 0; (sess = SessionNr (i)); i++)
        {
            M_print (i18n (917, "%-12s version %d for %s (%x), at %s:%d %s\n"),
                     SessionType (sess), sess->ver, ContactFindName (sess->uin), sess->status,
                     sess->server ? sess->server : UtilIOIP (sess->ip), sess->port,
                     sess->connect & CONNECT_FAIL ? i18n (497, "failed") :
                     sess->connect & CONNECT_OK   ? i18n (558, "connected") :
                     sess->connect & CONNECT_MASK ? i18n (911, "connecting") : i18n (912, "closed"));
            if (prG->verbose)
            M_print (i18n (559, "    type %d socket %d ip %s (%d) on [%s,%s] id %x/%x/%x\n"),
                     sess->type, sess->sok, UtilIOIP (sess->ip),
                     sess->connect, UtilIOIP (sess->our_local_ip), UtilIOIP (sess->our_outside_ip),
                     sess->our_session, sess->our_seq, sess->our_seq2);
        } 
        M_print (ESC "»\r");
    }
    else if (!strcmp (arg1, "login") || !strcmp (arg1, "open"))
    {
        i = atoi (strtok (NULL, "\n"));
        sess = SessionNr (i - 1);
        if (!sess)
        {
            M_print (i18n (894, "There is no connection number %d.\n"), i);
            return 0;
        }
        if (sess->connect & CONNECT_OK)
        {
            M_print (i18n (891, "Connection %d is already open.\n"), i);
            return 0;
        }

        SessionInit (sess);
    }
    else
    {
        M_print (i18n (892, "conn             List available connections.\n"));
        M_print (i18n (893, "conn login <nr>  Open connection <nr>.\n"));
    }
    return 0;
}


/*
 * Exits mICQ.
 */
JUMP_F(CmdUserQuit)
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
JUMP_F(CmdUserSearch)
{
    static char *email, *nick, *first, *last;
    SESSION(TYPE_SERVER_OLD);

    switch (status)
    {
        case 0:
            if (*args)
            {
                CmdPktCmdSearchUser (sess, args, "", "", "");
                return 0;
            }
            R_dopromptf ("%s ", i18n (655, "Enter the user's e-mail address:"));
            return status = 101;
        case 101:
            email = strdup ((char *) args);
            R_dopromptf ("%s ", i18n (656, "Enter the user's nick name:"));
            return ++status;
        case 102:
            nick = strdup ((char *) args);
            R_dopromptf ("%s ", i18n (657, "Enter the user's first name:"));
            return ++status;
        case 103:
            first = strdup ((char *) args);
            R_dopromptf ("%s", i18n (658, "Enter the user's last name:"));
            return ++status;
        case 104:
            last = strdup ((char *) args);
            CmdPktCmdSearchUser (sess, email, nick, first, last);
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
JUMP_F(CmdUserWpSearch)
{
    int temp;
    static MetaWP wp;
    SESSION(TYPE_SERVER_OLD);

    switch (status)
    {
        case 0:
            R_dopromptf ("%s ", i18n (656, "Enter the user's nick name:"));
            return 200;
        case 200:
            wp.nick = strdup ((char *) args);
            R_dopromptf ("%s ", i18n (657, "Enter the user's first name:"));
            return ++status;
        case 201:
            wp.first = strdup ((char *) args);
            R_dopromptf ("%s ", i18n (658, "Enter the user's last name:"));
            return ++status;
        case 202:
            wp.last = strdup ((char *) args);
            R_dopromptf ("%s ", i18n (655, "Enter the user's e-mail address:"));
            return ++status;
        case 203:
            wp.email = strdup ((char *) args);
            R_dopromptf ("%s ", i18n (560, "Enter min age (18-22,23-29,30-39,40-49,50-59,60-120):"));
            return ++status;
        case 204:
            wp.minage = atoi (args);
            R_dopromptf ("%s ", i18n (561, "Enter max age (22,29,39,49,59,120):"));
            return ++status;
        case 205:
            wp.maxage = atoi (args);
            R_doprompt (i18n (588, "Enter sex:"));
            return ++status;
        case 206:
            if (!strncasecmp (args, i18n (528, "female"), 1))
            {
                wp.sex = 1;
            }
            else if (!strncasecmp (args, i18n (529, "male"), 1))
            {
                wp.sex = 2;
            }
            else
            {
                wp.sex = 0;
            }
            R_dopromptf ("%s ", i18n (534, "Enter a language by number or L for a list:"));
            return ++status;
        case 207:
            temp = atoi (args);
            if ((0 == temp) && (toupper (args[0]) == 'L'))
            {
                TablePrintLang ();
                R_dopromptf ("%s ", i18n (534, "Enter a language by number or L for a list:"));
            }
            else
            {
                wp.language = temp;
                status++;
                R_dopromptf ("%s ", i18n (589, "Enter a city:"));
            }
            return status;
        case 208:
            wp.city = strdup ((char *) args);
            R_dopromptf ("%s ", i18n (602, "Enter a state:"));
            return ++status;
        case 209:
            wp.state = strdup ((char *) args);
            R_dopromptf ("%s ", i18n (578, "Enter country's phone ID number:"));
            return ++status;
        case 210:
            wp.country = atoi ((char *) args);
            R_dopromptf ("%s ", i18n (579, "Enter company: "));
            return ++status;
        case 211:
            wp.company = strdup ((char *) args);
            R_dopromptf ("%s ", i18n (587, "Enter department: "));
            return ++status;
        case 212:
            wp.department = strdup ((char *) args);
            R_dopromptf ("%s ", i18n (603, "Enter position: "));
            return ++status;
        case 213:
            wp.position = strdup ((char *) args);
            R_dopromptf ("%s ", i18n (604, "Should the users be online?"));
            return ++status;
/* A few more could be added here, but we're gonna make this
 the last one -KK */
        case 214:
            if (strcasecmp (args, i18n (28, "NO")) && strcasecmp (args, i18n (27, "YES")))
            {
                M_print ("%s\n", i18n (29, "Please enter YES or NO!"));
                R_dopromptf ("%s ", i18n (604, "Should the users be online?"));
                return status;
            }
            wp.online = strcasecmp (args, i18n (27, "YES")) ? FALSE : TRUE;

            CmdPktCmdMetaSearchWP (sess, &wp);

            free (wp.nick);
            free (wp.last);
            free (wp.first);
            free (wp.email);
            free (wp.company);
            free (wp.department);
            free (wp.position);
            free (wp.city);
            free (wp.state);
            return 0;
    }
    return 0;
}

/*
 * Update your basic user information.
 */
JUMP_F(CmdUserUpdate)
{
    static MetaGeneral user;
    SESSION(TYPE_SERVER_OLD);

    switch (status)
    {
        case 0:
            R_dopromptf ("%s ", i18n (553, "Enter Your New Nickname:"));
            return 300;
        case 300:
            user.nick = strdup ((char *) args);
            R_dopromptf ("%s ", i18n (554, "Enter your new first name:"));
            return ++status;
        case 301:
            user.first = strdup ((char *) args);
            R_dopromptf ("%s ", i18n (555, "Enter your new last name:"));
            return ++status;
        case 302:
            user.last = strdup ((char *) args);
            R_dopromptf ("%s ", i18n (556, "Enter your new email address:"));
            return ++status;
        case 303:
            user.email = strdup ((char *) args);
            R_dopromptf ("%s ", i18n (542, "Enter other email address:"));
            return ++status;
        case 304:
            user.email2 = strdup ((char *) args);
            R_dopromptf ("%s ", i18n (543, "Enter old email address:"));
            return ++status;
        case 305:
            user.email3 = strdup ((char *) args);
            R_dopromptf ("%s ", i18n (544, "Enter new city:"));
            return ++status;
        case 306:
            user.city = strdup ((char *) args);
            R_dopromptf ("%s ", i18n (545, "Enter new state:"));
            return ++status;
        case 307:
            user.state = strdup ((char *) args);
            R_dopromptf ("%s ", i18n (546, "Enter new phone number:"));
            return ++status;
        case 308:
            user.phone = strdup ((char *) args);
            R_dopromptf ("%s ", i18n (547, "Enter new fax number:"));
            return ++status;
        case 309:
            user.fax = strdup ((char *) args);
            R_dopromptf ("%s ", i18n (548, "Enter new street address:"));
            return ++status;
        case 310:
            user.street = strdup ((char *) args);
            R_dopromptf ("%s ", i18n (549, "Enter new cellular number:"));
            return ++status;
        case 311:
            user.cellular = strdup ((char *) args);
            R_dopromptf ("%s ", i18n (550, "Enter new zip code (must be numeric):"));
            return ++status;
        case 312:
            user.zip = atoi ((char *) args);
            R_dopromptf ("%s ", i18n (551, "Enter your country's phone ID number:"));
            return ++status;
        case 313:
            user.country = atoi ((char *) args);
            R_dopromptf ("%s ", i18n (552, "Enter your time zone (+/- 0-12):"));
            return ++status;
        case 314:
            user.tz = atoi ((char *) args);
            user.tz <<= 1;
            R_dopromptf ("%s ", i18n (557, "Do you want to require Mirabilis users to request your authorization? (YES/NO)"));
            return ++status;
        case 315:
            if (strcasecmp (args, i18n (28, "NO")) && strcasecmp (args, i18n (27, "YES")))
            {
                M_print ("%s\n", i18n (29, "Please enter YES or NO!"));
                R_dopromptf ("%s ", i18n (557, "Do you want to require Mirabilis users to request your authorization? (YES/NO)"));
                return status;
            }
            user.auth = strcasecmp (args, i18n (27, "YES")) ? FALSE : TRUE;
            R_dopromptf ("%s ", i18n (605, "Do you want your status to be available on the web? (YES/NO)"));
            return ++status;
        case 316:
            if (strcasecmp (args, i18n (28, "NO")) && strcasecmp (args, i18n (27, "YES")))
            {
                M_print ("%s\n", i18n (29, "Please enter YES or NO!"));
                R_dopromptf ("%s ", i18n (605, "Do you want your status to be available on the web? (YES/NO)"));
                return status;
            }
            user.webaware = strcasecmp (args, i18n (27, "YES")) ? FALSE : TRUE;
            R_dopromptf ("%s ", i18n (857, "Do you want to hide your IP from other contacts? (YES/NO)"));
            return ++status;
        case 317:
            if (strcasecmp (args, i18n (28, "NO")) && strcasecmp (args, i18n (27, "YES")))
            {
                M_print ("%s\n", i18n (29, "Please enter YES or NO!"));
                R_dopromptf ("%s ", i18n (857, "Do you want to hide your IP from other contacts? (YES/NO)"));
                return status;
            }
            user.hideip = strcasecmp (args, i18n (27, "YES")) ? FALSE : TRUE;
            R_dopromptf ("%s ", i18n (622, "Do you want to apply these changes? (YES/NO)"));
            return ++status;
        case 318:
            if (strcasecmp (args, i18n (28, "NO")) && strcasecmp (args, i18n (27, "YES")))
            {
                M_print ("%s\n", i18n (29, "Please enter YES or NO!"));
                R_dopromptf ("%s ", i18n (622, "Do you want to apply these changes? (YES/NO)"));
                return status;
            }

            if (!strcasecmp (args, i18n (27, "YES")))
                CmdPktCmdMetaGeneral (sess, &user);

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
JUMP_F(CmdUserOther)
{
    static MetaMore other;
    int temp;
    SESSION(TYPE_SERVER_OLD);

    switch (status)
    {
        case 0:
            R_dopromptf ("%s ", i18n (535, "Enter new age:"));
            return 400;
        case 400:
            other.age = atoi (args);
            R_dopromptf ("%s ", i18n (536, "Enter new sex:"));
            return ++status;
        case 401:
            if (!strncasecmp (args, i18n (528, "female"), 1))
            {
                other.sex = 1;
            }
            else if (!strncasecmp (args, i18n (529, "male"), 1))
            {
                other.sex = 2;
            }
            else
            {
                other.sex = 0;
            }
            R_dopromptf ("%s ", i18n (537, "Enter new homepage:"));
            return ++status;
        case 402:
            other.hp = strdup (args);
            R_dopromptf ("%s ", i18n (538, "Enter new year of birth (4 digits):"));
            return ++status;
        case 403:
            other.year = atoi (args) - 1900;
            R_dopromptf ("%s ", i18n (539, "Enter new month of birth:"));
            return ++status;
        case 404:
            other.month = atoi (args);
            R_dopromptf ("%s ", i18n (540, "Enter new day of birth:"));
            return ++status;
        case 405:
            other.day = atoi (args);
            R_dopromptf ("%s ", i18n (534, "Enter a language by number or L for a list:"));
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
            R_dopromptf ("%s ", i18n (534, "Enter a language by number or L for a list:"));
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
            R_dopromptf ("%s ", i18n (534, "Enter a language by number or L for a list:"));
            return status;
        case 408:
            temp = atoi (args);
            if ((0 == temp) && (toupper (args[0]) == 'L'))
            {
                TablePrintLang ();
                R_dopromptf ("%s ", i18n (534, "Enter a language by number or L for a list:"));
                return status;
            }
            other.lang3 = temp;
            CmdPktCmdMetaMore (sess, &other);
            free (other.hp);
            return 0;
    }
    return 0;
}

/*
 * Update about information.
 */
JUMP_F(CmdUserAbout)
{
    static int offset = 0;
    static char msg[1024];
    SESSION(TYPE_SERVER_OLD);

    if (status > 100)
    {
        msg[offset] = 0;
        if (!strcmp (args, END_MSG_STR))
        {
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
                CmdPktCmdMetaAbout (sess, msg);
                return 0;
            }
        }
    }
    else
    {
        if (args && strlen (args))
        {
            CmdPktCmdMetaAbout (sess, args);
            return 0;
        }
        offset = 0;
    }
    R_doprompt (i18n (541, "About> "));
    return 400;
}

/*
 * Process one command, but ignore idle times.
 */
void CmdUser (const char *command)
{
    int a, b;
    CmdUserProcess (command, &a, &b);
}

/*
 * Get one line of input and process it.
 */
void CmdUserInput (int *idle_val, int *idle_flag)
{
    CmdUserProcess (NULL, idle_val, idle_flag);
}

/*
 * Process one line of command, get it if necessary.
 */
void CmdUserProcess (const char *command, int *idle_val, int *idle_flag)
{
    unsigned char buf[1024];    /* This is hopefully enough */
    char *cmd;
    char *arg1;

    static jump_f *sticky = (jump_f *)NULL;
    static int status = 0;

/* GRYN - START */
    int idle_save;
    idle_save = *idle_val;
    *idle_val = time (NULL);
/* GRYN - STOP */

    if (command)
    {
        snprintf (buf, sizeof (buf), "%s", command);
    }
    else
    {
        memset (buf, 0, 1024);
#ifdef __BEOS__
        Be_GetText (buf);
#else
        R_getline (buf, 1024);
#endif
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
                if ((buf[1] < 31) || (buf[1] > 127))
                    M_print (i18n (660, "Invalid Command: %s\n"), &buf[1]);
                else
                    system (&buf[1]);
#else
                M_print (i18n (661, "Shell commands have been disabled.\n"));
#endif
                R_resume ();
                if (!command)
                    Prompt ();
                return;
            }
            cmd = strtok (buf, " \n\t");

            if (!cmd)
            {
                if (!command)
                    Prompt ();
                return;
            }

            /* skip all non-alphanumeric chars on the beginning
             * to accept IRC like commands starting with a /
             * or talker like commands starting with a .
             * or whatever */
            if (!isalnum (cmd[0]))
                cmd++;

            if (!*cmd)
            {
                if (!command)
                    Prompt ();
                return;
            }

            /* goto's removed and code fixed by Paul Laufer. Enjoy! */
            else if (!strcasecmp (cmd, "pass"))
            {
                arg1 = strtok (NULL, "");
                if (arg1)
                    CmdPktCmdMetaPass (NULL, arg1);      /* TODO */
            }
            else if (!strcasecmp (cmd, "ver"))
            {
                M_print (BuildVersion ());
            }
            else
            {
                char *args, *argsd;
                jump_t *j = (jump_t *)NULL;
                
                args = strtok (NULL, "\n");
                if (!args)
                    args = "";
                argsd = strdup (args);

                if (*cmd != '¶')
                    j = CmdUserLookup (cmd, CU_USER);
                if (!j)
                    j = CmdUserLookup (*cmd == '¶' ? cmd + 1 : cmd, CU_DEFAULT);

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
                    M_print (i18n (36, "Unknown command %s, type /help for help."), cmd);
                    M_print ("\n");
                }
                free (argsd);
            }
        }
    }
    if (!status && !uiG.quit && !command)
        Prompt ();
}
