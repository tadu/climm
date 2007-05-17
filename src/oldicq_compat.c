/* Copyright: This file may be distributed under version 2 of the GPL licence.
 * $Id$
 */

#include "micq.h"
#include <assert.h>
#include "oldicq_compat.h"
#include "contact.h"
#include "connection.h"
#include "preferences.h"
#include "packet.h"
#include "im_request.h"
#include "conv.h"
#include "oldicq_client.h"
#include "im_response.h"
#include "oscar_snac.h"
#include "oscar_oldicq.h"
#include "util_ui.h"

#define META_SRV_GEN_UPDATE     100
#define META_SRV_OTHER_UPDATE   120
#define META_SRV_ABOUT_UPDATE   130
#define META_SRV_SMS_OK         150
#define META_SRV_PASS_UPDATE    170
#define META_SRV_GEN            200
#define META_SRV_WORK           210
#define META_SRV_MORE           220
#define META_SRV_ABOUT          230
#define META_SRV_INTEREST       240
#define META_SRV_BACKGROUND     250
#define META_SRV_MOREEMAIL      235
#define META_SRV_INFO           260
#define META_SRV_UNKNOWN_270    270
#define META_SRV_WP_FOUND       420
#define META_SRV_WP_LAST_USER   430
#define META_SRV_RANDOM         870
#define META_SRV_RANDOM_UPDATE  880

/* test in this order */
#define STATUSF_ICQINV       0x00000100
#define STATUSF_ICQDND       0x00000002
#define STATUSF_ICQOCC       0x00000010
#define STATUSF_ICQNA        0x00000004
#define STATUSF_ICQAWAY      0x00000001
#define STATUSF_ICQFFC       0x00000020

#define STATUS_ICQOFFLINE    0xffffffff
#define STATUS_ICQINV         STATUSF_ICQINV
#define STATUS_ICQDND        (STATUSF_ICQDND | STATUSF_ICQOCC | STATUSF_ICQAWAY)
#define STATUS_ICQOCC        (STATUSF_ICQOCC | STATUSF_ICQAWAY)
#define STATUS_ICQNA         (STATUSF_ICQNA  | STATUSF_ICQAWAY)
#define STATUS_ICQAWAY        STATUSF_ICQAWAY
#define STATUS_ICQFFC         STATUSF_ICQFFC
#define STATUS_ICQONLINE     0x00000000


void Auto_Reply (Connection *conn, Contact *cont)
{
    const char *temp = NULL;

    if (!(prG->flags & FLAG_AUTOREPLY) || !cont)
        return;

    switch (ContactClearInv (cont->status))
    {
        case imr_dnd:  temp = ContactPrefStr (cont, CO_TAUTODND);  break;
        case imr_occ:  temp = ContactPrefStr (cont, CO_TAUTOOCC);  break;
        case imr_na:   temp = ContactPrefStr (cont, CO_TAUTONA);   break;
        case imr_away: temp = ContactPrefStr (cont, CO_TAUTOAWAY); break;
        case imr_ffc:  temp = ContactPrefStr (cont, CO_TAUTOFFC);  break;
        case imr_offline: return;
        case imr_online:
            if (ContactIsInv (cont->status))
                       temp = ContactPrefStr (cont, CO_TAUTOINV);
            else
                return;
    }
    
    if (!temp || !*temp)
    {
        switch (ContactClearInv (cont->status))
        {
            case imr_dnd:  temp = ContactPrefStr (cont, CO_AUTODND);  break;
            case imr_occ:  temp = ContactPrefStr (cont, CO_AUTOOCC);  break;
            case imr_na:   temp = ContactPrefStr (cont, CO_AUTONA);   break;
            case imr_away: temp = ContactPrefStr (cont, CO_AUTOAWAY); break;
            case imr_ffc:  temp = ContactPrefStr (cont, CO_AUTOFFC);  break;
            case imr_offline: assert (0);
            case imr_online:
                assert (ContactIsInv (cont->status));
                           temp = ContactPrefStr (cont, CO_AUTOINV);
        }
    }

    IMCliMsg (cont, MSG_AUTO, temp, NULL);
}

