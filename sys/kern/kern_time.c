/*	$NetBSD: kern_time.c,v 1.70.2.3 2004/09/21 13:35:11 skrll Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christopher G. Demetriou.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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
 *	@(#)kern_time.c	8.4 (Berkeley) 5/26/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_time.c,v 1.70.2.3 2004/09/21 13:35:11 skrll Exp $");

#include "fs_nfs.h"
#include "opt_nfs.h"
#include "opt_nfsserver.h"

#include <sys/param.h>
#include <sys/resourcevar.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/sa.h>
#include <sys/savar.h>
#include <sys/vnode.h>
#include <sys/signalvar.h>
#include <sys/syslog.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <uvm/uvm_extern.h>

#if defined(NFS) || defined(NFSSERVER)
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs_var.h>
#endif

#include <machine/cpu.h>

static void timerupcall(struct lwp *, void *);


/* Time of day and interval timer support.
 *
 * These routines provide the kernel entry points to get and set
 * the time-of-day and per-process interval timers.  Subroutines
 * here provide support for adding and subtracting timeval structures
 * and decrementing interval timers, optionally reloading the interval
 * timers when they expire.
 */

/* This function is used by clock_settime and settimeofday */
int
settime(struct timeval *tv)
{
	struct timeval delta;
	struct cpu_info *ci;
	int s;

	/* WHAT DO WE DO ABOUT PENDING REAL-TIME TIMEOUTS??? */
	s = splclock();
	timersub(tv, &time, &delta);
	if ((delta.tv_sec < 0 || delta.tv_usec < 0) && securelevel > 1) {
		splx(s);
		return (EPERM);
	}
#ifdef notyet
	if ((delta.tv_sec < 86400) && securelevel > 0) {
		splx(s);
		return (EPERM);
	}
#endif
	time = *tv;
	(void) spllowersoftclock();
	timeradd(&boottime, &delta, &boottime);
	/*
	 * XXXSMP
	 * This is wrong.  We should traverse a list of all
	 * CPUs and add the delta to the runtime of those
	 * CPUs which have a process on them.
	 */
	ci = curcpu();
	timeradd(&ci->ci_schedstate.spc_runtime, &delta,
	    &ci->ci_schedstate.spc_runtime);
#	if (defined(NFS) && !defined (NFS_V2_ONLY)) || defined(NFSSERVER)
		nqnfs_lease_updatetime(delta.tv_sec);
#	endif
	splx(s);
	resettodr();
	return (0);
}

/* ARGSUSED */
int
sys_clock_gettime(struct lwp *l, void *v, register_t *retval)
{
	struct sys_clock_gettime_args /* {
		syscallarg(clockid_t) clock_id;
		syscallarg(struct timespec *) tp;
	} */ *uap = v;
	clockid_t clock_id;
	struct timeval atv;
	struct timespec ats;
	int s;

	clock_id = SCARG(uap, clock_id);
	switch (clock_id) {
	case CLOCK_REALTIME:
		microtime(&atv);
		TIMEVAL_TO_TIMESPEC(&atv,&ats);
		break;
	case CLOCK_MONOTONIC:
		/* XXX "hz" granularity */
		s = splclock();
		atv = mono_time;
		splx(s);
		TIMEVAL_TO_TIMESPEC(&atv,&ats);
		break;
	default:
		return (EINVAL);
	}

	return copyout(&ats, SCARG(uap, tp), sizeof(ats));
}

/* ARGSUSED */
int
sys_clock_settime(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct sys_clock_settime_args /* {
		syscallarg(clockid_t) clock_id;
		syscallarg(const struct timespec *) tp;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	int error;

	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);

	return (clock_settime1(SCARG(uap, clock_id), SCARG(uap, tp)));
}


int
clock_settime1(clock_id, tp)
	clockid_t clock_id;
	const struct timespec *tp;
{
	struct timespec ats;
	struct timeval atv;
	int error;

	if ((error = copyin(tp, &ats, sizeof(ats))) != 0)
		return (error);

	switch (clock_id) {
	case CLOCK_REALTIME:
		TIMESPEC_TO_TIMEVAL(&atv, &ats);
		if ((error = settime(&atv)) != 0)
			return (error);
		break;
	case CLOCK_MONOTONIC:
		return (EINVAL);	/* read-only clock */
	default:
		return (EINVAL);
	}

	return 0;
}

