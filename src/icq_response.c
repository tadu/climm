/* $Id$ */
/* Copyright: This file may be distributed under version 2 of the GPL licence. */

#include "micq.h"
#include "icq_response.h"
#include "util_ui.h"
#include "util_io.h"
#include "tabs.h"
#include "contact.h"
#include "util_table.h"
#include "util.h"
#include "packet.h"
#include "cmd_pkt_cmd_v5.h"
#include "cmd_pkt_v8_snac.h"
#include "preferences.h"
#include "session.h"
#include "util_str.h"
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

/* Attribute-value-pair format string */
#define AVPFMT COLSERVER "%-15s" COLNONE " %s\n"

void Meta_User (Session *sess, UDWORD uin, Packet *p)
{
    UDWORD subtype, result;

    subtype = PacketRead2 (p);
    result  = PacketRead1 (p);

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
            if (p->id != 0x42424242)
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
        default:
            M_printf (i18n (1940, "Unknown Meta User result %x.\n"), result);
            return;
    }

    switch (subtype)
    {
        Contact *cont;
        const char *tabd;
        char *data, *data2;
        UWORD wdata, day, month, i;
        UDWORD dwdata, uin;
        int tz;

        case META_SRV_ABOUT_UPDATE:
        case META_SRV_OTHER_UPDATE:
        case META_SRV_GEN_UPDATE:
        case META_SRV_PASS_UPDATE:
        case META_SRV_RANDOM_UPDATE:
            break;
        case META_SRV_SMS_OK:
                    PacketRead4 (p);
                    PacketRead2 (p);
            data  = PacketReadStrB (p);
            data2 = PacketReadStrB (p);
            M_printf (i18n (2080, "The server says about the SMS delivery:\n%s\n"), data2);
            free (data);
            free (data2);
            break;
        case META_SRV_INFO:
            Display_Info_Reply (sess, p, NULL, 0);
            /* 3 unknown bytes ignored */
            break;
        case META_SRV_GEN:
            Display_Info_Reply (sess, p, NULL, 0);

            if (sess->type == TYPE_SERVER_OLD)
            {
                if (*(data = PacketReadLNTS (p)))
                    M_printf (AVPFMT, i18n (1503, "Other Email:"), data);
                free (data);
                if (*(data = PacketReadLNTS (p)))
                    M_printf (AVPFMT, i18n (1504, "Old Email:"), data);
                free (data);
            }

            data = PacketReadLNTS (p);
            data2 = PacketReadLNTS (p);
            if (*data && *data2)
                M_printf (COLSERVER "%-15s" COLNONE " %s, %s\n", 
                         i18n (1505, "Location:"), data, data2);
            else if (*data)
                M_printf (AVPFMT, i18n (1570, "City:"), data);
            else if (*data2)
                M_printf (AVPFMT, i18n (1574, "State:"), data2);
            free (data);
            free (data2);

            if (*(data = PacketReadLNTS (p)))
                M_printf (AVPFMT, i18n (1506, "Phone:"), data);
            free (data);
            if (*(data = PacketReadLNTS (p)))
                M_printf (AVPFMT, i18n (1507, "Fax:"), data);
            free (data);
            if (*(data = PacketReadLNTS (p)))
                M_printf (AVPFMT, i18n (1508, "Street:"), data);
            free (data);
            if (*(data = PacketReadLNTS (p)))
                M_printf (AVPFMT, i18n (1509, "Cellular:"), data);
            free (data);

            if (sess->type == TYPE_SERVER)
            {
                if (*(data = PacketReadLNTS (p)))
                    M_printf (AVPFMT, i18n (1510, "Zip:"), data);
                free (data);
            }
            else if ((dwdata = PacketRead4 (p)))
                M_printf (COLSERVER "%-15s" COLNONE " %05lu\n", 
                         i18n (1510, "Zip:"), dwdata);

            wdata = PacketRead2 (p);
            if ((tabd = TableGetCountry (wdata)) != NULL)
                M_printf (COLSERVER "%-15s" COLNONE " %s\t", 
                         i18n (1511, "Country:"), tabd);
            else
                M_printf (COLSERVER "%-15s" COLNONE " %d\t", 
                         i18n (1512, "Country code:"), wdata);
            tz = (signed char) PacketRead1 (p);
            M_printf ("(UTC %+05d)\n", -100 * (tz / 2) + 30 * (tz % 2));
            wdata = PacketRead1 (p); /* publish email, 01: yes, 00: no */
            M_printf (COLSERVER "%-15s" COLNONE " %s\n", 
                i18n (1941, "Publish Email:"), wdata 
                ? i18n (1567, "No authorization needed.")
                : i18n (1568, "Must request authorization."));
            /* one unknown word ignored according to v7 doc */
            break;
        case META_SRV_MORE:
            wdata = PacketRead2 (p);
            if (wdata != 0xffff && wdata != 0)
                M_printf (COLSERVER "%-15s" COLNONE " %d\n", 
                         i18n (1575, "Age:"), wdata);
            else
                M_printf (COLSERVER "%-15s" COLNONE " %s\n", 
                         i18n (1575, "Age:"), i18n (1200, "Not entered"));

            wdata = PacketRead1 (p);
            M_printf (COLSERVER "%-15s" COLNONE " %s\n", i18n (1696, "Sex:"),
                       wdata == 1 ? i18n (1528, "female")
                     : wdata == 2 ? i18n (1529, "male")
                     :              i18n (1530, "not specified"));

            if (*(data = PacketReadLNTS (p)))
                M_printf (AVPFMT, i18n (1531, "Homepage:"), data);
            free (data);

            if (sess->type == TYPE_SERVER_OLD)
                wdata = PacketRead1 (p) + 1900;
            else
                wdata = PacketRead2 (p);
            month = PacketRead1 (p);
            day = PacketRead1 (p);

            if (month >= 1 && month <= 12 && day && day < 32 && wdata)
                M_printf (COLSERVER "%-15s" COLNONE " %02d. %s %4d\n", 
                    i18n (1532, "Born:"), day, TableGetMonth (month), 
                    wdata);
            M_printf (COLSERVER "%-15s" COLNONE " ", 
                i18n (1533, "Languages:"));
            if ((tabd = TableGetLang (wdata = PacketRead1 (p))) != NULL)
                M_printf (tabd);
            else
                M_printf ("%X", wdata);
            if ((tabd = TableGetLang (wdata = PacketRead1 (p))) != NULL)
                M_printf (", %s", tabd);
            else
                M_printf (", %X", wdata);
            if ((tabd = TableGetLang (wdata = PacketRead1 (p))) != NULL)
                M_printf (", %s", tabd);
            else
                M_printf (", %X", wdata);
            M_print ("\n");

            /* one unknown word ignored according to v7 doc */
            break;
        case META_SRV_MOREEMAIL:
            if ((i = PacketRead1 (p)))
                M_printf (COLSERVER "%-15s" COLNONE "\n", 
                         i18n (1942, "Additional Email addresses:"));
            for (; i > 0; i--)
            {
                wdata = PacketRead1 (p);
                if (*(data = PacketReadLNTS (p)))
                    M_printf ("  %s %s\n", data, 
                               wdata == 1 ? i18n (1943, "(no authorization needed)") 
                             : wdata == 0 ? i18n (1944, "(must request authorization)")
                             : "");
                free (data);
            }
            break;
        case META_SRV_WORK:
            data = PacketReadLNTS (p);
            data2 = PacketReadLNTS (p);
            if (*data && *data2)
                M_printf (COLSERVER "%-15s" COLNONE " %s, %s\n", 
                         i18n (1524, "Work Location:"), data, data2);
            else if (*data)
                M_printf (AVPFMT, i18n (1873, "Work City:"), data);
            else if (*data2)
                M_printf (AVPFMT, i18n (1874, "Work State:"), data2);
            free (data);
            free (data2);

            if (*(data = PacketReadLNTS (p)))
                M_printf (AVPFMT, i18n (1523, "Work Phone:"), data);
            free (data);
            if (*(data = PacketReadLNTS (p)))
                M_printf (AVPFMT, i18n (1521, "Work Fax:"), data);
            free (data);
            if (*(data = PacketReadLNTS (p)))
                M_printf (AVPFMT, i18n (1522, "Work Address:"), data);
            free (data);

            if (sess->type == TYPE_SERVER)
            {  
                if (*(data = PacketReadLNTS (p)))
                    M_printf (AVPFMT, i18n (1520, "Work Zip:"), data);
                free (data);
            }
            else if ((dwdata = PacketRead4 (p)))
                M_printf (COLSERVER "%-15s" COLNONE " %lu\n", 
                         i18n (1520, "Work Zip:"), dwdata);

            if ((wdata = PacketRead2 (p)))
            {
                if ((tabd = TableGetCountry (wdata)))
                    M_printf (COLSERVER "%-15s" COLNONE " %s\n", 
                             i18n (1514, "Work Country:"), tabd);
                else
                    M_printf (COLSERVER "%-15s" COLNONE " %d\n", 
                             i18n (1513, "Work Country Code:"), wdata);
            }
            if (*(data = PacketReadLNTS (p)))
                M_printf (AVPFMT, i18n (1519, "Company Name:"), data);
            free (data);
            if (*(data = PacketReadLNTS (p)))
                M_printf (AVPFMT, i18n (1518, "Department:"), data);
            free (data);
            if (*(data = PacketReadLNTS (p)))
                M_printf (AVPFMT, i18n (1517, "Job Position:"), data);
            free (data);
            if ((wdata = PacketRead2 (p))) 
                M_printf (COLSERVER "%-15s" COLNONE " %s\n", 
                         i18n (1516, "Occupation:"), TableGetOccupation (wdata));
            if (*(data = PacketReadLNTS (p)))
                M_printf (AVPFMT, i18n (1515, "Work Homepage:"), data);
            free (data);

            break;
        case META_SRV_ABOUT:
            if (*(data = PacketReadLNTS (p)))
                M_printf (COLSERVER "%-15s" COLNONE "\n " COLCLIENT "%s" COLNONE "\n",
                         i18n (1525, "About:"), data);
            free (data);
            break;
        case META_SRV_INTEREST:
            if ((i = PacketRead1 (p)))
                M_printf (COLSERVER "%-15s" COLNONE "\n",
                         i18n (1875, "Personal interests:"));
            for (; i > 0; i--)
            {
                wdata = PacketRead2 (p);
                if (*(data = PacketReadLNTS (p)))
                {
                    if ((tabd = TableGetInterest (wdata)))
                        M_printf ("  %s: %s\n", tabd, data);
                    else
                        M_printf ("  %d: %s\n", wdata, data);
                }
                free (data);
            }
            break;
        case META_SRV_BACKGROUND:
            if ((i = PacketRead1 (p)))
                M_printf (COLSERVER "%-15s" COLNONE "\n", 
                         i18n (1876, "Personal past background:"));
            for (; i > 0; i--)
            {
                wdata = PacketRead2 (p);
                if (*(data = PacketReadLNTS (p)))
                {
                    if ((tabd = TableGetPast (wdata)))
                        M_printf ("  %s: %s\n", tabd, data);
                    else
                        M_printf ("  %d: %s\n", wdata, data);
                }
                free (data);
            }
            if ((i = PacketRead1 (p)))
                M_printf (COLSERVER "%-15s" COLNONE "\n", 
                         i18n (1879, "Affiliations:"));
            for (; i > 0; i--)
            {
                wdata = PacketRead2 (p);
                if (*(data = PacketReadLNTS (p)))
                {
                    if ((tabd = TableGetAffiliation (wdata)))
                        M_printf ("  %s: %s\n", tabd, data);
                    else
                        M_printf ("  %d: %s\n", wdata, data);
                }
                free (data);
            }

            break;
        case META_SRV_WP_FOUND:
        case META_SRV_WP_LAST_USER:
            if (PacketRead2 (p) < 19)
            {
                M_printf (i18n (2141, "Search %sfailed%s.\n"), COLCLIENT, COLNONE);
                break;
            }

            Display_Info_Reply (sess, p, i18n (1498, "Info for"), 
                IREP_HASAUTHFLAG);

            switch ((wdata = PacketRead2 (p))) {
                case 0:
                    M_printf (COLSERVER "%-15s" COLNONE " %s\n", i18n (1452, "Status:"),
                             i18n (1653, "Offline"));
                    break;
                case 1:
                    M_printf (COLSERVER "%-15s" COLNONE " %s\n", i18n (1452, "Status:"),
                             i18n (1654, "Online"));
                    break;
                case 2:
                    M_printf (COLSERVER "%-15s" COLNONE " %s\n", i18n (1452, "Status:"),
                             i18n (1888, "Not webaware"));
                    break;
                default:
                    M_printf (COLSERVER "%-15s" COLNONE " %d\n", i18n (1452, "Status:"), 
                             wdata);
                    break;
            }

            wdata = PacketRead1 (p);
            M_printf (COLSERVER "%-15s" COLNONE " %s\n", i18n (1696, "Sex:"),
                       wdata == 1 ? i18n (1528, "female")
                     : wdata == 2 ? i18n (1529, "male")
                     :              i18n (1530, "not specified"));

            wdata = PacketRead1 (p);
            if (wdata != 0xff && wdata != 0)
                M_printf (COLSERVER "%-15s" COLNONE " %d\n", 
                         i18n (1575, "Age:"), wdata);
            else
                M_printf (COLSERVER "%-15s" COLNONE " %s\n", 
                         i18n (1575, "Age:"), i18n (1200, "Not entered"));

            if (subtype == META_SRV_WP_LAST_USER && (dwdata = PacketRead4 (p)))
                M_printf ("%lu %s\n", dwdata, i18n (1621, "users not returned."));
            break;
        case META_SRV_RANDOM:
            UtilCheckUIN (sess, uin = PacketRead4 (p));
            wdata = PacketRead2 (p);
            M_printf (i18n (2009, "Found random chat partner UIN %d in chat group %d.\n"),
                     uin, wdata);
            if (sess->ver > 6)
                SnacCliMetareqinfo (sess, uin);
            else
                CmdPktCmdMetaReqInfo (sess, uin);
            cont = ContactFind (uin);
            if (!cont)
                break;
            cont->outside_ip      = PacketReadB4 (p);
            cont->port            = PacketRead4  (p);
            cont->local_ip        = PacketReadB4 (p);
            cont->connection_type = PacketRead1  (p);
            cont->TCP_version     = PacketRead2  (p);
            /* 14 unknown bytes ignored */
            break;
        case META_SRV_UNKNOWN_270:
            /* 0, counter like more-email-info? */
            if (!PacketRead2 (p))
                break;
        default:
            M_printf ("%s: " COLSERVER "%04X" COLNONE "\n", 
                     i18n (1945, "Unknown Meta User response"), subtype);
            Hex_Dump (p->data + p->rpos, p->len - p->rpos);
            break;
    }
}

