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
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
  #include <winsock2.h>
#else
  #include <unistd.h>
  #include <netinet/in.h>
  #include <sys/types.h>
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
#include "util_ui.h"	/* aaron -- to get access to Get_Max_Screen_Width()
						   function	*/

#ifdef __BEOS__
#include "beos.h"
#endif
 
MORE_INFO_STRUCT user;

#if 0
char *strdup( const char * );
int strcasecmp( const char *, const char * );
#endif

/* aaron
   Displays the Contact List in a wide format, similar to the ls command.	*/
void PrintContactWide() {
	int		*Online;  /* definitely won't need more; could	*/
	int		*Offline; /* probably get away with less.	*/
	int		MaxLen = 0;	/* legnth of longest contact name */
	int		i;
	int		OnIdx = 0;		/* for inserting and tells us how many there are*/
	int		OffIdx = 0;		/* for inserting and tells us how many there are*/
	int		NumCols;		/* number of columns to display on screen		*/
	int		Col;			/* the current column during output.			*/

	if ((Online = (int *)malloc(Num_Contacts * sizeof(int))) == NULL) {
		M_print("Insuffificient memory to display a wide Contact List.\n");
		return;
	}  /* end if */

	if ((Offline = (int *)malloc(Num_Contacts * sizeof(int))) == NULL) {
		M_print("Insuffificient memory to display a wide Contact List.\n");
		return;
	}  /* end if */

	/* We probably don't need to zero out the array, but just to be on the
		safe side...
		The arrays really should be only Num_Contacts in size... future
		improvement, I guess. Hopefully no one is running that tight on
		memory.																*/
	memset(Online, 0, sizeof(Online));
	memset(Offline, 0, sizeof(Offline));

	/* Filter the contact list into two lists -- online and offline. Also
		find the longest name in the list -- this is used to determine how
		many columns will fit on the screen.								*/
	for (i = 0; i < Num_Contacts; i++) {
		if ( (S_DWORD) Contacts[i].uin > 0 ) { /* Aliases */
			if (Contacts[i].status == STATUS_OFFLINE)
				Offline[OffIdx++] = i;
			else
				Online[OnIdx++] = i;
	
			if (strlen(Contacts[i].nick) > MaxLen)
				MaxLen = strlen(Contacts[i].nick);
		}
	}  /* end for */

	/* This is probably a very ugly way to determine the number of columns
		to use... it's probably specific to my own contact list.			*/
	NumCols = Get_Max_Screen_Width() / (MaxLen+4);
	if (NumCols < 1)
		NumCols = 1;		/* sanity check. :)  */
	
	/* Fairly simple print routine. We check that we only print the right
		number of columns to the screen.									*/
	M_print(MESSCOL );
	for ( i = 1; i < ( Get_Max_Screen_Width() - strlen( "Offline" ) ) / 2 ; i++ ) { M_print( "=" ); }
	M_print( CLIENTCOL "Offline" MESSCOL);
	for ( i = 1; i < ( Get_Max_Screen_Width() - strlen( "Offline" ) ) / 2 ; i++ ) { M_print( "=" ); }
	M_print( NOCOL "\n" );
	for (Col = 1, i = 0; i < OffIdx; i++)
		if (Col % NumCols == 0) {
			M_print(CONTACTCOL "  %-*s\n" NOCOL, MaxLen+2, 
					Contacts[Offline[i]].nick);
			Col = 1;
		} else {
			M_print(CONTACTCOL "  %-*s" NOCOL, MaxLen+2, 
					Contacts[Offline[i]].nick);
			Col++;
		}  /* end if */

	/* The user status for Online users is indicated by a one-character
		prefix to the nickname. Unfortunately not all statuses (statusae? :)
		are unique at one character. A better way to encode the information
		is needed.															*/
	if ( (Col-1) % NumCols != 0) { M_print( "\n" ); }
	M_print(MESSCOL );
	for ( i = 1; i < ( Get_Max_Screen_Width() - strlen( "Online" ) ) / 2 ; i++ ) { M_print( "=" ); }
	M_print( CLIENTCOL "Online" MESSCOL);
	for ( i = 1; i < ( Get_Max_Screen_Width() - strlen( "Online" ) ) / 2 ; i++ ) { M_print( "=" ); }
	M_print( NOCOL "\n" );
	for (Col = 1, i = 0; i < OnIdx; i++) {
		char *status;
		char weird = 'W'; /* for weird statuses that are reported as hex */

		status = Convert_Status_2_Str(Contacts[Online[i]].status);
		status = status ? status : &weird;
		if ( ( Contacts[Online[i]].status & 0xfff) == STATUS_ONLINE ) {
			status = " ";
		}
		if (Col % NumCols == 0) {
			M_print(NOCOL "%c " CONTACTCOL "%-*s\n" NOCOL, 
			/*		*Convert_Status_2_Str(Contacts[Online[i]].status), */ *status,
					MaxLen+2, Contacts[Online[i]].nick);
			Col = 1;
		} else {
			M_print(NOCOL "%c " CONTACTCOL "%-*s" NOCOL, 
/*					*Convert_Status_2_Str(Contacts[Online[i]].status), */ *status,
					MaxLen+2, Contacts[Online[i]].nick);
			Col++;
		}  /* end if */
	}
	if ( (Col-1) % NumCols != 0) { M_print( "\n" ); }
	M_print( MESSCOL );
	for ( i = 1; i < Get_Max_Screen_Width() - 1 ; i++ ) { M_print( "=" ); }
	M_print( NOCOL "\n" );
	free(Online);
	free(Offline);
}  /* end of aaron */

