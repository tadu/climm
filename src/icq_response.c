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

static void Handle_Interest (UBYTE count, UBYTE * data);
static void PktPrintString (UBYTE ** data, const char *fmt, ...);

static void PktPrintString (UBYTE ** data, const char *fmt, ...)
{
    char buf[2048];
    va_list args;
    int len;

    va_start (args, fmt);
    vsnprintf (buf, sizeof (buf), fmt, args);
    va_end (args);

    len = Chars_2_Word ((*data));
    *data += 2;
    if (len != 1)
    {
        (*data)[len - 1] = 0;   /* be safe */
        ConvWinUnix (*data);
        M_print (buf, *data);        /* Careful - fmt is used as a format string twice. */
    }
    *data += len;
}

static void Handle_Interest (UBYTE count, UBYTE * data)
{
    const char *nomen;
    UBYTE * data2;
    int len, nlen, count2;
    
    for (data2 = data, count2 = count, len = 0; count2; count2--)
    {
        nomen = TableGetInterest (Chars_2_Word (data));
        if (nomen && (nlen = strlen (nomen)) > len)
            len = nlen;
    }
    
    for (; count; count--)
    {
        nomen = TableGetInterest (Chars_2_Word (data));

        if (nomen)
            M_print (COLSERV "%*s:\t" COLMESS, len, nomen);
        else
            M_print (COLSERV "%*d:\t" COLMESS, len, Chars_2_Word (data));

        data += 2;
        PktPrintString (&data, "%%s");
        M_print (COLNONE "\n");
    }
}

