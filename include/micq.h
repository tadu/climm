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

#if TM_IN_SYS_TIME
  #include <sys/time.h>
  #if TIME_WITH_SYS_TIME
    #include <time.h>
  #endif
#else
  #include <time.h>
  #if TIME_WITH_SYS_TIME
    #include <sys/time.h>
  #endif
#endif
#include "micqconfig.h"
#include "datatype.h"
#include <stdlib.h>
#include "mreadline.h"
#include "msg_queue.h"
#include "mselect.h"
#include "color.h"

#define SOUND_ON 1
#define SOUND_OFF 0
#define SOUND_CMD 2

#define default_away_time 600

#define ICQ_VER 5

#define STDIN 0
#define STDOUT 1
#define STDERR 2

#include "icq_v2.h"
#include "icq_v4.h"
#include "icq_v5.h"

#ifndef login_1
#error Strange ICQ_VER version
#endif

typedef ICQ_pak *ICQ_PAK_PTR;
typedef SRV_ICQ_pak *SRV_ICQ_PAK_PTR;

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

typedef struct
{
   UBYTE uin[4];
   UBYTE year[2];
   UBYTE month;
   UBYTE day;
   UBYTE hour;
   UBYTE minute;
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

typedef struct
{
   char *nick;
   char *first;
   char *last;
   char *email;
   UWORD minage;
   UWORD maxage;
   UBYTE sex;
   UBYTE language;
   char *city;
   char *state;
   UWORD country;
   char *company;
   char *department;
   char *position;
   UBYTE online;
   BOOL auth;
   
} WP_STRUCT, *WP_PTR;

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

/*
 * Should you decide to move a global variable from one struct to another,
 * change it manually in the header file, and then in the src directory
 * the following incantation can be handy (assuming you want to move, say,
 * Hermit, from uiG to ssG):
 * perl -pi -e 's/(\W)uiG\.(Hermit)(\W)/$1ssG.$2$3/g;' *.c
 */

/* user interface global state variables */
typedef struct {
        Contact_Member Contacts[ MAX_CONTACTS ]; /* MAX_CONTACTS <= 100 */
        int Num_Contacts;
        BOOL Contact_List;     /* I think we always have a contact list now */
        BOOL Verbose;          /* displays extra debugging info */
        UBYTE Sound;           /* sound setting for normal beeps etc */
        UBYTE SoundOnline;     /* sound setting for users comming online */
        UBYTE SoundOffline;    /* sound settng for users going offline */
        UDWORD Current_Status;
        UDWORD last_recv_uin;
        UBYTE LogType;         /* Currently 0 = no logging
                                            1 = old style ~/micq_log
                                            2 = new style ~/micq.log/uin.log
                                            ****************************** */
        BOOL auto_resp;
        char auto_rep_str_na[450];
        char auto_rep_str_away[450];
        char auto_rep_str_occ[450];
        char auto_rep_str_inv[450];
        char auto_rep_str_dnd[450];
        UBYTE Sound_Str[150];           /* shellcmd to exec on normal beeps */
        UBYTE Sound_Str_Online[150];    /* shellcmd to exec on usr online */
        UBYTE Sound_Str_Offline[150];   /* shellcmd to exec on usr offline */
        BOOL del_is_bs;                 /* del char is backspace */
        BOOL last_uin_prompt;           /* use last UIN's nick as prompt */
        int  line_break_type;           /* see .rc file for modes */

        BOOL Russian;    /* Do we do koi8-r <->Cp1251 codeset translation? */
        BOOL JapaneseEUC;/* Do we do Shift-JIS <->EUC codeset translation? */
        BOOL Logging;          /* Do we log messages to ~/micq_log? This */
                               /* should probably have different levels  */
        BOOL Color;            /* Do we use ANSI color? */
        UWORD Max_Screen_Width;
        BOOL Hermit;
/* aaron
   Variable to hold the time that Micq is started, for the "uptime" command,
   which shows how long Micq has been running. */
        time_t MicqStartTime;
/* end of aaron */

#ifdef MSGEXEC
 /*
  * Ben Simon:
  * receive_script -- a script that gets called anytime we receive
  * a message
  */
        char receive_script[255];
#endif
} user_interface_state;

extern user_interface_state uiG;

/* session global state variables */
typedef struct {
        UDWORD UIN; /* current User Id Number */

/******* should use & 0x3ff in the indexing of all references to last_cmd */
        UWORD last_cmd[1024];   /* command issued for the first */
                                /* 1024 SEQ #'s                 */

        BOOL serv_mess[1024];   /* used so that we don't get duplicate */
                                /* messages with the same SEQ          */

        UWORD seq_num;  /* current sequence number */
        UDWORD our_ip;
        UDWORD our_port; /* the port to make tcp connections on */
        BOOL Quit;
        char passwd[100];
        char server[100];
        UDWORD remote_port;
        UDWORD set_status;
        unsigned int next_resend;
        UDWORD our_session;
        BOOL Done_Login;
        unsigned int away_time, away_time_prev;
        UDWORD real_packs_sent;
        UDWORD real_packs_recv;
        UDWORD Packets_Sent;
        UDWORD Packets_Recv;
        char*  last_message_sent;
        UDWORD last_message_sent_type;
} session_state;

extern session_state ssG;

/* SOCKS5 stuff begin*/
/* SOCKS5 global state variables */
typedef struct {
        int s5Use;
        char s5Host[100];
        unsigned short s5Port;
        int  s5Auth;
        char s5Name[64];
        char s5Pass[64];
        unsigned long s5DestIP;
        unsigned short s5DestPort;
} socks5_state;
/* SOCKS5 stuff end */

extern socks5_state s5G;

#define LOG_MESS 1
#define LOG_AUTO_MESS 2
#define LOG_ONLINE 3

#include "i18n.h"
int Connect_Remote (char *hostname, int port, FD_T aux);
