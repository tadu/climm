/* If this is defined words will not be broken up at the screen's edge
**  You should leave this defined unless something is broken or you like
**  the ugly display  */
#define WORD_WRAP

/* This is the number of times micq will try resending a packet(message)
** before completely giving up.  If you have problems with lost packets 
** increasing this *MAY* help.  It might do nothing.
**  The real icq uses a value of 6  */
#define MAX_RETRY_ATTEMPTS 8

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

/* You want this unless your terminal *REALLY* sucks */
#define ANSI_COLOR

/* If you change this things will probably come out formated badly.
** But you can change the tab stops if you so desire */
#define TAB_STOP 8

/* Define what to output additionally when sending a message. */
#define MSGSENTSTR ">>> "

/* Define what to output additionally when a message is acknowledged. */
#define MSGACKSTR  "ok: "

/* Define what to output additionally when a message is received. */
#define MSGRECSTR  "<<< "

/* Define what to output additionally when sending a tcp message. */
#define MSGTCPSENTSTR "��� "

/* Define what to output additionally when sending a tcp message. */
#define MSGTCPACKSTR "=== "

/* Define what to output additionally when a tcp message is received. */
#define MSGTCPRECSTR  "��� "

/* Define how many characters to print when messages are sent
 * or acknowledged to identify a message. At most the whole
 * line is printed. */
#define MSGID_LENGTH 1000

/* Color schemes  leave them all commented out for the default */
/*
#define COLOR_SCHEME_A
#define COLOR_SCHEME_B
#define COLOR_SCHEME_M
*/

/* FUNNY_MSGS */
/* use translation cc_fun for funny messages, */
/* it is not compiled into the program anymore */

/* Use the arrow keys on ansi terminals.  Very good to have if it works */
#define USE_MREADLINE

/* remove this line to disable shell commands from within micq */
#define SHELL_COMMANDS

/* enables you to run a custom command when a message is received */
#define MSGEXEC

/* enables TCP support */
#define TCP_COMM