#define s_read(s) s_repl (&s, ConvFromCont (PacketReadL2Str (pak, NULL), cont))

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
            rl_printf (i18n (2136, "Password change was %s%s%s.\n"),
                     COLCLIENT, result == META_SUCCESS ? i18n (1393, "successful")
                     : i18n (1394, "unsuccessful"), COLNONE);
            break;
        case META_SRV_ABOUT_UPDATE:
            rl_printf (i18n (2137, "About info change was %s%s%s.\n"),
                     COLCLIENT, result == META_SUCCESS ? i18n (1393, "successful")
                     : i18n (1394, "unsuccessful"), COLNONE);
            break;
        case META_SRV_GEN_UPDATE:
            rl_printf (i18n (2138, "Info change was %s%s%s.\n"),
                     COLCLIENT, result == META_SUCCESS ? i18n (1393, "successful")
                     : i18n (1394, "unsuccessful"), COLNONE);
            break;
        case META_SRV_OTHER_UPDATE:
            rl_printf (i18n (2139, "Other info change was %s%s%s.\n"),
                     COLCLIENT, result == META_SUCCESS ? i18n (1393, "successful")
                     : i18n (1394, "unsuccessful"), COLNONE);
            break;
        case META_SRV_RANDOM_UPDATE:
            if ((pak->ref & 0xffff) != 0x4242)
                rl_printf (i18n (2140, "Random chat group change was %s%s%s.\n"),
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
            rl_printf ("%s ", s_now);
            rl_printf (i18n (2141, "Search %sfailed%s.\n"), COLCLIENT, COLNONE);
            return;
        case META_READONLY:
            rl_printf ("%s %s\n", s_now, i18n (1900, "It's readonly."));
            return;
        case META_SUCCESS:
            break;
        case 0x46:
            rl_printf ("%s\n", pak->data + pak->rpos);
            return;
        default:
            rl_printf (i18n (1940, "Unknown Meta User result %lx.\n"), result);
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
                    rl_printf ("FIXME: meta reply ref %lx not found.\n", pak->ref);
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
            rl_printf (i18n (2080, "Server SMS delivery response:\n%s\n"), ConvFromServ (data));
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
            cont->meta_email = NULL;
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
                rl_printf (i18n (2141, "Search %sfailed%s.\n"), COLCLIENT, COLNONE);
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
            PacketRead2 (pak);
            if (subtype == META_SRV_WP_LAST_USER && (wdata = PacketRead2 (pak)))
                rl_printf ("%u %s\n", wdata, i18n (1621, "users not returned."));
            break;
        case META_SRV_RANDOM:
            uin = PacketRead4 (pak);
            event->cont = cont = ContactUIN (event->conn, uin);
            wdata = PacketRead2 (pak);
            rl_printf (i18n (2606, "Found random chat partner UIN %s in chat group %d.\n"),
                      cont->screen, wdata);
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
            rl_printf ("%s: %s%04x%s\n", 
                     i18n (1945, "Unknown Meta User response"), COLSERVER, subtype, COLNONE);
            rl_print  (s_dump (pak->data + pak->rpos, pak->len - pak->rpos));
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

    uin            = PacketRead4 (pak);
    stamp.tm_sec   = 0;
    stamp.tm_year  = PacketRead2 (pak) - 1900;
    stamp.tm_mon   = PacketRead1 (pak) - 1;
    stamp.tm_mday  = PacketRead1 (pak);
    stamp.tm_hour  = PacketRead1 (pak);
    stamp.tm_min   = PacketRead1 (pak);
    stamp.tm_isdst = -1;
    type           = PacketRead2 (pak);
    len            = PacketReadAt2 (pak, pak->rpos);
    ctext          = PacketReadL2Str (pak, NULL);
    
    cont = ContactUIN (conn, uin);
    if (!cont)
        return;
    
    if (len == ctext->len + 1 && ConvIsUTF8 (ctext->txt))
        text = ConvFrom (ctext, ENC_UTF8)->txt;
    else if (len == ctext->len + 10 || len == strlen (ctext->txt) + 2)
        /* work around bug in Miranda < 0.3.1 */
        text = type == MSG_NORM ? ConvFromCont (ctext, cont) : c_in_to_split (ctext, cont);
    else if (len != ctext->len + 1 && type == MSG_NORM && len & 1)
        text = ConvFrom (ctext, ENC_UCS2BE)->txt;
    else
        text = type == MSG_NORM ? ConvFromCont (ctext, cont) : c_in_to_split (ctext, cont);

    uiG.last_rcvd = cont;
    IMSrvMsg (cont, timegm (&stamp), CV_ORIGIN_v5, type, text);
}

