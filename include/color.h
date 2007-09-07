/* $Id$ */

#ifndef CLIMM_COLOR_H
#define CLIMM_COLOR_H

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
#define INV             ESC "[7m"

#define COLNONE         ContactPrefStr (NULL, CO_COLORNONE)
#define COLSERVER       ContactPrefStr (NULL, CO_COLORSERVER)
#define COLCLIENT       ContactPrefStr (NULL, CO_COLORCLIENT)
#define COLINVCHAR      ContactPrefStr (NULL, CO_COLORINVCHAR)
#define COLERROR        ContactPrefStr (NULL, CO_COLORERROR)
#define COLDEBUG        ContactPrefStr (NULL, CO_COLORDEBUG)
#define COLQUOTE        ContactPrefStr (NULL, CO_COLORQUOTE)
#define COLMESSAGE      ContactPrefStr (cont, CO_COLORMESSAGE)
#define COLSENT         ContactPrefStr (cont, CO_COLORSENT)
#define COLACK          ContactPrefStr (cont, CO_COLORACK)
#define COLINCOMING     ContactPrefStr (cont, CO_COLORINCOMING)
#define COLCONTACT      ContactPrefStr (cont, CO_COLORCONTACT)

#define COLINDENT       ESC "v"
#define COLEXDENT       ESC "^"
#define COLMSGINDENT    ESC "<"
#define COLMSGEXDENT    ESC ">"
#define COLSINGLE       ESC "."

#endif
