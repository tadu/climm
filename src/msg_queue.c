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
 * This file replaces a simple FIFO implementation for a similar,
 * but more limited, purpose by Lawrence Gold.
 *
 * $Id$
 */

#include <assert.h>
#include <limits.h>
#include "micq.h"
#include "msg_queue.h"
#include "util_ui.h"
#include "contact.h" /* for cont->screen */
#include "preferences.h"
#include "im_response.h" /* yuck */
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
static Event *q_QueuePop (DEBUG0PARAM);
static void   q_QueueEnqueue (Event *event);

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
static Event *q_QueuePop (DEBUG0PARAM)
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
        Debug (DEB_QUEUE, STR_DOT STR_DOT STR_DOT "> %s %p: %08lx %p %s @ %p",
               QueueType (event->type), event, event->seq, event->pak,
               event->cont ? event->cont->screen : "", event->conn);
        return event;
    }
    return NULL;
}

/*
 * Adds a new entry to the queue.
 */
#undef QueueEnqueue
void QueueEnqueue (Event *event DEBUGPARAM)
{
    Debug (DEB_QUEUE, "<" STR_DOT STR_DOT STR_DOT " %s %p: %08lx %p %s %x @ %p t %ld",
           QueueType (event->type), event, event->seq, event->pak,
           event->cont ? event->cont->screen : "", event->flags, event->conn, (long)event->due);
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
#undef QueueEnqueueData
Event *QueueEnqueueData (Connection *conn, UDWORD type, UDWORD id,
                         time_t due, Packet *pak, Contact *cont,
                         Opt *opt, Queuef *callback DEBUGPARAM)
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
    event->data = NULL;
    event->callback = callback;
    event->cancel = NULL;
    
    Debug (DEB_EVENT, "<+" STR_DOT STR_DOT " %s %p: %08lx %p %s %x @ %p",
           QueueType (event->type), event, event->seq, event->pak,
           event->cont ? event->cont->screen : "", event->flags, event->conn);
    q_QueueEnqueue (event);

    return event;
}

/*
 * Adds a new entry to the queue. Creates Event for you.
 */
#undef QueueEnqueueData2
Event *QueueEnqueueData2 (Connection *conn, UDWORD type, UDWORD ref, UDWORD wait,
                          void *data, Queuef *callback, Queuef *cancel DEBUGPARAM)
{
    Event *event = calloc (sizeof (Event), 1);
    uiG.events++;
    assert (event);
    
    event->conn = conn;
    event->type = type;
    event->seq  = ref;
    event->attempts = 1;
    event->cont  = NULL;
    event->due  = time (NULL) + wait;
    event->pak = NULL;
    event->opt = NULL;
    event->data = data;
    event->callback = callback;
    event->cancel = cancel;
    
    Debug (DEB_EVENT, "<+" STR_DOT STR_DOT " %s %p: %08lx %p @ %p",
           QueueType (event->type), event, event->seq, event->data, event->conn);
    q_QueueEnqueue (event);

    return event;
}

/*
 * Adds a new waiting entry to the queue. Creates Event for you.
 */
#undef QueueEnqueueDep
Event *QueueEnqueueDep (Connection *conn, UDWORD type, UDWORD id,
                        Event *dep, Packet *pak, Contact *cont,
                        Opt *opt, Queuef *callback DEBUGPARAM)
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
    
    Debug (DEB_EVENT, "<*" STR_DOT STR_DOT " %s %p: %08lx %p %s %x @ %p",
           QueueType (event->type), event, event->seq, event->pak,
           event->cont ? event->cont->screen : "", event->flags, event->conn);
    q_QueueEnqueue (event);

    return event;
}

#undef QueueDequeueEvent
Event *QueueDequeueEvent (Event *event DEBUGPARAM)
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
#undef QueueDequeue
Event *QueueDequeue (Connection *conn, UDWORD type, UDWORD seq DEBUGPARAM)
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
        Debug (DEB_QUEUE, STR_DOT STR_DOT "s> %s %p: %08lx %p %s",
               QueueType (type), event, seq, event->pak, event->cont ? event->cont->screen : "");
        return event;
    }
    for (iter = queue->head; iter->next; iter = iter->next)
    {
        if (   iter->next->event->conn == conn
            && iter->next->event->type == type
            && iter->next->event->seq  == seq)
        {
            event = q_QueueDequeueEvent (iter->next->event, iter);
            Debug (DEB_QUEUE, STR_DOT STR_DOT "s> %s %p: %08lx %p %s",
                   QueueType (type), event, seq, event->pak, event->cont ? event->cont->screen : "");
            return event;
        }
    }
    Debug (DEB_QUEUE, STR_DOT "??" STR_DOT " %s %08lx", QueueType (type), seq);
    return NULL;
}

/*
 * Removes and returns an event given by type, and sequence number and/or UIN.
 */
#undef QueueDequeue2
Event *QueueDequeue2 (Connection *conn, UDWORD type, UDWORD seq, Contact *cont DEBUGPARAM)
{
    Event *event;
    struct QueueEntry *iter;

    assert (queue);

    if (!queue->head)
    {
        Debug (DEB_QUEUE, STR_DOT "??" STR_DOT " %s %08lx %s", QueueType (type), seq, cont ? cont->screen : "");
        return NULL;
    }

    if (   queue->head->event->conn == conn
        && queue->head->event->type == type
        && (!seq || queue->head->event->seq == seq)
        && (!cont || queue->head->event->cont == cont))
    {
        event = q_QueueDequeueEvent (queue->head->event, NULL);
        Debug (DEB_QUEUE, STR_DOT STR_DOT "s> %s %p: %08lx %p %s",
               QueueType (type), event, event->seq, event->pak,
               event->cont ? event->cont->screen : "");
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
            Debug (DEB_QUEUE, STR_DOT STR_DOT "s> %s %p: %08lx %p %s",
                   QueueType (type), event, event->seq, event->pak,
                   event->cont ? event->cont->screen : "");
            return event;
        }
    }
    Debug (DEB_QUEUE, STR_DOT "??" STR_DOT " %s %08lx %s @ %p",
           QueueType (type), seq, cont ? cont->screen : "", conn);
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