void Meta_User (Session *sess, UBYTE * data, UDWORD len, UDWORD uin)
{
    UWORD subcmd;
    UDWORD zip;
    char *newline;
    /*int len_str; */
    int len2;
    char *tmp;
    subcmd = Chars_2_Word (data);

    switch (subcmd)
    {
        case META_SRV_PASS:
            M_print (i18n (197, "Password change was " COLCLIENT "%s" COLNONE ".\n"),
                     data[2] == 0xA ? i18n (393, "successful") : i18n (394, "unsuccessful"));
            break;
        case META_SRV_ABOUT_UPDATE:
            M_print (i18n (395, "About info change was " COLCLIENT "%s" COLNONE ".\n"),
                     data[2] == 0xA ? i18n (393, "successful") : i18n (394, "unsuccessful"));
            break;
        case META_SRV_GEN_UPDATE:
            M_print (i18n (396, "Info change was " COLCLIENT "%s" COLNONE ".\n"),
                     data[2] == 0xA ? i18n (393, "successful") : i18n (394, "unsuccessful"));
            break;
        case META_SRV_OTHER_UPDATE:
            M_print (i18n (397, "Other info change was " COLCLIENT "%s" COLNONE ".\n"),
                     data[2] == 0xA ? i18n (393, "successful") : i18n (394, "unsuccessful"));
            break;
        case META_SRV_MORE:
            data += 2;
            if (*data != 0x0A)
                break;
            data++;

            if (Chars_2_Word (data) != (UWORD) - 1)
                M_print (COLSERV "%-15s" COLNONE " %d\n", i18n (527, "Age:"),
                         Chars_2_Word (data));
            else
                M_print (COLSERV "%-15s" COLNONE " %s\n", i18n (527, "Age:"),
                         i18n (526, "Not Entered"));
            data += 2;

            M_print (COLSERV "%-15s" COLNONE " %s\n", i18n (696, "Sex:"),
                       *data == 1 ? i18n (528, "female")
                     : *data == 2 ? i18n (529, "male")
                     :              i18n (530, "not specified"));
            data++;

            PktPrintString (&data, COLSERV "%-15s" COLNONE " %%s\n", i18n (531, "Homepage:"));
            newline = "";
            if ((*(data + 1) > 0) && (*(data + 1) <= 12))
            {
                M_print (COLSERV "%-15s" COLNONE " %02d. %s %4d\n", i18n (532, "Born:"),
                         *(data + 2), TableGetMonth (*(data + 1)), *(data) + 1900);
            }
            data += 3;
            if (((*data != 0) && (*data != (UBYTE) 0xff))
                || ((*(data + 1) != 0) && (*(data + 1) != (UBYTE) 0xff))
                || ((*(data + 2) != 0) && (*(data + 2) != (UBYTE) 0xff)))
            {
                M_print (COLSERV "%-15s" COLNONE " ", i18n (533, "Languages:"));
                if (TableGetLang (*data) != NULL)
                {
                    M_print ("%s", TableGetLang (*data));
                }
                else
                {
                    M_print ("%X", *data);
                }
                data++;
                if (TableGetLang (*data) != NULL)
                {
                    M_print (", %s", TableGetLang (*data));
                }
                else
                {
                    M_print (", %X", *data);
                }
                data++;
                if (TableGetLang (*data) != NULL)
                {
                    M_print (", %s", TableGetLang (*data));
                }
                else
                {
                    M_print (", %X", *data);
                }
                M_print ("\n");
            }
            break;
        case META_SRV_ABOUT:
            data += 2;
            if (*data != 0x0A)
                break;
            data++;
            PktPrintString (&data, COLSERV "%s" COLCLIENT "\n%%s", i18n (525, "About:"));
            M_print (COLNONE);
            break;
        case META_SRV_WORK:
            data += 2;
            if (*data != 0x0A)
                break;
            data++;
            
            PktPrintString (&data, COLSERV "%-15s" COLNONE " %%s\n", i18n (524, "Work Location:"));
            PktPrintString (&data, COLSERV "%-15s" COLNONE " %%s\n", i18n (524, "Work Location:"));
            PktPrintString (&data, COLSERV "%-15s" COLNONE " %%s\n", i18n (523, "Work Phone:"));
            PktPrintString (&data, COLSERV "%-15s" COLNONE " %%s\n", i18n (521, "Work Fax:"));
            PktPrintString (&data, COLSERV "%-15s" COLNONE " %%s\n", i18n (522, "Work Address:"));

            if (Chars_2_DW (data))
            {
                M_print (COLSERV "%-15s" COLNONE " %d\n", i18n (520, "Work Zip:"), Chars_2_DW (data));
            }
            data += 4;
            
            if (Chars_2_Word (data))
            {
                if (TableGetCountry (Chars_2_Word (data)))
                    M_print (COLSERV "%-15s" COLNONE " %s\n", i18n (514, "Work Country:"),
                             TableGetCountry (Chars_2_Word (data)));
                else
                    M_print (COLSERV "%-15s" COLNONE " %d\n", i18n (513, "Work Country Code:"),
                             Chars_2_Word (data));
            }
            data += 2;
            
            PktPrintString (&data, COLSERV "%-15s" COLNONE " %%s\n", i18n (519, "Company Name:"));
            PktPrintString (&data, COLSERV "%-15s" COLNONE " %%s\n", i18n (518, "Department:"));
            PktPrintString (&data, COLSERV "%-15s" COLNONE " %%s\n", i18n (517, "Job Position:"));
            
            if (Chars_2_Word (data))
                M_print (COLSERV "%-15s" COLNONE " %s\n", i18n (516, "Occupation:"),
                         TableGetOccupation (Chars_2_Word (data)));
            data += 2;
            
            PktPrintString (&data, COLSERV "%-15s" COLNONE " %%s\n", i18n (515, "Work Homepage:"));
            M_print (COLNONE);
            break;
        case META_SRV_GEN:
            data += 2;
            if (*data != 0x0A)
                break;
            data++;
            PktPrintString (&data, COLSERV "%-15s" COLNONE " " COLCONTACT "%%s" COLNONE "\n", i18n (500, "Nickname:"));

        /*  PktPrintString (&data, COLSERV "%-15s" COLNONE " %%s\n", i18n (-1, "First Name:");
            PktPrintString (&data, COLSERV "%-15s" COLNONE " %%s\n", i18n (-1, "Last Name:");   */

            if (Chars_2_Word (data))
            {
                PktPrintString (&data, COLSERV "%-15s" COLNONE " %%s", i18n (501, "Name:"));
                PktPrintString (&data, "\t%%s");
                M_print ("\n");
            }

            PktPrintString (&data, COLSERV "%-15s" COLNONE " %%s\n", i18n (502, "Email:"));
            PktPrintString (&data, COLSERV "%-15s" COLNONE " %%s\n", i18n (503, "Other Email:"));
            PktPrintString (&data, COLSERV "%-15s" COLNONE " %%s\n", i18n (504, "Old Email:"));

        /*  PktPrintString (&data, COLSERV "%-15s" COLNONE " %%s\n", i18n (-1, "City:");
            PktPrintString (&data, COLSERV "%-15s" COLNONE " %%s\n", i18n (-1, "State:");   */

            if (Chars_2_Word (data))
            {
                PktPrintString (&data, COLSERV "%-15s" COLNONE " %%s", i18n (505, "Location:"));
                PktPrintString (&data, ", %%s");
                M_print ("\n");
            }

            PktPrintString (&data, COLSERV "%-15s" COLNONE " %%s\n", i18n (506, "Phone:"));
            PktPrintString (&data, COLSERV "%-15s" COLNONE " %%s\n", i18n (507, "Fax:"));
            PktPrintString (&data, COLSERV "%-15s" COLNONE " %%s\n", i18n (508, "Street:"));
            PktPrintString (&data, COLSERV "%-15s" COLNONE " %%s\n", i18n (509, "Cellular:"));

            zip = Chars_2_DW (data);
            if (((signed long) zip) > 0)
                M_print (COLSERV "%-15s" COLNONE " %05u\n", i18n (510, "Zip:"), zip);
            data += 4;

            if (TableGetCountry (Chars_2_Word (data)))
                M_print (COLSERV "%-15s" COLNONE " %s\t", i18n (511, "Country:"),
                         TableGetCountry (Chars_2_Word (data)));
            else
                M_print (COLSERV "%-15s" COLNONE " %d\t", i18n (512, "Country Code:"), Chars_2_Word (data));
            data += 2;

            if (((signed char) *data) >> 1 != -50)
                M_print ("(UTC %+d)", ((signed char) *data) >> 1);
            M_print (COLNONE "\n");
            break;
        case 0x00F0:
            data += 3;
            Handle_Interest ((UBYTE) * data, data + 1);
            break;
        case 0x00FA:           /* Silently ignore these so as not to confuse people */
            if (data[2] == 0x14)
            {
                M_print (i18n (398, "Search " COLCLIENT "failed" COLNONE ".\n"));
                break;
            }
        case META_SRV_WP_FOUND:
            if (data[0] != 0xA4)
                break;
            if (data[2] == 0x32 || (data[2]==0x14 && data[3]==0x15))
            {
                M_print (i18n (398, "Search " COLCLIENT "failed" COLNONE ".\n"));
                break;
            }
            M_print (COLSERV "%-15s" COLNONE " %d\n", i18n (498, "Info for:"), Chars_2_DW (&data[5]));
            len2 = Chars_2_Word (&data[9]);
            ConvUnixWin (&data[11]);
            M_print (COLSERV "%-15s" COLNONE " %s\n",i18n (563, "Nick name:"), &data[11]);
            tmp = &data[11 + len2];
            len2 = Chars_2_Word (tmp);
            ConvUnixWin (tmp + 2);
            M_print (COLSERV "%-15s" COLNONE " %s\n",i18n (564, "First name:"), tmp + 2);
            tmp += len2 + 2;
            len2 = Chars_2_Word (tmp);
            ConvUnixWin (tmp + 2);
            M_print (COLSERV "%-15s" COLNONE " %s\n",i18n (565, "Last name:"), tmp + 2);
            tmp += len2 + 2;
            len2 = Chars_2_Word (tmp);
            ConvUnixWin (tmp + 2);
            M_print (COLSERV "%-15s" COLNONE " %s\n",i18n (566, "Email address:"), tmp + 2);
            tmp += len2 + 2;
            if (*tmp == 1)
            {
                M_print (i18n (567, "No authorization needed." COLNONE " "));
            }
            else
            {
                M_print (i18n (568, "Must request authorization." COLNONE " "));
            }
            len2 = Chars_2_Word(tmp);
            tmp+=1;
            if (*tmp == 1)
            {
                M_print (" %s\n", i18n (921, "Online"));
            }
            else
            {
                M_print (" %s\n", i18n (928, "Offline"));
            }
            break;
        case META_SRV_WP_LAST_USER:
            if (data[0] != 0xAE)
                break;
            if (data[2] == 0x32 || (data[2]==0x14 && (data[3]==0x15 || data[3]==0x00)))
            {
                M_print (i18n (398, "Search " COLCLIENT "failed" COLNONE ".\n"));
                break;
            }
            M_print (COLSERV "%-15s" COLNONE " %d\n", i18n (498, "Info for:"), Chars_2_DW (&data[5]));
            len2 = Chars_2_Word (&data[9]);
            ConvUnixWin (&data[11]);
            M_print (COLSERV "%-15s" COLNONE " %s\n",i18n (563, "Nick name:"), &data[11]);
            tmp = &data[11 + len2];
            len2 = Chars_2_Word (tmp);
            ConvUnixWin (tmp + 2);
            M_print (COLSERV "%-15s" COLNONE " %s\n",i18n (564, "First name:"), tmp + 2);
            tmp += len2 + 2;
            len2 = Chars_2_Word (tmp);
            ConvUnixWin (tmp + 2);
            M_print (COLSERV "%-15s" COLNONE " %s\n",i18n (565, "Last name:"), tmp + 2);
            tmp += len2 + 2;
            len2 = Chars_2_Word (tmp);
            ConvUnixWin (tmp + 2);
            M_print (COLSERV "%-15s" COLNONE " %s\n",i18n (566, "Email address:"), tmp + 2);
            tmp += len2 + 2;
            if (*tmp == 1)
            {
                M_print (i18n (567, "No authorization needed." COLNONE " "));
            }
            else
            {
                M_print (i18n (568, "Must request authorization." COLNONE " "));
            }
            len2 = Chars_2_Word(tmp);
            tmp+=1;
            if (*tmp == 1)
            {
                M_print (" %s\n", i18n (921, "Online"));
            }
            else
            {
                M_print (" %s\n", i18n (928, "Offline"));
            }

            M_print (i18n (621, "%d users not returned."), *tmp);
            M_print ("\n");
            break;
        case 0x010E:
            if (!prG->verbose)
                break;
        default:
            M_print (i18n (399, "Unknown Meta User response " COLSERV "%04X" COLNONE "\n"), subcmd);
            if (prG->verbose)
                Hex_Dump (data, len);
            break;
    }
}

