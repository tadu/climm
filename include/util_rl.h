
#ifndef MICQ_UTIL_RL_H
#define MICQ_UTIL_RL_H 1

extern UDWORD rl_columns;
extern volatile UBYTE rl_signal;

void ReadLineInit (void);
void ReadLineTtyUnset (void);
void ReadLineTtySet (void);
void ReadLineClrScr (void);
void ReadLineHandleSig (void);

str_t ReadLine (UBYTE newbyte);

void ReadLinePrompt (void);
void ReadLinePromptHide (void);
void ReadLinePromptSet (const char *prompt);
void ReadLinePromptUpdate (const char *prompt);
void ReadLinePromptReset (void);


#endif
