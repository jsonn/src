/*	$NetBSD: pmap.h,v 1.2.2.4 1998/08/09 05:46:35 eeh Exp $	*/

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
 * In 64-bit mode this should be changed as follows
 *
 *   offset:						13 bits
 * 1st level: 1024 64-bit TTEs in an 8K page for	10 bits
 * 2nd level: 1024 64-bit pointers in an 8K page for 	10 bits
 * 3rd level: 128 64-bit pointers in the pmap for 	 7 bits
 *							-------
 * total:						40 bits
 */

#define PTSZ	(NBPG/8)
#define STSZ	(1<<9)

#define PTSHIFT		(13)
#define STSHIFT		(10+PTSHIFT)

#define PTMASK		(PTSZ-1)
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

#define va_to_seg(v)	(((v)>>STSHIFT)&STMASK)
#define va_to_pte(v)	(((v)>>PTSHIFT)&PTMASK)

/* typedef union sun4u_data ptab[PTSZ]; */

struct pmap {
	int pm_ctx;		/* Current context */
	int pm_refs;		/* ref count */
	paddr_t pm_physaddr;	/* physical address of pm_segs */
	union sun4u_data* pm_segs[STSZ];
};

/*
 * This comes from the PROM and is used to map prom entries.
 */
struct prom_map {
	u_int64_t	vstart;
	u_int64_t	vsize;
/*	struct sun4u_data	tte;*/
	u_int64_t	tte;
};

#define PMAP_NC		1	/* Set the E bit in the page */
#define PMAP_NVC	2	/* Don't enable the virtual cache */
#define PMAP_LITTLE	3	/* Map in little endian mode */
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
/* int pmap_change_wiring __P((pmap_t pm, vaddr_t va, boolean_t wired)); */
#define pmap_resident_count(pm)		pmap_count_res((pm))
#define	pmap_phys_address(x)		((((paddr_t)(x))<<PGSHIFT)|PMAP_NC)

void pmap_bootstrap __P((u_int kernelstart, u_int kernelend, u_int numctx));

/* This needs to be implemented when we get a kernel map */
void pmap_changeprot __P((pmap_t pmap, vaddr_t start, vm_prot_t prot, int size));

/* SPARC specific? */
void		pmap_redzone __P((void));
int             pmap_dumpsize __P((void));
int             pmap_dumpmmu __P((int (*)__P((dev_t, daddr_t, caddr_t, size_t)),
                                 daddr_t));
int		pmap_pa_exists __P((paddr_t));
struct user;
void		switchexit __P((vm_map_t, struct user *, int));

/* SPARC64 specific */
int	ctx_alloc __P((struct pmap*));
void	ctx_free __P((struct pmap*));
void	pmap_enter_phys __P((pmap_t, vaddr_t, u_int64_t, u_int64_t, vm_prot_t, boolean_t));


#endif	/* _KERNEL */
#endif	/* _LOCORE */
#endif	/* _MACHINE_PMAP_H_ */
