/*
 * This file handles character conversions.
 *
 * mICQ Copyright (C) © 2001,2002,2003 Rüdiger Kuhlmann
 *
 * mICQ is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 dated June, 1991.
 *
 * mICQ is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 *
 * $Id$
 */

#include "micq.h"
#if HAVE_STRING_H
#include <string.h>
#endif
#if HAVE_ERRNO_H
#include <errno.h>
#endif
#if HAVE_CTYPE_H
#include <ctype.h>
#endif
#include "conv.h"
#include "preferences.h"
#include "util_str.h"

typedef strc_t (iconv_func)(strc_t, UBYTE);

#if HAVE_ICONV
#include <iconv.h>
static strc_t iconv_from_iconv (strc_t, UBYTE);
static strc_t iconv_to_iconv (strc_t, UBYTE);
#endif
#if ENABLE_FALLBACK_UTF8
static iconv_func iconv_from_usascii, iconv_to_usascii;
#else
static iconv_func iconv_usascii;
#endif
#if ENABLE_FALLBACK_UTF8
static iconv_func iconv_utf8;
#endif
#if ENABLE_FALLBACK_LATIN1
static iconv_func iconv_from_latin1, iconv_to_latin1;
#endif
#if ENABLE_FALLBACK_LATIN9
static iconv_func iconv_from_latin9, iconv_to_latin9;
#endif
#if ENABLE_FALLBACK_KOI8
static iconv_func iconv_from_koi8, iconv_to_koi8;
#endif
#if ENABLE_FALLBACK_WIN1251
static iconv_func iconv_from_win1251, iconv_to_win1251;
#endif
#if ENABLE_FALLBACK_UCS2BE
static iconv_func iconv_from_ucs2be, iconv_to_ucs2be;
#endif


#if HAVE_ICONV
typedef struct { const char *enca; const char *encb; const char *encc;
                 iconv_func *fof; iconv_func *fto;
                 iconv_t     iof; iconv_t      ito; } enc_t;
#else
typedef struct { const char *enca; const char *encb; const char *encc;
                 iconv_func *fof; iconv_func *fto; } enc_t;
#endif

static int conv_nr = 0;
static enc_t *conv_encs = NULL;

UBYTE conv_error = 0;

/*
 * Initialize encoding table.
 */
