/*	$NetBSD: ahc_isa.c,v 1.5.10.2 1997/10/14 09:10:07 thorpej Exp $	*/

/*
 * Product specific probe and attach routines for:
 * 	284X VLbus SCSI controllers
 *
 * Copyright (c) 1994, 1995, 1996 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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
 * Copyright (c) 1995, 1996 Christopher G. Demetriou
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
 *      This product includes software developed by Christopher G. Demetriou
 *      for the NetBSD Project.
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

/*
 * This front-end driver is really sort of a hack.  The AHA-284X likes
 * to masquerade as an EISA device.  However, on VLbus machines with
 * no EISA signature in the BIOS, the EISA bus will never be scanned.
 * This is intended to catch the 284X controllers on those systems
 * by looking in "EISA i/o space" for 284X controllers.
 *
 * This relies heavily on i/o port accounting.  We also just use the
 * EISA macros for everything ... it's a real waste to redefine them.
 *
 * Note: there isn't any #ifdef for FreeBSD in this file, since the
 * FreeBSD EISA driver handles all cases of the 284X.
 *
 *	-- Jason R. Thorpe <thorpej@NetBSD.ORG>
 *	   July 12, 1996
 *
 * TODO: some code could be shared with ahc_eisa.c, but it would probably
 * be a logistical mightmare to even try.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/isa/isavar.h>

#include <dev/eisa/eisareg.h>
#include <dev/eisa/eisavar.h>
#include <dev/eisa/eisadevs.h>

#include <dev/ic/aic7xxxreg.h>
#include <dev/ic/aic7xxxvar.h>

/* IO port address setting range as EISA slot number */
#define AHC_ISA_MIN_SLOT	0x1	/* from iobase = 0x1c00 */
#define AHC_ISA_MAX_SLOT	0xe	/* to   iobase = 0xec00 */

#define AHC_ISA_SLOT_OFFSET	0xc00	/* offset from EISA IO space */
#define AHC_ISA_IOSIZE		0x100

/*
 * I/O port offsets
 */
#define INTDEF			0x5cul	/* Interrupt Definition Register */
#define	AHC_ISA_VID		(EISA_SLOTOFF_VID - AHC_ISA_SLOT_OFFSET)
#define	AHC_ISA_PID		(EISA_SLOTOFF_PID - AHC_ISA_SLOT_OFFSET)
#define	AHC_ISA_PRIMING		AHC_ISA_VID	/* enable vendor/product ID */

/*
 * AHC_ISA_PRIMING register values (write)
 */
#define	AHC_ISA_PRIMING_VID(index)	(AHC_ISA_VID + (index))
#define	AHC_ISA_PRIMING_PID(index)	(AHC_ISA_PID + (index))

int	ahc_isa_irq __P((bus_space_tag_t, bus_space_handle_t));
int	ahc_isa_idstring __P((bus_space_tag_t, bus_space_handle_t, char *));
int	ahc_isa_match __P((struct isa_attach_args *, bus_addr_t));

int	ahc_isa_probe __P((struct device *, void *, void *));
void	ahc_isa_attach __P((struct device *, struct device *, void *));

struct cfattach ahc_isa_ca = {
	sizeof(struct ahc_data), ahc_isa_probe, ahc_isa_attach
};

/*
 * This keeps track of which slots are to be checked next if the
 * iobase locator is a wildcard.  A simple static variable isn't enough,
 * since it's conceivable that a system might have more than one ISA
 * bus.
 *
 * The "bus" member is the unit number of the parent ISA bus, e.g. "0"
 * for "isa0".
 */
struct ahc_isa_slot {
	LIST_ENTRY(ahc_isa_slot)	link;
	int				bus;
	int				slot;
};
static LIST_HEAD(, ahc_isa_slot) ahc_isa_all_slots;
static int ahc_isa_slot_initialized;

/*
 * Return irq setting of the board, otherwise -1.
 */
int
ahc_isa_irq(iot, ioh)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
{
	int irq;
	u_char intdef;

	ahc_reset("ahc_isa", iot, ioh);
	intdef = bus_space_read_1(iot, ioh, INTDEF);
	switch (irq = (intdef & 0xf)) {
	case 9:
	case 10:
	case 11:
	case 12:
	case 14:
	case 15:
		break;
	default:
		printf("ahc_isa_irq: illegal irq setting %d\n", intdef);
		return -1;
	}

	/* Note that we are going and return (to probe) */
	return irq;
}

