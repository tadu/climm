/*
 * Implements the XMPP protocol using libiksemel >= 1.2.
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
 * $Id: oscar_base.c 2670 2009-03-01 16:48:03Z kuhlmann $
 */

//#define IKS_TRANS_USER_DATA Server
//#define IKS_FILTER_USER_DATA Server
//#define IKS_SOCK_USER_DATA Connection
#define IKS_TRANS_USER_DATA void
#define IKS_FILTER_USER_DATA void
#define IKS_SOCK_USER_DATA void

#include "climm.h"
#include <sys/types.h>
#include <errno.h>
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
#include <fcntl.h>
#include <assert.h>

#if ENABLE_XMPP

#include <iksemel.h>

#include "xmpp_base.h"
#include "connection.h"
#include "contact.h"
#include "conv.h"
#include "oscar_dc.h"
#include "util_io.h"
#include "im_response.h"
#include "im_request.h"
#include "util_ui.h"
#include "util_rl.h"
#include "buildmark.h"
#include "preferences.h"
#include "oscar_dc_file.h"
#include "io/io_dns.h"
#include "os.h"
#include "util_parse.h"

static jump_conn_f XMPPCallbackDispatch;

static int  iks_climm_TConnect (iksparser *prs, IKS_SOCK_USER_DATA **socketptr, const char *server, int port)
{
    Server *serv = iks_stream_user_data (prs);
    Connection *conn = serv->conn;
    *((Connection **)socketptr) = conn;
    return IKS_OK;
}

static void iks_climm_TClose (IKS_SOCK_USER_DATA *conn)
{
    UtilIOClose ((Connection *)conn);
}

static int iks_climm_TSend (IKS_SOCK_USER_DATA *conn, const char *data, size_t len)
{
    io_err_t rc = UtilIOWrite ((Connection *)conn, data, len);
    if (rc != IO_OK)
        return IKS_NET_RWERR;
    return IKS_OK;
}

static int iks_climm_TRecv (IKS_SOCK_USER_DATA *conn, char *data, size_t len, int timeout)
{
    int rc = UtilIORead ((Connection *)conn, data, len);
    if (rc > 0)
        return rc;
    if (rc == IO_OK || rc == IO_CONNECTED)
        return 0;
    return -1;
}

static const ikstransport iks_climm_transport = {
    IKS_TRANSPORT_V1,
    iks_climm_TConnect,
    iks_climm_TSend,
    iks_climm_TRecv,
    iks_climm_TClose,
    NULL,
};

static void XMPPCallBackTimeout (Event *event)
{
    Connection *conn = event->conn;

    if (!conn)
    {
        EventD (event);
        return;
    }
    assert (conn->serv);
    assert (conn->serv->type == TYPE_XMPP_SERVER);
    if (~conn->connect & CONNECT_OK)
        rl_print ("# XMPP timeout\n");
    EventD (event);
}

void XmppStreamError (Server *serv, const char *text)
{
    rl_printf ("Stream level error occurred: %s [%s]\n", text, UtilIOErr (serv->conn) ? UtilIOErr (serv->conn) : "");
    XMPPLogout (serv);
}

Event *XMPPLogin (Server *serv)
{
    const char *scrserv, *sp;
    char *semi;
    Event *event;
    time_t now = time (NULL);

    assert (serv->type == TYPE_XMPP_SERVER);

    if (!serv->screen || !serv->passwd)
        return NULL;
    if (!strchr (serv->screen, '@'))
    {
        const char *jid = s_sprintf ("%s@jabber.com", serv->screen);
        s_repl (&serv->screen, jid);
    }
    scrserv = strchr (serv->screen, '@') + 1;
    if (!serv->conn->server || !strcmp (scrserv, serv->conn->server))
    {
        const char *rrdns = io_dns_resolve (s_sprintf ("_xmpp-client._tcp.%s", scrserv));
        s_repl (&serv->conn->server, rrdns ? rrdns : scrserv);
    }

    XMPPLogout (serv);

    s_repl (&serv->xmpp_stamp, "YYYYmmddHHMMSS");
    strftime (serv->xmpp_stamp, 15, "%Y%m%d%H%M%S", gmtime (&now));
    s_repl (&serv->xmpp_privacy_list, NULL);
    s_repl (&serv->xmpp_privacy_items, NULL);

    semi = strchr (serv->conn->server, ';');
    if (semi)
        *semi = 0;
    sp = s_sprintf ("%s", s_wordquote (strchr (serv->conn->server, ':') ? serv->conn->server : s_sprintf ("%s:%lu", serv->conn->server, serv->conn->port)));
    rl_printf (i18n (2620, "Opening XMPP connection for %s at %s...\n"),
        s_wordquote (serv->screen), sp);
    if (semi)
        *semi = ';';

    if (!serv->conn->port)
        serv->conn->port = ~0;

    serv->conn->dispatch = &XMPPCallbackDispatch;
    serv->conn->connect = 0;

    if ((event = QueueDequeue2 (serv->conn, QUEUE_DEP_WAITLOGIN, 0, NULL)))
    {
        event->attempts++;
        event->due = time (NULL) + 10 * event->attempts + 10;
        event->callback = &XMPPCallBackTimeout;
        QueueEnqueue (event);
    }
    else
        event = QueueEnqueueData (serv->conn, QUEUE_DEP_WAITLOGIN, 0, time (NULL) + 5,
                                  NULL, serv->conn->cont, NULL, &XMPPCallBackTimeout);

    UtilIOConnectTCP (serv->conn);
    return event;
}

static void XmppSaveLog (IKS_TRANS_USER_DATA *userv, const char *text, size_t size, int in)
{
    Server *serv = (Server *)userv;
    const char *data;
    char *textnonl;
    size_t rc;
    int i;

    data = s_sprintf ("%s%s %s%s ",
        iks_is_secure (serv->xmpp_parser) ? "SSL " :  "", s_now,
        in & 1 ? "<<<" : ">>>", in & 2 ? " residual:" : "");

    if (in)
        DebugH (DEB_XMPPIN, "%s", data);
    else
        DebugH (DEB_XMPPOUT, "%s", data);

    if (!ServerPrefVal (serv, CO_LOGSTREAM))
        return;

    if (serv->logfd < 0)
    {
        const char *dir, *file;
        dir = s_sprintf ("%sdebug", PrefUserDir (prG));
        mkdir (dir, 0700);
        dir = s_sprintf ("%sdebug" _OS_PATHSEPSTR "packets.xmpp.%s", PrefUserDir (prG), serv->screen);
        mkdir (dir, 0700);
        file = s_sprintf ("%sdebug" _OS_PATHSEPSTR "packets.xmpp.%s/%lu", PrefUserDir (prG), serv->screen, time (NULL));
#if !defined(O_SYNC)
#define O_SYNC 0
#endif
        serv->logfd = open (file, O_WRONLY | O_CREAT | O_APPEND | O_SYNC, 0600);
    }
    if (serv->logfd < 0)
        return;

    rc = write (serv->logfd, data, strlen (data));

    textnonl = strdup (text);
    for (i = 0; textnonl[i]; i++)
        if (textnonl[i] == '\n' || textnonl[i] == '\r')
            textnonl[i] = ' ';
    rc = write (serv->logfd, textnonl, strlen (textnonl));
    rc = write (serv->logfd, "\n", 1);
    free (textnonl);
}

static void GetBothContacts (iksid *j, Server *conn, Contact **b, Contact **f, char crea)
{
    Contact *bb, *ff, **t;
    char *jb = j->partial;
    char *jr = j->resource;

    if ((bb = ContactFindScreen (conn->contacts, jb)))
    {
        if (!(ff = ContactFindScreenP (conn->contacts, bb, jr ? jr : "")))
        {
            ff = ContactScreenP (conn, bb, jr ? jr : "");
        }
    }
    else
    {
        bb = ContactScreen (conn, jb);
        if (crea)
            ContactCreate (conn, bb);
        ff = ContactScreenP (conn, bb, jr ? jr : "");
    }
    assert (bb);
    if (ff)
    {
        /* make ff the firstchild or the firstchild->next of bb */
        if (!bb->firstchild)
            bb->firstchild = ff;
        else if (bb->firstchild != ff && bb->firstchild->next != ff)
        {
            for (t = &bb->firstchild; *t; t = &((*t)->next))
                if (*t == ff)
                {
                    *t = ff->next;
                    ff->next = bb->firstchild->next;
                    bb->firstchild->next = ff;
                    t = NULL;
                    break;
                }
            if (t)
            {
                ff->next = bb->firstchild->next;
                bb->firstchild->next = ff;
            }
        }
    }
    else
        ff = bb;
    *b = bb;
    *f = ff;
}

static iks *find_with_ns_attrib (iks *tag, const char *childname, const char *childnamespace)
{
    iks *p, *a;
    for (p = iks_first_tag (tag); p; p = iks_next_tag(p))
    {
        for (a = iks_attrib (p); a; a  = iks_next (a))
        {
            if (iks_type (a) != IKS_ATTRIBUTE)
                continue;
            if (strncmp (iks_name (a), "xmlns", 5))
                continue;
            if (strcmp (iks_cdata  (a), childnamespace))
                continue;
            if (!iks_name (a)[5] || iks_name (a)[5] == ':')
                return p;
        }
    }
    return NULL;
}

#define foreach_subtag(iter,parent,name) \
    iks *iter; \
    for (iter = iks_first_tag (parent); iter; iter = iks_next_tag (iter)) \
        if (iks_type (iter) == IKS_TAG && !strcmp (iks_name (iter), name))


static enum ikshowtype StatusToIksstatus (status_t *status)
{
    switch (*status)
    {
        case ims_online:   return IKS_SHOW_AVAILABLE;   break;
        case ims_ffc:      return IKS_SHOW_CHAT;        break;
        case ims_away:     return IKS_SHOW_AWAY;        break;
        case ims_occ:      *status = ims_dnd;
        case ims_dnd:      return IKS_SHOW_DND;         break;
        case ims_na:       return IKS_SHOW_XA;          break;
        case ims_offline:  *status = ims_inv;
        default:           return IKS_SHOW_UNAVAILABLE; break;
    }
}

