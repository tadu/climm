/* $Id$ */

#ifndef MICQ_MSG_QUEUE_H
#define MICQ_MSG_QUEUE_H

typedef void (Queuef)(Event *event);

struct Event_s
{
    Event      *wait;
    Connection *conn;
    Contact    *cont;
    UDWORD      type;
    UDWORD      seq;
    UDWORD      attempts;
    time_t      due;
    Packet     *pak;
    Opt *opt;
    Queuef     *callback;
    UBYTE       flags;
};

void        QueueInit         (Queue **queue);
UDWORD      QueueTime         (void);
Event      *QueueDequeueEvent (Event *event DEBUGPARAM);
void        QueueEnqueue      (Event *event DEBUGPARAM);
Event      *QueueEnqueueData  (Connection *conn, UDWORD type, UDWORD seq, time_t due,
                               Packet *pak, Contact *cont, Opt *opt, Queuef *callback DEBUGPARAM);
Event      *QueueEnqueueDep   (Connection *conn, UDWORD type, UDWORD seq, Event *dep,
                               Packet *pak, Contact *cont, Opt *opt, Queuef *callback DEBUGPARAM);
Event      *QueueDequeue      (Connection *conn, UDWORD type, UDWORD seq DEBUGPARAM);
Event      *QueueDequeue2     (Connection *conn, UDWORD type, UDWORD seq, Contact *cont DEBUGPARAM);
void        QueueRetry        (Connection *conn, UDWORD type, Contact *cont DEBUGPARAM);
void        QueueRelease      (Event *event DEBUGPARAM);
void        QueueCancel       (Connection *conn DEBUGPARAM);
void        QueueRun          (DEBUG0PARAM);
void        EventD            (Event *event DEBUGPARAM);

#define QueueDequeueEvent(e)               QueueDequeueEvent(e DEBUGARGS)
#define QueueEnqueue(e)                    QueueEnqueue(e DEBUGARGS)
#define QueueEnqueueData(c,t,s,d,p,C,o,cb) QueueEnqueueData(c,t,s,d,p,C,o,cb DEBUGARGS)
#define QueueEnqueueDep(c,t,s,d,p,C,o,cb)  QueueEnqueueDep(c,t,s,d,p,C,o,cb DEBUGARGS)
#define QueueDequeue(c,t,s)                QueueDequeue(c,t,s DEBUGARGS)
#define QueueDequeue2(c,t,s,C)             QueueDequeue2(c,t,s,C DEBUGARGS)
#define QueueRetry(c,t,C)                  QueueRetry(c,t,C DEBUGARGS)
#define QueueRelease(e)                    QueueRelease(e DEBUGARGS)
#define QueueCancel(c)                     QueueCancel(c DEBUGARGS)
#define QueueRun()                         QueueRun(DEBUG0ARGS)
#define EventD(e)                          EventD(e DEBUGARGS)

const char *QueueType (UDWORD type);

#ifdef ENABLE_DEBUG
void        QueuePrint (void);

#endif

#define NOW   (time_t)-1
#define NEVER (time_t)0x7fffffff

#define QUEUE_PEER_RESEND       45
#define QUEUE_PEER_FILE         60
#define QUEUE_UDP_RESEND        43
#define QUEUE_TCP_RESEND        42
#define QUEUE_TYPE2_RESEND      41
#define QUEUE_TYPE2_RESEND_ACK  40
#define QUEUE_TCP_TIMEOUT       72
#define QUEUE_CON_TIMEOUT       77
#define QUEUE_TCP_RECEIVE       32
#define QUEUE_UDP_KEEPALIVE     23
#define QUEUE_SRV_KEEPALIVE     21
#define QUEUE_FLAP              34
#define QUEUE_REQUEST_ROSTER    90
#define QUEUE_REQUEST_META      91
#define QUEUE_CHANGE_ROSTER     92
#define QUEUE_USERFILEACK       81
#define QUEUE_ACKNOWLEDGE       80
#define QUEUE_MICQ_COMMAND      11
#define QUEUE_DEP_OSCARLOGIN    12
#define QUEUE_CACHE_MSG         100

#define QUEUE_FLAG_CONSIDERED     1 /* this event has been considered and won't
                                       be tried again in this queue run */

#endif /* MICQ_MSG_QUEUE_H */
