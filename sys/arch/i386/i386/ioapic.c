/* $NetBSD: ioapic.c,v 1.1.2.6 2000/08/18 13:54:25 sommerfeld Exp $ */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by RedBack Networks Inc.
 *
 * Author: Bill Sommerfeld
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
 * Copyright (c) 1999 Stefan Grefen
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
 *      This product includes software developed by the NetBSD 
 *      Foundation, Inc. and its contributors.  
 * 4. Neither the name of The NetBSD Foundation nor the names of its 
 *    contributors may be used to endorse or promote products derived  
 *    from this software without specific prior written permission.   
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR AND CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "opt_ddb.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/isa_machdep.h> /* XXX intrhand */
 
#include <uvm/uvm_extern.h>
#include <machine/i82093reg.h>
#include <machine/i82093var.h>

#include <machine/i82489reg.h>
#include <machine/i82489var.h>

#include <machine/pmap.h>

#include <machine/mpbiosvar.h>

/*
 * maps an IO-apic
 * TODO locking, export of interrupt functions
 * and mapping of interrupts.
 */

int     ioapic_match __P((struct device *, struct cfdata *, void *));
void    ioapic_attach __P((struct device *, struct device *, void *));

int     i386_mem_add_mapping __P((bus_addr_t, bus_size_t,
            int, bus_space_handle_t *)); /* XXX */

void	apic_vectorset __P((struct ioapic_softc *, int, int));

int apic_verbose = 0;

int ioapic_bsp_id = 0;
int ioapic_cold = 1;

static __inline  u_int32_t
ioapic_read(struct ioapic_softc *sc,int regid)
{
	u_int32_t val;
	
	/*
	 * TODO: lock apic  
	 */
	*(sc->sc_reg) = regid;
	val = *sc->sc_data;

	return val;
	
}

static __inline  void
ioapic_write(struct ioapic_softc *sc,int regid, int val)
{
	/*
	 * todo lock apic  
	 */

	*(sc->sc_reg) = regid;
	*(sc->sc_data) = val;
}

struct cfattach ioapic_ca = {
	sizeof(struct ioapic_softc), ioapic_match, ioapic_attach
};

/*
 * table of ioapics indexed by apic id.
 */

struct ioapic_softc *ioapics[16] = { 0 };

int
ioapic_match(parent, match, aux)
	struct device *parent;  
	struct cfdata *match;   
	void *aux;
{
	struct apic_attach_args * aaa = (struct apic_attach_args *) aux;

	if (strcmp(aaa->aaa_name, match->cf_driver->cd_name) == 0)
		return 1;
	return 0;
}

void ioapic_print_redir (struct ioapic_softc *sc, char *why, int pin)
{
	u_int32_t redirlo = ioapic_read(sc, IOAPIC_REDLO(pin));
	u_int32_t redirhi = ioapic_read(sc, IOAPIC_REDHI(pin));

	apic_format_redir(sc->sc_dev.dv_xname, why, pin, redirhi, redirlo);
}


/*
 * can't use bus_space_xxx as we don't have a bus handle ...
 */
