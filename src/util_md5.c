/*
 * md5 support functions
 *
 * climm Copyright (C) © 2001-2009 Rüdiger Kuhlmann
 *
 * climm is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 dated June, 1991.
 *
 * climm is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * In addition, as a special exception permission is granted to link the
 * code of this release of climm with the OpenSSL project's "OpenSSL"
 * library, and distribute the linked executables.  You must obey the GNU
 * General Public License in all respects for all of the code used other
 * than "OpenSSL".  If you modify this file, you may extend this exception
 * to your version of the file, but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your version
 * of this file.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * $Id: util_ssl.c 2723 2009-03-10 21:44:18Z kuhlmann $
 */

#include "climm.h"
#include "util_md5.h"
#include <assert.h>

#if ENABLE_GNUTLS
#include <gcrypt.h>
#endif
#if ENABLE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/dh.h>
#include <openssl/opensslv.h>
#include <openssl/md5.h>
#endif


struct util_md5ctx_s {
    char usegnutls;
#if ENABLE_GNUTLS
    gcry_md_hd_t h;
#endif
#if ENABLE_OPENSSL
    MD5_CTX ctx;
#endif
};

util_md5ctx_t *util_md5_init ()
{
    util_md5ctx_t *ctx = malloc (sizeof *ctx);
    if (!ctx)
        return NULL;
    
#if ENABLE_GNUTLS
    if (libgnutls_is_present)
    {
        gcry_error_t err;
        ctx->usegnutls = 1;
        err = gcry_md_open (&ctx->h, GCRY_MD_MD5, 0);
        if (!gcry_err_code (err))
            return ctx;
    }
#endif
#if ENABLE_OPENSSL
    if (libopenssl_is_present)
    {
        ctx->usegnutls = 0;
        MD5_Init (&ctx->ctx);
        return ctx;
    }
#endif
    free (ctx);
    return NULL;
}

void util_md5_write (util_md5ctx_t *ctx, char *buf, size_t len)
{
    if (!ctx)
        return;
#if ENABLE_GNUTLS
    if (libgnutls_is_present && ctx->usegnutls)
        gcry_md_write (ctx->h, buf, len);
#endif
#if ENABLE_OPENSSL
    if (libopenssl_is_present && !ctx->usegnutls)
       MD5_Update (&ctx->ctx, buf, len);
#endif
}

int util_md5_final (util_md5ctx_t *ctx, char *buf)
{
    if (!ctx)
        return -1;
#if ENABLE_GNUTLS
    if (libgnutls_is_present && ctx->usegnutls)
    {
        unsigned char *hash;
        gcry_md_final (ctx->h);
        hash = gcry_md_read (ctx->h, 0);
        assert (hash);
        memcpy (buf, hash, 16);
        gcry_md_close (ctx->h);
    }
#endif
#if ENABLE_OPENSSL
    if (libopenssl_is_present && !ctx->usegnutls)
        MD5_Final ((unsigned char *)buf, &ctx->ctx);
#endif
    free (ctx);
    return 0;
}
