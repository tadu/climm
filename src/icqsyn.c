/*
 * Dump packet according to syntax
 *
 * icqsyn Copyright (C) © 2007 Rüdiger Kuhlmann
 *
 * icqsyn is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 dated June, 1991.
 *
 * icqsyn is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * $Id$
 */

#include "climm.h"
#include <assert.h>
#include "util_syntax.h"
#include "conv.h"
#include "packet.h"

user_interface_state uiG = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
Preferences *prG;

const char *ContactPrefStr (Contact *cont, UDWORD flag) { return ""; }
const char  *i18n (int i, const char *s) { return s; }

BOOL DebugReal (UDWORD level, const char *str, ...)  { return 1; }
Contact *ContactScreen (Connection *serv, const char *screen DEBUGPARAM) { return NULL; }
const UDWORD BuildVersionNum = 0;

#undef COLDEBUG
#undef COLNONE
#define COLDEBUG YELLOW
#define COLNONE SGR0

/*
 * Read a complete line from a fd.
 * Returned string may not be free()d.
 */
strc_t UtilIOReadline (FILE *fd)
{
    static str_s str;
    char *p;

    s_init (&str, "", 256);
    while (1)
    {
        str.txt[str.max - 2] = 0;
        if (!fgets (str.txt + str.len, str.max - str.len, fd))
        {
            str.txt[str.len] = '\0';
            if (!str.len)
                return NULL;
            break;
        }
        str.txt[str.max - 1] = '\0';
        str.len = strlen (str.txt);
        if (!str.txt[str.max - 2])
            break;
        s_blow (&str, 128);
    }
    if ((p = strpbrk (str.txt, "\r\n")))
    {
        *p = 0;
        str.len = strlen (str.txt);
    }
    return &str;
}

typedef struct { UWORD fam; UWORD cmd; const char *name; void *f; } SNAC;

