/* $Id$ */
/* Copyright ? */

#include "micq.h"
#include "icq_response.h"
#include "util_ui.h"
#include "util_io.h"
#include "tabs.h"
#include "contact.h"
#include "util_table.h"
#include "util.h"
#include "conv.h"
#include "packet.h"
#include "cmd_pkt_cmd_v5.h"
#include "preferences.h"
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
#define AVPFMT COLSERV "%-15s" COLNONE " %s\n"

void Meta_User (Session *sess, UDWORD uin, Packet *p)
{
    UDWORD subtype, result;

    subtype = PacketRead2 (p);
    result  = PacketRead1 (p);

    switch (subtype)
    {
        case META_SRV_PASS:
            M_print (i18n (1197, "Password change was " COLCLIENT "%s" COLNONE ".\n"),
                     result == 0xA ? i18n (1393, "successful") : i18n (1394, "unsuccessful"));
            break;
        case META_SRV_ABOUT_UPDATE:
            M_print (i18n (1395, "About info change was " COLCLIENT "%s" COLNONE ".\n"),
                     result == 0xA ? i18n (1393, "successful") : i18n (1394, "unsuccessful"));
            break;
        case META_SRV_GEN_UPDATE:
            M_print (i18n (1396, "Info change was " COLCLIENT "%s" COLNONE ".\n"),
                     result == 0xA ? i18n (1393, "successful") : i18n (1394, "unsuccessful"));
            break;
        case META_SRV_OTHER_UPDATE:
            M_print (i18n (1397, "Other info change was " COLCLIENT "%s" COLNONE ".\n"),
                     result == 0xA ? i18n (1393, "successful") : i18n (1394, "unsuccessful"));
            break;
    }

    switch (result) /* default error handling */
    {
        case 0x32:
        case 0x14:
            Time_Stamp ();
            M_print (" %s\n", i18n (1398, "Search " COLCLIENT "failed" COLNONE "."));
            return;
        case 0x1E:
            Time_Stamp ();
            M_print (" %s\n", i18n (1900, "It's readonly."));
            return;
        case 0x0A:
            break;
        default:
            M_print (i18n (1940, "Unknown Meta User result %x.\n"), result);
            return;
    }

    switch (subtype)
    {
        const char *tabd, *data, *data2;
        UWORD wdata, day, month, i;
        UDWORD len, dwdata;
        int tz;

        case META_SRV_ABOUT_UPDATE:
        case META_SRV_OTHER_UPDATE:
        case META_SRV_GEN_UPDATE:
        case META_SRV_PASS:
            break;
        case 0x0104: /* 2001-short-info */
            Display_Info_Reply (sess, p, NULL, 0);
            /* 3 unknown bytes ignored */
            break;
        case META_SRV_GEN: /* 0x00C8, main-home-info */
            Display_Info_Reply (sess, p, NULL, 0);

            if (sess->type == TYPE_SERVER_OLD)
            {
                if (*(data = PacketReadLNTS (p)))
                    M_print (AVPFMT, i18n (1503, "Other Email:"), data);
                if (*(data = PacketReadLNTS (p)))
                    M_print (AVPFMT, i18n (1504, "Old Email:"), data);
            }

            data = PacketReadLNTS (p);
            data2 = PacketReadLNTS (p);
            if (*data && *data2)
                M_print (COLSERV "%-15s" COLNONE " %s, %s\n", 
                    i18n (1505, "Location:"), data, data2);
            else if (*data)
                M_print (AVPFMT, i18n (1570, "City:"), data);
            else if (*data2)
                M_print (AVPFMT, i18n (1574, "State:"), data2);

            if (*(data = PacketReadLNTS (p)))
                M_print (AVPFMT, i18n (1506, "Phone:"), data);
            if (*(data = PacketReadLNTS (p)))
                M_print (AVPFMT, i18n (1507, "Fax:"), data);
            if (*(data = PacketReadLNTS (p)))
                M_print (AVPFMT, i18n (1508, "Street:"), data);
            if (*(data = PacketReadLNTS (p)))
                M_print (AVPFMT, i18n (1509, "Cellular:"), data);

            if (sess->type == TYPE_SERVER)
            {
                if (*(data = PacketReadLNTS (p)))
                    M_print (AVPFMT, i18n (1510, "Zip:"), data);
            }
            else if ((dwdata = PacketRead4 (p)))
                M_print (COLSERV "%-15s" COLNONE " %05lu\n", 
                    i18n (1510, "Zip:"), dwdata);

            wdata = PacketRead2 (p);
            if ((tabd = TableGetCountry (wdata)) != NULL)
                M_print (COLSERV "%-15s" COLNONE " %s\t", 
                    i18n (1511, "Country:"), tabd);
            else
                M_print (COLSERV "%-15s" COLNONE " %d\t", 
                    i18n (1512, "Country code:"), wdata);
            tz = (signed char) PacketRead1 (p);
            M_print ("(UTC %+05d)\n", -100 * (tz / 2) + 30 * (tz % 2));
            wdata = PacketRead1 (p); /* publish email, 01: yes, 00: no */
            M_print (COLSERV "%-15s" COLNONE " %s\n", 
                i18n (1941, "Publish Email:"), wdata 
                ? i18n (1567, "No authorization needed." COLNONE " ")
                : i18n (1568, "Must request authorization." COLNONE " "));
            /* one unknown word ignored according to v7 doc */
            break;
        case META_SRV_MORE: /* 0x00DC, homepage-more-info */
            wdata = PacketRead2 (p);
            if (wdata != 0xffff && wdata != 0)
                M_print (COLSERV "%-15s" COLNONE " %d\n", 
                    i18n (1575, "Age:"), wdata);
            else
                M_print (COLSERV "%-15s" COLNONE " %s\n", 
                    i18n (1575, "Age:"), i18n (1200, "Not entered"));

            wdata = PacketRead1 (p);
            M_print (COLSERV "%-15s" COLNONE " %s\n", i18n (1696, "Sex:"),
                       wdata == 1 ? i18n (1528, "female")
                     : wdata == 2 ? i18n (1529, "male")
                     :              i18n (1530, "not specified"));

            if (*(data = PacketReadLNTS (p)))
                M_print (AVPFMT, i18n (1531, "Homepage:"), data);

            if (sess->type == TYPE_SERVER_OLD)
                wdata = PacketRead1 (p) + 1900;
            else
                wdata = PacketRead2 (p);
            month = PacketRead1 (p);
            day = PacketRead1 (p);

            if (month >= 1 && month <= 12 && day && day < 32 && wdata)
                M_print (COLSERV "%-15s" COLNONE " %02d. %s %4d\n", 
                    i18n (1532, "Born:"), day, TableGetMonth (month), 
                    wdata);
            M_print (COLSERV "%-15s" COLNONE " ", 
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
        case 0xEB: /* more-email-info */
            if ((i = PacketRead1 (p)))
                M_print (COLSERV "%-15s" COLNONE "\n", 
                    i18n (1942, "Additional Email addresses:"));
            for (; i > 0; i--)
            {
                wdata = PacketRead1 (p);
                if (*(data = PacketReadLNTS (p)))
                    M_print ("  %s %s\n", data, 
                        wdata == 1 ? i18n (1943, "(no authorization needed)") 
                        : wdata == 0 ? i18n (1944, "(must request authorization)")
                        : "");
            }
            break;
        case META_SRV_WORK: /* 0x00D2, work-info */
            data = PacketReadLNTS (p);
            data2 = PacketReadLNTS (p);
            if (*data && *data2)
                M_print (COLSERV "%-15s" COLNONE " %s, %s\n", 
                    i18n (1524, "Work Location:"), data, data2);
            else if (*data)
                M_print (AVPFMT, i18n (1873, "Work City:"), data);
            else if (*data2)
                M_print (AVPFMT, i18n (1874, "Work State:"), data2);

            if (*(data = PacketReadLNTS (p)))
                M_print (AVPFMT, i18n (1523, "Work Phone:"), data);
            if (*(data = PacketReadLNTS (p)))
                M_print (AVPFMT, i18n (1521, "Work Fax:"), data);
            if (*(data = PacketReadLNTS (p)))
                M_print (AVPFMT, i18n (1522, "Work Address:"), data);

            if (sess->type == TYPE_SERVER)
            {  
                if (*(data = PacketReadLNTS (p)))
                    M_print (AVPFMT, i18n (1520, "Work Zip:"), data);
            }
            else if ((dwdata = PacketRead4 (p)))
                M_print (COLSERV "%-15s" COLNONE " %lu\n", 
                    i18n (1520, "Work Zip:"), dwdata);

            if ((wdata = PacketRead2 (p)))
            {
                if ((tabd = TableGetCountry (wdata)))
                    M_print (COLSERV "%-15s" COLNONE " %s\n", 
                        i18n (1514, "Work Country:"), tabd);
                else
                    M_print (COLSERV "%-15s" COLNONE " %d\n", 
                        i18n (1513, "Work Country Code:"), wdata);
            }
            if (*(data = PacketReadLNTS (p)))
                M_print (AVPFMT, i18n (1519, "Company Name:"), data);
            if (*(data = PacketReadLNTS (p)))
                M_print (AVPFMT, i18n (1518, "Department:"), data);
            if (*(data = PacketReadLNTS (p)))
                M_print (AVPFMT, i18n (1517, "Job Position:"), data);
            if ((wdata = PacketRead2 (p))) 
                M_print (COLSERV "%-15s" COLNONE " %s\n", 
                    i18n (1516, "Occupation:"), TableGetOccupation (wdata));
            if (*(data = PacketReadLNTS (p)))
                M_print (AVPFMT, i18n (1515, "Work Homepage:"), data);

            break;
        case META_SRV_ABOUT: /* 0x00E6, about */
            if (*(data = PacketReadLNTS (p)))
                M_print (COLSERV "%-15s" COLNONE "\n " COLCLIENT 
                    "%s" COLNONE "\n", i18n (1525, "About:"), data);
            break;
        case 0x00F0: /* personal-interests-info */
            if ((i = PacketRead1 (p)))
                M_print (COLSERV "%-15s" COLNONE "\n",
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
            }
            break;
        case 0x00FA: /* past-background-info */
            if ((i = PacketRead1 (p)))
                M_print (COLSERV "%-15s" COLNONE "\n", 
                    i18n (1876, "Background:")); /* XXX unsure */
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

            }
            if ((i = PacketRead1 (p)))
                M_print (COLSERV "%-15s" COLNONE "\n", 
                    i18n (1879, "Personal Past:")); /* unsure XXX */
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
            }

            break;
        case META_SRV_WP_FOUND: /* 0x01A4, wp-info + lasting */
        case META_SRV_WP_LAST_USER: /* 0x01AE, wp-info */
            if ((len = PacketRead2 (p)) < 19)
            {
                M_print (i18n (1398, "Search " COLCLIENT "failed" COLNONE "."));
                M_print ("\n");
                break;
            }

            Display_Info_Reply (sess, p, i18n (1498, "Info for"), 
                IREP_HASAUTHFLAG);

            switch ((wdata = PacketRead2 (p))) {
                case 0:
                    M_print (COLSERV "%-15s" COLNONE " %s\n", i18n (1452, "Status:"),
                        i18n (1653, "Offline"));
                    break;
                case 1:
                    M_print (COLSERV "%-15s" COLNONE " %s\n", i18n (1452, "Status:"),
                        i18n (1654, "Online"));
                    break;
                case 2:
                    M_print (COLSERV "%-15s" COLNONE " %s\n", i18n (1452, "Status:"),
                        i18n (1888, "Not webaware"));
                    break;
                default:
                    M_print (COLSERV "%-15s" COLNONE " %d\n", i18n (1452, "Status:"), 
                        wdata);
                    break;
            }

            wdata = PacketRead1 (p);
            M_print (COLSERV "%-15s" COLNONE " %s\n", i18n (1696, "Sex:"),
                       wdata == 1 ? i18n (1528, "female")
                     : wdata == 2 ? i18n (1529, "male")
                     :              i18n (1530, "not specified"));

            wdata = PacketRead1 (p);
            if (wdata != 0xff && wdata != 0)
                M_print (COLSERV "%-15s" COLNONE " %d\n", 
                    i18n (1575, "Age:"), wdata);
            else
                M_print (COLSERV "%-15s" COLNONE " %s\n", 
                    i18n (1575, "Age:"), i18n (1200, "Not entered"));

            if (subtype == META_SRV_WP_LAST_USER && (dwdata = PacketRead4(p)))
                M_print ("%lu %s\n", dwdata, i18n (1621, "users not returned."));
            break;
        case 0x010E:
            /* 0, counter like more-email-info? */
            if (!PacketRead2 (p))
                break;
        default:
            M_print ("%s: " COLSERV "%04X" COLNONE "\n", 
                i18n (1945, "Unknown Meta User response"), subtype);
            Hex_Dump (p->data + p->rpos, p->len - p->rpos);
            break;
    }
}

