/* $Id$ */

/*
Send Message Function for ICQ... 
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Author : zed@mentasm.com
*/
#include <stdlib.h>
#include <stdio.h>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/socket.h>
#endif
#include <string.h>
#include <assert.h>
#include <limits.h>
#include "micq.h"
#include "util_ui.h"
#include "sendmsg.h"
#include "util.h"
#include "conv.h"
#include "tcp.h"

static size_t SOCKWRITE_LOW (SOK_T sok, void *ptr, size_t len);
static void Fill_Header (net_icq_pak * pak, UWORD cmd);
static void info_req_99 (SOK_T sok, UDWORD uin);
static void info_req_old (SOK_T sok, UDWORD uin);
static void Gen_Checksum (UBYTE * buf, UDWORD len);
static UDWORD Scramble_cc (UDWORD cc);
static void Wrinkle (void *buf, size_t len);

/* Historical */
void Initialize_Msg_Queue ()
{
    msg_queue_init (&queue);
#ifdef TCP_COMM
    msg_queue_init (&tcp_rq);
    msg_queue_init (&tcp_sq);
#endif
}

/****************************************************************
Checks if packets are waiting to be resent and sends them.
*****************************************************************/
void Do_Resend (SOK_T sok)
{
    struct msg *queued_msg;
    SIMPLE_MESSAGE_PTR s_mesg;
    UDWORD type;
    char *data;
    char *tmp;
    char *temp;
    char url_desc[1024], url_data[1024];
    net_icq_pak pak;

    if ((queued_msg = msg_queue_pop (queue)) != NULL)
    {
        queued_msg->attempts++;
        if (queued_msg->attempts <= MAX_RETRY_ATTEMPTS)
        {
            if (uiG.Verbose)
            {
                R_undraw ();
                M_print ("\n");
                M_print (i18n (624, "Resending message with SEQ num %04X CMD "), (queued_msg->seq >> 16));
                Print_CMD (Chars_2_Word (&queued_msg->body[CMD_OFFSET]));
                M_print (i18n (625, "(Attempt #%d.)"), queued_msg->attempts);
                M_print ("%d\n", queued_msg->len);
                R_redraw ();
            }
            if (0x1000 < Chars_2_Word (&queued_msg->body[CMD_OFFSET]))
            {
                Dump_Queue ();
            }
            temp = malloc (queued_msg->len + 3);        /* make sure packet fits in UDWORDS */
            assert (temp != NULL);
/*	    M_print( "Pre!\n" );*/
            memcpy (temp, queued_msg->body, queued_msg->len);
/*	    Hex_Dump( temp, queued_msg->len );*/
            SOCKWRITE_LOW (sok, temp, queued_msg->len);
            free (temp);
            queued_msg->exp_time = time (NULL) + 10;
            msg_queue_push (queued_msg, queue);
        }
        else
        {
            memcpy (&pak.head.ver, queued_msg->body, queued_msg->len);
            R_undraw ();
            if (CMD_SENDM == Chars_2_Word (pak.head.cmd))
            {
                s_mesg = (SIMPLE_MESSAGE_PTR) pak.data;
                M_print ("\n");
                M_print (i18n (626, "Discarding message to "));
                Print_UIN_Name (Chars_2_DW (s_mesg->uin));
                M_print (i18n (627, " after %d send attempts.  Message content:\n"), queued_msg->attempts - 1);

                type = Chars_2_Word (s_mesg->type);
                data = s_mesg->len + 2;
                if (type == URL_MESS || type == MRURL_MESS)
                {
                    tmp = strchr (data, '\xFE');
                    if (tmp != NULL)
                    {
                        *tmp = 0;
                        ConvUnixWin (data);
                        strcpy (url_desc, data);
                        tmp++;
                        data = tmp;
                        ConvUnixWin (data);
                        strcpy (url_data, data);

                        M_print (i18n (628, " Description: " COLMESS "%s" COLNONE "\n"), url_desc);
                        M_print (i18n (629, " URL:         " COLMESS "%s" COLNONE), url_data);
                    }
                }
                else if (type == NORM_MESS || type == MRNORM_MESS)
                {
                    ConvUnixWin (data);
                    M_print (COLMESS "%s", data);
                    M_print (COLNONE " ");
                }
            }
            else
            {
                M_print ("\n");
                M_print (i18n (630, "Discarded a "));
                Print_CMD (Chars_2_Word (pak.head.cmd));
                M_print (i18n (631, " packet."));
                if ((CMD_LOGIN == Chars_2_Word (pak.head.cmd))
                    || (CMD_KEEP_ALIVE == Chars_2_Word (pak.head.cmd)))
                {
                    M_print (i18n (632, "\n\aConnection unstable. Exiting...."));
                    ssG.Quit = TRUE;
                }
            }
            M_print ("\n");
            R_redraw ();

            free (queued_msg->body);
            free (queued_msg);
        }

        if ((queued_msg = msg_queue_peek (queue)) != NULL)
        {
            ssG.next_resend = queued_msg->exp_time;
        }
        else
        {
            ssG.next_resend = INT_MAX;
        }
    }
    else
    {
        ssG.next_resend = INT_MAX;
    }
}

/**************************************************
Fills in the header information for a packet.
***************************************************/
void Fill_Header (net_icq_pak * pak, UWORD cmd)
{
    Word_2_Chars (pak->head.ver, ICQ_VER);
    Word_2_Chars (pak->head.cmd, cmd);
    Word_2_Chars (pak->head.seq, ssG.seq_num++);
    DW_2_Chars (pak->head.UIN, ssG.UIN);
    DW_2_Chars (pak->data, rand ());
}

void Update_Other (SOK_T sok, OTHER_INFO_PTR info)
{
    net_icq_pak pak;
    UBYTE *buf;

    Fill_Header (&pak, CMD_META_USER);

    buf = pak.data;
    Word_2_Chars (buf, META_INFO_MORE);
    buf += 2;
    Word_2_Chars (buf, info->age);
    buf += 2;
    *buf++ = info->sex;
    Word_2_Chars (buf, strlen (info->hp) + 1);
    buf += 2;
    strcpy (buf, info->hp);
    buf += strlen (info->hp) + 1;
    Word_2_Chars (buf++, info->year);
    Word_2_Chars (buf++, info->month);
    Word_2_Chars (buf++, info->day);
    Word_2_Chars (buf++, info->lang1);
    Word_2_Chars (buf++, info->lang2);
    Word_2_Chars (buf++, info->lang3);

    ssG.last_cmd[(ssG.seq_num - 1) & 0x3ff] = Chars_2_Word (pak.head.cmd);
    SOCKWRITE (sok, &(pak.head.ver), sizeof (pak.head) - 2 + (buf - pak.data));
}

