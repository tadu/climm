/* $Id$ */

#ifndef MSG_QUEUE_H
#define MSG_QUEUE_H

typedef void (Queuef)(Event *event);

struct Event_s
{
    Session *sess;
    UDWORD   type;
    UDWORD   seq;
    UDWORD   attempts;
    UDWORD   uin;
    time_t   due;
    Packet  *pak;
    char    *info;
    Queuef  *callback;
    UBYTE    flags;
};

void        QueueInit        (Queue **queue);
void        QueueEnqueue     (Event *event);
void        QueueEnqueueData (Session *sess, UDWORD type, UDWORD seq,
                              UDWORD uin, time_t due,
                              Packet *pak, char *info, Queuef *callback);
Event      *QueueDequeue     (Session *sess, UDWORD type, UDWORD seq);
void        QueueRun         ();
void        QueueRetry       (Session *sess, UDWORD type, UDWORD uin);
void        QueueCancel      (Session *sess);

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

#define QUEUE_FLAG_CONSIDERED     1 /* this event has been considered and won't
                                       be tried again in this queue run */

#endif