/****************** send stuff ******************/

static void XmppSendIqGmail (Server *serv, int64_t newer, const char *newertid, const char *query)
{
    iks *x = iks_new ("iq"), *q;
    iks_insert_attrib (x, "type", "get");
    iks_insert_attrib (x, "to", serv->xmpp_id->partial);
    iks_insert_attrib (x, "id", s_sprintf ("%s-%s-%x", query ? "mailq" : "mail", serv->xmpp_stamp, serv->xmpp_sequence++));

    q = iks_insert (x, "query");
    iks_insert_attrib (q, "xmlns", "google:mail:notify");
    if (newer != 0)
        iks_insert_attrib (q, "newer-than-time", s_sprintf ("%llu", newer));
    if (newertid && *newertid)
        iks_insert_attrib (q, "newer-than-tid", newertid);
    if (query && *query)
        iks_insert_attrib (q, "q", query);
    iks_send (serv->xmpp_parser, x);
    iks_delete (x);
}

static void XmppSendIqTime (Server *serv)
{
    iks *x = iks_new ("iq");
    iks_insert_attrib (x, "type", "get");
    iks_insert_attrib (iks_insert (x, "time"), "xmlns", "urn:xmpp:time");
    iks_insert_attrib (x, "id", s_sprintf ("time-%s-%x", serv->xmpp_stamp, serv->xmpp_sequence++));
    iks_insert_attrib (x, "to", serv->xmpp_id->server);
    iks_send (serv->xmpp_parser, x);
    iks_delete (x);
}

static iks *xmpp_make_iq_privacy (Server *serv, xmpp_priv_t ntype, const char *list)
{
    iks *child, *x, *l = NULL;
    const char *id;
    const char *type = ntype == p_active ? "active" : ntype == p_default ? "default" : ntype == p_show || ntype == p_show_quiet ? "list" : NULL;
    id = s_sprintf ("priv-%d-%s-%x", ntype, serv->xmpp_stamp, ++serv->xmpp_sequence);
    x = iks_new ("iq");
    iks_insert_attrib (x, "type", ntype == p_active || ntype == p_default ? "set" : "get");
    iks_insert_attrib (x, "id", id);
    iks_insert_attrib (x, "from", serv->xmpp_id->full);
    child = iks_insert (x, "query");
    iks_insert_attrib (child, "xmlns", "jabber:iq:privacy");
    if (type)
        l = iks_insert (child, type);
    if (l && list)
        iks_insert_attrib (l, "name", list);
    return x;
}

static void XMPPSendIqPrivacy (Server *serv, xmpp_priv_t ntype, const char *list)
{
    iks *x = xmpp_make_iq_privacy (serv, ntype, list);
    iks_send (serv->xmpp_parser, x);
    iks_delete (x);
}

static int xmpp_uint_compare (const char *a, const char *b)
{
    if (*a == '+')
        a++;
    if (*b == '+')
        b++;
    while (*a == '0')
        a++;
    while (*b == '0')
        b++;
    if (strlen (a) < strlen (b))
        return -1;
    if (strlen (a) > strlen (b))
        return 1;
    while (*a && *b)
    {
        if (*a < *b)
            return -1;
        if (*a > *b)
            return -1;
        a++;
        b++;
    }
    return 0;
}

static char *xmpp_uint_add (const char *a, const char *b)
{
    int la = strlen (a), lb = strlen (b), lo = la + lb + 1;
    char *op = malloc (la + lb + 2), *ret;
    int o = 0;
    op[lo] = 0;
    
    while (la && lb)
    {
        la--;
        lb--;
        o += (a[la] - '0') + (b[lb] - '0');
        op[--lo] = (o % 10) + '0';
        o /= 10;
    }
    if (lb)
    {
        la = lb;
        a = b;
    }
    while (la)
    {
        la--;
        o += (a[la] - '0');
        op[--lo] = (o % 10) + '0';
        o /= 10;
    }
    if (o)
        op[--lo] = o + '0';
    ret = strdup (op + lo);
    free (op);
    return ret;
}

static void XMPPSendIqPrivacyEdit (Server *serv, xmpp_priv_t ntype, const char *list, const char *edit)
{
    iks *child, *x, *l = NULL, *old = NULL, *xx;
    UDWORD ig = 0;
    UDWORD count = 0;
    const char *insertiontstr, *id;
    char *insertionstr;
    int allow;

    id = s_sprintf ("priv-%d-%s-%x", ntype, serv->xmpp_stamp, ++serv->xmpp_sequence);
    x = iks_new ("iq");
    iks_insert_attrib (x, "type", "set");
    iks_insert_attrib (x, "id", id);
    iks_insert_attrib (x, "from", serv->xmpp_id->full);
    child = iks_insert (x, "query");
    iks_insert_attrib (child, "xmlns", "jabber:iq:privacy");
    l = iks_insert (child, "list");
    iks_insert_attrib (l, "name", list);
    
    xx = xmpp_make_iq_privacy (serv, p_show, list);

    if (ntype == p_edit)
    {
        insertiontstr = s_parseint_s (&edit, &ig, DEFAULT_SEP, 0);
        if (!insertiontstr || *insertiontstr == 'x' || *insertiontstr == '-')
            insertiontstr = "0";
        insertionstr = strdup (insertiontstr);
        old = iks_tree (serv->xmpp_privacy_items, 0, NULL);
        { foreach_subtag (item_i, old, "item")
        {
            const char *iorder  = iks_find_attrib (item_i, "order");
            if (iorder && xmpp_uint_compare (iorder, insertionstr) < 0)
                iks_insert_node (l, iks_copy_within (item_i, iks_stack (l)));
        }}
    }
    
    while ((s_parsekey (&edit, "allow") && (allow=1)) || (s_parsekey (&edit, "deny") && (allow=2)))
    {
        strc_t par;
        iks *ni = iks_insert (l, "item");
        char *newsum = xmpp_uint_add (insertionstr, s_sprintf ("%lu", count));
        iks_insert_attrib (ni, "order", newsum);
        free (newsum);
        iks_insert_attrib (ni, "action", allow == 1 ? "allow" : "deny");
        if (!s_parsekey (&edit, "all"))
            while (1)
            {
                if (s_parsekey_s (&edit, "msg", MULTI_SEP))
                    iks_insert (ni, "message");
                else if (s_parsekey_s (&edit, "pin", MULTI_SEP))
                    iks_insert (ni, "presence-in");
                else if (s_parsekey_s (&edit, "pout", MULTI_SEP))
                    iks_insert (ni, "presence-out");
                else if (s_parsekey_s (&edit, "iq", MULTI_SEP))
                    iks_insert (ni, "iq");
                else
                    break;
            }
        if (s_parsekey (&edit, "subscription") || s_parsekey (&edit, "sub"))
        {
            iks_insert_attrib (ni, "type", "subscription");
            if (s_parsekey (&edit, "both"))
                iks_insert_attrib (ni, "value", "both");
            else if (s_parsekey (&edit, "to"))
                iks_insert_attrib (ni, "value", "to");
            else if (s_parsekey (&edit, "from"))
                iks_insert_attrib (ni, "value", "from");
            else {
                s_parsekey (&edit, "none");
                iks_insert_attrib (ni, "value", "none");
            }
        }
        else if (s_parsekey (&edit, "group") && (par = s_parse (&edit)))
        {
            iks_insert_attrib (ni, "type", "group");
            iks_insert_attrib (ni, "value", par->txt);
        }
        else if ((s_parsekey (&edit, "jid") || 1) && (par = s_parse (&edit)))
        {
            iks_insert_attrib (ni, "type", "jid");
            iks_insert_attrib (ni, "value", par->txt);
        }
        count++;
    }
    
    if (ntype == p_edit)
    {
        char *shift, *skipstr;
        const char *skiptstr;
        if (s_parsekey (&edit, "delete") && (skiptstr = s_parseint_s (&edit, &ig, DEFAULT_SEP, 0)))
        {
            char *ninsertionstr = xmpp_uint_add (insertionstr, skiptstr);
            free (insertionstr);
            insertionstr = ninsertionstr;
            skipstr = strdup (skiptstr);
        }
        else
            skipstr = strdup ("0");
        shift = xmpp_uint_add (skipstr, s_sprintf ("%lu", count));
        { foreach_subtag (item_i, old, "item")
        {
            const char *iorder  = iks_find_attrib (item_i, "order");
            if (iorder && xmpp_uint_compare (iorder, insertionstr) >= 0)
            {
                iks *node = iks_copy_within (item_i, iks_stack (l));
                char *newsum = xmpp_uint_add (iorder, shift);
                iks_insert_attrib (node, "order", newsum);
                free (newsum);
                iks_insert_node (l, node);
            }
        }}
        free (shift);
        free (skipstr);
    }
    free (insertionstr);

    iks_send (serv->xmpp_parser, x);
    iks_delete (x);

    iks_send (serv->xmpp_parser, xx);
    iks_delete (xx);
}

static void sendIqGmailReqs (Event *event)
{
    Server *serv;
    if (!event->conn)
    {
        EventD (event);
        return;
    }
    serv = event->conn->serv;
    XmppSendIqGmail (serv, serv->xmpp_gmail_new_newer, serv->xmpp_gmail_new_newertid, NULL);
    event->due += 300;
    QueueEnqueue (event);
}

static void sendIqTimeReqs (Event *event)
{
    if (!event->conn)
    {
        EventD (event);
        return;
    }
    XmppSendIqTime (event->conn->serv);
    event->due += 300;
    QueueEnqueue (event);
}

/****************** IqHandler **********/

