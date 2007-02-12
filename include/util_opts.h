
#ifndef MICQ_UTIL_OPTS_H
#define MICQ_UTIL_OPTS_H 1

#define OPT_TABLESIZE 4

struct Opt_s
{
    Opt *next;
    UBYTE tags[OPT_TABLESIZE];
    val_t vals[OPT_TABLESIZE];
};

Opt *OptC (void);
void OptD (Opt *opt);

BOOL OptGetVal   (const Opt *opt, UDWORD flag, val_t *val DEBUGPARAM);
BOOL OptSetVal   (Opt *opt, UDWORD flag, val_t val DEBUGPARAM);
Opt *OptSetVals  (Opt *opt, UDWORD flag, ...);
val_t OptUndef   (Opt *opt, UDWORD flag DEBUGPARAM);

BOOL OptGetStr   (const Opt *opt, UDWORD flag, const char **res DEBUGPARAM);
BOOL OptSetStr   (Opt *opt, UDWORD flag, const char *val DEBUGPARAM);

#define OptGetVal(o,f,v) OptGetVal(o,f,v DEBUGARGS)
#define OptSetVal(o,f,v) OptSetVal(o,f,v DEBUGARGS)
#define OptUndef(o,f)    OptUndef(o,f DEBUGARGS)
#define OptGetStr(o,f,r) OptGetStr(o,f,r DEBUGARGS)
#define OptSetStr(o,f,r) OptSetStr(o,f,r DEBUGARGS)

const char *OptC2S (const char *color);
const char *OptS2C (const char *str);

int OptImport (Opt *opts, const char *args);
const char *OptString (const Opt *opts);

struct OptEntry_s
{
    const char *name;
    UDWORD flag;
};

extern struct OptEntry_s OptList[];

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

#define CO_ISSBL         (COF_BOOL    | CO_CONTACT | 0x000102UL) /* is on sbl                    */
#define CO_WANTSBL       (COF_BOOL    | CO_CONTACT | 0x000402UL) /* want it to be on sbl         */
#define CO_SHADOW        (COF_BOOL    | CO_CONTACT | 0x001002UL) /* don't display in contact list          */
#define CO_LOCAL         (COF_BOOL    | CO_CONTACT | 0x004002UL) /* do not request status changes for this */
#define CO_HIDEACK       (COF_BOOL    | CO_CONTACT | 0x010002UL) /* hide when message acknowledge arrives  */
#define CO_TALKEDTO      (COF_BOOL    | CO_CONTACT | 0x040002UL) /* sent a msg to this contact yet         */
#define CO_AUTOAUTO      (COF_BOOL    | CO_CONTACT | 0x100002UL) /* autogetauto on status change */
#define CO_PEEKME        (COF_BOOL    | CO_CONTACT | 0x400002UL) /* shall be peeked on peekall   */

#define CO_WEBAWARE      (COF_BOOL    | CO_GROUP   | 0x000103UL) /* this connection is webaware           */
#define CO_HIDEIP        (COF_BOOL    | CO_GROUP   | 0x000403UL) /* this connection hides its LAN ip      */
#define CO_DCAUTH        (COF_BOOL    | CO_GROUP   | 0x001003UL) /* this connection requires auth for dc  */
#define CO_DCCONT        (COF_BOOL    | CO_GROUP   | 0x004003UL) /* this connection: dc only for contacts */
#define CO_OBEYSBL       (COF_BOOL    | CO_GROUP   | 0x010003UL) /* this connection obeys the sbl         */
#define CO_AWAYCOUNT     (COF_BOOL    | CO_GROUP   | 0x100003UL) /* this connection counts msgs even if _manual_ na/away/... */

#define CO_ENCODING      (COF_NUMERIC | CO_CONTACT | 0x04UL) /* the default encoding for this contact */
#define CO_ENCODINGSTR   (COF_STRING  | CO_CONTACT | 0x05UL) /* the default encoding for this contact */
#define CO_CSCHEME       (COF_NUMERIC | CO_GLOBAL  | 0x06UL) /* the color scheme to use               */
#define CO_TABSPOOL      (COF_NUMERIC | CO_CONTACT | 0x07UL) /* spool contact into tab list           */

