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
#include "conv.h"
#include "session.h"
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
            M_print (i18n (2136, "Password change was %s%s%s.\n"),
                     COLCLIENT, result == META_SUCCESS ? i18n (1393, "successful")
                     : i18n (1394, "unsuccessful"), COLNONE);
            break;
        case META_SRV_ABOUT_UPDATE:
            M_print (i18n (2137, "About info change was %s%s%s.\n"),
                     COLCLIENT, result == META_SUCCESS ? i18n (1393, "successful")
                     : i18n (1394, "unsuccessful"), COLNONE);
            break;
        case META_SRV_GEN_UPDATE:
            M_print (i18n (2138, "Info change was %s%s%s.\n"),
                     COLCLIENT, result == META_SUCCESS ? i18n (1393, "successful")
                     : i18n (1394, "unsuccessful"), COLNONE);
            break;
        case META_SRV_OTHER_UPDATE:
            M_print (i18n (2139, "Other info change was %s%s%s.\n"),
                     COLCLIENT, result == META_SUCCESS ? i18n (1393, "successful")
                     : i18n (1394, "unsuccessful"), COLNONE);
            break;
        case META_SRV_RANDOM_UPDATE:
            M_print (i18n (2140, "Random chat group change was %s%s%s.\n"),
                     COLCLIENT, result == META_SUCCESS ? i18n (1393, "successful")
                     : i18n (1394, "unsuccessful"), COLNONE);
            break;
    }

    switch (result) /* default error handling */
    {
        case 0x32:
        case 0x14:
            Time_Stamp ();
            M_print (" ");
            M_print (i18n (2141, "Search %sfailed%s.\n"), COLCLIENT, COLNONE);
            return;
        case META_READONLY:
            Time_Stamp ();
            M_print (" %s\n", i18n (1900, "It's readonly."));
            return;
        case META_SUCCESS:
            break;
        default:
            M_print (i18n (1940, "Unknown Meta User result %x.\n"), result);
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
            M_print (i18n (2080, "The server says about the SMS delivery:\n%s\n"), data2);
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
                    M_print (AVPFMT, i18n (1503, "Other Email:"), data);
                free (data);
                if (*(data = PacketReadLNTS (p)))
                    M_print (AVPFMT, i18n (1504, "Old Email:"), data);
                free (data);
            }

            data = PacketReadLNTS (p);
            data2 = PacketReadLNTS (p);
            if (*data && *data2)
                M_print (COLSERVER "%-15s" COLNONE " %s, %s\n", 
                    i18n (1505, "Location:"), data, data2);
            else if (*data)
                M_print (AVPFMT, i18n (1570, "City:"), data);
            else if (*data2)
                M_print (AVPFMT, i18n (1574, "State:"), data2);
                free (data);
                free (data2);

            if (*(data = PacketReadLNTS (p)))
                M_print (AVPFMT, i18n (1506, "Phone:"), data);
                free (data);
            if (*(data = PacketReadLNTS (p)))
                M_print (AVPFMT, i18n (1507, "Fax:"), data);
                free (data);
            if (*(data = PacketReadLNTS (p)))
                M_print (AVPFMT, i18n (1508, "Street:"), data);
                free (data);
            if (*(data = PacketReadLNTS (p)))
                M_print (AVPFMT, i18n (1509, "Cellular:"), data);
                free (data);

            if (sess->type == TYPE_SERVER)
            {
                if (*(data = PacketReadLNTS (p)))
                    M_print (AVPFMT, i18n (1510, "Zip:"), data);
                free (data);
            }
            else if ((dwdata = PacketRead4 (p)))
                M_print (COLSERVER "%-15s" COLNONE " %05lu\n", 
                    i18n (1510, "Zip:"), dwdata);

            wdata = PacketRead2 (p);
            if ((tabd = TableGetCountry (wdata)) != NULL)
                M_print (COLSERVER "%-15s" COLNONE " %s\t", 
                    i18n (1511, "Country:"), tabd);
            else
                M_print (COLSERVER "%-15s" COLNONE " %d\t", 
                    i18n (1512, "Country code:"), wdata);
            tz = (signed char) PacketRead1 (p);
            M_print ("(UTC %+05d)\n", -100 * (tz / 2) + 30 * (tz % 2));
            wdata = PacketRead1 (p); /* publish email, 01: yes, 00: no */
            M_print (COLSERVER "%-15s" COLNONE " %s\n", 
                i18n (1941, "Publish Email:"), wdata 
                ? i18n (1567, "No authorization needed.")
                : i18n (1568, "Must request authorization."));
            /* one unknown word ignored according to v7 doc */
            break;
        case META_SRV_MORE:
            wdata = PacketRead2 (p);
            if (wdata != 0xffff && wdata != 0)
                M_print (COLSERVER "%-15s" COLNONE " %d\n", 
                    i18n (1575, "Age:"), wdata);
            else
                M_print (COLSERVER "%-15s" COLNONE " %s\n", 
                    i18n (1575, "Age:"), i18n (1200, "Not entered"));

            wdata = PacketRead1 (p);
            M_print (COLSERVER "%-15s" COLNONE " %s\n", i18n (1696, "Sex:"),
                       wdata == 1 ? i18n (1528, "female")
                     : wdata == 2 ? i18n (1529, "male")
                     :              i18n (1530, "not specified"));

            if (*(data = PacketReadLNTS (p)))
                M_print (AVPFMT, i18n (1531, "Homepage:"), data);
            free (data);

            if (sess->type == TYPE_SERVER_OLD)
                wdata = PacketRead1 (p) + 1900;
            else
                wdata = PacketRead2 (p);
            month = PacketRead1 (p);
            day = PacketRead1 (p);

            if (month >= 1 && month <= 12 && day && day < 32 && wdata)
                M_print (COLSERVER "%-15s" COLNONE " %02d. %s %4d\n", 
                    i18n (1532, "Born:"), day, TableGetMonth (month), 
                    wdata);
            M_print (COLSERVER "%-15s" COLNONE " ", 
                i18n (1533, "Languages:"));
            if ((tabd = TableGetLang (wdata = PacketRead1 (p))) != NULL)
                M_print ("%s", tabd);
            else
                M_print ("%X", wdata);
            if ((tabd = TableGetLang (wdata = PacketRead1 (p))) != NULL)
                M_print (", %s", tabd);
            else
                M_print (", %X", wdata);
            if ((tabd = TableGetLang (wdata = PacketRead1 (p))) != NULL)
                M_print (", %s", tabd);
            else
                M_print (", %X", wdata);
            M_print ("\n");

            /* one unknown word ignored according to v7 doc */
            break;
        case META_SRV_MOREEMAIL:
            if ((i = PacketRead1 (p)))
                M_print (COLSERVER "%-15s" COLNONE "\n", 
                    i18n (1942, "Additional Email addresses:"));
            for (; i > 0; i--)
            {
                wdata = PacketRead1 (p);
                if (*(data = PacketReadLNTS (p)))
                    M_print ("  %s %s\n", data, 
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
                M_print (COLSERVER "%-15s" COLNONE " %s, %s\n", 
                    i18n (1524, "Work Location:"), data, data2);
            else if (*data)
                M_print (AVPFMT, i18n (1873, "Work City:"), data);
            else if (*data2)
                M_print (AVPFMT, i18n (1874, "Work State:"), data2);
            free (data);
            free (data2);

            if (*(data = PacketReadLNTS (p)))
                M_print (AVPFMT, i18n (1523, "Work Phone:"), data);
            free (data);
            if (*(data = PacketReadLNTS (p)))
                M_print (AVPFMT, i18n (1521, "Work Fax:"), data);
            free (data);
            if (*(data = PacketReadLNTS (p)))
                M_print (AVPFMT, i18n (1522, "Work Address:"), data);
            free (data);

            if (sess->type == TYPE_SERVER)
            {  
                if (*(data = PacketReadLNTS (p)))
                    M_print (AVPFMT, i18n (1520, "Work Zip:"), data);
                free (data);
            }
            else if ((dwdata = PacketRead4 (p)))
                M_print (COLSERVER "%-15s" COLNONE " %lu\n", 
                    i18n (1520, "Work Zip:"), dwdata);

            if ((wdata = PacketRead2 (p)))
            {
                if ((tabd = TableGetCountry (wdata)))
                    M_print (COLSERVER "%-15s" COLNONE " %s\n", 
                        i18n (1514, "Work Country:"), tabd);
                else
                    M_print (COLSERVER "%-15s" COLNONE " %d\n", 
                        i18n (1513, "Work Country Code:"), wdata);
            }
            if (*(data = PacketReadLNTS (p)))
                M_print (AVPFMT, i18n (1519, "Company Name:"), data);
            free (data);
            if (*(data = PacketReadLNTS (p)))
                M_print (AVPFMT, i18n (1518, "Department:"), data);
            free (data);
            if (*(data = PacketReadLNTS (p)))
                M_print (AVPFMT, i18n (1517, "Job Position:"), data);
            free (data);
            if ((wdata = PacketRead2 (p))) 
                M_print (COLSERVER "%-15s" COLNONE " %s\n", 
                    i18n (1516, "Occupation:"), TableGetOccupation (wdata));
            if (*(data = PacketReadLNTS (p)))
                M_print (AVPFMT, i18n (1515, "Work Homepage:"), data);
            free (data);

            break;
        case META_SRV_ABOUT:
            if (*(data = PacketReadLNTS (p)))
                M_print (COLSERVER "%-15s" COLNONE "\n " COLCLIENT 
                    "%s" COLNONE "\n", i18n (1525, "About:"), data);
            free (data);
            break;
        case META_SRV_INTEREST:
            if ((i = PacketRead1 (p)))
                M_print (COLSERVER "%-15s" COLNONE "\n",
                    i18n (1875, "Personal interests:"));
            for (; i > 0; i--)
            {
                wdata = PacketRead2 (p);
                if (*(data = PacketReadLNTS (p)))
                {
                    if ((tabd = TableGetInterest (wdata)))
                        M_print ("  %s: %s\n", tabd, data);
                    else
                        M_print ("  %d: %s\n", wdata, data);
                }
                free (data);
            }
            break;
        case META_SRV_BACKGROUND:
            if ((i = PacketRead1 (p)))
                M_print (COLSERVER "%-15s" COLNONE "\n", 
                    i18n (1876, "Personal past background:"));
            for (; i > 0; i--)
            {
                wdata = PacketRead2 (p);
                if (*(data = PacketReadLNTS (p)))
                {
                    if ((tabd = TableGetPast (wdata)))
                        M_print ("  %s: %s\n", tabd, data);
                    else
                        M_print ("  %d: %s\n", wdata, data);
                }
                free (data);
            }
            if ((i = PacketRead1 (p)))
                M_print (COLSERVER "%-15s" COLNONE "\n", 
                    i18n (1879, "Affiliations:"));
            for (; i > 0; i--)
            {
                wdata = PacketRead2 (p);
                if (*(data = PacketReadLNTS (p)))
                {
                    if ((tabd = TableGetAffiliation (wdata)))
                        M_print ("  %s: %s\n", tabd, data);
                    else
                        M_print ("  %d: %s\n", wdata, data);
                }
                free (data);
            }

            break;
        case META_SRV_WP_FOUND:
        case META_SRV_WP_LAST_USER:
            if (PacketRead2 (p) < 19)
            {
                M_print (i18n (2141, "Search %sfailed%s.\n"), COLCLIENT, COLNONE);
                break;
            }

            Display_Info_Reply (sess, p, i18n (1498, "Info for"), 
                IREP_HASAUTHFLAG);

            switch ((wdata = PacketRead2 (p))) {
                case 0:
                    M_print (COLSERVER "%-15s" COLNONE " %s\n", i18n (1452, "Status:"),
                        i18n (1653, "Offline"));
                    break;
                case 1:
                    M_print (COLSERVER "%-15s" COLNONE " %s\n", i18n (1452, "Status:"),
                        i18n (1654, "Online"));
                    break;
                case 2:
                    M_print (COLSERVER "%-15s" COLNONE " %s\n", i18n (1452, "Status:"),
                        i18n (1888, "Not webaware"));
                    break;
                default:
                    M_print (COLSERVER "%-15s" COLNONE " %d\n", i18n (1452, "Status:"), 
                        wdata);
                    break;
            }

            wdata = PacketRead1 (p);
            M_print (COLSERVER "%-15s" COLNONE " %s\n", i18n (1696, "Sex:"),
                       wdata == 1 ? i18n (1528, "female")
                     : wdata == 2 ? i18n (1529, "male")
                     :              i18n (1530, "not specified"));

            wdata = PacketRead1 (p);
            if (wdata != 0xff && wdata != 0)
                M_print (COLSERVER "%-15s" COLNONE " %d\n", 
                    i18n (1575, "Age:"), wdata);
            else
                M_print (COLSERVER "%-15s" COLNONE " %s\n", 
                    i18n (1575, "Age:"), i18n (1200, "Not entered"));

            if (subtype == META_SRV_WP_LAST_USER && (dwdata = PacketRead4 (p)))
                M_print ("%lu %s\n", dwdata, i18n (1621, "users not returned."));
            break;
        case META_SRV_RANDOM:
            UtilCheckUIN (sess, uin = PacketRead4 (p));
            wdata = PacketRead2 (p);
            M_print (i18n (2009, "Found random chat partner UIN %d in chat group %d.\n"),
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
            M_print ("%s: " COLSERVER "%04X" COLNONE "\n", 
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
        M_print ("%s\n", i18n (1495, "No Random User Found"));
        return;
    }

    Hex_Dump (pak->data + pak->rpos, pak->len - pak->rpos);

    uin = PacketRead4 (pak);
    M_print ("%-15s %lu\n", i18n (1440, "Random User:"), uin);
    ip[0] = PacketRead1 (pak);
    ip[1] = PacketRead1 (pak);
    ip[2] = PacketRead1 (pak);
    ip[3] = PacketRead1 (pak);
    M_print ("%-15s %u.%u.%u.%u : %lu\n", i18n (1441, "IP:"), 
        ip[0], ip[1], ip[2], ip[3], PacketRead4 (pak));
    ip[0] = PacketRead1 (pak);
    ip[1] = PacketRead1 (pak);
    ip[2] = PacketRead1 (pak);
    ip[3] = PacketRead1 (pak);
    M_print ("%-15s %u.%u.%u.%u\n", i18n (1451, "IP2:"), 
        ip[0], ip[1], ip[2], ip[3]);
    M_print ("%-15s %s\n", i18n (1454, "Connection:"), PacketRead1 (pak) == 4
        ? i18n (1493, "Peer-to-Peer") : i18n (1494, "Server Only"));
    M_print ("%-15s ", i18n (1452, "Status:"));
    Print_Status (PacketRead4 (pak));
    M_print ("\n");
    M_print ("%-15s %d\n", i18n (1453, "TCP version:"), 
        PacketRead2 (pak));
    CmdPktCmdMetaReqInfo (sess, uin);
}

void Recv_Message (Session *sess, Packet *pak)
{
    struct tm stamp;
    UWORD type;
    char *text;

    uiG.last_rcvd_uin = PacketRead4 (pak);
    stamp.tm_year     = PacketRead2 (pak) - 1900;
    stamp.tm_mon      = PacketRead1 (pak) - 1;
    stamp.tm_mday     = PacketRead1 (pak);
    stamp.tm_hour     = PacketRead1 (pak);
    stamp.tm_min      = PacketRead1 (pak);
    /* kludge to convert that strange UTC-DST time to time_t */
    stamp.tm_sec      = -timezone;
    /* FIXME The following proabably only works correctly in Europe. */
    stamp.tm_isdst    = -1;
    type              = PacketRead2 (pak);
    text              = PacketReadLNTS (pak);

    Do_Msg (sess, mktime (&stamp), type, text, 
        uiG.last_rcvd_uin, STATUS_ONLINE);
    free (text);
}

void Display_Info_Reply (Session *sess, Packet *pak, const char *uinline,
    unsigned int flags)
{
    char *data, *data2;
    UWORD wdata;

    if (uinline)
        M_print ("%s " COLSERVER "%lu" COLNONE "\n", uinline, 
            PacketRead4 (pak));

    if (*(data = PacketReadLNTS (pak)))
        M_print (COLSERVER "%-15s" COLNONE " " COLCONTACT "%s" 
            COLNONE "\n", i18n (1500, "Nickname:"), data);
                free (data);

    data = PacketReadLNTS (pak);
    data2 = PacketReadLNTS (pak);
    if (*data && *data2)
        M_print (COLSERVER "%-15s" COLNONE " %s\t %s\n", 
            i18n (1501, "Name:"), data, data2);
    else if (*data)
        M_print (AVPFMT, i18n (1564, "First name:"), data);
    else if (*data2)
        M_print (AVPFMT, i18n (1565, "Last name:"), data2);
    free (data);
    free (data2);

    data = PacketReadLNTS (pak);

    if (flags & IREP_HASAUTHFLAG)
    {
        wdata = PacketRead1 (pak);
        if (*data)
            M_print (COLSERVER "%-15s" COLNONE " %s\t%s\n", 
                i18n (1566, "Email address:"), data,
                wdata == 1 ? i18n (1943, "(no authorization needed)")
                : i18n (1944, "(must request authorization)"));
    }
    else if (*data)
        M_print (AVPFMT, i18n (1502, "Email:"), data);
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
        M_print ("%s " COLSERVER "%lu" COLNONE "\n", uinline, 
            PacketRead4 (pak));

    data = PacketReadLNTS (pak);
    wdata = PacketRead2 (pak);
    tz = (signed char) PacketRead1 (pak);
    data2 = PacketReadLNTS (pak);

    if (*data && *data2)
        M_print (COLSERVER "%-15s" COLNONE " %s, %s\n", 
            i18n (1505, "Location:"), data, data2);
    else if (*data)
        M_print (AVPFMT, i18n (1570, "City:"), data);
    else if (*data2)
        M_print (AVPFMT, i18n (1574, "State:"), data2);
    free (data);
    free (data2);

    if ((tabd = TableGetCountry (wdata)) != NULL)
        M_print (COLSERVER "%-15s" COLNONE " %s\t", 
            i18n (1511, "Country:"), tabd);
    else
        M_print (COLSERVER "%-15s" COLNONE " %d\t", 
            i18n (1512, "Country code:"), wdata);

    M_print ("(UTC %+05d)\n", -100 * (tz / 2) + 30 * (tz % 2));

    wdata = PacketRead2 (pak);
    if (wdata != 0xffff && wdata != 0)
        M_print (COLSERVER "%-15s" COLNONE " %d\n", 
            i18n (1575, "Age:"), wdata);
    else
        M_print (COLSERVER "%-15s" COLNONE " %s\n", 
            i18n (1575, "Age:"), i18n (1200, "Not entered"));

    wdata = PacketRead1 (pak);
    M_print (COLSERVER "%-15s" COLNONE " %s\n", i18n (1696, "Sex:"),
               wdata == 1 ? i18n (1528, "female")
             : wdata == 2 ? i18n (1529, "male")
             :              i18n (1530, "not specified"));

    if (*(data = PacketReadLNTS (pak)))
        M_print (AVPFMT, i18n (1506, "Phone:"), data);
    free (data);
    if (*(data = PacketReadLNTS (pak)))
        M_print (AVPFMT, i18n (1531, "Homepage:"), data);
    free (data);
    if (*(data = PacketReadLNTS (pak)))
        M_print (COLSERVER "%-15s" COLNONE "\n " COLCLIENT 
            "%s" COLNONE "\n", i18n (1525, "About:"), data);
    free (data);
}

/*
 * Central entry point for incoming messages.
 */
void Do_Msg (Session *sess, time_t stamp, UWORD type, const char *text, UDWORD uin, UDWORD tstatus)
{
    char *cdata, *tmp = NULL;
    char *url_url, *url_desc;
    char sep = ConvSep ();
    Contact *cont;
    Session *serv;
    int x, m;
    
    cdata = strdup (text);

    TabAddUIN (uin);            /* Adds <uin> to the tab-list */

    /* Thanks rtc for breaking and not fixing it */
    for (serv = sess; serv && ~serv->type & TYPEF_SERVER; )
        serv = serv->parent;
    assert (serv);

    UtilCheckUIN (serv, uin);

    putlog (sess, stamp, uin, tstatus, 
        type == USER_ADDED_MESS ? LOG_ADDED : LOG_RECVD, type,
        *cdata ? "%s\n" : "%s", cdata);
    
    cont = ContactFind (uin);
    if (cont && (cont->flags & CONT_IGNORE))
        return;
    if (!cont && (prG->flags & FLAG_HERMIT))
        return;

    if (uiG.idle_flag)
    {
        char buf[2048];

        if ((uin != uiG.last_rcvd_uin) || !uiG.idle_uins)
        {
            snprintf (buf, sizeof (buf), "%s %s", uiG.idle_uins && uiG.idle_msgs ? uiG.idle_uins : "", ContactFindName (uin));
            if (uiG.idle_uins)
                free (uiG.idle_uins);
            uiG.idle_uins = strdup (buf);
        }

        uiG.idle_msgs++;
        R_setpromptf ("[" CYAN BOLD "%d%s" COLNONE "] " COLSERVER "%s" COLNONE "",
                      uiG.idle_msgs, uiG.idle_uins, i18n (1040, "mICQ> "));
    }

#ifdef MSGEXEC
    if (prG->event_cmd && strlen (prG->event_cmd))
        ExecScript (prG->event_cmd, uin, type, cdata);
#endif

    if (prG->sound & SFLAG_BEEP)
        printf ("\a");

    Time_Output (stamp);
    M_print (" " CYAN BOLD "%10s" COLNONE " ", ContactFindName (uin));
    
    if (tstatus != STATUS_OFFLINE && (!cont || cont->status == STATUS_OFFLINE || cont->flags & CONT_TEMPORARY))
    {
        M_print ("(");
        Print_Status (tstatus);
        M_print (") ");
    }

    uiG.last_rcvd_uin = uin;
    if (cont)
    {
        if (cont->last_message)
            free (cont->last_message);
        cont->last_message = strdup (text);
        cont->last_time = time (NULL);
    }

    switch (type & ~MESSF_MASS)
    {
        case USER_ADDED_MESS:
            if (*cdata)
            {
                tmp = strchr (cdata, sep);
                if (tmp == NULL)
                {
                    M_print (i18n (1585, "Ack!!!!!!!  Bad packet\n"));
                    return;
                }
                *tmp = '\0';
                M_print ("\n" COLCONTACT "%s" COLNONE " ", cdata);
            }

            M_print (i18n (1755, "has added you to their contact list.\n"));

            if (tmp == NULL)
                break;

            tmp++;
            cdata = tmp;
            tmp = strchr (tmp, sep);
            if (tmp == NULL)
            {
                M_print (i18n (1585, "Ack!!!!!!!  Bad packet\n"));
                return;
            }
            *tmp = 0;
            M_print ("%-15s " COLMESSAGE "%s" COLNONE "\n", i18n (1564, "First name:"), cdata);
            tmp++;
            cdata = tmp;
            tmp = strchr (tmp, sep);
            if (tmp == NULL)
            {
                M_print (i18n (1585, "Ack!!!!!!!  Bad packet\n"));
                return;
            }
            *tmp = 0;
            M_print ("%-15s " COLMESSAGE "%s" COLNONE "\n", i18n (1565, "Last name:"), cdata);
            tmp++;
            cdata = tmp;
            tmp = strchr (tmp, sep);
            *tmp = 0;
            M_print ("%-15s " COLMESSAGE "%s" COLNONE "\n", i18n (1566, "Email address:"), cdata);
            break;
        case AUTH_REQ_MESS:
            tmp = strchr (cdata, sep);
            *tmp = 0;
            M_print ("" COLCONTACT "%10s" COLNONE "%s\n", cdata,
                     i18n (1590, "has requested your authorization to be added to their contact list."));
            tmp++;
            cdata = tmp;
            tmp = strchr (tmp, sep);
            if (tmp == NULL)
            {
                M_print (i18n (1585, "Ack!!!!!!!  Bad packet\n"));
                return;
            }
            *tmp = 0;
            M_print ("%-15s " COLMESSAGE "%s" COLNONE "\n", i18n (1564, "First name:"), cdata);
            tmp++;
            cdata = tmp;
            tmp = strchr (tmp, sep);
            if (tmp == NULL)
            {
                M_print (i18n (1585, "Ack!!!!!!!  Bad packet\n"));
                return;
            }
            *tmp = 0;
            M_print ("%-15s " COLMESSAGE "%s" COLNONE "\n", i18n (1565, "Last name:"), cdata);
            tmp++;
            cdata = tmp;
            tmp = strchr (tmp, sep);
            if (tmp == NULL)
            {
                M_print (i18n (1585, "Ack!!!!!!!  Bad packet\n"));
                return;
            }
            *tmp = 0;
            M_print ("%-15s " COLMESSAGE "%s" COLNONE "\n", i18n (1566, "Email address:"), cdata);
            tmp++;
            cdata = tmp;
            tmp = strchr (tmp, sep);
            if (tmp == NULL)
            {
                M_print (i18n (1585, "Ack!!!!!!!  Bad packet\n"));
                return;
            }
            *tmp = 0;
            tmp++;
            cdata = tmp;
            tmp = strchr (tmp, sep);
            if (tmp == NULL)
            {
                M_print (i18n (1585, "Ack!!!!!!!  Bad packet\n"));
                return;
            }
            *tmp = 0;
            M_print ("%-15s " COLMESSAGE "%s" COLNONE "\n", i18n (1591, "Reason:"), cdata);
            break;
        case EMAIL_MESS:
        case WEB_MESS:
            tmp = strchr (cdata, sep);
            *tmp = 0;
            M_print ("\n%s ", cdata);
            tmp++;
            cdata = tmp;
            tmp = strchr (cdata, sep);
            tmp++;
            cdata = tmp;

            tmp = strchr (cdata, sep);
            tmp++;
            cdata = tmp;

            tmp = strchr (cdata, sep);
            *tmp = 0;
            if (type == EMAIL_MESS)
                M_print (i18n (1592, "<%s> emailed you a message:\n"), cdata);
            else
                M_print (i18n (1593, "<%s> send you a web message:\n"), cdata);
            tmp++;
            cdata = tmp;
            tmp = strchr (cdata, sep);
            *tmp = 0;
            if (prG->verbose)
            {
                M_print ("??? '%s'\n", cdata);
            }
            tmp++;
            cdata = tmp;
            M_print (COLMESSAGE "%s" COLNONE "\n", cdata);
            break;
        case URL_MESS:
            url_desc = cdata;
            url_url = strchr (cdata, sep);
            if (url_url == NULL)
            {
                url_url = url_desc;
                url_desc = "";
            }
            else
            {
                *url_url = '\0';
                url_url++;
            }

            M_print ("%s" COLMESSAGE "%s" COLNONE "\n",
                     sess->type & TYPEF_ANY_SERVER ? MSGRECSTR : MSGTCPRECSTR,
                     url_desc);
            Time_Stamp ();
            M_print (i18n (2127, "       URL: %s%s%s%s\n"), 
                     sess->type & TYPEF_ANY_SERVER ? MSGRECSTR : MSGTCPRECSTR,
                     COLMESSAGE, url_url, COLNONE);
            break;
        case CONTACT_MESS:
            tmp = strchr (cdata, sep);
            *tmp = 0;
            M_print (i18n (1595, "\nContact List.\n============================================\n%d Contacts\n"),
                     atoi (cdata));
            tmp++;
            m = atoi (cdata);
            for (x = 0; m > x; x++)
            {
                cdata = tmp;
                tmp = strchr (tmp, sep);
                *tmp = 0;
                M_print (COLCONTACT "%s\t\t\t", cdata);
                tmp++;
                cdata = tmp;
                tmp = strchr (tmp, sep);
                *tmp = 0;
                M_print (COLMESSAGE "%s" COLNONE "\n", cdata);
                tmp++;
            }
            break;
        case TCP_MSG_AUTO:
            M_print ("<%s>" COLMESSAGE COLMSGINDENT "%s" COLNONE COLMSGEXDENT "\n", 
                i18n (2108, "auto"), cdata);
            break;
        case TCP_MSG_GET_AWAY: 
            M_print ("<%s>" COLMESSAGE COLMSGINDENT "%s" COLNONE COLMSGEXDENT "\n", 
                i18n (1972, "away"), cdata);
            break;
        case TCP_MSG_GET_OCC:
            M_print ("<%s>" COLMESSAGE COLMSGINDENT "%s" COLNONE COLMSGEXDENT "\n", 
                i18n (1973, "occupied"), cdata);
            break;
        case TCP_MSG_GET_NA:
            M_print ("<%s>" COLMESSAGE COLMSGINDENT "%s" COLNONE COLMSGEXDENT "\n", 
                i18n (1974, "not available"), cdata);
            break;
        case TCP_MSG_GET_DND:
            M_print ("<%s>" COLMESSAGE COLMSGINDENT "%s" COLNONE COLMSGEXDENT "\n", 
                i18n (1971, "do not disturb"), cdata);
            break;
        case TCP_MSG_GET_FFC:
            M_print ("<%s>" COLMESSAGE COLMSGINDENT "%s" COLNONE COLMSGEXDENT "\n", 
                i18n (1976, "free for chat"), cdata);
            break;
        case TCP_MSG_GET_VER:
            M_print ("<%s>" COLMESSAGE COLMSGINDENT "%s" COLNONE COLMSGEXDENT "\n", 
                i18n (2109, "version"), cdata);
            break;
        default:
            while (*cdata && (cdata[strlen (cdata) - 1] == '\n' || cdata[strlen (cdata) - 1] == '\r'))
                cdata[strlen (cdata) - 1] = '\0';
            M_print ("%s" COLMESSAGE COLMSGINDENT "%s" COLNONE COLMSGEXDENT "\n", 
                sess->type & TYPEF_ANY_SERVER ? MSGRECSTR : MSGTCPRECSTR, cdata);
            break;
    }
}