void ConvInit (void)
{
    int i;

    conv_error = 0;
    conv_encs = calloc (sizeof (enc_t), conv_nr = 15);
    conv_encs[0].enca = "US-ASCII";
    conv_encs[1].enca = "UTF-8";
    conv_encs[2].enca = "ISO-8859-1";
    conv_encs[2].encb = "ISO8859-1";
    conv_encs[2].encc = "LATIN1";
    conv_encs[3].enca = "ISO-8859-15";
    conv_encs[3].encb = "ISO8859-15";
    conv_encs[3].encc = "LATIN9";
    conv_encs[4].enca = "KOI8-U";
    conv_encs[4].encb = "KOI8-R";
    conv_encs[4].encc = "KOI8";
    conv_encs[5].enca = "CP1251";
    conv_encs[5].encb = "WINDOWS-1251";
    conv_encs[5].encc = "CP-1251";
    conv_encs[6].enca = "UCS2BE";
    conv_encs[6].encb = "UNICODEBIG";
    conv_encs[7].enca = "CP1257";
    conv_encs[7].encb = "WINDOWS-1257";
    conv_encs[7].encc = "CP-1257";
    conv_encs[8].enca = "EUC-JP";
    conv_encs[9].enca = "SHIFT-JIS";
    conv_encs[9].encb = "SJIS";
    
#if HAVE_ICONV
    for (i = 0; i < 10; i++)
        ConvEnc (conv_encs[i].enca);
    /* extra check for UTF-8 */
    if (conv_encs[1].fof)
    {
        size_t inl = 2, outl = 10;
        char inb[10], outb[10], *inp = inb, *outp = outb;
        strcpy (inp, "\xfc.\xc0\xaf");
        if (iconv (conv_encs[1].ito, &inp, &inl, &outp, &outl) != (size_t)-1)
            conv_encs[1].fto = conv_encs[1].fof = NULL;
        else
        {
            inp = inb + 2;
            iconv (conv_encs[1].ito, NULL, NULL, NULL, NULL);
            if ((iconv (conv_encs[1].ito, &inp, &inl, &outp, &outl) != (size_t)-1) && *outp != '/')
                conv_encs[1].fto = conv_encs[1].fof = NULL;
        }
    }
#endif
    if (!conv_encs[0].fof)
    {
#if ENABLE_FALLBACK_ASCII
        conv_encs[0].fof  = &iconv_from_usascii;
        conv_encs[0].fto  = &iconv_to_usascii;
#else
        conv_encs[0].fof  = &iconv_usascii;
        conv_encs[0].fto  = &iconv_usascii;
#endif
    }
    if (!conv_encs[1].fof)
    {
#if ENABLE_FALLBACK_UTF8
        conv_encs[1].fof  = &iconv_utf8;
        conv_encs[1].fto  = &iconv_utf8;
#else
        conv_encs[1].fof  = conv_encs[0].fof;
        conv_encs[1].fto  = conv_encs[0].fto;
        conv_error = 1;
#endif
    }
    if (!conv_encs[2].fof)
    {
#if ENABLE_FALLBACK_LATIN1
        conv_encs[2].fof  = &iconv_from_latin1;
        conv_encs[2].fto  = &iconv_to_latin1;
#else
        conv_encs[2].fof  = conv_encs[0].fof;
        conv_encs[2].fto  = conv_encs[0].fto;
        conv_error = 2;
#endif
    }
    if (!conv_encs[3].fof)
    {
#if ENABLE_FALLBACK_LATIN9
        conv_encs[3].fof  = &iconv_from_latin9;
        conv_encs[3].fto  = &iconv_to_latin9;
#else
        conv_encs[3].fof  = conv_encs[0].fof;
        conv_encs[3].fto  = conv_encs[0].fto;
        conv_error = 3;
#endif
    }
    if (!conv_encs[4].fof)
    {
#if ENABLE_FALLBACK_KOI8
        conv_encs[4].fof  = &iconv_from_koi8;
        conv_encs[4].fto  = &iconv_to_koi8;
#else
        conv_encs[4].fof  = conv_encs[0].fof;
        conv_encs[4].fto  = conv_encs[0].fto;
        conv_error = 4;
#endif
    }
    if (!conv_encs[5].fof)
    {
#if ENABLE_FALLBACK_WIN1251
        conv_encs[5].fof  = &iconv_from_win1251;
        conv_encs[5].fto  = &iconv_to_win1251;
#else
        conv_encs[5].fof  = conv_encs[0].fof;
        conv_encs[5].fto  = conv_encs[0].fto;
        conv_error = 5;
#endif
    }
    if (!conv_encs[6].fof)
    {
#if ENABLE_FALLBACK_UCS2BE
        conv_encs[6].fof  = &iconv_from_ucs2be;
        conv_encs[6].fto  = &iconv_to_ucs2be;
#else
        conv_encs[6].fof  = conv_encs[0].fof;
        conv_encs[6].fto  = conv_encs[0].fto;
        conv_error = 6;
#endif
    }
}

/*
 * Give an ID for the given encoding name.
 */
