/*	$NetBSD: shb.c,v 1.6.2.1 2002/02/11 20:08:18 jdolecek Exp $	*/

/*-
 * Copyright (c) 1993, 1994 Charles Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <sh3/cpufunc.h>
#include <sh3/intcreg.h>
#include <sh3/trapreg.h>
#include <machine/shbvar.h>

#include <net/netisr.h>
#include <machine/debug.h>

int shbmatch __P((struct device *, struct cfdata *, void *));
void shbattach __P((struct device *, struct device *, void *));
int shbprint __P((void *, const char *));
void intr_calculatemasks __P((void));
int fakeintr __P((void *));
void	*shb_intr_establish __P((int irq, int type,
	    int level, int (*ih_fun)(void *), void *ih_arg));
int intrhandler __P((int, int, int, int, struct trapframe));
int check_ipending __P((int, int, int, int, struct trapframe));
void mask_irq __P((int));
void unmask_irq __P((int));
void Xsoftserial __P((void));
void Xsoftnet __P((void));
void Xsoftclock __P((void));
void init_soft_intr_handler __P((void));

struct cfattach shb_ca = {
	sizeof(struct shb_softc), shbmatch, shbattach
};

int	shbsearch __P((struct device *, struct cfdata *, void *));

int
shbmatch(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct mainbus_attach_args *ma = aux;

	if (strcmp(ma->ma_name, cf->cf_driver->cd_name))
		return (0);

        return (1);
}

void
shbattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct shb_softc *sc = (struct shb_softc *)self;

	printf("\n");

	sc->sc_iot = 0;
	sc->sc_memt = 0;

	TAILQ_INIT(&sc->sc_subdevs);
	config_search(shbsearch, self, NULL);

	init_soft_intr_handler();
}

int
shbprint(aux, isa)
	void *aux;
	const char *isa;
{
	struct shb_attach_args *ia = aux;

	if (ia->ia_iosize)
		printf(" port 0x%x", ia->ia_iobase);
	if (ia->ia_iosize > 1)
		printf("-0x%x", ia->ia_iobase + ia->ia_iosize - 1);
	if (ia->ia_msize)
		printf(" iomem 0x%x", ia->ia_maddr);
	if (ia->ia_msize > 1)
		printf("-0x%x", ia->ia_maddr + ia->ia_msize - 1);
	if (ia->ia_irq != IRQUNK)
		printf(" irq %d", ia->ia_irq);
	if (ia->ia_drq != DRQUNK)
		printf(" drq %d", ia->ia_drq);
	if (ia->ia_drq2 != DRQUNK)
		printf(" drq2 %d", ia->ia_drq2);
	return (UNCONF);
}

int
shbsearch(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct shb_softc *sc = (struct shb_softc *)parent;
	struct shb_attach_args ia;
	int tryagain;

	do {
		ia.ia_iot = sc->sc_iot;
		ia.ia_memt = sc->sc_memt;
		/* ia.ia_dmat = sc->sc_dmat; */
		/* ia.ia_ic = sc->sc_ic; */
		ia.ia_iobase = cf->cf_iobase;
		ia.ia_iosize = 0x666; /* cf->cf_iosize; */
		ia.ia_maddr = cf->cf_maddr;
		ia.ia_msize = cf->cf_msize;
		ia.ia_irq = cf->cf_irq == 2 ? 9 : cf->cf_irq;
		ia.ia_drq = cf->cf_drq;
		ia.ia_drq2 = cf->cf_drq2;
		/* ia.ia_delaybah = sc->sc_delaybah; */

		tryagain = 0;
		if ((*cf->cf_attach->ca_match)(parent, cf, &ia) > 0) {
			config_attach(parent, cf, &ia, shbprint);
			tryagain = (cf->cf_fstate == FSTATE_STAR);
		}
	} while (tryagain);

	return (0);
}

