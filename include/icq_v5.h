/*
 *  Defines for version 5 of the ICQ protocol.
 */

typedef struct
{
   UBYTE dummy[2]; /* to fix alignment problem */
   UBYTE ver[2];
   UBYTE zero[4];
   UBYTE UIN[4];
   UBYTE session[4];
   UBYTE cmd[2];
   UBYTE seq2[2];
   UBYTE seq[2];
   UBYTE checkcode[4];
} pak_5_cmd_s;

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
} pak_5_srv_s;

#define CMD_OFFSET 14
#define SEQ_OFFSET 16
#define SEQ2_OFFSET 18
#define DATA_OFFSET 4
#define PAK_DATA_OFFSET 24

typedef struct
{
   UBYTE X1[4];
   UBYTE ip[4];
   UBYTE X2[1];
   UBYTE status[4];
   UBYTE X3[4];
   UBYTE X4[4];
   UBYTE X5[4];
   UBYTE X6[4];
   UBYTE X7[4];
   UBYTE X8[4];
} pak_5_log2_s;


#if ICQ_VER == 5

#define ICQ_pak     pak_5_cmd_s
#define SRV_ICQ_pak pak_5_srv_s
#define login_1     pak_4_log1_s
#define login_2     pak_5_log2_s

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
/*#define LOGIN_X5_DEF 0x00d50008*/
#define LOGIN_X5_DEF 0x822c01ec
/*#define LOGIN_X5_DEF 0x00C80010*/
#define LOGIN_X6_DEF 0x00000050
#define LOGIN_X7_DEF 0x00000003
/*#define LOGIN_X8_DEF 0x36abff13*/ 
#define LOGIN_X8_DEF 0x36d61600 /* == 920 000 000 */

#endif

#define CMD_AUTH_UPDATE       	0x0514
#define CMD_KEEP_ALIVE2       	0x051e
#define CMD_RAND_SET          	0x0564
#define CMD_RAND_SEARCH       	0x056E
#define CMD_INVIS_LIST        	0x06A4
#define CMD_META_USER         	0x064A
#define CMD_VIS_LIST          	0x06AE
#define CMD_UPDATE_LIST       	0x06B8

#define SRV_GO_AWAY        	0x0028
#define SRV_BAD_PASS       	0x0064
#define SRV_NOT_CONNECTED  	0x00F0
#define SRV_TRY_AGAIN      	0x00FA
#define SRV_SYS_DELIVERED_MESS 	0x0104
#define SRV_UPDATE_FAIL    	0x01EA
#define SRV_AUTH_UPDATE    	0x01F4
#define SRV_MULTI_PACKET   	0x0212
#define SRV_RAND_USER      	0x024E
#define SRV_META_USER      	0x03DE

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

#define META_CMD_WP     	0x0533
#define META_SRV_WP_FOUND	0x01A4
#define META_SRV_WP_LAST_USER	0x01AE