UBYTE ConvEnc (const char *enc)
{
    UBYTE nr;

    for (nr = 0; conv_encs[nr].enca; nr++)
        if (!strcasecmp (conv_encs[nr].enca, enc) ||
            (conv_encs[nr].encb && !strcasecmp (conv_encs[nr].encb, enc)) ||
            (conv_encs[nr].encc && !strcasecmp (conv_encs[nr].encc, enc)))
        {
#if HAVE_ICONV
            if (conv_encs[nr].ito && conv_encs[nr].iof)
            {
                if (conv_encs[nr].ito != (iconv_t)(-1) && conv_encs[nr].iof != (iconv_t)(-1))
                    return nr;
                return ENC_AUTO | nr;
            }
#endif
            if (conv_encs[nr].fof && conv_encs[nr].fto)
                return nr;
            break;
        }
    
    if (nr == conv_nr - 1)
    {
        enc_t *new = realloc (conv_encs, sizeof (enc_t) * (conv_nr + 10));
        if (!new)
            return 0;
        conv_nr += 10;
        conv_encs = new;
    }
    if (!conv_encs[nr].enca)
    {
        char *p;
        for (conv_encs[nr].enca = p = strdup (enc); *p; p++)
            *p = toupper (*p);
        conv_encs[nr + 1].enca = NULL;
    }
#if HAVE_ICONV
    conv_encs[nr].iof = iconv_open ("UTF-8", conv_encs[nr].enca);
    conv_encs[nr].ito = iconv_open (conv_encs[nr].enca, "UTF-8");
    if ((conv_encs[nr].ito == (iconv_t)(-1) || conv_encs[nr].iof == (iconv_t)(-1))
        && conv_encs[nr].encb)
    {
        conv_encs[nr].iof = iconv_open ("UTF-8", conv_encs[nr].encb);
        conv_encs[nr].ito = iconv_open (conv_encs[nr].encb, "UTF-8");
    }
    if ((conv_encs[nr].ito == (iconv_t)(-1) || conv_encs[nr].iof == (iconv_t)(-1))
        && conv_encs[nr].encc)
    {
        conv_encs[nr].iof = iconv_open ("UTF-8", conv_encs[nr].encc);
        conv_encs[nr].ito = iconv_open (conv_encs[nr].encc, "UTF-8");
    }
    if (conv_encs[nr].ito != (iconv_t)(-1) && conv_encs[nr].iof != (iconv_t)(-1))
    {
        conv_encs[nr].fof = &iconv_from_iconv;
        conv_encs[nr].fto = &iconv_to_iconv;
        return nr;
    }
#endif
    conv_error = nr;
    return ENC_AUTO | nr;
}

/*
 * Give the encoding name for a given ID
 */
const char *ConvEncName (UBYTE enc)
{
    return conv_encs[enc & ~ENC_AUTO].enca;
}

const char *ConvCrush0xFE (const char *in)
{
    static str_s t;
    char *p;
    
    if (!in || !*in)
        return "";
    
    s_init (&t, in, 0);

    for (p = t.txt; *p; p++)
        if (*p == Conv0xFE)
            *p = '*';
    return t.txt;
}

/*
 * Convert a single unicode code point to UTF-8
 */
const char *ConvUTF8 (UDWORD ucs)
{
    static char b[5];
    
    if      (!(ucs & 0xffffff80))
    {
        b[0] = ucs;
        b[1] = '\0';
    }
    else if (!(ucs & 0xfffff800))
    {
        b[0] = 0xc0 |  (ucs               >>  6);
        b[1] = 0x80 |  (ucs &       0x3f);
        b[2] = '\0';
    }
    else if (!(ucs & 0xffff0000))
    {
        b[0] = 0xe0 | ( ucs               >> 12);
        b[1] = 0x80 | ((ucs &      0xfc0) >>  6);
        b[2] = 0x80 |  (ucs &       0x3f);
        b[3] = '\0';
    }
    else if (!(ucs & 0xffe00000))
    {
        b[0] = 0xf0 | ( ucs               >> 18);
        b[1] = 0x80 | ((ucs &    0x3f000) >> 12);
        b[2] = 0x80 | ((ucs &      0xfc0) >>  6);
        b[3] = 0x80 |  (ucs &       0x3f);
        b[4] = '\0';
    }
    else
    {
        b[0] = CHAR_BR;
        b[1] = '\0';
    }
    return b;
}

strc_t ConvFrom (strc_t text, UBYTE enc)
{
    enc &= ~ENC_AUTO;
    if ((enc >= conv_nr) || (!conv_encs[enc].fof))
    {
        conv_error = enc;
        enc = ENC_ASCII;
    }
    return conv_encs[enc].fof (text, enc);
}

strc_t ConvFromSplit (strc_t text, UBYTE enc)
{
    static str_s str;
    str_s tstr = { (char *)text->txt, text->len, 0 };
    const char *p;
    size_t tlen = text->len;
    
    s_init (&str, "", 100);
    while ((p = memchr (tstr.txt, '\xfe', tlen)))
    {
        tstr.len = p - tstr.txt;
        s_cat (&str, ConvFrom (&tstr, enc)->txt);
        s_catc (&str, '\xfe');
        tstr.txt += tstr.len + 1;
        tlen     -= tstr.len + 1;
    }
    s_cat (&str, ConvFrom (&tstr, enc)->txt);
    return &str;
}

