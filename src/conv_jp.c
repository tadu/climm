/* $Id$ */
/* Copyright: AYUHANA Tomonori <l@kondara.org> This file may be distributed under version 2 of the GPL licence. */

/*********************************
 jconv-0.1.3 for mICQ

 Japanese KANJI conversion

 AYUHANA Tomonori <l@kondara.org>
*********************************/
#include <stdio.h>
#include <string.h>

#include "micq.h"
#include "conv.h"

#define JP_CONV_SIZE 10000

/*********************************
 Shift-JIS(Win) -> EUC(Unix)
*********************************/
void ConvSjisEuc (char *pszDest)
{
    unsigned char *psz1st;
    unsigned char *psz2nd;
    unsigned char szSrcBuf[JP_CONV_SIZE];
    unsigned char *pszDestBuf;
    unsigned char *pszSrcBuf;

    pszDestBuf = pszDest;
    strncpy (szSrcBuf, pszDest, JP_CONV_SIZE);
    pszSrcBuf = szSrcBuf;

    for (; *pszSrcBuf != '\0';)
    {
        psz1st = pszSrcBuf;
        psz2nd = (pszSrcBuf + 1);

        /* code check */
        if (*psz1st < 0x80)
        {
            /* ASCII */
            *pszDestBuf = *psz1st;
            pszDestBuf++;
            pszSrcBuf++;
        }
        else if (*psz1st >= 0x81 && *psz1st <= 0x9f)
        {
            /* Shift-JIS */
            if (*psz2nd < 0x9f)
            {
                if (*psz1st < 0xa0)
                {
                    *psz1st -= 0x81;
                    *psz1st *= 2;
                    *psz1st += 0xa1;
                }
                else
                {
                    *psz1st -= 0xe0;
                    *psz1st *= 2;
                    *psz1st += 0xdf;
                }
                if (*psz2nd > 0x7f)
                {
                    --*psz2nd;
                }
                *psz2nd += 0x61;
            }
            else
            {
                if (*psz1st < 0xa0)
                {
                    *psz1st -= 0x81;
                    *psz1st *= 2;
                    *psz1st += 0xa2;
                }
                else
                {
                    *psz1st -= 0xe0;
                    *psz1st *= 2;
                    *psz1st += 0xe0;
                }
                *psz2nd += 2;
            }
            *pszDestBuf = *psz1st;
            *(pszDestBuf + 1) = *psz2nd;
            pszDestBuf += 2;
            pszSrcBuf += 2;
        }
        else
        {
            /* EUC */
            *pszDestBuf = *psz1st;
            *(pszDestBuf + 1) = *psz2nd;
            pszDestBuf += 2;
            pszSrcBuf += 2;
        }
    }
}

/*********************************
 EUC(Unix) -> Shift-JIS(Win)
*********************************/
void ConvEucSjis (char *pszDest)
{
    unsigned char *psz1st;
    unsigned char *psz2nd;
    unsigned char szSrcBuf[JP_CONV_SIZE];
    unsigned char *pszDestBuf;
    unsigned char *pszSrcBuf;

    pszDestBuf = pszDest;
    strncpy (szSrcBuf, pszDest, JP_CONV_SIZE);
    pszSrcBuf = szSrcBuf;

    for (; *pszSrcBuf != '\0';)
    {
        psz1st = pszSrcBuf;
        psz2nd = (pszSrcBuf + 1);

        /* code check */
        if (*psz1st < 0x80)
        {
            /* ASCII */
            *pszDestBuf = *psz1st;
            pszDestBuf++;
            pszSrcBuf++;
        }
        else if (*psz1st >= 0x81 && *psz1st <= 0x9f)
        {
            /* Shift-JIS */
            *pszDestBuf = *psz1st;
            *(pszDestBuf + 1) = *psz2nd;
            pszDestBuf += 2;
            pszSrcBuf += 2;
        }
        else
        {
            /* EUC */
            /* 2nd Byte */
            if ((*psz1st % 2) == 0)
            {
                *psz2nd -= 0x02;
            }
            else
            {
                *psz2nd -= 0x61;
                if (*psz2nd > 0x7e)
                {
                    ++*psz2nd;
                }
            }
            /* 1st Byte */
            if (*psz1st < 0xdf)
            {
                ++*psz1st;
                *psz1st /= 2;
                *psz1st += 0x30;
            }
            else
            {
                ++*psz1st;
                *psz1st /= 2;
                *psz1st += 0x70;
            }
            *pszDestBuf = *psz1st;
            *(pszDestBuf + 1) = *psz2nd;
            pszDestBuf += 2;
            pszSrcBuf += 2;
        }
    }
}
