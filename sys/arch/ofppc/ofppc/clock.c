/*	$NetBSD: clock.c,v 1.9.8.1 2005/11/29 21:23:03 yamt Exp $	*/

/*
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: clock.c,v 1.9.8.1 2005/11/29 21:23:03 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <dev/ofw/openfirm.h>

/*
 * Initially we assume a processor with a bus frequency of 12.5 MHz.
 */
static u_long ns_per_tick = 320;
long ticks_per_intr;
volatile u_long lasttb;

/*
 * For now we let the machine run with boot time, not changing the clock
 * at inittodr at all.
 *
 * We might continue to do this due to setting up the real wall clock with
 * a user level utility in the future.
 */
/* ARGSUSED */
void
inittodr(base)
	time_t base;
{
}

/*
 * Similar to the above
 */
void
resettodr()
{
}

void
decr_intr(frame)
	struct clockframe *frame;
{
	long ticks;
	int nticks;

	/*
	 * Check whether we are initialized.
	 */
	if (!ticks_per_intr)
		return;

	/*
	 * Based on the actual time delay since the last decrementer reload,
	 * we arrange for earlier interrupt next time.
	 */
	asm ("mfdec %0" : "=r"(ticks));
	for (nticks = 0; ticks < 0; nticks++)
		ticks += ticks_per_intr;
	asm volatile ("mtdec %0" :: "r"(ticks));

	clock_return(frame, nticks, ticks);
}

void
cpu_initclocks()
{
	struct cpu_softc *sc = cpu_cd.cd_devs[0];
	int msr, scratch;
	
	if (sc == NULL)
		panic("no cpu node");

	if (cpu_timebase == 0)
		panic("unknown timebase");

	/*
	 * Should check for correct CPU here?		XXX
	 */
	__asm __volatile ("mfmsr %0; andi. %1, %0, %2; mtmsr %1"
		: "=r"(msr), "=r"(scratch)
		: "K"((u_short)~PSL_EE));
	ns_per_tick = 1000000000 / cpu_timebase;
	ticks_per_intr = cpu_timebase / hz;
	__asm __volatile ("mftb %0" : "=r"(lasttb));
	__asm __volatile ("mtdec %0" :: "r"(ticks_per_intr));
	__asm __volatile ("mtmsr %0" :: "r"(msr));
}

/*
 * Fill in *tvp with current time with microsecond resolution.
 */
void
microtime(tvp)
	struct timeval *tvp;
{
	u_long tb;
	u_long ticks;
	int msr, scratch;
	
	asm volatile ("mfmsr %0; andi. %1,%0,%2; mtmsr %1"
		      : "=r"(msr), "=r"(scratch) : "K"((u_short)~PSL_EE));
	asm ("mftb %0" : "=r"(tb));
	ticks = (tb - lasttb) * ns_per_tick;
	*tvp = time;
	asm volatile ("mtmsr %0" :: "r"(msr));
	ticks /= 1000;
	tvp->tv_usec += ticks;
	while (tvp->tv_usec >= 1000000) {
		tvp->tv_usec -= 1000000;
		tvp->tv_sec++;
	}
}

/*
 * Wait for about n microseconds (at least!).
 */
void
delay(n)
	unsigned int n;
{
	u_quad_t tb;
	u_long tbh, tbl, scratch;
	
	tb = mftb();
	tb += (n * 1000 + ns_per_tick - 1) / ns_per_tick;
	tbh = tb >> 32;
	tbl = tb;
	asm volatile ("1: mftbu %0; cmplw %0,%1; blt 1b; bgt 2f;"
		      "mftb %0; cmplw %0,%2; blt 1b; 2:"
		      : "=&r"(scratch) : "r"(tbh), "r"(tbl));
}

/*
 * Nothing to do.
 */
void
setstatclockrate(arg)
	int arg;
{
	/* Do nothing */
}