BOOL Do_Multiline( SOK_T sok, char *buf )
{
   static int offset=0;
   static char msg[1024];
   
   msg[ offset ] = 0;
   if ( strcmp( buf, END_MSG_STR ) == 0 )
   {
      icq_sendmsg( sok, multi_uin, msg, NORM_MESS );
      M_print( MESSAGE_SENT_1_STR );
      Print_UIN_Name( multi_uin );
      M_print( MESSAGE_SENT_2_STR );
      last_uin = multi_uin;
      offset = 0;
      return FALSE;
   }
   else if ( strcmp( buf, CANCEL_MSG_STR ) == 0 )
   {
      M_print( MESSAGE_CANCELED_STR );
      last_uin = multi_uin;
      offset = 0;
      return FALSE;
   }
   else
   {
      if ( offset + strlen( buf ) < 450 )
      {
         strcat( msg, buf );
         strcat( msg, "\r\n" );
         offset += strlen( buf ) + 2;
         return TRUE;
      }
      else
      {
         M_print( MESSAGE_BUFFER_FULL_STR );
         M_print( MESSAGE_SENT_1_STR );
         Print_UIN_Name( multi_uin );
         M_print( MESSAGE_SENT_2_STR );
         icq_sendmsg( sok, multi_uin, msg, NORM_MESS );
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
void Info_Other_Update( SOK_T sok, char * buf)
{
	static OTHER_INFO_STRUCT other;
	int temp;

	switch ( status ) {
		case NEW_AGE:
			other.age = atoi( buf );
			M_print( UPDATE_SEX_STR );
			status = NEW_SEX;
			break;
		case NEW_SEX:
			if ( ( buf[0] == 'f' ) || ( buf[0] == 'F' ) ) {
				other.sex = 1;
			} else if ( ( buf[0] == 'm' ) || ( buf[0] == 'M' ) ) {
				other.sex = 2;
			} else {
				other.sex = 0;
			}
			M_print( UPDATE_HP_STR );
			status = NEW_HP;
			break;
		case NEW_HP:
			other.hp = strdup( buf );
			M_print( UPDATE_YEAR_OF_BIRTH_STR );
			status = NEW_YEAR;
			break;
		case NEW_YEAR:
			other.year = atoi( buf ) - 1900;
			M_print( UPDATE_MONTH_OF_BIRTH_STR );
			status = NEW_MONTH;
			break;
		case NEW_MONTH:
			other.month = atoi( buf ) ;
			M_print( UPDATE_DAY_OF_BIRTH_STR );
			status = NEW_DAY;
			break;
		case NEW_DAY:
			other.day = atoi( buf );
			M_print( UPDATE_LANG_STR );
			status = NEW_LANG1;
			break;
		case NEW_LANG1:
			temp = atoi( buf );
			if ( ( 0 == temp ) && ( toupper( buf[0] ) == 'L' ) ) {
				Print_Lang_Numbers();
				status = NEW_LANG1;
			} else {
				other.lang1 = temp;
				status = NEW_LANG2;
			}
			M_print( UPDATE_LANG_STR );
			break;
		case NEW_LANG2:
			temp = atoi( buf );
			if ( ( 0 == temp ) && ( toupper( buf[0] ) == 'L' ) ) {
				Print_Lang_Numbers();
				status = NEW_LANG2;
			} else {
				other.lang2 = temp;
				status = NEW_LANG3;
			}
			M_print( UPDATE_LANG_STR );
			break;
		case NEW_LANG3:
			temp = atoi( buf );
			if ( ( 0 == temp ) && ( toupper( buf[0] ) == 'L' ) ) {
				Print_Lang_Numbers();
				status = NEW_LANG3;
				M_print( UPDATE_LANG_STR );
			} else {
				other.lang3 = temp;
				Update_Other( sok, &other );
				free( other.hp );
				status = 0;
			}
			break;
			
	}
}

/***************************************************************************
This function gathers information for the main info update packet
name email etc
****************************************************************************/
void Info_Update( SOK_T sok, char *buf )
{
 
   switch ( status ) {
   case NEW_NICK:
      user.nick = strdup( (char *) buf );
   	M_print ( FIRST_NAME_UPDATE_STR );
      status++;
      break;
   case NEW_FIRST:
      user.first = strdup( (char *) buf );
   	M_print ( LAST_NAME_UPDATE_STR );
      status++;
      break;
   case NEW_LAST:
      user.last = strdup( (char *) buf );
   	M_print ( EMAIL_UPDATE_STR );
      status++;
      break;
   case NEW_EMAIL:
      user.email = strdup( (char *) buf );
   	M_print( EMAIL2_UPDATE_STR );
      status++;
      break;
   case NEW_EMAIL2:
      user.email2 = strdup( (char *) buf );
   	M_print( EMAIL3_UPDATE_STR );
      status++;
      break;
   case NEW_EMAIL3:
      user.email3 = strdup( (char *) buf );
   	M_print( CITY_UPDATE_STR );
      status++;
      break;
   case NEW_CITY:
      user.city = strdup( (char *) buf );
   	M_print( STATE_UPDATE_STR );
      status++;
      break;
   case NEW_STATE:
      user.state = strdup( (char *) buf );
   	M_print( PHONE_UPDATE_STR );
      status++;
      break;
   case NEW_PHONE:
      user.phone = strdup( (char *) buf );
   	M_print( FAX_UPDATE_STR );
      status++;
      break;
   case NEW_FAX:
      user.fax = strdup( (char *) buf );
   	M_print( STREET_UPDATE_STR );
      status++;
      break;
   case NEW_STREET:
      user.street = strdup( (char *) buf );
   	M_print( CELLULAR_UPDATE_STR );
      status++;
      break;
   case NEW_CELLULAR:
      user.cellular = strdup( (char *) buf );
   	M_print( ZIP_UPDATE_STR );
      status++;
      break;
   case NEW_ZIP:
      user.zip = atoi( (char *) buf );
      M_print( COUNTRY_UPDATE_STR );
      status = NEW_COUNTRY;
      break;
   case NEW_COUNTRY:
      user.country = atoi( (char *) buf );
      M_print( TIME_ZONE_UPDATE_STR );
      status = NEW_TIME_ZONE;
      break;
   case NEW_TIME_ZONE:
      user.c_status = atoi( (char *) buf );
      user.c_status <<= 1;
      M_print( AUTH_QUESTION_STR );
      status = NEW_AUTH;
      break;
   case NEW_AUTH:
      if ( ! strcasecmp( buf, NO_STR ) )
      {
         user.auth = FALSE;
         Update_More_User_Info( sok, &user );
         free( user.nick );
         free( user.last );
         free( user.first );
         free( user.email ); 
         status = 0;
      }
      else if ( ! strcasecmp( buf, YES_STR ) )
      {
         user.auth = TRUE;
         Update_More_User_Info( sok, &user );
         free( user.nick );
         free( user.last );
         free( user.first );
         free( user.email );
         status = 0;
      }
      else
      {
         M_print( YESNO_RESPONSE_STR );
      	M_print( AUTH_QUESTION_STR );
      }
      break;
   }
}

/**************************************************************************
Multi line about info updates
**************************************************************************/
BOOL About_Info( SOK_T sok, char *buf )
{
   static int offset=0;
   static char msg[1024];
   
   msg[ offset ] = 0;
   if ( strcmp( buf, END_MSG_STR ) == 0 )
   {
      Update_About( sok, msg ); 
      offset = 0;
      return FALSE;
   }
   else if ( strcmp( buf, CANCEL_MSG_STR ) == 0 )
   {
      offset = 0;
      return FALSE;
   }
   else
   {
      if ( offset + strlen( buf ) < 450 )
      {
         strcat( msg, buf );
         strcat( msg, "\r\n" );
         offset += strlen( buf ) + 2;
         return TRUE;
      }
      else
      {
         Update_About( sok, msg ); 
         offset = 0;
         return FALSE;
      }
   }
}

/**************************************************************************
Gets the info to search for a user by
***************************************************************************/
void User_Search( SOK_T sok, char *buf )
{
	switch (status) {
	case SRCH_START:
		M_print ( "Enter the Users E-mail address : ");
		status++;
	break;
	case SRCH_EMAIL:
		user.email = strdup( (char *) buf );
		M_print( "Enter the Users Nick : ");
		status++;
	break;
	case SRCH_NICK:
		user.nick = strdup( (char *) buf );
		M_print ( "Enter The Users First Name : ");
		status++;
	break;
	case SRCH_FIRST:
		user.first = strdup( (char *) buf );
		M_print ( "Enter The Users Last Name : ");
		status++;
	break;
	case SRCH_LAST:
		user.last = strdup( (char *) buf );
		start_search( sok, user.email, user.nick, 
				user.first, user.last );
		status = 0;
	break;
	}
}

/**************************************************************************
Messages everyone in the conact list
***************************************************************************/
BOOL Do_Multiline_All( SOK_T sok, char *buf )
{
   static int offset=0;
   static char msg[1024];
   char * temp;
   int i;
   
   msg[ offset ] = 0;
   if ( strcmp( buf, END_MSG_STR ) == 0 )
   {
      for ( i=0; i < Num_Contacts; i++ ) {
         temp = strdup ( msg );
         icq_sendmsg( sok, Contacts[i].uin, temp, MRNORM_MESS );
	 free( temp );
      }
      M_print( "Message sent!\n" );
      offset = 0;
      return FALSE;
   }
   else if ( strcmp( buf, CANCEL_MSG_STR ) == 0 )
   {
      M_print( MESSAGE_CANCELED_STR );
      offset = 0;
      return FALSE;
   }
   else
   {
      if ( offset + strlen( buf ) < 450 )
      {
         strcat( msg, buf );
         strcat( msg, "\r\n" );
         offset += strlen( buf ) + 2;
         return TRUE;
      }
      else
      {
         M_print( MESSAGE_BUFFER_FULL_STR );
         for ( i=0; i < Num_Contacts; i++ ) {
	    temp = strdup( msg );
            icq_sendmsg( sok, Contacts[i].uin, temp, MRNORM_MESS );
	    free( temp );
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
void Get_Input( SOK_T sok , int *idle_val, int *idle_flag ) /* GRYN */
{
   char buf[1024]; /* This is hopefully enough */
   char *cmd;
   char *arg1;
   char *arg2;
   int i;

/* GRYN - START */
   int idle_save;
   idle_save = *idle_val;
   *idle_val = time( NULL );
/* GRYN - STOP */

   memset( buf, 0, 1024 );
#ifdef __BEOS__
   Be_GetText(buf);
#else
   R_getline( buf, 1024 );
#endif
    M_print( "\r" ); /* reset char printed count for dumb terminals */
   buf[1023]=0; /* be safe */
   if ( status == 1 )
   {
      if ( ! Do_Multiline( sok, buf ) )
      {
         status = 2;
      } else {
	   R_doprompt ( MSG_PROMPT_STR );
   	}
   }
   else if ( status == 3 )
   {
      if ( ! Do_Multiline_All( sok, buf ) ) {
         status = 2;
      } else {
   	   R_doprompt ( MSGA_PROMPT_STR );
   	}
   }
   else if ( status == MULTI_ABOUT ) { 
      if ( ! About_Info( sok, buf ) )
      {
         status = 2;
      } else {
	   R_doprompt ( ABOUT_PROMPT_STR );
   	}
   }
   else if ( ( status >= NEW_NICK) && ( status <= NEW_AUTH) )
   {  Info_Update( sok, buf ); }
   else if ( ( status >= NEW_AGE) && ( status <= NEW_LANG3) )
   {  Info_Other_Update( sok, buf ); }
   else if ( ( status >= SRCH_START) && ( status <= SRCH_LAST) )
   { User_Search( sok, buf ); }
   else
   {
      if ( buf[0] != 0 )
      {
        if ( '!' == buf[0] )
        {
	   R_pause ();
#ifdef SHELL_COMMANDS
	   system( &buf[1] );
#else
	  M_print( "Shell commands have been disabled.\n" );
#endif
	   R_resume ();
           Prompt();
           return;
        }
         cmd = strtok( buf, " \n\t" );
        if ( NULL == cmd )
        {
           Prompt();
           return;
        }
         /* goto's removed and code fixed by Paul Laufer. Enjoy! */
         if ( ( strcasecmp( cmd , "quit" ) == 0 ) ||
              ( strcasecmp( cmd , "/quit" ) == 0 ) )
            {  Quit = TRUE; }
         else if ( strcasecmp( cmd, quit_cmd ) == 0 )
            {  Quit = TRUE; }
         else if ( strcasecmp( cmd, sound_cmd ) == 0 )
         {
            /* GRYN */ *idle_val = idle_save;
            if ( SOUND_ON == Sound )
            {
               Sound = SOUND_OFF;
               M_print( "Sound" SERVCOL " OFF" NOCOL ".\n" );
            }
            else if ( SOUND_OFF == Sound )
            {
               Sound = SOUND_ON;
               M_print( "Sound" SERVCOL  " ON" NOCOL ".\n" );
            }
         }
         else if ( strcasecmp( cmd, change_cmd ) == 0 )
            {  Change_Function( sok ); }
         else if ( strcasecmp( cmd, rand_cmd ) == 0 )
            {  Random_Function( sok ); }
         else if ( strcasecmp( cmd, "set" ) == 0 )
            {  Random_Set_Function( sok ); }
            else if ( ! strcasecmp( cmd, color_cmd ) )
            { 
               /* GRYN */ *idle_val = idle_save;
               Color = !Color;
               if ( Color )
               {
#ifdef FUNNY_MSGS
                  M_print( SERVCOL "Being " MESSCOL "all " CLIENTCOL
		  "colorful " NOCOL
		  "and " SERVCOL "cute " CONTACTCOL "now\n" NOCOL );
#else
                   M_print( "Color is " MESSCOL "on" NOCOL ".\n" );
#endif
               }
               else
               {
#ifdef FUNNY_MSGS
                  M_print( SERVCOL "Dull colorless mode on, resistance is futile\n" NOCOL );
#else
                   M_print( "Color is " MESSCOL "off" NOCOL ".\n" );
#endif
               }
            }
	      else if ( ! strcasecmp( cmd, online_cmd ) ) /* online command */
              {   /* GRYN */ *idle_flag = 0;
   	          CHANGE_STATUS( STATUS_ONLINE ); }
	      else if ( ! strcasecmp( cmd, away_cmd ) ) /* away command */
                 {   /* GRYN */ *idle_flag = 0;
	             CHANGE_STATUS( STATUS_AWAY ); }
	      else if ( ! strcasecmp( cmd, na_cmd ) ) /* Not Available command*/
   	         {   /* GRYN */ *idle_flag = 0;
                     CHANGE_STATUS( STATUS_NA ); }
	      else if ( ! strcasecmp( cmd, occ_cmd ) ) /* Occupied command */
                 {   /* GRYN */ *idle_flag = 0;
	             CHANGE_STATUS( STATUS_OCCUPIED ); }
	      else if ( ! strcasecmp( cmd, dnd_cmd ) ) /* Do not Disturb command */
              {   /* GRYN */ *idle_flag = 0;
   	          CHANGE_STATUS( STATUS_DND ); }
	      else if ( ! strcasecmp( cmd, ffc_cmd ) ) /* Free For Chat command */
              {   /* GRYN */ *idle_flag = 0;
   	          CHANGE_STATUS( STATUS_FREE_CHAT ); }
	      else if ( ! strcasecmp( cmd, inv_cmd ) ) /* Invisible command */
              {   /* GRYN */ *idle_flag = 0;
   	          CHANGE_STATUS( STATUS_INVISIBLE ); }
	      else if ( ! strcasecmp( cmd, search_cmd ) )
   	   {
             arg1 = strtok( NULL, "\n" );
             if ( arg1 == NULL )
             {
		      status = SRCH_START;
		      User_Search( sok, buf );
             }
             else
             {
   	          start_search( sok, arg1, "", "", "" );
             }
	      }
         else if ( ! strcasecmp( cmd, status_cmd ) ) 
         {  /* GRYN */ *idle_val = idle_save;
		arg1 = strtok( NULL, "\n"); 
		Show_Status( arg1 ); }
         else if ( ! strcasecmp( cmd, list_cmd ) ) 
         {  /* GRYN */ *idle_val = idle_save;
		Show_Quick_Status(); }
         else if ( ! strcasecmp( cmd, online_list_cmd ) ) 
         {  /* GRYN */ *idle_val = idle_save;
		Show_Quick_Online_Status(); }
         else if ( ! strcasecmp( cmd, msga_cmd ) )
         {
            status = 3;
            /* aaron
             Tell me that I'm sending a message to everyone                 */
            M_print("Composing message to " SERVCOL "all" NOCOL ":\n");
            /* end of aaron */
            R_doprompt( MSGA_PROMPT_STR );
         }
	 else if ( ! strcasecmp( cmd, "other" ) ) {
		status = NEW_AGE;
		M_print( UPDATE_AGE_STR );
	 }
         else if ( ! strcasecmp( cmd, reply_cmd ) ) /* reply command */
         {
            Reply_Function( sok );
         }
         else if ( ! strcasecmp( cmd, "reg" ) )
         {
            arg1 = strtok( NULL, "" );
            if ( arg1 != NULL )
            {  reg_new_user( sok, arg1 ); }
         }
         else if ( ! strcasecmp( cmd, "pass" ) )
         {
            arg1 = strtok( NULL, "" );
            if ( arg1 != NULL )
            {  Change_Password( sok, arg1 ); }
         }
         else if ( ! strcasecmp( cmd, about_cmd ) )
         {
            arg1 = strtok( NULL, "" );
            if ( NULL != arg1 ) 
            { Update_About( sok, arg1 ); }
            else { 
               status = MULTI_ABOUT;
	       R_doprompt ( ABOUT_PROMPT_STR );
            }
         }
         else if ( ! strcasecmp( cmd, again_cmd ) ) /* again command */
         {  Again_Function( sok ); }
         else if ( ! strcasecmp( cmd, clear_cmd))
   	   { /* GRYN */ *idle_val = idle_save; 
             clrscr(); }
         else if ( ! strcasecmp( cmd, info_cmd ) )
         {  Info_Function( sok ); }
         else if ( ! strcasecmp( cmd, togig_cmd ) )
         {
            arg1 = strtok( NULL, "\n" );
            if ( arg1 != NULL )
            {
            uin = nick2uin( arg1 );
            if ( -1 == uin )
            {
                M_print( "%s not recognized as a nick name", arg1 );
            }
		 else
		 {
		 CONTACT_PTR bud;
		 bud=UIN2Contact(uin);
		 if (bud->invis_list==TRUE)
		       {
		       bud->invis_list=FALSE;
		       update_list( sok, uin, INV_LIST_UPDATE, FALSE );
		       M_print( "Unignored %s!\n", UIN2nick(uin) );
		       
		 }	
		       else
		       {
		       bud->vis_list=FALSE;
		       bud->invis_list=TRUE;
		       update_list( sok, uin, INV_LIST_UPDATE, TRUE );
		       M_print( "Ignoring %s!\n", UIN2nick(uin) );
		       }
		 snd_contact_list( sok );
		 snd_invis_list( sok );
		 snd_vis_list( sok );
		 CHANGE_STATUS( Current_Status );
  		 }
            }
            else
            {
               M_print( SERVCOL "Must specify a nick name" NOCOL "\n" );
            }
         }
         else if ( ! strcasecmp( cmd, iglist_cmd ) ) 
         {  Show_Ignore_Status(); }
         else if ( ! strcasecmp( cmd, "ver" ) )
         { M_print( "Micq Version : " MESSCOL MICQ_VERSION NOCOL " Compiled on "__TIME__ " "  __DATE__"\n" ); }
         else if ( ! strcasecmp( cmd, add_cmd ) )
         {
            arg1 = strtok( NULL, " \t" );
            if ( arg1 != NULL )
            {
               uin = atoi( arg1 );
               arg1 = strtok( NULL, "" );
               if ( arg1 != NULL )
               {
                  Add_User( sok, uin, arg1 );
                  M_print( "%s added.\n", arg1 );
               }
            }
            else
            {
               M_print( SERVCOL "Must specify a nick name" NOCOL "\n" );
            }
         }
         else if ( ! strcasecmp( cmd, togvis_cmd ) )
         {
            arg1 = strtok( NULL, " \t" );
            if ( arg1 != NULL )
            {
            uin = nick2uin( arg1 );
            if ( -1 == uin )
          	  {
                M_print( "%s not recognized as a nick name", arg1 );
              }
			  else
			  {
			  CONTACT_PTR bud;
			  bud=UIN2Contact(uin);
			  if (bud->vis_list==TRUE)
				{
				bud->vis_list=FALSE;
		       update_list( sok, uin, VIS_LIST_UPDATE, FALSE );
				M_print( "Invisible to %s now.\n", UIN2nick(uin) );
				}
				else
				{
				bud->vis_list=TRUE;
		       update_list( sok, uin, VIS_LIST_UPDATE, TRUE );
				M_print( "Visible to %s now.\n", UIN2nick(uin) );
				}
			  /*FIXME*/
/*			  snd_contact_list( sok );
			  snd_invis_list( sok );
			  snd_vis_list( sok );
*/
  			  }
            }
            else
            {
               M_print( SERVCOL "Must specify a nick name" NOCOL "\n" );
            }
         }
         else if ( strcasecmp( cmd, "verbose" ) == 0 )
         { Verbose_Function(); }
         else if ( strcasecmp( cmd, "rinfo" ) == 0 )
         {
            Print_UIN_Name( last_recv_uin );
            M_print( "'s IP address is " );
            Print_IP( last_recv_uin );
            if ( (WORD) Get_Port( last_recv_uin ) != (WORD) 0xffff ) {
               M_print( "\tThe port is %d\n",(WORD) Get_Port( last_recv_uin ) );
            } else {
               M_print( "\tThe port is unknown\n" );
            }
/*            M_print( "\n" );*/
            send_info_req( sok, last_recv_uin );
/*            send_ext_info_req( sok, last_recv_uin );*/
         }
         else if ( ( strcasecmp( cmd, "/help" ) == 0 ) ||/* Help command */
                   ( strcasecmp( cmd, "help" ) == 0 ) )
         {  Help_Function(); }
         else if ( strcasecmp( cmd, auth_cmd ) == 0 )
         {
             arg1 = strtok( NULL, "" );
             if ( arg1 == NULL )
             {
                M_print( "Need uin to send to" );
             }
             else
             {
              uin = nick2uin( arg1 );
              if ( -1 == uin )
              {
                 M_print( "%s not recognized as a nick name", arg1 );
              }
              else icq_sendauthmsg( sok, uin );
             }         
         }
         else if ( strcasecmp( cmd, message_cmd ) == 0 ) /* "/msg" */
         {
            Message_Function( sok );
         }
	      else if ( strcasecmp( cmd, url_cmd ) == 0 ) /* "/msg" */
	      {
		      arg1 = strtok( NULL, " " );
		      if ( arg1 == NULL )
		      {
			      M_print( "Need uin to send to\n" );
		      }
		      else
		      {
			      uin = nick2uin( arg1 );
			      if ( uin == -1 )
			      {
				      M_print( "%s not recognized as a nick name\n", arg1 );
			      }
			      else
			      {
				      arg1 = strtok( NULL, " " );
				      last_uin = uin;
				      if ( arg1 != NULL )
				      {
					      arg2 = strtok(NULL, "");
/*					      sM_print(arg1, "%s%c%s", arg1, '\xFE', arg2);*/
					      icq_sendurl( sok, uin, arg1, arg2 );
					      M_print( "URL sent to ");
					      Print_UIN_Name( last_uin );
					      M_print( "!\n");
				      } else {
					      M_print("Need URL please.\n");
				      } 
			      }
		      }
	      }
         else if ( ! strcasecmp( cmd, alter_cmd ) ) /* alter command */
         {  Alter_Function(); }
         else if ( ! strcasecmp( cmd, save_cmd ) ) /* save command */
         {
            i=Save_RC();
            if (i==-1)
               M_print("Sorry saving your personal reply messages went wrong!\n");
            else
               M_print("Your personal settings have been saved!\n");
         }
         else if ( ! strcasecmp( cmd, "update" ) )
         {
            status = NEW_NICK;
            M_print( NICK_NAME_UPDATE_STR );
         }
         else if ( strcasecmp( cmd, auto_cmd ) == 0 )
         {  Auto_Function( sok ); }
         else if ( strcasecmp( cmd, "tabs" ) == 0 )
         {
             /* aaron
              Added check for early exit. If we haven't talked to 10 people
              yet then we get the first person in the Contact List repeated
              for every unused tab position. This isn't really a problem,
              it just looks a little messy.                                 */
            M_print("Last %d people you talked to:\n", TAB_SLOTS);
            for (i = 0; (i < TAB_SLOTS) && (tab_array[i] != NULL); i++)
            {
               DWORD temp_uin;
               CONTACT_PTR cont;
               temp_uin = nick2uin( tab_array[i] );
               M_print("   " );
               Print_UIN_Name( temp_uin );
               cont = UIN2Contact( temp_uin );
               if ( NULL != cont ) {
                  M_print( MESSCOL " (now " );
                  Print_Status(  cont->status );
                  M_print( ")" NOCOL );
               }
               M_print( "\n" );
/*               M_print("   %s%s%s (now ", CONTACTCOL, Contacts[tab_array[i]].nick, MESSCOL);
               Print_Status( tab_array[i] );
               M_print(")%s\n", NOCOL);*/
            }
         }
        /* aaron
         Displays the last message received from the given nickname. The
         <nickname> must be a member of the Contact List because that's the
         only place we store the last message.

         Removed space (" ") from strtok so that nicknames with spaces
         can be interpreted properly. I'm not sure if this is the correct
         way to do it, but it seems to work.                                */
         else if ( strcasecmp( cmd, "last") == 0 )
         {
             arg1 = strtok(NULL, "\t\n");  /* Is this right? */
             if (arg1 == NULL) {
			 	int		i;

#ifdef FUNNY_MSGS
				M_print("These people are being sociable today:\n");
#else
				M_print("You have received messages from:\n");
#endif
				for (i = 0; i < Num_Contacts; i++) {
					if (Contacts[i].LastMessage != NULL)
						M_print(CONTACTCOL "  %s\n" NOCOL, Contacts[i].nick);
				}  /* end if */
             } else {
                 if ((uin = nick2uin(arg1)) == -1) {
                     M_print("Unknown Contact: %s\n", arg1);
                 } else {
                     if (UIN2Contact(uin)->LastMessage != NULL) {
                         M_print("Last message from " CONTACTCOL "%s"
                                 NOCOL ":\n", UIN2Contact(uin)->nick);
                         M_print(MESSCOL "%s",
                                 UIN2Contact(uin)->LastMessage);
                         M_print(NOCOL " \n");
                     } else {
#ifdef FUNNY_MSGS
						 M_print(CONTACTCOL "%s" NOCOL " hasn't had anything "
						 		 "intelligent to say today.\n", 
								 UIN2Contact(uin)->nick);
#else
                         M_print("No messages received from " CONTACTCOL
						 		 "%s\n" NOCOL, UIN2Contact(uin)->nick);
#endif
                     }
                 }
             }
		 } else if (strcasecmp(cmd, "uptime") == 0) {
		 	double	TimeDiff = difftime(time(NULL), MicqStartTime);
			int		Days, Hours, Minutes, Seconds;

			Seconds = (int)TimeDiff % 60;
			TimeDiff = TimeDiff / 60.0;
			Minutes = (int)TimeDiff % 60;
			TimeDiff = TimeDiff / 60.0;
			Hours = (int)TimeDiff % 24;
			TimeDiff = TimeDiff / 24.0;
			Days = TimeDiff;

			M_print("Micq has been running for ");
			if (Days != 0)
				M_print(MESSCOL "%02d " NOCOL "days, ", Days);
			if (Hours != 0)
				M_print(MESSCOL "%02d " NOCOL "hours, ", Hours);
			if (Minutes != 0)
				M_print(MESSCOL "%02d " NOCOL "minutes, ", Minutes);
			M_print(MESSCOL "%02d " NOCOL "seconds.\n", Seconds);
			M_print( "Contacts " MESSCOL "%d" NOCOL " / %d\n",Num_Contacts ,MAX_CONTACTS );
			M_print( "Packets Sent : " MESSCOL );
			M_print( "%d" NOCOL "\tPackets Recieved : " MESSCOL "%d" NOCOL "\t", Packets_Sent, Packets_Recv );
			if ( Packets_Sent | Packets_Recv ) {
				M_print( "Lag : " MESSCOL );
				M_print( "%2.2f" NOCOL " %%\n", abs( Packets_Sent - Packets_Recv ) * ( 200.0 / ( Packets_Sent + Packets_Recv ) ) );
			}
			M_print( "Distinct Packets Sent : " MESSCOL );
			M_print( "%d" NOCOL "\tDistinct Packets Recieved : " MESSCOL "%d" NOCOL "\n", real_packs_sent, real_packs_recv );
		 } else if (strcasecmp(cmd, "wide") == 0) {
		 	PrintContactWide();
         } /* end of aaron's code */
         else 
         {  M_print( UNKNOWN_COMMAND_STR "\n", cmd ); }
      }
   }
   multi_uin = last_uin;
   if ( ( status == 0 ) || ( status == 2) )
   {
      if ( ! Quit )
#ifndef USE_MREADLINE
         Prompt();
#else
         Prompt();
#endif
   }
}

/**************************************************************
most detailed contact list display
***************************************************************/
static void Show_Status( char * name )
{
   int i;
   DWORD num;
   CONTACT_PTR cont;

   if ( name != NULL ) {
	num = nick2uin( name );
	if ( num == -1 ) {
		M_print( "Must give a valid uin/nickname\n" );
		return;
	}
 	cont = UIN2Contact( num );	
	if ( cont == NULL ) {
		M_print( "%s is not a valid user in your list.\n", name );
		return;
	}
           if ( cont->vis_list ) {
        	  M_print( "%s*%s", SERVCOL, NOCOL );
           } else if (cont->invis_list ) {
		M_print( SERVCOL "-" NOCOL );
           } else {
      		 M_print( " " );
           }
           M_print( "%6ld=", cont->uin );
           M_print( CONTACTCOL "%-20s\t%s(", cont->nick, MESSCOL );
           Print_Status( cont->status );
           M_print( ")%s\n", NOCOL );
	   if ( cont->status == STATUS_OFFLINE ) {
          	 if ( -1L != cont->last_time ) {
         	         M_print( W_LAST_ONLINE_STR , ctime( (time_t *) &cont->last_time ) );
          	 } else {
        		  M_print( W_LAST_ON_UNKNOWN_STR "\n" );
           	}
	   } else {
               if ( -1L != cont->last_time )
               {
                     M_print( W_ONLINE_SINCE_STR, ctime( (time_t *) &cont->last_time )  );
               } else {
        	  M_print( W_LAST_ON_UNKNOWN_STR "\n" );
		}
	   }
	M_print( "IP : %d.%d.%d.%d : %d\n", cont->current_ip[0], cont->current_ip[1], cont->current_ip[2], cont->current_ip[3], cont->port );
	M_print( "TCP Version : %d\t%s\n", cont->TCP_version, cont->connection_type == 4 ? "Peer-to-Peer mode" : "Server Only Communication" );
	return;
   }
   M_print( W_SEPERATOR );
   M_print( W_STATUS_STR );
   Print_Status( Current_Status );
   M_print( "\n" );
   /*  First loop sorts thru all offline users */
   M_print( W_SEPERATOR );
   M_print( W_OFFLINE_STR "\n" );
   for ( i=0; i< Num_Contacts; i++ )
   {
      if ( ( S_DWORD )Contacts[ i ].uin > 0 )
      {
         if ( FALSE == Contacts[ i ].invis_list )
	 {
            if ( Contacts[ i ].status == STATUS_OFFLINE )
            {
               if ( Contacts[i].vis_list ) {
        	  M_print( "%s*%s", SERVCOL, NOCOL );
               } else {
        	  M_print( " " );
               }
               M_print( "%8ld=", Contacts[ i ].uin );
               M_print( CONTACTCOL "%-20s\t%s(", Contacts[ i ].nick, MESSCOL );
               Print_Status( Contacts[ i ].status );
               M_print( ")%s", NOCOL );
               if ( -1L != Contacts[ i ].last_time )
               {
                  M_print( W_LAST_ONLINE_STR , ctime( (time_t *) &Contacts[ i ].last_time ) );
               }
               else
               {
        	  M_print( W_LAST_ON_UNKNOWN_STR "\n" );
        	  /* if time is unknow they can't be logged on cause we */
        	  /* set the time at login */
               }
            }
	 }
      }
   }
   /* The second loop displays all the online users */
   M_print( W_SEPERATOR );
   M_print( W_ONLINE_STR "\n" );
   for ( i=0; i< Num_Contacts; i++ )
   {
      if ( ( S_DWORD )Contacts[ i ].uin > 0 )
      {
         if ( FALSE == Contacts[ i ].invis_list )
	 {
            if ( Contacts[ i ].status != STATUS_OFFLINE )
            {
               if ( Contacts[i].vis_list ) {
        	  M_print( "%s*%s", SERVCOL, NOCOL );
               } else {
        	  M_print( " " );
               }
               M_print( "%8ld=", Contacts[ i ].uin );
               M_print( CONTACTCOL "%-20s\t%s(", Contacts[ i ].nick, MESSCOL );
               Print_Status( Contacts[ i ].status );
               M_print( ")%s", NOCOL );
               if ( -1L != Contacts[ i ].last_time )
               {
        	  if ( Contacts[ i ].status  == STATUS_OFFLINE )
                     M_print( W_LAST_ONLINE_STR, ctime( (time_t *) &Contacts[ i ].last_time ) );
        	  else
                     M_print( W_ONLINE_SINCE_STR, ctime( (time_t *) &Contacts[ i ].last_time )  );
               }
               else
               {
        	  M_print( W_LAST_ON_UNKNOWN_STR "\n" );
        	  /* if time is unknow they can't be logged on cause we */
        	  /* set the time at login */
               }
            }
	 }
      }
   }
   M_print( W_SEPERATOR );
}

/***************************************************************
nice clean "w" display
****************************************************************/
void Show_Quick_Status( void )
{
   int i;
   
   M_print( "" W_SEPERATOR );
   M_print( "%lu: ", UIN );
   M_print( W_STATUS_STR );
   Print_Status( Current_Status );
   M_print( "\n" );
   M_print( W_SEPERATOR );
   M_print( W_OFFLINE_STR "\n" );
   /*  First loop sorts thru all offline users */
   /*  This comes first so that if there are many contacts */
   /*  The online ones will be less likely to scroll off the screen */
   for ( i=0; i< Num_Contacts; i++ )
   {
      if ( ( S_DWORD )Contacts[ i ].uin > 0 )
      {
         if ( FALSE == Contacts[ i ].invis_list )
	 {
            if ( Contacts[ i ].status == STATUS_OFFLINE )
            {
               if ( Contacts[i].vis_list ) {
        	  M_print( "%s*%s", SERVCOL, NOCOL );
               } else {
        	  M_print( " " );
               }
               M_print( CONTACTCOL "%-20s\t" MESSCOL "(" , Contacts[ i ].nick );
               Print_Status( Contacts[ i ].status );
               M_print( ")" NOCOL "\n" );
            }
	 }
      }
   }
   /* The second loop displays all the online users */
   M_print( W_SEPERATOR );
   M_print( W_ONLINE_STR "\n" );
   for ( i=0; i< Num_Contacts; i++ )
   {
      if ( ( S_DWORD )Contacts[ i ].uin > 0 )
      {
         if ( FALSE == Contacts[ i ].invis_list )
	 {
            if ( Contacts[ i ].status != STATUS_OFFLINE )
            {
               if ( Contacts[i].vis_list ) {
        	  M_print( "%s*%s", SERVCOL, NOCOL );
               } else {
        	  M_print( " " );
               }
               M_print( CONTACTCOL "%-20s\t" MESSCOL "(", Contacts[ i ].nick );
               Print_Status( Contacts[ i ].status );
               M_print( ")" NOCOL "\n" );
            }
	 }
      }
   }
   M_print( W_SEPERATOR );
}

/***************************************************************
nice clean "i" display
****************************************************************/
void Show_Ignore_Status( void )
{
   int i;
   
   M_print( "" W_SEPERATOR );
   M_print( I_IGNORED_STR "\n" );
   /*  Sorts thru all ignored users */
   for ( i=0; i< Num_Contacts; i++ )
   {
      if ( ( S_DWORD )Contacts[ i ].uin > 0 )
      {
         if ( TRUE == Contacts[ i ].invis_list )
	       {
           if ( Contacts[i].vis_list ) {
        	  M_print( "%s*%s", SERVCOL, NOCOL );
             } else {
        	  M_print( " " );
             }
           M_print( CONTACTCOL "%-20s\t" MESSCOL "(" , Contacts[ i ].nick );
           Print_Status( Contacts[ i ].status );
           M_print( ")" NOCOL "\n" );
	       }
      }
   }
   M_print( W_SEPERATOR );
}

/***************************************************************
nicer cleaner "e" display :}
****************************************************************/
void Show_Quick_Online_Status( void )
{
   int i;
   
   M_print( "" W_SEPERATOR );
   M_print( "%lu: ", UIN );
   M_print( W_STATUS_STR );
   Print_Status( Current_Status );
   M_print( "\n" );
   M_print( W_SEPERATOR );

   /* Loop displays all the online users */
   M_print( W_ONLINE_STR "\n" );
   for ( i=0; i< Num_Contacts; i++ )
   {
      if ( ( S_DWORD )Contacts[ i ].uin > 0 )
      {
         if ( FALSE == Contacts[ i ].invis_list )
	 {
            if ( Contacts[ i ].status != STATUS_OFFLINE )
            {
               if ( Contacts[i].vis_list ) {
        	  M_print( "%s*%s", SERVCOL, NOCOL );
               } else {
        	  M_print( " " );
               }
               M_print( CONTACTCOL "%-20s\t" MESSCOL "(", Contacts[ i ].nick );
               Print_Status( Contacts[ i ].status );
               M_print( ")" NOCOL "\n" );
            }
	 }
      }
   }
   M_print( W_SEPERATOR );
}


/***************************************************
Do not call unless you've already made a call to strtok()
****************************************************/
static void Change_Function( SOK_T sok )
{
   char *arg1;
   arg1 = strtok( NULL, " \n\r" );
   if ( arg1 == NULL )
   {
      M_print(  CLIENTCOL "Status modes: \n" );
      M_print( "Status %s\t%d\n", STATUS_ONLINE_STR, STATUS_ONLINE );
      M_print( "Status %s\t%d\n", STATUS_AWAY_STR, STATUS_AWAY );
      M_print( "Status %s\t%d\n", STATUS_DND_STR, STATUS_DND );
      M_print( "Status %s\t%d\n", STATUS_NA_STR, STATUS_NA );
      M_print( "Status %s\t%d\n", STATUS_FFC_STR, STATUS_FREE_CHAT );
      M_print( "Status %s\t%d\n", STATUS_OCCUPIED_STR, STATUS_OCCUPIED );
      M_print( "Status %s\t%d", STATUS_INVISIBLE_STR, STATUS_INVISIBLE );
      M_print( NOCOL "\n" );
   }
   else
   {
      icq_change_status( sok, atoi( arg1 ) );
      Print_Status( Current_Status );
      M_print( "\n" );
   }
}

/***************************************************
Do not call unless you've already made a call to strtok()
****************************************************/
static void Random_Function( SOK_T sok )
{
   char *arg1;
   arg1 = strtok( NULL, " \n\r" );
   if ( arg1 == NULL )
   {
      M_print(  CLIENTCOL "Groups: \n" );
      M_print( "General                    1\n" );
      M_print( "Romance                    2\n" );
      M_print( "Games                      3\n" );
      M_print( "Students                   4\n" );
      M_print( "20 something               6\n" );
      M_print( "30 something               7\n" );
      M_print( "40 something               8\n" );
      M_print( "50+                        9\n" );
      M_print( "Man chat requesting women 10\n" );
      M_print( "Woman chat requesting men 11\n" );
      M_print( "Micq                      49 (might not work but try it)" );
      M_print( NOCOL "\n" );
   }
   else
   {
      icq_rand_user_req( sok, atoi( arg1 ) );
/*      M_print( "\n" );*/
   }
}

/***************************************************
Do not call unless you've already made a call to strtok()
****************************************************/
static void Random_Set_Function( SOK_T sok )
{
   char *arg1;
   arg1 = strtok( NULL, " \n\r" );
   if ( arg1 == NULL )
   {
      M_print(  CLIENTCOL "Groups: \n" );
      M_print( "None                      -1\n" );
      M_print( "General                    1\n" );
      M_print( "Romance                    2\n" );
      M_print( "Games                      3\n" );
      M_print( "Students                   4\n" );
      M_print( "20 something               6\n" );
      M_print( "30 something               7\n" );
      M_print( "40 something               8\n" );
      M_print( "50+                        9\n" );
      M_print( "Man chat requesting women 10\n" );
      M_print( "Woman chat requesting men 11\n" );
      M_print( "Micq                      49 (might not work but try it)" );
      M_print( NOCOL "\n" );
   }
   else
   {
      icq_rand_set( sok, atoi( arg1 ) );
/*      M_print( "\n" );*/
   }
}

/*****************************************************************
Displays help information.
******************************************************************/
static void Help_Function( void )
{
   char *arg1;
   arg1 = strtok( NULL, " \n\r" );
   
   if ( arg1 == NULL ){
       M_print( CLIENTCOL SELECT_GROUP_STR "\n" );
       M_print( CLIENT_CAT_STR "\t-\t" CLIENT_HELP_STR "\n" );
       M_print( MESSAGE_CAT_STR "\t-\t" MESSAGE_HELP_STR "\n" );
       M_print( USER_CAT_STR "\t-\t" USER_HELP_STR "\n" );
       M_print( ACCOUNT_CAT_STR "\t-\t" ACCOUNT_HELP_STR "\n" );
   } else if ( !strcasecmp( arg1, CLIENT_CAT_STR ) ){
	    M_print( MESSCOL "verbose #" NOCOL VERBOSE_HELP_STR);
            M_print( MESSCOL "%s" NOCOL CLEAR_HELP_STR, clear_cmd );
            M_print( MESSCOL "%s" NOCOL SOUND_HELP_STR, sound_cmd);
            M_print( MESSCOL "%s" NOCOL COLOR_HELP_STR, color_cmd);
            M_print( MESSCOL "%s\t" NOCOL QUIT_HELP_STR,quit_cmd );
            M_print( MESSCOL "%s" NOCOL AUTO1_HELP_STR,auto_cmd );
            M_print( MESSCOL "%s [on|off]" NOCOL AUTO2_HELP_STR,auto_cmd );
            M_print( MESSCOL "%s <status> <message>" NOCOL AUTO3_HELP_STR,auto_cmd );
            M_print( MESSCOL "%s <old cmd> <new cmd>" NOCOL ALTER_HELP_STR ,alter_cmd);
	    M_print( CLIENTCOL "\t! as the first character of a command will execute\n" );
	    M_print( CLIENTCOL "\ta shell command (e.g. \"!ls\"  \"!dir\" \"!mkdir temp\")" NOCOL "\n" );
   } else if ( !strcasecmp( arg1,MESSAGE_CAT_STR ) ){
            M_print( MESSCOL "%s <uin>" NOCOL AUTH_HELP_STR, auth_cmd );
            M_print( MESSCOL "%s <uin>/<message>" NOCOL MSG_HELP_STR, message_cmd );
            M_print( MESSCOL "%s <uin> <url> <message>" NOCOL URL_HELP_STR, url_cmd );
            M_print( MESSCOL "%s\t\t" NOCOL MSGA_HELP_STR, msga_cmd );
            M_print( MESSCOL "%s <message>" NOCOL AGAIN_HELP_STR, again_cmd);
            M_print( MESSCOL "%s <message>" NOCOL REPLY_HELP_STR, reply_cmd);
            M_print( MESSCOL "%s <nick>" NOCOL LAST_HELP_STR, "last");
            M_print( MESSCOL "uptime\t\tShows how long Micq has been "
					"running.\n" NOCOL);
	    M_print( CLIENTCOL "\tuin can be either a number or the nickname of the user.\n" );
            M_print( CLIENTCOL "\tSending a blank message will put the client into multiline mode.\n\tUse . on a line by itself to end message.\n" );
	    M_print( "\tUse # on a line by itself to cancel the message." NOCOL "\n" );
   } else if ( !strcasecmp( arg1, USER_CAT_STR ) ){
            M_print( MESSCOL "%s <uin>" NOCOL AUTH_HELP_STR, auth_cmd );
	    M_print( MESSCOL "%s [#]" NOCOL RAND_HELP_STR, rand_cmd );
	    M_print( MESSCOL "pass <secret>" NOCOL PASS_HELP_STR );
            M_print( MESSCOL "%s" NOCOL LIST_HELP_STR,list_cmd);
            M_print( MESSCOL "%s [user]" NOCOL STATUS_HELP_STR,status_cmd);
            M_print( MESSCOL "%s" NOCOL ONLINE_LIST_HELP_STR,online_list_cmd);
            M_print( MESSCOL "%s <uin>" NOCOL INFO_HELP_STR, info_cmd );
            M_print( MESSCOL "%s <nick>" NOCOL TOGIG_HELP_STR, togig_cmd );
            M_print( MESSCOL "%s\t" NOCOL IGLIST_HELP_STR, iglist_cmd );
            M_print( MESSCOL "%s [email@host]" NOCOL SEARCH_HELP_STR, search_cmd );
            M_print( MESSCOL "%s <uin> <nick>" NOCOL ADD_HELP_STR, add_cmd );
            M_print( MESSCOL "%s <nick>" NOCOL TOGVIS_HELP_STR, togvis_cmd );
   } else if ( !strcasecmp( arg1, ACCOUNT_CAT_STR ) ){
            M_print( MESSCOL "%s [#]" NOCOL CHANGE_HELP_STR,change_cmd);
	    M_print( MESSCOL "reg password" NOCOL REG_HELP_STR );
	    M_print( MESSCOL "%s" NOCOL ONLINE_HELP_STR,online_cmd);
	    M_print( MESSCOL "%s" NOCOL AWAY_HELP_STR,away_cmd);
            M_print( MESSCOL "%s" NOCOL NA_HELP_STR,na_cmd);
            M_print( MESSCOL "%s" NOCOL OCC_HELP_STR,occ_cmd);
            M_print( MESSCOL "%s" NOCOL DND_HELP_STR,dnd_cmd);
            M_print( MESSCOL "%s" NOCOL FFC_HELP_STR,ffc_cmd);
            M_print( MESSCOL "%s" NOCOL INV_HELP_STR,inv_cmd);
	    M_print( MESSCOL "%s" NOCOL UPDATE_HELP_STR,update_cmd);
	    M_print( MESSCOL "other" NOCOL UPDATE_OTHER_HELP_STR);
	    M_print( MESSCOL "%s" NOCOL ABOUT_HELP_STR,about_cmd);
	    M_print( MESSCOL "set [#]" NOCOL SET_RAND_HELP_STR );
   }
}

/****************************************************
Retrieves info a certain user
*****************************************************/
static void Info_Function( SOK_T sok )
{
   char * arg1;

   arg1 = strtok( NULL, "" );
   if ( arg1 == NULL )
   {
      M_print( "Need uin to send to\n" );
      return;
   }
   uin = nick2uin( arg1 );
   if ( -1 == uin )
   {
      M_print( NICK_NOT_FOUND_STR "\n", arg1 );
      return;
   }
   M_print( "%s's IP address is ", arg1 );
   Print_IP( uin );
   if ( (WORD) Get_Port( uin ) != (WORD) 0xffff ) {
      M_print( "\tThe port is %d\n",(WORD) Get_Port( uin ) );
   } else {
      M_print( "\tThe port is unknown\n" );
   }
   send_info_req( sok, uin );
/*   send_ext_info_req( sok, uin );*/
}

/**********************************************************
Handles automatic reply messages
***********************************************************/
static void Auto_Function( SOK_T sok )
{
   char *cmd;
   char *arg1;
   
   cmd = strtok( NULL, "" );
   if ( cmd == NULL )
   {
      M_print( "Automatic replies are %s\n", auto_resp ? "On" : "Off" );
      M_print( "The Do not disturb message is: %s\n", auto_rep_str_dnd );
      M_print( "The Away message is          : %s\n", auto_rep_str_away );
	   M_print( "The Not available message is  : %s\n", auto_rep_str_na );
 	   M_print( "The Occupied message is      : %s\n", auto_rep_str_occ );
 	   M_print( "The Invisible message is     : %s\n", auto_rep_str_inv );
 	   return;
   }
   else if ( strcasecmp( cmd, "on" ) == 0 )
   {
      auto_resp = TRUE;
      M_print( "Automatic replies are on.\n" );
   }
   else if ( strcasecmp( cmd, "off" ) == 0 )
   {
      auto_resp = FALSE;
      M_print( "Automatic replies are off.\n" );
   }
   else
   {
       arg1 = strtok( cmd," ");
 	   if (arg1 == NULL)
 	   {
 		   M_print( "Sorry wrong syntax, can't find a status somewhere.\r\n");
         return;
 	   }
 		if ( ! strcasecmp (arg1 , dnd_cmd ) )
 		{
 			 cmd = strtok( NULL,"");
			 if ( cmd == NULL ) {
				M_print( "Must give a message.\n" );
				return;
			 }
 			 strcpy( auto_rep_str_dnd, cmd);
 		}
 		else if ( !strcasecmp (arg1 ,away_cmd ))
 	   {
 		  cmd = strtok( NULL,"");
			 if ( cmd == NULL ) {
				M_print( "Must give a message.\n" );
				return;
			 }
 		  strcpy( auto_rep_str_away,cmd);
 	   }
 	   else if ( !strcasecmp (arg1,na_cmd))
 	   {
 		    cmd =strtok(NULL,"");
			 if ( cmd == NULL ) {
				M_print( "Must give a message.\n" );
				return;
			 }
 		    strcpy(auto_rep_str_na,cmd);
 	   }
 	   else if ( !strcasecmp (arg1,occ_cmd))
 	   {
 		    cmd = strtok(NULL,"");
			 if ( cmd == NULL ) {
				M_print( "Must give a message.\n" );
				return;
			 }
 		    strcpy (auto_rep_str_occ,cmd);
 	   }
 	   else if (!strcasecmp (arg1,inv_cmd))
 	   {
 		   cmd = strtok(NULL,"");
			 if ( cmd == NULL ) {
				M_print( "Must give a message.\n" );
				return;
			 }
 		   strcpy (auto_rep_str_inv,cmd);
 	   }
 	   else
 		   M_print ("Sorry wrong syntax. Read tha help man!\n");
       M_print( "Automatic reply setting\n" );

 	}
}

/*************************************************************
Alters one of the commands
**************************************************************/
static void Alter_Function( void )
{
   char * cmd;
   
   cmd = strtok (NULL," ");
   if ( cmd == NULL )
   {
     M_print( "Need a command to alter!\n" );
     return;
   }
   if ( ! strcasecmp(cmd, auto_cmd))
     strncpy(auto_cmd,strtok(NULL," \n\t"),16);
   else if ( !strcasecmp(cmd ,message_cmd))
     strncpy(message_cmd,strtok(NULL," \n\t"),16);
   else if (!strcasecmp(cmd,add_cmd))
     strncpy(add_cmd,strtok(NULL," \n\t"),16);
   else if (!strcasecmp(cmd,togvis_cmd))
     strncpy(togvis_cmd,strtok(NULL," \n\t"),16);
   else if (!strcasecmp(cmd,info_cmd))
     strncpy(info_cmd,strtok(NULL," \n\t"),16);
   else if (!strcasecmp(cmd,togig_cmd))
     strncpy(togig_cmd,strtok(NULL," \n\t"),16);
   else if (!strcasecmp(cmd,iglist_cmd))
     strncpy(iglist_cmd,strtok(NULL," \n\t"),16);
   else if (!strcasecmp(cmd,quit_cmd))
     strncpy(quit_cmd,strtok(NULL," \n\t"),16);
   else if (!strcasecmp(cmd,reply_cmd))
        strncpy(reply_cmd,strtok(NULL," \n\t"),16);
   else if (!strcasecmp(cmd,again_cmd))
           strncpy(again_cmd,strtok(NULL," \n\t"),16);
   else if (!strcasecmp(cmd,list_cmd))
           strncpy(list_cmd,strtok(NULL," \n\t"),16);
   else if (!strcasecmp(cmd,online_list_cmd))
           strncpy(online_list_cmd,strtok(NULL," \n\t"),16);
   else if (!strcasecmp(cmd,away_cmd))
           strncpy(away_cmd,strtok(NULL," \n\t"),16);
   else if (!strcasecmp(cmd,na_cmd))
           strncpy(na_cmd,strtok(NULL," \n\t"),16);
   else if (!strcasecmp(cmd,dnd_cmd))
           strncpy(dnd_cmd,strtok(NULL," \t\n"),16);
   else if (!strcasecmp(cmd,online_cmd))
           strncpy(online_cmd,strtok(NULL," \n\t"),16);
   else if (!strcasecmp(cmd,occ_cmd))
           strncpy(occ_cmd,strtok(NULL," \t\n"),16);
   else if (!strcasecmp(cmd,ffc_cmd))
           strncpy(ffc_cmd,strtok(NULL," \t\n"),16);
   else if (!strcasecmp(cmd,inv_cmd))
            strncpy(inv_cmd,strtok(NULL," \t\n"),16);
   else if (!strcasecmp(cmd,status_cmd))
           strncpy(status_cmd,strtok(NULL," \t\n"),16);
   else if (!strcasecmp(cmd,auth_cmd))
           strncpy(status_cmd,strtok(NULL," \n\t"),16);
   else if (!strcasecmp(cmd,change_cmd))
           strncpy(change_cmd,strtok(NULL," \n\t"),16);
   else if (!strcasecmp(cmd,search_cmd))
           strncpy(search_cmd,strtok(NULL," \t\n"),16);
   else if (!strcasecmp(cmd,save_cmd))
           strncpy(save_cmd,strtok(NULL," \t\n"),16);
   else if (!strcasecmp(cmd,alter_cmd))
           strncpy(alter_cmd,strtok(NULL," \t\n"),16);
   else if (!strcasecmp(cmd,msga_cmd))
          strncpy(msga_cmd,strtok(NULL," \n\t"),16);
   else if (!strcasecmp(cmd,about_cmd))
          strncpy(about_cmd,strtok(NULL," \n\t"),16);
   else
     M_print("Type help to see your current command, because this  one you typed wasn't one!\n");
}

/*************************************************************
Processes user input to send a message to a specified user
**************************************************************/
static void Message_Function( SOK_T sok )
{
   char * arg1;
   
   arg1 = strtok( NULL, UIN_DELIMS );
   if ( arg1 == NULL )
   {
      M_print( "Need uin to send to\n" );
      return;
   }
   uin = nick2uin( arg1 );
   if ( -1 == uin )
   {
      M_print( NICK_NOT_FOUND_STR "\n", arg1 );
      return;
   }
   arg1 = strtok( NULL, "" );
   last_uin = uin;
   add_tab( uin );
   if ( arg1 != NULL )
   {
      icq_sendmsg( sok, uin, arg1, NORM_MESS );
      M_print( MESSAGE_SENT_1_STR );
      Print_UIN_Name( last_uin );
      M_print( MESSAGE_SENT_2_STR );
   }
   else
   {
      status = 1;
      /* aaron
       Tell me that I'm sending a message to someone. The reason being that
       I have sent a reply to the wrong person far too many times. :)       */
      if (UIN2nick(last_uin) != NULL)
          M_print("Composing message to " CONTACTCOL "%s" NOCOL ":\n",
                  UIN2nick(last_uin));
      else
          M_print("Composing message to " CLIENTCOL "%d" NOCOL ":\n",
                  last_uin);
      /* end of aaron */
      R_doprompt( MSG_PROMPT_STR );
   }
}

/*******************************************************
Sends a reply message to the last person to message you.
********************************************************/
static void Reply_Function( SOK_T sok )
{
   char * arg1;
   
   if ( last_recv_uin == 0 )
   {
      M_print( "Must receive a message first\n" );
      return;
   }
   arg1 = strtok( NULL, "" );
   last_uin = last_recv_uin;
   add_tab( last_recv_uin );
   if ( arg1 != NULL )
   {
      icq_sendmsg( sok, last_recv_uin, arg1, NORM_MESS );
      M_print( MESSAGE_SENT_1_STR );
      Print_UIN_Name( last_recv_uin );
      M_print( MESSAGE_SENT_2_STR );
   }
   else
   {
      status = 1;
      /* aaron
       Tell me that I'm sending a message to someone. The reason being that
       I have sent a reply to the wrong person far too many times. :)       */
      if (UIN2nick(last_uin) != NULL)
          M_print("Composing message to " CONTACTCOL "%s" NOCOL ":\n",
                  UIN2nick(last_uin));
      else
          M_print("Composing message to " CLIENTCOL "%d" NOCOL ":\n",
                  last_uin);
      /* end of aaron */
      R_doprompt( MSG_PROMPT_STR );
   }
}

static void Again_Function( SOK_T sok )
{
   char * arg1;
   
   if ( last_uin == 0 )
   {
      M_print( "Must write one message first\n" );
      return;
   }
   add_tab( last_uin );
   arg1 = strtok( NULL, "" );
   if ( arg1 != NULL )
   {
      icq_sendmsg( sok, last_uin, arg1, NORM_MESS );
      M_print( MESSAGE_SENT_1_STR );
      Print_UIN_Name( last_uin );
      M_print( MESSAGE_SENT_2_STR );					 
   } else {
      status = 1;
      /* aaron
       Tell me that I'm sending a message to someone. The reason being that
       I have sent a reply to the wrong person far too many times. :)       */
      if (UIN2nick(last_uin) != NULL)
          M_print("Composing message to " CONTACTCOL "%s" NOCOL ":\n",
                  UIN2nick(last_uin));
      else
          M_print("Composing message to " CLIENTCOL "%d" NOCOL ":\n",
                  last_uin);
      /* end of aaron */
      R_doprompt( MSG_PROMPT_STR );
   }
}

static void Verbose_Function( void )
{
   char * arg1;
   
   arg1 = strtok( NULL, "" );
   if ( arg1 != NULL )
   {
      Verbose = atoi( arg1 );
   }
   M_print( VERBOSE_LEVEL_STR, Verbose );
}
