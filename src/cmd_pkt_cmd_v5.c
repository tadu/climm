
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
#include "contact.h"
#include "cmd_pkt_cmd_v5.h"
#include "cmd_pkt_cmd_v5_util.h"
#include "preferences.h"
#include "util.h"
#include "util_ui.h"
#include "buildmark.h"
#include <string.h>
#include <assert.h>
#include <netinet/in.h> /* for htonl, htons */

/*
 * CMD_ACK - acknowledge a received packet.
 */
void CmdPktCmdAck (Session *sess, UDWORD seq)
{
    Packet *pak = PacketCv5 (sess, CMD_ACK);
    PacketWrite4    (pak, rand ());
    PacketWriteAt4  (pak, CMD_v5_OFF_SEQ, seq);
    PacketEnqueuev5 (pak, sess);
}

/*
 * CMD_SEND_MESSAGE - send a message through ICQ.
 */
void CmdPktCmdSendMessage (Session *sess, UDWORD uin, const char *text, UDWORD type)
{
    Packet *pak = PacketCv5 (sess, CMD_SEND_MESSAGE);
    
    UtilCheckUIN (sess, uin);
    
    if (uiG.last_message_sent) free (uiG.last_message_sent);
    uiG.last_message_sent = strdup (text);
    uiG.last_message_sent_type = type;
    
    Time_Stamp ();
    M_print (" " COLSENT "%10s" COLNONE " " MSGSENTSTR "%s\n", ContactFindName (uin), MsgEllipsis (text));

    PacketWrite4 (pak, uin);
    PacketWrite2 (pak, type);
    PacketWriteStrCUW (pak, text);
    PacketEnqueuev5 (pak, sess);
}

/*
 * CMD_TCP_REQUEST - request peer to open a TCP connection.
 */
void CmdPktCmdTCPRequest (Session *sess, UDWORD tuin, UDWORD port)
{
    Packet *pak;

    if (!sess->assoc || !(sess->assoc->connect & CONNECT_OK))
        return;

    pak = PacketCv5 (sess, CMD_TCP_REQUEST);
    PacketWrite4 (pak, tuin);
    PacketWrite4 (pak, sess->our_local_ip);
    PacketWrite4 (pak, sess->assoc->port);
    PacketWrite1 (pak, 0x04);
    PacketWrite4 (pak, port);
    PacketWrite4 (pak, sess->assoc->port);
    PacketWrite2 (pak, 2);
    PacketEnqueuev5 (pak, sess);
}

/*
 * CMD_LOGIN - start login process by sending login request.
 */
void CmdPktCmdLogin (Session *sess)
{
    Packet *pak;

    if (sess->our_seq2 != 1)
    {
        sess->our_seq     = rand () & 0x3fff;
        sess->our_seq2    = 1;
        sess->our_session = rand () & 0x3fffffff;
    }
    sess->ver = 5;
    
    assert (strlen (sess->passwd) <= 8);
    
    pak = PacketCv5 (sess, CMD_LOGIN);
    PacketWrite4 (pak, time (NULL));
    PacketWrite4 (pak, sess->assoc && sess->assoc->connect & CONNECT_OK ?
                       sess->assoc->port : 0);
    PacketWriteLNTS (pak, sess->passwd);
    PacketWrite4 (pak, 0x000000d5);
    PacketWrite4 (pak, sess->our_local_ip);
    PacketWrite1 (pak, sess->assoc && sess->assoc->connect & CONNECT_OK ?
                       0x04 : 0);         /* 1=firewall | 2=proxy | 4=tcp */
    PacketWrite4 (pak, prG->status);
    PacketWrite2 (pak, TCP_VER);      /* 6 */
    PacketWrite2 (pak, 0);
    PacketWrite4 (pak, 0x822c01ec);   /* 0x00d50008, 0x00780008 */
    PacketWrite4 (pak, 0x00000050);
    PacketWrite4 (pak, 0x00000003);
    PacketWrite4 (pak, BUILD_LICQ | BuildVersionNum);
    PacketEnqueuev5 (pak, sess);
}

/*
 * CMD_REG_NEW_USER - request a new user ID.
 */
