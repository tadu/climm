
/*
 * Send ICQ commands, using protocol version 5.
 *
 * This file is Copyright © Rüdiger Kuhlmann; it may be distributed under
 * version 2 of the GPL licence.
 *
 * $Id$
 */

#include "micq.h"
#include "packet.h"
#include "session.h"
#include "contact.h"
#include "cmd_pkt_cmd_v5.h"
#include "cmd_pkt_cmd_v5_util.h"
#include "preferences.h"
#include "util.h"
#include "conv.h"
#include "icq_response.h"
#include "util_ui.h"
#include "buildmark.h"
#include "util_str.h"
#include <string.h>
#include <assert.h>
#include <netinet/in.h> /* for htonl, htons */

/*
 * CMD_ACK - acknowledge a received packet.
 */
void CmdPktCmdAck (Connection *conn, UDWORD seq)
{
    Packet *pak = PacketCv5 (conn, CMD_ACK);
    PacketWrite4    (pak, rand ());
    PacketWriteAt4  (pak, CMD_v5_OFF_SEQ, seq);
    PacketEnqueuev5 (pak, conn);
}

/*
 * CMD_SEND_MESSAGE - send a message through ICQ.
 */
void CmdPktCmdSendMessage (Connection *conn, UDWORD uin, const char *text, UDWORD type)
{
    Packet *pak;
    Contact *cont;
    
    cont = ContactByUIN (uin, 1);
    if (!cont)
        return;
    
    IMIntMsg (cont, conn, NOW, STATUS_OFFLINE, INT_MSGACK_V8, text, NULL);
                    
    pak = PacketCv5 (conn, CMD_SEND_MESSAGE);
    PacketWrite4    (pak, uin);
    PacketWrite2    (pak, type);
    PacketWriteLNTS (pak, c_out (text));
    PacketEnqueuev5 (pak, conn);
}

/*
 * CMD_TCP_REQUEST - request peer to open a TCP connection.
 */
void CmdPktCmdTCPRequest (Connection *conn, UDWORD tuin, UDWORD port)
{
    Packet *pak;

    if (!conn->assoc || !(conn->assoc->connect & CONNECT_OK))
        return;

    pak = PacketCv5 (conn, CMD_TCP_REQUEST);
    PacketWrite4 (pak, tuin);
    PacketWrite4 (pak, conn->our_local_ip);
    PacketWrite4 (pak, conn->assoc->port);
    PacketWrite1 (pak, conn->assoc->status);
    PacketWrite4 (pak, port);
    PacketWrite4 (pak, conn->assoc->port);
    PacketWrite2 (pak, 2);
    PacketEnqueuev5 (pak, conn);
}

/*
 * CMD_LOGIN - start login process by sending login request.
 */
void CmdPktCmdLogin (Connection *conn)
{
    Packet *pak;

    if (conn->our_seq2 != 1)
    {
        conn->our_seq     = rand () & 0x3fff;
        conn->our_seq2    = 1;
        conn->our_session = rand () & 0x3fffffff;
    }
    conn->ver = 5;
    
    assert (strlen (conn->passwd) <= 8);
    
    pak = PacketCv5 (conn, CMD_LOGIN);
    PacketWrite4 (pak, time (NULL));
    PacketWrite4 (pak, conn->assoc && conn->assoc->connect & CONNECT_OK ?
                       conn->assoc->port : 0);
    PacketWriteLNTS (pak, c_out (conn->passwd));
    PacketWrite4 (pak, 0x000000d5);
    PacketWrite4 (pak, conn->our_local_ip);
    PacketWrite1 (pak, conn->assoc && conn->assoc->connect & CONNECT_OK ?
                       conn->assoc->status : 0);         /* 1=firewall | 2=proxy | 4=tcp */
    PacketWrite4 (pak, prG->status);
    PacketWrite2 (pak, conn->assoc && conn->assoc->connect & CONNECT_OK ?
                       conn->assoc->ver : 0);
    PacketWrite2 (pak, 0);
    PacketWrite4 (pak, 0x822c01ec);   /* 0x00d50008, 0x00780008 */
    PacketWrite4 (pak, 0x00000050);
    PacketWrite4 (pak, 0x00000003);
    PacketWrite4 (pak, BUILD_MICQ_OLD);
    PacketEnqueuev5 (pak, conn);
}

/*
 * CMD_REG_NEW_USER - request a new user ID.
 */
