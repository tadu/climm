/*********************************************
**********************************************
This is a file of general utility functions useful
for programming in general and icq in specific

This software is provided AS IS to be used in
whatever way you see fit and is placed in the
public domain.

Author : Matthew Smith April 23, 1998
Contributors :  airog (crabbkw@rose-hulman.edu) May 13, 1998


Changes :
  6-18-98 Added support for saving auto reply messages. Fryslan
 
**********************************************
**********************************************/
#include "micq.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef _WIN32
   #include <io.h>
   #define S_IRUSR        _S_IREAD
   #define S_IWUSR        _S_IWRITE
#else
    #include <sys/time.h>
    #include <netinet/in.h>
    #ifndef __BEOS__
	#include <arpa/inet.h>
    #endif
#endif
#ifdef UNIX
   #include <unistd.h>
   #include <termios.h>
   #include "mreadline.h"
#endif

#ifdef _WIN32
typedef struct {
    long tv_sec;
    long tv_usec;
} timeval;
#endif

static char *Log_Dir = NULL;
static BOOL Normal_Log = TRUE;

typedef struct 
{
   const char *name;
   WORD code;
} LANG_CODE;

static LANG_CODE Lang_Codes[] = { { ARABIC_LANG_STR, 0x01 },
					{ BHOJPURI_LANG_STR, 0x02 },
					{ BULGARIAN_LANG_STR, 0x03 },
					{ BURMESE_LANG_STR, 0x04 },
					{ CANTONESE_LANG_STR, 0x05 }, 
					{ CATALAN_LANG_STR,  0x06},
					{ CHINESE_LANG_STR, 0x07 },
					{ CROATIAN_LANG_STR,  0x08},
					{ CZECH_LANG_STR, 0x09 },
					{ DANISH_LANG_STR, 0x0A },
					{ DUTCH_LANG_STR, 0x0B },
					{ ENGLISH_LANG_STR, 0x0C },
					{ ESPERANTO_LANG_STR, 0x0D },
					{ ESTONIAN_LANG_STR, 0x0E },
					{ FARSI_LANG_STR, 0x0F },
					{ FINNISH_LANG_STR, 0x10 },
					{ FRENCH_LANG_STR, 0x11 },
					{ GAELIC_LANG_STR, 0x12 },
					{ GERMAN_LANG_STR, 0x13 },
					{ GREEK_LANG_STR, 0x14 },
					{ HEBREW_LANG_STR, 0x15 },
					{ HINDI_LANG_STR, 0x16 },
					{ HUNGARIAN_LANG_STR, 0x17 },
					{ ICELANDIC_LANG_STR, 0x18 },
					{ INDONESIAN_LANG_STR, 0x19 },
					{ ITALIAN_LANG_STR, 0x1A },
					{ JAPANESE_LANG_STR, 0x1B },
					{ KHMER_LANG_STR, 0x1C },
					{ KOREAN_LANG_STR, 0x1D },
					{ LAO_LANG_STR, 0x1E },
					{ LATVIAN_LANG_STR, 0x1F },
					{ LITHUANIAN_LANG_STR, 0x20 },
					{ MALAY_LANG_STR, 0x21 },
					{ NORWEGIAN_LANG_STR, 0x22 },
					{ POLISH_LANG_STR, 0x23 },
					{ PORTUGUESE_LANG_STR, 0x24 },
					{ ROMANIAN_LANG_STR, 0x25 },
					{ RUSSIAN_LANG_STR, 0x26 },
					{ SERBOCROATIAN_LANG_STR, 0x27 },
					{ SLOVAK_LANG_STR, 0x28 },
					{ SLOVENIAN_LANG_STR, 0x29 },
					{ SOMALI_LANG_STR, 0x2A },
					{ SPANISH_LANG_STR, 0x2B },
					{ SWAHILI_LANG_STR, 0x2C },
					{ SWEDISH_LANG_STR, 0x2D },
					{ TAGALOG_LANG_STR, 0x2E },
					{ TARTAR_LANG_STR, 0x2F },
					{ THAI_LANG_STR, 0x30 },
					{ TURKISH_LANG_STR, 0x31 },
					{ UKRAINIAN_LANG_STR, 0x32 },
					{ URDU_LANG_STR, 0x33 },
					{ VIETNAMESE_LANG_STR, 0x34 },
					{ YIDDISH_LANG_STR, 0x35 },
					{ YORUBA_LANG_STR, 0x36 },
					{ AFRIKAANS_LANG_STR, 0x37 },
					{ BOSNIAN_LANG_STR, 0x38 },
					{ PERSIAN_LANG_STR, 0x39 },
					{ ALBANIAN_LANG_STR, 0x3A },
					{ ARMENIAN_LANG_STR, 0x3B },
                                 {NONE_LANG_STR, 0 },
                                 {NONE_LANG_STR, 0xff } };
					
