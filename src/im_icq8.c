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
    switch (mode)
    {
        ContactGroup *cg;
        Contact *cont;
        int i;

        case IMROSTER_UPLOAD:
        case IMROSTER_EXPORT:
//            for (i = 0; (cg = ContactGroupIndex (i)); i++)
//                SnacCliRosteradd (conn, cg, NULL);

//            for (i = 0, cg = conn->contacts; (cont = ContactIndex (cg, i)); i++)
//                if (~cont->flags & CONT_ISSBL)
//                    SnacCliRosteradd (conn, cg, cont);

//            for (i = 0; (cg = ContactGroupIndex (i)); i++)
//                SnacCliRosterupdate (conn, cg, NULL);
        case IMROSTER_DOWNLOAD:
        case IMROSTER_IMPORT:
        case IMROSTER_SYNC:
        case IMROSTER_SHOW:
        case IMROSTER_DIFF:
            for (i = 0, cg = conn->contacts; (cont = ContactIndex (cg, i)); i++)
                cont->flags &= ~CONT_ISSBL;
            QueueEnqueueData (conn, QUEUE_REQUEST_ROSTER, 0, 0x7fffffffL, NULL, mode, NULL, NULL);
            SnacCliCheckroster (conn);
            return RET_OK;
    }
    return RET_DEFER;
}