int
ahc_isa_idstring(iot, ioh, idstring)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	char *idstring;
{
	u_int8_t vid[EISA_NVIDREGS], pid[EISA_NPIDREGS];
	int i;

	/* Get the vendor ID bytes */
	for (i = 0; i < EISA_NVIDREGS; i++) {
		bus_space_write_1(iot, ioh, AHC_ISA_PRIMING,
		    AHC_ISA_PRIMING_VID(i));
		vid[i] = bus_space_read_1(iot, ioh, AHC_ISA_VID + i);
	}

	/* Check for device existence */
	if (EISA_VENDID_NODEV(vid)) {
#if 0
		printf("ahc_isa_idstring: no device at 0x%lx\n",
		    ioh); /* XXX knows about ioh guts */
		printf("\t(0x%x, 0x%x)\n", vid[0], vid[1]);
#endif
		return (0);
	}

	/* And check that the firmware didn't biff something badly */
	if (EISA_VENDID_IDDELAY(vid)) {
		printf("ahc_isa_idstring: BIOS biffed it at 0x%lx\n",
		    ioh);	/* XXX knows about ioh guts */
		return (0);
	}

	/* Get the product ID bytes */
	for (i = 0; i < EISA_NPIDREGS; i++) {
		bus_space_write_1(iot, ioh, AHC_ISA_PRIMING,
		    AHC_ISA_PRIMING_PID(i));
		pid[i] = bus_space_read_1(iot, ioh, AHC_ISA_PID + i);
	}

	/* Create the ID string from the vendor and product IDs */
	idstring[0] = EISA_VENDID_0(vid);
	idstring[1] = EISA_VENDID_1(vid);
	idstring[2] = EISA_VENDID_2(vid);
	idstring[3] = EISA_PRODID_0(pid);
	idstring[4] = EISA_PRODID_1(pid);
	idstring[5] = EISA_PRODID_2(pid);
	idstring[6] = EISA_PRODID_3(pid);
	idstring[7] = '\0';		/* sanity */

	return (1);
}

int
ahc_isa_match(ia, iobase)
	struct isa_attach_args *ia;
	bus_addr_t iobase;
{
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	int irq;
	char idstring[EISA_IDSTRINGLEN];

	/*
	 * Get a mapping for the while slot-specific address
	 * space.  If we can't, assume nothing's there, but
	 * warn about it.
	 */
	if (bus_space_map(iot, iobase, AHC_ISA_IOSIZE, 0, &ioh)) {
#if 0
		/*
		 * Don't print anything out here, since this could
		 * be common on machines configured to look for
		 * ahc_eisa and ahc_isa.
		 */
		printf("ahc_isa_match: can't map I/O space for 0x%x\n",
		    iobase);
#endif
		return (0);
	}

	if (!ahc_isa_idstring(iot, ioh, idstring))
		irq = -1;	/* cannot get the ID string */
	else if (strcmp(idstring, "ADP7756") &&
	    strcmp(idstring, "ADP7757"))
		irq = -1;	/* unknown ID strings */
	else
		irq = ahc_isa_irq(iot, ioh);

	bus_space_unmap(iot, ioh, AHC_ISA_IOSIZE);

	if (irq < 0)
		return (0);

	if (ia->ia_irq != IRQUNK &&
	    ia->ia_irq != irq) {
		printf("ahc_isa_match: irq mismatch (kernel %d, card %d)\n",
		    ia->ia_irq, irq);
		return (0);
	}

	/* We have a match */
	ia->ia_iobase = iobase;
	ia->ia_irq = irq;
	ia->ia_iosize = AHC_ISA_IOSIZE;
	ia->ia_msize = 0;
	return (1);
}

/*
 * Check the slots looking for a board we recognise
 * If we find one, note it's address (slot) and call
 * the actual probe routine to check it out.
 */
int
ahc_isa_probe(parent, match, aux)
        struct device *parent;
        void *match, *aux; 
{       
	struct isa_attach_args *ia = aux;
	struct ahc_isa_slot *as;

	if (ahc_isa_slot_initialized == 0) {
		LIST_INIT(&ahc_isa_all_slots);
		ahc_isa_slot_initialized = 1;
	}

	if (ia->ia_iobase != IOBASEUNK)
		return (ahc_isa_match(ia, ia->ia_iobase));

	/*
	 * Find this bus's state.  If we don't yet have a slot
	 * marker, allocate and initialize one.
	 */
	for (as = ahc_isa_all_slots.lh_first; as != NULL;
	    as = as->link.le_next)
		if (as->bus == parent->dv_unit)
			goto found_slot_marker;

	/*
	 * Don't have one, so make one.
	 */
	as = (struct ahc_isa_slot *)
	    malloc(sizeof(struct ahc_isa_slot), M_DEVBUF, M_NOWAIT);
	if (as == NULL)
		panic("ahc_isa_probe: can't allocate slot marker");

	as->bus = parent->dv_unit;
	as->slot = AHC_ISA_MIN_SLOT;
	LIST_INSERT_HEAD(&ahc_isa_all_slots, as, link);

 found_slot_marker:

	for (; as->slot <= AHC_ISA_MAX_SLOT; as->slot++) {
		if (ahc_isa_match(ia, EISA_SLOT_ADDR(as->slot) +
		    AHC_ISA_SLOT_OFFSET)) {
			as->slot++; /* next slot to search */
			return (1);
		}
	}

	/* No matching cards were found. */
	return (0);
}

