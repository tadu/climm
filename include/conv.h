/* $Id$ */

#ifndef MICQ_UTIL_CONV
#define MICQ_UTIL_CONV

char ConvSep ();
void ConvWinUnix (char *text);
void ConvUnixWin (char *text);

void ConvSjisEuc (char *);
void ConvEucSjis (char *);

void ConvWinKoi  (char *in);
void ConvKoiWin  (char *in);
const char *ConvToUTF8   (const char *in, UBYTE enc);
const char *ConvFromUTF8 (const char *in, UBYTE enc);

#ifdef ENABLE_UTF8
#define c_out(t) ConvFromUTF8 (t, prG->enc_rem)
#define c_in(t)  ConvToUTF8   (t, prG->enc_rem)
#else
#define c_out(t) t
#define c_in(t)  t
#endif


#endif /* MICQ_UTIL_CONV */
