/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)pte.h	5.5 (Berkeley) 5/9/91
 *	$Id: pte.h,v 1.4.4.2 1994/10/11 10:03:53 mycroft Exp $
 */

/*
 * 386 page table entry and page table directory
 * W.Jolitz, 8/89
 *
 * There are two major kinds of pte's: those which have ever existed (and are
 * thus either now in core or on the swap device), and those which have
 * never existed, but which will be filled on demand at first reference.
 * There is a structure describing each.  There is also an ancillary
 * structure used in page clustering.
 */

#ifndef _I386_PTE_H_
#define _I386_PTE_H_

#define	PDSHIFT		22		/* LOG2(NBPDR) */
#define	NBPD		(1 << PDSHIFT)	/* bytes/page dir */
#define	PDOFSET		(NBPD-1)	/* byte offset into page dir */
#define	NPTEPD		(NBPD / NBPG)

#ifndef LOCORE
typedef int	pd_entry_t;		/* page directory entry */
typedef int	pt_entry_t;		/* Mach page table entry */
#endif

#define	PD_MASK		0xffc00000	/* page directory address bits */
#define	PT_MASK		0x003ff000	/* page table address bits */

#define	PG_V		0x00000001	/* present */
#define	PG_RO		0x00000000	/* read-only by user (and kernel if 486) */
#define	PG_RW		0x00000002	/* read-write by user */
#define	PG_u		0x00000004	/* accessible by user */
#define	PG_PROT		0x00000006	/* all protection bits */
#define	PG_N		0x00000018	/* non-cacheable */
#define	PG_U		0x00000020	/* has been used */
#define	PG_M		0x00000040	/* has been modified */
#define	PG_W            0x00000200	/* page is wired */
#define	PG_FRAME	0xfffff000	/* page frame mask */

#define	PG_FZERO	0
#define	PG_FTEXT	1
#define	PG_FMAX		(PG_FTEXT)

#define	PG_NOACC	0
#define	PG_KR		0x00000000
#define	PG_KW		0x00000002
#define	PG_URKR		0x00000004
#define	PG_URKW		0x00000004
#define	PG_UW		0x00000006

/*
 * Page Protection Exception bits
 */
#define PGEX_P		0x01	/* Protection violation vs. not present */
#define PGEX_W		0x02	/* during a Write cycle */
#define PGEX_U		0x04	/* access from User mode (UPL) */

#ifndef LOCORE
#ifdef KERNEL
/* utilities defined in pmap.c */
extern	pt_entry_t *Sysmap;
#endif
#endif

#endif /* _I386_PTE_H_ */