/*********************************
This must be called to remove messages
from the server
**********************************/
void snd_got_messages (SOK_T sok)
{
    net_icq_pak pak;

    Word_2_Chars (pak.head.ver, ICQ_VER);
    Word_2_Chars (pak.head.cmd, CMD_ACK_MESSAGES);
    Word_2_Chars (pak.head.seq, ssG.seq_num++);
    DW_2_Chars (pak.head.UIN, ssG.UIN);
    DW_2_Chars (pak.data, rand ());

    ssG.last_cmd[(ssG.seq_num - 1) & 0x3ff] = Chars_2_Word (pak.head.cmd);
    SOCKWRITE (sok, &(pak.head.ver), sizeof (pak.head) - 2 + 4);
}

/************************************************
Updates the user's about info.
*************************************************/
void Update_About (SOK_T sok, const char *about)
{
    net_icq_pak pak;

    Word_2_Chars (pak.head.ver, ICQ_VER);
    Word_2_Chars (pak.head.cmd, CMD_META_USER);
    Word_2_Chars (pak.head.seq, ssG.seq_num++);
    DW_2_Chars (pak.head.UIN, ssG.UIN);
    Word_2_Chars (pak.data, META_INFO_ABOUT);
    Word_2_Chars (&pak.data[2], strlen (about) + 1);
    strcpy (&pak.data[4], about);
    ConvUnixWin (&pak.data[4]);
    pak.data[strlen (about) + 4] = 0;
    ssG.last_cmd[(ssG.seq_num - 1) & 0x3ff] = Chars_2_Word (pak.head.cmd);
    SOCKWRITE (sok, &(pak.head.ver), sizeof (pak.head) - 2 + 5 + strlen (about));
}

/*************************************
This adds or removes users from your
visible or invisible lists
**************************************/
void update_list (int sok, UDWORD uin, int which, BOOL add)
{
    net_icq_pak pak;

    Word_2_Chars (pak.head.ver, ICQ_VER);
    Word_2_Chars (pak.head.cmd, CMD_UPDATE_LIST);
    Word_2_Chars (pak.head.seq, ssG.seq_num++);
    DW_2_Chars (pak.head.UIN, ssG.UIN);

    DW_2_Chars (pak.data, uin);
    pak.data[4] = which;
    pak.data[5] = add;

    ssG.last_cmd[(ssG.seq_num - 1) & 0x3ff] = Chars_2_Word (pak.head.cmd);
    SOCKWRITE (sok, &(pak.head.ver), sizeof (pak.head) - 2 + 6);
}

/*************************************
this sends over the contact list
**************************************/
void snd_contact_list (int sok)
{
    net_icq_pak pak;
    int num_used;
    int i, size;
    int j;
    char *tmp;

    for (i = 0; i < uiG.Num_Contacts;)
    {
        Word_2_Chars (pak.head.ver, ICQ_VER);
        Word_2_Chars (pak.head.cmd, CMD_CONT_LIST);
        Word_2_Chars (pak.head.seq, ssG.seq_num++);
        DW_2_Chars (pak.head.UIN, ssG.UIN);

        tmp = pak.data;
        tmp++;
        for (j = 0, num_used = 0; (j < MAX_CONTS_PACKET) && (i < uiG.Num_Contacts); i++)
        {
            if ((SDWORD) uiG.Contacts[i].uin > 0)
            {
                DW_2_Chars (tmp, uiG.Contacts[i].uin);
                tmp += 4;
                num_used++;
                j++;
            }
        }
        pak.data[0] = num_used;
        size = sizeof (UDWORD) * num_used + 1;
        size += sizeof (pak.head) - 2;
        ssG.last_cmd[ssG.seq_num - 1] = Chars_2_Word (pak.head.cmd);
        SOCKWRITE (sok, &(pak.head.ver), size);
/*      M_print( "Sent %d Contacts.\n", num_used );*/
    }
}

/*************************************
this sends over the Invisible list
that allows certain users to see you
if you're invisible.
**************************************/
void snd_invis_list (int sok)
{
    net_icq_pak pak;
    int num_used;
    int i, size;
    char *tmp;

    Word_2_Chars (pak.head.ver, ICQ_VER);
    Word_2_Chars (pak.head.cmd, CMD_INVIS_LIST);
    Word_2_Chars (pak.head.seq, ssG.seq_num);
    DW_2_Chars (pak.head.UIN, ssG.UIN);

    tmp = pak.data;
    tmp++;
    for (i = 0, num_used = 0; i < uiG.Num_Contacts; i++)
    {
        if ((SDWORD) uiG.Contacts[i].uin > 0)
        {
            if (uiG.Contacts[i].invis_list)
            {
                DW_2_Chars (tmp, uiG.Contacts[i].uin);
                tmp += 4;
                num_used++;
            }
        }
    }
    if (num_used != 0)
    {
        pak.data[0] = num_used;
        size = 1 + num_used * 4;        /* ssG.UIN's are 4 bytes this is unlikely to ever change */
        /*size = ( ( int ) tmp - ( int ) pak.data ); */
        size += sizeof (pak.head) - 2;
        ssG.last_cmd[ssG.seq_num - 1] = Chars_2_Word (pak.head.cmd);
        SOCKWRITE (sok, &(pak.head.ver), size);
        ssG.seq_num++;
    }
}

/*************************************
this sends over the Visible list
that allows certain users to see you
if you're invisible.
**************************************/
void snd_vis_list (int sok)
{
    net_icq_pak pak;
    int num_used;
    int i, size;
    char *tmp;

    Word_2_Chars (pak.head.ver, ICQ_VER);
    Word_2_Chars (pak.head.cmd, CMD_VIS_LIST);
    Word_2_Chars (pak.head.seq, ssG.seq_num);
    DW_2_Chars (pak.head.UIN, ssG.UIN);

    tmp = pak.data;
    tmp++;
    for (i = 0, num_used = 0; i < uiG.Num_Contacts; i++)
    {
        if ((SDWORD) uiG.Contacts[i].uin > 0)
        {
            if (uiG.Contacts[i].vis_list)
            {
                DW_2_Chars (tmp, uiG.Contacts[i].uin);
                tmp += 4;
                num_used++;
                if (uiG.Contacts[i].invis_list)
                {
                    M_print (i18n (633, "ACK!!! %d\n"), uiG.Contacts[i].uin);
                }
            }
        }
    }
    if (num_used != 0)
    {
        pak.data[0] = num_used;
        size = 1 + num_used * 4;        /* ssG.UIN's are 4 bytes (32 bits) */
        /*size = ( ( int ) tmp - ( int ) pak.data ); */
        size += sizeof (pak.head) - 2;
        ssG.last_cmd[ssG.seq_num - 1] = Chars_2_Word (pak.head.cmd);
        SOCKWRITE (sok, &(pak.head.ver), size);
        ssG.seq_num++;
    }
}

