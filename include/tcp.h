
#ifndef MICQ_TCP_H
#define MICQ_TCP_H

#include "contact.h"

typedef struct
{
    SOK_T   sok;
    int     state;
    UDWORD  sid;
    struct Contact_t *cont;
} tcpsock_t;

SOK_T TCPInit (int port);

void TCPDirectOpen     (struct Contact_t *cont);
void TCPDirectClose    (struct Contact_t *cont);

/* probably only internal */

void TCPDirectReceive  (SOK_T sok);
int  TCPConnect        (tcpsock_t *sok, int mode);

void TCPHandleComm  (struct Contact_t *cont, int mode);
BOOL TCPSendMsg     (int srv_sok, UDWORD uin, char *msg, UWORD sub_cmd);

int Send_TCP_Ack (tcpsock_t *sok, UWORD seq, UWORD sub_cmd, BOOL accept);
void Handle_TCP_Comm (UDWORD uin);
int Decrypt_Pak (UBYTE *pak, UDWORD size);
void Encrypt_Pak (UBYTE *pak, UDWORD size);
void Get_Auto_Resp (UDWORD uin);
#if 0
char * Get_Auto_Reply ( );
#endif

#endif
