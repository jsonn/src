/*	$NetBSD: clock.c,v 1.67.2.3 2002/10/10 18:33:31 jdolecek Exp $	*/

/*-
 * Copyright (c) 1993, 1994 Charles M. Hannum.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz and Don Ahn.
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
 *	@(#)clock.c	7.2 (Berkeley) 5/12/91
 */
/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
  Copyright 1988, 1989 by Intel Corporation, Santa Clara, California.

		All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Intel
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

INTEL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL INTEL BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/*
 * Primitive clock interrupt routines.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: clock.c,v 1.67.2.3 2002/10/10 18:33:31 jdolecek Exp $");

/* #define CLOCKDEBUG */
/* #define CLOCK_PARANOIA */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/pio.h>
#include <machine/cpufunc.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/ic/mc146818reg.h>
#include <dev/ic/i8253reg.h>
#include <i386/isa/nvram.h>
#include <dev/clock_subr.h>
#include <machine/specialreg.h> 

#include "mca.h"
#if NMCA > 0
#include <machine/mca_machdep.h>	/* for MCA_system */
#endif

#include "pcppi.h"
#if (NPCPPI > 0)
#include <dev/isa/pcppivar.h>

#ifdef CLOCKDEBUG
int clock_debug = 0;
#define DPRINTF(arg) if (clock_debug) printf arg
#else
#define DPRINTF(arg)
#endif

int sysbeepmatch __P((struct device *, struct cfdata *, void *));
void sysbeepattach __P((struct device *, struct device *, void *));

CFATTACH_DECL(sysbeep, sizeof(struct device),
    sysbeepmatch, sysbeepattach, NULL, NULL);

static int ppi_attached;
static pcppi_tag_t ppicookie;
#endif /* PCPPI */

void	spinwait __P((int));
int	clockintr __P((void *));
int	gettick __P((void));
void	sysbeep __P((int, int));
void	rtcinit __P((void));
int	rtcget __P((mc_todregs *));
void	rtcput __P((mc_todregs *));
int 	bcdtobin __P((int));
int	bintobcd __P((int));

static inline int gettick_broken_latch __P((void));


__inline u_int mc146818_read __P((void *, u_int));
__inline void mc146818_write __P((void *, u_int, u_int));

__inline u_int
mc146818_read(sc, reg)
	void *sc;					/* XXX use it? */
	u_int reg;
{

	outb(IO_RTC, reg);
	return (inb(IO_RTC+1));
}

__inline void
mc146818_write(sc, reg, datum)
	void *sc;					/* XXX use it? */
	u_int reg, datum;
{

	outb(IO_RTC, reg);
	outb(IO_RTC+1, datum);
}

u_long rtclock_tval;
int clock_broken_latch = 0;

#ifdef CLOCK_PARANOIA
static int ticks[6];
#endif
/*
 * i8254 latch check routine:
 *     National Geode (formerly Cyrix MediaGX) has a serious bug in
 *     its built-in i8254-compatible clock module.
 *     machdep sets the variable 'clock_broken_latch' to indicate it.
 */

int
gettick_broken_latch()
{
	u_long ef;
	int v1, v2, v3;
	int w1, w2, w3;

	/* Don't want someone screwing with the counter
	   while we're here. */
	ef = read_eflags();
	disable_intr();

	v1 = inb(IO_TIMER1+TIMER_CNTR0);
	v1 |= inb(IO_TIMER1+TIMER_CNTR0) << 8;
	v2 = inb(IO_TIMER1+TIMER_CNTR0);
	v2 |= inb(IO_TIMER1+TIMER_CNTR0) << 8;
	v3 = inb(IO_TIMER1+TIMER_CNTR0);
	v3 |= inb(IO_TIMER1+TIMER_CNTR0) << 8;

	write_eflags(ef);

#ifdef CLOCK_PARANOIA
	if (clock_debug) {
		ticks[0] = ticks[3];
		ticks[1] = ticks[4];
		ticks[2] = ticks[5];
		ticks[3] = v1;
		ticks[4] = v2;
		ticks[5] = v3;
	}
#endif

	if (v1 >= v2 && v2 >= v3 && v1 - v3 < 0x200)
		return (v2);

#define _swap_val(a, b) do { \
	int c = a; \
	a = b; \
	b = c; \
} while (0)

	/*
	 * sort v1 v2 v3
	 */
	if (v1 < v2)
		_swap_val(v1, v2);
	if (v2 < v3)
		_swap_val(v2, v3);
	if (v1 < v2)
		_swap_val(v1, v2);

	/*
	 * compute the middle value
	 */

	if (v1 - v3 < 0x200)
		return (v2);

	w1 = v2 - v3;
	w2 = v3 - v1 + rtclock_tval;
	w3 = v1 - v2;
	if (w1 >= w2) {
		if (w1 >= w3)
		        return (v1);
	} else {
		if (w2 >= w3)
			return (v2);
	}
	return (v3);
}

