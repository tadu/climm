/* $Id$ */

/*********************************************
**********************************************
This file has the "ui" functions that read input
and send messages etc.

This software is provided AS IS to be used in
whatever way you see fit and is placed in the
public domain.

Author : Matthew Smith April 23, 1998
Contributors : Nicolas Sahlqvist April 27, 1998
               Michael Ivey May 4, 1998
               Ulf Hedlund -- Windows Support
               Michael Holzt May 5, 1998
                           aaron March 29, 2000
Changes :
22-6-98 Added the save and alter command and the
        new implementation of auto

**********************************************
**********************************************/

#include "micq.h"
#include "datatype.h"
#include "util.h"
#include "tabs.h"
#include "file_util.h"
#include "util_table.h"
#include "buildmark.h"
#include "sendmsg.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#ifndef __BEOS__
#include <arpa/inet.h>
#endif
#include <netdb.h>
#include "mreadline.h"
#endif
#include <fcntl.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "ui.h"
#include "util_ui.h"            /* aaron -- to get access to Get_Max_Screen_Width()
                                   function     */

#ifdef __BEOS__
#include "beos.h"
#endif

MORE_INFO_STRUCT user;
WP_STRUCT wp;

UDWORD last_uin = 0;

static UDWORD multi_uin;
static int status = 0;
static UDWORD uin;

static void Change_Function (SOK_T sok);
static void Help_Function (void);
static void Info_Function (SOK_T sok);
static void Trans_Function (SOK_T sok);
static void Auto_Function (SOK_T sok);
static void Alter_Function (void);
static void Message_Function (SOK_T sok);
static void Reply_Function (SOK_T sok);
static void Again_Function (SOK_T sok);
static void Verbose_Function (void);
static void Random_Function (SOK_T sok);
static void Random_Set_Function (SOK_T sok);
static void Show_Ignore_Status (void);
static void Show_Status (char *);

static void PrintContactWide ();
static BOOL Do_Multiline (SOK_T sok, char *buf);
static BOOL Do_Multiline_All (SOK_T sok, char *buf);
static void Info_Other_Update (SOK_T sok, char *buf);
static void Info_Update (SOK_T sok, char *buf);
static BOOL About_Info (SOK_T sok, char *buf);
static void User_Search (SOK_T sok, char *buf);

#if 0
char *strdup (const char *);
int strcasecmp (const char *, const char *);
#endif

/* aaron
   Displays the Contact List in a wide format, similar to the ls command.    */
void PrintContactWide ()
{
    int *Online;                /* definitely won't need more; could    */
    int *Offline;               /* probably get away with less.    */
    int MaxLen = 0;             /* legnth of longest contact name */
    int i;
    int OnIdx = 0;              /* for inserting and tells us how many there are */
    int OffIdx = 0;             /* for inserting and tells us how many there are */
    int NumCols;                /* number of columns to display on screen        */
    int Col;                    /* the current column during output.            */

    if ((Online = (int *) malloc (Num_Contacts * sizeof (int))) == NULL)
    {
        M_print (i18n (652, "Insuffificient memory to display a wide Contact List.\n"));
        return;
    }

    if ((Offline = (int *) malloc (Num_Contacts * sizeof (int))) == NULL)
    {
        M_print (i18n (652, "Insuffificient memory to display a wide Contact List.\n"));
        return;
    }

    /* We probably don't need to zero out the array, but just to be on the
       safe side...
       The arrays really should be only Num_Contacts in size... future
       improvement, I guess. Hopefully no one is running that tight on
       memory.                                                                */
    memset (Online, 0, sizeof (Online));
    memset (Offline, 0, sizeof (Offline));

    /* Filter the contact list into two lists -- online and offline. Also
       find the longest name in the list -- this is used to determine how
       many columns will fit on the screen.                                */
    for (i = 0; i < Num_Contacts; i++)
    {
        if ((SDWORD) Contacts[i].uin > 0)
        {                       /* Aliases */
            if (Contacts[i].status == STATUS_OFFLINE)
                Offline[OffIdx++] = i;
            else
                Online[OnIdx++] = i;

            if (strlen (Contacts[i].nick) > MaxLen)
                MaxLen = strlen (Contacts[i].nick);
        }
    }                           /* end for */

    /* This is probably a very ugly way to determine the number of columns
       to use... it's probably specific to my own contact list.            */
    NumCols = Get_Max_Screen_Width () / (MaxLen + 4);
    if (NumCols < 1)
        NumCols = 1;            /* sanity check. :)  */

    /* Fairly simple print routine. We check that we only print the right
       number of columns to the screen.                                    */
    M_print (COLMESS);
    for (i = 1; i < (Get_Max_Screen_Width () - strlen ("Offline")) / 2; i++)
    {
        M_print ("=");
    }
    M_print (COLCLIENT "%s" COLMESS, i18n (653, "Offline"));
    for (i = 1; i < (Get_Max_Screen_Width () - strlen ("Offline")) / 2; i++)
    {
        M_print ("=");
    }
    M_print (COLNONE "\n");
    for (Col = 1, i = 0; i < OffIdx; i++)
        if (Col % NumCols == 0)
        {
            M_print (COLCONTACT "  %-*s\n" COLNONE, MaxLen + 2, Contacts[Offline[i]].nick);
            Col = 1;
        }
        else
        {
            M_print (COLCONTACT "  %-*s" COLNONE, MaxLen + 2, Contacts[Offline[i]].nick);
            Col++;
        }                       /* end if */

    /* The user status for Online users is indicated by a one-character
       prefix to the nickname. Unfortunately not all statuses (statusae? :)
       are unique at one character. A better way to encode the information
       is needed.                                                            */
    if ((Col - 1) % NumCols != 0)
    {
        M_print ("\n");
    }
    M_print (COLMESS);
    for (i = 1; i < (Get_Max_Screen_Width () - strlen ("Online")) / 2; i++)
    {
        M_print ("=");
    }
    M_print (COLCLIENT "%s" COLMESS, i18n (654, "Online"));
    for (i = 1; i < (Get_Max_Screen_Width () - strlen ("Online")) / 2; i++)
    {
        M_print ("=");
    }
    M_print (COLNONE "\n");
    for (Col = 1, i = 0; i < OnIdx; i++)
    {
        const char *status;
        char weird = 'W';       /* for weird statuses that are reported as hex */

        status = Convert_Status_2_Str (Contacts[Online[i]].status);
        status = status ? status : &weird;
        if ((Contacts[Online[i]].status & 0xfff) == STATUS_ONLINE)
        {
            status = " ";
        }
        if (Col % NumCols == 0)
        {
            M_print (COLNONE "%c " COLCONTACT "%-*s\n" COLNONE,
                     /*        *Convert_Status_2_Str(Contacts[Online[i]].status), */
                     *status, MaxLen + 2, Contacts[Online[i]].nick);
            Col = 1;
        }
        else
        {
            M_print (COLNONE "%c " COLCONTACT "%-*s" COLNONE,
/*                    *Convert_Status_2_Str(Contacts[Online[i]].status), */
                     *status, MaxLen + 2, Contacts[Online[i]].nick);
            Col++;
        }                       /* end if */
    }
    if ((Col - 1) % NumCols != 0)
    {
        M_print ("\n");
    }
    M_print (COLMESS);
    for (i = 1; i < Get_Max_Screen_Width () - 1; i++)
    {
        M_print ("=");
    }
    M_print (COLNONE "\n");
    free (Online);
    free (Offline);
}                               /* end of aaron */

BOOL Do_Multiline (SOK_T sok, char *buf)
{
    static int offset = 0;
    static char msg[1024];

    msg[offset] = 0;
    if (strcmp (buf, END_MSG_STR) == 0)
    {
        icq_sendmsg (sok, multi_uin, msg, NORM_MESS);
        Time_Stamp ();
        M_print (" " COLSENT "%10s" COLNONE " " MSGSENTSTR "%s\n", UIN2Name (multi_uin),
                 MsgEllipsis (msg));
        last_uin = multi_uin;
        offset = 0;
        return FALSE;
    }
    else if (strcmp (buf, CANCEL_MSG_STR) == 0)
    {
        M_print (i18n (38, "Message canceled\n"));
        last_uin = multi_uin;
        offset = 0;
        return FALSE;
    }
    else
    {
        if (offset + strlen (buf) < 450)
        {
            strcat (msg, buf);
            strcat (msg, "\r\n");
            offset += strlen (buf) + 2;
            return TRUE;
        }
        else
        {
            M_print (i18n (37, "Message sent before last line buffer is full\n"));
            M_print (" " COLSENT "%10s" COLNONE " " MSGSENTSTR "%s\n", UIN2Name (multi_uin),
                     MsgEllipsis (msg));
            icq_sendmsg (sok, multi_uin, msg, NORM_MESS);
            last_uin = multi_uin;
            offset = 0;
            return FALSE;
        }
    }
}

