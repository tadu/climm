/* $Id$ */

#ifndef MICQ_UTIL_CONV
#define MICQ_UTIL_CONV

UBYTE       ConvEnc        (const char *enc);
const char *ConvEncName    (UBYTE enc);
const char *ConvUTF8       (UDWORD codepoint);
const char *ConvCrush0xFE  (const char *in);
#define     Conv0xFE       (char)0xfe

#ifdef ENABLE_UTF8
const char *ConvToUTF8     (const char *in, UBYTE enc, UBYTE keep0xfe);
const char *ConvFromUTF8   (const char *in, UBYTE enc);
#define     c_out_for(t,c) (CONT_UTF8 (c) ? t : c_out_to (t,c))
#define     c_out(t)       ConvFromUTF8 (t, prG->enc_rem)
#define     c_in(t)        ConvToUTF8   (t, prG->enc_rem, 1)
#define     c_out_to(t,c)  ConvFromUTF8 (t, (c) && (c)->encoding ? (c)->encoding : prG->enc_rem)
#define     c_in_to(t,c)   ConvToUTF8   (t, (c) && (c)->encoding ? (c)->encoding : prG->enc_rem, 1)
#define     c_in_to_0(t,c) ConvToUTF8   (t, (c) && (c)->encoding ? (c)->encoding : prG->enc_rem, 0)
#define     c_strlen(t)    (ENC(enc_loc) == ENC_UTF8 ? s_strlen (t) : strlen (t))
#define     c_offset(t,o)  (ENC(enc_loc) == ENC_UTF8 ? s_offset (t, o) : (o))
#define     c_delta(t)     (int)(ENC(enc_loc) == ENC_UTF8 ? strlen (t) - s_strlen (t) : 0)
#define     s_delta(t)     (int)strlen (t) - (int)s_strlen (t)
#else
#define     ConvToUTF8(i,e,k) i
#define     ConvFromUTF8(i,e) i
#define     c_out_for(t,c) t
#define     c_out(t)       t
#define     c_in(t)        t
#define     c_out_to(t,c)  t
#define     c_in_to(t,c)   t
#define     c_in_to_0(t,c) t
#define     c_strlen(t)    strlen (t)
#define     c_offset(t,o)  o
#define     c_delta(t)     0
#define     s_delta(t)     0
#endif

#define ENC_AUTO    0x80

#define ENC_UTF8    0x01
#define ENC_LATIN1  0x02
#define ENC_LATIN9  0x03
#define ENC_KOI8    0x04
#define ENC_WIN1251 0x05  /* Windows code page 1251 */
#define ENC_MAX_BUILTIN ENC_WIN1251
#define ENC_EUC     0x06
#define ENC_SJIS    0x07  /* Windows Shift-JIS */


#endif /* MICQ_UTIL_CONV */
