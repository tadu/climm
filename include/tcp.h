/* $Id$ */

#ifndef MICQ_PEER_H
#define MICQ_PEER_H

#include "contact.h"

#define TCP_STATE_WAITING      32

void ConnectionInitPeer   (Connection *list);

/* Open, close, disallow connection of UIN with this listener */
BOOL TCPDirectOpen     (Connection *list, UDWORD uin);
void TCPDirectClose    (Connection *list, UDWORD uin);
void TCPDirectOff      (Connection *list, UDWORD uin);

/* Do the given peer2peer request */
BOOL TCPSendMsg        (Connection *list, UDWORD uin, const char *msg, UWORD sub_cmd);
UBYTE PeerSendMsg      (Connection *list, Contact *cont, Extra *extra);
BOOL TCPSendFiles      (Connection *list, UDWORD uin, const char *description, const char **file, const char **as, int count);
BOOL TCPGetAuto        (Connection *list, UDWORD uin, UWORD which);
int  TCPSendMsgAck     (Connection *peer, UWORD seq, UWORD sub_cmd, BOOL accept);

Connection *TCPPeer (UDWORD uin);

Connection *PeerFileCreate    (Connection *serv);
UBYTE       PeerFileRequested (Connection *peer, const char *files, UDWORD bytes);
BOOL        PeerFileAccept    (Connection *peer, UWORD status, UDWORD port);

void PeerFileResend (Event *event);
void PeerFileDispatchIncoming (Connection *fpeer);
void PeerFileResend (Event *event);
void PeerFileDispatch (Connection *);

Packet *PeerPacketC    (Connection *peer, UBYTE cmd);
void    PeerPacketSend (Connection *peer, Packet *pak);

void       TCPDispatchShake   (Connection *peer);
void       TCPDispatchReconn  (Connection *peer);
void       TCPDispatchMain    (Connection *peer);
void       TCPDispatchConn    (Connection *peer);
void       TCPClose           (Connection *peer);
void       TCPPrint           (Packet *pak, Connection *peer, BOOL out);

#endif /* MICQ_PEER_H */
