#include "micq.h"
#include "mreadline.h"
#include "util_ui.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>


#ifdef _WIN32
   #include <io.h>
   #define S_IRUSR        _S_IREAD
   #define S_IWUSR        _S_IWRITE
#else 
    #include <sys/time.h>
    #include <netinet/in.h>
    #ifndef __BEOS__
       #include <arpa/inet.h>
    #endif
#endif

#ifdef UNIX
   #include <unistd.h>
   #include <termios.h>
   #include <sys/ioctl.h>
#endif

static 	BOOL No_Prompt = FALSE;
	WORD Max_Screen_Width = 0;


/***************************************************************
Turns keybord echo off for the password
****************************************************************/
S_DWORD Echo_Off( void )
{
#ifdef UNIX
    struct termios attr; /* used for getting and setting terminal
                attributes */

    /* Now turn off echo */
    if (tcgetattr(STDIN_FILENO, &attr) != 0) return(-1);
        /* Start by getting current attributes.  This call copies
        all of the terminal paramters into attr */

    attr.c_lflag &= ~(ECHO);
        /* Turn off echo flag.  NOTE: We are careful not to modify any
        bits except ECHO */
    if (tcsetattr(STDIN_FILENO,TCSAFLUSH,&attr) != 0) return(-2);
        /* Wait for all of the data to be printed. */
        /* Set all of the terminal parameters from the (slightly)
           modified struct termios */
        /* Discard any characters that have been typed but not yet read */
#endif
    return 0;
}


/***************************************************************
Turns keybord echo back on after the password
****************************************************************/
S_DWORD Echo_On( void )
{
#ifdef UNIX
    struct termios attr; /* used for getting and setting terminal
                attributes */

    if (tcgetattr(STDIN_FILENO, &attr) != 0) return(-1);

    attr.c_lflag |= ECHO;
    if(tcsetattr(STDIN_FILENO,TCSANOW,&attr) != 0) return(-1);
#endif
    return 0;
}

/**************************************************************
Same as M_print but for FD_T's
***************************************************************/
void M_fdprint( FD_T fd, char *str, ... )
{
   va_list args;
   int k;
   char buf[2048]; /* this should big enough */
        
   assert( buf != NULL );
   assert( 2048 >= strlen( str ) );
   
   va_start( args, str );
   vsnprintf( buf, sizeof(buf), str, args );
   k = write( fd, buf, strlen( buf ) );
   if ( k != strlen( buf ) )
   {
      perror(str);
      exit (10);
   }
   va_end(args);
}

#ifdef UNIX
static volatile DWORD scrwd = 0;
static void micq_sigwinch_handler (int a)
{
   struct winsize ws;
   
   scrwd = 0;
   ioctl (STDOUT, TIOCGWINSZ, &ws);
   scrwd = ws.ws_col;
};
#endif

WORD Get_Max_Screen_Width()
{
   DWORD scrwdtmp = scrwd;

   if (scrwdtmp)
       return scrwdtmp;

#ifdef UNIX
   micq_sigwinch_handler (0);
   if ((scrwdtmp = scrwd))
   {
      if (signal (SIGWINCH, &micq_sigwinch_handler) == SIG_ERR)
         scrwd = 0;
      return scrwdtmp;
   }
#else
   /* Could put more code here to determine the width dynamically of xterms etc */
#endif
   if (Max_Screen_Width)
      return Max_Screen_Width;
   return 80;  /* a reasonable screen width default. */
}

#ifdef WORD_WRAP
static int CharCount = 0;  /* number of characters printed on line. */

static void M_puts_wrap( char *str) 
{
	if ( strlen(str) <= Get_Max_Screen_Width() ) {
		if ( ( strlen(str) + CharCount ) > Get_Max_Screen_Width() ) {
			printf( "\n%s", str );
			CharCount = strlen(str);
		} else {
			CharCount += strlen(str);
			printf( "%s", str );
		}
	} else {
		int i,old;
		for ( i=0; str[i] != 0; i++ ) { 
			old = str[i+1];
			str[i+1] =0;
			M_puts_wrap( str+i ); 
			str[i+1] = old;
		}
	}
}

