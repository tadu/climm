
/*
 *  Defines for version 2 of the ICQ protocol.
 */

typedef struct
{
   UWORD dummy; /* to fix alignment problem */
   UBYTE ver[2];
   UBYTE cmd[2];
   UBYTE seq[2];
   UBYTE UIN[4];
} pak_2_cmd_s;

typedef struct
{
   UWORD dummy; /* to fix alignment problem */
   UBYTE ver[2];
   UBYTE cmd[2];
   UBYTE seq[2];
} pak_2_srv_s;

typedef struct
{
   UBYTE port[4];
   UBYTE len[2];
} pak_2_log1_s;

typedef struct
{
   UBYTE X1[4];
   UBYTE ip[4];
   UBYTE X2[1];
   UBYTE status[4];
   UBYTE X3[4];
   UBYTE seq[2];
   UBYTE X4[4];
   UBYTE X5[4];
} pak_2_log2_s;


#if ICQ_VER == 2

#define ICQ_pak     pak_2_cmd_s
#define SRV_ICQ_pak pak_2_srv_s
#define login_1     pak_2_log1_s
#define login_2     pak_2_log2_s

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

#endif

#define CMD_ACK               	0x000A 
#define CMD_SENDM             	0x010E
#define CMD_LOGIN             	0x03E8
#define CMD_REG_NEW_USER      	0x03FC
#define CMD_CONT_LIST         	0x0406
#define CMD_SEARCH_UIN        	0x041a
#define CMD_SEARCH_USER       	0x0424
#define CMD_KEEP_ALIVE        	0x042e
#define CMD_SEND_TEXT_CODE    	0x0438
#define CMD_ACK_MESSAGES      	0x0442
#define CMD_LOGIN_1           	0x044c
#define CMD_MSG_TO_NEW_USER   	0x0456
#define CMD_REQ_ADD_LIST      	0X0456
#define CMD_INFO_REQ          	0x0460
#define CMD_EXT_INFO_REQ      	0x046a
#define CMD_CHANGE_PW         	0x049c
#define CMD_NEW_USER_INFO     	0x04A6
#define CMD_UPDATE_EXT_INFO   	0X04B0
#define CMD_QUERY_SERVERS     	0X04BA
#define CMD_QUERY_ADDONS      	0X04C4
#define CMD_STATUS_CHANGE     	0x04d8
#define CMD_NEW_USER_1        	0X04EC
#define CMD_UPDATE_INFO       	0x050A
#define CMD_LOGIN_2           	0x0528
#define CMD_ADD_TO_LIST       	0X053C

#define SRV_ACK            	0x000A
#define SRV_NEW_UIN        	0x0046
#define SRV_LOGIN_REPLY    	0x005A
#define SRV_BAD_PASSWORD   	0x0064
#define SRV_USER_ONLINE    	0x006E
#define SRV_USER_OFFLINE   	0x0078
#define SRV_QUERY          	0x0082
#define SRV_USER_FOUND     	0x008C
#define SRV_END_OF_SEARCH  	0x00A0
#define SRV_NEW_USER       	0x00B4
#define SRV_UPDATE_EXT     	0x00C8
#define SRV_RECV_MESSAGE   	0x00DC
#define SRV_X2             	0x00E6
#define SRV_INFO_REPLY     	0x0118
#define SRV_EXT_INFO_REPLY 	0x0122
#define SRV_STATUS_UPDATE  	0x01A4
#define SRV_SYSTEM_MESSAGE 	0x01C2
#define SRV_UPDATE_SUCCESS 	0x01E0
#define SRV_X1             	0x021C