void Display_Rand_User (Session *sess, UBYTE *data, UDWORD len)
{
    if (len == 37)
    {
        M_print ("%-15s %d\n", i18n (1440, "Random User:"), Chars_2_DW (&data[0]));
        M_print ("%-15s %d.%d.%d.%d : %d\n", i18n (1441, "IP:"), data[4], data[5], data[6], data[7], Chars_2_DW (&data[8]));
        M_print ("%-15s %d.%d.%d.%d\n", i18n (1451, "IP2:"), data[12], data[13], data[14], data[15]);
        M_print ("%-15s ", i18n (1452, "Status:"));
        Print_Status (Chars_2_DW (&data[17]));
        M_print ("\n");
        M_print ("%-15s %d\n", i18n (1453, "TCP version:"), Chars_2_Word (&data[21]));
        M_print ("%-15s %s\n", i18n (1454, "Connection:"),
                 data[16] == 4 ? i18n (1493, "Peer-to-Peer") : i18n (1494, "Server Only"));
        if (prG->verbose > 1)
            Hex_Dump (data, len);
        CmdPktCmdMetaReqInfo (sess, Chars_2_DW (data));
    }
    else
    {
        M_print ("%s\n", i18n (1495, "No Random User Found"));
    }
}

void Recv_Message (Session *sess, UBYTE * pak)
{
    RECV_MESSAGE_PTR r_mesg;

    r_mesg = (RECV_MESSAGE_PTR) pak;
    uiG.last_rcvd_uin = Chars_2_DW (r_mesg->uin);
    M_print (i18n (1962, "%04d-%02d-%02d"), Chars_2_Word (r_mesg->year), r_mesg->month, r_mesg->day);
    M_print (" %02d:%02d UTC \a" CYAN BOLD "%10s" COLNONE " ",
             r_mesg->hour, r_mesg->minute, ContactFindName (Chars_2_DW (r_mesg->uin)));
    Do_Msg (sess, Chars_2_Word (r_mesg->type), Chars_2_Word (r_mesg->len),
            (r_mesg->len + 2), uiG.last_rcvd_uin, 0);
}