strc_t ConvTo (const char *ctext, UBYTE enc)
{
    str_s text = { (char *)ctext, strlen (ctext), 0 };
    enc &= ~ENC_AUTO;
    if ((enc >= conv_nr) || (!conv_encs[enc].fto))
    {
        conv_error = enc;
        enc = ENC_ASCII;
    }
    return conv_encs[enc].fto (&text, enc);
}

BOOL ConvFits (const char *in, UBYTE enc)
{
    char *inn, *p;
    int i;
    
    inn = strdup (in);
    if (!inn)
        return 0;
    for (p = inn; *p; p++)
        if (*p == Conv0xFE || *p == CHAR_NA || *p == CHAR_BR)
            *p = ' ';
    i = strpbrk (ConvTo (inn, enc)->txt, "?*") ? 0 : 1;
    free (inn);
    return i;
}

#if HAVE_ICONV
static strc_t iconv_from_iconv (strc_t text, UBYTE enc)
{
    static str_s str;

    size_t inleft, outleft;
    char *out;
    ICONV_CONST char *in;

    s_init (&str, "", 100);
    out = str.txt;
    outleft = str.max - 2;
    in = (ICONV_CONST char *) text->txt;
    inleft = text->len;

    iconv (conv_encs[enc].iof, NULL, NULL, NULL, NULL);
    while (iconv (conv_encs[enc].iof, &in, &inleft, &out, &outleft) == (size_t)(-1))
    {
        UDWORD rc = errno;
        str.len = out - str.txt;

        if (outleft < 10 || rc == E2BIG)
            s_blow (&str, 50 + inleft);
        else
        {
            s_catc (&str, CHAR_NA);
            in++;
            inleft--;
        }
        out = str.txt + str.len;
        outleft = str.max - str.len - 2;
    }
    *out = '\0';
    return &str;
}

static strc_t iconv_to_iconv (strc_t text, UBYTE enc)
{
    static str_s str;

    size_t inleft, outleft;
    char *out;
    ICONV_CONST char *in;

    s_init (&str, "", 100 + text->len);
    out = str.txt;
    outleft = str.max - 2;
    in = (ICONV_CONST char *) text->txt;
    inleft = text->len;

    iconv (conv_encs[enc].ito, NULL, NULL, NULL, NULL);
    while (iconv (conv_encs[enc].ito, &in, &inleft, &out, &outleft) == (size_t)(-1))
    {
        UDWORD rc = errno;
        str.len = out - str.txt;

        if (outleft < 10 || rc == E2BIG)
            s_blow (&str, 50 + inleft);
        else
        {
            s_catc (&str, CHAR_NA);
            in++;
            inleft--;
        }
        out = str.txt + str.len;
        outleft = str.max - str.len - 2;
    }
    *out = '\0';
    return &str;
}
#endif

#if !ENABLE_FALLBACK_ASCII
strc_t iconv_usascii (strc_t in, UBYTE enc)
{
    static str_s str;
    int off;
    char c;
    s_init (&str, "", in->len);
    
    for (off = 0; off < in->len; off++)
        if ((c = in->txt[off]) & 0x80)
            str.txt[off] = CHAR_BR;
        else
            str.txt[off] = c;
    str.txt[off] = '\0';
    str.len = in->len;
    return &str;
}
#endif

#if ENABLE_FALLBACK_ASCII || ENABLE_FALLBACK_UCS2BE || ENABLE_FALLBACK_WIN1251 || ENABLE_FALLBACK_KOI8 \
  || ENABLE_FALLBACK_LATIN9 || ENABLE_FALLBACK_LATIN1 || ENABLE_FALLBACK_UTF8

static UDWORD iconv_get_utf8 (str_t in, int *off)
{
     UDWORD ucs = 0;
     int i, continuations = 1;
     UBYTE  c = in->txt[*(off++)];

     if (~c & 0x80)
         return c;

     if (~c & 0x40)
         return CHAR_BR;

     while (c & 0x20)
     {
         continuations++;
         c <<= 1;
     }

     c &= 0x3f;
     c >>= continuations;

     for (i = 0, ucs = c; i < continuations; i++)
     {
         if (((c = in->txt[*off + i]) & 0xc0) != 0x80)
             return CHAR_BR;

         c &= 0x3f;
         ucs <<= 6;
         ucs |= c;
     }
     *off += continuations;
     return ucs;
}

