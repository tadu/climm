/* $Id$ */
/* Copyright: This file may be distributed under version 2 of the GPL licence. */

#include "micq.h"
#include "icq_response.h"
#include "util_ui.h"
#include "util_io.h"
#include "util_rl.h"
#include "tabs.h"
#include "contact.h"
#include "server.h"
#include "util_table.h"
#include "util_tcl.h"
#include "util.h"
#include "conv.h"
#include "packet.h"
#include "peer_file.h"
#include "cmd_pkt_cmd_v5.h"
#include "cmd_pkt_v8_snac.h"
#include "preferences.h"
#include "session.h"
#include "peer_file.h"
#include <stdarg.h>
#include <assert.h>
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#if HAVE_WINSOCK2_H
#include <winsock2.h>
#endif

#define s_read(s) s_repl (&s, ConvFromCont (PacketReadL2Str (pak, NULL), cont))
void HistMsg (Connection *conn, Contact *cont, time_t stamp, const char *msg);

#ifndef ENABLE_TCL
#define TCLMessage(from, text)
#define TCLEvent(from, type, data)
#endif


static BOOL Meta_Read_List (Packet *pak, ContactMeta **list, Contact *cont)
{
    UBYTE i, j;
    UWORD data;
    const char *text;
    
    i = PacketRead1 (pak);
    ContactMetaD (*list);
    *list = NULL;
    for (j = 0; j < i; j++)
    {
        data = PacketRead2 (pak);
        text = ConvFromCont (PacketReadL2Str (pak, NULL), cont);
        if (data && text && *text)
            ContactMetaAdd (list, data, text);
    }
    return TRUE;
}

