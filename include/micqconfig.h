/* If this is defined words will not be broken up at the screen's edge
**  You should leave this defined unless something is broken or you like
**  the ugly display  */
#define WORD_WRAP

/* Set this if your terminal is an ANSI terminal. Note: you can use color even
   if this is unset. */
#define ANSI_TERM

/* Use the arrow keys on ansi terminals.  Very good to have if it works */
#define USE_MREADLINE

/* remove this line to disable shell commands from within micq */
#define SHELL_COMMANDS

/* enables you to run a custom command when a message is received */
#define MSGEXEC

/* This is the number of times micq will try resending a packet(message)
** before completely giving up.  If you have problems with lost packets 
** increasing this *MAY* help.  It might do nothing.
**  The real icq uses a value of 6  */
#define MAX_RETRY_ATTEMPTS 8
#define MAX_RETRY_TYPE2_ATTEMPTS 3

/* This is the number of times micq will try to reconnect to the server
 * ** when the server sends a 'Go Away!' command. */
#define MAX_RECONNECT_ATTEMPTS 10

/* The maximum number of contacts to put in a single packet. */
/* This is for some people who use modems and have problems with large packets*/
#define MAX_CONTS_PACKET 100

/* This is how many packets to "remember" we've recieved. If you get the same
** message twice setting this higher will help.  The default should be fine
** for everyone except the paranoid.  Setting it lower will reduce the memory
** footprint slightly. */
#define MAX_SEQ_DEPTH 50

/* This is the maximum number of people you can have in your list.  This is 
** not only true contacts but aliases to your contacts also. */
#define MAX_CONTACTS 1024

/* If you change this things will probably come out formated badly.
** But you can change the tab stops if you so desire */
#define TAB_STOP 8

#ifndef ENABLE_UTF8
#define MSGSENTSTR      ">>> " /* Define what to output additionally when sending a message. */
#define MSGACKSTR       "ok: " /* Define what to output additionally when a message is acknowledged. */
#define MSGRECSTR       "<<< " /* Define what to output additionally when a message is received. */
#define MSGTCPSENTSTR   "=== " /* Define what to output additionally when sending a tcp message. */
#define MSGTCPACKSTR    "\xbb\xbb\xbb " /* Define what to output additionally when sending a tcp message. */
#define MSGTCPRECSTR    "\xab\xab\xab " /* Define what to output additionally when a tcp message is received. */
#define MSGTYPE2SENTSTR ">>+ " /* Define what to output additionally when a type-2 message is acknowledged. */
#define MSGTYPE2RECSTR  "+<< " /* Define what to output additionally when a type-2 message is received. */
#define STR_DOT         "\b7"
#else
#define MSGSENTSTR      ">>> "
#define MSGACKSTR       "ok: "
#define MSGRECSTR       "<<< "
#define MSGTCPSENTSTR   "=== "
#define MSGTCPACKSTR    "\xc2\xbb\xc2\xbb\xc2\xbb "
#define MSGTCPRECSTR    "\xc2\xab\xc2\xab\xc2\xab "
#define MSGTYPE2SENTSTR ">>\xc2\xbb "
#define MSGTYPE2RECSTR  "\xc2\xab<< "
#define STR_DOT "\xc2\xb7"
#endif

