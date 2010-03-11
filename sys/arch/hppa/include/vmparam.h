/*	$NetBSD: vmparam.h,v 1.12.10.2 2010/03/11 15:02:27 yamt Exp $	*/

/*	$OpenBSD: vmparam.h,v 1.33 2006/06/04 17:21:24 miod Exp $	*/

/* 
 * Copyright (c) 1988-1994, The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 * 	Utah $Hdr: vmparam.h 1.16 94/12/16$
 */

#ifndef _HPPA_VMPARAM_H_
#define _HPPA_VMPARAM_H_

/*
 * Machine dependent constants for HP PA
 */

/*
 * We use 4K pages on the HP PA.  Override the PAGE_* definitions
 * to be compile-time constants.
 */
#define	PAGE_SHIFT	12
#define	PAGE_SIZE	(1 << PAGE_SHIFT)
#define	PAGE_MASK	(PAGE_SIZE - 1)

/*
 * USRSTACK is the bottom (start) of the user stack.
 */
#define	USRSTACK	0x70000000		/* Start of user stack */
#define	SYSCALLGATE	0xC0000000		/* syscall gateway page */

/*
 * Virtual memory related constants, all in bytes
 */
#ifndef MAXTSIZ
#define	MAXTSIZ		(0x40000000)		/* max text size */
#endif
#ifndef DFLDSIZ
#define	DFLDSIZ		(16*1024*1024)		/* initial data size limit */
#endif
#ifndef MAXDSIZ
#define	MAXDSIZ		(USRSTACK-MAXTSIZ)	/* max data size */
#endif
#ifndef	DFLSSIZ
#define	DFLSSIZ		(2*1024*1024)		/* initial stack size limit */
#endif
#ifndef	MAXSSIZ
#define	MAXSSIZ		(256*1024*1024)		/* max stack size */
#endif

#ifndef USRIOSIZE
#define	USRIOSIZE	((2*HPPA_PGALIAS)/PAGE_SIZE)	/* 2mb */
#endif

/*
 * The time for a process to be blocked before being very swappable.
 * This is a number of seconds which the system takes as being a non-trivial
 * amount of real time.  You probably shouldn't change this;
 * it is used in subtle ways (fractions and multiples of it are, that is, like
 * half of a ``long time'', almost a long time, etc.)
 * It is related to human patience and other factors which don't really
 * change over time.
 */
/* XXXNH - remove??? */
#define	MAXSLP 		20

/* user/kernel map constants */
#define	VM_MIN_ADDRESS		((vaddr_t)0)
#define	VM_MAXUSER_ADDRESS	((vaddr_t)0xc0000000)
#define	VM_MAX_ADDRESS		VM_MAXUSER_ADDRESS

#define	VM_MIN_KERNEL_ADDRESS	((vaddr_t)0xc0001000)
#define	VM_MAX_KERNEL_ADDRESS	((vaddr_t)0xef000000)

/* virtual sizes (bytes) for various kernel submaps */
#define VM_PHYS_SIZE		(USRIOSIZE*PAGE_SIZE)

#define	VM_PHYSSEG_MAX	8	/* this many physmem segments */
#define	VM_PHYSSEG_STRAT	VM_PSTRAT_BIGFIRST

#define	VM_PHYSSEG_NOADD	/* XXX until uvm code is fixed */

#define	VM_NFREELIST		2
#define	VM_FREELIST_DEFAULT	0
#define	VM_FREELIST_ISADMA	1

#if defined(_KERNEL) && !defined(_LOCORE)
#define __HAVE_VM_PAGE_MD

struct pv_entry;

struct vm_page_md {
	kmutex_t	pvh_lock;	/* locks every pv on this list */
	struct pv_entry	*pvh_list;	/* head of list (locked by pvh_lock) */
	u_int		pvh_attrs;	/* to preserve ref/mod */
	int		pvh_aliases;	/* alias counting */
};

#define	VM_MDPAGE_INIT(pg) \
do {									\
	mutex_init(&(pg)->mdpage.pvh_lock, MUTEX_NODEBUG, IPL_VM);	\
	(pg)->mdpage.pvh_list = NULL;					\
	(pg)->mdpage.pvh_attrs = 0;					\
	(pg)->mdpage.pvh_aliases = 0;					\
} while (0)
#endif

#endif	/* _HPPA_VMPARAM_H_ */