static void M_prints(char *str) {
      int	i;
      int	TabSize;
      char	*Buffer;
      int             BufIdx = 0;
      int             BufSize = (strlen(str)+2 * sizeof(char));

      if ( NULL == (Buffer = (char *)malloc( BufSize )) )
              perror("Cannot allocate memory for print buffer!\n");
      memset(Buffer, 0, BufSize);
      for (i = 0; i < strlen(str)/*str[i] != 0*/; i++) {
              if (str[i] != '\a') {
                      switch ( str[i] ) {
			case '\n' : {
                              M_puts_wrap( Buffer );
                              M_puts_wrap( "\n" );
                              BufIdx = 0;
                              memset(Buffer, 0, BufSize);
                              CharCount = 0;
                              break;
                      	}
			case '\r': {
                              M_puts_wrap( Buffer );
                              M_puts_wrap( "\r" );
                              BufIdx = 0;
                              memset(Buffer, 0, BufSize);
                              CharCount = 0;
			      break;
                      }
			case '\t': {
                              M_puts_wrap( Buffer );
                              BufIdx = 0;
                              memset(Buffer, 0, BufSize);
                              TabSize = (CharCount % TAB_STOP);
                              for (TabSize = TAB_STOP-TabSize; TabSize != 0; TabSize--) {
				      M_puts_wrap( " " );
                              }  /* end for */
                              BufIdx = 0;
                              memset(Buffer, 0, BufSize);
			      break;
                      }
			case '-':
			case '.':
			case '?':
			case '!':
			case ';':
			case ':':
			case ',':
			case '/':
			case ' ': {
			      Buffer[ BufIdx++ ] = str[i];
                              M_puts_wrap( Buffer );
                              BufIdx = 0;
                              memset(Buffer, 0, BufSize);
				break;
                      } 
			default: 
                              Buffer[BufIdx++] = str[i];
                      }  /* end switch */
              } else if (Sound == SOUND_ON)
                      printf("\a");
              else if (Sound == SOUND_CMD)
                      system(Sound_Str);
      }  /* end for */

      M_puts_wrap( Buffer );

      free(Buffer);
}  /* end aaron's word-wrapping M_prints */

#else

/************************************************************
Prints the preformated sting to stdout.
Plays sounds if appropriate.
************************************************************/
static void M_prints( char *str )
{
   int i, temp;
   static int chars_printed=0;
   
   for ( i=0; str[i] != 0; i++ )
   {
      if ( str[i] != '\a' )
         {
            if ( str[ i ] == '\n' ) {
               printf( "\n" ) ;
               chars_printed = 0;
            }
            else if ( str[ i ] == '\r' ) {
               printf( "\r" ) ;
               chars_printed = 0;
            }
            else if ( str[ i ] == '\t' ) {
               temp = (chars_printed % TAB_STOP );
               /*chars_printed += TAB_STOP - temp; */
               temp = TAB_STOP - temp;
               for ( ; temp != 0; temp-- ) { 
			M_prints( " " );
		 }
            }
            else if ( ( str[ i ] >= 32 ) || ( str[i] < 0 ) ) {
               printf( "%c", str[i] );
               chars_printed++;
               if ( ( Get_Max_Screen_Width() != 0 ) &&
			( chars_printed > Get_Max_Screen_Width() ) ) {
                  printf( "\n" );
		  chars_printed = 0;
               }
            }
         }
      else if ( SOUND_ON == Sound )
         printf( "\a" );
      else if ( SOUND_CMD == Sound )
     system ( Sound_Str );
   }
}
#endif

