/*
 * Provides a time sorted queue, with callback for due events.
 *
 * mICQ Copyright (C) © 2001,2002,2003 Rüdiger Kuhlmann
 *
 * mICQ is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 dated June, 1991.
 *
 * mICQ is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
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
#include "contact.h" /* for cont->uin */
#include "preferences.h"
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

static Queue queued = { NULL, NEVER };
static Queue *queue = &queued;

static Event *q_QueueDequeueEvent (Event *event, struct QueueEntry *previous);
static Event *q_EventDep (Event *event);
static Event *q_QueuePop (void);
static void q_QueueEnqueue (Event *event);

/*
 * Create a new empty queue to use, or use the given queue instead.
 * Do not call unless you have several message queues.
 */
void QueueInit (Queue **myqueue)
{
    if (*myqueue)
        queue = *myqueue;
    else
    {
        queue = malloc (sizeof (Queue));
        queue->head = NULL;
        queue->due = NEVER;
        *myqueue = queue;
    }
}

/*
 * Returns the seconds till the next event is due.
 */
UDWORD QueueTime ()
{
    time_t now = time (NULL);
    
    if (queue->due <= now)
        return 0;
    if (queue->due - 10 > now)
        return 10;
    return queue->due - now;
}

/*
 * Removes the event most due from the queue, or NULL.
 */
static Event *q_QueuePop (void)
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
            queue->due = NEVER;
        else
            queue->due = queue->head->event->due;
        Debug (DEB_QUEUE, STR_DOT STR_DOT STR_DOT "> %s %p: %08lx %p %ld @ %p",
               QueueType (event->type), event, event->seq, event->pak,
               event->cont ? event->cont->uin : 0, event->conn);
        return event;
    }
    return NULL;
}

/*
 * Adds a new entry to the queue.
 */
void QueueEnqueue (Event *event)
{
    Debug (DEB_QUEUE, "<" STR_DOT STR_DOT STR_DOT " %s %p: %08lx %p %ld %x @ %p t %ld",
           QueueType (event->type), event, event->seq, event->pak,
           event->cont ? event->cont->uin : 0, event->flags, event->conn, (long)event->due);
    q_QueueEnqueue (event);
}

static void q_QueueEnqueue (Event *event)
{
    struct QueueEntry *entry;
    struct QueueEntry *iter;

    entry = malloc (sizeof (struct QueueEntry));

    assert (event);
    assert (entry);

    entry->next = NULL;
    entry->event  = event;

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
                         time_t due, Packet *pak, Contact *cont,
                         ContactOptions *opt, Queuef *callback)
{
    Event *event = calloc (sizeof (Event), 1);
    uiG.events++;
    assert (event);
    
    event->conn = conn;
    event->type = type;
    event->seq  = id;
    event->attempts = 1;
    event->cont  = cont;
    event->due  = due;
    event->pak = pak;
    event->opt = opt;
    event->callback = callback;
    
    Debug (DEB_EVENT, "<+" STR_DOT STR_DOT " %s %p: %08lx %p %ld %x @ %p",
           QueueType (event->type), event, event->seq, event->pak,
           event->cont ? event->cont->uin : 0, event->flags, event->conn);
    q_QueueEnqueue (event);

    return event;
}

/*
 * Adds a new waiting entry to the queue. Creates Event for you.
 */
Event *QueueEnqueueDep (Connection *conn, UDWORD type, UDWORD id,
                        Event *dep, Packet *pak, Contact *cont,
                        ContactOptions *opt, Queuef *callback)
{
    Event *event = calloc (sizeof (Event), 1);
    uiG.events++;
    assert (event);
    
    event->wait = dep;
    event->conn = conn;
    event->type = type;
    event->seq  = id;
    event->attempts = 1;
    event->cont  = cont;
    event->due  = NEVER;
    event->pak = pak;
    event->opt = opt;
    event->callback = callback;
    
    Debug (DEB_EVENT, "<*" STR_DOT STR_DOT " %s %p: %08lx %p %ld %x @ %p",
           QueueType (event->type), event, event->seq, event->pak,
           event->cont ? event->cont->uin : 0, event->flags, event->conn);
    q_QueueEnqueue (event);

    return event;
}