void Display_Rand_User (Session *sess, UBYTE * data, UDWORD len)
{
    if (len == 37)
    {
        M_print ("%-15s %d\n", i18n (440, "Random User:"), Chars_2_DW (&data[0]));
        M_print ("%-15s %d.%d.%d.%d : %d\n", i18n (441, "IP:"), data[4], data[5], data[6], data[7], Chars_2_DW (&data[8]));
        M_print ("%-15s %d.%d.%d.%d\n", i18n (451, "IP2:"), data[12], data[13], data[14], data[15]);
        M_print ("%-15s ", i18n (452, "Status:"));
        Print_Status (Chars_2_DW (&data[17]));
        M_print ("\n");
        M_print ("%-15s %d\n", i18n (453, "TCP version:"), Chars_2_Word (&data[21]));
        M_print ("%-15s %s\n", i18n (454, "Connection:"),
                 data[16] == 4 ? i18n (493, "Peer-to-Peer") : i18n (494, "Server Only"));
        if (prG->verbose > 1)
            Hex_Dump (data, len);
        CmdPktCmdMetaReqInfo (sess, Chars_2_DW (data));
    }
    else
    {
        M_print ("%s\n", i18n (495, "No Random User Found"));
    }
}

void Recv_Message (Session *sess, UBYTE * pak)
{
    RECV_MESSAGE_PTR r_mesg;

    r_mesg = (RECV_MESSAGE_PTR) pak;
    uiG.last_rcvd_uin = Chars_2_DW (r_mesg->uin);
    M_print (i18n (496, "%02d/%02d/%04d"), r_mesg->month, r_mesg->day, Chars_2_Word (r_mesg->year));
    M_print (" %02d:%02d UTC \a" CYAN BOLD "%10s" COLNONE " ",
             r_mesg->hour, r_mesg->minute, ContactFindName (Chars_2_DW (r_mesg->uin)));
    Do_Msg (sess, Chars_2_Word (r_mesg->type), Chars_2_Word (r_mesg->len),
            (r_mesg->len + 2), uiG.last_rcvd_uin, 0);
}