#if ENABLE_FALLBACK_USASCII
static strc_t iconv_from_usascii (strc_t in, UBYTE enc)
{
    static str_s str;
    int off;
    char c;
    s_init (&str, "", in->len);
    
    for (off = 0; off < in->len; off++)
        if ((c = in->txt[off]) & 0x80)
            s_catc (&str, CHAR_BR);
        else
            s_catc (&str, c);
    return &str;
}

static strc_t iconv_to_usascii (strc_t in, UBYTE enc)
{
    static str_s str;
    UDWORD ucs;
    int off;
    s_init (&str, "", in->len);
    
    for (off = 0; off < in->len; )
    {
        ucs = iconv_get_utf8 (&str, &off);
        s_catc (&str, ucs & 0x80 ? CHAR_NA : ucs);
    }
    return &str;
}
#endif

#if ENABLE_FALLBACK_UTF8
strc_t iconv_utf8 (strc_t in, UBYTE enc)
{
    static str_s str;
    UDWORD ucs;
    int off;
    s_init (&str, "", in.len);
    
    for (off = 0; off < in.len; )
    {
        ucs = iconv_get_utf8 (&str, &off);
        s_cat (&str, ConvUTF8 (ucs);
    }
    return &str;
}
#endif

#if ENABLE_FALLBACK_LATIN1
static strc_t iconv_from_latin1 (strc_t in, UBYTE enc)
{
    static str_s str;
    int off;
    
    for (off = 0; off < in.len; off++)
        s_cat (&str, ConvUTF8 (in.txt[off]));
}

static strc_t iconv_to_latin1 (strc_t in, UBYTE enc)
{
    static str_s str;
    UDWORD ucs;
    int off;
    s_init (&str, "", in.len);
    
    for (off = 0; off < in.len; )
    {
        ucs = iconv_get_utf8 (&str, &off);
        s_catc (&str, ucs & 0xffffff00 ? CHAR_NA : ucs);
    }
    return &str;
}
#endif

#if ENABLE_FALLBACK_LATIN9
static strc_t iconv_from_latin1 (strc_t in, UBYTE enc)
{
    static str_s str;
    UDWORD ucs;
    UBYTE c;
    int off;
    
    for (off = 0; off < in.len; off++)
    {
        c = in.txt[off];
        switch (c)
        {
            case 0xa4: ucs = 0x20ac; /* EURO */
            case 0xa6: ucs = 0x0160; /* SCARON */
            case 0xa8: ucs = 0x0161; /* SMALL SCARON */
            case 0xb4: ucs = 0x017d; /* ZCARON */
            case 0xb8: ucs = 0x017e; /* SMALL ZCARON */
            case 0xbc: ucs = 0x0152; /* OE */
            case 0xbd: ucs = 0x0153; /* SMALL OE */
            case 0xbe: ucs = 0x0178; /* Y DIAERESIS */
            default:   ucs = c;
        }
        s_cat (&str, ConvUTF8 (ucs));
    }
}

static strc_t iconv_to_latin1 (strc_t in, UBYTE enc)
{
    static str_s str;
    UDWORD ucs;
    int off;
    s_init (&str, "", in.len);
    
    for (off = 0; off < in.len; )
    {
        ucs = iconv_get_utf8 (&str, &off);
        if (!(ucs & 0xffffff00))
        {
            switch (ucs)
            {
                case 0xa4: case 0xa6: case 0xa8: case 0xb4:
                case 0xb8: case 0xbc: case 0xbd: case 0xbe:
                    ucs = CHAR_NA;
            }
            s_catc (&str, ucs);
        }
        else
        {
            switch (ucs)
            {
                case 0x20ac: s_catc (&str, '\xa4'); /* EURO */
                case 0x0160: s_catc (&str, '\xa6'); /* SCARON */
                case 0x0161: s_catc (&str, '\xa8'); /* SMALL SCARON */
                case 0x017d: s_catc (&str, '\xb4'); /* ZCARON */
                case 0x017e: s_catc (&str, '\xb8'); /* SMALL ZCARON */
                case 0x0152: s_catc (&str, '\xbc'); /* OE */
                case 0x0153: s_catc (&str, '\xbd'); /* SMALL OE */
                case 0x0178: s_catc (&str, '\xbe'); /* Y DIAERESIS */
                default:     s_catc (&str, CHAR_NA);
            }
        }
    }
    return &str;
}
#endif

#if ENABLE_FALLBACK_KOI8
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
    0x042c, 0x042b, 0x0417, 0x0428, 0x042d, 0x0429, 0x0427, 0x042a,
    0x0
};

