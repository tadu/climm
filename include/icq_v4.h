/*
 *  Defines for version 4 of the ICQ protocol.
 */

typedef struct
{
   UBYTE dummy[2]; /* to fix alignment problem */
   UBYTE ver[2];
   UBYTE rand[2];
   UBYTE zero[2];
   UBYTE cmd[2];
   UBYTE seq[2];
   UBYTE seq2[2];
   UBYTE UIN[4];
   UBYTE checkcode[4];
} pak_4_cmd_s;

typedef struct
{
   UWORD dummy; /* to fix alignment problem */
   UBYTE ver[2];
   UBYTE cmd[2];
   UBYTE seq[2];
   UBYTE seq2[2];
   UBYTE UIN[4];
   UBYTE check[4];
} pak_4_srv_s;

typedef struct
{
   UBYTE time[4];
   UBYTE port[4];
   UBYTE len[2];
} pak_4_log1_s;

typedef struct
{
   UBYTE X1[4];
   UBYTE ip[4];
   UBYTE X2[1];
   UBYTE status[4];
   UBYTE X3[4];
   UBYTE X4[4];
   UBYTE X5[4];
} pak_4_log2_s;


#if ICQ_VER == 4

#define ICQ_pak     pak_4_cmd_s
#define SRV_ICQ_pak pak_4_srv_s
#define login_1     pak_4_log1_s
#define login_2     pak_4_log1_s

/* those behind the // are for the spec on
 http://www.student.nada.kth.se/~d95-mih/icq/
 they didn't work for me so I changed them
 to values that did work.
 *********************************/
/*#define LOGIN_X1_DEF 0x00000078 */
#define LOGIN_X1_DEF 0x00000098
/*   #define LOGIN_X2_DEF 0x04*/
#define LOGIN_X2_DEF 0x06
/*#define LOGIN_X3_DEF 0x00000002*/
#define LOGIN_X3_DEF 0x00000003
/*#define LOGIN_X4_DEF 0x00000000*/
#define LOGIN_X4_DEF 0x00000000
/*#define LOGIN_X5_DEF 0x00780008*/
#define LOGIN_X5_DEF 0x00980000

#endif