/************************************************
This is called when a user goes offline
*************************************************/
void User_Offline (Session *sess, UBYTE * pak)
{
    Contact *con;
    int remote_uin;

    remote_uin = Chars_2_DW (&pak[0]);

    Time_Stamp ();
    M_print (" " COLCONTACT "%10s" COLNONE " %s\n",
             ContactFindName (remote_uin), i18n (30, "logged off."));
    log_event (remote_uin, LOG_ONLINE, "User logged off %s\n", ContactFindName (remote_uin));

    if (prG->sound & SFLAG_OFF_CMD)
        ExecScript (prG->sound_off_cmd, remote_uin, 0, NULL);
    else if (prG->sound & SFLAG_OFF_BEEP)
        printf ("\n");

    if ((con = ContactFind (remote_uin)) != NULL)
    {
        con->status = STATUS_OFFLINE;
        con->last_time = time (NULL);
    }
}

void UserOnlineSetVersion (Contact *con, UDWORD tstamp)
{
    char buf[100];
    char *new = NULL;
    unsigned int ver = tstamp & 0xffff, ssl = 0;

    if      ((tstamp & 0xff7f0000) == BUILD_LICQ)
    {
        if (ver > 1000) {new = "licq"; ver *= 10; }
        else new = "mICQ";
    }
    else if ((tstamp & 0xff7f0000) == BUILD_MICQ)   new = "mICQ";
    if       (tstamp &                BUILD_SSL)    ssl = 1;
    
    if (new)
    {
        strcpy (buf, new);
        strcat (buf, " ");
                       sprintf (buf + strlen (buf), "%d.%d", ver / 10000,
                                                            (ver / 100) % 100);
        if (ver % 100) sprintf (buf + strlen (buf), ".%d",  (ver / 10)  %  10);
        if (ver % 10)  sprintf (buf + strlen (buf), " cvs %d",   ver        %  10);
        if (ssl) strcat (buf, "/SSL");
    }
    else if (prG->verbose)
        sprintf (buf, "%s %08x", i18n (827, "Unknown client"), (unsigned int)tstamp);
    else
        buf[0] = '\0';

    if (con->version) free (con->version);
    con->version = strlen (buf) ? strdup (buf) : NULL;
}

