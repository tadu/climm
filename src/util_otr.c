/*
 * Off-the-Record communication support.
 *
 * climm OTR extension Copyright (C) © 2007 Robert Bartel
 *
 * This extension is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 dated June, 1991.
 *
 * This extension is distributed in the hope that it will be useful, but WITHOUT
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
#include "util_otr.h"

#ifdef ENABLE_OTR
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <libotr/proto.h>
#include <libotr/userstate.h>
#include <libotr/message.h>
#include <libotr/privkey.h>

#include "contact.h"
#include "connection.h"
#include "preferences.h"
#include "im_request.h"
#include "util_str.h"
#include "util_opts.h"

/* log OTR messages? */
#define OTR_ENABLE_LOG  (1)

/* Holds private keys + known fingerprints + context */
static OtrlUserState userstate = NULL;

/* filename for private keys / fingerprints / logfile */
#define KEYFILENAME ("otr.key")
#define FPRFILENAME ("otr.fpr")
#define LOGFILENAME ("otr.log")
static str_s keyfile = {0, 0, 0};
static str_s fprfile = {0, 0, 0};
static FILE *log_file = NULL;

/* callback prototypes */
static OtrlPolicy cb_policy (void *opdata, ConnContext *context);
static void cb_create_privkey (void *opdata, const char *accountname, const char *protocol);
static int cb_is_logged_in (void *opdata, const char *accountname, const char *protocol, const char *recipient);
static void cb_inject_message (void *opdata, const char *accountname, const char *protocol,
    const char *recipient, const char *message);
static void cb_notify (void *opdata, OtrlNotifyLevel level, const char *accountname, const char *protocol,
    const char *username, const char *title, const char *primary, const char *secondary);
static int cb_display_otr_message (void *opdata, const char *accountname,
    const char *protocol, const char *username, const char *msg);
static void cb_update_context_list (void *opdata);
static const char *cb_protocol_name (void *opdata, const char *protocol);
static void cb_protocol_name_free (void *opdata, const char *protocol_name);
static void cb_new_fingerprint (void *opdata, OtrlUserState us, const char *accountname,
    const char *protocol, const char *username, unsigned char fingerprint[20]);
static void cb_write_fingerprints (void *opdata);
static void cb_gone_secure (void *opdata, ConnContext *context);
static void cb_gone_insecure (void *opdata, ConnContext *context);
static void cb_still_secure (void *opdata, ConnContext *context, int is_reply);
static void cb_log_message (void *opdata, const char *message);

/* Callback structure */
static OtrlMessageAppOps ops =
{
    .policy                = cb_policy,
    .create_privkey        = cb_create_privkey,
    .is_logged_in          = cb_is_logged_in,
    .inject_message        = cb_inject_message,
    .notify                = cb_notify,
    .display_otr_message   = cb_display_otr_message,
    .update_context_list   = cb_update_context_list,
    .protocol_name         = cb_protocol_name,
    .protocol_name_free    = cb_protocol_name_free,
    .new_fingerprint       = cb_new_fingerprint,
    .write_fingerprints    = cb_write_fingerprints,
    .gone_secure           = cb_gone_secure,
    .gone_insecure         = cb_gone_insecure,
    .still_secure          = cb_still_secure,
    .log_message           = cb_log_message
};

/* connection type to protocol name mapping */
static const char *proto_name (UWORD proto_type)
{
    const char *proto;

    proto = ConnectionServerType (proto_type);
    /* make icq8, icq5 and peer the same */
    if (!strncmp (proto, "icq", 3) || !strcmp (proto, "peer"))
        proto = "icq";

    return proto;
}

/* protocol name to connection type mapping */
static UWORD proto_type (const char *proto_name)
{
    UWORD type;

    if (!strcmp (proto_name, "icq"))
        type = TYPE_SERVER;
    else
        type = ConnectionServerNType (proto_name, 0);

    return type;
}

