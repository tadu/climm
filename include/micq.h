/*********************************************
 * ($Id$)
 *
 * Header file for ICQ protocol structres and
 * constants
 *
 * This software is provided AS IS to be used in
 * whatever way you see fit and is placed in the
 * public domain.
 * 
 * Author : Matthew Smith April 19, 1998
 * Contributors : Lalo Martins Febr 26, 1999
 * 
 * Changes :
 *  4-21-98 Increase the size of data associtated
 *           with the packets to enable longer messages. mds
 *  4-22-98 Added function prototypes and extern variables mds
 *  4-22-98 Added SRV_GO_AWAY code for bad passwords etc.
 *  (I assume Matt did a lot of unrecorded changes after these - Lalo)
 *  2-26-99 Added TAB_SLOTS, tab_array, add_tab() and get_tab() (Lalo)
 **********************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "micqconfig.h"
#include "datatype.h"
#include <stdlib.h>
#include "mreadline.h"
#include <time.h>
#include "msg_queue.h"
#include "mselect.h"
#include "color.h"

#define SOUND_ON 1
#define SOUND_OFF 0
#define SOUND_CMD 2

#define default_away_time 600

#define ICQ_VER 0x0005

#define STDIN 0
#define STDOUT 1
#define STDERR 2

#if ICQ_VER == 0x0005
   typedef struct
   {
      UBYTE dummy[2]; /* to fix alignment problem */
      UBYTE ver[2];
      UBYTE zero[4];
      UBYTE  UIN[4];
      UBYTE session[4];
      UBYTE cmd[2];
      UBYTE seq2[2];
      UBYTE seq[2];
      UBYTE checkcode[4];
   } ICQ_pak, *ICQ_PAK_PTR;

   typedef struct
   {
      UBYTE dummy[2]; /* to fix alignment problem */
      UBYTE ver[2];
      UBYTE zero;
      UBYTE session[4];
      UBYTE cmd[2];
      UBYTE seq[2];
      UBYTE seq2[2];
      UBYTE UIN[4];
      UBYTE check[4];
   } SRV_ICQ_pak, *SRV_ICQ_PAK_PTR;

#define CMD_OFFSET 14
#define SEQ_OFFSET 16
#define SEQ2_OFFSET 18
#define DATA_OFFSET 4
#define PAK_DATA_OFFSET 24

#elif ICQ_VER == 0x0004

   typedef struct
   {
      UBYTE dummy[2]; /* to fix alignment problem */
      UBYTE ver[2];
      UBYTE rand[2];
      UBYTE zero[2];
      UBYTE cmd[2];
      UBYTE seq[2];
      UBYTE seq2[2];
      UBYTE  UIN[4];
      UBYTE checkcode[4];
   } ICQ_pak, *ICQ_PAK_PTR;

   typedef struct
   {
      UWORD dummy; /* to fix alignment problem */
      UBYTE ver[2];
      UBYTE cmd[2];
      UBYTE seq[2];
      UBYTE seq2[2];
      UBYTE UIN[4];
      UBYTE check[4];
   } SRV_ICQ_pak, *SRV_ICQ_PAK_PTR;

#else /* ICQ_VER */

   typedef struct
   {
      UWORD dummy; /* to fix alignment problem */
      UBYTE ver[2];
      UBYTE cmd[2];
      UBYTE seq[2];
      UBYTE  UIN[4];
   } ICQ_pak, *ICQ_PAK_PTR;

   typedef struct
   {
      UWORD dummy; /* to fix alignment problem */
      UBYTE ver[2];
      UBYTE cmd[2];
      UBYTE seq[2];
   } SRV_ICQ_pak, *SRV_ICQ_PAK_PTR;

#endif  /* ICQ_VER */

typedef struct
{
   ICQ_pak  head;
   unsigned char  data[1024];
} net_icq_pak, *NET_ICQ_PTR;

