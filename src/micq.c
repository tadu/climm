#include "micq.h"
#include "datatype.h"
#include "msg_queue.h"
#include "mselect.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <fcntl.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#ifdef _WIN32
 #include <conio.h>
 #include <io.h>
 #include <winsock2.h>
 #include <time.h>
#else
 #include <unistd.h>
 #include <netinet/in.h>
 #include <sys/types.h>
 #include <sys/stat.h>
 #include <sys/socket.h>
   #ifndef __BEOS__
     #include <arpa/inet.h>
   #endif
 #include <netdb.h>
 #include <sys/time.h>
 #include <sys/wait.h>
 #include "mreadline.h"
#endif

#ifdef __BEOS__
  #include "beos.h"
#endif

DWORD real_packs_sent = 0;
DWORD real_packs_recv = 0;

BYTE Sound = SOUND_ON; /* Beeps on by default */
BYTE Sound_Str[150];   /* the command to run from the shell to play sound files */
BOOL Hermit = FALSE;
BOOL Russian = FALSE;  /* Do we do kio8-r <->Cp1251 codeset translation? */
BOOL JapaneseEUC = FALSE; /* Do we do Shift-JIS <->EUC codeset translation? */
BYTE LogType = 2;      /* Currently 0 = no logging
			       1 = old style ~/micq_log
			       2 = new style ~/micq.log/uin.log
		     *********************************************/
BOOL Logging = TRUE;  /* Do we log messages to ~/micq_log?  This should probably have different levels */
BOOL Color = TRUE; /* Do we use ANSI color? */
BOOL Quit = FALSE;    /* set when it's time to exit the program */
BOOL Verbose = FALSE; /* this is displays extra debuging info */
BOOL serv_mess[ 1024 ]; /* used so that we don't get duplicate messages with the same SEQ */
WORD last_cmd[ 1024 ]; /* command issued for the first 1024 SEQ #'s */
/******************** if we have more than 1024 SEQ this will need some hacking */
WORD seq_num = 1;  /* current sequence number */
DWORD our_ip = 0x0100007f; /* localhost for some reason */
DWORD our_port; /* the port to make tcp connections on */
/************ We don't make tcp connections yet though :( */
DWORD UIN; /* current User Id Number */
BOOL Contact_List = FALSE; /* I think we always have a contact list now */
Contact_Member Contacts[ MAX_CONTACTS ]; /* no more than this many contacts max */
int Num_Contacts=0;
DWORD Current_Status=STATUS_OFFLINE;
DWORD last_recv_uin=0;
char passwd[100];
char server[100];
DWORD set_status;
DWORD remote_port;
BOOL Done_Login=FALSE;

/* SOCKS5 stuff begin */
int      s5Use;
char     s5Host[100];
WORD s5Port;
int      s5Auth;
char     s5Name[64];
char     s5Pass[64];
DWORD  s5DestIP;
WORD s5DestPort;
/* SOCKS5 stuff end */

BOOL auto_resp=FALSE;
char auto_rep_str_dnd[450] = { DND_DEFAULT_STR };
char auto_rep_str_away[450] = { AWAY_DEFAULT_STR };
char auto_rep_str_na[450] = { NA_DEFAULT_STR };
char auto_rep_str_occ[450] = { OCC_DEFAULT_STR };
char auto_rep_str_inv[450] = { INV_DEFAULT_STR };
char clear_cmd[16];
char message_cmd[16];
char info_cmd[16];
char quit_cmd[16];
char reply_cmd[16];
char again_cmd[16];
char add_cmd[16];
char togvis_cmd[16];
char about_cmd[16];

char list_cmd[16];
char online_list_cmd[16];
char togig_cmd[16];
char iglist_cmd[16];
char away_cmd[16];
char na_cmd[16];
char dnd_cmd[16];
char online_cmd[16];
char occ_cmd[16];
char ffc_cmd[16];
char inv_cmd[16];
char status_cmd[16];
char auth_cmd[16];
char auto_cmd[16];
char change_cmd[16];
char search_cmd[16];
char save_cmd[16];
char alter_cmd[16];
char msga_cmd[16];
char url_cmd[16];
char update_cmd[16];
char rand_cmd[16];
char color_cmd[16];
char sound_cmd[16];