void Display_Rand_User (Session *sess, Packet *pak)
{
    UDWORD uin;
    unsigned char ip[4];

    if (PacketReadLeft (pak) != 37)
    {
        M_printf ("%s\n", i18n (1495, "No Random User Found"));
        return;
    }

    Hex_Dump (pak->data + pak->rpos, pak->len - pak->rpos);

    uin = PacketRead4 (pak);
    M_printf ("%-15s %lu\n", i18n (1440, "Random User:"), uin);
    ip[0] = PacketRead1 (pak);
    ip[1] = PacketRead1 (pak);
    ip[2] = PacketRead1 (pak);
    ip[3] = PacketRead1 (pak);
    M_printf ("%-15s %u.%u.%u.%u : %lu\n", i18n (1441, "IP:"), 
        ip[0], ip[1], ip[2], ip[3], PacketRead4 (pak));
    ip[0] = PacketRead1 (pak);
    ip[1] = PacketRead1 (pak);
    ip[2] = PacketRead1 (pak);
    ip[3] = PacketRead1 (pak);
    M_printf ("%-15s %u.%u.%u.%u\n", i18n (1451, "IP2:"), 
        ip[0], ip[1], ip[2], ip[3]);
    M_printf ("%-15s %s\n", i18n (1454, "Connection:"), PacketRead1 (pak) == 4
        ? i18n (1493, "Peer-to-Peer") : i18n (1494, "Server Only"));
    M_printf ("%-15s %s\n", i18n (1452, "Status:"), s_status (PacketRead4 (pak)));
    M_printf ("%-15s %d\n", i18n (1453, "TCP version:"), 
        PacketRead2 (pak));
    CmdPktCmdMetaReqInfo (sess, uin);
}

