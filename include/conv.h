/* $Id$ */

#ifndef MICQ_UTIL_CONV
#define MICQ_UTIL_CONV

UBYTE ConvEnc (const char *enc);
const char *ConvEncName (UBYTE enc);

char ConvSep ();

#ifdef ENABLE_UTF8
#define     c_out_for(t,c) (CONT_UTF8 (c) ? t : c_out (t))
#define     c_out(t)       ConvFromUTF8 (t, prG->enc_rem)
#define     c_in(t)        ConvToUTF8   (t, prG->enc_rem)
#define     c_strlen(t)    (ENC(enc_loc) == ENC_UTF8 ? s_strlen (t) : strlen (t))
#define     c_delta(t)     (ENC(enc_loc) == ENC_UTF8 ? strlen (t) - s_strlen (t) : 0)
#define     s_delta(t)     strlen (t) - s_strlen (t)
const char *ConvUTF8       (UDWORD codepoint);
const char *ConvToUTF8     (const char *in, UBYTE enc);
const char *ConvFromUTF8   (const char *in, UBYTE enc);
#else
#define     c_out_for(t,c) t
#define     c_out(t)       t
#define     c_in(t)        t
#define     c_strlen(t)    strlen (t)
#define     c_delta(t)     0
#define     s_delta(t)     0
const char *ConvUTF8       (UDWORD codepoint);
#define     ConvToUTF8(i,e)   i
#define     ConvFromUTF8(i,e) i
#endif

#define ENC_AUTO    0x80

#define ENC_UTF8    0x01
#define ENC_LATIN1  0x02
#define ENC_LATIN9  0x03
#define ENC_EUC     0x04
#define ENC_SJIS    0x05  /* Windows Shift-JIS */
#define ENC_KOI8    0x06
#define ENC_WIN1251 0x07  /* Windows code page 1251 */
#define ENC_LATIN1b 0x08  /* also convert 0xfe */

#endif /* MICQ_UTIL_CONV */