static int XmppHandleIqGmail (IKS_FILTER_USER_DATA *fserv, ikspak *pak)
{
    Server *serv = (Server *)fserv;
    Contact *cont = serv->conn->cont;
    iks *mb = find_with_ns_attrib (pak->x, "mailbox", "google:mail:notify");
    char *n_s, *foundnewer;
    const char *ntid = "";
    int n = 0, ismail = 0;

    if (!mb)
        return IKS_FILTER_PASS;

    n_s = iks_find_attrib (mb, "total-matched");
    if (n_s)
        n = atoi (n_s);

    if (pak->id && !strncmp (pak->id, "mail-", 5))
    {
        ismail = 1;
        if (n || !strcmp (pak->id + strlen (pak->id) - 2, "-0"))
            rl_printf (i18n (2738, "Found %d new mails for %s.\n"), n, serv->xmpp_id->partial);
    }
    else
        rl_printf (i18n (2739, "Found %d mails for %s.\n"), n, serv->xmpp_id->partial);
    if (!n)
        return IKS_FILTER_EAT;

    {
    foreach_subtag (mb_c, mb, "mail-thread-info")
    {
        char *sub = iks_find_cdata (mb_c, "subject");
        char *snip = iks_find_cdata (mb_c, "snippet");
        char *dato = iks_find_attrib (mb_c, "date");
        time_t t = atoll (dato) / 1000ULL;
        ntid = iks_find_attrib (mb_c, "tid");
        rl_printf ("%s ", s_time (&t));
        rl_printf ("%s%s %s%s%s", COLMESSAGE, sub ? sub : "", COLQUOTE, COLSINGLE, snip ? snip : "");
        rl_print ("\n");
        {
        foreach_subtag (mb_s_c, iks_find (mb_c, "senders"), "sender")
        {
            if (!ismail || (iks_find_attrib (mb_s_c, "unread") && !strcmp (iks_find_attrib (mb_s_c, "unread"), "1")))
            {
                char *email = iks_find_attrib (mb_s_c, "address");
                char *name = iks_find_attrib (mb_s_c, "name");
                rl_printf ("            %s%s%s <%s%s%s>\n", COLQUOTE, name ? name : "", COLNONE, COLCONTACT, email ? email : "", COLNONE);
            }
        }}
    }}
    foundnewer = iks_find_attrib (mb, "result-time");
    if (ismail)
    {
        serv->xmpp_gmail_new_newer = atoll (foundnewer);
        s_repl (&serv->xmpp_gmail_new_newertid, ntid);
    }
    else
    {
        serv->xmpp_gmail_newer = atoll (foundnewer);
        s_repl (&serv->xmpp_gmail_newertid, ntid);
    }
    return IKS_FILTER_EAT;
}

static int XmppHandleIqDisco (IKS_FILTER_USER_DATA *fserv, ikspak *pak)
{
    Server *serv = (Server *)fserv;
    if (pak->subtype == IKS_TYPE_RESULT)
    {
        /*  what did we ask for? */
        if (!pak->from || pak->from->resource || pak->from->user || strcmp (pak->from->server, serv->xmpp_id->server))
           return IKS_FILTER_PASS;

        /* disco is from server */
        if (iks_find_with_attrib (iks_find (pak->x, "query"), "feature", "var", "google:mail:notify"))
        {
            /* have Gmail, so start requesting */
            XmppSendIqGmail (serv, 0, NULL, NULL);
            QueueEnqueueData2 (serv->conn, QUEUE_XMPP_GMAIL, 0, 300, NULL, &sendIqGmailReqs, NULL);
            iks_filter_add_rule (serv->xmpp_filter, XmppHandleIqGmail , serv, IKS_RULE_TYPE, IKS_PAK_IQ, IKS_RULE_SUBTYPE, IKS_TYPE_RESULT, IKS_RULE_NS, "google:mail:notify", IKS_RULE_DONE);
            return IKS_FILTER_EAT;
        }
    }
    else if (pak->subtype == IKS_TYPE_GET)
    {
        iks *x = iks_new ("iq"), *q, *f, *i;
        iks_insert_attrib (x, "type", "result");
        iks_insert_attrib (x, "id", pak->id);
        iks_insert_attrib (x, "to", pak->from->full);
        q = iks_insert (x, "query");
        iks_insert_attrib (q, "xmlns", "http://jabber.org/protocol/disco#info");
        iks_insert_attrib (q, "node", iks_find_attrib (pak->query, "node"));
        f = iks_insert (q, "feature");
        iks_insert_attrib (f, "var", "http://jabber.org/protocol/disco#info");
        f = iks_insert (q, "feature");
        iks_insert_attrib (f, "var", "http://jabber.org/protocol/chatstates");
        f = iks_insert (q, "feature");
        iks_insert_attrib (f, "var", "jabber:iq:version");
        f = iks_insert (q, "feature");
        iks_insert_attrib (f, "var", "jabber:iq:last");
        i =  iks_insert (q, "identity");
        iks_insert_attrib (i, "category", "client");
        iks_insert_attrib (i, "type", "console");
        iks_insert_attrib (i, "name", "climm");
        iks_send (serv->xmpp_parser, x);
        iks_delete (x);
        return IKS_FILTER_EAT;
    }

    return IKS_FILTER_PASS;
}

static int XmppHandleIqXEP12 (IKS_FILTER_USER_DATA *fserv, ikspak *pak)
{
    Server *serv = (Server *)fserv;
    if (pak->subtype == IKS_TYPE_GET)
    {
        iks *x = iks_new ("iq"), *q;
        iks_insert_attrib (x, "type", "result");
        iks_insert_attrib (x, "id", pak->id);
        iks_insert_attrib (x, "to", pak->from->full);
        q = iks_insert (x, "query");
        iks_insert_attrib (q, "xmlns", "jabber:iq:last");
        iks_insert_attrib (q, "seconds", s_sprintf ("%ld", uiG.idle_val ? os_DetermineIdleTime (time (NULL), uiG.idle_val) : 0));
        iks_send (serv->xmpp_parser, x);
        iks_delete (x);
        return IKS_FILTER_EAT;
    }
    return IKS_FILTER_PASS;
}

static int XmppHandleIqXEP92 (IKS_FILTER_USER_DATA *fserv, ikspak *pak)
{
    Server *serv = (Server *)fserv;
    if (pak->subtype == IKS_TYPE_GET)
    {
        iks *x = iks_new ("iq"), *q;
        iks_insert_attrib (x, "type", "result");
        iks_insert_attrib (x, "id", pak->id);
        iks_insert_attrib (x, "to", pak->from->full);
        q = iks_insert (x, "query");
        iks_insert_attrib (q, "xmlns", "jabber:iq:version");
        iks_insert_cdata (iks_insert (q, "name"), "climm", 0);
        iks_insert_cdata (iks_insert (q, "version"), BuildVersionStr, 0);
        iks_insert_cdata (iks_insert (q, "os"), BuildPlatformStr, 0);
        iks_send (serv->xmpp_parser, x);
        iks_delete (x);
        return IKS_FILTER_EAT;
    }
    return IKS_FILTER_PASS;
}

static int XmppHandleIqRoster (IKS_FILTER_USER_DATA *fserv, ikspak *pak)
{
    Server *serv = (Server *)fserv;
    ContactGroup *cg;
    Contact *contr, *contb;
    int i;
    
    if (pak->subtype == IKS_TYPE_RESULT)
    {
        Contact *cont;
        for (i = 0, cg = serv->contacts; (cont = ContactIndex (cg, i)); i++)
        {
            OptSetVal(&cont->copts, CO_ISSBL, 0);
            OptSetVal(&cont->copts, CO_TO_SBL, 0);
            OptSetVal(&cont->copts, CO_FROM_SBL, 0);
            OptSetVal(&cont->copts, CO_ASK_SBL, 0);
        }
    }
    { foreach_subtag(item_c, pak->query, "item")
    {
        char *jid = iks_find_attrib (item_c, "jid");
        char *subs = iks_find_attrib (item_c, "subscription");
        char *name = iks_find_attrib (item_c, "name");
        char *ask = iks_find_attrib (item_c, "ask");
        iksid *jjid;
        
        if (!jid || !subs)
            continue;
        
        jjid = iks_id_new (iks_stack (pak->x), jid);
        GetBothContacts (jjid, serv, &contb, &contr, 1);
            
        OptSetVal(&contb->copts, CO_ISSBL, 1);
        OptSetVal(&contb->copts, CO_TO_SBL, !strcmp (subs, "both") || !strcmp (subs, "to"));
        OptSetVal(&contb->copts, CO_FROM_SBL, !strcmp (subs, "both") || !strcmp (subs, "from"));
        OptSetVal(&contb->copts, CO_ASK_SBL, ask && !strcmp (ask, "subscribe"));
        
        if (name && !ContactFindScreen (serv->contacts, name))
            ContactAddAlias (contb, name);
        
        { foreach_subtag(group, item_c, "group")
        {
            char *g = iks_cdata (group);
            if (g && *g)
            {
                cg = ContactGroupFind (serv, 0, g);
                if (!cg)
                    cg = ContactGroupC (serv, 0, g);
                if (!ContactHas (cg, contb))
                    ContactAdd (cg, contb);
            }
        }}
    }}
    return IKS_FILTER_EAT;
}

