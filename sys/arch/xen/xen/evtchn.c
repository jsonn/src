/*	$NetBSD: evtchn.c,v 1.1.2.1 2004/12/13 17:52:21 bouyer Exp $	*/

/*
 *
 * Copyright (c) 2004 Christian Limpach.
 * Copyright (c) 2004, K A Fraser.
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
 *      This product includes software developed by Christian Limpach.
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


#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: evtchn.c,v 1.1.2.1 2004/12/13 17:52:21 bouyer Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/reboot.h>

#include <uvm/uvm.h>

#include <machine/intrdefs.h>

#include <machine/xen.h>
#include <machine/hypervisor.h>
#include <machine/evtchn.h>
#include <machine/ctrl_if.h>
#include <machine/xenfunc.h>

struct pic xenev_pic = {
	.pic_dev = {
		.dv_xname = "xen_fakepic",
	},
	.pic_type = PIC_XEN,
	.pic_lock = __SIMPLELOCK_UNLOCKED,
};

/*
 * This lock protects updates to the following mapping and reference-count
 * arrays. The lock does not need to be acquired to read the mapping tables.
 */
static struct simplelock irq_mapping_update_lock = SIMPLELOCK_INITIALIZER;

/* IRQ <-> event-channel mappings. */
int evtchn_to_irq[NR_EVENT_CHANNELS];
int irq_to_evtchn[NR_IRQS];

/* IRQ <-> VIRQ mapping. */
static int virq_to_irq[NR_VIRQS];

/* Reference counts for bindings to IRQs. */
static int irq_bindcount[NR_IRQS];

#if 0
static int xen_die_handler(void *);
#endif
static int xen_debug_handler(void *);
static int xen_misdirect_handler(void *);

void
events_default_setup()
{
	int i;

	/* No VIRQ -> IRQ mappings. */
	for (i = 0; i < NR_VIRQS; i++)
		virq_to_irq[i] = -1;

	/* No event-channel -> IRQ mappings. */
	for (i = 0; i < NR_EVENT_CHANNELS; i++) {
		evtchn_to_irq[i] = -1;
		hypervisor_mask_event(i); /* No event channels are 'live' right now. */
	}

	/* No IRQ -> event-channel mappings. */
	for (i = 0; i < NR_IRQS; i++)
		irq_to_evtchn[i] = -1;
}

void
init_events()
{
	int irq;

	irq = bind_virq_to_irq(VIRQ_DEBUG);
	event_set_handler(irq, &xen_debug_handler, NULL, IPL_DEBUG);
	hypervisor_enable_irq(irq);

	irq = bind_virq_to_irq(VIRQ_MISDIRECT);
	event_set_handler(irq, &xen_misdirect_handler, NULL, IPL_DIE);
	hypervisor_enable_irq(irq);

	/* This needs to be done early, but after the IRQ subsystem is
	 * alive. */
	ctrl_if_init();

	enable_intr();		/* at long last... */
}

unsigned int
do_event(int irq, struct intrframe *regs)
{
	struct cpu_info *ci;
	int ilevel;
	struct intrhand *ih;
	int	(*ih_fun)(void *, void *);
	extern struct uvmexp uvmexp;

	if (irq >= NR_IRQS) {
#ifdef DIAGNOSTIC
		printf("event irq number %d > NR_IRQS\n", irq);
#endif
		return ENOENT;
	}

	if (0 && irq == 4) {
		ci = &cpu_info_primary;
		printf("do_event %d/%d called, ilevel %d\n", irq,
		       irq_to_evtchn[irq], ci->ci_ilevel);
	}

	ci = &cpu_info_primary;

	hypervisor_acknowledge_irq(irq);
	if (ci->ci_isources[irq] == NULL) {
		hypervisor_enable_irq(irq);
		return 0;
	}
	ilevel = ci->ci_ilevel;
	if (ci->ci_isources[irq]->is_maxlevel <= ilevel) {
		ci->ci_ipending |= 1 << irq;
		/* leave masked */
		return 0;
	}
	uvmexp.intrs++;
	ci->ci_isources[irq]->is_evcnt.ev_count++;
	ci->ci_ilevel = ci->ci_isources[irq]->is_maxlevel;
	/* sti */
	ci->ci_idepth++;
#ifdef MULTIPROCESSOR
	x86_intlock(regs);
#endif
	ih = ci->ci_isources[irq]->is_handlers;
	while (ih != NULL) {
		if (ih->ih_level <= ilevel) {
#ifdef MULTIPROCESSOR
			x86_intunlock(regs);
#endif
			ci->ci_ipending |= 1 << irq;
			/* leave masked */
			ci->ci_idepth--;
			splx(ilevel);
			return 0;
		}
		ci->ci_ilevel = ih->ih_level;
		ih_fun = (void *)ih->ih_fun;
		ih_fun(ih->ih_arg, regs);
		ih = ih->ih_next;
	}
#ifdef MULTIPROCESSOR
	x86_intunlock(regs);
#endif
	hypervisor_enable_irq(irq);
	ci->ci_idepth--;
	splx(ilevel);

	if (0 && irq == 4)
		printf("do_event %d done, ipending %08x\n", irq,
		    ci->ci_ipending);

	return 0;
}