/**************************************
This sends the second login command
this is necessary to finish logging in.
***************************************/
void snd_login_1 (int sok)
{
    net_icq_pak pak;

    Word_2_Chars (pak.head.ver, ICQ_VER);
    Word_2_Chars (pak.head.cmd, CMD_LOGIN_1);
    Word_2_Chars (pak.head.seq, ssG.seq_num++);
    DW_2_Chars (pak.head.UIN, ssG.UIN);
    DW_2_Chars (pak.data, rand ());

    ssG.last_cmd[ssG.seq_num - 1] = Chars_2_Word (pak.head.cmd);
    SOCKWRITE (sok, &(pak.head.ver), sizeof (pak.head) - 2 + 4);
}

/*********************************
This must be called every 2 min.
so the server knows we're still alive.
JAVA client sends two different commands
so we do also :)
**********************************/
void Keep_Alive (int sok)
{
    net_icq_pak pak;

    Word_2_Chars (pak.head.ver, ICQ_VER);
    Word_2_Chars (pak.head.cmd, CMD_KEEP_ALIVE);
    Word_2_Chars (pak.head.seq, ssG.seq_num++);
    DW_2_Chars (pak.head.UIN, ssG.UIN);
    DW_2_Chars (pak.data, rand ());

    ssG.last_cmd[(ssG.seq_num - 1) & 0x3ff] = Chars_2_Word (pak.head.cmd);
    SOCKWRITE (sok, &(pak.head.ver), sizeof (pak.head) - 2 + 4);
#if 1
    Word_2_Chars (pak.head.ver, ICQ_VER);
    Word_2_Chars (pak.head.cmd, CMD_KEEP_ALIVE2);
    Word_2_Chars (pak.head.seq, ssG.seq_num++);
    DW_2_Chars (pak.head.UIN, ssG.UIN);
    DW_2_Chars (pak.data, rand ());

    ssG.last_cmd[(ssG.seq_num - 1) & 0x3ff] = Chars_2_Word (pak.head.cmd);
    SOCKWRITE (sok, &(pak.head.ver), sizeof (pak.head) - 2 + 4);
#endif

    if (uiG.Verbose)
    {
        R_undraw ();
        M_print (i18n (634, "\nSend Keep_Alive packet to the server\n"));
        R_redraw ();
    }
}

/********************************************************
The following data constitutes fair use for compatibility.
*********************************************************/
static const UBYTE table[] = {
    0x59, 0x60, 0x37, 0x6B, 0x65, 0x62, 0x46, 0x48, 0x53, 0x61, 0x4C, 0x59, 0x60, 0x57, 0x5B, 0x3D,
    0x5E, 0x34, 0x6D, 0x36, 0x50, 0x3F, 0x6F, 0x67, 0x53, 0x61, 0x4C, 0x59, 0x40, 0x47, 0x63, 0x39,
    0x50, 0x5F, 0x5F, 0x3F, 0x6F, 0x47, 0x43, 0x69, 0x48, 0x33, 0x31, 0x64, 0x35, 0x5A, 0x4A, 0x42,
    0x56, 0x40, 0x67, 0x53, 0x41, 0x07, 0x6C, 0x49, 0x58, 0x3B, 0x4D, 0x46, 0x68, 0x43, 0x69, 0x48,
    0x33, 0x31, 0x44, 0x65, 0x62, 0x46, 0x48, 0x53, 0x41, 0x07, 0x6C, 0x69, 0x48, 0x33, 0x51, 0x54,
    0x5D, 0x4E, 0x6C, 0x49, 0x38, 0x4B, 0x55, 0x4A, 0x62, 0x46, 0x48, 0x33, 0x51, 0x34, 0x6D, 0x36,
    0x50, 0x5F, 0x5F, 0x5F, 0x3F, 0x6F, 0x47, 0x63, 0x59, 0x40, 0x67, 0x33, 0x31, 0x64, 0x35, 0x5A,
    0x6A, 0x52, 0x6E, 0x3C, 0x51, 0x34, 0x6D, 0x36, 0x50, 0x5F, 0x5F, 0x3F, 0x4F, 0x37, 0x4B, 0x35,
    0x5A, 0x4A, 0x62, 0x66, 0x58, 0x3B, 0x4D, 0x66, 0x58, 0x5B, 0x5D, 0x4E, 0x6C, 0x49, 0x58, 0x3B,
    0x4D, 0x66, 0x58, 0x3B, 0x4D, 0x46, 0x48, 0x53, 0x61, 0x4C, 0x59, 0x40, 0x67, 0x33, 0x31, 0x64,
    0x55, 0x6A, 0x32, 0x3E, 0x44, 0x45, 0x52, 0x6E, 0x3C, 0x31, 0x64, 0x55, 0x6A, 0x52, 0x4E, 0x6C,
    0x69, 0x48, 0x53, 0x61, 0x4C, 0x39, 0x30, 0x6F, 0x47, 0x63, 0x59, 0x60, 0x57, 0x5B, 0x3D, 0x3E,
    0x64, 0x35, 0x3A, 0x3A, 0x5A, 0x6A, 0x52, 0x4E, 0x6C, 0x69, 0x48, 0x53, 0x61, 0x6C, 0x49, 0x58,
    0x3B, 0x4D, 0x46, 0x68, 0x63, 0x39, 0x50, 0x5F, 0x5F, 0x3F, 0x6F, 0x67, 0x53, 0x41, 0x25, 0x41,
    0x3C, 0x51, 0x54, 0x3D, 0x5E, 0x54, 0x5D, 0x4E, 0x4C, 0x39, 0x50, 0x5F, 0x5F, 0x5F, 0x3F, 0x6F,
    0x47, 0x43, 0x69, 0x48, 0x33, 0x51, 0x54, 0x5D, 0x6E, 0x3C, 0x31, 0x64, 0x35, 0x5A, 0x00, 0x00,
};

