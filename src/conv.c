/*
 * This file handles character conversions.
 *
 * mICQ Copyright (C) © 2001-2005 Rüdiger Kuhlmann
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
 * In addition, as a special exception permission is granted to link the
 * code of this release of mICQ with the OpenSSL project's "OpenSSL"
 * library, and distribute the linked executables.  You must obey the GNU
 * General Public License in all respects for all of the code used other
 * than "OpenSSL".  If you modify this file, you may extend this exception
 * to your version of the file, but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your version
 * of this file.
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
#if HAVE_ERRNO_H
#include <errno.h>
#endif
#if HAVE_CTYPE_H
#include <ctype.h>
#endif
#include "conv.h"
#include "preferences.h"
#if !HAVE_WCWIDTH
#undef ENABLE_FALLBACK_WCHART
#endif

typedef strc_t (iconv_func)(strc_t, UBYTE);

#if HAVE_ICONV
#include <iconv.h>
static strc_t iconv_from_iconv (strc_t, UBYTE);
static strc_t iconv_to_iconv (strc_t, UBYTE);
#endif
static iconv_func iconv_from_usascii, iconv_to_usascii;
#if ENABLE_FALLBACK_UTF8
static iconv_func iconv_from_utf8, iconv_to_utf8;
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
#if ENABLE_FALLBACK_WCHART
static iconv_func iconv_from_wchart, iconv_to_wchart;
#endif
typedef struct { const char *enca; const char *encb; const char *encc; const char *encd;
#if HAVE_ICONV
                 iconv_t     iof; iconv_t      ito;
#endif
                 iconv_func *fof; iconv_func *fto; } enc_t;

static int conv_nr = 0;
static enc_t *conv_encs = NULL;
static const char *Utf8Name = "UTF-8";

UBYTE conv_error = 0;

#if HAVE_ICONV
/*
 * Check whether iconv() can handle it.
 */
static BOOL iconv_check (UBYTE enc)
{
#ifdef ENABLE_TRANSLIT
    conv_encs[enc].ito = iconv_open (s_sprintf ("%s//TRANSLIT", conv_encs[enc].enca), Utf8Name);
    if (conv_encs[enc].ito == (iconv_t)-1)
#endif
        conv_encs[enc].ito = iconv_open (conv_encs[enc].enca, Utf8Name);
    conv_encs[enc].iof = iconv_open (Utf8Name, conv_encs[enc].enca);
    if ((conv_encs[enc].ito == (iconv_t)-1 || conv_encs[enc].iof == (iconv_t)-1)
        && conv_encs[enc].encb)
    {
#ifdef ENABLE_TRANSLIT
        conv_encs[enc].ito = iconv_open (s_sprintf ("%s//TRANSLIT", conv_encs[enc].encb), Utf8Name);
        if (conv_encs[enc].ito == (iconv_t)-1)
#endif
            conv_encs[enc].ito = iconv_open (conv_encs[enc].encb, Utf8Name);
        conv_encs[enc].iof = iconv_open (Utf8Name, conv_encs[enc].encb);
    }
    if ((conv_encs[enc].ito == (iconv_t)-1 || conv_encs[enc].iof == (iconv_t)-1)
        && conv_encs[enc].encc)
    {
#ifdef ENABLE_TRANSLIT
        conv_encs[enc].ito = iconv_open (s_sprintf ("%s//TRANSLIT", conv_encs[enc].encc), Utf8Name);
        if (conv_encs[enc].ito == (iconv_t)-1)
#endif
            conv_encs[enc].ito = iconv_open (conv_encs[enc].encc, Utf8Name);
        conv_encs[enc].iof = iconv_open (Utf8Name, conv_encs[enc].encc);
    }
    if ((conv_encs[enc].ito == (iconv_t)-1 || conv_encs[enc].iof == (iconv_t)-1)
        && conv_encs[enc].encd)
    {
#ifdef ENABLE_TRANSLIT
        conv_encs[enc].ito = iconv_open (s_sprintf ("%s//TRANSLIT", conv_encs[enc].encd), Utf8Name);
        if (conv_encs[enc].ito == (iconv_t)-1)
#endif
            conv_encs[enc].ito = iconv_open (conv_encs[enc].encd, Utf8Name);
        conv_encs[enc].iof = iconv_open (Utf8Name, conv_encs[enc].encd);
    }
    if (enc == ENC_LATIN1 && conv_encs[enc].ito == (iconv_t)-1)
    {
        conv_encs[enc].ito = iconv_open (conv_encs[enc].encc, "utf8");
        conv_encs[enc].iof = iconv_open ("utf8", conv_encs[enc].encc);
        if (conv_encs[enc].ito != (iconv_t)-1 && conv_encs[enc].iof != (iconv_t)-1)
            Utf8Name = "utf8";
    }
    if (conv_encs[enc].ito != (iconv_t)-1 && conv_encs[enc].iof != (iconv_t)-1)
    {
        conv_encs[enc].fof = &iconv_from_iconv;
        conv_encs[enc].fto = &iconv_to_iconv;
        return TRUE;
    }
    return FALSE;
}
#endif