/* otr context to contact */
static Contact *find_contact (const char *account, const char *proto, const char *contact)
{
    Server *serv;
    Contact *cont;

    /* may there be more than 1 TYPEF_HAVEUIN connection for a screen name? */
    serv = ServerFindScreen (proto_type (proto), account);
    if (!serv)
    {
        rl_print (COLDEBUG);
        rl_printf (i18n (2642, "util_otr: no connection found for account %s and protocol %s"),
                account, proto);
        rl_printf ("%s\n", COLNONE);
        return NULL;
    }

    cont = ContactScreen (serv, contact);
    if (!cont)
    {
        rl_print (COLDEBUG);
        rl_printf (i18n (2643, "util_otr: no contact %s found"), contact);
        rl_printf ("%s\n", COLNONE);
        return NULL;
    }

    return cont;
}

static void add_app_data_find (void *data, ConnContext *context)
{
    if (context->app_data)
        return;
    if (data)
        context->app_data = data;
    data = find_contact (context->accountname, context->protocol, context->username);
    assert (data);
    context->app_data = data;
}

/* contact to otr context */
static ConnContext *find_context (Contact *cont, int create)
{
    ConnContext *ctx;
    int created = 0;

    if (!cont || !userstate || !uiG.conn)
        return NULL;

    ctx = otrl_context_find (userstate, cont->screen, cont->serv->screen,
            proto_name (cont->serv->type), create, &created, add_app_data_find, cont);
    if (ctx)
        assert (ctx->app_data == cont);
    return ctx;
}

/* returns a newly allocated session id string */
static char *readable_sid (unsigned char *data, size_t len, OtrlSessionIdHalf half)
{
    unsigned int i;
    char *sid, *tmp;

    if (!data || !len)
        return NULL;

    sid = malloc (len * 3 + 2);
    if (!sid)
        return NULL;

    if (half == OTRL_SESSIONID_FIRST_HALF_BOLD)
    {
        sid[0] = '[';
        tmp = sid + 1;
    }
    else
        tmp = sid;

    for (i = 0; i < len; i++)
    {
        if (i == len/2)
        {
            tmp[3*i-1] = (half == OTRL_SESSIONID_FIRST_HALF_BOLD)? ']' : ' ';
            tmp[3*i] =   (half == OTRL_SESSIONID_FIRST_HALF_BOLD)? ' ' : '[';
            tmp++;
        }
        snprintf (tmp + 3*i, 3*(len - i), "%02hhX:", data[i]);
    }
    if (half != OTRL_SESSIONID_FIRST_HALF_BOLD)
        sid[len * 3] = ']';
    sid[len * 3 + 2 - 1] = 0;
    
    return sid;
}

/* Prints OTR connection state */
static void print_context (ConnContext *ctx)
{
    const char *state, *auth, *policy;
    OtrlPolicy p = cb_policy (ctx->app_data, ctx);

     if (!userstate || !ctx)
        return;

    switch (ctx->msgstate)
    {
        case OTRL_MSGSTATE_PLAINTEXT: state = i18n (2665, "plaintext"); break;
        case OTRL_MSGSTATE_ENCRYPTED: state = i18n (2666, "encrypted"); break;
        case OTRL_MSGSTATE_FINISHED:  state = i18n (2667, "finished");  break;
        default:                      state = i18n (2668, "unknown state");
    }
    switch (ctx->auth.authstate)
    {
        case OTRL_AUTHSTATE_NONE:
            switch (ctx->otr_offer)
            {
                case OFFER_NOT:      auth = i18n (2669, "no offer sent");  break;
                case OFFER_SENT:     auth = i18n (2670, "offer sent");     break;
                case OFFER_ACCEPTED: auth = i18n (2671, "offer accepted"); break;
                case OFFER_REJECTED: auth = i18n (2672, "offer rejected"); break;
                default:             auth = i18n (2673, "unknown auth");
            }
            break;
        case OTRL_AUTHSTATE_AWAITING_DHKEY:     auth = i18n (2674, "awaiting D-H key");          break;
        case OTRL_AUTHSTATE_AWAITING_REVEALSIG: auth = i18n (2675, "awaiting reveal signature"); break;
        case OTRL_AUTHSTATE_AWAITING_SIG:       auth = i18n (2676, "awaiting signature");        break;
        case OTRL_AUTHSTATE_V1_SETUP:           auth = i18n (2677, "v1 setup");                  break;
        default:                                auth = i18n (2673, "unknown auth");
    }
    if      (p == OTRL_POLICY_NEVER)         policy = i18n (2678, "never");
    else if (p == OTRL_POLICY_OPPORTUNISTIC) policy = i18n (2679, "try");
    else if (p == OTRL_POLICY_MANUAL)        policy = i18n (2680, "manual");
    else if (p == (OTRL_POLICY_ALWAYS & ~OTRL_POLICY_ALLOW_V1))
                                             policy = i18n (2681, "always");
    else                                     policy = i18n (2682, "unknown");

    rl_printf ("%s/%s/%s: %s%s%s (%s) [%s]\n",
            ctx->accountname, ctx->protocol, ctx->username, COLQUOTE,
            state, COLNONE, auth, policy);
    if (ctx->active_fingerprint && ctx->active_fingerprint->fingerprint)
    {
        char fpr[45], *tr;
        otrl_privkey_hash_to_human (fpr, ctx->active_fingerprint->fingerprint);
        tr = ctx->active_fingerprint->trust;
        rl_printf (" %45s %s(%s)%s\n", fpr, COLQUOTE, tr && *tr ? i18n (2683, "trusted") : i18n (2684, "untrusted"), COLNONE);
    }
    if (ctx->msgstate == OTRL_MSGSTATE_ENCRYPTED)
    {
        char *sid = readable_sid (ctx->sessionid, ctx->sessionid_len, ctx->sessionid_half);
        rl_printf (" V%u/%u: %s%s%s\n",
                ctx->protocol_version, ctx->generation, COLQUOTE, sid, COLNONE);
        free (sid);
    }
}