static SNAC SNACS[] = {
    {  1,  1, "SRV_SERVICEERR",      NULL },
    {  1,  3, "SRV_FAMILIES",        NULL },
    {  1,  7, "SRV_RATES",           NULL },
    {  1, 10, "SRV_RATEEXCEEDED",    NULL },
    {  1, 11, "SRV_SERVERPAUSE",     NULL },
    {  1, 15, "SRV_REPLYINFO",       NULL },
    {  1, 18, "SRV_MIGRATIONREQ",    NULL },
    {  1, 19, "SRV_MOTD",            NULL },
    {  1, 24, "SRV_FAMILIES2",       NULL },
    {  2,  1, "SRV_LOCATIONERR",     NULL },
    {  2,  3, "SRV_REPLYLOCATION",   NULL },
    {  2,  6, "SRV_USERINFO",        NULL },
    {  3,  1, "SRV_CONTACTERR",      NULL },
    {  3,  3, "SRV_REPLYBUDDY",      NULL },
    {  3, 11, "SRV_USERONLINE",      NULL },
    {  3, 10, "SRV_REFUSE",          NULL },
    {  3, 12, "SRV_USEROFFLINE",     NULL },
    {  4,  1, "SRV_ICBMERR",         NULL },
    {  4,  5, "SRV_REPLYICBM",       NULL },
    {  4,  7, "SRV_RECVMSG",         NULL },
    {  4, 10, "SRV_MISSEDICBM",      NULL },
    {  4, 11, "SRV/CLI_ACKMSG",      NULL },
    {  4, 12, "SRV_SRVACKMSG",       NULL },
    {  9,  1, "SRV_BOSERR",          NULL },
    {  9,  3, "SRV_REPLYBOS",        NULL },
    { 11,  2, "SRV_SETINTERVAL",     NULL },
    { 19,  3, "SRV_REPLYLISTS",      NULL },
    { 19,  6, "SRV_REPLYROSTER",     NULL },
    { 19,  8, "SRV_ROSTERADD",       NULL },
    { 19,  9, "SRV_ROSTERUPDATE",    NULL },
    { 19, 10, "SRV_ROSTERDELETE",    NULL },
    { 19, 14, "SRV_UPDATEACK",       NULL },
    { 19, 15, "SRV_REPLYROSTEROK",   NULL },
    { 19, 17, "SRV_ADDSTART",        NULL },
    { 19, 18, "SRV_ADDEND",          NULL },
    { 19, 25, "SRV_AUTHREQ",         NULL },
    { 19, 27, "SRV_AUTHREPLY",       NULL },
    { 19, 28, "SRV_ADDEDYOU",        NULL },
    { 21,  1, "SRV_TOICQERR",        NULL },
    { 21,  3, "SRV_FROMICQSRV",      NULL },
    { 23,  1, "SRV_REGREFUSED",      NULL },
    { 23,  3, "SRV_REPLYLOGIN",      NULL },
    { 23,  5, "SRV_NEWUIN",          NULL },
    { 23,  7, "SRV_LOGINKEY",        NULL },
    {  1,  2, "CLI_READY",           NULL},
    {  1,  6, "CLI_RATESREQUEST",    NULL},
    {  1,  8, "CLI_ACKRATES",        NULL},
    {  1, 12, "CLI_ACKSERVERPAUSE",  NULL},
    {  1, 14, "CLI_REQINFO",         NULL},
    {  1, 23, "CLI_FAMILIES",        NULL},
    {  1, 30, "CLI_SETSTATUS",       NULL},
    {  2,  2, "CLI_REQLOCATION",     NULL},
    {  2,  4, "CLI_SETUSERINFO",     NULL},
    {  2,  5, "CLI_REQUSERINFO",     NULL},
    {  3,  2, "CLI_REQBUDDY",        NULL},
    {  3,  4, "CLI_ADDCONTACT",      NULL},
    {  3,  5, "CLI_REMCONTACT",      NULL},
    {  4,  2, "CLI_SETICBM",         NULL},
    {  4,  4, "CLI_REQICBM",         NULL},
    {  4,  6, "CLI_SENDMSG",         NULL},
    {  9,  2, "CLI_REQBOS",          NULL},
    {  9,  5, "CLI_ADDVISIBLE",      NULL},
    {  9,  6, "CLI_REMVISIBLE",      NULL},
    {  9,  7, "CLI_ADDINVISIBLE",    NULL},
    {  9,  8, "CLI_REMINVISIBLE",    NULL},
    { 19,  2, "CLI_REQLISTS",        NULL},
    { 19,  4, "CLI_REQROSTER",       NULL},
    { 19,  5, "CLI_CHECKROSTER",     NULL},
    { 19,  7, "CLI_ROSTERACK",       NULL},
    { 19,  8, "CLI_ROSTERADD",       NULL},
    { 19,  9, "CLI_ROSTERUPDATE",    NULL},
    { 19, 10, "CLI_ROSTERDELETE",    NULL},
    { 19, 17, "CLI_ADDSTART",        NULL},
    { 19, 18, "CLI_ADDEND",          NULL},
    { 19, 20, "CLI_GRANTAUTH?",      NULL},
    { 19, 24, "CLI_REQAUTH",         NULL},
    { 19, 26, "CLI_AUTHORIZE",       NULL},
    { 21,  2, "CLI_TOICQSRV",        NULL},
    { 23,  2, "CLI_MD5LOGIN",        NULL},
    { 23,  4, "CLI_REGISTERUSER",    NULL},
    { 23,  6, "CLI_REQLOGIN",        NULL},
    {  0,  0, "_unknown",             NULL}
};

/*
 * Returns the name of the SNAC, or "unknown".
 */
const char *SnacName (UWORD fam, UWORD cmd)
{
    SNAC *s;

    for (s = SNACS; s->fam; s++)
        if (s->fam == fam && s->cmd == cmd)
            return strchr (s->name, '_') + 1;
    return s->name;
}

/*
 * Print a given SNAC.
 */
void SnacPrint (Packet *pak, int out)
{
    UWORD fam, cmd, flag, opos = pak->rpos;
    UDWORD ref, len;

    pak->rpos = 6;
    fam  = PacketReadB2 (pak);
    cmd  = PacketReadB2 (pak);
    flag = PacketReadB2 (pak);
    ref  = PacketReadB4 (pak);
    len  = PacketReadAtB2 (pak, pak->rpos);

    printf ("  %s %sSNAC     (%x,%x) [%s_%s] flags %x ref %lx",
             s_dumpnd (pak->data + 6, flag & 0x8000 ? 10 + len + 2 : 10),
             COLDEBUG, fam, cmd,
             out ? "CLI" : "SRV",
             SnacName (fam, cmd), flag, ref);
    if (flag & 0x8000)
    {
        printf ("   extra (%ld)", len);
        pak->rpos += len + 2;
    }
    printf ("%s\n", COLNONE);

    {
        char *f, *syn = strdup (s_sprintf ("gs%dx%ds", fam, cmd));
        f = PacketDump (pak, syn, COLDEBUG, COLNONE);
        free (syn);
        printf ("%s", s_ind (f));
        free (f);
    }

    pak->rpos = opos;
}

