/* $Id$ */

#ifndef MICQ_TCP_H
#define MICQ_TCP_H

#include "contact.h"

#define TCP_STATE_WAITING      32

void SessionInitPeer   (Session *list);

/* Open, close, disallow connection of UIN with this listener */
void TCPDirectOpen     (Session *list, UDWORD uin);
void TCPDirectClose    (Session *list, UDWORD uin);
void TCPDirectOff      (Session *list, UDWORD uin);

/* Do the given peer2peer request */
BOOL TCPSendMsg        (Session *list, UDWORD uin, char *msg, UWORD sub_cmd);
BOOL TCPSendFiles      (Session *list, UDWORD uin, char *description, char **file, char **as, int count);
BOOL TCPGetAuto        (Session *list, UDWORD uin, UWORD which);

Session *TCPPeer (UDWORD uin);

Session *PeerFileCreate    (Session *serv);
BOOL     PeerFileRequested (Session *peer, const char *files, UDWORD bytes);
BOOL     PeerFileAccept    (Session *peer, UWORD status, UDWORD port);

void PeerFileResend (Event *event);
void PeerFileDispatchIncoming (Session *fpeer);
void PeerFileResend (Event *event);
void PeerFileDispatch (Session *);

void       TCPDispatchShake   (Session *peer);
void       TCPDispatchReconn  (Session *peer);
void       TCPDispatchMain    (Session *peer);
void       TCPDispatchConn    (Session *peer);
void       TCPSendPacket      (Packet *pak, Session *peer);
void       TCPClose           (Session *peer);
void       TCPPrint           (Packet *pak, Session *peer, BOOL out);


#endif
