/*	$NetBSD: a34kbbc.c,v 1.12.6.2 2004/09/18 14:31:33 skrll Exp $ */

/*
 * Copyright (c) 1982, 1990 The Regents of the University of California.
 * All rights reserved.
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
 * 3. Neither the name of the University nor the names of its contributors
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
 * from: Utah $Hdr: clock.c 1.18 91/01/21$
 *
 *	@(#)clock.c	7.6 (Berkeley) 5/7/91
 */

/*
 * Copyright (c) 1988 University of Utah.
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
 * from: Utah $Hdr: clock.c 1.18 91/01/21$
 *
 *	@(#)clock.c	7.6 (Berkeley) 5/7/91
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: a34kbbc.c,v 1.12.6.2 2004/09/18 14:31:33 skrll Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <machine/psl.h>
#include <machine/cpu.h>
#include <amiga/amiga/device.h>
#include <amiga/amiga/custom.h>
#include <amiga/amiga/cia.h>
#include <amiga/dev/rtc.h>
#include <amiga/dev/zbusvar.h>

#include <dev/clock_subr.h>

int a34kbbc_match(struct device *, struct cfdata *, void *);
void a34kbbc_attach(struct device *, struct device *, void *);

CFATTACH_DECL(a34kbbc, sizeof(struct device),
    a34kbbc_match, a34kbbc_attach, NULL, NULL);

void *a34kclockaddr;
int a34kugettod(struct timeval *);
int a34kusettod(struct timeval *);

int
a34kbbc_match(struct device *pdp, struct cfdata *cfp, void *auxp)
{
	static int a34kbbc_matched = 0;

	if (!matchname("a34kbbc", auxp))
		return(0);

	/* Allow only once instance. */
	if (a34kbbc_matched)
		return(0);

	if (!(is_a3000() || is_a4000()))
		return(0);

	a34kclockaddr = (void *)ztwomap(0xdc0000);
	if (a34kugettod(0) == 0)
		return(0);

	a34kbbc_matched = 1;
	return(1);
}

/*
 * Attach us to the rtc function pointers.
 */
void
a34kbbc_attach(struct device *pdp, struct device *dp, void *auxp)
{
	printf("\n");
	a34kclockaddr = (void *)ztwomap(0xdc0000);

	ugettod = a34kugettod;
	usettod = a34kusettod;
}

int
a34kugettod(struct timeval *tvp)
{
	struct rtclock3000 *rt;
	struct clock_ymdhms dt;
	time_t secs;

	rt = a34kclockaddr;

	/* hold clock */
	rt->control1 = A3CONTROL1_HOLD_CLOCK;

	/* Copy the info.  Careful about the order! */
	dt.dt_sec   = rt->second1 * 10 + rt->second2;
	dt.dt_min   = rt->minute1 * 10 + rt->minute2;
	dt.dt_hour  = rt->hour1   * 10 + rt->hour2;
	dt.dt_wday  = rt->weekday;
	dt.dt_day   = rt->day1    * 10 + rt->day2;
	dt.dt_mon   = rt->month1  * 10 + rt->month2;
	dt.dt_year  = rt->year1   * 10 + rt->year2;

	dt.dt_year += CLOCK_BASE_YEAR;
	/* let it run again.. */
	rt->control1 = A3CONTROL1_FREE_CLOCK;

	if (dt.dt_year < STARTOFTIME)
		dt.dt_year += 100;


	if ((dt.dt_hour > 23) ||
	    (dt.dt_wday > 6) ||
	    (dt.dt_day  > 31) ||
	    (dt.dt_mon  > 12) ||
	    /* (dt.dt_year < STARTOFTIME) || */ (dt.dt_year > 2036))
		return (0);

	secs = clock_ymdhms_to_secs(&dt);
	if (tvp)
		tvp->tv_sec = secs;
	return (1);
}

int
a34kusettod(struct timeval *tvp)
{
	struct rtclock3000 *rt;
	struct clock_ymdhms dt;
	time_t secs;

	rt = a34kclockaddr;
	secs = tvp->tv_sec;
	/*
	 * there seem to be problems with the bitfield addressing
	 * currently used..
	 */

	if (! rt)
		return (0);

	clock_secs_to_ymdhms(secs, &dt);

	rt->control1 = A3CONTROL1_HOLD_CLOCK;		/* implies mode 0 */
	rt->second1 = dt.dt_sec / 10;
	rt->second2 = dt.dt_sec % 10;
	rt->minute1 = dt.dt_min / 10;
	rt->minute2 = dt.dt_min % 10;
	rt->hour1   = dt.dt_hour / 10;
	rt->hour2   = dt.dt_hour % 10;
	rt->weekday = dt.dt_wday;
	rt->day1    = dt.dt_day / 10;
	rt->day2    = dt.dt_day % 10;
	rt->month1  = dt.dt_mon / 10;
	rt->month2  = dt.dt_mon % 10;
	rt->year1   = (dt.dt_year / 10) % 10;
	rt->year2   = dt.dt_year % 10;
	rt->control1 = A3CONTROL1_HOLD_CLOCK | 1;	/* mode 1 registers */
	rt->leapyear = dt.dt_year; 		/* XXX implicit % 4 */
	rt->control1 = A3CONTROL1_FREE_CLOCK;		/* implies mode 1 */

	return (1);
}
