/*	$NetBSD: pte.h,v 1.18.4.1 2001/09/13 01:15:06 thorpej Exp $	  */

/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
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
 *     This product includes software developed at Ludd, University of Lule}.
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

#ifndef _VAX_PTE_H_
#define _VAX_PTE_H_

#ifndef _LOCORE
/*
 * VAX page table entries
 */
struct pte {
	unsigned int	pg_pfn:21;	/* Page Frame Number or 0 */
	unsigned int	pg_illegal:2;	/* Don't use this bits */
	unsigned int	pg_sref:1;	/* Help for ref simulation */
	unsigned int	pg_w:1;		/* Wired bit */
	unsigned int	pg_z:1;		/* Zero DIGITAL = 0 */
	unsigned int	pg_m:1;		/* Modify DIGITAL */
	unsigned int	pg_prot:4;	/* reserved at zero */
	unsigned int	pg_v:1;		/* valid bit */
};


typedef struct pte	pt_entry_t;	/* Mach page table entry */

#endif /* _LOCORE */

#define PT_ENTRY_NULL	((pt_entry_t *) 0)

#define PG_V		0x80000000
#define PG_NV		0x00000000
#define PG_PROT		0x78000000
#define PG_RW		0x20000000
#define PG_KW		0x10000000
#define PG_KR		0x18000000
#define PG_URKW		0x70000000
#define PG_RO		0x78000000
#define PG_NONE		0x00000000
#define PG_M		0x04000000
#define PG_W		0x01000000
#define PG_SREF		0x00800000
#define PG_ILLEGAL	0x00600000
#define PG_FRAME	0x001fffff
#define PG_PFNUM(x)	(((unsigned long)(x) & 0x3ffffe00) >> VAX_PGSHIFT)

#ifndef _LOCORE
extern pt_entry_t *Sysmap;
/*
 * Kernel virtual address to page table entry and to physical address.
 */
#endif

#ifdef __ELF__
#define VAX_SYSMAP	"Sysmap"
#else
#define VAX_SYSMAP	"_Sysmap"
#endif

#ifdef __GNUC__
#define kvtopte(va) ({ \
	struct pte *r; \
	asm("extzv $9,$21,%1,%0;moval *" VAX_SYSMAP "[%0],%0" : "=r"(r) : "g"(va)); \
	r; \
})
#define kvtophys(va) ({ \
	paddr_t r; \
	asm("extzv $9,$21,%1,%0;ashl $9,*" VAX_SYSMAP "[%0],%0;insv %1,$0,$9,%0" \
	    : "=&r"(r) : "g"(va) : "cc"); \
	r; \
})
#else /* __GNUC__ */
#define kvtophys(va) \
	(((kvtopte(va))->pg_pfn << VAX_PGSHIFT) | ((paddr_t)(va) & VAX_PGOFSET))
#define kvtopte(va) (&Sysmap[PG_PFNUM(va)])
#endif /* __GNUC__ */
#define uvtopte(va, pcb) \
	(((vaddr_t)va < 0x40000000) ? \
	&(((pcb)->P0BR)[PG_PFNUM(va)]) : \
	&(((pcb)->P1BR)[PG_PFNUM(va)]))

#endif