void Meta_User (Connection *conn, Contact *cont, Packet *pak)
{
    UWORD subtype;
    UDWORD result;
    Event *event = NULL;

    if (!cont)
        return;
    subtype = PacketRead2 (pak);
    result  = PacketRead1 (pak);

    switch (subtype)
    {
        case META_SRV_PASS_UPDATE:
            M_printf (i18n (2136, "Password change was %s%s%s.\n"),
                     COLCLIENT, result == META_SUCCESS ? i18n (1393, "successful")
                     : i18n (1394, "unsuccessful"), COLNONE);
            break;
        case META_SRV_ABOUT_UPDATE:
            M_printf (i18n (2137, "About info change was %s%s%s.\n"),
                     COLCLIENT, result == META_SUCCESS ? i18n (1393, "successful")
                     : i18n (1394, "unsuccessful"), COLNONE);
            break;
        case META_SRV_GEN_UPDATE:
            M_printf (i18n (2138, "Info change was %s%s%s.\n"),
                     COLCLIENT, result == META_SUCCESS ? i18n (1393, "successful")
                     : i18n (1394, "unsuccessful"), COLNONE);
            break;
        case META_SRV_OTHER_UPDATE:
            M_printf (i18n (2139, "Other info change was %s%s%s.\n"),
                     COLCLIENT, result == META_SUCCESS ? i18n (1393, "successful")
                     : i18n (1394, "unsuccessful"), COLNONE);
            break;
        case META_SRV_RANDOM_UPDATE:
            if ((pak->ref & 0xffff) != 0x4242)
                M_printf (i18n (2140, "Random chat group change was %s%s%s.\n"),
                         COLCLIENT, result == META_SUCCESS ? i18n (1393, "successful")
                         : i18n (1394, "unsuccessful"), COLNONE);
            break;
    }

    switch (result) /* default error handling */
    {
        case 0x32:
        case 0x14:
            if ((event = QueueDequeue (conn, QUEUE_REQUEST_META, pak->ref)))
                EventD (event);
            M_printf ("%s ", s_now);
            M_printf (i18n (2141, "Search %sfailed%s.\n"), COLCLIENT, COLNONE);
            return;
        case META_READONLY:
            M_printf ("%s %s\n", s_now, i18n (1900, "It's readonly."));
            return;
        case META_SUCCESS:
            break;
        case 0x46:
            M_printf ("%s\n", pak->data + pak->rpos);
            return;
        default:
            M_printf (i18n (1940, "Unknown Meta User result %lx.\n"), result);
            return;
    }

    switch (subtype)
    {
        case META_SRV_ABOUT_UPDATE:
        case META_SRV_OTHER_UPDATE:
        case META_SRV_GEN_UPDATE:
        case META_SRV_PASS_UPDATE:
        case META_SRV_RANDOM_UPDATE:
            return;
        case META_SRV_INFO:
        case META_SRV_GEN:
        case META_SRV_MORE:
        case META_SRV_MOREEMAIL:
        case META_SRV_WORK:
        case META_SRV_ABOUT:
        case META_SRV_INTEREST:
        case META_SRV_BACKGROUND:
        case META_SRV_UNKNOWN_270:
        case META_SRV_RANDOM:
            if (!(event = QueueDequeue (conn, QUEUE_REQUEST_META, pak->ref)) || !event->callback)
            {
                if (prG->verbose)
                    M_printf ("FIXME: meta reply ref %lx not found.\n", pak->ref);
                return;
            }
            if (event->cont)
                cont = event->cont;
    }

    switch (subtype)
    {
        strc_t data;
        UWORD wdata, i, j;
        UDWORD dwdata, uin;
        MetaGeneral *mg;
        MetaMore *mm;
        MetaWork *mw;
        MetaObsolete *mo;

        case META_SRV_SMS_OK:
                   PacketRead4 (pak);
                   PacketRead2 (pak);
                   PacketReadB2Str (pak, NULL);
            data = PacketReadB2Str (pak, NULL);
            M_printf (i18n (2080, "Server SMS delivery response:\n%s\n"), ConvFromServ (data));
            break;
        case META_SRV_INFO:
            Display_Info_Reply (cont, pak, 0);
            /* 3 unknown bytes ignored */

            event->callback (event);
            break;
        case META_SRV_GEN:
            Display_Info_Reply (cont, pak, 0);

            if (conn->type == TYPE_SERVER_OLD)
            {
                ContactMetaAdd (&cont->meta_email, 0, ConvFromCont (PacketReadL2Str (pak, NULL), cont));
                ContactMetaAdd (&cont->meta_email, 0, ConvFromCont (PacketReadL2Str (pak, NULL), cont));
            }

            if (!(mg = CONTACT_GENERAL (cont)))
                break;
            
            s_read (mg->city);
            s_read (mg->state);
            s_read (mg->phone);
            s_read (mg->fax);
            s_read (mg->street);
            s_read (mg->cellular);
            if (conn->type == TYPE_SERVER)
                s_read (mg->zip);
            else
            {
                dwdata = PacketRead4 (pak);
                s_repl (&mg->zip, dwdata ? s_sprintf ("%ld", dwdata) : "");
            }
            mg->country = PacketRead2 (pak);
            mg->tz      = PacketRead1 (pak);
            mg->auth    = PacketRead1 (pak);
            /* one unknown word ignored according to v7 doc */
            /* probably more security settings? */

            cont->updated |= UPF_GENERAL_B;
            event->callback (event);
            break;
        case META_SRV_MORE:
            if (!(mm = CONTACT_MORE (cont)))
                break;
            
            mm->age = PacketRead2 (pak);
            mm->sex = PacketRead1 (pak);
            s_read (mm->homepage);
            if (conn->type == TYPE_SERVER_OLD)
                mm->year = PacketRead1 (pak) + 1900;
            else
                mm->year = PacketRead2 (pak);
            mm->month = PacketRead1 (pak);
            mm->day   = PacketRead1 (pak);
            mm->lang1 = PacketRead1 (pak);
            mm->lang2 = PacketRead1 (pak);
            mm->lang3 = PacketRead1 (pak);
            /* one unknown word ignored according to v7 doc */

            cont->updated |= UPF_MORE;
            event->callback (event);
            break;
        case META_SRV_MOREEMAIL:
            ContactMetaD (cont->meta_email);
            if ((i = PacketRead1 (pak)))
            {
                for (j = 0; j < i; j++)
                {
                    int auth = PacketRead1 (pak);
                    ContactMetaAdd (&cont->meta_email, auth, ConvFromCont (PacketReadL2Str (pak, NULL), cont));
                }
            }
            cont->updated |= UPF_EMAIL;
            event->callback (event);
            break;
        case META_SRV_WORK:
            if (!(mw = CONTACT_WORK (cont)))
                break;
            
            s_read (mw->wcity);
            s_read (mw->wstate);
            s_read (mw->wphone);
            s_read (mw->wfax);
            s_read (mw->waddress);
            if (conn->type == TYPE_SERVER)
                s_read (mw->wzip);
            else
            {
                dwdata = PacketRead4 (pak);
                s_repl (&mw->wzip, dwdata ? s_sprintf ("%ld", dwdata) : "");
            }
            mw->wcountry = PacketRead2 (pak);
            s_read (mw->wcompany);
            s_read (mw->wdepart);
            s_read (mw->wposition);
            mw->woccupation = PacketRead2 (pak);
            s_read (mw->whomepage);
            
            cont->updated |= UPF_WORK;
            event->callback (event);
            break;
        case META_SRV_ABOUT:
            s_read (cont->meta_about);
            cont->updated |= UPF_ABOUT;
            event->callback (event);
            break;
        case META_SRV_INTEREST:
            if (!Meta_Read_List (pak, &cont->meta_interest, cont))
                break;

            cont->updated |= UPF_INTEREST;
            event->callback (event);
            break;
        case META_SRV_BACKGROUND:
            if (!Meta_Read_List (pak, &cont->meta_background, cont))
                break;
            if (!Meta_Read_List (pak, &cont->meta_affiliation, cont))
                break;

            cont->updated |= UPF_BACKGROUND | UPF_AFFILIATION;
            event->callback (event);
            break;
        case META_SRV_WP_FOUND:
        case META_SRV_WP_LAST_USER:
            if (PacketRead2 (pak) < 19)
            {
                M_printf (i18n (2141, "Search %sfailed%s.\n"), COLCLIENT, COLNONE);
                break;
            }
            cont = ContactUIN (conn, PacketRead4 (pak));
            if (!cont || !(mg = CONTACT_GENERAL (cont)) || !(mm = CONTACT_MORE (cont)))
                break;

            Display_Info_Reply (cont, pak, IREP_HASAUTHFLAG);
            mg->webaware = PacketRead2 (pak);
            mm->sex = PacketRead1 (pak);
            mm->age = PacketRead2 (pak);

            cont->updated |= UPF_GENERAL_C;
            
            UtilUIDisplayMeta (cont);
            if (subtype == META_SRV_WP_LAST_USER && (dwdata = PacketRead4 (pak)))
                M_printf ("%lu %s\n", dwdata, i18n (1621, "users not returned."));
            break;
        case META_SRV_RANDOM:
            uin = PacketRead4 (pak);
            event->cont = cont = ContactUIN (event->conn, uin);
            wdata = PacketRead2 (pak);
            M_printf (i18n (2009, "Found random chat partner UIN %ld in chat group %d.\n"),
                      cont->uin, wdata);
            if (!cont || !CONTACT_DC (cont))
                break;
            if (~cont->updated & UP_INFO)
            {
                if (conn->type == TYPE_SERVER)
                    event->seq = SnacCliMetareqinfo (conn, cont);
                else
                    event->seq = CmdPktCmdMetaReqInfo (conn, cont);
            }
            event->callback (event);
            cont->dc->ip_rem  = PacketReadB4 (pak);
            cont->dc->port    = PacketRead4  (pak);
            cont->dc->ip_loc  = PacketReadB4 (pak);
            cont->dc->type    = PacketRead1  (pak);
            cont->dc->version = PacketRead2  (pak);
            /* 14 unknown bytes ignored */
            break;
        case META_SRV_UNKNOWN_270:
            /* ignored - obsoleted as of ICQ 2002 */
            if (!(mo = CONTACT_OBSOLETE (cont)))
                break;

            if ((mo->given = PacketRead1 (pak)))
            {
                mo->unknown = PacketReadB2 (pak);
                s_read (mo->description);
            }
            mo->empty = PacketRead1 (pak);

            cont->updated |= UPF_OBSOLETE;
            event->callback (event);
            break;
        default:
            M_printf ("%s: %s%04x%s\n", 
                     i18n (1945, "Unknown Meta User response"), COLSERVER, subtype, COLNONE);
            M_print  (s_dump (pak->data + pak->rpos, pak->len - pak->rpos));
            break;
    }
}

