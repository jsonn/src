/*	$NetBSD: param.h,v 1.12.2.2 2002/10/10 18:35:19 jdolecek Exp $	*/

/*-
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
 * All rights reserved.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _POWERPC_PARAM_H
#define	_POWERPC_PARAM_H

#ifdef	_KERNEL
#ifndef	_LOCORE
#include <machine/cpu.h>
#endif	/* _LOCORE */
#endif

/*
 * Machine dependent constants for PowerPC (32-bit only currently)
 */
#ifndef MACHINE
#define	MACHINE		"powerpc"
#endif
#define	MACHINE_ARCH	"powerpc"
#define	MID_MACHINE	MID_POWERPC

#define	ALIGNBYTES		(sizeof(double) - 1)
#define	ALIGN(p)		(((u_int)(p) + ALIGNBYTES) & ~ALIGNBYTES)
#define ALIGNED_POINTER(p,t)	((((u_long)(p)) & (sizeof(t)-1)) == 0)

#define	PGSHIFT		12
#define	NBPG		(1 << PGSHIFT)	/* Page size */
#define	PGOFSET		(NBPG - 1)

#define	DEV_BSHIFT	9		/* log2(DEV_BSIZE) */
#define	DEV_BSIZE	(1 << DEV_BSHIFT)
#define	BLKDEV_IOSIZE	NBPG
#ifndef MAXPHYS
#define	MAXPHYS		(64 * 1024)	/* max raw I/O transfer size */
#endif

#define	UPAGES		4
#define	USPACE		(UPAGES * NBPG)

#ifndef	MSGBUFSIZE
#define	MSGBUFSIZE	(2*NBPG)	/* default message buffer size */
#endif

#ifndef KERNBASE
#define	KERNBASE	0x100000
#endif

/*
 * Constants related to network buffer management.
 * MCLBYTES must be no larger than NBPG (the software page size), and,
 * on machines that exchange pages of input or output buffers with mbuf
 * clusters (MAPPED_MBUFS), MCLBYTES must also be an integral multiple
 * of the hardware page size.
 */
#define	MSIZE		256		/* size of an mbuf */
#define	MCLSHIFT	11		/* convert bytes to m_buf clusters */
#define	MCLBYTES	(1 << MCLSHIFT)	/* size of a m_buf cluster */

#ifndef NMBCLUSTERS

#if defined(_KERNEL_OPT)
#include "opt_gateway.h"
#endif

#ifdef GATEWAY
#define	NMBCLUSTERS	2048		/* map size, max cluster allocation */
#else
#define	NMBCLUSTERS	1024		/* map size, max cluster allocation */
#endif
#endif

/*
 * Minimum and maximum sizes of the kernel malloc arena in PAGE_SIZE-sized
 * logical pages.
 */
#ifndef NKMEMPAGES_MIN_DEFAULT
#define	NKMEMPAGES_MIN_DEFAULT	((16 * 1024 * 1024) >> PAGE_SHIFT)
#endif
#ifndef NKMEMPAGES_MAX_DEFAULT
#define	NKMEMPAGES_MAX_DEFAULT	((128 * 1024 * 1024) >> PAGE_SHIFT)
#endif

/*
 * pages ("clicks") to disk blocks
 */
#define	ctod(x)		((x) << (PGSHIFT - DEV_BSHIFT))
#define	dtoc(x)		((x) >> (PGSHIFT - DEV_BSHIFT))
/*
 * bytes to pages
 */
#define	ctob(x)		((x) << PGSHIFT)
#define	btoc(x)		(((x) + PGOFSET) >> PGSHIFT)

/*
 * bytes to disk blocks
 */
#define	dbtob(x)	((x) << DEV_BSHIFT)
#define	btodb(x)	((x) >> DEV_BSHIFT)

#ifdef OLDPMAP
/*
 * Segment handling stuff
 */
#define	SEGMENT_LENGTH	0x10000000
#define	SEGMENT_MASK	0xf0000000

/*
 * Fixed segments
 */
#define	USER_SR		13
#define	KERNEL_SR	14
#define	KERNEL_SEGMENT	(0xf00000 + KERNEL_SR)
#define	EMPTY_SEGMENT	0xfffff0
#define	USER_ADDR	((void *)(USER_SR << ADDR_SR_SHFT))

/*
 * Some system constants
 */
#ifndef	NPMAPS
#define	NPMAPS		32768	/* Number of pmaps in system */
#endif

#endif /* OLDPMAP */

#endif /* _POWERPC_PARAM_H_ */