/***************************************************************************
This function gathers information for the other info update packet
age sex d.o.b. etc.
****************************************************************************/
void Info_Other_Update (SOK_T sok, char *buf)
{
    static OTHER_INFO_STRUCT other;
    int temp;

    switch (status)
    {
        case NEW_AGE:
            other.age = atoi (buf);
            M_print ("%s ", i18n (536, "Enter new sex:"));
            status = NEW_SEX;
            break;
        case NEW_SEX:
            if (!strncasecmp (buf, i18n (528, "female"), 1))
            {
                other.sex = 1;
            }
            else if (!strncasecmp (buf, i18n (529, "male"), 1))
            {
                other.sex = 2;
            }
            else
            {
                other.sex = 0;
            }
            M_print ("%s ", i18n (537, "Enter new homepage:"));
            status = NEW_HP;
            break;
        case NEW_HP:
            other.hp = strdup (buf);
            M_print ("%s ", i18n (538, "Enter new year of birth (4 digits):"));
            status = NEW_YEAR;
            break;
        case NEW_YEAR:
            other.year = atoi (buf) - 1900;
            M_print ("%s ", i18n (539, "Enter new month of birth:"));
            status = NEW_MONTH;
            break;
        case NEW_MONTH:
            other.month = atoi (buf);
            M_print ("%s ", i18n (540, "Enter new day of birth:"));
            status = NEW_DAY;
            break;
        case NEW_DAY:
            other.day = atoi (buf);
            M_print ("%s ", i18n (534, "Enter a language by number or L for a list:"));
            status = NEW_LANG1;
            break;
        case NEW_LANG1:
            temp = atoi (buf);
            if ((0 == temp) && (toupper (buf[0]) == 'L'))
            {
                TablePrintLang ();
                status = NEW_LANG1;
            }
            else
            {
                other.lang1 = temp;
                status = NEW_LANG2;
            }
            M_print ("%s ", i18n (534, "Enter a language by number or L for a list:"));
            break;
        case NEW_LANG2:
            temp = atoi (buf);
            if ((0 == temp) && (toupper (buf[0]) == 'L'))
            {
                TablePrintLang ();
                status = NEW_LANG2;
            }
            else
            {
                other.lang2 = temp;
                status = NEW_LANG3;
            }
            M_print ("%s ", i18n (534, "Enter a language by number or L for a list:"));
            break;
        case NEW_LANG3:
            temp = atoi (buf);
            if ((0 == temp) && (toupper (buf[0]) == 'L'))
            {
                TablePrintLang ();
                status = NEW_LANG3;
                M_print ("%s ", i18n (534, "Enter a language by number or L for a list:"));
            }
            else
            {
                other.lang3 = temp;
                Update_Other (sok, &other);
                free (other.hp);
                status = 0;
            }
            break;
    }
}

/***************************************************************************
This function gathers information for the main info update packet
name email etc
****************************************************************************/
void Info_Update (SOK_T sok, char *buf)
{
    switch (status)
    {
        case NEW_NICK:
            user.nick = strdup ((char *) buf);
            M_print ("%s ", i18n (554, "Enter your new first name:"));
            status++;
            break;
        case NEW_FIRST:
            user.first = strdup ((char *) buf);
            M_print ("%s ", i18n (555, "Enter your new last name:"));
            status++;
            break;
        case NEW_LAST:
            user.last = strdup ((char *) buf);
            M_print ("%s ", i18n (556, "Enter your new email address:"));
            status++;
            break;
        case NEW_EMAIL:
            user.email = strdup ((char *) buf);
            M_print ("%s ", i18n (542, "Enter other email address:"));
            status++;
            break;
        case NEW_EMAIL2:
            user.email2 = strdup ((char *) buf);
            M_print ("%s ", i18n (543, "Enter old email address:"));
            status++;
            break;
        case NEW_EMAIL3:
            user.email3 = strdup ((char *) buf);
            M_print ("%s ", i18n (544, "Enter new city:"));
            status++;
            break;
        case NEW_CITY:
            user.city = strdup ((char *) buf);
            M_print ("%s ", i18n (545, "Enter new state:"));
            status++;
            break;
        case NEW_STATE:
            user.state = strdup ((char *) buf);
            M_print ("%s ", i18n (546, "Enter new phone number:"));
            status++;
            break;
        case NEW_PHONE:
            user.phone = strdup ((char *) buf);
            M_print ("%s ", i18n (547, "Enter new fax number:"));
            status++;
            break;
        case NEW_FAX:
            user.fax = strdup ((char *) buf);
            M_print ("%s ", i18n (548, "Enter new street address:"));
            status++;
            break;
        case NEW_STREET:
            user.street = strdup ((char *) buf);
            M_print ("%s ", i18n (549, "Enter new cellular number:"));
            status++;
            break;
        case NEW_CELLULAR:
            user.cellular = strdup ((char *) buf);
            M_print ("%s ", i18n (550, "Enter new zip code (must be numeric):"));
            status++;
            break;
        case NEW_ZIP:
            user.zip = atoi ((char *) buf);
            M_print ("%s ", i18n (551, "Enter your country's phone ID number:"));
            status = NEW_COUNTRY;
            break;
        case NEW_COUNTRY:
            user.country = atoi ((char *) buf);
            M_print ("%s ", i18n (552, "Enter your time zone (+/- 0-12):"));
            status = NEW_TIME_ZONE;
            break;
        case NEW_TIME_ZONE:
            user.c_status = atoi ((char *) buf);
            user.c_status <<= 1;
            M_print ("%s ", i18n (557, "Do you want to require Mirabilis users to request your authorization? (YES/NO)"));
            status = NEW_AUTH;
            break;
        case NEW_AUTH:
            if (!strcasecmp (buf, i18n (28, "NO")))
            {
                user.auth = FALSE;
                status = NEW_DONE;
                M_print ("%s ", i18n (622, "Do you want to apply these changes? (YES/NO)"));
            }
            else if (!strcasecmp (buf, i18n (27, "YES")))
            {
                user.auth = TRUE;
                status = NEW_DONE;
                M_print ("%s ", i18n (622, "Do you want to apply these changes? (YES/NO)"));
            }
            else
            {
                M_print ("%s\n", i18n (29, "Please enter YES or NO!"));
                M_print ("%s ", i18n (557, "Do you want to require Mirabilis users to request your authorization? (YES/NO)"));
            }
            break;
        case NEW_DONE:
            if (!strcasecmp (buf, i18n (28, "NO")))
            {
                free (user.nick);
                free (user.last);
                free (user.first);
                free (user.email);
                status = 0;
            }
            else if (!strcasecmp (buf, i18n (27, "YES")))
            {
                Update_More_User_Info (sok, &user);
                free (user.nick);
                free (user.last);
                free (user.first);
                free (user.email);
                status = 0;
            }
            else
            {
                M_print ("%s\n", i18n (29, "Please enter YES or NO!"));
                M_print ("%s ", i18n (622, "Do you want to apply these changes? (YES/NO)"));
            }
            break;
    }
}

/*
 * This function gathers information for the whitepage search packet
 * name email etc
 */
void Wp_Search (SOK_T sok, char *buf)
{
    int temp;

    switch (status)
    {
        case WP_NICK:
            wp.nick = strdup ((char *) buf);
            M_print ("%s ", i18n (657, "Enter the user's first name:"));
            status = WP_FIRST;
            break;
        case WP_FIRST:
            wp.first = strdup ((char *) buf);
            M_print ("%s ", i18n (658, "Enter the user's last name:"));
            status = WP_LAST;
            break;
        case WP_LAST:
            wp.last = strdup ((char *) buf);
            M_print ("%s ", i18n (655, "Enter the user's e-mail address:"));
            status = WP_EMAIL;
            break;
        case WP_EMAIL:
            wp.email = strdup ((char *) buf);
            M_print ("%s ", i18n (558, "Enter min age (18-22,23-29,30-39,40-49,50-59,60-120):"));
            status = WP_MIN_AGE;
            break;
        case WP_MIN_AGE:
            wp.minage = atoi (buf);
            M_print ("%s ", i18n (559, "Enter max age (22,29,39,49,59,120):"));
            status = WP_MAX_AGE;
            break;
        case WP_MAX_AGE:
            wp.maxage = atoi (buf);
            M_print (i18n (663, "Enter sex:"));
            status=WP_SEX;
            break;
        case WP_SEX:
            if (!strncasecmp (buf, i18n (528, "female"), 1))
            {
                wp.sex = 1;
            }
            else if (!strncasecmp (buf, i18n (529, "male"), 1))
            {
                wp.sex = 2;
            }
            else
            {
                wp.sex = 0;
            }
            M_print ("%s ", i18n (534, "Enter a language by number or L for a list:"));
            status = WP_LANG1;
            break;
        case WP_LANG1:
            temp = atoi (buf);
            if ((0 == temp) && (toupper (buf[0]) == 'L'))
            {
                TablePrintLang ();
                status = WP_LANG1;
            M_print ("%s ", i18n (534, "Enter a language by number or L for a list:"));
            }
            else
            {
                wp.language = temp;
                status = WP_CITY;
                M_print ("%s ", i18n (560, "Enter a city:"));
            }
            break;
        case WP_CITY:
            wp.city = strdup ((char *) buf);
            M_print ("%s ", i18n (561, "Enter a state:"));
            status = WP_STATE;
            break;
        case WP_STATE:
            wp.state = strdup ((char *) buf);
            M_print ("%s ", i18n (578, "Enter country's phone ID number:"));
            status = WP_COUNTRY;
            break;
        case WP_COUNTRY:
            wp.country = atoi ((char *) buf);
            M_print ("%s ", i18n (579, "Enter company: "));
            status = WP_COMPANY;
            break;
        case WP_COMPANY:
            wp.company = strdup ((char *) buf);
            M_print ("%s ", i18n (587, "Enter department: "));
            status = WP_DEPARTMENT;
            break;
        case WP_DEPARTMENT:
            wp.department = strdup ((char *) buf);
            M_print ("%s ", i18n (588, "Enter position: "));
            status = WP_POSITION;
            break;
        case WP_POSITION:
            wp.position = strdup ((char *) buf);
            M_print ("%s ", i18n (589, "Should the users be online?"));
            status = WP_STATUS;
            break;
/* A few more could be added here, but we're gonna make this
 the last one -KK */
        case WP_STATUS:
            if (!strcasecmp (buf, i18n (28, "NO")))
            {
                wp.online = FALSE;
                Search_WP (sok, &wp);
                free (wp.nick);
                free (wp.last);
                free (wp.first);
                free (wp.email);
                free (wp.company);
                free (wp.department);
                free (wp.position);
                free (wp.city);
                free (wp.state);
                status = 0;
            }
            else if (!strcasecmp (buf, i18n (27, "YES")))
            {
                wp.online = TRUE;
                Search_WP (sok, &wp);
                free (wp.nick);
                free (wp.last);
                free (wp.first);
                free (wp.email);
                free (wp.company);
                free (wp.department);
                free (wp.position);
                free (wp.city);
                free (wp.state);
                status = 0;
            }
            else
            {
                M_print ("%s\n", i18n (29, "Please enter YES or NO!"));
                M_print ("%s ", i18n (589, "Should the users be online?"));
            }
            break;
    }
}