/* Prints all context states */
static void print_all_context ()
{
    ConnContext *ctx;

    if (!userstate)
        return;

    for (ctx = userstate->context_root; ctx; ctx = ctx->next)
        print_context (ctx);
}

/* Prints private key information */
static void print_keys ()
{
    OtrlPrivKey *key;

    if (!userstate)
        return;

    for (key = userstate->privkey_root; key; key = key->next)
    {
        char fpr[45] = "?";
        const char *type = "?";

        if (key->pubkey_type == OTRL_PUBKEY_TYPE_DSA)
            type = "DSA";
        otrl_privkey_fingerprint (userstate, fpr, key->accountname, key->protocol);
        rl_printf ("%s/%s: %45s (%s)\n", key->accountname, key->protocol, fpr, type);
    }
}

/* Initialize OTR library, create user state
 *  and read private key/fingerprint lists */
void OTRInit ()
{
    gcry_error_t ret;
    Server *serv;
    int i;

    if (!libotr_is_present)
    {
        rl_printf (i18n (2634, "Install libOTR 3.0.0 or newer and enjoy off-the-record encrypted messages!\n"));
        return;
    }

    OTRL_INIT;

    assert (!userstate);
    userstate = otrl_userstate_create ();

    /* build filename strings */
    s_init (&keyfile, PrefUserDirReal (prG), strlen (KEYFILENAME));
    s_cat (&keyfile, KEYFILENAME);
    s_init (&fprfile, PrefUserDirReal (prG), strlen (FPRFILENAME));
    s_cat (&fprfile, FPRFILENAME);

    /* get privat keys */
    ret = otrl_privkey_read (userstate, keyfile.txt);
    if (ret)
        rl_printf (i18n (2645, "Could not read OTR private key from %s.\n"), keyfile.txt);
    
    for (i = 0; (serv = ServerNr (i)); i++)
        if (!otrl_privkey_find (userstate, serv->screen, proto_name (serv->type)))
        {
            cb_create_privkey (NULL, serv->screen, proto_name (serv->type));
        }

    /* get fingerprints */
    otrl_privkey_read_fingerprints (userstate, fprfile.txt, add_app_data_find, NULL);
}

/* Close encrypted sessions, free userstate and write fingerprints */
void OTREnd ()
{
    ConnContext *ctx;
    gcry_error_t ret;

    if (!userstate)
        return;

    /* really needed? */
    ret = otrl_privkey_write_fingerprints (userstate, fprfile.txt);
    if (ret)
    {
        rl_print (COLERROR);
        rl_print (i18n (2646, "Writing OTR fingerprints failed!"));
        rl_printf ("%s\n", COLNONE);
    }

    /* close all encrypted sessions first */
    for (ctx = userstate->context_root; ctx; ctx = ctx->next)
        if (ctx->msgstate == OTRL_MSGSTATE_ENCRYPTED)
        {
            otrl_message_disconnect (userstate, &ops, ctx->app_data,
                ctx->accountname, ctx->protocol, ctx->username);
        }

    otrl_userstate_free (userstate);
    userstate = NULL;

    s_done (&fprfile);
    s_done (&keyfile);

    if (log_file)
    {
        fclose (log_file);
        log_file = NULL;
    }
}