int
sys_clock_getres(struct lwp *l, void *v, register_t *retval)
{
	struct sys_clock_getres_args /* {
		syscallarg(clockid_t) clock_id;
		syscallarg(struct timespec *) tp;
	} */ *uap = v;
	clockid_t clock_id;
	struct timespec ts;
	int error = 0;

	clock_id = SCARG(uap, clock_id);
	switch (clock_id) {
	case CLOCK_REALTIME:
	case CLOCK_MONOTONIC:
		ts.tv_sec = 0;
		ts.tv_nsec = 1000000000 / hz;
		break;
	default:
		return (EINVAL);
	}

	if (SCARG(uap, tp))
		error = copyout(&ts, SCARG(uap, tp), sizeof(ts));

	return error;
}

/* ARGSUSED */
int
sys_nanosleep(struct lwp *l, void *v, register_t *retval)
{
	static int nanowait;
	struct sys_nanosleep_args/* {
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
sys_gettimeofday(struct lwp *l, void *v, register_t *retval)
{
	struct sys_gettimeofday_args /* {
		syscallarg(struct timeval *) tp;
		syscallarg(void *) tzp;		really "struct timezone *"
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
sys_settimeofday(struct lwp *l, void *v, register_t *retval)
{
	struct sys_settimeofday_args /* {
		syscallarg(const struct timeval *) tv;
		syscallarg(const void *) tzp;	really "const struct timezone *"
	} */ *uap = v;
	struct proc *p = l->l_proc;
	int error;

	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);

	return settimeofday1(SCARG(uap, tv), SCARG(uap, tzp), p);
}

int
settimeofday1(utv, utzp, p)
	const struct timeval *utv;
	const struct timezone *utzp;
	struct proc *p;
{
	struct timeval atv;
	struct timezone atz;
	struct timeval *tv = NULL;
	struct timezone *tzp = NULL;
	int error;

	/* Verify all parameters before changing time. */
	if (utv) {
		if ((error = copyin(utv, &atv, sizeof(atv))) != 0)
			return (error);
		tv = &atv;
	}
	/* XXX since we don't use tz, probably no point in doing copyin. */
	if (utzp) {
		if ((error = copyin(utzp, &atz, sizeof(atz))) != 0)
			return (error);
		tzp = &atz;
	}

	if (tv)
		if ((error = settime(tv)) != 0)
			return (error);
	/*
	 * NetBSD has no kernel notion of time zone, and only an
	 * obsolete program would try to set it, so we log a warning.
	 */
	if (tzp)
		log(LOG_WARNING, "pid %d attempted to set the "
		    "(obsolete) kernel time zone\n", p->p_pid);
	return (0);
}

int	tickdelta;			/* current clock skew, us. per tick */
long	timedelta;			/* unapplied time correction, us. */
long	bigadj = 1000000;		/* use 10x skew above bigadj us. */
int	time_adjusted;			/* set if an adjustment is made */

/* ARGSUSED */
int
sys_adjtime(struct lwp *l, void *v, register_t *retval)
{
	struct sys_adjtime_args /* {
		syscallarg(const struct timeval *) delta;
		syscallarg(struct timeval *) olddelta;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	int error;

	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);

	return adjtime1(SCARG(uap, delta), SCARG(uap, olddelta), p);
}

int
adjtime1(delta, olddelta, p)
	const struct timeval *delta;
	struct timeval *olddelta;
	struct proc *p;
{
	struct timeval atv;
	long ndelta, ntickdelta, odelta;
	int error;
	int s;

	error = copyin(delta, &atv, sizeof(struct timeval));
	if (error)
		return (error);

	/*
	 * Compute the total correction and the rate at which to apply it.
	 * Round the adjustment down to a whole multiple of the per-tick
	 * delta, so that after some number of incremental changes in
	 * hardclock(), tickdelta will become zero, lest the correction
	 * overshoot and start taking us away from the desired final time.
	 */
	ndelta = atv.tv_sec * 1000000 + atv.tv_usec;
	if (ndelta > bigadj || ndelta < -bigadj)
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
	if (ndelta != 0)
		/* We need to save the system clock time during shutdown */
		time_adjusted |= 1;
	s = splclock();
	odelta = timedelta;
	timedelta = ndelta;
	tickdelta = ntickdelta;
	splx(s);

	if (olddelta) {
		atv.tv_sec = odelta / 1000000;
		atv.tv_usec = odelta % 1000000;
		error = copyout(&atv, olddelta, sizeof(struct timeval));
	}
	return error;
}