/**************************************************************************
Multi line about info updates
**************************************************************************/
BOOL About_Info (SOK_T sok, char *buf)
{
    static int offset = 0;
    static char msg[1024];

    msg[offset] = 0;
    if (strcmp (buf, END_MSG_STR) == 0)
    {
        Update_About (sok, msg);
        offset = 0;
        return FALSE;
    }
    else if (strcmp (buf, CANCEL_MSG_STR) == 0)
    {
        offset = 0;
        return FALSE;
    }
    else
    {
        if (offset + strlen (buf) < 450)
        {
            strcat (msg, buf);
            strcat (msg, "\r\n");
            offset += strlen (buf) + 2;
            return TRUE;
        }
        else
        {
            Update_About (sok, msg);
            offset = 0;
            return FALSE;
        }
    }
}

/**************************************************************************
Gets the info to search for a user by
***************************************************************************/
void User_Search (SOK_T sok, char *buf)
{
    switch (status)
    {
        case SRCH_START:
            M_print ("%s ", i18n (655, "Enter the user's e-mail address:"));
            status++;
            break;
        case SRCH_EMAIL:
            user.email = strdup ((char *) buf);
            M_print ("%s ", i18n (656, "Enter the user's nick name:"));
            status++;
            break;
        case SRCH_NICK:
            user.nick = strdup ((char *) buf);
            M_print ("%s ", i18n (657, "Enter the user's first name:"));
            status++;
            break;
        case SRCH_FIRST:
            user.first = strdup ((char *) buf);
            M_print ("%s", i18n (658, "Enter the user's last name:"));
            status++;
            break;
        case SRCH_LAST:
            user.last = strdup ((char *) buf);
            start_search (sok, user.email, user.nick, user.first, user.last);
            status = 0;
            break;
    }
}

/**************************************************************************
Messages everyone in the conact list
***************************************************************************/
BOOL Do_Multiline_All (SOK_T sok, char *buf)
{
    static int offset = 0;
    static char msg[1024];
    char *temp;
    int i;

    msg[offset] = 0;
    if (strcmp (buf, END_MSG_STR) == 0)
    {
        for (i = 0; i < Num_Contacts; i++)
        {
            temp = strdup (msg);
            icq_sendmsg (sok, Contacts[i].uin, temp, MRNORM_MESS);
            free (temp);
        }
        M_print (i18n (659, "Message sent!\n"));
        offset = 0;
        return FALSE;
    }
    else if (!strcmp (buf, CANCEL_MSG_STR))
    {
        M_print (i18n (38, "Message canceled\n"));
        offset = 0;
        return FALSE;
    }
    else
    {
        if (offset + strlen (buf) < 450)
        {
            strcat (msg, buf);
            strcat (msg, "\r\n");
            offset += strlen (buf) + 2;
            return TRUE;
        }
        else
        {
            M_print (i18n (37, "Message sent before last line buffer is full\n"));
            for (i = 0; i < Num_Contacts; i++)
            {
                temp = strdup (msg);
                icq_sendmsg (sok, Contacts[i].uin, temp, MRNORM_MESS);
                free (temp);
            }
            offset = 0;
            return FALSE;
        }
    }
}

