/*
 * Provides a time sorted queue, with callback for due events.
 *
 * This file is Copyright © Rüdiger Kuhlmann; it may be distributed under
 * version 2 of the GPL licence.
 *
 * This file replaces a simple FIFO implementation for a similar,
 * but more limited, purpose by Lawrence Gold.
 *
 * $Id$
 */

#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include "micq.h"
#include "msg_queue.h"
#include "util_ui.h"

struct QueueEntry
{
    Event             *event;
    struct QueueEntry *next;
};

struct Queue_s
{
    struct QueueEntry *head;
    time_t due;
};

/*
 * Create a new empty queue.
 */
void QueueInit (struct Queue **queue)
{
    assert (queue);
    
    *queue = malloc (sizeof (struct Queue));
    (*queue)->head = NULL;
    (*queue)->due = INT_MAX;
}

/*
 * Returns the event most due without removing it, or NULL.
 */
struct Event *QueuePeek (struct Queue *queue)
{
    assert (queue);

    if (queue->head)
        return queue->head->event;
    return NULL;
}

/*
 * Removes the event most due from the queue, or NULL.
 */
Event *QueuePop (struct Queue *queue)
{
    Event *event;
    struct QueueEntry *temp;

    assert (queue);

    if (queue->head)
    {
        event = queue->head->event;
        temp = queue->head->next;
        free (queue->head);
        queue->head = temp;
        if (!queue->head)
            queue->due = INT_MAX;
        else
            queue->due = queue->head->event->due;
        Debug (DEB_QUEUE, i18n (1963, "popping type %d seq %08x at %p (pak %p)"),
               event->type, event->seq, event, event->pak);
        return event;
    }
    return NULL;
}

/*
 * Adds a new entry to the queue.
 */
void QueueEnqueue (Event *event)
{
    struct QueueEntry *entry;
    struct QueueEntry *iter;

    assert (queue);
    assert (event);

    entry = malloc (sizeof (struct QueueEntry));

    assert (entry);

    entry->next = NULL;
    entry->event  = event;

    Debug (DEB_QUEUE, i18n (1626, "enqueuing type %d seq %08x at %p (pak %p)"),
           event->type, event->seq, event, event->pak);

    if (!queue->head)
    {
        queue->head = entry;
    }
    else if (event->due < queue->head->event->due)
    {
        entry->next = queue->head;
        queue->head = entry;
    }
    else
    {
        for (iter = queue->head; iter->next &&
             event->due >= iter->next->event->due; iter = iter->next)
        {
           assert (iter->next);
        }
        entry->next = iter->next;
        iter->next = entry;
    }
    queue->due = queue->head->event->due;
}

/*
 * Adds a new entry to the queue. Creates struct Event for you.
 */
void QueueEnqueueData (struct Queue *queue, Session *sess, UDWORD seq, UDWORD type,
                       UDWORD uin, time_t due,
                       Packet *pak, UBYTE *info, Queuef *callback)
{
    Event *event = malloc (sizeof (Event));
    assert (event);
    
    event->seq  = seq;
    event->type = type;
    event->attempts = 1;
    event->uin  = uin;
    event->due  = due;
    event->pak = pak;
    event->info = info;
    event->callback = callback;
    event->sess = sess;
    
    QueueEnqueue (queue, event);
}

/*
 * Removes and returns an event given by sequence number and type.
 */
Event *QueueDequeue (UDWORD seq, UDWORD type)
{
    Event *event;
    struct QueueEntry *iter;
    struct QueueEntry *tmp;

    assert (queue);

    if (!queue->head)
    {
        Debug (DEB_QUEUE, i18n (1964, "couldn't dequeue type %d seq %08x"), type, seq);
        return NULL;
    }

    if (queue->head->event->seq == seq && queue->head->event->type == type)
    {
        tmp = queue->head;
        event = tmp->event;
        queue->head = queue->head->next;
        free (tmp);

        if (!queue->head)
            queue->due = INT_MAX;
        else
            queue->due = queue->head->event->due;

        Debug (DEB_QUEUE, i18n (1630, "dequeue type %d seq %08x at %p (pak %p)"),
               type, seq, event, event->pak);
        return event;
    }
    for (iter = queue->head; iter->next; iter = iter->next)
    {
        if (iter->next->event->seq == seq && iter->next->event->type == type)
        {
            tmp = iter->next;
            event = tmp->event;
            iter->next=iter->next->next;
            free (tmp);
            Debug (DEB_QUEUE, i18n (1630, "dequeue type %d seq %08x at %p (pak %p)"),
                   type, seq, event, event->pak);
            return event;
        }
    }
    Debug (DEB_QUEUE, i18n (1964, "couldn't dequeue type %d seq %08x"), type, seq);
    return NULL;
}

/*
 * Runs over the queue, removes all due events, and calls their callback
 * function.
 * Callback function may re-enqueue them with a later due time.
 */
void QueueRun (struct Queue *queue)
{
    time_t now = time (NULL);
    struct Event *event;
    
    while (queue->due <= now)
    {
        event = QueuePop (queue);
        if (event->callback)
            event->callback (event);
    }
}