static int XmppHandleIqPrivacy (IKS_FILTER_USER_DATA *fserv, ikspak *pak)
{
    Server *serv = (Server *)fserv;
    iks *c;
    const char *name = NULL;
    int type = 0;
    
    if (strlen (pak->id) > 5 && pak->id[5] >= '1' && pak->id[5] <= '8')
        type = pak->id[5] - '0';
    
    if (pak->subtype == IKS_TYPE_RESULT && type)
    {
        if (type == p_list || type == p_list_quiet)
        {
            const char *active = NULL, *def = NULL;
            int items = 0;
            if ((c = iks_find (pak->query, "active")))
                active = iks_find_attrib (c, "name");
            if ((c = iks_find (pak->query, "default")))
                def    = iks_find_attrib (c, "name");
            s_repl (&serv->xmpp_privacy_list, NULL);
            s_repl (&serv->xmpp_privacy_items, NULL);
            if (type == p_list_quiet)
                return IKS_FILTER_EAT;
            if (active)
                rl_printf (i18n (2761, "Active privacy list: %s.\n"), s_wordquote (active));
            else
                rl_printf (i18n (2762, "No active privacy list.\n"));
            if (def)
                rl_printf (i18n (2763, "Default privacy list: %s.\n"), s_wordquote (def));
            else
                rl_printf (i18n (2764, "No default privacy list.\n"));
            rl_print (i18n (2765, "Available privacy lists: "));
            { foreach_subtag(item_c, pak->query, "list")
            {
                if (!(name = iks_find_attrib (item_c, "name")))
                    continue;
                rl_printf (items++ ? ", %s" : "%s", s_wordquote (name));
            }}
            rl_printf (".\n");
            return IKS_FILTER_EAT;
        }
        else if (type == p_show || type == p_show_quiet)
        {
            { foreach_subtag(item_c, pak->query, "list")
            {
                if (!(name = iks_find_attrib (item_c, "name")))
                    continue;
                s_repl (&serv->xmpp_privacy_list, name);
                s_repl (&serv->xmpp_privacy_items, iks_string (iks_stack (item_c), item_c));
                if (type == p_show_quiet)
                    return IKS_FILTER_EAT;
                rl_printf (i18n (2766, "Listing privacy list: %s.\n"), s_wordquote (name));
                { foreach_subtag(item_i, item_c, "item")
                {
                    const char *itype   = iks_find_attrib (item_i, "type");
                    const char *ivalue  = iks_find_attrib (item_i, "value");
                    const char *iaction = iks_find_attrib (item_i, "action");
                    const char *iorder  = iks_find_attrib (item_i, "order");
                    iks *b_ms = iks_find (item_i, "message");
                    iks *b_pi = iks_find (item_i, "presence-in");
                    iks *b_po = iks_find (item_i, "presence-out");
                    iks *b_iq = iks_find (item_i, "iq");
                    const char *affects;
                    if (b_ms || b_pi || b_po || b_iq)
                    {
                        affects = s_sprintf ("%s%s%s%s", b_ms ? "msg," : "", b_pi ? "pi" : "", b_po ? "po," : "", b_iq ? "iq," : "");
                        affects = s_sprintf ("%*s", strlen (affects) - 1, affects);
                    }
                    else
                        affects = "all";
                    
                    rl_printf ("%-4s %-5s %-5s %-5s %s\n", iorder ? iorder : 0, iaction ? iaction : "", affects, itype ? itype : "", ivalue ? ivalue : "");
                }}
            }}
            return IKS_FILTER_EAT;
        }
        else if (type == p_active || type == p_default)
        {
            s_repl (&serv->xmpp_privacy_list, NULL);
            s_repl (&serv->xmpp_privacy_items, NULL);
            XMPPPrivacy (serv, p_list, NULL, NULL);
            return IKS_FILTER_EAT;
        }
    }
    else if (pak->subtype == IKS_TYPE_SET)
    {
        if ((c = iks_find (pak->query, "list")))
            name = iks_find_attrib (c, "name");
        s_repl (&serv->xmpp_privacy_list, NULL);
        s_repl (&serv->xmpp_privacy_items, NULL);
        rl_printf (i18n (2767, "Server updates privacy list %s.\n"), s_wordquote (name));
        return IKS_FILTER_EAT;
    }
    return 0;
}

static int XmppHandleIqTime (IKS_FILTER_USER_DATA *fserv, ikspak *pak)
{
    return IKS_FILTER_EAT;
}

static int XmppHandleIqDefault (IKS_FILTER_USER_DATA *fserv, ikspak *pak)
{
    Server *serv = (Server *)fserv;
    iks *x, *e, *t, *c;
    x = iks_new ("iq");
    iks_insert_attrib (x, "type", "error");
    iks_insert_attrib (x, "id", pak->id);
    e = iks_insert (x, "error");
    iks_insert_attrib  (e, "type", "cancel");
    t = iks_insert (e, "feature-not-implemented");
    iks_insert_attrib  (t, "xmlns", "urn:ietf:params:xml:ns:xmpp-stanzas");
    c = iks_tree (iks_string (iks_stack (x), iks_first_tag (x)), 0, NULL);
    iks_insert_node (x, c);
    iks_send (serv->xmpp_parser, x);
    iks_delete (c);
    iks_delete (x);
    return IKS_FILTER_PASS;
}

/*********** other handlers *************/

static int __SkipChar (const char **s, char c)
{
    if (**s == c && **s)
        (*s)++;
    if (**s)
        return *((*s)++) - '0';
    return 0;
}

static time_t ParseUTCDate (const char *s)
{
   struct tm tm;
   tm.tm_year =  __SkipChar (&s, 0) * 1000;
   tm.tm_year += __SkipChar (&s, 0) * 100;
   tm.tm_year += __SkipChar (&s, 0) * 10;
   tm.tm_year += __SkipChar (&s, 0) - 1900;
   tm.tm_mon  =  __SkipChar (&s, '-') * 10;
   tm.tm_mon  += __SkipChar (&s, 0) - 1;
   tm.tm_mday =  __SkipChar (&s, '-') * 10;
   tm.tm_mday += __SkipChar (&s, 0);
   tm.tm_hour =  __SkipChar (&s, 'T') * 10;
   tm.tm_hour += __SkipChar (&s, 0);
   tm.tm_min  =  __SkipChar (&s, ':') * 10;
   tm.tm_min  += __SkipChar (&s, 0);
   tm.tm_sec  =  __SkipChar (&s, ':') * 10;
   if (!*s)
       return -1;
   tm.tm_sec  += __SkipChar (&s, 0);
   if (*s)
       return -1;
    return timegm (&tm);
}

static time_t XmppHandleXEP91 (iks *x)
{
    time_t date = NOW;
    iks *delay;
    if ((delay = find_with_ns_attrib (x, "x", "jabber:x:delay")))
    {
        struct tm;
        char *stamp;
        // char *dfrom = 
        iks_find_attrib (delay, "from");
        stamp = iks_find_attrib (delay, "stamp");
        date = ParseUTCDate (stamp);
//        if (date != NOW)
//            DropAttrib (delay, "stamp");
//        DropAttrib (delay, "from");
//        CheckInvalid (delay);
    }
    return date;
}

static void XmppHandleXEP115 (iks *x, Contact *contr)
{
    iks *caps;
    if ((caps = find_with_ns_attrib (x, "c", "http://jabber.org/protocol/caps")))
    {
        char *node = iks_find_attrib (caps, "node");
        char *ver = iks_find_attrib (caps, "ver");
        char *ext = iks_find_attrib (caps, "ext");
        if (!strcmp (node, "http://www.climm.org/xmpp/caps"))
            node = "climm";
        else if (!strcmp (node, "http://mail.google.com/xmpp/client/caps"))
            node = "GoogleMail";
        else if (!strcmp (node, "http://www.google.com/xmpp/client/caps"))
            node = "GoogleTalk";
        else if (!strcmp (node, "http://www.android.com/gtalk/client/caps"))
            node = "Android";
        else if (!strcmp (node, "http://pidgin.im/caps"))
            node = "Pidgin";
        else if (!strcmp (node, "http://gaim.sf.net/caps"))
            node = "Gaim";
        else if (!strcmp (node, "http://kopete.kde.org/jabber/caps"))
            node = "Kopete";
        else if (!strcmp (node, "http://psi-im.org/caps"))
            node = "Psi";
        else if (!strcmp (node, "http://miranda-im.org/caps"))
            node = "Miranda";
        else if (!strcmp (node, "apple:ichat:caps") || !strcmp (node, "http://www.apple.com/ichat/caps"))
            node = "iChat";
        else if (!strcmp (node, "http://telepathy.freedesktop.org/caps"))
            node = "Telepathy";
        else if (!strcmp (node, "http://talkgadget.google.com/client/caps"))
            node = "TalkGadget";
        else if (!strcmp (node, "http://trillian.im/caps"))
            node = "Trillian";
        s_repl (&contr->cap_string, ext);
        s_repl (&contr->version, s_sprintf ("%s %s", node, ver));
//        DropAttrib (caps, "ver");
//        DropAttrib (caps, "ext");
//        DropAttrib (caps, "node");
//        CheckInvalid (caps);
    }
}

static void XmppHandleXEP22a (Server *serv, iks *x, Contact *cfrom)
{
    char *refid;
    int ref = -1;
    int_msg_t type;
    iks *ch;

    if ((refid = iks_find_cdata (x, "id")))
    {
//        DropCData (tid);
//        CheckInvalid (tid);
    }
    if (refid && !strncmp (refid, "xmpp-", 5) && !strncmp (refid + 5, serv->xmpp_stamp, 14)
        && strlen (refid) > 19 && refid[19] == '-')
        sscanf (refid + 20, "%x", &ref);

    if ((ch =  iks_find (x, "offline")))
    {
        type = INT_MSGOFF;
//        CheckInvalid (dotag);
    }
    else if ((ch = iks_find (x, "paused")))
    {
//        CheckInvalid (dotag);
        IMIntMsg (cfrom, NOW, ims_offline, INT_MSGNOCOMP, "");
        ref = -1;
    }
    else if ((ch = iks_find (x, "delivered")))
    {
        type = INT_MSGACK_TYPE2;
//        CheckInvalid (dotag);
    }
    else if ((ch = iks_find (x, "displayed")))
    {
        type = INT_MSGDISPL;
//        CheckInvalid (dotag);
    }
    else if ((ch = iks_find (x, "composing")))
    {
//        CheckInvalid (dotag);
        IMIntMsg (cfrom, NOW, ims_offline, INT_MSGCOMP, "");
        ref = -1;
    }
    else
        ref = -1;

    if (ref != -1)
    {
        Event *event = QueueDequeue (serv->conn, QUEUE_XMPP_RESEND_ACK, ref);
        if (event)
        {
            Message *msg = (Message *)event->data;
            assert (msg);
            if (msg->send_message && !msg->otrinjected)
            {
                msg->type = type;
                IMIntMsgMsg (msg, NOW, ims_offline);
            }
            event->attempts += 5;
            QueueEnqueue (event);
        }
    }
//    CheckInvalid (XEP22);
}

