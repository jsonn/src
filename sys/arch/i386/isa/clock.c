/*-
 * Copyright (c) 1993, 1994 Charles Hannum.
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
 *	from: @(#)clock.c	7.2 (Berkeley) 5/12/91
 *	$Id: clock.c,v 1.24.2.1 1994/07/25 03:34:45 cgd Exp $
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
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <machine/cpu.h>
#include <machine/pio.h>
#include <machine/cpufunc.h>

#include <i386/isa/icu.h>
#include <i386/isa/isareg.h>
#include <i386/isa/isavar.h>
#include <i386/isa/clock.h>
#include <i386/isa/rtc.h>
#include <i386/isa/timerreg.h>
#include <i386/isa/spkrreg.h>

void spinwait __P((int));

void
startrtclock()
{
	int s;

	findcpuspeed();		/* use the clock (while it's free)
					to find the cpu speed */
	/* initialize 8253 clock */
	outb(TIMER_MODE, TIMER_SEL0|TIMER_RATEGEN|TIMER_16BIT);

	/* Correct rounding will buy us a better precision in timekeeping */
	outb(IO_TIMER1, TIMER_DIV(hz)%256);
	outb(IO_TIMER1, TIMER_DIV(hz)/256);

        /* Check diagnostic status */
        outb (IO_RTC, RTC_DIAG);
	if (s = inb (IO_RTC+1))
		printf("RTC BIOS diagnostic error %b\n", s, RTCDG_BITS);
}

int
clockintr(frame)
	struct clockframe *frame;
{

	hardclock(frame);
	return -1;
}

int
gettick()
{
	u_char lo, hi;

	/* Don't want someone screwing with the counter while we're here. */
	disable_intr();
	/* Select counter 0 and latch it. */
	outb(TIMER_MODE, TIMER_SEL0 | TIMER_LATCH);
	lo = inb(TIMER_CNTR0);
	hi = inb(TIMER_CNTR0);
	enable_intr();
	return ((hi << 8) | lo);
}

/*
 * Wait "n" microseconds.
 * Relies on timer 1 counting down from (TIMER_FREQ / hz) at TIMER_FREQ Hz.
 * Note: timer had better have been programmed before this is first used!
 * (Note that we use `rate generator' mode, which counts at 1:1; `square
 * wave' mode counts at 2:1).
 */
void
delay(n)
	int n;
{
	int limit, tick, otick;

	/*
	 * Read the counter first, so that the rest of the setup overhead is
	 * counted.
	 */
	otick = gettick();

#ifdef __GNUC__
	/*
	 * Calculate ((n * TIMER_FREQ) / 1e6) using explicit assembler code so
	 * we can take advantage of the intermediate 64-bit quantity to prevent
	 * loss of significance.
	 */
	n -= 5;
	if (n < 0)
		return;
	{register int m;
	__asm __volatile("mul %3"
			 : "=a" (n), "=d" (m)
			 : "0" (n), "r" (TIMER_FREQ));
	__asm __volatile("div %3"
			 : "=a" (n)
			 : "0" (n), "d" (m), "r" (1000000)
			 : "%edx");}
#else
	/*
	 * Calculate ((n * TIMER_FREQ) / 1e6) without using floating point and
	 * without any avoidable overflows.
	 */
	n -= 20;
	{
		int sec = n / 1000000,
		    usec = n % 1000000;
		n = sec * TIMER_FREQ +
		    usec * (TIMER_FREQ / 1000000) +
		    usec * ((TIMER_FREQ % 1000000) / 1000) / 1000 +
		    usec * (TIMER_FREQ % 1000) / 1000000;
	}
#endif

	limit = TIMER_FREQ / hz;

	while (n > 0) {
		tick = gettick();
		if (tick > otick)
			n -= limit - (tick - otick);
		else
			n -= otick - tick;
		otick = tick;
	}
}

static int beeping;

void
sysbeepstop(arg)
	void *arg;
{

	/* disable counter 2 */
	disable_intr();
	outb(PITAUX_PORT, inb(PITAUX_PORT) & ~PIT_SPKR);
	enable_intr();
	beeping = 0;
}

void
sysbeep(pitch, period)
	int pitch, period;
{
	static int last_pitch, last_period;

	if (beeping)
		untimeout(sysbeepstop, 0);
	if (!beeping || last_pitch != pitch) {
		disable_intr();
		outb(TIMER_MODE, TIMER_SEL2 | TIMER_16BIT | TIMER_SQWAVE);
		outb(TIMER_CNTR2, TIMER_DIV(pitch)%256);
		outb(TIMER_CNTR2, TIMER_DIV(pitch)/256);
		outb(PITAUX_PORT, inb(PITAUX_PORT) | PIT_SPKR);	/* enable counter 2 */
		enable_intr();
	}
	last_pitch = pitch;
	beeping = last_period = period;
	timeout(sysbeepstop, 0, period);
}

unsigned int delaycount;	/* calibrated loop variable (1 millisecond) */

#define FIRST_GUESS	0x2000
findcpuspeed()
{
	int i;
	int remainder;

	/* Put counter in count down mode */
	outb(TIMER_MODE, TIMER_SEL0 | TIMER_16BIT | TIMER_RATEGEN);
	outb(TIMER_CNTR0, 0xff);
	outb(TIMER_CNTR0, 0xff);
	for (i = FIRST_GUESS; i; i--)
		;
	/* Read the value left in the counter */
	remainder = gettick();
	/* Formula for delaycount is :
	 *  (loopcount * timer clock speed)/ (counter ticks * 1000)
	 */
	delaycount = (FIRST_GUESS * TIMER_DIV(1000)) / (0xffff-remainder);
}

