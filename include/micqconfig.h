/* If this is defined words will not be broken up at the screen's edge
**  You should leave this defined unless something is broken or you like
**  the ugly display  */
#define WORD_WRAP

/* Set this if your terminal is an ANSI terminal. Note: you can use color even
   if this is unset. */
#define ANSI_TERM

/* Use the arrow keys on ansi terminals.  Very good to have if it works */
#define USE_MREADLINE

/* remove this line to disable shell commands from within mICQ */
#define SHELL_COMMANDS

/* enables you to run a custom command when a message is received */
#define MSGEXEC

/* This is the number of times mICQ will try resending a packet(message)
** before completely giving up.  If you have problems with lost packets 
** increasing this *MAY* help.  It might do nothing.
**  The real icq uses a value of 6  */
#define MAX_RETRY_ATTEMPTS 8
#define MAX_RETRY_TYPE2_ATTEMPTS 3

/* This is the time in seconds mICQ waits before retrying resp. before
** giving the user feedback about the delay. */
#define RETRY_DELAY_TYPE2 30
#define QUEUE_P2P_RESEND_ACK 20

/* This is the number of times mICQ will try to reconnect to the server
 * ** when the server sends a 'Go Away!' command. */
#define MAX_RECONNECT_ATTEMPTS 10

/* The maximum number of contacts to put in a single packet. */
/* This is for some people who use modems and have problems with large packets*/
#define MAX_CONTS_PACKET 100

/* This is how many packets to "remember" we've received. If you get the same
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

/* Underline every fifth line in e,eg,w,wg,ee,eeg,ww,wwg commands */
#undef CONFIG_UNDERLINE