void UserOnlineSetVersion (Contact *con, UDWORD tstamp, UDWORD tstamp2)
{
    char buf[100];
    char *new = NULL;
    unsigned int ver = tstamp & 0xffff, ssl = 0;
    char v1 = 0, v2 = 0, v3 = 0, v4 = 0;

    if      ((tstamp & 0xff7f0000) == BUILD_LICQ && ver > 1000)
    {
        new = "licq";
        if (tstamp & BUILD_SSL)
            ssl = 1;
        v1 = ver / 1000;
        v2 = (ver / 10) % 100;
        v3 = ver % 10;
        v4 = 0;
    }
    else if ((tstamp & 0xff7f0000) == BUILD_MICQ || (tstamp & 0xff7f0000) == BUILD_LICQ)
    {
        new = "mICQ";
        if (tstamp & BUILD_SSL)
            ssl = 1;
        v1 = ver / 10000;
        v2 = (ver / 100) % 100;
        v3 = (ver / 10) % 10;
        v4 = ver % 10;
    }
    else if (tstamp == 0xffffffff)
    {
        new = "Miranda";
        v1 = (tstamp2 & 0x7f000000) >> 24;
        v2 = (tstamp2 &   0xff0000) >> 16;
        v3 = (tstamp2 &     0xff00) >> 8;
        v4 =  tstamp2 &       0xff;
    }
    
    if (new)
    {
        strcpy (buf, new);
        strcat (buf, " ");
                      sprintf (buf + strlen (buf), "%d.%d", v1, v2);
        if (v3 || v4) sprintf (buf + strlen (buf), ".%d", v3);
        if (v4)       sprintf (buf + strlen (buf), " cvs %d", v4);
        if (ssl) strcat (buf, "/SSL");
    }
    else if (prG->verbose)
        sprintf (buf, "%s %08x", i18n (1827, "Unknown client"), (unsigned int)tstamp);
    else
        buf[0] = '\0';

    if (con->version) free (con->version);
    con->version = strlen (buf) ? strdup (buf) : NULL;
}


