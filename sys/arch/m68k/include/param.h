/*	$NetBSD: param.h,v 1.7.4.1 2000/07/23 03:49:31 itojun Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: machparam.h 1.16 92/12/20$
 *
 *	@(#)param.h	8.1 (Berkeley) 6/10/93
 */
#ifndef _M68K_PARAM_H_
#define _M68K_PARAM_H_

/*
 * Machine independent constants for m68k
 */
#define	_MACHINE_ARCH	m68k
#define	MACHINE_ARCH	"m68k"
#define	MID_MACHINE	MID_M68K

/*
 * Round p (pointer or byte index) up to a correctly-aligned value for all
 * data types (int, long, ...).   The result is u_int and must be cast to
 * any desired pointer type.
 *
 * ALIGNED_POINTER is a boolean macro that checks whether an address
 * is valid to fetch data elements of type t from on this architecture.
 * This does not reflect the optimal alignment, just the possibility
 * (within reasonable limits). 
 *
 */
#define ALIGNBYTES		(sizeof(int) - 1)
#define	ALIGN(p)		(((u_int)(p) + ALIGNBYTES) &~ ALIGNBYTES)
#define ALIGNED_POINTER(p,t)	((((u_long)(p)) & (sizeof(t)-1)) == 0)

#define	NBPG		(1 << PGSHIFT)	/* bytes/page */
#define	PGOFSET		(NBPG-1)	/* byte offset into page */

#define	BTOPKERNBASE	((u_long)KERNBASE >> PGSHIFT)

#define	DEV_BSHIFT	9		/* log2(DEV_BSIZE) */
#define	DEV_BSIZE	(1 << DEV_BSHIFT)
#define BLKDEV_IOSIZE	2048
#define	MAXPHYS		(64 * 1024)	/* max raw I/O transfer size */

#define	SSIZE		1		/* initial stack size/NBPG */
#define	SINCR		1		/* increment of stack/NBPG */

#define	USPACE		(UPAGES * NBPG)

#ifndef MSGBUFSIZE
#define MSGBUFSIZE	NBPG		/* default message buffer size */
#endif

/*
 * Constants related to network buffer management.
 * MCLBYTES must be no larger than CLBYTES (the software page size), and,
 * on machines that exchange pages of input or output buffers with mbuf
 * clusters (MAPPED_MBUFS), MCLBYTES must also be an integral multiple
 * of the hardware page size.
 */
#define	MSIZE		256		/* size of an mbuf */

#ifndef	MCLSHIFT
# define	MCLSHIFT	11	/* convert bytes to m_buf clusters */
#endif	/* MCLSHIFT */

#define	MCLBYTES	(1 << MCLSHIFT)
#define	MCLOFSET	(MCLBYTES - 1)
#ifndef NMBCLUSTERS

#if defined(_KERNEL) && !defined(_LKM)
#include "opt_gateway.h"
#endif /* _KERNEL && ! _LKM */

#ifdef GATEWAY
# define	NMBCLUSTERS	512	/* map size, max cluster allocation */
#else
# define	NMBCLUSTERS	256	/* map size, max cluster allocation */
#endif
#endif

/* pages ("clicks") to disk blocks */
#define	ctod(x)		((x) << (PGSHIFT - DEV_BSHIFT))
#define	dtoc(x)		((x) >> (PGSHIFT - DEV_BSHIFT))

/* pages to bytes */
#define	ctob(x)		((x) << PGSHIFT)
#define	btoc(x)		(((x) + PGOFSET) >> PGSHIFT)

/* bytes to disk blocks */
#define	btodb(x)	((x) >> DEV_BSHIFT)
#define	dbtob(x)	((x) << DEV_BSHIFT)

/*
 * Map a ``block device block'' to a file system block.
 * This should be device dependent, and should use the bsize
 * field from the disk label.
 * For now though just use DEV_BSIZE.
 */
#define	bdbtofsb(bn)	((bn) / (BLKDEV_IOSIZE/DEV_BSIZE))

/*
 * Mach-derived conversion macros
 */
#define	m68k_round_seg(x)	((((unsigned)(x)) + SEGOFSET) & ~SEGOFSET)
#define	m68k_trunc_seg(x)	((unsigned)(x) & ~SEGOFSET)
#define	m68k_seg_offset(x)	((unsigned)(x) & SEGOFSET)
#define	m68k_round_page(x)	((((unsigned)(x)) + PGOFSET) & ~PGOFSET)
#define	m68k_trunc_page(x)	((unsigned)(x) & ~PGOFSET)
#define	m68k_page_offset(x)	((unsigned)(x) & PGOFSET)
#define	m68k_btop(x)		((unsigned)(x) >> PGSHIFT)
#define	m68k_ptob(x)		((unsigned)(x) << PGSHIFT)

#endif	/* !_M68K_PARAM_H_ */
