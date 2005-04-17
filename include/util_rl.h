
#ifndef MICQ_UTIL_RL_H
#define MICQ_UTIL_RL_H 1

typedef struct rl_autoexpand_s {
  const struct rl_autoexpand_s *next;
  const char *command;
  const char *replace;
} rl_autoexpand_t;

extern UDWORD rl_columns;
extern volatile UBYTE rl_signal;

void ReadLineInit (void);
void ReadLineTtyUnset (void);
void ReadLineTtySet (void);
void ReadLineClrScr (void);
void ReadLineHandleSig (void);
void ReadLineAllowTab (UBYTE onoff);

const rl_autoexpand_t *ReadLineListAutoExpand (void);
void ReadLineAutoExpand (const char *command, const char *replace);

str_t ReadLine (UBYTE newbyte);

void ReadLinePrompt (void);
void ReadLinePromptHide (void);
void ReadLinePromptSet (const char *prompt);
void ReadLinePromptUpdate (const char *prompt);
void ReadLinePromptReset (void);

const char *ReadLinePrintWidth (const char *text, const char *left, const char *right, UWORD *width);

#endif
