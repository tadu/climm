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

static Queue queued = { NULL, INT_MAX };
static Queue *queue = &queued;

static Event *QueueDequeueEvent (Event *event, struct QueueEntry *previous);

/*
 * Create a new empty queue to use, or use the given queue instead.
 * Do not call unless you have several message queues.
 */
void QueueInit (Queue **myqueue)
{
    assert (myqueue);
    
    if (myqueue)
        queue = *myqueue;
    else
    {
        queue = malloc (sizeof (Queue));
        queue->head = NULL;
        queue->due = INT_MAX;
        *myqueue = queue;
    }
}

/*
 * Returns the event most due without removing it, or NULL.
 */
Event *QueuePeek ()
{
    if (queue->head)
        return queue->head->event;
    return NULL;
}

/*
 * Removes the event most due from the queue, or NULL.
 */
Event *QueuePop ()
{
    Event *event;
    struct QueueEntry *temp;

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
        Debug (DEB_QUEUE, i18n (2074, "popping type %s seq %08x at %p (pak %p)"),
               QueueType (event->type), event->seq, event, event->pak);
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

    entry = malloc (sizeof (struct QueueEntry));

    assert (event);
    assert (entry);

    entry->next = NULL;
    entry->event  = event;

    Debug (DEB_QUEUE, i18n (2082, "enqueuing type %s seq %08x at %p (pak %p) %x"),
           QueueType (event->type), event->seq, event, event->pak, event->flags);

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
        for (iter = queue->head; iter->next; iter = iter->next)
            if (event->due <= iter->next->event->due)
                break;

        if (event->flags & QUEUE_FLAG_CONSIDERED)
            for ( ; iter->next; iter = iter->next)
                if (event->due != iter->next->event->due)
                    break;

        entry->next = iter->next;
        iter->next = entry;
    }
    queue->due = queue->head->event->due;
}

/*
 * Adds a new entry to the queue. Creates Event for you.
 */
void QueueEnqueueData (Session *sess, UDWORD seq, UDWORD type,
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
    
    QueueEnqueue (event);
}

/*
 * Removes and returns a given event.
 */
Event *QueueDequeueEvent (Event *event, struct QueueEntry *previous)
{
    struct QueueEntry *iter;
    struct QueueEntry *tmp;
    
    if (!queue->head)
        return NULL;

    if (queue->head->event == event)
    {
        tmp = queue->head;
        queue->head = queue->head->next;
        free (tmp);

        if (!queue->head)
            queue->due = INT_MAX;
        else
            queue->due = queue->head->event->due;

        return event;
    }
    if (previous)
    {
        assert (previous->next->event == event);
        
        tmp = previous->next;
        previous->next = previous->next->next;
        free (tmp);

        return event;
    }
    for (iter = queue->head; iter->next; iter = iter->next)
    {
        if (iter->next->event == event)
        {
            tmp = iter->next;
            event = tmp->event;
            iter->next = iter->next->next;
            free (tmp);

            return event;
        }
    }
    return NULL;
}

/*
 * Removes and returns an event given by sequence number and type.
 */
Event *QueueDequeue (UDWORD seq, UDWORD type)
{
    Event *event;
    struct QueueEntry *iter;

    assert (queue);

    if (!queue->head)
    {
        Debug (DEB_QUEUE, i18n (2076, "couldn't dequeue type %s seq %08x"),
               QueueType (type), seq);
        return NULL;
    }

    if (queue->head->event->seq == seq && queue->head->event->type == type)
    {
        event = QueueDequeueEvent (queue->head->event, NULL);
        Debug (DEB_QUEUE, i18n (2077, "dequeue type %s seq %08x at %p (pak %p)"),
               QueueType (type), seq, event, event->pak);
        return event;
    }
    for (iter = queue->head; iter->next; iter = iter->next)
    {
        if (iter->next->event->seq == seq && iter->next->event->type == type)
        {
            event = QueueDequeueEvent (iter->next->event, iter);
            Debug (DEB_QUEUE, i18n (2077, "dequeue type %s seq %08x at %p (pak %p)"),
                   QueueType (type), seq, event, event->pak);
            return event;
        }
    }
    Debug (DEB_QUEUE, i18n (2076, "couldn't dequeue type %s seq %08x"),
           QueueType (type), seq);
    return NULL;
}