typedef struct
{
   SRV_ICQ_pak  head;
   unsigned char  data[1024];
} srv_net_icq_pak, *SRV_NET_ICQ_PTR;

#define CMD_ACK               	0x000A 
#define CMD_SENDM             	0x010E
#define CMD_LOGIN             	0x03E8
#define CMD_CONT_LIST         	0x0406
#define CMD_SEARCH_UIN        	0x041a
#define CMD_SEARCH_USER       	0x0424
#define CMD_KEEP_ALIVE        	0x042e
#define CMD_KEEP_ALIVE2       	0x051e
#define CMD_SEND_TEXT_CODE    	0x0438
#define CMD_LOGIN_1           	0x044c
#define CMD_INFO_REQ          	0x0460
#define CMD_EXT_INFO_REQ      	0x046a
#define CMD_CHANGE_PW         	0x049c
#define CMD_STATUS_CHANGE     	0x04d8
#define CMD_LOGIN_2           	0x0528
#define CMD_UPDATE_INFO       	0x050A
#define CMD_UPDATE_EXT_INFO   	0X04B0
#define CMD_ADD_TO_LIST       	0X053C
#define CMD_REQ_ADD_LIST      	0X0456
#define CMD_QUERY_SERVERS     	0X04BA
#define CMD_QUERY_ADDONS      	0X04C4
#define CMD_NEW_USER_1        	0X04EC
#define CMD_NEW_USER_INFO     	0x04A6
#define CMD_ACK_MESSAGES      	0x0442
#define CMD_MSG_TO_NEW_USER   	0x0456
#define CMD_REG_NEW_USER      	0x03FC
#define CMD_VIS_LIST          	0x06AE
#define CMD_INVIS_LIST        	0x06A4
#define CMD_META_USER         	0x064A
#define CMD_RAND_SEARCH       	0x056E
#define CMD_RAND_SET          	0x0564
#define CMD_AUTH_UPDATE       	0x0514
#define CMD_UPDATE_LIST       	0x06B8

#define SRV_ACK            	0x000A
#define SRV_LOGIN_REPLY    	0x005A
#define SRV_BAD_PASSWORD   	0x0064
#define SRV_USER_ONLINE    	0x006E
#define SRV_USER_OFFLINE   	0x0078
#define SRV_USER_FOUND     	0x008C
#define SRV_RECV_MESSAGE   	0x00DC
#define SRV_END_OF_SEARCH  	0x00A0
#define SRV_INFO_REPLY     	0x0118
#define SRV_EXT_INFO_REPLY 	0x0122
#define SRV_STATUS_UPDATE  	0x01A4
#define SRV_X1             	0x021C
#define SRV_X2             	0x00E6
#define SRV_UPDATE_EXT     	0x00C8
#define SRV_NEW_UIN        	0x0046
#define SRV_NEW_USER       	0x00B4
#define SRV_QUERY          	0x0082
#define SRV_SYSTEM_MESSAGE 	0x01C2
#define SRV_SYS_DELIVERED_MESS 	0x0104
#define SRV_GO_AWAY        	0x0028
#define SRV_NOT_CONNECTED  	0x00F0
#define SRV_BAD_PASS       	0x0064
#define SRV_TRY_AGAIN      	0x00FA
#define SRV_UPDATE_FAIL    	0x01EA
#define SRV_UPDATE_SUCCESS 	0x01E0
#define SRV_MULTI_PACKET   	0x0212
#define SRV_META_USER      	0x03DE
#define SRV_RAND_USER      	0x024E
#define SRV_AUTH_UPDATE    	0x01F4

#define META_INFO_SET       	0x03E8
#define META_INFO_REQ       	0x04B0
#define META_INFO_SECURE    	0x0424
#define META_INFO_PASS      	0x042E
#define META_SRV_GEN        	0x00C8
#define META_SRV_MORE       	0x00DC
#define META_SRV_WORK       	0x00D2
#define META_SRV_ABOUT      	0x00E6
#define META_SRV_PASS       	0x00AA
#define META_SRV_GEN_UPDATE 	0x0064
#define META_SRV_ABOUT_UPDATE 	0x082
#define META_SRV_OTHER_UPDATE 	0x078
#define META_INFO_ABOUT     	0x0406
#define META_INFO_MORE      	0x03FC

