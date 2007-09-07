/* $Id$ */
/* Copyright: This file may be distributed under version 2 of the GPL licence. */

#include "climm.h"
#include <assert.h>
#include "im_response.h"
#include "oldicq_compat.h"
#include "util_rl.h"
#include "tabs.h"
#include "contact.h"
#include "im_request.h"
#include "util.h"
#include "conv.h"
#include "preferences.h"
#include "connection.h"
#include "util_otr.h"

#if 0
static cb_status  cb_status_tcl,   cb_status_log,   cb_status_exec,  cb_status_auto, cb_status_tui;
static cb_int_msg cb_int_msg_tcl,  cb_int_msg_hist,                                  cb_int_msg_tui;
static cb_srv_msg cb_srv_msg_tcl,  cb_srv_msg_log,  cb_srv_msg_exec, cb_srv_msg_auto,
                  cb_srv_msg_hist, cb_srv_msg_auto,                                  cb_srv_msg_tui;
#endif

/** TCL **/
#ifdef ENABLE_TCL
static int cb_status_tcl (Contact *cont, parentmode_t pm, change_t ch, const char *text)
{
#if ENABLE_CONT_HIER
    if (cont->parent)
        return 0;
#endif
    TCLEvent (cont, "status", s_sprintf ("{%s}", ContactStatusStr (cont->status)));
    return 0;
}

static int cb_int_msg_tcl (Contact *cont, parentmode_t pm, time_t stamp, fat_int_msg_t *msg)
{
    return 0;
}

static int cb_srv_msg_tcl (Contact *cont, parentmode_t pm, time_t stamp, fat_srv_msg_t *msg)
{
#if ENABLE_CONT_HIER
    if (cont->parent)
        return 0;
#endif
    switch (msg->type & ~MSGF_MASS)
    {
        case MSG_NORM_SUBJ:
            if (msg->subj)
            {
                TCLEvent (cont, "message", s_sprintf ("{%s: %s}", msg->subj, msg->msgtext));
                TCLMessage (cont, msg->subj);
                TCLMessage (cont, msg->msgtext);
                break;
            }
            /* fall-through */
        case MSG_NORM:
        default:
            TCLEvent (cont, "message", s_sprintf ("{%s}", msg->msgtext));
            TCLMessage (cont, msg->msgtext);
            break;
        case MSG_FILE:
            TCLEvent (cont, "file_request", s_sprintf ("{%s} %ld %ld", msg->msgtext, msg->bytes, msg->ref));
            break;
        case MSG_AUTO:
        case MSGF_GETAUTO | MSG_GET_AWAY: 
        case MSGF_GETAUTO | MSG_GET_OCC:
        case MSGF_GETAUTO | MSG_GET_NA:
        case MSGF_GETAUTO | MSG_GET_DND:
        case MSGF_GETAUTO | MSG_GET_FFC:
        case MSGF_GETAUTO | MSG_GET_VER:
        case MSG_URL:
        case MSG_CONTACT:
            break;
        case MSG_AUTH_REQ:
            TCLEvent (cont, "authorization", "request");
            break;
        case MSG_AUTH_DENY:
            TCLEvent (cont, "authorization", "refused");
            break;
        case MSG_AUTH_GRANT:
            TCLEvent (cont, "authorization", "granted");
            break;
        case MSG_AUTH_ADDED:
            TCLEvent (cont, "contactlistadded", "");
            break;
        case MSG_EMAIL:
        case MSG_WEB:
            if (msg->type == MSG_EMAIL)
                TCLEvent (cont, "mail", s_sprintf ("{%s} {%s} {%s} {%s}", msg->tmp[0], msg->tmp[3], msg->tmp[4], msg->tmp[5]));
            else
                TCLEvent (cont, "web", s_sprintf ("{%s} {%s} {%s} {%s}", msg->tmp[0], msg->tmp[3], msg->tmp[4], msg->tmp[5]));
            break;
    }
    return 0;
}

#endif /* ENABLE_TCL */


/** LOGGING **/
static int cb_status_log (Contact *cont, parentmode_t pm, change_t ch, const char *text)
{
#if ENABLE_CONT_HIER
    if (cont->parent)
        return 0;
#endif
    assert (cont->serv);
    putlog (cont->serv, NOW, cont, cont->status, cont->nativestatus,
            ch == st_on ? LOG_ONLINE : ch == st_ch ? LOG_CHANGE : LOG_OFFLINE, 0xFFFF, "");
    return 0;
}

static int cb_srv_msg_log (Contact *cont, parentmode_t pm, time_t stamp, fat_srv_msg_t *msg)
{
#if ENABLE_CONT_HIER
    if (cont->parent)
        return 0;
#endif
    putlog (cont->serv, stamp, cont, IcqToStatus (msg->nativestatus), msg->nativestatus,
        (msg->type == MSG_AUTH_ADDED) ? LOG_ADDED : LOG_RECVD, msg->type,
         msg->msgtext);
    return 0;
}


/** SHELL SCRIPTING **/
#ifdef MSGEXEC
static int cb_status_exec (Contact *cont, parentmode_t pm, change_t ch, const char *text)
{
    if (prG->event_cmd && *prG->event_cmd)
        EventExec (cont, prG->event_cmd, ch == st_on ? ev_on : ch == st_ch ? ev_status : ev_off, cont->nativestatus, cont->status, NULL);
    return 0;
}