/******************************************************
Read a line of input and processes it.

This is huge and ugly it should be fixed.
*******************************************************/
void Get_Input (SOK_T sok, int *idle_val, int *idle_flag)
{
    unsigned char buf[1024];    /* This is hopefully enough */
    char *cmd;
    char *arg1;
    char *arg2;
    int i;

/* GRYN - START */
    int idle_save;
    idle_save = *idle_val;
    *idle_val = time (NULL);
/* GRYN - STOP */

    memset (buf, 0, 1024);
#ifdef __BEOS__
    Be_GetText (buf);
#else
    R_getline (buf, 1024);
#endif
    M_print ("\r");             /* reset char printed count for dumb terminals */
    buf[1023] = 0;              /* be safe */
    if (status == 1)
    {
        if (!Do_Multiline (sok, buf))
            status = 2;
        else
            R_doprompt (i18n (41, "msg> "));
    }
    else if (status == 3)
    {
        if (!Do_Multiline_All (sok, buf))
            status = 2;
        else
            R_doprompt (i18n (42, "msg all> "));
    }
    else if (status == MULTI_ABOUT)
    {
        if (!About_Info (sok, buf))
            status = 2;
        else
            R_doprompt (i18n (541, "About> "));
    }
    else if ((status >= NEW_NICK) && (status <= NEW_DONE))
    {
        Info_Update (sok, buf);
    }
    else if ((status >= NEW_AGE) && (status <= NEW_LANG3))
    {
        Info_Other_Update (sok, buf);
    }
    else if ((status >= SRCH_START) && (status <= SRCH_LAST))
    {
        User_Search (sok, buf);
    }
    else if ((status >= WP_START) && (status <= WP_END))
    {
        Wp_Search (sok, buf);
    }
    else
    {
        if (buf[0] != 0)
        {
            if ('!' == buf[0])
            {
                R_pause ();
#ifdef SHELL_COMMANDS
                if ((buf[1] < 31) || (buf[1] > 127))
                    M_print (i18n (660, "Invalid Command: %s\n"), &buf[1]);
                else
                    system (&buf[1]);
#else
                M_print (i18n (661, "Shell commands have been disabled.\n"));
#endif
                R_resume ();
                Prompt ();
                return;
            }
            cmd = strtok (buf, " \n\t");

            if ( cmd == NULL)
            {
                Prompt ();
                return;
            }

            /* skip all non-alhphanumeric chars on the beginning
             * to accept IRC like commands starting with a /
             * or talker like commands starting with a .
             * or whatever */
            if (!isalnum (cmd[0]))
                cmd++;

            if ( *cmd == '\0' )
            {
                Prompt ();
                return;
            }

            /* goto's removed and code fixed by Paul Laufer. Enjoy! */
            if ((strcasecmp (cmd, "quit") == 0) || (strcasecmp (cmd, "/quit") == 0))
            {
                Quit = TRUE;
            }
            else if (strcasecmp (cmd, quit_cmd) == 0)
            {
                Quit = TRUE;
            }
            else if (strcasecmp (cmd, sound_cmd) == 0)
            {
                /* GRYN */ *idle_val = idle_save;
                if ((arg1 = strtok (NULL, "")))
                {
                    *Sound_Str = 0;
                    if (!strcasecmp(arg1, "on"))
                       Sound = SOUND_ON;
                    else if (!strcasecmp(arg1, "off"))
                       Sound = SOUND_OFF;
                    else /* treat it as a command */
                       strcpy(Sound_Str, arg1);
                }
                if (*Sound_Str)
                    M_print ("%s " COLSERV "%s" COLNONE ".\n", i18n (83, "Sound cmd"), Sound_Str);
                else if ( SOUND_ON == Sound )
                    M_print ("%s " COLSERV "%s" COLNONE ".\n", i18n (84, "Sound"), i18n (85, "on"));
                else if (SOUND_OFF == Sound)
                    M_print ("%s " COLSERV "%s" COLNONE ".\n", i18n (84, "Sound"), i18n (86, "off"));
            }
            else if (strcasecmp (cmd, change_cmd) == 0)
            {
                Change_Function (sok);
            }
            else if (strcasecmp (cmd, rand_cmd) == 0)
            {
                Random_Function (sok);
            }
            else if (strcasecmp (cmd, "set") == 0)
            {
                Random_Set_Function (sok);
            }
            else if (!strcasecmp (cmd, color_cmd))
            {
                /* GRYN */ *idle_val = idle_save;
                Color = !Color;
                if (Color)
                {
                    M_print (i18n (662, "Color is " COLMESS "%s" COLNONE ".\n"), i18n (85, "on"));
                }
                else
                {
                    M_print (i18n (662, "Color is " COLMESS "%s" COLNONE ".\n"), i18n (86, "off"));
                }
            }
            else if (!strcasecmp (cmd, online_cmd))     /* online command */
            {                   /* GRYN */
                *idle_flag = 0;
                CHANGE_STATUS (STATUS_ONLINE);
            }
            else if (!strcasecmp (cmd, away_cmd))       /* away command */
            {                   /* GRYN */
                *idle_flag = 0;
                CHANGE_STATUS (STATUS_AWAY);
            }
            else if (!strcasecmp (cmd, na_cmd)) /* Not Available command */
            {                   /* GRYN */
                *idle_flag = 0;
                CHANGE_STATUS (STATUS_NA);
            }
            else if (!strcasecmp (cmd, occ_cmd))        /* Occupied command */
            {                   /* GRYN */
                *idle_flag = 0;
                CHANGE_STATUS (STATUS_OCCUPIED);
            }
            else if (!strcasecmp (cmd, dnd_cmd))        /* Do not Disturb command */
            {                   /* GRYN */
                *idle_flag = 0;
                CHANGE_STATUS (STATUS_DND);
            }
            else if (!strcasecmp (cmd, ffc_cmd))        /* Free For Chat command */
            {                   /* GRYN */
                *idle_flag = 0;
                CHANGE_STATUS (STATUS_FREE_CHAT);
            }
            else if (!strcasecmp (cmd, inv_cmd))        /* Invisible command */
            {                   /* GRYN */
                *idle_flag = 0;
                CHANGE_STATUS (STATUS_INVISIBLE);
            }
            else if (!strcasecmp (cmd, search_cmd))
            {
                arg1 = strtok (NULL, "\n");
                if (arg1 == NULL)
                {
                    status = SRCH_START;
                    User_Search (sok, buf);
                }
                else
                {
                    start_search (sok, arg1, "", "", "");
                }
            }
            else if (!strcasecmp (cmd, status_cmd))
            {                   /* GRYN */
                *idle_val = idle_save;
                arg1 = strtok (NULL, "\n");
                Show_Status (arg1);
            }
            else if (!strcasecmp (cmd, list_cmd))
            {                   /* GRYN */
                *idle_val = idle_save;
                Show_Quick_Status ();
            }
            else if (!strcasecmp (cmd, online_list_cmd))
            {                   /* GRYN */
                *idle_val = idle_save;
                Show_Quick_Online_Status ();
            }
            else if (!strcasecmp (cmd, msga_cmd))
            {
                status = 3;
                /* aaron
                   Tell me that I'm sending a message to everyone                      */
                M_print (i18n (664, "Composing message to " COLSERV "all" COLNONE ":\n"));
                /* end of aaron */
                R_doprompt (i18n (42, "msg all> "));
            }
            else if (!strcasecmp (cmd, "other"))
            {
                status = NEW_AGE;
                M_print ("%s ", i18n (535, "Enter new age:"));
            }
            else if (!strcasecmp (cmd, reply_cmd))
            {
                Reply_Function (sok);
            }
            else if (!strcasecmp (cmd, "reg"))
            {
                arg1 = strtok (NULL, "");
                if (arg1 != NULL)
                    reg_new_user (sok, arg1);
            }
            else if (!strcasecmp (cmd, "trans"))
            {
                Trans_Function (sok);
            }
            else if (!strcasecmp (cmd, "pass"))
            {
                arg1 = strtok (NULL, "");
                if (arg1 != NULL)
                    Change_Password (sok, arg1);
            }
            else if (!strcasecmp (cmd, about_cmd))
            {
                arg1 = strtok (NULL, "");
                if (NULL != arg1)
                {
                    Update_About (sok, arg1);
                }
                else
                {
                    status = MULTI_ABOUT;
                    R_doprompt (i18n (541, "About> "));
                }
            }
            else if (!strcasecmp (cmd, again_cmd))      /* again command */
            {
                Again_Function (sok);
            }
            else if (!strcasecmp (cmd, clear_cmd))
            {                   /* GRYN */
                *idle_val = idle_save;
                clrscr ();
            }
            else if (!strcasecmp (cmd, info_cmd))
            {
                Info_Function (sok);
            }
            else if (!strcasecmp (cmd, togig_cmd))
            {
                arg1 = strtok (NULL, "\n");
                if (arg1)
                {
                    uin = nick2uin (arg1);
                    if (uin == -1)
                    {
                        M_print (i18n (665, "%s not recognized as a nick name."), arg1);
                    }
                    else
                    {
                        CONTACT_PTR bud =  UIN2Contact (uin);
                        if (!bud)
                        {
                            M_print (i18n (90, "%s is a UIN, not a nick name."), arg1);
                        }
                        else
                        {
                            if (bud->invis_list == TRUE)
                            {
                                bud->invis_list = FALSE;
                                update_list (sok, uin, INV_LIST_UPDATE, FALSE);
                                M_print (i18n (666, "Unignored %s."), UIN2nick (uin));
                            }
                            else
                            {
                                bud->vis_list = FALSE;
                                bud->invis_list = TRUE;
                                update_list (sok, uin, INV_LIST_UPDATE, TRUE);
                                M_print (i18n (667, "Ignoring %s."), UIN2nick (uin));
                            }
                            snd_contact_list (sok);
                            snd_invis_list (sok);
                            snd_vis_list (sok);
                            CHANGE_STATUS (Current_Status);
                        }
                    }
                }
                else
                {
                    M_print (i18n (668, "You must specify a nick name."));
                }
                M_print ("\n");
            }
            else if (!strcasecmp (cmd, iglist_cmd))
            {
                Show_Ignore_Status ();
            }
            else if (!strcasecmp (cmd, "ver"))
            {
                M_print (BuildVersion ());
            }
            else if (!strcasecmp (cmd, add_cmd))
            {
                arg1 = strtok (NULL, " \t");
                if (arg1 != NULL)
                {
                    uin = atoi (arg1);
                    arg1 = strtok (NULL, "");
                    if (Add_User (sok, uin, arg1))
                        M_print (i18n (669, "%s added."), arg1);
                }
                else
                {
                    M_print (i18n (668, "You must specify a nick name."));
                }
                M_print ("\n");
            }
            else if (!strcasecmp (cmd, togvis_cmd))
            {
                arg1 = strtok (NULL, " \t");
                if (arg1)
                {
                    uin = nick2uin (arg1);
                    if (uin == -1)
                    {
                        M_print (i18n (665, "%s not recognized as a nick name."), arg1);
                    }
                    else
                    {
                        CONTACT_PTR bud = UIN2Contact (uin);
                        if (!bud)
                        {
                            M_print (i18n (90, "%s is a UIN, not a nick name."), arg1);
                        }
                        else
                        {
                            if (bud->vis_list == TRUE)
                            {
                                bud->vis_list = FALSE;
                                update_list (sok, uin, VIS_LIST_UPDATE, FALSE);
                                M_print (i18n (670, "Invisible to %s now."), UIN2nick (uin));
                            }
                            else
                            {
                                bud->vis_list = TRUE;
                                update_list (sok, uin, VIS_LIST_UPDATE, TRUE);
                                M_print (i18n (671, "Visible to %s now."), UIN2nick (uin));
                            }
                             /*FIXME*/          /* 
                            snd_contact_list( sok );
                            snd_invis_list( sok );
                            snd_vis_list( sok ); */
                        }
                    }
                }
                else
                {
                    M_print (i18n (668, "You must specify a nick name."));
                }
                M_print ("\n");
            }
            else if (strcasecmp (cmd, "verbose") == 0)
            {
                Verbose_Function ();
            }
            else if (strcasecmp (cmd, "rinfo") == 0)
            {
                Print_UIN_Name (last_recv_uin);
                M_print (i18n (672, "'s IP address is "));
                Print_IP (last_recv_uin);
                if ((UWORD) Get_Port (last_recv_uin) != (UWORD) 0xffff)
                    M_print (i18n (673, "\tThe port is %d\n"), (UWORD) Get_Port (last_recv_uin));
                else
                    M_print (i18n (674, "\tThe port is unknown\n"));
/*              M_print ("\n" );*/
                send_info_req (sok, last_recv_uin);
/*              send_ext_info_req( sok, last_recv_uin );*/
            }
            else if ((strcasecmp (cmd, "/help") == 0) ||        /* Help command */
                     (strcasecmp (cmd, "help") == 0))
            {
                Help_Function ();
            }
            else if (strcasecmp (cmd, auth_cmd) == 0)
            {
                arg1 = strtok (NULL, "");
                if (arg1 == NULL)
                {
                    M_print (i18n (675, "Need uin to send to"));
                }
                else
                {
                    uin = nick2uin (arg1);
                    if (-1 == uin)
                    {
                        M_print (i18n (665, "%s not recognized as a nick name."), arg1);
                    }
                    else
                        icq_sendauthmsg (sok, uin);
                }
            }
            else if (strcasecmp (cmd, message_cmd) == 0)        /* "/msg" */
            {
                Message_Function (sok);
            }
            else if (strcasecmp (cmd, url_cmd) == 0)    /* "/msg" */
            {
                arg1 = strtok (NULL, " ");
                if (arg1 == NULL)
                {
                    M_print (i18n (676, "Need uin to send to\n"));
                }
                else
                {
                    uin = nick2uin (arg1);
                    if (uin == -1)
                    {
                        M_print (i18n (677, "%s not recognized as a nick name\n"), arg1);
                    }
                    else
                    {
                        arg1 = strtok (NULL, " ");
                        last_uin = uin;
                        if (arg1 != NULL)
                        {
                            arg2 = strtok (NULL, "");
                            if (!arg2)
                                arg2 = "";
                            icq_sendurl (sok, uin, arg1, arg2);
                            Time_Stamp ();
                            M_print (" ");
                            Print_UIN_Name_10 (last_uin);
                            M_print (" " MSGSENTSTR "%s\n", MsgEllipsis (arg1));
                        }
                        else
                        {
                            M_print (i18n (678, "Need URL please.\n"));
                        }
                    }
                }
            }
            else if (!strcasecmp (cmd, alter_cmd))      /* alter command */
            {
                Alter_Function ();
            }
            else if (!strcasecmp (cmd, save_cmd))       /* save command */
            {
                i = Save_RC ();
                if (i == -1)
                    M_print (i18n (679, "Sorry saving your personal reply messages went wrong!\n"));
                else
                    M_print (i18n (680, "Your personal settings have been saved!\n"));
            }
            else if (!strcasecmp (cmd, "update"))
            {
                status = NEW_NICK;
                M_print ("%s ", i18n (553, "Enter Your New Nickname:"));
            }
            else if (!strcasecmp (cmd, "wpsearch"))
            {
                status = WP_START;
                M_print ("%s ", i18n (656, "Enter the user's nick name:"));
            }
            else if (strcasecmp (cmd, auto_cmd) == 0)
            {
                Auto_Function (sok);
            }
            else if (strcasecmp (cmd, "tabs") == 0)
            {
                for (i = 0, TabReset (); TabHasNext (); i++)
                    TabGetNext ();
                M_print (i18n (681, "Last %d people you talked to:\n"), i);
                for (TabReset (); TabHasNext ();)
                {
                    UDWORD uin = TabGetNext ();
                    CONTACT_PTR cont;
                    M_print ("    ");
                    Print_UIN_Name (uin);
                    cont = UIN2Contact (uin);
                    if (cont)
                    {
                        M_print (COLMESS " (now ");
                        Print_Status (cont->status);
                        M_print (")" COLNONE);
                    }
                    M_print ("\n");
                }
            }
            /* aaron
               Displays the last message received from the given nickname. The
               <nickname> must be a member of the Contact List because that's the
               only place we store the last message.

               Removed space (" ") from strtok so that nicknames with spaces
               can be interpreted properly. I'm not sure if this is the correct
               way to do it, but it seems to work.                                          */
            else if (strcasecmp (cmd, "last") == 0)
            {
                arg1 = strtok (NULL, "\t\n");   /* Is this right? */
                if (arg1 == NULL)
                {
                    int i;
                    M_print (i18n (682, "You have received messages from:\n"));
                    for (i = 0; i < Num_Contacts; i++)
                        if (Contacts[i].LastMessage != NULL)
                            M_print (COLCONTACT "  %s" COLNONE "\n", Contacts[i].nick);
                }
                else
                {
                    if ((uin = nick2uin (arg1)) == -1)
                        M_print (i18n (683, "Unknown Contact: %s\n"), arg1);
                    else
                    {
                        if (UIN2Contact (uin) == NULL)
                            M_print (i18n (684, "%s is not a known Contact\n"), arg1);
                        else
                        {
                            if (UIN2Contact (uin)->LastMessage != NULL)
                            {
                                M_print (i18n (685, "Last message from " COLCONTACT "%s" COLNONE ":\n"),
                                         UIN2Contact (uin)->nick);
                                M_print (COLMESS "%s" COLNONE "\n", UIN2Contact (uin)->LastMessage);
                            }
                            else
                            {
                                M_print (i18n (686, "No messages received from " COLCONTACT "%s" COLNONE "\n"),
                                         UIN2Contact (uin)->nick);
                            }
                        }
                    }
                }
            }
            else if (strcasecmp (cmd, "uptime") == 0)
            {
                double TimeDiff = difftime (time (NULL), MicqStartTime);
                int Days, Hours, Minutes, Seconds;

                Seconds = (int) TimeDiff % 60;
                TimeDiff = TimeDiff / 60.0;
                Minutes = (int) TimeDiff % 60;
                TimeDiff = TimeDiff / 60.0;
                Hours = (int) TimeDiff % 24;
                TimeDiff = TimeDiff / 24.0;
                Days = TimeDiff;

                M_print ("%s ", i18n (687, "Micq has been running for"));
                if (Days != 0)
                    M_print (COLMESS "%02d" COLNONE "%s, ", Days, i18n (688, "days"));
                if (Hours != 0)
                    M_print (COLMESS "%02d" COLNONE "%s, ", Hours, i18n (689, "hours"));
                if (Minutes != 0)
                    M_print (COLMESS "%02d" COLNONE "%s, ", Minutes, i18n (690, "minutes"));
                M_print (COLMESS "%02d" COLNONE "%s.\n", Seconds, i18n (691, "seconds"));
                M_print ("%s " COLMESS "%d" COLNONE " / %d\n", i18n (692, "Contacts:"), Num_Contacts, MAX_CONTACTS);
                M_print ("%s " COLMESS "%d" COLNONE "\t", i18n (693, "Packets sent:"), Packets_Sent);
                M_print ("%s " COLMESS "%d" COLNONE "\t", i18n (694, "Packets recieved:"), Packets_Recv);
                if (Packets_Sent || Packets_Recv)
                {
                    M_print ("%s " COLMESS "%2.2f" COLNONE "%%\n", i18n (695, "Lag:"),
                             abs (Packets_Sent - Packets_Recv) * (200.0 / (Packets_Sent + Packets_Recv)));
                }
                M_print ("%s " COLMESS "%d" COLNONE "\t", i18n (697, "Distinct packets sent:"), real_packs_sent);
                M_print ("%s " COLMESS "%d" COLNONE "\n", i18n (698, "Distinct packets recieved:"), real_packs_recv);
            }
            else if (strcasecmp (cmd, "wide") == 0)
            {
                PrintContactWide ();
            }                   /* end of aaron's code */
            else
            {
                M_print (i18n (36, "Unknown command %s, type /help for help."), cmd);
                M_print ("\n");
            }
        }
    }
    multi_uin = last_uin;
    if ((status == 0) || (status == 2))
    {
        if (!Quit)
#ifndef USE_MREADLINE
            Prompt ();
#else
            Prompt ();
#endif
    }
}