typedef struct  			
{
   const char *name;
   WORD code;
} COUNTRY_CODE;


static COUNTRY_CODE Country_Codes[] = { { USA_COUNTRY_STR, 1 },
                                 { Afghanistan_COUNTRY_STR, 93 },
                                 { Albania_COUNTRY_STR, 355 },
                                 { Algeria_COUNTRY_STR, 213 },
                                 { American_Samoa_COUNTRY_STR, 684 },
                                 { Andorra_COUNTRY_STR, 376 },
                                 { Angola_COUNTRY_STR, 244 },
                                 { Anguilla_COUNTRY_STR, 101 },
                                 { Antigua_COUNTRY_STR, 102 },
                                 { Argentina_COUNTRY_STR, 54 },
                                 { Armenia_COUNTRY_STR, 374 },
                                 { Aruba_COUNTRY_STR, 297 },
                                 { Ascention_Island_COUNTRY_STR, 274 },
                                 { Australia_COUNTRY_STR, 61 },
                                 { Australian_Antartic_Territory_COUNTRY_STR, 6721 },
                                 { Austria_COUNTRY_STR, 43 },
                                 { Azerbaijan_COUNTRY_STR, 934 },
                                 { Bahamas_COUNTRY_STR, 103 },
                                 { Bahrain_COUNTRY_STR, 973 },
                                 { Bangladesh_COUNTRY_STR, 880 },
                                 { Barbados_COUNTRY_STR, 104 },
                                 { Belarus_COUNTRY_STR, 375 },
                                 { Belgium_COUNTRY_STR, 32 },
                                 { Belize_COUNTRY_STR, 501 },
                                 { Benin_COUNTRY_STR, 229 },
                                 { Bermuda_COUNTRY_STR, 105 },
                                 { Bhutan_COUNTRY_STR, 975 },
                                 { Bolivia_COUNTRY_STR, 591 },
                                 { Bosnia_Herzegovina_COUNTRY_STR, 387 },
                                 { Botswana_COUNTRY_STR, 267 },
                                 { Brazil_COUNTRY_STR, 55 },
                                 { British_Virgin_Islands_COUNTRY_STR, 106 },
                                 { Brunei_COUNTRY_STR, 673 },
                                 { Bulgaria_COUNTRY_STR, 359 },
                                 { Burkina_Faso_COUNTRY_STR, 226 },
                                 { Burundi_COUNTRY_STR, 257 },
                                 { Cambodia_COUNTRY_STR, 855 },
                                 { Cameroon_COUNTRY_STR, 237 },
                                 { Canada_COUNTRY_STR, 107 },
                                 { Cape_Verde_Islands_COUNTRY_STR, 238 },
                                 { Cayman_Islands_COUNTRY_STR, 108},
                                 { Central_African_Republic_COUNTRY_STR, 236},
                                 { Chad_COUNTRY_STR, 235},
                                 { Christmas_Island_COUNTRY_STR, 672},
                                 { Cocos_Keeling_Islands_COUNTRY_STR, 6101},
                                 { Comoros_COUNTRY_STR, 2691},
                                 { Congo_COUNTRY_STR, 242},
                                 { Cook_Islands_COUNTRY_STR, 682},
                                 { Chile_COUNTRY_STR, 56 },
                                 { China_COUNTRY_STR, 86 },
                                 { Columbia_COUNTRY_STR, 57 },
                                 { Costa_Rice_COUNTRY_STR, 506 },
                                 { Croatia_COUNTRY_STR, 385 }, /* Observerd */
                                 { Cuba_COUNTRY_STR, 53 },
                                 { Cyprus_COUNTRY_STR, 357 },
                                 { Czech_Republic_COUNTRY_STR, 42 },
                                 { Denmark_COUNTRY_STR, 45 },
                                 { Diego_Garcia_COUNTRY_STR, 246},
                                 { Djibouti_COUNTRY_STR, 253},
                                 { Dominica_COUNTRY_STR, 109},
                                 { Dominican_Republic_COUNTRY_STR, 110},
                                 { Ecuador_COUNTRY_STR, 593 },
                                 { Egypt_COUNTRY_STR, 20 },
                                 { El_Salvador_COUNTRY_STR, 503 },
                                 { Equitorial_Guinea_COUNTRY_STR, 240},
                                 { Eritrea_COUNTRY_STR, 291},
                                 { Estonia_COUNTRY_STR, 372},
                                 { Ethiopia_COUNTRY_STR, 251 },
                                 { Former_Yugoslavia_COUNTRY_STR, 389},
                                 { Faeroe_Islands_COUNTRY_STR, 298},
                                 { Falkland_Islands_COUNTRY_STR, 500},
                                 { Federated_States_of_Micronesia_COUNTRY_STR, 691 },
                                 { Fiji_COUNTRY_STR, 679 },
                                 { Finland_COUNTRY_STR, 358 },
                                 { France_COUNTRY_STR, 33 },
                                 { French_Antilles_COUNTRY_STR, 596 },  /* Leave it */
                                 { French_Antilles_COUNTRY_STR, 5901 }, /* Either on or the other is right :) */
                                 { French_Guiana_COUNTRY_STR, 594 },
                                 { French_Polynesia_COUNTRY_STR, 689 },
                                 { Gabon_COUNTRY_STR, 241 },
                                 { Gambia_COUNTRY_STR, 220 },
                                 { Georgia_COUNTRY_STR, 995 },
                                 { Germany_COUNTRY_STR, 49 },
                                 { Ghana_COUNTRY_STR, 233 },
                                 { Gibraltar_COUNTRY_STR, 350 },
                                 { Greece_COUNTRY_STR, 30 },
                                 { Greenland_COUNTRY_STR, 299 },
                                 { Grenada_COUNTRY_STR, 111 },
                                 { Guadeloupe_COUNTRY_STR, 590 },
                                 { Guam_COUNTRY_STR, 671 },
                                 { Guantanomo_Bay_COUNTRY_STR, 5399 },
                                 { Guatemala_COUNTRY_STR, 502 },
                                 { Guinea_COUNTRY_STR, 224 },
                                 { Guinea_Bissau_COUNTRY_STR, 245 },
                                 { Guyana_COUNTRY_STR, 592 },
                                 { Haiti_COUNTRY_STR, 509 },
                                 { Honduras_COUNTRY_STR, 504 },
                                 { Hong_Kong_COUNTRY_STR, 852 },
                                 { Hungary_COUNTRY_STR, 36 },
                                 { Iceland_COUNTRY_STR, 354 },
                                 { India_COUNTRY_STR, 91 },
                                 { Indonesia_COUNTRY_STR, 62 },
                                 { INMARSAT_COUNTRY_STR, 870 },
                                 { INMARSAT_Atlantic_East_COUNTRY_STR, 870 },
                                 { Iran_COUNTRY_STR, 98 },
                                 { Iraq_COUNTRY_STR, 964 },
                                 { Ireland_COUNTRY_STR, 353 },
                                 { Israel_COUNTRY_STR, 972 },
                                 { Italy_COUNTRY_STR, 39 },
                                 { Ivory_Coast_COUNTRY_STR, 225 },
                                 { Japan_COUNTRY_STR, 81 },
                                 { Jordan_COUNTRY_STR, 962 },
                                 { KAZAKHSTAN_COUNTRY_STR, 705 },
                                 { Kenya_COUNTRY_STR, 254 },
                                 { South_Korea_COUNTRY_STR, 82 },
                                 { Kuwait_COUNTRY_STR, 965 },
                                 { Liberia_COUNTRY_STR, 231 },
                                 { Libya_COUNTRY_STR, 218 },
                                 { Liechtenstein_COUNTRY_STR, 4101 },
                                 { Luxembourg_COUNTRY_STR, 352 },
                                 { Malawi_COUNTRY_STR, 265 },
                                 { Malaysia_COUNTRY_STR, 60 },
                                 { Mali_COUNTRY_STR, 223 },
                                 { Malta_COUNTRY_STR, 356 },
                                 { Mexico_COUNTRY_STR, 52 },
                                 { Monaco_COUNTRY_STR, 33 },
                                 { Morocco_COUNTRY_STR, 212 },
                                 { Namibia_COUNTRY_STR, 264 },
                                 { Nepal_COUNTRY_STR, 977 },
                                 { Netherlands_COUNTRY_STR, 31 },
                                 { Netherlands_Antilles_COUNTRY_STR, 599 },
                                 { New_Caledonia_COUNTRY_STR, 687 },
                                 { New_Zealand_COUNTRY_STR, 64 },
                                 { Nicaragua_COUNTRY_STR, 505 },
                                 { Nigeria_COUNTRY_STR, 234 },
                                 { Norway_COUNTRY_STR, 47 }, 
                                 { Oman_COUNTRY_STR, 968 },
                                 { Pakistan_COUNTRY_STR, 92 },
                                 { Panama_COUNTRY_STR, 507 },
                                 { Papua_New_Guinea_COUNTRY_STR, 675 },
                                 { Paraguay_COUNTRY_STR, 595 },
                                 { Peru_COUNTRY_STR, 51 },
                                 { Philippines_COUNTRY_STR, 63 },
                                 { Poland_COUNTRY_STR, 48 },
                                 { Portugal_COUNTRY_STR, 351 },
				 { Puerto_Rico_COUNTRY_STR, 121 },
                                 { Qatar_COUNTRY_STR, 974 },
                                 { Romania_COUNTRY_STR, 40 },
                                 { Russia_COUNTRY_STR, 7 },
                                 { Saipan_COUNTRY_STR, 670 },
                                 { San_Marino_COUNTRY_STR, 39 },
                                 { Saudia_Arabia_COUNTRY_STR, 966 },
                                 { Senegal_COUNTRY_STR, 221},
                                 { Singapore_COUNTRY_STR, 65 },
                                 { Slovakia_COUNTRY_STR, 42 },
                                 { South_Africa_COUNTRY_STR, 27 },
                                 { Spain_COUNTRY_STR, 34 },
                                 { Sri_Lanka_COUNTRY_STR, 94 },
                                 { Suriname_COUNTRY_STR, 597 },
                                 { Sweden_COUNTRY_STR, 46 },
                                 { Switzerland_COUNTRY_STR, 41 },
                                 { Taiwan_COUNTRY_STR, 886 },
                                 { Tanzania_COUNTRY_STR, 255 },
                                 { Thailand_COUNTRY_STR, 66 },
				 { Tinian_Island_COUNTRY_STR, 6702 },
				 { Togo_COUNTRY_STR, 228 },
				 { Tokelau_COUNTRY_STR, 690 },
				 { Tonga_COUNTRY_STR, 676 },
				 { Trinadad_and_Tabago_COUNTRY_STR, 117 },
                                 { Tunisia_COUNTRY_STR, 216 },
                                 { Turkey_COUNTRY_STR, 90 },
				 { Turkmenistan_COUNTRY_STR, 709 },
				 { Turks_and_Caicos_Islands_COUNTRY_STR, 118 },
				 { Tuvalu_COUNTRY_STR, 688 },
				 { Uganda_COUNTRY_STR, 256 },
                                 { Ukraine_COUNTRY_STR, 380 },
                                 { United_Arab_Emirates_COUNTRY_STR, 971 },
                                 { UK_COUNTRY_STR, 0x2c },
				 { US_Virgin_Islands, 123 }, 
                                 { Uruguay_COUNTRY_STR, 598 },
				 { Uzbekistan_COUNTRY_STR, 711 },
				 { Vanuatu_COUNTRY_STR, 678 },
                                 { Vatican_City_COUNTRY_STR, 379 },
                                 { Venezuela_COUNTRY_STR, 58 },
                                 { Vietnam_COUNTRY_STR, 84 },
				 { Wallis_and_Futuna_Islands_COUNTRY_STR, 681 },
				 { Western_Samoa_COUNTRY_STR, 685 },
                                 { Yemen_COUNTRY_STR, 967 },
                                 { Yugoslavia_COUNTRY_STR, 381 },
                                 { Zaire_COUNTRY_STR, 243 },
				 { Zambia_COUNTRY_STR, 260 },
                                 { Zimbabwe_COUNTRY_STR, 263 },
#ifdef FUNNY_MSGS
                                 {NON_COUNTRY_FUNNY_STR, 9999 },
                                 {NON_COUNTRY_FUNNY_STR, 0 },
                                 {NON_COUNTRY_FUNNY_STR, 0xffff } };