static int cb_srv_msg_exec (Contact *cont, parentmode_t pm, time_t stamp, fat_srv_msg_t *msg)
{
    if (prG->event_cmd && *prG->event_cmd)
        EventExec (cont, prG->event_cmd, ev_msg, msg->type, cont->status, msg->msgtext);
    return 0;
}
#endif /* MSGEXEC */


/** HISTORY **/
#define HISTSIZE 100
struct History_s
{
    Connection *conn;
    Contact *cont;
    time_t stamp;
    char *msg;
    UWORD inout;
};
typedef struct History_s History;

static History hist[HISTSIZE];

void HistMsg (Connection *conn, Contact *cont, time_t stamp, const char *msg, UWORD inout)
{
    int i, j, k;

    if (hist[HISTSIZE - 1].conn && hist[0].conn)
    {
        free (hist[0].msg);
        for (i = 0; i < HISTSIZE - 1; i++)
            hist[i] = hist[i + 1];
        hist[HISTSIZE - 1].conn = NULL;
    }

    for (i = k = j = 0; j < HISTSIZE - 1 && hist[j].conn; j++)
        if (cont == hist[j].cont)
        {
            if (!k)
                i = j;
            if (++k == 10)
            {
                free (hist[i].msg);
                for ( ; i < HISTSIZE - 1; i++)
                    hist[i] = hist[i + 1];
                hist[HISTSIZE - 1].conn = NULL;
                j--;
            }
        }
    
    hist[j].conn = conn;
    hist[j].cont = cont;
    hist[j].stamp = stamp;
    hist[j].msg = strdup (msg);
    hist[j].inout = inout;
}

void HistShow (Contact *cont)
{
    int i;
    
    for (i = 0; i < HISTSIZE; i++)
        if (hist[i].conn && (!cont || hist[i].cont == cont))
            rl_printf ("%s%s %s %s %s" COLMSGINDENT "%s\n",
                       COLDEBUG, s_time (&hist[i].stamp),
                       ReadLinePrintWidth (hist[i].cont->nick,
                           hist[i].inout == HIST_IN ? COLINCOMING : COLACK,
                           hist[i].inout == HIST_IN ? "<-" : "->",
                           &uiG.nick_len),
                       COLNONE, COLMESSAGE, hist[i].msg);
}
static int cb_int_msg_hist (Contact *cont, parentmode_t pm, time_t stamp, fat_int_msg_t *msg)
{
#if ENABLE_CONT_HIER
    if (cont->parent)
        return 0;
#endif
    HistMsg (cont->serv, cont, stamp, msg->msgtext, HIST_OUT);
    return 0;
}

static int cb_srv_msg_hist (Contact *cont, parentmode_t pm, time_t stamp, fat_srv_msg_t *msg)
{
#if ENABLE_CONT_HIER
    if (cont->parent)
        return 0;
#endif
    switch (msg->type & ~MSGF_MASS)
    {
        case MSG_NORM_SUBJ:
            if (msg->subj)
            {
                HistMsg (cont->serv, cont, stamp, msg->subj, HIST_IN);
                HistMsg (cont->serv, cont, stamp, msg->msgtext, HIST_IN);
                break;
            }
        case MSG_NORM:
        default:
            HistMsg (cont->serv, cont, stamp, msg->msgtext, HIST_IN);
            break;
        case MSG_FILE:
            break;
        case MSG_AUTO:
            break;
        case MSGF_GETAUTO | MSG_GET_AWAY: 
            break;
        case MSGF_GETAUTO | MSG_GET_OCC:
            break;
        case MSGF_GETAUTO | MSG_GET_NA:
            break;
        case MSGF_GETAUTO | MSG_GET_DND:
            break;
        case MSGF_GETAUTO | MSG_GET_FFC:
            break;
        case MSGF_GETAUTO | MSG_GET_VER:
            break;
        case MSG_URL:
            HistMsg (cont->serv, cont, stamp, msg->msgtext, HIST_IN);
            break;
        case MSG_AUTH_REQ:
            break;
        case MSG_AUTH_DENY:
            break;
        case MSG_AUTH_GRANT:
            break;
        case MSG_AUTH_ADDED:
            break;
        case MSG_EMAIL:
        case MSG_WEB:
            break;
        case MSG_CONTACT:
            break;
    }
    return 0;
}

/** AUTOFINGER FEATURE **/

static int cb_status_auto (Contact *cont, parentmode_t pm, change_t ch, const char *text)
{
    assert (cont->serv);
    if (ch != st_off && ContactPrefVal (cont, CO_AUTOAUTO))
    {
        int cdata = 0;

        switch (ContactClearInv (cont->status))
        {
            case imr_dnd:     cdata = MSGF_GETAUTO | MSG_GET_DND;  break;
            case imr_occ:     cdata = MSGF_GETAUTO | MSG_GET_OCC;  break;
            case imr_na:      cdata = MSGF_GETAUTO | MSG_GET_NA;   break;
            case imr_away:    cdata = MSGF_GETAUTO | MSG_GET_AWAY; break;
            case imr_ffc:     cdata = MSGF_GETAUTO | MSG_GET_FFC;  break;
            case imr_online:
            case imr_offline: cdata = 0;
        }

        if (cdata)
            IMCliMsg (cont, cdata, "\xff", NULL);
    }
    return 0;
}