/* minimal initialization, enough for delay() */
void
initrtclock()
{
	u_long tval;

	/*
	 * Compute timer_count, the count-down count the timer will be
	 * set to.  Also, correctly round
	 * this by carrying an extra bit through the division.
	 */
	tval = (TIMER_FREQ * 2) / (u_long) hz;
	tval = (tval / 2) + (tval & 0x1);

	/* initialize 8253 clock */
	outb(IO_TIMER1+TIMER_MODE, TIMER_SEL0|TIMER_RATEGEN|TIMER_16BIT);

	/* Correct rounding will buy us a better precision in timekeeping */
	outb(IO_TIMER1+TIMER_CNTR0, tval % 256);
	outb(IO_TIMER1+TIMER_CNTR0, tval / 256);

	rtclock_tval = tval;
}

/*
 * microtime() makes use of the following globals.  Note that isa_timer_tick
 * may be redundant to the `tick' variable, but is kept here for stability.
 * isa_timer_count is the countdown count for the timer.  timer_msb_table[]
 * and timer_lsb_table[] are used to compute the microsecond increment
 * for time.tv_usec in the follow fashion:
 *
 * time.tv_usec += isa_timer_msb_table[cnt_msb] - isa_timer_lsb_table[cnt_lsb];
 */
#define	ISA_TIMER_MSB_TABLE_SIZE	128

u_long	isa_timer_tick;		/* the number of microseconds in a tick */
u_short	isa_timer_count;	/* the countdown count for the timer */
u_short	isa_timer_msb_table[ISA_TIMER_MSB_TABLE_SIZE];	/* timer->usec MSB */
u_short	isa_timer_lsb_table[256];	/* timer->usec conversion for LSB */

void
startrtclock()
{
	int s;
	u_long tval;
	u_long t, msb, lsb, quotient, remainder;

	if (!rtclock_tval)
		initrtclock();

	/*
	 * Compute timer_tick from hz.  We truncate this value (i.e.
	 * round down) to minimize the possibility of a backward clock
	 * step if hz is not a nice number.
	 */
	isa_timer_tick = 1000000 / (u_long) hz;

	/*
	 * We can't stand any number with an MSB larger than
	 * TIMER_MSB_TABLE_SIZE will accomodate.
	 */
	tval = rtclock_tval;
	if ((tval / 256) >= ISA_TIMER_MSB_TABLE_SIZE
	    || TIMER_FREQ > (8*1024*1024)) {
		panic("startrtclock: TIMER_FREQ/HZ unsupportable");
	}
	isa_timer_count = (u_short) tval;

	/*
	 * Now compute the translation tables from timer ticks to
	 * microseconds.  We go to some length to ensure all values
	 * are rounded-to-nearest (i.e. +-0.5 of the exact values)
	 * as this will ensure the computation
	 *
	 * isa_timer_msb_table[msb] - isa_timer_lsb_table[lsb]
	 *
	 * will produce a result which is +-1 usec away from the
	 * correctly rounded conversion (in fact, it'll be exact about
	 * 75% of the time, 1 too large 12.5% of the time, and 1 too
	 * small 12.5% of the time).
	 */
	for (s = 0; s < 256; s++) {
		/* LSB table is easy, just divide and round */
		t = ((u_long) s * 1000000 * 2) / TIMER_FREQ;
		isa_timer_lsb_table[s] = (u_short) ((t / 2) + (t & 0x1));

		/* MSB table is zero unless the MSB is <= isa_timer_count */
		if (s < ISA_TIMER_MSB_TABLE_SIZE) {
			msb = ((u_long) s) * 256;
			if (msb > tval) {
				isa_timer_msb_table[s] = 0;
			} else {
				/*
				 * Harder computation here, since multiplying
				 * the value by 1000000 can overflow a long.
				 * To avoid 64-bit computations we divide
				 * the high order byte and the low order
				 * byte of the numerator separately, adding
				 * the remainder of the first computation
				 * into the second.  The constraint on
				 * TIMER_FREQ above should prevent overflow
				 * here.
				 */
				msb = tval - msb;
				lsb = msb % 256;
				msb = (msb / 256) * 1000000;
				quotient = msb / TIMER_FREQ;
				remainder = msb % TIMER_FREQ;
				t = ((remainder * 256 * 2)
				    + (lsb * 1000000 * 2)) / TIMER_FREQ;
				isa_timer_msb_table[s] = (u_short)((t / 2)
				    + (t & 0x1) + (quotient * 256));
			}
		}
	}

	/* Check diagnostic status */
	if ((s = mc146818_read(NULL, NVRAM_DIAG)) != 0) { /* XXX softc */
		char bits[128];
		printf("RTC BIOS diagnostic error %s\n",
		    bitmask_snprintf(s, NVRAM_DIAG_BITS, bits, sizeof(bits)));
	}
}

