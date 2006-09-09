/*	$NetBSD: vmparam.h,v 1.16.32.1 2006/09/09 02:42:59 rpaulo Exp $	*/

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

#ifndef _SH3_VMPARAM_H_
#define	_SH3_VMPARAM_H_
#include <sys/queue.h>

/*
 * We use 4K pages on the sh3/sh4.  Override the PAGE_* definitions
 * to be compile-time constants.
 */
#define	PAGE_SHIFT	12
#define	PAGE_SIZE	(1 << PAGE_SHIFT)
#define	PAGE_MASK	(PAGE_SIZE - 1)

/* Virtual address map. */
#define	VM_MIN_ADDRESS		((vaddr_t)0)
#define	VM_MAXUSER_ADDRESS	((vaddr_t)0x7ffff000)
#define	VM_MAX_ADDRESS		((vaddr_t)0x7ffff000)
#define	VM_MIN_KERNEL_ADDRESS	((vaddr_t)0xc0000000)
#define	VM_MAX_KERNEL_ADDRESS	((vaddr_t)0xe0000000)

/* top of stack */
#define	USRSTACK		VM_MAXUSER_ADDRESS

/* Virtual memory resoruce limit. */
#define	MAXTSIZ			(64 * 1024 * 1024)	/* max text size */
#ifndef MAXDSIZ
#define	MAXDSIZ			(512 * 1024 * 1024)	/* max data size */
#endif
#ifndef	MAXSSIZ
#define	MAXSSIZ			(32 * 1024 * 1024)	/* max stack size */
#endif

/* initial data size limit */
#ifndef DFLDSIZ
#define	DFLDSIZ			(128 * 1024 * 1024)
#endif
/* initial stack size limit */
#ifndef	DFLSSIZ
#define	DFLSSIZ			(2 * 1024 * 1024)
#endif

/*
 * Size of shared memory map
 */
#ifndef SHMMAXPGS
#define	SHMMAXPGS		1024
#endif

/* Size of user raw I/O map */
#ifndef USRIOSIZE
#define	USRIOSIZE		(MAXBSIZE / PAGE_SIZE * 8)
#endif

#define	VM_PHYS_SIZE		(USRIOSIZE * PAGE_SIZE)

/* Physical memory segments */
#define	VM_PHYSSEG_STRAT	VM_PSTRAT_BSEARCH
#define	VM_PHYSSEG_NOADD

#define	sh3_round_page(x)	((((uint32_t)(x)) + PGOFSET) & ~PGOFSET)
#define	sh3_trunc_page(x)	((uint32_t)(x) & ~PGOFSET)
#define	sh3_btop(x)		((uint32_t)(x) >> PGSHIFT)
#define	sh3_ptob(x)		((uint32_t)(x) << PGSHIFT)

/* pmap-specific data store in the vm_page structure. */
#define	__HAVE_VM_PAGE_MD
#define	PVH_REFERENCED		1
#define	PVH_MODIFIED		2

#ifndef _LOCORE
struct pv_entry;
struct vm_page_md {
	SLIST_HEAD(, pv_entry) pvh_head;
	int pvh_flags;
};

#define	VM_MDPAGE_INIT(pg)						\
do {									\
	struct vm_page_md *pvh = &(pg)->mdpage;				\
	SLIST_INIT(&pvh->pvh_head);					\
	pvh->pvh_flags = 0;						\
} while (/*CONSTCOND*/0)
#endif /* _LOCORE */
#endif /* !_SH3_VMPARAM_H_ */