static int cb_srv_msg_auto (Contact *cont, parentmode_t pm, time_t stamp, fat_srv_msg_t *msg)
{
    if (prG->flags & FLAG_AUTOFINGER && ~cont->updated & UPF_AUTOFINGER &&
        ~cont->updated & UPF_SERVER && ~cont->updated & UPF_DISC)
    {
        cont->updated |= UPF_AUTOFINGER;
        IMCliInfo (cont->serv, cont, 0);
    }
    return 0;
}


/** TEXT UI **/

#define MSGICQACKSTR         ">>>"
#define MSGICQACKSTROTR      "&>>"
#define MSGICQACKSTROTRSEC   "%>>"
#define MSGICQ5ACKSTR        "> >"
#define MSGICQ5ACKSTROTR     "& >"
#define MSGICQ5ACKSTROTRSEC  "% >"
#define MSGTCPACKSTR         ConvTranslit ("\xc2\xbb\xc2\xbb\xc2\xbb", "}}}")
#define MSGTCPACKSTROTR      ConvTranslit ("&\xc2\xbb\xc2\xbb", "&}}")
#define MSGTCPACKSTROTRSEC   ConvTranslit ("%\xc2\xbb\xc2\xbb", "%}}")
#define MSGSSLACKSTR         ConvTranslit ("\xc2\xbb%\xc2\xbb", "}%}")
#define MSGSSLACKSTROTR      ConvTranslit ("&%\xc2\xbb", "&%}")
#define MSGSSLACKSTROTRSEC   ConvTranslit ("%%\xc2\xbb", "%%}")
#define MSGTYPE2ACKSTR       ConvTranslit (">>\xc2\xbb", ">>}")
#define MSGTYPE2ACKSTROTR    ConvTranslit ("&>\xc2\xbb", "&>}")
#define MSGTYPE2ACKSTROTRSEC ConvTranslit ("%>\xc2\xbb", "%>}")
#define MSGICQRECSTR         "<<<"
#define MSGICQRECSTROTR      "<<&"
#define MSGICQRECSTROTRSEC   "<<%"
#define MSGTCPRECSTR         ConvTranslit ("\xc2\xab\xc2\xab\xc2\xab", "{{{")
#define MSGTCPRECSTROTR      ConvTranslit ("\xc2\xab\xc2\xab&", "{{&")
#define MSGTCPRECSTROTRSEC   ConvTranslit ("\xc2\xab\xc2\xab%", "{{%")
#define MSGSSLRECSTR         ConvTranslit ("\xc2\xab%\xc2\xab", "{%{")
#define MSGSSLRECSTROTR      ConvTranslit ("\xc2\xab%&", "{%&")
#define MSGSSLRECSTROTRSEC   ConvTranslit ("\xc2\xab%%", "{%%")
#define MSGTYPE2RECSTR       ConvTranslit ("\xc2\xab<<", "{<<")
#define MSGTYPE2RECSTROTR    ConvTranslit ("\xc2\xab<&", "{<&")
#define MSGTYPE2RECSTROTRSEC ConvTranslit ("\xc2\xab<%", "{<%")

#if ENABLE_CONT_HIER
static void cb_tui_tail (Contact *cont)
{
    if (!cont->parent)
        return;
    cb_tui_tail (cont->parent);
    rl_printf (i18n (2619, " with %s%s%s"), COLCONTACT, cont->nick, COLNONE);
}
#endif

static int cb_status_tui (Contact *cont, parentmode_t pm, change_t ch, const char *text)
{
    Contact *pcont = cont;

#if ENABLE_CONT_HIER
    int dotail = 0;
    if (pm == pm_parent)
        return 0;
    
    for ( ; pcont->parent; pcont = pcont->parent)
        if (pcont->parent->firstchild != pcont || pcont->parent->firstchild->next)
            dotail = 1;
#endif
    rl_log_for (pcont->nick, COLCONTACT);
    
    if (ch == st_off)
        rl_printf ("%s", i18n (1030, "logged off"));
    else if (ch == st_on)
        rl_printf (i18n (2213, "logged on (%s)"), s_status (cont->status, cont->nativestatus));
    else
        rl_printf (i18n (2212, "changed status to %s"), s_status (cont->status, cont->nativestatus));

    if (cont->version && ch == st_on)
        rl_printf (" [%s]", cont->version);

/*    if ((cont->flags & imf_birth) && ((~oldf & imf_birth) || ch == st_on)) */
    if (cont->flags & imf_birth && ch != st_off)
        rl_printf (" (%s)", i18n (2033, "born today"));

#if ENABLE_CONT_HIER
    if (dotail)
        cb_tui_tail (cont);
#endif

    if (text && *text)
        rl_printf (". %s\n", s_wordquote (text));
    else
        rl_print (".\n");

    if (ch == st_on && cont->dc && prG->verbose)
    {
        rl_printf ("    %s %s / ", i18n (1642, "IP:"), s_ip (cont->dc->ip_rem));
        rl_printf ("%s:%ld    %s %d    %s (%d)\n", s_ip (cont->dc->ip_loc),
            cont->dc->port, i18n (1453, "TCP version:"), cont->dc->version,
            cont->dc->type == 4 ? i18n (1493, "Peer-to-Peer") : i18n (1494, "Server Only"),
            cont->dc->type);
    }
    return 0;
}