int
clockintr(arg)
	void *arg;
{
	struct clockframe *frame = arg;		/* not strictly necessary */
#if defined(I586_CPU) || defined(I686_CPU)
	static int microset_iter; /* call tsc_microset once/sec */
	struct cpu_info *ci = curcpu();
	extern struct timeval tsc_time;
	
	/*
	 * If we have a cycle counter, do the microset thing.
	 */
	if (ci->ci_feature_flags & CPUID_TSC) {
		if (
#if defined(MULTIPROCESSOR)
		    CPU_IS_PRIMARY(ci) &&
#endif
		    (microset_iter--) == 0) {
			tsc_time = time;
			microset_iter = hz-1;
#if defined(MULTIPROCESSOR)
			i386_broadcast_ipi(I386_IPI_MICROSET);
#endif
			tsc_microset(ci);
		}
	}
#endif

	hardclock(frame);

#if NMCA > 0
	if (MCA_system) {
		/* Reset PS/2 clock interrupt by asserting bit 7 of port 0x61 */
		outb(0x61, inb(0x61) | 0x80);
	}
#endif
	return -1;
}

int
gettick()
{
	u_long ef;
	u_char lo, hi;

	if (clock_broken_latch)
		return (gettick_broken_latch());

	/* Don't want someone screwing with the counter while we're here. */
	ef = read_eflags();
	disable_intr();
	/* Select counter 0 and latch it. */
	outb(IO_TIMER1+TIMER_MODE, TIMER_SEL0 | TIMER_LATCH);
	lo = inb(IO_TIMER1+TIMER_CNTR0);
	hi = inb(IO_TIMER1+TIMER_CNTR0);
	write_eflags(ef);
	return ((hi << 8) | lo);
}

/*
 * Wait approximately `n' microseconds.
 * Relies on timer 1 counting down from (TIMER_FREQ / hz) at TIMER_FREQ Hz.
 * Note: timer had better have been programmed before this is first used!
 * (Note that we use `rate generator' mode, which counts at 1:1; `square
 * wave' mode counts at 2:1).
 * Don't rely on this being particularly accurate.
 */
