/*	$NetBSD: kern_time.c,v 1.36.8.1 1999/06/21 01:24:03 thorpej Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)kern_time.c	8.4 (Berkeley) 5/26/95
 */

#include "fs_nfs.h"
#include "opt_nfsserver.h"

#include <sys/param.h>
#include <sys/resourcevar.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/signalvar.h>
#include <sys/syslog.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <vm/vm.h>
#include <uvm/uvm_extern.h>

#if defined(NFS) || defined(NFSSERVER)
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs_var.h>
#endif

#include <machine/cpu.h>

static int	settime __P((struct timeval *));

/*
 * Time of day and interval timer support.
 *
 * These routines provide the kernel entry points to get and set
 * the time-of-day and per-process interval timers.  Subroutines
 * here provide support for adding and subtracting timeval structures
 * and decrementing interval timers, optionally reloading the interval
 * timers when they expire.
 */

/* This function is used by clock_settime and settimeofday */
static int
settime(tv)
	struct timeval *tv;
{
	struct timeval delta;
	int s;

	/* WHAT DO WE DO ABOUT PENDING REAL-TIME TIMEOUTS??? */
	s = splclock();
	timersub(tv, &time, &delta);
	if ((delta.tv_sec < 0 || delta.tv_usec < 0) && securelevel > 1)
		return (EPERM);
#ifdef notyet
	if ((delta.tv_sec < 86400) && securelevel > 0)
		return (EPERM);
#endif
	time = *tv;
	(void) splsoftclock();
	timeradd(&boottime, &delta, &boottime);
	timeradd(&runtime, &delta, &runtime);
#	if defined(NFS) || defined(NFSSERVER)
		nqnfs_lease_updatetime(delta.tv_sec);
#	endif
	splx(s);
	resettodr();
	return (0);
}

/* ARGSUSED */
int
sys_clock_gettime(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_clock_gettime_args /* {
		syscallarg(clockid_t) clock_id;
		syscallarg(struct timespec *) tp;
	} */ *uap = v;
	clockid_t clock_id;
	struct timeval atv;
	struct timespec ats;

	clock_id = SCARG(uap, clock_id);
	if (clock_id != CLOCK_REALTIME)
		return (EINVAL);

	microtime(&atv);
	TIMEVAL_TO_TIMESPEC(&atv,&ats);

	return copyout(&ats, SCARG(uap, tp), sizeof(ats));
}

/* ARGSUSED */
int
sys_clock_settime(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_clock_settime_args /* {
		syscallarg(clockid_t) clock_id;
		syscallarg(const struct timespec *) tp;
	} */ *uap = v;
	clockid_t clock_id;
	struct timeval atv;
	struct timespec ats;
	int error;

	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);

	clock_id = SCARG(uap, clock_id);
	if (clock_id != CLOCK_REALTIME)
		return (EINVAL);

	if ((error = copyin(SCARG(uap, tp), &ats, sizeof(ats))) != 0)
		return (error);

	TIMESPEC_TO_TIMEVAL(&atv,&ats);
	if ((error = settime(&atv)))
		return (error);

	return 0;
}

int
sys_clock_getres(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_clock_getres_args /* {
		syscallarg(clockid_t) clock_id;
		syscallarg(struct timespec *) tp;
	} */ *uap = v;
	clockid_t clock_id;
	struct timespec ts;
	int error = 0;

	clock_id = SCARG(uap, clock_id);
	if (clock_id != CLOCK_REALTIME)
		return (EINVAL);

	if (SCARG(uap, tp)) {
		ts.tv_sec = 0;
		ts.tv_nsec = 1000000000 / hz;

		error = copyout(&ts, SCARG(uap, tp), sizeof(ts));
	}

	return error;
}

