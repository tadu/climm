
#define TCP_VER		6
#define TCP_VER_REV	0

/* TCP related constants */
#ifdef TCP_COMM

/* TCP port to listen on... 0 = random -- james */
#define TCP_PORT 0 

#if TCP_VER == 6
/* packet struct */
typedef struct
{
    UBYTE cmd;           /* 0xFF */
    UBYTE version[2];
    UBYTE rev[2];
    UBYTE dest_uin[4];
    UBYTE X1[2];         /* null */
    UBYTE port[4];
    UBYTE uin[4];
    UBYTE current_ip[4];
    UBYTE other_ip[4];
    UBYTE connection_type;
    UBYTE other_port[4];
    UBYTE session_id[4];
} TCP_INIT_PAK, *TCP_INIT_PTR;

typedef struct
{
    UBYTE cmd;          /* 0x01 */
    UBYTE X1[3];
} TCP_INIT_ACK_PAK, *TCP_INIT_ACK_PTR;

typedef struct
{
    UBYTE checksum[4];
    UBYTE cmd[2];
    UBYTE X1[2];
    UBYTE seq[2];
    UBYTE X2[4];
    UBYTE X3[4];
    UBYTE X4[4];
    UBYTE sub_cmd[2];
    UBYTE status[2];
    UBYTE msg_type[2];
    UBYTE size[2];
} TCP_MSG_PAK, *TCP_MSG_PTR;
#else
#error Undefined TCP version
#endif

#define TCP_OK_FLAG	0x04

/* Commands */
#define TCP_INIT		0xFF
#define TCP_INIT_ACK		0x01
#define TCP_CMD_CANCEL		0x07D0
#define TCP_CMD_ACK		0x07DA
#define TCP_CMD_MESSAGE		0x07EE 
#define TCP_CMD_GET_AWAY	0x03E8
#define TCP_CMD_GET_OCC		0x03E9
#define TCP_CMD_GET_NA		0x03EA
#define TCP_CMD_GET_DND		0x03EB
#define TCP_CMD_GET_FFC		0x03EC
#define TCP_AUTO_RESPONSE_MASK  0x03E0

/* Message types */
#define TCP_MSG_AUTO		0x0000
#define TCP_MSG_REAL		0x0010
#define TCP_MSG_LIST		0x0020
#define TCP_MSG_URGENT		0x0040
#define TCP_MSGF_INV		0x0080
#define TCP_MSGF_AWAY		0x0100
#define TCP_MSGF_OCC		0x0200
#define TCP_MSGF_NA		0x0800
#define TCP_MSGF_DND		0x1000

/* Message statuses */
#define TCP_STAT_OK		0x0000
#define TCP_STAT_REFUSE		0x0001
#define TCP_STAT_AWAY		0x0004
#define TCP_STAT_OCC		0x0009
#define TCP_STAT_DND		0x000A
#define TCP_STAT_NA		0x000E

/* Miscellaneous */
#define TCP_MSG_OFFSET  30
#define TCP_MSG_X1      0x000E
#define COL_FG          0x00000000	/* Foreground colour in msg body */
#define COL_BG          0x00FFFFFF	/* Background colour in msg boxy */
#define TCP_MSG_QUEUE   10

extern struct msg_queue *tcp_rq, *tcp_sq;   /* recv and send queues */
#endif


#define CHAT_MESS       0x0002
#define FILE_MESS       0x0003