static void XmppHandleXEP22c (Server *serv, iksid *from, char *tof, char *id, char *type)
{
    iks *msg  = iks_make_msg (IKS_TYPE_NONE, from->full, NULL), *x;
    iks_insert_attrib (msg, "id", s_sprintf ("ack-%s-%x", serv->xmpp_stamp, ++serv->xmpp_sequence));
    x = iks_insert (msg, "x");
    iks_insert_attrib (x, "xmlns", "jabber:x:event");
    iks_insert (x, type);
    iks_insert_cdata (iks_insert (x, "id"), id, 0);
    iks_send (serv->xmpp_parser, msg);
    iks_delete (msg);
}

static void XmppHandleXEP22b (Server *serv, iks *x, iksid *from, char *tof, char *id)
{
    iks *ch;
    if ((ch = iks_find (x, "delivered")))
    {
        XmppHandleXEP22c (serv, from, tof, id, "delivered");
//        CheckInvalid (dotag);
    }
    if ((ch = iks_find (x, "displayed")))
    {
        XmppHandleXEP22c (serv, from, tof, id, "displayed");
//        CheckInvalid (dotag);
    }
    if ((ch = iks_find (x, "composing")))
    {
//        CheckInvalid (dotag);
    }
    if ((ch = iks_find (x, "offline")))
    {
//        CheckInvalid (dotag);
    }
}

static char XmppHandleXEP22 (Server *serv, iks *t, Contact *cfrom, iksid *from, char *tof, char *id)
{
    char ret = 0;
    iks *ch;
    if ((ch = find_with_ns_attrib (t, "x", "jabber:x:event")))
    {
        ret = 1;
        if (!iks_find (t, "body"))
        {
            XmppHandleXEP22a (serv, ch, cfrom);
            return 1;
        }
        else
            XmppHandleXEP22b (serv, ch, from, tof, id);
    }
    return 0;
}

static char XmppHandleXEP85 (Server *serv, iks *t, Contact *cfrom, iksid *from, char *tof, char *id)
{
    char ret = 0;
    iks *ch;
    if ((ch = find_with_ns_attrib (t, "addresses", "http://jabber.org/protocol/address")))
    {
//        DropAllChildsTree (address, "address");
//        CheckInvalid (address);
        ret = 1;
    }

    if ((ch = find_with_ns_attrib (t, "active", "http://jabber.org/protocol/chatstates")))
    {
//        CheckInvalid (active);
        ret = 1;
    }
    if ((ch = find_with_ns_attrib (t, "composing", "http://jabber.org/protocol/chatstates")))
    {
//        CheckInvalid (composing);
        IMIntMsg (cfrom, NOW, ims_offline, INT_MSGCOMP, "");
        ret = 1;
    }
    if ((ch = find_with_ns_attrib (t, "paused", "http://jabber.org/protocol/chatstates")))
    {
//        CheckInvalid (paused);
        IMIntMsg (cfrom, NOW, ims_offline, INT_MSGNOCOMP, "");
        ret = 1;
    }
    if ((ch = find_with_ns_attrib (t, "inactive", "http://jabber.org/protocol/chatstates")))
    {
//        CheckInvalid (inactive);
        ret = 1;
    }
    if ((ch = find_with_ns_attrib (t, "gone", "http://jabber.org/protocol/chatstates")))
    {
//        CheckInvalid (gone);
        ret = 1;
    }
    if (ret && !iks_find (t, "body"))
        return 1;
    return 0;
}

static int XmppHandleMessage (IKS_FILTER_USER_DATA *fserv, ikspak *pak)
{
    Server *serv = (Server *)fserv;
//    char *toX = iks_find_attrib (pak->x, "to");
//    iksid *to = iks_id_new (iks_stack (pak->x), toX);

//    Contact *cto = ContactScreen (serv, pak->tof);
//    std::string subtypeval = t->findAttribute ("type");
//    std::string body = t->body();
//    std::string subject = t->subject();
//    std::string html;
//    gloox::Tag *htmltag = find_with_ns_attrib (t, "html", "http://jabber.org/protocol/xhtml-im");
//    if (htmltag)
//        htmltag = find_with_ns_attrib (htmltag, "body", "http://www.w3.org/1999/xhtml");
//    if (htmltag)
//        html = htmltag->cdata();
//    DropAttrib (t, "type");
    time_t delay = -1;
    Contact *contb, *contr;
    Opt *opt;

    GetBothContacts (pak->from, serv, &contb, &contr, 0);

//    DropAllChilds (t, "subject");
//    DropAllChilds (t, "thread");
//    handleXEP71 (t);
//    handleGoogleNosave (t);
//    handleGoogleSig (t);
//    handleGoogleChatstate (t);
//    handleXEP136 (t);
    delay = XmppHandleXEP91 (pak->x);
    XmppHandleXEP115 (pak->x, contr); // entity capabilities (used also for client version)
    if (XmppHandleXEP22 (serv, pak->x, contr, pak->from, iks_find_attrib (pak->x, "to"), pak->id))
        return IKS_FILTER_EAT;
    if (XmppHandleXEP85 (serv, pak->x, contr, pak->from, iks_find_attrib (pak->x, "to"), pak->id))
        return IKS_FILTER_EAT;
//    DropAllChilds (t, "body");

    opt = OptSetVals (NULL, CO_ORIGIN, CV_ORIGIN_v8, CO_MSGTYPE, MSG_NORM, CO_MSGTEXT, iks_find_cdata (pak->x, "body"), 0);

//    if (!subject.empty())
//        opt = OptSetVals (opt, CO_MSGTYPE, MSG_NORM_SUBJ, CO_SUBJECT, subject.c_str(), 0);
//    if (!strcmp (html.c_str(), body.c_str()))
//        opt = OptSetVals (opt, CO_SAMEHTML, 1);
    IMSrvMsgFat (contr, delay, opt);

//    if (gloox::Tag *x = t->findChild ("x"))
//        CheckInvalid (x);
    return IKS_FILTER_EAT;
}


static int XmppHandlePresenceErr (IKS_FILTER_USER_DATA *fserv, ikspak *pak)
{
    Contact *contb, *contr;

    Server *serv = (Server *)fserv;
    GetBothContacts (pak->from, serv, &contb, &contr, 1);
    IMOffline (contr);
    return IKS_FILTER_EAT;
}

/*
 * React to subscription requests.
 */
static int XmppHandleSubscription (IKS_FILTER_USER_DATA *fserv, ikspak *pak)
{
    Server *serv = (Server *)fserv;
    Contact *contb, *contr;
    GetBothContacts (pak->from, serv, &contb, &contr, 1);

    if (pak->subtype == IKS_TYPE_SUBSCRIBE)
        IMSrvMsg (contr, NOW, CV_ORIGIN_v8, MSG_AUTH_REQ, NULL);
    else if (pak->subtype == IKS_TYPE_SUBSCRIBED)
        IMSrvMsg (contr, NOW, CV_ORIGIN_v8, MSG_AUTH_GRANT, NULL);
    else if (pak->subtype == IKS_TYPE_UNSUBSCRIBE)
        IMSrvMsg (contr, NOW, CV_ORIGIN_v8, MSG_AUTH_DENY, NULL);
    else if (pak->subtype == IKS_TYPE_UNSUBSCRIBED)
        IMSrvMsg (contr, NOW, CV_ORIGIN_v8, MSG_AUTH_DONE, NULL);
    else
        return IKS_FILTER_PASS;
    return IKS_FILTER_EAT;
}

static int XmppHandlePresence (IKS_FILTER_USER_DATA *fserv, ikspak *pak)
{
    Server *serv = (Server *)fserv;
//    ContactGroup *tcg;
    Contact *contb, *contr; // , *c;
    status_t status;
//    std::string pri;
    time_t delay;

    GetBothContacts (pak->from, serv, &contb, &contr, 1);

//    if (gloox::Tag *priority = s->findChild ("priority"))
//    {
//        pri = priority->cdata();
//        DropCData (priority);
//        CheckInvalid (priority);
//    }

    delay = XmppHandleXEP91 (pak->x);
    // FIXME: do something with it!

    XmppHandleXEP115 (pak->x, contr); // entity capabilities (used also for client version)
//    handleXEP153 (s, contb); // vcard-based avatar, nickname
//    handleXEP27 (s);         // OpenPGP signature (obsolete)
//    handleXEP8 (s);          // iq-based avatar (obsolete)

    if (pak->subtype == IKS_TYPE_UNAVAILABLE)
    {
        status = ims_offline;
//        DropAttrib (s, "type");
        IMOffline (contr);
        return IKS_FILTER_EAT;
    }
    else if (pak->show == IKS_SHOW_CHAT)      status = ims_ffc;
    else if (pak->show == IKS_SHOW_AWAY)      status = ims_away;
    else if (pak->show == IKS_SHOW_DND)       status = ims_dnd;
    else if (pak->show == IKS_SHOW_XA)        status = ims_na;
    else if (pak->show == IKS_SHOW_AVAILABLE) status = ims_online;
    else assert (0);

    if (!ContactPrefVal (contb, CO_TO_SBL))
        XMPPAuthorize (serv, contb, auth_req, "auto re-sync");

    IMOnline (contr, status, imf_none, pak->show, iks_find_cdata (pak->x, "status"));
    return IKS_FILTER_EAT;
}


static int XmppUnknown (IKS_FILTER_USER_DATA *fserv, ikspak *pak)
{
    Server *serv = (Server *)fserv;
    XmppSaveLog (serv, iks_string (iks_stack (pak->x), pak->x), 0, 2);
    return IKS_FILTER_EAT;
}

/***************** end handlers *******************/