void Recv_Message (Session *sess, Packet *pak)
{
    struct tm stamp;
    UDWORD uin;
    UWORD type;
    char *text;

    uin            = PacketRead4 (pak);
    stamp.tm_year  = PacketRead2 (pak) - 1900;
    stamp.tm_mon   = PacketRead1 (pak) - 1;
    stamp.tm_mday  = PacketRead1 (pak);
    stamp.tm_hour  = PacketRead1 (pak);
    stamp.tm_min   = PacketRead1 (pak);
    /* kludge to convert that strange UTC-DST time to time_t */
    stamp.tm_sec   = -timezone;
    /* FIXME: The following probably only works correctly in Europe. */
    stamp.tm_isdst = -1;
    type           = PacketRead2 (pak);
    text           = PacketReadLNTS (pak);

    UtilCheckUIN (sess, uiG.last_rcvd_uin = uin);
    IMSrvMsg (ContactFind (uin), sess, mktime (&stamp), type, text, STATUS_ONLINE);
    free (text);
}

void Display_Info_Reply (Session *sess, Packet *pak, const char *uinline,
    unsigned int flags)
{
    char *data, *data2;
    UWORD wdata;

    if (uinline)
        M_printf ("%s " COLSERVER "%lu" COLNONE "\n",
                 uinline, PacketRead4 (pak));

    if (*(data = PacketReadLNTS (pak)))
        M_printf (COLSERVER "%-15s" COLNONE " " COLCONTACT "%s" COLNONE "\n",
                 i18n (1500, "Nickname:"), data);
    free (data);

    data = PacketReadLNTS (pak);
    data2 = PacketReadLNTS (pak);
    if (*data && *data2)
        M_printf (COLSERVER "%-15s" COLNONE " %s\t %s\n",
                 i18n (1501, "Name:"), data, data2);
    else if (*data)
        M_printf (AVPFMT, i18n (1564, "First name:"), data);
    else if (*data2)
        M_printf (AVPFMT, i18n (1565, "Last name:"), data2);
    free (data);
    free (data2);

    data = PacketReadLNTS (pak);

    if (flags & IREP_HASAUTHFLAG)
    {
        wdata = PacketRead1 (pak);
        if (*data)
            M_printf (COLSERVER "%-15s" COLNONE " %s\t%s\n", 
                     i18n (1566, "Email address:"), data,
                     wdata == 1 ? i18n (1943, "(no authorization needed)")
                                : i18n (1944, "(must request authorization)"));
    }
    else if (*data)
        M_printf (AVPFMT, i18n (1502, "Email:"), data);
    free (data);
}