/* Free message allocated by libotr */
void OTRFree (char *msg)
{
    otrl_message_free (msg);
}

/* Process incoming message
 * returns != 0 when message was internal for OTR protocol
 * (like key exchange...)
 */
int OTRMsgIn (Contact *cont, fat_srv_msg_t *msg)
{
    Contact *pcont = cont;
    int ret;
    assert (userstate);

    char *otr_text = NULL;

#if ENABLE_CONT_HIER
    while (pcont->parent && pcont->parent->serv == pcont->serv)
        pcont = pcont->parent;
#endif

    ret = otrl_message_receiving (userstate, &ops, pcont, pcont->serv->screen,
            proto_name (pcont->serv->type), pcont->screen, msg->msgtext, &otr_text, NULL, add_app_data_find, pcont);

    if (ret)
    {
        if (otr_text)
            OTRFree (otr_text);
        return 1;
    }
    
    if (otr_text && msg->samehtml && strchr (otr_text, '<'))
    { /* throw message back to broken client (e.g. Pidgin) */
        Message *msg = MsgC ();
        msg->cont = cont->parent ? cont->parent : cont;
        msg->type = MSG_NORM;
        msg->send_message = strdup (s_sprintf ("Your broken client sent HTML tags in plain text field, so your message could not be delivered. Please report this as an error to the author of %s.", cont->version ? cont->version : "(unknown client)"));
        msg->otrinjected = 1;
        msg->trans = CV_MSGTRANS_ANY;
        IMCliReMsg (msg->cont, msg);
        return 1;
    }

    if (otr_text)
    { /* replace text with decrypted version */
        ConnContext *ctx;

        free (msg->msgtext);
        msg->msgtext = strdup (otr_text);
        OTRFree (otr_text);
        ctx = find_context (pcont, 0);
        assert (ctx);
        if (ctx->msgstate == OTRL_MSGSTATE_ENCRYPTED)
        {
            msg->otrencrypted = 1;
            if (ctx->active_fingerprint && ctx->active_fingerprint->trust && *ctx->active_fingerprint->trust)
                msg->otrencrypted = 2;
        }
    }
    return 0;
}

/* Process outgoing message 
 * returns != 0 when message could not be encrypted
 * (do not send cleartext in this case)
 */
Message *OTRMsgOut (Message *msg)
{
    char *otr_text = NULL;
    Contact *cont = msg->cont;
    gcry_error_t ret;

    assert (userstate);

#if ENABLE_CONT_HIER
    while (cont->parent && cont->parent->serv == cont->serv)
        cont = cont->parent;
#endif

    ret = otrl_message_sending (userstate, &ops, cont, cont->serv->screen, proto_name (cont->serv->type),
            cont->screen, msg->send_message, NULL, &otr_text, add_app_data_find, cont);

    if (ret)
    {
        rl_print (COLERROR);
        rl_printf (i18n (2641, "Message for %s could not be encrypted and was NOT sent!"),
                msg->cont->nick);
        rl_printf ("%s\n", COLNONE);
        MsgD (msg);
        if (otr_text)
            OTRFree (otr_text);
        return NULL;
    }
    /* replace text if OTR changed it */
    if (otr_text && strcmp (msg->send_message, otr_text))
    {
        ConnContext *ctx;
        assert (!msg->plain_message);
        ctx = find_context (cont, 0);
        assert (ctx);
        if (ctx->msgstate == OTRL_MSGSTATE_ENCRYPTED)
        {
            msg->otrencrypted = 1;
            if (ctx->active_fingerprint && ctx->active_fingerprint->trust && *ctx->active_fingerprint->trust)
                msg->otrencrypted = 2;
        }
        msg->plain_message = msg->send_message;
        msg->send_message = strdup (otr_text);
        OTRFree (otr_text);
    }
    return msg;
}