/*
 * Initialize encoding table.
 */
void ConvInit (void)
{
    conv_error = 0;
    conv_encs = calloc (sizeof (enc_t), conv_nr = 15);
    conv_encs[ENC_ASCII].enca = "US-ASCII";
    conv_encs[ENC_ASCII].encb = "USASCII";
    conv_encs[ENC_ASCII].encc = "ANSI_X3.4-1968";
    conv_encs[ENC_UTF8].enca = "UTF-8";
    conv_encs[ENC_LATIN1].enca = "ISO-8859-1";
    conv_encs[ENC_LATIN1].encb = "ISO8859-1";
    conv_encs[ENC_LATIN1].encc = "iso8859_1"; /* don't re-sort */
    conv_encs[ENC_LATIN1].encd = "LATIN1";
    conv_encs[ENC_LATIN9].enca = "ISO-8859-15";
    conv_encs[ENC_LATIN9].encb = "ISO8859-15";
    conv_encs[ENC_LATIN9].encc = "iso8859_15";
    conv_encs[ENC_LATIN9].encd = "LATIN9";
    conv_encs[ENC_KOI8].enca = "KOI8-U";
    conv_encs[ENC_KOI8].encb = "KOI8-R";
    conv_encs[ENC_KOI8].encc = "KOI8";
    conv_encs[ENC_WIN1251].enca = "CP1251";
    conv_encs[ENC_WIN1251].encb = "WINDOWS-1251";
    conv_encs[ENC_WIN1251].encc = "CP-1251";
    conv_encs[ENC_UCS2BE].enca = "UCS-2BE";
    conv_encs[ENC_UCS2BE].encb = "UNICODEBIG";
    conv_encs[ENC_WIN1257].enca = "CP1257";
    conv_encs[ENC_WIN1257].encb = "WINDOWS-1257";
    conv_encs[ENC_WIN1257].encc = "CP-1257";
    conv_encs[ENC_EUC].enca = "EUC-JP";
    conv_encs[ENC_SJIS].enca = "SHIFT-JIS";
    conv_encs[ENC_SJIS].encb = "SJIS";
    conv_encs[ENC_WCHART].enca = "WCHAR_T";
    
#if HAVE_ICONV
    /* extra check for UTF-8 */
    ConvEnc (conv_encs[ENC_UTF8].enca);
    if (conv_encs[ENC_UTF8].fof)
    {
        size_t inl = 2, outl = 10;
        char inb[10], outb[10], *outp = outb;
        ICONV_CONST char *inp = inb;
        strcpy (inp, "\xfc.\xc0\xaf");
        if (iconv (conv_encs[ENC_UTF8].ito, &inp, &inl, &outp, &outl) != (size_t)-1)
            conv_encs[ENC_UTF8].fto = conv_encs[ENC_UTF8].fof = NULL;
        else
        {
            inp = inb + 2;
            iconv (conv_encs[ENC_UTF8].ito, NULL, NULL, NULL, NULL);
            if ((iconv (conv_encs[ENC_UTF8].ito, &inp, &inl, &outp, &outl) != (size_t)-1) && *outp != '/')
                conv_encs[ENC_UTF8].fto = conv_encs[ENC_UTF8].fof = NULL;
        }
    }
#endif
    if (!conv_encs[ENC_ASCII].fof)
    {
        conv_encs[ENC_ASCII].fof  = &iconv_from_usascii;
        conv_encs[ENC_ASCII].fto  = &iconv_to_usascii;
    }
    if (!conv_encs[ENC_UTF8].fof)
    {
#if ENABLE_FALLBACK_UTF8
        conv_encs[ENC_UTF8].fof  = &iconv_from_utf8;
        conv_encs[ENC_UTF8].fto  = &iconv_to_utf8;
#else
        conv_encs[ENC_UTF8].fof  = conv_encs[ENC_ASCII].fof;
        conv_encs[ENC_UTF8].fto  = conv_encs[ENC_ASCII].fto;
#endif
    }
    if (!conv_encs[ENC_LATIN1].fof)
    {
#if ENABLE_FALLBACK_LATIN1
        conv_encs[ENC_LATIN1].fof  = &iconv_from_latin1;
        conv_encs[ENC_LATIN1].fto  = &iconv_to_latin1;
#else
        conv_encs[ENC_LATIN1].fof  = conv_encs[ENC_ASCII].fof;
        conv_encs[ENC_LATIN1].fto  = conv_encs[ENC_ASCII].fto;
#endif
    }
    if (!conv_encs[ENC_LATIN9].fof)
    {
#if ENABLE_FALLBACK_LATIN9
        conv_encs[ENC_LATIN9].fof  = &iconv_from_latin9;
        conv_encs[ENC_LATIN9].fto  = &iconv_to_latin9;
#else
        conv_encs[ENC_LATIN9].fof  = conv_encs[ENC_ASCII].fof;
        conv_encs[ENC_LATIN9].fto  = conv_encs[ENC_ASCII].fto;
#endif
    }
    if (!conv_encs[ENC_KOI8].fof)
    {
#if ENABLE_FALLBACK_KOI8
        conv_encs[ENC_KOI8].fof  = &iconv_from_koi8;
        conv_encs[ENC_KOI8].fto  = &iconv_to_koi8;
#else
        conv_encs[ENC_KOI8].fof  = conv_encs[ENC_ASCII].fof;
        conv_encs[ENC_KOI8].fto  = conv_encs[ENC_ASCII].fto;
#endif
    }
    if (!conv_encs[ENC_WIN1251].fof)
    {
#if ENABLE_FALLBACK_WIN1251
        conv_encs[ENC_WIN1251].fof  = &iconv_from_win1251;
        conv_encs[ENC_WIN1251].fto  = &iconv_to_win1251;
#else
        conv_encs[ENC_WIN1251].fof  = conv_encs[ENC_ASCII].fof;
        conv_encs[ENC_WIN1251].fto  = conv_encs[ENC_ASCII].fto;
#endif
    }
    if (!conv_encs[ENC_UCS2BE].fof)
    {
#if ENABLE_FALLBACK_UCS2BE
        conv_encs[ENC_UCS2BE].fof  = &iconv_from_ucs2be;
        conv_encs[ENC_UCS2BE].fto  = &iconv_to_ucs2be;
#else
        conv_encs[ENC_UCS2BE].fof  = conv_encs[ENC_ASCII].fof;
        conv_encs[ENC_UCS2BE].fto  = conv_encs[ENC_ASCII].fto;
#endif
    }
    if (!conv_encs[ENC_WCHART].fof)
    {
#if ENABLE_FALLBACK_WCHART
        conv_encs[ENC_WCHART].fof  = &iconv_from_wchart;
        conv_encs[ENC_WCHART].fto  = &iconv_to_wchart;
#else
        conv_encs[ENC_WCHART].fof  = conv_encs[ENC_ASCII].fof;
        conv_encs[ENC_WCHART].fto  = conv_encs[ENC_ASCII].fto;
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
            (conv_encs[nr].encc && !strcasecmp (conv_encs[nr].encc, enc)) ||
            (conv_encs[nr].encd && !strcasecmp (conv_encs[nr].encd, enc)))
        {
#if HAVE_ICONV
            if (!conv_encs[nr].ito || !conv_encs[nr].iof)
                iconv_check (nr);
            if (conv_encs[nr].ito != (iconv_t)(-1) && conv_encs[nr].iof != (iconv_t)(-1))
                return nr;
            return ENC_FAUTO | nr;
#endif
            if (conv_encs[nr].fof && conv_encs[nr].fto)
                return nr;
            break;
        }
    
    if (nr == conv_nr - 1)
    {
        enc_t *newc = realloc (conv_encs, sizeof (enc_t) * (conv_nr + 8));
        if (!newc)
            return 0;
        conv_nr += 8;
        conv_encs = newc;
    }
    if (!conv_encs[nr].enca)
    {
        char *p;
        for (conv_encs[nr].enca = p = strdup (enc); *p; p++)
            *p = toupper (*p);
        conv_encs[nr].encb = strdup (enc);
        conv_encs[nr + 1].enca = NULL;
    }
#if HAVE_ICONV
    if (iconv_check (nr))
        return nr;
#endif
    conv_error = nr;
    return ENC_FAUTO | nr;
}