void User_Online (Session *sess, Packet *pak)
{
    Contact *con;
    int uin, status;

    uin = PacketRead4 (pak);
    con = ContactFind (uin);
    log_event (uin, LOG_ONLINE, "User logged on %s\n", ContactFindName (uin));
    
    if (!con)
        return;

    con->last_time = time (NULL);
    con->outside_ip      = PacketRead4 (pak);
    con->port            = PacketRead4 (pak);
    con->local_ip        = PacketRead4 (pak);
    con->connection_type = PacketRead1 (pak);
    status               = PacketRead4 (pak);
    con->TCP_version     = PacketRead4 (pak);
                           PacketRead4 (pak);
                           PacketRead4 (pak);
                           PacketRead4 (pak);
    UserOnlineSetVersion (con, PacketRead4 (pak));

    if (status == con->status)
        return;
    
    con->status = status;
    if (sess->connect & CONNECT_OK)
    {

        if (prG->sound & SFLAG_ON_CMD)
            ExecScript (prG->sound_on_cmd, uin, 0, NULL);
        else if (prG->sound & SFLAG_ON_BEEP)
            printf ("\a");

        Time_Stamp ();
        M_print (" " COLCONTACT "%10s" COLNONE " %s (",
                 ContactFindName (uin), i18n (31, "logged on"));
        Print_Status (con->status);
        M_print (")");
        if (con && con->version)
            M_print ("[%s]", con->version);
        M_print (".\n");
        if (prG->verbose)
        {
            M_print ("%-15s %s\n", i18n (441, "IP:"), UtilIOIP (con->outside_ip));
            M_print ("%-15s %s\n", i18n (451, "IP2:"), UtilIOIP (con->local_ip));
            M_print ("%-15s %d\n", i18n (453, "TCP version:"), con->TCP_version);
            M_print ("%-15s %s\n", i18n (454, "Connection:"), con->connection_type == 4 
                                 ? i18n (493, "Peer-to-Peer") : i18n (494, "Server Only"));
        }
    }
    else
    {
        Kill_Prompt ();
    }
}