/*
 * Interval timer support. Both the BSD getitimer() family and the POSIX
 * timer_*() family of routines are supported.
 *
 * All timers are kept in an array pointed to by p_timers, which is
 * allocated on demand - many processes don't use timers at all. The
 * first three elements in this array are reserved for the BSD timers:
 * element 0 is ITIMER_REAL, element 1 is ITIMER_VIRTUAL, and element
 * 2 is ITIMER_PROF. The rest may be allocated by the timer_create()
 * syscall.
 *
 * Realtime timers are kept in the ptimer structure as an absolute
 * time; virtual time timers are kept as a linked list of deltas.
 * Virtual time timers are processed in the hardclock() routine of
 * kern_clock.c.  The real time timer is processed by a callout
 * routine, called from the softclock() routine.  Since a callout may
 * be delayed in real time due to interrupt processing in the system,
 * it is possible for the real time timeout routine (realtimeexpire,
 * given below), to be delayed in real time past when it is supposed
 * to occur.  It does not suffice, therefore, to reload the real timer
 * .it_value from the real time timers .it_interval.  Rather, we
 * compute the next time in absolute time the timer should go off.  */

/* Allocate a POSIX realtime timer. */
int
sys_timer_create(struct lwp *l, void *v, register_t *retval)
{
	struct sys_timer_create_args /* {
		syscallarg(clockid_t) clock_id;
		syscallarg(struct sigevent *) evp;
		syscallarg(timer_t *) timerid;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	clockid_t id;
	struct sigevent *evp;
	struct ptimer *pt;
	timer_t timerid;
	int error;

	id = SCARG(uap, clock_id);
	if (id < CLOCK_REALTIME ||
	    id > CLOCK_PROF)
		return (EINVAL);

	if (p->p_timers == NULL)
		timers_alloc(p);

	/* Find a free timer slot, skipping those reserved for setitimer(). */
	for (timerid = 3; timerid < TIMER_MAX; timerid++)
		if (p->p_timers->pts_timers[timerid] == NULL)
			break;

	if (timerid == TIMER_MAX)
		return EAGAIN;

	pt = pool_get(&ptimer_pool, PR_WAITOK);
	evp = SCARG(uap, evp);
	if (evp) {
		if (((error =
		    copyin(evp, &pt->pt_ev, sizeof (pt->pt_ev))) != 0) ||
		    ((pt->pt_ev.sigev_notify < SIGEV_NONE) ||
			(pt->pt_ev.sigev_notify > SIGEV_SA))) {
			pool_put(&ptimer_pool, pt);
			return (error ? error : EINVAL);
		}
	} else {
		pt->pt_ev.sigev_notify = SIGEV_SIGNAL;
		switch (id) {
		case CLOCK_REALTIME:
			pt->pt_ev.sigev_signo = SIGALRM;
			break;
		case CLOCK_VIRTUAL:
			pt->pt_ev.sigev_signo = SIGVTALRM;
			break;
		case CLOCK_PROF:
			pt->pt_ev.sigev_signo = SIGPROF;
			break;
		}
		pt->pt_ev.sigev_value.sival_int = timerid;
	}
	pt->pt_info.ksi_signo = pt->pt_ev.sigev_signo;
	pt->pt_info.ksi_errno = 0;
	pt->pt_info.ksi_code = 0;
	pt->pt_info.ksi_pid = p->p_pid;
	pt->pt_info.ksi_uid = p->p_cred->p_ruid;
	pt->pt_info.ksi_sigval = pt->pt_ev.sigev_value;

	pt->pt_type = id;
	pt->pt_proc = p;
	pt->pt_overruns = 0;
	pt->pt_poverruns = 0;
	pt->pt_entry = timerid;
	timerclear(&pt->pt_time.it_value);
	if (id == CLOCK_REALTIME)
		callout_init(&pt->pt_ch);
	else
		pt->pt_active = 0;

	p->p_timers->pts_timers[timerid] = pt;

	return copyout(&timerid, SCARG(uap, timerid), sizeof(timerid));
}


