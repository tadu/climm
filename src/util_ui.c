/* $Id$ */
/* Copyright: This file may be distributed under version 2 of the GPL licence. */

#include "micq.h"
#include <stdarg.h>
#include "util_ui.h"
#include "util_table.h"
#include "color.h"
#include "contact.h"
#include "preferences.h"
#include "util_rl.h"
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
    if (level & DEB_XMPPIN)      return "XMPPIn ";
    if (level & DEB_XMPPOUT)     return "XMPPOut";
    if (level & DEB_XMPPOTHER)   return "XMPPOth";
    return "unknown";
}

/*
 * Output a given string if the debug level is appropriate.
 */
BOOL DebugReal (UDWORD level, const char *str, ...)
{
    va_list args;
    char buf[2048], c;
    const char *name, *coldebug, *colnone;

    if (!(prG->verbose & level) && level)
        return 0;

    va_start (args, str);
    vsnprintf (buf, sizeof (buf), str, args);
    va_end (args);

    name = DebugStr (level & prG->verbose);
    level = prG->verbose;
    prG->verbose = 0;
    coldebug = COLDEBUG;
    colnone = COLNONE;
    prG->verbose = 8;

    rl_print ("");
    if ((c = rl_pos ()))
        rl_print ("\n");
    rl_printf ("%s %s%7.7s%s %s", s_now, coldebug, name, colnone, buf);
    if (!c)
        rl_print ("\n");
    prG->verbose = level;

    return 1;
}

#define mq s_wordquote
#define al(s) ReadLinePrintWidth (s, COLSERVER, COLNONE, &width)

/*
 * Display all meta information regarding a given contact.
 */
