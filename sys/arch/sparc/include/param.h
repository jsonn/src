/*	$NetBSD: param.h,v 1.48.8.4 2002/08/01 02:43:23 nathanw Exp $ */

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
 * Sun4M support by Aaron Brown, Harvard University.
 * Changes Copyright (c) 1995 The President and Fellows of Harvard College.
 * All rights reserved.
 */
#define	_MACHINE	sparc
#define	MACHINE		"sparc"
#define	_MACHINE_ARCH	sparc
#define	MACHINE_ARCH	"sparc"
#define	MID_MACHINE	MID_SPARC

#ifdef _KERNEL_OPT
#include "opt_sparc_arch.h"
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
#define	ALIGNBYTES		7
#define	ALIGN(p)		(((u_int)(p) + ALIGNBYTES) & ~ALIGNBYTES)
#define ALIGNED_POINTER(p,t)	((((u_long)(p)) & (sizeof(t)-1)) == 0)

#define SUN4_PGSHIFT	13	/* for a sun4 machine */
#define SUN4CM_PGSHIFT	12	/* for a sun4c or sun4m machine */

/*
 * The following variables are always defined and initialized (in locore)
 * so independently compiled modules (e.g. LKMs) can be used irrespective
 * of the `options SUN4?' combination a particular kernel was configured with.
 * See also the definitions of NBPG, PGOFSET and PGSHIFT below.
 */
#if (defined(_KERNEL) || defined(_STANDALONE)) && !defined(_LOCORE)
extern int nbpg, pgofset, pgshift;
#endif

#define	KERNBASE	0xf0000000	/* start of kernel virtual space */
#define KERNEND		0xfe000000	/* end of kernel virtual space */
/* Arbitrarily only use 1/4 of the kernel address space for buffers. */
#define VM_MAX_KERNEL_BUF	((KERNEND - KERNBASE)/4)
#define PROM_LOADADDR	0x00004000	/* where the prom loads us */
#define	KERNTEXTOFF	(KERNBASE+PROM_LOADADDR)/* start of kernel text */

#define	DEV_BSIZE	512
#define	DEV_BSHIFT	9		/* log2(DEV_BSIZE) */
#define	BLKDEV_IOSIZE	2048
#define	MAXPHYS		(64 * 1024)

#define	SSIZE		1		/* initial stack size in pages */
#define	USPACE		8192

/*
 * Constants related to network buffer management.
 * MCLBYTES must be no larger than NBPG (the software page size), and,
 * on machines that exchange pages of input or output buffers with mbuf
 * clusters (MAPPED_MBUFS), MCLBYTES must also be an integral multiple
 * of the hardware page size.
 */
#define	MSIZE		256		/* size of an mbuf */

#ifndef MCLSHIFT
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
#define	NKMEMPAGES_MIN_DEFAULT	((6 * 1024 * 1024) >> PAGE_SHIFT)
#define	NKMEMPAGES_MAX_DEFAULT	((6 * 1024 * 1024) >> PAGE_SHIFT)

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
#define	bdbtofsb(bn)	((bn) / (BLKDEV_IOSIZE / DEV_BSIZE))

/*
 * Values for the cputyp variable.
 */
#define CPU_SUN4	0
#define CPU_SUN4C	1
#define CPU_SUN4M	2
#define CPU_SUN4U	3
#define	CPU_SUN4D	4

#if defined(_KERNEL) || defined(_STANDALONE)
#ifndef _LOCORE

extern int cputyp;

extern void	delay __P((unsigned int));
#define	DELAY(n)	delay(n)
#endif /* _LOCORE */


/*
 * microSPARC-IIep is a sun4m but with an integrated PCI controller.
 * In a lot of places (like pmap &c) we want it to be treated as SUN4M.
 * But since various low-level things are done very differently from
 * normal sparcs (and since for now it requires a relocated kernel
 * anyway), the MSIIEP kernels are not supposed to support any other
 * system.  So insist on SUN4M defined and SUN4 and SUN4C not defined.
 */
#if defined(MSIIEP)
#if defined(SUN4) || defined(SUN4C) || defined(SUN4D)
#error "microSPARC-IIep kernels cannot support sun4, sun4c, or sun4d"
#endif
#if !defined(SUN4M)
#error "microSPARC-IIep kernel must have 'options SUN4M'"
#endif
#endif /* MSIIEP */

/*
 * Shorthand CPU-type macros.  Let compiler optimize away code
 * conditional on constants.
 */

/*
 * Step 1: Count the number of CPU types configured into the
 * kernel.
 */
#define	CPU_NTYPES	(defined(SUN4) + defined(SUN4C) + \
			 defined(SUN4M) + defined(SUN4D))

/*
 * Step 2: Define the CPU type predicates.  Rules:
 *
 *	* If CPU types are configured in, and the CPU type
 *	  is not one of them, then the test is always false.
 *
 *	* If exactly one CPU type is configured in, and it's
 *	  this one, then the test is always true.
 *
 *	* Otherwise, we have to reference the cputyp variable.
 */
#if CPU_NTYPES != 0 && !defined(SUN4)
#	define CPU_ISSUN4	(0)
#elif CPU_NTYPES == 1 && defined(SUN4)
#	define CPU_ISSUN4	(1)
#else
#	define CPU_ISSUN4	(cputyp == CPU_SUN4)
#endif

#if CPU_NTYPES != 0 && !defined(SUN4C)
#	define CPU_ISSUN4C	(0)
#elif CPU_NTYPES == 1 && defined(SUN4C)
#	define CPU_ISSUN4C	(1)
#else
#	define CPU_ISSUN4C	(cputyp == CPU_SUN4C)
#endif

#if CPU_NTYPES != 0 && !defined(SUN4M)
#	define CPU_ISSUN4M	(0)
#elif CPU_NTYPES == 1 && defined(SUN4M)
#	define CPU_ISSUN4M	(1)
#else
#	define CPU_ISSUN4M	(cputyp == CPU_SUN4M)
#endif

#if CPU_NTYPES != 0 && !defined(SUN4D)
#	define CPU_ISSUN4D	(0)
#elif CPU_NTYPES == 1 && defined(SUN4D)
#	define CPU_ISSUN4D	(1)
#else
#	define CPU_ISSUN4D	(cputyp == CPU_SUN4D)
#endif

#define	CPU_ISSUN4U		(0)

/*
 * Step 3: Sun4 machines have a page size of 8192.  All other machines
 * have a page size of 4096.  Short cut page size variables if we can.
 */
#if CPU_NTYPES != 0 && !defined(SUN4)
#	define NBPG		4096
#	define PGOFSET		(NBPG-1)
#	define PGSHIFT		SUN4CM_PGSHIFT
#elif CPU_NTYPES == 1 && defined(SUN4)
#	define NBPG		8192
#	define PGOFSET		(NBPG-1)
#	define PGSHIFT		SUN4_PGSHIFT
#else
#	define NBPG		nbpg
#	define PGOFSET		pgofset
#	define PGSHIFT		pgshift
#endif

/*
 * Step 4: Sun4M and Sun4D systems have an SRMMU.  Define some
 * short-hand for this.
 */
#define	CPU_HAS_SRMMU		(CPU_ISSUN4M || CPU_ISSUN4D)

#endif /* _KERNEL || _STANDALONE */
