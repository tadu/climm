/* $Id$ */

/*
 * This file is the general dumpster for functions
 * that better be rewritten. Or moved to a better place.
 */

#include "climm.h"
#include "util.h"
#include "contact.h"
#include "connection.h"
#include "preferences.h"
#include "oscar_base.h"
#include "conv.h"
#include <assert.h>
#include <errno.h>
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif
#include <fcntl.h>

/*
 * Log the event provided to the log with a time stamp.
 */
int putlog (Server *conn, time_t stamp, Contact *cont, 
            status_t status, UDWORD nativestatus, enum logtype level, UWORD type, const char *log)
{
    char buffer[LOG_MAX_PATH + 1];                   /* path to the logfile */
    char *target = buffer;                         /* Target of the sym link */
    const char *username = PrefLogName (prG);
    FILE *logfile;
    int fd;
    char *mylog; /* Buffer to compute log entry */
    str_s t = { NULL, 0, 0 };
    size_t lcnt;
    char *pos, *indic;
    time_t now;
    struct tm *utctime;

    if (!cont)
        return 0;

    switch (level)
    {
        case LOG_MISC:    indic = "--"; break;
        case LOG_SENT:    indic = "->"; if (!ContactPrefVal (cont, CO_LOGMESS)) return 0; break;
        case LOG_AUTO:    indic = "=>"; if (!ContactPrefVal (cont, CO_LOGMESS)) return 0; break;
        case LOG_RECVD:   indic = "<-"; if (!ContactPrefVal (cont, CO_LOGMESS)) return 0; break;
        case LOG_CHANGE:  indic = "::"; if (!ContactPrefVal (cont, CO_LOGCHANGE)) return 0; break;
        case LOG_REPORT:  indic = ":!"; if (!ContactPrefVal (cont, CO_LOGCHANGE)) return 0; break;
        case LOG_GUESS:   indic = ":?"; if (!ContactPrefVal (cont, CO_LOGCHANGE)) return 0; break;
        case LOG_ONLINE:  indic = ":+"; if (!ContactPrefVal (cont, CO_LOGONOFF)) return 0; break;
        case LOG_OFFLINE: indic = ":-"; if (!ContactPrefVal (cont, CO_LOGONOFF)) return 0; break;
        case LOG_ACK:     indic = "<#"; if (!ContactPrefVal (cont, CO_LOGMESS)) return 0; break;
        case LOG_ADDED:   indic = "<*"; if (!ContactPrefVal (cont, CO_LOGMESS)) return 0; break;
        case LOG_LIST:    indic = "*>"; break;
        case LOG_EVENT:   indic = "<="; break;
        default:          indic = "=="; break;
    }
    
    now = time (NULL);

    for (lcnt = 0, pos = mylog = strdup (ConvCrush0xFE (log)); *pos; lcnt += *pos++ == '\n')
        if (pos[0] == '\r' && pos[1] == '\n')
            *pos = ' ';

    utctime = gmtime (&now);

    s_init (&t, "", 100);
    s_catf (&t, "# %04d%02d%02d%02d%02d%02d%s%.0ld ",
        utctime->tm_year + 1900, utctime->tm_mon + 1, 
        utctime->tm_mday, utctime->tm_hour, utctime->tm_min, 
        utctime->tm_sec, stamp != -1 ? "/" : "", 
        stamp != NOW ? stamp - now : 0L);

    pos = strchr (cont->nick, ' ') ? "\"" : "";
    
    switch (conn->type)
    {
        case TYPE_MSN_SERVER:
        case TYPE_XMPP_SERVER:
        case TYPE_SERVER:
            s_catf (&t, "[%s:%s]!%s %s %s%s%s[%s:%s+%lX %s]",
                ConnectionServerType (conn->type), conn->screen, username, indic, pos, cont->nick, pos,
                ConnectionServerType (conn->type), cont->screen, UD2UL (nativestatus), ContactStatusStr (status));
            break;
        case TYPE_MSGLISTEN:
        case TYPE_MSGDIRECT:
            s_catf (&t, "%s %s %s%s%s[tcp:%s+%lX %s]",
                      username, indic, pos, cont->nick, pos, cont->screen, UD2UL (nativestatus), ContactStatusStr (status));
            break;
        default:
            s_catf (&t, "%s %s %s%s%s[tcp:%s+%lX %s]",
                      username, indic, pos, cont->nick, pos, cont->screen, UD2UL (nativestatus), ContactStatusStr (status));
            break;
    }

    if ((lcnt += *mylog != '\0') != 0)
        s_catf (&t, " +%u", (unsigned)lcnt);

    if (type != 0xFFFF && type != MSG_NORM)
        s_catf (&t, " (%u)", type);
    
    s_catc (&t, '\n');

    if (*mylog)
    {
        s_cat (&t, mylog);
        s_catc (&t, '\n');
    }
    free (mylog);

    if (!prG->logplace)
        prG->logplace = "history" _OS_PATHSEPSTR;

    /* Check for '/' below doesn't work for empty strings. */
    assert (*prG->logplace != '\0');

    snprintf (buffer, sizeof (buffer), s_realpath (prG->logplace));
    target += strlen (buffer);
    
    if (target[-1] == _OS_PATHSEP)
    {
        if (mkdir (buffer, S_IRWXU) == -1 && errno != EEXIST)
        {
            s_done (&t);
            return -1;
        }

        snprintf (target, buffer + sizeof (buffer) - target, "%s.log", cont->screen);
        if (strchr (target, _OS_PATHSEP) != NULL)
            strcpy (strchr (target, _OS_PATHSEP), ".log");

#if HAVE_SYMLINK
        if (strcmp (cont->screen, cont->nick))
        {
            char symbuf[LOG_MAX_PATH + 1];                  /* path of a sym link */
            char *b = target - buffer + symbuf;

            strncpy (symbuf, buffer, target - buffer);
            snprintf (b, symbuf + sizeof (symbuf) - b, "nick-%s.log", cont->nick);

            while ((b = strchr (b, _OS_PATHSEP)) != NULL)
                *b = '_';
            (void) symlink (target, symbuf);
        }
#endif
    }

    if ((fd = open (buffer, O_CREAT | O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR))
        == -1)
    {
        fprintf (stderr, "\nCouldn't open %s for logging\n", buffer);
        s_done (&t);
        return -1;
    }
    if (!(logfile = fdopen (fd, "a")))
    {
        fprintf (stderr, "\nCouldn't open %s for logging\n", buffer);
        s_done (&t);
        close (fd);
        return -1;
    }

    /* Write the log entry as a single chunk to cope with concurrent writes 
     * of multiple climm instances.
     */
    fputs (t.txt, logfile);
    s_done (&t);

    /* Closes stream and file descriptor. */
    fclose (logfile);

    return 0;
}

