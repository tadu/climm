/* If this is defined words will not be broken up at the screen's edge
**  You should leave this defined unless something is broken or you like
**  the ugly display  */
#define WORD_WRAP

/* This is the number of times micq will try resending a packet(message)
** before completely giving up.  If you have problems with lost packets 
** increasing this *MAY* help.  It might do nothing.
**  The real icq uses a value of 6  */
#define MAX_RETRY_ATTEMPTS 8

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

/* Define how many characters to print when messages are sent
 * or acknowledged to identify a message. */
#define MSGID_LENGTH 20

/* Color schemes  leave them all commented out for the default */
/*
#define COLOR_SCHEME_A
#define COLOR_SCHEME_B
#define COLOR_SCHEME_M
*/

/* Language which micq will use. You will need to comment english
** out if you want to use another one. */
#define ENGLISH_LANG
/* #define BULGARIAN_LANG
#define POLISH_LANG
#define SPANISH_LANG
#define DUTCH_LANG
#define GERMAN_LANG
#define SWEDISH_LANG
#define CHINESE_LANG
#define BRAZIL_LANG
#define RUSSIAN_LANG
#define ITALIAN_LANG
#define UKRAINIAN_LANG
#define SERBOCROATIAN_LANG
#define CROATIAN_LANG
#define INDONESIAN_LANG
#define FRENCH_LANG
#define FINNISH_LANG
*/

/* Uncomment the below line for humorous messages*/
/* #define FUNNY_MSGS */

/* Use the arrow keys on ansi terminals.  Very good to have if it works */
#define USE_MREADLINE

/* remove this line to disable shell commands from within micq */
#define SHELL_COMMANDS

/* enables you to run a custom command when a message is received */
#define MSGEXEC