/**************************************************************
most detailed contact list display
***************************************************************/
static void Show_Status (char *name)
{
    int i;
    UDWORD num;
    CONTACT_PTR cont;

    if (name != NULL)
    {
        num = nick2uin (name);
        if (num == -1)
        {
            M_print (i18n (699, "Must give a valid uin/nickname\n"));
            return;
        }
        cont = UIN2Contact (num);
        if (cont == NULL)
        {
            M_print (i18n (700, "%s is not a valid user in your list.\n"), name);
            return;
        }
        if (cont->vis_list)
        {
            M_print ("%s*%s", COLSERV, COLNONE);
        }
        else if (cont->invis_list)
        {
            M_print (COLSERV "-" COLNONE);
        }
        else
        {
            M_print (" ");
        }
        M_print ("%6ld=", cont->uin);
        M_print (COLCONTACT "%-20s\t%s(", cont->nick, COLMESS);
        Print_Status (cont->status);
        M_print (")%s\n", COLNONE);
        if (cont->status == STATUS_OFFLINE)
        {
            if (-1L != cont->last_time)
            {
                M_print (i18n (69, " Last online at %s"), ctime ((time_t *) & cont->last_time));
            }
            else
            {
                M_print (i18n (70, " Last on-line unknown."));
                M_print ("\n");
            }
        }
        else
        {
            if (-1L != cont->last_time)
            {
                M_print (i18n (68, " Online since %s"), ctime ((time_t *) & cont->last_time));
            }
            else
            {
                M_print (i18n (70, " Last on-line unknown."));
                M_print ("\n");
            }
        }
        M_print ("%-15s %d.%d.%d.%d : %d\n", i18n (441, "IP:"), cont->current_ip[0], cont->current_ip[1],
                 cont->current_ip[2], cont->current_ip[3], cont->port);
        M_print ("%-15s %d\n", i18n (453, "TCP version:"), cont->TCP_version);
        M_print ("%-15s %s\n", i18n (454, "Connection:"),
                 cont->connection_type == 4 ? i18n (493, "Peer-to-Peer") : i18n (494, "Server Only"));
        return;
    }
    M_print (W_SEPERATOR);
    Time_Stamp ();
    M_print (" ");
    M_print (i18n (71, "Your status is "));
    Print_Status (Current_Status);
    M_print ("\n");
    /*  First loop sorts thru all offline users */
    M_print ("%s%s\n", W_SEPERATOR, i18n (72, "Users offline:"));
    for (i = 0; i < Num_Contacts; i++)
    {
        if ((SDWORD) Contacts[i].uin > 0)
        {
            if (FALSE == Contacts[i].invis_list)
            {
                if (Contacts[i].status == STATUS_OFFLINE)
                {
                    if (Contacts[i].vis_list)
                    {
                        M_print ("%s*%s", COLSERV, COLNONE);
                    }
                    else
                    {
                        M_print (" ");
                    }
                    M_print ("%8ld=", Contacts[i].uin);
                    M_print (COLCONTACT "%-20s\t%s(", Contacts[i].nick, COLMESS);
                    Print_Status (Contacts[i].status);
                    M_print (")" COLNONE);
                    if (-1L != Contacts[i].last_time)
                    {
                        M_print (i18n (69, " Last online at %s"),
                                 ctime ((time_t *) & Contacts[i].last_time));
                    }
                    else
                    {
                        M_print (i18n (70, " Last on-line unknown."));
                        M_print ("\n");
                        /* if time is unknow they can't be logged on cause we */
                        /* set the time at login */
                    }
                }
            }
        }
    }
    /* The second loop displays all the online users */
    M_print ("%s%s\n", W_SEPERATOR, i18n (73, "Users online:"));
    for (i = 0; i < Num_Contacts; i++)
    {
        if ((SDWORD) Contacts[i].uin > 0)
        {
            if (FALSE == Contacts[i].invis_list)
            {
                if (Contacts[i].status != STATUS_OFFLINE)
                {
                    if (Contacts[i].vis_list)
                    {
                        M_print ("%s*%s", COLSERV, COLNONE);
                    }
                    else
                    {
                        M_print (" ");
                    }
                    M_print ("%8ld=", Contacts[i].uin);
                    M_print (COLCONTACT "%-20s\t%s(", Contacts[i].nick, COLMESS);
                    Print_Status (Contacts[i].status);
                    M_print (")" COLNONE);
                    if (-1L != Contacts[i].last_time)
                    {
                        if (Contacts[i].status == STATUS_OFFLINE)
                            M_print (i18n (69, " Last online at %s"),
                                     ctime ((time_t *) & Contacts[i].last_time));
                        else
                            M_print (i18n (68, " Online since %s"),
                                     ctime ((time_t *) & Contacts[i].last_time));
                    }
                    else
                    {
                        M_print (i18n (70, " Last on-line unknown."));
                        M_print ("\n");
                        /* if time is unknow they can't be logged on cause we */
                        /* set the time at login */
                    }
                }
            }
        }
    }
    M_print (W_SEPERATOR);
}