/**************************************************************
M_print with colors.
***************************************************************/
void M_print( char *str, ... )
{
   va_list args;
   char buf[2048];
   char *str1, *str2;
   
   va_start( args, str );
#ifndef CURSES_UI
   vsprintf( buf, str, args );
   str2 = buf;
   while ( (void *) NULL != ( str1 = strchr( str2, '\x1b' ) ) )
   {
      str1[0] = 0;
      M_prints( str2 );
      str1[0] = 0x1B;
      str2 = strchr (str1, 'm');
      if (str2)
      {
         str2[0] = 0;
         if (Color) printf ("%sm", str1);
         str2++;
      }
      else
         str2 = str1 + 1;
   }
   M_prints( str2 );
#else
   #error No curses support included yet.
   #error You must add it yourself.
#endif
   va_end( args );
}

/***********************************************************
Reads a line of input from the file descriptor fd into buf
an entire line is read but no more than len bytes are 
actually stored
************************************************************/
int M_fdnreadln( FD_T fd, char *buf, size_t len )
{
   int i,j;
   char tmp;

   assert( buf != NULL );
   assert( len > 0 );
   tmp = 0;
   len--;
   for ( i=-1; ( tmp != '\n' )  ; )
   {
      if  ( ( i < len ) || ( i == -1 ) )
      {
         i++;
         j = read( fd, &buf[i], 1 );
         tmp = buf[i];
      }
      else
      {
         j = read( fd, &tmp, 1 );
      }
      assert( j != -1 );
      if ( j == 0 )
      {
         buf[i] =  0;
         return -1;
      }
   }
   if ( i < 1 )
   {
      buf[i] = 0;
   }
   else
   {
      if ( buf[i-1] == '\r' )
      {
         buf[i-1] = 0;
      }
      else
      {
         buf[i] = 0;
      }
   } 
   return 0;
}

/*****************************************************
Disables the printing of the next prompt.
useful for multipacket messages.
******************************************************/
void Kill_Prompt( void )
{
     No_Prompt = TRUE;
}

/*****************************************************
Displays the Micq prompt.  Maybe someday this will be 
configurable
******************************************************/
void Prompt( void )
{
#if 0
   if ( !No_Prompt ) 
#endif
      R_doprompt ( SERVCOL PROMPT_STR NOCOL );
#ifndef USE_MREADLINE
      fflush( stdout );
#endif
   No_Prompt = FALSE;
}

/*****************************************************
Displays the Micq prompt.  Maybe someday this will be 
configurable
******************************************************/
void Soft_Prompt( void )
{
#if 1
   R_doprompt ( SERVCOL PROMPT_STR NOCOL );
   No_Prompt = FALSE;
#else
   if ( !No_Prompt ) {
      M_print( PROMPT_STR );
      fflush( stdout );
   } else {
     No_Prompt = FALSE;
   }
#endif
}

void Time_Stamp( void )
{
   struct tm *thetime;
   time_t p;
   
   p=time(NULL);
   thetime=localtime(&p);

   M_print( "%.02d:%.02d:%.02d",thetime->tm_hour,thetime->tm_min,thetime->tm_sec );
}


/***   adds <uin> to the tablist  ***/
void add_tab( DWORD uin )
{
    int i, j;
    char *new = NULL;
    char *temp = NULL;
    char *orig = NULL;

    /** If the UIN is in the Userlist, add the nick **/
    for ( i=0; i < Num_Contacts; i++ )
    {
	if ( Contacts[i].uin == uin )
	{
	    /* checking for repeated entry missing */
	    new = strdup( Contacts[i].nick );
	    break;
	}
    }
    
    /** else, add the UIN only. **/
    if ( NULL == new ) 
    {
	new = calloc( 1, 3 * sizeof( DWORD ) + 1 );
	sprintf( new, "%ld", uin );
    }
    
    orig = new;
    
    /** insert the new UIN at place [0]
     ** place [0] into [1] (...) until 
     ** (([n] == NULL) or ([0] == [n])) -> entry exists.
    **/
    for ( j=0; j<TAB_SLOTS; j++ ) 
    {
        temp = tab_array[j];
	tab_array[j] = new;

        if ( temp == NULL ) 
	    break;

        if ( ! strcasecmp( orig, temp ) )
    	    break;

        new = temp;
    }
    
    /** Now, if the tab-list is full, throw the last entry out **/
    if ( temp != NULL )
	    free( temp );

}
