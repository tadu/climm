/*
 * This file handles character conversions.
 *
 * This file is Copyright Rüdiger Kuhlmann; it may be distributed under
 * version 2 of the GPL licence.
 *
 * $Id$
 */

#include "micq.h"
#include <string.h>
#include <errno.h>
#include "conv.h"
#include "preferences.h"
#include "util_str.h"

#ifdef ENABLE_ICONV
#include <iconv.h>
typedef struct { const char *enc; iconv_t to; iconv_t from; } enc_t;
#else
typedef struct { const char *enc; } enc_t;
#endif

static int conv_nr = 0;
static enc_t *conv_encs = NULL;

/*
 * Give an ID for the given encoding name
 */
UBYTE ConvEnc (const char *enc)
{
    UBYTE nr;

    if (!conv_encs || !conv_nr)
    {
        conv_encs = calloc (sizeof (enc_t), conv_nr = 10);
        conv_encs[0].enc = strdup ("none");
        conv_encs[1].enc = strdup ("utf-8");
        conv_encs[2].enc = strdup ("iso-8859-1");
        conv_encs[3].enc = strdup ("iso-8859-15");
        conv_encs[4].enc = strdup ("koi8-u");
        conv_encs[5].enc = strdup ("windows-1251");
        conv_encs[6].enc = strdup ("euc-jp");
        conv_encs[7].enc = strdup ("shift-jis");
    }
    for (nr = 0; conv_encs[nr].enc; nr++)
        if (!strcasecmp (conv_encs[nr].enc, enc))
            return nr;
    
#ifdef ENABLE_ICONV
    if (nr == conv_nr - 1)
    {
        enc_t *new = realloc (conv_encs, sizeof (const char*) * conv_nr + 10);
        if (!new)
            return 0;
        conv_nr += 10;
        conv_encs = new;
    }
    conv_encs[nr].to   = iconv_open ("utf-8", enc);
    conv_encs[nr].from = iconv_open (enc, "utf-8");
    if (conv_encs[nr].to == (iconv_t)(-1) || conv_encs[nr].from == (iconv_t)(-1))
        return 0;
    conv_encs[nr].enc = strdup (enc);
    conv_encs[nr + 1].enc = NULL;
    return nr;
#else
    return 0;
#endif
}

/*
 * Give the encoding name for a given ID
 */
const char *ConvEncName (UBYTE enc)
{
    return conv_encs[enc & ~ENC_AUTO].enc;
}

char ConvSep ()
{
    static char conv = '\0';
    
    if (conv)
        return conv;
    return conv = ConvFromUTF8 (ConvToUTF8 ("\xfe", prG->enc_rem), prG->enc_loc)[0];
}

#ifdef ENABLE_UTF8
/*
 * Convert a single unicode code point to UTF-8
 */
const char *ConvUTF8 (UDWORD x)
{
    static char b[7];
    
    if      (!(x & 0xffffff80))
    {
        b[0] = x;
        b[1] = '\0';
    }
    else if (!(x & 0xfffff800))
    {
        b[0] = 0xc0 |  (x               >>  6);
        b[1] = 0x80 |  (x &       0x3f);
        b[2] = '\0';
    }
    else if (!(x & 0xffff0000))
    {
        b[0] = 0xe0 | ( x               >> 12);
        b[1] = 0x80 | ((x &      0xfc0) >>  6);
        b[2] = 0x80 |  (x &       0x3f);
        b[3] = '\0';
    }
    else if (!(x) & 0xffe00000)
    {
        b[0] = 0xf0 | ( x               >> 18);
        b[1] = 0x80 | ((x &    0x3f000) >> 12);
        b[2] = 0x80 | ((x &      0xfc0) >>  6);
        b[3] = 0x80 |  (x &       0x3f);
        b[4] = '\0';
    }
    else if (!(x) & 0xfc000000)
    {
        b[0] = 0xf8 | ( x               >> 24);
        b[1] = 0x80 | ((x &   0xfc0000) >> 18);
        b[2] = 0x80 | ((x &    0x3f000) >> 12);
        b[3] = 0x80 | ((x &      0xfc0) >>  6);
        b[4] = 0x80 |  (x &       0x3f);
        b[5] = '\0';
    }
    else if (!(x) & 0x80000000)
    {
        b[0] = 0xfc | ( x               >> 30);
        b[1] = 0x80 | ((x & 0x3f000000) >> 24);
        b[2] = 0x80 | ((x &   0xfc0000) >> 18);
        b[3] = 0x80 | ((x &    0x3f000) >> 12);
        b[4] = 0x80 | ((x &      0xfc0) >>  6);
        b[5] = 0x80 |  (x &       0x3f);
        b[6] = '\0';
    }
    else
        return "?";
    return b;
}

#ifdef ENABLE_ICONV

