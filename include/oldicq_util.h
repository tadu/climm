/* $Id$ */

#ifndef MICQ_ICQV5_UTIL_H
#define MICQ_ICQV5_UTIL_H

Packet *PacketCv5 (Connection *conn, UWORD cmd);
void PacketEnqueuev5 (Packet *pak, Connection *conn);
void PacketSendv5 (const Packet *pak, Connection *conn);
void UDPCallBackResend (Event *event);
const char *CmdPktCmdName (UWORD cmd);

#define CMD_v5_OFF_VER    0
#define CMD_v5_OFF_ZERO   2
#define CMD_v5_OFF_UIN    6
#define CMD_v5_OFF_SESS  10
#define CMD_v5_OFF_CMD   14
#define CMD_v5_OFF_SEQ   16
#define CMD_v5_OFF_SEQ2  18
#define CMD_v5_OFF_CHECK 20
#define CMD_v5_OFF_PARAM 24

#define CMD_ACK                 10
#define CMD_SEND_MESSAGE        270
#define CMD_TCP_REQUEST         350
#define CMD_LOGIN               1000
#define CMD_REG_NEW_USER        1020
#define CMD_CONTACT_LIST        1030
#define CMD_SEARCH_UIN          1050 /* */
#define CMD_SEARCH_USER         1060
#define CMD_KEEP_ALIVE          1070
#define CMD_SEND_TEXT_CODE      1080
#define CMD_ACK_MESSAGES        1090
#define CMD_LOGIN_1             1100
#define CMD_MSG_TO_NEW_USER     1110 /* */
#define CMD_INFO_REQ            1120 /* */ 
#define CMD_EXT_INFO_REQ        1130
#define CMD_CHANGE_PW           1180 /* - */
#define CMD_NEW_USER_INFO       1190 /* */
#define CMD_UPDATE_EXT_INFO     1200 /* */
#define CMD_QUERY_SERVERS       1210 /* */
#define CMD_QUERY_ADDONS        1220 /* */
#define CMD_STATUS_CHANGE       1240
#define CMD_NEW_USER_1          1260 /* */
#define CMD_UPDATE_INFO         1290
#define CMD_AUTH_UPDATE         1300 /* -> */
#define CMD_KEEP_ALIVE2         1310 /* -> */
#define CMD_LOGIN_2             1320 /* - */
#define CMD_ADD_TO_LIST         1340 /* */
#define CMD_RAND_SET            1380
#define CMD_RAND_SEARCH         1390
#define CMD_META_USER           1610
#define CMD_INVIS_LIST          1700
#define CMD_VIS_LIST            1710
#define CMD_UPDATE_LIST         1720

#endif /* MICQ_ICQV5_UTIL_H */
