#include "datatype.h"
#include "micq.h"
#include "mreadline.h"
#include <stdlib.h>
#include <stdio.h>
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

char *Get_Occupation( WORD x )
{
	switch ( x ) {
		case 1: return "Academic";
		case 2: return "Administrative";
		case 3: return "Art/Entertainmant";
		case 4: return "College Student";
		case 5: return "Computers";
		case 6: return "Community & Social";
		case 7: return "Education";
		case 8: return "Engineering";
		case 9: return "Financial Services";
		case 10: return "Government";
		case 11: return "High School Student";
		case 12: return "Home";
		case 13: return "ICQ - Providing Help";
		case 14: return "Law";
		case 15: return "Managerial";
		case 16: return "Manufacturing";
		case 17: return "Medical/Health";
		case 18: return "Military";
		case 19: return "Non-government Organization";
		case 20: return "Professional";
		case 21: return "Retail";
		case 22: return "Retired";
		case 23: return "Science & Research";
		case 24: return "Sports";
		case 25: return "Technical";
		case 26: return "University Student";
		case 27: return "Web Building";
		case 99: return "Other Services";
		default: return "Not Entered";
	}
}

char * Get_Interest_Name( WORD x )
{
	switch ( x )
	{
    		case 100:  return "Art               ";
	        case 101:  return "Cars              ";
	        case 102:  return "Celebrity Fans    ";
        	case 103:  return "Collections       ";
	        case 104:  return "Computers         ";
	        case 105:  return "Culture & Literature ";
	        case 106:  return "Fitness           ";
	        case 107:  return "Games             ";
	        case 108:  return "Hobbies           ";
	        case 109:  return "ICQ - Providing Help ";
	        case 110:  return "Internet          ";
	        case 111:  return "Lifestyle         ";
	        case 112:  return "Movies/TV         ";
	        case 113:  return "Music             ";
	        case 114:  return "Outdoor Activities";
	        case 115:  return "Parenting         ";
	        case 116:  return "Pets/Animals      ";
	        case 117:  return "Religion          ";
	        case 118:  return "Science/Technology";
	       	case 119:  return "Skills            ";
		case 120:  return "Sports            ";
	        case 121:  return "Web Design        ";
	        case 122:  return "Nature and Environment";
	        case 123:  return "News & Media      ";
	        case 124:  return "Government        ";
	        case 125:  return "Business & Economy";
	        case 126:  return "Mystics           ";
	        case 127:  return "Travel            ";
	        case 128:  return "Astronomy         ";
	        case 129:  return "Space             ";
	        case 130:  return "Clothing          ";
	        case 131:  return "Parties           ";
	        case 132:  return "Women             ";
	        case 133:  return "Social science    ";
	        case 134:  return "60's              ";
	        case 135:  return "70's              ";
	        case 136:  return "80's              ";
	        case 137:  return "50's              ";
	        case 199:  return "Other             ";
	        default   : return NULL;
	}
}


char * Month[13] = { NON_MONTH_STR,
			JAN_MONTH_STR,
			FEB_MONTH_STR,
			MAR_MONTH_STR,
			APR_MONTH_STR,
			MAY_MONTH_STR,
			JUN_MONTH_STR,
			JUL_MONTH_STR,
			AUG_MONTH_STR,
			SEP_MONTH_STR,
			OCT_MONTH_STR,
			NOV_MONTH_STR,
			DEC_MONTH_STR };


BOOL Show_Short_String( BYTE **data, char *str )
{
   int len;

	 len = Chars_2_Word( (*data) );
	 *data +=2;
	 if ( len != 1 )
	 {
	 	(*data)[ len-1 ] = 0; /* be safe */
                char_conv( "wc", *data );
		 M_print( "%s%s",str, *data );
	        *data += len;
                return TRUE;
	 }
	 *data += len;
         return FALSE;
}

void Show_String( BYTE **data, char *str )
{
   if ( Show_Short_String( data, str ) ) {
      M_print( "\n" );
   }
}

void Handle_Interest( BYTE count, BYTE *data )
{
	char * nomen;
	for ( ; count; count-- ) {
		nomen =  Get_Interest_Name( Chars_2_Word( data ) );
		if ( nomen == NULL ) {
			M_print( SERVCOL "%d\t: " MESSCOL, Chars_2_Word( data ) );
		} else {
			M_print( SERVCOL "%s\t: " MESSCOL, Get_Interest_Name( Chars_2_Word( data ) ) );
		}
		data += 2;	
		Show_Short_String( &data, "" );
		M_print( NOCOL "\n" );
	}
}

