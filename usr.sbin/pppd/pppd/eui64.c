/*	$NetBSD: eui64.c,v 1.2.8.2 2000/09/30 06:21:42 simonb Exp $	*/

/*
    eui64.c - EUI64 routines for IPv6CP.
    Copyright (C) 1999  Tommi Komulainen <Tommi.Komulainen@iki.fi>

    Redistribution and use in source and binary forms are permitted
    provided that the above copyright notice and this paragraph are
    duplicated in all such forms and that any documentation,
    advertising materials, and other materials related to such
    distribution and use acknowledge that the software was developed
    by Tommi Komulainen.  The name of the author may not be used
    to endorse or promote products derived from this software without
    specific prior written permission.
    THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
    WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.


    Id: eui64.c,v 1.5 2000/08/05 06:46:47 paulus Exp 
*/

#include <sys/cdefs.h>
#ifndef lint
#if 0
#define RCSID	"Id: eui64.c,v 1.3 1999/08/25 04:15:51 paulus Exp "
#else
__RCSID("$NetBSD: eui64.c,v 1.2.8.2 2000/09/30 06:21:42 simonb Exp $");
#endif
#endif

#include "pppd.h"

#ifdef RCSID
static const char rcsid[] = RCSID;
#endif

#ifdef INET6
/*
 * eui64_ntoa - Make an ascii representation of an interface identifier
 */
char *
eui64_ntoa(e)
    eui64_t e;
{
    static char buf[32];

    snprintf(buf, 32, "%02x%02x:%02x%02x:%02x%02x:%02x%02x",
	     e.e8[0], e.e8[1], e.e8[2], e.e8[3], 
	     e.e8[4], e.e8[5], e.e8[6], e.e8[7]);
    return buf;
}
#endif