const char *ConvToUTF8 (const char *inn, UBYTE enc)
{
    static char *t = NULL;
    static UDWORD size = 0;
    size_t inleft, outleft;
    char *in, *out, *tmp;

    t = s_catf (t, &size, "%*s", 100, "");
    *t = '\0';
    
    enc &= ~ENC_AUTO;
    if (!conv_nr)
        ConvEnc ("utf-8");
    if (enc >= conv_nr || !enc)
        return s_sprintf ("<invalid encoding %d>", enc);
    if (!conv_encs[enc].to)
    {
        conv_encs[enc].to = iconv_open ("utf-8", conv_encs[enc].enc);
        if (conv_encs[enc].to == (iconv_t)(-1))
            return "<invalid encoding unsupported>";
    }
    iconv (conv_encs[enc].to, NULL, NULL, NULL, NULL);
    in = (char *)inn;
    out = t;
    inleft = strlen (in);
    outleft = size - 1;
    while (iconv (conv_encs[enc].to, &in, &inleft, &out, &outleft) == (size_t)(-1))
    {
        UDWORD rc = errno;
        if (outleft < 10 || rc == E2BIG)
        {
            UDWORD done = out - t;
            tmp = realloc (t, size + 50);
            if (!tmp)
                break;
            t = tmp;
            size += 50;
            outleft += 50;
            out = t + done;
        }
        else if (rc == EILSEQ)
        {
            *out++ = (*in == (char)0xfe) ? 0xfe : '?';
            outleft--;
            in++;
            inleft++;
        }
        else /* EINVAL */
        {
            *out++ = '?';
            break;
        }
    }
    *out = '\0';
    return t;
}

const char *ConvFromUTF8 (const char *inn, UBYTE enc)
{
    static char *t = NULL;
    static UDWORD size = 0;
    size_t inleft, outleft;
    char *in, *out, *tmp;

    t = s_catf (t, &size, "%*s", 100, "");
    *t = '\0';
    
    enc &= ~ENC_AUTO;
    if (!conv_nr)
        ConvEnc ("utf-8");
    if (enc >= conv_nr || !enc)
        return s_sprintf ("<invalid encoding %d>", enc);
    if (!conv_encs[enc].from)
    {
        conv_encs[enc].from = iconv_open (conv_encs[enc].enc, "utf-8");
        if (conv_encs[enc].from == (iconv_t)(-1))
            return "<invalid encoding unsupported>";
    }
    iconv (conv_encs[enc].from, NULL, NULL, NULL, NULL);
    in = (char *)inn;
    out = t;
    inleft = strlen (in);
    outleft = size - 1;
    while (iconv (conv_encs[enc].from, &in, &inleft, &out, &outleft) == (size_t)(-1))
    {
        UDWORD rc = errno;
        if (outleft < 10 || rc == E2BIG)
        {
            UDWORD done = out - t;
            tmp = realloc (t, size + 50);
            if (!tmp)
                break;
            t = tmp;
            size += 50;
            outleft += 50;
            out = t + done;
        }
        else if (rc == EILSEQ)
        {
            *out++ = (*in == (char)0xfe) ? 0xfe : '?';
            outleft--;
            in++;
            inleft++;
        }
        else /* EINVAL */
        {
            *out++ = '?';
            break;
        }
    }
    *out = '\0';
    return t;
}

#else

#define PUT_UTF8(x) t = s_cat (t, &size, ConvUTF8 (x))

#define GET_UTF8(in,y) \
 do { UDWORD vl = 0; int todo = 1; UDWORD org = *in++; if ((org & 0xc0) != 0xc0) { y = '!'; continue; } \
      while (org & 0x20) { todo += 1; org <<= 2; }; org &= 0x3f; for (vl = 1; vl < todo; vl++) org >>= 2; \
      vl = org; while (todo > 0) { org = *in++; \
      if ((org & 0xc0) != 0x80) { todo = -1; continue; } org &= 0x3f; \
      vl <<= 6; vl |= org; todo--; } if (todo == -1) y = '?'; else y = vl; } while (0)


