#ifdef _MICQ_COLOR_HEADER_
 #undef _MICQ_COLOR_HEADER_
 #undef BRIGHT_BLACK
 #undef BRIGHT_RED
 #undef BRIGHT_GREEN
 #undef BRIGHT_BROWN
 #undef BRIGHT_BLUE
 #undef BRIGHT_MAGENTA
 #undef BRIGHT_CYAN
 #undef BRIGHT_WHITE
 #undef BRIGHT_BLACK
 #undef BRIGHT_RED
 #undef BRIGHT_GREEN
 #undef BRIGHT_BROWN
 #undef BRIGHT_BLUE
 #undef BRIGHT_MAGENTA
 #undef BRIGHT_CYAN
 #undef BRIGHT_WHITE
#else
 #define _MICQ_COLOR_HEADER_
 #define BLACK 		"\x1B[0;30m"
 #define RED 		"\x1B[0;31m"
 #define GREEN		"\x1B[0;32m"
 #define BROWN		"\x1B[0;33m"
 #define BLUE 		"\x1B[0;34m"
 #define MAGENTA	"\x1B[0;35m"
 #define CYAN 		"\x1B[0;36m"
 #define WHITE 		"\x1B[0;37m"
 #define BRIGHT_BLACK 	"\x1B[0;1;30m"
 #define BRIGHT_RED 	"\x1B[0;1;31m"
 #define BRIGHT_GREEN 	"\x1B[0;1;32m"
 #define BRIGHT_BROWN 	"\x1B[0;1;33m"
 #define BRIGHT_BLUE 	"\x1B[0;1;34m"
 #define BRIGHT_MAGENTA "\x1B[0;1;35m"
 #define BRIGHT_CYAN  	"\x1B[0;1;36m"
 #define BRIGHT_WHITE 	"\x1B[0;1;37m"
 #define CLRSCR       	"\x1B[J;"
#endif
