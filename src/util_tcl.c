/*
 * TCL scripting extension.
 *
 * mICQ TCL extension Copyright (C) © 2003 Roman Hoog Antink
 *
 * This extension is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 dated June, 1991.
 *
 * This extension is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * $Id$
 */

#include "micq.h"
#include "util_tcl.h"

#ifdef ENABLE_TCL

#include "contact.h"
#include "preferences.h"
#include "session.h"
#include "cmd_user.h"
#include "util_str.h"
#include "color.h"
#if HAVE_TCL8_4_TCL_H
#include <tcl8.4/tcl.h>
#elif HAVE_TCL8_3_TCL_H
#include <tcl8.3/tcl.h>
#else
#include <tcl.h>
#endif
#include <string.h>


static Tcl_Interp *tinterp;
static tcl_hook_p tcl_events = NULL;
static tcl_hook_p tcl_msgs = NULL;

static char *buf = NULL;
static UDWORD buflen = 0;

char *RemEscapes (const char *s)
{
    /* Ansi sequences: 
            ESC [ <chars> m
            ESC ! <char>
            ESC <char>
    */
    const char *c = s, *d;
    char *p, *q;
    
    q = strdup (s);
    for (p = q; *c; )
    {
        if (*c == *ESC)
        {
            if (c[1] == '[')
            {
                d = strchr (c, 'm');
                if (!d)
                    c += 2;
                else
                    c = d;
            }
            else if (c[1] == '!' && c[2])
                c += 3;
            else
                c += 2;
        }
        else
            *(p++) = *(c++);
    }
    *p = 0;
    return q;
}

TCL_CALLBACK (TCL_collect_out)
{
    char *unes;
    
    unes = RemEscapes (s);
    buf = s_cat (buf, &buflen, unes);
    free (unes);
}

TCL_COMMAND (TCL_command_help)
{
    if (argc <= 2)
    {
        M_printf (i18n (2346, "The following Tcl commands are supported:\n"));
        M_printf (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s\n%s" COLEXDENT "\n",
                 i18n (2347, "micq receive <script> [<contact>]"),
                 i18n (2348, "Install hook to receive messages from UIN or nick, or all if omitted."),
                 i18n (2349, "Callback arguments: from message"));
        M_printf (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 i18n (2350, "micq unreceive [<contact>]"),
                 i18n (2351, "Uninstall message hook for UIN or nick."));
        M_printf (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 i18n (2352, "micq event <script>"),
                 i18n (2353, "Install event hook. Callback arguments: type ..."));
        M_printf (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 i18n (2354, "micq unevent"),
                 i18n (2355, "Uninstall event hook."));
        M_printf (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 "micq hooks",
                 i18n (2356, "List all installed hooks. Format: <type> <command> <filter>."));
        M_printf (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 i18n (2357, "micq exec <cmd>"),
                 i18n (2358, "Execute micq command."));
        M_printf (COLMESSAGE "%s" COLNONE "\n\t" COLINDENT "%s" COLEXDENT "\n",
                 i18n (2359, "micq nick <uin>"),
                 i18n (2360, "Find nick from <uin>."));
        return TCL_OK;
    }
    else
    {
        Tcl_SetResult (interp, (char *)i18n (2362, "Wrong number of arguments. Try 'help'."), TCL_VOLATILE);
        return TCL_ERROR;
    }
}

#define TCL_CHECK_PARMS(n) {if (argc < n + 2) { \
        snprintf (interp->result, TCL_RESULT_SIZE, \
            i18n (2361, "Wrong number of arguments for command '%s %s'. Expected %d.\n"), \
                  argv[0], argv[1], n); \
        return TCL_ERROR; \
        } \
    }