/***************************************************
Sends a message to uin. Text is the message to send.
***************************************************/
void icq_sendmsg( SOK_T sok, UDWORD uin, char *text, UDWORD msg_type)
{	
#ifdef TCP_COMM

    CONTACT_PTR cont;

    /* attempt to send message directly if user is on contact list */
    cont = UIN2Contact (uin);
    if ((cont->connection_type == TCP_OK_FLAG) && (cont->status != STATUS_OFFLINE))
    {
        Send_TCP_Msg (sok, uin, text, msg_type);
    }
    else        
    {
#endif
    /* send message via server */
    icq_sendmsg_srv (sok, uin, text, msg_type);
#ifdef TCP_COMM  
  }
#endif
}


/***************************************************
Sends a message thru the server to uin.  Text is the
message to send.
****************************************************/
void icq_sendmsg_srv (SOK_T sok, UDWORD uin, char *text, UDWORD msg_type)
{
    SIMPLE_MESSAGE msg;
    net_icq_pak pak;
    int size, len;
    if (ssG.last_message_sent != NULL) free(ssG.last_message_sent);
    ssG.last_message_sent = strdup(text);
    ssG.last_message_sent_type = msg_type;
    if (UIN2nick (uin) != NULL)
        log_event (uin, LOG_MESS, "You sent instant message to %s\n%s\n", UIN2nick (uin), text);
    else
        log_event (uin, LOG_MESS, "You sent instant message to %d\n%s\n", uin, text);

    ConvUnixWin (text);

    len = strlen (text);
    Word_2_Chars (pak.head.ver, ICQ_VER);
    Word_2_Chars (pak.head.cmd, CMD_SENDM);
    Word_2_Chars (pak.head.seq, ssG.seq_num++);
    DW_2_Chars (pak.head.UIN, ssG.UIN);

    DW_2_Chars (msg.uin, uin);
    Word_2_Chars (msg.type, msg_type);
    Word_2_Chars (msg.len, len + 1);    /* length + the NULL */

    memcpy (&pak.data, &msg, sizeof (msg));
    memcpy (&pak.data[8], text, len + 1);

    size = sizeof (msg) + len + 1;

    ssG.last_cmd[ssG.seq_num - 1] = Chars_2_Word (pak.head.cmd);
    SOCKWRITE (sok, &(pak.head.ver), size + sizeof (pak.head) - 2);
}

/**************************************************
Sends a authorization to the server so the Mirabilis
client can add the user.
***************************************************/
void icq_sendauthmsg (SOK_T sok, UDWORD uin)
{
    SIMPLE_MESSAGE msg;
    net_icq_pak pak;
    int size;

    Word_2_Chars (pak.head.ver, ICQ_VER);
    Word_2_Chars (pak.head.cmd, CMD_SENDM);
    Word_2_Chars (pak.head.seq, ssG.seq_num++);
    DW_2_Chars (pak.head.UIN, ssG.UIN);

    DW_2_Chars (msg.uin, uin);
    DW_2_Chars (msg.type, AUTH_MESSAGE);        /* A type authorization msg */
    Word_2_Chars (msg.len, 2);

    memcpy (&pak.data, &msg, sizeof (msg));

    pak.data[sizeof (msg)] = 0x03;
    pak.data[sizeof (msg) + 1] = 0x00;

    size = sizeof (msg) + 2;

    ssG.last_cmd[ssG.seq_num - 1] = Chars_2_Word (pak.head.cmd);
    SOCKWRITE (sok, &(pak.head.ver), size + sizeof (pak.head) - 2);
}

/***************************************************
Requests a random user from the specified group.
****************************************************/
void icq_rand_user_req (SOK_T sok, UDWORD group)
{
    net_icq_pak pak;
    int size;


    Word_2_Chars (pak.head.ver, ICQ_VER);
    Word_2_Chars (pak.head.cmd, CMD_RAND_SEARCH);
    Word_2_Chars (pak.head.seq, ssG.seq_num++);
    DW_2_Chars (pak.head.UIN, ssG.UIN);

    DW_2_Chars (pak.data, group);

    size = 4;

    ssG.last_cmd[ssG.seq_num - 1] = Chars_2_Word (pak.head.cmd);
    SOCKWRITE (sok, &(pak.head.ver), size + sizeof (pak.head) - 2);
}

/***************************************************
Sets our Random chat group
****************************************************/
void icq_rand_set (SOK_T sok, UDWORD group)
{
    net_icq_pak pak;
    int size;


    Word_2_Chars (pak.head.ver, ICQ_VER);
    Word_2_Chars (pak.head.cmd, CMD_RAND_SET);
    Word_2_Chars (pak.head.seq, ssG.seq_num++);
    DW_2_Chars (pak.head.UIN, ssG.UIN);

    DW_2_Chars (pak.data, group);

    size = 4;

    ssG.last_cmd[ssG.seq_num - 1] = Chars_2_Word (pak.head.cmd);
    SOCKWRITE (sok, &(pak.head.ver), size + sizeof (pak.head) - 2);
}


/***************************************************
Changes the users status on the server
****************************************************/
void icq_change_status (SOK_T sok, UDWORD status)
{
    net_icq_pak pak;
    int size;


    Word_2_Chars (pak.head.ver, ICQ_VER);
    Word_2_Chars (pak.head.cmd, CMD_STATUS_CHANGE);
    Word_2_Chars (pak.head.seq, ssG.seq_num++);
    DW_2_Chars (pak.head.UIN, ssG.UIN);

    DW_2_Chars (pak.data, status);
    uiG.Current_Status = status;

    size = 4;

    ssG.last_cmd[ssG.seq_num - 1] = Chars_2_Word (pak.head.cmd);
    SOCKWRITE (sok, &(pak.head.ver), size + sizeof (pak.head) - 2);
}

/******************************************************************
Logs off ICQ should handle other cleanup as well
********************************************************************/
void Quit_ICQ (int sok)
{
    net_icq_pak pak;
    int size, len;

    Word_2_Chars (pak.head.ver, ICQ_VER);
    Word_2_Chars (pak.head.cmd, CMD_SEND_TEXT_CODE);
    Word_2_Chars (pak.head.seq, ssG.seq_num++);
    DW_2_Chars (pak.head.UIN, ssG.UIN);

    len = strlen ("B_USER_DISCONNECTED") + 1;
    *(short *) pak.data = len;
    size = len + 4;

    memcpy (&pak.data[2], "B_USER_DISCONNECTED", len);
    pak.data[2 + len] = 05;
    pak.data[3 + len] = 00;

    ssG.last_cmd[ssG.seq_num - 1] = Chars_2_Word (pak.head.cmd);
    SOCKWRITE (sok, &(pak.head.ver), size + sizeof (pak.head) - 2);

    SOCKCLOSE (sok);
    SOCKCLOSE (sok);
}

