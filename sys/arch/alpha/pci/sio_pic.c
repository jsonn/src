/* $NetBSD: sio_pic.c,v 1.16.2.1 1997/06/01 04:13:48 cgd Exp $ */

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <machine/options.h>		/* Config options headers */
#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: sio_pic.c,v 1.16.2.1 1997/06/01 04:13:48 cgd Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/syslog.h>

#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <alpha/pci/siovar.h>

#ifndef EVCNT_COUNTERS
#include <machine/intrcnt.h>
#endif

#include "sio.h"

/*
 * To add to the long history of wonderful PROM console traits,
 * AlphaStation PROMs don't reset themselves completely on boot!
 * Therefore, if an interrupt was turned on when the kernel was
 * started, we're not going to EVER turn it off...  I don't know
 * what will happen if new interrupts (that the PROM console doesn't
 * want) are turned on.  I'll burn that bridge when I come to it.
 */
#define	BROKEN_PROM_CONSOLE

/*
 * Private functions and variables.
 */

bus_space_tag_t sio_iot;
bus_space_handle_t sio_ioh_icu1, sio_ioh_icu2, sio_ioh_elcr;

#define	ICU_LEN		16		/* number of ISA IRQs */

static struct alpha_shared_intr *sio_intr;
#ifdef EVCNT_COUNTERS
struct evcnt sio_intr_evcnt;
#endif

#ifndef STRAY_MAX
#ifdef BROKEN_PROM_CONSOLE
/*
 * If prom console is broken, because initial interrupt settings
 * must be kept, there's no way to escape stray interrupts.
 */
#define	STRAY_MAX	0
#else
#define	STRAY_MAX	5
#endif
#endif

#ifdef BROKEN_PROM_CONSOLE
/*
 * If prom console is broken, must remember the initial interrupt
 * settings and enforce them.  WHEE!
 */
u_int8_t initial_ocw1[2];
u_int8_t initial_elcr[2];
#define	INITIALLY_ENABLED(irq) \
	    ((initial_ocw1[(irq) / 8] & (1 << ((irq) % 8))) == 0)
#define	INITIALLY_LEVEL_TRIGGERED(irq) \
	    ((initial_elcr[(irq) / 8] & (1 << ((irq) % 8))) != 0)
#else
#define	INITIALLY_ENABLED(irq)		((irq) == 2 ? 1 : 0)
#define	INITIALLY_LEVEL_TRIGGERED(irq)	0
#endif

void	sio_setirqstat __P((int, int, int));

