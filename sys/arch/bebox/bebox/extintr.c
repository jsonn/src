/*	$NetBSD: extintr.c,v 1.1.2.1 1997/11/28 19:34:08 mellon Exp $	*/
/*      $OpenBSD: isabus.c,v 1.1 1997/10/11 11:53:00 pefo Exp $ */

/*-
 * Copyright (c) 1995 Per Fogelstrom
 * Copyright (c) 1993, 1994 Charles Hannum.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz and Don Ahn.
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
 *	@(#)isa.c	7.2 (Berkeley) 5/12/91
 */
#include <sys/param.h>
#include <sys/malloc.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <machine/intr.h>
#include <machine/psl.h>

unsigned int imen = 0xffffffff;
volatile int cpl, ipending, astpending, tickspending;
int imask[NIPL];
int intrtype[ICU_LEN], intrmask[ICU_LEN], intrlevel[ICU_LEN];
struct intrhand *intrhand[ICU_LEN];

void intr_calculatemasks();

int fakeintr(arg)
	void *arg;
{
	return 0;
}

extern vm_offset_t bebox_mb_reg;

#define	BEBOX_ISA_INTR		16

#define	BEBOX_SET_MASK		0x80000000
#define	BEBOX_INTR_MASK		0x0fffffdc

#define	BEBOX_INTR(x)		(0x80000000 >> x)
#define	BEBOX_INTR_SERIAL3	BEBOX_INTR(6)
#define	BEBOX_INTR_SERIAL4	BEBOX_INTR(7)
#define	BEBOX_INTR_MIDI1	BEBOX_INTR(8)
#define	BEBOX_INTR_MIDI2	BEBOX_INTR(9)
#define	BEBOX_INTR_SCSI		BEBOX_INTR(10)
#define	BEBOX_INTR_PCI1		BEBOX_INTR(11)
#define	BEBOX_INTR_PCI2		BEBOX_INTR(12)
#define	BEBOX_INTR_PCI3		BEBOX_INTR(13)
#define	BEBOX_INTR_SOUND	BEBOX_INTR(14)
#define	BEBOX_INTR_8259		BEBOX_INTR(26)
#define	BEBOX_INTR_IRDA		BEBOX_INTR(27)
#define	BEBOX_INTR_A2D		BEBOX_INTR(28)
#define	BEBOX_INTR_GEEKPORT	BEBOX_INTR(29)

static struct {
	int intr;
	int irq;
} irq_map[] = {
	{ BEBOX_INTR_SERIAL3, 	16 },
	{ BEBOX_INTR_SERIAL4, 	17 },
	{ BEBOX_INTR_MIDI1, 	18 },
	{ BEBOX_INTR_MIDI2, 	19 },
	{ BEBOX_INTR_SCSI, 	20 },
	{ BEBOX_INTR_PCI1, 	21 },
	{ BEBOX_INTR_PCI2, 	22 },
	{ BEBOX_INTR_PCI3, 	23 },
	{ BEBOX_INTR_SOUND, 	24 },
	{ BEBOX_INTR_8259, 	25 },
	{ BEBOX_INTR_IRDA, 	26 },
	{ BEBOX_INTR_A2D, 	27 },
	{ BEBOX_INTR_GEEKPORT, 	28 },
};
static int intr_map_len = sizeof (irq_map) / sizeof (int);

/*
 * external interrupt handler
 */
void
ext_intr()
{
	int i, irq;
	int o_imen, r_imen;
	int pcpl;
	struct intrhand *ih;
	volatile unsigned long cpu0_int_mask;
	volatile unsigned long int_state;
	extern long intrcnt[];

	pcpl = splhigh() ;	/* Turn off all */
	cpu0_int_mask = BEBOX_INTR_MASK &
		*(unsigned int *)(bebox_mb_reg + CPU0_INT_MASK);
	int_state = *(unsigned int *)(bebox_mb_reg + INT_STATE_REG);

	if (int_state & BEBOX_INTR_8259) {
		irq = isa_intr();
	} else if (int_state &= cpu0_int_mask) {
		for (i = 0; i < intr_map_len; i++)
			if (int_state & irq_map[i].intr) {
				irq = irq_map[i].irq;
				break;
			}
	} else {
		printf("ext_intr: unknown interrupt 0x%x\n", int_state);
		goto out;
	}

	o_imen = imen;
	r_imen = 1 << irq;
	imen |= r_imen;

	if (irq < BEBOX_ISA_INTR) {
		isa_intr_mask(imen);
	} else {
		; /* XXX BEBOX INTERRUPT MASK */
	}

	if((pcpl & r_imen) != 0) {
		ipending |= r_imen;	/* Masked! Mark this as pending */
	} else {
		ih = intrhand[irq];
		while (ih) {
			(*ih->ih_fun)(ih->ih_arg);
			ih = ih->ih_next;
		}
		imen = o_imen;

		if (irq < BEBOX_ISA_INTR) {
			isa_intr_clr(irq);
			isa_intr_mask(imen);
		} else {
			; /* XXX BEBOX INTERRUPT MASK */
		}

		intrcnt[irq]++;
	}
out:
	splx(pcpl);	/* Process pendings. */
}