void Meta_User( SOK_T sok, BYTE *data, DWORD len, DWORD uin )
{
   WORD subcmd;
   DWORD zip;
   char * newline;
   /*int len_str;*/
   
   subcmd = Chars_2_Word( data );
   
   switch ( subcmd ) {
      case META_SRV_PASS:
         M_print( "Password Change was " CLIENTCOL "%s" NOCOL ".\n", data[2] == 0xA ? "successful" : "unsuccessful" );
      break;
      case META_SRV_ABOUT_UPDATE:
         M_print( "About info Change was " CLIENTCOL "%s" NOCOL ".\n", data[2] == 0xA ? "successful" : "unsuccessful" );
      break;
      case META_SRV_GEN_UPDATE:
         M_print( "Info Change was " CLIENTCOL "%s" NOCOL ".\n", data[2] == 0xA ? "successful" : "unsuccessful" );
      break;
      case META_SRV_OTHER_UPDATE:
         M_print( "Other Info Change was " CLIENTCOL "%s" NOCOL ".\n", data[2] == 0xA ? "successful" : "unsuccessful" );
      break;
      case META_SRV_MORE:
      	 data+=2;
	 if ( *data != 0x0A ) break;
	 data++;
	 if ( Chars_2_Word( data ) != (WORD) -1 )
	         M_print( SERVCOL AGE_INFO_STR "%d    \t", Chars_2_Word( data ) );
	 else 		
	         M_print( SERVCOL AGE_INFO_STR INFO_NE_STR "\t" );
	 data+=2;
	 if ( *data == 1 )
            M_print( FEMALE_INFO_STR "\n" );
	 else if ( *data == 2 )
            M_print( MALE_INFO_STR "\n" );
	 else
	      M_print( SEX_UNKNOWN_INFO_STR "\n" );
         data++;
         Show_String( &data, HOMEPAGE_INFO_STR ); 
	newline = "";
	if ( ( *(data+1) > 0 ) && ( *(data+1) <= 12 ) ) {
        	M_print( BIRTH_INFO_STR "%s %02d, %4d \t", Month[ *(data+1) ], *(data+2), *(data)+1900 );
		newline = "\n";
	}
	 data += 3;
         if ( ( ( *data != 0 ) && ( *data != 0xff ) )  ||
		( ( *(data+1) != 0 ) && ( *(data+1) != 0xff ) )  ||
		( ( *(data+2) != 0 ) && ( *(data+2) != 0xff ) ) ) {
		 M_print( LANGUAGE_INFO_STR ); 
		 if ( Get_Lang_Name( *data ) != NULL ) {
		 	M_print( "%s ", Get_Lang_Name( *data ) );
		 } else {
		 	M_print( "%X ", *data );
		 }
		 data++;
		 if ( Get_Lang_Name( *data ) != NULL ) {
		 	M_print( "%s ", Get_Lang_Name( *data ) );
		 } else {
		 	M_print( "%X ", *data );
		 }
		 data++;
		 if ( Get_Lang_Name( *data ) != NULL ) {
		 	M_print( "%s ", Get_Lang_Name( *data ) );
		 } else {
		 	M_print( "%X ", *data );
	 	}
		newline = "\n";
         }
	M_print( NOCOL "%s", newline );
      break;
      case META_SRV_ABOUT:
         data += 2;
	 if ( *data != 0x0A ) break;
	 data++;
         Show_String( &data, SERVCOL ABOUT_INFO_STR "\n" CLIENTCOL );
	 M_print( NOCOL );
      break;
      case META_SRV_WORK:
         data += 2;
	 if ( *data != 0x0A ) break;
	 data++;
         if ( ( Chars_2_Word( data ) > 1 )||
            ( Chars_2_Word( data + 2 + Chars_2_Word( data ) ) > 1 ) ) { 
            M_print( SERVCOL WORK_LOCATION_INFO_STR );
            Show_Short_String( &data, "" );
            Show_Short_String( &data, ", " );
            M_print( "\n" );
         } else {
            data += 6;
         }
	 Show_String( &data, SERVCOL WORK_PHONE_INFO_STR );
	 Show_String( &data, SERVCOL WORK_FAX_INFO_STR );
	 Show_String( &data, SERVCOL WORK_ADDRESS_INFO_STR );
	 if ( Chars_2_DW( data ) != 0 )  {
		newline = "\n";
             M_print( SERVCOL WORK_ZIP_INFO_STR "%05d\t", Chars_2_DW ( data ) );
	} else { newline = ""; }
	 data += 4;
	 if ( 0 != Chars_2_Word( data) ) {
		 if ( Get_Country_Name( Chars_2_Word(data) ) != NULL )
		    M_print( SERVCOL WORK_COUNTRY_INFO_STR "%s\n",Get_Country_Name( Chars_2_Word(data) ) );
		 else
		    M_print( SERVCOL WORK_COUNTRY_CODE_INFO_STR "%d\n", Chars_2_Word( data ) );
		newline = "";
	 }
	M_print( "%s", newline);
	 data += 2;
	 Show_String( &data, SERVCOL WORK_COMP_NAME_INFO_STR );
	 Show_String( &data, SERVCOL WORK_DEPT_INFO_STR );
	 Show_String( &data, SERVCOL WORK_JOB_POS_INFO_STR );
	if ( 0 != Chars_2_Word( data ) ) 
		 M_print( SERVCOL WORK_OCCUPATION_INFO_STR "%s\n", Get_Occupation( Chars_2_Word( data)) );
	 data += 2;
	 Show_String( &data, SERVCOL WORK_HOMEPAGE_INFO_STR );
      	 M_print( NOCOL );
      break;
      case META_SRV_GEN:
         data += 2;
	 if ( *data != 0x0A ) break;
	 data++;
	 Show_String( &data, SERVCOL NICKNAME_INFO_STR CONTACTCOL );
	 /*Show_String( &data, SERVCOL "First Name   : " );
	 Show_String( &data, SERVCOL "Last Name    : " );*/
         M_print( SERVCOL NAME_INFO_STR );
         Show_Short_String( &data, "" );
         Show_Short_String( &data, "\t" );
         M_print( "\n" );
	 Show_String( &data, SERVCOL EMAIL_INFO_STR );
	 Show_String( &data, SERVCOL OTHER_EMAIL_INFO_STR );
	 Show_String( &data, SERVCOL OLD_EMAIL_INFO_STR );
         if ( ( Chars_2_Word( data ) > 1 )||
            ( Chars_2_Word( data + 2 + Chars_2_Word( data ) ) > 1 ) ) {
            M_print( SERVCOL LOCATION_INFO_STR );
            Show_Short_String( &data, "" );
            Show_Short_String( &data, ", " );
            M_print( "\n" );
         } else {
            data += 6;
         }
	 /*Show_String( &data, SERVCOL "City         : " );
	 Show_String( &data, SERVCOL "State        : " );*/

	 Show_String( &data, SERVCOL PHONE_INFO_STR );
	 Show_String( &data, SERVCOL FAX_INFO_STR );
	 Show_String( &data, SERVCOL STREET_INFO_STR );
	 Show_String( &data, SERVCOL CELLULAR_INFO_STR );
	 zip = Chars_2_DW( data );
	 data += 4;
	 if ( ((signed long)zip) > 0 )
	 	M_print(     SERVCOL ZIP_INFO_STR "%05u\n", zip );
	 if ( Get_Country_Name( Chars_2_Word(data) ) != NULL )
	    M_print( COUNTRY_INFO_STR "%s\t",Get_Country_Name( Chars_2_Word(data) ) );
	 else
	    M_print( COUNTRY_CODE_INFO_STR "%d\t", Chars_2_Word( data ) );
	 data += 2;
	 if ( ((signed char) *data)>>1 != -50 )
		 M_print( "(GMT %+d)" NOCOL "\n", ((signed char) *data)>>1  );
 	 else
	 	 M_print( NOCOL "\n" );
      break; 
      case 0x00F0:
	data+=3;
	Handle_Interest( (BYTE)*data, data+1 );
	break;
      case 0x00FA: /* Silently ignor these so as not to confuse people*/
	if ( data[2] == 0x14 ) {
		M_print( "Search " CLIENTCOL "failed" NOCOL ".\n" );
		break;
	}
      case 0x010E:
      	if ( ! Verbose ) break;
      default:
         M_print( "Unknown Meta User response " SERVCOL "%04X" NOCOL "\n", subcmd );
	 if ( Verbose ) {
	    Hex_Dump( data, len );
	    M_print( "\n" );
	 }
      break;
   }
}