void 
ioapic_attach(parent, self, aux)   
	struct device *parent, *self;
	void *aux;
{
	struct ioapic_softc *sc = (struct ioapic_softc *)self;  
	struct apic_attach_args  *aaa = (struct apic_attach_args  *) aux;
	int apic_id;
	bus_space_handle_t bh;
	u_int32_t ver_sz;
	int i;
	
	sc->sc_flags = aaa->flags;
	sc->sc_apicid = aaa->apic_id;

	printf(" apid %d (I/O APIC)\n", aaa->apic_id);

	if (ioapics[aaa->apic_id] != NULL) {
		printf("%s: duplicate apic id (ignored)\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	ioapics[aaa->apic_id] = sc;
	
	printf("%s: pa 0x%lx", sc->sc_dev.dv_xname, aaa->apic_address);

	if (i386_mem_add_mapping(aaa->apic_address, NBPG, 0, &bh) != 0) {
		printf(": map failed\n");
		return;
	}
	sc->sc_reg = (volatile u_int32_t *)(bh + IOAPIC_REG);
	sc->sc_data = (volatile u_int32_t *)(bh + IOAPIC_DATA);	

	apic_id = (ioapic_read(sc,IOAPIC_ID)&IOAPIC_ID_MASK)>>IOAPIC_ID_SHIFT;
	ver_sz = ioapic_read(sc, IOAPIC_VER);
	
	sc->sc_apic_vers = (ver_sz & IOAPIC_VER_MASK) >> IOAPIC_VER_SHIFT;
	sc->sc_apic_sz = (ver_sz & IOAPIC_MAX_MASK) >> IOAPIC_MAX_SHIFT;
	sc->sc_apic_sz++;

	if (mp_verbose) {
		printf(", %s mode",
		    aaa->flags & IOAPIC_PICMODE ? "PIC" : "virtual wire");
	}
	
	printf(", version %x, %d pins\n", sc->sc_apic_vers, sc->sc_apic_sz);

	sc->sc_pins = malloc(sizeof(struct ioapic_pin) * sc->sc_apic_sz,
	    M_DEVBUF, M_WAITOK);

	for (i=0; i<sc->sc_apic_sz; i++) {
		sc->sc_pins[i].ip_handler = NULL;
		sc->sc_pins[i].ip_next = NULL;
		sc->sc_pins[i].ip_map = NULL;
		sc->sc_pins[i].ip_vector = 0;
		sc->sc_pins[i].ip_type = 0;
		sc->sc_pins[i].ip_level = 0;
	}
	
	/*
	 * In case the APIC is not initialized to the correct ID
	 * do it now.
	 * Maybe we should record the original ID for interrupt
	 * mapping later ...
	 */
	if (apic_id != sc->sc_apicid) {
		printf("%s: misconfigured as apic %d\n", sc->sc_dev.dv_xname, apic_id);

		ioapic_write(sc,IOAPIC_VER,
		    (ioapic_read(sc,IOAPIC_ID)&~IOAPIC_ID_MASK)
		    |(sc->sc_apicid<<IOAPIC_ID_SHIFT));
		
		apic_id = (ioapic_read(sc,IOAPIC_ID)&IOAPIC_ID_MASK)>>IOAPIC_ID_SHIFT;
		
		if (apic_id != sc->sc_apicid) {
			printf("%s: can't remap to apid %d\n",
			    sc->sc_dev.dv_xname,
			    sc->sc_apicid);
		} else {
			printf("%s: remapped to apic %d\n",
			    sc->sc_dev.dv_xname,
			    sc->sc_apicid);
		}
	}
#if 0
	/* output of this was boring. */
	if (mp_verbose)
		for (i=0; i<sc->sc_apic_sz; i++)
			ioapic_print_redir(sc, "boot", i);
#endif
}

/*
 * Interrupt mapping.
 *
 * Multiple handlers may exist for each pin, so there's an
 * intrhand chain for each pin.
 *
 * Ideally, each pin maps to a single vector at the priority of the
 * highest level interrupt for that pin.
 *
 * XXX in the event that there are more than 16 interrupt sources at a
 * single level, some doubling-up may be needed.  This is not yet
 * implemented.
 *
 * XXX we are wasting some space here because we only use a limited
 * range of the vectors here.  (0x30..0xef)
 */

struct intrhand *apic_intrhand[256];
int	apic_intrcount[256];

#if 0
int apic_intrtype[APIC_ICU_LEN];
int apic_intrlevel[NIPL];
int apic_imask[NIPL];
#endif

/* XXX should check vs. softc max int number */
#define	LEGAL_IRQ(x)	((x) >= 0 && (x) < APIC_ICU_LEN && (x) != 2)

static void
apic_set_redir (struct ioapic_softc *sc, int irq)
{
	u_int32_t redlo;
	u_int32_t redhi = 0;
	int delmode;

	struct ioapic_pin *pin;
	struct mp_intr_map *map;
	
	pin = &sc->sc_pins[irq];
	map = pin->ip_map;
	if (map == NULL) {
		redlo = IOAPIC_REDLO_MASK;
	} else {
		redlo = map->redir;
	}
	delmode = (redlo & IOAPIC_REDLO_DEL_MASK) >> IOAPIC_REDLO_DEL_SHIFT;
	
	/* XXX magic numbers */
	if ((delmode != 0) && (delmode != 1))
		;
	else if (pin->ip_handler == NULL) {
		redlo |= IOAPIC_REDLO_MASK;
	} else {
		redlo |= (pin->ip_vector & 0xff);
		redlo |= (IOAPIC_REDLO_DEL_FIXED<<IOAPIC_REDLO_DEL_SHIFT);
		redlo &= ~IOAPIC_REDLO_DSTMOD;
		
		/* destination: BSP CPU */

		/*
		 * XXX will want to eventually switch to
		 * lowest-priority delivery mode, possibly with focus
		 * processor.
		 */
		redhi |= (ioapic_bsp_id << IOAPIC_REDHI_DEST_SHIFT);

		/* XXX derive this bit from BIOS info */
		if (pin->ip_type == IST_LEVEL)
			redlo |= IOAPIC_REDLO_LEVEL;
		else
			redlo &= ~IOAPIC_REDLO_LEVEL;
		/* XXX polarity goo, too */
	}
	ioapic_write(sc,IOAPIC_REDLO(irq), redlo);
	ioapic_write(sc,IOAPIC_REDHI(irq), redhi);
	if (mp_verbose)
		ioapic_print_redir(sc, "int", irq);
}

static int fakeintr __P((void *)); 	/* XXX headerify */
extern char *isa_intr_typename (int); 	/* XXX headerify */

static int fakeintr(arg)
	void *arg;
{
	return 0;
}


/*
 * apic_vectorset: allocate a vector for the given pin, based on
 * the levels of the interrupts on that pin.
 *
 * XXX if the level of the pin changes while the pin is
 * masked, need to do something special to prevent pending
 * interrupts from being lost.
 * (the answer may be to hang the interrupt chain off of both vectors
 * until any interrupts from the old source have been handled.  the trouble
 * is that we don't have a global view of what interrupts are pending.
 *
 * Deferring for now since MP systems are more likely servers rather
 * than laptops or desktops, and thus will have relatively static
 * interrupt configuration.
 */

void
apic_vectorset (sc, irq, level)
	struct ioapic_softc *sc;
	int irq;
	int level;
{
	struct ioapic_pin *pin = &sc->sc_pins[irq];
	int ovector = 0;
	int nvector = 0;
	void (*handler)(void);
	
	ovector = pin->ip_vector;

	if (level == 0) {
		/* no vector needed. */
		pin->ip_level = 0;
		pin->ip_vector = 0;
	} else if (level != pin->ip_level) {
		nvector = idt_vec_alloc (level, level+15);

		if (nvector == NULL) {
			/*
			 * XXX XXX we should be able to deal here..
			 * need to double-up an existing vector
			 * and install a slightly different handler.
			 */
			panic("apic_vectorset: no free vectors");
		}
		handler = apichandler[(nvector & 0xf) +
		    ((level > IPL_HIGH) ? 0x10 : 0)];
		idt_vec_set(nvector, handler);
		pin->ip_vector = nvector;
		pin->ip_level = level;
	}
	apic_intrhand[pin->ip_vector] = pin->ip_handler;

	if (ovector) {
		/*
		 * XXX should defer this until we're sure the old vector
		 * doesn't have a pending interrupt on any processor.
		 * do this by setting a counter equal to the number of CPU's,
		 * and firing off a low-priority broadcast IPI to all cpu's.
		 * each cpu then decrements the counter; when it
		 * goes to zero, free the vector..
		 * i.e., defer until all processors have run with a CPL
		 * less than the level of the interrupt..
		 *
		 * this is only an issue for dynamic interrupt configuration
		 * (e.g., cardbus or pcmcia).
		 */
		apic_intrhand[ovector] = NULL;
		idt_vec_free (ovector);
		printf("freed vector %x\n", ovector);
	}
	
	apic_set_redir (sc, irq);
}

/*
 * Throw the switch and enable interrupts..
 */

void
ioapic_enable ()
{
	int a, p, maxlevel;
	struct intrhand *q;
	extern void intr_calculatemasks __P((void)); /* XXX */
	int did_imcr = 0;

	intr_calculatemasks();	/* for softints, AST's */

	ioapic_cold = 0;

	lapic_set_lvt();
	
	for (a=0; a<16; a++) {
		struct ioapic_softc *sc = ioapics[a];
		if (sc != NULL) {
			printf("%s: enabling\n", sc->sc_dev.dv_xname);

			if (!did_imcr &&
			    (sc->sc_flags & IOAPIC_PICMODE)) {
				/*
				 * XXX not tested yet..
				 */
				printf("%s: writing to IMCR to disable pics\n",
				    sc->sc_dev.dv_xname);
				outb (IMCR_ADDR, IMCR_REGISTER);
				outb (IMCR_DATA, IMCR_APIC);
				printf("%s: here's hoping it works\n",
				    sc->sc_dev.dv_xname);
				did_imcr = 1;
			}
			
			for (p=0; p<sc->sc_apic_sz; p++) {
				maxlevel = 0;
				
				for (q = sc->sc_pins[p].ip_handler;
				     q != NULL;
				     q = q->ih_next) {
					if (q->ih_level > maxlevel)
						maxlevel = q->ih_level;
				}
				apic_vectorset (sc, p, maxlevel);
			}
		}
	}
}




/*
 * Interrupt handler management with the apic is radically different from the
 * good old 8259.
 *
 * The APIC adds an additional level of indirection between interrupt
 * signals and interrupt vectors in the IDT.
 * It also encodes a priority into the high-order 4 bits of the IDT vector
 * number. 
 *
 *
 * interrupt establishment:
 *	-> locate interrupt pin.
 *	-> locate or allocate vector for pin.
 *	-> locate or allocate handler chain for vector.
 *	-> chain interrupt into handler chain.
 * 	#ifdef notyet
 *	-> if level of handler chain increases, reallocate vector, move chain.
 *	#endif
 */

void *
apic_intr_establish(irq, type, level, ih_fun, ih_arg)
	int irq;
	int type;
	int level;
	int (*ih_fun) __P((void *));
	void *ih_arg;
{
	unsigned int ioapic = APIC_IRQ_APIC(irq);
	unsigned int intr = APIC_IRQ_PIN(irq);
	struct ioapic_softc *sc = ioapics[ioapic];
	struct ioapic_pin *pin;
	struct intrhand **p, *q, *ih;
	static struct intrhand fakehand = {fakeintr};
	extern int cold;
	int maxlevel;

	if (sc == NULL)
		panic("unknown ioapic id %d", ioapic);

	if ((irq & APIC_INT_VIA_APIC) == NULL)
		panic("apic_intr_establish of non-apic interrupt 0x%x", irq);
	
	pin = &sc->sc_pins[intr];
	if (intr >= sc->sc_apic_sz || type == IST_NONE)
		panic("apic_intr_establish: bogus intr or type");
	
	/* no point in sleeping unless someone can free memory. */
	ih = malloc(sizeof *ih, M_DEVBUF, cold ? M_NOWAIT : M_WAITOK);
	if (ih == NULL)
		panic("apic_intr_establish: can't malloc handler info");


	switch (pin->ip_type) {
	case IST_NONE:
		pin->ip_type = type;
		break;
	case IST_EDGE:
	case IST_LEVEL:
		if (type == pin->ip_type)
			break;
	case IST_PULSE:
		if (type != IST_NONE)
			/* XXX should not panic here! */
			panic("apic_intr_establish: intr %d can't share %s with %s",
			      intr,
			      isa_intr_typename(sc->sc_pins[intr].ip_type),
			      isa_intr_typename(type));
		break;
	}

	/*
	 * Figure out where to put the handler.
	 * This is O(N^2) to establish N interrupts, but we want to
	 * preserve the order, and N is generally small.
	 */
	maxlevel = level;
	for (p = &pin->ip_handler; (q = *p) != NULL; p = &q->ih_next) {
		if (q->ih_level > maxlevel)
			maxlevel = q->ih_level;
	}

	/*
	 * Actually install a fake handler momentarily, since we might be doing
	 * this with interrupts enabled and don't want the real routine called
	 * until masking is set up.
	 */
	fakehand.ih_level = level;
	*p = &fakehand;

	/*
	 * fix up the vector for this pin.
	 * XXX perhaps defer this until most interrupts have been established?
	 * (to avoid too much thrashing of the idt..)
	 */

	if (!ioapic_cold)
		apic_vectorset(sc, intr, maxlevel);

#if 0
	apic_calculatemasks();
#endif

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
 * apic disestablish:
 *	locate handler chain.
 * 	dechain intrhand from handler chain
 *	if chain empty {
 *		reprogram apic for "safe" vector.
 *		free vector (point at stray handler).
 *	} 
 *	#ifdef notyet
 *	else {
 *		recompute level for current chain.
 *		if changed, reallocate vector, move chain.
 *	}
 *	#endif
 */

void
apic_intr_disestablish(arg)
	void *arg;
{
	struct intrhand *ih = arg;
	int irq = ih->ih_irq;
	unsigned int ioapic = APIC_IRQ_APIC(irq);
	unsigned int intr = APIC_IRQ_PIN(irq);
	struct ioapic_softc *sc = ioapics[ioapic];
	struct ioapic_pin *pin = &sc->sc_pins[intr];
	struct intrhand **p, *q;
	int maxlevel;
	
	if (intr >= sc->sc_apic_sz)
		panic("apic_intr_establish: bogus irq");

	/*
	 * Remove the handler from the chain.
	 * This is O(n^2), too.
	 */
	maxlevel = 0;
	for (p = &pin->ip_handler; (q = *p) != NULL && q != ih;
	     p = &q->ih_next)
		if (q->ih_level > maxlevel)
			maxlevel = q->ih_level;
		
	if (q)
		*p = q->ih_next;
	else
		panic("intr_disestablish: handler not registered");
	for (; q != NULL; q = q->ih_next)
		if (q->ih_level > maxlevel)
			maxlevel = q->ih_level;
	
	if (!ioapic_cold)
		apic_vectorset(sc, intr, maxlevel);

	free(ih, M_DEVBUF);
}

