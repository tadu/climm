/* $Id$ */

#include "micq.h"
#include "conv.h"

void ConvWinUnix (char *text)
{
    if (!Russian && !JapaneseEUC)
        return;

    if (Russian)
        ConvWinKoi (text);
    else
        ConvSjisEuc (text);    
}

void ConvUnixWin (char *text)
{
    if (!Russian && !JapaneseEUC)
        return;

    if (Russian)
        ConvKoiWin (text);
    else
        ConvEucSjis (text);
}
