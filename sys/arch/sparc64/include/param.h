/*	$NetBSD: param.h,v 1.11.8.1 1999/12/21 23:16:18 wrstuden Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)param.h	8.1 (Berkeley) 6/11/93
 */

/*
 * Copyright (c) 1996-1999 Eduardo Horvath
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR  ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR  BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */



#define	_MACHINE	sparc64
#define	MACHINE		"sparc64"
#ifdef __arch64__
#define	_MACHINE_ARCH	sparc64
#define	MACHINE_ARCH	"sparc64"
#define	MID_MACHINE	MID_SPARC64
#else
#define	_MACHINE_ARCH	sparc
#define	MACHINE_ARCH	"sparc"
#define	MID_MACHINE	MID_SPARC
#endif

#ifdef _KERNEL				/* XXX */
#ifndef _LOCORE				/* XXX */
#include <machine/cpu.h>		/* XXX */
#endif					/* XXX */
#endif					/* XXX */

/*
 * Round p (pointer or byte index) up to a correctly-aligned value for
 * the machine's strictest data type.  The result is u_int and must be
 * cast to any desired pointer type.
 *
 * ALIGNED_POINTER is a boolean macro that checks whether an address
 * is valid to fetch data elements of type t from on this architecture.
 * This does not reflect the optimal alignment, just the possibility
 * (within reasonable limits). 
 *
 */
#define ALIGNBYTES32		0x7
#define ALIGNBYTES64		0xf
#ifdef __arch64__
#define	ALIGNBYTES		ALIGNBYTES64
#else
#define	ALIGNBYTES		ALIGNBYTES32
#endif
#define	ALIGN(p)		(((u_long)(p) + ALIGNBYTES) & ~ALIGNBYTES)
#define ALIGN32(p)		(((u_long)(p) + ALIGNBYTES32) & ~ALIGNBYTES32)
#define ALIGNED_POINTER(p,t)	((((u_long)(p)) & (sizeof(t)-1)) == 0)


/*
 * The following variables are always defined and initialized (in locore)
 * so independently compiled modules (e.g. LKMs) can be used irrespective
 * of the `options SUN4?' combination a particular kernel was configured with.
 * See also the definitions of NBPG, PGOFSET and PGSHIFT below.
 */
#if defined(_KERNEL) && !defined(_LOCORE)
extern int nbpg, pgofset, pgshift;
#endif

#define	KERNBASE	0xf8000000	/* start of kernel virtual space */
#define	KERNTEXTOFF	(KERNBASE+0x4000)	/* start of kernel text */

#define	DEF_BSIZE	512
#define	DEF_BSHIFT	9		/* log2(DEF_BSIZE) */
#define	BLKDEV_IOSIZE	2048
#define	MAXPHYS		(64 * 1024)

#define	CLSIZE		1
#define	CLSIZELOG2	0

/* NOTE: SSIZE must be multiple of CLSIZE */
#ifdef __arch64__
/* We get stack overflows w/8K stacks in 64-bit mode */
#define	SSIZE		2		/* initial stack size in pages */
#else
#define	SSIZE		1
#endif
#define	USPACE		(SSIZE*8192)

/*
 * Constants related to network buffer management.
 * MCLBYTES must be no larger than CLBYTES (the software page size), and,
 * on machines that exchange pages of input or output buffers with mbuf
 * clusters (MAPPED_MBUFS), MCLBYTES must also be an integral multiple
 * of the hardware page size.
 */
#define	MSIZE		128		/* size of an mbuf */
#define	MCLBYTES	2048		/* enough for whole Ethernet packet */
#define	MCLSHIFT	11		/* log2(MCLBYTES) */
#define	MCLOFSET	(MCLBYTES - 1)

#if defined(_KERNEL) && !defined(_LKM)
#include "opt_gateway.h"
#include "opt_non_po2_blocks.h"
#endif /* _KERNEL && ! _LKM */

#ifndef NMBCLUSTERS
#ifdef GATEWAY
#define	NMBCLUSTERS	512		/* map size, max cluster allocation */
#else
#define	NMBCLUSTERS	256		/* map size, max cluster allocation */
#endif
#endif

#define MSGBUFSIZE	NBPG

/*
 * Size of kernel malloc arena in CLBYTES-sized logical pages.
 */
#ifndef	NKMEMCLUSTERS
#define	NKMEMCLUSTERS	(6 * 1024 * 1024 / CLBYTES)
#endif

/* pages ("clicks") to disk blocks */
#define	ctod(x, sh)		((x) << (PGSHIFT - (sh)))
#define	dtoc(x, sh)		((x) >> (PGSHIFT - (sh)))

/* pages to bytes */
#define	ctob(x)		((x) << PGSHIFT)
#define	btoc(x)		(((x) + PGOFSET) >> PGSHIFT)

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

/*
 * Map a ``block device block'' to a file system block.
 * This should be device dependent, and should use the bsize
 * field from the disk label.
 * For now though just use DEV_BSIZE.
 */
#define	bdbtofsb(bn)	((bn) / (BLKDEV_IOSIZE / DEV_BSIZE))

/*
 * dvmamap manages a range of DVMA addresses intended to create double
 * mappings of physical memory. In a way, `dvmamap' is a submap of the
 * VM map `phys_map'. The difference is the use of the `resource map'
 * routines to manage page allocation, allowing DVMA addresses to be
 * allocated and freed from within interrupt routines.
 *
 * Note that `phys_map' can still be used to allocate memory-backed pages
 * in DVMA space.
 */
#ifdef _KERNEL
#ifndef _LOCORE
extern vaddr_t	dvma_base;
extern vaddr_t	dvma_end;
extern struct map	*dvmamap;
/*
 * The dvma resource map is defined in page units, which are numbered 1 to N.
 * Use these macros to convert to/from virtual addresses.
 */
#define rctov(n)		(ctob(((n)-1))+dvma_base)
#define vtorc(v)		((btoc((v)-dvma_base))+1)

extern caddr_t	kdvma_mapin __P((caddr_t, int, int));
extern caddr_t	dvma_malloc __P((size_t, void *, int));
extern void	dvma_free __P((caddr_t, size_t, void *));

extern void	delay __P((unsigned int));
#define	DELAY(n)	delay(n)

extern int cputyp;
extern int cpumod;
extern int mmumod;

#endif /* _LOCORE */
#endif /* _KERNEL */

/*
 * Values for the cputyp variable.
 */
#define CPU_SUN4	0
#define CPU_SUN4C	1
#define CPU_SUN4M	2
#define CPU_SUN4U	3

/*
 * Shorthand CPU-type macros. Enumerate all eight cases.
 * Let compiler optimize away code conditional on constants.
 *
 * On a sun4 machine, the page size is 8192, while on a sun4c and sun4m
 * it is 4096. Therefore, in the (SUN4 && (SUN4C || SUN4M)) cases below,
 * NBPG, PGOFSET and PGSHIFT are defined as variables which are initialized
 * early in locore.s after the machine type has been detected.
 *
 * Note that whenever the macros defined below evaluate to expressions
 * involving variables, the kernel will perform slighly worse due to the
 * extra memory references they'll generate.
 */

#define CPU_ISSUN4M	(0)
#define CPU_ISSUN4C	(0)
#define CPU_ISSUN4	(0)
#define CPU_ISSUN4OR4C	(0)
#define CPU_ISSUN4COR4M	(0)
#define	NBPG		8192		/* bytes/page */
#define	PGOFSET		(NBPG-1)	/* byte offset into page */
#define	PGSHIFT		13		/* log2(NBPG) */