#define STATUS_OFFLINE  	(-1L)
#define STATUS_ONLINE  		0x00
#define STATUS_INVISIBLE 	0x100
#define STATUS_NA_99        	0x04
#define STATUS_NA      		0x05
#define STATUS_FREE_CHAT 	0x20
#define STATUS_OCCUPIED_MAC 	0x10
#define STATUS_OCCUPIED 	0x11
#define STATUS_AWAY    		0x01
#define STATUS_DND    		0x13
#define STATUS_DND_99    	0x02

#define AUTH_MESSAGE  		0x0008

#define USER_ADDED_MESS 	0x000C
#define AUTH_REQ_MESS 		0x0006
#define URL_MESS		0x0004
#define WEB_MESS		0x000d
#define EMAIL_MESS		0x000e
#define MASS_MESS_MASK  	0x8000
#define MRURL_MESS		0x8004
#define NORM_MESS		0x0001
#define MRNORM_MESS		0x8001
#define CONTACT_MESS		0x0013
#define MRCONTACT_MESS		0x8013

#define INV_LIST_UPDATE 	0x01
#define VIS_LIST_UPDATE 	0x02
#define CONT_LIST_UPDATE 	0x00

/*#define USA_COUNTRY_CODE 0x01
#define UK_COUNTRY_CODE 0x2C*/
#if ICQ_VER == 0x0005

   typedef struct
   {
      UBYTE time[4];
      UBYTE port[4];
      UBYTE len[2];
   } login_1, *LOGIN_1_PTR;

   typedef struct
   {
      UBYTE X1[4];
      UBYTE ip[4];
      UBYTE  X2[1];
      UBYTE  status[4];
      UBYTE X3[4];
   /*   UBYTE seq[2];*/
      UBYTE  X4[4];
      UBYTE X5[4];
      UBYTE X6[4];
      UBYTE X7[4];
      UBYTE X8[4];
   } login_2, *LOGIN_2_PTR;

   /* those behind the // are for the spec on
    http://www.student.nada.kth.se/~d95-mih/icq/
    they didn't work for me so I changed them
    to values that did work.
    *********************************/
   /*#define LOGIN_X1_DEF 0x00000078 */
/*   #define LOGIN_X2_DEF 0x04*/
   #define LOGIN_X1_DEF 0x000000d5
   /*#define LOGIN_X1_DEF 0x000000C8*/
   #define LOGIN_X2_DEF 0x06 /* SERVER ONLY */
   /*#define LOGIN_X3_DEF 0x00000002*/
   #define LOGIN_X3_DEF 0x00000006 /* TCP VERSION */
   /*#define LOGIN_X4_DEF 0x00000000*/
   #define LOGIN_X4_DEF 0x00000000
   /*#define LOGIN_X5_DEF 0x00780008*/
/*   #define LOGIN_X5_DEF 0x00d50008*/
   #define LOGIN_X5_DEF 0x822c01ec
   /*#define LOGIN_X5_DEF 0x00C80010*/
   #define LOGIN_X6_DEF 0x00000050
   #define LOGIN_X7_DEF 0x00000003
/*   #define LOGIN_X8_DEF 0x36abff13*/ 
   #define LOGIN_X8_DEF 0x36d61600 /* == 920 000 000 */