char *
shb_intr_typename(type)
	int type;
{

	switch (type) {
        case IST_NONE :
		return ("none");
        case IST_PULSE:
		return ("pulsed");
        case IST_EDGE:
		return ("edge-triggered");
        case IST_LEVEL:
		return ("level-triggered");
	default:
		panic("shb_intr_typename: invalid type %d", type);
	}
}
int intrtype[ICU_LEN], intrmask[ICU_LEN], intrlevel[ICU_LEN];
struct intrhand *intrhand[ICU_LEN];

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
		int levels = 0;
		for (q = intrhand[irq]; q; q = q->ih_next)
			levels |= 1 << q->ih_level;
		intrlevel[irq] = levels;
	}

	/* Then figure out which IRQs use each level. */
	for (level = 0; level < NIPL; level++) {
		int irqs = 0;
		for (irq = 0; irq < ICU_LEN; irq++)
			if (intrlevel[irq] & (1 << level))
				irqs |= 1 << irq;
		imask[level] = irqs;
	}

	/*
	 * Initialize soft interrupt masks to block themselves.
	 */
	imask[IPL_SOFTCLOCK] |= 1 << SIR_CLOCK;
	imask[IPL_SOFTNET] |= 1 << SIR_NET;
	imask[IPL_SOFTSERIAL] |= 1 << SIR_SERIAL;

	/*
	 * IPL_NONE is used for hardware interrupts that are never blocked,
	 * and do not block anything else.
	 */
	imask[IPL_NONE] = 0;

	/*
	 * Enforce a hierarchy that gives slow devices a better chance at not
	 * dropping data.
	 */
	imask[IPL_SOFTCLOCK] |= imask[IPL_NONE];
	imask[IPL_SOFTNET] |= imask[IPL_SOFTCLOCK];
	imask[IPL_BIO] |= imask[IPL_SOFTNET];
	imask[IPL_NET] |= imask[IPL_BIO];
	imask[IPL_SOFTSERIAL] |= imask[IPL_NET];
	imask[IPL_TTY] |= imask[IPL_SOFTSERIAL];

	/*
	 * There are tty, network and disk drivers that use free() at interrupt
	 * time, so imp > (tty | net | bio).
	 */
	imask[IPL_IMP] |= imask[IPL_TTY];

	imask[IPL_AUDIO] |= imask[IPL_IMP];

	/*
	 * Since run queues may be manipulated by both the statclock and tty,
	 * network, and disk drivers, clock > imp.
	 */
	imask[IPL_CLOCK] |= imask[IPL_AUDIO];

	/*
	 * IPL_HIGH must block everything that can manipulate a run queue.
	 */
	imask[IPL_HIGH] |= imask[IPL_CLOCK];

	/*
	 * We need serial drivers to run at the absolute highest priority to
	 * avoid overruns, so serial > high.
	 */
	imask[IPL_SERIAL] |= imask[IPL_HIGH];

	/* And eventually calculate the complete masks. */
	for (irq = 0; irq < ICU_LEN; irq++) {
		int irqs = 1 << irq;
		for (q = intrhand[irq]; q; q = q->ih_next)
			irqs |= imask[q->ih_level];
		intrmask[irq] = irqs;
	}

#ifdef	TODO
	/* Lastly, determine which IRQs are actually in use. */
	{
		int irqs = 0;
		for (irq = 0; irq < ICU_LEN; irq++)
			if (intrhand[irq])
				irqs |= 1 << irq;
		if (irqs >= 0x100) /* any IRQs >= 8 in use */
			irqs |= 1 << IRQ_SLAVE;
		imen = ~irqs;
		SET_ICUS();
	}
#endif

}

/*
 * Set up an interrupt handler to start being called.
 * XXX PRONE TO RACE CONDITIONS, UGLY, 'INTERESTING' INSERTION ALGORITHM.
 */