static strc_t iconv_from_koi8 (strc_t in, UBYTE enc)
{
    static str_s str;
    UBYTE c;
    int off;
    
    for (off = 0; off < in.len; off++)
        s_cat (&str, ConvUTF8 ((c = in.txt[off]) & 0x80 ? koi8_utf8[c] : c));
}

static strc_t iconv_to_koi8 (strc_t in, UBYTE enc)
{
    static str_s str;
    UDWORD ucs;
    UBYTE c;
    int off;
    s_init (&str, "", in.len);
    
    for (off = 0; off < in.len; )
    {
        ucs = iconv_get_utf8 (&str, &off);
        if (ucs & 0xffffff00)
        {
            for (c = 0; ~c & 0x80; c++)
                if (koi8_utf8[c] == ucs)
                    break;
            
            s_catc (&str, c & 0x80 ? CHAR_NA : c);
        }
        else
            s_catc (&str, c);
}
#endif

#if ENABLE_FALLBACK_WIN1251
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
    0x0448, 0x0449, 0x044a, 0x044b, 0x044c, 0x044d, 0x044e, 0x044f,
    0x0
};

static strc_t iconv_from_win1251 (strc_t in, UBYTE enc)
{
    static str_s str;
    UBYTE c;
    int off;
    
    for (off = 0; off < in.len; off++)
        s_cat (&str, ConvUTF8 ((c = in.txt[off]) & 0x80 ? win1251_utf8[c] : c));
}

static strc_t iconv_to_win1251 (strc_t in, UBYTE enc)
{
    static str_s str;
    UDWORD ucs;
    UBYTE c;
    int off;
    s_init (&str, "", in.len);
    
    for (off = 0; off < in.len; )
    {
        ucs = iconv_get_utf8 (&str, &off);
        if (ucs & 0xffffff00)
        {
            for (c = 0; ~c & 0x80; c++)
                if (win1251_utf8[c] == ucs)
                    break;
            
            s_catc (&str, c & 0x80 ? CHAR_NA : c);
        }
        else
            s_catc (&str, c);
}
#endif

#if ENABLE_FALLBACK_UCS2BE
static strc_t iconv_from_ucs2be (strc_t in, UBYTE enc)
{
    static str_s str;
    UDWORD ucs, ucs2;
    UBYTE c;
    int off;
    s_init (&str, "", in.len);
    
    for (off = 0; off < in.len; )
    {
        if (off + 1 == in.len)
        {
            s_catc (&str, CHAR_BR);
            break;
        }

        ucs = in->txt[off++] << 8;
        ucs |= in->txt[off++];
        if ((ucs & 0xfc00) == 0xd800)
        {
            if (off + 3 >= in.len)
            {
                s_catc (&str, CHAR_BR);
                break;
            }
            ucs &= 0x4ff;
            ucs <<= 2;
            if (((c = in->txt[off++]) & 0xfc) != 0xc)
            {
                s_catc (&str, CHAR_BR);
                off++;
                continue;
            }
            ucs |= 0x3 & c;
            ucs <<= 8;
            ucs = in->txt[off++];
        }
        s_cat (&str, ConvUTF8 (ucs));
    }
}

static strc_t iconv_to_ucs2be (strc_t in, UBYTE enc)
{
    static str_s str;
    UDWORD ucs;
    int off;
    s_init (&str, "", in.len);
    
    for (off = 0; off < in.len; )
    {
        ucs = iconv_get_utf8 (&str, &off);
        if (ucs & 0xffff || (val & 0xf800) == 0xd800)
        {
            s_catc (&str, 0xd8 | ((ucs >> 18) & 0x3)));
            s_catc (&str, (ucs >> 10) & 0xff);
            s_catc (&str, 0xdc | ((ucs >> 8) & 0x3));
            s_catc (&str, ucs & 0xff);
        }
        else
        {
            s_catc (&str, (ucs >> 8) & 0xff);
            s_catc (&str, ucs & 0xff);
        }
        s_catc (&str, 0);
        s_catc (&str, 0);
}
#endif
#endif /* ENABLE_FALLBACK_* */

