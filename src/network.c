/* $Id$ */
/* Copyright: This file may be distributed under version 2 of the GPL licence. */

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
            i = -1;
        }
        if (recv_packs[i] == this_seq)
        {
            if (prG->verbose & DEB_PROTOCOL)
                M_printf (i18n (1623, "Double packet %04x.\n"), this_seq);
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