static int cb_int_msg_tui (Contact *cont, parentmode_t pm, time_t stamp, fat_int_msg_t *msg)
{
    const char *line = "";
    const char *col = COLCONTACT;
    char *p, *q;
    const char *marker = NULL;

#if ENABLE_CONT_HIER
    int dotail = 0;
    if (pm == pm_parent)
        return 0;

    for ( ; cont->parent; cont = cont->parent)
        if (cont->parent->firstchild != cont || cont->parent->firstchild->next)
            dotail = 1;
#endif
                                        
    switch (msg->type)
    {
        case INT_MSGTRY_TYPE2:
        case INT_MSGTRY_DC:
        case INT_MSGACK_TYPE2:
        case INT_MSGACK_DC:
        case INT_MSGACK_SSL:
        case INT_MSGACK_V8:
        case INT_MSGACK_V5:
        case INT_MSGDISPL:
        case INT_MSGCOMP:
        case INT_MSGNOCOMP:
            if (ContactPrefVal (cont, CO_HIDEACK))
                return 1;
            break;
        default:
            ;
    }

    switch (msg->type)
    {
        case INT_FILE_ACKED:
            line = s_sprintf (i18n (2462, "File transfer %s to port %s%ld%s."),
                              s_qquote (msg->opt_text), COLQUOTE, msg->port, COLNONE);
            break;
        case INT_FILE_REJED:
            line = s_sprintf (i18n (2463, "File transfer %s rejected by peer: %s."),
                              s_qquote (msg->opt_text), s_wordquote (msg->msgtext));
            break;
        case INT_FILE_ACKING:
            line = s_sprintf (i18n (2464, "Accepting file %s (%s%ld%s bytes)."),
                              s_qquote (msg->opt_text), COLQUOTE, msg->bytes, COLNONE);
            break;
        case INT_FILE_REJING:
            line = s_sprintf (i18n (2465, "Refusing file request %s (%s%ld%s bytes): %s."),
                              s_qquote (msg->opt_text), COLQUOTE, msg->bytes, COLNONE, s_wordquote (msg->msgtext));
            break;
        case INT_CHAR_REJING:
            line = s_sprintf (i18n (2466, "Refusing chat request (%s/%s) from %s%s%s."),
                              p = strdup (s_qquote (msg->opt_text)), q = strdup (s_qquote (msg->msgtext)),
                              COLCONTACT, cont->nick, COLNONE);
            free (p);
            free (q);
            break;
        case INT_MSGDISPL:      marker = "<displayed>";      break;
        case INT_MSGCOMP:       marker = "<composing>";      break;
        case INT_MSGNOCOMP:     marker = "<no composing>";   break;
        case INT_MSGOFF:        marker = "<offline>";        break;
        case INT_MSGTRY_TYPE2:  marker = i18n (2293, "--="); break;
        case INT_MSGTRY_DC:     marker = i18n (2294, "==="); break;
        case INT_MSGACK_TYPE2:  marker = msg->bytes == 2 ? MSGTYPE2ACKSTROTRSEC
                                       : msg->bytes == 1 ? MSGTYPE2ACKSTROTR
                                       :                   MSGTYPE2ACKSTR;     break;
        case INT_MSGACK_DC:     marker = msg->bytes == 2 ? MSGTCPACKSTROTRSEC
                                       : msg->bytes == 1 ? MSGTCPACKSTROTR
                                       :                   MSGTCPACKSTR;       break;
#ifdef ENABLE_SSL
        case INT_MSGACK_SSL:    marker = msg->bytes == 2 ? MSGSSLACKSTROTRSEC
                                       : msg->bytes == 1 ? MSGSSLACKSTROTR
                                       :                   MSGSSLACKSTR;       break;
#endif
        case INT_MSGACK_V8:     marker = msg->bytes == 2 ? MSGICQACKSTROTRSEC
                                       : msg->bytes == 1 ? MSGICQACKSTROTR
                                       :                   MSGICQACKSTR;       break;
        case INT_MSGACK_V5:     marker = msg->bytes == 2 ? MSGICQ5ACKSTROTRSEC
                                       : msg->bytes == 1 ? MSGICQ5ACKSTROTR
                                       :                   MSGICQ5ACKSTR;      break;
        default:
            line = "";
    }
    
    if (marker)
    {
        col = COLACK;
        line = s_sprintf ("%s%s %s", marker, COLSINGLE, msg->msgtext);
    }

    for (p = q = strdup (line); *q; q++)
        if (*q == (char)0xfe)
            *q = '*';

    rl_printf ("%s ", s_time (&stamp));
    rl_printf ("%s", ReadLinePrintCont (cont->nick, col));
    
    if (prG->verbose > 1)
        rl_printf ("<%d> ", msg->type);

    if (msg->tstatus != ims_offline && (!cont || cont->status == ims_offline || !cont->group))
        rl_printf ("(%s) ", s_status (msg->tstatus, 0));
    
    rl_print (p);
#if ENABLE_CONT_HIER
    if (dotail)
        cb_tui_tail (cont);
#endif
    rl_print ("\n");
    free (p);

    return 0;
}