void Display_Rand_User( SOK_T sok, BYTE *data, DWORD len )
{
   if ( len == 37 ) {
      M_print( "Random User :\t%d\n", Chars_2_DW( &data[0] ) );
      M_print( "IP          :\t%d.%d.%d.%d : ", data[4], data[5], data[6], data[7]  );
      M_print( "%d\n", Chars_2_DW( &data[8] ) );
      M_print( "IP2         :\t%d.%d.%d.%d\n", data[12], data[13], data[14], data[15]  );
      M_print( "Status      :\t" );
      Print_Status( Chars_2_DW( &data[17] ) );
      M_print( "\nTCP version :\t%d\t", Chars_2_Word( &data[21] ) );
      M_print( "Connection  :\t%s", data[16] == 4 ? "Peer-to-Peer" : "Server Only" );
      if ( Verbose > 1 ) {
         M_print( "\n" );
	 Hex_Dump( data, len );
      }
      send_info_req( sok, Chars_2_DW( data ) );
/*      send_ext_info_req( sok, Chars_2_DW( data ) );*/
   } else {
      M_print( "No Random User Found" );
   }
}

void Recv_Message( int sok, BYTE * pak )
{
   RECV_MESSAGE_PTR r_mesg;

/*   M_print( "\n" );*/
   r_mesg = ( RECV_MESSAGE_PTR )pak;
   last_recv_uin = Chars_2_DW( r_mesg->uin );
   Print_UIN_Name( Chars_2_DW( r_mesg->uin ) );
   M_print( ":\a\nDate %d/%d/%d\t%d:%02d GMT\n", r_mesg->month, r_mesg->day, 
            Chars_2_Word( r_mesg->year ), r_mesg->hour ,  r_mesg->minute );
            
   M_print( "Type : %d \t Len : %d\n", Chars_2_Word( r_mesg->type ),
       Chars_2_Word( r_mesg->len ) );
   Do_Msg( sok, Chars_2_Word( r_mesg->type ), Chars_2_Word( r_mesg->len ), ( r_mesg->len + 2 ), last_recv_uin ); 
   
/*   M_print( MESSCOL "%s\n" NOCOL, ((char *) &r_mesg->len) + 2 );*/
/*   ack_srv( sok, Chars_2_Word( pak.head.seq ) ); */
}


