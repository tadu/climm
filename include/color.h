/* $ Id: $ */

#ifndef MICQ_COLOR_H
#define MICQ_COLOR_H

#define ESC "\x1b"

#define BLACK           ESC "[0;30m"
#define RED             ESC "[0;31m"
#define GREEN           ESC "[0;32m"
#define BROWN           ESC "[0;33m"
#define BLUE            ESC "[0;34m"
#define MAGENTA         ESC "[0;35m"
#define CYAN            ESC "[0;36m"
#define WHITE           ESC "[0;37m"
#define SGR0            ESC "[0m"
#define BOLD            ESC "[1m"
#define CLRSCR          ESC "[J;"

#define COLCHR '©'
#define COLSTR "©"

#define COLNONE         ESC COLSTR "0"
#define COLSERV         ESC COLSTR "1"
#define COLCLIENT       ESC COLSTR "2"
#define COLMESS         ESC COLSTR "3"
#define COLCONTACT      ESC COLSTR "4"
#define COLSENT         ESC COLSTR "5"
#define COLACK          ESC COLSTR "6"
#define COLERR          ESC COLSTR "7"

/*********  Leeched from Xicq :) xtrophy@it.dk ********/
/*********  changed to use escape codes like you should :) ***/
/*********  changed colors ***********************************/
/* Last 2 digit number selects the color */
/* Experiment and let me know if you come up with */
/* A better scheme */
/* these were done by Cherrycoke */
#ifdef COLOR_SCHEME_A
   #define SERVCOL         BLUE BOLD
   #define MESSCOL         BLUE BOLD
   #define CONTACTCOL      GREEN
   #define CLIENTCOL       RED BOLD
   #define NOCOL           SGR0
   #define SENTCOL         MAGENTA BOLD
   #define ACKCOL          GREEN BOLD
   #define ERRCOL          RED BOLD
#elif defined (COLOR_SCHEME_B)
   #define SERVCOL         MAGENTA
   #define MESSCOL         CYAN
   #define CONTACTCOL      CYAN
   #define CLIENTCOL       CYAN
   #define NOCOL           SGR0
   #define SENTCOL         MAGENTA BOLD
   #define ACKCOL          GREEN BOLD
   #define ERRCOL          RED BOLD
#elif defined (COLOR_SCHEME_M)
   #define SERVCOL         SGR0
   #define MESSCOL         GREEN
   #define CONTACTCOL      GREEN BOLD
   #define CLIENTCOL       GREEN
   #define NOCOL           GREEN
   #define SENTCOL         MAGENTA BOLD
   #define ACKCOL          GREEN BOLD
   #define ERRCOL          RED BOLD
#else
   #define SERVCOL         RED
   #define MESSCOL         BLUE BOLD
   #define CONTACTCOL      MAGENTA BOLD
   #define CLIENTCOL       GREEN
   #define NOCOL           SGR0
   #define SENTCOL         MAGENTA BOLD
   #define ACKCOL          GREEN BOLD
   #define ERRCOL          RED BOLD
#endif

#endif