static int cb_srv_msg_tui (Contact *ocont, parentmode_t pm, time_t stamp, fat_srv_msg_t *msg)
{
    UDWORD j;
    int i, is_awaycount;
    status_noi_t noinv;
    const char *tmp, *tmp2, *tmp3, *carr;
    Contact *cont = ocont;

#if ENABLE_CONT_HIER
    int dotail = 0;
    if (pm == pm_parent)
        return 0;
    
    for ( ; cont->parent; cont = cont->parent)
        if (cont->parent->firstchild != cont || cont->parent->firstchild->next)
            dotail = 1;
#endif
    
    is_awaycount = ContactGroupPrefVal (cont->serv->contacts, CO_AWAYCOUNT);
    noinv = ContactClearInv (cont->serv->status);
    if (   (is_awaycount && noinv != imr_online && noinv != imr_ffc)
        || (cont->serv->idle_flag != i_idle)
        || uiG.idle_msgs)
    {
        if ((cont != uiG.last_rcvd) || !uiG.idle_uins || !uiG.idle_msgs)
            s_repl (&uiG.idle_uins, s_sprintf ("%s %s", uiG.idle_uins && uiG.idle_msgs ? uiG.idle_uins : "", cont->nick));

        uiG.idle_msgs++;
        ReadLinePromptSet (s_sprintf ("[%s%ld%s%s]%s%s",
                           COLINCOMING, uiG.idle_msgs, uiG.idle_uins,
                           COLNONE, COLSERVER, i18n (2467, "climm>")));
    }

    if (!msg->otrencrypted)
        carr = (msg->origin == CV_ORIGIN_dc) ? MSGTCPRECSTR :
#ifdef ENABLE_SSL
               (msg->origin == CV_ORIGIN_ssl) ? MSGSSLRECSTR :
#endif
               (msg->origin == CV_ORIGIN_v8) ? MSGTYPE2RECSTR : MSGICQRECSTR;
    else if (msg->otrencrypted == 1)
        carr = (msg->origin == CV_ORIGIN_dc) ? MSGTCPRECSTROTR :
#ifdef ENABLE_SSL
               (msg->origin == CV_ORIGIN_ssl) ? MSGSSLRECSTROTR :
#endif
               (msg->origin == CV_ORIGIN_v8) ? MSGTYPE2RECSTROTR : MSGICQRECSTROTR;
    else
        carr = (msg->origin == CV_ORIGIN_dc) ? MSGTCPRECSTROTRSEC :
#ifdef ENABLE_SSL
               (msg->origin == CV_ORIGIN_ssl) ? MSGSSLRECSTROTRSEC :
#endif
               (msg->origin == CV_ORIGIN_v8) ? MSGTYPE2RECSTROTRSEC : MSGICQRECSTROTRSEC;


    if (uiG.nick_len < 4)
        uiG.nick_len = 4;
    rl_printf ("\a%s %s", s_time (&stamp), ReadLinePrintCont (cont->nick, COLINCOMING));
    
#if ENABLE_CONT_HIER
    if (dotail)
        cb_tui_tail (cont);
#endif
    
    if (msg->nativestatus != -1 && (ocont->status != IcqToStatus (msg->nativestatus) || !cont->group))
        rl_printf ("(%s) ", s_status (IcqToStatus (msg->nativestatus), msg->nativestatus));

    if (prG->verbose > 1)
        rl_printf ("<%ld> ", msg->type);

    switch (msg->type & ~MSGF_MASS)
    {
        case MSGF_MASS: /* not reached here, but quiets compiler warning */
        while (1)
        {
            rl_printf ("(?%lx?) %s" COLMSGINDENT "%s\n", msg->type, COLMESSAGE, msg->orig_data);
            rl_printf ("    '");
            for (j = 0; j < strlen (msg->orig_data); j++)
                rl_printf ("%c", ((msg->msgtext[j] & 0xe0) && (msg->msgtext[j] != 127)) ? msg->msgtext[j] : '.');
            rl_print ("'\n");
            return 1;

        case MSG_NORM_SUBJ:
            if (msg->subj)
            {
                rl_printf ("%s \"%s\"\n", carr, s_wordquote (msg->subj));
                rl_printf ("%s" COLMSGINDENT "%s\n", COLMESSAGE, msg->msgtext);
                break;
            }

        case MSG_NORM:
        default:
            rl_printf ("%s %s" COLMSGINDENT "%s\n", carr, COLMESSAGE, msg->msgtext);
            break;

        case MSG_FILE:
            rl_printf (i18n (2468, "requests file transfer %s of %s%ld%s bytes (sequence %s%ld%s).\n"),
                      s_qquote (msg->msgtext), COLQUOTE, msg->bytes, COLNONE, COLQUOTE, msg->ref, COLNONE);
            break;

        case MSG_AUTO:
            rl_printf ("<%s> %s" COLMSGINDENT "%s\n", i18n (2108, "auto"), COLMESSAGE, msg->msgtext);
            break;

        case MSGF_GETAUTO | MSG_GET_AWAY: 
            rl_printf ("<%s> %s" COLMSGINDENT "%s\n", i18n (1972, "away"), COLMESSAGE, msg->msgtext);
            break;

        case MSGF_GETAUTO | MSG_GET_OCC:
            rl_printf ("<%s> %s" COLMSGINDENT "%s\n", i18n (1973, "occupied"), COLMESSAGE, msg->msgtext);
            break;

        case MSGF_GETAUTO | MSG_GET_NA:
            rl_printf ("<%s> %s" COLMSGINDENT "%s\n", i18n (1974, "not available"), COLMESSAGE, msg->msgtext);
            break;

        case MSGF_GETAUTO | MSG_GET_DND:
            rl_printf ("<%s> %s" COLMSGINDENT "%s\n", i18n (1971, "do not disturb"), COLMESSAGE, msg->msgtext);
            break;

        case MSGF_GETAUTO | MSG_GET_FFC:
            rl_printf ("<%s> %s" COLMSGINDENT "%s\n", i18n (1976, "free for chat"), COLMESSAGE, msg->msgtext);
            break;

        case MSGF_GETAUTO | MSG_GET_VER:
            rl_printf ("<%s> %s" COLMSGINDENT "%s\n", i18n (2109, "version"), COLMESSAGE, msg->msgtext);
            break;

        case MSG_URL:
            if (!msg->tmp[1]) continue;
            rl_printf ("%s %s\n%s %*s", carr, s_msgquote (msg->tmp[0]), s_now, uiG.nick_len - 4, "");
            rl_printf ("%s %s %s\n", i18n (2469, "URL:"), carr, s_wordquote (msg->tmp[1]));
            break;

        case MSG_AUTH_REQ:
            if (msg->tmp[1])
            {
                if (!msg->tmp[5]) continue;
            }
            else
            {
                msg->tmp[5] = msg->tmp[0];
                msg->tmp[0] = NULL;
            }
            rl_printf (i18n (2470, "requests authorization: %s\n"), s_msgquote (msg->tmp[5]));
            
            if (msg->tmp[0] && strlen (msg->tmp[0]))
                rl_printf ("%-15s %s\n", "???1:", s_wordquote (msg->tmp[0]));
            if (msg->tmp[1] && strlen (msg->tmp[1]))
                rl_printf ("%-15s %s\n", i18n (1564, "First name:"), s_wordquote (msg->tmp[1]));
            if (msg->tmp[2] && strlen (msg->tmp[2]))
                rl_printf ("%-15s %s\n", i18n (1565, "Last name:"), s_wordquote (msg->tmp[2]));
            if (msg->tmp[3] && strlen (msg->tmp[3]))
                rl_printf ("%-15s %s\n", i18n (1566, "Email address:"), s_wordquote (msg->tmp[3]));
            if (msg->tmp[4] && strlen (msg->tmp[4]))
                rl_printf ("%-15s %s\n", "???5:", s_wordquote (msg->tmp[4]));
            break;

        case MSG_AUTH_DENY:
            rl_printf (i18n (2233, "refused authorization: %s%s%s\n"), COLMESSAGE, COLMSGINDENT, msg->msgtext);
            break;

        case MSG_AUTH_GRANT:
            rl_print (i18n (1901, "has authorized you to add them to your contact list.\n"));
            break;

        case MSG_AUTH_ADDED:
            if (!msg->tmp[0])
            {
                rl_print (i18n (1755, "has added you to their contact list.\n"));
                break;
            }
            if (!msg->tmp[3]) continue;
            rl_printf ("%s ", s_cquote (msg->tmp[0], COLCONTACT));
            rl_print  (i18n (1755, "has added you to their contact list.\n"));
            rl_printf ("%-15s %s\n", i18n (1564, "First name:"), s_wordquote (msg->tmp[1]));
            rl_printf ("%-15s %s\n", i18n (1565, "Last name:"), s_wordquote (msg->tmp[2]));
            rl_printf ("%-15s %s\n", i18n (1566, "Email address:"), s_wordquote (msg->tmp[3]));
            break;

        case MSG_EMAIL:
        case MSG_WEB:
            if (!msg->tmp[5]) continue;
            if (msg->type == MSG_EMAIL)
            {
                rl_printf (i18n (2571, "\"%s\" <%s> emailed you a message [%s]: %s\n"),
                    s_cquote (msg->tmp[0], COLCONTACT), s_cquote (msg->tmp[3], COLCONTACT),
                    s_cquote (msg->tmp[4], COLCONTACT), s_msgquote (msg->tmp[5]));
            }
            else
            {
                rl_printf (i18n (2572, "\"%s\" <%s> sent you a web message [%s]: %s\n"),
                    s_cquote (msg->tmp[0], COLCONTACT), s_cquote (msg->tmp[3], COLCONTACT),
                    s_cquote (msg->tmp[4], COLCONTACT), s_msgquote (msg->tmp[5]));
            }
            break;

        case MSG_CONTACT:
            {
            tmp = s_msgtok (msg->msgtext); if (!tmp) continue;

            rl_printf (i18n (1595, "\nContact List.\n============================================\n%d Contacts\n"),
                     i = atoi (tmp));

            while (i--)
            {
                tmp2 = s_msgtok (NULL); if (!tmp2) continue;
                tmp3 = s_msgtok (NULL); if (!tmp3) continue;
                
                rl_print  (s_cquote (tmp2, COLCONTACT));
                rl_printf ("\t\t\t%s\n", s_msgquote (tmp3));
            }
            }
            break;
        }
    }
    return 0;
}

