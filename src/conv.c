/* $Id$ */

#include "micq.h"
#include "conv.h"

void ConvWinUnix (char *text)
{
    if (!uiG.Russian && !uiG.JapaneseEUC)
        return;

    if (uiG.Russian)
        ConvWinKoi (text);
    else
        ConvSjisEuc (text);    
}

void ConvUnixWin (char *text)
{
    if (!uiG.Russian && !uiG.JapaneseEUC)
        return;

    if (uiG.Russian)
        ConvKoiWin (text);
    else
        ConvEucSjis (text);
}