void Status_Update (Session *sess, UBYTE * pak)
{
    Contact *cont;
    int remote_uin, new_status;

    remote_uin = Chars_2_DW (&pak[0]);

    new_status = Chars_2_DW (&pak[4]);
    if (pak[8])
        M_print ("%02X\n", pak[8]);
    cont = ContactFind (remote_uin);
    if (cont)
    {
        if (cont->status == new_status)
        {
            Kill_Prompt ();
            return;
        }
        cont->status = new_status;
    }
    Time_Stamp ();
    M_print (" " COLCONTACT "%10s" COLNONE " %s ",
             ContactFindName (remote_uin), i18n (35, "changed status to"));
    Print_Status (new_status);
    M_print ("\n");
}

void Display_Info_Reply (Session *sess, UBYTE * pak)
{
    char *tmp;
    int len;

    M_print (i18n (562, COLSERV "Info for %ld\n"), Chars_2_DW (&pak[0]));
    len = Chars_2_Word (&pak[4]);
    ConvWinUnix (&pak[6]);
    M_print ("%-15s %s\n", i18n (563, "Nick name:"), &pak[6]);
    tmp = &pak[6 + len];
    len = Chars_2_Word (tmp);
    ConvWinUnix (tmp + 2);
    M_print ("%-15s %s\n", i18n (564, "First name:"), tmp + 2);
    tmp += len + 2;
    len = Chars_2_Word (tmp);
    ConvWinUnix (tmp + 2);
    M_print ("%-15s %s\n", i18n (565, "Last name:"), tmp + 2);
    tmp += len + 2;
    len = Chars_2_Word (tmp);
    ConvWinUnix (tmp + 2);
    M_print ("%-15s %s\n", i18n (566, "Email address:"), tmp + 2);
    tmp += len + 2;
    if (*tmp == 1)
    {
        M_print (i18n (567, "No authorization needed." COLNONE " "));
    }
    else
    {
        M_print (i18n (568, "Must request authorization." COLNONE " "));
    }
/*    ack_srv( sok, Chars_2_Word( pak.head.seq ) ); */
}

