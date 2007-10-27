/*	$NetBSD: vmparam.h,v 1.58.10.4 2007/10/27 11:26:46 yamt Exp $	*/

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
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)vmparam.h	5.9 (Berkeley) 5/12/91
 */

#ifndef _VMPARAM_H_
#define _VMPARAM_H_

#include <sys/tree.h>

/*
 * Machine dependent constants for 386.
 */

/*
 * Page size on the IA-32 is not variable in the traditional sense.
 * We override the PAGE_* definitions to compile-time constants.
 */
#define	PAGE_SHIFT	12
#define	PAGE_SIZE	(1 << PAGE_SHIFT)
#define	PAGE_MASK	(PAGE_SIZE - 1)

/*
 * Virtual address space arrangement. On 386, both user and kernel
 * share the address space, not unlike the vax.
 * USRSTACK is the top (end) of the user stack. Immediately above the
 * user stack is the page table map, and then kernel address space.
 */
#define	USRSTACK	VM_MAXUSER_ADDRESS

/*
 * Virtual memory related constants, all in bytes
 */
#define	MAXTSIZ		(64*1024*1024)		/* max text size */
#ifndef DFLDSIZ
#define	DFLDSIZ		(256*1024*1024)		/* initial data size limit */
#endif
#ifndef MAXDSIZ
#define	MAXDSIZ		(3U*1024*1024*1024)	/* max data size */
#endif
#ifndef	DFLSSIZ
#define	DFLSSIZ		(2*1024*1024)		/* initial stack size limit */
#endif
#ifndef	MAXSSIZ
#define	MAXSSIZ		(64*1024*1024)		/* max stack size */
#endif

/*
 * IA-32 can't do per-page execute permission, so instead we implement
 * two executable segments for %cs, one that covers everything and one
 * that excludes some of the address space (currently just the stack).
 * I386_MAX_EXE_ADDR is the upper boundary for the smaller segment.
 */
#define I386_MAX_EXE_ADDR	(USRSTACK - MAXSSIZ)

/*
 * Size of shared memory map
 */
#ifndef SHMMAXPGS
#define SHMMAXPGS	2048
#endif

/*
 * Size of User Raw I/O map
 */
#define	USRIOSIZE 	300

/*
 * Mach derived constants
 */

/* user/kernel map constants */
#define VM_MIN_ADDRESS		((vaddr_t)0)
#define	VM_MAXUSER_ADDRESS	((vaddr_t)(PDIR_SLOT_PTE << L2_SHIFT))
#define	VM_MAX_ADDRESS		\
	((vaddr_t)((PDIR_SLOT_PTE << L2_SHIFT) + (PDIR_SLOT_PTE << L1_SHIFT)))
#define	VM_MIN_KERNEL_ADDRESS	((vaddr_t)(PDIR_SLOT_KERN << L2_SHIFT))
#define	VM_MAX_KERNEL_ADDRESS	((vaddr_t)(PDIR_SLOT_APTE << L2_SHIFT))

/*
 * The address to which unspecified mapping requests default
 */
#ifdef _KERNEL_OPT
#include "opt_uvm.h"
#endif
#define __USE_TOPDOWN_VM
#define VM_DEFAULT_ADDRESS(da, sz) \
	trunc_page(USRSTACK - MAXSSIZ - (sz))

/* XXX max. amount of KVM to be used by buffers. */
#ifndef VM_MAX_KERNEL_BUF
#define VM_MAX_KERNEL_BUF	(384 * 1024 * 1024)
#endif

/* virtual sizes (bytes) for various kernel submaps */
#define VM_PHYS_SIZE		(USRIOSIZE*PAGE_SIZE)

#define VM_PHYSSEG_MAX		10	/* 1 "hole" + 9 free lists */
#define VM_PHYSSEG_STRAT	VM_PSTRAT_BIGFIRST
#define VM_PHYSSEG_NOADD		/* can't add RAM after vm_mem_init */

#define	VM_NFREELIST		2
#define	VM_FREELIST_DEFAULT	0
#define	VM_FREELIST_FIRST16	1

#define	__HAVE_VM_PAGE_MD
#define	VM_MDPAGE_INIT(pg)							\
	memset(&(pg)->mdpage, 0, sizeof((pg)->mdpage));				\
	mutex_init(&(pg)->mdpage.mp_pvhead.pvh_lock, MUTEX_NODEBUG, IPL_VM);	\
	SPLAY_INIT(&(pg)->mdpage.mp_pvhead.pvh_root);

struct pv_entry;

struct pv_head {
	kmutex_t pvh_lock;		/* locks every pv in this tree */
	SPLAY_HEAD(pvtree, pv_entry) pvh_root;
					/* head of tree (locked by pvh_lock) */
};

struct vm_page_md {
	struct pv_head mp_pvhead;
	struct vm_page *mp_link;
	int mp_attrs;	/* only 2 bits (PG_U and PG_M) are actually used. */
};

#endif /* _VMPARAM_H_ */