void CmdPktCmdRegNewUser (Connection *conn, const char *pass)
{
    Packet *pak = PacketCv5 (conn, CMD_REG_NEW_USER);

    assert (strlen (pass) <= 9);

    PacketWriteAt4  (pak, CMD_v5_OFF_UIN, 0); /* no UIN */
    PacketWriteLNTS (pak, c_out (pass));
    PacketWrite4    (pak, 0xA0);
    PacketWrite4    (pak, 0x2461);
    PacketWrite4    (pak, 0xA00000);
    PacketWrite4    (pak, 0x00);
    
    if (!conn->our_session)
        PacketWriteAt4 (pak, CMD_v5_OFF_SESS, rand () & 0x3fffffff);

    PacketEnqueuev5 (pak, conn);
}

/*
 * CMD_CONTACT_LIST - send over the contact list.
 */
void CmdPktCmdContactList (Connection *conn)
{
    ContactGroup *cg = conn->contacts;
    Contact *cont;
    Packet *pak = NULL;
    UWORD pbytes = 0;
    int i, j;

    for (i = j = 0; (cont = ContactIndex (cg, j)); j++)
    {
        if (cont->flags & CONT_ALIAS)
            continue;

        if (!pak)
        {
            pak = PacketCv5 (conn, CMD_CONTACT_LIST);
            pbytes = PacketWritePos (pak);
            PacketWrite1 (pak, 0);
        }
        PacketWrite4 (pak, cont->uin);
        i++;

        if (i < MAX_CONTS_PACKET)
            continue;

        PacketWriteAt1 (pak, pbytes, i);
        PacketEnqueuev5 (pak, conn);
        pak = NULL;
    }
    if (pak)
    {
        PacketWriteAt1 (pak, pbytes, i);
        PacketEnqueuev5 (pak, conn);
    }
}

/*
 * CMD_SEARCH_UIN - (not implemented)
 */

/*
 * CMD_SEARCH_USER - search for a user.
 */
void CmdPktCmdSearchUser (Connection *conn, const char *email, const char *nick,
                                         const char *first, const char *last)
{
    Packet *pak = PacketCv5 (conn, CMD_SEARCH_USER);
    PacketWriteLNTS (pak, c_out (nick));
    PacketWriteLNTS (pak, c_out (first));
    PacketWriteLNTS (pak, c_out (last));
    PacketWriteLNTS (pak, c_out (email));
    PacketEnqueuev5 (pak, conn);
}

/*
 * CMD_KEEP_ALIVE, CMD_KEEP_ALIVE2 - send keep alive packets.
 */
void CmdPktCmdKeepAlive (Connection *conn)
{
    Packet *pak = PacketCv5 (conn, CMD_KEEP_ALIVE);
    PacketWrite4 (pak, rand ());
    PacketEnqueuev5 (pak, conn);
#if 1
    pak = PacketCv5 (conn, CMD_KEEP_ALIVE2);
    PacketWrite4 (pak, rand ());
    PacketEnqueuev5 (pak, conn);
#endif
}

/*
 * CMD_SEND_TEXT_CODE - sends a text-based command to server.
 */
void CmdPktCmdSendTextCode (Connection *conn, const char *text)
{
    Packet *pak = PacketCv5 (conn, CMD_SEND_TEXT_CODE);
    PacketWriteLNTS (pak, c_out (text));
    PacketWrite1    (pak, 5);
    PacketWrite1    (pak, 0);
    PacketEnqueuev5 (pak, conn);
}

/*
 * CMD_ACK_MESSAGES - acknowledge the receipt of all offline messages.
 */
void CmdPktCmdAckMessages (Connection *conn)
{
    Packet *pak = PacketCv5 (conn, CMD_ACK_MESSAGES);
    PacketWrite2 (pak, rand ());
    PacketEnqueuev5 (pak, conn);
}

/*
 * CMD_LOGIN_1 - finishes login process.
 */
void CmdPktCmdLogin1 (Connection *conn)
{
    Packet *pak = PacketCv5 (conn, CMD_LOGIN_1);
    PacketWrite4 (pak, rand ());
    PacketEnqueuev5 (pak, conn);
}

/*
 * CMD_MSG_TO_NEW_USER - (not implemented)
 */

/*
 * CMD_INFO_REQ - (not implemented)
 */

/*
 * CMD_EXT_INFO_REQ - request extended information on a user (unused)
 */