void
i8254_delay(n)
	int n;
{
	int tick, otick;
	static const int delaytab[26] = {
		 0,  2,  3,  4,  5,  6,  7,  9, 10, 11,
		12, 13, 15, 16, 17, 18, 19, 21, 22, 23,
		24, 25, 27, 28, 29, 30,
	};

	/* allow DELAY() to be used before startrtclock() */
	if (!rtclock_tval)
		initrtclock();

	/*
	 * Read the counter first, so that the rest of the setup overhead is
	 * counted.
	 */
	otick = gettick();

	if (n <= 25)
		n = delaytab[n];
	else {
#ifdef __GNUC__
		/*
		 * Calculate ((n * TIMER_FREQ) / 1e6) using explicit assembler
		 * code so we can take advantage of the intermediate 64-bit
		 * quantity to prevent loss of significance.
		 */
		int m;
		__asm __volatile("mul %3"
				 : "=a" (n), "=d" (m)
				 : "0" (n), "r" (TIMER_FREQ));
		__asm __volatile("div %4"
				 : "=a" (n), "=d" (m)
				 : "0" (n), "1" (m), "r" (1000000));
#else
		/*
		 * Calculate ((n * TIMER_FREQ) / 1e6) without using floating
		 * point and without any avoidable overflows.
		 */
		int sec = n / 1000000,
		    usec = n % 1000000;
		n = sec * TIMER_FREQ +
		    usec * (TIMER_FREQ / 1000000) +
		    usec * ((TIMER_FREQ % 1000000) / 1000) / 1000 +
		    usec * (TIMER_FREQ % 1000) / 1000000;
#endif
	}

	while (n > 0) {
#ifdef CLOCK_PARANOIA
		int delta;
		tick = gettick();
		if (tick > otick)
			delta = rtclock_tval - (tick - otick);
		else
			delta = otick - tick;
		if (delta < 0 || delta >= rtclock_tval / 2) {
			DPRINTF(("delay: ignore ticks %.4x-%.4x",
				 otick, tick));
			if (clock_broken_latch) {
				DPRINTF(("  (%.4x %.4x %.4x %.4x %.4x %.4x)\n",
				         ticks[0], ticks[1], ticks[2],
				         ticks[3], ticks[4], ticks[5]));
			} else {
				DPRINTF(("\n"));
			}
		} else
			n -= delta;
#else
		tick = gettick();
		if (tick > otick)
			n -= rtclock_tval - (tick - otick);
		else
			n -= otick - tick;
#endif
		otick = tick;
	}
}

#if (NPCPPI > 0)
int
sysbeepmatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	return (!ppi_attached);
}

void
sysbeepattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	printf("\n");

	ppicookie = ((struct pcppi_attach_args *)aux)->pa_cookie;
	ppi_attached = 1;
}
#endif

void
sysbeep(pitch, period)
	int pitch, period;
{
#if (NPCPPI > 0)
	if (ppi_attached)
		pcppi_bell(ppicookie, pitch, period, 0);
#endif
}

void
i8254_initclocks()
{

	/*
	 * XXX If you're doing strange things with multiple clocks, you might
	 * want to keep track of clock handlers.
	 */
	(void)isa_intr_establish(NULL, 0, IST_PULSE, IPL_CLOCK, clockintr, 0);
}

void
rtcinit()
{
	static int first_rtcopen_ever = 1;

	if (!first_rtcopen_ever)
		return;
	first_rtcopen_ever = 0;

	mc146818_write(NULL, MC_REGA,			/* XXX softc */
	    MC_BASE_32_KHz | MC_RATE_1024_Hz);
	mc146818_write(NULL, MC_REGB, MC_REGB_24HR);	/* XXX softc */
}

int
rtcget(regs)
	mc_todregs *regs;
{

	rtcinit();
	if ((mc146818_read(NULL, MC_REGD) & MC_REGD_VRT) == 0) /* XXX softc */
		return (-1);
	MC146818_GETTOD(NULL, regs);			/* XXX softc */
	return (0);
}	

void
rtcput(regs)
	mc_todregs *regs;
{

	rtcinit();
	MC146818_PUTTOD(NULL, regs);			/* XXX softc */
}

int
bcdtobin(n)
	int n;
{

	return (((n >> 4) & 0x0f) * 10 + (n & 0x0f));
}

int
bintobcd(n)
	int n;
{

	return ((u_char)(((n / 10) << 4) & 0xf0) | ((n % 10) & 0x0f));
}

static int timeset;

/*
 * check whether the CMOS layout is "standard"-like (ie, not PS/2-like),
 * to be called at splclock()
 */
static int cmoscheck __P((void));
static int
cmoscheck()
{
	int i;
	unsigned short cksum = 0;

	for (i = 0x10; i <= 0x2d; i++)
		cksum += mc146818_read(NULL, i); /* XXX softc */

	return (cksum == (mc146818_read(NULL, 0x2e) << 8)
			  + mc146818_read(NULL, 0x2f));
}