/************************************************
This is called when a user goes offline
*************************************************/
void User_Offline( int sok, BYTE * pak )
{
   int remote_uin;
   int index;

   remote_uin = Chars_2_DW( &pak[0] );

/*   M_print( "\n" );*/
   M_print( CONTACTCOL );
   index = Print_UIN_Name( remote_uin );
   M_print( NOCOL );
   M_print( LOGGED_OFF_STR "\t" );
   Time_Stamp();
/*   M_print( "\n" );*/
   if ( UIN2nick( remote_uin ) != NULL )
      log_event( remote_uin, LOG_ONLINE, "User logged off %s\n", UIN2nick( remote_uin ) );
   else
      log_event( remote_uin, LOG_ONLINE, "User logged off %d\n", remote_uin );
   if ( index != -1 )
   {
      Contacts[ index ].status = STATUS_OFFLINE;
      Contacts[ index ].last_time = time( NULL );
   }
}

void User_Online( int sok, BYTE * pak )
{
   int remote_uin, new_status;
   int index;

   remote_uin = Chars_2_DW( &pak[0] );

   new_status = Chars_2_DW( &pak[17] );
   
   if ( Done_Login )
   {
/*      M_print( "\n" );*/
      R_undraw();
      M_print( CONTACTCOL );
      index = Print_UIN_Name( remote_uin );
      M_print( NOCOL );
      if ( index != -1 )
      {
         Contacts[ index ].status = new_status;
         Contacts[ index ].current_ip[0] =  pak[4];
         Contacts[ index ].current_ip[1] =  pak[5];
         Contacts[ index ].current_ip[2] =  pak[6];
         Contacts[ index ].current_ip[3] =  pak[7];
         Contacts[ index ].other_ip[0] =  pak[12];
         Contacts[ index ].other_ip[1] =  pak[13];
         Contacts[ index ].other_ip[2] =  pak[14];
         Contacts[ index ].other_ip[3] =  pak[15];
         Contacts[ index ].port = Chars_2_DW( &pak[8] );
         Contacts[ index ].last_time = time( NULL );
	Contacts[ index ].TCP_version = Chars_2_Word( &pak[21] );
	Contacts[ index ].connection_type = pak[16];
      }
      M_print( " (" );
      Print_Status( new_status );
      M_print( ")" LOGGED_ON_STR "\t" );
      Time_Stamp();
/*      M_print( "\n" );*/

      if ( UIN2nick( remote_uin ) != NULL )
         log_event( remote_uin, LOG_ONLINE, "User logged on %s\n", UIN2nick( remote_uin ) );
      else
         log_event( remote_uin, LOG_ONLINE, "User logged on %d\n", remote_uin );
      
      if ( Verbose )
      {
         M_print( "\nThe IP address is %u.%u.%u.%u\n", pak[4], pak[5], pak[6], pak[7] );
         M_print( "The \"real\" IP address is %u.%u.%u.%u\n", pak[12], pak[13], pak[14], pak[15] );
	 M_print( "%s\n", pak[16] == 4 ? "Peer-to-Peer mode" : "Server Only Communication." );
	 M_print( "TCP ICQ version : %d\n", Chars_2_Word( &pak[21] ) );
	 Hex_Dump( pak, 0x2B );
      }
      M_print( "\n" );
      R_redraw();
   }
   else
   {
      Kill_Prompt();
      for ( index=0; index < Num_Contacts; index++ )
      {
         if ( Contacts[index].uin == remote_uin )
         {
            Contacts[ index ].status = new_status;
            Contacts[ index ].current_ip[0] =  pak[4];
            Contacts[ index ].current_ip[1] =  pak[5];
            Contacts[ index ].current_ip[2] =  pak[6];
            Contacts[ index ].current_ip[3] =  pak[7];
            Contacts[ index ].port = Chars_2_DW( &pak[8] );
            Contacts[ index ].last_time = time( NULL );
	Contacts[ index ].TCP_version = Chars_2_Word( &pak[21] );
	Contacts[ index ].connection_type = pak[16];
            break;
         }
      }
   }
}