void Display_Ext_Info_Reply (Session *sess, UBYTE * pak)
{
    unsigned char *tmp;
    int len;

    M_print (i18n (569, COLSERV "More Info for %ld\n"), Chars_2_DW (&pak[0]));
    len = Chars_2_Word (&pak[4]);
    ConvWinUnix (&pak[6]);
    M_print ("%-15s %s\n", i18n (570, "City:"), &pak[6]);
    if (TableGetCountry (Chars_2_Word (&pak[6 + len])) != NULL)
        M_print ("%-15s %s\n", i18n (571, "Country:"), TableGetCountry (Chars_2_Word (&pak[6 + len])));
    else
        M_print ("%-15s %d\n", i18n (572, "Country code:"), Chars_2_Word (&pak[6 + len]));
    M_print ("%-15s UTC %+d\n", i18n (573, "Time Zone:"), ((signed char) pak[len + 8]) >> 1);
    tmp = &pak[9 + len];
    len = Chars_2_Word (tmp);
    ConvWinUnix (tmp + 2);
    M_print ("%-15s %s\n", i18n (574, "State:"), tmp + 2);
    if (Chars_2_Word (tmp + 2 + len) != 0xffff)
        M_print ("%-15s %d\n", i18n (575, "Age:"), Chars_2_Word (tmp + 2 + len));
    else
        M_print ("%-15s %s\n", i18n (575, "Age:"), i18n (576, "not entered"));
    if (*(tmp + len + 4) == 2)
        M_print ("%-15s %s\n", i18n (577, "Sex:"), i18n (529, "male"));
    else if (*(tmp + len + 4) == 1)
        M_print ("%-15s %s\n", i18n (577, "Sex:"), i18n (528, "female"));
    else
        M_print ("%-15s %s\n", i18n (577, "Sex:"), i18n (576, "not entered"));
    tmp += len + 5;
    len = Chars_2_Word (tmp);
    ConvWinUnix (tmp + 2);
    M_print ("%-15s %s\n", i18n (580, "Phone number:"), tmp + 2);
    tmp += len + 2;
    len = Chars_2_Word (tmp);
    ConvWinUnix (tmp + 2);
    M_print ("%-15s %s\n", i18n (581, "Home page:"), tmp + 2);
    tmp += len + 2;
    len = Chars_2_Word (tmp);
    ConvWinUnix (tmp + 2);
    M_print ("%-15s %s\n", i18n (582, "About:"), tmp + 2);
/*    ack_srv( sok, Chars_2_Word( pak.head.seq ) ); */
}

void Display_Search_Reply (Session *sess, UBYTE * pak)
{
    char *tmp;
    int len;
    M_print (i18n (583, COLSERV "User found %ld\n"), Chars_2_DW (&pak[0]));
    len = Chars_2_Word (&pak[4]);
    ConvWinUnix (&pak[6]);
    M_print ("%-15s %s\n", i18n (563, "Nick name:"), &pak[6]);
    tmp = &pak[6 + len];
    len = Chars_2_Word (tmp);
    ConvWinUnix (tmp + 2);
    M_print ("%-15s %s\n", i18n (564, "First name:"), tmp + 2);
    tmp += len + 2;
    len = Chars_2_Word (tmp);
    ConvWinUnix (tmp + 2);
    M_print ("%-15s %s\n", i18n (565, "Last name:"), tmp + 2);
    tmp += len + 2;
    len = Chars_2_Word (tmp);
    ConvWinUnix (tmp + 2);
    M_print ("%-15s %s\n", i18n (566, "Email address:"), tmp + 2);
    tmp += len + 2;
    if (*tmp == 1)
    {
        M_print (i18n (567, "No authorization needed." COLNONE " "));
    }
    else
    {
        M_print (i18n (568, "Must request authorization." COLNONE " "));
    }
}