/** INTERNAL FUNCTIONS **/

#define hide_noleaf 4
#define hide_cont   2
#define hide_hide   1

static int __IMOnline (Contact *cont, status_t status, statusflag_t flags, UDWORD nativestatus, const char *text, int hide)
{
    status_t old;
    statusflag_t oldf;
#if ENABLE_CONT_HIER
    parentmode_t pm = hide & hide_noleaf ? pm_parent : pm_leaf;
#else
#define pm pm_leaf
#endif
    change_t ch = status == ims_offline ? st_off : cont->status == ims_offline ? st_on : st_ch;

    if (cont->group)
        hide |= hide_cont;
    
    OptSetVal (&cont->copts, CO_TIMESEEN, time (NULL));
    old = cont->status;
    oldf = cont->flags;
    cont->status = status;
    if (status != ims_offline)
        cont->flags = flags;
    cont->nativestatus = nativestatus;
    cont->oldflags &= ~CONT_SEENAUTO;

    cb_status_log (cont, ch, pm, text);
    
    if (ContactPrefVal (cont, CO_IGNORE)
        || (!ContactPrefVal (cont, CO_SHOWONOFF)  && (old == ims_offline || status == ims_offline))
        || (!ContactPrefVal (cont, CO_SHOWCHANGE) && old != ims_offline && status != ims_offline)
        || (cont->serv && ~cont->serv->connect & CONNECT_OK))
        hide |= hide_hide;
    
#if ENABLE_CONT_HIER
    if (cont->parent)
    {
        Contact *pcont = cont;

        assert (cont->parent->firstchild);

#if 0
        if (status == ims_offline && !cont->group)
        {
            Contact **t;
            for (t = &(cont->parent->firstchild); *t; t=&((*t)->next))
                if (*t == cont)
                {
                    *t = cont->next;
                    cont->next = NULL;
                    if (!*t)
                        break;
                }
        }
#endif

        if (ContactStatusCmp (status, cont->parent->status) >= 0)
        {
            Contact *tcont;
            for (tcont = cont->parent->firstchild; tcont; tcont = tcont->next)
            {
                assert (tcont->parent == cont->parent);
                if (ContactStatusCmp (tcont->status, pcont->status) < 0)
                    pcont = tcont;
            }
        }
        s_repl (&cont->parent->version, pcont->version);
        hide |= __IMOnline (cont->parent, pcont->status, pcont->flags, pcont->nativestatus, text, hide | hide_noleaf);
    }
#endif
    
    if (hide & hide_hide || ~hide & hide_cont)
        return hide_hide;

#ifdef MSGEXEC
    cb_status_exec (cont, pm, ch, text);
#endif
    cb_status_tui  (cont, pm, ch, text);
    cb_status_auto (cont, pm, ch, text);
#ifdef ENABLE_TCL
    cb_status_tcl  (cont, pm, ch, text);
#endif
    return hide;
}
#undef pm