void info_req_99 (SOK_T sok, UDWORD uin)
{
    net_icq_pak pak;
    int size;

    Word_2_Chars (pak.head.ver, ICQ_VER);
    Word_2_Chars (pak.head.cmd, CMD_META_USER);
    Word_2_Chars (pak.head.seq, ssG.seq_num++);
    DW_2_Chars (pak.head.UIN, ssG.UIN);
#if ICQ_VER == 0x0002
    Word_2_Chars (pak.data, ssG.seq_num);
    DW_2_Chars (pak.data + 2, uin);

    size = 6;
#else
    Word_2_Chars (pak.data, META_INFO_REQ);
    DW_2_Chars (pak.data + 2, uin);

    size = 6;
#endif

    ssG.last_cmd[ssG.seq_num - 1] = Chars_2_Word (pak.head.cmd);
    SOCKWRITE (sok, &(pak.head.ver), size + sizeof (pak.head) - 2);

}

void info_req_old (SOK_T sok, UDWORD uin)
{
    net_icq_pak pak;
    int size;

#if 1
    Word_2_Chars (pak.head.ver, ICQ_VER);
    Word_2_Chars (pak.head.cmd, CMD_INFO_REQ);
    Word_2_Chars (pak.head.seq, ssG.seq_num++);
    DW_2_Chars (pak.head.UIN, ssG.UIN);
#if ICQ_VER == 0x0002
    Word_2_Chars (pak.data, ssG.seq_num);
    DW_2_Chars (pak.data + 2, uin);

    size = 6;
#else
    DW_2_Chars (pak.data, uin);

    size = 4;
#endif

    ssG.last_cmd[ssG.seq_num - 1] = Chars_2_Word (pak.head.cmd);
    SOCKWRITE (sok, &(pak.head.ver), size + sizeof (pak.head) - 2);
#endif
}

/*********************************************************
Sends a request to the server for info on a specific user
**********************************************************/
void send_info_req (SOK_T sok, UDWORD uin)
{
#if 0
    info_req_old (sok, uin);
#else
    info_req_99 (sok, uin);
#endif
}

/*********************************************************
Sends a request to the server for info on a specific user
**********************************************************/
void send_ext_info_req (SOK_T sok, UDWORD uin)
{
    net_icq_pak pak;
    int size;


    Word_2_Chars (pak.head.ver, ICQ_VER);
    Word_2_Chars (pak.head.cmd, CMD_EXT_INFO_REQ);
    Word_2_Chars (pak.head.seq, ssG.seq_num++);
    DW_2_Chars (pak.head.UIN, ssG.UIN);

#if ICQ_VER == 0x0002
    Word_2_Chars (pak.data, ssG.seq_num);
    DW_2_Chars (pak.data + 2, uin);
    size = 6;
#else
    DW_2_Chars (pak.data, uin);
    size = 4;
#endif


    ssG.last_cmd[ssG.seq_num - 1] = Chars_2_Word (pak.head.cmd);
    SOCKWRITE (sok, &(pak.head.ver), size + sizeof (pak.head) - 2);
}

/*
 * Initializes a server search for the information specified
 */
void start_search (SOK_T sok, char *email, char *nick, char *first, char *last)
{
    net_icq_pak pak;
    int size;

    Word_2_Chars (pak.head.ver, ICQ_VER);
    Word_2_Chars (pak.head.cmd, CMD_SEARCH_USER);
    Word_2_Chars (pak.head.seq, ssG.seq_num++);
    DW_2_Chars (pak.head.UIN, ssG.UIN);
/*
   Word_2_Chars( pak.data , ssG.seq_num++ );
   size = 2;
*/
    size = 0;
    Word_2_Chars (pak.data + size, strlen (nick) + 1);
    size += 2;
    strcpy (pak.data + size, nick);
    size += strlen (nick) + 1;
    Word_2_Chars (pak.data + size, strlen (first) + 1);
    size += 2;
    strcpy (pak.data + size, first);
    size += strlen (first) + 1;
    Word_2_Chars (pak.data + size, strlen (last) + 1);
    size += 2;
    strcpy (pak.data + size, last);
    size += strlen (last) + 1;
    Word_2_Chars (pak.data + size, strlen (email) + 1);
    size += 2;
    strcpy (pak.data + size, email);
    size += strlen (email) + 1;
    ssG.last_cmd[ssG.seq_num - 2] = Chars_2_Word (pak.head.cmd);
    SOCKWRITE (sok, &(pak.head.ver), size + sizeof (pak.head) - 2);
}

/***************************************************
Registers a new UIN in the ICQ network
****************************************************/
void reg_new_user (SOK_T sok, char *pass)
{
#if ICQ_VER == 0x0002
    srv_net_icq_pak pak;
#else
    net_icq_pak pak;
#endif
    char len_buf[2];
    int size, len;

    len = strlen (pass);
    Word_2_Chars (pak.head.ver, ICQ_VER);
    Word_2_Chars (pak.head.cmd, CMD_REG_NEW_USER);
    Word_2_Chars (pak.head.seq, ssG.seq_num++);
#if ICQ_VER != 0x0002
    Word_2_Chars (pak.head.seq2, ssG.seq_num - 1);
#endif
    Word_2_Chars (len_buf, len);
#if ICQ_VER == 0x0002
    memcpy (&pak.data, "\x02\x00", 2);
    memcpy (&pak.data[2], len_buf, 2);
    memcpy (&pak.data[4], pass, len + 1);
    DW_2_Chars (&pak.data[4 + len], 0x0072);
    DW_2_Chars (&pak.data[8 + len], 0x0000);
    size = len + 12;
#else
/*	memcpy(&pak.data, "\x02\x00", 2 );*/
    memcpy (&pak.data[0], len_buf, 2);
    memcpy (&pak.data[2], pass, len + 1);
    DW_2_Chars (&pak.data[2 + len], 0xA0);
    DW_2_Chars (&pak.data[6 + len], 0x2461);
    DW_2_Chars (&pak.data[10 + len], 0xa00000);
    DW_2_Chars (&pak.data[14 + len], 0x00);
    size = len + 18;
#endif
#if ICQ_VER == 0x0005
    if (ssG.our_session == 0)
    {
        ssG.our_session = rand ();
        DW_2_Chars (pak.head.session, ssG.our_session);
        ssG.our_session = 0;
    }
    else
    {
        DW_2_Chars (pak.head.session, ssG.our_session);
    }
    DW_2_Chars (pak.head.zero, 0L);
    DW_2_Chars (pak.head.UIN, 0L);
#endif

    ssG.last_cmd[ssG.seq_num - 1] = Chars_2_Word (pak.head.cmd);
    SOCKWRITE_LOW (sok, &(pak.head.ver), size + sizeof (pak.head) - 2);
}