void Display_Info_Reply (Contact *cont, Packet *pak, UBYTE flags)
{
    MetaGeneral *mg;
    
    if (!(mg = CONTACT_GENERAL (cont)))
        return;

    s_read (mg->nick);
    s_read (mg->first);
    s_read (mg->last);
    s_read (mg->email);
    mg->auth = (flags & IREP_HASAUTHFLAG) ? PacketRead1 (pak) : 0;
    
    cont->updated |= UPF_GENERAL_A;
}

void Display_Ext_Info_Reply (Connection *conn, Packet *pak)
{
    Contact *cont;
    MetaGeneral *mg;
    MetaMore *mm;

    if (!(cont = ContactUIN (conn, PacketRead4 (pak))))
        return;
    
    if (!(mg = CONTACT_GENERAL (cont)) || !(mm = CONTACT_MORE (cont)))
        return;

    s_read (mg->city);
    mg->country = PacketRead2 (pak);
    mg->tz      = PacketRead1 (pak);
    s_read (mg->state);
    mm->age     = PacketRead2 (pak);
    mm->sex     = PacketRead1 (pak);
    s_read (mg->phone);
    s_read (mg->fax);
    s_read (mm->homepage);
    s_read (cont->meta_about);

    cont->updated |= UPF_GENERAL_E;

    UtilUIDisplayMeta (cont);
}