static void XmppLoggedIn (Server *serv)
{
    iks *x;

    if (serv->xmpp_filter)
        iks_filter_delete (serv->xmpp_filter);
    serv->xmpp_filter = iks_filter_new ();
    iks_filter_add_rule (serv->xmpp_filter, XmppUnknown, serv, IKS_RULE_DONE);
    iks_filter_add_rule (serv->xmpp_filter, XmppHandlePresence, serv, IKS_RULE_TYPE, IKS_PAK_PRESENCE, IKS_RULE_DONE);
    iks_filter_add_rule (serv->xmpp_filter, XmppHandleSubscription, serv, IKS_RULE_TYPE, IKS_PAK_S10N, IKS_RULE_DONE);
    iks_filter_add_rule (serv->xmpp_filter, XmppHandlePresenceErr, serv, IKS_RULE_TYPE, IKS_PAK_S10N, IKS_RULE_SUBTYPE, IKS_TYPE_ERROR, IKS_RULE_DONE);
    iks_filter_add_rule (serv->xmpp_filter, XmppHandleMessage, serv, IKS_RULE_TYPE, IKS_PAK_MESSAGE, IKS_RULE_DONE);

    serv->conn->connect = CONNECT_OK | CONNECT_SELECT_R;

    XMPPSetstatus (serv, NULL, serv->status, serv->conn->cont->status_message);

    x = iks_make_iq (IKS_TYPE_GET, IKS_NS_ROSTER);
    iks_insert_attrib (x, "id", s_sprintf ("roster-%s-%x", serv->xmpp_stamp, serv->xmpp_sequence++));
    iks_send (serv->xmpp_parser, x);
    iks_delete (x);
    iks_filter_add_rule (serv->xmpp_filter, XmppHandleIqRoster, serv, IKS_RULE_TYPE, IKS_PAK_IQ, IKS_RULE_SUBTYPE, IKS_TYPE_RESULT, IKS_RULE_NS, IKS_NS_ROSTER, IKS_RULE_DONE);
    iks_filter_add_rule (serv->xmpp_filter, XmppHandleIqRoster, serv, IKS_RULE_TYPE, IKS_PAK_IQ, IKS_RULE_SUBTYPE, IKS_TYPE_SET, IKS_RULE_NS, IKS_NS_ROSTER, IKS_RULE_DONE);

    QueueEnqueueData2 (serv->conn, QUEUE_SRV_KEEPALIVE, 0, 300, NULL, &sendIqTimeReqs, NULL);
    iks_filter_add_rule (serv->xmpp_filter, XmppHandleIqTime, serv, IKS_RULE_TYPE, IKS_PAK_IQ, IKS_RULE_NS, "urn:xmpp:time", IKS_RULE_DONE);

    x = iks_make_iq (IKS_TYPE_GET, "http://jabber.org/protocol/disco#info");
    iks_insert_attrib (x, "id", s_sprintf ("disco-%s-%x", serv->xmpp_stamp, serv->xmpp_sequence++));
    iks_insert_attrib (x, "to", serv->xmpp_id->server);
    iks_send (serv->xmpp_parser, x);
    iks_delete (x);
    iks_filter_add_rule (serv->xmpp_filter, XmppHandleIqDisco, serv, IKS_RULE_TYPE, IKS_PAK_IQ, IKS_RULE_NS, "http://jabber.org/protocol/disco#info", IKS_RULE_DONE);

    iks_filter_add_rule (serv->xmpp_filter, XmppHandleIqXEP92, serv, IKS_RULE_TYPE, IKS_PAK_IQ, IKS_RULE_NS, "jabber:iq:version", IKS_RULE_DONE);
    iks_filter_add_rule (serv->xmpp_filter, XmppHandleIqXEP12, serv, IKS_RULE_TYPE, IKS_PAK_IQ, IKS_RULE_NS, "jabber:iq:last", IKS_RULE_DONE);
    iks_filter_add_rule (serv->xmpp_filter, XmppHandleIqPrivacy, serv, IKS_RULE_TYPE, IKS_PAK_IQ, IKS_RULE_NS, "jabber:iq:privacy", IKS_RULE_DONE);

    iks_filter_add_rule (serv->xmpp_filter, XmppHandleIqDefault, serv, IKS_RULE_TYPE, IKS_PAK_IQ, IKS_RULE_SUBTYPE, IKS_TYPE_SET, IKS_RULE_DONE);
    iks_filter_add_rule (serv->xmpp_filter, XmppHandleIqDefault, serv, IKS_RULE_TYPE, IKS_PAK_IQ, IKS_RULE_SUBTYPE, IKS_TYPE_GET, IKS_RULE_DONE);
    
    {
        const char *list = ServerPrefStr (serv, CO_XMPP_PRIV);
        if (list)
            XMPPSendIqPrivacy (serv, p_active, list);
    }
}

static int XmppSessionResult (IKS_FILTER_USER_DATA *fserv, ikspak *pak)
{
    Server *serv = (Server *)fserv;
    if (pak->subtype != IKS_TYPE_RESULT)
        XmppStreamError (serv, s_sprintf ("Couldn't get session %s.", iks_string (iks_stack (pak->x), pak->x)));
    else
        XmppLoggedIn (serv);
    return IKS_FILTER_EAT;
}

static int XmppUserResult (IKS_FILTER_USER_DATA *fserv, ikspak *pak)
{
    Server *serv = (Server *)fserv;
    rl_printf ("Message %s resulted in:\n%s\n", pak->id, iks_string (iks_stack (pak->x), pak->x));
    return IKS_FILTER_EAT;
}

static int XmppBindResult (IKS_FILTER_USER_DATA *fserv, ikspak *pak)
{
    Server *serv = (Server *)fserv;
    if (pak->subtype == IKS_TYPE_RESULT)
    {
        iks *bind = iks_find_with_attrib (pak->x, "bind", "xmlns", "urn:ietf:params:xml:ns:xmpp-bind");
        char *newjid;
        if  (bind && (newjid = iks_find_cdata (bind, "jid")))
        {
            iksid *newid = iks_id_new (iks_parser_stack (serv->xmpp_parser), newjid);
            if (strchr (serv->screen,  '/'))
                s_repl (&serv->screen, s_sprintf ("%s/%s", newid->partial, strchr (serv->screen,  '/')  + 1));
            else
                s_repl (&serv->screen, newid->partial);
            if (strcmp (serv->xmpp_id->partial, newid->partial))
                rl_printf ("Server changed JID from %s to %s.\n", serv->xmpp_id->partial, newid->partial);
            serv->xmpp_id = newid;
        }
    } else
        XmppStreamError (serv, s_sprintf ("Couldn't bind. %s", iks_string (iks_stack (pak->x), pak->x)));
    return IKS_FILTER_EAT;
}

#if LIBIKS_VERSION < 0x0103 || defined(ENABLE_AUTOPACKAGE)
static char *base64_encode (const char *data, int count)
{
    char *base64 = iks_base64_encode (data, count);
    char *b64 = strdup (base64);
    int len = strlen (b64);
    iks_free (base64);
    while ((len % 3) && b64[len - 1] == '=')
      len--;
    b64[len] = 0;
    return b64;
}

void replace_iks_start_sasl (iksparser *prs, char *user, char *password)
{
    char *base64 = base64_encode (s_sprintf ("%c%s%c%s", 0, user, 0, password), strlen (user) + strlen (password) + 2);
    iks_send_raw (prs, s_sprintf ("<auth xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\" mechanism=\"PLAIN\">%s</auth>", base64));
    free (base64);
}
#endif

static int XmppStreamHook (IKS_TRANS_USER_DATA *userv, int type, iks *node)
{
    Server *serv = (Server *)userv;
    iksparser *prs = serv->xmpp_parser;
    switch (type)
    {
        case IKS_NODE_STOP:
            XmppStreamError (serv, s_sprintf ("server disconnect [%s]", iks_name (iks_first_tag (node))));
            if (node) iks_delete (node);
            return IKS_NET_DROPPED;
        case IKS_NODE_ERROR:
            if (!strcmp (iks_name (iks_first_tag (node)), "conflict"))
                OptSetVal (&serv->copts, CO_AUTOTAGRES, 1);
            XmppStreamError (serv, s_sprintf ("stream error [%s]", iks_name (iks_first_tag (node))));
            if (node) iks_delete (node);
            return IKS_NET_DROPPED;
        case IKS_NODE_START:
            // nothing to do with it
            break;
        case IKS_NODE_NORMAL:
            if (!strcmp ("stream:features", iks_name (node)))
            {
                int feat = iks_stream_features (node);
                if (feat & IKS_STREAM_STARTTLS && UtilIOSSLSupported() == IO_SSL_OK)
                    iks_send_raw (prs, "<starttls xmlns=\"urn:ietf:params:xml:ns:xmpp-tls\"/>");
                else if (feat & IKS_STREAM_SASL_MD5)
                    iks_start_sasl (prs, IKS_SASL_DIGEST_MD5, serv->xmpp_id->user, serv->passwd);
                else if (feat & IKS_STREAM_SASL_PLAIN)
#if LIBIKS_VERSION < 0x0103 || defined(USE_AUTOPACKAGE)
                    replace_iks_start_sasl (prs, serv->xmpp_id->user, serv->passwd);
#else
                    iks_start_sasl (prs, IKS_SASL_PLAIN, serv->xmpp_id->user, serv->passwd);
#endif
                else
                {
                    if (feat & (IKS_STREAM_BIND | IKS_STREAM_SESSION))
                    {
                        if (serv->xmpp_filter)
                            iks_filter_delete (serv->xmpp_filter);
                        serv->xmpp_filter = iks_filter_new ();
                        iks_filter_add_rule (serv->xmpp_filter, XmppUnknown, serv, IKS_RULE_DONE);
                        if (feat & IKS_STREAM_BIND)
                        {
                            iks *t = iks_make_resource_bind (serv->xmpp_id);
                            iks_insert_attrib (t, "id", "bind");
                            iks_filter_add_rule (serv->xmpp_filter, XmppBindResult, serv,
                                IKS_RULE_TYPE, IKS_PAK_IQ, IKS_RULE_ID, "bind", IKS_RULE_DONE);
                            iks_send (prs, t);
                            iks_delete (t);
                        }
                        if (feat & IKS_STREAM_SESSION)
                        {
                            iks *t = iks_make_session ();
                            iks_insert_attrib (t, "id", "auth");
                            iks_filter_add_rule (serv->xmpp_filter, XmppSessionResult, serv,
                                IKS_RULE_TYPE, IKS_PAK_IQ, IKS_RULE_ID, "auth", IKS_RULE_DONE);
                            iks_send (prs, t);
                            iks_delete (t);
                        }
                    }
                    else
                        XmppLoggedIn (serv);
                }
            }
            else if (strcmp ("failure", iks_name (node)) == 0)
                XmppStreamError (serv, "sasl authentication failed");
            else if (strcmp ("success", iks_name (node)) == 0)
                iks_send_header (prs, serv->xmpp_id->server);
            else if (strcmp ("proceed", iks_name (node)) == 0)
            {
                io_ssl_err_t rce = UtilIOSSLOpen (serv->conn, 2);
                if (rce != IO_SSL_OK)
                {
                    XmppStreamError (serv, s_sprintf ("ssl error %d %s", rce, UtilIOErr (serv->conn)));
                    return IKS_NET_RWERR;
                }
                serv->conn->connect &= ~CONNECT_OK & ~4;
                return IKS_OK;
            }
            else
            {
                ikspak *pak = iks_packet (node);
                iks_filter_packet (serv->xmpp_filter, pak);
            }
    }
    if (node) iks_delete (node);
    return IKS_OK;
}

