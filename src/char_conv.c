#include "micq.h"
#include <assert.h>

void rus_conv( char *, char* );
void jp_conv( char *, char* );

void char_conv( char *type, char *text)
{
	assert( (type[0] == 'w') || (type[0] == 'c') ); 
	assert( (type[1] == 'w') || (type[1] == 'c') ); 
	if ( Russian ) {
		rus_conv( type, text );
	} else {
		if ( JapaneseEUC ) {
			jp_conv( type, text );
		}
	}
}