/******************************************************
Changes the password on the server.
*******************************************************/
void Change_Password (SOK_T sok, char *pass)
{
#if ICQ_VER == 0x0002
    srv_net_icq_pak pak;
#else
    net_icq_pak pak;
#endif
    char len_buf[2];
    int size, len;

    len = strlen (pass);
    len++;
    if (len > 9)
        len = 9;
    Word_2_Chars (pak.head.ver, ICQ_VER);
    Word_2_Chars (pak.head.cmd, CMD_META_USER);
    Word_2_Chars (pak.head.seq, ssG.seq_num++);
    DW_2_Chars (pak.head.UIN, ssG.UIN);
#if ICQ_VER != 0x0002
    Word_2_Chars (pak.head.seq2, ssG.seq_num - 1);
#endif
    Word_2_Chars (len_buf, len);
    Word_2_Chars (pak.data, META_INFO_PASS);
    memcpy (&pak.data[2], len_buf, 2);
    memcpy (&pak.data[4], pass, len);
    size = len + 4;

    ssG.last_cmd[ssG.seq_num - 1] = Chars_2_Word (pak.head.cmd);
    SOCKWRITE (sok, &(pak.head.ver), size + sizeof (pak.head) - 2);
}

void Update_User_Info (SOK_T sok, USER_INFO_PTR user)
{
    net_icq_pak pak;
    int size;

    Word_2_Chars (pak.head.ver, ICQ_VER);
    Word_2_Chars (pak.head.cmd, CMD_UPDATE_INFO);
    Word_2_Chars (pak.head.seq, ssG.seq_num++);
    DW_2_Chars (pak.head.UIN, ssG.UIN);

#if ICQ_VER == 0x0002
    Word_2_Chars (pak.data, ssG.seq_num++);
    size = 2;
#else
    size = 0;
#endif
    Word_2_Chars (pak.data + size, strlen (user->nick) + 1);
    size += 2;
    strcpy (pak.data + size, user->nick);
    size += strlen (user->nick) + 1;
    Word_2_Chars (pak.data + size, strlen (user->first) + 1);
    size += 2;
    strcpy (pak.data + size, user->first);
    size += strlen (user->first) + 1;
    Word_2_Chars (pak.data + size, strlen (user->last) + 1);
    size += 2;
    strcpy (pak.data + size, user->last);
    size += strlen (user->last) + 1;
    Word_2_Chars (pak.data + size, strlen (user->email) + 1);
    size += 2;
    strcpy (pak.data + size, user->email);
    size += strlen (user->email) + 1;
    pak.data[size] = user->auth;
    size++;
    ssG.last_cmd[(ssG.seq_num - 1) & 0x3ff] = Chars_2_Word (pak.head.cmd);
    SOCKWRITE (sok, &(pak.head.ver), size + sizeof (pak.head) - 2);

    Word_2_Chars (pak.head.ver, ICQ_VER);
    Word_2_Chars (pak.head.cmd, CMD_AUTH_UPDATE);
    Word_2_Chars (pak.head.seq, ssG.seq_num++);
    DW_2_Chars (pak.head.UIN, ssG.UIN);

    DW_2_Chars (pak.data, !user->auth);
    ssG.last_cmd[(ssG.seq_num - 1) & 0x3ff] = Chars_2_Word (pak.head.cmd);
    SOCKWRITE (sok, &(pak.head.ver), 4 + sizeof (pak.head) - 2);
}

void Update_More_User_Info (SOK_T sok, MORE_INFO_PTR user)
{
    net_icq_pak pak;
    int size;

    Word_2_Chars (pak.head.ver, ICQ_VER);
    Word_2_Chars (pak.head.cmd, CMD_META_USER);
    Word_2_Chars (pak.head.seq, ssG.seq_num++);
    DW_2_Chars (pak.head.UIN, ssG.UIN);

    Word_2_Chars (pak.data, META_INFO_SET);
    size = 2;
    Word_2_Chars (pak.data + size, strlen (user->nick) + 1);
    size += 2;
    ConvUnixWin (user->nick);
    strcpy (pak.data + size, user->nick);
    size += strlen (user->nick) + 1;
    Word_2_Chars (pak.data + size, strlen (user->first) + 1);
    size += 2;
    ConvUnixWin (user->first);
    strcpy (pak.data + size, user->first);
    size += strlen (user->first) + 1;
    Word_2_Chars (pak.data + size, strlen (user->last) + 1);
    size += 2;
    ConvUnixWin (user->last);
    strcpy (pak.data + size, user->last);
    size += strlen (user->last) + 1;
    Word_2_Chars (pak.data + size, strlen (user->email) + 1);
    size += 2;
    ConvUnixWin (user->email);
    strcpy (pak.data + size, user->email);
    size += strlen (user->email) + 1;
    Word_2_Chars (pak.data + size, strlen (user->email2) + 1);
    size += 2;
    ConvUnixWin (user->email2);
    strcpy (pak.data + size, user->email2);
    size += strlen (user->email2) + 1;
    Word_2_Chars (pak.data + size, strlen (user->email3) + 1);
    size += 2;
    ConvUnixWin (user->email3);
    strcpy (pak.data + size, user->email3);
    size += strlen (user->email3) + 1;
    Word_2_Chars (pak.data + size, strlen (user->city) + 1);
    size += 2;
    ConvUnixWin (user->city);
    strcpy (pak.data + size, user->city);
    size += strlen (user->city) + 1;
    Word_2_Chars (pak.data + size, strlen (user->state) + 1);
    size += 2;
    ConvUnixWin (user->state);
    strcpy (pak.data + size, user->state);
    size += strlen (user->state) + 1;
    Word_2_Chars (pak.data + size, strlen (user->phone) + 1);
    size += 2;
    ConvUnixWin (user->phone);
    strcpy (pak.data + size, user->phone);
    size += strlen (user->phone) + 1;
    Word_2_Chars (pak.data + size, strlen (user->fax) + 1);
    size += 2;
    ConvUnixWin (user->fax);
    strcpy (pak.data + size, user->fax);
    size += strlen (user->fax) + 1;
    Word_2_Chars (pak.data + size, strlen (user->street) + 1);
    size += 2;
    ConvUnixWin (user->street);
    strcpy (pak.data + size, user->street);
    size += strlen (user->street) + 1;
    Word_2_Chars (pak.data + size, strlen (user->cellular) + 1);
    size += 2;
    ConvUnixWin (user->cellular);
    strcpy (pak.data + size, user->cellular);
    size += strlen (user->cellular) + 1;
    DW_2_Chars (&pak.data[size], user->zip);
    size += 4;
    Word_2_Chars (&pak.data[size], user->country);
    size += 2;
    pak.data[size++] = user->c_status;
    pak.data[size] = user->hide_email;
    size++;
    ssG.last_cmd[(ssG.seq_num - 1) & 0x3ff] = Chars_2_Word (pak.head.cmd);
    SOCKWRITE (sok, &(pak.head.ver), size + sizeof (pak.head) - 2);

    Word_2_Chars (pak.head.ver, ICQ_VER);
    Word_2_Chars (pak.head.cmd, CMD_AUTH_UPDATE);
    Word_2_Chars (pak.head.seq, ssG.seq_num++);
    DW_2_Chars (pak.head.UIN, ssG.UIN);

    DW_2_Chars (pak.data, !user->auth);
    ssG.last_cmd[(ssG.seq_num - 1) & 0x3ff] = Chars_2_Word (pak.head.cmd);
    SOCKWRITE (sok, &(pak.head.ver), 4 + sizeof (pak.head) - 2);
}

