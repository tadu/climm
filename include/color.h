
#define ESC "\x1b"

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
 #define BLACK		ESC "[0;30m"
 #define RED		ESC "[0;31m"
 #define GREEN		ESC "[0;32m"
 #define BROWN		ESC "[0;33m"
 #define BLUE		ESC "[0;34m"
 #define MAGENTA	ESC "[0;35m"
 #define CYAN		ESC "[0;36m"
 #define WHITE		ESC "[0;37m"
 #define BRIGHT_BLACK	ESC "[0;1;30m"
 #define BRIGHT_RED	ESC "[0;1;31m"
 #define BRIGHT_GREEN	ESC "[0;1;32m"
 #define BRIGHT_BROWN	ESC "[0;1;33m"
 #define BRIGHT_BLUE	ESC "[0;1;34m"
 #define BRIGHT_MAGENTA	ESC "[0;1;35m"
 #define BRIGHT_CYAN	ESC "[0;1;36m"
 #define BRIGHT_WHITE	ESC "[0;1;37m"
 #define CLRSCR		ESC "[J;"
 #define BOLD		ESC "[1m"
#endif

/*********  Leeched from Xicq :) xtrophy@it.dk ********/
/*********  changed to use escape codes like you should :) ***/
/*********  changed colors ***********************************/
#ifdef ANSI_COLOR
/* Last 2 digit number selects the color */
/* Experiment and let me know if you come up with */
/* A better scheme */
/* these were done by Cherrycoke */
#ifdef COLOR_SCHEME_A
   #define SERVCOL         BRIGHT_BLUE
   #define MESSCOL         BRIGHT_BLUE
   #define CONTACTCOL      GREEN
   #define CLIENTCOL       BRIGHT_RED
   #define NOCOL           ESC "[0m"
#elif defined (COLOR_SCHEME_B)
   #define SERVCOL         MAGENTA
   #define MESSCOL         CYAN
   #define CONTACTCOL      CYAN
   #define CLIENTCOL       CYAN
   #define NOCOL           ESC "[0m"
#elif defined (COLOR_SCHEME_M)
   #define SERVCOL         ESC "[0m"
   #define MESSCOL         GREEN
   #define CONTACTCOL      BRIGHT_GREEN
   #define CLIENTCOL       GREEN
   #define NOCOL           GREEN
#else
   #define SERVCOL         RED
   #define MESSCOL         BRIGHT_BLUE
   #define CONTACTCOL      BRIGHT_MAGENTA
   #define CLIENTCOL       GREEN
   #define NOCOL           ESC "[0m"
   #define ERRCOL          BRIGHT_RED
#endif

#define SENTCOL            MAGENTA BOLD
#define ACKCOL             GREEN BOLD
 
#else
   #define SERVCOL         ""
   #define MESSCOL         ""
   #define CONTACTCOL      ""
   #define CLIENTCOL       ""
   #define NOCOL           ""
   #define SENTCOL         ""
   #define ACKCOL          ""
#endif

#define COLCHR '©'
#define COLSTR "©"

#define COLNONE    ESC COLSTR "0"
#define COLSERV    ESC COLSTR "1"
#define COLCLIENT  ESC COLSTR "2"
#define COLMESS    ESC COLSTR "3"
#define COLCONTACT ESC COLSTR "4"
#define COLSENT    ESC COLSTR "5"
#define COLACK     ESC COLSTR "6"
#define COLERR     ESC COLSTR "7"