const char *_squash_shell_chars (str_t s, const char *input)
{
    char *tmp;
    s_init (s, input, 0);
    for (tmp = s->txt; tmp && *tmp; tmp++)
        if (*tmp == '\'' || *tmp == '\\')
            *tmp = '"';
    return s->txt;
}

/*
 * Executes a program and feeds some shell-proof information data into it
 */
void EventExec (Contact *cont, const char *script, evtype_t type, UDWORD msgtype, status_t status, const char *text)
{
    int rc;
    static str_s cmd = { NULL, 0, 0 };
    static str_s buf = { NULL, 0, 0 };
    const char *mytext, *mynick, *mytype, *myagent, *mygroup;
    char *myscript, *myscreen;

    mytext = (text ? text : "");
    mynick = (cont && strcmp (cont->nick, cont->screen) ? cont->nick : "");
    myscreen = strdup (cont && cont->parent ? s_sprintf ("%s/%s", cont->parent->screen, cont->screen) : cont ? cont->screen : "");
    myagent = (cont && cont->version ? cont->version : "");
    mytype = (type == ev_msg ? "msg" : type == ev_on ? "on" : type == ev_off ? "off" : 
              type == ev_beep ? "beep" : type == ev_status ? "status" : "other");
    myscript = strdup (s_realpath (script));
    mygroup =  (cont && cont->group && cont->group->name ? cont->group->name :
                cont && cont->parent && cont->parent->group && cont->parent->group->name ? cont->parent->group->name : "global");

#if HAVE_SETENV
    setenv ("CLIMM_TEXT", mytext, 1);   setenv ("MICQ_TEXT", mytext, 1);
    setenv ("CLIMM_NICK", mynick, 1);   setenv ("MICQ_NICK", mynick, 1);
    setenv ("CLIMM_SCREEN", myscreen, 1);
    setenv ("CLIMM_AGENT", myagent, 1); setenv ("MICQ_AGENT", myagent, 1);
    setenv ("CLIMM_GROUP", mygroup, 1); setenv ("MICQ_GROUP", mygroup, 1);
    setenv ("CLIMM_TYPE", mytype, 1);   setenv ("MICQ_TYPE", mytype, 1);
    setenv ("CLIMM_STATUS", ContactStatusStr (status), 1);
    setenv ("MICQ_STATUS", ContactStatusStr (status), 1);
#endif

    s_init (&cmd, "", 100);
    s_cat  (&cmd, myscript);
    s_catf (&cmd, " %s", cont && cont->serv ? ConnectionServerType (cont->serv->type)
                       : cont && cont->parent && cont->parent->serv ? ConnectionServerType (cont->parent->serv->type)
                       : "global");
    s_catf (&cmd, " '%s'", _squash_shell_chars (&buf, myscreen));
    s_catf (&cmd, " '%s'", _squash_shell_chars (&buf, mynick));
    s_catf (&cmd, " '%s'", _squash_shell_chars (&buf, mygroup));
    s_catf (&cmd, " %s", mytype);
    s_catf (&cmd, " %ld", UD2UL (msgtype));
    s_catf (&cmd, " '%s'", _squash_shell_chars (&buf, mytext));
    s_catf (&cmd, " '%s'", _squash_shell_chars (&buf, myagent));
    s_catf (&cmd, " '%s'", ContactStatusStr (status));

    rc = system (cmd.txt);
    if (rc)
        rl_printf (i18n (2222, "Script command '%s' failed: %s (%d).\n"),
                 myscript, strerror (rc), rc);
    free (myscript);
    free (myscreen);
}
