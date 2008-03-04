
#ifndef CLIMM_IM_ICQ8
#define CLIMM_IM_ICQ8

UBYTE IMRoster (Server *serv, int mode);
UBYTE IMDeleteID (Server *serv, int tag, int id, const char *name);

#define IMROSTER_EXPORT   1 /* export local to sbl */
#define IMROSTER_UPLOAD   2 /* add local to sbl */
#define IMROSTER_DOWNLOAD 3 /* add sbl to local */
#define IMROSTER_IMPORT   4 /* import sbl as local */
#define IMROSTER_SYNC     5 /* import sbl as local if appropriate */
#define IMROSTER_SHOW     6 /* show sbl */
#define IMROSTER_DIFF     7 /* show sbl vs local diff */

#define IMROSTER_ISDOWN(m) ((m) == IMROSTER_DOWNLOAD || (m) == IMROSTER_IMPORT || (m) == IMROSTER_SYNC)

#endif
