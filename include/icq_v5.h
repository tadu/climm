/*
 *  Defines for version 5 of the ICQ protocol.
 */

#define BUILD_MICQ 0x7a000000
#define BUILD_LICQ 0x7d000000
#define BUILD_SSL  0x00800000

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