int __IMIntMsg (Contact *cont, time_t stamp, fat_int_msg_t *msg, int hide)
{
    int noerr = 0;
#if ENABLE_CONT_HIER
    parentmode_t pm = hide & hide_noleaf ? pm_parent : pm_leaf;
#else
#define pm pm_leaf
#endif
/*    cb_int_msg_log (cont, pm, stamp, msg); */

    if (ContactPrefVal (cont, CO_IGNORE))
        return hide_hide;

#if ENABLE_CONT_HIER
    if (cont->parent)
    {
        assert (cont->parent->firstchild);
        hide |= __IMIntMsg (cont->parent, stamp, msg, hide | hide_noleaf);
    }
#endif
    if (hide & hide_hide)
        return hide_hide;

#ifdef MSGEXEC
/*    if (!cb_int_msg_exec (cont, pm, stamp, msg)) */
#endif
    if (!cb_int_msg_tui  (cont, pm, stamp, msg))
/*    if (!cb_int_msg_auto (cont, pm, stamp, msg)) */
    if (!cb_int_msg_hist (cont, pm, stamp, msg))
#ifdef ENABLE_TCL
    if (!cb_int_msg_tcl  (cont, pm, stamp, msg))
#endif
        noerr = 1;
    if (!noerr)
        hide |= hide_hide;

    return hide;
}

int __IMSrvMsg (Contact *cont, time_t stamp, fat_srv_msg_t *msg, int hide)
{
    int noerr = 0;
#if ENABLE_CONT_HIER
    parentmode_t pm = hide & hide_noleaf ? pm_parent : pm_leaf;
#else
#define pm pm_leaf
#endif
    cb_srv_msg_log (cont, pm, stamp, msg);

    if (ContactPrefVal (cont, CO_IGNORE))
        return hide_hide;

#if ENABLE_CONT_HIER
    if (cont->parent)
    {
        assert (cont->parent->firstchild);
        hide |= __IMSrvMsg (cont->parent, stamp, msg, hide | hide_noleaf);
    }
#endif
    if (hide & hide_hide)
        return hide_hide;

    s_repl (&cont->last_message, msg->msgtext);
    cont->last_time = stamp;

    if (!cb_srv_msg_exec   (cont, pm, stamp, msg))
    if (!cb_srv_msg_tui    (cont, pm, stamp, msg))
    if (!cb_srv_msg_auto   (cont, pm, stamp, msg))
    if (!cb_srv_msg_hist   (cont, pm, stamp, msg))
#ifdef ENABLE_TCL
    if (!cb_srv_msg_tcl    (cont, pm, stamp, msg))
#endif
        noerr = 1;
    if (!noerr)
        hide |= hide_hide;

#if ENABLE_CONT_HIER
    while (cont->parent)
        cont = cont->parent;
    if (pm == pm_leaf)
#endif
        uiG.last_rcvd = cont;
    TabAddIn (cont);
    return hide;
}

/** FUNCTIONS TO BE CALLED FROM EXTERN **/

/*
 * Inform that a user went offline
 */
void IMOffline (Contact *cont)
{
    IMOnline (cont, ims_offline, 0, -1, "");
}

/*
 * Inform that a user went online
 */
