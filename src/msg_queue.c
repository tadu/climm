/* $Id$ */

/************************************************************
Author Lawrence Gold
Handles resending missed packets.
*************************************************************/
#include "micq.h"
#include "util_ui.h"
#include "util.h"
#include "cmd_pkt_server.h"
#include "contact.h"
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

void msg_queue_init (struct msg_queue **queue)
{
    assert (queue);
    
    *queue = malloc (sizeof (**queue));
    (*queue)->entries = 0;
    (*queue)->head = (*queue)->tail = NULL;
    (*queue)->exp_time = INT_MAX;
}


struct msg *msg_queue_peek (struct msg_queue *queue)
{
    assert (queue);

    if (queue->head)
        return queue->head->msg;
    return NULL;
}


struct msg *msg_queue_pop (struct msg_queue *queue)
{
    struct msg *popped_msg;
    struct msg_queue_entry *temp;

    assert (queue);

    if (queue->head && (queue->entries > 0))
    {
        popped_msg = queue->head->msg;
        temp = queue->head->next;
        free (queue->head);
        queue->head = temp;
        queue->entries--;
        if (!queue->head)
        {
            queue->tail = NULL;
            queue->exp_time = INT_MAX;
        }
        else
            queue->exp_time = queue->head->msg->exp_time;
        return popped_msg;
    }
    return NULL;
}


void msg_queue_enqueue (struct msg_queue *queue, struct msg *new_msg)
{
    struct msg_queue_entry *new_entry;
    struct msg_queue_entry *iter_entry;

    assert (queue);
    assert (new_msg);

    new_entry = malloc (sizeof (struct msg_queue_entry));

    assert (new_entry);

    new_entry->next = NULL;
    new_entry->msg = new_msg;

    if (!queue->head || !queue->tail)
    {
        queue->head = queue->tail = new_entry;
    }
    else if (new_msg->exp_time <= queue->head->msg->exp_time)
    {
        new_entry->next = queue->head;
        queue->head = new_entry;
    }
    else if (new_msg->exp_time > queue->tail->msg->exp_time)
    {
        queue->tail->next = new_entry;
        queue->tail = new_entry;
    }
    else
    {
        for (iter_entry = queue->head; new_msg->exp_time > iter_entry->next->msg->exp_time; iter_entry = iter_entry->next)
        {
           assert (iter_entry->next);
           assert (iter_entry->next->next);
        }
        new_entry->next = iter_entry->next;
        iter_entry->next = new_entry;
    }
    queue->entries++;
    queue->exp_time = queue->head->msg->exp_time;
}

struct msg *msg_queue_dequeue_seq (struct msg_queue *queue, UDWORD seq)
{
    struct msg *queued_msg;
    struct msg_queue_entry *iter_entry;
    struct msg_queue_entry *tmp_entry;

    assert (queue);
    assert (0 <= queue->entries);

    if (queue->head && queue->head->msg->seq == seq)
    {
        tmp_entry = queue->head;
        queued_msg = tmp_entry->msg;
        queue->head = queue->head->next;
        free (tmp_entry);
        if (!queue->head)
        {
            queue->tail = NULL;
            queue->exp_time = INT_MAX;
        }
        else
            queue->exp_time = queue->head->msg->exp_time;
        return queued_msg;
    }
    for (iter_entry = queue->head; iter_entry->next; iter_entry = iter_entry->next)
    {
        if (iter_entry->next->msg->seq == seq)
        {
            tmp_entry = iter_entry->next;
            queued_msg = tmp_entry->msg;
            iter_entry->next=iter_entry->next->next;
            free (tmp_entry);
            if (queue->tail == tmp_entry)
                queue->tail = iter_entry;
            return queued_msg;
        }
    }
    return NULL;
}

void Check_Queue (UDWORD seq, struct msg_queue *queue)
{
    struct msg *queued_msg = msg_queue_dequeue_seq (queue, seq);

    if (!queued_msg)
        return;

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
                 ContactFindName (Chars_2_DW (&queued_msg->body[PAK_DATA_OFFSET])),
                 MSGACKSTR, MsgEllipsis (&queued_msg->body[32]));
        R_redraw ();
    }
    free (queued_msg->body);
    free (queued_msg);

    ssG.next_resend = queue->exp_time;
}