void Do_Msg (Session *sess, UDWORD type, UWORD len, const char *data, UDWORD uin, BOOL tcp)
{
    char *cdata, *tmp = NULL;
    char *url_url, *url_desc;
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
                M_print (i18n (585, "Ack!!!!!!!  Bad packet"));
                return;
            }
            *tmp = 0;
            ConvWinUnix (cdata); /* By Kunia User's nick was not transcoded...;( */
            M_print (i18n (586, COLCONTACT "\n%s" COLNONE " has added you to their contact list.\n"), cdata);
            tmp++;
            cdata = tmp;
            tmp = strchr (tmp, '\xFE');
            if (tmp == NULL)
            {
                M_print (i18n (585, "Ack!!!!!!!  Bad packet"));
                return;
            }
            *tmp = 0;
            ConvWinUnix (cdata);
            M_print ("%-15s " COLMESS "%s" COLNONE "\n", i18n (564, "First name:"), cdata);
            tmp++;
            cdata = tmp;
            tmp = strchr (tmp, '\xFE');
            if (tmp == NULL)
            {
                M_print (i18n (585, "Ack!!!!!!!  Bad packet"));
                return;
            }
            *tmp = 0;
            ConvWinUnix (cdata);
            M_print ("%-15s " COLMESS "%s" COLNONE "\n", i18n (565, "Last name:"), cdata);
            tmp++;
            cdata = tmp;
            tmp = strchr (tmp, '\xFE');
            *tmp = 0;
            ConvWinUnix (cdata);
            M_print ("%-15s " COLMESS "%s" COLNONE "\n", i18n (566, "Email address:"), cdata);
            break;
        case AUTH_REQ_MESS:
            tmp = strchr (cdata, '\xFE');
            *tmp = 0;
            M_print (i18n (590, COLCONTACT "\n%s" COLNONE " has requested your authorization to be added to their contact list.\n"), cdata);
            tmp++;
            cdata = tmp;
            tmp = strchr (tmp, '\xFE');
            if (tmp == NULL)
            {
                M_print (i18n (585, "Ack!!!!!!!  Bad packet"));
                return;
            }
            *tmp = 0;
            ConvWinUnix (cdata);
            M_print ("%-15s " COLMESS "%s" COLNONE "\n", i18n (564, "First name:"), cdata);
            tmp++;
            cdata = tmp;
            tmp = strchr (tmp, '\xFE');
            if (tmp == NULL)
            {
                M_print (i18n (585, "Ack!!!!!!!  Bad packet"));
                return;
            }
            *tmp = 0;
            ConvWinUnix (cdata);
            M_print ("%-15s " COLMESS "%s" COLNONE "\n", i18n (565, "Last name:"), cdata);
            tmp++;
            cdata = tmp;
            tmp = strchr (tmp, '\xFE');
            if (tmp == NULL)
            {
                M_print (i18n (585, "Ack!!!!!!!  Bad packet"));
                return;
            }
            *tmp = 0;
            ConvWinUnix (cdata);
            M_print ("%-15s " COLMESS "%s" COLNONE "\n", i18n (566, "Email address:"), cdata);
            tmp++;
            cdata = tmp;
            tmp = strchr (tmp, '\xFE');
            if (tmp == NULL)
            {
                M_print (i18n (585, "Ack!!!!!!!  Bad packet"));
                return;
            }
            *tmp = 0;
            tmp++;
            cdata = tmp;
            tmp = strchr (tmp, '\x00');
            if (tmp == NULL)
            {
                M_print (i18n (585, "Ack!!!!!!!  Bad packet"));
                return;
            }
            *tmp = 0;
            ConvWinUnix (cdata);
            M_print ("%-15s " COLMESS "%s" COLNONE "\n", i18n (591, "Reason:"), cdata);
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
                M_print (i18n (592, "<%s> emailed you a message:\n"), cdata);
            else
                M_print (i18n (593, "<%s> send you a web message:\n"), cdata);
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
            M_print (i18n (594, "        URL: %s" COLMESS "%s" COLNONE "\n"),
                     tcp ? MSGTCPRECSTR : MSGRECSTR, url_url);
            break;
        case CONTACT_MESS:
        case MRCONTACT_MESS:
            tmp = strchr (cdata, '\xFE');
            *tmp = 0;
            M_print (i18n (595, "\nContact List.\n" COLMESS "============================================\n" COLNONE "%d Contacts\n"),
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
    /* aaron
       If we just received a message from someone on the contact list,
       save it with the person's contact information. If they are not in
       the list, don't do anything special with it.                              */
    if (ContactFind (uiG.last_rcvd_uin) != NULL)
    {
        ContactFind (uiG.last_rcvd_uin)->LastMessage =
           realloc (ContactFind (uiG.last_rcvd_uin)->LastMessage, len + 5);
        /* I add on so many characters because I always have segfaults in
           my own program when I try to allocate strings like this. Somehow
           it tries to write too much to the string even though I think I
           allocate the right amount. Oh well. It shouldn't be too much
           wasted space, I hope.                                                          */
        ConvWinUnix (cdata);
        strcpy (ContactFind (uiG.last_rcvd_uin)->LastMessage, cdata);
    }
    /* end of aaron */
}
