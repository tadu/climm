
#ifndef MICQ_TCP_H
#define MICQ_TCP_H

#include "contact.h"

#define TCP_STATE_ESTABLISHED 32
#define TCP_STATE_CONNECTED    6
#define TCP_STATE_WAITING     -2
#define TCP_STATE_FAILED      -1

typedef struct
{
    SOK_T   sok;
    int     state;
    int     ip;
    UDWORD  sid;
    struct Contact_t *cont;
} tcpsock_t;

void TCPInit           (Session *sess, int port);

void TCPDirectOpen     (Session *sess, struct Contact_t *cont);
void TCPDirectClose    (               struct Contact_t *cont);

void TCPAddSockets     (Session *sess);
void TCPDispatch       (Session *sess);

/* probably only internal */

void TCPDirectReceive  (Session *sess);
int  TCPConnect        (Session *sess, tcpsock_t *sok, int mode);

void TCPHandleComm     (Session *sess, struct Contact_t *cont, int mode);
BOOL TCPSendMsg        (Session *sess, UDWORD uin, char *msg, UWORD sub_cmd);

void CallBackLoginTCP  (struct Event *event);
int Send_TCP_Ack (Session *sess, tcpsock_t *sok, UWORD seq, UWORD sub_cmd, BOOL accept);
void Handle_TCP_Comm (Session *sess, UDWORD uin);
/*int Decrypt_Pak (UBYTE *pak, UDWORD size);
void Encrypt_Pak (UBYTE *pak, UDWORD size);*/
void Get_Auto_Resp (UDWORD uin);
void SessionInitPeer (Session *sess);
#if 0
char * Get_Auto_Reply ( );
#endif

#endif