/*** auto away values ***/
int idle_val=0;
int idle_flag=0;
unsigned int away_time;

unsigned int next_resend;

char * tab_array[TAB_SLOTS];
int tab_pointer;

/* aaron
   Actual definition of the variable holding Micq's start time.				*/
time_t	MicqStartTime;
/* end of aaron */

#ifdef MSGEXEC
 /*
  *Ben Simon:
  * receive_script -- a script that gets called anytime we receive
  * a message
  */
 char receive_script[255];

#endif

/*/////////////////////////////////////////////
// Connects to hostname on port port
// hostname can be DNS or nnn.nnn.nnn.nnn
// write out messages to the FD aux */
SOK_T Connect_Remote( char *hostname, int port, FD_T aux )
{
/* SOCKS5 stuff begin */
   int res;
   char buf[64];
   struct sockaddr_in s5sin;
   int s5Sok;
   unsigned short s5OurPort;
   unsigned long s5IP;
/* SOCKS5 stuff end */

   int conct, length;
   int sok;
   struct sockaddr_in sin;  /* used to store inet addr stuff */
   struct hostent *host_struct; /* used in DNS lookup */

   sok = socket( AF_INET, SOCK_DGRAM, 0 );/* create the unconnected socket*/

   if ( sok == -1 ) 
   {
      perror( SOCKET_FAIL_STR );
      exit( 1 );
   }   
   if ( Verbose )
   {
      M_fdprint( aux, SOCKET_CREAT_STR );
   }

if( s5Use )
{
   sin.sin_addr.s_addr = INADDR_ANY;
   sin.sin_family = AF_INET;
   sin.sin_port = 0;

   if(bind(sok, (struct sockaddr*)&sin, sizeof(struct sockaddr))<0)
   {
     M_fdprint(aux, "Can't bind socket to free port\n");
     return -1;
   }

  length = sizeof(sin);
  getsockname(sok, (struct sockaddr*)&sin, &length);
  s5OurPort = ntohs(sin.sin_port);

  s5sin.sin_addr.s_addr = inet_addr(s5Host);
  if(s5sin.sin_addr.s_addr  == (unsigned long)-1) /* name isn't n.n.n.n so must be DNS */
  {
    host_struct = gethostbyname(s5Host);
    if(host_struct == 0L)
    {
      M_print("[SOCKS] Can't find hostname: %s\n", s5Host);
      return -1;
    }
    s5sin.sin_addr = *((struct in_addr*)host_struct->h_addr);
  }
  s5IP = ntohl(s5sin.sin_addr.s_addr);
  s5sin.sin_family = AF_INET; /* we're using the inet not appletalk*/
  s5sin.sin_port = htons(s5Port); /* port */
  s5Sok = socket(AF_INET, SOCK_STREAM, 0);/* create the unconnected socket*/
  if(s5Sok == -1)
  {
    M_print("[SOCKS] Socket creation failed\n");
    return -1;
  }
  conct = connect(s5Sok, (struct sockaddr *) &s5sin, sizeof(s5sin));
  if(conct == -1) /* did we connect ?*/
  {
    M_print("[SOCKS] Connection refused\n");
    return -1;
  }
  buf[0] = 5; /* protocol version */
  buf[1] = 1; /* number of methods */
  if(!strlen(s5Name) || !strlen(s5Pass) || !s5Auth)
    buf[2] = 0; /* no authorization required */
  else
    buf[2] = 2; /* method username/password */
  send(s5Sok, buf, 3, 0);
  res = recv(s5Sok, buf, 2, 0);
  if(strlen(s5Name) && strlen(s5Pass) && s5Auth)
  {
    if(res != 2 || buf[0] != 5 || buf[1] != 2) /* username/password authentication*/
    {
      M_print("[SOCKS] Authentication method incorrect\n");
      close(s5Sok);
      return -1;
    }
    buf[0] = 1; /* version of subnegotiation */
    buf[1] = strlen(s5Name);
    memcpy(&buf[2], s5Name, buf[1]);
    buf[2+buf[1]] = strlen(s5Pass);
    memcpy(&buf[3+buf[1]], s5Pass, buf[2+buf[1]]);
    send(s5Sok, buf, buf[1]+buf[2+buf[1]]+3, 0);
    res = recv(s5Sok, buf, 2, 0);
    if(res != 2 || buf[0] != 1 || buf[1] != 0)
    {
      M_print("[SOCKS] Authorization failure\n");
      close(s5Sok);
      return -1;
    }
  }
  else
  {
    if(res != 2 || buf[0] != 5 || buf[1] != 0) /* no authentication required */
    {
      M_print("[SOCKS] Authentication method incorrect\n");
      close(s5Sok);
      return -1;
    }
  }
  buf[0] = 5; /* protocol version */
  buf[1] = 3; /* command UDP associate */
  buf[2] = 0; /* reserved */
  buf[3] = 1; /* address type IP v4 */
  buf[4] = (char)0;
  buf[5] = (char)0;
  buf[6] = (char)0;
  buf[7] = (char)0;
  *(unsigned short*)&buf[8] = htons(s5OurPort);
/*     memcpy(&buf[8], &s5OurPort, 2); */
  send(s5Sok, buf, 10, 0);
  res = recv(s5Sok, buf, 10, 0);
  if(res != 10 || buf[0] != 5 || buf[1] != 0)
  {
      M_print("[SOCKS] General SOCKS server failure\n");
      close(s5Sok);
      return -1;
  }
}

   sin.sin_addr.s_addr = inet_addr( hostname ); 
   if ( sin.sin_addr.s_addr  == -1 ) /* name isn't n.n.n.n so must be DNS */
   {
	   host_struct = gethostbyname( hostname );
      if ( host_struct == NULL )
      {
         if ( Verbose )
         {
            M_fdprint( aux, BAD_HOST1_STR );
            M_fdprint( aux, BAD_HOST2_STR, hostname );
            /*herror( "Can't find hostname" );*/
         }
         return -1;
      }
      sin.sin_addr = *((struct in_addr *)host_struct->h_addr);
   }
   sin.sin_family = AF_INET; /* we're using the inet not appletalk*/
   sin.sin_port = ntohs (port);

	if( s5Use )
	{
	 s5DestIP = ntohl(sin.sin_addr.s_addr);
	  memcpy(&sin.sin_addr.s_addr, &buf[4], 4);
  
	  sin.sin_family = AF_INET; /* we're using the inet not appletalk*/
	  s5DestPort = port;
	  memcpy(&sin.sin_port, &buf[8], 2);
	}

   conct = connect( sok, (struct sockaddr *) &sin, sizeof( sin ) );

   if ( conct == -1 )/* did we connect ?*/
   {
      if ( Verbose )
      {
   	   M_fdprint( aux, CONNECT_REFUSED_STR, port, hostname );
         #ifdef FUNNY_MSGS
            M_fdprint( aux, " D'oh!\n" );
         #endif
   	   perror( "connect" );
      }
	   return -1;
   }

   length = sizeof( sin ) ;
   getsockname( sok, (struct sockaddr *) &sin, &length );
   our_ip = ntohl(sin.sin_addr.s_addr);
   our_port = ntohs(sin.sin_port);

   if (Verbose )
   {
      M_fdprint( aux, OUR_PORT_STR, ntohs( sin.sin_port ) );
      M_fdprint( aux, CONNECT_SUCCESS, hostname );
   }

   return sok;
}


