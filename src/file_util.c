#include "micq.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef _WIN32
   #include <io.h>
   #define S_IRUSR        _S_IREAD
   #define S_IWUSR        _S_IWRITE
#else
    #ifndef __BEOS__
    	#include <arpa/inet.h>
    #endif
    #include <sys/time.h>
    #include <netinet/in.h>
    #include <termios.h>
#endif

#ifdef UNIX
   #include <unistd.h>
#endif


/****/

BOOL Log_Dir_Normal( void );
char * Get_Log_Dir( void );

#define      ADD_COMMAND(a, b)     else if ( ! strcasecmp( tmp, a) )   \
          {                                                          \
              strncpy( b,strtok(NULL," \n\t"), 16 );                  \
          }                                                          

#define      ADD_MESS(a, b)     else if ( ! strcasecmp( tmp, a) )   \
          {                                                          \
              strncpy( b,strtok(NULL,"\n"), 450 );                  \
          }                                                          

static char rcfile[256];

void Set_rcfile( char * name )
{
   char *path;
   char *home;

	if ( NULL == name ) {
		
#ifdef _WIN32
   path = ".\\";
#endif

#ifdef __amigaos__
   path = "PROGDIR:";
#endif

#ifdef UNIX
   home = getenv ("HOME");
   if (!home) home = "";
   path = malloc( strlen( home ) + 2 );
   strcpy( path, home );
   if ( path[ strlen( path ) - 1 ] != '/' )
      strcat( path, "/" );
#endif

   strcpy( rcfile, path );
   strcat( rcfile, ".micqrc" );

	} else {
		strncpy( rcfile, name, 256 );
		name[255] = 0;
	}
}

/************************************************************************
 Copies src string into dest.  If src is NULL dest becomes ""
*************************************************************************/
static void M_strcpy( char * dest, char * src )
{
	if ( NULL == src ) {
		dest = "";
	} else {
		strcpy( dest, src );
	}
}

