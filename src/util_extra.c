
/*
 * Handles (mostly optional) extra data (e.g. for messages).
 *
 * This file is Copyright  Rdiger Kuhlmann; it may be distributed under
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
        Debug (DEB_EXTRA, "***> %p (%04x: '%s' %08x)", extra, extra->tag,
               extra->text ? extra->text : "", extra->data);
    while (extra)
    {
        tmp = extra->more;
        s_free (extra->text);
        free (extra);
        extra = tmp;
    }
}

Extra *ExtraFind (Extra *extra, UWORD type)
{
    while (extra && extra->tag != type)
        extra = extra->more;
    if (extra)
        return extra;
    return 0;
}

UDWORD ExtraGet (Extra *extra, UWORD type)
{
    while (extra && extra->tag != type)
        extra = extra->more;
    if (extra)
        return extra->data;
    return 0;
}

const char *ExtraGetS (Extra *extra, UWORD type)
{
    while (extra && extra->tag != type)
        extra = extra->more;
    if (extra)
        return extra->text;
    return NULL;
}

Extra *ExtraSet (Extra *extra, UWORD type, UDWORD value, const char *text)
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
    Debug (DEB_EXTRA, "<*** %p (%04x: '%s' %08x)", tmp, type, text ? text : "", value);
    tmp->tag = type;
    tmp->data = value;
    tmp->text = text ? strdup (text) : NULL;
    if (!old)
        return tmp;
    old->more = tmp;
    return extra;
}


