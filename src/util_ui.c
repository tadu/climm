/* $Id$ */
/* Copyright: This file may be distributed under version 2 of the GPL licence. */

#include "micq.h"
#include <stdarg.h>
#include "util_ui.h"
#include "util_str.h"
#include "util_table.h"
#include "util_extra.h"
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
    if (level & DEB_EXTRA)       return "Extra  ";
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
    return "unknown";
}

/*
 * Output a given string if the debug level is appropriate.
 */
BOOL Debug (UDWORD level, const char *str, ...)
{
    va_list args;
    char buf[2048], c;
    const char *name = DebugStr (level & prG->verbose);

    if (!(prG->verbose & level) && level)
        return 0;

    va_start (args, str);
    vsnprintf (buf, sizeof (buf), str, args);
    va_end (args);

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

#define mq(s) s_mquote(s,COLQUOTE,0)

/*
 * Display all meta information regarding a given contact.
 */
void UtilUIDisplayMeta (Contact *cont)
{
    MetaGeneral *mg;
    MetaMore *mm;
    MetaEmail *me;
    MetaWork *mw;
    Extra *ml;
    MetaObsolete *mo;
    ContactDC *dc;
    const char *tabd;

    if (!cont)
        return;

    if ((cont->updated & (UPF_GENERAL_E | UPF_GENERAL_A)) == UPF_GENERAL_E)
        M_printf ("%s " COLSERVER "%lu" COLNONE "\n", i18n (1967, "More Info for"), cont->uin);
    else
        M_printf (i18n (2236, "Information for %s%s%s (%ld):\n"),
                  COLCONTACT, cont->nick, COLNONE, cont->uin);
    
    if ((dc = cont->dc))
    {
        M_printf (COLSERVER "%-15s" COLNONE " " COLQUOTE "%s" COLNONE ":" COLQUOTE "%ld" COLNONE "\n",
                  i18n (9999, "IP:"), s_ip (dc->ip_rem), dc->port);
    }

    if ((mg = cont->meta_general) && cont->updated & (UPF_GENERAL_A | UPF_DISC))
    {
        M_printf (COLSERVER "%-15s" COLNONE " %s\t%s\n",
                  i18n (1500, "Nickname:"), mg->nick ? s_mquote (mg->nick, COLCONTACT, 0) : "",
                  mg->auth == 1 ? i18n (1943, "(no authorization needed)")
                                 : i18n (1944, "(must request authorization)"));
        if (mg->first && *mg->first && mg->last && *mg->last)
        {
            M_printf (COLSERVER "%-15s" COLNONE " %s\t ", i18n (1501, "Name:"), mq (mg->first));
            M_printf ("%s\n", mq (mg->last));
        }
        else if (mg->first && *mg->first)
            M_printf (AVPFMT, i18n (1564, "First name:"), mq (mg->first));
        else if (mg->first && *mg->last)
            M_printf (AVPFMT, i18n (1565, "Last name:"), mq (mg->last));
        if (mg->email && *mg->email)
            M_printf (COLSERVER "%-15s" COLNONE " %s\n", 
                      i18n (1566, "Email address:"), mq (mg->email));
    }
    if (mg && cont->updated & (UPF_GENERAL_B | UPF_GENERAL_E | UPF_DISC))
    {
        if (mg->city && *mg->city && mg->state && *mg->state)
        {
            M_printf (COLSERVER "%-15s" COLNONE " %s, ", i18n (1505, "Location:"), mq (mg->city));
            M_printf ("%s\n", mq (mg->state));
        }
        else if (mg->city && *mg->city)
            M_printf (AVPFMT, i18n (1570, "City:"), mq (mg->city));
        else if (mg->state && *mg->state)
            M_printf (AVPFMT, i18n (1574, "State:"), mq (mg->state));
        if (mg->phone && *mg->phone)
            M_printf (AVPFMT, i18n (1506, "Phone:"), mq (mg->phone));
        if (mg->fax && *mg->fax)
            M_printf (AVPFMT, i18n (1507, "Fax:"), mq (mg->fax));
    }
    if (mg && cont->updated & (UPF_GENERAL_B | UPF_DISC))
    {
        if (mg->street && *mg->street)
            M_printf (AVPFMT, i18n (1508, "Street:"), mq (mg->street));
        if (mg->cellular && *mg->cellular)
            M_printf (AVPFMT, i18n (1509, "Cellular:"), mq (mg->cellular));
        if (mg->zip && *mg->zip)
            M_printf (AVPFMT, i18n (1510, "Zip:"), mq (mg->zip));
    }
    if (mg && cont->updated & (UPF_GENERAL_B | UPF_GENERAL_E | UPF_DISC))
    {
        if ((tabd = TableGetCountry (mg->country)) != NULL)
            M_printf (COLSERVER "%-15s" COLNONE " %s\t", 
                     i18n (1511, "Country:"), mq (tabd));
        else
            M_printf (COLSERVER "%-15s" COLNONE " " COLQUOTE "%d" COLNONE "\t", 
                     i18n (1512, "Country code:"), mg->country);
        M_printf ("(UTC %+05d)\n", -100 * (mg->tz / 2) + 30 * (mg->tz % 2));
    }
    if (mg && cont->updated & (UPF_GENERAL_C | UPF_DISC))
    {
        M_printf (COLSERVER "%-15s" COLNONE " %s\n", i18n (2237, "Webaware:"),
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
        M_printf (COLSERVER "%-15s" COLNONE " " COLQUOTE "%d" COLNONE "\n",
                  i18n (2238, "Hide IP:"), mg->hideip);
    }
    if ((me = cont->meta_email))
    {
        M_printf (COLSERVER "%-15s" COLNONE "\n", 
                  i18n (1942, "Additional Email addresses:"));

        for ( ; me; me = me->meta_email)
        {
            if (me->email && *me->email)
                M_printf (" " COLQUOTE "%s" COLNONE " %s\n", mq (me->email),
                           me->auth == 1 ? i18n (1943, "(no authorization needed)") 
                         : me->auth == 0 ? i18n (1944, "(must request authorization)")
                         : "");
        }
    }
    if ((mm = cont->meta_more) 
        && cont->updated & (UPF_MORE | UPF_GENERAL_C | UPF_GENERAL_E | UPF_DISC))
    {
        if (mm->age && ~mm->age)
            M_printf (COLSERVER "%-15s" COLNONE " " COLQUOTE "%d" COLNONE "\n", 
                     i18n (1575, "Age:"), mm->age);
        else
            M_printf (COLSERVER "%-15s" COLNONE " %s\n", 
                     i18n (1575, "Age:"), i18n (1200, "not entered"));

        M_printf (COLSERVER "%-15s" COLNONE " " COLQUOTE "%s" COLNONE "\n", i18n (1696, "Sex:"),
                   mm->sex == 1 ? i18n (1528, "female")
                 : mm->sex == 2 ? i18n (1529, "male")
                 :                i18n (1530, "not specified"));
    }
    if (mm && cont->updated & (UPF_MORE | UPF_GENERAL_E | UPF_DISC))
    {
        if (mm->homepage && *mm->homepage)
            M_printf (AVPFMT, i18n (1531, "Homepage:"), mq (mm->homepage));
    }
    if (mm && cont->updated & (UPF_MORE | UPF_DISC))
    {
        if (mm->month >= 1 && mm->month <= 12 && mm->day && mm->day < 32 && mm->year)
            M_printf (COLSERVER "%-15s" COLNONE " " COLQUOTE "%02d. %s %4d" COLNONE "\n", 
                i18n (1532, "Born:"), mm->day, TableGetMonth (mm->month), mm->year);

        M_printf (COLSERVER "%-15s" COLNONE " ", i18n (1533, "Languages:"));
        if ((tabd = TableGetLang (mm->lang1)))
            M_printf (COLQUOTE "%s" COLNONE, tabd);
        else
            M_printf ("%x", mm->lang1);
        if ((tabd = TableGetLang (mm->lang2)))
            M_printf (", " COLQUOTE "%s" COLNONE, tabd);
        else
            M_printf (", %x", mm->lang2);
        if ((tabd = TableGetLang (mm->lang3)))
            M_printf (", " COLQUOTE "%s" COLNONE ".\n", tabd);
        else
            M_printf (", %x.\n", mm->lang3);
        if (mm->unknown)
            M_printf (COLSERVER "%-15s" COLNONE " " COLQUOTE "%d" COLNONE "\n", i18n (2239, "Unknown more:"), mm->unknown);
    }
    if ((mw = cont->meta_work))
    {
        if (mw->wcity && *mw->wcity && mw->wstate && *mw->wstate)
        {
            M_printf (COLSERVER "%-15s" COLNONE " %s, ", i18n (1524, "Work Location:"), mq (mw->wcity));
            M_printf ("%s\n", mq (mw->wstate));
        }
        else if (mw->wcity && *mw->wcity)
            M_printf (AVPFMT, i18n (1873, "Work City:"), mq (mw->wcity));
        else if (mw->wstate && *mw->wstate)
            M_printf (AVPFMT, i18n (1874, "Work State:"), mq (mw->wstate));
        if (mw->wphone && *mw->wphone)
            M_printf (AVPFMT, i18n (1523, "Work Phone:"), mq (mw->wphone));
        if (mw->wfax && *mw->wfax)
            M_printf (AVPFMT, i18n (1521, "Work Fax:"), mq (mw->wfax));
        if (mw->waddress && *mw->waddress)
            M_printf (AVPFMT, i18n (1522, "Work Address:"), mq (mw->waddress));
        if (mw->wzip && *mw->wzip)
            M_printf (AVPFMT, i18n (1520, "Work Zip:"), mq (mw->wzip));
        if (mw->wcountry)
        {
            if ((tabd = TableGetCountry (mw->wcountry)))
                M_printf (COLSERVER "%-15s" COLNONE " " COLQUOTE "%s" COLNONE "\n", 
                         i18n (1514, "Work Country:"), tabd);
            else
                M_printf (COLSERVER "%-15s" COLNONE " " COLQUOTE "%d" COLNONE "\n", 
                         i18n (1513, "Work Country Code:"), mw->wcountry);
        }
        if (mw->wcompany && *mw->wcompany)
            M_printf (AVPFMT, i18n (1519, "Company Name:"), mq (mw->wcompany));
        if (mw->wdepart && *mw->wdepart)
            M_printf (AVPFMT, i18n (1518, "Department:"), mq (mw->wdepart));
        if (mw->wposition && *mw->wposition)
            M_printf (AVPFMT, i18n (1517, "Job Position:"), mq (mw->wposition));
        if (mw->woccupation)
            M_printf (COLSERVER "%-15s" COLNONE " " COLQUOTE "%s" COLNONE "\n", 
                      i18n (1516, "Occupation:"), TableGetOccupation (mw->woccupation));
        if (mw->whomepage && *mw->whomepage)
            M_printf (AVPFMT, i18n (1515, "Work Homepage:"), mq (mw->whomepage));
    }
    if ((ml = cont->meta_interest))
    {
        M_printf (COLSERVER "%-15s" COLNONE "\n", i18n (1875, "Personal interests:"));
        for ( ; ml; ml = ml->more)
        {
            if (!ml->text)
                continue;
            if ((tabd = TableGetInterest (ml->data)))
                M_printf ("  %s: %s\n", tabd, mq (ml->text));
            else
                M_printf ("  %ld: %s\n", ml->data, mq (ml->text));
        }
    }
    if ((ml = cont->meta_background))
    {
        M_printf (COLSERVER "%-15s" COLNONE "\n", i18n (1876, "Personal past background:"));
        for ( ; ml; ml = ml->more)
        {
            if (!ml->text)
                continue;
            if ((tabd = TableGetPast (ml->data)))
                M_printf ("  %s: %s\n", tabd, mq (ml->text));
            else
                M_printf ("  %ld: %s\n", ml->data, mq (ml->text));
        }
    }
    if ((ml = cont->meta_affiliation))
    {
        M_printf (COLSERVER "%-15s" COLNONE "\n", i18n (1879, "Affiliations:"));
        for ( ; ml; ml = ml->more)
        {
            if (!ml->text)
                continue;
            if ((tabd = TableGetAffiliation (ml->data)))
                M_printf ("  %s: %s\n", tabd, mq (ml->text));
            else
                M_printf ("  %ld: %s\n", ml->data, mq (ml->text));
        }
    }
    if ((mo = cont->meta_obsolete))
    {
        if (mo->unknown)
            M_printf (COLSERVER "%-15s" COLNONE " " COLQUOTE "%04x" COLNONE " = " COLQUOTE "%d" COLNONE "\n",
                      i18n (2195, "Obsolete number:"), mo->unknown, mo->unknown);
        if (mo->description && *mo->description)
            M_printf (COLSERVER "%-15s" COLNONE " %s\n",
                      i18n (2196, "Obsolete text:"), mq (mo->description));
        if (mo->empty)
            M_printf (COLSERVER "%-15s" COLNONE " " COLQUOTE "%d" COLNONE "\n",
                      i18n (2197, "Obsolete byte"), mo->empty);
    }
    if (cont->meta_about && *cont->meta_about)
        M_printf (COLSERVER "%-15s" COLNONE "\n%s\n",
                  i18n (1525, "About:"), s_mquote (s_ind (cont->meta_about), COLQUOTE, 1));
}