/*/////////////////////////////////////////////
// Connects to hostname on port port
// hostname can be DNS or nnn.nnn.nnn.nnn
// write out messages to the FD aux */
int Connect_Remote_Old( char *hostname, int port, FD_T aux )
{
   int conct, length;
   int sok;
   struct sockaddr_in sin;  /* used to store inet addr stuff */
   struct hostent *host_struct; /* used in DNS lookup */

#if 1
	sin.sin_addr.s_addr = inet_addr( hostname ); 
  	if ( sin.sin_addr.s_addr  == -1 ) /* name isn't n.n.n.n so must be DNS */
#else

	if ( inet_aton( hostname, &sin.sin_addr )  == 0 ) /* checks for n.n.n.n notation */
#endif
	{
	   host_struct = gethostbyname( hostname );/* name isn't n.n.n.n so must be DNS */
      if ( host_struct == NULL )
      {
         if ( Verbose )
         {
            M_fdprint( aux, BAD_HOST1_STR );
            M_fdprint( aux, BAD_HOST2_STR, hostname );
            /*herror( "Can't find hostname" );*/
         }
         return 0;
      }
	   sin.sin_addr = *((struct in_addr *)host_struct->h_addr);
   }
	sin.sin_family = AF_INET; /* we're using the inet not appletalk*/
	sin.sin_port = htons( port );	/* port */
	sok = socket( AF_INET, SOCK_DGRAM, 0 );/* create the unconnected socket*/
   if ( sok == -1 ) 
   {
      perror( SOCKET_FAIL_STR );
      exit( 1 );
   }   
   if ( Verbose )
   {
      M_fdprint( aux, SOCKET_CREAT_STR );
   }
	conct = connect( sok, (struct sockaddr *) &sin, sizeof( sin ) );
	if ( conct == -1 )/* did we connect ?*/
	{
      if ( Verbose )
      {
   	   M_fdprint( aux,CONNECT_REFUSED_STR , port, hostname );
         #ifdef FUNNY_MSGS
            M_fdprint( aux, " D'oh!\n" );
         #endif
   	   perror( "connect" );
      }
	   return 0;
	}
   length = sizeof( sin ) ;
   getsockname( sok, (struct sockaddr *) &sin, &length );
   our_ip = sin.sin_addr.s_addr;
   our_port = sin.sin_port;
   if (Verbose )
   {
      M_fdprint( aux, OUR_PORT_STR, ntohs( sin.sin_port ) );
      M_fdprint( aux, CONNECT_SUCCESS, hostname );
   }
   return sok;
}


