UWORD  Get_Max_Screen_Width();
SDWORD Echo_Off (void);
SDWORD Echo_On (void);

void   Prompt (void);
void   Soft_Prompt (void);
void   Time_Stamp (void);
void   Kill_Prompt (void);
void   M_print (const char *str, ...);
int    M_pos ();
void   Debug (UDWORD level, const char *str, ...);

void   UtilUIUserOnline  (Contact *cont, UDWORD status);
void   UtilUIUserOffline (Contact *cont);