static void XMPPCallbackDispatch (Connection *conn)
{
    iksparser *prs = conn->serv->xmpp_parser;
    io_err_t rce;
    char *semi;
    const char *sp;
    int rc;

    if (!(conn->connect & (CONNECT_OK | 4)))
    {
        rc = UtilIORead (conn, NULL, 0);
        rce = UtilIOShowError (conn, rc);
        switch (rce) {
            case IO_RW:
                semi = strchr (conn->server, ';');
                if (!semi)
                    return;
                memmove (conn->server, semi + 1, strlen (semi));
                semi = strchr (conn->server, ';');
                if (semi)
                    *semi = 0;
                sp = s_sprintf ("%s", s_wordquote (strchr (conn->server, ':') ? conn->server : s_sprintf ("%s:%lu", conn->server, conn->port)));
                rl_printf (i18n (2620, "Opening XMPP connection for %s at %s...\n"),
                    s_wordquote (conn->serv->screen), sp);
                if (semi)
                    *semi = ';';
                UtilIOConnectTCP (conn);
                return;
            case IO_OK:
                return;
            case IO_CONNECTED:
                rl_print ("");
                if (prG->verbose || (conn->serv && conn == conn->serv->conn))
                    if (rl_pos () > 0)
                         rl_print (i18n (1634, "ok.\n"));

                if (conn->port == 443 || conn->port == 5223)
                {
                    io_ssl_err_t rce = UtilIOSSLOpen (conn, 2);
                    if (rce != IO_SSL_OK)
                    {
                        XmppStreamError (conn->serv, s_sprintf ("ssl error %d %s", rce, UtilIOErr (conn)));
                        EventD (QueueDequeue2 (conn, QUEUE_DEP_WAITLOGIN, 0, NULL));
                        return;
                    }
                }

                if (!conn->serv->xmpp_parser)
                {
                    prs = iks_stream_new (IKS_NS_CLIENT, conn->serv, &XmppStreamHook);
                    conn->serv->xmpp_id = iks_id_new (iks_parser_stack (prs), conn->serv->screen);
                    if (!conn->serv->xmpp_id->resource)
                        conn->serv->xmpp_id = iks_id_new (iks_parser_stack (prs),
                            s_sprintf ("%s@%s/climm", conn->serv->xmpp_id->user, conn->serv->xmpp_id->server));
                    if (ServerPrefVal (conn->serv, CO_TAGRESSOURCE) || ServerPrefVal (conn->serv, CO_AUTOTAGRES))
                        conn->serv->xmpp_id = iks_id_new (iks_parser_stack (prs),
                            s_sprintf ("%s/%s%04X%04X", conn->serv->xmpp_id->partial, conn->serv->xmpp_id->resource, rand() % 0xffff, rand() % 0xffff));
                    iks_set_log_hook (prs, XmppSaveLog);
                }
                conn->serv->xmpp_parser = prs;
                rc = iks_connect_with (prs, conn->server, conn->port, conn->serv->xmpp_id->server, &iks_climm_transport);
                if (rc != IKS_OK)
                {
                    XmppStreamError (conn->serv, "could not send stream header");
                    return;
                }
                conn->connect |= 4;
                return;
            default:
                assert (0);
        }
    }
    rc = iks_recv (prs, 0);
    if (rc != IKS_OK && conn->dispatcher)
        XmppStreamError (conn->serv, s_sprintf ("failing with error code %d", rc));
    if (!conn->dispatcher)
        IMCallBackReconn (conn);
}

void XMPPLogout (Server *serv)
{
    if (serv->xmpp_parser)
    {
        iksparser *prs = serv->xmpp_parser;
        serv->xmpp_parser = NULL;
        iks_disconnect (prs);
        iks_parser_delete (prs);
        serv->xmpp_id = NULL;
        serv->xmpp_filter = NULL;
    }
    QueueCancel (serv->conn);
    UtilIOClose (serv->conn);
}

/* **************** */

static void SnacCallbackXmpp (Event *event)
{
    Message *msg = (Message *)event->data;

    assert (event);
    assert (msg);
    assert (event->cont);

    if (event->attempts < 5)
    {
        if (msg->send_message && !msg->otrinjected)
        {
            msg->type = INT_MSGACK_V8;
//            IMIntMsgMsg (msg, NOW, ims_offline);
        }
        event->attempts = 20;
        event->due = time (NULL) + 600;
        QueueEnqueue (event);
    }
    else
    {
        MsgD (msg);
        event->data = NULL;
        EventD (event);
    }
}

static void SnacCallbackXmppCancel (Event *event)
{
    Message *msg = (Message *)event->data;
    if (msg->send_message && !msg->otrinjected)
        rl_printf (i18n (2234, "Message %s discarded - lost session.\n"),
    msg->plain_message ? msg->plain_message : msg->send_message);
    MsgD (msg);
    event->data = NULL;
    EventD (event);
}

UBYTE XMPPSendmsg (Server *serv, Contact *cont, Message *msg)
{
    iks *x, *xx;
    Event *event;

    assert (cont);
    assert (msg->send_message);

    if (~serv->conn->connect & CONNECT_OK)
        return RET_DEFER;
    if (msg->type != MSG_NORM)
        return RET_DEFER;

    x = iks_make_msg (IKS_TYPE_CHAT, cont->screen, msg->send_message);
    iks_insert_attrib (x, "id", s_sprintf ("xmpp-%s-%x", serv->xmpp_stamp, ++serv->xmpp_sequence));
    iks_insert_attrib (x, "from", serv->xmpp_id->full);
    xx = iks_insert (x, "x");
    iks_insert_attrib (xx, "xmlns", "jabber:x:event");
    iks_insert (xx, "offline");
    iks_insert (xx, "delivered");
    iks_insert (xx, "displayed");
    iks_insert (xx, "composing");
    iks_send (serv->xmpp_parser, x);
    iks_delete (x);

    event = QueueEnqueueData2 (serv->conn, QUEUE_XMPP_RESEND_ACK, serv->xmpp_sequence, 120, msg, &SnacCallbackXmpp, &SnacCallbackXmppCancel);
    event->cont = cont;

    return RET_OK;
}

void XMPPSetstatus (Server *serv, Contact *cont, status_t status, const char *msg)
{
    enum ikshowtype p = StatusToIksstatus (&status);
    iks *x = iks_make_pres (p, msg);
    if (p != IKS_SHOW_UNAVAILABLE)
    {
        iks *caps = iks_insert (x, "c");
        iks_insert_attrib (caps, "xmlns", "http://jabber.org/protocol/caps");
        iks_insert_attrib (caps, "node", "http://www.climm.org/xmpp/caps");
        iks_insert_attrib (caps, "ver", BuildVersionStr);
        iks_insert_cdata (iks_insert (x, "priority"), "5", 0);
    }
    if (cont)
        iks_insert_attrib (x, "to", cont->screen);
    else
    {
        s_repl (&serv->conn->cont->status_message, msg);
        serv->status = status;
        serv->nativestatus = p;
    }
    iks_send (serv->xmpp_parser, x);
    iks_delete (x);
}

UBYTE XMPPSendIq (Server *serv, int8_t which, const char *screen, const char *msg)
{
    int err;
    iks *child = iks_tree (msg, 0, &err), *x;
    const char *id;
    if (!child)
        return RET_FAIL;
    id = s_sprintf ("user-%s-%x", serv->xmpp_stamp, ++serv->xmpp_sequence);
    iks_filter_add_rule (serv->xmpp_filter, XmppUserResult, serv, IKS_RULE_TYPE, IKS_PAK_IQ, IKS_RULE_ID, id, IKS_RULE_DONE);
    x = iks_new_within ("iq", iks_stack (child));
    iks_insert_attrib (x, "type", which ? "set" : "get");
    iks_insert_attrib (x, "id", id);
    iks_insert_attrib (x, "from", serv->xmpp_id->full);
    if (screen && *screen)
        iks_insert_attrib (x, "to", screen);
    iks_insert_node (x, child);
    iks_send (serv->xmpp_parser, x);
    iks_delete (x);
    return RET_OK;
}

void XMPPAuthorize (Server *serv, Contact *cont, auth_t how, const char *msg)
{
    iks *x;
    while (cont->parent && cont->parent->serv == cont->serv)
        cont = cont->parent;

    x = iks_make_s10n (how == auth_grant ? IKS_TYPE_SUBSCRIBED
                     : how == auth_deny  ? IKS_TYPE_UNSUBSCRIBED
                     : how == auth_req   ? IKS_TYPE_SUBSCRIBE
                                         : IKS_TYPE_UNSUBSCRIBE,
                       cont->screen, msg);
    iks_send (serv->xmpp_parser, x);
    iks_delete (x);
}