ConnContext *Cont2Ctx (Contact *cont, int create)
{
    ConnContext *ctx;
    if (!cont)
        return NULL;
    ctx = find_context (cont, create);
    if (!ctx)
    {
        rl_printf (i18n (2685, "Could not find context for %s.\n"), s_cquote (cont->screen, COLQUOTE));
        return NULL;
    }
    return ctx;
}

void OTRContext (Contact *cont)
{
    ConnContext *ctx = Cont2Ctx (cont, 0);
    if (!userstate || (cont && !ctx))
        return;
    if (ctx)
        print_context (ctx);
    else
        print_all_context ();
}

void OTRStart (Contact *cont, UBYTE start)
{
    ConnContext *ctx = Cont2Ctx (cont, start);
    if (!userstate || !ctx)
        return;

    if (start && ctx->msgstate == OTRL_MSGSTATE_ENCRYPTED)
        otrl_message_disconnect (userstate, &ops, cont, ctx->accountname, ctx->protocol, ctx->username);

    if (start)
    {
        OtrlPolicy policy = cb_policy (cont, ctx);
        char *msg;
        if (policy == OTRL_POLICY_NEVER)
        {
            rl_printf (i18n (2648, "No OTR is allowed as long as policy is set to %s.\n"), s_qquote ("never"));
            rl_printf (i18n (2686, "Use %s to change.\n"), s_qquote (s_sprintf ("opt %s otrpolicy", cont->nick)));
            return;
        }
        msg = otrl_proto_default_query_msg (ctx->accountname, policy);
        IMCliMsg (cont, MSG_NORM, msg, OptSetVals (NULL, CO_OTRINJECT, TRUE, 0));
        free (msg);
    }
    else
        otrl_message_disconnect (userstate, &ops, cont, ctx->accountname, ctx->protocol, ctx->username);
}

void OTRSetTrust (Contact *cont, UBYTE trust)
{
    char fpr[45], *tr;
    ConnContext *ctx = Cont2Ctx (cont, 0);
    if (!userstate || !ctx)
        return;

    if (!ctx->active_fingerprint || !ctx->active_fingerprint->fingerprint)
    {
        rl_print (i18n (2651, "No active fingerprint set - start OTR session first.\n"));
        return;
    }

    otrl_privkey_hash_to_human (fpr, ctx->active_fingerprint->fingerprint);
    if (trust)
        otrl_context_set_trust (ctx->active_fingerprint, trust == 2 ? "." : NULL);

    tr = ctx->active_fingerprint->trust;
    printf ("%s [%44s]: %s\n", ctx->username, fpr, tr && *tr ? i18n (2683, "trusted") : i18n (2684, "untrusted"));
}

void OTRGenKey (void)
{
    if (!userstate)
        return;
    if (uiG.conn && !otrl_privkey_find (userstate,
            uiG.conn->screen, proto_name (uiG.conn->type)))
    {
        cb_create_privkey (NULL, uiG.conn->screen, proto_name (uiG.conn->type));
    }
    else
        rl_print (i18n (2652, "Delete existing key first!\n"));
}

void OTRListKeys (void)
{
    if (!userstate)
        return;
    print_keys ();
}

/* Return the OTR policy for the given context.
 * maybe combination of:
 * OTRL_POLICY_ALLOW_V1, OTRL_POLICY_ALLOW_V2, OTRL_POLICY_REQUIRE_ENCRYPTION
 * OTRL_POLICY_SEND_WHITESPACE_TAG, OTRL_POLICY_WHITESPACE_START_AKE,
 * OTRL_POLICY_ERROR_START_AKE
 * or shorter:
 * OTRL_POLICY_NEVER, OTRL_POLICY_OPPORTUNISTIC, OTRL_POLICY_MANUAL,
 * OTRL_POLICY_ALWAYS, OTRL_POLICY_DEFAULT
 * */
