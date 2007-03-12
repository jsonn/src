/*	$NetBSD: intr.c,v 1.52.4.1 2007/03/12 05:50:49 rmind Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT OT LIMITED TO, THE
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
 *	@(#)intr.c	8.3 (Berkeley) 11/11/93
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: intr.c,v 1.52.4.1 2007/03/12 05:50:49 rmind Exp $");

#include "opt_ddb.h"
#include "pcons.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <dev/cons.h>

#include <net/netisr.h>

#include <machine/cpu.h>
#include <machine/ctlreg.h>
#include <machine/instr.h>
#include <machine/trap.h>

/*
 * The following array is to used by locore.s to map interrupt packets
 * to the proper IPL to send ourselves a softint.  It should be filled
 * in as the devices are probed.  We should eventually change this to a
 * vector table and call these things directly.
 */
struct intrhand *intrlev[MAXINTNUM];

void	strayintr(const struct trapframe64 *, int);
int	softintr(void *);
int	softnet(void *);
int	intr_list_handler(void *);

/*
 * Stray interrupt handler.  Clear it if possible.
 * If not, and if we get 10 interrupts in 10 seconds, panic.
 */
int ignore_stray = 1;
int straycnt[16];

void
strayintr(const struct trapframe64 *fp, int vectored)
{
	static int straytime, nstray;
	int timesince;
	char buf[256];
#if 0
	extern int swallow_zsintrs;
#endif

	if (fp->tf_pil < 16)
		straycnt[(int)fp->tf_pil]++;

	if (ignore_stray)
		return;

	/* If we're in polled mode ignore spurious interrupts */
	if ((fp->tf_pil == PIL_SER) /* && swallow_zsintrs */) return;

	printf("stray interrupt ipl %u pc=%llx npc=%llx pstate=%s vecttored=%d\n",
	    fp->tf_pil, (unsigned long long)fp->tf_pc,
	    (unsigned long long)fp->tf_npc, 
	    bitmask_snprintf((fp->tf_tstate>>TSTATE_PSTATE_SHIFT),
	      PSTATE_BITS, buf, sizeof(buf)), vectored);

	timesince = time_second - straytime;
	if (timesince <= 10) {
		if (++nstray > 500)
			panic("crazy interrupts");
	} else {
		straytime = time_second;
		nstray = 1;
	}
#ifdef DDB
	Debugger();
#endif
}

/*
 * Level 1 software interrupt (could also be Sbus level 1 interrupt).
 * Three possible reasons:
 *	Network software interrupt
 *	Soft clock interrupt
 */
int
softintr(void *fp)
{
#if NPCONS >0
	extern void pcons_dopoll(void);

	pcons_dopoll();
#endif
	return (1);
}

int
softnet(void *fp)
{
	int n, s;
	
	s = splhigh();
	n = netisr;
	netisr = 0;
	splx(s);
	
#define DONETISR(bit, fn) do {		\
	if (n & (1 << bit))		\
		fn();			\
} while (0)
#include <net/netisr_dispatch.h>
#undef DONETISR
	return (1);
}

struct intrhand soft01intr = { .ih_fun = softintr, .ih_number = 1 };
struct intrhand soft01net = { .ih_fun = softnet, .ih_number = 1 };

#if 1
void 
setsoftint() {
	send_softint(-1, IPL_SOFTINT, &soft01intr);
}
void 
setsoftnet() {
	send_softint(-1, IPL_SOFTNET, &soft01net);
}
#endif

/*
 * Level 15 interrupts are special, and not vectored here.
 * Only `prewired' interrupts appear here; boot-time configured devices
 * are attached via intr_establish() below.
 */
struct intrhand *intrhand[16] = {
	NULL,			/*  0 = error */
	&soft01intr,		/*  1 = software level 1 + Sbus */
	NULL,	 		/*  2 = Sbus level 2 (4m: Sbus L1) */
	NULL,			/*  3 = SCSI + DMA + Sbus level 3 (4m: L2,lpt)*/
	NULL,			/*  4 = software level 4 (tty softint) (scsi) */
	NULL,			/*  5 = Ethernet + Sbus level 4 (4m: Sbus L3) */
	NULL,			/*  6 = software level 6 (not used) (4m: enet)*/
	NULL,			/*  7 = video + Sbus level 5 */
	NULL,			/*  8 = Sbus level 6 */
	NULL,			/*  9 = Sbus level 7 */
	NULL,			/* 10 = counter 0 = clock */
	NULL,			/* 11 = floppy */
	NULL,			/* 12 = zs hardware interrupt */
	NULL,			/* 13 = audio chip */
	NULL,			/* 14 = counter 1 = profiling timer */
	NULL			/* 15 = async faults */
};

