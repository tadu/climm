/* $Id$ */

/************************************************************
Author Lawrence Gold
Handles resending missed packets.
*************************************************************/
#include "micq.h"
#include "util_ui.h"
#include "util.h"
#include "cmd_pkt_server.h"
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

static struct msg_queue *queue = NULL;


void msg_queue_init (struct msg_queue **queue)
{
    *queue = malloc (sizeof (*queue));
    (*queue)->entries = 0;
    (*queue)->head = (*queue)->tail = NULL;
}


struct msg *msg_queue_peek (struct msg_queue *queue)
{
    if (NULL != queue->head)
    {
        return queue->head->msg;
    }
    else
    {
        return NULL;
    }
}


struct msg *msg_queue_pop (struct msg_queue *queue)
{
    struct msg *popped_msg;
    struct msg_queue_entry *temp;

    if ((NULL != queue->head) && (queue->entries > 0))
    {
        popped_msg = queue->head->msg;
        temp = queue->head->next;
        free (queue->head);
        queue->head = temp;
        queue->entries--;
        if (NULL == queue->head)
        {
            queue->tail = NULL;
        }
        /* if ( 0x1000 < Chars_2_Word( &popped_msg->body[6] ) ) {
           M_print( "\n\n******************************************\a\a\a\n"
           "Error!!!!!!!!!!!!!!!!!!!!!\n" );
           } */
        return popped_msg;
    }
    else
    {
        return NULL;
    }
}


void msg_queue_push (struct msg *new_msg, struct msg_queue *queue)
{
    struct msg_queue_entry *new_entry;

    assert (NULL != new_msg);

    if (NULL == queue)
        return;

    new_entry = malloc (sizeof (struct msg_queue_entry));
    if (new_entry == NULL)
    {
        M_print (i18n (617, "Memory exhausted!!\a\n"));
        exit (-1);
    }
    new_entry->next = NULL;
    new_entry->msg = new_msg;

    if (queue->tail)
    {
        queue->tail->next = new_entry;
        queue->tail = new_entry;
    }
    else
    {
        queue->head = queue->tail = new_entry;
    }

    queue->entries++;
}

void Dump_Queue (void)
{
    int i;
    struct msg *queued_msg;

    assert (queue != NULL);
    assert (0 <= queue->entries);

    M_print (i18n (618, "\nTotal entries %d\n"), queue->entries);
    for (i = 0; i < queue->entries; i++)
    {
        queued_msg = msg_queue_pop (queue);
        M_print (i18n (619, "SEQ = %04x\tCMD = %04x\tattempts = %d\tlen = %d\n"),
                 (queued_msg->seq >> 16), Chars_2_Word (&queued_msg->body[CMD_OFFSET]),
                 queued_msg->attempts, queued_msg->len);
        if (uiG.Verbose & 16)
        {
            Hex_Dump (queued_msg->body, queued_msg->len);
        }
        msg_queue_push (queued_msg, queue);
    }
}

void Check_Queue (UDWORD seq, struct msg_queue *queue)
{
    int i;
    struct msg *queued_msg;

    assert (0 <= queue->entries);

    for (i = 0; i < queue->entries; i++)
    {
        queued_msg = msg_queue_pop (queue);
        if (queued_msg->seq != seq)
        {
            msg_queue_push (queued_msg, queue);
        }
        else
        {
            if (uiG.Verbose & 16)
            {
                UDWORD tmp = Chars_2_Word (&queued_msg->body[CMD_OFFSET]);
                R_undraw ();
                M_print (i18n (824, "Acknowledged packet type %04x (%s) sequence %04x removed from queue.\n"),
                         tmp, CmdPktSrvName (tmp), queued_msg->seq >> 16);
                R_redraw ();
            }
            if (Chars_2_Word (&queued_msg->body[CMD_OFFSET]) == CMD_SENDM)
            {
                R_undraw ();
                Time_Stamp ();
                M_print (" " COLACK "%10s" COLNONE " %s%s\n",
                         UIN2Name (Chars_2_DW (&queued_msg->body[PAK_DATA_OFFSET])),
                         MSGACKSTR, MsgEllipsis (&queued_msg->body[32]));
                R_redraw ();
            }
            free (queued_msg->body);
            free (queued_msg);

            if ((queued_msg = msg_queue_peek (queue)) != NULL)
            {
                ssG.next_resend = queued_msg->exp_time;
            }
            else
            {
                ssG.next_resend = INT_MAX;
            }
            break;
        }
    }
}
