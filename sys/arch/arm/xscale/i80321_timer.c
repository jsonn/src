/*	$NetBSD: i80321_timer.c,v 1.13.20.1 2006/11/18 21:29:07 ad Exp $	*/

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

/*
 * Timer/clock support for the Intel i80321 I/O processor.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: i80321_timer.c,v 1.13.20.1 2006/11/18 21:29:07 ad Exp $");

#include "opt_perfctrs.h"
#include "opt_i80321.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/timetc.h>

#include <dev/clock_subr.h>

#include <machine/bus.h>
#include <arm/cpufunc.h>

#include <arm/xscale/i80321reg.h>
#include <arm/xscale/i80321var.h>

#include <arm/xscale/xscalevar.h>

void	(*i80321_hardclock_hook)(void);

#ifndef COUNTS_PER_SEC
#define	COUNTS_PER_SEC		200000000	/* 200MHz */
#endif
#define	COUNTS_PER_USEC		(COUNTS_PER_SEC / 1000000)

#ifdef __HAVE_TIMECOUNTER
static void tmr1_tc_init(void);
#endif

static void *clock_ih;

static uint32_t counts_per_hz;

int	clockhandler(void *);

static inline uint32_t
tmr0_read(void)
{
	uint32_t rv;

	__asm volatile("mrc p6, 0, %0, c0, c1, 0"
		: "=r" (rv));
	return (rv);
}

static inline void
tmr0_write(uint32_t val)
{

	__asm volatile("mcr p6, 0, %0, c0, c1, 0"
		:
		: "r" (val));
}

static inline uint32_t
tcr0_read(void)
{
	uint32_t rv;

	__asm volatile("mrc p6, 0, %0, c2, c1, 0"
		: "=r" (rv));
	return (rv);
}

static inline void
tcr0_write(uint32_t val)
{

	__asm volatile("mcr p6, 0, %0, c2, c1, 0"
		:
		: "r" (val));
}

static inline void
trr0_write(uint32_t val)
{

	__asm volatile("mcr p6, 0, %0, c4, c1, 0"
		:
		: "r" (val));
}

#ifdef __HAVE_TIMECOUNTER

static inline uint32_t
tmr1_read(void)
{
	uint32_t rv;

	__asm volatile("mrc p6, 0, %0, c1, c1, 0"
		: "=r" (rv));
	return (rv);
}

static inline void
tmr1_write(uint32_t val)
{

	__asm volatile("mcr p6, 0, %0, c1, c1, 0"
		:
		: "r" (val));
}

static inline uint32_t
tcr1_read(void)
{
	uint32_t rv;

	__asm volatile("mrc p6, 0, %0, c3, c1, 0"
		: "=r" (rv));
	return (rv);
}

static inline void
tcr1_write(uint32_t val)
{

	__asm volatile("mcr p6, 0, %0, c3, c1, 0"
		:
		: "r" (val));
}

static inline void
trr1_write(uint32_t val)
{

	__asm volatile("mcr p6, 0, %0, c5, c1, 0"
		:
		: "r" (val));
}

#endif /* __HAVE_TIMECOUNTER */

static inline void
tisr_write(uint32_t val)
{

	__asm volatile("mcr p6, 0, %0, c6, c1, 0"
		:
		: "r" (val));
}

/*
 * i80321_calibrate_delay:
 *
 *	Calibrate the delay loop.
 */
void
i80321_calibrate_delay(void)
{

	/*
	 * Just use hz=100 for now -- we'll adjust it, if necessary,
	 * in cpu_initclocks().
	 */
	counts_per_hz = COUNTS_PER_SEC / 100;

	tmr0_write(0);			/* stop timer */
	tisr_write(TISR_TMR0);		/* clear interrupt */
	trr0_write(counts_per_hz);	/* reload value */
	tcr0_write(counts_per_hz);	/* current value */

	tmr0_write(TMRx_ENABLE|TMRx_RELOAD|TMRx_CSEL_CORE);
}

/*
 * cpu_initclocks:
 *
 *	Initialize the clock and get them going.
 */
