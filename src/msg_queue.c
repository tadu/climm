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
#include "util_str.h"
#include "util_extra.h"
#include "icq_response.h" /* yuck */
#include "packet.h" /* yuck */

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
    
    if (*myqueue)
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
        Debug (DEB_QUEUE, STR_DOT STR_DOT STR_DOT "> %s %p: %08lx %p", QueueType (event->type), event, event->seq, event->pak);
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

    Debug (DEB_QUEUE, "<" STR_DOT STR_DOT STR_DOT " %s %p: %08lx %p %x", QueueType (event->type), event, event->seq, event->pak, event->flags);

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
Event *QueueEnqueueData (Connection *conn, UDWORD type, UDWORD id,
                         time_t due, Packet *pak, UDWORD uin,
                         Extra *extra, Queuef *callback)
{
    Event *event = calloc (sizeof (Event), 1);
    assert (event);
    
    event->conn = conn;
    event->type = type;
    event->seq  = id;
    event->attempts = 1;
    event->uin  = uin;
    event->due  = due;
    event->pak = pak;
    event->extra = extra;
    event->callback = callback;
    
    Debug (DEB_EVENT, "<+" STR_DOT STR_DOT " %s %p: %08lx %p %x", QueueType (event->type), event, event->seq, event->pak, event->flags);
    QueueEnqueue (event);

    return event;
}

/*
 * Removes and returns a given event.
 */
static Event *QueueDequeueEvent (Event *event, struct QueueEntry *previous)
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
Event *QueueDequeue (Connection *conn, UDWORD type, UDWORD seq)
{
    Event *event;
    struct QueueEntry *iter;

    assert (queue);

    if (!queue->head)
    {
        Debug (DEB_QUEUE, STR_DOT "??" STR_DOT " %s %08lx", QueueType (type), seq);
        return NULL;
    }

    if (   queue->head->event->conn == conn
        && queue->head->event->type == type
        && queue->head->event->seq  == seq)
    {
        event = QueueDequeueEvent (queue->head->event, NULL);
        Debug (DEB_QUEUE, STR_DOT STR_DOT "s> %s %p: %08lx %p", QueueType (type), event, seq, event->pak);
        return event;
    }
    for (iter = queue->head; iter->next; iter = iter->next)
    {
        if (   iter->next->event->conn == conn
            && iter->next->event->type == type
            && iter->next->event->seq  == seq)
        {
            event = QueueDequeueEvent (iter->next->event, iter);
            Debug (DEB_QUEUE, STR_DOT STR_DOT "s> %s %p: %08lx %p", QueueType (type), event, seq, event->pak);
            return event;
        }
    }
    Debug (DEB_QUEUE, STR_DOT "??" STR_DOT " %s %08lx", QueueType (type), seq);
    return NULL;
}

void EventD (Event *event)
{
    if (!event)
        return;
    Debug (DEB_EVENT, STR_DOT STR_DOT ">> %s %p: %08lx %p", QueueType (event->type), event, event->seq, event->pak);
    if (event->pak)
        PacketD (event->pak);
    ExtraD (event->extra);
    free (event);
}

/*
 * Cancels all events for a given (to be removed) session
 * by calling the callback with empty session pointer.
 */
void QueueCancel (Connection *conn)
{
    Event *event;
    struct QueueEntry *iter;

    assert (queue);

    while (queue->head && queue->head->event->conn == conn)
    {
        event = QueueDequeueEvent (queue->head->event, NULL);
        Debug (DEB_QUEUE, STR_DOT STR_DOT "!> %s %p %p: %08lx %p", QueueType (event->type), conn, event, event->seq, event->pak);
        event->conn = NULL;
        if (event->callback)
            event->callback (event);
    }

    if (!queue->head)
        return;

    for (iter = queue->head; iter && iter->next; iter = iter->next)
    {
        while (iter->next && iter->next->event->conn == conn)
        {
            event = QueueDequeueEvent (iter->next->event, iter);
            Debug (DEB_QUEUE, STR_DOT STR_DOT "!> %s %p %p: %08lx %p", QueueType (event->type),
                   conn, event, event->seq, event->pak);
            event->conn = NULL;
            if (event->callback)
                event->callback (event);
        }
    }
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
void QueueRetry (Connection *conn, UDWORD type, UDWORD uin)
{
    struct QueueEntry *iter;
    Event *event = NULL;
    
    assert (queue);
    for (iter = queue->head; iter; iter = iter->next)
        if (iter->event->conn == conn
            && iter->event->type == type
            && iter->event->uin == uin)
        {
            if (!event || event->seq > iter->event->seq)
                event = iter->event;
        }
    
    if (event)
        event = QueueDequeueEvent (event, NULL);
    
    if (event)
    {
        event->due = time (NULL);
        if (event->callback)
            event->callback (event);
    }
    else
        Debug (DEB_QUEUE, STR_DOT STR_DOT "s" STR_DOT " %ld %s", uin, QueueType (type));
}

/*
 * Returns a string representation of the queue type.
 */
const char *QueueType (UDWORD type)
{
    switch (type)
    {
        case QUEUE_FLAP:          return "FLAP";
        case QUEUE_SRV_KEEPALIVE: return "SRV_KEEPALIVE";
        case QUEUE_UDP_KEEPALIVE: return "UDP_KEEPALIVE";
        case QUEUE_TCP_RECEIVE:   return "TCP_RECEIVE";
        case QUEUE_CON_TIMEOUT:   return "CON_TIMEOUT";
        case QUEUE_TCP_TIMEOUT:   return "TCP_TIMEOUT";
        case QUEUE_TCP_RESEND:    return "TCP_RESEND";
        case QUEUE_UDP_RESEND:    return "UDP_RESEND";
        case QUEUE_PEER_FILE:     return "PEER_FILE";
        case QUEUE_PEER_RESEND:   return "PEER_RESEND";
        case QUEUE_TYPE2_RESEND:  return "TYPE2_RESEND";
    }
    return s_sprintf ("%lx", type);
}

