/*	$NetBSD: vmparam.h,v 1.19.54.1.4.1 2007/11/10 02:56:43 matt Exp $	*/

/*
 * Copyright (c) 2001, 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _ARM_ARM32_VMPARAM_H_
#define	_ARM_ARM32_VMPARAM_H_

#ifdef _KERNEL

/*
 * Virtual Memory parameters common to all arm32 platforms.
 */

#ifndef __ASSEMBLER__
#include <sys/lock.h>		/* struct simplelock */ 
#endif /* __ASSEMBLER__ */
#include <arm/arm32/pte.h>	/* pt_entry_t */

#define	USRSTACK	VM_MAXUSER_ADDRESS

/*
 * Note that MAXTSIZ can't be larger than 32M, otherwise the compiler
 * would have to be changed to not generate "bl" instructions.
 */
#define	MAXTSIZ		(16*1024*1024)		/* max text size */
#ifndef	DFLDSIZ
#define	DFLDSIZ		(128*1024*1024)		/* initial data size limit */
#endif
#ifndef	MAXDSIZ
#define	MAXDSIZ		(512*1024*1024)		/* max data size */
#endif
#ifndef	DFLSSIZ
#define	DFLSSIZ		(2*1024*1024)		/* initial stack size limit */
#endif
#ifndef	MAXSSIZ
#define	MAXSSIZ		(8*1024*1024)		/* max stack size */
#endif

/*
 * Size of SysV shared memory map
 */
#ifndef SHMMAXPGS
#define	SHMMAXPGS	1024
#endif

/*
 * While the ARM architecture defines Section mappings, large pages,
 * and small pages, the standard page size is (and will always be) 4K.
 */
#define	PAGE_SHIFT	12
#define	PAGE_SIZE	(1 << PAGE_SHIFT)
#define	PAGE_MASK	(PAGE_SIZE - 1)

/*
 * Mach derived constants
 */
#define	VM_MIN_ADDRESS		((vaddr_t) 0x00001000)
#define	VM_MAXUSER_ADDRESS	((vaddr_t) KERNEL_BASE)
#define	VM_MAX_ADDRESS		VM_MAXUSER_ADDRESS

#define	VM_MIN_KERNEL_ADDRESS	((vaddr_t) KERNEL_BASE)
#define	VM_MAX_KERNEL_ADDRESS	((vaddr_t) 0xffffffff)

#ifndef __ASSEMBLER__
/* XXX max. amount of KVM to be used by buffers. */
#ifndef VM_MAX_KERNEL_BUF
extern vaddr_t virtual_avail;
extern vaddr_t virtual_end;

#define	VM_MAX_KERNEL_BUF	\
	((virtual_end - virtual_avail) * 4 / 10)
#endif

/*
 * pmap-specific data store in the vm_page structure.
 */
#define	__HAVE_VM_PAGE_MD
struct vm_page_md {
	struct pv_entry *pvh_list;		/* pv_entry list */
	struct simplelock pvh_slock;		/* lock on this head */
	int pvh_attrs;				/* page attributes */
	u_int uro_mappings;
	u_int urw_mappings;
	union {
		u_short s_mappings[2];	/* Assume kernel count <= 65535 */
		u_int i_mappings;
	} k_u;
#define	kro_mappings	k_u.s_mappings[0]
#define	krw_mappings	k_u.s_mappings[1]
#define	k_mappings	k_u.i_mappings
};
/*
 * Set the default color of each page.
 */
#if ARM_MMU_V6 > 0
#define	VM_MDPAGE_PVH_ATTRS_INIT(pg) \
	(pg)->mdpage.pvh_attrs = (pg)->phys_addr & arm_cache_prefer_mask
#else
#define	VM_MDPAGE_PVH_ATTRS_INIT(pg) \
	(pg)->mdpage.pvh_attrs = 0
#endif

#define	VM_MDPAGE_INIT(pg)						\
do {									\
	(pg)->mdpage.pvh_list = NULL;					\
	simple_lock_init(&(pg)->mdpage.pvh_slock);			\
	(pg)->mdpage.pvh_attrs = 0;					\
	(pg)->mdpage.uro_mappings = 0;					\
	(pg)->mdpage.urw_mappings = 0;					\
	(pg)->mdpage.k_mappings = 0;					\
} while (/*CONSTCOND*/0)
#endif /* __ASSEMBLER__ */

#endif /* _KERNEL */

#endif /* _ARM_ARM32_VMPARAM_H_ */