/*
 * Give the encoding name for a given ID
 */
const char *ConvEncName (UBYTE enc)
{
    if ((enc & ~ENC_FAUTO) > conv_nr)
        return "<auto/undefined>";
    return conv_encs[enc & ~ENC_FAUTO].enca;
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
        b[0] = CHAR_BROKEN;
        b[1] = '\0';
    }
    return b;
}

UDWORD ConvGetUTF8 (strc_t in, int *off)
{
     UDWORD ucs = 0;
     int i, continuations = 1;
     UBYTE  c = in->txt[(*off)++];

     if (~c & 0x80)
         return c;

     if (~c & 0x40)
         return CHAR_BROKEN;

     while (c & 0x20)
     {
         continuations++;
         c <<= 1;
     }

     c &= 0x3f;
     c >>= continuations - 1;

     for (i = 0, ucs = c; i < continuations; i++)
     {
         if (((c = in->txt[*off + i]) & 0xc0) != 0x80)
             return c ? CHAR_BROKEN : CHAR_INCOMPLETE;

         c &= 0x3f;
         ucs <<= 6;
         ucs |= c;
     }
     *off += continuations;
     return ucs;
}

strc_t ConvFrom (strc_t text, UBYTE enc)
{
    enc &= ~ENC_FAUTO;
#if HAVE_ICONV
    if ((enc < conv_nr) && !conv_encs[enc].iof)
        iconv_check (enc);
#endif
    if ((enc >= conv_nr) || (!conv_encs[enc].fof))
        enc = ENC_ASCII;
    return conv_encs[enc].fof (text, enc);
}