void Display_Info_Reply (Session *sess, Packet *pak, const char *uinline,
    unsigned int flags)
{
    const char *data, *data2;
    UWORD wdata;

    if (uinline)
        M_print ("%s " COLSERV "%lu" COLNONE "\n", uinline, 
            PacketRead4 (pak));

    if (*(data = PacketReadLNTS (pak)))
        M_print (COLSERV "%-15s" COLNONE " " COLCONTACT "%s" 
            COLNONE "\n", i18n (1500, "Nickname:"), data);

    data = PacketReadLNTS (pak);
    data2 = PacketReadLNTS (pak);
    if (*data && *data2)
        M_print (COLSERV "%-15s" COLNONE " %s\t %s\n", 
            i18n (1501, "Name:"), data, data2);
    else if (*data)
        M_print (AVPFMT, i18n (1564, "First name:"), data);
    else if (*data2)
        M_print (AVPFMT, i18n (1565, "Last name:"), data2);

    data = PacketReadLNTS (pak);

    if (flags & IREP_HASAUTHFLAG)
    {
        wdata = PacketRead1 (pak);
        if (*data)
            M_print (COLSERV "%-15s" COLNONE " %s\t%s\n", 
                i18n (1566, "Email address:"), data,
                wdata == 1 ? i18n (1943, "(no authorization needed)")
                : i18n (1944, "(must request authorization)"));
    }
    else if (*data)
        M_print (AVPFMT, i18n (1502, "Email:"), data);

/*    ack_srv( sok, Chars_2_Word( pak.head.seq ) ); */
}