static OtrlPolicy cb_policy (void *opdata, ConnContext *context)
{
    Contact *cont = opdata;
    const char *policy = NULL;
    
    assert (cont);
    assert (cont == context->app_data);
    
    if (cont)
        policy = ContactPrefStr (cont, CO_OTRPOLICY);
    if (policy)
    {
        if (!strcmp (policy, "never"))
            return OTRL_POLICY_NEVER;
        else if (!strcmp (policy, "try"))
            return OTRL_POLICY_OPPORTUNISTIC;
        else if (!strcmp (policy, "manual"))
            return OTRL_POLICY_MANUAL;
        else if (!strcmp (policy, "always"))
            return OTRL_POLICY_ALWAYS & ~OTRL_POLICY_ALLOW_V1;
        else if (*policy)
        {
            rl_print (COLDEBUG);
            rl_printf (i18n (2653, "util_otr: \"%s\" not a policy (use never|try|manual|always)"), policy);
            rl_printf ("%s\n", COLNONE);
        }
    }
    return cont ? OTRL_POLICY_DEFAULT : OTRL_POLICY_ALLOW_V1;
}

/* Create a private key for the given accountname/protocol if
 * desired. */
static void cb_create_privkey (void *opdata, const char *accountname, const char *protocol)
{
    gcry_error_t ret;
    char *prot = strdup (s_qquote (protocol));

    rl_printf (i18n (2654, "Generating new OTR key for %s account %s. This may take a while..."),
               prot, s_qquote (accountname));
    s_free (prot);
    ret = otrl_privkey_generate (userstate, keyfile.txt, accountname, protocol);
    if (ret)
        rl_print (i18n (2655, "failed.\n"));
    else
        rl_print (i18n (2656, "successfull.\n"));
}

/* Report whether you think the given user is online.  Return 1 if
 * you think he is, 0 if you think he isn't, -1 if you're not sure.
 *
 * If you return 1, messages such as heartbeats or other
 * notifications may be sent to the user, which could result in "not
 * logged in" errors if you're wrong. */
static int cb_is_logged_in (void *opdata, const char *accountname, const char *protocol, const char *recipient)
{
    Contact *cont = opdata;
    assert (cont);
    assert (cont == find_contact (accountname, protocol, recipient));

    if (!cont->group)
        return -1;
    else if (cont->status == ims_offline)
        return 0;
    else
        return 1;
}

/* Send the given IM to the given recipient from the given
 * accountname/protocol. */
static void cb_inject_message (void *opdata, const char *accountname, const char *protocol, const char *recipient, const char *message)
{
    Contact *cont = opdata;
    Message *msg;
    UBYTE ret;

    assert (cont);
    assert (cont == find_contact (accountname, protocol, recipient));

    msg = MsgC ();
    msg->cont = cont;
    msg->type = MSG_NORM;
    msg->send_message = strdup (message);
    msg->otrinjected = 1;
    msg->trans = CV_MSGTRANS_ANY;
    ret = IMCliReMsg (cont, msg);
    if (ret == RET_DEFER && msg->maxsize)
    {
        str_s str = { NULL, 0, 0 };
        UDWORD fragments, frag;
        
        fragments = strlen (message) / msg->maxsize + 1;
        frag = 1;
        assert (fragments < 65536);
        while (frag <= fragments)
        {
            s_init (&str, "", msg->maxsize);
            s_catf (&str, "?OTR,%hu,%hu,", (unsigned short)frag, (unsigned short)fragments);
            s_catn (&str, message, strlen (message) / (fragments + 1 - frag));
            s_catc (&str, ',');
            message += strlen (message) / (fragments + 1 - frag);
            cb_inject_message (opdata, accountname, protocol, recipient, str.txt);
        }
        s_done (&str);
        MsgD (msg);
        return;
    }
    
    if (!RET_IS_OK (ret))
    {
        rl_print (COLDEBUG);
        rl_printf (i18n (2659, "otr_inject: message injection for contact %s failed"), recipient);
        rl_printf ("%s\n", COLNONE);
    }
}

/* Display a notification message for a particular accountname /
 * protocol / username conversation. */
static void cb_notify (void *opdata, OtrlNotifyLevel level, const char *accountname, const char *protocol,
        const char *username, const char *title, const char *primary, const char *secondary)
{
    Contact *cont = (Contact *)opdata;
    const char *type;

    assert (cont);
    assert (cont == find_contact (accountname, protocol, username));

    switch (level)
    {
        case OTRL_NOTIFY_ERROR:    type = "Error";   break;
        case OTRL_NOTIFY_WARNING:  type = "Warning"; break;
        case OTRL_NOTIFY_INFO:     type = "Info";    break;
        default:                   type = "Notify";
    }

    rl_log_for (cont->nick, COLCONTACT);
    rl_printf ("%sOTR %s:%s %s\n %s\n %s\n", COLERROR, type, COLNONE,
            title, primary, secondary);
}

