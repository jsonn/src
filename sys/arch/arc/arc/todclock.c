/* $NetBSD: todclock.c,v 1.1.8.1 2001/11/12 21:16:24 thorpej Exp $ */
/* NetBSD: clock.c,v 1.31 2001/05/27 13:53:24 sommerfeld Exp  */

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 * from: Utah Hdr: clock.c 1.18 91/01/21
 *
 *	@(#)clock.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: todclock.c,v 1.1.8.1 2001/11/12 21:16:24 thorpej Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>

#include <dev/clock_subr.h>

#include <arc/arc/todclockvar.h>

#define MINYEAR 2001 /* "today" */

struct device *todclockdev;
const struct todclockfns *todclockfns;
int todclock_year_offset;
int todclockinitted;

void
todclockattach(dev, fns, year_offset)
	struct device *dev;
	const struct todclockfns *fns;
	int year_offset;
{

	/*
	 * Just bookkeeping.
	 */

	if (todclockfns != NULL)
		panic("todclockattach: multiple todclocks");
	todclockdev = dev;
	todclockfns = fns;
	todclock_year_offset = year_offset;
}

/*
 * Machine-dependent real-time clock routines.
 *
 * Inittodr initializes the time of day hardware which provides
 * date functions.  Its primary function is to use some file
 * system information in case the hardare clock lost state.
 *
 * Resettodr restores the time of day hardware after a time change.
 */

/*
 * Initialze the time of day register, based on the time base which is, e.g.
 * from a filesystem.  Base provides the time to within six months,
 * and the time of year clock (if any) provides the rest.
 */
void
inittodr(base)
	time_t base;
{
	struct todclocktime ct;
	int year;
	struct clock_ymdhms dt;
	time_t deltat;
	int badbase;

	if (todclockfns == NULL)
		panic("inittodr: no real time clock attached");

	if (base < (MINYEAR-1970)*SECYR) {
		printf("WARNING: preposterous time in file system");
		/* read the system clock anyway */
		base = (MINYEAR-1970)*SECYR;
		badbase = 1;
	} else
		badbase = 0;

	(*todclockfns->tcf_get)(todclockdev, base, &ct);
#ifdef DEBUG
	printf("readclock: %d/%d/%d/%d/%d/%d", ct.year, ct.mon, ct.day,
	       ct.hour, ct.min, ct.sec);
#endif
	todclockinitted = 1;

	year = 1900 + todclock_year_offset + ct.year;
	if (year < 1970)
		year += 100;
	/* simple sanity checks (2037 = time_t overflow) */
	if (year < MINYEAR || year > 2037 ||
	    ct.mon < 1 || ct.mon > 12 || ct.day < 1 ||
	    ct.day > 31 || ct.hour > 23 || ct.min > 59 || ct.sec > 59) {
		/*
		 * Believe the time in the file system for lack of
		 * anything better, resetting the TODR.
		 */
		time.tv_sec = base;
		if (!badbase) {
			printf("WARNING: preposterous clock chip time\n");
			resettodr();
		}
		goto bad;
	}

	dt.dt_year = year;
	dt.dt_mon = ct.mon;
	dt.dt_day = ct.day;
	dt.dt_hour = ct.hour;
	dt.dt_min = ct.min;
	dt.dt_sec = ct.sec;
	time.tv_sec = clock_ymdhms_to_secs(&dt);
#ifdef DEBUG
	printf("=>%ld (%ld)\n", time.tv_sec, base);
#endif

	if (!badbase) {
		/*
		 * See if we gained/lost two or more days;
		 * if so, assume something is amiss.
		 */
		deltat = time.tv_sec - base;
		if (deltat < 0)
			deltat = -deltat;
		if (deltat < 2 * SECDAY)
			return;
		printf("WARNING: clock %s %ld days",
		    time.tv_sec < base ? "lost" : "gained",
		    (long)deltat / SECDAY);
	}
bad:
	printf(" -- CHECK AND RESET THE DATE!\n");
}

/*
 * Reset the TODR based on the time value; used when the TODR
 * has a preposterous value and also when the time is reset
 * by the stime system call.  Also called when the TODR goes past
 * TODRZERO + 100*(SECYEAR+2*SECDAY) (e.g. on Jan 2 just after midnight)
 * to wrap the TODR around.
 */
void
resettodr()
{
	struct clock_ymdhms dt;
	struct todclocktime ct;

	if (!todclockinitted)
		return;

	clock_secs_to_ymdhms(time.tv_sec, &dt);

	/* rt clock wants 2 digits */
	ct.year = (dt.dt_year - todclock_year_offset) % 100;
	ct.mon = dt.dt_mon;
	ct.day = dt.dt_day;
	ct.hour = dt.dt_hour;
	ct.min = dt.dt_min;
	ct.sec = dt.dt_sec;
	ct.dow = dt.dt_wday;
#ifdef DEBUG
	printf("setclock: %d/%d/%d/%d/%d/%d\n", ct.year, ct.mon, ct.day,
	       ct.hour, ct.min, ct.sec);
#endif

	(*todclockfns->tcf_set)(todclockdev, &ct);
}
