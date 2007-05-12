/*
 * Off-the-Record communication support.
 *
 * mICQ OTR extension Copyright (C) © 2007 Robert Bartel
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
 * code of this release of mICQ with the OpenSSL project's "OpenSSL"
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

#include "micq.h"
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
    policy:                 cb_policy,
    create_privkey:         cb_create_privkey,
    is_logged_in:           cb_is_logged_in,
    inject_message:         cb_inject_message,
    notify:                 cb_notify,
    display_otr_message:    cb_display_otr_message,
    update_context_list:    cb_update_context_list,
    protocol_name:          cb_protocol_name,
    protocol_name_free:     cb_protocol_name_free,
    new_fingerprint:        cb_new_fingerprint,
    write_fingerprints:     cb_write_fingerprints,
    gone_secure:            cb_gone_secure,
    gone_insecure:          cb_gone_insecure,
    still_secure:           cb_still_secure,
    log_message:            cb_log_message
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
        type = TYPEF_HAVEUIN;
    else
        type = ConnectionServerNType (proto_name, 0);

    return type;
}

/* otr context to contact */
static Contact *find_contact (const char *account, const char *proto, const char *contact)
{
    Connection *conn;
    Contact *cont;

    /* may there be more than 1 TYPEF_HAVEUIN connection for a screen name? */
    conn = ConnectionFindScreen (proto_type (proto), account);
    if (!conn)
    {
        rl_print (COLDEBUG);
        rl_printf (i18n (2642, "util_otr: no connection found for account %s and protocol %s"),
                account, proto);
        rl_printf ("%s\n", COLNONE);
        return NULL;
    }

    cont = ContactScreen (conn, contact);
    if (!cont)
    {
        rl_print (COLDEBUG);
        rl_printf (i18n (2643, "util_otr: no contact %s found"), contact);
        rl_printf ("%s\n", COLNONE);
        return NULL;
    }

    return cont;
}

