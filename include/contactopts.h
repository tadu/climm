
#ifndef MICQ_CONTACTOPTS_H
#define MICQ_CONTACTOPTS_H 1

struct ContactOptions_s
{
    UWORD set, val;
    union
    {
        struct
        {
            UWORD  vala, valb;
            UBYTE  taga, tagb;
        } co_dir;
        struct
        {
            UWORD  size;
            struct ContactOptionsTable_s *table;
        } co_indir;
    } co_un;
};

struct ContactOption_s
{
    const char *name;
    UWORD flag;
};

BOOL ContactOptionsGetVal (const ContactOptions *opt, UWORD flag, UWORD *val);
BOOL ContactOptionsSetVal (ContactOptions *opt, UWORD flag, UWORD val);
void ContactOptionsUndef  (ContactOptions *opt, UWORD flag);

BOOL ContactOptionsGetStr (const ContactOptions *opt, UWORD flag, const char **res);
BOOL ContactOptionsSetStr (ContactOptions *opt, UWORD flag, const char *val);

int ContactOptionsImport (ContactOptions *opts, const char *args);
const char *ContactOptionsString (const ContactOptions *opts);

extern struct ContactOption_s ContactOptionsList[];

#define COF_BOOL    0x8000
#define COF_NUMERIC 0x4000
#define COF_COLOR   0x2000
#define COF_STRING  0x1000

#define COF_DIRECT  COF_BOOL

#define CO_IGNORE        (COF_DIRECT | 0x0001)  /* ignore contact               */
#define CO_HIDEFROM      (COF_DIRECT | 0x0002)  /* always pretend to be offline */
#define CO_INTIMATE      (COF_DIRECT | 0x0004)  /* can see even if invisible    */

#define CO_AUTOAWAY      (COF_STRING  |   0x04) /* the away auto reply message           */
#define CO_AUTONA        (COF_STRING  |   0x05) /* the not available auto reply message  */
#define CO_AUTOOCC       (COF_STRING  |   0x06) /* the occupied auto reply message       */
#define CO_AUTODND       (COF_STRING  |   0x07) /* the do not disturb auto reply message */
#define CO_AUTOFFC       (COF_STRING  |   0x08) /* the free for chat auto reply message  */
#define CO_AUTOINV       (COF_STRING  |   0x09) /* the invisible auto reply message      */

#endif
