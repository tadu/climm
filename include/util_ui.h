/* $Id$ */

UWORD  Get_Max_Screen_Width ();
SDWORD Echo_Off (void);
SDWORD Echo_On (void);

void   M_print (const char *str, ...);
int    M_pos ();
BOOL   Debug (UDWORD level, const char *str, ...);

void   UtilUIUserOnline  (Session *sess, Contact *cont, UDWORD status);
void   UtilUIUserOffline (Session *sess, Contact *cont);
void   UtilUISetVersion (Contact *cont);

#define DEB_PROTOCOL      0x00000008L
#define DEB_PACKET        0x00000010L
#define DEB_QUEUE         0x00000020L
#define DEB_SESSION       0x00000040L
#define DEB_PACK5DATA     0x00000100L
#define DEB_PACK8         0x00001000L
#define DEB_PACK8DATA     0x00002000L
#define DEB_PACK8SAVE     0x00004000L
#define DEB_PACKTCP       0x00010000L
#define DEB_PACKTCPDATA   0x00020000L
#define DEB_PACKTCPSAVE   0x00040000L
#define DEB_TCP           0x00200000L
#define DEB_IO            0x00400000L