/* Delete a POSIX realtime timer */
int
sys_timer_delete(struct lwp *l, void *v, register_t *retval)
{
	struct sys_timer_delete_args /*  {
		syscallarg(timer_t) timerid;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	timer_t timerid;
	struct ptimer *pt, *ptn;
	int s;

	timerid = SCARG(uap, timerid);

	if ((p->p_timers == NULL) ||
	    (timerid < 2) || (timerid >= TIMER_MAX) ||
	    ((pt = p->p_timers->pts_timers[timerid]) == NULL))
		return (EINVAL);

	if (pt->pt_type == CLOCK_REALTIME)
		callout_stop(&pt->pt_ch);
	else if (pt->pt_active) {
		s = splclock();
		ptn = LIST_NEXT(pt, pt_list);
		LIST_REMOVE(pt, pt_list);
		for ( ; ptn; ptn = LIST_NEXT(ptn, pt_list))
			timeradd(&pt->pt_time.it_value, &ptn->pt_time.it_value,
			    &ptn->pt_time.it_value);
		splx(s);
	}

	p->p_timers->pts_timers[timerid] = NULL;
	pool_put(&ptimer_pool, pt);

	return (0);
}

/*
 * Set up the given timer. The value in pt->pt_time.it_value is taken
 * to be an absolute time for CLOCK_REALTIME timers and a relative
 * time for virtual timers.
 * Must be called at splclock().
 */
void
timer_settime(struct ptimer *pt)
{
	struct ptimer *ptn, *pptn;
	struct ptlist *ptl;

	if (pt->pt_type == CLOCK_REALTIME) {
		callout_stop(&pt->pt_ch);
		if (timerisset(&pt->pt_time.it_value)) {
			/*
			 * Don't need to check hzto() return value, here.
			 * callout_reset() does it for us.
			 */
			callout_reset(&pt->pt_ch, hzto(&pt->pt_time.it_value),
			    realtimerexpire, pt);
		}
	} else {
		if (pt->pt_active) {
			ptn = LIST_NEXT(pt, pt_list);
			LIST_REMOVE(pt, pt_list);
			for ( ; ptn; ptn = LIST_NEXT(ptn, pt_list))
				timeradd(&pt->pt_time.it_value,
				    &ptn->pt_time.it_value,
				    &ptn->pt_time.it_value);
		}
		if (timerisset(&pt->pt_time.it_value)) {
			if (pt->pt_type == CLOCK_VIRTUAL)
				ptl = &pt->pt_proc->p_timers->pts_virtual;
			else
				ptl = &pt->pt_proc->p_timers->pts_prof;

			for (ptn = LIST_FIRST(ptl), pptn = NULL;
			     ptn && timercmp(&pt->pt_time.it_value,
				 &ptn->pt_time.it_value, >);
			     pptn = ptn, ptn = LIST_NEXT(ptn, pt_list))
				timersub(&pt->pt_time.it_value,
				    &ptn->pt_time.it_value,
				    &pt->pt_time.it_value);

			if (pptn)
				LIST_INSERT_AFTER(pptn, pt, pt_list);
			else
				LIST_INSERT_HEAD(ptl, pt, pt_list);

			for ( ; ptn ; ptn = LIST_NEXT(ptn, pt_list))
				timersub(&ptn->pt_time.it_value,
				    &pt->pt_time.it_value,
				    &ptn->pt_time.it_value);

			pt->pt_active = 1;
		} else
			pt->pt_active = 0;
	}
}

void
timer_gettime(struct ptimer *pt, struct itimerval *aitv)
{
	struct ptimer *ptn;

	*aitv = pt->pt_time;
	if (pt->pt_type == CLOCK_REALTIME) {
		/*
		 * Convert from absolute to relative time in .it_value
		 * part of real time timer.  If time for real time
		 * timer has passed return 0, else return difference
		 * between current time and time for the timer to go
		 * off.
		 */
		if (timerisset(&aitv->it_value)) {
			if (timercmp(&aitv->it_value, &time, <))
				timerclear(&aitv->it_value);
			else
				timersub(&aitv->it_value, &time,
				    &aitv->it_value);
		}
	} else if (pt->pt_active) {
		if (pt->pt_type == CLOCK_VIRTUAL)
			ptn = LIST_FIRST(&pt->pt_proc->p_timers->pts_virtual);
		else
			ptn = LIST_FIRST(&pt->pt_proc->p_timers->pts_prof);
		for ( ; ptn && ptn != pt; ptn = LIST_NEXT(ptn, pt_list))
			timeradd(&aitv->it_value,
			    &ptn->pt_time.it_value, &aitv->it_value);
		KASSERT(ptn != NULL); /* pt should be findable on the list */
	} else
		timerclear(&aitv->it_value);
}



/* Set and arm a POSIX realtime timer */
int
sys_timer_settime(struct lwp *l, void *v, register_t *retval)
{
	struct sys_timer_settime_args /* {
		syscallarg(timer_t) timerid;
		syscallarg(int) flags;
		syscallarg(const struct itimerspec *) value;
		syscallarg(struct itimerspec *) ovalue;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	int error, s, timerid;
	struct itimerval val, oval;
	struct itimerspec value, ovalue;
	struct ptimer *pt;

	timerid = SCARG(uap, timerid);

	if ((p->p_timers == NULL) ||
	    (timerid < 2) || (timerid >= TIMER_MAX) ||
	    ((pt = p->p_timers->pts_timers[timerid]) == NULL))
		return (EINVAL);

	if ((error = copyin(SCARG(uap, value), &value,
	    sizeof(struct itimerspec))) != 0)
		return (error);

	TIMESPEC_TO_TIMEVAL(&val.it_value, &value.it_value);
	TIMESPEC_TO_TIMEVAL(&val.it_interval, &value.it_interval);
	if (itimerfix(&val.it_value) || itimerfix(&val.it_interval))
		return (EINVAL);

	oval = pt->pt_time;
	pt->pt_time = val;

	s = splclock();
	/*
	 * If we've been passed a relative time for a realtime timer,
	 * convert it to absolute; if an absolute time for a virtual
	 * timer, convert it to relative and make sure we don't set it
	 * to zero, which would cancel the timer, or let it go
	 * negative, which would confuse the comparison tests.
	 */
	if (timerisset(&pt->pt_time.it_value)) {
		if (pt->pt_type == CLOCK_REALTIME) {
			if ((SCARG(uap, flags) & TIMER_ABSTIME) == 0)
				timeradd(&pt->pt_time.it_value, &time,
				    &pt->pt_time.it_value);
		} else {
			if ((SCARG(uap, flags) & TIMER_ABSTIME) != 0) {
				timersub(&pt->pt_time.it_value, &time,
				    &pt->pt_time.it_value);
				if (!timerisset(&pt->pt_time.it_value) ||
				    pt->pt_time.it_value.tv_sec < 0) {
					pt->pt_time.it_value.tv_sec = 0;
					pt->pt_time.it_value.tv_usec = 1;
				}
			}
		}
	}

	timer_settime(pt);
	splx(s);

	if (SCARG(uap, ovalue)) {
		TIMEVAL_TO_TIMESPEC(&oval.it_value, &ovalue.it_value);
		TIMEVAL_TO_TIMESPEC(&oval.it_interval, &ovalue.it_interval);
		return copyout(&ovalue, SCARG(uap, ovalue),
		    sizeof(struct itimerspec));
	}

	return (0);
}

/* Return the time remaining until a POSIX timer fires. */
int
sys_timer_gettime(struct lwp *l, void *v, register_t *retval)
{
	struct sys_timer_gettime_args /* {
		syscallarg(timer_t) timerid;
		syscallarg(struct itimerspec *) value;
	} */ *uap = v;
	struct itimerval aitv;
	struct itimerspec its;
	struct proc *p = l->l_proc;
	int s, timerid;
	struct ptimer *pt;

	timerid = SCARG(uap, timerid);

	if ((p->p_timers == NULL) ||
	    (timerid < 2) || (timerid >= TIMER_MAX) ||
	    ((pt = p->p_timers->pts_timers[timerid]) == NULL))
		return (EINVAL);

	s = splclock();
	timer_gettime(pt, &aitv);
	splx(s);

	TIMEVAL_TO_TIMESPEC(&aitv.it_interval, &its.it_interval);
	TIMEVAL_TO_TIMESPEC(&aitv.it_value, &its.it_value);

	return copyout(&its, SCARG(uap, value), sizeof(its));
}

/*
 * Return the count of the number of times a periodic timer expired
 * while a notification was already pending. The counter is reset when
 * a timer expires and a notification can be posted.
 */
int
sys_timer_getoverrun(struct lwp *l, void *v, register_t *retval)
{
	struct sys_timer_getoverrun_args /* {
		syscallarg(timer_t) timerid;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	int timerid;
	struct ptimer *pt;

	timerid = SCARG(uap, timerid);

	if ((p->p_timers == NULL) ||
	    (timerid < 2) || (timerid >= TIMER_MAX) ||
	    ((pt = p->p_timers->pts_timers[timerid]) == NULL))
		return (EINVAL);

	*retval = pt->pt_poverruns;

	return (0);
}

/* Glue function that triggers an upcall; called from userret(). */
static void
timerupcall(struct lwp *l, void *arg)
{
	struct ptimers *pt = (struct ptimers *)arg;
	unsigned int i, fired, done;
	extern struct pool siginfo_pool;	/* XXX Ew. */

	KDASSERT(l->l_proc->p_sa);
	/* Bail out if we do not own the virtual processor */
	if (l->l_savp->savp_lwp != l)
		return ;
	
	KERNEL_PROC_LOCK(l);

	fired = pt->pts_fired;
	done = 0;
	while ((i = ffs(fired)) != 0) {
		siginfo_t *si;
		int mask = 1 << --i;
		int f;

		f = l->l_flag & L_SA;
		l->l_flag &= ~L_SA;
		si = pool_get(&siginfo_pool, PR_WAITOK);
		si->_info = pt->pts_timers[i]->pt_info.ksi_info;
		if (sa_upcall(l, SA_UPCALL_SIGEV | SA_UPCALL_DEFER, NULL, l,
		    sizeof(*si), si) == 0)
			done |= mask;
		fired &= ~mask;
		l->l_flag |= f;
	}
	pt->pts_fired &= ~done;
	if (pt->pts_fired == 0)
		l->l_proc->p_userret = NULL;

	KERNEL_PROC_UNLOCK(l);
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
realtimerexpire(void *arg)
{
	struct ptimer *pt;
	int s;

	pt = (struct ptimer *)arg;

	itimerfire(pt);

	if (!timerisset(&pt->pt_time.it_interval)) {
		timerclear(&pt->pt_time.it_value);
		return;
	}
	for (;;) {
		s = splclock();
		timeradd(&pt->pt_time.it_value,
		    &pt->pt_time.it_interval, &pt->pt_time.it_value);
		if (timercmp(&pt->pt_time.it_value, &time, >)) {
			/*
			 * Don't need to check hzto() return value, here.
			 * callout_reset() does it for us.
			 */
			callout_reset(&pt->pt_ch, hzto(&pt->pt_time.it_value),
			    realtimerexpire, pt);
			splx(s);
			return;
		}
		splx(s);
		pt->pt_overruns++;
	}
}

/* BSD routine to get the value of an interval timer. */
/* ARGSUSED */
int
sys_getitimer(struct lwp *l, void *v, register_t *retval)
{
	struct sys_getitimer_args /* {
		syscallarg(int) which;
		syscallarg(struct itimerval *) itv;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	struct itimerval aitv;
	int s, which;

	which = SCARG(uap, which);

	if ((u_int)which > ITIMER_PROF)
		return (EINVAL);

	if ((p->p_timers == NULL) || (p->p_timers->pts_timers[which] == NULL)){
		timerclear(&aitv.it_value);
		timerclear(&aitv.it_interval);
	} else {
		s = splclock();
		timer_gettime(p->p_timers->pts_timers[which], &aitv);
		splx(s);
	}

	return (copyout(&aitv, SCARG(uap, itv), sizeof(struct itimerval)));

}

/* BSD routine to set/arm an interval timer. */
/* ARGSUSED */
int
sys_setitimer(struct lwp *l, void *v, register_t *retval)
{
	struct sys_setitimer_args /* {
		syscallarg(int) which;
		syscallarg(const struct itimerval *) itv;
		syscallarg(struct itimerval *) oitv;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	int which = SCARG(uap, which);
	struct sys_getitimer_args getargs;
	struct itimerval aitv;
	const struct itimerval *itvp;
	struct ptimer *pt;
	int s, error;

	if ((u_int)which > ITIMER_PROF)
		return (EINVAL);
	itvp = SCARG(uap, itv);
	if (itvp &&
	    (error = copyin(itvp, &aitv, sizeof(struct itimerval)) != 0))
		return (error);
	if (SCARG(uap, oitv) != NULL) {
		SCARG(&getargs, which) = which;
		SCARG(&getargs, itv) = SCARG(uap, oitv);
		if ((error = sys_getitimer(l, &getargs, retval)) != 0)
			return (error);
	}
	if (itvp == 0)
		return (0);
	if (itimerfix(&aitv.it_value) || itimerfix(&aitv.it_interval))
		return (EINVAL);

	/*
	 * Don't bother allocating data structures if the process just
	 * wants to clear the timer.
	 */
	if (!timerisset(&aitv.it_value) &&
	    ((p->p_timers == NULL) ||(p->p_timers->pts_timers[which] == NULL)))
		return (0);

	if (p->p_timers == NULL)
		timers_alloc(p);
	if (p->p_timers->pts_timers[which] == NULL) {
		pt = pool_get(&ptimer_pool, PR_WAITOK);
		pt->pt_ev.sigev_notify = SIGEV_SIGNAL;
		pt->pt_ev.sigev_value.sival_int = which;
		pt->pt_overruns = 0;
		pt->pt_proc = p;
		pt->pt_type = which;
		pt->pt_entry = which;
		switch (which) {
		case ITIMER_REAL:
			callout_init(&pt->pt_ch);
			pt->pt_ev.sigev_signo = SIGALRM;
			break;
		case ITIMER_VIRTUAL:
			pt->pt_active = 0;
			pt->pt_ev.sigev_signo = SIGVTALRM;
			break;
		case ITIMER_PROF:
			pt->pt_active = 0;
			pt->pt_ev.sigev_signo = SIGPROF;
			break;
		}
	} else
		pt = p->p_timers->pts_timers[which];

	pt->pt_time = aitv;
	p->p_timers->pts_timers[which] = pt;

	s = splclock();
	if ((which == ITIMER_REAL) && timerisset(&pt->pt_time.it_value)) {
		/* Convert to absolute time */
		timeradd(&pt->pt_time.it_value, &time, &pt->pt_time.it_value);
	}
	timer_settime(pt);
	splx(s);

	return (0);
}

/* Utility routines to manage the array of pointers to timers. */
void
timers_alloc(struct proc *p)
{
	int i;
	struct ptimers *pts;

	pts = malloc(sizeof (struct ptimers), M_SUBPROC, 0);
	LIST_INIT(&pts->pts_virtual);
	LIST_INIT(&pts->pts_prof);
	for (i = 0; i < TIMER_MAX; i++)
		pts->pts_timers[i] = NULL;
	pts->pts_fired = 0;
	p->p_timers = pts;
}

/*
 * Clean up the per-process timers. If "which" is set to TIMERS_ALL,
 * then clean up all timers and free all the data structures. If
 * "which" is set to TIMERS_POSIX, only clean up the timers allocated
 * by timer_create(), not the BSD setitimer() timers, and only free the
 * structure if none of those remain.
 */
void
timers_free(struct proc *p, int which)
{
	int i, s;
	struct ptimers *pts;
	struct ptimer *pt, *ptn;
	struct timeval tv;

	if (p->p_timers) {
		pts = p->p_timers;
		if (which == TIMERS_ALL)
			i = 0;
		else {
			s = splclock();
			timerclear(&tv);
			for (ptn = LIST_FIRST(&p->p_timers->pts_virtual);
			     ptn && ptn != pts->pts_timers[ITIMER_VIRTUAL];
			     ptn = LIST_NEXT(ptn, pt_list))
				timeradd(&tv, &ptn->pt_time.it_value, &tv);
			LIST_FIRST(&p->p_timers->pts_virtual) = NULL;
			if (ptn) {
				timeradd(&tv, &ptn->pt_time.it_value,
				    &ptn->pt_time.it_value);
				LIST_INSERT_HEAD(&p->p_timers->pts_virtual,
				    ptn, pt_list);
			}

			timerclear(&tv);
			for (ptn = LIST_FIRST(&p->p_timers->pts_prof);
			     ptn && ptn != pts->pts_timers[ITIMER_PROF];
			     ptn = LIST_NEXT(ptn, pt_list))
				timeradd(&tv, &ptn->pt_time.it_value, &tv);
			LIST_FIRST(&p->p_timers->pts_prof) = NULL;
			if (ptn) {
				timeradd(&tv, &ptn->pt_time.it_value,
				    &ptn->pt_time.it_value);
				LIST_INSERT_HEAD(&p->p_timers->pts_prof, ptn,
				    pt_list);
			}
			splx(s);
			i = 3;
		}
		for ( ; i < TIMER_MAX; i++)
			if ((pt = pts->pts_timers[i]) != NULL) {
				if (pt->pt_type == CLOCK_REALTIME)
					callout_stop(&pt->pt_ch);
				pts->pts_timers[i] = NULL;
				pool_put(&ptimer_pool, pt);
			}
		if ((pts->pts_timers[0] == NULL) &&
		    (pts->pts_timers[1] == NULL) &&
		    (pts->pts_timers[2] == NULL)) {
			p->p_timers = NULL;
			free(pts, M_SUBPROC);
		}
	}
}

/*
 * Check that a proposed value to load into the .it_value or
 * .it_interval part of an interval timer is acceptable, and
 * fix it to have at least minimal value (i.e. if it is less
 * than the resolution of the clock, round it up.)
 */
int
itimerfix(struct timeval *tv)
{

	if (tv->tv_sec < 0 || tv->tv_usec < 0 || tv->tv_usec >= 1000000)
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
itimerdecr(struct ptimer *pt, int usec)
{
	struct itimerval *itp;

	itp = &pt->pt_time;
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
		timer_settime(pt);
	} else
		itp->it_value.tv_usec = 0;		/* sec is already 0 */
	return (0);
}

void
itimerfire(struct ptimer *pt)
{
	struct proc *p = pt->pt_proc;
	struct sadata_vp *vp;
	int s;
	unsigned int i;

	if (pt->pt_ev.sigev_notify == SIGEV_SIGNAL) {
		/*
		 * No RT signal infrastructure exists at this time;
		 * just post the signal number and throw away the
		 * value.
		 */
		if (sigismember(&p->p_sigctx.ps_siglist, pt->pt_ev.sigev_signo))
			pt->pt_overruns++;
		else {
			ksiginfo_t ksi;
			(void)memset(&ksi, 0, sizeof(ksi));
			ksi.ksi_signo = pt->pt_ev.sigev_signo;
			ksi.ksi_code = SI_TIMER;
			ksi.ksi_sigval = pt->pt_ev.sigev_value;
			pt->pt_poverruns = pt->pt_overruns;
			pt->pt_overruns = 0;
			kpsignal(p, &ksi, NULL);
		}
	} else if (pt->pt_ev.sigev_notify == SIGEV_SA && (p->p_flag & P_SA)) {
		/* Cause the process to generate an upcall when it returns. */

		if (p->p_userret == NULL) {
			/*
			 * XXX stop signals can be processed inside tsleep,
			 * which can be inside sa_yield's inner loop, which
			 * makes testing for sa_idle alone insuffucent to
			 * determine if we really should call setrunnable.
			 */
			pt->pt_poverruns = pt->pt_overruns;
			pt->pt_overruns = 0;
			i = 1 << pt->pt_entry;
			p->p_timers->pts_fired = i;
			p->p_userret = timerupcall;
			p->p_userret_arg = p->p_timers;
			
			SCHED_LOCK(s);
			SLIST_FOREACH(vp, &p->p_sa->sa_vps, savp_next) {
				if (vp->savp_lwp->l_flag & L_SA_IDLE) {
					vp->savp_lwp->l_flag &= ~L_SA_IDLE;
					sched_wakeup(vp->savp_lwp);
					break;
				}
			}
			SCHED_UNLOCK(s);
		} else if (p->p_userret == timerupcall) {
			i = 1 << pt->pt_entry;
			if ((p->p_timers->pts_fired & i) == 0) {
				pt->pt_poverruns = pt->pt_overruns;
				pt->pt_overruns = 0;
				p->p_timers->pts_fired |= i;
			} else
				pt->pt_overruns++;
		} else {
			pt->pt_overruns++;
			if ((p->p_flag & P_WEXIT) == 0)
				printf("itimerfire(%d): overrun %d on timer %x (userret is %p)\n",
				    p->p_pid, pt->pt_overruns,
				    pt->pt_ev.sigev_value.sival_int,
				    p->p_userret);
		}
	}

}

/*
 * ratecheck(): simple time-based rate-limit checking.  see ratecheck(9)
 * for usage and rationale.
 */
int
ratecheck(struct timeval *lasttime, const struct timeval *mininterval)
{
	struct timeval tv, delta;
	int s, rv = 0;

	s = splclock();
	tv = mono_time;
	splx(s);

	timersub(&tv, lasttime, &delta);

	/*
	 * check for 0,0 is so that the message will be seen at least once,
	 * even if interval is huge.
	 */
	if (timercmp(&delta, mininterval, >=) ||
	    (lasttime->tv_sec == 0 && lasttime->tv_usec == 0)) {
		*lasttime = tv;
		rv = 1;
	}

	return (rv);
}

/*
 * ppsratecheck(): packets (or events) per second limitation.
 */
int
ppsratecheck(struct timeval *lasttime, int *curpps, int maxpps)
{
	struct timeval tv, delta;
	int s, rv;

	s = splclock();
	tv = mono_time;
	splx(s);

	timersub(&tv, lasttime, &delta);

	/*
	 * check for 0,0 is so that the message will be seen at least once.
	 * if more than one second have passed since the last update of
	 * lasttime, reset the counter.
	 *
	 * we do increment *curpps even in *curpps < maxpps case, as some may
	 * try to use *curpps for stat purposes as well.
	 */
	if ((lasttime->tv_sec == 0 && lasttime->tv_usec == 0) ||
	    delta.tv_sec >= 1) {
		*lasttime = tv;
		*curpps = 0;
	}
	if (maxpps < 0)
		rv = 1;
	else if (*curpps < maxpps)
		rv = 1;
	else
		rv = 0;

#if 1 /*DIAGNOSTIC?*/
	/* be careful about wrap-around */
	if (*curpps + 1 > *curpps)
		*curpps = *curpps + 1;
#else
	/*
	 * assume that there's not too many calls to this function.
	 * not sure if the assumption holds, as it depends on *caller's*
	 * behavior, not the behavior of this function.
	 * IMHO it is wrong to make assumption on the caller's behavior,
	 * so the above #if is #if 1, not #ifdef DIAGNOSTIC.
	 */
	*curpps = *curpps + 1;
#endif

	return (rv);
}
