#include "micq.h"
#include "util_ui.h"
#include "datatype.h"
#include "mreadline.h"
#include "msg_queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <limits.h>

#ifdef _WIN32
  #include <conio.h>
  #include <io.h>
  #include <winsock2.h>
  #include <time.h>
#else
  #include <sys/types.h>
  #include <unistd.h>
  #include <netinet/in.h>
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

/*extern unsigned int next_resend;*/
/*extern BOOL serv_mess[ 1024 ];  used so that we don't get duplicate messages with the same SEQ */

static void Auto_Reply (SOK_T sok, SIMPLE_MESSAGE_PTR s_mesg)
{
      char * temp;

	/*** !!!!???? What does this if do?  Why is it here ????!!!!! ****/
      if ( 0xfe != *( ((unsigned char *) s_mesg ) + sizeof( s_mesg ) ) )
      {
         if ( auto_resp &&
	    (Current_Status != STATUS_ONLINE) &&
	    (Current_Status != STATUS_FREE_CHAT)  )
         {
	    switch ( Current_Status & 0x1ff )
	    {
	    case STATUS_OCCUPIED:
	       /* Dup the string so the russian translation only happens once */
	       temp = strdup( auto_rep_str_occ );
	       break;
	    case STATUS_AWAY:
	       temp = strdup( auto_rep_str_away );
	       break;
	    case STATUS_DND:
	       temp = strdup( auto_rep_str_dnd );
	       break;
	    case STATUS_INVISIBLE:
	       temp = strdup( auto_rep_str_inv );
	       break;
	    case STATUS_NA:
	       temp = strdup( auto_rep_str_na );
	       break;
	    default:
	       temp = strdup( auto_rep_str_occ );
	       M_print("You have encounterd a bug in my code :( I now own you a beer!\nGreetings Fryslan!\n");
            }

         icq_sendmsg( sok, Chars_2_DW( s_mesg->uin ), temp, NORM_MESS );
	 free( temp );
	 M_print( "[ Sent auto-reply message to %d(%d)]\n",
	                     Chars_2_DW(s_mesg->uin), last_recv_uin);

	 if ( UIN2nick( last_recv_uin ) != NULL )
            log_event( last_recv_uin, LOG_AUTO_MESS, "Sending an auto-reply message to %s\n", UIN2nick( last_recv_uin ) );
	 else
            log_event( last_recv_uin, LOG_AUTO_MESS, "Sending an auto-reply message to %d\n", last_recv_uin);
	 }
      }
}

static void Multi_Packet( SOK_T sok, BYTE *data )
{
   int num_pack, i;
   int len;
   BYTE * j;
   srv_net_icq_pak pak;
   num_pack = (unsigned char) data[0];
   j = data;
   j++;
   
   for ( i=0; i < num_pack; i++ )
   {
       len = Chars_2_Word( j );
/*       pak = *( srv_net_icq_pak *) j ;*/
       memcpy( &pak, j, sizeof( pak ) );
       j += 2;
#if 0
       M_print ("\nPacket Number %d\n", i);
       M_print ("%s %04X\n", i18n (46, "Length"), len );
       M_print (COMMAND_STR " %04X\n", Chars_2_Word (pak.head.cmd));
       M_print ("%s %04X\n", i18n (48, "SEQ"), Chars_2_Word (pak.head.seq));
       M_print ("%s %04X\n", i18n (49, "Ver"), Chars_2_Word (pak.head.ver));
#endif
      Kill_Prompt();
       Server_Response( sok, pak.data, (len+2) - sizeof( pak.head ), Chars_2_Word( pak.head.cmd ),
                 Chars_2_Word( pak.head.ver ), Chars_2_DW( pak.head.seq )
                 , Chars_2_DW( pak.head.UIN ) );
       j += len;
   }
}