#else
                                 {NON_COUNTRY_STR, 9999 },
                                 {NON_COUNTRY_STR, 0 },
                                 {NON_COUNTRY_STR, 0xffff } };
#endif


const char *Get_Country_Name( WORD code )
{
   int i;
   
   for ( i = 0; Country_Codes[i].code != 0xffff; i++)
   {
      if ( Country_Codes[i].code == code )
      {
         return Country_Codes[i].name;
      }
   }
   if ( Country_Codes[i].code == code )
   {
      return Country_Codes[i].name;
   }
   return NULL;
}



const char *Get_Lang_Name( BYTE code )
{
   int i;
   
   for ( i = 0; Lang_Codes[i].code != 0xff; i++)
   {
      if ( Lang_Codes[i].code == code )
      {
         return Lang_Codes[i].name;
      }
   }
   if ( Lang_Codes[i].code == code )
   {
      return Lang_Codes[i].name;
   }
   return NULL;
}

void Print_Lang_Numbers( void )
{
	int i;
	
	for ( i=0; Lang_Codes[i].code != 0xff; i++ )
	{
		M_print( "%2d. %-7s", Lang_Codes[i].code, Lang_Codes[i].name );
		if ( (i+1) & 3 ) {
			M_print( "\t" );
		} else {
			M_print( "\n" );
		}
	}
	M_print( "\n" );
}

