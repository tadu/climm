/* $Id$ */

#ifndef MICQ_TCP_H
#define MICQ_TCP_H

#include "contact.h"

#define TCP_STATE_WAITING      32

void SessionInitPeer   (Session *sess);

void TCPDirectOpen     (Session *sess, UDWORD uin);
void TCPDirectClose    (               UDWORD uin);
void TCPDirectOff      (               UDWORD uin);

BOOL TCPSendMsg        (Session *sess, UDWORD uin, char *msg, UWORD sub_cmd);
BOOL TCPSendFiles      (Session *sess, UDWORD uin, char *description, char **file, char **as, int count);
BOOL TCPGetAuto        (Session *sess, UDWORD uin, UWORD which);

Session *TCPPeer (UDWORD uin);

Session *PeerFileCreate (Session *serv);
BOOL     PeerFileRequested (Session *peer, const char *files, UDWORD bytes);
BOOL     PeerFileAccept (Session *peer, UWORD status, UDWORD port);

void PeerFileResend (Event *event);
void PeerFileDispatchIncoming (Session *fpeer);
void PeerFileResend (Event *event);
void PeerFileDispatch (Session *);

void       TCPDispatchShake   (Session *sess);
void       TCPDispatchReconn  (Session *sess);
void       TCPDispatchMain    (Session *sess);
void       TCPDispatchConn    (Session *sess);
void       TCPSendPacket      (Packet *pak, Session *sess);
void       TCPClose           (Session *sess);
void       TCPPrint           (Packet *pak, Session *sess, BOOL out);


#endif