void Display_Ext_Info_Reply (Session *sess, Packet *pak, const char *uinline)
{
    const char *tabd;
    char *data, *data2;
    UWORD wdata;
    int tz;

    /* Unfortunatly this is a bit different from any structure 
     * in meta user format, so it cannot be reused there. 
     */

    if (uinline)
        M_printf ("%s " COLSERVER "%lu" COLNONE "\n", uinline, 
            PacketRead4 (pak));

    data = PacketReadLNTS (pak);
    wdata = PacketRead2 (pak);
    tz = (signed char) PacketRead1 (pak);
    data2 = PacketReadLNTS (pak);

    if (*data && *data2)
        M_printf (COLSERVER "%-15s" COLNONE " %s, %s\n", 
                 i18n (1505, "Location:"), data, data2);
    else if (*data)
        M_printf (AVPFMT, i18n (1570, "City:"), data);
    else if (*data2)
        M_printf (AVPFMT, i18n (1574, "State:"), data2);
    free (data);
    free (data2);

    if ((tabd = TableGetCountry (wdata)) != NULL)
        M_printf (COLSERVER "%-15s" COLNONE " %s\t", 
                 i18n (1511, "Country:"), tabd);
    else
        M_printf (COLSERVER "%-15s" COLNONE " %d\t", 
                 i18n (1512, "Country code:"), wdata);

    M_printf ("(UTC %+05d)\n", -100 * (tz / 2) + 30 * (tz % 2));

    wdata = PacketRead2 (pak);
    if (wdata != 0xffff && wdata != 0)
        M_printf (COLSERVER "%-15s" COLNONE " %d\n", 
                 i18n (1575, "Age:"), wdata);
    else
        M_printf (COLSERVER "%-15s" COLNONE " %s\n", 
                 i18n (1575, "Age:"), i18n (1200, "Not entered"));

    wdata = PacketRead1 (pak);
    M_printf (COLSERVER "%-15s" COLNONE " %s\n", i18n (1696, "Sex:"),
               wdata == 1 ? i18n (1528, "female")
             : wdata == 2 ? i18n (1529, "male")
             :              i18n (1530, "not specified"));

    if (*(data = PacketReadLNTS (pak)))
        M_printf (AVPFMT, i18n (1506, "Phone:"), data);
    free (data);
    if (*(data = PacketReadLNTS (pak)))
        M_printf (AVPFMT, i18n (1531, "Homepage:"), data);
    free (data);
    if (*(data = PacketReadLNTS (pak)))
        M_printf (COLSERVER "%-15s" COLNONE "\n " COLCLIENT "%s" COLNONE "\n",
                 i18n (1525, "About:"), data);
    free (data);
}

