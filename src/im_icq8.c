/*
 * Glue code between mICQ and it's ICQv8 protocol code.
 *
 * This file is Copyright © Rüdiger Kuhlmann; it may be distributed under
 * version 2 of the GPL licence.
 *
 * $Id$
 */

#include "micq.h"
#include "contact.h"
#include "session.h"
#include "cmd_pkt_v8.h"
#include "cmd_pkt_v8_snac.h"
#include "cmd_pkt_v8_flap.h"
#include "cmd_pkt_v8_tlv.h"
#include "im_icq8.h"

UBYTE IMRoster (Connection *conn, int mode)
{
    ContactGroup *cg;
    Contact *cont;
    int i;
    
    for (i = 0, cg = conn->contacts; (cont = ContactIndex (cg, i)); i++)
        cont->oldflags &= ~(CONT_TMPSBL | CONT_ISSBL);
    
    switch (mode)
    {
        case IMROSTER_UPLOAD:
        case IMROSTER_EXPORT:
        case IMROSTER_DOWNLOAD:
        case IMROSTER_IMPORT:
        case IMROSTER_SYNC:
        case IMROSTER_SHOW:
        case IMROSTER_DIFF:
            QueueEnqueueData (conn, QUEUE_REQUEST_ROSTER, mode, 0x7fffffffL, NULL, NULL, NULL, NULL);
            SnacCliCheckroster (conn);
            return RET_OK;
    }
    return RET_DEFER;
}
