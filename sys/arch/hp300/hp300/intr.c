/*	$NetBSD: intr.c,v 1.31.2.1 2007/12/08 18:16:59 mjf Exp $	*/

/*-
 * Copyright (c) 1996, 1997, 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass, Gordon W. Ross, and Jason R. Thorpe.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Link and dispatch interrupts.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: intr.c,v 1.31.2.1 2007/12/08 18:16:59 mjf Exp $");

#define _HP300_INTR_H_PRIVATE

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/vmmeter.h>
#include <sys/cpu.h>
#include <sys/intr.h>

#include <uvm/uvm_extern.h>

/*
 * The location and size of the autovectored interrupt portion
 * of the vector table.
 */
#define ISRLOC		0x18
#define NISR		8

struct hp300_intr hp300_intr_list[NISR];
static const char *hp300_intr_names[NISR] = {
	"spurious",
	"lev1",
	"lev2",
	"lev3",
	"lev4",
	"lev5",
	"clock",
	"nmi",
};

u_short hp300_ipl2psl[NIPL];
volatile uint8_t ssir;
int idepth;

void	intr_computeipl(void);
void	netintr(void);

void
intr_init(void)
{
	struct hp300_intr *hi;
	int i;

	/* Initialize the ISR lists. */
	for (i = 0; i < NISR; ++i) {
		hi = &hp300_intr_list[i];
		LIST_INIT(&hi->hi_q);
		evcnt_attach_dynamic(&hi->hi_evcnt, EVCNT_TYPE_INTR,
		    NULL, hp300_intr_names[i], "intr");
	}

	/* Default interrupt priorities. */
	hp300_ipl2psl[IPL_NONE]       = 0;
	hp300_ipl2psl[IPL_SOFTCLOCK]  = PSL_S|PSL_IPL1;
	hp300_ipl2psl[IPL_SOFTNET]    = PSL_S|PSL_IPL1;
	hp300_ipl2psl[IPL_SOFTSERIAL] = PSL_S|PSL_IPL1;
	hp300_ipl2psl[IPL_SOFTBIO]    = PSL_S|PSL_IPL1;
	hp300_ipl2psl[IPL_VM]         = PSL_S|PSL_IPL3;
	hp300_ipl2psl[IPL_SCHED]      = PSL_S|PSL_IPL6;
	hp300_ipl2psl[IPL_HIGH]       = PSL_S|PSL_IPL7;
}

/*
 * Scan all of the ISRs, recomputing the interrupt levels for the spl*()
 * calls.  This doesn't have to be fast.
 */
void
intr_computeipl(void)
{
	struct hp300_intrhand *ih;
	int ipl;

	/* Start with low values. */
	hp300_ipl2psl[IPL_VM] = PSL_S|PSL_IPL3;

	for (ipl = 0; ipl < NISR; ipl++) {
		for (ih = LIST_FIRST(&hp300_intr_list[ipl].hi_q); ih != NULL;
		    ih = LIST_NEXT(ih, ih_q)) {
			/*
			 * Bump up the level for a given priority,
			 * if necessary.
			 */
			switch (ih->ih_priority) {
			case IPL_VM:
				if (ipl > PSLTOIPL(hp300_ipl2psl[IPL_VM]))
					hp300_ipl2psl[IPL_VM] = IPLTOPSL(ipl);
				break;
			default:
				printf("priority = %d\n", ih->ih_priority);
				panic("intr_computeipl: bad priority");
			}
		}
	}
}

void
intr_printlevels(void)
{

	printf("interrupt levels: vm = %d\n",
	    PSLTOIPL(hp300_ipl2psl[IPL_VM]));
}

/*
 * Establish an interrupt handler.
 * Called by driver attach functions.
 */
void *
intr_establish(int (*func)(void *), void *arg, int ipl, int priority)
{
	struct hp300_intrhand *newih, *curih;

	if ((ipl < 0) || (ipl >= NISR))
		panic("intr_establish: bad ipl %d", ipl);

	newih = malloc(sizeof(struct hp300_intrhand), M_DEVBUF, M_NOWAIT);
	if (newih == NULL)
		panic("intr_establish: can't allocate space for handler");

	/* Fill in the new entry. */
	newih->ih_fn = func;
	newih->ih_arg = arg;
	newih->ih_ipl = ipl;
	newih->ih_priority = priority;

	/*
	 * Some devices are particularly sensitive to interrupt
	 * handling latency.  The DCA, for example, can lose many
	 * characters if its interrupt isn't handled with reasonable
	 * speed.  For this reason, we sort ISRs by IPL_* priority,
	 * inserting higher priority interrupts before lower priority
	 * interrupts.
	 */

	/*
	 * Get the appropriate ISR list.  If the list is empty, no
	 * additional work is necessary; we simply insert ourselves
	 * at the head of the list.
	 */

	if (LIST_FIRST(&hp300_intr_list[ipl].hi_q) == NULL) {
		LIST_INSERT_HEAD(&hp300_intr_list[ipl].hi_q, newih, ih_q);
		goto compute;
	}

	/*
	 * A little extra work is required.  We traverse the list
	 * and place ourselves after any ISRs with our current (or
	 * higher) priority.
	 */

	for (curih = LIST_FIRST(&hp300_intr_list[ipl].hi_q);
	    LIST_NEXT(curih,ih_q) != NULL;
	    curih = LIST_NEXT(curih,ih_q)) {
		if (newih->ih_priority > curih->ih_priority) {
			LIST_INSERT_BEFORE(curih, newih, ih_q);
			goto compute;
		}
	}

	/*
	 * We're the least important entry, it seems.  We just go
	 * on the end.
	 */
	LIST_INSERT_AFTER(curih, newih, ih_q);

 compute:
	/* Compute new interrupt levels. */
	intr_computeipl();
	return newih;
}

/*
 * Disestablish an interrupt handler.
 */
void
intr_disestablish(void *arg)
{
	struct hp300_intrhand *ih = arg;

	LIST_REMOVE(ih, ih_q);
	free(ih, M_DEVBUF);
	intr_computeipl();
}

/*
 * This is the dispatcher called by the low-level
 * assembly language interrupt routine.
 */
void
intr_dispatch(int evec /* format | vector offset */)
{
	struct hp300_intrhand *ih;
	struct hp300_intr *list;
	int handled, ipl, vec;
	static int straycount, unexpected;

	idepth++;

	vec = (evec & 0xfff) >> 2;
#ifdef DIAGNOSTIC
	if ((vec < ISRLOC) || (vec >= (ISRLOC + NISR)))
		panic("intr_dispatch: bad vec 0x%x", vec);
#endif
	ipl = vec - ISRLOC;

	hp300_intr_list[ipl].hi_evcnt.ev_count++;
	uvmexp.intrs++;

	list = &hp300_intr_list[ipl];
	if (LIST_FIRST(&list->hi_q) == NULL) {
		printf("intr_dispatch: ipl %d unexpected\n", ipl);
		if (++unexpected > 10)
			panic("intr_dispatch: too many unexpected interrupts");
		return;
	}

	handled = 0;
	/* Give all the handlers a chance. */
	for (ih = LIST_FIRST(&list->hi_q) ; ih != NULL;
	    ih = LIST_NEXT(ih, ih_q))
		handled |= (*ih->ih_fn)(ih->ih_arg);

	if (handled)
		straycount = 0;
	else if (++straycount > 50)
		panic("intr_dispatch: too many stray interrupts");
	else
		printf("intr_dispatch: stray level %d interrupt\n", ipl);

	idepth--;
}

bool
cpu_intr_p(void)
{

	return idepth != 0;
}
