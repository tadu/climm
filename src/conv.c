/*
 * This file handles character conversions.
 *
 * This file is Copyright Rüdiger Kuhlmann; it may be distributed under
 * version 2 of the GPL licence.
 *
 * $Id$
 */

#include "micq.h"
#include "conv.h"
#include "preferences.h"
#include "util_str.h"

void ConvWinUnix (char *text)
{
    if (!(prG->flags & FLAG_CONVRUSS) && !(prG->flags & FLAG_CONVEUC))
        return;

    if (prG->flags & FLAG_CONVRUSS)
        ConvWinKoi (text);
    else
        ConvSjisEuc (text);    
}

void ConvUnixWin (char *text)
{
    if (!(prG->flags & FLAG_CONVRUSS) && !(prG->flags & FLAG_CONVEUC))
        return;

    if (prG->flags & FLAG_CONVRUSS)
        ConvKoiWin (text);
    else
        ConvEucSjis (text);
}

char ConvSep ()
{
    static char conv[2] = "\0";
    
    if (conv[0])
        return conv[0];
    conv[0] = '\xfe';
    ConvWinUnix (conv);
    return conv[0];
}

#define PUT_UTF8(x) \
 do { if (!((x) & 0xffffff80)) t = s_catf (t, &size, "%c",              x); \
 else if (!((x) & 0xfffff800)) t = s_catf (t, &size, "%c%c",          ((x)               >>  6) | 0xc0,  \
                                                                      ((x) &       0x3f)        | 0x80); \
 else if (!((x) & 0xffff0000)) t = s_catf (t, &size, "%c%c%c",        ((x)               >> 12) | 0xe0,  \
                                                                     (((x) &      0xfc0) >>  6) | 0x80,  \
                                                                      ((x) &       0x3f)        | 0x80); \
 else if (!((x) & 0xffe00000)) t = s_catf (t, &size, "%c%c%c%c",      ((x)               >> 18) | 0xf0,  \
                                                                     (((x) &    0x3f000) >> 12) | 0x80,  \
                                                                     (((x) &      0xfc0) >>  6) | 0x80,  \
                                                                      ((x) &       0x3f)        | 0x80); \
 else if (!((x) & 0xfc000000)) t = s_catf (t, &size, "%c%c%c%c%c",    ((x)               >> 24) | 0xf8,  \
                                                                     (((x) &   0xfc0000) >> 18) | 0x80,  \
                                                                     (((x) &    0x3f000) >> 12) | 0x80,  \
                                                                     (((x) &      0xfc0) >>  6) | 0x80,  \
                                                                      ((x) &       0x3f)        | 0x80); \
 else if (!((x) & 0x80000000)) t = s_catf (t, &size, "%c%c%c%c%c%c",  ((x)               >> 30) | 0xf4,  \
                                                                     (((x) & 0x3f000000) >> 24) | 0x80,  \
                                                                     (((x) &   0xfc0000) >> 18) | 0x80,  \
                                                                     (((x) &    0x3f000) >> 12) | 0x80,  \
                                                                     (((x) &      0xfc0) >>  6) | 0x80,  \
                                                                      ((x) &       0x3f)        | 0x80); \
 else t = s_catf (t, &size, "_"); } while (0)

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
    
    t = s_catf (t, &size, "%*s", strlen (in) * 3, "");
    *t = '\0';
    
    for (*t = '\0'; *in; in++)
    {
        if (~*in & 0x80)
        {
            t = s_catf (t, &size, "%c", *in);
            continue;
        }
        switch (enc)
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
                    case '\xa4': PUT_UTF8 (0x20ac); continue; /* EURO */
                    case '\xa6': PUT_UTF8 (0x0160); continue; /* SCARON */
                    case '\xa8': PUT_UTF8 (0x0161); continue; /* SMALL SCARON */
                    case '\xb4': PUT_UTF8 (0x017d); continue; /* ZCARON */
                    case '\xb8': PUT_UTF8 (0x017e); continue; /* SMALL ZCARON */
                    case '\xbc': PUT_UTF8 (0x0152); continue; /* OE */
                    case '\xbd': PUT_UTF8 (0x0153); continue; /* SMALL OE */
                    case '\xbe': PUT_UTF8 (0x0178); continue; /* Y DIAERESIS */
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
            case ENC_SJIS:
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
        switch (enc)
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
            case ENC_SJIS:
            default:
                t = s_cat (t, &size, "?");
        }
    }
    return t;
}