void Display_Ext_Info_Reply (Session *sess, Packet *pak, const char *uinline)
{
    const char *tabd, *data, *data2;
    UWORD wdata;
    int tz;

    /* Unfortunatly this is a bit different from any structure 
     * in meta user format, so it cannot be reused there. 
     */

    if (uinline)
        M_print ("%s " COLSERV "%lu" COLNONE "\n", uinline, 
            PacketRead4 (pak));

    data = PacketReadLNTS (pak);
    wdata = PacketRead2 (pak);
    tz = (signed char) PacketRead1 (pak);
    data2 = PacketReadLNTS (pak);

    if (*data && *data2)
        M_print (COLSERV "%-15s" COLNONE " %s, %s\n", 
            i18n (1505, "Location:"), data, data2);
    else if (*data)
        M_print (AVPFMT, i18n (1570, "City:"), data);
    else if (*data2)
        M_print (AVPFMT, i18n (1574, "State:"), data2);

    if ((tabd = TableGetCountry (wdata)) != NULL)
        M_print (COLSERV "%-15s" COLNONE " %s\t", 
            i18n (1511, "Country:"), tabd);
    else
        M_print (COLSERV "%-15s" COLNONE " %d\t", 
            i18n (1512, "Country code:"), wdata);

    M_print ("(UTC %+05d)\n", -100 * (tz / 2) + 30 * (tz % 2));

    wdata = PacketRead2 (pak);
    if (wdata != 0xffff && wdata != 0)
        M_print (COLSERV "%-15s" COLNONE " %d\n", 
            i18n (1575, "Age:"), wdata);
    else
        M_print (COLSERV "%-15s" COLNONE " %s\n", 
            i18n (1575, "Age:"), i18n (1200, "Not entered"));

    wdata = PacketRead1 (pak);
    M_print (COLSERV "%-15s" COLNONE " %s\n", i18n (1696, "Sex:"),
               wdata == 1 ? i18n (1528, "female")
             : wdata == 2 ? i18n (1529, "male")
             :              i18n (1530, "not specified"));

    if (*(data = PacketReadLNTS (pak)))
        M_print (AVPFMT, i18n (1506, "Phone:"), data);
    if (*(data = PacketReadLNTS (pak)))
        M_print (AVPFMT, i18n (1531, "Homepage:"), data);
    if (*(data = PacketReadLNTS (pak)))
        M_print (COLSERV "%-15s" COLNONE "\n " COLCLIENT 
            "%s" COLNONE "\n", i18n (1525, "About:"), data);
/*    ack_srv( sok, Chars_2_Word( pak.head.seq ) ); */
}