static int
find_unbound_irq(void)
{
	int irq;

	for (irq = 0; irq < NR_IRQS; irq++)
		if (irq_bindcount[irq] == 0)
			break;

	if (irq == NR_IRQS)
		panic("No available IRQ to bind to: increase NR_IRQS!\n");

	return irq;
}

int
bind_virq_to_irq(int virq)
{
	evtchn_op_t op;
	int evtchn, irq;

	simple_lock(&irq_mapping_update_lock);

	irq = virq_to_irq[virq];
	if (irq == -1) {
		op.cmd = EVTCHNOP_bind_virq;
		op.u.bind_virq.virq = virq;
		if (HYPERVISOR_event_channel_op(&op) != 0)
			panic("Failed to bind virtual IRQ %d\n", virq);
		evtchn = op.u.bind_virq.port;

		irq = find_unbound_irq();
		evtchn_to_irq[evtchn] = irq;
		irq_to_evtchn[irq] = evtchn;

		virq_to_irq[virq] = irq;
	}

	irq_bindcount[irq]++;

	simple_unlock(&irq_mapping_update_lock);
    
	return irq;
}

void
unbind_virq_from_irq(int virq)
{
	evtchn_op_t op;
	int irq = virq_to_irq[virq];
	int evtchn = irq_to_evtchn[irq];

	simple_lock(&irq_mapping_update_lock);

	irq_bindcount[irq]--;
	if (irq_bindcount[irq] == 0) {
		op.cmd = EVTCHNOP_close;
		op.u.close.dom = DOMID_SELF;
		op.u.close.port = evtchn;
		if (HYPERVISOR_event_channel_op(&op) != 0)
			panic("Failed to unbind virtual IRQ %d\n", virq);

		evtchn_to_irq[evtchn] = -1;
		irq_to_evtchn[irq] = -1;
		virq_to_irq[virq] = -1;
	}

	simple_unlock(&irq_mapping_update_lock);
}

int bind_evtchn_to_irq(int evtchn)
{
	int irq;

	simple_lock(&irq_mapping_update_lock);

	irq = evtchn_to_irq[evtchn];
	if (irq == -1) {
		irq = find_unbound_irq();
		evtchn_to_irq[evtchn] = irq;
		irq_to_evtchn[irq] = evtchn;
	}

	irq_bindcount[irq]++;

	simple_unlock(&irq_mapping_update_lock);
    
	return irq;
}

int
event_set_handler(int irq, ev_handler_t handler, void *arg, int level)
{
	struct intrsource *isp;
	struct intrhand *ih;
	struct cpu_info *ci;

	if (irq >= NR_IRQS) {
#ifdef DIAGNOSTIC
		printf("irq number %d > NR_IRQS\n", irq);
#endif
		return ENOENT;
	}

#if 0
	printf("event_set_handler irq %d/%d handler %p level %d\n", irq,
	       irq_to_evtchn[irq], handler, level);
#endif
	/* XXXcl handle already bound irq */

	MALLOC(isp, struct intrsource *, sizeof (struct intrsource), M_DEVBUF,
	    M_WAITOK|M_ZERO);
	if (isp == NULL)
		panic("can't allocate fixed interrupt source");
	MALLOC(ih, struct intrhand *, sizeof (struct intrhand), M_DEVBUF,
	    M_WAITOK|M_ZERO);
	if (ih == NULL)
		panic("can't allocate fixed interrupt source");

	ci = &cpu_info_primary;

	isp->is_recurse = xenev_stubs[irq].ist_recurse;
	isp->is_resume = xenev_stubs[irq].ist_resume;
	ih->ih_level = level;
	ih->ih_fun = handler;
	ih->ih_arg = arg;
	ih->ih_next = NULL;
	isp->is_handlers = ih;
	isp->is_pic = &xenev_pic;
	ci->ci_isources[irq] = isp;
	evcnt_attach_dynamic(&isp->is_evcnt, EVCNT_TYPE_INTR, NULL,
	    ci->ci_dev->dv_xname, "xenev");

	intr_calculatemasks(ci);

	return 0;
}

void hypervisor_enable_irq(unsigned int irq)
{

	hypervisor_unmask_event(irq_to_evtchn[irq]);
}

void hypervisor_disable_irq(unsigned int irq)
{

	hypervisor_mask_event(irq_to_evtchn[irq]);
}

void hypervisor_acknowledge_irq(unsigned int irq)
{

	hypervisor_mask_event(irq_to_evtchn[irq]);
	hypervisor_clear_event(irq_to_evtchn[irq]);
}

#if 0
static int
xen_die_handler(void *arg)
{
	printf("hypervisor: DIE event received...\n");
	cpu_reboot(0, NULL);
	/* NOTREACHED */
	return 0;
}
#endif

static int
xen_debug_handler(void *arg)
{
	printf("debug event\n");
	return 0;
}

static int
xen_misdirect_handler(void *arg)
{
#if 0
	char *msg = "misdirect\n";
	(void)HYPERVISOR_console_io(CONSOLEIO_write, strlen(msg), msg);
#endif
	return 0;
}