/********************************************
returns a string describing the status or
a NULL if no such string exists
*********************************************/
char *Convert_Status_2_Str( DWORD status )
{
   if ( STATUS_OFFLINE == status ) /* this because -1 & 0xFFFF is not -1 */
   {
      return STATUS_OFFLINE_STR;
   }
   
   switch ( status & 0x1ff )
   {
   case STATUS_ONLINE:
      return STATUS_ONLINE_STR;
      break;
   case STATUS_DND_99 :
   case STATUS_DND:
      return STATUS_DND_STR;
      break;
   case STATUS_AWAY:
      return STATUS_AWAY_STR;
      break;
   case STATUS_OCCUPIED_MAC:
   case STATUS_OCCUPIED:
      return STATUS_OCCUPIED_STR;
      break;
   case STATUS_NA:
   case STATUS_NA_99:
      return STATUS_NA_STR;
      break;
   case STATUS_INVISIBLE:
      return STATUS_INVISIBLE_STR;
      break;
   case STATUS_FREE_CHAT:
      return STATUS_FFC_STR;
      break;
   default :
      return NULL;
      break;
   }
}


/********************************************
Prints a informative string to the screen.
describing the command
*********************************************/
void Print_CMD( WORD cmd )
{
   switch ( cmd )
   {
   case CMD_KEEP_ALIVE:
      M_print( "Keep Alive" );
      break;
   case CMD_KEEP_ALIVE2:
      M_print( "Secondary Keep Alive" );
      break;
   case CMD_CONT_LIST:
      M_print( "Contact List" );
      break;
   case CMD_INVIS_LIST:
      M_print( "Invisible List" );
      break;
   case CMD_VIS_LIST:
      M_print( "Visible List" );
      break;
   case CMD_RAND_SEARCH:
      M_print( "Random Search" );
      break;
   case CMD_RAND_SET:
      M_print( "Set Random" );
      break;
   case CMD_ACK_MESSAGES:
      M_print( "Delete Server Messages" );
      break;
   case CMD_LOGIN_1:
      M_print( "Finish Login" );
      break;
   case CMD_LOGIN:
      M_print( "Login" );
      break;
   case CMD_SENDM:
      M_print( "Send Message" );
      break;
   case CMD_INFO_REQ:
      M_print( "Info Request" );
      break;
   case CMD_EXT_INFO_REQ:
      M_print( "Extended Info Request" );
      break;
   default :
      M_print( "%04X", cmd );
      break;
   }
}

