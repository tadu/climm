
#ifndef MICQ_CONTACTOPTS_H
#define MICQ_CONTACTOPTS_H 1

#define CONTACTOPTS_TABLESIZE 4

struct ContactOptions_s
{
    ContactOptions *next;
    UBYTE tags[CONTACTOPTS_TABLESIZE];
    val_t vals[CONTACTOPTS_TABLESIZE];
};

struct ContactOption_s
{
    const char *name;
    UDWORD flag;
};

BOOL ContactOptionsGetVal   (const ContactOptions *opt, UDWORD flag, val_t *val);
BOOL ContactOptionsSetVal   (ContactOptions *opt, UDWORD flag, val_t val);
val_t ContactOptionsUndef   (ContactOptions *opt, UDWORD flag);

BOOL ContactOptionsGetStr   (const ContactOptions *opt, UDWORD flag, const char **res);
BOOL ContactOptionsSetStr   (ContactOptions *opt, UDWORD flag, const char *val);

const char *ContactOptionsC2S (const char *color);
const char *ContactOptionsS2C (const char *str);

int ContactOptionsImport (ContactOptions *opts, const char *args);
const char *ContactOptionsString (const ContactOptions *opts);

extern struct ContactOption_s ContactOptionsList[];

#define COF_BOOL    0x80000000UL
#define COF_NUMERIC 0x40000000UL
#define COF_COLOR   0x20000000UL
#define COF_STRING  0x10000000UL

#define CO_BOOLMASK 0x00ffff00UL

#define COF_GLOBAL  0x04000000UL
#define COF_GROUP   0x02000000UL
#define COF_CONTACT 0x01000000UL

#define CO_DIRECT   (COF_DIRECT | COF_BOOL)
#define CO_CONTACT  (COF_GLOBAL | COF_GROUP | COF_CONTACT)
#define CO_GROUP    (COF_GLOBAL | COF_GROUP)
#define CO_GLOBAL    COF_GLOBAL

#define CO_IGNORE        (COF_BOOL    | CO_CONTACT | 0x000101UL) /* ignore contact               */
#define CO_HIDEFROM      (COF_BOOL    | CO_CONTACT | 0x000401UL) /* always pretend to be offline */
#define CO_INTIMATE      (COF_BOOL    | CO_CONTACT | 0x001001UL) /* can see even if invisible    */
#define CO_LOGONOFF      (COF_BOOL    | CO_CONTACT | 0x004001UL) /* log on/offline               */
#define CO_LOGCHANGE     (COF_BOOL    | CO_CONTACT | 0x010001UL) /* log status changes           */
#define CO_LOGMESS       (COF_BOOL    | CO_CONTACT | 0x040001UL) /* log messages                 */
#define CO_SHOWONOFF     (COF_BOOL    | CO_CONTACT | 0x100001UL) /* show on/offline              */
#define CO_SHOWCHANGE    (COF_BOOL    | CO_CONTACT | 0x400001UL) /* show status changes          */

#define CO_WEBAWARE      (COF_BOOL    | CO_GROUP   | 0x000102UL) /* this connection is webaware           */
#define CO_HIDEIP        (COF_BOOL    | CO_GROUP   | 0x000402UL) /* this connection hides its LAN ip      */
#define CO_DCAUTH        (COF_BOOL    | CO_GROUP   | 0x001002UL) /* this connection requires auth for dc  */
#define CO_DCCONT        (COF_BOOL    | CO_GROUP   | 0x004002UL) /* this connection: dc only for contacts */

#define CO_ENCODING      (COF_NUMERIC | CO_CONTACT | 0x03UL) /* the default encoding for this contact */
#define CO_ENCODINGSTR   (COF_STRING  | CO_CONTACT | 0x04UL) /* the default encoding for this contact */
#define CO_CSCHEME       (COF_NUMERIC | CO_GLOBAL  | 0x05UL) /* the color scheme to use               */
#define CO_TABSPOOL      (COF_NUMERIC | CO_CONTACT | 0x06UL) /* spool contact into tab list           */

#define CO_TIMESEEN      (COF_NUMERIC | COF_CONTACT| 0x07UL) /* time contact was last seen            */
#define CO_TIMEONLINE    (COF_NUMERIC | COF_CONTACT| 0x08UL) /* time since contact is online          */
#define CO_TIMEMICQ      (COF_NUMERIC | COF_CONTACT| 0x09UL) /* time contact last used mICQ           */

#define CO_AUTOAWAY      (COF_STRING  | CO_CONTACT | 0x10UL) /* the away auto reply message           */
#define CO_AUTONA        (COF_STRING  | CO_CONTACT | 0x11UL) /* the not available auto reply message  */
#define CO_AUTOOCC       (COF_STRING  | CO_CONTACT | 0x12UL) /* the occupied auto reply message       */
#define CO_AUTODND       (COF_STRING  | CO_CONTACT | 0x13UL) /* the do not disturb auto reply message */
#define CO_AUTOFFC       (COF_STRING  | CO_CONTACT | 0x14UL) /* the free for chat auto reply message  */
#define CO_AUTOINV       (COF_STRING  | CO_CONTACT | 0x15UL) /* the invisible auto reply message      */

#define CO_COLORNONE     (COF_COLOR   | CO_GLOBAL  | 0x80UL) /* the escape sequence to print for no color              */
#define CO_COLORSERVER   (COF_COLOR   | CO_GLOBAL  | 0x81UL) /* the escape sequence to print for server message        */
#define CO_COLORCLIENT   (COF_COLOR   | CO_GLOBAL  | 0x82UL) /* the escape sequence to print for client stuff          */
#define CO_COLORINVCHAR  (COF_COLOR   | CO_GLOBAL  | 0x83UL) /* the escape sequence to print for invalid characters    */
#define CO_COLORERROR    (COF_COLOR   | CO_GLOBAL  | 0x84UL) /* the escape sequence to print for errors                */
#define CO_COLORDEBUG    (COF_COLOR   | CO_GLOBAL  | 0x85UL) /* the escape sequence to print for debug messages        */
#define CO_COLORQUOTE    (COF_COLOR   | CO_GLOBAL  | 0x86UL) /* the escape sequence to print for options/quotes        */
#define CO_COLORMESSAGE  (COF_COLOR   | CO_CONTACT | 0x87UL) /* the escape sequence to print for messages              */
#define CO_COLORSENT     (COF_COLOR   | CO_CONTACT | 0x88UL) /* the escape sequence to print for sent messages         */
#define CO_COLORACK      (COF_COLOR   | CO_CONTACT | 0x89UL) /* the escape sequence to print for acknowledged messages */
#define CO_COLORINCOMING (COF_COLOR   | CO_CONTACT | 0x8aUL) /* the escape sequence to print for incoming messages     */
#define CO_COLORCONTACT  (COF_COLOR   | CO_CONTACT | 0x8bUL) /* the escape sequence to print for contacts              */

#endif
