
/*
 * Handles (mostly optional) extra data (e.g. for messages).
 *
 * This file is Copyright © Rüdiger Kuhlmann; it may be distributed under
 * version 2 of the GPL licence.
 *
 * $Id$
 */

#include "micq.h"
#include <string.h>
#include "util_extra.h"
#include "util_str.h"
#include "util_ui.h"

void ExtraD (Extra *extra)
{
    Extra *tmp;
    if (extra)
        Debug (DEB_EXTRA, "***> %p (%04lx: '%s' %08lx)", extra, extra->tag,
               extra->text ? extra->text : "", extra->data);
    while (extra)
    {
        tmp = extra->more;
        s_free (extra->text);
        free (extra);
        extra = tmp;
    }
}

Extra *ExtraClone (Extra *extra)
{
    Extra *nex;

    if (!extra)
        return NULL;
    if (!(nex = calloc (1, sizeof (Extra))))
        return NULL;
    *nex = *extra;
    nex->text = nex->text ? strdup (nex->text) : nex->text;
    Debug (DEB_EXTRA, "*+=* %p (%04lx: '%s' %08lx)", nex, nex->tag,
           nex->text ? nex->text : "", nex->data);
    nex->more = ExtraClone (extra->more);
    return nex;
}

Extra *ExtraFind (Extra *extra, UDWORD type)
{
    while (extra && extra->tag != type)
        extra = extra->more;
    return extra ? extra : NULL;
}

UDWORD ExtraGet (Extra *extra, UDWORD type)
{
    while (extra && extra->tag != type)
        extra = extra->more;
    return extra ? extra->data : 0;
}

const char *ExtraGetS (Extra *extra, UDWORD type)
{
    while (extra && extra->tag != type)
        extra = extra->more;
    return extra ? extra->text : NULL;
}

Extra *ExtraSet (Extra *extra, UDWORD type, UDWORD value, const char *text)
{
    Extra *tmp, *old = NULL;

    for (tmp = extra; tmp; tmp = tmp->more)
    {
        old = tmp;
        if (tmp->tag == type)
        {
            tmp->data = value;
            s_repl (&tmp->text, text);
            return extra;
        }
    }

    tmp = calloc (1, sizeof (Extra));
    if (!tmp)
        return extra;
    tmp->tag = type;
    tmp->data = value;
    tmp->text = text ? strdup (text) : NULL;
    if (!old)
    {
        Debug (DEB_EXTRA, "<*** %p (%04x: '%s' %08lx)", tmp, type, tmp->text ? tmp->text : "", value);
        return tmp;
    }
    old->more = tmp;
    return extra;
}


