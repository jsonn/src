/*	$NetBSD: cmdtab.c,v 1.7 1999/08/02 17:27:27 ad Exp $	*/

/*-
 * Copyright (c) 1980, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)cmdtab.c	8.1 (Berkeley) 6/6/93";
#endif
__RCSID("$NetBSD: cmdtab.c,v 1.7 1999/08/02 17:27:27 ad Exp $");
#endif /* not lint */

#include "systat.h"
#include "extern.h"

struct	cmdtab cmdtab[] = {
	{ "pigs",	showpigs,	fetchpigs,	labelpigs,
	  initpigs,	openpigs,	closepigs,	0,
	  CF_LOADAV },
	{ "swap",	showswap,	fetchswap,	labelswap,
	  initswap,	openswap,	closeswap,	0,
	  CF_LOADAV },
	{ "mbufs",	showmbufs,	fetchmbufs,	labelmbufs,
	  initmbufs,	openmbufs,	closembufs,	0,
	  CF_LOADAV },
	{ "iostat",	showiostat,	fetchiostat,	labeliostat,
	  initiostat,	openiostat,	closeiostat,	cmdiostat,
	  CF_LOADAV },
	{ "vmstat",	showkre,	fetchkre,	labelkre,
	  initkre,	openkre,	closekre,	cmdkre,
	  0 },
	{ "netstat",	shownetstat,	fetchnetstat,	labelnetstat,
	  initnetstat,	opennetstat,	closenetstat,	cmdnetstat,
	  CF_LOADAV },
	{ "ps",		showps,		fetchpigs,	labelps,
	  initpigs,	openpigs,	closepigs,	0,
	  CF_LOADAV },
	{ "inet.tcp",	showtcp,	fetchtcp,	labeltcp,
	  inittcp,	opentcp,	closetcp,	0,
	  CF_LOADAV },
	{ "inet.tcpsyn",showtcpsyn,	fetchtcp,	labeltcpsyn,
	  inittcp,	opentcp,	closetcp,	0,
	  CF_LOADAV },
	{ "inet.ip",	showip,		fetchip,	labelip,
	  initip,	openip,		closeip,	0,
	  CF_LOADAV },
	{ "inet.icmp",	showicmp,	fetchicmp,	labelicmp,
	  initicmp,	openicmp,	closeicmp,	0,
	  CF_LOADAV },
	{ 0 }
};
struct  cmdtab *curcmd = &cmdtab[0];