/******************************************
Handles packets that the server sends to us.
*******************************************/
void Handle_Server_Response( SOK_T sok )
{
   srv_net_icq_pak pak;
   static DWORD last_seq=-1;
   int s;
   
   s = SOCKREAD( sok, &pak.head.ver, sizeof( pak ) - 2  );
   if ( s < 0 )
   	return;
      
#if 0
   M_print( "Cmd : %04X\t",Chars_2_Word( pak.head.cmd ) );
   M_print( "Ver : %04X\t",Chars_2_Word( pak.head.ver ) );
   M_print( "Seq : %08X\t",Chars_2_DW( pak.head.seq ) );
   M_print( "Ses : %08X\n",Chars_2_DW( pak.head.session ) );
#endif
   if ( Chars_2_Word( pak.head.ver ) != ICQ_VER ) {
       	R_undraw();
	   M_print( "Invalid server response:\tVersion: %d\n", Chars_2_Word( pak.head.ver ) );
	if ( Verbose ) {
		Hex_Dump( pak.head.ver, s );
	}
	R_redraw();
	return;
   }
  if ( Chars_2_DW( pak.head.session ) != our_session ) {
     if ( Verbose ) {
       	R_undraw();
        M_print( "Got a bad session ID %08X with CMD %04X ignored.\n", 
            Chars_2_DW( pak.head.session ), Chars_2_Word( pak.head.cmd ) );
	R_redraw();
     }
     return;
  }
  /* !!! TODO make a check checksum routine to verify the packet further */
/*   if ( ( serv_mess[ Chars_2_Word( pak.head.seq2 ) ] ) && 
      ( Chars_2_Word( pak.head.cmd ) != SRV_NEW_UIN ) )*/
/*   if ( ( last_seq == Chars_2_DW( pak.head.seq ) ) && 
      ( Chars_2_Word( pak.head.cmd ) != SRV_NEW_UIN ) ) */
    if ( ( Chars_2_Word( pak.head.cmd ) != SRV_NEW_UIN ) &&
	( Is_Repeat_Packet( Chars_2_Word( pak.head.seq ) ) ) )
    {
      if ( Chars_2_Word( pak.head.seq ) == 0 ) {;} else {  /*yes ugly*/
      if ( Chars_2_Word( pak.head.cmd ) != SRV_ACK ) /* ACKs don't matter */
      {
         if ( Verbose ) {
       	    R_undraw();
            M_print( IGNORE_CMD_STR, Chars_2_Word( pak.head.cmd )  );
	    R_redraw();
	 }
         ack_srv( sok, Chars_2_DW( pak.head.seq ) ); /* LAGGGGG!! */
         return;
      }
      }
   }
   if ( Chars_2_Word( pak.head.cmd ) != SRV_ACK )
   {
      serv_mess[ Chars_2_Word( pak.head.seq2 ) ] = TRUE;
      last_seq = Chars_2_DW( pak.head.seq );
      Got_SEQ( Chars_2_DW( pak.head.seq ) );
      ack_srv( sok, Chars_2_DW( pak.head.seq ) );
	real_packs_recv++;
   }
   Server_Response( sok, pak.head.check + DATA_OFFSET , s - ( sizeof( pak.head ) - 2 ), 
      Chars_2_Word( pak.head.cmd ), Chars_2_Word( pak.head.ver ),
      Chars_2_DW( pak.head.seq ), Chars_2_DW( pak.head.UIN ) );
}

