
void Do_TCP_Resend (SOK_T srv_sok);
void Handle_TCP_Comm (UDWORD uin);
void New_TCP (int sok);
void Recv_TCP_Init (int sok);

int Wait_TCP_Init_Ack (int sok);
int Recv_TCP_Pak (int sok, void **pak);
int Send_TCP_Pak (int sok, void *pak, UWORD s);
int Close_TCP_Sok (int sok);
void Save_Dead_Sok (CONTACT_PTR cont);
void Send_TCP_Msg (int srv_sok, UDWORD uin, char *msg, UWORD sub_cmd);
void Send_TCP_Ack (int sok, UWORD seq, UWORD sub_cmd, BOOL accept);
int Decrypt_Pak (UBYTE *pak, UDWORD size);
void Encrypt_Pak (UBYTE *pak, UDWORD size);
void Get_Auto_Resp (UDWORD uin);
char * Get_Auto_Reply ( );
