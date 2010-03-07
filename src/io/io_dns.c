/*
 * Implements a simple, synchroneous DNS SRV query.
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
 * $Id$
 */


#include "climm.h"
#include "io/io_dns.h"

/* OS X needs a compatibility flag for the resolver */
#define BIND_8_COMPAT 1
#if HAVE_NETINET_IN_H 
#include <netinet/in.h>
#endif
#if HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
#if HAVE_RESOLV_H
#include <resolv.h>
#endif
#ifndef T_SRV
#define T_SRV    33
#endif

typedef struct rrdns_s {
    char *hostname;
    UWORD port;
    UWORD pref;
    UWORD weight;
} rrdns_t;

const char *io_dns_resolve (const char *query)
{
    unsigned char response[1024], *end, *pos;
    static str_s str;
    char name[256];
    rrdns_t rrdns[32];
    int count = 0, i, size, qdcount, ancount;
    UWORD type, len, pref, weight, port;
                    
    size = res_query (query, C_IN, T_SRV, response, sizeof (response));
    
    if (size <= sizeof (HEADER))
        return NULL;

    qdcount = ntohs (((HEADER *)response)->qdcount);
    ancount = ntohs (((HEADER *)response)->ancount);
    
    pos = response + sizeof (HEADER);
    end = response + size;

    /* skip over query */
    while (pos < end && qdcount-- > 0)
    {
        size = dn_expand (response, end, pos, name, 256);
        if (size < 0)
            return NULL;
        pos += size;
        pos += QFIXEDSZ;
    }

    while (pos < end && ancount-- > 0 && count < 32)
    {
        size = dn_expand (response, end, pos, name, 256);
        if (size < 0)
            break;
        pos += size;
        GETSHORT (type, pos);
        pos += 6;
        GETSHORT (len, pos);

        if (type != T_SRV)
        {
            pos += len;
            continue;
        }

        GETSHORT (pref, pos);
        GETSHORT (weight, pos);
        GETSHORT (port, pos);

        size = dn_expand (response, end, pos, name, 256);
        if (size < 0)
            return NULL;
        pos += size;
        
        rrdns[count].hostname = strdup (name);
        rrdns[count].port = port;
        rrdns[count].pref = pref;
        rrdns[count].weight = weight;
        count++;
    }
    if (!count)
        return NULL;
    for (i = 0; i < count - 1; i++)
        if (rrdns[i].pref > rrdns[i+1].pref
            || (rrdns[i].pref == rrdns[i+1].pref && rrdns[i].weight < rrdns[i+1].weight))
        {
            rrdns_t tmp = rrdns[i];
            rrdns[i] = rrdns[i+1];
            rrdns[i+1] = tmp;
        }

    s_init (&str, "", 50);
//    s_catf (&str, ";example.org:1");
    for (i = 0; i < count; i++)
    {
        s_catf (&str, ";%s:%u", rrdns[i].hostname, rrdns[i].port);
        free (rrdns[i].hostname);
    }
    return str.txt + 1;
}