void
ahc_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	ahc_type type;
	struct ahc_data *ahc = (void *)self;
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	int irq;
	char idstring[EISA_IDSTRINGLEN];
	const char *model;

	if (bus_space_map(iot, ia->ia_iobase, ia->ia_iosize, 0, &ioh))
		panic("ahc_isa_attach: could not map slot I/O addresses");
	if (!ahc_isa_idstring(iot, ioh, idstring))
		panic("ahc_isa_attach: could not read ID string");
	if ((irq = ahc_isa_irq(iot, ioh)) < 0)
		panic("ahc_isa_attach: ahc_isa_irq failed!");

	if (strcmp(idstring, "ADP7756") == 0) {
		model = EISA_PRODUCT_ADP7756;
		type = AHC_284;
	} else if (strcmp(idstring, "ADP7757") == 0) {
		model = EISA_PRODUCT_ADP7757;
		type = AHC_284;
	} else {
		panic("ahc_isa_attach: Unknown device type %s\n", idstring);
	}
	printf(": %s\n", model);

	ahc_construct(ahc, iot, ioh, type, AHC_FNONE);

#ifdef DEBUG
	/*
	 * Tell the user what type of interrupts we're using.
	 * usefull for debugging irq problems
	 */
	printf( "%s: Using %s Interrupts\n", ahc_name(ahc),
	    ahc->pause & IRQMS ?  "Level Sensitive" : "Edge Triggered");
#endif

	/*
	 * Now that we know we own the resources we need, do the 
	 * card initialization.
	 *
	 * First, the aic7770 card specific setup.
	 */

	/* XXX
	 * On AHA-284x,
	 * all values are automagically intialized at
	 * POST for these cards, so we can always rely
	 * on the Scratch Ram values.  However, we should
	 * read the SEEPROM here (Dan has the code to do
	 * it) so we can say what kind of translation the
	 * BIOS is using.  Printing out the geometry could
	 * save a lot of users the grief of failed installs.
	 */

	/*      
	 * See if we have a Rev E or higher aic7770. Anything below a
	 * Rev E will have a R/O autoflush disable configuration bit.
	 * It's still not clear exactly what is differenent about the Rev E.
	 * We think it allows 8 bit entries in the QOUTFIFO to support
	 * "paging" SCBs so you can have more than 4 commands active at
	 * once.
	 */     
	{
		char *id_string;
		u_char sblkctl;
		u_char sblkctl_orig;

		sblkctl_orig = AHC_INB(ahc, SBLKCTL);
		sblkctl = sblkctl_orig ^ AUTOFLUSHDIS;
		AHC_OUTB(ahc, SBLKCTL, sblkctl);
		sblkctl = AHC_INB(ahc, SBLKCTL);
		if(sblkctl != sblkctl_orig)
		{
			id_string = "aic7770 >= Rev E, ";
			/*
			 * Ensure autoflush is enabled
			 */
			sblkctl &= ~AUTOFLUSHDIS;
			AHC_OUTB(ahc, SBLKCTL, sblkctl);

			/* Allow paging on this adapter */
			ahc->flags |= AHC_PAGESCBS;
		}
		else
			id_string = "aic7770 <= Rev C, ";

		printf("%s: %s", ahc_name(ahc), id_string);
	}

	/* Setup the FIFO threshold and the bus off time */
	{
		u_char hostconf = AHC_INB(ahc, HOSTCONF);
		AHC_OUTB(ahc, BUSSPD, hostconf & DFTHRSH);
		AHC_OUTB(ahc, BUSTIME, (hostconf << 2) & BOFF);
	}

	/*
	 * Generic aic7xxx initialization.
	 */
	if(ahc_init(ahc)){
		ahc_free(ahc);
		return;
	}

	/*
	 * Enable the board's BUS drivers
	 */
	AHC_OUTB(ahc, BCTL, ENABLE);

	/*
	 * The IRQMS bit enables level sensitive interrupts only allow
	 * IRQ sharing if its set.
	 */
	ahc->sc_ih = isa_intr_establish(ia->ia_ic, irq,
	    ahc->pause & IRQMS ? IST_LEVEL : IST_EDGE, IPL_BIO, ahc_intr, ahc);
	if (ahc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt\n",
		       ahc->sc_dev.dv_xname);
		ahc_free(ahc);
		return;
	}

	/* Attach sub-devices - always succeeds */
	ahc_attach(ahc);
}
