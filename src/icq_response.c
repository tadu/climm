/* $Id$ */
/* Copyright: This file may be distributed under version 2 of the GPL licence. */

#include "micq.h"
#include "icq_response.h"
#include "util_ui.h"
#include "util_io.h"
#include "tabs.h"
#include "contact.h"
#include "server.h"
#include "util_table.h"
#include "util.h"
#include "conv.h"
#include "packet.h"
#include "tcp.h"
#include "peer_file.h"
#include "cmd_pkt_cmd_v5.h"
#include "cmd_pkt_v8_snac.h"
#include "preferences.h"
#include "session.h"
#include "util_str.h"
#include "util_extra.h"
#include "peer_file.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#endif
#include <time.h>
#include <string.h>
#include <assert.h>

#define s_read(s) do { char *data = PacketReadLNTS (pak); s_repl (&s, c_in_to (data, cont)); free (data); } while (0)

static BOOL Meta_Read_List (Packet *pak, Extra **list, Contact *cont)
{
    UBYTE i, j;
    
    i = PacketRead1 (pak);
    for (j = 0; j < i; j++)
    {
        if (!*list)
            *list = calloc (1, sizeof (Extra));
        if (!*list)
            return FALSE;
        (*list)->data = PacketRead2 (pak);
        s_read ((*list)->text);
        if ((*list)->data || *(*list)->text)
            list = &(*list)->more;
    }
    ExtraD (*list);
    *list = NULL;
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
            M_printf ("FIXME: meta reply ref %lx not found.\n", pak->ref);
            return;
        }
        if (event->uin)
        {
            cont = ContactUIN (event->conn, event->uin);
            if (!cont)
                return;
        }
    }

    switch (subtype)
    {
        char *data, *data2;
        UWORD wdata, i, j;
        UDWORD dwdata;
        MetaGeneral *mg;
        MetaMore *mm;
        MetaEmail *me;
        MetaWork *mw;
        MetaObsolete *mo;

        case META_SRV_SMS_OK:
                    PacketRead4 (pak);
                    PacketRead2 (pak);
            data  = PacketReadStrB (pak);
            data2 = PacketReadStrB (pak);
            M_printf (i18n (2080, "Server SMS delivery response:\n%s\n"), c_in (data2));
            free (data);
            free (data2);
            break;
        case META_SRV_INFO:
            Display_Info_Reply (conn, cont, pak, 0);
            /* 3 unknown bytes ignored */

            event->callback (event);
            break;
        case META_SRV_GEN:
            Display_Info_Reply (conn, cont, pak, 0);

            if (conn->type == TYPE_SERVER_OLD)
            {
                if (!CONTACT_EMAIL (cont) || !CONTACT_EMAIL (cont->meta_email))
                    break;

                s_read (cont->meta_email->email);
                s_read (cont->meta_email->meta_email->email);
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
            if (!CONTACT_EMAIL (cont) || !CONTACT_EMAIL (cont->meta_email))
                break;
            
            i = PacketRead1 (pak);
            for (me = cont->meta_email, j = 0; j < i; j++)
            {
                if (!CONTACT_EMAIL (me->meta_email))
                    break;
                me = me->meta_email;
                me->auth = PacketRead1 (pak);
                s_read (me->email);
            }
            
            if (j < i)
                break;
            
            while (me->meta_email)
            {
                MetaEmail *met = me->meta_email;
                s_free (met->email);
                me->meta_email = met->meta_email;
                free (met);
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
            s_free (cont->meta_about);
            cont->meta_about = PacketReadLNTS (pak);

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

            Display_Info_Reply (conn, cont, pak, IREP_HASAUTHFLAG);
            mg->webaware = PacketRead2 (pak);
            mm->sex = PacketRead1 (pak);
            mm->age = PacketRead1 (pak);
            
            UtilUIDisplayMeta (cont);
            if (subtype == META_SRV_WP_LAST_USER && (dwdata = PacketRead4 (pak)))
                M_printf ("%lu %s\n", dwdata, i18n (1621, "users not returned."));
            break;
        case META_SRV_RANDOM:
            event->uin = PacketRead4 (pak);
            cont = ContactUIN (event->conn, event->uin);
            wdata = PacketRead2 (pak);
            M_printf (i18n (2009, "Found random chat partner UIN %ld in chat group %d.\n"),
                      cont->uin, wdata);
            if (!cont || !CONTACT_DC (cont))
                break;
            if (~cont->updated & UP_INFO)
            {
                if (conn->ver > 6)
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
            M_printf ("%s: " COLSERVER "%04x" COLNONE "\n", 
                     i18n (1945, "Unknown Meta User response"), subtype);
            M_print  (s_dump (pak->data + pak->rpos, pak->len - pak->rpos));
            break;
    }
}

void Display_Info_Reply (Connection *conn, Contact *cont, Packet *pak, UBYTE flags)
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
    const char *tabd;
    Contact *cont;
    MetaGeneral *mg;
    MetaMore *mo;

    if (!(cont = ContactUIN (conn, PacketRead4 (pak))))
        return;
    
    if (!(mg = CONTACT_GENERAL (cont)) || !(mo = CONTACT_MORE (cont)))
        return;

    M_printf ("%s " COLSERVER "%lu" COLNONE "\n", i18n (1967, "More Info for"), cont->uin);

    s_read (mg->city);
    mg->country = PacketRead2 (pak);
    mg->tz      = PacketRead1 (pak);
    s_read (mg->state);
    mo->age     = PacketRead2 (pak);
    mo->sex     = PacketRead1 (pak);
    s_read (mg->phone);
    s_read (mg->fax);
    s_read (mo->homepage);
    s_read (cont->meta_about);

    if (*mg->city && *mg->state)
        M_printf (COLSERVER "%-15s" COLNONE " %s, %s\n",
                 i18n (1505, "Location:"), mg->city, mg->state);
    else if (*mg->city)
        M_printf (AVPFMT, i18n (1570, "City:"), mg->city);
    else if (*mg->state)
        M_printf (AVPFMT, i18n (1574, "State:"), mg->state);

    if ((tabd = TableGetCountry (mg->country)))
        M_printf (COLSERVER "%-15s" COLNONE " %s\t", 
                 i18n (1511, "Country:"), tabd);
    else
        M_printf (COLSERVER "%-15s" COLNONE " %d\t", 
                 i18n (1512, "Country code:"), mg->country);

    M_printf ("(UTC %+05d)\n", -100 * (mg->tz / 2) + 30 * (mg->tz % 2));

    if (mo->age && ~mo->age)
        M_printf (COLSERVER "%-15s" COLNONE " %d\n", 
                 i18n (1575, "Age:"), mo->age);
    else
        M_printf (COLSERVER "%-15s" COLNONE " %s\n", 
                 i18n (1575, "Age:"), i18n (1200, "not entered"));

    M_printf (COLSERVER "%-15s" COLNONE " %s\n", i18n (1696, "Sex:"),
               mo->sex == 1 ? i18n (1528, "female")
             : mo->sex == 2 ? i18n (1529, "male")
             :                i18n (1530, "not specified"));

    if (*mg->phone)
        M_printf (AVPFMT, i18n (1506, "Phone:"), mg->phone);
    if (*mo->homepage)
        M_printf (AVPFMT, i18n (1531, "Homepage:"), mo->homepage);
    if (*cont->meta_about)
        M_printf (COLSERVER "%-15s" COLNONE "\n%s\n",
                 i18n (1525, "About:"), s_ind (cont->meta_about));
}

void Recv_Message (Connection *conn, Packet *pak)
{
    struct tm stamp;
    Contact *cont;
    char *text, *ctext;
    UDWORD uin;
    UWORD type;

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
    ctext          = PacketReadLNTS (pak);
    
    cont = ContactUIN (conn, uin);
    if (!cont)
        return;

    text = strdup (c_in_to (ctext, cont));
    free (ctext);

    uiG.last_rcvd_uin = uin;
    IMSrvMsg (cont, conn, mktime (&stamp),
              ExtraSet (NULL, EXTRA_MESSAGE, type, text));
    free (text);
}

/*
 * Inform that a user went online
 */
void IMOnline (Contact *cont, Connection *conn, UDWORD status)
{
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
    
    cont->seen_time = time (NULL);

    old = cont->status;
    cont->status = status;
    cont->flags &= ~CONT_SEENAUTO;
    
    putlog (conn, NOW, cont, status, ~old ? LOG_CHANGE : LOG_ONLINE, 0xFFFF, "");
 
    if ((cont->flags & (CONT_TEMPORARY | CONT_IGNORE)) || (prG->flags & FLAG_ULTRAQUIET)
        || ((prG->flags & FLAG_QUIET) && (old != STATUS_OFFLINE)) || (~conn->connect & CONNECT_OK))
        return;

    if (prG->event_cmd && *prG->event_cmd)
        EventExec (cont, prG->event_cmd, !~old ? 2 : 5, status, NULL);

    M_printf ("%s " COLCONTACT "%*s" COLNONE " ", s_now, uiG.nick_len + s_delta (cont->nick), cont->nick);
    M_printf (~old ? i18n (2212, "changed status to %s") : i18n (2213, "logged on (%s)"), s_status (status));
    if (cont->version && !~old)
        M_printf (" [%s]", cont->version);
    if ((status & STATUSF_BIRTH) && (!(old & STATUSF_BIRTH) || !~old))
        M_printf (" (%s)", i18n (2033, "born today"));
    M_print (".\n");

    if (prG->verbose && !~old && cont->dc)
    {
        M_printf ("%-15s %s\n", i18n (1441, "remote IP:"), s_ip (cont->dc->ip_rem));
        M_printf ("%-15s %s\n", i18n (1451, "local  IP:"), s_ip (cont->dc->ip_loc));
        M_printf ("%-15s %d\n", i18n (1453, "TCP version:"), cont->dc->version);
        M_printf ("%-15s %s\n", i18n (1454, "Connection:"),
                 cont->dc->type == 4 ? i18n (1493, "Peer-to-Peer") : i18n (1494, "Server Only"));
    }
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

    old = cont->status;
    cont->status = STATUS_OFFLINE;
    cont->seen_time = time (NULL);

    if ((cont->flags & (CONT_TEMPORARY | CONT_IGNORE)) || (prG->flags & FLAG_ULTRAQUIET))
        return;

    if (prG->event_cmd && *prG->event_cmd)
        EventExec (cont, prG->event_cmd, 3, old, NULL);
 
    M_printf ("%s " COLCONTACT "%*s" COLNONE " %s\n",
             s_now, uiG.nick_len + s_delta (cont->nick), cont->nick, i18n (1030, "logged off."));
}

#ifndef ENABLE_UTF8
#define MSGTCPACKSTR   "\xbb\xbb\xbb"
#define MSGTCPRECSTR   "\xab\xab\xab"
#define MSGTYPE2ACKSTR ">>+"
#define MSGTYPE2RECSTR "+<<"
#else
#define i19n i18n
#define MSGTCPACKSTR   i19n (2289, "Â»Â»Â»")  /* i18n (2289, "»»»") */
#define MSGTCPRECSTR   i19n (2290, "Â«Â«Â«")  /* i18n (2290, "«««") */
#define MSGTYPE2ACKSTR i19n (2291, ">>Â»")    /* i18n (2291, ">>»") */
#define MSGTYPE2RECSTR i19n (2292, "Â«<<")    /* i18n (2292, "«<<") */
#endif

/*
 * Central entry point for protocol triggered output.
 */
void IMIntMsg (Contact *cont, Connection *conn, time_t stamp, UDWORD tstatus, UWORD type, const char *text, Extra *extra)
{
    const char *line;
    const char *col = COLCONTACT; 

    if (cont)
    {
        if (cont->flags & CONT_IGNORE || ((cont->flags & CONT_TEMPORARY) && (prG->flags & FLAG_HERMIT)))
        {
            ExtraD (extra);
            return;
        }
    }

    switch (type)
    {
        case INT_FILE_ACKED:
            line = s_sprintf (i18n (2070, "File transfer '%s' to port %ld.\n"), extra->text, extra->data);
            break;
        case INT_FILE_REJED:
            line = s_sprintf (i18n (2231, "File transfer '%s' rejected by peer: %s.\n"), extra->text, text);
            break;
        case INT_FILE_ACKING:
            line = s_sprintf (i18n (2186, "Accepting file '%s' (%ld bytes).\n"), extra->text, extra->data);
            break;
        case INT_FILE_REJING:
            line = s_sprintf (i18n (2229, "Refusing file request '%s' (%ld bytes): %s.\n"), extra->text, extra->data, text);
            break;
        case INT_CHAR_REJING:
            line = s_sprintf (i18n (2230, "Refusing chat request (%s/%s) from %s.\n"), extra->text, text, cont->nick);
            break;
        case INT_MSGTRY_TYPE2:
            line = s_sprintf ("%s%s %s\n", i18n (2293, "--="), COLSINGLE, text);
            break;
        case INT_MSGTRY_DC:
            line = s_sprintf ("%s%s %s\n", i18n (2294, "==="), COLSINGLE, text);
            break;
        case INT_MSGACK_TYPE2:
            col = COLACK;
            line = s_sprintf ("%s%s %s\n", MSGTYPE2ACKSTR, COLSINGLE, text);
            break;
        case INT_MSGACK_DC:
            col = COLACK;
            line = s_sprintf ("%s%s %s\n", MSGTCPACKSTR, COLSINGLE, text);
            break;
        case INT_MSGACK_V8:
        case INT_MSGACK_V5:
            col = COLACK;
            line = s_sprintf ("%s%s %s\n", i18n (2295, ">>>"), COLSINGLE, text);
            break;
        default:
            line = s_sprintf ("\n");
    }

    if (tstatus != STATUS_OFFLINE && (!cont || cont->status == STATUS_OFFLINE || cont->flags & CONT_TEMPORARY))
        M_printf ("(%s) ", s_status (tstatus));

    M_printf ("%s ", s_time (&stamp));
    if (cont)
        M_printf ("%s%*s" COLNONE " ", col, uiG.nick_len + s_delta (cont->nick), cont->nick);
    
    if (prG->verbose > 1)
        M_printf ("<%d> ", type);
    M_print (line);

    ExtraD (extra);
}    


/*
 * Central entry point for incoming messages.
 */
void IMSrvMsg (Contact *cont, Connection *conn, time_t stamp, Extra *extra)
{
    const char *tmp, *tmp2, *tmp3, *tmp4, *tmp5, *tmp6;
    char *cdata;
    Extra *e;
    const char *e_msg_text, *carr;
    UDWORD e_msg_type;
    int i;
    
    if (!cont)
    {
        ExtraD (extra);
        return;
    }

    cdata = strdup (e_msg_text = ExtraGetS (extra, EXTRA_MESSAGE));
    assert (cdata);
    while (*cdata && strchr ("\n\r", cdata[strlen (cdata) - 1]))
        cdata[strlen (cdata) - 1] = '\0';
    e_msg_type = ExtraGet (extra, EXTRA_MESSAGE);

    carr = ExtraGet (extra, EXTRA_ORIGIN) == EXTRA_ORIGIN_dc ? MSGTCPRECSTR :
           ExtraGet (extra, EXTRA_ORIGIN) == EXTRA_ORIGIN_v8 ? MSGTYPE2RECSTR : i18n (2296, "<<<");

    putlog (conn, stamp, cont,
        (e = ExtraFind (extra, EXTRA_STATUS)) ? e->data : STATUS_OFFLINE, 
        e_msg_type == MSG_AUTH_ADDED ? LOG_ADDED : LOG_RECVD, e_msg_type,
        cdata);
    
    if (cont->flags & CONT_IGNORE || (((cont->flags & CONT_TEMPORARY) && (prG->flags & FLAG_HERMIT))))
    {
        ExtraD (extra);
        return;
    }

    TabAddUIN (cont->uin);            /* Adds <uin> to the tab-list */

    if (uiG.idle_flag)
    {
        char buf[2048];

        if ((cont->uin != uiG.last_rcvd_uin) || !uiG.idle_uins)
        {
            snprintf (buf, sizeof (buf), "%s %s", uiG.idle_uins && uiG.idle_msgs ? uiG.idle_uins : "", cont->nick);
            s_repl (&uiG.idle_uins, buf);
        }

        uiG.idle_msgs++;
        R_setpromptf ("[" COLINCOMING "%ld%s" COLNONE "] " COLSERVER "%s" COLNONE "",
                      uiG.idle_msgs, uiG.idle_uins, i18n (1040, "mICQ> "));
    }

    if (cont->flags & CONT_TEMPORARY && ~cont->updated & UPF_SERVER)
        IMCliInfo (conn, cont, 0);

#ifdef MSGEXEC
    if (prG->event_cmd && *prG->event_cmd)
        EventExec (cont, prG->event_cmd, 1, e_msg_type, e_msg_text);
#endif
    M_printf ("\a%s " COLINCOMING "%*s" COLNONE " ", s_time (&stamp), uiG.nick_len + s_delta (cont->nick), cont->nick);
    
    if ((e = ExtraFind (extra, EXTRA_STATUS)) && (!cont || cont->status != e->data || cont->flags & CONT_TEMPORARY))
        M_printf ("(%s) ", s_status (e->data));

    if (prG->verbose > 1)
        M_printf ("<%ld> ", e_msg_type);

    uiG.last_rcvd_uin = cont->uin;
    if (cont)
    {
        s_repl (&cont->last_message, cdata);
        cont->last_time = time (NULL);
    }

    switch (e_msg_type & ~MSGF_MASS)
    {
        while (1)
        {
            M_printf ("?%lx? %s%s\n", e_msg_type, COLMSGINDENT, e_msg_text);
            M_printf ("    ");
            for (i = 0; i < strlen (e_msg_text); i++)
                M_printf ("%c", cdata[i] ? cdata[i] : '.');
            M_print ("'\n");
            break;

        case MSG_NORM:
        default:
            M_printf ("%s " COLMSGINDENT "%s\n", carr, cdata);
            break;

        case TCP_MSG_FILE:
            M_printf (i18n (2259, "requests file transfer '%s' of %ld bytes (sequence %ld).\n"),
                      cdata, ExtraGet (extra, EXTRA_FILETRANS), ExtraGet (extra, EXTRA_REF));
            break;

        case MSG_AUTO:
            M_printf ("<%s> " COLMSGINDENT "%s\n", i18n (2108, "auto"), cdata);
            break;

        case MSGF_GETAUTO | MSG_GET_AWAY: 
            M_printf ("<%s> " COLMSGINDENT "%s\n", i18n (1972, "away"), cdata);
            break;

        case MSGF_GETAUTO | MSG_GET_OCC:
            M_printf ("<%s> " COLMSGINDENT "%s\n", i18n (1973, "occupied"), cdata);
            break;

        case MSGF_GETAUTO | MSG_GET_NA:
            M_printf ("<%s> " COLMSGINDENT "%s\n", i18n (1974, "not available"), cdata);
            break;

        case MSGF_GETAUTO | MSG_GET_DND:
            M_printf ("<%s> " COLMSGINDENT "%s\n", i18n (1971, "do not disturb"), cdata);
            break;

        case MSGF_GETAUTO | MSG_GET_FFC:
            M_printf ("<%s> " COLMSGINDENT "%s\n", i18n (1976, "free for chat"), cdata);
            break;

        case MSGF_GETAUTO | MSG_GET_VER:
            M_printf ("<%s> " COLMSGINDENT "%s\n", i18n (2109, "version"), cdata);
            break;

        case MSG_URL:
            tmp  = s_msgtok (cdata); if (!tmp)  continue;
            tmp2 = s_msgtok (NULL);  if (!tmp2) continue;
            
            M_printf ("%s " COLMESSAGE "%s" COLNONE "\n%s", carr, tmp, s_now);
            M_printf (i18n (2127, "       URL: %s %s%s%s\n"), carr, COLMESSAGE, tmp2, COLNONE);
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

            M_printf (i18n (2232, "requests authorization: %s%s\n"),
                      COLMSGINDENT, tmp6);
            
            if (tmp && strlen (tmp))
                M_printf ("%-15s " COLMESSAGE "%s" COLNONE "\n", "???1:", tmp);
            if (tmp2 && strlen (tmp2))
                M_printf ("%-15s " COLMESSAGE "%s" COLNONE "\n", i18n (1564, "First name:"), tmp2);
            if (tmp3 && strlen (tmp3))
                M_printf ("%-15s " COLMESSAGE "%s" COLNONE "\n", i18n (1565, "Last name:"), tmp3);
            if (tmp4 && strlen (tmp4))
                M_printf ("%-15s " COLMESSAGE "%s" COLNONE "\n", i18n (1566, "Email address:"), tmp4);
            if (tmp5 && strlen (tmp5))
                M_printf ("%-15s " COLMESSAGE "%s" COLNONE "\n", "???5:", tmp5);
            M_print (COLMSGEXDENT);
            break;

        case MSG_AUTH_DENY:
            M_printf (i18n (2233, "refused authorization: %s%s\n"), COLMSGINDENT, cdata);
            break;

        case MSG_AUTH_GRANT:
            M_print (i18n (1901, "has authorized you to add them to your contact list.\n"));
            break;

        case MSG_AUTH_ADDED:
            tmp = s_msgtok (cdata);
            if (!tmp)
            {
                M_print (i18n (1755, "has added you to their contact list.\n"));
                break;
            }
            tmp2 = s_msgtok (NULL); if (!tmp2) continue;
            tmp3 = s_msgtok (NULL); if (!tmp3) continue;
            tmp4 = s_msgtok (NULL); if (!tmp4) continue;

            M_printf ("\n" COLCONTACT "%s" COLNONE " ", tmp);
            M_print  (i18n (1755, "has added you to their contact list.\n"));
            M_printf ("%-15s " COLMESSAGE "%s" COLNONE "\n", i18n (1564, "First name:"), tmp2);
            M_printf ("%-15s " COLMESSAGE "%s" COLNONE "\n", i18n (1565, "Last name:"), tmp3);
            M_printf ("%-15s " COLMESSAGE "%s" COLNONE "\n", i18n (1566, "Email address:"), tmp4);
            break;

        case MSG_EMAIL:
        case MSG_WEB:
            tmp  = s_msgtok (cdata); if (!tmp)  continue;
            tmp2 = s_msgtok (NULL);  if (!tmp2) continue;
            tmp3 = s_msgtok (NULL);  if (!tmp3) continue;
            tmp4 = s_msgtok (NULL);  if (!tmp4) continue;
            tmp5 = s_msgtok (NULL);  if (!tmp5) continue;

            M_printf ("\n%s ", tmp);
            M_printf ("\n??? %s", tmp2);
            M_printf ("\n??? %s", tmp3);

            if (e_msg_type == MSG_EMAIL)
                M_printf (i18n (1592, "<%s> emailed you a message:\n"), tmp4);
            else
                M_printf (i18n (1593, "<%s> send you a web message:\n"), tmp4);

            M_printf (COLMESSAGE "%s" COLNONE "\n", tmp5);
            break;

        case MSG_CONTACT:
            tmp = s_msgtok (cdata); if (!tmp) continue;

            M_printf (i18n (1595, "\nContact List.\n============================================\n%d Contacts\n"),
                     i = atoi (cdata));

            while (i--)
            {
                tmp2 = s_msgtok (NULL); if (!tmp2) continue;
                tmp3 = s_msgtok (NULL); if (!tmp3) continue;
                
                M_printf (COLCONTACT "%s\t\t\t", tmp2);
                M_printf (COLMESSAGE "%s" COLNONE "\n", tmp3);
            }
            break;
        }
    }
    free (cdata);
    ExtraD (extra);
}