#elif ICQ_VER == 0x0004

   typedef struct
   {
      UBYTE time[4];
      UBYTE port[4];
      UBYTE len[2];
   } login_1, *LOGIN_1_PTR;

   typedef struct
   {
      UBYTE X1[4];
      UBYTE ip[4];
      UBYTE  X2[1];
      UBYTE  status[4];
      UBYTE X3[4];
   /*   UBYTE seq[2];*/
      UBYTE  X4[4];
      UBYTE X5[4];
   } login_2, *LOGIN_2_PTR;

   /* those behind the // are for the spec on
    http://www.student.nada.kth.se/~d95-mih/icq/
    they didn't work for me so I changed them
    to values that did work.
    *********************************/
   /*#define LOGIN_X1_DEF 0x00000078 */
   #define LOGIN_X1_DEF 0x00000098
/*   #define LOGIN_X2_DEF 0x04*/
   #define LOGIN_X2_DEF 0x06
   /*#define LOGIN_X3_DEF 0x00000002*/
   #define LOGIN_X3_DEF 0x00000003
   /*#define LOGIN_X4_DEF 0x00000000*/
   #define LOGIN_X4_DEF 0x00000000
   /*#define LOGIN_X5_DEF 0x00780008*/
   #define LOGIN_X5_DEF 0x00980000

#else /* ICQ_VER */

   typedef struct
   {
      UBYTE port[4];
      UBYTE len[2];
   } login_1, *LOGIN_1_PTR;

   typedef struct
   {
      UBYTE X1[4];
      UBYTE ip[4];
      UBYTE  X2[1];
      UBYTE  status[4];
      UBYTE X3[4];
      UBYTE seq[2];
      UBYTE  X4[4];
      UBYTE X5[4];
   } login_2, *LOGIN_2_PTR;

   /* those behind the // are for the spec on
    http://www.student.nada.kth.se/~d95-mih/icq/
    they didn't work for me so I changed them
    to values that did work.
    *********************************/
   /*#define LOGIN_X1_DEF 0x00000078 */
   #define LOGIN_X1_DEF 0x00040072
   #define LOGIN_X2_DEF 0x06
   /*#define LOGIN_X3_DEF 0x00000002*/
   #define LOGIN_X3_DEF 0x00000003
   /*#define LOGIN_X4_DEF 0x00000000*/
   #define LOGIN_X4_DEF 0x00000000
   /*#define LOGIN_X5_DEF 0x00780008*/
   #define LOGIN_X5_DEF 0x00720004

#endif /* ICQ_VER */

typedef struct
{
   UBYTE   uin[4];
   UBYTE year[2];
   UBYTE  month;
   UBYTE  day;
   UBYTE  hour;
   UBYTE  minute;
   UBYTE type[2];
   UBYTE len[2];
} RECV_MESSAGE, *RECV_MESSAGE_PTR;

typedef struct
{
   UBYTE uin[4];
   UBYTE type[2]; 
   UBYTE len[2];
} SIMPLE_MESSAGE, *SIMPLE_MESSAGE_PTR;

typedef struct
{
   UDWORD uin;
   UDWORD status;
   UDWORD last_time; /* last time online or when came online */
   UBYTE current_ip[4];
   UDWORD port;
   BOOL invis_list;
   BOOL vis_list;
   BOOL not_in_list;
   SOK_T sok;
   UWORD TCP_version;
   UBYTE connection_type;
   UBYTE other_ip[4];
   /* aaron
    Pointer to a string containing the last message recieved from this
    person. If we haven't received a message from them, this pointer will
    be NULL due to the way the Contact is initially initialized.            */
   char *LastMessage;
   /* end of aaron */
   char nick[20];
} Contact_Member, *CONTACT_PTR;

typedef struct
{
   char *nick;
   char *first;
   char *last;
   char *email;
   BOOL auth;
} USER_INFO_STRUCT, *USER_INFO_PTR;

typedef struct
{
   char *nick;
   char *first;
   char *last;
   char *email;
   char *email2;
   char *email3;
   char *city;
   char *state;
   char *phone;
   char *fax;
   char *street;
   char *cellular;
   UDWORD zip;
   UWORD country;
   UBYTE c_status;
   BOOL hide_email;
   BOOL auth;
} MORE_INFO_STRUCT, *MORE_INFO_PTR;