Event *QueueDequeueEvent (Event *event)
{
    return q_QueueDequeueEvent (event, NULL);
}

/*
 * Removes and returns a given event.
 */
static Event *q_QueueDequeueEvent (Event *event, struct QueueEntry *previous)
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
            queue->due = NEVER;
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
        event = q_QueueDequeueEvent (queue->head->event, NULL);
        Debug (DEB_QUEUE, STR_DOT STR_DOT "s> %s %p: %08lx %p %ld",
               QueueType (type), event, seq, event->pak, event->cont ? event->cont->uin : 0);
        return event;
    }
    for (iter = queue->head; iter->next; iter = iter->next)
    {
        if (   iter->next->event->conn == conn
            && iter->next->event->type == type
            && iter->next->event->seq  == seq)
        {
            event = q_QueueDequeueEvent (iter->next->event, iter);
            Debug (DEB_QUEUE, STR_DOT STR_DOT "s> %s %p: %08lx %p %ld",
                   QueueType (type), event, seq, event->pak, event->cont ? event->cont->uin : 0);
            return event;
        }
    }
    Debug (DEB_QUEUE, STR_DOT "??" STR_DOT " %s %08lx", QueueType (type), seq);
    return NULL;
}

/*
 * Removes and returns an event given by type, and sequence number and/or UIN.
 */
Event *QueueDequeue2 (Connection *conn, UDWORD type, UDWORD seq, Contact *cont)
{
    Event *event;
    struct QueueEntry *iter;

    assert (queue);

    if (!queue->head)
    {
        Debug (DEB_QUEUE, STR_DOT "??" STR_DOT " %s %08lx %ld", QueueType (type), seq, cont ? cont->uin : 0);
        return NULL;
    }

    if (   queue->head->event->conn == conn
        && queue->head->event->type == type
        && (!seq || queue->head->event->seq == seq)
        && (!cont || queue->head->event->cont == cont))
    {
        event = q_QueueDequeueEvent (queue->head->event, NULL);
        Debug (DEB_QUEUE, STR_DOT STR_DOT "s> %s %p: %08lx %p %ld",
               QueueType (type), event, event->seq, event->pak,
               event->cont ? event->cont->uin : 0);
        return event;
    }
    for (iter = queue->head; iter->next; iter = iter->next)
    {
        if (   iter->next->event->conn == conn
            && iter->next->event->type == type
            && (!seq || iter->next->event->seq  == seq)
            && (!cont || iter->next->event->cont == cont))
        {
            event = q_QueueDequeueEvent (iter->next->event, iter);
            Debug (DEB_QUEUE, STR_DOT STR_DOT "s> %s %p: %08lx %p %ld",
                   QueueType (type), event, event->seq, event->pak,
                   event->cont ? event->cont->uin : 0);
            return event;
        }
    }
    Debug (DEB_QUEUE, STR_DOT "??" STR_DOT " %s %08lx %ld @ %p",
           QueueType (type), seq, cont ? cont->uin : 0, conn);
    return NULL;
}

static Event *q_EventDep (Event *event)
{
    struct QueueEntry *iter;

    if (!queue->head)
        return NULL;
    
    if (queue->head->event->wait == event && queue->head->event->due)
        return q_QueueDequeueEvent (queue->head->event, NULL);

    for (iter = queue->head; iter->next; iter = iter->next)
        if (iter->next->event->wait == event && iter->next->event->due)
            return q_QueueDequeueEvent (iter->next->event, iter);
    
    return NULL;
}

