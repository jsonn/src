/*	$NetBSD: intr.h,v 1.8.2.3 2001/04/21 17:54:29 bouyer Exp $	*/

/*
 * Copyright (c) 1998 Jonathan Stone.  All rights reserved.
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
 *	This product includes software developed by Jonathan Stone for
 *      the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _PMAX_INTR_H_
#define _PMAX_INTR_H_

#define	IPL_NONE	0	/* disable only this interrupt */
#define	IPL_BIO		1	/* disable block I/O interrupts */
#define	IPL_NET		2	/* disable network interrupts */
#define	IPL_TTY		3	/* disable terminal interrupts */
#define	IPL_CLOCK	4	/* disable clock interrupts */
#define	IPL_STATCLOCK	5	/* disable profiling interrupts */
#define	IPL_SERIAL	6	/* disable serial hardware interrupts */
#define	IPL_DMA		7	/* disable DMA reload interrupts */
#define	IPL_HIGH	8	/* disable all interrupts */

#ifdef _KERNEL
#ifndef _LOCORE

#include <mips/cpuregs.h>

int	_splraise __P((int));
int	_spllower __P((int));
int	_splset __P((int));
int	_splget __P((void));
void	_splnone __P((void));
void	_setsoftintr __P((int));
void	_clrsoftintr __P((int));

#define splhigh()	_splraise(MIPS_INT_MASK)
#define spl0()		(void)_spllower(0)
#define splx(s)		(void)_splset(s)
#define splbio()	(_splraise(splvec.splbio))
#define splnet()	(_splraise(splvec.splnet))
#define spltty()	(_splraise(splvec.spltty))
#define splvm()		(_splraise(splvec.splvm))
#define splclock()	(_splraise(splvec.splclock))
#define splstatclock()	(_splraise(splvec.splstatclock))
#define spllowersoftclock() _spllower(MIPS_SOFT_INT_MASK_0)
#define splsoftclock()	_splraise(MIPS_SOFT_INT_MASK_0)
#define splsoftnet()	_splraise(MIPS_SOFT_INT_MASK_0|MIPS_SOFT_INT_MASK_1)

#define	splsched()	splhigh()
#define	spllock()	splhigh()

struct splvec {
	int	splbio;
	int	splnet;
	int	spltty;
	int	splvm;
	int	splclock;
	int	splstatclock;
};
extern struct splvec splvec;

/* Conventionals ... */

#define MIPS_SPLHIGH (MIPS_INT_MASK)
#define MIPS_SPL0 (MIPS_INT_MASK_0|MIPS_SOFT_INT_MASK_0|MIPS_SOFT_INT_MASK_1)
#define MIPS_SPL1 (MIPS_INT_MASK_1|MIPS_SOFT_INT_MASK_0|MIPS_SOFT_INT_MASK_1)
#define MIPS_SPL3 (MIPS_INT_MASK_3|MIPS_SOFT_INT_MASK_0|MIPS_SOFT_INT_MASK_1)
#define MIPS_SPL_0_1	 (MIPS_INT_MASK_1|MIPS_SPL0)
#define MIPS_SPL_0_1_2	 (MIPS_INT_MASK_2|MIPS_SPL_0_1)
#define MIPS_SPL_0_1_3	 (MIPS_INT_MASK_3|MIPS_SPL_0_1)
#define MIPS_SPL_0_1_2_3 (MIPS_INT_MASK_3|MIPS_SPL_0_1_2)

/*
 * Index into intrcnt[], which is defined in locore
 */
extern u_long intrcnt[];

#define	SOFTCLOCK_INTR	0
#define	SOFTNET_INTR	1
#define	SERIAL0_INTR	2
#define	SERIAL1_INTR	3
#define	LANCE_INTR	4
#define	SCSI_INTR	5
#define	ERROR_INTR	6
#define	HARDCLOCK	7
#define	FPU_INTR	8
#define	SLOT0_INTR	9
#define	SLOT1_INTR	10
#define	SLOT2_INTR	11
#define	DTOP_INTR	12
#define	ISDN_INTR	13
#define	FLOPPY_INTR	14
#define	STRAY_INTR	15

struct intrhand {
	int	(*ih_func) __P((void *));
	void	*ih_arg;
};
extern struct intrhand intrtab[];

#define SYS_DEV_SCSI	SCSI_INTR
#define SYS_DEV_LANCE	LANCE_INTR
#define SYS_DEV_SCC0	SERIAL0_INTR
#define SYS_DEV_SCC1	SERIAL1_INTR
#define SYS_DEV_DTOP	DTOP_INTR
#define SYS_DEV_FDC	FLOPPY_INTR
#define SYS_DEV_ISDN	ISDN_INTR
#define SYS_DEV_OPT0	SLOT0_INTR
#define SYS_DEV_OPT1	SLOT1_INTR
#define SYS_DEV_OPT2	SLOT2_INTR
#define SYS_DEV_BOGUS	-1
#define MAX_DEV_NCOOKIES 16

/*
 * software simulated interrupt
 */
extern unsigned ssir;

#define SIR_NET		0x1

#define setsoftnet()	setsoft(SIR_NET)
#define setsoft(x) \
	do { ssir |= (x); _setsoftintr(MIPS_SOFT_INT_MASK_1); } while (0)

#define setsoftclock()	_setsoftintr(MIPS_SOFT_INT_MASK_0)

#endif /* !_LOCORE */
#endif /* _KERNEL */

#endif	/* !_PMAX_INTR_H_ */
