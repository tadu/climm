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

#if ICQ_VER == 5

#define ICQ_pak     pak_5_cmd_s
#define SRV_ICQ_pak pak_5_srv_s

#define BUILD_MICQ 0x7a000000

#endif

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

#define META_SRV_GEN        	0x00C8
#define META_SRV_MORE       	0x00DC
#define META_SRV_WORK       	0x00D2
#define META_SRV_ABOUT      	0x00E6
#define META_SRV_PASS       	0x00AA
#define META_SRV_GEN_UPDATE 	0x0064
#define META_SRV_ABOUT_UPDATE 	0x082
#define META_SRV_OTHER_UPDATE 	0x078

#define META_SRV_WP_FOUND	0x01A4
#define META_SRV_WP_LAST_USER	0x01AE