void
cpu_initclocks(void)
{
	u_int oldirqstate;
#if defined(PERFCTRS)
	void *pmu_ih;
#endif

	if (hz < 50 || COUNTS_PER_SEC % hz) {
		aprint_error("Cannot get %d Hz clock; using 100 Hz\n", hz);
		hz = 100;
	}
#ifndef __HAVE_TIMECOUNTER
	tick = 1000000 / hz;	/* number of microseconds between interrupts */
	tickfix = 1000000 - (hz * tick);
	if (tickfix) {
		int ftp;

		ftp = min(ffs(tickfix), ffs(hz));
		tickfix >>= (ftp - 1);
		tickfixinterval = hz >> (ftp - 1);
	}
#endif

	/*
	 * We only have one timer available; stathz and profhz are
	 * always left as 0 (the upper-layer clock code deals with
	 * this situation).
	 */
	if (stathz != 0)
		aprint_error("Cannot get %d Hz statclock\n", stathz);
	stathz = 0;

	if (profhz != 0)
		aprint_error("Cannot get %d Hz profclock\n", profhz);
	profhz = 0;

	/* Report the clock frequency. */
	aprint_normal("clock: hz=%d stathz=%d profhz=%d\n", hz, stathz, profhz);

	oldirqstate = disable_interrupts(I32_bit);

	/* Hook up the clock interrupt handler. */
	clock_ih = i80321_intr_establish(ICU_INT_TMR0, IPL_CLOCK,
	    clockhandler, NULL);
	if (clock_ih == NULL)
		panic("cpu_initclocks: unable to register timer interrupt");

#if defined(PERFCTRS)
	pmu_ih = i80321_intr_establish(ICU_INT_PMU, IPL_STATCLOCK,
	    xscale_pmc_dispatch, NULL);
	if (pmu_ih == NULL)
		panic("cpu_initclocks: unable to register timer interrupt");
#endif

	/* Set up the new clock parameters. */

	tmr0_write(0);			/* stop timer */
	tisr_write(TISR_TMR0);		/* clear interrupt */

	counts_per_hz = COUNTS_PER_SEC / hz;

	trr0_write(counts_per_hz);	/* reload value */
	tcr0_write(counts_per_hz);	/* current value */

	tmr0_write(TMRx_ENABLE|TMRx_RELOAD|TMRx_CSEL_CORE);

	restore_interrupts(oldirqstate);

#ifdef	__HAVE_TIMECOUNTER
	tmr1_tc_init();
#endif
}

/*
 * setstatclockrate:
 *
 *	Set the rate of the statistics clock.
 *
 *	We assume that hz is either stathz or profhz, and that neither
 *	will change after being set by cpu_initclocks().  We could
 *	recalculate the intervals here, but that would be a pain.
 */
void
setstatclockrate(int newhz)
{

	/*
	 * XXX Use TMR1?
	 */
}

#ifndef __HAVE_TIMECOUNTER

/*
 * microtime:
 *
 *	Fill in the specified timeval struct with the current time
 *	accurate to the microsecond.
 */
void
microtime(struct timeval *tvp)
{
	static struct timeval lasttv;
	u_int oldirqstate;
	uint32_t counts;

	oldirqstate = disable_interrupts(I32_bit);

	counts = counts_per_hz - tcr0_read();

	/* Fill in the timeval struct. */
	*tvp = time;
	tvp->tv_usec += (counts / COUNTS_PER_USEC);

	/* Make sure microseconds doesn't overflow. */
	while (tvp->tv_usec >= 1000000) {
		tvp->tv_usec -= 1000000;
		tvp->tv_sec++;
	}

	/* Make sure the time has advanced. */
	if (tvp->tv_sec == lasttv.tv_sec &&
	    tvp->tv_usec <= lasttv.tv_usec) {
		tvp->tv_usec = lasttv.tv_usec + 1;
		if (tvp->tv_usec >= 1000000) {
			tvp->tv_usec -= 1000000;
			tvp->tv_sec++;
		}
	}

	lasttv = *tvp;

	restore_interrupts(oldirqstate);
}


