void M_select_init (void);
void M_set_timeout (UDWORD sec, UDWORD usec);
void M_Add_rsocket (FD_T sok);
void M_Add_wsocket (FD_T sok);
void M_Add_xsocket (FD_T sok);
BOOL M_Is_Set (FD_T sok);
int  M_select (void);