void Recv_Message (Connection *conn, Packet *pak)
{
    struct tm stamp;
    Contact *cont;
    strc_t ctext;
    const char *text;
    UDWORD uin;
    UWORD type, len;

#ifdef HAVE_TIMEZONE
    stamp.tm_sec   = -timezone;
#else
    time_t now = time (NULL);
    stamp = *localtime (&now);
#ifdef HAVE_TM_GMTOFF
    stamp.tm_sec   = -stamp.tm_gmtoff;
#endif
#endif
    uin            = PacketRead4 (pak);
    stamp.tm_year  = PacketRead2 (pak) - 1900;
    stamp.tm_mon   = PacketRead1 (pak) - 1;
    stamp.tm_mday  = PacketRead1 (pak);
    stamp.tm_hour  = PacketRead1 (pak);
    stamp.tm_min   = PacketRead1 (pak);
    /* kludge to convert that strange UTC-DST time to time_t */
    /* FIXME: The following probably only works correctly in Europe. */
    stamp.tm_isdst = -1;
    type           = PacketRead2 (pak);
    len            = PacketReadAt2 (pak, pak->rpos);
    ctext          = PacketReadL2Str (pak, NULL);
    
    cont = ContactUIN (conn, uin);
    if (!cont)
        return;
    
    if (len == ctext->len + 1 && ConvIsUTF8 (ctext->txt))
        text = ConvFrom (ctext, ENC_UTF8)->txt;
    else if (len == ctext->len + 10)
        text = c_in_to_split (ctext, cont); /* work around bug in Miranda < 0.3.1 */
    else if (len != ctext->len + 1 && type == MSG_NORM && len & 1)
        text = ConvFrom (ctext, ENC_UCS2BE)->txt;
    else
        text = c_in_to_split (ctext, cont);

    uiG.last_rcvd = cont;
    IMSrvMsg (cont, conn, mktime (&stamp),
              ContactOptionsSetVals (NULL, CO_ORIGIN, CV_ORIGIN_v5, CO_MSGTYPE, type, CO_MSGTEXT, text, 0));
}

/*
 * Inform that a user went online
 */
void IMOnline (Contact *cont, Connection *conn, UDWORD status)
{
    Event *egevent;
    UDWORD old;

    if (!cont)
        return;

    if (status == cont->status)
        return;
    
    if (status == STATUS_OFFLINE)
    {
        IMOffline (cont, conn);
        return;
    }
    
    ContactOptionsSetVal (&cont->copts, CO_TIMESEEN, time (NULL));

    old = cont->status;
    cont->status = status;
    cont->oldflags &= ~CONT_SEENAUTO;
    
    putlog (conn, NOW, cont, status, old != STATUS_OFFLINE ? LOG_CHANGE : LOG_ONLINE, 0xFFFF, "");
 
    if (!cont->group || ContactPrefVal (cont, CO_IGNORE)
        || (!ContactPrefVal (cont, CO_SHOWONOFF)  && old == STATUS_OFFLINE)
        || (!ContactPrefVal (cont, CO_SHOWCHANGE) && old != STATUS_OFFLINE)
        || (~conn->connect & CONNECT_OK))
        return;
    
    if ((egevent = QueueDequeue2 (conn, QUEUE_DEP_OSCARLOGIN, 0, 0)))
    {
        egevent->due = time (NULL) + 3;
        QueueEnqueue (egevent);
        return;
    }

    if (prG->event_cmd && *prG->event_cmd)
        EventExec (cont, prG->event_cmd, !~old ? 2 : 5, status, NULL);

    M_printf ("%s %s%*s%s ", s_now, COLCONTACT, uiG.nick_len + s_delta (cont->nick), cont->nick, COLNONE);
    M_printf (~old ? i18n (2212, "changed status to %s") : i18n (2213, "logged on (%s)"), s_status (status));
    if (cont->version && !~old)
        M_printf (" [%s]", cont->version);
    if ((status & STATUSF_BIRTH) && (!(old & STATUSF_BIRTH) || !~old))
        M_printf (" (%s)", i18n (2033, "born today"));
    M_print (".\n");

    if (prG->verbose && !~old && cont->dc)
    {
        M_printf ("    %s %s / ", i18n (1642, "IP:"), s_ip (cont->dc->ip_rem));
        M_printf ("%s:%ld    %s %d    %s (%d)\n", s_ip (cont->dc->ip_loc),
            cont->dc->port, i18n (1453, "TCP version:"), cont->dc->version,
            cont->dc->type == 4 ? i18n (1493, "Peer-to-Peer") : i18n (1494, "Server Only"),
            cont->dc->type);
    }
    TCLEvent (cont, "status", s_status (status));
}

/*
 * Inform that a user went offline
 */