/********************************************
prints out the status of new_status as a string
if possible otherwise as a hex number
*********************************************/
void Print_Status( DWORD new_status  )
{
   BOOL inv = FALSE;
   if ( STATUS_OFFLINE != new_status )  {
	   if ( new_status &  STATUS_INVISIBLE ) {
		inv = TRUE;
       		 new_status = new_status & (~STATUS_INVISIBLE) ;
	   }
   }
   if ( Convert_Status_2_Str( new_status ) != NULL )
   {
	if ( inv ) {
		M_print( STATUS_INVISIBLE_STR "-%s", Convert_Status_2_Str( new_status ) );
		new_status = new_status | (STATUS_INVISIBLE) ;
	} else {
		M_print( "%s", Convert_Status_2_Str( new_status ) );
	}
      if ( Verbose )
         M_print( " %06X",( WORD ) ( new_status >> 8 ) );
   }
   else
   {
      if ( inv ) new_status = new_status | (STATUS_INVISIBLE) ;
      M_print( "%08lX", new_status );
   }
}

/**********************************************
 * Returns the nick of a UIN if we know it else
 * it will return Unknow UIN
 **********************************************/
char *UIN2nick( DWORD uin)
{
   int i;
    
   for ( i=0; i < Num_Contacts; i++ )
   {
     if ( Contacts[i].uin == uin )
        break;
   }
    
   if ( i == Num_Contacts )
   {
      return NULL;
   }
   else
   {
      return Contacts[i].nick;
   }
}

