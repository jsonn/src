/*	$NetBSD: kern_todr.c,v 1.24.26.1 2008/01/09 01:56:12 matt Exp $	*/

/*
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
 * from: Utah Hdr: clock.c 1.18 91/01/21
 *
 *	@(#)clock.c	8.1 (Berkeley) 6/10/93
 */
/*
 * Copyright (c) 1988 University of Utah.
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
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_todr.c,v 1.24.26.1 2008/01/09 01:56:12 matt Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/timetc.h>
#include <sys/intr.h>

#include <dev/clock_subr.h>	/* hmm.. this should probably move to sys */

#ifdef	__HAVE_GENERIC_TODR

static todr_chip_handle_t todr_handle = NULL;

/*
 * Attach the clock device to todr_handle.
 */
void
todr_attach(todr_chip_handle_t todr)
{

	if (todr_handle) {
		printf("todr_attach: TOD already configured\n");
		return;
	}
	todr_handle = todr;
}

static int timeset = 0;

/*
 * Set up the system's time, given a `reasonable' time value.
 */
void
inittodr(time_t base)
{
	int badbase = 0, waszero = (base == 0), goodtime = 0, badrtc = 0;
	int s;
	struct timespec ts;
	struct timeval tv;

	if (base < 5 * SECYR) {
		struct clock_ymdhms basedate;

		/*
		 * If base is 0, assume filesystem time is just unknown
		 * instead of preposterous. Don't bark.
		 */
		if (base != 0)
			printf("WARNING: preposterous time in file system\n");
		/* not going to use it anyway, if the chip is readable */
		basedate.dt_year = 2006;
		basedate.dt_mon = 1;
		basedate.dt_day = 1;
		basedate.dt_hour = 12;
		basedate.dt_min = 0;
		basedate.dt_sec = 0;
		base = clock_ymdhms_to_secs(&basedate);
		badbase = 1;
	}

	/*
	 * Some ports need to be supplied base in order to fabricate a time_t.
	 */
	if (todr_handle)
		todr_handle->base_time = base;

	if ((todr_handle == NULL) ||
	    (todr_gettime(todr_handle, &tv) != 0) ||
	    (tv.tv_sec < (25 * SECYR))) {

		if (todr_handle != NULL)
			printf("WARNING: preposterous TOD clock time\n");
		else
			printf("WARNING: no TOD clock present\n");
		badrtc = 1;
	} else {
		int deltat = tv.tv_sec - base;

		if (deltat < 0)
			deltat = -deltat;

		if ((badbase == 0) && deltat >= 2 * SECDAY) {
			
			if (tv.tv_sec < base) {
				/*
				 * The clock should never go backwards
				 * relative to filesystem time.  If it
				 * does by more than the threshold,
				 * believe the filesystem.
				 */
				printf("WARNING: clock lost %d days\n",
				    deltat / SECDAY);
				badrtc = 1;
			} else {
				printf("WARNING: clock gained %d days\n",
				    deltat / SECDAY);
			}
		} else {
			goodtime = 1;
		}
	}

	/* if the rtc time is bad, use the filesystem time */
	if (badrtc) {
		if (badbase) {
			printf("WARNING: using default initial time\n");
		} else {
			printf("WARNING: using filesystem time\n");
		}
		tv.tv_sec = base;
		tv.tv_usec = 0;
	}

	timeset = 1;

	ts.tv_sec = tv.tv_sec;
	ts.tv_nsec = tv.tv_usec * 1000;
	s = splclock();
	tc_setclock(&ts);
	splx(s);

	if (waszero || goodtime)
		return;

	printf("WARNING: CHECK AND RESET THE DATE!\n");
}

/*
 * Reset the TODR based on the time value; used when the TODR
 * has a preposterous value and also when the time is reset
 * by the stime system call.  Also called when the TODR goes past
 * TODRZERO + 100*(SECYEAR+2*SECDAY) (e.g. on Jan 2 just after midnight)
 * to wrap the TODR around.
 */
void
resettodr(void)
{
	struct timeval tv;

	/*
	 * We might have been called by boot() due to a crash early
	 * on.  Don't reset the clock chip if we don't know what time
	 * it is.
	 */
	if (!timeset)
		return;

	getmicrotime(&tv);

	if (tv.tv_sec == 0)
		return;

	if (todr_handle)
		if (todr_settime(todr_handle, &tv) != 0)
			printf("Cannot set TOD clock time\n");
}

#endif	/* __HAVE_GENERIC_TODR */

