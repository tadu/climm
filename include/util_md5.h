/* $Id: util_ssl.h 2711 2009-03-10 00:20:14Z kuhlmann $ */

#ifndef CLIMM_UTIL_MD5_H
#define CLIMM_UTIL_MD5_H

typedef struct util_md5ctx_s util_md5ctx_t;

util_md5ctx_t *util_md5_init  (void);
void           util_md5_write (util_md5ctx_t *ctx, char *buf, size_t len);
int            util_md5_final (util_md5ctx_t *ctx, char *buf);

#endif