void
sio_setirqstat(irq, enabled, type)
	int irq, enabled;
	int type;
{
	u_int8_t ocw1[2], elcr[2];
	int icu, bit;

#if 0
	printf("sio_setirqstat: irq %d: %s, %s\n", irq,
	    enabled ? "enabled" : "disabled", isa_intr_typename(type));
#endif

	icu = irq / 8;
	bit = irq % 8;

	ocw1[0] = bus_space_read_1(sio_iot, sio_ioh_icu1, 1);
	ocw1[1] = bus_space_read_1(sio_iot, sio_ioh_icu2, 1);
	elcr[0] = bus_space_read_1(sio_iot, sio_ioh_elcr, 0);	/* XXX */
	elcr[1] = bus_space_read_1(sio_iot, sio_ioh_elcr, 1);	/* XXX */

	/*
	 * interrupt enable: set bit to mask (disable) interrupt.
	 */
	if (enabled)
		ocw1[icu] &= ~(1 << bit);
	else
		ocw1[icu] |= 1 << bit;

	/*
	 * interrupt type select: set bit to get level-triggered.
	 */
	if (type == IST_LEVEL)
		elcr[icu] |= 1 << bit;
	else
		elcr[icu] &= ~(1 << bit);

#ifdef not_here
	/* see the init function... */
	ocw1[0] &= ~0x04;		/* always enable IRQ2 on first PIC */
	elcr[0] &= ~0x07;		/* IRQ[0-2] must be edge-triggered */
	elcr[1] &= ~0x21;		/* IRQ[13,8] must be edge-triggered */
#endif

#ifdef BROKEN_PROM_CONSOLE
	/*
	 * make sure that the initially clear bits (unmasked interrupts)
	 * are never set, and that the initially-level-triggered
	 * intrrupts always remain level-triggered, to keep the prom happy.
	 */
	if ((ocw1[0] & ~initial_ocw1[0]) != 0 ||
	    (ocw1[1] & ~initial_ocw1[1]) != 0 ||
	    (elcr[0] & initial_elcr[0]) != initial_elcr[0] ||
	    (elcr[1] & initial_elcr[1]) != initial_elcr[1]) {
		printf("sio_sis: initial: ocw = (%2x,%2x), elcr = (%2x,%2x)\n",
		    initial_ocw1[0], initial_ocw1[1],
		    initial_elcr[0], initial_elcr[1]);
		printf("         current: ocw = (%2x,%2x), elcr = (%2x,%2x)\n",
		    ocw1[0], ocw1[1], elcr[0], elcr[1]);
		panic("sio_setirqstat: hosed");
	}
#endif

	bus_space_write_1(sio_iot, sio_ioh_icu1, 1, ocw1[0]);
	bus_space_write_1(sio_iot, sio_ioh_icu2, 1, ocw1[1]);
	bus_space_write_1(sio_iot, sio_ioh_elcr, 0, elcr[0]);	/* XXX */
	bus_space_write_1(sio_iot, sio_ioh_elcr, 1, elcr[1]);	/* XXX */
}

void
sio_intr_setup(iot)
	bus_space_tag_t iot;
{
	int i;

	sio_iot = iot;

	if (bus_space_map(sio_iot, IO_ICU1, IO_ICUSIZE, 0, &sio_ioh_icu1) ||
	    bus_space_map(sio_iot, IO_ICU2, IO_ICUSIZE, 0, &sio_ioh_icu2) ||
	    bus_space_map(sio_iot, 0x4d0, 2, 0, &sio_ioh_elcr))
		panic("sio_intr_setup: can't map I/O ports");

#ifdef BROKEN_PROM_CONSOLE
	/*
	 * Remember the initial values, because the prom is stupid.
	 */
	initial_ocw1[0] = bus_space_read_1(sio_iot, sio_ioh_icu1, 1);
	initial_ocw1[1] = bus_space_read_1(sio_iot, sio_ioh_icu2, 1);
	initial_elcr[0] = bus_space_read_1(sio_iot, sio_ioh_elcr, 0); /* XXX */
	initial_elcr[1] = bus_space_read_1(sio_iot, sio_ioh_elcr, 1); /* XXX */
#if 0
	printf("initial_ocw1[0] = 0x%x\n", initial_ocw1[0]);
	printf("initial_ocw1[1] = 0x%x\n", initial_ocw1[1]);
	printf("initial_elcr[0] = 0x%x\n", initial_elcr[0]);
	printf("initial_elcr[1] = 0x%x\n", initial_elcr[1]);
#endif
#endif

	sio_intr = alpha_shared_intr_alloc(ICU_LEN);

	/*
	 * set up initial values for interrupt enables.
	 */
	for (i = 0; i < ICU_LEN; i++) {
		alpha_shared_intr_set_maxstrays(sio_intr, i, STRAY_MAX);

		switch (i) {
		case 0:
		case 1:
		case 8:
		case 13:
			/*
			 * IRQs 0, 1, 8, and 13 must always be
			 * edge-triggered.
			 */
			if (INITIALLY_LEVEL_TRIGGERED(i))
				printf("sio_intr_setup: %d LT!\n", i);
			sio_setirqstat(i, INITIALLY_ENABLED(i), IST_EDGE);
			alpha_shared_intr_set_dfltsharetype(sio_intr, i,
			    IST_EDGE);
			break;

		case 2:
			/*
			 * IRQ 2 must be edge-triggered, and should be
			 * enabled (otherwise IRQs 8-15 are ignored).
			 */
			if (INITIALLY_LEVEL_TRIGGERED(i))
				printf("sio_intr_setup: %d LT!\n", i);
			if (!INITIALLY_ENABLED(i))
				printf("sio_intr_setup: %d not enabled!\n", i);
			sio_setirqstat(i, 1, IST_EDGE);
			alpha_shared_intr_set_dfltsharetype(sio_intr, i,
			    IST_UNUSABLE);
			break;

		default:
			/*
			 * Otherwise, disable the IRQ and set its
			 * type to (effectively) "unknown."
			 */
			sio_setirqstat(i, INITIALLY_ENABLED(i),
			    INITIALLY_LEVEL_TRIGGERED(i) ? IST_LEVEL :
				IST_NONE);
			alpha_shared_intr_set_dfltsharetype(sio_intr, i,
			    INITIALLY_LEVEL_TRIGGERED(i) ? IST_LEVEL :
                                IST_NONE);
			break;
		}
	}
}