void IMOffline (Contact *cont, Connection *conn)
{
    UDWORD old;

    if (!cont)
        return;
    
    if (cont->status == STATUS_OFFLINE)
        return;

    putlog (conn, NOW, cont, STATUS_OFFLINE, LOG_OFFLINE, 0xFFFF, "");

    ContactOptionsSetVal (&cont->copts, CO_TIMESEEN, time (NULL));
    old = cont->status;
    cont->status = STATUS_OFFLINE;

    if (!cont->group || ContactPrefVal (cont, CO_IGNORE) || !ContactPrefVal (cont, CO_SHOWONOFF))
        return;

    if (prG->event_cmd && *prG->event_cmd)
        EventExec (cont, prG->event_cmd, 3, old, NULL);
 
    M_printf ("%s %s%*s%s %s\n",
             s_now, COLCONTACT, uiG.nick_len + s_delta (cont->nick), cont->nick,
             COLNONE, i18n (1030, "logged off."));
    TCLEvent (cont, "status", "logged_off");
}

#define MSGICQACKSTR   ">>>"
#define MSGICQRECSTR   "<<<"
#define MSGTCPACKSTR   ConvTranslit ("\xc2\xbb\xc2\xbb\xc2\xbb", "}}}")
#define MSGTCPRECSTR   ConvTranslit ("\xc2\xab\xc2\xab\xc2\xab", "{{{")
#define MSGSSLACKSTR   ConvTranslit ("\xc2\xbb%\xc2\xbb", "}%}")
#define MSGSSLRECSTR   ConvTranslit ("\xc2\xab%\xc2\xab", "{%{")
#define MSGTYPE2ACKSTR ConvTranslit (">>\xc2\xbb", ">>}")
#define MSGTYPE2RECSTR ConvTranslit ("\xc2\xab<<", "{<<")

/*
 * Central entry point for protocol triggered output.
 */
void IMIntMsg (Contact *cont, Connection *conn, time_t stamp, UDWORD tstatus, UWORD type, const char *text, ContactOptions *opt)
{
    const char *line, *opt_text;
    const char *col = COLCONTACT;
    UDWORD opt_port, opt_bytes;
    char *p, *q;

    if (!cont || ContactPrefVal (cont, CO_IGNORE))
    {
        ContactOptionsD (opt);
        return;
    }
    if (!ContactOptionsGetStr (opt, CO_MSGTEXT, &opt_text))
        opt_text = "";
    if (!ContactOptionsGetVal (opt, CO_PORT, &opt_port))
        opt_port = 0;
    if (!ContactOptionsGetVal (opt, CO_BYTES, &opt_bytes))
        opt_bytes = 0;

    switch (type)
    {
        case INT_FILE_ACKED:
            line = s_sprintf (i18n (9999, "File transfer %s to port %s%ld%s."),
                              s_qquote (opt_text), COLQUOTE, opt_port, COLNONE);
            break;
        case INT_FILE_REJED:
            line = s_sprintf (i18n (9999, "File transfer %s rejected by peer: %s."),
                              s_qquote (opt_text), s_wordquote (text));
            break;
        case INT_FILE_ACKING:
            line = s_sprintf (i18n (9999, "Accepting file %s (%s%ld%s bytes)."),
                              s_qquote (opt_text), COLQUOTE, opt_bytes, COLNONE);
            break;
        case INT_FILE_REJING:
            line = s_sprintf (i18n (9999, "Refusing file request %s (%s%ld%s bytes): %s."),
                              s_qquote (opt_text), COLQUOTE, opt_bytes, COLNONE, s_wordquote (text));
            break;
        case INT_CHAR_REJING:
            line = s_sprintf (i18n (9999, "Refusing chat request (%s/%s) from %s%s%s."),
                              p = strdup (s_qquote (opt_text)), q = strdup (s_qquote (text)),
                              COLCONTACT, cont->nick, COLNONE);
            free (p);
            free (q);
            break;
        case INT_MSGTRY_TYPE2:
            line = s_sprintf ("%s%s %s", i18n (2293, "--="), COLSINGLE, text);
            break;
        case INT_MSGTRY_DC:
            line = s_sprintf ("%s%s %s", i18n (2294, "==="), COLSINGLE, text);
            break;
        case INT_MSGACK_TYPE2:
            col = COLACK;
            line = s_sprintf ("%s%s %s", MSGTYPE2ACKSTR, COLSINGLE, text);
            break;
        case INT_MSGACK_DC:
            col = COLACK;
            line = s_sprintf ("%s%s %s", MSGTCPACKSTR, COLSINGLE, text);
            break;
#ifdef ENABLE_SSL
        case INT_MSGACK_SSL:
            col = COLACK;
            line = s_sprintf ("%s%s %s", MSGSSLACKSTR, COLSINGLE, text);
            break;
#endif
        case INT_MSGACK_V8:
        case INT_MSGACK_V5:
            col = COLACK;
            line = s_sprintf ("%s%s %s", MSGICQACKSTR, COLSINGLE, text);
            break;
        default:
            line = "";
    }

    if (tstatus != STATUS_OFFLINE && (!cont || cont->status == STATUS_OFFLINE || !cont->group))
        M_printf ("(%s) ", s_status (tstatus));
    
    M_printf ("%s ", s_time (&stamp));
    if (cont)
        M_printf ("%s%*s%s ", col, uiG.nick_len + s_delta (cont->nick), cont->nick, COLNONE);
    
    if (prG->verbose > 1)
        M_printf ("<%d> ", type);

    for (p = q = strdup (line); *q; q++)
        if (*q == (char)0xfe)
            *q = '*';
    M_print (p);
    M_print ("\n");
    free (p);

    ContactOptionsD (opt);
}

