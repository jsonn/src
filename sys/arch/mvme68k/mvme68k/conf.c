/*	$NetBSD: conf.c,v 1.22.8.5 2002/09/17 21:16:05 nathanw Exp $	*/

/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
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
 *
 *      @(#)conf.c	7.9 (Berkeley) 5/28/91
 */

#include <sys/param.h>
#include <sys/conf.h>

/*
 * Returns true if dev is /dev/mem or /dev/kmem.
 */
int
iskmemdev(dev)
	dev_t dev;
{

	return (major(dev) == mem_no && minor(dev) < 2);
}

/*
 * This entire table could be autoconfig()ed but that would mean that
 * the kernel's idea of the console would be out of sync with that of
 * the standalone boot.  I think it best that they both use the same
 * known algorithm unless we see a pressing need otherwise.
 */
#include <dev/cons.h>

#define zsc_pcccnpollc		nullcnpollc
#include "zsc_pcc.h"
cons_decl(zsc_pcc);

#define zsc_pcctwocnpollc	nullcnpollc
#define zsc_pcctwocngetc	zsc_pcccngetc
#define zsc_pcctwocnputc	zsc_pcccnputc
#include "zsc_pcctwo.h"
cons_decl(zsc_pcctwo);

#define clmpcccnpollc		nullcnpollc
#include "clmpcc_pcctwo.h"
cons_decl(clmpcc);


struct	consdev constab[] = {
#if NZSC_PCC > 0
	cons_init(zsc_pcc),
#endif
#if NZSC_PCCTWO > 0
	cons_init(zsc_pcctwo),
#endif
#if NCLMPCC_PCCTWO > 0
	cons_init(clmpcc),
#endif
	{ 0 },
};
