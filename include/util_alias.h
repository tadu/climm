
#ifndef MICQ_UTIL_ALIAS_H
#define MICQ_UTIL_ALIAS_H 1

typedef struct alias_s {
  const struct alias_s *next;
  const char *command;
  const char *replace;
  char autoexpand;
} alias_t;

const alias_t *AliasList (void);
void AliasAdd (const char *command, const char *replace, char autoexpand);
char AliasRemove (const char *command);
const char *AliasExpand (const char *string, UDWORD bytepos, char autoexpand);

#endif