void Status_Update( int sok, BYTE * pak )
{
   CONTACT_PTR bud;
   int remote_uin, new_status;
   int index;

   remote_uin = Chars_2_DW( &pak[0] );

   new_status = Chars_2_DW( &pak[4] );
   if ( pak[8] ) 
	   M_print( "%02X\n", pak[8] );
   bud = UIN2Contact( remote_uin );
   if ( bud != NULL ) {
      if ( bud->status == new_status ) {
          Kill_Prompt();
          return;
      }
   }
/*   M_print( "\n" );*/
   M_print( CONTACTCOL );
   index = Print_UIN_Name( remote_uin );
   M_print( NOCOL );
   if ( index != -1 )
   {
      Contacts[ index ].status = new_status;
   }
   M_print( CHANGE_STATUS_STR );
   Print_Status( new_status );
   M_print( "\t" );
   Time_Stamp();
   M_print( "\n" );
   
}

/* This procedure logins into the server with UIN and pass
   on the socket sok and gives our ip and port.
   It does NOT wait for any kind of a response.         */
void Login( int sok, int UIN, char *pass, int ip, int port, DWORD status )
{
   net_icq_pak pak;
   int size;
   WORD passlen;
   login_1 s1;
   login_2 s2;
	struct sockaddr_in sin;  /* used to store inet addr stuff  */
   
   Word_2_Chars( pak.head.ver, ICQ_VER );
   Word_2_Chars( pak.head.cmd, CMD_LOGIN );
   Word_2_Chars( pak.head.seq, seq_num++ );
   DW_2_Chars( pak.head.UIN, UIN );
   
   DW_2_Chars( s1.port, ntohs( port ) + 0x1000 );
   passlen = strlen( pass );
   passlen++;
   if ( passlen > 9 ) {
      passlen = 9;
      pass[ passlen-1 ] = 0;
   }
   Word_2_Chars( s1.len, passlen );
   DW_2_Chars( s1.time, time( NULL ) );  
   
   DW_2_Chars( s2.ip, ip );
   sin.sin_addr.s_addr = Chars_2_DW( s2.ip );
   DW_2_Chars( s2.status, status );
/*   Word_2_Chars( s2.seq, seq_num++ );*/
   
   DW_2_Chars( s2.X1, LOGIN_X1_DEF );
   s2.X2[0] = LOGIN_X2_DEF;
   DW_2_Chars( s2.X3, LOGIN_X3_DEF );
   DW_2_Chars( s2.X4, LOGIN_X4_DEF );
   DW_2_Chars( s2.X5, LOGIN_X5_DEF );
   DW_2_Chars( s2.X6, LOGIN_X6_DEF );
   DW_2_Chars( s2.X7, LOGIN_X7_DEF );
   DW_2_Chars( s2.X8, LOGIN_X8_DEF );
   
   memcpy( pak.data, &s1, sizeof( s1 ) );
   size = sizeof( s1 );
   memcpy( &pak.data[size], pass, Chars_2_Word( s1.len ) );
   size += Chars_2_Word( s1.len );
   memcpy( &pak.data[size], &s2.X1, sizeof( s2.X1 ) );
   size += sizeof( s2.X1 );
   memcpy( &pak.data[size], &s2.ip, sizeof( s2.ip ) );
   size += sizeof( s2.ip );
   memcpy( &pak.data[size], &s2.X2, sizeof( s2.X2 ) );
   size += sizeof( s2.X2 );
   memcpy( &pak.data[size], &s2.status, sizeof( s2.status ) );
   size += sizeof( s2.status );
   memcpy( &pak.data[size], &s2.X3, sizeof( s2.X3 ) );
   size += sizeof( s2.X3 );
/*   memcpy( &pak.data[size], &s2.seq, sizeof( s2.seq ) );
/   size += sizeof( s2.seq );*/
   memcpy( &pak.data[size], &s2.X4, sizeof( s2.X4 ) );
   size += sizeof( s2.X4 );
   memcpy( &pak.data[size], &s2.X5, sizeof( s2.X5 ) );
   size += sizeof( s2.X5 );
   memcpy( &pak.data[size], &s2.X6, sizeof( s2.X6 ) );
   size += sizeof( s2.X6 );
   memcpy( &pak.data[size], &s2.X7, sizeof( s2.X7 ) );
   size += sizeof( s2.X7 );
   memcpy( &pak.data[size], &s2.X8, sizeof( s2.X8 ) );
   size += sizeof( s2.X8 );
#if ICQ_VER == 0x0004
   last_cmd[ seq_num - 1 ] = Chars_2_Word( pak.head.cmd );
#elif ICQ_VER == 0x0005
   last_cmd[ seq_num - 1 ] = Chars_2_Word( pak.head.cmd );
#else
   last_cmd[ seq_num - 2 ] = Chars_2_Word( pak.head.cmd );
#endif
   SOCKWRITE( sok, &(pak.head.ver), size + sizeof( pak.head )- 2 );
} 