/*
 * PCI devices can share interrupts so we need to have
 * a handler to hand out interrupts.
 */
int
intr_list_handler(void *arg)
{
	int claimed = 0;
	struct intrhand *ih = (struct intrhand *)arg;

	if (!arg) panic("intr_list_handler: no handlers!");
	while (ih && !claimed) {
		claimed = (*ih->ih_fun)(ih->ih_arg);
#ifdef DEBUG
		{
			extern int intrdebug;
			if (intrdebug & 1)
				printf("intr %p %x arg %p %s\n",
					ih, ih->ih_number, ih->ih_arg,
					claimed ? "claimed" : "");
		}
#endif
		ih = ih->ih_next;
	}
	return (claimed);
}


/*
 * Attach an interrupt handler to the vector chain for the given level.
 * This is not possible if it has been taken away as a fast vector.
 */
void
intr_establish(int level, struct intrhand *ih)
{
	register struct intrhand **p, *q = NULL;
	int s;

	s = splhigh();
	/*
	 * This is O(N^2) for long chains, but chains are never long
	 * and we do want to preserve order.
	 */
	ih->ih_pil = level; /* XXXX caller should have done this before */
	ih->ih_pending = 0; /* XXXX caller should have done this before */
	ih->ih_next = NULL;

	/*
	 * Store in fast lookup table
	 */
#ifdef NOT_DEBUG
	if (!ih->ih_number) {
		printf("\nintr_establish: NULL vector fun %p arg %p pil %p\n",
			  ih->ih_fun, ih->ih_arg, ih->ih_number, ih->ih_pil);
		Debugger();
	}
#endif
	if (ih->ih_number < MAXINTNUM && ih->ih_number >= 0) {
		if ((q = intrlev[ih->ih_number])) {
			struct intrhand *nih;
			/*
			 * Interrupt is already there.  We need to create a
			 * new interrupt handler and interpose it.
			 */
#ifdef DEBUG
			printf("intr_establish: intr reused %x\n", 
				ih->ih_number);
#endif
			if (q->ih_fun != intr_list_handler) {
				nih = (struct intrhand *)
					malloc(sizeof(struct intrhand),
						M_DEVBUF, M_NOWAIT);
				/* Point the old IH at the new handler */
				*nih = *q;
				nih->ih_next = NULL;
				q->ih_arg = (void *)nih;
				q->ih_fun = intr_list_handler;
			}
			/* Add the ih to the head of the list */
			ih->ih_next = (struct intrhand *)q->ih_arg;
			q->ih_arg = (void *)ih;
		} else {
			intrlev[ih->ih_number] = ih;
		}
#ifdef NOT_DEBUG
		printf("\nintr_establish: vector %x pil %x mapintr %p "
			"clrintr %p fun %p arg %p\n",
			ih->ih_number, ih->ih_pil, (void *)ih->ih_map,
			(void *)ih->ih_clr, (void *)ih->ih_fun,
			(void *)ih->ih_arg);
		/*Debugger();*/
#endif
	} else
		panic("intr_establish: bad intr number %x", ih->ih_number);

	/* If it's not shared, stick it in the intrhand list for that level. */
	if (q == NULL) {
		for (p = &intrhand[level]; (q = *p) != NULL; p = &q->ih_next)
			;
		*p = ih;
	}

	splx(s);
}

void *
softintr_establish(int level, void (*fun)(void *), void *arg)
{
	struct intrhand *ih;

	ih = malloc(sizeof(*ih), M_DEVBUF, 0);
	memset(ih, 0, sizeof(*ih));
	ih->ih_fun = (int (*)(void *))fun;	/* XXX */
	ih->ih_arg = arg;
	ih->ih_pil = level;
	ih->ih_pending = 0;
	ih->ih_clr = NULL;
	return (void *)ih;
}

void
softintr_disestablish(void *cookie)
{
	free(cookie, M_DEVBUF);
}

void
softintr_schedule(void *cookie)
{
	struct intrhand *ih = (struct intrhand *)cookie;

	send_softint(-1, ih->ih_pil, ih);
}