void Search_WP (SOK_T sok, WP_PTR user)
{
    net_icq_pak pak;
    int size;
    Word_2_Chars (pak.head.ver, ICQ_VER);
    Word_2_Chars (pak.head.cmd, CMD_META_USER);
    Word_2_Chars (pak.head.seq, ssG.seq_num++);
    DW_2_Chars (pak.head.UIN, ssG.UIN);

    Word_2_Chars (pak.data, META_CMD_WP);
    size = 2;
    Word_2_Chars (pak.data + size, strlen (user->first) + 1);
    size += 2;
    ConvUnixWin (user->first);
    strcpy (pak.data + size, user->first);
    size += strlen (user->first) + 1;
    Word_2_Chars (pak.data + size, strlen (user->last) + 1);
    size += 2;
    ConvUnixWin (user->last);
    strcpy (pak.data + size, user->last);
    size += strlen (user->last) + 1;
    Word_2_Chars (pak.data + size, strlen (user->nick) + 1);
    size += 2;
    ConvUnixWin (user->nick);
    strcpy (pak.data + size, user->nick);
    size += strlen (user->nick) + 1;
    Word_2_Chars (pak.data + size, strlen (user->email) + 1);
    size += 2;
    ConvUnixWin (user->email);
    strcpy (pak.data + size, user->email);
    size += strlen (user->email) + 1;
    Word_2_Chars (pak.data + size, user->minage);
    size += 2;
    Word_2_Chars (pak.data + size, user->maxage);
    size += 2;
    pak.data[size] = user->sex;
    size += 1;
    Word_2_Chars (pak.data+size,user->language);  
    size += 1;
    Word_2_Chars (pak.data + size, strlen (user->city) + 1);
    size += 2;
    ConvUnixWin (user->city);
    strcpy (pak.data + size, user->city);
    size += strlen (user->city) + 1;
    Word_2_Chars (pak.data + size, strlen (user->state) + 1);
    size += 2;
    ConvUnixWin (user->state);
    strcpy (pak.data + size, user->state);
    size += strlen (user->state) + 1;
    Word_2_Chars (pak.data + size, user->country);
    size += 2;
    Word_2_Chars (pak.data + size, strlen (user->company) + 1);
    size += 2;
    ConvUnixWin (user->company);
    strcpy (pak.data + size, user->company);
    size += strlen (user->company) + 1;
    Word_2_Chars (pak.data + size, strlen (user->department) + 1);
    size += 2;
    ConvUnixWin (user->department);
    strcpy (pak.data + size, user->department);
    size += strlen (user->department) + 1;
    Word_2_Chars (pak.data + size, strlen (user->position) + 1);
    size += 2;
    ConvUnixWin (user->position);
    strcpy (pak.data + size, user->position);
    size += strlen (user->position) + 1;
/*  Now it gets REALLY shakey, as I don't know even what
    these particular bits of information are.
    If you know, fill them in. Pretty sure they are
    interests, organizations, homepage and something else
    but not sure what order. -KK */
    Word_2_Chars (pak.data+size, 0x00);
    size += 1;
    Word_2_Chars (pak.data+size, 0x0000);
    size += 2;
    Word_2_Chars (pak.data+size, 0x01);
    size += 1;
    DW_2_Chars (pak.data+size, 0x00000000);
    size += 4;
    Word_2_Chars (pak.data+size, 0x01);
    size += 1;
    DW_2_Chars (pak.data+size, 0x00000000);
    size += 4;
    Word_2_Chars (pak.data+size, 0x01);
    size += 1;
    DW_2_Chars (pak.data+size, 0x00000000);
    size += 4;
    Word_2_Chars (pak.data+size, 0x01);
    size += 1;
    Word_2_Chars (pak.data+size, 0x0000);
    size += 2;
    pak.data[size] = user->online;
    size+=1;
    ssG.last_cmd[(ssG.seq_num - 1) & 0x3ff] = Chars_2_Word (pak.head.cmd);
    SOCKWRITE (sok, &(pak.head.ver), size + sizeof (pak.head) - 2);
}

void icq_sendurl (SOK_T sok, UDWORD uin, char *description, char *url)
{
    char buf[450];

    sprintf (buf, "%s\xFE%s", url, description);
    icq_sendmsg (sok, uin, buf, URL_MESS);
}

void Gen_Checksum (UBYTE * buf, UDWORD len)
{
    UDWORD cc, cc2;
    UDWORD r1, r2;

    cc = buf[8];
    cc <<= 8;
    cc += buf[4];
    cc <<= 8;
    cc += buf[2];
    cc <<= 8;
    cc += buf[6];

    r1 = rand () % (len - 0x18);
    r1 += 0x18;
    r2 = rand () & 0xff;

    cc2 = r1;
    cc2 <<= 8;
    cc2 += buf[r1];
    cc2 <<= 8;
    cc2 += r2;
    cc2 <<= 8;
    cc2 += table[r2];
    cc2 ^= 0xff00ff;
/*   printf( "tbl[%02lX] = %02X\n%08lX\n\n", r2,table[ r2 ], cc ^ cc2 ); */

    DW_2_Chars (&buf[0x14], cc ^ cc2);
}

UDWORD Scramble_cc (UDWORD cc)
{
    UDWORD a[6];

    a[1] = cc & 0x0000001F;
    a[2] = cc & 0x03E003E0;
    a[3] = cc & 0xF8000400;
    a[4] = cc & 0x0000F800;
    a[5] = cc & 0x041F0000;

    a[1] <<= 0x0C;
    a[2] <<= 0x01;
    a[3] >>= 0x0A;
    a[4] <<= 0x10;
    a[5] >>= 0x0F;

    return a[1] + a[2] + a[3] + a[4] + a[5];
}

