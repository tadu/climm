/* $Id$ */

UWORD  Get_Max_Screen_Width();
SDWORD Echo_Off (void);
SDWORD Echo_On (void);

void   Prompt (void);
void   Soft_Prompt (void);
void   Time_Stamp (void);
void   Kill_Prompt (void);
void   M_print (const char *str, ...);
int    M_pos ();
void   Debug (UDWORD level, const char *str, ...);

void   UtilUIUserOnline  (Session *sess, Contact *cont, UDWORD status);
void   UtilUIUserOffline (Contact *cont);

#define DEB_PACKET        64
#define DEB_QUEUE         32
#define DEB_PACK5DATA      4
#define DEB_PACKTCP       16
#define DEB_PACKTCPDATA 2048
#define DEB_PACKTCPSAVE 8192
#define DEB_PACK8       4096
#define DEB_PACK8DATA    128
#define DEB_PACK8SAVE    256
#define DEB_PROTOCOL       8
#define DEB_TCP          512
#define DEB_IO          1024