/**********************************************
Prints the name of a user or there UIN if name
is not know.
***********************************************/
int Print_UIN_Name( DWORD uin )
{
   int i;
   
   for ( i=0; i < Num_Contacts; i++ )
   {
      if ( Contacts[i].uin == uin )
         break;
   }

   if ( i == Num_Contacts )
   {
      M_print( CLIENTCOL "%lu" NOCOL, uin );
      return -1 ;
   }
   else
   {
      M_print( "%s%s%s", CONTACTCOL, Contacts[i].nick, NOCOL );
      return i;
   }
}

/**********************************************
Returns the contact list with uin
***********************************************/
CONTACT_PTR UIN2Contact( DWORD uin )
{
   int i;
   
   for ( i=0; i < Num_Contacts; i++ )
   {
      if ( Contacts[i].uin == uin )
         break;
   }

   if ( i == Num_Contacts )
   {
      return (CONTACT_PTR) NULL ;
   }
   else
   {
      return &Contacts[i];
   }
}

/*********************************************
Converts a nick name into a uin from the contact
list.
**********************************************/
DWORD nick2uin( char *nick )
{
   int i;
   BOOL non_numeric=FALSE;

   /*cut off whitespace at the end (i.e. \t or space*/
   i = strlen(nick) - 1;
   while (isspace(nick[i]))
          i--;
   nick[i+1] = '\0';

   
   for ( i=0; i< Num_Contacts; i++ )
   {
      if ( ! strncasecmp( nick, Contacts[i].nick, 19  ) )
      {
         if ( (S_DWORD) Contacts[i].uin > 0 )
            return Contacts[i].uin;
         else
            return -Contacts[i].uin; /* alias */
      }
   }
   for ( i=0; i < strlen( nick ); i++ )
   {
      if ( ! isdigit( (int) nick[i] ) )
      {
         non_numeric=TRUE;
         break;
      }
   }
   if ( non_numeric )
      return -1; /* not found and not a number */
   else
      return atoi( nick );
}

/**************************************************
Automates the process of creating a new user.
***************************************************/
void Init_New_User( void )
{
   SOK_T sok; 
   srv_net_icq_pak pak;
   int s;
   struct timeval tv;
#ifdef _WIN32
   int i;
   WSADATA wsaData;
   FD_SET readfds;
#else
   fd_set readfds;
#endif
      
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
   M_print( "\nCreating Connection...\n");
   sok = Connect_Remote( server, remote_port, STDERR );
   if ( ( sok == -1 ) || ( sok == 0 ) ) 
   {
       M_print( "Couldn't establish connection\n" );
       exit( 1 );
   }
   M_print( "Sending Request...\n" );
   reg_new_user( sok, passwd );
   for ( ; ; )
   {
#ifdef UNIX
      tv.tv_sec = 3;
      tv.tv_usec = 500000;
#else
      tv.tv_sec = 0;
      tv.tv_usec = 100000;
#endif

      FD_ZERO(&readfds);
      FD_SET(sok, &readfds);

      /* don't care about writefds and exceptfds: */
      select(sok+1, &readfds, NULL, NULL, &tv);
      M_print( "Waiting for response....\n" );
      if (FD_ISSET(sok, &readfds))
      {
         s = SOCKREAD( sok, &pak.head.ver, sizeof( pak ) - 2  );
         if ( Chars_2_Word( pak.head.cmd ) == SRV_NEW_UIN )
         {
            UIN = Chars_2_DW( pak.head.UIN );
            M_print( "\nYour new UIN is %s%ld%s!\n",SERVCOL, UIN, NOCOL );
            return;
         }
         else
         {
/*            Hex_Dump( &pak.head.ver, s );*/
         }
      }
      reg_new_user( sok, passwd );
   }
}


void Print_IP( DWORD uin )
{
   int i;
#if 0
   struct in_addr sin;
#endif
   
   for ( i=0; i< Num_Contacts; i++ )
   {
      if ( Contacts[i].uin == uin )
      {
         if ( * (DWORD *)Contacts[i].current_ip != -1L )
         {
           M_print( "%d.%d.%d.%d", Contacts[i].current_ip[0],
                                   Contacts[i].current_ip[1],
                                   Contacts[i].current_ip[2],
                                   Contacts[i].current_ip[3] );
         }
         else
         {
            M_print( "unknown" );
         }
         return;
      }
   }
   M_print( "unknown" );
}

