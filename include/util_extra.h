
#ifndef MICQ_UTIL_EXTRA_H
#define MICQ_UTIL_EXTRA_H

struct Extra_s
{
    Extra  *more;
    UDWORD  tag;
    UDWORD  data;
    char   *text;
};

Extra      *ExtraSet   (Extra *extra, UDWORD type, UDWORD value, const char *text);
Extra      *ExtraClone (Extra *extra);
Extra      *ExtraFind  (Extra *extra, UDWORD type);
UDWORD      ExtraGet   (Extra *extra, UDWORD type);
const char *ExtraGetS  (Extra *extra, UDWORD type);
void        ExtraD     (Extra *extra);

#define EXTRA_TRANS        0x11
#define EXTRA_MESSAGE      0x23
#define EXTRA_TIME         0x31
#define EXTRA_STATUS       0x41
#define EXTRA_ORIGIN       0x51
#define EXTRA_REF          0x61
#define EXTRA_FORCE        0x71
#define EXTRA_FILETRANS    0x83
#define EXTRA_FILEACCEPT   0x93

#define EXTRA_TRANS_DC     0x1
#define EXTRA_TRANS_TYPE2  0x2
#define EXTRA_TRANS_ICQv8  0x4
#define EXTRA_TRANS_ICQv5  0x8
#define EXTRA_TRANS_ANY    0xf

#define EXTRA_ORIGIN_v5    0x1
#define EXTRA_ORIGIN_v8    0x2
#define EXTRA_ORIGIN_dc    0x3
#define EXTRA_ORIGIN_ssl   0x4

#endif /* MICQ_UTIL_EXTRA_H */