void FlapPrint (Packet *pak, int out)
{
    UWORD opos = pak->rpos;
    UBYTE ch;
    UWORD seq, len;

    pak->rpos = 1;
    ch  = PacketRead1 (pak);
    seq = PacketReadB2 (pak);
    len = PacketReadB2 (pak);

    printf ("%s\n  %s %sFLAP %s ch %d seq %08x length %04x%s\n",
              COLNONE, s_dumpnd (pak->data, 6), COLDEBUG,
              out ? ">>>" : "<<<",
              ch, seq, len, COLNONE);

    if (ch == 2)
        SnacPrint (pak, out);
    else
        printf ("%s", s_ind (s_dump (pak->data + 6, pak->len - 6)));

    pak->rpos = opos;
}


int main (int argc, char **argv)
{
    strc_t line;
    
    ConvInit ();
    Packet *pak[2];
    FILE *f = stdin;
    int nooffset = 0;
    int out = 0;
    const char *l = NULL;
    
    pak[0] = PacketC ();
    pak[1] = PacketC ();

    if (argc > 2 || (argc > 1 && (!strcmp (argv[1], "--help") || !strcmp (argv[1], "-h"))))
    {
        printf ("Usage: %s\n", argv[0]);
        printf ("  Read wireshark saved packet dump and convert into readable dump.\n");
        printf ("  Dump is expected on stdin and written to stdout.\n");
        exit (1);
    }
    if (argc > 1)
    {
        f = fopen (argv[1], "r");
        assert (f);
    }
    
    while ((line = UtilIOReadline (f)))
    {
        l = line->txt;
        if (!nooffset && !out)
            out = 2 + (*l == ' ' ? 0 : 1);
        int i;
        UDWORD hex;
        if (!*l)
            continue;
        if (!strncmp (l, "Incoming", 8) || !strncmp (l, "Outgoing", 8))
        {
            if (PacketWritePos (pak[out&1]) > 0)
            {
                printf (out&1 ? "Outgoing partial v8 server packet:" : "Incoming partial v8 server packet:");
                pak[out&1]->len = PacketReadAtB2 (pak[out&1], 4) + 6;
                FlapPrint (pak[out&1], out&1);
                pak[out&1]->wpos = 0;
                pak[out&1]->rpos = 0;
                pak[out&1]->len = 0;
            }
            nooffset = 1;
            out = 2 + !strncmp (l, "Outgoing", 8);
            continue;
        }
        if (strstr (l, "bytes) received:") || strstr (l, "bytes) sent:"))
        {
            if (PacketWritePos (pak[out&1]) > 0)
            {
                printf (out&1 ? "Outgoing partial v8 server packet:" : "Incoming partial v8 server packet:");
                pak[out&1]->len = PacketReadAtB2 (pak[out&1], 4) + 6;
                FlapPrint (pak[out&1], out&1);
                pak[out&1]->wpos = 0;
                pak[out&1]->rpos = 0;
                pak[out&1]->len = 0;
            }
            out = 2 + (strstr (l, "bytes) sent:") ? 1 : 0);
            continue;
        }
        while (*l == ' ')
            l++;
        while (!nooffset && strchr ("0123456789abcdefABCDEF", *l))
            l++;
        if (*l == ':')
            l++;
        for (i = 0; i < 16; i++)
        {
            if (*l == ' ') l++;
            if (*l == ' ') l++;
            if (!*l || !strchr ("0123456789abcdefABCDEF", *l))
                break;
            if      (*l >= '0' && *l <= '9')  hex = *l - '0';
            else if (*l >= 'a' && *l <= 'f')  hex = *l - 'a' + 10;
            else if (*l >= 'A' && *l <= 'F')  hex = *l - 'A' + 10;
            else assert (0);
            hex *= 16;
            l++;
            if      (*l >= '0' && *l <= '9')  hex += *l - '0';
            else if (*l >= 'a' && *l <= 'f')  hex += *l - 'a' + 10;
            else if (*l >= 'A' && *l <= 'F')  hex += *l - 'A' + 10;
            else assert (0);
            l++;
            PacketWrite1 (pak[out&1], hex);
            if (PacketWritePos (pak[out&1]) >= 6)
            {
                UWORD l = PacketReadAtB2 (pak[out&1], 4);
                if (PacketWritePos (pak[out&1]) >= 6 + l)
                {
                    printf (out&1 ? "Outgoing v8 server packet:" : "Incoming v8 server packet:");
                    
                    FlapPrint (pak[out&1], out&1);
                    pak[out&1]->wpos = 0;
                    pak[out&1]->rpos = 0;
                    pak[out&1]->len = 0;
                }
            }
        }
    }
    return 0;
}
