/*      $NetBSD: param.h,v 1.2.4.4 2004/09/21 13:20:04 skrll Exp $    */
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

#ifndef _PDP10_PARAM_H_
#define _PDP10_PARAM_H_

/*
 * Machine dependent constants for PDP10.
 */

#define	_MACHINE	pdp10
#define	MACHINE		"pdp10"
#define	_MACHINE_ARCH	pdp10
#define	MACHINE_ARCH	"pdp10"
#define	MID_MACHINE	MID_PDP10

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

/* XXX - how should this macro look like??? */
#define ALIGNBYTES		(sizeof(int) - 1)
#define ALIGN(p)		(((u_int)(p) + ALIGNBYTES) &~ ALIGNBYTES)
#define ALIGNED_POINTER(p,t)	((((u_long)(p)) & (sizeof(t)-1)) == 0)

#define	PGSHIFT		11			/* LOG2(NBPG) */
#define	NBPG		(1 << PGSHIFT)		/* (1 << PGSHIFT) bytes/page */
#define	PGOFSET		(NBPG - 1)		/* byte offset into page */

#define	KERNBASE	01000000		/* start of kernel virtual */

#define	DEV_BSHIFT	9		        /* log2(DEV_BSIZE) */
#define	DEV_BSIZE	(1 << DEV_BSHIFT)

#define BLKDEV_IOSIZE	2048
#define	MAXPHYS		(64 * 1024)	/* max raw I/O transfer size */
#define	MAXBSIZE	0x4000		/* max FS block size - XXX */

#define	UPAGES		2		/* pages of u-area */
#define USPACE		(NBPG*UPAGES)

#ifndef MSGBUFSIZE
#define MSGBUFSIZE	NBPG		/* default message buffer size */
#endif

/*
 * KVA is very tight on pdp10, reduce the amount of KVA used by pipe
 * "direct" write code to reasonably low value.
 */
#ifndef PIPE_DIRECT_CHUNK
#define PIPE_DIRECT_CHUNK	65536
#endif

/*
 * Constants related to network buffer management.
 * MCLBYTES must be no larger than NBPG (the software page size), and,
 * on machines that exchange pages of input or output buffers with mbuf
 * clusters (MAPPED_MBUFS), MCLBYTES must also be an integral multiple
 * of the hardware page size.
 */
#define	MSIZE		256		/* size of an mbuf */

#ifndef	MCLSHIFT
#define	MCLSHIFT	11		/* convert bytes to m_buf clusters */
					/* 2K cluster can hold Ether frame */
#endif	/* MCLSHIFT */

#define	MCLBYTES	(1 << MCLSHIFT)	/* size of a m_buf cluster */

#ifndef NMBCLUSTERS
#if defined(_KERNEL_OPT)
#include "opt_gateway.h"
#endif

#ifdef GATEWAY
#define	NMBCLUSTERS	512		/* map size, max cluster allocation */
#else
#define	NMBCLUSTERS	256		/* map size, max cluster allocation */
#endif
#endif

/*
 * Minimum and maximum sizes of the kernel malloc arena in PAGE_SIZE-sized
 * logical pages.
 */
#define	NKMEMPAGES_MIN_DEFAULT	((4 * 1024 * 1024) >> PAGE_SHIFT)
#define	NKMEMPAGES_MAX_DEFAULT	((4 * 1024 * 1024) >> PAGE_SHIFT)

#define	PAGER_MAP_SIZE		(4 * 1024 * 1024)

/*
 * Some macros for units conversion
 */

/* pages ("clicks") to disk blocks */
#define	ctod(x)		((x) << (PGSHIFT - DEV_BSHIFT))
#define	dtoc(x)		((x) >> (PGSHIFT - DEV_BSHIFT))

/* clicks to bytes */
#define	ctob(x)		((x) << PGSHIFT)
#define	btoc(x)		(((unsigned)(x) + PGOFSET) >> PGSHIFT)
#define	btop(x)		(((unsigned)(x)) >> PGSHIFT)

/* bytes to disk blocks */
#define	btodb(x)	((unsigned long)(x) >> DEV_BSHIFT)
#define	dbtob(x)	((unsigned long)(x) << DEV_BSHIFT)

/*
 * Map a ``block device block'' to a file system block.
 * This should be device dependent, and will be if we
 * add an entry to cdevsw/bdevsw for that purpose.
 * For now though just use DEV_BSIZE.
 */

#define	bdbtofsb(bn)	((bn) / (BLKDEV_IOSIZE/DEV_BSIZE))

#ifdef _KERNEL
#include <machine/intr.h>

/* Prototype needed for delay() */
#ifndef	_LOCORE
void	delay __P((int));
void *	alloca(size_t);		/* XXX should be somewhere else */
#endif

#define	DELAY(x) delay(x)
#endif /* _KERNEL */

#endif /* _PDP10_PARAM_H_ */