status_t IcqToStatus (UDWORD status)
{
    status_t tmp;
    
    if (status == (UWORD)STATUS_ICQOFFLINE)
        return ims_offline;
    tmp = (status & STATUSF_ICQINV) ? ims_inv : ims_online;
    if (status & STATUSF_ICQDND)
        return ContactCopyInv (tmp, imr_dnd);
    if (status & STATUSF_ICQOCC)
        return ContactCopyInv (tmp, imr_occ);
    if (status & STATUSF_ICQNA)
        return ContactCopyInv (tmp, imr_na);
    if (status & STATUSF_ICQAWAY)
        return ContactCopyInv (tmp, imr_away);
    if (status & STATUSF_ICQFFC)
        return ContactCopyInv (tmp, imr_ffc);
    return tmp;
}

UDWORD IcqFromStatus (status_t status)
{
    UDWORD isinv = ContactIsInv (status) ? STATUSF_ICQINV : 0;
    switch (ContactClearInv (status))
    {
        case imr_offline:  return STATUS_ICQOFFLINE;
        default:
        case imr_online:   return STATUS_ICQONLINE | isinv;
        case imr_ffc:      return STATUS_ICQFFC    | isinv;
        case imr_away:     return STATUS_ICQAWAY   | isinv;
        case imr_na:       return STATUS_ICQNA     | isinv;
        case imr_occ:      return STATUS_ICQOCC    | isinv;
        case imr_dnd:      return STATUS_ICQDND    | isinv;
    }
}

#define STATUSF_ICQBIRTH     0x00080000
#define STATUSF_ICQWEBAWARE  0x00010000
#define STATUSF_ICQIP        0x00020000
#define STATUSF_ICQDC_AUTH   0x10000000
#define STATUSF_ICQDC_CONT   0x20000000

statusflag_t IcqToFlags (UDWORD status)
{
    statusflag_t flags = imf_none;
    if (status & STATUSF_ICQBIRTH)
        flags |= imf_birth;
    if (status & STATUSF_ICQWEBAWARE)
        flags |= imf_web;
    if (status & STATUSF_ICQDC_AUTH)
        flags |= imf_dcauth;
    if (status & STATUSF_ICQDC_CONT)
        flags |= imf_dccont;
    return flags;
}

UDWORD IcqFromFlags (statusflag_t flags)
{
    UDWORD status = 0;
    if (flags & imf_birth)
        status |= STATUSF_ICQBIRTH;
    if (flags & imf_web)
        status |= STATUSF_ICQWEBAWARE;
    if (flags & imf_dcauth)
        status |= STATUSF_ICQDC_AUTH;
    if (flags & imf_dccont)
        status |= STATUSF_ICQDC_CONT;
    return status;
}

UDWORD IcqIsUIN (const char *screen)
{
    UDWORD uin = 0;
    while (*screen)
    {
        if (*screen < '0' || *screen > '9')
            return 0;
        uin *= 10;
        uin += *screen - '0';
        screen++;
    }
    return uin;
}