void UtilUIDisplayMeta (Contact *cont)
{
    static UWORD width = 15;
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
        rl_printf ("%s %s%s%s\n", i18n (1967, "More Info for"), COLSERVER, cont->screen, COLNONE);
    else
        rl_printf (i18n (2610, "Information for %s%s%s (%s):\n"),
                  COLCONTACT, cont->nick, COLNONE, cont->screen);
    
    if ((dc = cont->dc))
    {
        rl_print (al (i18n (2534, "IP:")));
        rl_printf (" %s%s%s:%s%ld%s\n", COLQUOTE, s_ip (dc->ip_rem), COLNONE, COLQUOTE, dc->port, COLNONE);
    }

    if ((mg = cont->meta_general) && cont->updated & (UPF_GENERAL_A | UPF_DISC))
    {
        rl_printf ("%s %s\t%s\n", al (i18n (1500, "Nickname:")),
                  s_mquote (mg->nick, COLCONTACT, 0),
                  mg->auth == 1 ? i18n (1943, "(no authorization needed)")
                                : i18n (1944, "(must request authorization)"));
        if (mg->first && *mg->first && mg->last && *mg->last)
        {
            rl_printf ("%s %s\t ", al (i18n (1501, "Name:")), mq (mg->first));
            rl_printf ("%s\n", mq (mg->last));
        }
        else if (mg->first && *mg->first)
            rl_printf ("%s %s\n", al (i18n (1564, "First name:")), mq (mg->first));
        else if (mg->first && *mg->last)
            rl_printf ("%s %s\n", al (i18n (1565, "Last name:")), mq (mg->last));
        if (mg->email && *mg->email)
            rl_printf ("%s %s\n", al (i18n (1566, "Email address:")), mq (mg->email));
    }
    if (mg && cont->updated & (UPF_GENERAL_B | UPF_GENERAL_E | UPF_DISC))
    {
        if (mg->city && *mg->city && mg->state && *mg->state)
        {
            rl_printf ("%s %s, ", al (i18n (1505, "Location:")), mq (mg->city));
            rl_printf ("%s\n", mq (mg->state));
        }
        else if (mg->city && *mg->city)
            rl_printf ("%s %s\n", al (i18n (1570, "City:")), mq (mg->city));
        else if (mg->state && *mg->state)
            rl_printf ("%s %s\n", al (i18n (1574, "State:")), mq (mg->state));
        if (mg->phone && *mg->phone)
            rl_printf ("%s %s\n", al (i18n (1506, "Phone:")), mq (mg->phone));
        if (mg->fax && *mg->fax)
            rl_printf ("%s %s\n", al (i18n (1507, "Fax:")), mq (mg->fax));
    }
    if (mg && cont->updated & (UPF_GENERAL_B | UPF_DISC))
    {
        if (mg->street && *mg->street)
            rl_printf ("%s %s\n", al (i18n (1508, "Street:")), mq (mg->street));
        if (mg->cellular && *mg->cellular)
            rl_printf ("%s %s\n", al (i18n (1509, "Cellular:")), mq (mg->cellular));
        if (mg->zip && *mg->zip)
            rl_printf ("%s %s\n", al (i18n (1510, "Zip:")), mq (mg->zip));
    }
    if (mg && cont->updated & (UPF_GENERAL_B | UPF_GENERAL_E | UPF_DISC))
    {
        if ((tabd = TableGetCountry (mg->country)) != NULL)
            rl_printf ("%s %s\t", al (i18n (1511, "Country:")), mq (tabd));
        else
            rl_printf ("%s %s%d%s\t", al (i18n (1512, "Country code:")), COLQUOTE, mg->country, COLNONE);
        rl_printf ("(UTC %+05d)\n", -100 * (mg->tz / 2) + 30 * (mg->tz % 2));
    }
    if (mg && cont->updated & (UPF_GENERAL_C | UPF_DISC))
    {
        rl_printf ("%s %s\n", al (i18n (2237, "Webaware:")),
                       !mg->webaware      ? i18n (1969, "offline") :
                        mg->webaware == 1 ? i18n (1970, "online")  :
                        mg->webaware == 2 ? i18n (1888, "not webaware") :
                                            s_sprintf ("%d", mg->webaware));
    }

#if 0
    /* FIXME Needs to be flagged if this is set, otherwise it's output as zero
     * which is surely not correct in all cases.
     */
    i18n (2238, "Hide IP:")
#endif
    if ((ml = cont->meta_email))
    {
        rl_printf ("%s%s%s\n", COLSERVER, i18n (1942, "Additional Email addresses:"), COLNONE);
        for ( ; ml; ml = ml->next)
        {
            if (ml->text && *ml->text)
                rl_printf (" %s %s\n", mq (ml->text),
                           ml->data == 1 ? i18n (1943, "(no authorization needed)") 
                         : ml->data == 0 ? i18n (1944, "(must request authorization)")
                         : "");
        }
    }
    if ((mm = cont->meta_more) 
        && cont->updated & (UPF_MORE | UPF_GENERAL_C | UPF_GENERAL_E | UPF_DISC))
    {
        if (mm->age && ~mm->age)
            rl_printf ("%s %s%u%s\n", al (i18n (1575, "Age:")), COLQUOTE, mm->age, COLNONE);
        else
            rl_printf ("%s %s\n", al (i18n (1575, "Age:")), i18n (1200, "not entered"));

        rl_printf ("%s %s%s%s\n", al (i18n (1696, "Sex:")), COLQUOTE,
                   mm->sex == 1 ? i18n (1528, "female")
                 : mm->sex == 2 ? i18n (1529, "male")
                 :                i18n (1530, "not specified"), COLNONE);
    }
    if (mm && cont->updated & (UPF_MORE | UPF_GENERAL_E | UPF_DISC))
    {
        if (mm->homepage && *mm->homepage)
            rl_printf ("%s %s\n", al (i18n (1531, "Homepage:")), mq (mm->homepage));
    }
    if (mm && cont->updated & (UPF_MORE | UPF_DISC))
    {
        if (mm->month >= 1 && mm->month <= 12 && mm->day && mm->day < 32 && mm->year)
            rl_printf ("%s %s%02d. %s %4d%s\n", al (i18n (1532, "Born:")),
                       COLQUOTE, mm->day, TableGetMonth (mm->month), mm->year, COLNONE);

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
            rl_printf ("%s ", al (i18n (1533, "Languages:")));
            if ((tabd = TableGetLang (mm->lang1)))
                rl_printf ("%s%s%s", COLQUOTE, tabd, COLNONE);
            else
                rl_printf ("%x", mm->lang1);
            if (mm->lang2 && (tabd = TableGetLang (mm->lang2)))
                rl_printf (", %s%s%s", COLQUOTE, tabd, COLNONE);
            else if (mm->lang2)
                rl_printf (", %x", mm->lang2);
            if (mm->lang3 && (tabd = TableGetLang (mm->lang3)))
                rl_printf (", %s%s%s", COLQUOTE, tabd, COLNONE);
            else if (mm->lang3)
                rl_printf (", %x", mm->lang3);
            rl_print ("\n");
        }
        if (mm->unknown)
            rl_printf ("%s %s%d%s\n", al (i18n (2239, "Unknown more:")),
                      COLQUOTE, mm->unknown, COLNONE);
    }
    if ((mw = cont->meta_work))
    {
        if (mw->wcity && *mw->wcity && mw->wstate && *mw->wstate)
        {
            rl_printf ("%s %s, %s\n", al (i18n (1524, "Work Location:")),
                      mq (mw->wcity), mq (mw->wstate));
        }
        else if (mw->wcity && *mw->wcity)
            rl_printf ("%s %s\n", al (i18n (1873, "Work City:")), mq (mw->wcity));
        else if (mw->wstate && *mw->wstate)
            rl_printf ("%s %s\n", al (i18n (1874, "Work State:")), mq (mw->wstate));
        if (mw->wphone && *mw->wphone)
            rl_printf ("%s %s\n", al (i18n (1523, "Work Phone:")), mq (mw->wphone));
        if (mw->wfax && *mw->wfax)
            rl_printf ("%s %s\n", al (i18n (1521, "Work Fax:")), mq (mw->wfax));
        if (mw->waddress && *mw->waddress)
            rl_printf ("%s %s\n", al (i18n (1522, "Work Address:")), mq (mw->waddress));
        if (mw->wzip && *mw->wzip)
            rl_printf ("%s %s\n", al (i18n (1520, "Work Zip:")), mq (mw->wzip));
        if (mw->wcountry)
        {
            if ((tabd = TableGetCountry (mw->wcountry)))
                rl_printf ("%s %s%s%s\n", al (i18n (1514, "Work Country:")),
                           COLQUOTE, tabd, COLNONE);
            else
                rl_printf ("%s %s%d%s\n", al (i18n (1513, "Work Country Code:")),
                           COLQUOTE, mw->wcountry, COLNONE);
        }
        if (mw->wcompany && *mw->wcompany)
            rl_printf ("%s %s\n", al (i18n (1519, "Company Name:")), mq (mw->wcompany));
        if (mw->wdepart && *mw->wdepart)
            rl_printf ("%s %s\n", al (i18n (1518, "Department:")), mq (mw->wdepart));
        if (mw->wposition && *mw->wposition)
            rl_printf ("%s %s\n", al (i18n (1517, "Job Position:")), mq (mw->wposition));
        if (mw->woccupation)
            rl_printf ("%s %s%s%s\n", al (i18n (1516, "Occupation:")),
                      COLQUOTE, TableGetOccupation (mw->woccupation), COLNONE);
        if (mw->whomepage && *mw->whomepage)
            rl_printf ("%s %s\n", al (i18n (1515, "Work Homepage:")), mq (mw->whomepage));
    }
    if ((ml = cont->meta_interest))
    {
        rl_printf ("%s%s%s\n", COLSERVER, i18n (1875, "Personal interests:"), COLNONE);
        for ( ; ml; ml = ml->next)
        {
            if (!ml->text)
                continue;
            if ((tabd = TableGetInterest (ml->data)))
                rl_printf ("  %s: %s\n", tabd, mq (ml->text));
            else
                rl_printf ("  %d: %s\n", ml->data, mq (ml->text));
        }
    }
    if ((ml = cont->meta_background))
    {
        rl_printf ("%s%s%s\n", COLSERVER, i18n (1876, "Personal past background:"), COLNONE);
        for ( ; ml; ml = ml->next)
        {
            if (!ml->text)
                continue;
            if ((tabd = TableGetPast (ml->data)))
                rl_printf ("  %s: %s\n", tabd, mq (ml->text));
            else
                rl_printf ("  %d: %s\n", ml->data, mq (ml->text));
        }
    }
    if ((ml = cont->meta_affiliation))
    {
        rl_printf ("%s%s%s\n", COLSERVER, i18n (1879, "Affiliations:"), COLNONE);
        for ( ; ml; ml = ml->next)
        {
            if (!ml->text)
                continue;
            if ((tabd = TableGetAffiliation (ml->data)))
                rl_printf ("  %s: %s\n", tabd, mq (ml->text));
            else
                rl_printf ("  %d: %s\n", ml->data, mq (ml->text));
        }
    }
    if ((mo = cont->meta_obsolete))
    {
        if (mo->unknown)
            rl_printf ("%s %s%04x%s = %s%d%s\n", al (i18n (2195, "Obsolete number:")),
                       COLQUOTE, mo->unknown, COLNONE, COLQUOTE, mo->unknown, COLNONE);
        if (mo->description && *mo->description)
            rl_printf ("%s %s\n", al (i18n (2196, "Obsolete text:")),
                       mq (mo->description));
        if (mo->empty)
            rl_printf ("%s %s%d%s\n", al (i18n (2197, "Obsolete byte")),
                      COLQUOTE, mo->empty, COLNONE);
    }
    if (cont->meta_about && *cont->meta_about)
        rl_printf ("%s%s%s\n%s\n", COLSERVER, i18n (1525, "About:"), COLNONE,
                   s_msgquote (s_ind (cont->meta_about)));
}
