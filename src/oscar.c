/* $Id$ */

#include "micq.h"
#include "util_ui.h"
#include "file_util.h"
#include "util.h"
#include "buildmark.h"
#include "sendmsg.h"
#include "network.h"
#include "cmd_user.h"
#include "icq_response.h"
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#include <winsock2.h>
#else
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#ifndef __BEOS__
#include <arpa/inet.h>
#endif
#include <netdb.h>
#include <sys/wait.h>
#endif

#ifdef __BEOS__
#include "beos.h"
#endif


int create_tlv( char *tlvbuf, int type, char *srcbuf) {

    strncpy(&tlvbuf[0], Word_2_Char(type), 2);
    strncpy(&tlvbuf[2], Word_2_Char(strlen(srcbuf)), 2);
    strncpy(&tlvbuf[4], srcbuf, strlen(srcbuf));

    return (strlen(srcbuf) + 4);

}

int create_snac() {


}

int create_flap() {

}