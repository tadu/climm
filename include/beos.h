/* $Id$ */

/* old BeOS port (c) catlettc@canvaslink.com */

#ifndef MICQ_BEOS_H
#define MICQ_BEOS_H

#include <KernelKit.h>

/* BeOS input stuff */
#define BE_NO   0
#define BE_YES  1

#define BE_HOLD 0
#define BE_GO   1
#define BE_DIE  2

extern int BeStatus;
extern int BeThread;
extern char BeBuffer[];

int Be_GetText(char *s);
int Be_TextReady(void);
void Be_Start(void);
void Be_Stop(void);

#endif