/***************************************************************
nice clean "w" display
****************************************************************/
void Show_Quick_Status (void)
{
    int i;

    M_print (W_SEPERATOR);
    Time_Stamp ();
    M_print (" " MAGENTA BOLD "%10lu" COLNONE " ", UIN);
    M_print (i18n (71, "Your status is "));
    Print_Status (Current_Status);
    M_print ("\n");
    M_print ("%s%s\n", W_SEPERATOR, i18n (72, "Users offline:"));
    /*  First loop sorts thru all offline users */
    /*  This comes first so that if there are many contacts */
    /*  The online ones will be less likely to scroll off the screen */
    for (i = 0; i < Num_Contacts; i++)
    {
        if ((SDWORD) Contacts[i].uin > 0)
        {
            if (FALSE == Contacts[i].invis_list)
            {
                if (Contacts[i].status == STATUS_OFFLINE)
                {
                    if (Contacts[i].vis_list)
                    {
                        M_print (COLSERV "*" COLNONE);
                    }
                    else
                    {
                        M_print (" ");
                    }
                    M_print (COLCONTACT "%-20s\t" COLMESS "(", Contacts[i].nick);
                    Print_Status (Contacts[i].status);
                    M_print (")" COLNONE "\n");
                }
            }
        }
    }
    /* The second loop displays all the online users */
    M_print ("%s%s\n", W_SEPERATOR, i18n (73, "Users online:"));
    for (i = 0; i < Num_Contacts; i++)
    {
        if ((SDWORD) Contacts[i].uin > 0)
        {
            if (FALSE == Contacts[i].invis_list)
            {
                if (Contacts[i].status != STATUS_OFFLINE)
                {
                    if (Contacts[i].vis_list)
                    {
                        M_print (COLSERV "*" COLNONE);
                    }
                    else
                    {
                        M_print (" ");
                    }
                    M_print (COLCONTACT "%-20s\t" COLMESS "(", Contacts[i].nick);
                    Print_Status (Contacts[i].status);
                    M_print (")" COLNONE "\n");
                }
            }
        }
    }
    M_print (W_SEPERATOR);
}

/***************************************************************
nice clean "i" display
****************************************************************/
static void Show_Ignore_Status (void)
{
    int i;

    M_print ("%s%s\n", W_SEPERATOR, i18n (62, "Users ignored:"));
    /*  Sorts thru all ignored users */
    for (i = 0; i < Num_Contacts; i++)
    {
        if ((SDWORD) Contacts[i].uin > 0)
        {
            if (TRUE == Contacts[i].invis_list)
            {
                if (Contacts[i].vis_list)
                {
                    M_print (COLSERV "*" COLNONE);
                }
                else
                {
                    M_print (" ");
                }
                M_print (COLCONTACT "%-20s\t" COLMESS "(", Contacts[i].nick);
                Print_Status (Contacts[i].status);
                M_print (")" COLNONE "\n");
            }
        }
    }
    M_print (W_SEPERATOR);
}

/***************************************************************
nicer cleaner "e" display :}
****************************************************************/
void Show_Quick_Online_Status (void)
{
    int i;

    M_print (W_SEPERATOR);
    Time_Stamp ();
    M_print (" " MAGENTA BOLD "%10lu" COLNONE " ", UIN);
    M_print (i18n (71, "Your status is "));
    Print_Status (Current_Status);
    M_print ("\n");

    /* Loop displays all the online users */
    M_print ("%s%s\n", W_SEPERATOR, i18n (73, "Users online:"));
    for (i = 0; i < Num_Contacts; i++)
    {
        if ((SDWORD) Contacts[i].uin > 0)
        {
            if (FALSE == Contacts[i].invis_list)
            {
                if (Contacts[i].status != STATUS_OFFLINE)
                {
                    if (Contacts[i].vis_list)
                    {
                        M_print (COLSERV "*" COLNONE);
                    }
                    else
                    {
                        M_print (" ");
                    }
                    M_print (COLCONTACT "%-20s\t" COLMESS "(", Contacts[i].nick);
                    Print_Status (Contacts[i].status);
                    M_print (")" COLNONE "\n");
                }
            }
        }
    }
    M_print (W_SEPERATOR);
}


/***************************************************
Do not call unless you've already made a call to strtok()
****************************************************/
static void Change_Function (SOK_T sok)
{
    char *arg1;
    arg1 = strtok (NULL, " \n\r");
    if (arg1 == NULL)
    {
        M_print (i18n (703, COLCLIENT "Status modes: \n"));
        M_print ("Status %s\t%d\n", i18n (1, "Online"), STATUS_ONLINE);
        M_print ("Status %s\t%d\n", i18n (3, "Away"), STATUS_AWAY);
        M_print ("Status %s\t%d\n", i18n (2, "Do not disturb"), STATUS_DND);
        M_print ("Status %s\t%d\n", i18n (4, "Not Available"), STATUS_NA);
        M_print ("Status %s\t%d\n", i18n (7, "Free for chat"), STATUS_FREE_CHAT);
        M_print ("Status %s\t%d\n", i18n (5, "Occupied"), STATUS_OCCUPIED);
        M_print ("Status %s\t%d", i18n (6, "Invisible"), STATUS_INVISIBLE);
        M_print (COLNONE "\n");
    }
    else
    {
        icq_change_status (sok, atoi (arg1));
        Time_Stamp ();
        M_print (" ");
        Print_Status (Current_Status);
        M_print ("\n");
    }
}

/***************************************************
Do not call unless you've already made a call to strtok()
****************************************************/
static void Random_Function (SOK_T sok)
{
    char *arg1;
    arg1 = strtok (NULL, " \n\r");
    if (arg1 == NULL)
    {
        M_print (i18n (704, COLCLIENT "Groups: \n"));
        M_print (i18n (705, "General                    1\n"));
        M_print (i18n (706, "Romance                    2\n"));
        M_print (i18n (707, "Games                      3\n"));
        M_print (i18n (708, "Students                   4\n"));
        M_print (i18n (709, "20 something               6\n"));
        M_print (i18n (710, "30 something               7\n"));
        M_print (i18n (711, "40 something               8\n"));
        M_print (i18n (712, "50+                        9\n"));
        M_print (i18n (713, "Man chat requesting women 10\n"));
        M_print (i18n (714, "Woman chat requesting men 11\n"));
        M_print (i18n (715, "Micq                      49 (might not work but try it)"));
        M_print (COLNONE "\n");
    }
    else
    {
        icq_rand_user_req (sok, atoi (arg1));
/*      M_print ("\n" );*/
    }
}

/***************************************************
Do not call unless you've already made a call to strtok()
****************************************************/
static void Random_Set_Function (SOK_T sok)
{
    char *arg1;
    arg1 = strtok (NULL, " \n\r");
    if (arg1 == NULL)
    {
        M_print (i18n (704, COLCLIENT "Groups: \n"));
        M_print (i18n (716, "None                      -1\n"));
        M_print (i18n (705, "General                    1\n"));
        M_print (i18n (706, "Romance                    2\n"));
        M_print (i18n (707, "Games                      3\n"));
        M_print (i18n (708, "Students                   4\n"));
        M_print (i18n (709, "20 something               6\n"));
        M_print (i18n (710, "30 something               7\n"));
        M_print (i18n (711, "40 something               8\n"));
        M_print (i18n (712, "50+                        9\n"));
        M_print (i18n (713, "Man chat requesting women 10\n"));
        M_print (i18n (714, "Woman chat requesting men 11\n"));
        M_print (i18n (715, "Micq                      49 (might not work but try it)"));
        M_print (COLNONE "\n");
    }
    else
    {
        icq_rand_set (sok, atoi (arg1));
/*      M_print ("\n" );*/
    }
}