void CmdPktCmdExtInfoReq (Connection *conn, UDWORD uin)
{
    Packet *pak = PacketCv5 (conn, CMD_EXT_INFO_REQ);
    PacketWrite4 (pak, uin);
    PacketEnqueuev5 (pak, conn);
}

/*
 * CMD_CHANGE_PW - (not implemented) (obsolete)
 */

/*
 * CMD_NEW_USER_INFO - (not implemented)
 */

/*
 * CMD_UPDATE_EXT_INFO - (not implemented)
 */

/*
 * CMD_QUERY_SERVERS - (not implemented)
 */

/*
 * CMD_QUERY_ADDONS - (not implemented)
 */

/*
 * CMD_STATUS_CHANGE - change status.
 */
void CmdPktCmdStatusChange (Connection *conn, UDWORD status)
{
    Packet *pak = PacketCv5 (conn, CMD_STATUS_CHANGE);
    PacketWrite4 (pak, status);
    PacketEnqueuev5 (pak, conn);

    conn->status = status;
}

/*
 * CMD_NEW_USER_1 - (not implemented)
 */

/*
 * CMD_UPDATE_INFO - update basic info on server. (unused)
 */
void CmdPktCmdUpdateInfo (Connection *conn, const char *email, const char *nick,
                          const char *first, const char *last, BOOL auth)
{
    Packet *pak = PacketCv5 (conn, CMD_UPDATE_INFO);
    PacketWriteLNTS (pak, c_out (nick));
    PacketWriteLNTS (pak, c_out (first));
    PacketWriteLNTS (pak, c_out (last));
    PacketWriteLNTS (pak, c_out (email));
    PacketEnqueuev5 (pak, conn);

    pak = PacketCv5 (conn, CMD_AUTH_UPDATE);
    PacketWrite4 (pak, auth);
    PacketEnqueuev5 (pak, conn);
}

/*
 * CMD_AUTH_UPDATE -> see CMD_UPDATE_INFO, CMD_META_USER
 */

/*
 * CMD_KEEP_ALIVE2 -> see CMD_KEEP_ALIVE
 */

/*
 * CMD_LOGIN_2 - (not implemented) (obsolete)
 */

/*
 * CMD_ADD_TO_LIST - (not implemented)
 */

/*
 * CMD_RAND_SET - set random chat group.
 */
void CmdPktCmdRandSet (Connection *conn, UDWORD group)
{
    Packet *pak = PacketCv5 (conn, CMD_RAND_SET);
    PacketWrite4 (pak, group);
    PacketEnqueuev5 (pak, conn);
}

/*
 * CMD_RAND_SEARCH - search a random user from given chat group.
 */
UDWORD CmdPktCmdRandSearch (Connection *conn, UDWORD group)
{
    Packet *pak = PacketCv5 (conn, CMD_RAND_SEARCH);
    UDWORD ref = pak->ref;
    PacketWrite4 (pak, group);
    PacketEnqueuev5 (pak, conn);
    return ref;
}

/*
 * CMD_META_USER : META_SET_GENERAL_INFO - Update general information.
 */
void CmdPktCmdMetaGeneral (Connection *conn, Contact *cont)
{
    Packet *pak = PacketCv5 (conn, CMD_META_USER);
    PacketWrite2    (pak, META_SET_GENERAL_INFO_v5);
    if (cont->meta_general)
    {
        PacketWriteLNTS (pak, c_out (cont->meta_general->nick));
        PacketWriteLNTS (pak, c_out (cont->meta_general->first));
        PacketWriteLNTS (pak, c_out (cont->meta_general->last));
        PacketWriteLNTS (pak, c_out (cont->meta_general->email));
        PacketWriteLNTS (pak, cont->meta_email ? c_out (cont->meta_general->email) : "");
        PacketWriteLNTS (pak, cont->meta_email && cont->meta_email->meta_email ?
                              c_out (cont->meta_email->meta_email->email) : "");
        PacketWriteLNTS (pak, c_out (cont->meta_general->city));
        PacketWriteLNTS (pak, c_out (cont->meta_general->state));
        PacketWriteLNTS (pak, c_out (cont->meta_general->phone));
        PacketWriteLNTS (pak, c_out (cont->meta_general->fax));
        PacketWriteLNTS (pak, c_out (cont->meta_general->street));
        PacketWriteLNTS (pak, c_out (cont->meta_general->cellular));
        PacketWrite4    (pak, atoi (cont->meta_general->zip));
        PacketWrite2    (pak, cont->meta_general->country);
        PacketWrite1    (pak, cont->meta_general->tz);
        PacketWrite1    (pak, !cont->meta_general->auth);
        PacketWrite1    (pak, cont->meta_general->webaware);
        PacketWrite1    (pak, cont->meta_general->hideip);
    }
    else
    {
        PacketWriteLNTS (pak, c_out (cont->nick));
        PacketWriteLNTS (pak, "");
        PacketWriteLNTS (pak, "<unset>");
        PacketWriteLNTS (pak, "<unset>");
        PacketWriteLNTS (pak, "");
        PacketWriteLNTS (pak, "");
        PacketWriteLNTS (pak, "");
        PacketWriteLNTS (pak, "");
        PacketWriteLNTS (pak, "");
        PacketWriteLNTS (pak, "");
        PacketWriteLNTS (pak, "");
        PacketWriteLNTS (pak, "");
        PacketWrite4    (pak, 0);
        PacketWrite2    (pak, 0);
        PacketWrite1    (pak, 0);
        PacketWrite1    (pak, 0);
        PacketWrite1    (pak, 0);
        PacketWrite1    (pak, 0);
    }
    PacketEnqueuev5 (pak, conn);
}