strc_t ConvFromSplit (strc_t text, UBYTE enc)
{
    static str_s str;
    str_s tstr;
    const char *p;
    size_t tlen = text->len;
    
    s_init (&str, "", 100);
    tstr.txt = (char *)text->txt;
    tstr.len = text->len;
    tstr.max = 0;
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

strc_t ConvToLen (const char *ctext, UBYTE enc, size_t len)
{
    str_s text;
    enc &= ~ENC_FAUTO;
#if HAVE_ICONV
    if ((enc < conv_nr) && !conv_encs[enc].ito)
        iconv_check (enc);
#endif
    text.txt = (char *)ctext;
    text.len = len;
    text.max = 0;
    if ((enc >= conv_nr) || (!conv_encs[enc].fto))
        enc = ENC_ASCII;
    return conv_encs[enc].fto (&text, enc);
}

strc_t ConvTo (const char *ctext, UBYTE enc)
{
    return ConvToLen (ctext, enc, ctext ? strlen (ctext) : 0);
}

/*
 * Transliterates manually a string if it doesn't fit into the local
 * encoding
 */
const char *ConvTranslit (const char *orig, const char *trans)
{
    UBYTE enc = prG->enc_loc;

    if (strcmp (orig, ConvFrom (ConvTo (orig, enc), enc)->txt))
        return trans;
    return orig;
}

BOOL ConvFits (const char *in, UBYTE enc)
{
    char *inn, *p;
    int i;
    
    inn = strdup (in);
    if (!inn)
        return 0;
    for (p = inn; *p; p++)
        if (*p == Conv0xFE || *p == CHAR_NOT_AVAILABLE || *p == CHAR_INCOMPLETE || *p == CHAR_BROKEN)
            *p = ' ';
    i = strpbrk (ConvFrom (ConvTo (inn, enc), enc)->txt, "?*_") ? 0 : 1;
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
        else if (rc == EINVAL)
        {
            s_catc (&str, CHAR_INCOMPLETE);
            str.txt[str.len] = '\0';
            return &str;
        }
        else
        {
            s_catc (&str, CHAR_NOT_AVAILABLE);
            in++;
            inleft--;
        }
        out = str.txt + str.len;
        outleft = str.max - str.len - 2;
        iconv (conv_encs[enc].ito, NULL, NULL, NULL, NULL);
    }
    *out = '\0';
    str.len = out - str.txt;
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
        else if (rc == EINVAL)
        {
            char inc = CHAR_INCOMPLETE;
            ICONV_CONST char *in = &inc;
            size_t inleft = 1;
            iconv (conv_encs[enc].ito, NULL, NULL, NULL, NULL);
            iconv (conv_encs[enc].ito, &in, &inleft, &out, &outleft);
            str.len = out - str.txt;
            str.txt[str.len] = '\0';
            return &str;
        }
        else
        {
            char inc = CHAR_NOT_AVAILABLE;
            ICONV_CONST char *inn = &inc;
            size_t innleft = 1;
            iconv (conv_encs[enc].ito, NULL, NULL, NULL, NULL);
            iconv (conv_encs[enc].ito, &inn, &innleft, &out, &outleft);
            in++;
            inleft--;
            iconv (conv_encs[enc].ito, NULL, NULL, NULL, NULL);
            continue;
        }
        out = str.txt + str.len;
        outleft = str.max - str.len - 2;
        iconv (conv_encs[enc].ito, NULL, NULL, NULL, NULL);
    }
    *out = '\0';
    str.len = out - str.txt;
    return &str;
}
#endif