const char *
sio_intr_string(v, irq)
	void *v;
	int irq;
{
	static char irqstr[12];		/* 8 + 2 + NULL + sanity */

	if (irq == 0 || irq >= ICU_LEN || irq == 2)
		panic("sio_intr_string: bogus isa irq 0x%x\n", irq);

	sprintf(irqstr, "isa irq %d", irq);
	return (irqstr);
}

void *
sio_intr_establish(v, irq, type, level, fn, arg)
	void *v, *arg;
        int irq;
        int type;
        int level;
        int (*fn)(void *);
{
	void *cookie;

	if (irq > ICU_LEN || type == IST_NONE)
		panic("sio_intr_establish: bogus irq or type");

	cookie = alpha_shared_intr_establish(sio_intr, irq, type, level, fn,
	    arg, "isa irq");

	if (cookie)
		sio_setirqstat(irq, alpha_shared_intr_isactive(sio_intr, irq),
		    alpha_shared_intr_get_sharetype(sio_intr, irq));

	return (cookie);
}

void
sio_intr_disestablish(v, cookie)
	void *v;
	void *cookie;
{

	printf("sio_intr_disestablish(%p)\n", cookie);
	/* XXX */

	/* XXX NEVER ALLOW AN INITIALLY-ENABLED INTERRUPT TO BE DISABLED */
	/* XXX NEVER ALLOW AN INITIALLY-LT INTERRUPT TO BECOME UNTYPED */
}

void
sio_iointr(framep, vec)
	void *framep;
	unsigned long vec;
{
	int irq;

	irq = (vec - 0x800) >> 4;
#ifdef DIAGNOSTIC
	if (irq > ICU_LEN || irq < 0)
		panic("sio_iointr: irq out of range (%d)", irq);
#endif

#ifdef EVCNT_COUNTERS
	sio_intr_evcnt.ev_count++;
#else
#ifdef DEBUG
	if (ICU_LEN != INTRCNT_ISA_IRQ_LEN)
		panic("sio interrupt counter sizes inconsistent");
#endif
	intrcnt[INTRCNT_ISA_IRQ + irq]++;
#endif

	if (!alpha_shared_intr_dispatch(sio_intr, irq))
		alpha_shared_intr_stray(sio_intr, irq, "isa irq");

	/*
	 * Some versions of the machines which use the SIO
	 * (or is it some PALcode revisions on those machines?)
	 * require the non-specific EOI to be fed to the PIC(s)
	 * by the interrupt handler.
	 */
	if (irq > 7)
		bus_space_write_1(sio_iot,
		    sio_ioh_icu2, 0, 0x20 | (irq & 0x07));	/* XXX */
	bus_space_write_1(sio_iot,
	    sio_ioh_icu1, 0, 0x20 | (irq > 7 ? 2 : irq));	/* XXX */
}