/*
 * Removes and returns an event for a given (to be removed) session.
 */
Event *QueueDangling (Session *sess)
{
    Event *event;
    struct QueueEntry *iter;

    assert (queue);

    if (!queue->head)
        return NULL;

    if (queue->head->event->sess == sess)
    {
        event = QueueDequeueEvent (queue->head->event, NULL);
        Debug (DEB_QUEUE, i18n (2081, "dangling type %s seq %08x at %p (pak %p)"),
               QueueType (event->type), event->seq, event, event->pak);
        return event;
    }
    for (iter = queue->head; iter->next; iter = iter->next)
    {
        if (iter->next->event->sess == sess)
        {
            event = QueueDequeueEvent (iter->next->event, iter);
            Debug (DEB_QUEUE, i18n (2081, "dangling type %s seq %08x at %p (pak %p)"),
                   QueueType (event->type), event->seq, event, event->pak);
            return event;
        }
    }
    return NULL;
}

/*
 * Delivers all due events by removing them from the queue and calling their
 * callback function.  Callback function may re-enqueue them, but the event
 * is not reconsidered even if it is still due.
 */
void QueueRun ()
{
    time_t now = time (NULL);
    Event *event;
    struct QueueEntry *iter;
    
    assert (queue);
    
    for (iter = queue->head; iter && iter->event->due <= now; iter = iter->next)
        iter->event->flags &= ~QUEUE_FLAG_CONSIDERED;
    
    while (queue->due <= now)
    {
        if (queue->head->event->flags & QUEUE_FLAG_CONSIDERED)
            break;

        event = QueuePop ();
        event->flags |= QUEUE_FLAG_CONSIDERED;
        if (event->callback)
            event->callback (event);
    }
}

/*
 * Checks whether there is an event waiting for uin of that type,
 * and delivers the event with the lowest sequence number
 */
void QueueRetry (UDWORD uin, UDWORD type)
{
    struct QueueEntry *iter;
    Event *event = NULL;
    
    assert (queue);
    for (iter = queue->head; iter; iter = iter->next)
        if (iter->event->uin == uin && iter->event->type == type)
            if (!event || event->seq > iter->event->seq)
                event = iter->event;
    
    if (event)
        event = QueueDequeue (event->seq, type);
    
    if (event && event->callback)
        event->callback (event);
}

/*
 * Returns a string representation of the queue type.
 */
const char *QueueType (UDWORD type)
{
    static char buf[10];
    switch (type)
    {
        case QUEUE_TYPE_FLAP:          return "FLAP";
        case QUEUE_TYPE_SRV_KEEPALIVE: return "SRV_KEEPALIVE";
        case QUEUE_TYPE_UDP_KEEPALIVE: return "UDP_KEEPALIVE";
        case QUEUE_TYPE_TCP_RECEIVE:   return "TCP_RECEIVE";
        case QUEUE_TYPE_CON_TIMEOUT:   return "CON_TIMEOUT";
        case QUEUE_TYPE_TCP_TIMEOUT:   return "TCP_TIMEOUT";
        case QUEUE_TYPE_TCP_RESEND:    return "TCP_RESEND";
        case QUEUE_TYPE_UDP_RESEND:    return "UDP_RESEND";
        case QUEUE_TYPE_PEER_FILE:     return "PEER_FILE";
        case QUEUE_TYPE_PEER_RESEND:   return "PEER_RESEND";
    }
    snprintf (buf, sizeof (buf), "%lx", type);
    return buf;
}

