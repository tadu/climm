
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

const char *ContactOptionsC2S (const char *color);
const char *ContactOptionsS2C (const char *str);

int ContactOptionsImport (ContactOptions *opts, const char *args);
const char *ContactOptionsString (const ContactOptions *opts);

extern struct ContactOption_s ContactOptionsList[];

#define COF_BOOL    0x8000
#define COF_NUMERIC 0x4000
#define COF_COLOR   0x2000
#define COF_STRING  0x1000

#define COF_DIRECT  0x0800
#define COF_GLOBAL  0x0400
#define COF_GROUP   0x0200
#define COF_CONTACT 0x0100

#define CO_FLAGS    0x00ff

#define CO_DIRECT   (COF_DIRECT | COF_BOOL)
#define CO_CONTACT  (COF_GLOBAL | COF_GROUP | COF_CONTACT)
#define CO_GROUP    (COF_GLOBAL | COF_GROUP)
#define CO_GLOBAL    COF_GLOBAL

#define CO_IGNORE        (CO_DIRECT   | CO_CONTACT | 0x01) /* ignore contact               */
#define CO_HIDEFROM      (CO_DIRECT   | CO_CONTACT | 0x02) /* always pretend to be offline */
#define CO_INTIMATE      (CO_DIRECT   | CO_CONTACT | 0x04) /* can see even if invisible    */
#define CO_LOGONOFF      (CO_DIRECT   | CO_CONTACT | 0x08) /* log on/offline               */
#define CO_LOGCHANGE     (CO_DIRECT   | CO_CONTACT | 0x10) /* log status changes           */
#define CO_LOGMESS       (CO_DIRECT   | CO_CONTACT | 0x20) /* log messages                 */
#define CO_SHOWONOFF     (CO_DIRECT   | CO_CONTACT | 0x40) /* show on/offline              */
#define CO_SHOWCHANGE    (CO_DIRECT   | CO_CONTACT | 0x80) /* show status changes          */

#define CO_ENCODING      (COF_NUMERIC | CO_CONTACT | 0x01) /* the default encoding for this contact */
#define CO_ENCODINGSTR   (COF_STRING  | CO_CONTACT | 0x02) /* the default encoding for this contact */
#define CO_CSCHEME       (COF_NUMERIC | CO_GLOBAL  | 0x03) /* the color scheme to use               */
#define CO_TABSPOOL      (COF_NUMERIC | CO_CONTACT | 0x0a) /* spool contact into tab list           */

#define CO_AUTOAWAY      (COF_STRING  | CO_CONTACT | 0x04) /* the away auto reply message           */
#define CO_AUTONA        (COF_STRING  | CO_CONTACT | 0x05) /* the not available auto reply message  */
#define CO_AUTOOCC       (COF_STRING  | CO_CONTACT | 0x06) /* the occupied auto reply message       */
#define CO_AUTODND       (COF_STRING  | CO_CONTACT | 0x07) /* the do not disturb auto reply message */
#define CO_AUTOFFC       (COF_STRING  | CO_CONTACT | 0x08) /* the free for chat auto reply message  */
#define CO_AUTOINV       (COF_STRING  | CO_CONTACT | 0x09) /* the invisible auto reply message      */

#define CO_COLORNONE     (COF_COLOR   | CO_GLOBAL  | 0x10) /* the escape sequence to print for no color              */
#define CO_COLORSERVER   (COF_COLOR   | CO_GLOBAL  | 0x11) /* the escape sequence to print for server message        */
#define CO_COLORCLIENT   (COF_COLOR   | CO_GLOBAL  | 0x12) /* the escape sequence to print for client stuff          */
#define CO_COLORINVCHAR  (COF_COLOR   | CO_GLOBAL  | 0x13) /* the escape sequence to print for invalid characters    */
#define CO_COLORERROR    (COF_COLOR   | CO_GLOBAL  | 0x14) /* the escape sequence to print for errors                */
#define CO_COLORDEBUG    (COF_COLOR   | CO_GLOBAL  | 0x15) /* the escape sequence to print for debug messages        */
#define CO_COLORQUOTE    (COF_COLOR   | CO_GLOBAL  | 0x16) /* the escape sequence to print for options/quotes        */
#define CO_COLORMESSAGE  (COF_COLOR   | CO_CONTACT | 0x17) /* the escape sequence to print for messages              */
#define CO_COLORSENT     (COF_COLOR   | CO_CONTACT | 0x18) /* the escape sequence to print for sent messages         */
#define CO_COLORACK      (COF_COLOR   | CO_CONTACT | 0x19) /* the escape sequence to print for acknowledged messages */
#define CO_COLORINCOMING (COF_COLOR   | CO_CONTACT | 0x1a) /* the escape sequence to print for incoming messages     */
#define CO_COLORCONTACT  (COF_COLOR   | CO_CONTACT | 0x1b) /* the escape sequence to print for contacts              */

#endif