/*
 * Inform that a user went online
 */
void IMOnline (Contact *cont, Session *sess, UDWORD status)
{
    UDWORD old;

    cont->seen_time = time (NULL);
    UtilUISetVersion (cont);

    if (status == cont->status)
        return;
    
    old = cont->status;
    cont->status = status;
    cont->flags &= ~CONT_SEENAUTO;

    putlog (sess, NOW, cont->uin, status, ~old ? LOG_CHANGE : LOG_ONLINE, 
        0xFFFF, "");
 
    if ((cont->flags & (CONT_TEMPORARY | CONT_IGNORE)) || (prG->flags & FLAG_QUIET) || !(sess->connect & CONNECT_OK))
        return;

    if (~old)
    {
        if (prG->sound & SFLAG_ON_CMD)
            ExecScript (prG->sound_on_cmd, cont->uin, 0, NULL);
        else if (prG->sound & SFLAG_ON_BEEP)
            printf ("\a");
    }
    M_printf ("%s " COLCONTACT "%10s" COLNONE " %s ",
             s_now, cont->nick,
             ~old ? i18n (1035, "changed status to") : i18n (1031, "logged on"));

    M_printf (~old ? "%s" : "(%s)", s_status (status));
    if (cont->version && !~old)
        M_printf (" [%s]", cont->version);
    if ((status & STATUSF_BIRTH) && (!(old & STATUSF_BIRTH) || !~old))
        M_printf (" (%s)", i18n (2033, "born today"));
    M_print (".\n");

    if (prG->verbose && !~old)
    {
        M_printf ("%-15s %s\n", i18n (1441, "IP:"), s_ip (cont->outside_ip));
        M_printf ("%-15s %s\n", i18n (1451, "IP2:"), s_ip (cont->local_ip));
        M_printf ("%-15s %d\n", i18n (1453, "TCP version:"), cont->TCP_version);
        M_printf ("%-15s %s\n", i18n (1454, "Connection:"),
                 cont->connection_type == 4 ? i18n (1493, "Peer-to-Peer") : i18n (1494, "Server Only"));
    }
}

