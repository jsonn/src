/* $NetBSD: pci_6600.c,v 1.1.4.2 1999/06/29 06:46:47 ross Exp $ */

/*-
 * Copyright (c) 1999 by Ross Harvey.  All rights reserved.
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
 *	This product includes software developed by Ross Harvey.
 * 4. The name of Ross Harvey may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ROSS HARVEY ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURP0SE
 * ARE DISCLAIMED.  IN NO EVENT SHALL ROSS HARVEY BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>

__KERNEL_RCSID(0, "$NetBSD: pci_6600.c,v 1.1.4.2 1999/06/29 06:46:47 ross Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <vm/vm.h>

#include <machine/autoconf.h>
#define _ALPHA_BUS_DMA_PRIVATE
#include <machine/bus.h>
#include <machine/rpb.h>
#include <machine/intrcnt.h>
#include <machine/alpha.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>

#include <alpha/pci/tsreg.h>
#include <alpha/pci/tsvar.h>
#include <alpha/pci/pci_6600.h>

#define pci_6600() { Generate ctags(1) key. }

#include "sio.h"
#if NSIO
#include <alpha/pci/siovar.h>
#endif

#define	PCI_STRAY_MAX		5
#define	DEC_6600_MAX_IRQ	INTRCNT_OTHER_LEN

static char *irqtype = "6600 irq";
static struct tsp_config *sioprimary;

static void checkmaxirq __P((pci_intr_handle_t ih));
void dec_6600_intr_disestablish __P((void *, void *));
void *dec_6600_intr_establish __P((
    void *, pci_intr_handle_t, int, int (*func)(void *), void *));
const char *dec_6600_intr_string __P((void *, pci_intr_handle_t));
int dec_6600_intr_map __P((void *, pcitag_t, int, int, pci_intr_handle_t *));
void *dec_6600_pciide_compat_intr_establish __P((void *, struct device *,
    struct pci_attach_args *, int, int (*)(void *), void *));

struct alpha_shared_intr *dec_6600_pci_intr;

void dec_6600_iointr __P((void *framep, unsigned long vec));
extern void dec_6600_intr_enable __P((int irq));
extern void dec_6600_intr_disable __P((int irq));

void
pci_6600_pickintr(pcp)
	struct tsp_config *pcp;
{
	bus_space_tag_t iot = &pcp->pc_iot;
	pci_chipset_tag_t pc = &pcp->pc_pc;
	int i;

        pc->pc_intr_v = pcp;
        pc->pc_intr_map = dec_6600_intr_map;
        pc->pc_intr_string = dec_6600_intr_string;
        pc->pc_intr_establish = dec_6600_intr_establish;
        pc->pc_intr_disestablish = dec_6600_intr_disestablish;
	pc->pc_pciide_compat_intr_establish = NULL;

	/*
	 * System-wide and Pchip-0-only logic...
	 */
	if (dec_6600_pci_intr == NULL) {
		sioprimary = pcp;
		pc->pc_pciide_compat_intr_establish =
		    dec_6600_pciide_compat_intr_establish;
		dec_6600_pci_intr = alpha_shared_intr_alloc(DEC_6600_MAX_IRQ);
		for (i = 0; i < DEC_6600_MAX_IRQ; i++)
			alpha_shared_intr_set_maxstrays(dec_6600_pci_intr, i,
			    PCI_STRAY_MAX);
#if NSIO
		sio_intr_setup(pc, iot);
		dec_6600_intr_enable(55);	/* irq line for sio */
#endif
		set_iointr(dec_6600_iointr);
	}
}

int     
dec_6600_intr_map(acv, bustag, buspin, line, ihp)
        void *acv;
        pcitag_t bustag; 
        int buspin, line;
        pci_intr_handle_t *ihp;
{
	struct tsp_config *pcp = acv;
	pci_chipset_tag_t pc = &pcp->pc_pc;
	int bus, device, function;

	if (buspin == 0) {
		/* No IRQ used. */
		return 1;
	}
	if (buspin > 4) {
		printf("intr_map: bad interrupt pin %d\n", buspin);
		return 1;
	}

	/*
	 * The console places the interrupt mapping in the "line" value.
	 * A value of (char)-1 indicates there is no mapping.
	 */
	if (line >= 64) {	/* for usb host bridge, line == 0xea (?!) */
		alpha_pci_decompose_tag(pc, bustag, &bus, &device, &function);
		printf("intr_map: line=0x%x, no mapping for %d/%d/%d\n",
		    line, bus, device, function);
		return (1);
	}

	if (line >= INTRCNT_OTHER_LEN)
		panic("intr_map: irq too large (%d)\n", line);

	*ihp = line;
	checkmaxirq(*ihp);
	return (0);
}

