
struct Extra_s
{
    Extra  *more;
    char   *text;
    UDWORD  data;
    UDWORD  tag;
};

Extra      *ExtraSet  (Extra *extra, UWORD type, UDWORD value, const char *text);
Extra      *ExtraFind (Extra *extra, UWORD type);
UDWORD      ExtraGet  (Extra *extra, UWORD type);
const char *ExtraGetS (Extra *extra, UWORD type);
void        ExtraD    (Extra *extra);