const UDWORD koi8u_utf8[] = { /* 7bit are us-ascii */
    0x2500, 0x2502, 0x250c, 0x2510, 0x2514, 0x2518, 0x251c, 0x2524,
    0x252c, 0x2534, 0x253c, 0x2580, 0x2584, 0x2588, 0x258c, 0x2590,
    0x2591, 0x2592, 0x2593, 0x2320, 0x25a0, 0x2022, 0x221a, 0x2248,
    0x2264, 0x2265, 0x00a0, 0x2321, 0x00b0, 0x00b2, 0x00b7, 0x00f7,
    0x2550, 0x2551, 0x2552, 0x0451, 0x0454, 0x2554, 0x0456, 0x0457,
    0x2557, 0x2558, 0x2559, 0x255a, 0x255b, 0x0491, 0x255d, 0x255e,
    0x255f, 0x2560, 0x2561, 0x0401, 0x0403, 0x2563, 0x0406, 0x0407,
    0x2566, 0x2567, 0x2568, 0x2569, 0x256a, 0x0490, 0x256c, 0x00a9,
    0x044e, 0x0430, 0x0431, 0x0446, 0x0434, 0x0435, 0x0444, 0x0433,
    0x0445, 0x0438, 0x0439, 0x043a, 0x043b, 0x043c, 0x043d, 0x043e,
    0x043f, 0x044f, 0x0440, 0x0441, 0x0442, 0x0443, 0x0436, 0x0432,
    0x044c, 0x044b, 0x0437, 0x0448, 0x044d, 0x0449, 0x0447, 0x044a,
    0x042e, 0x0410, 0x0411, 0x0426, 0x0414, 0x0415, 0x0424, 0x0413,
    0x0425, 0x0418, 0x0419, 0x041a, 0x041b, 0x041c, 0x041d, 0x041e,
    0x041f, 0x042f, 0x0420, 0x0421, 0x0422, 0x0423, 0x0416, 0x0412,
    0x042c, 0x042b, 0x0417, 0x0428, 0x042d, 0x0429, 0x0427, 0x042a
};

const UDWORD win1251_utf8[] = { /* 7bit are us-ascii */
    0x0402, 0x0403, 0x201a, 0x0453, 0x201e, 0x2026, 0x2020, 0x2021,
    0x0088, 0x2030, 0x0409, 0x2039, 0x040a, 0x040c, 0x040b, 0x040f,
    0x0452, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
    0x0098, 0x2122, 0x0459, 0x203a, 0x045a, 0x045c, 0x045b, 0x045f,
    0x00a0, 0x040e, 0x045e, 0x0408, 0x00a4, 0x0490, 0x00a6, 0x00a7,
    0x0401, 0x00a9, 0x0404, 0x00ab, 0x00ac, 0x00ad, 0x00ae, 0x0407,
    0x00b0, 0x00b1, 0x0406, 0x0456, 0x0491, 0x00b5, 0x00b6, 0x00b7,
    0x0451, 0x2116, 0x0454, 0x00bb, 0x0458, 0x0405, 0x0455, 0x0457,
    0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417,
    0x0418, 0x0419, 0x041a, 0x041b, 0x041c, 0x041d, 0x041e, 0x041f,
    0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427,
    0x0428, 0x0429, 0x042a, 0x042b, 0x042c, 0x042d, 0x042e, 0x042f,
    0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437,
    0x0438, 0x0439, 0x043a, 0x043b, 0x043c, 0x043d, 0x043e, 0x043f,
    0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447,
    0x0448, 0x0449, 0x044a, 0x044b, 0x044c, 0x044d, 0x044e, 0x044f
};

const char *ConvToUTF8 (const char *inn, UBYTE enc)
{
    static char *t = NULL;
    static UDWORD size = 0;
    const unsigned char *in = inn;
    UDWORD i;
    unsigned char x, y;
    
    t = s_catf (t, &size, "%*s", strlen (in) * 3, "");
    *t = '\0';
    
    for (*t = '\0'; *in; in++)
    {
        if (~*in & 0x80)
        {
            t = s_catf (t, &size, "%c", *in);
            continue;
        }
        switch (enc & ~ENC_AUTO)
        {
            case ENC_UTF8:
                if (*in == 0xfe) /* we _do_ allow 0xFE here, it's the ICQ seperator character */
                {
                    t = s_catf (t, &size, "\xfe");
                    continue;
                }
                GET_UTF8 (in, i);
                in--;
                PUT_UTF8 (i);
                continue;
            case ENC_LATIN1:
                PUT_UTF8 (*in);
                continue;
            case ENC_LATIN9:
                switch (*in)
                {
                    case 0xa4: PUT_UTF8 (0x20ac); continue; /* EURO */
                    case 0xa6: PUT_UTF8 (0x0160); continue; /* SCARON */
                    case 0xa8: PUT_UTF8 (0x0161); continue; /* SMALL SCARON */
                    case 0xb4: PUT_UTF8 (0x017d); continue; /* ZCARON */
                    case 0xb8: PUT_UTF8 (0x017e); continue; /* SMALL ZCARON */
                    case 0xbc: PUT_UTF8 (0x0152); continue; /* OE */
                    case 0xbd: PUT_UTF8 (0x0153); continue; /* SMALL OE */
                    case 0xbe: PUT_UTF8 (0x0178); continue; /* Y DIAERESIS */
                    default:
                        PUT_UTF8 (*in);
                }
                continue;
            case ENC_KOI8:
                PUT_UTF8 (koi8u_utf8[*in & 0x7f]);
                continue;
            case ENC_WIN1251:
                PUT_UTF8 (win1251_utf8[*in & 0x7f]);
                continue;
            case ENC_EUC:
                /* FIXME: No, this is no real UTF-8. We just stuff EUC
                          into the private use area U+Fxxxx */
                PUT_UTF8 (0xf0000 | (*in << 8) | in[1]);
                in++;
                continue;
            case ENC_SJIS:
                x = *in++;
                y = *in;
                if ((x & 0xe0) == 0x80)
                {
                    if (y < 0x9f)
                    {
                        x = 2 * x - (x >= 0xe0 ? 0xe1 : 0x61);
                        y += 0x61 - (y >= 0x7f ?    1 :    0);
                    }
                    else
                    {
                        x = 2 * x - (x >= 0xe0 ? 0xe0 : 0x60);
                        y += 2;
                    }
                }
                PUT_UTF8 (0xf0000 | (x << 8) | y);
            default:
                t = s_cat (t, &size, "?");
        }
    }
    return t;
}