void
cpu_initclocks()
{
	static struct intrhand clockhand;

	clockhand.ih_fun = clockintr;
	clockhand.ih_arg = 0;
	clockhand.ih_level = IPL_CLOCK;
	intr_establish(IRQ0, &clockhand);
}

void
rtcinit()
{
	static int first_rtcopen_ever = 1;

        if (!first_rtcopen_ever)
		return;
	first_rtcopen_ever = 0;

	outb(IO_RTC, RTC_STATUSA);
	outb(IO_RTC+1, RTC_DIV2 | RTC_RATE6);
	outb(IO_RTC, RTC_STATUSB);
	outb(IO_RTC+1, RTC_HM);
}

int
rtcget(rtc_regs)
	struct rtc_st *rtc_regs;
{
	int i;
        u_char *regs = (u_char *)rtc_regs;
        
	rtcinit();
	outb(IO_RTC, RTC_D); 
	if (inb(IO_RTC+1) & RTC_VRT == 0) return(-1);
	outb(IO_RTC, RTC_STATUSA);	
	while (inb(IO_RTC+1) & RTC_UIP)		/* busy wait */
		outb(IO_RTC, RTC_STATUSA);	
	for (i = 0; i < RTC_NREG; i++) {
		outb(IO_RTC, i);
		regs[i] = inb(IO_RTC+1);
	}
	return(0);
}	

void
rtcput(rtc_regs)
	struct rtc_st *rtc_regs;
{
	u_char x;
	int i;
        u_char *regs = (u_char *)rtc_regs;

	rtcinit();
	outb(IO_RTC, RTC_STATUSB);
	x = inb(IO_RTC+1);
	outb(IO_RTC, RTC_STATUSB);
	outb(IO_RTC+1, x | RTC_SET); 	
	for (i = 0; i < RTC_NREGP; i++) {
		outb(IO_RTC, i);
		outb(IO_RTC+1, regs[i]);
	}
	outb(IO_RTC, RTC_STATUSB);
	outb(IO_RTC+1, x & ~RTC_SET); 
}

static int month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static int
yeartoday(year)
	int year;
{

	return((year%4) ? 365 : 366);
}

int
hexdectodec(n)
	char n;
{

	return(((n>>4)&0x0F)*10 + (n&0x0F));
}

char
dectohexdec(n)
	int n;
{

	return((char)(((n/10)<<4)&0xF0) | ((n%10)&0x0F));
}

static int timeset;

/*
 * Initialize the time of day register, based on the time base which is, e.g.
 * from a filesystem.
 */
void
inittodr(base)
	time_t base;
{
        /*
         * We ignore the suggested time for now and go for the RTC
         * clock time stored in the CMOS RAM.
         */
	struct rtc_st rtclk;
	time_t n;
	int sec, min, hr, dom, mon, yr;
	int i, days = 0;
	int s;

	s = splclock();
	if (rtcget(&rtclk)) {
		splx(s);
		return;
	}
	splx(s);

	timeset = 1;

	sec = hexdectodec(rtclk.rtc_sec);
	min = hexdectodec(rtclk.rtc_min);
	hr = hexdectodec(rtclk.rtc_hr);
	dom = hexdectodec(rtclk.rtc_dom);
	mon = hexdectodec(rtclk.rtc_mon);
	yr = hexdectodec(rtclk.rtc_yr);
	yr = (yr < 70) ? yr+100 : yr;

	n = sec + 60 * min + 3600 * hr;
	n += (dom - 1) * 3600 * 24;

	if (yeartoday(yr) == 366)
		month[1] = 29;
	for (i = mon - 2; i >= 0; i--)
		days += month[i];
	month[1] = 28;
	for (i = 70; i < yr; i++)
		days += yeartoday(i);
	n += days * 3600 * 24;

	n += tz.tz_minuteswest * 60;
	if (tz.tz_dsttime)
		n -= 3600;
	time.tv_sec = n;
        time.tv_usec = 0;
}

/*
 * Reset the clock.
 */
void
resettodr()
{
	struct rtc_st rtclk;
	time_t n;
	int diff, i, j;
	int s;

	/*
	 * We might have been called by boot() due to a crash early
	 * on.  Don't reset the clock chip in this case.
	 */
	if (!timeset)
		return;

	s = splclock();
	if (rtcget(&rtclk))
                bzero(&rtclk, sizeof(rtclk));
	splx(s);

	diff = tz.tz_minuteswest * 60;
	if (tz.tz_dsttime)
		diff -= 3600;
	n = (time.tv_sec - diff) % (3600 * 24);   /* hrs+mins+secs */
	rtclk.rtc_sec = dectohexdec(n%60);
	n /= 60;
	rtclk.rtc_min = dectohexdec(n%60);
	rtclk.rtc_hr = dectohexdec(n/60);

	n = (time.tv_sec - diff) / (3600 * 24);	/* days */
	rtclk.rtc_dow = (n + 4) % 7;  /* 1/1/70 is Thursday */

	for (j = 1970, i = yeartoday(j); n >= i; j++, i = yeartoday(j))
		n -= i;

	rtclk.rtc_yr = dectohexdec(j - 1900);

	if (i == 366)
		month[1] = 29;
	for (i = 0; n >= month[i]; i++)
		n -= month[i];
	month[1] = 28;
	rtclk.rtc_mon = dectohexdec(++i);

	rtclk.rtc_dom = dectohexdec(++n);

	s = splclock();
	rtcput(&rtclk);
	splx(s);
}

void
setstatclockrate(arg)
	int arg;
{
}
