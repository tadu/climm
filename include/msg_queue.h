/* $Id$ */

#ifndef MICQ_MSG_QUEUE_H
#define MICQ_MSG_QUEUE_H

typedef void (Queuef)(Event *event);

struct Event_s
{
    Connection *conn;
    UDWORD      type;
    UDWORD      seq;
    UDWORD      attempts;
    UDWORD      uin;
    time_t      due;
    Packet     *pak;
    char       *info;
    MetaList   *extra;
    Queuef     *callback;
    UBYTE       flags;
};

void        QueueInit        (Queue **queue);
void        QueueEnqueue     (Event *event);
Event      *QueueEnqueueData (Connection *conn, UDWORD type, UDWORD seq,
                              UDWORD uin, time_t due,
                              Packet *pak, char *info, Queuef *callback);
Event      *QueueDequeue     (Connection *conn, UDWORD type, UDWORD seq);
void        QueueRun         ();
void        QueueRetry       (Connection *conn, UDWORD type, UDWORD uin);
void        QueueCancel      (Connection *conn);

Event      *QueuePeek        ();
Event      *QueuePop         ();

const char *QueueType   (UDWORD type);

#define QUEUE_PEER_RESEND   45
#define QUEUE_PEER_FILE     60
#define QUEUE_UDP_RESEND    43
#define QUEUE_TCP_RESEND    42
#define QUEUE_TCP_TIMEOUT   72
#define QUEUE_CON_TIMEOUT   77
#define QUEUE_TCP_RECEIVE   32
#define QUEUE_UDP_KEEPALIVE 23
#define QUEUE_SRV_KEEPALIVE 21
#define QUEUE_FLAP          34
#define QUEUE_REQUEST_ROSTER  90
#define QUEUE_REQUEST_META    91

#define QUEUE_FLAG_CONSIDERED     1 /* this event has been considered and won't
                                       be tried again in this queue run */

#endif /* MICQ_MSG_QUEUE_H */