/************************************************
Gets the TCP port of the specified UIN
************************************************/
DWORD Get_Port( DWORD uin )
{
   int i;
   
   for ( i=0; i< Num_Contacts; i++ )
   {
      if ( Contacts[i].uin == uin )
      {
         return Contacts[i].port;
      }
   }
   return -1L;
}

/********************************************
Converts an intel endian character sequence to
a DWORD
*********************************************/
DWORD Chars_2_DW( BYTE *buf )
{
   DWORD i;
   
   i= buf[3];
   i <<= 8;
   i+= buf[2];
   i <<= 8;
   i+= buf[1];
   i <<= 8;
   i+= buf[0];
   
   return i;
}

/********************************************
Converts an intel endian character sequence to
a WORD
*********************************************/
WORD Chars_2_Word( BYTE *buf )
{
   WORD i;
   
   i= buf[1];
   i <<= 8;
   i += buf[0];
   
   return i;
}

/********************************************
Converts a DWORD to
an intel endian character sequence 
*********************************************/
void DW_2_Chars( BYTE *buf, DWORD num )
{
   buf[3] = ( unsigned char ) ((num)>>24)& 0x000000FF;
   buf[2] = ( unsigned char ) ((num)>>16)& 0x000000FF;
   buf[1] = ( unsigned char ) ((num)>>8)& 0x000000FF;
   buf[0] = ( unsigned char ) (num) & 0x000000FF;
}

/********************************************
Converts a WORD to
an intel endian character sequence 
*********************************************/
void Word_2_Chars( BYTE *buf, WORD num )
{
   buf[1] = ( unsigned char ) (((unsigned)num)>>8) & 0x00FF;
   buf[0] = ( unsigned char ) ((unsigned)num) & 0x00FF;
}

BOOL Log_Dir_Normal( void )
{
	return Normal_Log;
}

/************************************************************************
Sets the Log directory to newpath 
if newpath is null it will set it to a reasonable default
if newpath doesn't end with a valid directory seperator one is added.
the path used is returned.
*************************************************************************/
char * Set_Log_Dir( const char *newpath )
{
	if ( NULL == newpath ) {
	   char *path;
	   char *home;
	   Normal_Log = TRUE;
#ifdef _WIN32
	   path = ".\\";
#endif

#ifdef UNIX
	   home = getenv( "HOME" );
	   path = malloc( strlen( home ) + 2 );
	   strcpy( path, home );
	   if ( path[ strlen( path ) - 1 ] != '/' )
	      strcat( path, "/" );
#endif

#ifdef __amigaos__
	   path = "PROGDIR:";
#endif
		Log_Dir = path;
		return path;
	} else {
#ifdef _WIN32
		char sep = '\\';
#else
		char sep = '/';
#endif
		Normal_Log=FALSE;
		if ( newpath[ strlen( newpath ) - 1 ] != sep ) {
			Log_Dir = malloc( strlen( newpath ) + 2 );
			sprintf( Log_Dir, "%s%c", newpath, sep );
		} else {
			Log_Dir = strdup( newpath );
		}
		return Log_Dir;
	}
}

/************************************************************************
Returns the current directory used for logging.  If none has been 
specified it sets it tobe a reasonable default (~/)
*************************************************************************/
char * Get_Log_Dir( void )
{
	if ( NULL == Log_Dir ) {
		return Set_Log_Dir( NULL );
	} else {
		return Log_Dir;
	}
}

/*************************************************************************
 *      Function: log_event
 *      Purpose: Log the event provided to the log with a time stamp.
 *      Andrew Frolov dron@ilm.net
 *      6-20-98 Added names to the logs. Fryslan
 *************************************************************************/
