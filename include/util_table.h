/* $Id$ */

#ifndef MICQ_UTIL_TABLE
#define MICQ_UTIL_TABLE

const char *TableGetMonth (int code);
const char *TableGetLang (UBYTE code);
void        TablePrintLang (void);
const char *TableGetCountry (UWORD code);
const char *TableGetAffiliation (UWORD code);
const char *TableGetPast (UWORD code);
const char *TableGetOccupation (UWORD code);
const char *TableGetInterest (UWORD code);

#endif /* MICQ_UTIL_TABLE */