/**********************************************
Verifies that we are in the correct endian
***********************************************/
void Check_Endian( void )
{
   int i;
   char passwd[10];
   
   passwd[0] = 1;
   passwd[1] = 0;
   passwd[2] = 0;
   passwd[3] = 0;
   passwd[4] = 0;
   passwd[5] = 0;
   passwd[6] = 0;
   passwd[7] = 0;
   passwd[8] = 0;
   passwd[9] = 0;
   i = *  ( DWORD *) passwd;
   if ( i == 1 )
   {
      M_print( INTEL_BO_STR  );
   }
   else
   {
      M_print( MOTOROLA_BO_STR );
   }
}

/******************************
Idle checking function
added by Warn Kitchen 1/23/99
******************************/
void Idle_Check( SOK_T sok )
{
   int tm;

   if ( away_time == 0 ) return;
   tm = ( time( NULL ) - idle_val );
   if ( ( Current_Status == STATUS_AWAY || Current_Status == STATUS_NA )
           && tm < away_time && idle_flag == 1) {
      icq_change_status(sok,STATUS_ONLINE);
      R_undraw ();
      M_print( AUTO_STATUS_CHANGE_STR );
      Print_Status(Current_Status);
      M_print( " " );
      Time_Stamp();
      M_print( "\n" );
      R_redraw ();
      idle_flag=0;
      return;
   }
   if ( (Current_Status == STATUS_AWAY) && (tm >= (away_time*2)) && (idle_flag == 1) ) {
      icq_change_status(sok,STATUS_NA);
      R_undraw ();
      M_print( AUTO_STATUS_CHANGE_STR );
      Print_Status(Current_Status);
      M_print( " " );
      Time_Stamp();
      M_print( "\n" );
      R_redraw ();
      return;
   }
   if ( Current_Status != STATUS_ONLINE && Current_Status != STATUS_FREE_CHAT ) {
      return;
   }
   if(tm>=away_time) {
      icq_change_status(sok,STATUS_AWAY); 
      R_undraw ();
      M_print( AUTO_STATUS_CHANGE_STR );
      Print_Status(Current_Status);
      M_print( " " );
      Time_Stamp();
      M_print( "\n" );
      R_redraw ();
      idle_flag=1;
   }
   return;
}

void Usage()
{
    M_print("Usage: micq [-v|-V] [-f|-F <rc-file>] [-l|-L <logfile>] [-?|-h]\n");
    M_print("        -v   Turn on verbose Mode (useful for Debugging only)\n");
    M_print("        -f   specifies an alternate Config File (default: ~/.micqrc)\n"); 
    M_print("        -l   specifies an alternate LogFile\n");
    M_print("        -?   gives this help screen\n\n");
    exit (0);
}