static strc_t iconv_from_usascii (strc_t in, UBYTE enc)
{
    static str_s str = { NULL, 0, 0 };
    int off;
    char c;

    s_init (&str, "", in->len);
    for (off = 0; off < in->len; off++)
        s_catc (&str, (c = in->txt[off]) & 0x80 ? CHAR_BROKEN : c);
    return &str;
}

static strc_t iconv_to_usascii (strc_t in, UBYTE enc)
{
    static str_s str = { NULL, 0, 0 };
    UDWORD ucs;
    int off;

    s_init (&str, "", in->len);
    for (off = 0; off < in->len; )
    {
        ucs = ConvGetUTF8 (in, &off);
        s_catc (&str, ucs >= 0x80 ? CHAR_NOT_AVAILABLE : ucs);
    }
    return &str;
}

#if ENABLE_FALLBACK_UCS2BE || ENABLE_FALLBACK_WIN1251 || ENABLE_FALLBACK_KOI8 \
  || ENABLE_FALLBACK_LATIN9 || ENABLE_FALLBACK_LATIN1 || ENABLE_FALLBACK_UTF8 || ENABLE_FALLBACK_WCHART

#if ENABLE_FALLBACK_UTF8
strc_t iconv_utf8_buf (str_t out, strc_t in, UBYTE enc)
{
    UDWORD ucs;
    int off;

    s_init (out, "", in->len);
    for (off = 0; off < in->len; )
    {
        ucs = ConvGetUTF8 (in, &off);
        s_cat (out, ConvUTF8 (ucs));
    }
    return out;
}