#else

static inline uint32_t
tmr1_tc_get(struct timecounter *tch)
{
	return (~tcr1_read());
}

void
tmr1_tc_init(void)
{
	static struct timecounter tmr1_tc = {
		.tc_get_timecount = tmr1_tc_get,
		.tc_frequency = COUNTS_PER_SEC,
		.tc_counter_mask = ~0,
		.tc_name = "tmr1_count",
		.tc_quality = 100,
	};

	/* program the tc */
	trr1_write(~0);	/* reload value */
	tcr1_write(~0);	/* current value */

	tmr1_write(TMRx_ENABLE|TMRx_RELOAD|TMRx_CSEL_CORE);


	trr1_write(~0);
	tc_init(&tmr1_tc);
}
#endif

/*
 * delay:
 *
 *	Delay for at least N microseconds.
 */
void
delay(u_int n)
{
	uint32_t cur, last, delta, usecs;

	/*
	 * This works by polling the timer and counting the
	 * number of microseconds that go by.
	 */
	last = tcr0_read();
	delta = usecs = 0;

	while (n > usecs) {
		cur = tcr0_read();

		/* Check to see if the timer has wrapped around. */
		if (last < cur)
			delta += (last + (counts_per_hz - cur));
		else
			delta += (last - cur);

		last = cur;

		if (delta >= COUNTS_PER_USEC) {
			usecs += delta / COUNTS_PER_USEC;
			delta %= COUNTS_PER_USEC;
		}
	}
}

#ifndef __HAVE_GENERIC_TODR
todr_chip_handle_t todr_handle;

/*
 * todr_attach:
 *
 *	Set the specified time-of-day register as the system real-time clock.
 */
void
todr_attach(todr_chip_handle_t todr)
{

	if (todr_handle)
		panic("todr_attach: rtc already configured");
	todr_handle = todr;
}

/*
 * inittodr:
 *
 *	Initialize time from the time-of-day register.
 */
#define	MINYEAR		2003	/* minimum plausible year */
void
inittodr(time_t base)
{
	time_t deltat;
	int badbase;

	if (base < (MINYEAR - 1970) * SECYR) {
		printf("WARNING: preposterous time in file system");
		/* read the system clock anyway */
		base = (MINYEAR - 1970) * SECYR;
		badbase = 1;
	} else
		badbase = 0;

	if (todr_handle == NULL ||
	    todr_gettime(todr_handle, &time) != 0 ||
	    time.tv_sec == 0) {
		/*
		 * Believe the time in the file system for lack of
		 * anything better, resetting the TODR.
		 */
		time.tv_sec = base;
		time.tv_usec = 0;
		if (todr_handle != NULL && !badbase) {
			printf("WARNING: preposterous clock chip time\n");
			resettodr();
		}
		goto bad;
	}

	if (!badbase) {
		/*
		 * See if we gained/lost two or more days; if
		 * so, assume something is amiss.
		 */
		deltat = time.tv_sec - base;
		if (deltat < 0)
			deltat = -deltat;
		if (deltat < 2 * SECDAY)
			return;		/* all is well */
		printf("WARNING: clock %s %ld days\n",
		    time.tv_sec < base ? "lost" : "gained",
		    (long)deltat / SECDAY);
	}
 bad:
	printf("WARNING: CHECK AND RESET THE DATE!\n");
}

/*
 * resettodr:
 *
 *	Reset the time-of-day register with the current time.
 */
void
resettodr(void)
{

	if (time.tv_sec == 0)
		return;

	if (todr_handle != NULL &&
	    todr_settime(todr_handle, &time) != 0)
		printf("resettodr: failed to set time\n");
}
#endif

/*
 * clockhandler:
 *
 *	Handle the hardclock interrupt.
 */
int
clockhandler(void *arg)
{
	struct clockframe *frame = arg;

	tisr_write(TISR_TMR0);

	hardclock(frame);

	if (i80321_hardclock_hook != NULL)
		(*i80321_hardclock_hook)();

	return (1);
}