/* ARGSUSED */
int
sys_nanosleep(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	static int nanowait;
	register struct sys_nanosleep_args/* {
		syscallarg(struct timespec *) rqtp;
		syscallarg(struct timespec *) rmtp;
	} */ *uap = v;
	struct timespec rqt;
	struct timespec rmt;
	struct timeval atv, utv;
	int error, s, timo;

	error = copyin((caddr_t)SCARG(uap, rqtp), (caddr_t)&rqt,
		       sizeof(struct timespec));
	if (error)
		return (error);

	TIMESPEC_TO_TIMEVAL(&atv,&rqt)
	if (itimerfix(&atv))
		return (EINVAL);

	s = splclock();
	timeradd(&atv,&time,&atv);
	timo = hzto(&atv);
	/* 
	 * Avoid inadvertantly sleeping forever
	 */
	if (timo == 0)
		timo = 1;
	splx(s);

	error = tsleep(&nanowait, PWAIT | PCATCH, "nanosleep", timo);
	if (error == ERESTART)
		error = EINTR;
	if (error == EWOULDBLOCK)
		error = 0;

	if (SCARG(uap, rmtp)) {
		int error;

		s = splclock();
		utv = time;
		splx(s);

		timersub(&atv, &utv, &utv);
		if (utv.tv_sec < 0)
			timerclear(&utv);

		TIMEVAL_TO_TIMESPEC(&utv,&rmt);
		error = copyout((caddr_t)&rmt, (caddr_t)SCARG(uap,rmtp),
			sizeof(rmt));
		if (error)
			return (error);
	}

	return error;
}

/* ARGSUSED */
int
sys_gettimeofday(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_gettimeofday_args /* {
		syscallarg(struct timeval *) tp;
		syscallarg(struct timezone *) tzp;
	} */ *uap = v;
	struct timeval atv;
	int error = 0;
	struct timezone tzfake;

	if (SCARG(uap, tp)) {
		microtime(&atv);
		error = copyout(&atv, SCARG(uap, tp), sizeof(atv));
		if (error)
			return (error);
	}
	if (SCARG(uap, tzp)) {
		/*
		 * NetBSD has no kernel notion of time zone, so we just
		 * fake up a timezone struct and return it if demanded.
		 */
		tzfake.tz_minuteswest = 0;
		tzfake.tz_dsttime = 0;
		error = copyout(&tzfake, SCARG(uap, tzp), sizeof(tzfake));
	}
	return (error);
}

/* ARGSUSED */
int
sys_settimeofday(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_settimeofday_args /* {
		syscallarg(const struct timeval *) tv;
		syscallarg(const struct timezone *) tzp;
	} */ *uap = v;
	struct timeval atv;
	struct timezone atz;
	int error;

	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);
	/* Verify all parameters before changing time. */
	if (SCARG(uap, tv) && (error = copyin(SCARG(uap, tv),
	    &atv, sizeof(atv))))
		return (error);
	/* XXX since we don't use tz, probably no point in doing copyin. */
	if (SCARG(uap, tzp) && (error = copyin(SCARG(uap, tzp),
	    &atz, sizeof(atz))))
		return (error);
	if (SCARG(uap, tv))
		if ((error = settime(&atv)))
			return (error);
	/*
	 * NetBSD has no kernel notion of time zone, and only an
	 * obsolete program would try to set it, so we log a warning.
	 */
	if (SCARG(uap, tzp))
		log(LOG_WARNING, "pid %d attempted to set the "
		    "(obsolete) kernel time zone\n", p->p_pid); 
	return (0);
}

int	tickdelta;			/* current clock skew, us. per tick */
long	timedelta;			/* unapplied time correction, us. */
long	bigadj = 1000000;		/* use 10x skew above bigadj us. */

