/**  rus_conv.c  **/

#include "micq.h"
#include "conv.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

/* 19981206 alvar (Alexander Varin) added conversion of russian letter io */

/********************************************************
Russian language ICQ fix.
Usual Windows ICQ users do use Windows 1251 encoding but
unix users do use koi8 encoding, so we need to convert it.
This function will convert string from windows 1251 to koi8
or from koi8 to windows 1251.
Andrew Frolov dron@ilm.net
*********************************************************
Fixed for win1251<->koi8-u convertion. Now all Ukrainian letters
                are transcoded!
Kunytsa Olexander aka Cawko Xakep       UKMA InterNet Project
xakep@snark.ukma.kiev.ua                #19983820
*********************************************************/

static UBYTE kw[] = {
    128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
    144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
    160, 161, 162, 184, 186, 165, 179, 191, 168, 169, 170, 171, 172, 180, 174, 175,
    176, 177, 178, 168, 170, 181, 178, 175, 184, 185, 170, 187, 188, 165, 190, 169,
    254, 224, 225, 246, 228, 229, 244, 227, 245, 232, 233, 234, 235, 236, 237, 238,
    239, 255, 240, 241, 242, 243, 230, 226, 252, 251, 231, 248, 253, 249, 247, 250,
    222, 192, 193, 214, 196, 197, 212, 195, 213, 200, 201, 202, 203, 204, 205, 206,
    207, 223, 208, 209, 210, 211, 198, 194, 220, 219, 199, 216, 221, 217, 215, 218
};

static UBYTE wk[] = {
    128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
    144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
    160, 161, 162, 163, 164, 189, 166, 167, 179, 191, 180, 171, 172, 173, 174, 183,
    176, 177, 182, 166, 173, 181, 182, 183, 163, 185, 164, 187, 188, 189, 190, 167,
    225, 226, 247, 231, 228, 229, 246, 250, 233, 234, 235, 236, 237, 238, 239, 240,
    242, 243, 244, 245, 230, 232, 227, 254, 251, 253, 255, 249, 248, 252, 224, 241,
    193, 194, 215, 199, 196, 197, 214, 218, 201, 202, 203, 204, 205, 206, 207, 208,
    210, 211, 212, 213, 198, 200, 195, 222, 219, 221, 223, 217, 216, 220, 192, 209
};

#ifdef _WIN32
#include <windows.h>

/* If you have errors here, try OemToAnsi, AnsiToOem instead. */

void ConvWinKoi (char *in)
{
    char tempbuff[strlen (in) + 1];
    
/*    if (!(tempbuff = strdup (in))
        return;*/

    CharToOem (in, tempbuff);
    strcpy (in, tempbuff);
/*    free (tempbuff);*/
}

void ConvKoiWin (char *in)
{
    char tempbuff[strlen (in) + 1];
    
/*    if (!(tempbuff = strdup (in))
        return;*/
    
    OemToChar (in, tempbuff);
    strcpy (in, tempbuff);
/*    free (tempbuff);*/
}

#else

void ConvKoiWin (char *in)
{
    for ( ; *in; in++)
    {
        *in &= 0377;
        if (*in & 0200)
            *in = kw[*in & 0177];
    }
}

void ConvWinKoi (char *in)
{
    for ( ; *in; in++)
    {
        *in &= 0377;
        if (*in & 0200)
            *in = wk[*in & 0177];
    }
}
#endif
