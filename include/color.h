/* $Id$ */

#ifndef MICQ_COLOR_H
#define MICQ_COLOR_H

#define ESC "\x1b"

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

#define CXNONE          0  /* "default" color */
#define CXSERVER        1  /* server messages */
#define CXCLIENT        2  /* client stuff    */
#define CXMESSAGE       3  /* messages        */
#define CXCONTACT       4  /* contacts        */
#define CXSENT          5  /* sent messages   */
#define CXACK           6  /* ack'ed messages */
#define CXERROR         7  /* errors          */
#define CXINCOMING      8  /* incoming msgs   */
#define CXDEBUG         9  /* debug messages  */
#define CXCOUNT         10

#define COLNONE         ESC COLSTR "0"
#define COLSERVER       ESC COLSTR "1"
#define COLCLIENT       ESC COLSTR "2"
#define COLMESSAGE      ESC COLSTR "3"
#define COLCONTACT      ESC COLSTR "4"
#define COLSENT         ESC COLSTR "5"
#define COLACK          ESC COLSTR "6"
#define COLERROR        ESC COLSTR "7"
#define COLINCOMING     ESC COLSTR "8"
#define COLDEBUG        ESC COLSTR "9"

#define COLINDENT       ESC "v"
#define COLEXDENT       ESC "^"
#define COLMSGINDENT    ESC "<"
#define COLMSGEXDENT    ESC ">"
#define COLSINGLE       ESC "."

#endif
