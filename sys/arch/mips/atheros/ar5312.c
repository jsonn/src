/* $NetBSD: ar5312.c,v 1.1.2.2 2006/09/03 15:23:21 yamt Exp $ */

/*
 * Copyright (c) 2006 Urbana-Champaign Independent Media Center.
 * Copyright (c) 2006 Garrett D'Amore.
 * All rights reserved.
 *
 * Portions of this code were written by Garrett D'Amore for the
 * Champaign-Urbana Community Wireless Network Project.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgements:
 *      This product includes software developed by the Urbana-Champaign
 *      Independent Media Center.
 *	This product includes software developed by Garrett D'Amore.
 * 4. Urbana-Champaign Independent Media Center's name and Garrett
 *    D'Amore's name may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE URBANA-CHAMPAIGN INDEPENDENT
 * MEDIA CENTER AND GARRETT D'AMORE ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE URBANA-CHAMPAIGN INDEPENDENT
 * MEDIA CENTER OR GARRETT D'AMORE BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This file includes a bunch of implementation specific bits for
 * AR5312, which differents these from other members of the AR5315
 * family.
 */
#include "opt_ddb.h"
#include "opt_kgdb.h"

#include "opt_memsize.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>

#include <mips/cache.h>
#include <mips/locore.h>
#include <mips/cpuregs.h>

#include <mips/atheros/include/ar5312reg.h>
#include <mips/atheros/include/ar531xvar.h>
#include <mips/atheros/include/arbusvar.h>
#include "com.h"

uint32_t
ar531x_memsize(void)
{
	uint32_t memsize;
	uint32_t memcfg, bank0, bank1;

	/*
	 * Determine the memory size.  Use the `memsize' PMON
	 * variable.  If that's not available, panic.
	 *
	 * NB: we allow compile time override
	 */
#if defined(MEMSIZE)
	memsize = MEMSIZE;
#else
	memcfg = GETSDRAMREG(AR5312_SDRAMCTL_MEM_CFG1);
	bank0 = (memcfg & AR5312_MEM_CFG1_BANK0_MASK) >>
	    AR5312_MEM_CFG1_BANK0_SHIFT;
	bank1 = (memcfg & AR5312_MEM_CFG1_BANK1_MASK) >>
	    AR5312_MEM_CFG1_BANK1_SHIFT;

	memsize = (bank0 ? (1 << (bank0 + 1)) : 0) +
	    (bank1 ? (1 << (bank1 + 1)) : 0);
	memsize <<= 20;
#endif

	return (memsize);
}

void
ar531x_wdog(uint32_t period)
{

	if (period == 0) {
		PUTSYSREG(AR5312_SYSREG_WDOG_CTL, AR5312_WDOG_CTL_IGNORE);
		PUTSYSREG(AR5312_SYSREG_WDOG_TIMER, 0);
	} else {
		PUTSYSREG(AR5312_SYSREG_WDOG_TIMER, period);
		PUTSYSREG(AR5312_SYSREG_WDOG_CTL, AR5312_WDOG_CTL_RESET);
	}
}

const char *
ar531x_cpuname(void)
{
	uint32_t	revision;

	revision = GETSYSREG(AR5312_SYSREG_REVISION);
	switch (AR5312_REVISION_MAJOR(revision)) {
	case AR5312_REVISION_MAJ_AR5311:
		return ("Atheros AR5311");
	case AR5312_REVISION_MAJ_AR5312:
		return ("Atheros AR5312");
	case AR5312_REVISION_MAJ_AR2313:
		return ("Atheros AR2313");
	case AR5312_REVISION_MAJ_AR5315:
		return ("Atheros AR5315");
	default:
		return ("Atheros AR531X");
	}
}

void
ar531x_consinit(void)
{
	/*
	 * Everything related to console initialization is done
	 * in mach_init().
	 */
#if NCOM > 0
	/* Setup polled serial for early console I/O */
	/* XXX: pass in CONSPEED? */
	com_arbus_cnattach(AR5312_UART0_BASE);
#else
	panic("Not configured to use serial console!\n");
	/* not going to see that message now, are we? */
#endif
}

void
ar531x_businit(void)

{
	/*
	 * Clear previous AHB errors
	 */
	GETSYSREG(AR5312_SYSREG_AHBPERR);
	GETSYSREG(AR5312_SYSREG_AHBDMAE);
}

uint32_t
ar531x_cpu_freq(void)
{
	static uint32_t	cpufreq;
	uint32_t	wisoc = GETSYSREG(AR5312_SYSREG_REVISION);

	uint32_t	predivmask;
	uint32_t	predivshift;
	uint32_t	multmask;
	uint32_t	multshift;
	uint32_t	doublermask;
	uint32_t	divisor;
	uint32_t	multiplier;
	uint32_t	clockctl;

	const int	predivide_table[4] = { 1, 2, 4, 5 };

	/* XXX: in theory we might be able to get clock from bootrom */

	/*
	 * This logic looks at the clock control register and
	 * determines the actual CPU frequency.  These parts lack any
	 * kind of real-time clock on them, but the cpu clocks should
	 * be very accurate -- WiFi requires usec resolution timers.
	 */

	if (cpufreq) {
		return cpufreq;
	}

	if (AR5312_REVISION_MAJOR(wisoc) == AR5312_REVISION_MAJ_AR2313) {
		predivmask = AR2313_CLOCKCTL_PREDIVIDE_MASK;
		predivshift = AR2313_CLOCKCTL_PREDIVIDE_SHIFT;
		multmask = AR2313_CLOCKCTL_MULTIPLIER_MASK;
		multshift = AR2313_CLOCKCTL_MULTIPLIER_SHIFT;
		doublermask = AR2313_CLOCKCTL_DOUBLER_MASK;
	} else {
		predivmask = AR5312_CLOCKCTL_PREDIVIDE_MASK;
		predivshift = AR5312_CLOCKCTL_PREDIVIDE_SHIFT;
		multmask = AR5312_CLOCKCTL_MULTIPLIER_MASK;
		multshift = AR5312_CLOCKCTL_MULTIPLIER_SHIFT;
		doublermask = AR5312_CLOCKCTL_DOUBLER_MASK;
	}

	/*
	 * Note that the source clock involved here is a 40MHz.
	 */

	clockctl = GETSYSREG(AR5312_SYSREG_CLOCKCTL);
	divisor = predivide_table[(clockctl & predivmask) >> predivshift];
	multiplier = (clockctl & multmask) >> multshift;

	if (clockctl & doublermask)
		multiplier <<= 1;

	cpufreq = (40000000 / divisor) * multiplier;

	return (cpufreq);
}

uint32_t
ar531x_sys_freq(void)
{
	return (ar531x_cpu_freq() / 4);
}
