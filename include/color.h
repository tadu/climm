/* $Id$ */

#ifndef MICQ_COLOR_H
#define MICQ_COLOR_H

#define ESC "\x1b"
#define ESCC '\x1b'

#define BLACK           ESC "[0;30m"
#define RED             ESC "[0;31m"
#define GREEN           ESC "[0;32m"
#define YELLOW          ESC "[0;33m"
#define BLUE            ESC "[0;34m"
#define MAGENTA         ESC "[0;35m"
#define CYAN            ESC "[0;36m"
#define WHITE           ESC "[0;37m"
#define SGR0            ESC "[0m"
#define BOLD            ESC "[1m"
#define UL              ESC "[4m"

#define COLCHR '!'
#define COLSTR "!"

#define CXCOUNT         7

#define COLNONE         ESC COLSTR "0"
#define COLSERVER       ESC COLSTR "1"
#define COLCLIENT       ESC COLSTR "2"
#define COLCONTACT      ESC COLSTR "3"
#define COLERROR        ESC COLSTR "4"
#define COLDEBUG        ESC COLSTR "5"
#define COLQUOTE        ESC COLSTR "6"
#define COLMESSAGE      ContactPrefStr (cont, CO_COLORMESSAGE)
#define COLSENT         ContactPrefStr (cont, CO_COLORSENT)
#define COLACK          ContactPrefStr (cont, CO_COLORACK)
#define COLINCOMING     ContactPrefStr (cont, CO_COLORINCOMING)

#define COLINDENT       ESC "v"
#define COLEXDENT       ESC "^"
#define COLMSGINDENT    ESC "<"
#define COLMSGEXDENT    ESC ">"
#define COLSINGLE       ESC "."

#endif
