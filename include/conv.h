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

#endif /* MICQ_UTIL_CONV */