#if 0
void
ext_intr_mask(irq, set)
	int irq;
	int set;
{
	int i, j;

	if (irq >= 16)
		for (i = 0; i < intr_map_len; i++)
			if (irq_map[i].irq == irq) {
				*(volatile unsigned int *)(bebox_mb_reg +
				    CPU0_INT_MASK)
					= (set ? BEBOX_SET_MASK : 0) |
						irq_map[i].intr;
				ext_default_mask |= irq_map[i].intr;
				break;
			}

	for (i = 0; i < NIPL; i++)
		for (j = 0; j < intr_map_len; j++)
			if (imask[i] & (1 << irq_map[j].irq))
				ext_imask[i] |= irq_map[j].intr;
}
#endif

/*
 * Register an interrupt handler.
 */
void *
intr_establish(irq, type, level, ih_fun, ih_arg)
	int irq;
	int type;
	int level;
	int (*ih_fun) __P((void *));
	void *ih_arg;
{
	struct intrhand **p, *q, *ih;
	static struct intrhand fakehand = {fakeintr};
	extern int cold;

	/* no point in sleeping unless someone can free memory. */
	ih = malloc(sizeof *ih, M_DEVBUF, cold ? M_NOWAIT : M_WAITOK);
	if (ih == NULL)
		panic("isa_intr_establish: can't malloc handler info");

	if (!LEGAL_IRQ(irq) || type == IST_NONE)
		panic("intr_establish: bogus irq or type");

	switch (intrtype[irq]) {
	case IST_EDGE:
	case IST_LEVEL:
		if (type == intrtype[irq])
			break;
	case IST_PULSE:
		if (type != IST_NONE)
			panic("intr_establish: can't share %s with %s",
			    isa_intr_typename(intrtype[irq]),
			    isa_intr_typename(type));
		break;
	}

	/*
	 * Figure out where to put the handler.
	 * This is O(N^2), but we want to preserve the order, and N is
	 * generally small.
	 */
	for (p = &intrhand[irq]; (q = *p) != NULL; p = &q->ih_next)
		;

	/*
	 * Actually install a fake handler momentarily, since we might be doing
	 * this with interrupts enabled and don't want the real routine called
	 * until masking is set up.
	 */
	fakehand.ih_level = level;
	*p = &fakehand;

	intr_calculatemasks();

	/*
	 * Poke the real handler in now.
	 */
	ih->ih_fun = ih_fun;
	ih->ih_arg = ih_arg;
	ih->ih_count = 0;
	ih->ih_next = NULL;
	ih->ih_level = level;
	ih->ih_irq = irq;
	*p = ih;

	return (ih);
}

/*
 * Deregister an interrupt handler.
 */
void
intr_disestablish(arg)
	void *arg;
{
	struct intrhand *ih = arg;
	int irq = ih->ih_irq;
	struct intrhand **p, *q;

	if (!LEGAL_IRQ(irq))
		panic("intr_disestablish: bogus irq");

	/*
	 * Remove the handler from the chain.
	 * This is O(n^2), too.
	 */
	for (p = &intrhand[irq]; (q = *p) != NULL && q != ih; p = &q->ih_next)
		;
	if (q)
		*p = q->ih_next;
	else
		panic("intr_disestablish: handler not registered");
	free((void *)ih, M_DEVBUF);

	intr_calculatemasks();

	if (intrhand[irq] == NULL)
		intrtype[irq] = IST_NONE;
}

/*
 * Recalculate the interrupt masks from scratch.
 * We could code special registry and deregistry versions of this function that
 * would be faster, but the code would be nastier, and we don't expect this to
 * happen very much anyway.
 */