typedef struct {
	UWORD age;
	UBYTE sex;
	char *hp;
	UBYTE year;
	UBYTE month;
	UBYTE day;
	UBYTE lang1;
	UBYTE lang2;
	UBYTE lang3;
} OTHER_INFO_STRUCT, *OTHER_INFO_PTR;

#include "i18n.h"
int Connect_Remote( char *hostname, int port, FD_T aux );

extern Contact_Member Contacts[ MAX_CONTACTS ]; /* no more than 100 contacts max */
extern int Num_Contacts;
extern UDWORD UIN; /* current User Id Number */
extern BOOL Contact_List;
extern UWORD last_cmd[ 1024 ]; /* command issued for the first 1024 SEQ #'s */
/******************** should use & 0x3ff on all references to this */
extern UWORD seq_num;  /* current sequence number */
extern UDWORD our_ip;
extern UDWORD our_port; /* the port to make tcp connections on */
extern BOOL Quit;
extern BOOL Verbose;
extern UBYTE Sound;
extern UDWORD Current_Status;
extern UDWORD last_recv_uin;
extern char passwd[100];
extern char server[100];
extern UBYTE LogType;
extern UDWORD remote_port;

/* SOCKS5 stuff begin*/
extern int s5Use;
extern char s5Host[100];
extern unsigned short s5Port;
extern int  s5Auth;
extern char s5Name[64];
extern char s5Pass[64];
extern unsigned long s5DestIP;
extern unsigned short s5DestPort;
/* SOCKS5 stuff end */
 
extern UDWORD set_status;
extern BOOL auto_resp;
extern char auto_rep_str_na[450];
extern char auto_rep_str_away[450];
extern char auto_rep_str_occ[450];
extern char auto_rep_str_inv[450];
extern char auto_rep_str_dnd[450];
extern UBYTE Sound_Str[150];
extern BOOL Done_Login;
extern char clear_cmd[16];
extern char message_cmd[16];
extern char info_cmd[16];
extern char quit_cmd[16];
extern char reply_cmd[16];
extern char again_cmd[16];
extern char add_cmd[16];
extern char togvis_cmd[16];
extern char about_cmd[16];

extern char list_cmd[16];
extern char away_cmd[16];
extern char na_cmd[16];
extern char dnd_cmd[16];
extern char online_cmd[16];
extern char occ_cmd[16];
extern char ffc_cmd[16];
extern char inv_cmd[16];
extern char status_cmd[16];
extern char auth_cmd[16];
extern char auto_cmd[16];
extern char search_cmd[16];
extern char save_cmd[16];
extern char change_cmd[16];
extern char alter_cmd[16];
extern char msga_cmd[16];
extern char url_cmd[16];
extern char sound_cmd[16];
extern char color_cmd[16];
extern char rand_cmd[16];
extern char update_cmd[16];
extern char online_list_cmd[16];
extern char togig_cmd[16];
extern char iglist_cmd[16];

extern BOOL del_is_bs;
extern BOOL last_uin_prompt;
extern int  line_break_type;

extern BOOL Russian;
extern BOOL JapaneseEUC;
extern BOOL Logging;
extern BOOL Color;

extern unsigned int next_resend;
extern UDWORD our_session;
		
#define LOG_MESS 1
#define LOG_AUTO_MESS 2
#define LOG_ONLINE 3

extern unsigned int away_time;
extern BOOL Hermit;

extern UWORD Max_Screen_Width;
extern UDWORD real_packs_sent;
extern UDWORD real_packs_recv;
extern UDWORD Packets_Sent;
extern UDWORD Packets_Recv;

/* aaron
   Variable to hold the time that Micq is started, for the "uptime" command,
   which shows how long Micq has been running.                               */
extern time_t MicqStartTime;
/* end of aaron */


#ifdef MSGEXEC
	extern char receive_script[255];
#endif