/* ARGSUSED */
int
sys_adjtime(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_adjtime_args /* {
		syscallarg(const struct timeval *) delta;
		syscallarg(struct timeval *) olddelta;
	} */ *uap = v;
	struct timeval atv;
	register long ndelta, ntickdelta, odelta;
	int s, error;

	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);

	error = copyin(SCARG(uap, delta), &atv, sizeof(struct timeval));
	if (error)
		return (error);
	if (SCARG(uap, olddelta) != NULL &&
	    uvm_useracc((caddr_t)SCARG(uap, olddelta), sizeof(struct timeval),
	     B_WRITE) == FALSE)
		return (EFAULT);

	/*
	 * Compute the total correction and the rate at which to apply it.
	 * Round the adjustment down to a whole multiple of the per-tick
	 * delta, so that after some number of incremental changes in
	 * hardclock(), tickdelta will become zero, lest the correction
	 * overshoot and start taking us away from the desired final time.
	 */
	ndelta = atv.tv_sec * 1000000 + atv.tv_usec;
	if (ndelta > bigadj)
		ntickdelta = 10 * tickadj;
	else
		ntickdelta = tickadj;
	if (ndelta % ntickdelta)
		ndelta = ndelta / ntickdelta * ntickdelta;

	/*
	 * To make hardclock()'s job easier, make the per-tick delta negative
	 * if we want time to run slower; then hardclock can simply compute
	 * tick + tickdelta, and subtract tickdelta from timedelta.
	 */
	if (ndelta < 0)
		ntickdelta = -ntickdelta;
	s = splclock();
	odelta = timedelta;
	timedelta = ndelta;
	tickdelta = ntickdelta;
	splx(s);

	if (SCARG(uap, olddelta)) {
		atv.tv_sec = odelta / 1000000;
		atv.tv_usec = odelta % 1000000;
		(void) copyout(&atv, SCARG(uap, olddelta),
		    sizeof(struct timeval));
	}
	return (0);
}

/*
 * Get value of an interval timer.  The process virtual and
 * profiling virtual time timers are kept in the p_stats area, since
 * they can be swapped out.  These are kept internally in the
 * way they are specified externally: in time until they expire.
 *
 * The real time interval timer is kept in the process table slot
 * for the process, and its value (it_value) is kept as an
 * absolute time rather than as a delta, so that it is easy to keep
 * periodic real-time signals from drifting.
 *
 * Virtual time timers are processed in the hardclock() routine of
 * kern_clock.c.  The real time timer is processed by a timeout
 * routine, called from the softclock() routine.  Since a callout
 * may be delayed in real time due to interrupt processing in the system,
 * it is possible for the real time timeout routine (realitexpire, given below),
 * to be delayed in real time past when it is supposed to occur.  It
 * does not suffice, therefore, to reload the real timer .it_value from the
 * real time timers .it_interval.  Rather, we compute the next time in
 * absolute time the timer should go off.
 */
/* ARGSUSED */
int
sys_getitimer(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_getitimer_args /* {
		syscallarg(int) which;
		syscallarg(struct itimerval *) itv;
	} */ *uap = v;
	int which = SCARG(uap, which);
	struct itimerval aitv;
	int s;

	if ((u_int)which > ITIMER_PROF)
		return (EINVAL);
	s = splclock();
	if (which == ITIMER_REAL) {
		/*
		 * Convert from absolute to relative time in .it_value
		 * part of real time timer.  If time for real time timer
		 * has passed return 0, else return difference between
		 * current time and time for the timer to go off.
		 */
		aitv = p->p_realtimer;
		if (timerisset(&aitv.it_value)) {
			if (timercmp(&aitv.it_value, &time, <))
				timerclear(&aitv.it_value);
			else
				timersub(&aitv.it_value, &time, &aitv.it_value);
		}
	} else
		aitv = p->p_stats->p_timer[which];
	splx(s);
	return (copyout(&aitv, SCARG(uap, itv), sizeof(struct itimerval)));
}

