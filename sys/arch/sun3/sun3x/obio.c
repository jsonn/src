/*	$NetBSD: obio.c,v 1.12.4.2 2002/10/10 18:37:11 jdolecek Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass and Gordon W. Ross.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/mon.h>
#include <machine/pte.h>

#include <sun3/sun3/machdep.h>
#include <sun3/sun3x/obio.h>

static int  obio_match __P((struct device *, struct cfdata *, void *));
static void obio_attach __P((struct device *, struct device *, void *));
static int  obio_print __P((void *, const char *parentname));
static int	obio_submatch __P((struct device *, struct cfdata *, void *));

CFATTACH_DECL(obio, sizeof(struct device),
    obio_match, obio_attach, NULL, NULL);

static int
obio_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct confargs *ca = aux;

	if (ca->ca_bustype != BUS_OBIO)
		return (0);
	return(1);
}

/*
 * We need control over the order of attachment on OBIO,
 * so do "direct" style autoconfiguration with addresses
 * from the list below.  OBIO addresses are fixed forever.
 *
 * Warning: This whole list is very carefully ordered!
 * In general, anything not already shown here should
 * be added at or near the end.
 */
static int obio_alist[] = {

	/* This is used by the Ethernet and SCSI drivers. */
	OBIO_IOMMU,

	/* Misc. registers - needed by many things */
	OBIO_ENABLEREG,
	OBIO_BUSERRREG,
	OBIO_DIAGREG,	/* leds.c */
	OBIO_IDPROM1,	/* idprom.c (3/470) */
	OBIO_MEMREG,	/* memerr.c */
	OBIO_INTERREG,	/* intreg.c */

	/* Zilog UARTs */
	OBIO_ZS_KBD_MS,
	OBIO_ZS_TTY_AB,

	/* eeprom.c */
	OBIO_EEPROM,

	/* Note: This must come after OBIO_IDPROM1. */
	OBIO_IDPROM2,	/* idprom.c (3/80) */

	/* Note: Must probe for the Intersil first! */
	OBIO_CLOCK1,	/* clock.c (3/470) */
	OBIO_CLOCK2,	/* clock.c (3/80) */

	OBIO_INTEL_ETHER,
	OBIO_LANCE_ETHER,

	/* Need esp DMA before SCSI. */
	OBIO_EMULEX_DMA,  /* 3/80 only */
	OBIO_EMULEX_SCSI, /* 3/80 only */

	/* Memory subsystem */
	OBIO_PCACHE_TAGS,
	OBIO_ECCPARREG,
	OBIO_IOC_TAGS,
	OBIO_IOC_FLUSH,

	OBIO_FDC,	/* floppy disk (3/80) */
	OBIO_PRINTER_PORT, /* printer port (3/80) */
};
#define OBIO_ALIST_LEN (sizeof(obio_alist) / \
                        sizeof(obio_alist[0]))

static void
obio_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct confargs *ca = aux;
	int	i;

	printf("\n");

	/* Configure these in the order listed above. */
	for (i = 0; i < OBIO_ALIST_LEN; i++) {
		/* Our parent set ca->ca_bustype already. */
		ca->ca_paddr = obio_alist[i];
		/* These are filled-in by obio_submatch. */
		ca->ca_intpri = -1;
		ca->ca_intvec = -1;
		(void) config_found_sm(self, ca, obio_print, obio_submatch);
	}
}

/*
 * Print out the confargs.  The (parent) name is non-NULL
 * when there was no match found by config_found().
 */
static int
obio_print(args, name)
	void *args;
	const char *name;
{

	/* Be quiet about empty OBIO locations. */
	if (name)
		return(QUIET);

	/* Otherwise do the usual. */
	return(bus_print(args, name));
}

int
obio_submatch(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct confargs *ca = aux;

	/*
	 * Note that a defaulted address locator can never match
	 * the value of ca->ca_paddr set by the obio_attach loop.
	 * Without this diagnostic, any device with a defaulted
	 * address locator would always be silently unmatched.
	 * Therefore, just disallow default addresses on OBIO.
	 */
#ifdef	DIAGNOSTIC
	if (cf->cf_paddr == -1)
		panic("obio_submatch: invalid address for: %s%d",
			cf->cf_name, cf->cf_unit);
#endif

	/*
	 * Note that obio_attach calls config_found_sm() with
	 * this function as the "submatch" and ca->ca_paddr
	 * set to each of the possible OBIO locations, so we
	 * want to reject any unmatched address here.
	 */
	if (cf->cf_paddr != ca->ca_paddr)
		return 0;

	/*
	 * Copy the locators into our confargs for the child.
	 * Note: ca->ca_bustype was set by our parent driver
	 * (mainbus) and ca->ca_paddr was set by obio_attach.
	 */
	ca->ca_intpri = cf->cf_intpri;
	ca->ca_intvec = cf->cf_intvec;

	/* Now call the match function of the potential child. */
	return (config_match(parent, cf, aux));
}