TCL_COMMAND (TCL_command_micq)
{
    if (argc < 2)
    {
        Tcl_SetResult (interp, 
            (char *)i18n (2362, "Wrong number of arguments. Try 'help'."),
            TCL_VOLATILE);
        return TCL_ERROR;
    }

    if (!strcmp (argv[1], "receive"))
    {
        tcl_hook_p hook = tcl_msgs;
        const char *filter = argc > 3 ? argv[3] : "";
        TCL_CHECK_PARMS (1);
        while (hook)
        {
            if (!strcmp (hook->filter, filter))
            {
                Tcl_SetResult (interp, hook->cmd, TCL_VOLATILE);
                s_repl (&hook->cmd, argv[2]);
                return TCL_OK;
            }
            hook = hook->next;
        }
        hook = malloc (sizeof (tcl_hook_s));
        if (!hook)
            return TCL_ERROR;

        hook->cmd = strdup (argv[2]);
        hook->filter = strdup (filter);
        hook->next = tcl_msgs;
        tcl_msgs = hook;
    }
    else if (!strcmp (argv[1], "unreceive"))
    {
        const char *filter = argc > 3 ? argv[3] : "";
        tcl_hook_p last = NULL, hook = tcl_msgs;
        while (hook)
        {
            if (!strcmp (hook->filter, filter))
            {
                Tcl_SetResult (interp, hook->cmd, TCL_VOLATILE);
                s_free (hook->cmd);
                if (last)
                    last->next = hook->next;
                else
                    tcl_msgs = hook->next;
                free (hook->filter);
                free (hook);
                break;
            }
            last = hook;
            hook = hook->next;
        }
        return TCL_OK;
    }
    else if (!strcmp (argv[1], "event"))
    {
        tcl_hook_p hook = tcl_events;
        TCL_CHECK_PARMS (1);
        if (hook)
        {
            Tcl_SetResult (interp, hook->cmd, TCL_VOLATILE);
            s_repl (&hook->cmd, argv[2]);
            return TCL_OK;
        }
        hook = calloc (1, sizeof (tcl_hook_s));
        if (!hook)
            return TCL_ERROR;
        hook->cmd = strdup (argv[2]);
        hook->next = tcl_msgs;
        tcl_events = hook;
    }
    else if (!strcmp (argv[1], "unevent"))
    {
        if (tcl_events)
        {
            Tcl_SetResult (interp, tcl_events->cmd, TCL_VOLATILE);
            s_free (tcl_events->cmd);
            free (tcl_events);
            tcl_events = NULL;
        }
    }
    else if (!strcmp (argv[1], "hooks"))
    {
        tcl_hook_p hook = tcl_msgs;
        while (hook)
        {
            const char *s = s_sprintf ("{%s} {%s}", hook->filter, hook->cmd);
            Tcl_AppendElement (interp, s);
            hook = hook->next;
        }
        if (tcl_events)
            Tcl_AppendElement (interp, s_sprintf ("{%s} {%s}", "event hook", 
                               tcl_events->cmd));
    }
    else if (!strcmp (argv[1], "exec"))
    {
        TCL_CHECK_PARMS (1);
        if (prG->tclout)
        {
            Tcl_SetResult (tinterp, 
                (char *)i18n (2363, "Error: recursive 'micq exec' not allowed."),
                TCL_VOLATILE);
            return TCL_ERROR;
        }
        prG->tclout = TCL_collect_out;
        tinterp = interp; /* not necessary but clean. */
        buf = NULL;
        buflen = 0;
        CmdUser (argv[2]); 
        prG->tclout = NULL;
        Tcl_SetResult (interp, buf, TCL_VOLATILE);
        free (buf);
        buflen = 0;
    }
    else if (!strcmp (argv[1], "nick"))
    {
        Connection *tconn;
        Contact *cont;
        UDWORD uin = atol (argv[2]);
        TCL_CHECK_PARMS (1);
        
        if (!(tconn = ConnectionFind (TYPEF_ANY_SERVER, 0, NULL)))
        {
            Tcl_SetResult (tinterp, 
                (char *)i18n (2364, "No connection found."),
                TCL_VOLATILE);
            return TCL_ERROR;
        }
        cont = ContactFind (tconn->contacts, 0, uin, NULL, 0);
        if (cont)
            Tcl_SetResult (tinterp, cont->nick, TCL_VOLATILE);
    }
    else if (!strcmp (argv[1], "help"))
        return TCL_command_help (cd, interp, argc, argv);
    else
    {
        snprintf (interp->result, TCL_RESULT_SIZE, 
            i18n (2365, "unknown command: %s"), argv[1]);
        return TCL_ERROR;
    }
    return TCL_OK;
}