struct History_s
{
    Connection *conn;
    Contact *cont;
    time_t stamp;
    char *msg;
};
typedef struct History_s History;

static History hist[50];
/*
 * History
 */
void HistMsg (Connection *conn, Contact *cont, time_t stamp, const char *msg)
{
    int i, j, k;

    if (hist[49].conn && hist[0].conn)
    {
        free (hist[0].msg);
        for (i = 0; i < 49; i++)
            hist[i] = hist[i + 1];
        hist[49].conn = NULL;
    }

    for (i = k = j = 0; j < 49 && hist[j].conn; j++)
        if (cont == hist[j].cont)
        {
            if (!k)
                i = j;
            if (++k == 10)
            {
                free (hist[i].msg);
                for ( ; i < 49; i++)
                    hist[i] = hist[i + 1];
                hist[49].conn = NULL;
                j--;
            }
        }
    
    hist[j].conn = conn;
    hist[j].cont = cont;
    hist[j].stamp = stamp;
    hist[j].msg = strdup (msg);
}

void HistShow (Contact *cont)
{
    int i;
    
    for (i = 0; i < 50; i++)
        if (hist[i].conn && (!cont || hist[i].cont == cont))
            M_printf ("%s%s %s%*s%s %s" COLMSGINDENT "%s\n",
                      COLDEBUG, s_time (&hist[i].stamp), COLINCOMING, uiG.nick_len + s_delta (hist[i].cont->nick),
                      hist[i].cont->nick, COLNONE, COLMESSAGE, hist[i].msg);
}

/*
 * Central entry point for incoming messages.
 */
