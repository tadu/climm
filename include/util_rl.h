
#ifndef MICQ_UTIL_RL_H
#define MICQ_UTIL_RL_H 1

extern int rl_columns;

void ReadLineInit (void);
void ReadLineTtyUnset (void);
void ReadLineTtySet (void);
void ReadLineClrScr (void);

str_t ReadLine (UBYTE newbyte);

void ReadLinePrompt (void);
void ReadLinePromptHide (void);
void ReadLinePromptSet (const char *prompt);
void ReadLinePromptUpdate (const char *prompt);
void ReadLinePromptReset (void);


#endif
