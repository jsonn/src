/*      $NetBSD: param.h,v 1.38.6.2 1999/12/27 18:34:12 wrstuden Exp $    */
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
 *	@(#)param.h	5.8 (Berkeley) 6/28/91
 */

#ifndef _VAX_PARAM_H_
#define _VAX_PARAM_H_

/*
 * Machine dependent constants for VAX.
 */

#define	_MACHINE	vax
#define	MACHINE		"vax"
#define	_MACHINE_ARCH	vax
#define	MACHINE_ARCH	"vax"
#define	MID_MACHINE	MID_VAX

/*
 * Round p (pointer or byte index) up to a correctly-aligned value
 * for all data types (int, long, ...).   The result is u_int and
 * must be cast to any desired pointer type.
 *
 * ALIGNED_POINTER is a boolean macro that checks whether an address
 * is valid to fetch data elements of type t from on this architecture.
 * This does not reflect the optimal alignment, just the possibility
 * (within reasonable limits). 
 *
 */

#define ALIGNBYTES		(sizeof(int) - 1)
#define ALIGN(p)		(((u_int)(p) + ALIGNBYTES) &~ ALIGNBYTES)
#define ALIGNED_POINTER(p,t)	((((u_long)(p)) & (sizeof(t)-1)) == 0)

#define	PGSHIFT		12			/* LOG2(NBPG) */
#define	NBPG		(1 << PGSHIFT)		/* (1 << PGSHIFT) bytes/page */
#define	PGOFSET		(NBPG - 1)               /* byte offset into page */

#define	VAX_PGSHIFT	9
#define	VAX_NBPG	(1 << VAX_PGSHIFT)
#define	VAX_PGOFSET	(VAX_NBPG - 1)

#define	KERNBASE	0x80000000		/* start of kernel virtual */

#define	DEV_BSHIFT	9		               /* log2(DEV_BSIZE) */
#define	DEV_BSIZE	(1 << DEV_BSHIFT)

#define BLKDEV_IOSIZE	2048
#define	MAXPHYS		(63 * 1024)	/* max raw I/O transfer size */
#define	MAXBSIZE	0x4000		/* max FS block size - XXX */

#define	UPAGES		2		/* pages of u-area */
#define USPACE		(NBPG*UPAGES)
#define	REDZONEADDR	(VAX_NBPG*3)	/* Must be > sizeof(struct user) */

#ifndef MSGBUFSIZE
#define MSGBUFSIZE	NBPG		/* default message buffer size */
#endif

/*
 * Constants related to network buffer management.
 * MCLBYTES must be no larger than NBPG (the software page size), and,
 * on machines that exchange pages of input or output buffers with mbuf
 * clusters (MAPPED_MBUFS), MCLBYTES must also be an integral multiple
 * of the hardware page size.
 */

#ifndef	MSIZE
#define	MSIZE		128		/* size of an mbuf */
#endif	/* MSIZE */

#ifndef	MCLSHIFT
#define	MCLSHIFT	11		/* convert bytes to m_buf clusters */
#endif	/* MCLSHIFT */
#define	MCLBYTES	(1 << MCLSHIFT)	/* size of an m_buf cluster */
#define	MCLOFSET	(MCLBYTES - 1)	/* offset within an m_buf cluster */

#ifndef NMBCLUSTERS

#if defined(_KERNEL) && !defined(_LKM)
#include "opt_gateway.h"
#include "opt_non_po2_blocks.h"
#endif /* _KERNEL && ! _LKM */

#ifdef GATEWAY
#define	NMBCLUSTERS	512		/* map size, max cluster allocation */
#else
#define	NMBCLUSTERS	256		/* map size, max cluster allocation */
#endif	/* GATEWAY */
#endif	/* NMBCLUSTERS */

/*
 * Size of kernel malloc arena in NBPG-sized logical pages
 */ 

#ifndef NKMEMCLUSTERS
#define	NKMEMCLUSTERS	(4096*1024/NBPG)
#endif

/*
 * Some macros for units conversion
 */

/* pages ("clicks") to disk blocks */
#define	ctod(x, sh)		((x) << (PGSHIFT - (sh)))
#define	dtoc(x, sh)		((x) >> (PGSHIFT - (sh)))

/* clicks to bytes */
#define	ctob(x)		((x) << PGSHIFT)
#define	btoc(x)		(((unsigned)(x) + PGOFSET) >> PGSHIFT)
#define	btop(x)		(((unsigned)(x)) >> PGSHIFT)

/* bytes to disk blocks */
#if defined(_LKM) || defined(NON_PO2_BLOCKS)
#define	dbtob(x, sh)	(((sh) >= 0) ? ((x) << (sh)) : ((x) * (-sh)))
#define	btodb(x, sh)	(((sh) >= 0) ? ((x) >> (sh)) : ((x) / (-sh)))
#define	blocksize(sh)	(((sh) >= 0) ? (1 << (sh))   : (-sh))
#else
#define	dbtob(x, sh)	((x) << (sh))
#define	btodb(x, sh)	((x) >> (sh))
#define	blocksize(sh)	(((sh) >= 0) ? (1 << (sh))   : (0))
#endif

/* MD conversion macros */
#define	vax_btoc(x)	(((unsigned)(x) + VAX_PGOFSET) >> VAX_PGSHIFT)
#define	vax_btop(x)	(((unsigned)(x)) >> VAX_PGSHIFT)

/*
 * Map a ``block device block'' to a file system block.
 * This should be device dependent, and will be if we
 * add an entry to cdevsw/bdevsw for that purpose.
 * For now though just use DEV_BSIZE.
 */

#define	bdbtofsb(bn)	((bn) / (BLKDEV_IOSIZE/DEV_BSIZE))

#ifdef _KERNEL
#ifndef lint
#define splx(reg)						\
({								\
	register int val;					\
	__asm __volatile ("mfpr $0x12,%0;mtpr %1,$0x12"		\
				: "&=g" (val)			\
				: "g" (reg));			\
	val;							\
})

#define	_splraise(reg)						\
({								\
	register int val;					\
	__asm __volatile ("mfpr $0x12,%0"			\
				: "&=g" (val)			\
				: );				\
	if ((reg) > val) {					\
		__asm __volatile ("mtpr %0,$0x12"		\
				:				\
				: "g" (reg));			\
	}							\
	val;							\
})
#endif

#define	spl0()		splx(0)		/* IPL0  */
#define spllowersoftclock() splx(8)	/* IPL08 */
#define splsoftclock()	_splraise(8)	/* IPL08 */
#define splsoftnet()	_splraise(0xc)	/* IPL0C */
#define	splddb()	_splraise(0xf)	/* IPL0F */
#define splbio()	_splraise(0x15)	/* IPL15 */
#define splnet()	_splraise(0x15)	/* IPL15 */
#define spltty()	_splraise(0x15)	/* IPL15 */
#define splimp()	_splraise(0x17)	/* IPL17 */
#define splclock()	_splraise(0x18)	/* IPL18 */
#define splhigh()	_splraise(0x1f)	/* IPL1F */
#define	splstatclock()	splclock()

/* These are better to use when playing with VAX buses */
#define	spl4()		splx(0x14)
#define	spl5()		splx(0x15)
#define	spl6()		splx(0x16)
#define	spl7()		splx(0x17)

#define vmapbuf(p,q)
#define vunmapbuf(p,q)

/* Prototype needed for delay() */
#ifndef	_LOCORE
void	delay __P((int));
/* inline macros used inside kernel */
#include <machine/macros.h>
#endif

#define	DELAY(x) delay(x)
#endif /* _KERNEL */

#endif /* _VAX_PARAM_H_ */