#if NMCA > 0
/*
 * Check whether the CMOS layout is PS/2 like, to be called at splclock().
 */
static int cmoscheckps2 __P((void));
static int
cmoscheckps2()
{
#if 0
	/* Disabled until I find out the CRC checksum algorithm IBM uses */
	int i;
	unsigned short cksum = 0;

	for (i = 0x10; i <= 0x31; i++)
		cksum += mc146818_read(NULL, i); /* XXX softc */

	return (cksum == (mc146818_read(NULL, 0x32) << 8)
			  + mc146818_read(NULL, 0x33));
#else
	/* Check 'incorrect checksum' bit of IBM PS/2 Diagnostic Status Byte */
	return ((mc146818_read(NULL, NVRAM_DIAG) & (1<<6)) == 0);
#endif
}
#endif /* NMCA > 0 */

/*
 * patchable to control century byte handling:
 * 1: always update
 * -1: never touch
 * 0: try to figure out itself
 */
int rtc_update_century = 0;

/*
 * Expand a two-digit year as read from the clock chip
 * into full width.
 * Being here, deal with the CMOS century byte.
 */
static int centb = NVRAM_CENTURY;
static int clock_expandyear __P((int));
static int
clock_expandyear(clockyear)
	int clockyear;
{
	int s, clockcentury, cmoscentury;

	clockcentury = (clockyear < 70) ? 20 : 19;
	clockyear += 100 * clockcentury;

	if (rtc_update_century < 0)
		return (clockyear);

	s = splclock();
	if (cmoscheck())
		cmoscentury = mc146818_read(NULL, NVRAM_CENTURY);
#if NMCA > 0
	else if (MCA_system && cmoscheckps2())
		cmoscentury = mc146818_read(NULL, (centb = 0x37));
#endif
	else
		cmoscentury = 0;
	splx(s);
	if (!cmoscentury) {
#ifdef DIAGNOSTIC
		printf("clock: unknown CMOS layout\n");
#endif
		return (clockyear);
	}
	cmoscentury = bcdtobin(cmoscentury);

	if (cmoscentury != clockcentury) {
		/* XXX note: saying "century is 20" might confuse the naive. */
		printf("WARNING: NVRAM century is %d but RTC year is %d\n",
		       cmoscentury, clockyear);

		/* Kludge to roll over century. */
		if ((rtc_update_century > 0) ||
		    ((cmoscentury == 19) && (clockcentury == 20) &&
		     (clockyear == 2000))) {
			printf("WARNING: Setting NVRAM century to %d\n",
			       clockcentury);
			s = splclock();
			mc146818_write(NULL, centb, bintobcd(clockcentury));
			splx(s);
		}
	} else if (cmoscentury == 19 && rtc_update_century == 0)
		rtc_update_century = 1; /* will update later in resettodr() */

	return (clockyear);
}

/*
 * Initialize the time of day register, based on the time base which is, e.g.
 * from a filesystem.
 */