/*
 * CMD_META_USER : META_SET_WORK_INFO - (not implemented)
 */

/*
 * CMD_META_USER : META_SET_MORE_INFO - Update additional information.
 */
void CmdPktCmdMetaMore (Connection *conn, Contact *cont)
{
    Packet *pak = PacketCv5 (conn, CMD_META_USER);
    PacketWrite2    (pak, META_SET_MORE_INFO);
    if (cont->meta_more)
    {
        PacketWrite2    (pak, cont->meta_more->age);
        PacketWrite1    (pak, cont->meta_more->sex);
        PacketWriteLNTS (pak, c_out (cont->meta_more->homepage));
        PacketWrite2    (pak, cont->meta_more->year);
        PacketWrite2    (pak, cont->meta_more->month);
        PacketWrite2    (pak, cont->meta_more->day);
        PacketWrite2    (pak, cont->meta_more->lang1);
        PacketWrite2    (pak, cont->meta_more->lang2);
        PacketWrite2    (pak, cont->meta_more->lang3);
    }
    else
    {
        PacketWrite2    (pak, 0);
        PacketWrite1    (pak, 0);
        PacketWriteLNTS (pak, "");
        PacketWrite2    (pak, 0);
        PacketWrite2    (pak, 0);
        PacketWrite2    (pak, 0);
        PacketWrite2    (pak, 0);
        PacketWrite2    (pak, 0);
        PacketWrite2    (pak, 0);
    }
    PacketEnqueuev5 (pak, conn);
}

/*
 * CMD_META_USER : META_SET_ABOUT_INFO - Set "about" information.
 */
void CmdPktCmdMetaAbout (Connection *conn, const char *about)
{
    Packet *pak = PacketCv5 (conn, CMD_META_USER);
    PacketWrite2    (pak, META_SET_ABOUT_INFO);
    PacketWriteLNTS (pak, c_out (about));
    PacketEnqueuev5 (pak, conn);
}

/*
 * CMD_META_USER : META_INFO_SECURE - (not implemented)
 */

/*
 * CMD_META_USER : META_SET_PASS - set a new password.
 */
void CmdPktCmdMetaPass (Connection *conn, char *pass)
{
    Packet *pak = PacketCv5 (conn, CMD_META_USER);

    assert (strlen (pass) <= 9);

    PacketWrite2    (pak, META_SET_PASS);
    PacketWriteLNTS (pak, c_out (pass));
    PacketEnqueuev5 (pak, conn);
}

/*
 * CMD_META_USER : META_REQ_INFO - request information on user.
 */
UDWORD CmdPktCmdMetaReqInfo (Connection *conn, Contact *cont)
{
    Packet *pak = PacketCv5 (conn, CMD_META_USER);
    UDWORD ref = pak->ref;
    PacketWrite2 (pak, META_REQ_INFO_v5);
    PacketWrite4 (pak, cont->uin);
    PacketEnqueuev5 (pak, conn);
    return ref;
}

/*
 * CMD_META_USER : META_SEARCH_WP - do an extensive white page search.
 */