void CmdPktCmdRegNewUser (Session *sess, const char *pass)
{
    Packet *pak = PacketCv5 (sess, CMD_REG_NEW_USER);

    assert (strlen (pass) <= 9);

    PacketWriteAt4 (pak, CMD_v5_OFF_UIN, 0); /* no UIN */
    PacketWriteLNTS (pak, pass);
    PacketWrite4   (pak, 0xA0);
    PacketWrite4   (pak, 0x2461);
    PacketWrite4   (pak, 0xA00000);
    PacketWrite4   (pak, 0x00);
    
    if (!sess->our_session)
        PacketWriteAt4 (pak, CMD_v5_OFF_SESS, rand () & 0x3fffffff);

    PacketEnqueuev5 (pak, sess);
}

/*
 * CMD_CONTACT_LIST - send over the contact list.
 */
void CmdPktCmdContactList (Session *sess)
{
    Contact *cont;
    Packet *pak;
    UWORD pbytes = 0;
    int i;

    for (cont = ContactStart (); ContactHasNext (cont); cont = ContactNext (cont))
    {
        for (i = 0, pak = NULL; i < MAX_CONTS_PACKET && ContactHasNext (cont); cont = ContactNext (cont))
        {
            if ((SDWORD) cont->uin > 0)
            {
                if (!pak)
                {
                    pak = PacketCv5 (sess, CMD_CONTACT_LIST);
                    pbytes = PacketWritePos (pak);
                    PacketWrite1 (pak, 0);
                }
                PacketWrite4 (pak, cont->uin);
                i++;
            }
        }
        if (pak)
        {
            PacketWriteAt1 (pak, pbytes, i);
            PacketEnqueuev5 (pak, sess);
        }
        if (!ContactHasNext (cont))
            break;
    }
}

/*
 * CMD_SEARCH_UIN - (not implemented)
 */

/*
 * CMD_SEARCH_USER - search for a user.
 */
void CmdPktCmdSearchUser (Session *sess, const char *email, const char *nick,
                                         const char *first, const char *last)
{
    Packet *pak = PacketCv5 (sess, CMD_SEARCH_USER);
    PacketWriteLNTS (pak, nick);
    PacketWriteLNTS (pak, first);
    PacketWriteLNTS (pak, last);
    PacketWriteLNTS (pak, email);
    PacketEnqueuev5 (pak, sess);
}

/*
 * CMD_KEEP_ALIVE, CMD_KEEP_ALIVE2 - send keep alive packets.
 */
void CmdPktCmdKeepAlive (Session *sess)
{
    Packet *pak = PacketCv5 (sess, CMD_KEEP_ALIVE);
    PacketWrite4 (pak, rand ());
    PacketEnqueuev5 (pak, sess);
#if 1
    pak = PacketCv5 (sess, CMD_KEEP_ALIVE2);
    PacketWrite4 (pak, rand ());
    PacketEnqueuev5 (pak, sess);
#endif
}

/*
 * CMD_SEND_TEXT_CODE - sends a text-based command to server.
 */
void CmdPktCmdSendTextCode (Session *sess, const char *text)
{
    Packet *pak = PacketCv5 (sess, CMD_SEND_TEXT_CODE);
    PacketWriteLNTS (pak, text);
    PacketWrite1 (pak, 5);
    PacketWrite1 (pak, 0);
    PacketEnqueuev5 (pak, sess);
}

/*
 * CMD_ACK_MESSAGES - acknowledge the receipt of all offline messages.
 */
void CmdPktCmdAckMessages (Session *sess)
{
    Packet *pak = PacketCv5 (sess, CMD_ACK_MESSAGES);
    PacketWrite2 (pak, rand ());
    PacketEnqueuev5 (pak, sess);
}

/*
 * CMD_LOGIN_1 - finishes login process.
 */
