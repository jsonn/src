/*
 * Copyright (c) 1994 Gordon W. Ross
 * Copyright (c) 1993 Adam Glass
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
 *	This product includes software developed by Adam Glass and Gordon Ross.
 * 4. The name of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id: obio.c,v 1.11.2.2 1994/09/20 16:24:46 gwr Exp $
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/obio.h>
#include <machine/pte.h>
#include <machine/param.h>
#include <machine/mon.h>
#include <machine/isr.h>

extern void obioattach __P((struct device *, struct device *, void *));

struct obio_softc {
	struct device obio_dev;
};
	
struct cfdriver obiocd = {
	NULL, "obio", always_match, obioattach, DV_DULL,
	sizeof(struct obio_softc), 0 };

void obio_print(addr, level)
	int addr, level;
{
	printf(" addr 0x%x", addr);
	if (level >= 0)
		printf(" level %d", level);
}

void obioattach(parent, self, args)
	struct device *parent;
	struct device *self;
	void *args;
{
	struct cfdata *new_match;
	
	printf("\n");
	while (1) {
		new_match = config_search(NULL, self, NULL);
		if (!new_match) break;
		config_attach(self, new_match, NULL, NULL);
	}
}


/*
 * Spacing of "interesting" OBIO mappings.  We will
 * record only those with an OBIO address that is a
 * multiple of SAVE_INCR and below SAVE_LAST.
 * The saved mappings are just one page each, which
 * is good enough for all the devices that use this.
 */
#define SAVE_SHIFT 17
#define SAVE_INCR (1<<SAVE_SHIFT)
#define SAVE_MASK (SAVE_INCR-1)
#define SAVE_SLOTS  16
#define SAVE_LAST (SAVE_SLOTS * SAVE_INCR)

/*
 * This is our record of "interesting" OBIO mappings that
 * the PROM has left in the virtual space reserved for it.
 * Each non-null array element holds the virtual address
 * of an OBIO mapping where the OBIO address mapped is:
 *     (array_index * SAVE_INCR)
 * and the length of the mapping is one page.
 */
static caddr_t prom_mappings[SAVE_SLOTS];

caddr_t obio_find_mapping(int pa, int size)
{
	if ((size <= NBPG) &&
		(pa < SAVE_LAST) &&
		((pa & SAVE_MASK) == 0))
	{
		return prom_mappings[pa >> SAVE_SHIFT];
	}
	return (caddr_t)0;
}

/*
 * This defines the permission bits to put in our PTEs.
 * Device space is never cached, and the PROM appears to
 * leave off the "no-cache" bit, so we can do the same.
 */
#define PGBITS (PG_VALID|PG_WRITE|PG_SYSTEM)

static void save_prom_mappings()
{
	vm_offset_t pa;
	caddr_t segva, pgva;
	int pte, sme, i;
	
	segva = (caddr_t)MONSTART;
	while (segva < (caddr_t)MONEND) {
		sme = get_segmap(segva);
		if (sme == SEGINV) {
			segva += NBSG;
			continue;			/* next segment */
		}
		/*
		 * We have a valid segmap entry, so examine the
		 * PTEs for all the pages in this segment.
		 */
		pgva = segva;	/* starting page */
		segva += NBSG;	/* ending page (next seg) */
		while (pgva < segva) {
			pte = get_pte(pgva);
			if ((pte & (PG_VALID | PG_TYPE)) ==
				(PG_VALID | MAKE_PGTYPE(PG_OBIO)))
			{
				/* Have a valid OBIO mapping. */
				pa = PG_PA(pte);
				/* Is it one we want to record? */
				if ((pa < SAVE_LAST) &&
					((pa & SAVE_MASK) == 0))
				{
					i = pa >> SAVE_SHIFT;
					if (prom_mappings[i] == NULL) {
						prom_mappings[i] = pgva;
#ifdef	DEBUG
						mon_printf("obio: found pa=0x%x\n", pa);
#endif
					}
				}
				/* Make sure it has the right permissions. */
				if ((pte & PGBITS) != PGBITS) {
#ifdef	DEBUG
					mon_printf("obio: fixing pte=0x%x\n", pte);
#endif
					pte |= PGBITS;
					set_pte(pgva, pte);
				}
			}
			pgva += NBPG;		/* next page */
		}
	}
}

/*
 * These are all the OBIO address that are required early
 * in the life of the kernel.  All are less one page long.
 */
static vm_offset_t required_mappings[] = {
	/* Basically the first six OBIO devices. */
	OBIO_KEYBD_MS,
	OBIO_ZS,
	OBIO_EEPROM,
	OBIO_CLOCK,
	OBIO_MEMERR,
	OBIO_INTERREG,
	(vm_offset_t)-1,	/* end marker */
};

static void make_required_mappings()
{
	vm_offset_t pa, *rmp;
	int idx;
	
	rmp = required_mappings;
	while (*rmp != (vm_offset_t)-1) {
		if (!obio_find_mapping(*rmp, NBPG)) {
			/*
			 * XXX - Ack! Need to create one!
			 * I don't think this can happen, but if
			 * it does, we can allocate a PMEG in the
			 * "high segment" and add it there. -gwr
			 */
			mon_printf("obio: no mapping for 0x%x\n", *rmp);
			mon_panic("obio: Ancient PROM?\n");
		}
		rmp++;
	}
}


/*
 * this routine "configures" any internal OBIO devices which must be
 * accessible before the mainline OBIO autoconfiguration as part of
 * configure().
 */
void obio_init()
{
	save_prom_mappings();
	make_required_mappings();
}

caddr_t obio_alloc(obio_addr, obio_size)
	int obio_addr, obio_size;
{
	int npages;
	vm_offset_t va, high_segment_alloc(), obio_pa, obio_va, pte_proto;
	caddr_t cp;
	int obio_flags = OBIO_WRITE;
	
	cp = obio_find_mapping((vm_offset_t)obio_addr, obio_size);
	if (cp) return (cp);
	
	npages = PA_PGNUM(sun3_round_page(obio_size));
	if (!npages)
		panic("obio_alloc: attempt to allocate 0 pages for obio");
	va = high_segment_alloc(npages);
	if (!va)
		va = (vm_offset_t) obio_vm_alloc(npages);
	if (!va) 
		panic("obio_alloc: unable to allocate va for obio mapping");
	pte_proto = PG_VALID|PG_SYSTEM|MAKE_PGTYPE(PG_OBIO);
	if ((obio_flags & OBIO_CACHE) == 0)
		pte_proto |= PG_NC;
	if (obio_flags & OBIO_WRITE)
		pte_proto |= PG_WRITE;
	obio_va = va;
	obio_pa = (vm_offset_t) obio_addr;
	for (; npages ; npages--, obio_va += NBPG, obio_pa += NBPG)
		set_pte(obio_va, pte_proto | PA_PGNUM(obio_pa));
	return (caddr_t) va;
}

int
obio_probe_byte(oba)
	caddr_t oba;	/* OBIO address to probe */
{
	/* XXX - Not yet... */
	return 0;
}
