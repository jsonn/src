/*	$NetBSD: ifpga_intr.h,v 1.7.6.1 2008/06/02 13:22:01 mjf Exp $	*/

/*
 * Copyright (c) 2001, 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _IFPGA_INTR_H_
#define _IFPGA_INTR_H_

#define	ARM_IRQ_HANDLER	_C_LABEL(ifpga_intr_dispatch)

#ifndef _LOCORE

#include <arm/armreg.h>
#include <arm/cpufunc.h>

#include <evbarm/ifpga/ifpgareg.h>
#include <evbarm/ifpga/ifpgavar.h>

static inline void __attribute__((__unused__))
ifpga_set_intrmask(void)
{
	extern volatile uint32_t intr_enabled;
	extern struct ifpga_softc *ifpga_sc;
	uint32_t mask = intr_enabled;

	bus_space_write_4 (ifpga_sc->sc_iot, ifpga_sc->sc_irq_ioh,
	    IFPGA_INTR_ENABLECLR, ~mask);
	bus_space_write_4 (ifpga_sc->sc_iot, ifpga_sc->sc_irq_ioh,
	    IFPGA_INTR_ENABLESET, mask);
}

static inline void __attribute__((__unused__))
ifpga_splx(int new)
{
	extern volatile uint32_t intr_enabled;
	extern volatile int ifpga_ipending;
	int oldirqstate, hwpend;

	__insn_barrier();

	oldirqstate = disable_interrupts(I32_bit);
	set_curcpl(new);

	hwpend = (ifpga_ipending & IFPGA_INTR_HWMASK) & ~new;
	if (hwpend != 0) {
		intr_enabled |= hwpend;
		ifpga_set_intrmask();
	}

	restore_interrupts(oldirqstate);

#ifdef __HAVE_FAST_SOFTINTS
	cpu_dosoftints();
#endif
}

static inline int __attribute__((__unused__))
ifpga_splraise(int ipl)
{
	extern int ifpga_imask[];
	const int old = curcpl();
	set_curcpl(old | ifpga_imask[ipl]);

	__insn_barrier();

	return (old);
}

static inline int __attribute__((__unused__))
ifpga_spllower(int ipl)
{
	extern int ifpga_imask[];
	const int old = curcpl();

	ifpga_splx(ifpga_imask[ipl]);
	return(old);
}

#if !defined(EVBARM_SPL_NOINLINE)

#define splx(new)		ifpga_splx(new)
#define	_spllower(ipl)		ifpga_spllower(ipl)
#define	_splraise(ipl)		ifpga_splraise(ipl)

#else

int	_splraise(int);
int	_spllower(int);
void	splx(int);

#endif /* ! EVBARM_SPL_NOINLINE */

#endif /* _LOCORE */

#endif /* _IFPGA_INTR_H_ */
