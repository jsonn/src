/*	$NetBSD: pmap.h,v 1.16.2.2 2002/01/10 19:49:23 thorpej Exp $	*/

/*-
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_MACHINE_PMAP_H_
#define	_MACHINE_PMAP_H_

#ifndef _LOCORE
#include <machine/pte.h>
#include <sys/queue.h>
#endif

/*
 * This scheme uses 2-level page tables.
 *
 * While we're still in 32-bit mode we do the following:
 *
 *   offset:						13 bits
 * 1st level: 1024 64-bit TTEs in an 8K page for	10 bits
 * 2nd level: 512 32-bit pointers in the pmap for 	 9 bits
 *							-------
 * total:						32 bits
 *
 * In 64-bit mode the Spitfire and Blackbird CPUs support only
 * 44-bit virtual addresses.  All addresses between 
 * 0x0000 07ff ffff ffff and 0xffff f800 0000 0000 are in the
 * "VA hole" and trap, so we don't have to track them.  However,
 * we do need to keep them in mind during PT walking.  If they
 * ever change the size of the address "hole" we need to rework
 * all the page table handling.
 *
 *   offset:						13 bits
 * 1st level: 1024 64-bit TTEs in an 8K page for	10 bits
 * 2nd level: 1024 64-bit pointers in an 8K page for 	10 bits
 * 3rd level: 1024 64-bit pointers in the segmap for 	10 bits
 *							-------
 * total:						43 bits
 *
 * Of course, this means for 32-bit spaces we always have a (practically)
 * wasted page for the segmap (only one entry used) and half a page wasted
 * for the page directory.  We still have need of one extra bit 8^(.
 */

#define HOLESHIFT	(43)

#define PTSZ	(NBPG/8)
#define PDSZ	(PTSZ)
#define STSZ	(PTSZ)

#define PTSHIFT		(13)
#define	PDSHIFT		(10+PTSHIFT)
#define STSHIFT		(10+PDSHIFT)

#define PTMASK		(PTSZ-1)
#define PDMASK		(PDSZ-1)
#define STMASK		(STSZ-1)

#ifndef _LOCORE

/*
 * Support for big page sizes.  This maps the page size to the
 * page bits.
 */
struct page_size_map {
	u_int64_t mask;
	u_int64_t code;
#ifdef DEBUG
	u_int64_t use;
#endif
};
extern struct page_size_map page_size_map[];

/*
 * Pmap stuff
 */

#define va_to_seg(v)	(int)((((paddr_t)(v))>>STSHIFT)&STMASK)
#define va_to_dir(v)	(int)((((paddr_t)(v))>>PDSHIFT)&PDMASK)
#define va_to_pte(v)	(int)((((paddr_t)(v))>>PTSHIFT)&PTMASK)

struct pmap {
	int pm_ctx;		/* Current context */
	int pm_refs;		/* ref count */
	/* 
	 * This contains 64-bit pointers to pages that contain 
	 * 1024 64-bit pointers to page tables.  All addresses
	 * are physical.  
	 *
	 * !!! Only touch this through pseg_get() and pseg_set() !!!
	 */
	paddr_t pm_physaddr;	/* physical address of pm_segs */
	int64_t *pm_segs;
	struct simplelock pm_lock;
};

/*
 * This comes from the PROM and is used to map prom entries.
 */
struct prom_map {
	u_int64_t	vstart;
	u_int64_t	vsize;
	u_int64_t	tte;
};

#define PMAP_NC		0x001	/* Set the E bit in the page */
#define PMAP_NVC	0x002	/* Don't enable the virtual cache */
#define PMAP_LITTLE	0x004	/* Map in little endian mode */
/* Large page size hints -- we really should use another param to pmap_enter() */
#define PMAP_8K		0x000
#define PMAP_64K	0x008	/* Use 64K page */
#define PMAP_512K	0x010
#define PMAP_4M		0x018
#define PMAP_SZ_TO_TTE(x)	(((x)&0x018)<<58)
/* If these bits are different in va's to the same PA then there is an aliasing in the d$ */
#define VA_ALIAS_MASK   (1<<14)	

typedef	struct pmap *pmap_t;

/* 
 * Encode IO space for pmap_enter() 
 *
 * Since sun4u machines don't have separate IO spaces, this is a noop.
 */
#define PMAP_IOENC(io)	0

#ifdef	_KERNEL
extern struct pmap kernel_pmap_;
#define	pmap_kernel()	(&kernel_pmap_)

int pmap_count_res __P((pmap_t pmap));
int pmap_count_wired __P((pmap_t pmap));
#define	pmap_resident_count(pm)		pmap_count_res((pm))
#define	pmap_wired_count(pm)		pmap_count_wired((pm))
#define	pmap_from_phys_address(x,f)	((x)&~PGOFSET)
#define	pmap_phys_address(x)		(x)
#define	pmap_update(pmap)		/* nothing (yet) */

void pmap_bootstrap __P((u_long kernelstart, u_long kernelend, u_int numctx));
/* make sure all page mappings are modulo 16K to prevent d$ aliasing */
#define	PMAP_PREFER(pa, va)	(*(va)+=(((*(va))^(pa))&(1<<(PGSHIFT+1))))

#define	PMAP_GROWKERNEL         /* turn on pmap_growkernel interface */

/* SPARC specific? */
void		pmap_redzone __P((void));
int             pmap_dumpsize __P((void));
int             pmap_dumpmmu __P((int (*)__P((dev_t, daddr_t, caddr_t, size_t)),
                                 daddr_t));
int		pmap_pa_exists __P((paddr_t));
struct proc;
void		switchexit __P((struct proc *));

/* SPARC64 specific */
int	ctx_alloc __P((struct pmap*));
void	ctx_free __P((struct pmap*));


#endif	/* _KERNEL */

/* This is only for compatibility with the SPARC */ 
struct segmap {
	int	*sg_pte;		/* points to NPTESG PTEs */
	pmeg_t	sg_pmeg;		/* the MMU segment number (4c) */
	u_char	sg_npte;		/* number of valid PTEs per seg */
};

#endif	/* _LOCORE */
#endif	/* _MACHINE_PMAP_H_ */