/* This routine sends the aknowlegement cmd to the
   server it appears that this must be done after
   everything the server sends us                 */
void ack_srv( SOK_T sok, DWORD seq )
{
   net_icq_pak pak;
   
   Word_2_Chars( pak.head.ver, ICQ_VER );
   Word_2_Chars( pak.head.cmd, CMD_ACK );
   DW_2_Chars( pak.head.seq2, seq );
   DW_2_Chars( pak.head.UIN, UIN);
   DW_2_Chars( pak.data, rand() );
   
   SOCKWRITE( sok, &(pak.head.ver), sizeof( pak.head ) - 2 + 4 );
/*   Hex_Dump( &(pak.head.ver), sizeof( pak.head ) - 2 + 4 );*/

}

void Display_Info_Reply( int sok, BYTE * pak )
{
   char *tmp;
   int len;
   
   M_print( SERVCOL "Info for %ld\n", Chars_2_DW( &pak[0] ) );
   len = Chars_2_Word( &pak[4] );
   char_conv( "wc", &pak[6] );
   M_print( "Nick Name :\t%s\n", &pak[6] );
   tmp = &pak[6 + len ];
   len = Chars_2_Word( tmp );
   char_conv( "wc", tmp+2 );
   M_print( "First name :\t%s\n", tmp+2 );
   tmp += len + 2;
   len = Chars_2_Word( tmp );
   char_conv( "wc", tmp+2 );
   M_print( "Last name :\t%s\n", tmp+2 );
   tmp += len + 2;
   len = Chars_2_Word( tmp );
   char_conv( "wc", tmp+2 );
   M_print( "Email Address :\t%s\n", tmp+2 );
   tmp += len + 2;
   if ( *tmp == 1 )
   {
      M_print( "No authorization needed." NOCOL " " );
   }
   else
   {
      M_print( "Must request authorization." NOCOL " " );
   }
/*   ack_srv( sok, Chars_2_Word( pak.head.seq ) ); */
}

void Display_Ext_Info_Reply( int sok, BYTE * pak )
{
   unsigned char *tmp;
   int len;

   M_print( SERVCOL "More Info for %ld\n", Chars_2_DW( &pak[0] ) );
   len = Chars_2_Word( &pak[4] );
   char_conv( "wc", &pak[6] );
   M_print( "City         :\t%s\n", &pak[6] );
   if ( Get_Country_Name( Chars_2_Word(&pak[6+len]) ) != NULL )
      M_print( "Country      :\t%s\n",Get_Country_Name( Chars_2_Word(&pak[6+len]) ) );
   else
      M_print( "Country Code :\t%d\n", Chars_2_Word( &pak[6+len] ) );
   M_print( "Time Zone    :\tGMT %+d\n", ((signed char) pak[len+8])>>1  );
   tmp = &pak[9 + len ];
   len = Chars_2_Word( tmp );
   char_conv( "wc", tmp+2 );
   M_print( "State        :\t%s\n", tmp+2 );
   if ( Chars_2_Word( tmp+2+len ) != 0xffff )
      M_print( "Age          :\t%d\n", Chars_2_Word( tmp+2+len ) );
   else
      M_print( "Age          :\tNot Entered\n");
   if (*(tmp + len + 4) == 2 )
      M_print( "Sex          :\tMale\n" );
   else if (*(tmp + len + 4) == 1 )
      M_print( "Sex          :\tFemale\n" );
   else
#ifdef FUNNY_MSGS
      M_print( "Sex          :\tYes please!\n" );
#else
      M_print( "Sex          :\tNot specified\n" );
#endif
   tmp += len + 5;
   len = Chars_2_Word( tmp );
   char_conv( "wc", tmp+2 );
   M_print( "Phone Number :\t%s\n", tmp+2 );
   tmp += len + 2;
   len = Chars_2_Word( tmp );
   char_conv( "wc", tmp+2 );
   M_print( "Home Page    :\t%s\n", tmp+2 );
   tmp += len + 2;
   len = Chars_2_Word( tmp );
   char_conv( "wc", tmp+2 );
   M_print( "About        :\n%s", tmp+2 );
/*   ack_srv( sok, Chars_2_Word( pak.head.seq ) ); */
}