static void Initalize_RC_File( void )
{
   FD_T rcf;
   char passwd2[ sizeof(passwd) ];
   strcpy( server, "icq1.mirabilis.com" );
   remote_port = 4000;

   away_time = default_away_time;

   M_print( "Enter UIN or 0 for new UIN: #" );
   fflush( stdout );
   scanf( "%ld", &UIN );
password_entry:
   M_print( "Enter password : " );
   fflush( stdout );
   Echo_Off();
   memset( passwd, 0, sizeof( passwd ) );
   M_fdnreadln(STDIN, passwd, sizeof(passwd));
   Echo_On();
   if ( UIN == 0 )
   {
      if ( 0 == passwd[0] )
      {
         M_print( "\nMust enter password!\n" );
         goto password_entry;
      }
      M_print( "\nReenter password to verify: " );
      fflush( stdout );
      Echo_Off();
      memset( passwd2, 0, sizeof( passwd2 ) );
      M_fdnreadln(STDIN, passwd2, sizeof(passwd));
      Echo_On();
      if ( strcmp( passwd, passwd2 ) )
      {
         M_print( "\nPasswords did not match reenter\n" );
         goto password_entry;
      }
      Init_New_User();
   }

/* SOCKS5 stuff begin */
   M_print( "\nSOCKS5 server hostname or IP (type 0 if you don't want to use this) : " );
   fflush( stdout );
   scanf( "%s", s5Host );
   if( strlen( s5Host ) > 1 )
   {
     s5Use=1;
     M_print( "SOCKS5 port (in general 1080) : " );
     fflush( stdout );
     scanf( "%hu", &s5Port );

     M_print( "SOCKS5 username (type 0 if you don't need authentification) : " );
     fflush( stdout );
     scanf( "%s", s5Name );
     if( strlen( s5Name) > 1 )
     {
       s5Auth=1;
       M_print( "SOCKS5 password : " );
       fflush( stdout );
       scanf( "%s", s5Pass );
     }
     else
     {
       s5Auth=0;
       strcpy( s5Name, "0");
       strcpy( s5Pass, "0");
     }
   }
   else
     s5Use=0;
/* SOCKS5 stuff end */

   set_status = STATUS_ONLINE;
   Num_Contacts = 2;
   Contacts[ 0 ].vis_list = FALSE;
   Contacts[ 1 ].vis_list = FALSE;
   Contacts[0].uin = 11290140;
   strcpy( Contacts[0].nick, "Micq Author" );
   Contacts[0].status = STATUS_OFFLINE;
   Contacts[0].last_time = -1L;
   Contacts[0].current_ip[0] = 0xff;
   Contacts[0].current_ip[1] = 0xff;
   Contacts[0].current_ip[2] = 0xff;
   Contacts[0].current_ip[3] = 0xff;
   Contacts[ 0 ].port = 0;
   Contacts[ 0 ].sok = (SOK_T ) -1L;
   Contacts[1].uin = -11290140;
   strcpy( Contacts[1].nick, "alias1" );
   Contacts[1].status = STATUS_OFFLINE;
   Contacts[1].current_ip[0] = 0xff;
   Contacts[1].current_ip[1] = 0xff;
   Contacts[1].current_ip[2] = 0xff;
   Contacts[1].current_ip[3] = 0xff;
   Contacts[1].current_ip[0] = 0xff;
   Contacts[1].current_ip[1] = 0xff;
   Contacts[1].current_ip[2] = 0xff;
   Contacts[1].current_ip[3] = 0xff;
   Contacts[ 1 ].port = 0;
   Contacts[ 1 ].sok = (SOK_T ) -1L;
   
    strcpy(clear_cmd, "clear");
    strcpy(message_cmd, "msg");
    strcpy(info_cmd, "info");
    strcpy(add_cmd, "add");
    strcpy(togvis_cmd, "togvis");
    strcpy(quit_cmd, "q");
    strcpy(reply_cmd, "r");       
    strcpy(again_cmd, "a");
    strcpy(list_cmd, "w");
    strcpy(online_list_cmd, "e");
    strcpy(away_cmd, "away");
    strcpy(na_cmd, "na");
    strcpy(dnd_cmd, "dnd");   
    strcpy(online_cmd, "online");
    strcpy(occ_cmd, "occ");
    strcpy(ffc_cmd, "ffc");
    strcpy(inv_cmd, "inv");
    strcpy(status_cmd, "status");
    strcpy(again_cmd, "a");
    strcpy(auth_cmd, "auth");
    strcpy(change_cmd, "change");
    strcpy(auto_cmd, "auto");
    strcpy(search_cmd, "search");
    strcpy(save_cmd, "save");
    strcpy(alter_cmd, "alter");
    strcpy(msga_cmd, "msga");
    strcpy(rand_cmd, "rand");
    strcpy(about_cmd, "about");
    strcpy(color_cmd, "color");
    strcpy(sound_cmd, "sound");
    strcpy(url_cmd, "url");
    strcpy(update_cmd, "update");
    strcpy(togig_cmd, "togig");
    strcpy(iglist_cmd, "i");
   
   Current_Status = STATUS_ONLINE;
   
   rcf = open( rcfile, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
   if ( rcf == -1 )
   {
      perror( "Error creating config file " );
      exit( 1);
   }
   close( rcf );

   if ( Save_RC() == -1 )
   {
      perror( "Error creating config file " );
      exit( 1);
   }
}

static void Read_RC_File( FD_T rcf )
{
   char buf[450];
   char *tmp;
   int i;
   DWORD tmp_uin;
   char *tab_nick_spool[TAB_SLOTS];
   int spooled_tab_nicks;
   
   clear_cmd[0]='\0';/* for error checking later */
   message_cmd[0]='\0';/* for error checking later */
   quit_cmd[0]='\0';   /* for error checking later */
   info_cmd[0]='\0';   /* for error checking later */
   reply_cmd[0]='\0';  /* for error checking later */
   again_cmd[0]='\0';  /* for error checking later */
   add_cmd[0]='\0';    /* for error checking later */
   togvis_cmd[0]='\0';    /* for error checking later */

   list_cmd[0]='\0';   /* for error checking later */
   online_list_cmd[0]='\0';   /* for error checking later */
   away_cmd[0]='\0';   /* for error checking later */
   na_cmd[0]='\0';     /* for error checking later */
   dnd_cmd[0]='\0';    /* for error checking later */
   online_cmd[0]='\0'; /* for error checking later */
   occ_cmd[0]='\0';    /* for error checking later */
   ffc_cmd[0]='\0';    /* for error checking later */
   inv_cmd[0]='\0';    /* for error checking later */
   status_cmd[0]='\0'; /* for error checking later */
   auth_cmd[0]='\0';   /* for error checking later */
   auto_cmd[0]='\0';   /* for error checking later */
   change_cmd[0]='\0'; /* for error checking later */
   search_cmd[0]='\0'; /* for error checking later */
   save_cmd[0]='\0';   /* for error checking later */
   alter_cmd[0]='\0';  /* for error checking later */
   msga_cmd[0]='\0';   /* for error checking later */
   url_cmd[0]='\0';   /* for error checking later */
   update_cmd[0]='\0';   /* for error checking later */
   sound_cmd[0]='\0';   /* for error checking later */
   color_cmd[0]='\0';   /* for error checking later */
   rand_cmd[0]='\0';   /* for error checking later */
   Sound_Str[0]='\0';   /* for error checking later */
   togig_cmd[0]='\0';   /* for error checking later */
   iglist_cmd[0]='\0';   /* for error checking later */
   about_cmd[0]='\0';   /* for error checking later */
   passwd[0] = 0;
   UIN = 0;
   away_time = default_away_time;

#ifdef MSGEXEC
	receive_script[0] = '\0';
#endif
 
/* SOCKS5 stuff begin */
   s5Use = 0;
   s5Host[0]='\0';
   s5Port = 0;
   s5Auth = 0;
   s5Name[0]='\0';
   s5Pass[0]='\0';
/* SOCKS5 stuff end */
    
   spooled_tab_nicks = 0;
   Contact_List = FALSE;
   for ( i=1; !Contact_List || buf == 0; i++ )
   {
/*      M_print( "Starting Line " SERVCOL " %d" NOCOL "\n", i );*/
      M_fdnreadln( rcf, buf, sizeof( buf ) );
      if ( ( buf[0] != '#' ) && ( buf[0] != 0 ) )
      {
         tmp = strtok( buf, " " );
         if ( ! strcasecmp( tmp, "Server" ) )
            { strcpy( server, strtok( NULL, " \n\t" ) ); }
         else if ( ! strcasecmp( tmp, "Password" ) )
            { strcpy( passwd, strtok( NULL, "\n\t" ) ); }
	 else if ( ! strcasecmp( tmp, "ReceiveScript") )
#ifdef MSGEXEC
	    { strcpy( receive_script, strtok( NULL, "\n\t" ) );}
#else
	    { printf("Warning: ReceiveScript Feature not enabled!\n"); }
#endif


/* SOCKS5 stuff begin */
         else if ( ! strcasecmp( tmp, "s5_use" ) )
            {  s5Use = atoi( strtok( NULL, " \n\t" ) ); }
         else if ( ! strcasecmp( tmp, "s5_host" ) )
            { M_strcpy( s5Host, strtok( NULL, "\n\t" ) ); }
         else if ( ! strcasecmp( tmp, "s5_port" ) )
            {  s5Port = atoi( strtok( NULL, " \n\t" ) ); }
         else if ( ! strcasecmp( tmp, "s5_auth" ) )
            {  s5Auth = atoi( strtok( NULL, " \n\t" ) ); }
         else if ( ! strcasecmp( tmp, "s5_name" ) )
            { M_strcpy( s5Name, strtok( NULL, "\n\t" ) ); }
         else if ( ! strcasecmp( tmp, "s5_pass" ) )
            { M_strcpy( s5Pass, strtok( NULL, "\n\t" ) ); }
/* SOCKS5 stuff end */

         else if ( ! strcasecmp( tmp, "Russian" ) )
            {  Russian = TRUE; }
        else if ( ! strcasecmp( tmp, "JapaneseEUC" ) )
           {  JapaneseEUC = TRUE; }
          else if ( ! strcasecmp( tmp, "Hermit" ) )
            {  Hermit = TRUE; }
       else if ( ! strcasecmp( tmp, "LogType" ) )
               { LogType = atoi( strtok( NULL," \n\t" ) ); }
         else if ( ! strcasecmp( tmp, "No_Log" ) )
            { 
		LogType = 0;
		M_print( CONTACTCOL "\"No_Log\" is deprecated.\nUse \"LogType 0\" Instead.\n" NOCOL );
		Logging = FALSE;   
	    }
         else if ( ! strcasecmp( tmp, "No_Color" ) )
            { Color = FALSE;   }
         else if ( ! strcasecmp (tmp, "Last_UIN_Prompt"))
            { last_uin_prompt = TRUE; }
         else if ( ! strcasecmp (tmp, "Del_is_Del"))
            { del_is_bs = FALSE; }
         else if ( ! strcasecmp( tmp, "UIN" ) )
            {  UIN = atoi( strtok( NULL, " \n\t" ) );         }
         else if ( ! strcasecmp( tmp, "port" ) )
            {  remote_port = atoi( strtok( NULL, " \n\t" ) ); }
         else if ( ! strcasecmp( tmp, "Status" ) )
            {  set_status = atoi( strtok( NULL, " \n\t" ) ); }
         else if ( ! strcasecmp( tmp, "Auto" ) )
            { auto_resp = TRUE; }
         ADD_COMMAND( "clear_cmd", clear_cmd )
         ADD_COMMAND( "message_cmd", message_cmd )
         ADD_COMMAND( "info_cmd", info_cmd )
         ADD_COMMAND( "rand_cmd", rand_cmd )
         ADD_COMMAND( "color_cmd", color_cmd )
         ADD_COMMAND( "sound_cmd", sound_cmd )
         ADD_COMMAND( "quit_cmd", quit_cmd )
         ADD_COMMAND( "reply_cmd", reply_cmd )
         ADD_COMMAND( "again_cmd", again_cmd )
         ADD_COMMAND( "list_cmd", list_cmd )
         ADD_COMMAND( "away_cmd", away_cmd )
         ADD_MESS( "auto_rep_str_away", auto_rep_str_away )
         ADD_MESS( "auto_rep_str_na", auto_rep_str_na )
         ADD_COMMAND( "na_cmd", na_cmd )
         ADD_COMMAND( "dnd_cmd", dnd_cmd )
         ADD_MESS( "auto_rep_str_dnd", auto_rep_str_dnd )
         ADD_MESS( "auto_rep_str_occ", auto_rep_str_occ )
         ADD_MESS( "auto_rep_str_inv", auto_rep_str_inv )
	 else if ( ! strcasecmp( tmp, "LogDir" ) ) {
		Set_Log_Dir( strtok( NULL, "\n" ) );
	 }
         else if ( ! strcasecmp( tmp, "Sound" ) ) {  
            M_strcpy( Sound_Str, strtok( NULL, "\n\t" ) ); 
	    if ( Sound_Str[0] ) {
            	Sound = SOUND_CMD;
	    } else {
            	Sound = SOUND_OFF;
	    }
         }
	 else if ( ! strcasecmp( tmp, "No_Sound" ) ) {
		Sound = SOUND_OFF;
	 }
         else if ( ! strcasecmp( tmp, "Auto_away" ) )
            {  away_time = atoi( strtok( NULL, " \n\t" ) ); }
         else if ( ! strcasecmp( tmp, "Screen_width" ) )
            {  Max_Screen_Width = atoi( strtok( NULL, " \n\t" ) ); }
         ADD_COMMAND( "togig_cmd", togig_cmd )
         ADD_COMMAND( "iglist_cmd", iglist_cmd )
         ADD_COMMAND( "online_cmd", online_cmd )
         ADD_COMMAND( "occ_cmd", occ_cmd )
         ADD_COMMAND( "ffc_cmd", ffc_cmd )
         ADD_COMMAND( "inv_cmd", inv_cmd )
         ADD_COMMAND( "status_cmd", status_cmd )
         ADD_COMMAND( "auth_cmd", auth_cmd )
         ADD_COMMAND( "auto_cmd", auto_cmd )
         ADD_COMMAND( "change_cmd", change_cmd )
         ADD_COMMAND( "add_cmd", add_cmd )
         ADD_COMMAND( "togvis_cmd", togvis_cmd )
         ADD_COMMAND( "search_cmd", search_cmd )
         ADD_COMMAND( "save_cmd", save_cmd )
         ADD_COMMAND( "alter_cmd", alter_cmd )
         ADD_COMMAND( "online_list_cmd", online_list_cmd )
         ADD_COMMAND( "msga_cmd", msga_cmd )
         ADD_COMMAND( "update_cmd", update_cmd )
         ADD_COMMAND( "url_cmd", url_cmd )
         ADD_COMMAND( "about_cmd", about_cmd )
         else if ( ! strcasecmp( tmp, "Tab" ) )
         {
            if ( spooled_tab_nicks < TAB_SLOTS )
               tab_nick_spool[spooled_tab_nicks++] = strdup ( strtok( NULL, "\n\t" ) );
         }
         else if ( ! strcasecmp( tmp, "Contacts" ) )
         {
            Contact_List = TRUE;
         }
         else
         {
            M_print( SERVCOL "Unrecognized command in rc file : %s, ignored." NOCOL "\n", tmp );
         }
       }
    }
   for ( ; ! M_fdnreadln( rcf, buf, sizeof( buf ) ); )
   {
      if ( Num_Contacts == MAX_CONTACTS )
         break;
      if ( ( buf[0] != '#' ) && ( buf[0] != 0 ) )
      {
         if ( isdigit( (int) buf[0] ) )
         {
            Contacts[ Num_Contacts ].uin = atoi( strtok( buf, " " ) );
            Contacts[ Num_Contacts ].status = STATUS_OFFLINE;
            Contacts[ Num_Contacts ].last_time = -1L;
            Contacts[ Num_Contacts ].current_ip[0] = 0xff;
            Contacts[ Num_Contacts ].current_ip[1] = 0xff;
            Contacts[ Num_Contacts ].current_ip[2] = 0xff;
            Contacts[ Num_Contacts ].current_ip[3] = 0xff;
            tmp = strtok( NULL, "" );
            if ( tmp != NULL )
               memcpy( Contacts[ Num_Contacts ].nick, tmp, sizeof( Contacts->nick )  );
            else
               Contacts[ Num_Contacts ].nick[0] = 0;
            if ( Contacts[ Num_Contacts ].nick[19] != 0 )
               Contacts[ Num_Contacts ].nick[19] = 0;
            if ( Verbose > 2 )
               M_print( "%ld = %s\n", Contacts[ Num_Contacts ].uin, Contacts[ Num_Contacts ].nick );
            Contacts[ Num_Contacts ].vis_list = FALSE;
            Num_Contacts++;
         }
         else if ( buf[0] == '*' )
         {
            Contacts[ Num_Contacts ].uin = atoi( strtok( &buf[1], " " ) );
            Contacts[ Num_Contacts ].status = STATUS_OFFLINE;
            Contacts[ Num_Contacts ].last_time = -1L;
            Contacts[ Num_Contacts ].current_ip[0] = 0xff;
            Contacts[ Num_Contacts ].current_ip[1] = 0xff;
            Contacts[ Num_Contacts ].current_ip[2] = 0xff;
            Contacts[ Num_Contacts ].current_ip[3] = 0xff;
            tmp = strtok( NULL, "" );
            if ( tmp != NULL )
               memcpy( Contacts[ Num_Contacts ].nick, tmp, sizeof( Contacts->nick )  );
            else
               Contacts[ Num_Contacts ].nick[0] = 0;
            if ( Contacts[ Num_Contacts ].nick[19] != 0 )
               Contacts[ Num_Contacts ].nick[19] = 0;
            if ( Verbose > 2 )
               M_print( "%ld = %s\n", Contacts[ Num_Contacts ].uin, Contacts[ Num_Contacts ].nick );
            Contacts[ Num_Contacts ].invis_list = FALSE;
            Contacts[ Num_Contacts ].vis_list = TRUE;
            Num_Contacts++;
         }
         else if ( buf[0] == '~' )
         {
            Contacts[ Num_Contacts ].uin = atoi( strtok( &buf[1], " " ) );
            Contacts[ Num_Contacts ].status = STATUS_OFFLINE;
            Contacts[ Num_Contacts ].last_time = -1L;
            Contacts[ Num_Contacts ].current_ip[0] = 0xff;
            Contacts[ Num_Contacts ].current_ip[1] = 0xff;
            Contacts[ Num_Contacts ].current_ip[2] = 0xff;
            Contacts[ Num_Contacts ].current_ip[3] = 0xff;
            tmp = strtok( NULL, "" );
            if ( tmp != NULL )
               memcpy( Contacts[ Num_Contacts ].nick, tmp, sizeof( Contacts->nick )  );
            else
               Contacts[ Num_Contacts ].nick[0] = 0;
            if ( Contacts[ Num_Contacts ].nick[19] != 0 )
               Contacts[ Num_Contacts ].nick[19] = 0;
            if ( Verbose > 2 )
               M_print( "%ld = %s\n", Contacts[ Num_Contacts ].uin, Contacts[ Num_Contacts ].nick );
            Contacts[ Num_Contacts ].invis_list = TRUE;
            Contacts[ Num_Contacts ].vis_list = FALSE;
            Num_Contacts++;
         }
         else
         {
            tmp_uin = Contacts[ Num_Contacts - 1 ].uin;
            tmp = strtok( buf, ", \t" ); /* aliases may not have spaces */
            for ( ; tmp!=NULL; Num_Contacts++ )
            {
               Contacts[ Num_Contacts ].uin = -tmp_uin;
               Contacts[ Num_Contacts ].status = STATUS_OFFLINE;
               Contacts[ Num_Contacts ].last_time = -1L;
               Contacts[ Num_Contacts ].current_ip[0] = 0xff;
               Contacts[ Num_Contacts ].current_ip[1] = 0xff;
               Contacts[ Num_Contacts ].current_ip[2] = 0xff;
               Contacts[ Num_Contacts ].current_ip[3] = 0xff;
               Contacts[ Num_Contacts ].port = 0;
               Contacts[ Num_Contacts ].sok = (SOK_T ) -1L;
               Contacts[ Num_Contacts ].invis_list = FALSE;
               Contacts[ Num_Contacts ].vis_list = FALSE;
               memcpy( Contacts[ Num_Contacts ].nick, tmp, sizeof( Contacts->nick )  );
               tmp = strtok( NULL, ", \t" );
            }
         }
      }
   }

   /* now tab the nicks we may have spooled earlier */
   for( i=0; i<spooled_tab_nicks; i++)
   {
      add_tab( nick2uin( tab_nick_spool[i] ) );
      free( tab_nick_spool[i] );
   }

   if(clear_cmd[0]=='\0')
       strcpy(clear_cmd, "clear");
   if(message_cmd[0]=='\0')
       strcpy(message_cmd, "msg");
   if(update_cmd[0]=='\0')
       strcpy(update_cmd, "update");
   if(info_cmd[0]=='\0')
       strcpy(info_cmd, "info");
   if(quit_cmd[0]=='\0')
       strcpy(quit_cmd, "q");
   if(reply_cmd[0]=='\0')
       strcpy(reply_cmd, "r");       
   if(again_cmd[0]=='\0')
       strcpy(again_cmd, "a");

   if(list_cmd[0]=='\0')
       strcpy(list_cmd, "w");
   if(online_list_cmd[0]=='\0')
       strcpy(online_list_cmd, "e");
   if(away_cmd[0]=='\0')
       strcpy(away_cmd, "away");
   if(na_cmd[0]=='\0')
       strcpy(na_cmd, "na");
   if(dnd_cmd[0]=='\0')
       strcpy(dnd_cmd, "dnd");   
   if(online_cmd[0]=='\0')
       strcpy(online_cmd, "online");
   if(occ_cmd[0]=='\0')
       strcpy(occ_cmd, "occ");
   if(ffc_cmd[0]=='\0')
       strcpy(ffc_cmd, "ffc");
   if(inv_cmd[0]=='\0')
       strcpy(inv_cmd, "inv");
   if(status_cmd[0]=='\0')
       strcpy(status_cmd, "status");
   if(add_cmd[0]=='\0')
       strcpy(add_cmd, "add");
   if(togvis_cmd[0]=='\0')
       strcpy(togvis_cmd, "togvis");
   if(auth_cmd[0]=='\0')
       strcpy(auth_cmd, "auth");
   if(auto_cmd[0]=='\0')
       strcpy(auto_cmd, "auto");
   if(search_cmd[0]=='\0')
       strcpy(search_cmd, "search");
   if(save_cmd[0]=='\0')
       strcpy(save_cmd, "save");
   if(alter_cmd[0]=='\0')
       strcpy(alter_cmd, "alter");
   if(msga_cmd[0]=='\0')
       strcpy(msga_cmd, "msga");
   if(url_cmd[0]=='\0')
       strcpy(url_cmd, "url");
   if(rand_cmd[0]=='\0')
       strcpy(rand_cmd, "rand");   
   if(color_cmd[0]=='\0')
       strcpy(color_cmd, "color");   
   if(sound_cmd[0]=='\0')
       strcpy(sound_cmd, "sound");   
   if(togig_cmd[0]=='\0')
       strcpy(togig_cmd, "togig");
   if(iglist_cmd[0]=='\0')
       strcpy(iglist_cmd, "i");
   if(about_cmd[0]=='\0')
       strcpy(about_cmd, "about");


   if(change_cmd[0]=='\0')
       strcpy(change_cmd, "change");
   if ( Verbose )
   {
      M_print( "UIN = %ld\n", UIN );
      M_print( "port = %ld\n", remote_port );
      M_print( "passwd = %s\n", passwd );
      M_print( "server = %s\n", server );
      M_print( "status = %ld\n", set_status );
      M_print( "# of contacts = %d\n", Num_Contacts );
      M_print( "UIN of contact[0] = %ld\n", Contacts[0].uin );
      M_print( "Message_cmd = %s\n", message_cmd );
   }
   if (UIN == 0 ) 
   {
      fprintf( stderr, "Bad .micqrc file.  No UIN found aborting.\a\n" );
      exit( 1);
   }
}

/************************************************
 *   This function should save your auto reply messages in the rc file.
 *   NOTE: the code isn't realy neat yet, I hope to change that soon.
 *   Added on 6-20-98 by Fryslan
 ***********************************************/
int Save_RC()
{
   FD_T rcf;
   time_t t;
   int i, j;
 
   rcf = open( rcfile, O_RDWR );
   if ( rcf == -1 ) return -1;
   M_fdprint( rcf, "# This file was generated by Micq of %s %s\n",__TIME__,__DATE__);
   t = time( NULL );
   M_fdprint( rcf, "# This file was generated on %s", ctime( &t ) );   
   M_fdprint( rcf, "# Micq version " MICQ_VERSION );
   M_fdprint( rcf, "\n\nUIN %d\n", UIN );
   M_fdprint( rcf, "# Remove the entire below line if you want to be prompted for your password.\n" );
   M_fdprint( rcf, "Password %s\n", passwd );
   M_fdprint( rcf, "\n#Partial list of status you can use here.  (more from the change command )\n" );
   M_fdprint( rcf, "#    0 Online\n" );
   M_fdprint( rcf, "#   32 Free for Chat\n" );
   M_fdprint( rcf, "#  256 Invisible\n" );
   M_fdprint( rcf, "Status %d\n", Current_Status );
   M_fdprint( rcf, "\nServer %s\n", "icq1.mirabilis.com" );
   M_fdprint( rcf, "Port %d\n", 4000 );

/* SOCKS5 stuff begin */
   M_fdprint( rcf, "\n# Support for SOCKS5 server\n" );
   M_fdprint( rcf, "s5_use %d\n", s5Use );
   if ( NULL == s5Host )
	   M_fdprint( rcf, "s5_host [none]\n" );
   else
	   M_fdprint( rcf, "s5_host %s\n", s5Host );
   M_fdprint( rcf, "s5_port %d\n", s5Port );
   M_fdprint( rcf, "# If you need authentification, put 1 for s5_auth and fill your name/password\n" );
   M_fdprint( rcf, "s5_auth %d\n", s5Auth );
   if ( NULL == s5Name )
  	 M_fdprint( rcf, "s5_name [none]\n" );
   else
  	 M_fdprint( rcf, "s5_name %s\n", s5Name );
   if ( NULL == s5Pass )
	   M_fdprint( rcf, "s5_pass [none]\n" );
   else
	   M_fdprint( rcf, "s5_pass %s\n", s5Pass );
/* SOCKS5 stuff end */

   M_fdprint( rcf, "\n#If you want messages from people on your list ONLY uncomment below.\n" );
   if ( !Hermit )
      M_fdprint( rcf, "#Hermit\n\n" );
   else
      M_fdprint( rcf, "Hermit\n\n" );

      M_fdprint( rcf, "# This is the location messages will be logged to\n" );
      if ( Log_Dir_Normal() ) { 
	M_fdprint( rcf, "#" );
      }
      M_fdprint( rcf, "LogDir %s\n", Get_Log_Dir() );
      M_fdprint( rcf, "\n# LogType defines if we log events to an individual file (per UIN),\n" );
      M_fdprint( rcf, "# if we log everything to ~/micq.log or if we don't log at all.\n" );
      M_fdprint( rcf, "# Values are:\n# 0 - Don't Log\n" );
      M_fdprint( rcf, "# 1 - Log to ~/micq_log\n" );
      M_fdprint( rcf, "# 2 - Log to ~/micq.log/<uin>.log\n");
      M_fdprint( rcf, "# 3 - Same as 2 but don't log online/offline changes\n");
      M_fdprint( rcf, "# by Buanzox@usa.net\n" );
 
      M_fdprint( rcf, "LogType %d\n\n",LogType );

   if ( Color )
      M_fdprint( rcf, "#No_Color\n" );
   else
      M_fdprint( rcf, "No_Color\n" );
   if (del_is_bs)
      M_fdprint (rcf, "#Del_is_Del\n");
   else
      M_fdprint (rcf, "Del_is_Del\n");
   if (last_uin_prompt)
      M_fdprint (rcf, "Last_UIN_Prompt\n");
   else
      M_fdprint (rcf, "#Last_UIN_Prompt\n");
   if ( Russian )
      M_fdprint( rcf, "\nRussian\n#if you want KOI8-R/U to CP1251 Cyrillic translation uncomment the above line.\n" );
   else
      M_fdprint( rcf, "\n#Russian\n#if you want KOI8-R/U to CP1251 Cyrillic translation uncomment the above line.\n" );
  if( JapaneseEUC ) {
    M_fdprint( rcf, "\nJapaneseEUC\n#if you want Shift-JIS <-> EUC Japanese translation uncomment the above line.\n" );
  } else {
    M_fdprint( rcf, "\n#JapaneseEUC\n#if you want Shift-JIS <-> EUC Japanese translation uncomment the above line.\n" );
  }
   if ( auto_resp )
      M_fdprint( rcf, "\n#Automatic responses on.\nAuto\n" );
   else
      M_fdprint( rcf, "\n#Automatic responses off.\n#Auto\n" );

	M_fdprint( rcf, "\n#in seconds\nAuto_Away %d\n", away_time );
	M_fdprint( rcf, "\n#For dumb terminals that don't wrap set this." );
	M_fdprint( rcf, "\nScreen_Width %d\n", Max_Screen_Width );

   M_fdprint( rcf, "\n# Below are the commands which can be changed to most anything you want :)\n" );
   M_fdprint( rcf, "clear_cmd %s\n",clear_cmd);
   M_fdprint( rcf, "message_cmd %s\n",message_cmd);
   M_fdprint( rcf, "togig_cmd %s\n",togig_cmd);
   M_fdprint( rcf, "iglist_cmd %s\n",iglist_cmd);
   M_fdprint( rcf, "info_cmd %s\n",info_cmd);
   M_fdprint( rcf, "quit_cmd %s\n",quit_cmd);
   M_fdprint( rcf, "reply_cmd %s\n",reply_cmd);
   M_fdprint( rcf, "again_cmd %s\n",again_cmd);
   M_fdprint( rcf, "list_cmd %s\n",list_cmd);
   M_fdprint( rcf, "online_list_cmd %s\n",online_list_cmd);
   M_fdprint( rcf, "away_cmd %s\n",away_cmd);
   M_fdprint( rcf, "na_cmd %s\n",na_cmd);
   M_fdprint( rcf, "dnd_cmd %s\n",dnd_cmd);
   M_fdprint( rcf, "online_cmd %s\n",online_cmd);
   
   M_fdprint( rcf, "occ_cmd %s\n",occ_cmd);
   M_fdprint( rcf, "ffc_cmd %s\n",ffc_cmd);
   M_fdprint( rcf, "inv_cmd %s\n",inv_cmd);
   M_fdprint( rcf, "search_cmd %s\n",search_cmd);
   M_fdprint( rcf, "status_cmd %s\n",status_cmd);
   M_fdprint( rcf, "auth_cmd %s\n",auth_cmd);
   M_fdprint( rcf, "auto_cmd %s\n",auto_cmd);
   M_fdprint( rcf, "add_cmd %s\n",add_cmd);
   M_fdprint( rcf, "togvis_cmd %s\n",togvis_cmd);
   M_fdprint( rcf, "change_cmd %s\n",change_cmd);
   M_fdprint( rcf, "save_cmd %s\n",save_cmd);
   M_fdprint( rcf, "alter_cmd %s\n",alter_cmd);
   M_fdprint( rcf, "msga_cmd %s\n",msga_cmd);
   M_fdprint( rcf, "url_cmd %s\n",url_cmd);
   M_fdprint( rcf, "update_cmd %s\n",update_cmd);
   M_fdprint( rcf, "about_cmd %s\n",about_cmd);

   M_fdprint( rcf, "\n#Now auto response messages\n" );     
   M_fdprint( rcf, "auto_rep_str_away %s\n",auto_rep_str_away);
   M_fdprint( rcf, "auto_rep_str_na %s\n",auto_rep_str_na);    
   M_fdprint( rcf, "auto_rep_str_dnd %s\n",auto_rep_str_dnd);
   M_fdprint( rcf, "auto_rep_str_occ %s\n",auto_rep_str_occ);
   M_fdprint( rcf, "auto_rep_str_inv %s\n",auto_rep_str_inv);


   M_fdprint( rcf, "\n# Ok now the contact list\n" );
   M_fdprint( rcf, "#  Use * in front of the number of anyone you want to see you while you're invisble.\n" );
   M_fdprint( rcf, "#  Use ~ in front of the number of anyone you want to always see you as offline.\n" );
   M_fdprint( rcf, "#  People in the second group won't show up in your list.\n" );
   M_fdprint( rcf, "Contacts\n" );
    /* adding contacts to the rc file. */
    /* we start counting at zero in the index. */
    
   for (i=0;i<Num_Contacts;i++)
   {
      if ( ! ( Contacts[i].uin & 0x80000000L ) )
      {
         if ( Contacts[i].vis_list )
         {
            M_fdprint( rcf, "*" );
         }
         if ( Contacts[i].invis_list )
         {
            M_fdprint( rcf, "~" );
         }
     M_fdprint( rcf, "%d %s\n",Contacts[i].uin,Contacts[i].nick);
/*     M_fdprint( rcf, "#Begining of aliases for %s\n", Contacts[i].nick ); */
     for ( j=0; j< Num_Contacts; j++ )
     {
        if ( Contacts[j].uin == -Contacts[i].uin )
        {
           M_fdprint( rcf, "%s ", Contacts[j].nick );
        }
     }
     M_fdprint( rcf, "\n" );
/*     M_fdprint( rcf, "\n#End of aliases for %s\n", Contacts[i].nick ); */
      }
   }
    M_fdprint( rcf, "\n" );
   return close( rcf );
}

/*******************************************************
Gets config info from the rc file in the users home 
directory.
********************************************************/
void Get_Unix_Config_Info( void )
{
   FD_T rcf;

   rcf = open( rcfile, O_RDONLY );
   if ( rcf == -1 )
   {
      if ( errno == ENOENT ) /* file not found */
      {
         Initalize_RC_File();
      }
      else
      {
         perror( "Error reading config file exiting " );
         exit( 1 );
      }
   }
   else
   {
      Read_RC_File( rcf );
   }
}

void Add_User( SOK_T sok, DWORD uin, char *name )
{
   FD_T rcf;

   rcf = open( rcfile, O_RDWR | O_APPEND );
   M_fdprint( rcf, "%d %s\n", uin, name );
   close( rcf );
   Contacts[ Num_Contacts ].uin = uin;
   Contacts[ Num_Contacts ].status = STATUS_OFFLINE;
   Contacts[ Num_Contacts ].last_time = -1L;
   Contacts[ Num_Contacts ].current_ip[0] = 0xff;
   Contacts[ Num_Contacts ].current_ip[1] = 0xff;
   Contacts[ Num_Contacts ].current_ip[2] = 0xff;
   Contacts[ Num_Contacts ].current_ip[3] = 0xff;
   Contacts[ Num_Contacts ].port = 0;
   Contacts[ Num_Contacts ].sok = (SOK_T ) -1L;
   Contacts[ Num_Contacts ].vis_list = FALSE;
   Contacts[ Num_Contacts ].invis_list = FALSE;
   memcpy( Contacts[ Num_Contacts ].nick, name, sizeof( Contacts->nick )  );
   Num_Contacts++;
   snd_contact_list( sok );
}


