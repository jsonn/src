/*	$NetBSD: vmparam.h,v 1.1.4.2 2002/01/08 00:23:12 nathanw Exp $	*/

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
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

/* for pt_entry_t definition */
#include <arm/arm32/pte.h>

#define	USRTEXT		VM_MIN_ADDRESS
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
 * Linear page table space: number of PTEs required to map the 4G address
 * space * size of each PTE.
 */
#define	PAGE_TABLE_SPACE	((1 << (32 - PGSHIFT)) * sizeof(pt_entry_t))

/* Address where the page talbles are mapped. */
#define	PAGE_TABLE_SPACE_START	(KERNEL_SPACE_START - PAGE_TABLE_SPACE)

/*
 * Mach derived constants
 */
#define	VM_MIN_ADDRESS		((vaddr_t) 0x00001000)
#define	VM_MAXUSER_ADDRESS	((vaddr_t) (PAGE_TABLE_SPACE_START -	\
					    UPAGES * NBPG))
#define	VM_MAX_ADDRESS		((vaddr_t) (PAGE_TABLE_SPACE_START +	\
					    (KERNEL_SPACE_START >> PGSHIFT) * \
					    sizeof(pt_entry_t)))
#define	VM_MIN_KERNEL_ADDRESS	((vaddr_t) KERNEL_TEXT_BASE)
#define	VM_MAXKERN_ADDRESS	((vaddr_t) KERNEL_VM_BASE + KERNEL_VM_SIZE)
#define	VM_MAX_KERNEL_ADDRESS	((vaddr_t) 0xffffffff)

/*
 * define structure pmap_physseg: there is one of these structures
 * for each chunk of noncontig RAM you have.
 */

#define	__HAVE_PMAP_PHYSSEG

struct pmap_physseg {
	struct pv_head *pvhead;		/* pv_entry array */
	char *attrs;			/* attrs array */
};

#endif /* _KERNEL */

#endif /* _ARM_ARM32_VMPARAM_H_ */