void Display_Search_Reply( int sok, BYTE * pak )
{
   char *tmp;
   int len;
   M_print( SERVCOL "User found %ld\n", Chars_2_DW( &pak[0] ) );
   len = Chars_2_Word( &pak[4] );
   char_conv( "wc", &pak[6] );
   M_print( "Nick Name :\t%s\n", &pak[6] );
   tmp = &pak[6 + len ];
   len = Chars_2_Word( tmp );
   char_conv( "wc", tmp+2 );
   M_print( "First name :\t%s\n", tmp+2 );
   tmp += len + 2;
   len = Chars_2_Word( tmp );
   char_conv( "wc", tmp+2 );
   M_print( "Last name :\t%s\n", tmp+2 );
   tmp += len + 2;
   len = Chars_2_Word( tmp );
   char_conv( "wc", tmp+2 );
   M_print( "Email Address :\t%s\n", tmp+2 );
   tmp += len + 2;
   if ( *tmp == 1 )
   {
      M_print( "No authorization needed." NOCOL " " );
   }
   else
   {
      M_print( "Must request authorization." NOCOL " " );
   }
}

void Do_Msg( SOK_T sok, DWORD type, WORD len, char * data, DWORD uin )
{
   char *tmp = NULL;
   int   x,
	 m;
   char message[11264];
   char url_data[5120];
   char url_desc[5120];

#ifdef MSGEXEC
    char *cmd = NULL, *who = NULL;
    int script_exit_status = -1;
#endif

   add_tab( uin );	/* Adds <uin> to the tab-list */

#ifdef MSGEXEC
    /*
     * run our script if we have one, but only
     * if we have one (submitted by Benjamin Simon)
     */
    if(receive_script[0] != '\0')
    {
        if(UIN2nick( uin ) != NULL)
        {
            who = strdup(UIN2nick( uin ));
        }
        else
        {
            who = (char*)malloc(20);
            sprintf(who, "%ld", uin);
        }

        cmd = (char*)malloc(strlen(receive_script) +
                            strlen(data)           +
                            strlen(who)            +
                            20);

        sprintf(cmd, "%s %s %ld '%s'",
                receive_script,
                who,
                type,
                data);
        script_exit_status = system(cmd);
        if(script_exit_status != 0)
        {
            M_print( "Script command %s failed with %d exit value",
                     script_exit_status);
        }
    }
#endif

   if ( type == USER_ADDED_MESS )
   {
      tmp = strchr( data, '\xFE' );
      if ( tmp == NULL )
      {
         M_print( "Ack!!!!!!!  Bad packet" );
         return;
      }
      *tmp = 0;
      M_print( CONTACTCOL "\n%s" NOCOL " has added you to their contact list.\n", data );
      tmp++;
      data = tmp;
      tmp = strchr( tmp, '\xFE' );
      if ( tmp == NULL )
      {
         M_print( "Ack!!!!!!!  Bad packet" );
         return;
      }
      *tmp = 0;
      char_conv ("wc",data);
      M_print( "First name    : " MESSCOL "%s" NOCOL "\n" , data );
      tmp++;
      data = tmp;
      tmp = strchr( tmp, '\xFE' );
      if ( tmp == NULL )
      {
         M_print( "Ack!!!!!!!  Bad packet" );
         return;
      }
      *tmp = 0;
      char_conv ("wc",data);
      M_print( "Last name     : " MESSCOL "%s" NOCOL "\n" , data );
      tmp++;
      data = tmp;
      tmp = strchr( tmp, '\xFE' );
      *tmp = 0;
      char_conv ("wc",data);
      M_print( "Email address : " MESSCOL "%s" NOCOL "\n" , data );
   }
   else if ( type == AUTH_REQ_MESS )
   {
      tmp = strchr( data, '\xFE' );
      *tmp = 0;
      M_print( CONTACTCOL "\n%s" NOCOL " has requested your authorization to be added to their contact list.\n", data );
      tmp++;
      data = tmp;
      tmp = strchr( tmp, '\xFE' );
      if ( tmp == NULL )
      {
         M_print( "Ack!!!!!!!  Bad packet" );
         return;
      }
      *tmp = 0;
      char_conv ("wc",data);
      M_print( "First name    : " MESSCOL "%s" NOCOL "\n" , data );
      tmp++;
      data = tmp;
      tmp = strchr( tmp, '\xFE' );
      if ( tmp == NULL )
      {
         M_print( "Ack!!!!!!!  Bad packet" );
         return;
      }
      *tmp = 0;
      char_conv ("wc",data);
      M_print( "Last name     : " MESSCOL "%s" NOCOL "\n" , data );
      tmp++;
      data = tmp;
      tmp = strchr( tmp, '\xFE' );
      if ( tmp == NULL )
      {
         M_print( "Ack!!!!!!!  Bad packet" );
         return;
      }
      *tmp = 0;
      char_conv ("wc",data);
      M_print( "Email address : " MESSCOL "%s" NOCOL "\n" , data );
      tmp++;
      data = tmp;
      tmp = strchr( tmp, '\xFE' );
      if ( tmp == NULL )
      {
         M_print( "Ack!!!!!!!  Bad packet" );
         return;
      }
      *tmp = 0;
      tmp++;
      data = tmp;
      tmp = strchr( tmp, '\x00' );
      if ( tmp == NULL )
      {
         M_print( "Ack!!!!!!!  Bad packet" );
         return;
      }
      *tmp = 0;
      char_conv ("wc",data);
      M_print( "Reason : " MESSCOL "%s" NOCOL "\n" , data );
   }
   else if ((type == EMAIL_MESS )|| (type == WEB_MESS) ) {
	tmp = strchr( data, '\xFE' );
	*tmp = 0;
	M_print( "\n%s ", data );
	tmp++;
	data=tmp;
	tmp = strchr( data, '\xFE' );
	tmp++;
	data=tmp;

        tmp = strchr( data, '\xFE' );
        tmp++;
	data= tmp;

        tmp = strchr( data, '\xFE' );
	*tmp = 0;
        if ( type == EMAIL_MESS )
		M_print("<%s> emailed you a message:\n", data);
	else
		M_print("<%s> send you a web message:\n", data);
	tmp++;
	data=tmp;
	tmp = strchr( data, '\xFE' );
	*tmp = 0;
	if ( Verbose ) {
		M_print( "??? '%s'\n", data );
	}
	tmp ++;
	data=tmp;
	M_print( MESSCOL "%s" NOCOL "\n", data );
   }
   else if (type == URL_MESS || type == MRURL_MESS)
   {

      tmp = strchr( data, '\xFE' );
      if ( tmp == NULL )
      {
         M_print( "Ack!!!!!!!  Bad packet" );
         return;
      }
      *tmp = 0;
      char_conv ("wc",data);
// temporaryy fix to buffer overflow
// should be solved better -mc
//      strcpy (url_desc,data);
      url_desc[0] = '\0';
      strncat(url_desc,data,sizeof(url_data)-1);

      tmp++;
      data = tmp;
      char_conv ("wc",data);
// same apllies here --mc
//      strcpy (url_data,data);
      url_data[0] = '\0';
      strncat (url_data,data,sizeof(url_data)-1);

// and again
//      sprintf (message,"Description: %s \n                          URL: %s",url_desc,url_data);  
      snprintf (message,sizeof(message),"Description: %s \nURL: %s",url_desc,url_data); 
      if ( UIN2nick( uin ) != NULL )
         log_event( uin, LOG_MESS, "You received URL message from %s\n%s\n", UIN2nick(uin), message );
      else
         log_event( uin, LOG_MESS, "You received URL message from %d\n%s\n", uin, message );

      M_print( " URL Message.\n Description: " MESSCOL "%s" NOCOL "\n", url_desc );
      M_print(               " URL        : " MESSCOL "%s" NOCOL "\n", url_data );
   }
	else if (type == CONTACT_MESS || type==MRCONTACT_MESS)
	{
      tmp = strchr( data, '\xFE' );
      *tmp = 0;
      M_print( "\nContact List.\n" MESSCOL "============================================\n" NOCOL "%d Contacts\n", atoi(data) );
      tmp++;
      m = atoi(data);
      for(x=0; m > x ; x++)
      {
         data = tmp;
         tmp = strchr( tmp, '\xFE' );
         *tmp = 0;
         M_print( CONTACTCOL "%s\t\t\t", data );
         tmp++;
         data = tmp;
         tmp = strchr( tmp, '\xFE' );
         *tmp = 0;
         M_print( MESSCOL "%s" NOCOL "\n" , data );
         tmp++;
      }
	}
   else
   {
      char_conv ("wc",data);
      if ( UIN2nick( uin ) != NULL )
         log_event( uin, LOG_MESS, "You received instant message from %s\n%s\n", UIN2nick(uin), data );
      else
         log_event( uin, LOG_MESS, "You received instant message from %d\n%s\n", uin, data );
      M_print( MESSCOL "\n%s", data );
      M_print( NOCOL " " );
   }
      /* aaron
         If we just received a message from someone on the contact list,
         save it with the person's contact information. If they are not in
         the list, don't do anything special with it.                       */
      if (UIN2Contact(last_recv_uin) != NULL) {
          UIN2Contact(last_recv_uin)->LastMessage
              = realloc(UIN2Contact(last_recv_uin)->LastMessage,
                        len+5);
          /* I add on so many characters because I always have segfaults in
           my own program when I try to allocate strings like this. Somehow
           it tries to write too much to the string even though I think I
           allocate the right amount. Oh well. It shouldn't be too much
           wasted space, I hope.                                            */
          strcpy(UIN2Contact(last_recv_uin)->LastMessage, data);
      }
      /* end of aaron */
}