int log_event( DWORD uin, int type, char *str, ... )
{
   char symbuf[256]; /* holds the path for a sym link */
   FILE    *msgfd;
   va_list args;
   int k;
   char buf[2048]; /* this should big enough - This holds the message to be logged */
   char    buffer[256];
   time_t  timeval;
   char *path;
/*   char *home; */
   struct stat statbuf;

   if ( ! LogType )
      return 0;

   if ( ( 3 == LogType  ) && ( LOG_ONLINE == type ) )
      return 0;
   
   timeval = time(0);
   va_start( args, str );
   sprintf( buf, "\n%-24.24s ", ctime(&timeval) );
   vsprintf( &buf[ strlen( buf ) ], str, args );

/*************************************************      
#ifdef _WIN32
   path = ".\\";
#endif

#ifdef UNIX
   home = getenv( "HOME" );
   path = malloc( strlen( home ) + 2 );
   strcpy( path, home );
   if ( path[ strlen( path ) - 1 ] != '/' )
      strcat( path, "/" );
#endif

#ifdef __amigaos__
   path = "PROGDIR:";
#endif

   strcpy( buffer, path );
*************************************************/

   path = Get_Log_Dir();
   strcpy( buffer, path );

switch (LogType) {
	case 1:
		strcat(buffer,"micq_log");
		break;
	case 2:
	case 3:
	default:
	   strcat( buffer, "micq.log" );
	   if ( -1 == stat( buffer, &statbuf ) ) {
		if ( errno == ENOENT ) {
	    	     mkdir( buffer, 0700 );
	         } else {
	        	 return -1;
      		 }
	   }
#ifdef _WIN32
   	strcat( buffer, "\\" );
#else
  	 strcat( buffer, "/" );
#endif
		strcpy( symbuf, buffer);
	   sprintf( &buffer[ strlen( buffer ) ], "%ld.log", uin );
      
#ifdef UNIX
	if ( NULL != UIN2nick(uin) ) {
     		 sprintf( &symbuf[strlen(symbuf)],"%s.log",UIN2nick(uin) );
	      symlink(buffer,symbuf);
	}
#endif        
       

	   break;
}
   if( ( msgfd = fopen(buffer, "a") ) == (FILE *) NULL ) 
   {
           fprintf(stderr, "\nCouldn't open %s for logging\n",
                            buffer);
           return(-1);
   }
/*    if ( ! strcasecmp(UIN2nick(uin),"Unknow UIN"))
       fprintf(msgfd, "\n%-24.24s %s %ld\n%s\n", ctime(&timeval), desc, uin, msg);
    else
       fprintf(msgfd, "\n%-24.24s %s %s\n%s\n", ctime(&timeval), desc, UIN2nick(uin), msg);*/

   if ( strlen( buf ) ) {
   	fwrite( "<", 1, 1, msgfd );
   	k = fwrite( buf, 1, strlen( buf ), msgfd );
   	if ( k != strlen( buf ) )
   	{
       		perror( "\nLog file write error\n" );
      		return -1;
   	}
   	fwrite( ">\n", 1, 2, msgfd );
   }
   va_end( args );
     
   fclose(msgfd);
#ifdef UNIX
   chmod( buffer, 0600 );
#endif
   return(0);
}

/*************************************************
 clears the screen 
**************************************************/
void clrscr(void)
{
#ifdef UNIX
    system( "clear" );
#else
#ifdef _WIN32
    system( "cls" );
#else
    int x;
    char newline = '\n';    

     for(x = 0; x<=25; x++)
        M_print("%c",newline);
#endif
#endif
}

/************************************************************
Displays a hex dump of buf on the screen.
*************************************************************/
void Hex_Dump( void *buffer, size_t len )
{
      int i;
      int j;
      int k;
      char *buf;
      
      buf = buffer;
      assert( len > 0 );
      if ( ( len > 1000 ) ) {
         M_print( "Ack!!!!!!  %d\a\n" , len );
	 return;
      }
      for ( i=0 ; i < len; i++ )
      {
         M_print( "%02x ", ( unsigned char ) buf[i] );
         if ( ( i & 15 ) == 15 )
         {
            M_print( "  " );
            for ( j = 15; j >= 0; j-- )
            {
               if ( buf[i-j] > 31 )
                  M_print( "%c", buf[i-j] );
               else
                  M_print( "." );
               if ( ( (i-j) & 3 ) == 3 )
                  M_print( " " );
            }
            M_print( "\n" );
         }
         else if ( ( i & 7 ) == 7 )
            M_print( "- " );
         else if ( ( i & 3 ) == 3 )
            M_print( "  " );
      }
      for ( k = i % 16; k <16; k++  )
      {
         M_print( "   " );
         if ( ( k & 7 ) == 7 )
            M_print( "  " );
         else if ( ( k & 3 ) == 3 )
            M_print( "  " );
      }
      for ( j = i % 16; j > 0; j-- )
      {
         if ( buf[i-j] > 31 )
            M_print( "%c", buf[i-j] );
         else
            M_print( "." );
         if ( ( (i-j) & 3 ) == 3 )
            M_print( " " );
      }
}
