#include "micq.h"
#include "mreadline.h"
#include <assert.h>

static WORD recv_packs[ MAX_SEQ_DEPTH ];
static DWORD start=0;
static DWORD end=0;

BOOL Is_Repeat_Packet( WORD this_seq )
{
	int i;
	
	assert( end <= MAX_SEQ_DEPTH );
	for ( i=start; i != end; i++ ) {
		if ( i > MAX_SEQ_DEPTH ) { i = -1; }
		if ( recv_packs[ i ] == this_seq ) {
			if (Verbose ) {
				R_undraw();	
				M_print( "\nDoubled packet %04X\n", this_seq );
				R_redraw();
			}
			return TRUE;
		}
	}
	return FALSE;
}

void Got_SEQ( WORD this_seq )
{
	recv_packs[ end++ ] = this_seq;
	if ( end > MAX_SEQ_DEPTH ) { end = 0; }
	if ( end == start ) { start++; }
	if ( start > MAX_SEQ_DEPTH ) { start = 0; }
}