void TCLEvent (const char *type, const char *data)
{
    const char *result;
    
    if (tcl_events)
    {
        char *cdata = strdup (data);
        Tcl_Eval (tinterp, s_sprintf ("%s {%s} %s", tcl_events->cmd, type, cdata));
        result = Tcl_GetStringResult (tinterp);
        if (strlen (result) > 0)
            M_printf ("%s\n", Tcl_GetStringResult (tinterp));
        s_free (cdata);
    }
}

void TCLMessage (Contact *from, const char *text)
{
    tcl_hook_p hook = tcl_msgs;
    tcl_hook_p generic = NULL;
    char *uin = NULL;
    const char *result;

    if (from)
        uin = strdup (s_sprintf ("%lu", from->uin));

    while (hook)
    {
        if (!generic && !*hook->filter)
            generic = hook;
        else if (from && (!strcmp (hook->filter, from->nick) || 
                  !strcmp (hook->filter, uin)))
        {
            Tcl_Eval (tinterp, s_sprintf ("%s %s {%s}", hook->cmd, uin, text));
            generic = NULL;
            break;
        }
        hook = hook->next;
    }        
    if (generic)
        Tcl_Eval (tinterp, s_sprintf ("%s %s {%s}", generic->cmd, uin, text));

    result = Tcl_GetStringResult (tinterp);
    if (strlen (result) > 0)
        M_printf ("%s\n", Tcl_GetStringResult (tinterp));
    s_free (uin);
}

void TCLInit ()
{
    tcl_pref_p pref;
    int i, result;
    Connection *conn;

    tinterp = Tcl_CreateInterp ();   

    Tcl_CreateCommand (tinterp, "micq", TCL_command_micq, 
        (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand (tinterp, "help", TCL_command_help, 
        (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_SetVar (tinterp, "micq_basedir", prG->basedir, 0);
    Tcl_SetVar (tinterp, "micq_version", MICQ_VERSION, 0);

    for (i = 0; (conn = ConnectionNr (i)); i++)
        if (conn->spref->type & TYPEF_ANY_SERVER)
            Tcl_SetVar (tinterp, "micq_uin", s_sprintf ("%lu", conn->spref->uin),
                        TCL_LIST_ELEMENT | TCL_APPEND_VALUE);

    tcl_events = NULL;
    tcl_msgs = NULL;

    /* execute start scripts defined in rc file */
    pref = prG->tclscript;
    while (pref)
    {
        result = pref->type == TCL_FILE ? Tcl_EvalFile (tinterp, pref->file) : 
                    Tcl_Eval (tinterp, pref->file);
        if (result != TCL_OK)
            M_printf (i18n (2366, "TCL error in file %s: %s\n"), 
                     pref->file, Tcl_GetStringResult (tinterp));
        else
        {
            const char *r = Tcl_GetStringResult (tinterp);
            if (strlen (r) > 0)
                M_printf ("%s", r);
        }
        pref = pref->next;
    }
}

JUMP_F (CmdUserTclScript)
{
    int result;

    result = data ? Tcl_Eval (tinterp, args) : Tcl_EvalFile (tinterp, args);
    if (result != TCL_OK)
        M_printf (i18n (2367, "TCL error: %s\n"), Tcl_GetStringResult (tinterp));
    else
    {
        const char *r = Tcl_GetStringResult (tinterp);
        if (strlen (r) > 0) 
            M_printf ("%s", r);
    }
    return 0;
}

void TCLPrefAppend (Tcl_type type, char *file)
{
    tcl_pref_p tpref;

    /* alloc structure for script file */
    tpref = malloc (sizeof (tcl_pref_s));
    if (!tpref)
        return;

    /* alloc string for script file */
    if (type==TCL_FILE && file[0] != '/')
        tpref->file = strdup (s_sprintf ("%s%s", prG->basedir, file));
    else /* type==TCL_CMD || file[0] == '/' */
        tpref->file = strdup (file);

    if (!tpref->file)
    {
        free (tpref);
        return;
    }
    tpref->type = type;

    /* append to list */
    tpref->next = prG->tclscript;
    prG->tclscript = tpref;
}
#endif /* ENABLE_TCL */