void CmdPktCmdMetaSearchWP (Connection *conn, MetaWP *user)
{
    Packet *pak = PacketCv5 (conn, CMD_META_USER);
    PacketWrite2    (pak, META_SEARCH_WP);
    PacketWriteLNTS (pak, c_out (user->first));
    PacketWriteLNTS (pak, c_out (user->last));
    PacketWriteLNTS (pak, c_out (user->nick));
    PacketWriteLNTS (pak, c_out (user->email));
    PacketWrite2    (pak, user->minage);
    PacketWrite2    (pak, user->maxage);
    PacketWrite1    (pak, user->sex);
    PacketWrite2    (pak, user->language);
    PacketWriteLNTS (pak, c_out (user->city));
    PacketWriteLNTS (pak, c_out (user->state));
    PacketWrite2    (pak, user->country);
    PacketWriteLNTS (pak, c_out (user->company));
    PacketWriteLNTS (pak, c_out (user->department));
    PacketWriteLNTS (pak, c_out (user->position));

/*  Now it gets REALLY shakey, as I don't know even what
    these particular bits of information are.
    If you know, fill them in. Pretty sure they are
    interests, organizations, homepage and something else
    but not sure what order. -KK */
    
    PacketWrite1      (pak, 0);
    PacketWrite2      (pak, 0);
    PacketWrite1      (pak, 1);
    PacketWrite4      (pak, 0);
    PacketWrite1      (pak, 1);
    PacketWrite4      (pak, 0);
    PacketWrite1      (pak, 1);
    PacketWrite4      (pak, 0);
    PacketWrite1      (pak, 1);
    PacketWrite2      (pak, 0);
    PacketWrite1      (pak, user->online);
    PacketEnqueuev5 (pak, conn);
}

/*
 * CMD_META_USER : META_SET_WEB_PRESENCE - (not implemented)
 */

/*
 * CMD_INVIS_LIST - send list of contacts that should never see you
 */
void CmdPktCmdInvisList (Connection *conn)
{
    ContactGroup *cg = conn->contacts;
    Contact *cont;
    Packet *pak = NULL;
    UWORD pbytes = 0;
    int i, j;

    for (i = j = 0; (cont = ContactIndex (cg, j)); j++)
    {
        if (cont->flags & CONT_ALIAS || ~cont->flags & CONT_HIDEFROM)
            continue;

        if (!pak)
        {
            pak = PacketCv5 (conn, CMD_INVIS_LIST);
            pbytes = PacketWritePos (pak);
            PacketWrite1 (pak, 0);
        }
        PacketWrite4 (pak, cont->uin);
        i++;

        if (i < MAX_CONTS_PACKET)
            continue;

        PacketWriteAt1 (pak, pbytes, i);
        PacketEnqueuev5 (pak, conn);
        pak = NULL;
    }
    if (pak)
    {
        PacketWriteAt1 (pak, pbytes, i);
        PacketEnqueuev5 (pak, conn);
    }
}

/*
 * CMD_VIS_LIST - send list of contacts that may see you while being invisible
 */
void CmdPktCmdVisList (Connection *conn)
{
    ContactGroup *cg = conn->contacts;
    Contact *cont;
    Packet *pak = NULL;
    UWORD pbytes = 0;
    int i, j;

    for (i = j = 0; (cont = ContactIndex (cg, j)); j++)
    {
        if (cont->flags & CONT_ALIAS || ~cont->flags & CONT_INTIMATE)
            continue;

        if (!pak)
        {
            pak = PacketCv5 (conn, CMD_INVIS_LIST);
            pbytes = PacketWritePos (pak);
            PacketWrite1 (pak, 0);
        }
        PacketWrite4 (pak, cont->uin);
        i++;

        if (i < MAX_CONTS_PACKET)
            continue;

        PacketWriteAt1 (pak, pbytes, i);
        PacketEnqueuev5 (pak, conn);
        pak = NULL;
    }
    if (pak)
    {
        PacketWriteAt1 (pak, pbytes, i);
        PacketEnqueuev5 (pak, conn);
    }
}

/*
 * CMD_UPDATE_LIST - update contact visible/invisible status
 */
void CmdPktCmdUpdateList (Connection *conn, UDWORD uin, int which, BOOL add)
{
    Packet *pak = PacketCv5 (conn, CMD_UPDATE_LIST);
    PacketWrite4 (pak, uin);
    PacketWrite1 (pak, which);
    PacketWrite1 (pak, add);
    PacketEnqueuev5 (pak, conn);
}