void
intr_calculatemasks()
{
	int irq, level;
	struct intrhand *q;

	/* First, figure out which levels each IRQ uses. */
	for (irq = 0; irq < ICU_LEN; irq++) {
		register int levels = 0;
		for (q = intrhand[irq]; q; q = q->ih_next)
			levels |= 1 << q->ih_level;
		intrlevel[irq] = levels;
	}

	/* Then figure out which IRQs use each level. */
	for (level = 0; level < NIPL; level++) {
		register int irqs = 0;
		for (irq = 0; irq < ICU_LEN; irq++)
			if (intrlevel[irq] & (1 << level))
				irqs |= 1 << irq;
		imask[level] = irqs | SINT_MASK;
	}

	/*
	 * IPL_NONE is used for hardware interrupts that are never blocked,
	 * and do not block anything else.
	 */
	imask[IPL_NONE] = 0;

	/*
	 * Enforce a hierarchy that gives slow devices a better chance at not
	 * dropping data.
	 */
	imask[IPL_TTY] |= imask[IPL_NET] | imask[IPL_BIO];
	imask[IPL_NET] |= imask[IPL_BIO];

	/*
	 * There are tty, network and disk drivers that use free() at interrupt
	 * time, so imp > (tty | net | bio).
	 */
	imask[IPL_IMP] |= imask[IPL_TTY] | imask[IPL_NET] | imask[IPL_BIO];
	imask[IPL_AUDIO] |= imask[IPL_IMP];

	/*
	 * Since run queues may be manipulated by both the statclock and tty,
	 * network, and disk drivers, clock > imp.
	 */
	imask[IPL_CLOCK] |= imask[IPL_AUDIO];

	/*
	 * IPL_HIGH must block everything that can manipulate a run queue.
	 */
	imask[IPL_HIGH] = 0xffffffff;

	/* And eventually calculate the complete masks. */
	for (irq = 0; irq < ICU_LEN; irq++) {
		register int irqs = 1 << irq;
		for (q = intrhand[irq]; q; q = q->ih_next)
			irqs |= imask[q->ih_level];
		intrmask[irq] = irqs | SINT_MASK;
	}

	{
		register int irqs = 0;
		for (irq = 0; irq < ICU_LEN; irq++)
			if (intrhand[irq])
				irqs |= 1 << irq;
		if ((irqs & ((1 << BEBOX_ISA_INTR) - 1)) >= 0x100)
			irqs |= 1 << IRQ_SLAVE;
		imen = ~irqs;

		isa_intr_mask(imen);
					/* XXX BEBOX INTERRUPT MASK */
	}
}

void
do_pending_int()
{
	struct intrhand *ih;
	int irq;
	int pcpl;
	int hwpend;
	int emsr, dmsr;
	static int processing;
	extern long intrcnt[];

	if (processing)
		return;

	processing = 1;
	asm volatile("mfmsr %0" : "=r"(emsr));
	dmsr = emsr & ~PSL_EE;
	asm volatile("mtmsr %0" :: "r"(dmsr));

	pcpl = splhigh();		/* Turn off all */
	hwpend = ipending & ~pcpl;	/* Do now unmasked pendings */
	imen &= ~hwpend;
	while (hwpend) {
		irq = ffs(hwpend) - 1;
		hwpend &= ~(1L << irq);
		ih = intrhand[irq];
		while(ih) {
			(*ih->ih_fun)(ih->ih_arg);
			ih = ih->ih_next;
		}

		if (irq < BEBOX_ISA_INTR)
			isa_intr_clr(irq);

		intrcnt[irq]++;
	}
	if (irq < BEBOX_ISA_INTR) {
		isa_intr_mask(imen);
	} else {
		; /* XXX BEBOX INTERRUPT MASK */
	}

	if(ipending & SINT_CLOCK) {
		ipending &= ~SINT_CLOCK;
		softclock();
		intrcnt[CNT_SINT_CLOCK]++;
	}
	if(ipending & SINT_NET) {
		extern int netisr;
		int pisr = netisr;
		netisr = 0;
		ipending &= ~SINT_NET;
		softnet(pisr);
		intrcnt[CNT_SINT_NET]++;
	}
	ipending &= pcpl;
	cpl = pcpl;	/* Don't use splx... we are here already! */
	asm volatile("mtmsr %0" :: "r"(emsr));
	processing = 0;
}
