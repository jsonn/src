/* $NetBSD: pci_kn20aa.c,v 1.21.2.1 1997/06/01 04:13:33 cgd Exp $ */

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

__KERNEL_RCSID(0, "$NetBSD: pci_kn20aa.c,v 1.21.2.1 1997/06/01 04:13:33 cgd Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/syslog.h>

#include <vm/vm.h>

#include <machine/autoconf.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <alpha/pci/ciareg.h>
#include <alpha/pci/ciavar.h>

#include <alpha/pci/pci_kn20aa.h>

#ifndef EVCNT_COUNTERS
#include <machine/intrcnt.h>
#endif

#include "sio.h"
#if NSIO
#include <alpha/pci/siovar.h>
#endif

int	dec_kn20aa_intr_map __P((void *, pcitag_t, int, int,
	    pci_intr_handle_t *));
const char *dec_kn20aa_intr_string __P((void *, pci_intr_handle_t));
void	*dec_kn20aa_intr_establish __P((void *, pci_intr_handle_t,
	    int, int (*func)(void *), void *));
void	dec_kn20aa_intr_disestablish __P((void *, void *));

#define	KN20AA_PCEB_IRQ	31
#define	KN20AA_MAX_IRQ	32
#define	PCI_STRAY_MAX	5

struct alpha_shared_intr *kn20aa_pci_intr;
#ifdef EVCNT_COUNTERS
struct evcnt kn20aa_intr_evcnt;
#endif

void	kn20aa_iointr __P((void *framep, unsigned long vec));
void	kn20aa_enable_intr __P((int irq));
void	kn20aa_disable_intr __P((int irq));

void
pci_kn20aa_pickintr(ccp)
	struct cia_config *ccp;
{
	int i;
	bus_space_tag_t iot = ccp->cc_iot;
	pci_chipset_tag_t pc = &ccp->cc_pc;

        pc->pc_intr_v = ccp;
        pc->pc_intr_map = dec_kn20aa_intr_map;
        pc->pc_intr_string = dec_kn20aa_intr_string;
        pc->pc_intr_establish = dec_kn20aa_intr_establish;
        pc->pc_intr_disestablish = dec_kn20aa_intr_disestablish;

	kn20aa_pci_intr = alpha_shared_intr_alloc(KN20AA_MAX_IRQ);
	for (i = 0; i < KN20AA_MAX_IRQ; i++)
		alpha_shared_intr_set_maxstrays(kn20aa_pci_intr, i,
		    PCI_STRAY_MAX);

#if NSIO
	sio_intr_setup(iot);
	kn20aa_enable_intr(KN20AA_PCEB_IRQ);
#endif

	set_iointr(kn20aa_iointr);
}

int     
dec_kn20aa_intr_map(ccv, bustag, buspin, line, ihp)
        void *ccv;
        pcitag_t bustag; 
        int buspin, line;
        pci_intr_handle_t *ihp;
{
	struct cia_config *ccp = ccv;
	pci_chipset_tag_t pc = &ccp->cc_pc;
	int device;
	int kn20aa_irq;

        if (buspin == 0) {
                /* No IRQ used. */
                return 1;
        }
        if (buspin > 4) {
                printf("pci_map_int: bad interrupt pin %d\n", buspin);
                return 1;
        }

	/*
	 * Slot->interrupt translation.  Appears to work, though it
	 * may not hold up forever.
	 *
	 * The DEC engineers who did this hardware obviously engaged
	 * in random drug testing.
	 */
	pci_decompose_tag(pc, bustag, NULL, &device, NULL);
	switch (device) {
	case 11:
	case 12:
		kn20aa_irq = ((device - 11) + 0) * 4;
		break;

	case 7:
		kn20aa_irq = 8;
		break;

	case 9:
		kn20aa_irq = 12;
		break;

	case 6:					/* 21040 on AlphaStation 500 */
		kn20aa_irq = 13;
		break;

	case 8:
		kn20aa_irq = 16;
		break;

	default:
                printf("dec_kn20aa_intr_map: weird device number %d\n",
		    device);
                return 1;
	}

	kn20aa_irq += buspin - 1;
	if (kn20aa_irq > KN20AA_MAX_IRQ)
		panic("pci_kn20aa_map_int: kn20aa_irq too large (%d)\n",
		    kn20aa_irq);

	*ihp = kn20aa_irq;
	return (0);
}

const char *
dec_kn20aa_intr_string(ccv, ih)
	void *ccv;
	pci_intr_handle_t ih;
{
#if 0
	struct cia_config *ccp = ccv;
#endif
        static char irqstr[15];          /* 11 + 2 + NULL + sanity */

        if (ih > KN20AA_MAX_IRQ)
                panic("dec_kn20aa_intr_string: bogus kn20aa IRQ 0x%x\n",
		    ih);

        sprintf(irqstr, "kn20aa irq %ld", ih);
        return (irqstr);
}

void *
dec_kn20aa_intr_establish(ccv, ih, level, func, arg)
        void *ccv, *arg;
        pci_intr_handle_t ih;
        int level;
        int (*func) __P((void *));
{           
#if 0
        struct cia_config *ccp = ccv;
#endif
	void *cookie;

        if (ih > KN20AA_MAX_IRQ)
                panic("dec_kn20aa_intr_establish: bogus kn20aa IRQ 0x%x\n",
		    ih);

	cookie = alpha_shared_intr_establish(kn20aa_pci_intr, ih, IST_LEVEL,
	    level, func, arg, "kn20aa irq");

	if (cookie != NULL &&
	    alpha_shared_intr_isactive(kn20aa_pci_intr, ih))
		kn20aa_enable_intr(ih);
	return (cookie);
}

void    
dec_kn20aa_intr_disestablish(ccv, cookie)
        void *ccv, *cookie;
{
#if 0
	struct cia_config *ccp = ccv;
#endif

	panic("dec_kn20aa_intr_disestablish not implemented"); /* XXX */
}

void
kn20aa_iointr(framep, vec)
	void *framep;
	unsigned long vec;
{
	int irq;

	if (vec >= 0x900) {
		if (vec >= 0x900 + (KN20AA_MAX_IRQ << 4))
			panic("kn20aa_iointr: vec 0x%x out of range\n", vec);
		irq = (vec - 0x900) >> 4;

#ifdef EVCNT_COUNTERS
		kn20aa_intr_evcnt.ev_count++;
#else
		if (KN20AA_MAX_IRQ != INTRCNT_KN20AA_IRQ_LEN)
			panic("kn20aa interrupt counter sizes inconsistent");
		intrcnt[INTRCNT_KN20AA_IRQ + irq]++;
#endif

		if (!alpha_shared_intr_dispatch(kn20aa_pci_intr, irq)) {
			alpha_shared_intr_stray(kn20aa_pci_intr, irq,
			    "kn20aa irq");
			if (kn20aa_pci_intr[irq].intr_nstrays ==
			    kn20aa_pci_intr[irq].intr_maxstrays)
				kn20aa_disable_intr(irq);
		}
		return;
	}
#if NSIO
	if (vec >= 0x800) {
		sio_iointr(framep, vec);
		return;
	} 
#endif
	panic("kn20aa_iointr: weird vec 0x%x\n", vec);
}

void
kn20aa_enable_intr(irq)
	int irq;
{

	/*
	 * From disassembling small bits of the OSF/1 kernel:
	 * the following appears to enable a given interrupt request.
	 * "blech."  I'd give valuable body parts for better docs or
	 * for a good decompiler.
	 */
	alpha_mb();
	REGVAL(0x8780000000L + 0x40L) |= (1 << irq);	/* XXX */
	alpha_mb();
}

void
kn20aa_disable_intr(irq)
	int irq;
{

	alpha_mb();
	REGVAL(0x8780000000L + 0x40L) &= ~(1 << irq);	/* XXX */
	alpha_mb();
}
