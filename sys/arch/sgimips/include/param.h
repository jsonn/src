/*	$NetBSD: param.h,v 1.2.2.2 2000/11/20 20:23:45 bouyer Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 */

#include <mips/mips_param.h>

#define	_MACHINE_ARCH	mipseb
#define	MACHINE_ARCH	"mipseb"
#define	_MACHINE	sgimips
#define	MACHINE		"sgimips"
#define	MID_MACHINE	MID_MIPS

#define	DEV_BSIZE	512
#define	DEV_BSHIFT	9		/* log2(DEV_BSIZE) */
#define BLKDEV_IOSIZE	2048
#define	MAXPHYS		(64 * 1024)	/* Maximum raw I/O transfer size */

#define	CLSIZE		1
#define	CLSIZELOG2	0

#define	MSIZE		256		/* Size of an mbuf */

#ifndef MCLSHIFT
#define	MCLSHIFT	11		/* Convert bytes to m_buf clusters. */
#endif

#define	MCLBYTES	(1 << MCLSHIFT)	/* Size of a m_buf cluster */
#define	MCLOFSET	(MCLBYTES - 1)

#if defined(_KERNEL) && !defined(_LKM)
#include "opt_gateway.h"
#endif /* _KERNEL && ! _LKM */

#ifndef NMBCLUSTERS
#ifdef GATEWAY
#define	NMBCLUSTERS	2048		/* Map size, max cluster allocation */
#else
#define	NMBCLUSTERS	1024		/* Map size, max cluster allocation */
#endif
#endif

#ifdef _KERNEL
#ifndef _LOCORE

__inline extern void	delay(unsigned long);
#define DELAY(n)	delay(n)

#include <machine/intr.h>

#endif	/* _LOCORE */
#endif	/* _KERNEL */