/* ARGSUSED */
int
sys_setitimer(p, v, retval)
	struct proc *p;
	register void *v;
	register_t *retval;
{
	register struct sys_setitimer_args /* {
		syscallarg(int) which;
		syscallarg(const struct itimerval *) itv;
		syscallarg(struct itimerval *) oitv;
	} */ *uap = v;
	int which = SCARG(uap, which);
	struct sys_getitimer_args getargs;
	struct itimerval aitv;
	register const struct itimerval *itvp;
	int s, error;

	if ((u_int)which > ITIMER_PROF)
		return (EINVAL);
	itvp = SCARG(uap, itv);
	if (itvp && (error = copyin(itvp, &aitv, sizeof(struct itimerval))))
		return (error);
	if (SCARG(uap, oitv) != NULL) {
		SCARG(&getargs, which) = which;
		SCARG(&getargs, itv) = SCARG(uap, oitv);
		if ((error = sys_getitimer(p, &getargs, retval)) != 0)
			return (error);
	}
	if (itvp == 0)
		return (0);
	if (itimerfix(&aitv.it_value) || itimerfix(&aitv.it_interval))
		return (EINVAL);
	s = splclock();
	if (which == ITIMER_REAL) {
		untimeout(realitexpire, p);
		if (timerisset(&aitv.it_value)) {
			timeradd(&aitv.it_value, &time, &aitv.it_value);
			timeout(realitexpire, p, hzto(&aitv.it_value));
		}
		p->p_realtimer = aitv;
	} else
		p->p_stats->p_timer[which] = aitv;
	splx(s);
	return (0);
}

/*
 * Real interval timer expired:
 * send process whose timer expired an alarm signal.
 * If time is not set up to reload, then just return.
 * Else compute next time timer should go off which is > current time.
 * This is where delay in processing this timeout causes multiple
 * SIGALRM calls to be compressed into one.
 */
void
realitexpire(arg)
	void *arg;
{
	register struct proc *p;
	int s;

	p = (struct proc *)arg;
	psignal(p, SIGALRM);
	if (!timerisset(&p->p_realtimer.it_interval)) {
		timerclear(&p->p_realtimer.it_value);
		return;
	}
	for (;;) {
		s = splclock();
		timeradd(&p->p_realtimer.it_value,
		    &p->p_realtimer.it_interval, &p->p_realtimer.it_value);
		if (timercmp(&p->p_realtimer.it_value, &time, >)) {
			timeout(realitexpire, p,
			    hzto(&p->p_realtimer.it_value));
			splx(s);
			return;
		}
		splx(s);
	}
}

/*
 * Check that a proposed value to load into the .it_value or
 * .it_interval part of an interval timer is acceptable, and
 * fix it to have at least minimal value (i.e. if it is less
 * than the resolution of the clock, round it up.)
 */
int
itimerfix(tv)
	struct timeval *tv;
{

	if (tv->tv_sec < 0 || tv->tv_sec > 100000000 ||
	    tv->tv_usec < 0 || tv->tv_usec >= 1000000)
		return (EINVAL);
	if (tv->tv_sec == 0 && tv->tv_usec != 0 && tv->tv_usec < tick)
		tv->tv_usec = tick;
	return (0);
}

/*
 * Decrement an interval timer by a specified number
 * of microseconds, which must be less than a second,
 * i.e. < 1000000.  If the timer expires, then reload
 * it.  In this case, carry over (usec - old value) to
 * reduce the value reloaded into the timer so that
 * the timer does not drift.  This routine assumes
 * that it is called in a context where the timers
 * on which it is operating cannot change in value.
 */
int
itimerdecr(itp, usec)
	register struct itimerval *itp;
	int usec;
{

	if (itp->it_value.tv_usec < usec) {
		if (itp->it_value.tv_sec == 0) {
			/* expired, and already in next interval */
			usec -= itp->it_value.tv_usec;
			goto expire;
		}
		itp->it_value.tv_usec += 1000000;
		itp->it_value.tv_sec--;
	}
	itp->it_value.tv_usec -= usec;
	usec = 0;
	if (timerisset(&itp->it_value))
		return (1);
	/* expired, exactly at end of interval */
expire:
	if (timerisset(&itp->it_interval)) {
		itp->it_value = itp->it_interval;
		itp->it_value.tv_usec -= usec;
		if (itp->it_value.tv_usec < 0) {
			itp->it_value.tv_usec += 1000000;
			itp->it_value.tv_sec--;
		}
	} else
		itp->it_value.tv_usec = 0;		/* sec is already 0 */
	return (0);
}
