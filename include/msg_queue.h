/* $Id$ */

#ifndef MSG_QUEUE_H
#define MSG_QUEUE_H

typedef void (Queuef)(Event *event);

struct Event_s
{
    UDWORD   seq;
    UDWORD   type;
    UDWORD   attempts;
    UDWORD   uin;
    time_t   due;
    Packet  *pak;
    UBYTE   *info;
    Queuef  *callback;
    Session *sess;
};

void        QueueInit        (Queue **queue);
void        QueueEnqueue     (Event *event);
void        QueueEnqueueData (Session *sess, UDWORD seq, UDWORD type,
                              UDWORD uin, time_t due,
                              Packet *pak, UBYTE *info, Queuef *callback);
Event      *QueueDequeue     (UDWORD seq, UDWORD type);
void        QueueRun         ();

Event      *QueuePeek        ();
Event      *QueuePop         ();

#define QUEUE_TYPE_PEER_RESEND   45
#define QUEUE_TYPE_UDP_RESEND    43
#define QUEUE_TYPE_TCP_RESEND    42
#define QUEUE_TYPE_TCP_TIMEOUT   72
#define QUEUE_TYPE_CON_TIMEOUT   77
#define QUEUE_TYPE_TCP_RECEIVE   32
#define QUEUE_TYPE_UDP_KEEPALIVE 23
#define QUEUE_TYPE_SRV_KEEPALIVE 21
#define QUEUE_TYPE_FLAP          34

#endif
