#ifndef MSG_QUEUE_H
#define MSG_QUEUE_H

#include "datatype.h"

struct msg
{
    UDWORD seq;
    UDWORD attempts;
    UDWORD exp_time;
    UBYTE *body;
    UDWORD len;
    UDWORD dest_uin;
};

struct msg_queue_entry
{
    struct msg *msg;
    struct msg_queue_entry *next;
};

struct msg_queue
{
    int entries;
    struct msg_queue_entry *head;
    struct msg_queue_entry *tail;
    UDWORD exp_time;
};

void        msg_queue_init          (struct msg_queue **queue);
struct msg *msg_queue_peek          (struct msg_queue  *queue);
struct msg *msg_queue_pop           (struct msg_queue  *queue);
void        msg_queue_enqueue       (struct msg_queue  *queue, struct msg *new_msg);
struct msg *msg_queue_dequeue_seq   (struct msg_queue  *queue, UDWORD seq);
void        Check_Queue (UDWORD seq, struct msg_queue *queue);

#define     msg_queue_push(m,q)   msg_queue_enqueue(q,m)

#endif