void Wrinkle (void *buf, size_t len)
{
/*   UDWORD plen=size;*/
    UDWORD checkcode;
    UDWORD code1, code2, code3;
    UDWORD pos;
    UDWORD data;

    Gen_Checksum (buf, len);
    checkcode = Chars_2_DW (&((UBYTE *) buf)[20]);
    code1 = len * 0x68656c6cL;
    code2 = code1 + checkcode;
    pos = 0xa;
    for (; pos < (len); pos += 4)
    {
        code3 = code2 + table[pos & 0xFF];
        data = Chars_2_DW (&((UBYTE *) buf)[pos]);
        data ^= code3;
        DW_2_Chars (&((UBYTE *) buf)[pos], data);
    }
    checkcode = Scramble_cc (checkcode);
    DW_2_Chars (&((UBYTE *) buf)[0x14], checkcode);
}

/***************************************************************
This handles actually sending a packet after it's been assembled.
When V5 is implemented this will wrinkle the packet and calculate 
the checkcode.
Adds packet to the queued messages.
****************************************************************/
size_t SOCKWRITE (SOK_T sok, void *ptr, size_t len)
{
    struct msg *msg_to_queue;
    static UWORD seq2 = 0;
    UWORD temp;
    UWORD cmd;

#if ICQ_VER == 0x0004
    Word_2_Chars (&((UBYTE *) ptr)[4], 0);
    ((UBYTE *) ptr)[0x0A] = ((UBYTE *) ptr)[8];
    ((UBYTE *) ptr)[0x0B] = ((UBYTE *) ptr)[9];
#elif ICQ_VER == 0x0005
    DW_2_Chars (&((UBYTE *) ptr)[2], 0L);
    if (0 == ssG.our_session)
    {
        ssG.our_session = rand () & 0x3fffffff;
    }
    DW_2_Chars (&((UBYTE *) ptr)[0x0A], ssG.our_session);
#endif
    cmd = Chars_2_Word ((((ICQ_PAK_PTR) ((UBYTE *) ptr - 2))->cmd));
    if (cmd != CMD_ACK)
    {
        ssG.real_packs_sent++;
        msg_to_queue = (struct msg *) malloc (sizeof (struct msg));
#if ICQ_VER == 0x0004
        msg_to_queue->seq = Chars_2_Word (&((UBYTE *) ptr)[SEQ_OFFSET]);
#elif ICQ_VER == 0x0005
        if (0 == seq2)
        {
            seq2 = rand () & 0x7fff;
        }
        temp = Chars_2_Word (&((UBYTE *) ptr)[SEQ2_OFFSET]);
        temp += seq2;
        Word_2_Chars (&((UBYTE *) ptr)[SEQ_OFFSET], temp);
        if (CMD_KEEP_ALIVE == Chars_2_Word (&((UBYTE *) ptr)[CMD_OFFSET]))
        {
            Word_2_Chars (&((UBYTE *) ptr)[SEQ2_OFFSET], 0);
/*       seq2++;*/
            ssG.seq_num--;
        }
        msg_to_queue->seq = Chars_2_DW (&((UBYTE *) ptr)[SEQ_OFFSET]);
#else
#error Incorrect ICQ_VERsion
#endif
        msg_to_queue->attempts = 1;
        msg_to_queue->exp_time = time (NULL) + 10;
        msg_to_queue->body = (UBYTE *) malloc (len);
        msg_to_queue->len = len;
        memcpy (msg_to_queue->body, ptr, msg_to_queue->len);
        msg_queue_push (msg_to_queue, queue);

        if (msg_queue_peek (queue) == msg_to_queue)
        {
            ssG.next_resend = msg_to_queue->exp_time;
        }
    }
    return SOCKWRITE_LOW (sok, ptr, len);
}

/***************************************************************
This handles actually sending a packet after it's been assembled.
When V5 is implemented this will wrinkle the packet and calculate 
the checkcode.
Doesn't add packet to the queue.
****************************************************************/
static size_t SOCKWRITE_LOW (SOK_T sok, void *ptr, size_t len)
{

    /* these 3 lines used for socks5 */
    char tmpbuf[4096];

    assert (len > 0x18);

#if 1
    if (uiG.Verbose & 4)
    {
        R_undraw ();
        Time_Stamp ();
        M_print (" \x1b«" COLCLIENT "");
        M_print (i18n (775, "Outgoing packet:"));
#if ICQ_VER == 5
        M_print (" %04X %08X:%08X %04X (", Chars_2_Word (ptr),
                 Chars_2_DW (ptr + 10), Chars_2_DW (ptr + 16),
                 Chars_2_Word (ptr + 14));
        Print_CMD (Chars_2_Word (ptr + 14));
        M_print (")" COLNONE "\n");
#else
        M_print ("\n");
#endif
        Hex_Dump (ptr, len);
        M_print ("\x1b»\n");
        R_redraw ();
    }
#endif

    Wrinkle (ptr, len);

#if 0
    if (uiG.Verbose > 1)
        Hex_Dump (ptr, len);
#endif
/*   Dump_Queue()*/
    ssG.Packets_Sent++;
/* SOCKS5 stuff begin */
    if (s5G.s5Use)
    {
        tmpbuf[0] = 0;          /* reserved */
        tmpbuf[1] = 0;          /* reserved */
        tmpbuf[2] = 0;          /* standalone packet */
        tmpbuf[3] = 1;          /* address type IP v4 */
        *(unsigned long *) &tmpbuf[4] = htonl (s5G.s5DestIP);
        *(unsigned short *) &tmpbuf[8] = htons (s5G.s5DestPort);
        memcpy (&tmpbuf[10], ptr, len);
        return sockwrite (sok, tmpbuf, len + 10) - 10;
    }
    else
        return sockwrite (sok, ptr, len);
/* SOCKS5 stuff end */
}

size_t SOCKREAD (SOK_T sok, void *ptr, size_t len)
{
    size_t sz;

    sz = sockread (sok, ptr, len);
    ssG.Packets_Recv++;
/* SOCKS5 stuff begin */
    if (s5G.s5Use)
    {
        sz -= 10;
        memcpy (ptr, ptr + 10, sz);
    }
/* SOCKS5 stuff end */

    if ((uiG.Verbose > 2) && (sz > 0))
    {
        M_print ("\n");
        Hex_Dump (ptr, sz);
    }
    return sz;
}