void
inittodr(base)
	time_t base;
{
	mc_todregs rtclk;
	struct clock_ymdhms dt;
	int s;
#if defined(I586_CPU) || defined(I686_CPU)
	struct cpu_info *ci = curcpu();
	extern struct timeval tsc_time;
#endif
	/*
	 * We mostly ignore the suggested time (which comes from the
	 * file system) and go for the RTC clock time stored in the
	 * CMOS RAM.  If the time can't be obtained from the CMOS, or
	 * if the time obtained from the CMOS is 5 or more years less
	 * than the suggested time, we used the suggested time.  (In
	 * the latter case, it's likely that the CMOS battery has
	 * died.)
	 */

	/*
	 * XXX Traditionally, the dates in this code snippet get
	 * updated every few years. It would be neater if they could
	 * somehow be automatically set when the kernel was built.
	 */
	if (base && base < 30*SECYR) {	/* if before 2000, something's odd. */
		printf("WARNING: preposterous time in file system\n");
		/* Since the fs time is silly, set the base time to 2002 */
		base = 32*SECYR;
	}

	s = splclock();
	if (rtcget(&rtclk)) {
		splx(s);
		printf("WARNING: invalid time in clock chip\n");
		goto fstime;
	}
	splx(s);
#ifdef DEBUG_CLOCK
	printf("readclock: %x/%x/%x %x:%x:%x\n", rtclk[MC_YEAR],
	    rtclk[MC_MONTH], rtclk[MC_DOM], rtclk[MC_HOUR], rtclk[MC_MIN],
	    rtclk[MC_SEC]);
#endif

	dt.dt_sec = bcdtobin(rtclk[MC_SEC]);
	dt.dt_min = bcdtobin(rtclk[MC_MIN]);
	dt.dt_hour = bcdtobin(rtclk[MC_HOUR]);
	dt.dt_day = bcdtobin(rtclk[MC_DOM]);
	dt.dt_mon = bcdtobin(rtclk[MC_MONTH]);
	dt.dt_year = clock_expandyear(bcdtobin(rtclk[MC_YEAR]));

	/*
	 * If time_t is 32 bits, then the "End of Time" is 
	 * Mon Jan 18 22:14:07 2038 (US/Eastern)
	 * This code copes with RTC's past the end of time if time_t
	 * is an int32 or less. Needed because sometimes RTCs screw
	 * up or are badly set, and that would cause the time to go
	 * negative in the calculation below, which causes Very Bad
	 * Mojo. This at least lets the user boot and fix the problem.
	 * Note the code is self eliminating once time_t goes to 64 bits.
	 */
	if (sizeof(time_t) <= sizeof(int32_t)) {
		if (dt.dt_year >= 2038) {
			printf("WARNING: RTC time at or beyond 2038.\n");
			dt.dt_year = 2037;
			printf("WARNING: year set back to 2037.\n");
			printf("WARNING: CHECK AND RESET THE DATE!\n");
		}
	}

	time.tv_sec = clock_ymdhms_to_secs(&dt) + rtc_offset * 60;
#ifdef DEBUG_CLOCK
	printf("readclock: %ld (%ld)\n", time.tv_sec, base);
#endif
#if defined(I586_CPU) || defined(I686_CPU)
	if (ci->ci_feature_flags & CPUID_TSC) {
		tsc_time = time;
		tsc_microset(ci);
	}
#endif

	if (base != 0 && base < time.tv_sec - 5*SECYR)
		printf("WARNING: file system time much less than clock time\n");
	else if (base > time.tv_sec + 5*SECYR) {
		printf("WARNING: clock time much less than file system time\n");
		printf("WARNING: using file system time\n");
		goto fstime;
	}

	timeset = 1;
	return;

fstime:
	timeset = 1;
	time.tv_sec = base;
	printf("WARNING: CHECK AND RESET THE DATE!\n");
}

/*
 * Reset the clock.
 */
void
resettodr()
{
	mc_todregs rtclk;
	struct clock_ymdhms dt;
	int century;
	int s;

	/*
	 * We might have been called by boot() due to a crash early
	 * on.  Don't reset the clock chip in this case.
	 */
	if (!timeset)
		return;

	s = splclock();
	if (rtcget(&rtclk))
		memset(&rtclk, 0, sizeof(rtclk));
	splx(s);

	clock_secs_to_ymdhms(time.tv_sec - rtc_offset * 60, &dt);

	rtclk[MC_SEC] = bintobcd(dt.dt_sec);
	rtclk[MC_MIN] = bintobcd(dt.dt_min);
	rtclk[MC_HOUR] = bintobcd(dt.dt_hour);
	rtclk[MC_DOW] = dt.dt_wday;
	rtclk[MC_YEAR] = bintobcd(dt.dt_year % 100);
	rtclk[MC_MONTH] = bintobcd(dt.dt_mon);
	rtclk[MC_DOM] = bintobcd(dt.dt_day);

#ifdef DEBUG_CLOCK
	printf("setclock: %x/%x/%x %x:%x:%x\n", rtclk[MC_YEAR], rtclk[MC_MONTH],
	   rtclk[MC_DOM], rtclk[MC_HOUR], rtclk[MC_MIN], rtclk[MC_SEC]);
#endif
	s = splclock();
	rtcput(&rtclk);
	if (rtc_update_century > 0) {
		century = bintobcd(dt.dt_year / 100);
		mc146818_write(NULL, centb, century); /* XXX softc */
	}
	splx(s);
}

void
setstatclockrate(arg)
	int arg;
{
}
