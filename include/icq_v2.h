
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
#define SRV_TCP_REQUEST		0x015E
#define SRV_STATUS_UPDATE  	0x01A4
#define SRV_SYSTEM_MESSAGE 	0x01C2
#define SRV_UPDATE_SUCCESS 	0x01E0
#define SRV_X1             	0x021C