const char *ConvFromUTF8 (const char *inn, UBYTE enc)
{
    static char *t = NULL;
    static UDWORD size = 0;
    const unsigned char *in = inn;
    UDWORD val, i;
    unsigned char x, y;

    t = s_catf (t, &size, "%*s", strlen (in) * 3, "");
    *t = '\0';
    
    for (*t = '\0'; *in; in++)
    {
        if (~*in & 0x80)
        {
            t = s_catf (t, &size, "%c", *in);
            continue;
        }
        if (*in == 0xfe) /* we _do_ allow 0xFE here, it's the ICQ seperator character */
        {
            t = s_catf (t, &size, "\xfe");
            continue;
        }
        val = '?';
        GET_UTF8 (in,val);
        in--;
        if (val == '?')
        {
            t = s_catf (t, &size, "·");
            continue;
        }
        switch (enc & ~ENC_AUTO)
        {
            case ENC_UTF8:
                PUT_UTF8 (val);
                continue;
            case ENC_LATIN1:
                if (!(val & 0xffffff00))
                    t = s_catf (t, &size, "%c", val);
                else
                    t = s_catf (t, &size, "?");
                continue;
            case ENC_LATIN9:
                if (!(val & 0xffffff00))
                    t = s_catf (t, &size, "%c", val);
                else
                    switch (val)
                    {
                        case 0x20ac: t = s_catf (t, &size, "\xa4"); continue; /* EURO */
                        case 0x0160: t = s_catf (t, &size, "\xa6"); continue; /* SCARON */
                        case 0x0161: t = s_catf (t, &size, "\xa8"); continue; /* SMALL SCARON */
                        case 0x017d: t = s_catf (t, &size, "\xb4"); continue; /* ZCARON */
                        case 0x017e: t = s_catf (t, &size, "\xb8"); continue; /* SMALL ZCARON */
                        case 0x0152: t = s_catf (t, &size, "\xbc"); continue; /* OE */
                        case 0x0153: t = s_catf (t, &size, "\xbd"); continue; /* SMALL OE */
                        case 0x0178: t = s_catf (t, &size, "\xbe"); continue; /* Y DIAERESIS */
                        default:
                            t = s_catf (t, &size, "?");
                    }
                continue;
            case ENC_KOI8:
                for (i = 0; i < 128; i++)
                    if (koi8u_utf8[i] == val)
                    {
                        t = s_catf (t, &size, "%c", i + 128);
                        continue;
                    }
                t = s_catf (t, &size, "?");
                continue;
            case ENC_WIN1251:
                for (i = 0; i < 128; i++)
                    if (win1251_utf8[i] == val)
                    {
                        t = s_catf (t, &size, "%c", i + 128);
                        continue;
                    }
                t = s_catf (t, &size, "?");
                continue;
            case ENC_EUC:
                if ((val & 0xffff0000) != 0xf0000)
                {
                    t = s_catf (t, &size, "?");
                    continue;
                }
                t = s_catf (t, &size, "%c%c", (val & 0xff00) >> 8, val & 0xff);
                continue;
            case ENC_SJIS:
                if ((val & 0xffff0000) != 0xf0000)
                {
                    t = s_catf (t, &size, "?");
                    continue;
                }
                x = (val & 0xff00) >> 8;
                y = val & 0xff;
                if (x & 1)
                {
                    x = x / 2 + (x < 0xdf ? 0x31 : 0x71);
                    y -= 0x61 - (y < 0xe0 ?    0 :    1);
                }
                else
                {
                    x = x / 2 + (x < 0xdf ? 0x30 : 0x70);
                    y -= 2;
                }
                continue;
            default:
                t = s_cat (t, &size, "?");
        }
    }
    return t;
}

#endif /* ENABLE_ICONV */
#endif /* ENABLE_UTF8 */
