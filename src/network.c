/* $Id$ */

#include "micq.h"
#include "util_ui.h"
#include "network.h"
#include "preferences.h"
#include <assert.h>

static UWORD recv_packs[MAX_SEQ_DEPTH];
static UDWORD start = 0;
static UDWORD end = 0;

BOOL Is_Repeat_Packet (UWORD this_seq)
{
    int i;

    assert (end <= MAX_SEQ_DEPTH);
    for (i = start; i != end; i++)
    {
        if (i > MAX_SEQ_DEPTH)
        {
            i = 0;
        }
        if (recv_packs[i] == this_seq)
        {
            if (prG->verbose)
                M_print (i18n (623, "Doubled packet %04X\n"), this_seq);
            return TRUE;
        }
    }
    return FALSE;
}

void Got_SEQ (UWORD this_seq)
{
    recv_packs[end++] = this_seq;
    if (end > MAX_SEQ_DEPTH)
    {
        end = 0;
    }
    if (end == start)
    {
        start++;
    }
    if (start > MAX_SEQ_DEPTH)
    {
        start = 0;
    }
}
