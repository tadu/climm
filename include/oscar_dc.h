/* $Id$ */

#ifndef CLIMM_PEER_H
#define CLIMM_PEER_H

#include "contact.h"

Event *ConnectionInitPeer   (Connection *list);

/* Open, close, disallow connection of UIN with this listener */
BOOL TCPDirectOpen     (Connection *list, Contact *cont);
void TCPDirectClose    (Connection *list, Contact *cont);
void TCPDirectOff      (Connection *list, Contact *cont);

/* Do the given peer2peer request */
UBYTE PeerSendMsg      (Connection *list, Contact *cont, UDWORD type, const char *text);
UBYTE PeerSendMsgFat   (Connection *list, Contact *cont, Message *msg);
BOOL TCPSendFiles      (Connection *list, Contact *cont, const char *description, const char **file, const char **as, int count);
BOOL TCPGetAuto        (Connection *list, Contact *cont, UWORD which);
BOOL TCPSendSSLReq     (Connection *list, Contact *cont);

Connection *PeerFileCreate    (Server *serv);
BOOL        PeerFileAccept    (Connection *peer, UWORD ackstatus, UDWORD port);
UBYTE       PeerFileIncAccept (Connection *list, Event *event);

void PeerFileResend (Event *event);
void PeerFileDispatchIncoming (Connection *fpeer);
void PeerFileDispatch (Connection *);

Packet *PeerPacketC    (Connection *peer, UBYTE cmd);
void    PeerPacketSend (Connection *peer, Packet *pak);

void       TCPDispatchShake   (Connection *peer);
void       TCPDispatchReconn  (Connection *peer);
void       TCPDispatchMain    (Connection *peer);
void       TCPDispatchConn    (Connection *peer);
void       TCPClose           (Connection *peer);
void       TCPPrint           (Packet *pak, Connection *peer, BOOL out);

#define ASSERT_MSGLISTEN(s)   (assert (s), assert ((s)->type == TYPE_MSGLISTEN), assert ((s)->serv), assert ((s)->serv->oscar_dc == (s)))
#define ASSERT_MSGDIRECT(s)   (assert (s), assert ((s)->type == TYPE_MSGDIRECT), assert ((s)->serv), ASSERT_MSGLISTEN ((s)->serv->oscar_dc))
#define ASSERT_FILELISTEN(s)  (assert (s), assert ((s)->type == TYPE_FILELISTEN), assert ((s)->serv))
#define ASSERT_FILEDIRECT(s)  (assert (s), assert ((s)->type == TYPE_FILEDIRECT), assert ((s)->serv), ASSERT_FILELISTEN ((s)->serv->oscar_dc))
#define ASSERT_FILE(s)        (assert (s), assert ((s)->type == TYPE_FILE), ASSERT_FILEDIRECT ((s)->serv))

#endif /* CLIMM_PEER_H */
