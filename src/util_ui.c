/* $Id$ */
/* Copyright: This file may be distributed under version 2 of the GPL licence. */

#include "micq.h"
#include <stdarg.h>
#include "util_ui.h"
#include "util_table.h"
#include "color.h"
#include "contact.h"
#include "preferences.h"
#include "conv.h"

static const char *DebugStr (UDWORD level);

/*
 * Returns string describing debug level
 */
static const char *DebugStr (UDWORD level)
{
    if (level & DEB_PACKET)      return "Packet ";
    if (level & DEB_QUEUE)       return "Queue  ";
    if (level & DEB_EVENT)       return "Event  ";
    if (level & DEB_OPTS)        return "Options";
    if (level & DEB_PACK5DATA)   return "v5 data";
    if (level & DEB_PACK8)       return "v8 pack";
    if (level & DEB_PACK8DATA)   return "v8 data";
    if (level & DEB_PACK8SAVE)   return "v8 save";
    if (level & DEB_PACKTCP)     return "TCPpack";
    if (level & DEB_PACKTCPDATA) return "TCPdata";
    if (level & DEB_PACKTCPSAVE) return "TCPsave";
    if (level & DEB_PROTOCOL)    return "Protocl";
    if (level & DEB_TCP)         return "TCP HS ";
    if (level & DEB_IO)          return "In/Out ";
    if (level & DEB_CONNECT)     return "Connect";
    if (level & DEB_CONTACT)     return "Contact";
    if (level & DEB_SSL)         return "SSL    ";
    return "unknown";
}

/*
 * Output a given string if the debug level is appropriate.
 */
BOOL DebugReal (UDWORD level, const char *str, ...)
{
    va_list args;
    char buf[2048], c;
    const char *name;

    if (!(prG->verbose & level) && level)
        return 0;

    va_start (args, str);
    vsnprintf (buf, sizeof (buf), str, args);
    va_end (args);

    name = DebugStr (level & prG->verbose);
    level = prG->verbose;
    prG->verbose = 0;

    M_print ("");
    if ((c = M_pos ()))
        M_print ("\n");
    
    M_printf ("%s %s%7.7s%s %s", s_now, COLDEBUG, name, COLNONE, buf);

    if (!c)
        M_print ("\n");

    prG->verbose = level;

    return 1;
}

#define mq s_wordquote

/*
 * Display all meta information regarding a given contact.
 */
