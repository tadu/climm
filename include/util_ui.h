UWORD  Get_Max_Screen_Width();
SDWORD Echo_Off (void);
SDWORD Echo_On (void);

void   Prompt (void);
void   Soft_Prompt (void);
void   Time_Stamp (void);
void   Kill_Prompt (void);
FD_T   M_fdopen (const char *, ...);
void   M_fdprint (FD_T fd, const char *str, ...);
int    M_fdnreadln (FD_T fd, char *buf, size_t len);
void   M_print (const char *str, ...);
void   Debug (UDWORD level, const char *str, ...);