/*****************************************************************
Displays help information.
******************************************************************/
static void Help_Function (void)
{
    char *arg1;
    arg1 = strtok (NULL, " \n\r");

    if (!arg1)
    {
        M_print (COLCLIENT "%s\n", i18n (442, "Please select one of the help topics below."));
        M_print ("%s\t-\t%s\n", i18n (447, "Client"),
                 i18n (443, "Commands relating to micq displays and configuration."));
        M_print ("%s\t-\t%s\n", i18n (448, "Message"),
                 i18n (446, "Commands relating to sending messages."));
        M_print ("%s\t-\t%s\n", i18n (449, "User"),
                 i18n (444, "Commands relating to finding other users."));
        M_print ("%s\t-\t%s\n", i18n (450, "Account"),
                 i18n (445, "Commands relating to your ICQ account."));
    }
    else if (!strcasecmp (arg1, i18n (447, "Client")))
    {
        M_print (COLMESS "verbose #" COLNONE "%s",
                 i18n (418, "\t\t\tSet the verbosity level ( default = 0 ).\n"));
        M_print (COLMESS "%s" COLNONE "%s", clear_cmd, i18n (419, "\t\t\t\tClears the screen.\n"));
        M_print (COLMESS "%s" COLNONE "%s", sound_cmd,
                 i18n (420, "\t\t\t\tToggles beeping when recieving new messages.\n"));
        M_print (COLMESS "%s" COLNONE "%s", color_cmd,
                 i18n (421, "\t\t\t\tToggles displaying colors.\n"));
        M_print (COLMESS "%s\t" COLNONE "%s", quit_cmd, i18n (422, "\t\t\tLogs off and quits\n"));
        M_print (COLMESS "%s" COLNONE "%s", auto_cmd,
                 i18n (423, "\t\t\t\tDisplays your autoreply status\n"));
        M_print (COLMESS "%s [on|off]" COLNONE "%s", auto_cmd,
                 i18n (424, "\t\t\tToggles sending messages when your status is DND, NA, etc.\n"));
        M_print (COLMESS "%s <status> <message>" COLNONE "%s", auto_cmd,
                 i18n (425, "\t\tSets the message to send as an auto reply for the status\n"));
        M_print (COLMESS "%s <old cmd> <new cmd>" COLNONE "%s", alter_cmd,
                 i18n (417, "\tThis command allows you to alter your command set on the fly.\n"));
        M_print (COLMESS "%s <lang>" COLNONE "%s", "trans", 
                 i18n (800, "\t\tChange the working language to <lang>\n"));  
        M_print (i18n (717, COLCLIENT "\t! as the first character of a command will execute\n"));
        M_print (i18n (718, COLCLIENT "\ta shell command (e.g. \"!ls\"  \"!dir\" \"!mkdir temp\")" COLNONE "\n"));
    }
    else if (!strcasecmp (arg1, i18n (448, "Message")))
    {
        M_print (COLMESS "%s <uin>" COLNONE "%s", auth_cmd,
                 i18n (413, "\t\tAuthorize uin to add you to their list\n"));
        M_print (COLMESS "%s <uin>/<message>" COLNONE "%s", message_cmd,
                 i18n (409, "\t\tSends a message to uin\n"));
        M_print (COLMESS "%s <uin> <url> <message>" COLNONE "%s", url_cmd,
                 i18n (410, "\tSends a url and message to uin\n"));
        M_print (COLMESS "%s\t\t" COLNONE "%s", msga_cmd,
                 i18n (411, "\tSends a multiline message to everyone on your list.\n"));
        M_print (COLMESS "%s <message>" COLNONE "%s", again_cmd,
                 i18n (412, "\t\tSends a message to the last person you sent a message to\n"));
        M_print (COLMESS "%s <message>" COLNONE "%s", reply_cmd,
                 i18n (414, "\t\tReplys to the last person to send you a message\n"));
        M_print (COLMESS "%s <nick>" COLNONE "%s", "last",
                 i18n (403, "\t\tDisplays the last message received from <nick>.\n\t\t\tThey must be in your Contact List.\n\t\tOr a list of who has send you at least one message.\n"));
        M_print (COLMESS "%s" COLNONE "%s", "tabs", i18n (702, "\t\tDisplay a list of nicknames that you can tab through.\n")); 
        M_print (i18n (719, COLMESS "uptime\t\tShows how long Micq has been running.\n" COLNONE));
        M_print (i18n (720, COLCLIENT "\tuin can be either a number or the nickname of the user.\n"));
        M_print (i18n (721, COLCLIENT "\tSending a blank message will put the client into multiline mode.\n\tUse . on a line by itself to end message.\n"));
        M_print (i18n (722, "\tUse # on a line by itself to cancel the message." COLNONE "\n"));
    }
    else if (!strcasecmp (arg1, i18n (449, "User")))
    {
        M_print (COLMESS "%s <uin>" COLNONE "%s", auth_cmd,
                 i18n (413, "\t\tAuthorize uin to add you to their list\n"));
        M_print (COLMESS "%s [#]" COLNONE "%s", rand_cmd,
                 i18n (415, "\t\tFinds a random user in the specified group or lists the groups.\n"));
        M_print (COLMESS "pass <secret>" COLNONE "%s",
                 i18n (408, "\t\tChanges your password to secret.\n"));
        M_print (COLMESS "%s" COLNONE "%s", list_cmd,
                 i18n (416, "\t\t\tDisplays the current status of everyone on your contact list\n"));
        M_print (COLMESS "%s [user]" COLNONE "%s", status_cmd,
                 i18n (400, "\t\tShows locally stored info on user\n"));
        M_print (COLMESS "%s" COLNONE "%s", online_list_cmd,
                 i18n (407, "\t\t\tDisplays the current status of online people on your contact list\n"));
        M_print (COLMESS "%s" COLNONE "%s", "wide", 
                 i18n (801, "\t\tDisplays a list of people on your contact list in a screen wide format.\n")); 
        M_print (COLMESS "%s <uin>" COLNONE "%s", info_cmd,
                 i18n (430, "\t\tDisplays general info on uin\n"));
        M_print (COLMESS "%s <nick>" COLNONE "%s", togig_cmd,
                 i18n (404, "\t\tToggles ignoring/unignoring nick\n"));
        M_print (COLMESS "%s\t" COLNONE "%s", iglist_cmd,
                 i18n (405, "\t\tLists ignored nicks/uins\n"));
        M_print (COLMESS "%s [email@host]" COLNONE "%s", search_cmd,
                 i18n (429, "\tSearches for a ICQ user.\n"));
        M_print (COLMESS "%s <uin> <nick>" COLNONE "%s", add_cmd,
                 i18n (428, "\tAdds the uin number to your contact list with nickname.\n"));
        M_print (COLMESS "%s <nick>" COLNONE "%s", togvis_cmd,
                 i18n (406, "\tToggles your visibility to a user when you're invisible.\n"));
    }
    else if (!strcasecmp (arg1, i18n (450, "Account")))
    {
        M_print (COLMESS "%s [#]" COLNONE "%s", change_cmd,
                 i18n (427, "\tChanges your status to the status number.\n\t\tWithout a number it lists the available modes.\n"));
        M_print (COLMESS "reg password" COLNONE "%s",
                 i18n (426, "\tCreates a new UIN with the specified password.\n"));
        M_print (COLMESS "%s" COLNONE "%s", online_cmd, i18n (431, "\t\tMark as Online.\n"));
        M_print (COLMESS "%s" COLNONE "%s", away_cmd, i18n (432, "\t\tMark as Away.\n"));
        M_print (COLMESS "%s" COLNONE "%s", na_cmd, i18n (433, "\t\tMark as Not Available.\n"));
        M_print (COLMESS "%s" COLNONE "%s", occ_cmd, i18n (434, "\t\tMark as Occupied.\n"));
        M_print (COLMESS "%s" COLNONE "%s", dnd_cmd, i18n (435, "\t\tMark as Do not Disturb.\n"));
        M_print (COLMESS "%s" COLNONE "%s", ffc_cmd, i18n (436, "\t\tMark as Free for Chat.\n"));
        M_print (COLMESS "%s" COLNONE "%s", inv_cmd, i18n (437, "\t\tMark as Invisible.\n"));
        M_print (COLMESS "%s" COLNONE "%s", update_cmd,
                 i18n (438, "\t\tUpdates your basic info (email, nickname, etc.)\n"));
        M_print (COLMESS "other" COLNONE "%s",
                 i18n (401, "\t\tUpdates other user info like age and sex\n"));
        M_print (COLMESS "%s" COLNONE "%s", about_cmd,
                 i18n (402, "\t\tUpdates your about user info.\n"));
        M_print (COLMESS "set [#]" COLNONE "%s", i18n (439, "\t\tSets your random user group.\n"));
    }
}

/****************************************************
Retrieves info a certain user
*****************************************************/
static void Info_Function (SOK_T sok)
{
    char *arg1;

    arg1 = strtok (NULL, "");
    if (arg1 == NULL)
    {
        M_print (i18n (676, "Need uin to send to\n"));
        return;
    }
    uin = nick2uin (arg1);
    if (-1 == uin)
    {
        M_print (i18n (61, "%s not recognized as a nick name"), arg1);
        M_print ("\n");
        return;
    }
    M_print (i18n (723, "%s's IP address is "), arg1);
    Print_IP (uin);
    if ((UWORD) Get_Port (uin) != (UWORD) 0xffff)
    {
        M_print (i18n (673, "\tThe port is %d\n"), (UWORD) Get_Port (uin));
    }
    else
    {
        M_print (i18n (674, "\tThe port is unknown\n"));
    }
    send_info_req (sok, uin);
/*   send_ext_info_req( sok, uin );*/
}

/*
 * Gives information about internationalization and translates
 * strings by number.
 */
static void Trans_Function (SOK_T sok)
{
    const char *arg1;

    arg1 = strtok (NULL, " \t");
    if (!arg1)
    {
        M_print (i18n (79, "No translation; using compiled-in strings.\n"));
        return;
    }
    for (;arg1; arg1 = strtok (NULL, " \t"))
    {
        char *p;
        int i;
        i = strtol (arg1, &p, 10);
        if (i || !*p)
        {
            if (*p)
                M_print ("%s\n", i18n (87, "Ignoring garbage after number."));
            M_print ("%3d:%s\n", i, i18n (i, i18n (78, "No translation available.")));
        }
        else
        {
            if (!strcmp (arg1, "all"))
            {
                const char *p;
                int l = 0;
                for (i = 0; i - l < 100; i++)
                {
                    p = i18n (i, NULL);
                    if (p)
                    {
                        l = i;
                        M_print ("%3d:%s\n", i, p);
                    }
                }
            }
            else
            {
                if (!strcmp (arg1, ".") || !strcmp (arg1, "none") || !strcmp (arg1, "unload"))
                {
                    p = strdup (i18n (89, "Unloaded translation."));
                    i18nClose ();
                    M_print ("%s\n", p);
                    free (p);
                    continue;
                }
                i = i18nOpen (arg1);
                if (i == -1)
                    M_print (i18n (80, "Couldn't load \"%s\" internationalization.\n"), arg1);
                else if (i)
                    M_print (i18n (81, "Successfully loaded en translation (%d entries).\n"), i);
                else
                    M_print ("No internationalization requested.\n");
            } 
        }
    }
}