/******************************
Main function connects gets UIN
and passwd and logins in and sits
in a loop waiting for server responses.
******************************/
int main( int argc, char *argv[] )
{
   int sok;
   int i;
   int next;
   int time_delay = 120;
#ifdef _WIN32
   WSADATA wsaData;
#endif

   setbuf (stdout, NULL); /* Don't buffer stdout */
   M_print( SERVCOL "Matt's ICQ clone " NOCOL "compiled on %s %s\n" \
            SERVCOL "Version " MICQ_VERSION NOCOL "\n",      \
	    __TIME__, __DATE__ );
	    
#ifdef FUNNY_MSGS
   M_print( "No Mirabilis client was maimed, hacked, tortured, sodomized or otherwise harmed\nin the making of this utility.\n" );
#else
   M_print( "This program was made without any help from Mirabilis or their consent.\n" );
   M_print( "No reverse engineering or decompilation of any Mirabilis code took place\nto make this program.\n" );
#endif

   /* aaron
    "tabs" is a neat feature, but if you've talked to less than TAB_SLOTS
    people, the first person in the list will repeat. This appears to be
    because tab_array is initialized to zero (*somewhere*, but I can't find
    it). So we'll try initializing to -1 and see if it helps at all -- this
    way we can detect an early-exit condition.                              */
   memset(tab_array, 0, (TAB_SLOTS * sizeof(int) ) );

   /* and now we save the time that Micq was started, so that uptime will be
      able to appear semi intelligent.										*/
   MicqStartTime = time(NULL);
   /* end of aaron */

   Set_rcfile( NULL );
   if (argc > 1 )
   {
      for ( i=1; i< argc; i++ )
      {
         if ( argv[i][0] != '-' ) { 
	    ; 
	 } else if ( (argv[i][1] == 'v' ) || (argv[i][1] == 'V' ) ) {
            Verbose++;
         } else if ( (argv[i][1] == 'f' ) || (argv[i][1] == 'F' ) ) {
		i++; /* skip the argument to f */
		Set_rcfile( argv[i] );
		M_print( "The config file for this session is \"%s\"\n", argv[i]);
         } else if ( (argv[i][1] == 'l' ) || (argv[i][1] == 'L' ) ) {
		i++;
		M_print( "The logging directory for this session is \"%s\"\n", Set_Log_Dir( argv[i] ) );
         } else if ( (argv[i][1] == '?' ) || (argv[i][1] == 'h' ) ) {
	        Usage();
		/* One will never get here ... */
	 }
      }
   }
   
   Get_Config_Info();
   srand( time( NULL ) );
   if ( !strcmp( passwd, "" ) )
   {
      M_print( ENTER_PW_STR );
      Echo_Off();
      M_fdnreadln(STDIN, passwd, sizeof(passwd));
      Echo_On();
   }
   memset( serv_mess, FALSE, 1024 );

#ifdef __BEOS__
   Be_Start();
   M_print("Started BeOS InputThread\n\r");
#endif

   Initialize_Msg_Queue();
   Check_Endian();
   
   
#ifdef _WIN32
   i = WSAStartup( 0x0101, &wsaData );
   if ( i != 0 ) {
#ifdef FUNNY_MSGS
		perror("Windows Sockets broken blame Bill -");
#else
		perror("Sorry, can't initialize Windows Sockets...");
#endif
	    exit(1);
   }
#endif


   sok = Connect_Remote( server, remote_port, STDERR );

#ifdef __BEOS__
   if ( sok == -1 )
#else
   if ( ( sok == -1 ) || ( sok == 0 ) )
#endif
   {
   	M_print( CONNECT_ERROR );
   	exit( 1 );
   }
   Login( sok, UIN, &passwd[0], our_ip, our_port, set_status );
   next = time( NULL );
   idle_val = time( NULL );
   next += 120;
   next_resend = 10;
   R_init ();
   M_print ("\n");
   Prompt();
   for ( ; !Quit; )
   {
      Idle_Check( sok );
#ifdef UNIX
      M_set_timeout( 2, 500000 );
#else
      M_set_timeout( 0, 100000 );
#endif

#ifdef __BEOS__
      M_set_timeout( 0, 100000 );
#endif
 
      M_select_init();
      M_Add_rsocket( sok );
#ifndef _WIN32
      M_Add_rsocket( STDIN );
#endif

      M_select();

      if ( M_Is_Set( sok ) )
          Handle_Server_Response( sok );
#if _WIN32
	  if (_kbhit())		/* sorry, this is a bit ugly...   [UH]*/
#else      
  #ifndef __BEOS__
          if ( M_Is_Set( STDIN ) )
  #else
          if (Be_TextReady())
  #endif
#endif
      {
/*         idle_val = time( NULL );*/
	 if (R_process_input ())
		Get_Input( sok, &idle_val, &idle_flag );
      }

      if ( time( NULL ) > next )
      {
         next = time( NULL ) + time_delay;
         Keep_Alive( sok );
      }

      if ( time( NULL ) > next_resend )
      {
        Do_Resend( sok );
      }
#ifdef UNIX
      while(waitpid(-1, NULL, WNOHANG) > 0); /* clean up child processes */
#endif
   }

#ifdef __BEOS__
   Be_Stop();
#endif

   Quit_ICQ( sok );
   return 0;
}
