/*	$NetBSD: autoconf.c,v 1.11.2.1 2000/06/22 17:04:55 minoura Exp $ */
/*
 * Copyright (c) 1994, 1998 Ludd, University of Lule}, Sweden.
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
 *     This product includes software developed at Ludd, University of Lule}.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

 /* All bugs are subject to removal without further notice */
		


#include <sys/param.h>

#include <lib/libsa/stand.h>

#include "../include/mtpr.h"
#include "../include/sid.h"
#include "../include/intr.h"
#include "../include/rpb.h"
#include "../include/scb.h"

#include "vaxstand.h"

void autoconf(void);
void findcpu(void);
void consinit(void);
void scbinit(void);
int getsecs(void);
void scb_stray(void *);
void longjmp(int *);
void rtimer(void *);

/*
 * Autoconf routine is really stupid; but it actually don't
 * need any intelligence. We just assume that all possible
 * devices exists on each cpu. Fast & easy.
 */

void
autoconf()
{

	findcpu(); /* Configures CPU variables */
	consinit(); /* Allow us to print out things */
	scbinit(); /* Fix interval clock etc */

	switch (vax_boardtype) {

	case VAX_BTYP_46:
	case VAX_BTYP_48:
		{int *map, i;

		/* Map all 16MB of I/O space to low 16MB of memory */
		map = (int *)0x700000; /* XXX */
		*(int *)0x20080008 = (int)map; /* XXX */
		for (i = 0; i < 0x8000; i++)
			map[i] = 0x80000000 | i;
		}break;

		break;
	}
}

/*
 * Clock handling routines, needed to do timing in standalone programs.
 */

volatile int tickcnt;

int
getsecs()
{
	return tickcnt/100;
}

struct ivec_dsp **scb;
struct ivec_dsp *scb_vec;
extern struct ivec_dsp idsptch;

/*
 * Init the SCB and set up a handler for all vectors in the lower space,
 * to detect unwanted interrupts.
 */
void
scbinit()
{
	int i;

	/*
	 * Allocate space. We need one page for the SCB, and 128*20 == 2.5k
	 * for the vectors. The SCB must be on a page boundary.
	 */
	i = (int)alloc(VAX_NBPG + 128*sizeof(scb_vec[0])) + VAX_PGOFSET;
	i &= ~VAX_PGOFSET;

	mtpr(i, PR_SCBB);
	scb = (void *)i;
	scb_vec = (struct ivec_dsp *)(i + VAX_NBPG);

	for (i = 0; i < 128; i++) {
		scb[i] = &scb_vec[i];
		(int)scb[i] |= SCB_ISTACK;	/* Only interrupt stack */
		scb_vec[i] = idsptch;
		scb_vec[i].hoppaddr = scb_stray;
		scb_vec[i].pushlarg = (void *) (i * 4);
		scb_vec[i].ev = NULL;
	}
	scb_vec[0xc0/4].hoppaddr = rtimer;

	mtpr(-10000, PR_NICR);		/* Load in count register */
	mtpr(0x800000d1, PR_ICCS);	/* Start clock and enable interrupt */

	mtpr(20, PR_IPL);
}

extern int jbuf[10];
extern int sluttid, senast, skip;

void
rtimer(void *arg)
{
	mtpr(IPL_HIGH, PR_IPL);
	tickcnt++;
	mtpr(0xc1, PR_ICCS);
	if (skip)
		return;
	if ((vax_boardtype == VAX_BTYP_46) ||
	    (vax_boardtype == VAX_BTYP_48) ||
	    (vax_boardtype == VAX_BTYP_49)) {
		int nu = sluttid - getsecs();
		if (senast != nu) {
			mtpr(20, PR_IPL);
			longjmp(jbuf);
		}
	}
}

asm("
	.align	2
	.globl  _idsptch, _eidsptch
_idsptch:
	pushr   $0x3f
	.word	0x9f16
	.long   _cmn_idsptch
	.long	0
	.long	0
	.long	0
_eidsptch:

_cmn_idsptch:
	movl	(sp)+,r0
	pushl	4(r0)
	calls	$1,*(r0)
	popr	$0x3f
	rei
");

/*
 * Stray interrupt handler.
 * This function must _not_ save any registers (in the reg save mask).
 */
void
scb_stray(void *arg)
{
	static int vector, ipl;

	ipl = mfpr(PR_IPL);
	vector = (int) arg;
	printf("stray interrupt: vector 0x%x, ipl %d\n", vector, ipl);
}
