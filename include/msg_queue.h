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
};

void        msg_queue_init (void);
struct msg *msg_queue_peek (void);
struct msg *msg_queue_pop (void);
void        msg_queue_push (struct msg *new_msg);
void        Check_Queue (UDWORD seq);
void        Dump_Queue (void);

#endif