void EventD (Event *event)
{
    Event *oevent;

    if (!event)
        return;
    if (q_QueueDequeueEvent (event, NULL))
        M_printf ("FIXME: Deleting still queued event %p!\n", event);
    Debug (DEB_EVENT, STR_DOT STR_DOT ">> %s %p: %08lx %p %ld",
           QueueType (event->type), event, event->seq, event->pak,
           event->cont ? event->cont->uin : 0);
    if (event->pak)
        PacketD (event->pak);
    ContactOptionsD (event->opt);

    while ((oevent = q_EventDep (event)))
    {
        Debug (DEB_QUEUE, STR_DOT "!!" STR_DOT " << %p", event);
        oevent->wait = NULL;
        oevent->due = 0;
        q_QueueEnqueue (oevent);
    }
    uiG.events--;
    free (event);
}

/*
 * Activate all events depending on given event
 */
void QueueRelease (Event *event)
{
    Event *oevent;
    
    Debug (DEB_QUEUE, STR_DOT "!!" STR_DOT " %p", event);
    while ((oevent = q_EventDep (event)))
    {
        Debug (DEB_QUEUE, STR_DOT "!!" STR_DOT " << %p", event);
        oevent->due = 0;
        q_QueueEnqueue (oevent);
    }
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
        event = q_QueueDequeueEvent (queue->head->event, NULL);
        Debug (DEB_QUEUE, STR_DOT STR_DOT "!> %s %p %p: %08lx %p %ld",
               QueueType (event->type), conn, event, event->seq, event->pak,
               event->cont ? event->cont->uin : 0);
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
            event = q_QueueDequeueEvent (iter->next->event, iter);
            Debug (DEB_QUEUE, STR_DOT STR_DOT "!> %s %p %p: %08lx %p %ld",
                   QueueType (event->type), conn, event, event->seq, event->pak,
                   event->cont ? event->cont->uin : 0);
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
void QueueRun (void)
{
    time_t now = time (NULL);
    Event *event;
    struct QueueEntry *iter;
    
    for (iter = queue->head; iter && iter->event->due <= now; iter = iter->next)
        iter->event->flags &= ~QUEUE_FLAG_CONSIDERED;
    
    while (queue->due <= now)
    {
        if (queue->head->event->flags & QUEUE_FLAG_CONSIDERED)
            break;

        event = q_QueuePop ();
        event->flags |= QUEUE_FLAG_CONSIDERED;
        event->callback (event);
    }
}

/*
 * Checks whether there is an event waiting for uin of that type,
 * and delivers the event with the lowest sequence number
 */
void QueueRetry (Connection *conn, UDWORD type, Contact *cont)
{
    struct QueueEntry *iter;
    Event *event = NULL;
    
    assert (queue);
    for (iter = queue->head; iter; iter = iter->next)
        if (iter->event->conn == conn
            && iter->event->type == type
            && iter->event->cont == cont)
        {
            if (!event || event->seq > iter->event->seq)
                event = iter->event;
        }
    
    if (event)
        event = q_QueueDequeueEvent (event, NULL);
    
    if (event)
    {
        event->due = time (NULL);
        if (event->callback)
            event->callback (event);
    }
    else
        Debug (DEB_QUEUE, STR_DOT STR_DOT "s" STR_DOT " %ld %s", cont ? cont->uin : 0, QueueType (type));
}

#ifdef ENABLE_DEBUG
/*
 * Prints the event queue.
 */
void QueuePrint (void)
{
    struct QueueEntry *iter;
    int i = 0;
    
    for (iter = queue->head; iter; iter = iter->next)
        M_printf ("%02u %08lx %p %-15s conn %p cont %p pak %p seq %ld att %ld call %p dep %p\n",
                  i++, iter->event->due, iter->event, QueueType (iter->event->type),
                  iter->event->conn, iter->event->cont, iter->event->pak,
                  iter->event->seq, iter->event->attempts, iter->event->callback, iter->event->wait);
    M_printf ("Total: %d events of %ld queued.\n", i, uiG.events);
}
#endif

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
        case QUEUE_ACKNOWLEDGE:   return "ACKNOWLEDGE";
        case QUEUE_USERFILEACK:   return "USERFILEACK";
        case QUEUE_REQUEST_ROSTER:return "REQUEST_ROSTER";
        case QUEUE_TODO_EG:       return "TODO_EG";
    }
    return s_sprintf ("%lx", type);
}