void Do_Msg (Session *sess, UDWORD type, UWORD len, const char *data, UDWORD uin, BOOL tcp)
{
    char *cdata, *tmp = NULL;
    char *url_url, *url_desc;
    Contact *cont;
    int x, m;
    
    cdata = strdup (data);

    TabAddUIN (uin);            /* Adds <uin> to the tab-list */
    UtilCheckUIN (sess, uin);

#ifdef MSGEXEC
    if (prG->event_cmd && strlen (prG->event_cmd))
        ExecScript (prG->event_cmd, uin, type, cdata);
#endif

    switch (type)
    {
        case USER_ADDED_MESS:
            tmp = strchr (cdata, '\xFE');
            if (tmp == NULL)
            {
                M_print (i18n (1585, "Ack!!!!!!!  Bad packet\n"));
                return;
            }
            *tmp = 0;
            ConvWinUnix (cdata); /* By Kunia User's nick was not transcoded...;( */
            M_print (i18n (1586, COLCONTACT "\n%s" COLNONE " has added you to their contact list.\n"), cdata);
            tmp++;
            cdata = tmp;
            tmp = strchr (tmp, '\xFE');
            if (tmp == NULL)
            {
                M_print (i18n (1585, "Ack!!!!!!!  Bad packet\n"));
                return;
            }
            *tmp = 0;
            ConvWinUnix (cdata);
            M_print ("%-15s " COLMESS "%s" COLNONE "\n", i18n (1564, "First name:"), cdata);
            tmp++;
            cdata = tmp;
            tmp = strchr (tmp, '\xFE');
            if (tmp == NULL)
            {
                M_print (i18n (1585, "Ack!!!!!!!  Bad packet\n"));
                return;
            }
            *tmp = 0;
            ConvWinUnix (cdata);
            M_print ("%-15s " COLMESS "%s" COLNONE "\n", i18n (1565, "Last name:"), cdata);
            tmp++;
            cdata = tmp;
            tmp = strchr (tmp, '\xFE');
            *tmp = 0;
            ConvWinUnix (cdata);
            M_print ("%-15s " COLMESS "%s" COLNONE "\n", i18n (1566, "Email address:"), cdata);
            break;
        case AUTH_REQ_MESS:
            tmp = strchr (cdata, '\xFE');
            *tmp = 0;
            M_print (i18n (1590, COLCONTACT "%10s " COLNONE " has requested your authorization to be added to their contact list.\n"), cdata);
            tmp++;
            cdata = tmp;
            tmp = strchr (tmp, '\xFE');
            if (tmp == NULL)
            {
                M_print (i18n (1585, "Ack!!!!!!!  Bad packet\n"));
                return;
            }
            *tmp = 0;
            ConvWinUnix (cdata);
            M_print ("%-15s " COLMESS "%s" COLNONE "\n", i18n (1564, "First name:"), cdata);
            tmp++;
            cdata = tmp;
            tmp = strchr (tmp, '\xFE');
            if (tmp == NULL)
            {
                M_print (i18n (1585, "Ack!!!!!!!  Bad packet\n"));
                return;
            }
            *tmp = 0;
            ConvWinUnix (cdata);
            M_print ("%-15s " COLMESS "%s" COLNONE "\n", i18n (1565, "Last name:"), cdata);
            tmp++;
            cdata = tmp;
            tmp = strchr (tmp, '\xFE');
            if (tmp == NULL)
            {
                M_print (i18n (1585, "Ack!!!!!!!  Bad packet\n"));
                return;
            }
            *tmp = 0;
            ConvWinUnix (cdata);
            M_print ("%-15s " COLMESS "%s" COLNONE "\n", i18n (1566, "Email address:"), cdata);
            tmp++;
            cdata = tmp;
            tmp = strchr (tmp, '\xFE');
            if (tmp == NULL)
            {
                M_print (i18n (1585, "Ack!!!!!!!  Bad packet\n"));
                return;
            }
            *tmp = 0;
            tmp++;
            cdata = tmp;
            tmp = strchr (tmp, '\x00');
            if (tmp == NULL)
            {
                M_print (i18n (1585, "Ack!!!!!!!  Bad packet\n"));
                return;
            }
            *tmp = 0;
            ConvWinUnix (cdata);
            M_print ("%-15s " COLMESS "%s" COLNONE "\n", i18n (1591, "Reason:"), cdata);
            break;
        case EMAIL_MESS:
        case WEB_MESS:
            tmp = strchr (cdata, '\xFE');
            *tmp = 0;
            M_print ("\n%s ", cdata);
            tmp++;
            cdata = tmp;
            tmp = strchr (cdata, '\xFE');
            tmp++;
            cdata = tmp;

            tmp = strchr (cdata, '\xFE');
            tmp++;
            cdata = tmp;

            tmp = strchr (cdata, '\xFE');
            *tmp = 0;
            if (type == EMAIL_MESS)
                M_print (i18n (1592, "<%s> emailed you a message:\n"), cdata);
            else
                M_print (i18n (1593, "<%s> send you a web message:\n"), cdata);
            tmp++;
            cdata = tmp;
            tmp = strchr (cdata, '\xFE');
            *tmp = 0;
            if (prG->verbose)
            {
                M_print ("??? '%s'\n", cdata);
            }
            tmp++;
            cdata = tmp;
            M_print (COLMESS "%s" COLNONE "\n", cdata);
            break;
        case URL_MESS:
        case MRURL_MESS:
            url_desc = cdata;
            url_url = strchr (cdata, '\xFE');
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

            ConvWinUnix (url_desc);
            ConvWinUnix (url_url);

            log_event (uin, LOG_MESS,
                       "You received URL message from %s\nDescription: %s\nURL: %s\n",
                       ContactFindName (uin), url_desc, url_url);

            M_print ("%s" COLMESS "%s" COLNONE "\n", tcp ? MSGTCPRECSTR : MSGRECSTR, url_desc);
            Time_Stamp ();
            M_print (i18n (1594, "        URL: %s" COLMESS "%s" COLNONE "\n"),
                     tcp ? MSGTCPRECSTR : MSGRECSTR, url_url);
            break;
        case CONTACT_MESS:
        case MRCONTACT_MESS:
            tmp = strchr (cdata, '\xFE');
            *tmp = 0;
            M_print (i18n (1595, "\nContact List.\n" COLMESS "============================================\n" COLNONE "%d Contacts\n"),
                     atoi (cdata));
            tmp++;
            m = atoi (cdata);
            for (x = 0; m > x; x++)
            {
                cdata = tmp;
                tmp = strchr (tmp, '\xFE');
                *tmp = 0;
                M_print (COLCONTACT "%s\t\t\t", cdata);
                tmp++;
                cdata = tmp;
                tmp = strchr (tmp, '\xFE');
                *tmp = 0;
                M_print (COLMESS "%s" COLNONE "\n", cdata);
                tmp++;
            }
            break;
        default:
            ConvWinUnix (cdata);
            while (*cdata && (cdata[strlen (cdata) - 1] == '\n' || cdata[strlen (cdata) - 1] == '\r'))
                cdata[strlen (cdata) - 1] = '\0';
            log_event (uin, LOG_MESS, "You received instant message from %s\n%s\n",
                       ContactFindName (uin), cdata);
            M_print ("%s" COLMESS "\x1b<%s" COLNONE "\x1b>\n", tcp ? MSGTCPRECSTR : MSGRECSTR, cdata);
            break;
    }
    uiG.last_rcvd_uin = uin;
    if ((cont = ContactFind (uin)))
    {
        cont->LastMessage = realloc (cont->LastMessage, len + 1);
        ConvWinUnix (cdata);
        strncpy (cont->LastMessage, cdata, len + 1);
        cont->LastMessage [len] = '\0';
    }
}