void IMSrvMsg (Contact *cont, Connection *conn, time_t stamp, ContactOptions *opt)
{
    const char *tmp, *tmp2, *tmp3, *tmp4, *tmp5, *tmp6;
    char *cdata;
    const char *opt_text, *carr;
    UDWORD opt_type, opt_origin, opt_status, opt_bytes, opt_ref, j;
    int i;
    
    if (!cont)
    {
        ContactOptionsD (opt);
        return;
    }

    if (!ContactOptionsGetStr (opt, CO_MSGTEXT, &opt_text))
        opt_text = "";
    cdata = strdup (opt_text);
    while (*cdata && strchr ("\n\r", cdata[strlen (cdata) - 1]))
        cdata[strlen (cdata) - 1] = '\0';
    if (!ContactOptionsGetVal (opt, CO_MSGTYPE, &opt_type))
        opt_type = MSG_NORM;
    if (!ContactOptionsGetVal (opt, CO_ORIGIN, &opt_origin))
        opt_origin = CV_ORIGIN_v5;

    carr = (opt_origin == CV_ORIGIN_dc) ? MSGTCPRECSTR :
#ifdef ENABLE_SSL
           (opt_origin == CV_ORIGIN_ssl) ? MSGSSLRECSTR :
#endif
           (opt_origin == CV_ORIGIN_v8) ? MSGTYPE2RECSTR : MSGICQRECSTR;

    putlog (conn, stamp, cont,
        ContactOptionsGetVal (opt, CO_STATUS, &opt_status) ? opt_status : STATUS_OFFLINE, 
        (opt_type == MSG_AUTH_ADDED) ? LOG_ADDED : LOG_RECVD, opt_type,
        cdata);
    
    if (ContactPrefVal (cont, CO_IGNORE))
    {
        free (cdata);
        ContactOptionsD (opt);
        return;
    }

    TabAddIn (cont);

    if (uiG.idle_flag)
    {
        if ((cont != uiG.last_rcvd) || !uiG.idle_uins || !uiG.idle_msgs)
            s_repl (&uiG.idle_uins, s_sprintf ("%s %s", uiG.idle_uins && uiG.idle_msgs ? uiG.idle_uins : "", cont->nick));

        uiG.idle_msgs++;
        ReadLinePromptSet (s_sprintf ("[%s%ld%s%s]%s%s",
                           COLINCOMING, uiG.idle_msgs, uiG.idle_uins,
                           COLNONE, COLSERVER, i18n (9999, "mICQ>")));
    }

    if (prG->flags & FLAG_AUTOFINGER && ~cont->updated & UPF_AUTOFINGER &&
        ~cont->updated & UPF_SERVER && ~cont->updated & UPF_DISC)
    {
        cont->updated |= UPF_AUTOFINGER;
        IMCliInfo (conn, cont, 0);
    }

#ifdef MSGEXEC
    if (prG->event_cmd && *prG->event_cmd)
        EventExec (cont, prG->event_cmd, 1, opt_type, opt_text);
#endif
    M_printf ("\a%s %s%*s%s ", s_time (&stamp), COLINCOMING, uiG.nick_len + s_delta (cont->nick), cont->nick, COLNONE);
    
    if (ContactOptionsGetVal (opt, CO_STATUS, &opt_status) && (!cont || cont->status != opt_status || !cont->group))
        M_printf ("(%s) ", s_status (opt_status));

    if (prG->verbose > 1)
        M_printf ("<%ld> ", opt_type);

    uiG.last_rcvd = cont;
    if (cont)
    {
        s_repl (&cont->last_message, cdata);
        cont->last_time = time (NULL);
    }

    switch (opt_type & ~MSGF_MASS)
    {
        case MSGF_MASS: /* not reached here, but quiets compiler warning */
        while (1)
        {
            M_printf ("(?%lx?) %s" COLMSGINDENT "%s\n", opt_type, COLMESSAGE, opt_text);
            M_printf ("    '");
            for (j = 0; j < strlen (opt_text); j++)
                M_printf ("%c", ((cdata[j] & 0xe0) && (cdata[j] != 127)) ? cdata[j] : '.');
            M_print ("'\n");
            break;

        case MSG_NORM:
        default:
            M_printf ("%s %s" COLMSGINDENT "%s\n", carr, COLMESSAGE, cdata);
            HistMsg (conn, cont, stamp == NOW ? time (NULL) : stamp, cdata);
            TCLEvent (cont, "message", s_sprintf ("{%s}", cdata));
            TCLMessage (cont, cdata);
            break;

        case MSG_FILE:
            if (!ContactOptionsGetVal (opt, CO_BYTES, &opt_bytes))
                opt_bytes = 0;
            if (!ContactOptionsGetVal (opt, CO_REF, &opt_ref))
                opt_ref = 0;
            M_printf (i18n (9999, "requests file transfer %s of %s%ld%s bytes (sequence %s%ld%s).\n"),
                      s_qquote (cdata), COLQUOTE, opt_bytes, COLNONE, COLQUOTE, opt_ref, COLNONE);
            TCLEvent (cont, "file_request", s_sprintf ("{%s} %ld %ld", cdata, opt_bytes, opt_ref));
            break;

        case MSG_AUTO:
            M_printf ("<%s> %s" COLMSGINDENT "%s\n", i18n (2108, "auto"), COLMESSAGE, cdata);
            break;

        case MSGF_GETAUTO | MSG_GET_AWAY: 
            M_printf ("<%s> %s" COLMSGINDENT "%s\n", i18n (1972, "away"), COLMESSAGE, cdata);
            break;

        case MSGF_GETAUTO | MSG_GET_OCC:
            M_printf ("<%s> %s" COLMSGINDENT "%s\n", i18n (1973, "occupied"), COLMESSAGE, cdata);
            break;

        case MSGF_GETAUTO | MSG_GET_NA:
            M_printf ("<%s> %s" COLMSGINDENT "%s\n", i18n (1974, "not available"), COLMESSAGE, cdata);
            break;

        case MSGF_GETAUTO | MSG_GET_DND:
            M_printf ("<%s> %s" COLMSGINDENT "%s\n", i18n (1971, "do not disturb"), COLMESSAGE, cdata);
            break;

        case MSGF_GETAUTO | MSG_GET_FFC:
            M_printf ("<%s> %s" COLMSGINDENT "%s\n", i18n (1976, "free for chat"), COLMESSAGE, cdata);
            break;

        case MSGF_GETAUTO | MSG_GET_VER:
            M_printf ("<%s> %s" COLMSGINDENT "%s\n", i18n (2109, "version"), COLMESSAGE, cdata);
            break;

        case MSG_URL:
            tmp  = s_msgtok (cdata); if (!tmp)  continue;
            tmp2 = s_msgtok (NULL);  if (!tmp2) continue;
            
            M_printf ("%s %s\n%s", carr, s_msgquote (tmp), s_now);
            M_printf (i18n (2127, "       URL: %s %s\n"), carr, s_wordquote (tmp2));
            break;

        case MSG_AUTH_REQ:
            tmp = s_msgtok (cdata); if (!tmp) continue;
            tmp2 = s_msgtok (NULL);
            tmp3 = tmp4 = tmp5 = tmp6 = NULL;
            
            if (tmp2)
            {
                tmp3 = s_msgtok (NULL); if (!tmp3) continue;
                tmp4 = s_msgtok (NULL); if (!tmp4) continue;
                tmp5 = s_msgtok (NULL); if (!tmp5) continue;
                tmp6 = s_msgtok (NULL); if (!tmp6) continue;
            }
            else
            {
                tmp6 = tmp;
                tmp = NULL;
            }

            M_printf (i18n (9999, "requests authorization: %s\n"), s_msgquote (tmp6));
            
            if (tmp && strlen (tmp))
                M_printf ("%-15s %s\n", "???1:", s_wordquote (tmp));
            if (tmp2 && strlen (tmp2))
                M_printf ("%-15s %s\n", i18n (1564, "First name:"), s_wordquote (tmp2));
            if (tmp3 && strlen (tmp3))
                M_printf ("%-15s %s\n", i18n (1565, "Last name:"), s_wordquote (tmp3));
            if (tmp4 && strlen (tmp4))
                M_printf ("%-15s %s\n", i18n (1566, "Email address:"), s_wordquote (tmp4));
            if (tmp5 && strlen (tmp5))
                M_printf ("%-15s %s\n", "???5:", s_wordquote (tmp5));
            TCLEvent (cont, "authorization", "request");
            break;

        case MSG_AUTH_DENY:
            M_printf (i18n (2233, "refused authorization: %s%s%s\n"), COLMESSAGE, COLMSGINDENT, cdata);
            TCLEvent (cont, "authorization", "refused");
            break;

        case MSG_AUTH_GRANT:
            M_print (i18n (1901, "has authorized you to add them to your contact list.\n"));
            TCLEvent (cont, "authorization", "granted");
            break;

        case MSG_AUTH_ADDED:
            tmp = s_msgtok (cdata);
            if (!tmp)
            {
                M_print (i18n (1755, "has added you to their contact list.\n"));
                TCLEvent (cont, "contactlistadded", "");
                break;
            }
            tmp2 = s_msgtok (NULL); if (!tmp2) continue;
            tmp3 = s_msgtok (NULL); if (!tmp3) continue;
            tmp4 = s_msgtok (NULL); if (!tmp4) continue;

            M_printf ("%s ", s_cquote (tmp, COLCONTACT));
            M_print  (i18n (1755, "has added you to their contact list.\n"));
            M_printf ("%-15s %s\n", i18n (1564, "First name:"), s_wordquote (tmp2));
            M_printf ("%-15s %s\n", i18n (1565, "Last name:"), s_wordquote (tmp3));
            M_printf ("%-15s %s\n", i18n (1566, "Email address:"), s_wordquote (tmp4));
            TCLEvent (cont, "contactlistadded", "");
            break;

        case MSG_EMAIL:
        case MSG_WEB:
            tmp  = s_msgtok (cdata); if (!tmp)  continue;
            tmp2 = s_msgtok (NULL);  if (!tmp2) continue;
            tmp3 = s_msgtok (NULL);  if (!tmp3) continue;
            tmp4 = s_msgtok (NULL);  if (!tmp4) continue;
            tmp5 = s_msgtok (NULL);  if (!tmp5) continue;

            M_printf ("\n%s ", s_wordquote (tmp));
            M_printf ("\n??? %s", s_wordquote (tmp2));
            M_printf ("\n??? %s", s_wordquote (tmp3));

            if (opt_type == MSG_EMAIL)
            {
                M_printf (i18n (1592, "<%s> emailed you a message:\n"), s_cquote (tmp4, COLCONTACT));
                TCLEvent (cont, "mail", s_sprintf ("{%s}", tmp4));
            }
            else
            {
                M_printf (i18n (1593, "<%s> send you a web message:\n"), s_cquote (tmp4, COLCONTACT));
                TCLEvent (cont, "web", s_sprintf ("{%s}", tmp4));
            }

            M_printf ("%s\n", s_msgquote (tmp5));
            break;

        case MSG_CONTACT:
            tmp = s_msgtok (cdata); if (!tmp) continue;

            M_printf (i18n (1595, "\nContact List.\n============================================\n%d Contacts\n"),
                     i = atoi (tmp));

            while (i--)
            {
                tmp2 = s_msgtok (NULL); if (!tmp2) continue;
                tmp3 = s_msgtok (NULL); if (!tmp3) continue;
                
                M_print  (s_cquote (tmp2, COLCONTACT));
                M_printf ("\t\t\t%s\n", s_msgquote (tmp3));
            }
            break;
        }
    }
    free (cdata);
    ContactOptionsD (opt);
}
