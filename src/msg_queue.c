/************************************************************
Author Lawrence Gold
Handles resending missed packets.
*************************************************************/
#include "micq.h"
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

static struct msg_queue *queue= NULL;


void msg_queue_init( void )
{
    queue = malloc( sizeof( *queue ) );
    queue->entries = 0;
    queue->head = queue->tail = NULL;
}


struct msg *msg_queue_peek( void )
{
    if ( NULL != queue->head )
    {
        return queue->head->msg;
    }
    else
    {
        return NULL;
    }
}


struct msg *msg_queue_pop( void )
{
    struct msg *popped_msg;
    struct msg_queue_entry *temp;

    if ( ( NULL != queue->head ) && ( queue->entries > 0 ) )
    {
        popped_msg = queue->head->msg;    
        temp = queue->head->next;
        free(queue->head);
        queue->head = temp;
        queue->entries--;
        if ( NULL == queue->head )
        {
            queue->tail = NULL;
        }
       /* if ( 0x1000 < Chars_2_Word( &popped_msg->body[6] ) ) {
	   M_print( "\n\n******************************************\a\a\a\n"
	            "Error!!!!!!!!!!!!!!!!!!!!!\n" );
	}*/
        return popped_msg;
    }
    else
    {
        return NULL;
    }
}


void msg_queue_push( struct msg *new_msg)
{
    struct msg_queue_entry *new_entry;

    assert( NULL != new_msg );
    
    if ( NULL == queue ) return;

    new_entry = malloc(sizeof(struct msg_queue_entry));
    if ( new_entry == NULL ) {
       M_print( "Memory exhausted!!\a\n" );
       exit( -1 );
    }
    new_entry->next = NULL;
    new_entry->msg = new_msg;

    if (queue->tail)
    {
        queue->tail->next = new_entry;
        queue->tail = new_entry;
    }
    else
    {
        queue->head = queue->tail = new_entry; 
    }

    queue->entries++;
}

void Dump_Queue( void )
{
   int i;
   struct msg *queued_msg;
	
   assert( queue != NULL );
   assert( 0 <= queue->entries );
   
   M_print( "\nTotal entries %d\n", queue->entries );
   for (i = 0; i < queue->entries; i++)
   {
       queued_msg = msg_queue_pop();
       M_print( "SEQ = %04x\tCMD = %04x\tattempts = %d\tlen = %d\n",
          (queued_msg->seq>>16), Chars_2_Word( &queued_msg->body[CMD_OFFSET] ),
	  queued_msg->attempts, queued_msg->len );
       if ( Verbose ) {
          Hex_Dump( queued_msg->body, queued_msg->len );
       }
       msg_queue_push( queued_msg );
   }
}

void Check_Queue( DWORD seq )
{
   int i;
   struct msg *queued_msg;
	
   assert( 0 <= queue->entries );
   
   for (i = 0; i < queue->entries; i++)
   {
       queued_msg = msg_queue_pop();
       if (queued_msg->seq != seq)
       {
           msg_queue_push( queued_msg );
       }
       else
       {
           if ( Verbose )
           {
	       R_undraw ();
               M_print( "\nRemoved message with SEQ %04X CMD ", queued_msg->seq>>16);
	       Print_CMD( Chars_2_Word( &queued_msg->body[CMD_OFFSET] ) );
	       M_print( " from resend queue because of ack.\n" );
	       R_redraw ();
           }
	   if ( Chars_2_Word( &queued_msg->body[CMD_OFFSET] ) == CMD_SENDM ) {
		R_undraw();
		Time_Stamp ();
		M_print (" " ACKCOL "%10s" NOCOL " " MSGACKSTR "%s\n",
		  UIN2Name (Chars_2_DW (&queued_msg->body[PAK_DATA_OFFSET])),
		  MsgEllipsis (&queued_msg->body[32]));
		R_redraw();
           }
           free(queued_msg->body);
           free(queued_msg);

           if ((queued_msg = msg_queue_peek()) != NULL)
           {
               next_resend = queued_msg->exp_time;
           }
           else
           {
               next_resend = INT_MAX;
           }
           break;
       }
   }
}