/*
 * Inform that a user went offline
 */
void IMOffline (Contact *cont, Session *sess)
{
    putlog (sess, NOW, cont->uin, STATUS_OFFLINE, LOG_OFFLINE, 0xFFFF, "");

    cont->status = STATUS_OFFLINE;
    cont->seen_time = time (NULL);

    if ((cont->flags & (CONT_TEMPORARY | CONT_IGNORE)) || (prG->flags & FLAG_QUIET))
        return;

    if (prG->sound & SFLAG_OFF_CMD)
        ExecScript (prG->sound_off_cmd, cont->uin, 0, NULL);
    else if (prG->sound & SFLAG_OFF_BEEP)
        printf ("\a");
 
    M_printf ("%s " COLCONTACT "%10s" COLNONE " %s\n",
             s_now, cont->nick, i18n (1030, "logged off."));
}

/*
 * Central entry point for incoming messages.
 */
void IMSrvMsg (Contact *cont, Session *sess, time_t stamp, UWORD type, const char *text, UDWORD tstatus)
{
    char *cdata, *carr;
    
    if (!cont)
        return;
    if (cont->flags & CONT_IGNORE)
        return;
    if ((cont->flags & CONT_TEMPORARY) && (prG->flags & FLAG_HERMIT))
        return;

    cdata = strdup (text);
    assert (cdata);
    while (*cdata && strchr ("\n\r", cdata[strlen (cdata) - 1]))
        cdata[strlen (cdata) - 1] = '\0';

    carr = sess->type & TYPEF_ANY_SERVER ? MSGRECSTR : MSGTCPRECSTR;

    TabAddUIN (cont->uin);            /* Adds <uin> to the tab-list */

    putlog (sess, stamp, cont->uin, tstatus, 
        type == MSG_AUTH_ADDED ? LOG_ADDED : LOG_RECVD, type,
        *cdata ? "%s\n" : "%s", cdata);
    
    if (uiG.idle_flag)
    {
        char buf[2048];

        if ((cont->uin != uiG.last_rcvd_uin) || !uiG.idle_uins)
        {
            snprintf (buf, sizeof (buf), "%s %s", uiG.idle_uins && uiG.idle_msgs ? uiG.idle_uins : "", cont->nick);
            s_repl (&uiG.idle_uins, buf);
        }

        uiG.idle_msgs++;
        R_setpromptf ("[" COLINCOMING "%d%s" COLNONE "] " COLSERVER "%s" COLNONE "",
                      uiG.idle_msgs, uiG.idle_uins, i18n (1040, "mICQ> "));
    }

#ifdef MSGEXEC
    if (prG->event_cmd && strlen (prG->event_cmd))
        ExecScript (prG->event_cmd, cont->uin, type, cdata);
#endif

    if (prG->sound & SFLAG_CMD)
        ExecScript (prG->sound_cmd, cont->uin, 0, NULL);
    if (prG->sound & SFLAG_BEEP)
        printf ("\a");

    M_printf ("%s " COLINCOMING "%10s" COLNONE " ", s_time (&stamp), cont->nick);
    
    if (tstatus != STATUS_OFFLINE && (!cont || cont->status == STATUS_OFFLINE || cont->flags & CONT_TEMPORARY))
        M_printf ("(%s) ", s_status (tstatus));

    if (prG->verbose)
        M_printf ("<%d> ", type);

    uiG.last_rcvd_uin = cont->uin;
    if (cont)
    {
        s_repl (&cont->last_message, text);
        cont->last_time = time (NULL);
    }

    switch (type & ~MSGF_MASS)
    {
        const char *tmp, *tmp2, *tmp3, *tmp4, *tmp5, *tmp6;
        int i;

        while (1) {
            M_printf ("??%x?? %s'%s'%s\n'", type, COLMESSAGE COLMSGINDENT, text, COLNONE);
            for (i = 0; i < strlen (text); i++)
                M_printf ("%c", cdata[i] ? cdata[i] : '.');
            M_print ("'" COLMSGEXDENT "\n");
            break;

        case MSG_NORM:
        default:
            M_printf ("%s" COLMESSAGE COLMSGINDENT "%s" COLNONE COLMSGEXDENT "\n",
                     carr, cdata);
            break;

        case 33:
            M_printf ("<cap> " COLMESSAGE COLMSGINDENT "%s" COLNONE COLMSGEXDENT "\n",
                      cdata);
            break;

        case MSG_AUTO:
            M_printf ("<%s> " COLMESSAGE COLMSGINDENT "%s" COLNONE COLMSGEXDENT "\n",
                     i18n (2108, "auto"), cdata);
            break;

        case MSGF_GETAUTO | MSG_GET_AWAY: 
            M_printf ("<%s> " COLMESSAGE COLMSGINDENT "%s" COLNONE COLMSGEXDENT "\n", 
                     i18n (1972, "away"), cdata);
            break;

        case MSGF_GETAUTO | MSG_GET_OCC:
            M_printf ("<%s> " COLMESSAGE COLMSGINDENT "%s" COLNONE COLMSGEXDENT "\n", 
                     i18n (1973, "occupied"), cdata);
            break;

        case MSGF_GETAUTO | MSG_GET_NA:
            M_printf ("<%s> " COLMESSAGE COLMSGINDENT "%s" COLNONE COLMSGEXDENT "\n", 
                     i18n (1974, "not available"), cdata);
            break;

        case MSGF_GETAUTO | MSG_GET_DND:
            M_printf ("<%s> " COLMESSAGE COLMSGINDENT "%s" COLNONE COLMSGEXDENT "\n", 
                     i18n (1971, "do not disturb"), cdata);
            break;

        case MSGF_GETAUTO | MSG_GET_FFC:
            M_printf ("<%s> " COLMESSAGE COLMSGINDENT "%s" COLNONE COLMSGEXDENT "\n", 
                     i18n (1976, "free for chat"), cdata);
            break;

        case MSGF_GETAUTO | MSG_GET_VER:
            M_printf ("<%s> " COLMESSAGE COLMSGINDENT "%s" COLNONE COLMSGEXDENT "\n", 
                     i18n (2109, "version"), cdata);
            break;

        case MSG_URL:
            tmp  = s_msgtok (cdata); if (!tmp)  continue;
            tmp2 = s_msgtok (NULL);  if (!tmp2) continue;
            
            M_printf ("%s" COLMESSAGE "%s" COLNONE "\n%s", carr, tmp, s_now);
            M_printf (i18n (2127, "       URL: %s%s%s%s\n"), carr, COLMESSAGE, tmp2, COLNONE);
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

            M_printf (i18n (2144, "requests authorization: %s%s%s\n"),
                     COLMESSAGE COLMSGINDENT, tmp6, COLNONE);
            
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
            M_printf (i18n (2143, "refused authorization: %s%s%s\n"),
                     COLMESSAGE COLMSGINDENT, cdata, COLNONE COLMSGEXDENT);
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

            if (type == MSG_EMAIL)
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
}