void XMPPGoogleMail (Server *serv, time_t since, const char *query)
{
    assert (query);
    if (since == 1)
        XmppSendIqGmail (serv, serv->xmpp_gmail_newer, serv->xmpp_gmail_newertid, serv->xmpp_gmail_query);
    else
        XmppSendIqGmail (serv, since * 1000ULL, NULL, query);
    s_repl (&serv->xmpp_gmail_query, query);
}

void XMPPPrivacy (Server *serv, xmpp_priv_t type, const char *list, const char *edit)
{
    if (type == p_edit)
    {
        if (!list)
            return;
        if (!serv->xmpp_privacy_list || strcmp (list, serv->xmpp_privacy_list))
        {
            s_repl (&serv->xmpp_privacy_list, NULL);
            s_repl (&serv->xmpp_privacy_items, NULL);
        }
        if (!serv->xmpp_privacy_items)
        {
            XMPPPrivacy (serv, p_show_quiet, list, NULL);
            rl_printf (i18n (2768, "Fetching privacy list (was not cached), please try again.\n"));
            return;
        }
    }
    if (type == p_edit || type == p_set)
        XMPPSendIqPrivacyEdit (serv, type, list, edit ? edit : "");
    else
        XMPPSendIqPrivacy (serv, type, list);
}

#if 0
class CLIMMXMPP: public gloox::ConnectionListener, public gloox::MessageHandler,
    private :
        void handleXEP8 (gloox::Tag *t);
        void handleXEP27 (gloox::Tag *t);
        void handleXEP71 (gloox::Tag *t);
        void handleXEP153 (gloox::Tag *t, Contact *contr);
        void handleGoogleNosave (gloox::Tag *t);
        void handleGoogleSig (gloox::Tag *t);
        void handleGoogleChatstate (gloox::Tag *t);
        void handleXEP136 (gloox::Tag *t);
        virtual void handleSubscription (gloox::Stanza *stanza);
};

static bool DropChild (gloox::Tag *s, gloox::Tag *c)
{
    if (s->children().size())
        s->setCData ("");
    s->children().remove (c);
    delete c;
}

static bool DropAttrib (gloox::Tag *s, const std::string &a)
{
    if (s->children().size())
        s->setCData ("");
#if defined(LIBGLOOX_VERSION) && LIBGLOOX_VERSION >= 0x000900
    s->attributes().remove (gloox::Tag::Attribute (a, s->findAttribute (a)));
#else
    s->attributes().erase (a);
#endif
}

static bool DropCData (gloox::Tag *s)
{
    s->setCData ("");
}

static bool CheckInvalid (gloox::Tag *s)
{
    if (!s->attributes().size() && !s->children().size() && s->cdata().empty())
    {
        if (s->parent())
            DropChild (s->parent(), s);
        return true;
    }
    return false;
}

static void DropAllChilds (gloox::Tag *s, const std::string &a)
{
    gloox::Tag::TagList::const_iterator it;
    for (it = s->children().begin(); it != s->children().end(); ++it)
    {
        if ((*it)->name() == a)
        {
            gloox::Tag *b = *it;
            DropCData (b);
            DropAttrib (b, "xml:lang");
            if (CheckInvalid (b))
                return (DropAllChilds (s, a));
        }
    }
}

static void DropAllChildsTree (gloox::Tag *s, const std::string &a)
{
    gloox::Tag::TagList::const_iterator it;
    if (a.empty ())
    {
        s->attributes().clear();
        for (it = s->children().begin(); it != s->children().end(); it = s->children().begin())
        {
            gloox::Tag *b = *it;
            DropCData (b);
            DropAllChildsTree (b, "");
            CheckInvalid (b);
        }
    }
    else
    {
        for (it = s->children().begin(); it != s->children().end(); ++it)
        {
            while ((*it)->name() == a)
            {
                gloox::Tag *b = *it;
                DropCData (b);
                DropAllChildsTree (b, "");
                CheckInvalid (b);
                it = s->children().begin();
            }
        }
    }
}

void CLIMMXMPP::handleXEP8 (gloox::Tag *t)
{
    if (gloox::Tag *avatar = find_with_ns_attrib (t, "x", "jabber:x:avatar"))
    {
        if (gloox::Tag *hash = avatar->findChild ("hash"))
        {
            DropCData (hash);
            CheckInvalid (hash);
        }
        CheckInvalid (avatar);
    }
}

void CLIMMXMPP::handleXEP27 (gloox::Tag *t)
{
    if (gloox::Tag *sig = find_with_ns_attrib (t, "x", "jabber:x:signed"))
    {
        DropCData (sig);
        CheckInvalid (sig);
    }
}

void CLIMMXMPP::handleXEP71 (gloox::Tag *t)
{
    if (gloox::Tag *xhtmlim = find_with_ns_attrib (t, "html", "http://jabber.org/protocol/xhtml-im"))
    {
        DropAllChildsTree (xhtmlim, "body");
        CheckInvalid (xhtmlim);
    }
}

void CLIMMXMPP::handleXEP136 (gloox::Tag *t)
{
    if (gloox::Tag *arc = find_with_ns_attrib (t, "record", "http://jabber.org/protocol/archive"))
    {
        DropAttrib (arc, "otr");
        CheckInvalid (arc);
    }
}

void CLIMMXMPP::handleXEP153 (gloox::Tag *t, Contact *contr)
{
    if (gloox::Tag *vcard = find_with_ns_attrib (t, "x", "vcard-temp:x:update"))
    {
        if (gloox::Tag *photo = vcard->findChild ("photo"))
        {
            DropCData (photo);
            CheckInvalid (photo);
        }
        if (gloox::Tag *nick = vcard->findChild ("nickname"))
        {
            std::string nickname = nick->cdata();
            ContactAddAlias (contr, nickname.c_str());
            DropCData (nick);
            CheckInvalid (nick);
        }
        CheckInvalid (vcard);
    }
}

void CLIMMXMPP::handleGoogleNosave (gloox::Tag *t)
{
    if (gloox::Tag *nosave = find_with_ns_attrib (t, "x", "google:nosave"))
    {
        DropAttrib (nosave, "value");
        CheckInvalid (nosave);
    }
}

void CLIMMXMPP::handleGoogleSig (gloox::Tag *t)
{
    if (gloox::Tag *sig = find_with_ns_attrib (t, "google-mail-signature", "google:metadata"))
    {
        DropCData (sig);
        CheckInvalid (sig);
    }
}

void CLIMMXMPP::handleGoogleChatstate(gloox::Tag *t)
{
    if (gloox::Tag *chat = find_with_ns_attrib (t, "active", "http://jabber.org/protocol/chatstates"))
        CheckInvalid (chat);
}


#if defined(LIBGLOOX_VERSION) && LIBGLOOX_VERSION >= 0x000900
void CLIMMXMPP::handleMessage (gloox::Stanza *s, gloox::MessageSession *session)
#else
void CLIMMXMPP::handleMessage (gloox::Stanza *s)
#endif
{
    assert (s);
    assert (s->type() == gloox::StanzaMessage);

#if defined(LIBGLOOX_VERSION) && LIBGLOOX_VERSION >= 0x000900
    gloox::Stanza *t = new gloox::Stanza (s);
#else
    gloox::Stanza *t = s->clone();
#endif

    if (t->subtype() == gloox::StanzaMessageError)
        DropAllChildsTree (t, "");
    else
        handleMessage2 (t, s->from(), s->to().full(), s->id(), s->subtype ());

    DropAttrib (t, "from");
    DropAttrib (t, "to");
    DropAttrib (t, "id");
    DropAttrib (t, "xml:lang");
    if (t->hasAttribute ("xmlns", "jabber:client"))
        DropAttrib (t, "xmlns");
    if (!CheckInvalid (t))
    {
        std::string txml = t->xml();
        CLIMMXMPPSave (m_serv, txml.c_str(), 3);
    }
    delete t;
}

void CLIMMXMPP::handlePresence (gloox::Stanza *s)
{
    assert (s);
    assert (s->type() == gloox::StanzaPresence);

#if defined(LIBGLOOX_VERSION) && LIBGLOOX_VERSION >= 0x000900
    gloox::Stanza *t = new gloox::Stanza(s);
#else
    gloox::Stanza *t = s->clone();
#endif

    if (t->subtype() == gloox::StanzaPresenceError)
        DropAllChildsTree (t, "");
    else
        handlePresence2 (t, s->from(), s->to(), s->status());

    DropAttrib (t, "from");
    DropAttrib (t, "to");
    DropAttrib (t, "id");
    DropAttrib (t, "xml:lang");
    if (t->hasAttribute ("xmlns", "jabber:client"))
        DropAttrib (t, "xmlns");
    DropAllChilds (t, "status");

    if (!CheckInvalid (t))
    {
        std::string txml = t->xml();
        CLIMMXMPPSave (m_serv, txml.c_str(), 3);
    }
    delete t;
}

void CLIMMXMPP::handleSubscription (gloox::Stanza *s)
{
    assert (s);
    assert (s->type() == gloox::StanzaS10n);
    rl_printf ("handleSubscription from:<%s> to:<%s> type:<%d> id:<%s> status:<%s> prio:<%d> body:<%s> subj:<%s> thread:<%s> pres:<%d> xml:<%s>\n",
               s->from().full().c_str(), s->to().full().c_str(), s->subtype(),
               s->id().c_str(), s->status().c_str(), s->priority(),
               s->body().c_str(), s->subject().c_str(),
#if defined(LIBGLOOX_VERSION) && LIBGLOOX_VERSION >= 0x000900
               s->thread().c_str(), s->presence(),
#else
               s->thread().c_str(), s->show(),
#endif
               s->xml().c_str());
}
#endif

#endif