void IMOnline (Contact *cont, status_t status, statusflag_t flags, UDWORD nativestatus, const char *text)
{
    Event *egevent;
    int hide = 0;

    if (!cont)
        return;

    if ((egevent = QueueDequeue2 (cont->serv, QUEUE_DEP_WAITLOGIN, 0, 0)))
    {
        egevent->due = time (NULL) + 3;
        QueueEnqueue (egevent);
        if (!text || !*text)
            hide = hide_hide;
    }
    
    if (status == cont->status && (status == ims_offline || flags == cont->flags) && (!text || !*text))
        return;

    __IMOnline (cont, status, flags, nativestatus, text, hide);
}


/*
 * Central entry point for protocol triggered output.
 */
void IMIntMsg (Contact *cont, time_t stamp, status_t tstatus, int_msg_t type, const char *text)
{
    IMIntMsgFat (cont, stamp, tstatus, type, text, "", 0, 0);
}

void IMIntMsgMsg (Message *msg, time_t stamp, status_t tstatus)
{
    IMIntMsgFat (msg->cont, stamp, tstatus, msg->type, msg->plain_message ? msg->plain_message : msg->send_message,
                 NULL, 0, !msg->otrencrypted ? 0 : msg->otrencrypted ? 1 : 2);
}

void IMIntMsgFat (Contact *cont, time_t stamp, status_t tstatus, int_msg_t type,
                  const char *text, const char *opt_text, UDWORD port, UDWORD bytes)
{
    fat_int_msg_t msg;
    char *deleteme = NULL;

    if (!cont)
        return;

    memset (&msg, 0, sizeof msg);
    if (stamp == NOW)
        stamp = time (NULL);

    msg.orig_data = text;
    msg.msgtext = text;
    msg.type = type;
    msg.tstatus = tstatus;
    msg.opt_text = opt_text ? opt_text : "";
    msg.port = port;
    msg.bytes = bytes;

    if (!strncasecmp (msg.orig_data, "<font ", 6))
    {
      char *t = deleteme = strdup (msg.orig_data);
      while (*t && *t != '>')
        t++;
      if (*t)
      {
        size_t l = strlen (++t);
        if (!strcasecmp (t + l - 7, "</font>"))
          t[l - 7] = 0;
        msg.msgtext = t;
      }
    }
    
    __IMIntMsg (cont, stamp, &msg, 0);

    if (deleteme)
        s_free (deleteme);
}

/*
 * Central entry point for incoming messages.
 */
void IMSrvMsg (Contact *cont, time_t stamp, UDWORD opt_origin, UDWORD opt_type, const char *text)
{
    IMSrvMsgFat (cont, stamp, OptSetVals (NULL, CO_ORIGIN, opt_origin, CO_MSGTYPE, opt_type, CO_MSGTEXT, text, 0));
}

void IMSrvMsgFat (Contact *cont, time_t stamp, Opt *opt)
{
    fat_srv_msg_t msg;
    int max_0xff = 0, i;
    
    if (!cont)
    {
        OptD (opt);
        return;
    }
    
    memset (&msg, 0, sizeof msg);
    
    if (stamp == NOW)
        stamp = time (NULL);

    if (!OptGetVal (opt, CO_MSGTYPE, &msg.type))
        msg.type = MSG_NORM;
    if (!OptGetVal (opt, CO_ORIGIN, &msg.origin))
        msg.origin = CV_ORIGIN_v5;
    if (!OptGetVal (opt, CO_STATUS, &msg.nativestatus))
        msg.nativestatus = -1;
    if (!OptGetVal (opt, CO_BYTES, &msg.bytes))
        msg.bytes = 0;
    if (!OptGetVal (opt, CO_REF, &msg.ref))
        msg.ref = 0;
    if (!OptGetStr (opt, CO_SUBJECT, &msg.subj))
        msg.subj = NULL;

    if (!OptGetStr (opt, CO_MSGTEXT, &msg.orig_data))
        msg.orig_data = "";
    msg.msgtext = strdup (msg.orig_data);

#ifdef ENABLE_OTR
    /* process incomming messages for OTR decryption */
    if (msg.type == MSG_NORM && libotr_is_present)
        if (OTRMsgIn (cont, &msg))
        {
            OptD (opt);
            return; /* no msg ack/logging? */
        }
#endif /* ENABLE_OTR */
    while (*msg.msgtext && strchr ("\n\r \t", msg.msgtext[strlen (msg.msgtext) - 1]))
        msg.msgtext[strlen (msg.msgtext) - 1] = '\0';

    max_0xff = 0;
    switch (msg.type & ~MSGF_MASS)
    {
        default:
            break;
        case MSG_URL:
            max_0xff = 2;
            break;
        case MSG_AUTH_ADDED:
            max_0xff = 4;
            break;
        case MSG_AUTH_REQ:
        case MSG_EMAIL:
        case MSG_WEB:
            max_0xff = 6;
            break;
    }
    for (i = 0; i < max_0xff; i++)
        msg.tmp[i] = s_msgtok (i ? NULL : msg.msgtext);

    if (!strncasecmp (msg.msgtext, "<font ", 6))
    {
      char *t = msg.msgtext;
      while (*t && *t != '>')
        t++;
      if (*t)
      {
        size_t l = strlen (++t);
        if (!strcasecmp (t + l - 7, "</font>"))
          t[l - 7] = 0;
        t = strdup (t);
        free (msg.msgtext);
        msg.msgtext = t;
      }
    }
    
    __IMSrvMsg (cont, stamp, &msg, 0);
    
    free (msg.msgtext);
    OptD (opt);
}
