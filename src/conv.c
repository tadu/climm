/* $Id$ */

#include "micq.h"
#include "conv.h"
#include "preferences.h"

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