/*****************************************************************/

/*
 * This is our record of "interesting" OBIO mappings that
 * the PROM has left in the virtual space reserved for it.
 * Each row of the array holds a virtual address and the
 * physical address it maps to (if found).
 */
static struct prom_map {
	paddr_t pa;
	vaddr_t va;
} prom_mappings[] = {
	{ OBIO_ENABLEREG, 0 },	/* regs: Sys ENA, Bus ERR, etc. */
	{ OBIO_ZS_KBD_MS, 0 },	/* Keyboard and Mouse */
	{ OBIO_ZS_TTY_AB, 0 },	/* Serial Ports */
	{ OBIO_EEPROM,    0 },	/* EEPROM/IDPROM/clock */
};
#define PROM_MAP_CNT (sizeof(prom_mappings) / \
		      sizeof(prom_mappings[0]))

/*
 * Find a virtual address for a device at physical address 'pa'.
 * If one is found among the mappings already made by the PROM
 * at power-up time, use it.  Otherwise return 0 as a sign that
 * a mapping will have to be created.
 */
caddr_t
obio_find_mapping(paddr_t pa, psize_t sz)
{
	int i;
	vsize_t off;

	off = pa & PGOFSET;
	pa -= off;
	sz += off;

	/* The saved mappings are all one page long. */
	if (sz > NBPG)
		return (caddr_t)0;

	/* Linear search for it.  The list is short. */
	for (i = 0; i < PROM_MAP_CNT; i++) {
		if (pa == prom_mappings[i].pa) {
			return ((caddr_t)(prom_mappings[i].va + off));
		}
	}
	return (caddr_t)0;
}

/*
 * Search the PROM page tables for OBIO mappings that
 * we would like to borrow.
 */
static void
save_prom_mappings __P((void))
{
	int *mon_pte;
	vaddr_t va;
	paddr_t pa;
	int i;

	/* Note: mon_ctbl[0] maps SUN3X_MON_KDB_BASE */
	mon_pte = *romVectorPtr->monptaddr;

	for (va = SUN3X_MON_KDB_BASE; va < SUN3X_MONEND;
		 va += NBPG, mon_pte++)
	{
		/* Is this a valid mapping to OBIO? */
		/* XXX - Some macros would be nice... */
		if ((*mon_pte & 0xF0000003) != 0x60000001)
			continue;

		/* Yes it is.  Is this a mapping we want? */
		pa = *mon_pte & MMU_SHORT_PTE_BASEADDR;
		for (i = 0; i < PROM_MAP_CNT; i++) {
			if (pa != prom_mappings[i].pa)
				continue;
			/* Yes, we want it.  Save the va? */
			if (prom_mappings[i].va == 0) {
				prom_mappings[i].va = va;
			}
		}
	}

}

/*
 * These are all the OBIO address that are required early in
 * the life of the kernel.  All are less than one page long.
 * This function should make any required mappings that we
 * were not able to find among the PROM monitor's mappings.
 */
static void
make_required_mappings __P((void))
{
	int i;

	for (i = 0; i < PROM_MAP_CNT; i++) {
		if (prom_mappings[i].va == 0) {
			/*
			 * Actually, the PROM always has all the
			 * "required" mappings we need, (smile)
			 * but this makes sure that is true.
			 */
			mon_printf("obio: no mapping for pa=0x%x\n",
			    prom_mappings[i].pa);
			sunmon_abort();  /* Ancient PROM? */
		}
	}
}


/*
 * Find mappings for devices that are needed before autoconfiguration.
 * We first look for and record any useful PROM mappings, then call
 * the "init" functions for drivers that we need to use before the
 * normal autoconfiguration calls configure().  Warning: this is
 * called before pmap_bootstrap, so no allocation allowed!
 */
void
obio_init()
{
	save_prom_mappings();
	make_required_mappings();

	enable_init();

	/*
	 * Find the interrupt reg mapping and turn off the
	 * interrupts, otherwise the PROM clock interrupt
	 * would poll the zs and toggle some LEDs...
	 */
	intreg_init();
}
