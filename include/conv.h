/* $Id$ */

#ifndef MICQ_UTIL_CONV
#define MICQ_UTIL_CONV

const char *ConvUTF8     (UDWORD codepoint);
const char *ConvToUTF8   (const char *in, UBYTE enc);
const char *ConvFromUTF8 (const char *in, UBYTE enc);

char ConvSep ();

#ifdef ENABLE_UTF8
#define c_out(t)    ConvFromUTF8 (t, prG->enc_rem)
#define c_in(t)     ConvToUTF8   (t, prG->enc_rem)
#define c_strlen(t) (prG->enc_loc == ENC_UTF8 ? s_strlen (t) : strlen (t))
#else
#define c_out(t)    t
#define c_in(t)     t
#define c_strlen(t) strlen (t)
#endif


#endif /* MICQ_UTIL_CONV */