void UtilUIDisplayMeta (Contact *cont)
{
    MetaGeneral *mg;
    MetaMore *mm;
    MetaWork *mw;
    ContactMeta *ml;
    MetaObsolete *mo;
    ContactDC *dc;
    const char *tabd;

    if (!cont)
        return;

    if ((cont->updated & (UPF_GENERAL_E | UPF_GENERAL_A)) == UPF_GENERAL_E)
        M_printf ("%s %s%lu%s\n", i18n (1967, "More Info for"), COLSERVER, cont->uin, COLNONE);
    else
        M_printf (i18n (2236, "Information for %s%s%s (%ld):\n"),
                  COLCONTACT, cont->nick, COLNONE, cont->uin);
    
    if ((dc = cont->dc))
    {
        M_printf ("%s%-15s%s %s%s%s:%s%ld%s\n", COLSERVER, i18n (9999, "IP:"), COLNONE,
                  COLQUOTE, s_ip (dc->ip_rem), COLNONE, COLQUOTE, dc->port, COLNONE);
    }

    if ((mg = cont->meta_general) && cont->updated & (UPF_GENERAL_A | UPF_DISC))
    {
        M_printf ("%s%-15s%s %s\t%s\n", COLSERVER, i18n (1500, "Nickname:"), COLNONE,
                  s_mquote (mg->nick, COLCONTACT, 0),
                  mg->auth == 1 ? i18n (1943, "(no authorization needed)")
                                : i18n (1944, "(must request authorization)"));
        if (mg->first && *mg->first && mg->last && *mg->last)
        {
            M_printf ("%s%-15s%s %s\t ", COLSERVER, i18n (1501, "Name:"), COLNONE, mq (mg->first));
            M_printf ("%s\n", mq (mg->last));
        }
        else if (mg->first && *mg->first)
            M_printf (AVPFMT, COLSERVER, i18n (1564, "First name:"), COLNONE, mq (mg->first));
        else if (mg->first && *mg->last)
            M_printf (AVPFMT, COLSERVER, i18n (1565, "Last name:"), COLNONE, mq (mg->last));
        if (mg->email && *mg->email)
            M_printf ("%s%-15s%s %s\n", 
                      COLSERVER, i18n (1566, "Email address:"), COLNONE, mq (mg->email));
    }
    if (mg && cont->updated & (UPF_GENERAL_B | UPF_GENERAL_E | UPF_DISC))
    {
        if (mg->city && *mg->city && mg->state && *mg->state)
        {
            M_printf ("%s%-15s%s %s, ", COLSERVER, i18n (1505, "Location:"), COLNONE, mq (mg->city));
            M_printf ("%s\n", mq (mg->state));
        }
        else if (mg->city && *mg->city)
            M_printf (AVPFMT, COLSERVER, i18n (1570, "City:"), COLNONE, mq (mg->city));
        else if (mg->state && *mg->state)
            M_printf (AVPFMT, COLSERVER, i18n (1574, "State:"), COLNONE, mq (mg->state));
        if (mg->phone && *mg->phone)
            M_printf (AVPFMT, COLSERVER, i18n (1506, "Phone:"), COLNONE, mq (mg->phone));
        if (mg->fax && *mg->fax)
            M_printf (AVPFMT, COLSERVER, i18n (1507, "Fax:"), COLNONE, mq (mg->fax));
    }
    if (mg && cont->updated & (UPF_GENERAL_B | UPF_DISC))
    {
        if (mg->street && *mg->street)
            M_printf (AVPFMT, COLSERVER, i18n (1508, "Street:"), COLNONE, mq (mg->street));
        if (mg->cellular && *mg->cellular)
            M_printf (AVPFMT, COLSERVER, i18n (1509, "Cellular:"), COLNONE, mq (mg->cellular));
        if (mg->zip && *mg->zip)
            M_printf (AVPFMT, COLSERVER, i18n (1510, "Zip:"), COLNONE, mq (mg->zip));
    }
    if (mg && cont->updated & (UPF_GENERAL_B | UPF_GENERAL_E | UPF_DISC))
    {
        if ((tabd = TableGetCountry (mg->country)) != NULL)
            M_printf ("%s%-15s%s %s\t", 
                      COLSERVER, i18n (1511, "Country:"), COLNONE, mq (tabd));
        else
            M_printf ("%s%-15s%s %s%d%s\t", 
                      COLSERVER, i18n (1512, "Country code:"), COLNONE, COLQUOTE, mg->country, COLNONE);
        M_printf ("(UTC %+05d)\n", -100 * (mg->tz / 2) + 30 * (mg->tz % 2));
    }
    if (mg && cont->updated & (UPF_GENERAL_C | UPF_DISC))
    {
        M_printf ("%s%-15s%s %s\n", COLSERVER, i18n (2237, "Webaware:"), COLNONE,
                       !mg->webaware      ? i18n (1969, "offline") :
                        mg->webaware == 1 ? i18n (1970, "online")  :
                        mg->webaware == 2 ? i18n (1888, "not webaware") :
                                            s_sprintf ("%d", mg->webaware));
    }
    /* FIXME Needs to be flagged if this is set, otherwise it's output as zero
     * which is surely not correct in all cases.
     */
    if (mg && 0)
    {
        M_printf ("%s%-15s%s %s%d%s\n",
                  COLSERVER, i18n (2238, "Hide IP:"), COLNONE, COLQUOTE, mg->hideip, COLNONE);
    }
    if ((ml = cont->meta_email))
    {
        M_printf ("%s%-15s%s\n", 
                  COLSERVER, i18n (1942, "Additional Email addresses:"), COLNONE);
        for ( ; ml; ml = ml->next)
        {
            if (ml->text && *ml->text)
                M_printf (" %s %s\n", mq (ml->text),
                           ml->data == 1 ? i18n (1943, "(no authorization needed)") 
                         : ml->data == 0 ? i18n (1944, "(must request authorization)")
                         : "");
        }
    }
    if ((mm = cont->meta_more) 
        && cont->updated & (UPF_MORE | UPF_GENERAL_C | UPF_GENERAL_E | UPF_DISC))
    {
        if (mm->age && ~mm->age)
            M_printf ("%s%-15s%s %s%d%s\n", 
                      COLSERVER, i18n (1575, "Age:"), COLNONE, COLQUOTE, mm->age, COLNONE);
        else
            M_printf ("%s%-15s%s %s\n", 
                      COLSERVER, i18n (1575, "Age:"), COLNONE, i18n (1200, "not entered"));

        M_printf ("%s%-15s%s %s%s%s\n",
                  COLSERVER, i18n (1696, "Sex:"), COLNONE, COLQUOTE,
                   mm->sex == 1 ? i18n (1528, "female")
                 : mm->sex == 2 ? i18n (1529, "male")
                 :                i18n (1530, "not specified"), COLNONE);
    }
    if (mm && cont->updated & (UPF_MORE | UPF_GENERAL_E | UPF_DISC))
    {
        if (mm->homepage && *mm->homepage)
            M_printf (AVPFMT, COLSERVER, i18n (1531, "Homepage:"), COLNONE, mq (mm->homepage));
    }
    if (mm && cont->updated & (UPF_MORE | UPF_DISC))
    {
        if (mm->month >= 1 && mm->month <= 12 && mm->day && mm->day < 32 && mm->year)
            M_printf ("%s%-15s%s %s%02d. %s %4d%s\n", 
                      COLSERVER, i18n (1532, "Born:"), COLNONE, COLQUOTE, mm->day, TableGetMonth (mm->month), mm->year, COLNONE);

        if (!mm->lang1)
        {
            if (!mm->lang2)
            {
                mm->lang1 = mm->lang3;
                mm->lang2 = 0;
                mm->lang3 = 0;
            }
            else
            {
                mm->lang1 = mm->lang2;
                mm->lang2 = mm->lang3;
                mm->lang3 = 0;
            }
        }
        if (mm->lang1)
        {
            M_printf ("%s%-15s%s ", COLSERVER, i18n (1533, "Languages:"), COLNONE);
            if ((tabd = TableGetLang (mm->lang1)))
                M_printf ("%s%s%s", COLQUOTE, tabd, COLNONE);
            else
                M_printf ("%x", mm->lang1);
            if (mm->lang2 && (tabd = TableGetLang (mm->lang2)))
                M_printf (", %s%s%s", COLQUOTE, tabd, COLNONE);
            else if (mm->lang2)
                M_printf (", %x", mm->lang2);
            if (mm->lang3 && (tabd = TableGetLang (mm->lang3)))
                M_printf (", %s%s%s", COLQUOTE, tabd, COLNONE);
            else if (mm->lang3)
                M_printf (", %x", mm->lang3);
            M_print ("\n");
        }
        if (mm->unknown)
            M_printf ("%s%-15s%s %s%d%s\n", COLSERVER, i18n (2239, "Unknown more:"), COLNONE,
                      COLQUOTE, mm->unknown, COLNONE);
    }
    if ((mw = cont->meta_work))
    {
        if (mw->wcity && *mw->wcity && mw->wstate && *mw->wstate)
        {
            M_printf ("%s%-15s%s %s, %s\n", COLSERVER, i18n (1524, "Work Location:"), COLNONE,
                      mq (mw->wcity), mq (mw->wstate));
        }
        else if (mw->wcity && *mw->wcity)
            M_printf (AVPFMT, COLSERVER, i18n (1873, "Work City:"), COLNONE, mq (mw->wcity));
        else if (mw->wstate && *mw->wstate)
            M_printf (AVPFMT, COLSERVER, i18n (1874, "Work State:"), COLNONE, mq (mw->wstate));
        if (mw->wphone && *mw->wphone)
            M_printf (AVPFMT, COLSERVER, i18n (1523, "Work Phone:"), COLNONE, mq (mw->wphone));
        if (mw->wfax && *mw->wfax)
            M_printf (AVPFMT, COLSERVER, i18n (1521, "Work Fax:"), COLNONE, mq (mw->wfax));
        if (mw->waddress && *mw->waddress)
            M_printf (AVPFMT, COLSERVER, i18n (1522, "Work Address:"), COLNONE, mq (mw->waddress));
        if (mw->wzip && *mw->wzip)
            M_printf (AVPFMT, COLSERVER, i18n (1520, "Work Zip:"), COLNONE, mq (mw->wzip));
        if (mw->wcountry)
        {
            if ((tabd = TableGetCountry (mw->wcountry)))
                M_printf ("%s%-15s%s %s%s%s\n", COLSERVER, i18n (1514, "Work Country:"),
                          COLNONE, COLQUOTE, tabd, COLNONE);
            else
                M_printf ("%s%-15s%s %s%d%s\n", COLSERVER, i18n (1513, "Work Country Code:"),
                          COLNONE, COLQUOTE, mw->wcountry, COLNONE);
        }
        if (mw->wcompany && *mw->wcompany)
            M_printf (AVPFMT, COLSERVER, i18n (1519, "Company Name:"), COLNONE, mq (mw->wcompany));
        if (mw->wdepart && *mw->wdepart)
            M_printf (AVPFMT, COLSERVER, i18n (1518, "Department:"), COLNONE, mq (mw->wdepart));
        if (mw->wposition && *mw->wposition)
            M_printf (AVPFMT, COLSERVER, i18n (1517, "Job Position:"), COLNONE, mq (mw->wposition));
        if (mw->woccupation)
            M_printf ("%s%-15s%s %s%s%s\n", 
                      COLSERVER, i18n (1516, "Occupation:"), COLNONE,
                      COLQUOTE, TableGetOccupation (mw->woccupation), COLNONE);
        if (mw->whomepage && *mw->whomepage)
            M_printf (AVPFMT, COLSERVER, i18n (1515, "Work Homepage:"), COLNONE, mq (mw->whomepage));
    }
    if ((ml = cont->meta_interest))
    {
        M_printf ("%s%-15s%s\n", COLSERVER, i18n (1875, "Personal interests:"), COLNONE);
        for ( ; ml; ml = ml->next)
        {
            if (!ml->text)
                continue;
            if ((tabd = TableGetInterest (ml->data)))
                M_printf ("  %s: %s\n", tabd, mq (ml->text));
            else
                M_printf ("  %d: %s\n", ml->data, mq (ml->text));
        }
    }
    if ((ml = cont->meta_background))
    {
        M_printf ("%s%-15s%s\n", COLSERVER, i18n (1876, "Personal past background:"), COLNONE);
        for ( ; ml; ml = ml->next)
        {
            if (!ml->text)
                continue;
            if ((tabd = TableGetPast (ml->data)))
                M_printf ("  %s: %s\n", tabd, mq (ml->text));
            else
                M_printf ("  %d: %s\n", ml->data, mq (ml->text));
        }
    }
    if ((ml = cont->meta_affiliation))
    {
        M_printf ("%s%-15s%s\n", COLSERVER, i18n (1879, "Affiliations:"), COLNONE);
        for ( ; ml; ml = ml->next)
        {
            if (!ml->text)
                continue;
            if ((tabd = TableGetAffiliation (ml->data)))
                M_printf ("  %s: %s\n", tabd, mq (ml->text));
            else
                M_printf ("  %d: %s\n", ml->data, mq (ml->text));
        }
    }
    if ((mo = cont->meta_obsolete))
    {
        if (mo->unknown)
            M_printf ("%s%-15s%s %s%04x%s = %s%d%s\n", COLSERVER, i18n (2195, "Obsolete number:"),
                      COLNONE, COLQUOTE, mo->unknown, COLNONE, COLQUOTE, mo->unknown, COLNONE);
        if (mo->description && *mo->description)
            M_printf ("%s%-15s%s %s\n", COLSERVER, i18n (2196, "Obsolete text:"),
                      COLNONE, mq (mo->description));
        if (mo->empty)
            M_printf ("%s%-15s%s %s%d%s\n", COLSERVER, i18n (2197, "Obsolete byte"), COLNONE,
                      COLQUOTE, mo->empty, COLNONE);
    }
    if (cont->meta_about && *cont->meta_about)
        M_printf ("%s%-15s%s\n%s\n", COLSERVER, 
                  i18n (1525, "About:"), COLNONE, s_msgquote (s_ind (cont->meta_about)));
}
