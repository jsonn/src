/*	$NetBSD: param.h,v 1.28.4.1 1997/03/11 20:59:39 is Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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

/*
 * Machine dependent constants for HP9000 series 300.
 */
#define	_MACHINE	hp300
#define	MACHINE		"hp300"
#define	_MACHINE_ARCH	m68k
#define	MACHINE_ARCH	"m68k"
#define	MID_MACHINE	MID_M68K4K

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
#define	ALIGNBYTES		(sizeof(int) - 1)
#define	ALIGN(p)		(((u_int)(p) + ALIGNBYTES) &~ ALIGNBYTES)
#define ALIGNED_POINTER(p,t)	((((u_long)(p)) & (sizeof(t)-1)) == 0)

#define	PGSHIFT		12		/* LOG2(NBPG) */
#define	NBPG		(1 << PGSHIFT)	/* bytes/page */
#define	PGOFSET		(NBPG-1)	/* byte offset into page */
#define	NPTEPG		(NBPG/(sizeof (pt_entry_t)))

#define	SEGSHIFT	22		/* LOG2(NBSEG) */
#define NBSEG		(1 << SEGSHIFT)	/* bytes/segment */
#define	SEGOFSET	(NBSEG-1)	/* byte offset into segment */

#define	KERNBASE	0x00000000	/* start of kernel virtual */
#define	BTOPKERNBASE	((u_long)KERNBASE >> PGSHIFT)

#define	DEV_BSHIFT	9		/* log2(DEV_BSIZE) */
#define	DEV_BSIZE	(1 << DEV_BSHIFT)
#define BLKDEV_IOSIZE	2048
#define	MAXPHYS		(64 * 1024)	/* max raw I/O transfer size */

#define	CLSIZELOG2	0
#define	CLSIZE		(1 << CLSIZELOG2)

/* NOTE: SSIZE, SINCR and UPAGES must be multiples of CLSIZE */
#define	SSIZE		1		/* initial stack size/NBPG */
#define	SINCR		1		/* increment of stack/NBPG */
#define	UPAGES		2		/* pages of u-area */
#define	USPACE		(UPAGES * NBPG)	/* total size of u-area */

/*
 * Constants related to network buffer management.
 * MCLBYTES must be no larger than CLBYTES (the software page size), and,
 * on machines that exchange pages of input or output buffers with mbuf
 * clusters (MAPPED_MBUFS), MCLBYTES must also be an integral multiple
 * of the hardware page size.
 */
#define	MSIZE		128		/* size of an mbuf */
#define	MCLSHIFT	11
#define	MCLBYTES	(1 << MCLSHIFT)	/* large enough for ether MTU */
#define	MCLOFSET	(MCLBYTES - 1)

#ifndef NMBCLUSTERS
#ifdef GATEWAY
#define	NMBCLUSTERS	512		/* map size, max cluster allocation */
#else
#define	NMBCLUSTERS	256		/* map size, max cluster allocation */
#endif
#endif

/*
 * Size of kernel malloc arena in CLBYTES-sized logical pages
 */ 
#ifndef NKMEMCLUSTERS
#define	NKMEMCLUSTERS	(2048 * 1024 / CLBYTES)
#endif

/* pages ("clicks") to disk blocks */
#define	ctod(x)		((x) << (PGSHIFT - DEV_BSHIFT))
#define	dtoc(x)		((x) >> (PGSHIFT - DEV_BSHIFT))

/* pages to bytes */
#define	ctob(x)		((x) << PGSHIFT)
#define	btoc(x)		(((x) + PGOFSET) >> PGSHIFT)

/* bytes to disk blocks */
#define	dbtob(x)	((x) << DEV_BSHIFT)
#define	btodb(x)	((x) >> DEV_BSHIFT)

/*
 * Map a ``block device block'' to a file system block.
 * This should be device dependent, and should use the bsize
 * field from the disk label.
 * For now though just use DEV_BSIZE.
 */
#define	bdbtofsb(bn)	((bn) / (BLKDEV_IOSIZE / DEV_BSIZE))

/*
 * Mach derived conversion macros
 */
#define hp300_round_page(x)	((((unsigned)(x)) + PGOFSET) & ~PGOFSET)
#define hp300_trunc_page(x)	((unsigned)(x) & ~PGOFSET)
#define hp300_btop(x)		((unsigned)(x) >> PGSHIFT)
#define hp300_ptob(x)		((unsigned)(x) << PGSHIFT)

/*
 * spl functions; all but spl0 are done in-line
 */
#include <machine/psl.h>

#define _spl(s) \
({ \
        register int _spl_r; \
\
        __asm __volatile ("clrl %0; movew sr,%0; movew %1,sr" : \
                "&=d" (_spl_r) : "di" (s)); \
        _spl_r; \
})

#define	_splraise(s) \
({ \
	register int _spl_r; \
\
	__asm __volatile ("clrl %0; movew sr,%0;" : "&=d" (_spl_r) : ); \
	if ((_spl_r & PSL_IPL) < ((s) & PSL_IPL)) \
		__asm __volatile ("movew %0,sr;" : : "di" (s)); \
	_spl_r; \
})

/* spl0 requires checking for software interrupts */
#define spl1()  _spl(PSL_S|PSL_IPL1)
#define spl2()  _spl(PSL_S|PSL_IPL2)
#define spl3()  _spl(PSL_S|PSL_IPL3)
#define spl4()  _spl(PSL_S|PSL_IPL4)
#define spl5()  _spl(PSL_S|PSL_IPL5)
#define spl6()  _spl(PSL_S|PSL_IPL6)
#define spl7()  _spl(PSL_S|PSL_IPL7)

#if defined(_KERNEL) && !defined(_LOCORE)
/*
 * These four globals contain the appropriate PSL_S|PSL_IPL? values
 * to raise interrupt priority to the requested level.
 */
extern	unsigned short hp300_bioipl;
extern	unsigned short hp300_netipl;
extern	unsigned short hp300_ttyipl;
extern	unsigned short hp300_impipl;
#endif /* _KERNEL && !_LOCORE */

/* These spl calls are _not_ to be used by machine-independent code. */
#define splhil()	_splraise(PSL_S|PSL_IPL1)
#define splkbd()	splhil()

/* These spl calls are used by machine-independent code. */
#define splsoftclock()	spl1()
#define splsoftnet()	spl1()
#define splbio()	_splraise(hp300_bioipl)
#define splnet()	_splraise(hp300_netipl)
#define spltty()	_splraise(hp300_ttyipl)
#define splimp()	_splraise(hp300_impipl)
#define splclock()	spl6()
#define splstatclock()	spl6()
#define splvm()		spl6()
#define splhigh()	spl7()
#define splsched()	spl7()

/* watch out for side effects */
#define splx(s)         (s & PSL_IPL ? _spl(s) : spl0())

#if defined(_KERNEL) && !defined(_LOCORE)
extern void _delay __P((u_int));
#define	delay(us)	_delay((us) << 8)
#define DELAY(us)	delay(us)
#endif /* _KERNEL && !_LOCORE */

#ifdef COMPAT_HPUX
/*
 * Constants/macros for HPUX multiple mapping of user address space.
 * Pages in the first 256Mb are mapped in at every 256Mb segment.
 */
#define HPMMMASK	0xF0000000
#define ISHPMMADDR(v) \
	((curproc->p_md.md_flags & MDP_HPUXMMAP) && \
	 ((unsigned)(v) & HPMMMASK) && \
	 ((unsigned)(v) & HPMMMASK) != HPMMMASK)
#define HPMMBASEADDR(v) \
	((unsigned)(v) & ~HPMMMASK)
#endif