/* Display an OTR control message for a particular accountname /
 * protocol / username conversation.  Return 0 if you are able to
 * successfully display it.  If you return non-0 (or if this
 * function is NULL), the control message will be displayed inline,
 * as a received message, or else by using the above notify()
 * callback. */
static int cb_display_otr_message (void *opdata, const char *accountname,
        const char *protocol, const char *username, const char *msg)
{
    Contact *cont = (Contact *)opdata;
    assert (cont);
    assert (cont == find_contact (accountname, protocol, username));
    rl_log_for (cont->nick, COLCONTACT);
    rl_printf ("%sOTR%s: %s\n", COLERROR, COLNONE, msg);
    return 0;
}

/* When the list of ConnContexts changes (including a change in
 * state), this is called so the UI can be updated. */
static void cb_update_context_list (void *opdata)
{
    /* anything useful here? */
    /* print_all_context (); */
}

/* Return a newly-allocated string containing a human-friendly name
 * for the given protocol id */
static const char *cb_protocol_name (void *opdata, const char *protocol)
{
    /* is this function used anywhere? */
    return strdup (protocol);
}

/* Deallocate a string allocated by protocol_name */
static void cb_protocol_name_free (void *opdata, const char *protocol_name) {
    free ((char *) protocol_name);
}

/* A new fingerprint for the given user has been received. */
static void cb_new_fingerprint (void *opdata, OtrlUserState us, const char *accountname,
        const char *protocol, const char *username, unsigned char fingerprint[20])
{
    Contact *cont = (Contact *)opdata;
    char fpr[45];
    assert (cont);
    assert (cont == find_contact (accountname, protocol, username));
    otrl_privkey_hash_to_human (fpr, fingerprint);
    rl_log_for (cont->nick, COLCONTACT);
    rl_printf (i18n (2660, "new OTR fingerprint: %s.\n"), fpr);
}

/* The list of known fingerprints has changed.  Write them to disk. */
static void cb_write_fingerprints (void *opdata)
{
    otrl_privkey_write_fingerprints (userstate, fprfile.txt);
}

/* A ConnContext has entered a secure state. */
static void cb_gone_secure (void *opdata, ConnContext *context)
{
    Contact *cont = opdata;
    assert (cont);
    assert (cont == context->app_data);
    rl_log_for (cont->nick, COLCONTACT);
    rl_printf (i18n (2661, "secure OTR channel established.\n"));
}

/* A ConnContext has left a secure state. */
static void cb_gone_insecure (void *opdata, ConnContext *context)
{
    Contact *cont = opdata;
    assert (cont);
    assert (cont == context->app_data);
    rl_log_for (cont->nick, COLCONTACT);
    rl_printf (i18n (2662, "secure OTR channel closed.\n"));
}

/* We have completed an authentication, using the D-H keys we
 * already knew.  is_reply indicates whether we initiated the AKE. */
static void cb_still_secure (void *opdata, ConnContext *context, int is_reply)
{
    Contact *cont = opdata;
    assert (cont);
    assert (cont == context->app_data);
    rl_log_for (cont->nick, COLCONTACT);
    if (is_reply)
        rl_printf (i18n (2663, "secure OTR channel reestablished.\n"));
    else
        rl_printf (i18n (2664, "reestablished secure OTR channel.\n"));
}

/* Log a message.  The passed message will end in "\n". */
static void cb_log_message (void *opdata, const char *message)
{
    if (!OTR_ENABLE_LOG)
        return;

    if (log_file == NULL)
    {
        str_s logname = {0, 0, 0};
        s_init (&logname, PrefUserDirReal (prG), strlen (LOGFILENAME));
        s_cat (&logname, LOGFILENAME);
        log_file = fopen (logname.txt, "a");
        s_done (&logname);
    }
    if (log_file != NULL)
        fputs (message, log_file);
}
#endif /* ENABLE_OTR */