#undef EventD
void EventD (Event *event DEBUGPARAM)
{
    Event *oevent;

    if (!event)
        return;
    if (q_QueueDequeueEvent (event, NULL))
        rl_printf ("FIXME: Deleting still queued event %p!\n", event);
    Debug (DEB_EVENT, STR_DOT STR_DOT ">> %s %p: %08lx %p %s",
           QueueType (event->type), event, event->seq, event->pak,
           event->cont ? event->cont->screen : "");
    if (event->pak)
        PacketD (event->pak);
    OptD (event->opt);

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
#undef QueueRelease
void QueueRelease (Event *event DEBUGPARAM)
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
#undef QueueCancel
void QueueCancel (Connection *conn DEBUGPARAM)
{
    Event *event;
    struct QueueEntry *iter;

    assert (queue);

    while (queue->head && queue->head->event->conn == conn)
    {
        event = q_QueueDequeueEvent (queue->head->event, NULL);
        Debug (DEB_QUEUE, STR_DOT STR_DOT "!> %s %p %p: %08lx %p %s",
               QueueType (event->type), conn, event, event->seq, event->pak,
               event->cont ? event->cont->screen : "");
        if (event->cancel)
            event->cancel (event);
        else
        {
            event->conn = NULL;
            if (event->callback)
                event->callback (event);
        }
    }

    if (!queue->head)
        return;

    for (iter = queue->head; iter && iter->next; iter = iter->next)
    {
        while (iter->next && iter->next->event->conn == conn)
        {
            event = q_QueueDequeueEvent (iter->next->event, iter);
            Debug (DEB_QUEUE, STR_DOT STR_DOT "!> %s %p %p: %08lx %p %s",
                   QueueType (event->type), conn, event, event->seq, event->pak,
                   event->cont ? event->cont->screen : "");
            if (event->cancel)
                event->cancel (event);
            else
            {
                event->conn = NULL;
                if (event->callback)
                    event->callback (event);
            }
        }
    }
}

/*
 * Delivers all due events by removing them from the queue and calling their
 * callback function.  Callback function may re-enqueue them, but the event
 * is not reconsidered even if it is still due.
 */
#undef QueueRun
void QueueRun (DEBUG0PARAM)
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

        event = q_QueuePop (DEBUG0ARGS);
        event->flags |= QUEUE_FLAG_CONSIDERED;
        event->callback (event);
    }
}

/*
 * Checks whether there is an event waiting for uin of that type,
 * and delivers the event with the lowest sequence number
 */
#undef QueueRetry
void QueueRetry (Connection *conn, UDWORD type, Contact *cont DEBUGPARAM)
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
        Debug (DEB_QUEUE, STR_DOT STR_DOT "s" STR_DOT " %s %s", cont ? cont->screen : "", QueueType (type));
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
        rl_printf ("%02u %08lx %p %-15s conn %p cont %p pak %p seq %ld att %ld call %p dep %p\n",
                  i++, iter->event->due, iter->event, QueueType (iter->event->type),
                  iter->event->conn, iter->event->cont, iter->event->pak,
                  iter->event->seq, iter->event->attempts, iter->event->callback, iter->event->wait);
    rl_printf ("Total: %d events of %ld queued.\n", i, uiG.events);
}
#endif

/*
 * Returns a string representation of the queue type.
 */
const char *QueueType (UDWORD type)
{
    switch (type)
    {
        case QUEUE_FLAP:           return "FLAP";
        case QUEUE_SRV_KEEPALIVE:  return "SRV_KEEPALIVE";
        case QUEUE_UDP_KEEPALIVE:  return "UDP_KEEPALIVE";
        case QUEUE_TCP_RECEIVE:    return "TCP_RECEIVE";
        case QUEUE_CON_TIMEOUT:    return "CON_TIMEOUT";
        case QUEUE_TCP_TIMEOUT:    return "TCP_TIMEOUT";
        case QUEUE_TCP_RESEND:     return "TCP_RESEND";
        case QUEUE_UDP_RESEND:     return "UDP_RESEND";
        case QUEUE_PEER_FILE:      return "PEER_FILE";
        case QUEUE_PEER_RESEND:    return "PEER_RESEND";
        case QUEUE_TYPE2_RESEND:   return "TYPE2_RESEND";
        case QUEUE_TYPE1_RESEND_ACK: return "TYPE1_R_ACK";
        case QUEUE_TYPE2_RESEND_ACK: return "TYPE2_R_ACK";
        case QUEUE_TYPE4_RESEND_ACK: return "TYPE4_R_ACK";
        case QUEUE_ACKNOWLEDGE:    return "ACKNOWLEDGE";
        case QUEUE_USERFILEACK:    return "USERFILEACK";
        case QUEUE_REQUEST_ROSTER: return "REQUEST_ROSTER";
        case QUEUE_MICQ_COMMAND:   return "MICQ_COMMAND";
        case QUEUE_DEP_WAITLOGIN:  return "DEP_WAITLOGIN";
    }
    return s_sprintf ("%lx", type);
}

