
#ifndef MSG_QUEUE_H
#define MSG_QUEUE_H

#include "datatype.h"

struct Event;
struct Queue;

typedef void (Queuef)(SOK_T srvsok, struct Event *event);

struct Event
{
    UDWORD  seq;
    UDWORD  type;
    UDWORD  attempts;
    UDWORD  uin;
    time_t  due;
    UDWORD  len;
    UBYTE  *body;
    UBYTE  *info;
    Queuef *callback;
};

void          QueueInit        (struct Queue **queue);
void          QueueEnqueue     (struct Queue  *queue, struct Event *event);
void          QueueEnqueueData (struct Queue  *queue, UDWORD seq, UDWORD type,
                                UDWORD uin, time_t due, UDWORD len,
                                UBYTE *body, UBYTE *info, Queuef *callback);
struct Event *QueueDequeue     (struct Queue  *queue, UDWORD seq, UDWORD type);
void          QueueRun         (struct Queue  *queue, SOK_T srvsok);

struct Event *QueuePeek        (struct Queue  *queue);
struct Event *QueuePop         (struct Queue  *queue);

#define QUEUE_TYPE_UDP_RESEND  23
#define QUEUE_TYPE_TCP_RESEND  42
#define QUEUE_TYPE_TCP_TIMEOUT 76
#define QUEUE_TYPE_TCP_RECEIVE 31

#endif