strc_t iconv_to_utf8 (strc_t in, UBYTE enc)
{
    static str_s str = { NULL, 0, 0 };
    return iconv_utf8_buf (&str, in, enc);
}

strc_t iconv_from_utf8 (strc_t in, UBYTE enc)
{
    static str_s str = { NULL, 0, 0 };
    return iconv_utf8_buf (&str, in, enc);
}
#endif

#if ENABLE_FALLBACK_LATIN1
static strc_t iconv_from_latin1 (strc_t in, UBYTE enc)
{
    static str_s str = { NULL, 0, 0 };
    int off;

    s_init (&str, "", in->len);
    for (off = 0; off < in->len; off++)
        s_cat (&str, ConvUTF8 ((UBYTE)in->txt[off]));
    return &str;
}

static strc_t iconv_to_latin1 (strc_t in, UBYTE enc)
{
    static str_s str = { NULL, 0, 0 };
    UDWORD ucs;
    int off;

    s_init (&str, "", in->len);
    for (off = 0; off < in->len; )
    {
        ucs = ConvGetUTF8 (in, &off);
        s_catc (&str, ucs & 0xffffff00 ? CHAR_NOT_AVAILABLE : ucs);
    }
    return &str;
}
#endif

#if ENABLE_FALLBACK_LATIN9
static strc_t iconv_from_latin9 (strc_t in, UBYTE enc)
{
    static str_s str = { NULL, 0, 0 };
    UDWORD ucs;
    UBYTE c;
    int off;
    
    s_init (&str, "", in->len);
    for (off = 0; off < in->len; off++)
    {
        c = in->txt[off];
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
    return &str;
}

static strc_t iconv_to_latin9 (strc_t in, UBYTE enc)
{
    static str_s str = { NULL, 0, 0 };
    UDWORD ucs;
    int off;

    s_init (&str, "", in->len);
    for (off = 0; off < in->len; )
    {
        ucs = ConvGetUTF8 (in, &off);
        if (!(ucs & 0xffffff00))
        {
            switch (ucs)
            {
                case 0xa4: case 0xa6: case 0xa8: case 0xb4:
                case 0xb8: case 0xbc: case 0xbd: case 0xbe:
                    ucs = CHAR_NOT_AVAILABLE;
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
                default:     s_catc (&str, CHAR_NOT_AVAILABLE);
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
    static str_s str = { NULL, 0, 0 };
    UBYTE c;
    int off;
    
    s_init (&str, "", in->len);
    for (off = 0; off < in->len; off++)
        s_cat (&str, ConvUTF8 ((c = in->txt[off]) & 0x80 ? koi8u_utf8[c & 0x7f] : c));
    return &str;
}

static strc_t iconv_to_koi8 (strc_t in, UBYTE enc)
{
    static str_s str = { NULL, 0, 0 };
    UDWORD ucs;
    UBYTE c;
    int off;

    s_init (&str, "", in->len);
    for (off = 0; off < in->len; )
    {
        ucs = ConvGetUTF8 (in, &off);
        if (ucs & 0xffffff80)
        {
            for (c = 0; ~c & 0x80; c++)
                if (koi8u_utf8[c] == ucs)
                    break;
            
            s_catc (&str, c & 0x80 ? CHAR_NOT_AVAILABLE : c | 0x80);
        }
        else
            s_catc (&str, ucs);
    }
    return &str;
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
    static str_s str = { NULL, 0, 0 };
    UBYTE c;
    int off;
    
    s_init (&str, "", in->len);
    for (off = 0; off < in->len; off++)
        s_cat (&str, ConvUTF8 ((c = in->txt[off]) & 0x80 ? win1251_utf8[c & 0x7f] : c));
    return &str;
}

static strc_t iconv_to_win1251 (strc_t in, UBYTE enc)
{
    static str_s str = { NULL, 0, 0 };
    UDWORD ucs;
    UBYTE c;
    int off;

    s_init (&str, "", in->len);
    for (off = 0; off < in->len; )
    {
        ucs = ConvGetUTF8 (in, &off);
        if (ucs & 0xffffff80)
        {
            for (c = 0; ~c & 0x80; c++)
                if (win1251_utf8[c] == ucs)
                    break;
            
            s_catc (&str, c & 0x80 ? CHAR_NOT_AVAILABLE : c | 0x80);
        }
        else
            s_catc (&str, ucs);
    }
    return &str;
}
#endif

#if ENABLE_FALLBACK_UCS2BE
static strc_t iconv_from_ucs2be (strc_t in, UBYTE enc)
{
    static str_s str = { NULL, 0, 0 };
    UDWORD ucs;
    int off;

    s_init (&str, "", in->len);
    for (off = 0; off < in->len; )
    {
        if (off + 1 >= in->len)
        {
            s_catc (&str, CHAR_INCOMPLETE);
            break;
        }

        ucs = (UBYTE)in->txt[off++] << 8;
        ucs |= (UBYTE)in->txt[off++];
        if ((ucs & 0xf800) == 0xd800)
            s_catc (&str, CHAR_BROKEN);
        else
            s_cat (&str, ConvUTF8 (ucs));
    }
    return &str;
}

static strc_t iconv_to_ucs2be (strc_t in, UBYTE enc)
{
    static str_s str = { NULL, 0, 0 };
    UDWORD ucs;
    int off;

    s_init (&str, "", in->len);
    for (off = 0; off < in->len; )
    {
        ucs = ConvGetUTF8 (in, &off);
        if (ucs & 0xffff0000)
        {
            s_catc (&str, 0);
            s_catc (&str, CHAR_NOT_AVAILABLE);
        }
        else
        {
            s_catc (&str, (ucs >> 8) & 0xff);
            s_catc (&str, ucs & 0xff);
        }
    }
    return &str;
}
#endif

#if ENABLE_FALLBACK_WCHART
static strc_t iconv_from_wchart (strc_t in, UBYTE enc)
{
    static str_s str = { NULL, 0, 0 };
    UDWORD ucs;
    int off;

    s_init (&str, "", in->len);
    for (off = 0; off < in->len; )
    {
        if (off + sizeof (wchar_t) > in->len)
        {
            s_catc (&str, CHAR_INCOMPLETE);
            break;
        }
        
        ucs = *((wchar_t *)(in->txt + off));
        off += sizeof (wchar_t);
        if ((ucs & 0xf800) == 0xd800)
            s_catc (&str, CHAR_BROKEN);
        else
            s_cat (&str, ConvUTF8 (ucs));
    }
    return &str;
}

static strc_t iconv_to_wchart (strc_t in, UBYTE enc)
{
    static str_s str = { NULL, 0, 0 };
    UDWORD ucs;
    wchar_t na = CHAR_NOT_AVAILABLE;
    int off;

    s_init (&str, "", in->len);
    for (off = 0; off < in->len; )
    {
        ucs = ConvGetUTF8 (in, &off);
        if ((ucs & 0xf800) == 0xd800)
            s_catc (&str, CHAR_BROKEN);
        else if (   (sizeof (wchar_t) <= 1 && ucs & 0xffffff00)
                 || (sizeof (wchar_t) <= 2 && ucs & 0xffff0000))
            s_catn (&str, (const char *)&na, sizeof (wchar_t));
        else
            s_catn (&str, (const char *)&ucs, sizeof (wchar_t));
    }
    return &str;
}
#endif
#endif /* ENABLE_FALLBACK_* */
