/* BeOS port (c) catlettc@canvaslink.com */

#include <KernelKit.h>
#include <stdio.h>
#include <string.h>
#include "beos.h"

int BeStatus=BE_NO;
int BeThread=BE_GO;
char BeBuffer[1024];
thread_id BeInputThreadId;

int Be_TextReady(void)
{
//	printf("\n\rEntering Be_TextReady();\n\r");
	if(BeStatus==BE_NO)
	{
		return 0;
	}
//	printf("\n\rLeaving Be_TextReady();\n\r");
	return 1;
}

int32 BeOS_Input_Thread(void *data)
{
	char Buffer[1024];
	char Key;
	int Counter;
	memset(Buffer,0,1024);
	Counter=0;
	while(BeStatus!=BE_DIE)
	{
		if(BeThread==BE_GO)
		{
			Key = getchar();
			if(Key != '\n')
			{
				Buffer[Counter]=Key;
				Counter++;
			} else {
				BeThread=BE_HOLD;
				BeStatus=BE_YES;
				Counter=0;
				strcpy(BeBuffer,Buffer);
				memset(Buffer,0,1024);
			}
		}
	}
	return 0;
}

int Be_GetText(char *s)
{
//	printf("\n\rEntering Be_GetText();\n\r");
	if(BeStatus == BE_NO || BeThread == BE_GO)
	{
//		printf("\n\rLeaving Be_GetText();\n\r");
		return 0;
	}
	
	strcpy(s,BeBuffer);
	BeStatus = BE_NO;
	BeThread = BE_GO;
//	printf("\n\rLeaving Be_GetText();\n\r");
	memset(BeBuffer,0,1024);
	return strlen(s);
}


void Be_Start(void)
{
//	printf("\n\rEntering Be_Start();\n\r");
	BeInputThreadId = spawn_thread(BeOS_Input_Thread,"MICQ: BeOS Input Thread",B_LOW_PRIORITY,0);
	resume_thread(BeInputThreadId);
//	printf("\n\rLeaving Be_Start(%i);\n\r",BeInputThreadId);
}

void Be_Stop(void)
{
//	printf("\n\rEntering Be_Stop();\n\r");
	BeThread = BE_DIE;
//	printf("\n\rLeaving Be_Stop();\n\r");
}