void Server_Response( SOK_T sok, BYTE *data, DWORD len, WORD cmd, WORD ver, DWORD seq, DWORD uin )
{
   SIMPLE_MESSAGE_PTR s_mesg;
   static int loginmsg = 0;
   
   switch ( cmd )
   {
   case SRV_ACK:
      if ( Verbose ) {
	 Kill_Prompt();
	 R_undraw ();
      }
      if (Verbose > 1)
      {
         M_print (i18n (51, "The server acknowledged the %04x command."), last_cmd[ seq>>16 ] );
         M_print ("\nThe SEQ was %04X\n", seq );
      }
      Check_Queue (seq);
      if (Verbose) {
	 if (len != 0)
	 {
	    M_print ("%s %s %d\n", i18n (47, "Extra Data"), i18n (46, "Length"), len );
	    Hex_Dump (data, len);
            M_print ("\n");
	 }
	 R_redraw ();
      }
      break;
   case SRV_META_USER:
      R_undraw ();
      Meta_User( sok, data, len, uin );
      R_redraw ();
      break;
   case SRV_MULTI_PACKET:
/*      printf( "\n" );
      Hex_Dump( data, len );*/
      Multi_Packet( sok, data );
      break;
   case SRV_NEW_UIN:
      R_undraw ();
      M_print( "The new UIN is %ld!\n", uin );
      R_redraw ();
      break;
   case SRV_UPDATE_FAIL:
      R_undraw ();
      M_print( "Failed to update info.\n" );
      R_redraw ();
      break;
   case SRV_UPDATE_SUCCESS:
      R_undraw ();
      M_print( "User info successfully updated.\n" );
      R_redraw ();
      break;      
   case SRV_LOGIN_REPLY:
/*      UIN = Chars_2_DW( &pak.data[0] ); */
      R_undraw ();
      our_ip = Chars_2_DW( &data[0] );
      Time_Stamp();
      M_print (" " MAGENTA BOLD "%10lu" NOCOL " %s\n", uin, i18n (50, "Login successful!"));
      if (loginmsg++)
        break;
      Time_Stamp ();
#if ICQ_VER == 0x0002
      M_print (" " MAGENTA BOLD "%10s" NOCOL " IP: %u.%u.%u.%u\n", UIN2Name (uin), data[4], data[5], data[6], data[7]);
#elif ICQ_VER == 0x0004
      M_print (" " MAGENTA BOLD "%10s" NOCOL " IP: %u.%u.%u.%u\n", UIN2Name (uin), data[0], data[1], data[2], data[3]);
#else
      M_print (" " MAGENTA BOLD "%10s" NOCOL " IP: %u.%u.%u.%u\n", UIN2Name (uin), data[12], data[13], data[14], data[15]);
#endif
      R_redraw ();
      snd_login_1( sok );
      snd_contact_list( sok );
      snd_invis_list( sok );
      snd_vis_list( sok );
      Current_Status = set_status;
/*      icq_change_status( sok, set_status );*/
/*      Prompt();*/
      break;
   case SRV_RECV_MESSAGE:
         R_undraw();
         Recv_Message( sok, data );
	 M_print( "\n" );
         R_redraw();
      break;
   case SRV_X1: 
      R_undraw ();
      if ( Verbose ) {
         M_print( "Acknowleged SRV_X1 0x021C Done Contact list?\n" );
      }
      Show_Quick_Online_Status();
     /* Show_Quick_Status();*/
      R_redraw ();
      Done_Login = TRUE;
      break;
   case SRV_X2: 
/*      Kill_Prompt();*/
      if ( Verbose ) {
         R_undraw ();
         M_print( "Acknowleged SRV_X2 0x00E6 Done old messages?\n" );
         R_redraw ();
      }
      snd_got_messages( sok );
      break;
   case SRV_INFO_REPLY:
      R_undraw ();
      Display_Info_Reply( sok, data );
      M_print( "\n" );
      R_redraw ();
      break;
   case SRV_EXT_INFO_REPLY:
      R_undraw ();
      Display_Ext_Info_Reply( sok, data );
      M_print( "\n" );
      R_redraw ();
      break;
   case SRV_USER_OFFLINE:
      R_undraw ();
      User_Offline( sok, data );
      R_redraw ();
      break;
   case SRV_BAD_PASS:
      R_undraw ();
      M_print( MESSCOL "You entered an incorrect password." NOCOL "\n" );
      exit( 1 );
      break;
   case SRV_TRY_AGAIN:
      R_undraw ();
      M_print( MESSCOL "Server is busy please try again.\nTrying again...\n");
#ifdef UNIX
  #ifndef __BEOS__
      if ( fork() == 0 )
      {
         sleep( 5 );
         Login( sok, UIN, &passwd[0], our_ip, our_port, set_status );
         exit( 0 );
      }
  #else
      Login( sok, UIN, &passwd[0], our_ip, our_port, set_status );
  #endif
#else
      Login( sok, UIN, &passwd[0], our_ip, our_port, set_status );
#endif
     R_redraw ();
     break;
   case SRV_USER_ONLINE:
      User_Online( sok, data );
      break;
   case SRV_STATUS_UPDATE:
      R_undraw ();
      Status_Update( sok, data );
      /*M_print( "\n" );*/
      R_redraw ();
      break;
   case SRV_GO_AWAY:
   case SRV_NOT_CONNECTED:
      R_undraw ();
      Time_Stamp();
      #ifdef FUNNY_MSGS 
      M_print (" Server sent \"Go away!!\" command. Lag sucks! :(\n" );
      #else
      M_print (" Server has forced us to disconnect.  This may be because of network lag.\n" );
      #endif
      R_redraw ();
      Quit = TRUE;
      break;
   case SRV_END_OF_SEARCH:
      R_undraw ();
      M_print (i18n (45, "Search Done."));
      if (len >= 1) {
         M_print ("\t");
         if (data[0] == 1) 
         {
            M_print (i18n (44, "Too many users found."));
	 } else {
            M_print (i18n (43, "All users found."));
	 }
      } 
      M_print ("\n");
      R_redraw ();
      break;
   case SRV_USER_FOUND:
      R_undraw ();
      Display_Search_Reply( sok, data );
      M_print ("\n");
      R_redraw ();
      break;
   case SRV_RAND_USER:
      R_undraw ();
      Display_Rand_User( sok, data, len );
      M_print ("\n");
      R_redraw ();
      break;
   case SRV_SYS_DELIVERED_MESS:
	/***  There are 2 places we get messages!! */
	/*** Don't edit here unless you're sure you know the difference */
	/*** Edit Do_Msg() in icq_response.c so you handle all messages */
	/*** doing msg does not cause cancer that is all thank you */
      R_undraw ();
      s_mesg = ( SIMPLE_MESSAGE_PTR ) data;
      if (!( ( NULL == UIN2Contact( Chars_2_DW( s_mesg->uin ) ) ) &&
         ( Hermit ) ))
      {
        last_recv_uin = Chars_2_DW( s_mesg->uin );
        Time_Stamp();
        M_print ("\a " CYAN BOLD "%10s" NOCOL " ", UIN2Name (Chars_2_DW (s_mesg->uin)));
        /*
        if ( 0 == ( Chars_2_Word( s_mesg->type ) & MASS_MESS_MASK ) )
           M_print (i18n (32, " - Instant Message"));
        else
           M_print (i18n (33, " - Instant " SERVCOL "Mass" NOCOL " Message"));
        M_print ("\a ");
        */
        if ( Verbose )
           M_print( " Type = %04x\t", Chars_2_Word( s_mesg->type ) );
        Do_Msg( sok, Chars_2_Word( s_mesg->type ), Chars_2_Word( s_mesg->len ), 
                s_mesg->len + 2,last_recv_uin);
        Auto_Reply ( sok, s_mesg );
      }
      R_redraw ();
      break;
   case SRV_AUTH_UPDATE:
       break;
   default: /* commands we dont handle yet */
       R_undraw ();
       Time_Stamp();
       M_print( " " CLIENTCOL "The response was %04X\t", cmd );
       M_print( "The version was %X\t", ver  );
       M_print( "\nThe SEQ was %04X\t", seq  );
       M_print( "The size was %d\n", len );
       if ( Verbose )
       {
            if ( len > 0 )
                Hex_Dump( data, len );
       }
       M_print( NOCOL "\n" );
       R_redraw ();
       break;
   }
}

