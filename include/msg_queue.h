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

void          QueueInit        (struct Queue **queue);
void          QueueEnqueue     (struct Queue  *queue, struct Event *event);
void          QueueEnqueueData (struct Queue  *queue, Session *sess, UDWORD seq, UDWORD type,
                                UDWORD uin, time_t due,
                                Packet *pak, UBYTE *info, Queuef *callback);
struct Event *QueueDequeue     (struct Queue  *queue, UDWORD seq, UDWORD type);
void          QueueRun         (struct Queue  *queue);

struct Event *QueuePeek        (struct Queue  *queue);
struct Event *QueuePop         (struct Queue  *queue);

#define QUEUE_TYPE_PEER_RESEND   45
#define QUEUE_TYPE_UDP_RESEND    43
#define QUEUE_TYPE_TCP_RESEND    42
#define QUEUE_TYPE_TCP_TIMEOUT   72
#define QUEUE_TYPE_CON_TIMEOUT   77
#define QUEUE_TYPE_TCP_RECEIVE   32
#define QUEUE_TYPE_UDP_KEEPALIVE 23
#define QUEUE_TYPE_SRV_KEEPALIVE 21
#define QUEUE_TYPE_FLAC          34

#endif
