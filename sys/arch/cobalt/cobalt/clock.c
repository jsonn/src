/*	$NetBSD: clock.c,v 1.5.30.2 2004/09/03 12:44:29 skrll Exp $	*/

/*
 * Copyright (c) 2000 Soren S. Jorvang.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: clock.c,v 1.5.30.2 2004/09/03 12:44:29 skrll Exp $");

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <dev/clock_subr.h>

#include <dev/ic/mc146818reg.h>

#include <cobalt/cobalt/clockvar.h>

void (*timer_start)(void *);
long (*timer_read)(void *);
void *timer_cookie;

void
cpu_initclocks()
{

	/* start timer */
	if (timer_start == NULL)
		panic("cpu_initclocks(): no timer configured");
	(*timer_start)(timer_cookie);

	return;
}

u_int
mc146818_read(sc, reg)
	void *sc;
	u_int reg;
{
	(*(volatile u_int8_t *)(MIPS_PHYS_TO_KSEG1(0x10000070))) = reg;
	return (*(volatile u_int8_t *)(MIPS_PHYS_TO_KSEG1(0x10000071)));
}

void
mc146818_write(sc, reg, datum)
	void *sc;
	u_int reg;
	u_int datum;
{
	(*(volatile u_int8_t *)(MIPS_PHYS_TO_KSEG1(0x10000070))) = reg;
	(*(volatile u_int8_t *)(MIPS_PHYS_TO_KSEG1(0x10000071))) = datum;
}

void
inittodr(base)
	time_t base;
{
	struct clock_ymdhms dt;
	mc_todregs regs;
	int s;

	s = splclock();
	MC146818_GETTOD(NULL, &regs)
	splx(s);

	dt.dt_year = FROMBCD(regs[MC_YEAR]) + 2000;
	dt.dt_mon = FROMBCD(regs[MC_MONTH]);
	dt.dt_day = FROMBCD(regs[MC_DOM]);
	dt.dt_wday = FROMBCD(regs[MC_DOW]);
	dt.dt_hour = FROMBCD(regs[MC_HOUR]);
	dt.dt_min = FROMBCD(regs[MC_MIN]);
	dt.dt_sec = FROMBCD(regs[MC_SEC]);

	time.tv_sec = clock_ymdhms_to_secs(&dt);

	return;
}

void
resettodr()
{
	mc_todregs regs;
	struct clock_ymdhms dt;
	int s;

	if (cold == 1)
		return;

	s = splclock();
	MC146818_GETTOD(NULL, &regs);
	splx(s);

	clock_secs_to_ymdhms(time.tv_sec, &dt);
	regs[MC_YEAR] = TOBCD(dt.dt_year % 100);
	regs[MC_MONTH] = TOBCD(dt.dt_mon);
	regs[MC_DOM] = TOBCD(dt.dt_day);
	regs[MC_DOW] = TOBCD(dt.dt_wday);
	regs[MC_HOUR] = TOBCD(dt.dt_hour);
	regs[MC_MIN] = TOBCD(dt.dt_min);
	regs[MC_SEC] = TOBCD(dt.dt_sec);

	s = splclock();
	MC146818_PUTTOD(NULL, &regs);
	splx(s);

	return;
}

void
setstatclockrate(arg)
	int arg;
{
	/* XXX */

	return;
}

void
microtime(struct timeval *tvp)
{
	int s;
	static struct timeval lasttime;

	s = splclock();

	*tvp = time;

	if (timer_read)
		tvp->tv_usec += (*timer_read)(timer_cookie);

	if (tvp->tv_usec >= 1000000) {
		tvp->tv_usec -= 1000000;
		tvp->tv_sec++;
	}

	if (tvp->tv_sec == lasttime.tv_sec &&
	    tvp->tv_usec <= lasttime.tv_usec &&
	    (tvp->tv_usec = lasttime.tv_usec + 1) >= 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}

	lasttime = *tvp;
	splx(s);
}