#define CO_TIMESEEN      (COF_NUMERIC | COF_CONTACT| 0x08UL) /* time contact was last seen            */
#define CO_TIMEONLINE    (COF_NUMERIC | COF_CONTACT| 0x09UL) /* time since contact is online          */
#define CO_TIMEMICQ      (COF_NUMERIC | COF_CONTACT| 0x0aUL) /* time contact last used mICQ           */

#define CO_REVEALTIME    (COF_NUMERIC | CO_CONTACT | 0x0bUL) /* time to reveal invisibility to contact*/

#define CO_AUTOAWAY      (COF_STRING  | CO_CONTACT | 0x10UL) /* the away auto reply message           */
#define CO_AUTONA        (COF_STRING  | CO_CONTACT | 0x11UL) /* the not available auto reply message  */
#define CO_AUTOOCC       (COF_STRING  | CO_CONTACT | 0x12UL) /* the occupied auto reply message       */
#define CO_AUTODND       (COF_STRING  | CO_CONTACT | 0x13UL) /* the do not disturb auto reply message */
#define CO_AUTOFFC       (COF_STRING  | CO_CONTACT | 0x14UL) /* the free for chat auto reply message  */
#define CO_AUTOINV       (COF_STRING  | CO_CONTACT | 0x15UL) /* the invisible auto reply message      */

#define CO_TAUTOAWAY     (COF_STRING  | CO_CONTACT | 0x20UL) /* the temp away auto reply message           */
#define CO_TAUTONA       (COF_STRING  | CO_CONTACT | 0x21UL) /* the temp not available auto reply message  */
#define CO_TAUTOOCC      (COF_STRING  | CO_CONTACT | 0x22UL) /* the temp occupied auto reply message       */
#define CO_TAUTODND      (COF_STRING  | CO_CONTACT | 0x23UL) /* the temp do not disturb auto reply message */
#define CO_TAUTOFFC      (COF_STRING  | CO_CONTACT | 0x24UL) /* the temp free for chat auto reply message  */
#define CO_TAUTOINV      (COF_STRING  | CO_CONTACT | 0x25UL) /* the temp invisible auto reply message      */

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

#define CO_MICQCOMMAND   (COF_STRING               | 0x30UL) /* the mICQ command to execute */

#define CO_MSGTEXT       (COF_STRING               | 0x20UL) /* the message text */
#define CO_MSGTYPE       (COF_NUMERIC              | 0x21UL) /* the message type */
#define CO_MSGTRANS      (COF_NUMERIC              | 0x22UL) /* message transportations to try */
#define CO_ORIGIN        (COF_NUMERIC              | 0x23UL) /* the message's origin */
#define CO_STATUS        (COF_NUMERIC              | 0x24UL) /* a status */
#define CO_BYTES         (COF_NUMERIC              | 0x25UL) /* a file length */
#define CO_PORT          (COF_NUMERIC              | 0x26UL) /* a port number */
#define CO_REF           (COF_NUMERIC              | 0x27UL) /* a reference */
#define CO_FORCE         (COF_NUMERIC              | 0x28UL) /* force action */
#define CO_FILENAME      (COF_STRING               | 0x29UL) /* a filename */
#define CO_FILEACCEPT    (COF_NUMERIC              | 0x2aUL) /* accept a file transfer */
#define CO_REFUSE        (COF_STRING               | 0x2bUL) /* refuse message */
#define CO_SUBJECT       (COF_STRING               | 0x2cUL) /* message with subject */

#define CV_ORIGIN_v5  8
#define CV_ORIGIN_v8  2
#define CV_ORIGIN_dc  1
#define CV_ORIGIN_ssl 16

#define CV_MSGTRANS_DC      1
#define CV_MSGTRANS_TYPE2   2
#define CV_MSGTRANS_ICQv8   4
#define CV_MSGTRANS_ICQv5   8
#define CV_MSGTRANS_JABBER 16
#define CV_MSGTRANS_ANY    31

#endif