/* contact to otr context */
static ConnContext *find_context (Contact *cont, int create)
{
    ConnContext *ctx;
    int created = 0;

    if (!cont || !userstate || !uiG.conn)
        return NULL;

    ctx = otrl_context_find (userstate, cont->screen, uiG.conn->screen,
            proto_name (uiG.conn->type), create, &created, NULL, NULL);
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
    const char *state = "unknown state",
          *auth = "unknown auth",
          *policy = "unknown";
    OtrlPolicy p = cb_policy (NULL, ctx);

    if (!userstate || !ctx)
        return;

    switch (ctx->msgstate)
    {
        case OTRL_MSGSTATE_PLAINTEXT:
            state = "plaintext";
            break;
        case OTRL_MSGSTATE_ENCRYPTED:
            state = "encrypted";
            break;
        case OTRL_MSGSTATE_FINISHED:
            state = "finished";
            break;
    }
    switch (ctx->auth.authstate)
    {
        case OTRL_AUTHSTATE_NONE:
            switch (ctx->otr_offer)
            {
                case OFFER_NOT:
                    auth = "no offer sent";
                    break;
                case OFFER_SENT:
                    auth = "offer sent";
                    break;
                case OFFER_ACCEPTED:
                    auth = "offer accepted";
                    break;
                case OFFER_REJECTED:
                    auth = "offer rejected";
                    break;
            }
            break;
        case OTRL_AUTHSTATE_AWAITING_DHKEY:
            auth = "awaiting D-H key";
            break;
        case OTRL_AUTHSTATE_AWAITING_REVEALSIG:
            auth = "awaiting reveal signature";
            break;
        case OTRL_AUTHSTATE_AWAITING_SIG:
            auth = "awaiting signature";
            break;
        case OTRL_AUTHSTATE_V1_SETUP:
            auth = "v1 setup";
            break;
    }
    if (p == OTRL_POLICY_NEVER)
        policy = "never";
    else if (p == OTRL_POLICY_OPPORTUNISTIC)
        policy = "try";
    else if (p == OTRL_POLICY_MANUAL)
        policy = "manual";
    else if (p == OTRL_POLICY_ALWAYS)
        policy = "always";

    rl_printf ("%10s (from %s/%s): %s%s%s (%s) policy: %s\n",
            ctx->username, ctx->accountname, ctx->protocol, COLERROR,
            state, COLNONE, auth, policy);
    if (ctx->active_fingerprint && ctx->active_fingerprint->fingerprint)
    {
        char fpr[45] = "?";
        otrl_privkey_hash_to_human (fpr, ctx->active_fingerprint->fingerprint);
        rl_printf (" fpr: %45s %s(%s)%s\n", fpr, COLERROR, ctx->active_fingerprint->trust?
                ctx->active_fingerprint->trust : "no trust", COLNONE);
    }
    if (ctx->msgstate == OTRL_MSGSTATE_ENCRYPTED)
    {
        char *sid = readable_sid (ctx->sessionid, ctx->sessionid_len, ctx->sessionid_half);
        rl_printf (" V%u session %u: %s%s%s\n",
                ctx->protocol_version, ctx->generation, COLERROR, sid, COLNONE);
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
    if (ret && uiG.conn->screen)
    {
        rl_printf (i18n (2645, "Could not read OTR private key from %s\n"), keyfile.txt);
        cb_create_privkey (NULL, uiG.conn->screen, proto_name (uiG.conn->type));
    }

    /* get fingerprints */
    otrl_privkey_read_fingerprints (userstate, fprfile.txt, NULL, NULL);
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
            otrl_message_disconnect (userstate, &ops, NULL,
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
int OTRMsgIn (const char *msg, Contact *cont, char **new_msg)
{
    int ret;

    if (!userstate)
        return 0;
    
#if ENABLE_CONT_HIER
    while (cont->parent && cont->parent->serv == cont->serv)
        cont = cont->parent;
#endif

    ret = otrl_message_receiving (userstate, &ops, NULL, cont->serv->screen,
            proto_name (cont->serv->type), cont->screen, msg, new_msg, NULL, NULL, NULL);

    return ret;
}

/* Process outgoing message 
 * returns != 0 when message could not be encrypted
 * (do not send cleartext in this case)
 */
int OTRMsgOut (const char *msg, Connection *conn, Contact *cont, char **new_msg)
{
    gcry_error_t ret;

    if (!userstate)
        return 1;

#if ENABLE_CONT_HIER
    while (cont->parent && cont->parent->serv == cont->serv)
        cont = cont->parent;
#endif

    ret = otrl_message_sending (userstate, &ops, NULL, conn->screen, proto_name (conn->type),
            cont->screen, msg, NULL, new_msg, NULL, NULL);

    if (ret)
        return 1;
    else
        return 0;
}

/* OTR user command handler */
int OTRCmd (int cmd, Contact *cont, const char *arg)
{
    ConnContext *ctx = NULL;

    if (!userstate)
        return 0;
    
    if (cont)
        ctx = find_context (cont, cmd == OTR_CMD_START? 1 : 0);

    switch (cmd)
    {
        case OTR_CMD_LIST: /* list */
            rl_print (i18n (2647, "OTR connection states:\n"));
            if (ctx)
                print_context (ctx);
            else
                print_all_context ();
            break;
        case OTR_CMD_START: /* start */
            if (ctx && ctx->msgstate != OTRL_MSGSTATE_ENCRYPTED &&
                    cont && cont->status != ims_offline)
            {
                char *msg;
                OtrlPolicy policy = cb_policy (NULL, ctx);
                if (policy == OTRL_POLICY_NEVER)
                {
                    rl_print (i18n (2648, "Change otr policy first!\n"));
                    break;
                }
                msg = otrl_proto_default_query_msg (ctx->accountname, policy);
                IMCliMsg (cont, OptSetVals (NULL, CO_MSGTYPE, MSG_NORM,
                            CO_MSGTEXT, msg, CO_OTRINJECT, TRUE, 0));
                //free (msg); /* freed automatical by IMCliMsg? */
            }
            else
                rl_print (i18n (2649, "Contact offline or already encrypted?\n"));
            break;
        case OTR_CMD_STOP: /* stop */
            if (ctx && ctx->msgstate == OTRL_MSGSTATE_ENCRYPTED &&
                    cont && cont->status != ims_offline)
            {
                otrl_message_disconnect (userstate, &ops, NULL,
                    ctx->accountname, ctx->protocol, ctx->username);
            }
            else
                rl_print (i18n (2650, "Contact offline or not encrypted?\n"));
            break;
        case OTR_CMD_TRUST: /* trust */
            if (ctx && ctx->active_fingerprint && ctx->active_fingerprint->fingerprint)
            {
                char fpr[45];

                otrl_privkey_hash_to_human (fpr, ctx->active_fingerprint->fingerprint);
                if (arg) {
                    otrl_context_set_trust (ctx->active_fingerprint, strdup (arg));
                }
                arg = ctx->active_fingerprint->trust;
                printf ("%s [%45s]: %s\n", ctx->username, fpr,
                       arg? arg:"no trust");
            }
            else
                rl_print (i18n (2651, "No active fingerprint set!\n"));
            break;
        case OTR_CMD_GENKEY: /* genkey */
            if (uiG.conn && !otrl_privkey_find (userstate,
                    uiG.conn->screen, proto_name(uiG.conn->type)))
            {
                cb_create_privkey (NULL, uiG.conn->screen, proto_name (uiG.conn->type));
            }
            else
                rl_print (i18n (2652, "Delete existing key first!\n"));
            break;
        case OTR_CMD_KEYS: /* keys */
            print_keys ();
            break;
    }

    return 0;
}

/***** Callback functions follow *****/

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
    Contact *cont = NULL;
    const char *policy = NULL;

    if (context)
    {
        cont = find_contact (context->accountname, context->protocol,
                context->username);
        if (cont)
            policy = ContactPrefStr (cont, CO_OTRPOLICY);
    }
    if (policy)
    {
        if (!strcmp (policy, "never"))
            return OTRL_POLICY_NEVER;
        else if (!strcmp (policy, "try"))
            return OTRL_POLICY_OPPORTUNISTIC;
        else if (!strcmp (policy, "manual"))
            return OTRL_POLICY_MANUAL;
        else if (!strcmp (policy, "always"))
            return OTRL_POLICY_ALWAYS;
        else if (*policy)
        {
            rl_print (COLDEBUG);
            rl_printf (i18n (2653, "util_otr: \"%s\" not a policy (use never|try|manual|always)"), policy);
            rl_printf ("%s\n", COLNONE);
        }
    }

    return OTRL_POLICY_DEFAULT;
}

/* Create a private key for the given accountname/protocol if
 * desired. */
static void cb_create_privkey (void *opdata, const char *accountname, const char *protocol)
{
    gcry_error_t ret;

    rl_printf (i18n (2654, "OTR: generating new key for %s (%s)\nthis may take a while..."),
               accountname, protocol);
    ret = otrl_privkey_generate (userstate, keyfile.txt, accountname, protocol);
    if (ret)
        rl_print (i18n (2655, "something went wrong\n"));
    else
        rl_print (i18n (2656, "done\n"));
}

/* Report whether you think the given user is online.  Return 1 if
 * you think he is, 0 if you think he isn't, -1 if you're not sure.
 *
 * If you return 1, messages such as heartbeats or other
 * notifications may be sent to the user, which could result in "not
 * logged in" errors if you're wrong. */
static int cb_is_logged_in (void *opdata, const char *accountname, const char *protocol, const char *recipient)
{
    Contact *cont;

    cont = find_contact (accountname, protocol, recipient);
    if (!cont)
    {
        rl_print (COLDEBUG);
        rl_printf (i18n (2657, "util_otr: could not check whether %s is online"), recipient);
        rl_printf ("%s\n", COLNONE);
        return -1;
    }

    if (cont->status == ims_offline)
        return 0;
    else
        return 1;
}

/* Send the given IM to the given recipient from the given
 * accountname/protocol. */
static void cb_inject_message (void *opdata, const char *accountname, const char *protocol, const char *recipient, const char *message)
{
    Contact *cont;

    cont = find_contact (accountname, protocol, recipient);
    if (!cont)
    {
        rl_print (COLDEBUG);
        rl_printf (i18n (2658, "otr_inject: no contact %s found - message could not be injected!"), recipient);
        rl_printf ("%s\n", COLNONE);
        return;
    }

    if (IMCliMsg (cont, OptSetVals (NULL, CO_MSGTYPE, MSG_NORM,
                    CO_MSGTEXT, message, CO_OTRINJECT, TRUE, 0)) == RET_FAIL)
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
    const char *type;

    switch (level)
    {
        case OTRL_NOTIFY_ERROR:
            type = "Error";
        break;
        case OTRL_NOTIFY_WARNING:
            type = "Warning";
        break;
        case OTRL_NOTIFY_INFO:
            type = "Info";
        break;
        default:
            type = "Notify";
    }

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
    rl_printf ("%sOTR[%s]:%s %s\n", COLERROR, username, COLNONE, msg);
    return 0;
}

/* When the list of ConnContexts changes (including a change in
 * state), this is called so the UI can be updated. */
static void cb_update_context_list (void *opdata)
{
    /* anything useful here? */
    print_all_context ();
}

/* Return a newly-allocated string containing a human-friendly name
 * for the given protocol id */
static const char *cb_protocol_name (void *opdata, const char *protocol)
{
    /* is this function used anywhere? */
    return strdup (protocol);
}

/* Deallocate a string allocated by protocol_name */
static void cb_protocol_name_free(void *opdata, const char *protocol_name) {
    free ((char *) protocol_name);
}

/* A new fingerprint for the given user has been received. */
static void cb_new_fingerprint (void *opdata, OtrlUserState us, const char *accountname,
        const char *protocol, const char *username, unsigned char fingerprint[20])
{
    char fpr[45] = "?";

    otrl_privkey_hash_to_human (fpr, fingerprint);
    rl_printf (i18n (2660, "OTR: new fingerprint for %s: %s\n"), username, fpr);
}

/* The list of known fingerprints has changed.  Write them to disk. */
static void cb_write_fingerprints (void *opdata)
{
    otrl_privkey_write_fingerprints (userstate, fprfile.txt);
}

/* A ConnContext has entered a secure state. */
static void cb_gone_secure (void *opdata, ConnContext *context)
{
    rl_printf (i18n (2661, "OTR: secure channel to %s established\n"), context->username);
}

/* A ConnContext has left a secure state. */
static void cb_gone_insecure (void *opdata, ConnContext *context)
{
    rl_printf (i18n (2662, "OTR: secure channel to %s closed\n"), context->username);
}

/* We have completed an authentication, using the D-H keys we
 * already knew.  is_reply indicates whether we initiated the AKE. */
static void cb_still_secure (void *opdata, ConnContext *context, int is_reply)
{
    if (is_reply)
        rl_printf (i18n (2663, "OTR: secure channel to %s reestablished\n"), context->username);
    else
        rl_printf (i18n (2664, "OTR: %s reestablished secure channel\n"), context->username);
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
