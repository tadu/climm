/* $Id$ */

#ifdef ENABLE_PEER2PEER

/* TCP related constants */
#define TCP_OK_FLAG      0x04

/* Packets */
#define PEER_INIT        0xff
#define PEER_INITACK     0x01
#define PEER_INIT2       0x03
#define PEER_MSG         0x02

/* Commands */
#define TCP_CMD_CANCEL          2000
#define TCP_CMD_ACK             2010
#define TCP_CMD_MESSAGE         2030

/* Message types */
#define TCP_MSG_CHAT            0x0002
#define TCP_MSG_FILE            0x0003
#define TCP_MSG_GREETING        0x001A
/* see also micq.h */

/* Status flags */
#define TCP_MSGF_REAL           0x0010
#define TCP_MSGF_LIST           0x0020
#define TCP_MSGF_URGENT         0x0040
#define TCP_MSGF_INV            0x0080
#define TCP_MSGF_AWAY           0x0100
#define TCP_MSGF_OCC            0x0200
#define TCP_MSGF_NA             0x0800
#define TCP_MSGF_DND            0x1000

/* Acknowledge statuses */
#define TCP_STAT_ONLINE         0x0000
#define TCP_STAT_REFUSE         0x0001
#define TCP_STAT_AWAY           0x0004
#define TCP_STAT_OCC            0x0009
#define TCP_STAT_DND            0x000A
#define TCP_STAT_NA             0x000E

/* Miscellaneous */
#define TCP_MSG_X1      0x000E
#define TCP_COL_FG      0x00000000      /* Foreground colour in msg body */
#define TCP_COL_BG      0x00FFFFFF      /* Background colour in msg body */
#define TCP_MSG_QUEUE   10

#else
#define TCP_OK_FLAG      0x00
#endif
