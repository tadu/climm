
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
    BOOL isbool;
};

BOOL ContactOptionsGet (const ContactOptions *opt, UWORD flag, const char **res);
void ContactOptionsSet (ContactOptions *opt, UWORD flag, const char *val);

int ContactOptionsImport (ContactOptions *opts, const char *args);
const char *ContactOptionsString (const ContactOptions *opts);

extern struct ContactOption_s ContactOptionsList[];

#define CO_DIRECT 0x8000

#define CO_IGNORE    (CO_DIRECT | 0x0001) /* ignore contact. */
#define CO_HIDEFROM  (CO_DIRECT | 0x0002) /* always pretend to be offline. */
#define CO_INTIMATE  (CO_DIRECT | 0x0004) /* can see even if invisible. */

#define CO_AUTOAWAY  0x0001
#define CO_AUTONA    0x0002
#define CO_AUTOOCC   0x0003
#define CO_AUTODND   0x0004
#define CO_AUTOFFC   0x0005
#define CO_AUTOINV   0x0006

#endif