#ifdef	TODR_DEBUG
static void
todr_debug(const char *prefix, int rv, struct clock_ymdhms *dt,
    volatile struct timeval *tvp)
{
	struct timeval tv_val;
	struct clock_ymdhms dt_val;

	if (dt == NULL) {
		clock_secs_to_ymdhms(tvp->tv_sec, &dt_val);
		dt = &dt_val;
	}
	if (tvp == NULL) {
		tvp = &tv_val;
		tvp->tv_sec = clock_ymdhms_to_secs(dt);
		tvp->tv_usec = 0;
	}
	printf("%s: rv = %d\n", prefix, rv);
	printf("%s: rtc_offset = %d\n", prefix, rtc_offset);
	printf("%s: %4u/%02u/%02u %02u:%02u:%02u, (wday %d) (epoch %u.%06u)\n",
	    prefix,
	    dt->dt_year, dt->dt_mon, dt->dt_day,
	    dt->dt_hour, dt->dt_min, dt->dt_sec,
	    dt->dt_wday, (unsigned)tvp->tv_sec, (unsigned)tvp->tv_usec);
}
#else	/* !TODR_DEBUG */
#define	todr_debug(prefix, rv, dt, tvp)
#endif	/* TODR_DEBUG */


int
todr_gettime(todr_chip_handle_t tch, volatile struct timeval *tvp)
{
	struct clock_ymdhms	dt;
	int			rv;

	if (tch->todr_gettime) {
		rv = tch->todr_gettime(tch, tvp);
		/*
		 * Some unconverted ports have their own references to
		 * rtc_offset.   A converted port must not do that.
		 */
#ifdef	__HAVE_GENERIC_TODR
		if (rv == 0)
			tvp->tv_sec += rtc_offset * 60;
#endif
		todr_debug("TODR-GET-SECS", rv, NULL, tvp);
		return rv;
	} else if (tch->todr_gettime_ymdhms) {
		rv = tch->todr_gettime_ymdhms(tch, &dt);
		todr_debug("TODR-GET-YMDHMS", rv, &dt, NULL);
		if (rv)
			return rv;

		/*
		 * Formerly we had code here that explicitly checked
		 * for 2038 year rollover.
		 *
		 * However, clock_ymdhms_to_secs performs the same
		 * check for us, so we need not worry about it.  Note
		 * that this assumes that the tvp->tv_sec member is
		 * a time_t.
		 */

		/*
		 * Simple sanity checks.  Note that this includes a
		 * value for clocks that can return a leap second.
		 * Note that we don't support double leap seconds,
		 * since this was apparently an error/misunderstanding
		 * on the part of the ISO C committee, and can never
		 * actually occur.  If your clock issues us a double
		 * leap second, it must be broken.  Ultimately, you'd
		 * have to be trying to read time at precisely that
		 * instant to even notice, so even broken clocks will
		 * work the vast majority of the time.  In such a case
		 * it is recommended correction be applied in the
		 * clock driver.
		 */
		if (dt.dt_mon < 1 || dt.dt_mon > 12 ||
		    dt.dt_day < 1 || dt.dt_day > 31 ||
		    dt.dt_hour > 23 || dt.dt_min > 59 || dt.dt_sec > 60) {
			return EINVAL;
		}
		tvp->tv_sec = clock_ymdhms_to_secs(&dt) + rtc_offset * 60;
		tvp->tv_usec = 0;
		return tvp->tv_sec < 0 ? EINVAL : 0;
	}

	return ENXIO;
}

int
todr_settime(todr_chip_handle_t tch, volatile struct timeval *tvp)
{
	struct clock_ymdhms	dt;
	int			rv;

	if (tch->todr_settime) {
		/* See comments above in gettime why this is ifdef'd */
#ifdef	__HAVE_GENERIC_TODR
		struct timeval	copy = *tvp;
		copy.tv_sec -= rtc_offset * 60;
		rv = tch->todr_settime(tch, &copy);
#else
		rv = tch->todr_settime(tch, tvp);
#endif
		todr_debug("TODR-SET-SECS", rv, NULL, tvp);
		return rv;
	} else if (tch->todr_settime_ymdhms) {
		time_t	sec = tvp->tv_sec - rtc_offset * 60;
		if (tvp->tv_usec >= 500000)
			sec++;
		clock_secs_to_ymdhms(sec, &dt);
		rv = tch->todr_settime_ymdhms(tch, &dt);
		todr_debug("TODR-SET-YMDHMS", rv, &dt, NULL);
		return rv;
	} else {
		return ENXIO;
	}
}