void CmdPktCmdLogin1 (Session *sess)
{
    Packet *pak = PacketCv5 (sess, CMD_LOGIN_1);
    PacketWrite4 (pak, rand ());
    PacketEnqueuev5 (pak, sess);
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
void CmdPktCmdExtInfoReq (Session *sess, UDWORD uin)
{
    Packet *pak = PacketCv5 (sess, CMD_EXT_INFO_REQ);
    PacketWrite4 (pak, uin);
    PacketEnqueuev5 (pak, sess);
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
void CmdPktCmdStatusChange (Session *sess, UDWORD status)
{
    Packet *pak = PacketCv5 (sess, CMD_STATUS_CHANGE);
    PacketWrite4 (pak, status);
    PacketEnqueuev5 (pak, sess);

    sess->status = status;
}

/*
 * CMD_NEW_USER_1 - (not implemented)
 */

/*
 * CMD_UPDATE_INFO - update basic info on server. (unused)
 */
void CmdPktCmdUpdateInfo (Session *sess, const char *email, const char *nick,
                                         const char *first, const char *last, BOOL auth)
{
    Packet *pak = PacketCv5 (sess, CMD_UPDATE_INFO);
    PacketWriteLNTS (pak, nick);
    PacketWriteLNTS (pak, first);
    PacketWriteLNTS (pak, last);
    PacketWriteLNTS (pak, email);
    PacketEnqueuev5 (pak, sess);

    pak = PacketCv5 (sess, CMD_AUTH_UPDATE);
    PacketWrite4 (pak, auth);
    PacketEnqueuev5 (pak, sess);
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
void CmdPktCmdRandSet (Session *sess, UDWORD group)
{
    Packet *pak = PacketCv5 (sess, CMD_RAND_SET);
    PacketWrite4 (pak, group);
    PacketEnqueuev5 (pak, sess);
}

/*
 * CMD_RAND_SEARCH - search a random user from given chat group.
 */
void CmdPktCmdRandSearch (Session *sess, UDWORD group)
{
    Packet *pak = PacketCv5 (sess, CMD_RAND_SEARCH);
    PacketWrite4 (pak, group);
    PacketEnqueuev5 (pak, sess);
}

/*
 * CMD_META_USER : META_SET_GENERAL_INFO - Update general information.
 */
void CmdPktCmdMetaGeneral (Session *sess, MetaGeneral *user)
{
    Packet *pak = PacketCv5 (sess, CMD_META_USER);
    PacketWrite2      (pak, META_SET_GENERAL_INFO);
    PacketWriteStrCUW (pak, user->nick);
    PacketWriteStrCUW (pak, user->first);
    PacketWriteStrCUW (pak, user->last);
    PacketWriteStrCUW (pak, user->email);
    PacketWriteStrCUW (pak, user->email2);
    PacketWriteStrCUW (pak, user->email3);
    PacketWriteStrCUW (pak, user->city);
    PacketWriteStrCUW (pak, user->state);
    PacketWriteStrCUW (pak, user->phone);
    PacketWriteStrCUW (pak, user->fax);
    PacketWriteStrCUW (pak, user->street);
    PacketWriteStrCUW (pak, user->cellular);
    PacketWrite4      (pak, user->zip);
    PacketWrite2      (pak, user->country);
    PacketWrite1      (pak, user->tz);
    PacketWrite1      (pak, !user->auth);
    PacketWrite1      (pak, user->webaware);
    PacketWrite1      (pak, user->hideip);
    PacketEnqueuev5   (pak, sess);
}

/*
 * CMD_META_USER : META_SET_WORK_INFO - (not implemented)
 */

/*
 * CMD_META_USER : META_SET_MORE_INFO - Update additional information.
 */
void CmdPktCmdMetaMore (Session *sess, MetaMore *info)
{
    Packet *pak = PacketCv5 (sess, CMD_META_USER);
    PacketWrite2    (pak, META_SET_MORE_INFO);
    PacketWrite2    (pak, info->age);
    PacketWrite1    (pak, info->sex);
    PacketWriteLNTS  (pak, info->hp);
    PacketWrite2    (pak, info->year);
    PacketWrite2    (pak, info->month);
    PacketWrite2    (pak, info->day);
    PacketWrite2    (pak, info->lang1);
    PacketWrite2    (pak, info->lang2);
    PacketWrite2    (pak, info->lang3);
    PacketEnqueuev5 (pak, sess);
}

/*
 * CMD_META_USER : META_SET_ABOUT_INFO - Set "about" information.
 */
void CmdPktCmdMetaAbout (Session *sess, const char *about)
{
    Packet *pak = PacketCv5 (sess, CMD_META_USER);
    PacketWrite2 (pak, META_SET_ABOUT_INFO);
    PacketWriteStrCUW (pak, about);
    PacketEnqueuev5 (pak, sess);
}

/*
 * CMD_META_USER : META_INFO_SECURE - (not implemented)
 */

/*
 * CMD_META_USER : META_SET_PASS - set a new password.
 */
void CmdPktCmdMetaPass (Session *sess, char *pass)
{
    Packet *pak = PacketCv5 (sess, CMD_META_USER);

    assert (strlen (pass) <= 9);

    PacketWrite2 (pak, META_SET_PASS);
    PacketWriteLNTS (pak, pass);
    PacketEnqueuev5 (pak, sess);
}

/*
 * CMD_META_USER : META_REQ_INFO - request information on user.
 */
void CmdPktCmdMetaReqInfo (Session *sess, UDWORD uin)
{
    Packet *pak = PacketCv5 (sess, CMD_META_USER);
    PacketWrite2 (pak, META_REQ_INFO);
    PacketWrite4 (pak, uin);
    PacketEnqueuev5 (pak, sess);
}

/*
 * CMD_META_USER : META_SEARCH_WP - do an extensive white page search.
 */
void CmdPktCmdMetaSearchWP (Session *sess, MetaWP *user)
{
    Packet *pak = PacketCv5 (sess, CMD_META_USER);
    PacketWrite2      (pak, META_SEARCH_WP);
    PacketWriteStrCUW (pak, user->first);
    PacketWriteStrCUW (pak, user->last);
    PacketWriteStrCUW (pak, user->nick);
    PacketWriteStrCUW (pak, user->email);
    PacketWrite2      (pak, user->minage);
    PacketWrite2      (pak, user->maxage);
    PacketWrite1      (pak, user->sex);
    PacketWrite2      (pak, user->language);
    PacketWriteStrCUW (pak, user->city);
    PacketWriteStrCUW (pak, user->state);
    PacketWrite2      (pak, user->country);
    PacketWriteStrCUW (pak, user->company);
    PacketWriteStrCUW (pak, user->department);
    PacketWriteStrCUW (pak, user->position);

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
    PacketEnqueuev5 (pak, sess);
}

/*
 * CMD_META_USER : META_SET_WEB_PRESENCE - (not implemented)
 */

/*
 * CMD_INVIS_LIST - send list of contacts that should never see you
 */
void CmdPktCmdInvisList (Session *sess)
{
    Contact *cont;
    Packet *pak;
    UWORD pbytes = 0;
    int i;

    for (cont = ContactStart (); ContactHasNext (cont); cont = ContactNext (cont))
    {
        for (i = 0, pak = NULL; i < MAX_CONTS_PACKET && ContactHasNext (cont); cont = ContactNext (cont))
        {
            if ((SDWORD) cont->uin > 0 && cont->invis_list)
            {
                if (!pak)
                {
                    pak = PacketCv5 (sess, CMD_INVIS_LIST);
                    pbytes = PacketWritePos (pak);
                    PacketWrite1 (pak, 0);
                }
                PacketWrite4 (pak, cont->uin);
                i++;
            }
        }
        if (pak)
        {
            PacketWriteAt1 (pak, pbytes, i);
            PacketEnqueuev5 (pak, sess);
        }
        if (!ContactHasNext (cont))
            break;
    }
}

/*
 * CMD_VIS_LIST - send list of contacts that may see you while being invisible
 */
void CmdPktCmdVisList (Session *sess)
{
    Contact *cont;
    Packet *pak;
    UWORD pbytes = 0;
    int i;

    for (cont = ContactStart (); ContactHasNext (cont); cont = ContactNext (cont))
    {
        for (i = 0, pak = NULL; i < MAX_CONTS_PACKET && ContactHasNext (cont); cont = ContactNext (cont))
        {
            if ((SDWORD) cont->uin > 0 && cont->vis_list)
            {
                if (!pak)
                {
                    pak = PacketCv5 (sess, CMD_VIS_LIST);
                    pbytes = PacketWritePos (pak);
                    PacketWrite1 (pak, 0);
                }
                PacketWrite4 (pak, cont->uin);
                i++;
            }
        }
        if (pak)
        {
            PacketWriteAt1 (pak, pbytes, i);
            PacketEnqueuev5 (pak, sess);
        }
        if (!ContactHasNext (cont))
            break;
    }
}

/*
 * CMD_UPDATE_LIST - update contact visible/invisible status
 */
void CmdPktCmdUpdateList (Session *sess, UDWORD uin, int which, BOOL add)
{
    Packet *pak = PacketCv5 (sess, CMD_UPDATE_LIST);
    PacketWrite4 (pak, uin);
    PacketWrite1 (pak, which);
    PacketWrite1 (pak, add);
    PacketEnqueuev5 (pak, sess);
}