/**********************************************************
Handles automatic reply messages
***********************************************************/
static void Auto_Function (SOK_T sok)
{
    char *cmd;
    char *arg1;

    cmd = strtok (NULL, "");
    if (cmd == NULL)
    {
        M_print (i18n (724, "Automatic replies are %s.\n"), auto_resp ? i18n (85, "on") : i18n (86, "off"));
        M_print ("%30s %s\n", i18n (727, "The Do not disturb message is:"), auto_rep_str_dnd);
        M_print ("%30s %s\n", i18n (728, "The Away message is:"),           auto_rep_str_away);
        M_print ("%30s %s\n", i18n (729, "The Not available message is:"),  auto_rep_str_na);
        M_print ("%30s %s\n", i18n (730, "The Occupied message is:"),       auto_rep_str_occ);
        M_print ("%30s %s\n", i18n (731, "The Invisible message is:"),      auto_rep_str_inv);
        return;
    }
    else if (strcasecmp (cmd, "on") == 0)
    {
        auto_resp = TRUE;
        M_print (i18n (724, "Automatic replies are %s.\n"), i18n (85, "on"));
    }
    else if (strcasecmp (cmd, "off") == 0)
    {
        auto_resp = FALSE;
        M_print (i18n (724, "Automatic replies are %s.\n"), i18n (86, "off"));
    }
    else
    {
        arg1 = strtok (cmd, " ");
        if (arg1 == NULL)
        {
            M_print (i18n (734, "Sorry wrong syntax, can't find a status somewhere.\r\n"));
            return;
        }
        if (!strcasecmp (arg1, dnd_cmd))
        {
            cmd = strtok (NULL, "");
            if (cmd == NULL)
            {
                M_print (i18n (735, "Must give a message.\n"));
                return;
            }
            strcpy (auto_rep_str_dnd, cmd);
        }
        else if (!strcasecmp (arg1, away_cmd))
        {
            cmd = strtok (NULL, "");
            if (cmd == NULL)
            {
                M_print (i18n (735, "Must give a message.\n"));
                return;
            }
            strcpy (auto_rep_str_away, cmd);
        }
        else if (!strcasecmp (arg1, na_cmd))
        {
            cmd = strtok (NULL, "");
            if (cmd == NULL)
            {
                M_print (i18n (735, "Must give a message.\n"));
                return;
            }
            strcpy (auto_rep_str_na, cmd);
        }
        else if (!strcasecmp (arg1, occ_cmd))
        {
            cmd = strtok (NULL, "");
            if (cmd == NULL)
            {
                M_print (i18n (735, "Must give a message.\n"));
                return;
            }
            strcpy (auto_rep_str_occ, cmd);
        }
        else if (!strcasecmp (arg1, inv_cmd))
        {
            cmd = strtok (NULL, "");
            if (cmd == NULL)
            {
                M_print (i18n (735, "Must give a message.\n"));
                return;
            }
            strcpy (auto_rep_str_inv, cmd);
        }
        else
            M_print (i18n (736, "Sorry wrong syntax. Read tha help man!\n"));
        M_print (i18n (737, "Automatic reply setting\n"));

    }
}

/*************************************************************
Alters one of the commands
**************************************************************/
static void Alter_Function (void)
{
    char *cmd;

    cmd = strtok (NULL, " ");
    if (cmd == NULL)
    {
        M_print (i18n (738, "Need a command to alter!\n"));
        return;
    }
    if (!strcasecmp (cmd, auto_cmd))
        strncpy (auto_cmd, strtok (NULL, " \n\t"), 16);
    else if (!strcasecmp (cmd, message_cmd))
        strncpy (message_cmd, strtok (NULL, " \n\t"), 16);
    else if (!strcasecmp (cmd, add_cmd))
        strncpy (add_cmd, strtok (NULL, " \n\t"), 16);
    else if (!strcasecmp (cmd, togvis_cmd))
        strncpy (togvis_cmd, strtok (NULL, " \n\t"), 16);
    else if (!strcasecmp (cmd, info_cmd))
        strncpy (info_cmd, strtok (NULL, " \n\t"), 16);
    else if (!strcasecmp (cmd, togig_cmd))
        strncpy (togig_cmd, strtok (NULL, " \n\t"), 16);
    else if (!strcasecmp (cmd, iglist_cmd))
        strncpy (iglist_cmd, strtok (NULL, " \n\t"), 16);
    else if (!strcasecmp (cmd, quit_cmd))
        strncpy (quit_cmd, strtok (NULL, " \n\t"), 16);
    else if (!strcasecmp (cmd, reply_cmd))
        strncpy (reply_cmd, strtok (NULL, " \n\t"), 16);
    else if (!strcasecmp (cmd, again_cmd))
        strncpy (again_cmd, strtok (NULL, " \n\t"), 16);
    else if (!strcasecmp (cmd, list_cmd))
        strncpy (list_cmd, strtok (NULL, " \n\t"), 16);
    else if (!strcasecmp (cmd, online_list_cmd))
        strncpy (online_list_cmd, strtok (NULL, " \n\t"), 16);
    else if (!strcasecmp (cmd, away_cmd))
        strncpy (away_cmd, strtok (NULL, " \n\t"), 16);
    else if (!strcasecmp (cmd, na_cmd))
        strncpy (na_cmd, strtok (NULL, " \n\t"), 16);
    else if (!strcasecmp (cmd, dnd_cmd))
        strncpy (dnd_cmd, strtok (NULL, " \t\n"), 16);
    else if (!strcasecmp (cmd, online_cmd))
        strncpy (online_cmd, strtok (NULL, " \n\t"), 16);
    else if (!strcasecmp (cmd, occ_cmd))
        strncpy (occ_cmd, strtok (NULL, " \t\n"), 16);
    else if (!strcasecmp (cmd, ffc_cmd))
        strncpy (ffc_cmd, strtok (NULL, " \t\n"), 16);
    else if (!strcasecmp (cmd, inv_cmd))
        strncpy (inv_cmd, strtok (NULL, " \t\n"), 16);
    else if (!strcasecmp (cmd, status_cmd))
        strncpy (status_cmd, strtok (NULL, " \t\n"), 16);
    else if (!strcasecmp (cmd, auth_cmd))
        strncpy (status_cmd, strtok (NULL, " \n\t"), 16);
    else if (!strcasecmp (cmd, change_cmd))
        strncpy (change_cmd, strtok (NULL, " \n\t"), 16);
    else if (!strcasecmp (cmd, search_cmd))
        strncpy (search_cmd, strtok (NULL, " \t\n"), 16);
    else if (!strcasecmp (cmd, save_cmd))
        strncpy (save_cmd, strtok (NULL, " \t\n"), 16);
    else if (!strcasecmp (cmd, alter_cmd))
        strncpy (alter_cmd, strtok (NULL, " \t\n"), 16);
    else if (!strcasecmp (cmd, msga_cmd))
        strncpy (msga_cmd, strtok (NULL, " \n\t"), 16);
    else if (!strcasecmp (cmd, about_cmd))
        strncpy (about_cmd, strtok (NULL, " \n\t"), 16);
    else
        M_print
           ("Type help to see your current command, because this  one you typed wasn't one!\n");
}

/*************************************************************
Processes user input to send a message to a specified user
**************************************************************/
static void Message_Function (SOK_T sok)
{
    char *arg1;

    arg1 = strtok (NULL, UIN_DELIMS);
    if (!arg1)
    {
        M_print (i18n (676, "Need uin to send to\n"));
        return;
    }
    uin = nick2uin (arg1);
    if (uin == -1)
    {
        M_print (i18n (61, "%s not recognized as a nick name"), arg1);
        M_print ("\n");
        return;
    }
    arg1 = strtok (NULL, "");
    last_uin = uin;
    TabAddUIN (uin);
    if (arg1)
    {
        icq_sendmsg (sok, uin, arg1, NORM_MESS);
        Time_Stamp ();
        M_print (" " COLSENT "%10s" COLNONE " " MSGSENTSTR "%s\n", UIN2Name (last_uin),
                 MsgEllipsis (arg1));
    }
    else
    {
        status = 1;
        /* aaron
           Tell me that I'm sending a message to someone. The reason being that
           I have sent a reply to the wrong person far too many times. :)       */
        if (UIN2nick (last_uin))
            M_print (i18n (739, "Composing message to " COLCONTACT "%s" COLNONE ":\n"), UIN2nick (last_uin));
        else
            M_print (i18n (740, "Composing message to " COLCLIENT "%d" COLNONE ":\n"), last_uin);
        /* end of aaron */
        R_doprompt (i18n (41, "msg> "));
    }
}

/*******************************************************
Sends a reply message to the last person to message you.
********************************************************/
static void Reply_Function (SOK_T sok)
{
    char *arg1;

    if (!last_recv_uin)
    {
        M_print (i18n (741, "Must receive a message first\n"));
        return;
    }
    arg1 = strtok (NULL, "");
    last_uin = last_recv_uin;
    TabAddUIN (last_recv_uin);
    if (arg1)
    {
        icq_sendmsg (sok, last_recv_uin, arg1, NORM_MESS);
        Time_Stamp ();
        M_print (" " COLSENT "%10s" COLNONE " " MSGSENTSTR "%s\n", UIN2Name (last_recv_uin),
                 MsgEllipsis (arg1));
    }
    else
    {
        status = 1;
        /* aaron
           Tell me that I'm sending a message to someone. The reason being that
           I have sent a reply to the wrong person far too many times. :)       */
        if (UIN2nick (last_uin))
            M_print (i18n (739, "Composing message to " COLCONTACT "%s" COLNONE ":\n"), UIN2nick (last_uin));
        else
            M_print (i18n (740, "Composing message to " COLCLIENT "%d" COLNONE ":\n"), last_uin);
        /* end of aaron */
        R_doprompt (i18n (41, "msg> "));
    }
}

static void Again_Function (SOK_T sok)
{
    char *arg1;

    if (!last_uin)
    {
        M_print (i18n (742, "Must write one message first\n"));
        return;
    }
    TabAddUIN (last_uin);
    arg1 = strtok (NULL, "");
    if (arg1)
    {
        icq_sendmsg (sok, last_uin, arg1, NORM_MESS);
        Time_Stamp ();
        M_print (" " COLSENT "%10s" COLNONE " " MSGSENTSTR "%s\n", UIN2Name (last_uin),
                 MsgEllipsis (arg1));
    }
    else
    {
        status = 1;
        /* aaron
           Tell me that I'm sending a message to someone. The reason being that
           I have sent a reply to the wrong person far too many times. :)       */
        if (UIN2nick (last_uin))
            M_print (i18n (739, "Composing message to " COLCONTACT "%s" COLNONE ":\n"), UIN2nick (last_uin));
        else
            M_print (i18n (740, "Composing message to " COLCLIENT "%d" COLNONE ":\n"), last_uin);
        /* end of aaron */
        R_doprompt (i18n (41, "msg> "));
    }
}

static void Verbose_Function (void)
{
    char *arg1;

    arg1 = strtok (NULL, "");
    if (arg1 != NULL)
    {
        Verbose = atoi (arg1);
    }
    M_print (i18n (60, "Verbosity level is %d.\n"), Verbose);
}
