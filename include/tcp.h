/* $Id$ */

#ifndef MICQ_PEER_H
#define MICQ_PEER_H

#include "contact.h"

#define TCP_STATE_WAITING      32

void ConnectionInitPeer   (Connection *list);

/* Open, close, disallow connection of UIN with this listener */
BOOL TCPDirectOpen     (Connection *list, Contact *cont);
void TCPDirectClose    (Connection *list, Contact *cont);
void TCPDirectOff      (Connection *list, Contact *cont);

/* Do the given peer2peer request */
BOOL TCPSendMsg        (Connection *list, Contact *cont, const char *msg, UWORD sub_cmd);
UBYTE PeerSendMsg      (Connection *list, Contact *cont, ContactOptions *opt);
BOOL TCPSendFiles      (Connection *list, Contact *cont, const char *description, const char **file, const char **as, int count);
BOOL TCPGetAuto        (Connection *list, Contact *cont, UWORD which);

Connection *PeerFileCreate    (Connection *serv);
BOOL        PeerFileAccept    (Connection *peer, UWORD status, UDWORD port);
UBYTE       PeerFileIncAccept (Connection *list, Event *event);

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
