extern UDWORD last_uin;

#define UIN_DELIMS ":|/" /* add space if you don't want your nick names to have spaces */
#define END_MSG_STR "."
#define CANCEL_MSG_STR "#"
#define W_SEPERATOR MESSCOL "============================================" NOCOL "\n"
#define SRCH_START 15
#define SRCH_EMAIL 16
#define SRCH_NICK 17
#define SRCH_FIRST 18
#define SRCH_LAST 19
#define NEW_NICK 910
#define NEW_FIRST 911
#define NEW_LAST 912
#define NEW_EMAIL 913
#define NEW_EMAIL2 914
#define NEW_EMAIL3 915
#define NEW_CITY 916
#define NEW_STATE 917
#define NEW_PHONE 918
#define NEW_FAX 919
#define NEW_STREET 920
#define NEW_CELLULAR 921
#define NEW_ZIP 922
#define NEW_COUNTRY 923
#define NEW_TIME_ZONE 924
#define NEW_AUTH 925
#define NEW_DONE 926
#define MULTI_ABOUT 1000
#define NEW_AGE 1001
#define NEW_SEX 1002
#define NEW_HP 1003
#define NEW_YEAR 1004
#define NEW_MONTH 1005
#define NEW_DAY 1006
#define NEW_LANG1 1007
#define NEW_LANG2 1008
#define NEW_LANG3 1009

#define WP_START 101
#define WP_NICK 101
#define WP_FIRST 102
#define WP_LAST 103
#define WP_EMAIL 104
#define WP_MIN_AGE 105
#define WP_MAX_AGE 106
#define WP_SEX 107
#define WP_LANG1 108
#define WP_CITY 109
#define WP_STATE 110
#define WP_COUNTRY 111
#define WP_COMPANY 112
#define WP_DEPARTMENT 113
#define WP_POSITION 114
#define WP_STATUS 115
#define WP_END 115
  
#define CHANGE_STATUS(a)    icq_change_status (sok, a); \
                            Time_Stamp (); \
                            M_print (" "); \
                            Print_Status (Current_Status); \
                            M_print ("\n");

void Get_Input (SOK_T sok, int *idle_val, int *idle_flag);
void Show_Quick_Status (void);
void Show_Quick_Online_Status (void);
