
SOK_T TCPInit (int port);
BOOL TCPSendMsg (int srv_sok, UDWORD uin, char *msg, UWORD sub_cmd);
void TCPClose (Contact *cont);
void TCPHandleComm (Contact *cont);
int TCPConnect (Contact *cont);
int TCPReadPacket (SOK_T sok, void **pak);
int TCPSendPacket (SOK_T sok, void *pak, UWORD size);


void Do_TCP_Resend (SOK_T srv_sok);
void Handle_TCP_Comm (UDWORD uin);
void New_TCP (int sok);
int Recv_TCP_Init (int sok);
void Send_TCP_Init (UDWORD dest_uin);
int Wait_TCP_Init_Ack (int sok);
int Recv_TCP_Pak (int sok, void **pak);
int Send_TCP_Pak (int sok, void *pak, UWORD s);
int Close_TCP_Sok (int sok);
int Send_TCP_Ack (int sok, UWORD seq, UWORD sub_cmd, BOOL accept);
int Decrypt_Pak (UBYTE *pak, UDWORD size);
void Encrypt_Pak (UBYTE *pak, UDWORD size);
void Get_Auto_Resp (UDWORD uin);
#if 0
char * Get_Auto_Reply ( );
#endif