static void
checkmaxirq(ih)
	pci_intr_handle_t ih;
{
	if (ih  > DEC_6600_MAX_IRQ)
		panic("extreme irq %ld\n", ih);
}

const char *
dec_6600_intr_string(acv, ih)
	void *acv;
	pci_intr_handle_t ih;
{

	static const char irqfmt[] = "dec_6600 irq %ld";
        static char irqstr[sizeof irqfmt];

	checkmaxirq(ih);
        snprintf(irqstr, sizeof irqstr, irqfmt, ih);
        return (irqstr);
}

void *
dec_6600_intr_establish(acv, ih, level, func, arg)
        void *acv, *arg;
        pci_intr_handle_t ih;
        int level;
        int (*func) __P((void *));
{
	void *cookie;

	checkmaxirq(ih);
	cookie = alpha_shared_intr_establish(dec_6600_pci_intr, ih, IST_LEVEL,
	    level, func, arg, irqtype);

	if (cookie != NULL && alpha_shared_intr_isactive(dec_6600_pci_intr, ih))
		dec_6600_intr_enable(ih);
	return (cookie);
}

void
dec_6600_intr_disestablish(acv, cookie)
        void *acv, *cookie;
{
	struct alpha_shared_intrhand *ih = cookie;
	unsigned int irq = ih->ih_num;
	int s;
 
	s = splhigh();

	alpha_shared_intr_disestablish(dec_6600_pci_intr, cookie, irqtype);
	if (alpha_shared_intr_isactive(dec_6600_pci_intr, irq) == 0) {
		dec_6600_intr_disable(irq);
		alpha_shared_intr_set_dfltsharetype(dec_6600_pci_intr, irq,
		    IST_NONE);
	}
 
	splx(s);
}

void
dec_6600_iointr(framep, vec)
	void *framep;
	unsigned long vec;
{
	int irq; 

	if (vec >= 0x900) {
		irq = (vec - 0x900) >> 4;

		if(irq >= INTRCNT_OTHER_LEN)
			panic("iointr: irq %d is too high", irq);
		++intrcnt[INTRCNT_OTHER_BASE + irq];

		if (!alpha_shared_intr_dispatch(dec_6600_pci_intr, irq)) {
			alpha_shared_intr_stray(dec_6600_pci_intr, irq,
			    irqtype);
			if (ALPHA_SHARED_INTR_DISABLE(dec_6600_pci_intr, irq))
				dec_6600_intr_disable(irq);
		}
		return;
	}
#if NSIO
	if (vec >= 0x800) {
		sio_iointr(framep, vec);
		return;
	}
#endif
	panic("iointr: weird vec 0x%lx\n", vec);
}

void
dec_6600_intr_enable(irq)
	int irq;
{
	alpha_mb();
	STQP(TS_C_DIM0) |= 1UL << irq;
	alpha_mb();
}

void
dec_6600_intr_disable(irq)
	int irq;
{
	alpha_mb();
	STQP(TS_C_DIM0) &= ~(1UL << irq);
	alpha_mb();
}

void *
dec_6600_pciide_compat_intr_establish(v, dev, pa, chan, func, arg)
	void *v;
	struct device *dev;
	struct pci_attach_args *pa;
	int chan;
	int (*func) __P((void *));
	void *arg;
{
	pci_chipset_tag_t pc = pa->pa_pc;
	void *cookie = NULL;
	int bus, irq;

	alpha_pci_decompose_tag(pc, pa->pa_tag, &bus, NULL, NULL);

	if (bus != 0 || pc->pc_intr_v != sioprimary)
		printf("Warning: strange pciide\n");

	irq = PCIIDE_COMPAT_IRQ(chan);
#if NSIO
	cookie = sio_intr_establish(NULL /*XXX*/, irq, IST_EDGE, IPL_BIO,
	    func, arg);
#endif
	return (cookie);
}
