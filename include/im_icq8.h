
#ifndef MICQ_IM_ICQ8
#define MICQ_IM_ICQ8

UBYTE IMRoster (Connection *conn, int mode);

#define IMROSTER_EXPORT   1 /* export local to sbl */
#define IMROSTER_UPLOAD   2 /* add local to sbl */
#define IMROSTER_DOWNLOAD 3 /* add sbl to local */
#define IMROSTER_IMPORT   4 /* import sbl as local */
#define IMROSTER_SYNC     5 /* import sbl as local if appropriate */
#define IMROSTER_SHOW     6 /* show sbl */
#define IMROSTER_DIFF     7 /* show sbl vs local diff */

#define IMROSTER_ISDOWN(m) ((m) == IMROSTER_DOWNLOAD || (m) == IMROSTER_IMPORT || (m) == IMROSTER_SYNC)

#endif