void *
shb_intr_establish(irq, type, level, ih_fun, ih_arg)
	int irq;
	int type;
	int level;
	int (*ih_fun) __P((void *));
	void *ih_arg;
{
	struct intrhand **p, *q, *ih;
	static struct intrhand fakehand = {fakeintr};

	/* no point in sleeping unless someone can free memory. */
	ih = malloc(sizeof *ih, M_DEVBUF, cold ? M_NOWAIT : M_WAITOK);
	if (ih == NULL)
		panic("shb_intr_establish: can't malloc handler info");

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

	/* unmask H/W interrupt mask register */
	if (irq < SHB_MAX_HARDINTR)
		unmask_irq(irq);

	return (ih);
}

int
fakeintr(arg)
	void *arg;
{

	return 0;
}


#define	IRQ_BIT(irq_num)	(1 << (irq_num))

/*ARGSUSED*/
int	/* 1 = check ipending on return, 0 = fast intr return */
intrhandler(p1, p2, p3, p4, frame)
	int p1, p2, p3, p4; /* dummy param */
	struct trapframe frame;
{
	unsigned int irl;
	struct intrhand *ih;
	unsigned int irq_num;
	int ocpl;

#if 0
	u_int32_t intevt = *(volatile u_int32_t *)0xffffffd8;
	u_int32_t intevt2 = *(volatile u_int32_t *)0xa4000000;
	if (intevt < 0xf00 && intevt != 0x420)
		printf("INTEVT=%08x INTEVT2=%08x\n", intevt, intevt2);
#endif
	__dbg_heart_beat(HEART_BEAT_WHITE);

#if 0
	printf("intr_handler:int_no %x spc %x ssr %x r15 %x curproc %x\n",
	       frame.tf_trapno, frame.tf_spc, frame.tf_ssr, frame.tf_r15,
	       (int)curproc);
#endif

	irl = (unsigned int)frame.tf_trapno;

	if (irl >= INTEVT_SOFT) {
		/* This is software interrupt */
		irq_num = (irl - INTEVT_SOFT);
	} else if (irl == INTEVT_TMU1) {
		irq_num = TMU1_IRQ;
	} else if (IS_INTEVT_SCI0(irl)) {	/* XXX TOO DIRTY */
		irq_num = SCI_IRQ;
#ifdef SH4
	} else if ((irl & 0x0f00) == INTEVT_SCIF) {
		irq_num = SCIF_IRQ;
#endif
	} else {
		irq_num = (irl - 0x200) >> 5;		
	}

	mask_irq(irq_num);

	if (cpl & IRQ_BIT(irq_num)) {
		ipending |= IRQ_BIT(irq_num);
		return 0;
	}

	ocpl = cpl;
	cpl |= intrmask[irq_num];
	ih = intrhand[irq_num];
	if (ih == NULL) {

		/* this is stray interrupt */
		cpl = ocpl;

#if 0	/* This is commented by T.Horiuchi */
		unmask_irq(irq_num);
#endif
		return 1;
	}

	enable_ext_intr();
	while (ih) {
		if (ih->ih_arg)
			(*ih->ih_fun)(ih->ih_arg);
		else
			(*ih->ih_fun)(&frame);
		ih = ih->ih_next;
	}
	disable_ext_intr();

	cpl = ocpl;

	unmask_irq(irq_num);

#if 0
	printf("intr_handler:end\n");
#endif
	return 1;
}

int	/* 1 = resume ihandler on return, 0 = go to fast intr return */
check_ipending(p1, p2, p3, p4, frame)
	int p1, p2, p3, p4; /* dummy param */
	struct trapframe frame;
{
	int ir;
	int i;
	int mask;
#define MASK_LEN 32

  restart:
	ir = (~cpl) & ipending;
	if (ir == 0)
		return 0;

#if 0
	mask = 1;
	for (i = 0; i < MASK_LEN; i++, mask <<= 1) {
		if (ir & mask)
			break;
	}
#else
	mask = 1 << IRQ_LOW;
	for (i = IRQ_LOW; i <= IRQ_HIGH; i++, mask <<= 1) {
		if (ir & mask)
			break;
	}
	if (IRQ_HIGH < i) {
		mask = 1 << SIR_LOW;
		for (i = SIR_LOW; i <= SIR_HIGH; i++, mask <<= 1) {
			if (ir & mask)
				break;
		}
	}
#endif

