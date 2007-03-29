/*	$NetBSD: pmap.h,v 1.30.8.1 2007/03/29 19:27:31 reinoud Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
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
 * NetBSD/sh3 pmap:
 *	pmap.pm_ptp[512] ... 512 slot of page table page
 *	page table page contains 1024 PTEs. (PAGE_SIZE / sizeof(pt_entry_t))
 *	va -> [ PTP 10bit | PTOFSET 10bit | PGOFSET 12bit ]
 */

#ifndef _SH3_PMAP_H_
#define	_SH3_PMAP_H_
#include <sys/queue.h>
#include <sh3/pte.h>

#define	PMAP_NEED_PROCWR
#define	PMAP_STEAL_MEMORY
#define	PMAP_GROWKERNEL

#define	__PMAP_PTP_N	512	/* # of page table page maps 2GB. */
typedef struct pmap {
	pt_entry_t **pm_ptp;
	int pm_asid;
	int pm_refcnt;
	struct pmap_statistics	pm_stats;	/* pmap statistics */
} *pmap_t;
extern struct pmap __pmap_kernel;

void pmap_bootstrap(void);
void pmap_procwr(struct proc *, vaddr_t, size_t);
#define	pmap_kernel()			(&__pmap_kernel)
#define	pmap_update(pmap)		((void)0)
#define	pmap_copy(dp,sp,d,l,s)		((void)0)
#define	pmap_collect(pmap)		((void)0)
#define	pmap_wired_count(pmap)		((pmap)->pm_stats.wired_count)
#define	pmap_resident_count(pmap)	((pmap)->pm_stats.resident_count)

/* ARGSUSED */
static __inline void
pmap_remove_all(struct pmap *pmap)
{
	/* Nothing. */
}

/*
 * pmap_prefer() helps to avoid virtual cache aliases on SH4 CPUs
 * which have the virtually-indexed cache.
 */
#ifdef SH4
#define PMAP_PREFER(pa, va, sz, td)     pmap_prefer((pa), (va))
void pmap_prefer(vaddr_t, vaddr_t *);
#endif /* SH4 */

#define	PMAP_MAP_POOLPAGE(pa)		SH3_PHYS_TO_P1SEG((pa))
#define	PMAP_UNMAP_POOLPAGE(va)		SH3_P1SEG_TO_PHYS((va))

/* MD pmap utils. */
pt_entry_t *__pmap_pte_lookup(pmap_t, vaddr_t);
pt_entry_t *__pmap_kpte_lookup(vaddr_t);
bool __pmap_pte_load(pmap_t, vaddr_t, int);
#endif /* !_SH3_PMAP_H_ */
