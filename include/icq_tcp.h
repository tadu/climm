
#define TCP_VER_REV      0

/* TCP related constants */
#ifdef TCP_COMM

/* TCP port to listen on... 0 = random -- james */
#define TCP_PORT 0 

#define TCP_OK_FLAG      0x04

/* packets */

#define PEER_INIT        0xff
#define PEER_INITACK     0x01
#define PEER_INIT2       0x03
#define PEER_MSG         0x02

/* Commands */
#define TCP_CMD_GET_AWAY        1000
#define TCP_CMD_GET_OCC         1001
#define TCP_CMD_GET_NA          1002
#define TCP_CMD_GET_DND         1003
#define TCP_CMD_GET_FFC         1004
#define TCP_AUTO_RESPONSE_MASK  0x03E0
#define TCP_CMD_CANCEL          2000
#define TCP_CMD_ACK             2010
#define TCP_CMD_MESSAGE         2030

/* Message types */
#define TCP_MSG_AUTO            0x0000
#define TCP_MSG_MESS            0x0002
#define TCP_MSG_FILE            0x0003
#define TCP_MSG_URL             0x0004
#define TCP_MSG_REQ_AUTH        0x0006
#define TCP_MSG_DENY_AUTH       0x0007
#define TCP_MSG_GIVE_AUTH       0x0008
#define TCP_MSG_ADDED           0x000C
#define TCP_MSG_WEB_PAGER       0x000D
#define TCP_MSG_EMAIL_PAGER     0x000E
#define TCP_MSG_ADDUIN          0x0013
#define TCP_MSG_GREETING        0x001A

/* Status flags */
#define TCP_MSGF_REAL           0x0010
#define TCP_MSGF_LIST           0x0020
#define TCP_MSGF_URGENT         0x0040
#define TCP_MSGF_INV            0x0080
#define TCP_MSGF_AWAY           0x0100
#define TCP_MSGF_OCC            0x0200
#define TCP_MSGF_NA             0x0800
#define TCP_MSGF_DND            0x1000

/* Message statuses */
#define TCP_STAT_ONLINE         0x0000
#define TCP_STAT_REFUSE         0x0001
#define TCP_STAT_AWAY           0x0004
#define TCP_STAT_OCC            0x0009
#define TCP_STAT_DND            0x000A
#define TCP_STAT_NA             0x000E

/* Miscellaneous */
#define TCP_MSG_X1      0x000E
#define TCP_COL_FG      0x00000000      /* Foreground colour in msg body */
#define TCP_COL_BG      0x00FFFFFF      /* Background colour in msg boxy */
#define TCP_MSG_QUEUE   10

#endif