	if ((mask & ipending) == 0)
		goto restart;

	ipending &= ~mask;

	if (i < SHB_MAX_HARDINTR) {
		/* set interrupt event register, this value is referenced in ihandler */
		SHREG_INTEVT = (i << 5) + 0x200;
	} else {
		/* This is software interrupt */
		SHREG_INTEVT = INTEVT_SOFT+i;
	}

	return 1;
}

#ifdef SH7709A_BROKEN_IPR	/* broken IPR patch */

#define IPRA	0
#define IPRB	1
#define IPRC	2
#define IPRD	3
#define IPRE	4

static unsigned short ipr[ 5 ];

#endif /* SH7709A_BROKEN_IPR */

void
mask_irq(irq)
	int irq;
{
	switch (irq) {
	case TMU1_IRQ:
#ifdef SH7709A_BROKEN_IPR
		ipr[IPRA] &= ~((15)<<8);
		SHREG_IPRA = ipr[IPRA];
#else
		SHREG_IPRA &= ~((15)<<8);
#endif
		break;
	case SCI_IRQ:
#ifdef SH7709A_BROKEN_IPR
		ipr[IPRB] &= ~((15)<<4);
		SHREG_IPRB = ipr[IPRB];
#else
		SHREG_IPRB &= ~((15)<<4);
#endif
		break;
#if defined(SH7709) || defined(SH7709A) || defined(SH7729)
	case SCIF_IRQ:
#ifdef SH7709A_BROKEN_IPR
		ipr[IPRE] &= ~((15)<<4);
		SHREG_IPRE = ipr[IPRE];
#else
		SHREG_IPRE &= ~((15)<<4);
#endif
		break;
#endif
	case IRQ4_IRQ:
#ifdef SH7709A_BROKEN_IPR
		ipr[IPRD] &= ~15;
		SHREG_IPRD = ipr[IPRD];
#else
		SHREG_IPRD &= ~0xf;
#endif
		break;
	default:
		if (irq < SHB_MAX_HARDINTR)
			printf("masked unknown irq(%d)!\n", irq);
	}
}

void
unmask_irq(irq)
	int irq;
{

	switch (irq) {
	case TMU1_IRQ:
#ifdef SH7709A_BROKEN_IPR
		ipr[ IPRA ] |= ((15 - irq)<<8);
		SHREG_IPRA = ipr[ IPRA ];
#else
		SHREG_IPRA |= ((15 - irq)<<8);
#endif
		break;
	case SCI_IRQ:
#ifdef SH7709A_BROKEN_IPR
		ipr[IPRB] |= ((15 - irq)<<4);
		SHREG_IPRB = ipr[IPRB];
#else
		SHREG_IPRB |= ((15 - irq)<<4);
#endif
		break;
#if defined(SH7709) || defined(SH7709A) || defined(SH7729)
	case SCIF_IRQ:
#ifdef SH7709A_BROKEN_IPR
		ipr[ IPRE ] |= ((15 - irq)<<4);
		SHREG_IPRE = ipr[ IPRE ];
#else
		SHREG_IPRE |= ((15 - irq)<<4);
#endif
		break;
#endif
	case IRQ4_IRQ:
#ifdef SH7709A_BROKEN_IPR
		ipr[IPRD] |= (15 - irq);
		SHREG_IPRD = ipr[IPRD];
#else
		SHREG_IPRD |= (15 - irq);
#endif
		break;
	default:
		if (irq < SHB_MAX_HARDINTR)
			printf("unmasked unknown irq(%d)!\n", irq);
	}
}

void
init_soft_intr_handler(void)
{
#include	"sci.h"
#include	"scif.h"
#include	"com.h"
#if (NSCI > 0) || (NSCIF > 0) || (NCOM > 0)
	shb_intr_establish(SIR_SERIAL, IST_LEVEL, IPL_SOFTSERIAL,
		      	(int (*) (void *))Xsoftserial, NULL);
#endif

	shb_intr_establish(SIR_NET, IST_LEVEL, IPL_SOFTNET,
		      (int (*) (void *))Xsoftnet, NULL);

	shb_intr_establish(SIR_CLOCK, IST_LEVEL, IPL_SOFTCLOCK,
		      (int (*) (void *))Xsoftclock, NULL);
}

#if (NSCI > 0) || (NSCIF > 0) || (NCOM > 0)
void scisoft __P((void *));
void scifsoft __P((void *));
void comsoft __P((void *));

void
Xsoftserial(void)
{
	__dbg_heart_beat(HEART_BEAT_BLUE);

#if (NSCI > 0)
	scisoft(NULL);
#endif
#if (NSCIF > 0)
	scifsoft(NULL);
#endif
#if (NCOM > 0)
	comsoft(NULL);
#endif
}	
#endif /* NSCI > 0 || NSCIF > 0 || NCOM > 0 */

void
Xsoftnet(void)
{
	int s, ni;

	__dbg_heart_beat(HEART_BEAT_RED);

	s = splhigh();
	ni = netisr;
	netisr = 0;
	splx(s);

#define DONETISR(bit, fn) do {		\
	if (ni & (1 << bit))		\
		fn();			\
} while (0)

#include <net/netisr_dispatch.h>

#undef DONETISR
}

void
Xsoftclock(void)
{
	__dbg_heart_beat(HEART_BEAT_GREEN);
        softclock(NULL);
}

#define	LEGAL_IRQ(x)	((x) >= 0 && (x) < SHB_MAX_HARDINTR && (x) != 2)

int
sh_intr_alloc(mask, type, irq)
	int mask;
	int type;
	int *irq;
{
	int i, tmp, bestirq, count;
	struct intrhand **p, *q;

	if (type == IST_NONE)
		panic("intr_alloc: bogus type");

	bestirq = -1;
	count = -1;

	/* some interrupts should never be dynamically allocated */
	mask &= 0xdef8;

	/*
	 * XXX some interrupts will be used later (6 for fdc, 12 for pms).
	 * the right answer is to do "breadth-first" searching of devices.
	 */
	mask &= 0xefbf;

	for (i = 0; i < SHB_MAX_HARDINTR; i++) {
		if (LEGAL_IRQ(i) == 0 || (mask & (1 << i)) == 0)
			continue;

		switch(intrtype[i]) {
		case IST_NONE:
			/*
			 * if nothing's using the irq, just return it
			 */
			*irq = i;
			return (0);

		case IST_EDGE:
		case IST_LEVEL:
			if (type != intrtype[i])
				continue;
			/*
			 * if the irq is shareable, count the number of other
			 * handlers, and if it's smaller than the last irq like
			 * this, remember it
			 *
			 * XXX We should probably also consider the
			 * interrupt level and stick IPL_TTY with other
			 * IPL_TTY, etc.
			 */
			for (p = &intrhand[i], tmp = 0; (q = *p) != NULL;
			     p = &q->ih_next, tmp++)
				;
			if ((bestirq == -1) || (count > tmp)) {
				bestirq = i;
				count = tmp;
			}
			break;

		case IST_PULSE:
			/* this just isn't shareable */
			continue;
		}
	}

	if (bestirq == -1)
		return (1);

	*irq = bestirq;

	return (0);
}

/*
 * Deregister an interrupt handler.
 */
void
shb_intr_disestablish(ic, arg)
	void *ic;
	void *arg;
{
	struct intrhand *ih = arg;
	int irq = ih->ih_irq;
	struct intrhand **p, *q;

	mask_irq(irq);

	/*
	 * Remove the handler from the chain.
	 * This is O(n^2), too.
	 */
	for (p = &intrhand[irq]; (q = *p) != NULL && q != ih; p = &q->ih_next)
		;
	if (q)
		*p = q->ih_next;
	else
		panic("shb_intr_disestablish: handler not registered");
	free(ih, M_DEVBUF);
}
