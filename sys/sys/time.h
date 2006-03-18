/*	$NetBSD: time.h,v 1.52 2005/12/05 00:16:34 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1993
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
 *	@(#)time.h	8.5 (Berkeley) 5/4/95
 */

#ifndef _SYS_TIME_H_
#define	_SYS_TIME_H_

#include <sys/featuretest.h>
#include <sys/types.h>
#ifdef _KERNEL
#include <sys/callout.h>
#include <sys/signal.h>
#include <sys/queue.h>
#endif

/*
 * Structure returned by gettimeofday(2) system call,
 * and used in other calls.
 */
struct timeval {
	long	tv_sec;		/* seconds */
	long	tv_usec;	/* and microseconds */
};

/*
 * Structure defined by POSIX.1b to be like a timeval.
 */
struct timespec {
	time_t	tv_sec;		/* seconds */
	long	tv_nsec;	/* and nanoseconds */
};

#define	TIMEVAL_TO_TIMESPEC(tv, ts) do {				\
	(ts)->tv_sec = (tv)->tv_sec;					\
	(ts)->tv_nsec = (tv)->tv_usec * 1000;				\
} while (/*CONSTCOND*/0)
#define	TIMESPEC_TO_TIMEVAL(tv, ts) do {				\
	(tv)->tv_sec = (ts)->tv_sec;					\
	(tv)->tv_usec = (ts)->tv_nsec / 1000;				\
} while (/*CONSTCOND*/0)

/*
 * Note: timezone is obsolete. All timezone handling is now in
 * userland. Its just here for back compatibility.
 */
struct timezone {
	int	tz_minuteswest;	/* minutes west of Greenwich */
	int	tz_dsttime;	/* type of dst correction */
};

/* Operations on timevals. */
#define	timerclear(tvp)		(tvp)->tv_sec = (tvp)->tv_usec = 0
#define	timerisset(tvp)		((tvp)->tv_sec || (tvp)->tv_usec)
#define	timercmp(tvp, uvp, cmp)						\
	(((tvp)->tv_sec == (uvp)->tv_sec) ?				\
	    ((tvp)->tv_usec cmp (uvp)->tv_usec) :			\
	    ((tvp)->tv_sec cmp (uvp)->tv_sec))
#define	timeradd(tvp, uvp, vvp)						\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec + (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec + (uvp)->tv_usec;	\
		if ((vvp)->tv_usec >= 1000000) {			\
			(vvp)->tv_sec++;				\
			(vvp)->tv_usec -= 1000000;			\
		}							\
	} while (/* CONSTCOND */ 0)
#define	timersub(tvp, uvp, vvp)						\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;	\
		if ((vvp)->tv_usec < 0) {				\
			(vvp)->tv_sec--;				\
			(vvp)->tv_usec += 1000000;			\
		}							\
	} while (/* CONSTCOND */ 0)

/* Operations on timespecs. */
#define	timespecclear(tsp)		(tsp)->tv_sec = (tsp)->tv_nsec = 0
#define	timespecisset(tsp)		((tsp)->tv_sec || (tsp)->tv_nsec)
#define	timespeccmp(tsp, usp, cmp)					\
	(((tsp)->tv_sec == (usp)->tv_sec) ?				\
	    ((tsp)->tv_nsec cmp (usp)->tv_nsec) :			\
	    ((tsp)->tv_sec cmp (usp)->tv_sec))
#define	timespecadd(tsp, usp, vsp)					\
	do {								\
		(vsp)->tv_sec = (tsp)->tv_sec + (usp)->tv_sec;		\
		(vsp)->tv_nsec = (tsp)->tv_nsec + (usp)->tv_nsec;	\
		if ((vsp)->tv_nsec >= 1000000000L) {			\
			(vsp)->tv_sec++;				\
			(vsp)->tv_nsec -= 1000000000L;			\
		}							\
	} while (/* CONSTCOND */ 0)
#define	timespecsub(tsp, usp, vsp)					\
	do {								\
		(vsp)->tv_sec = (tsp)->tv_sec - (usp)->tv_sec;		\
		(vsp)->tv_nsec = (tsp)->tv_nsec - (usp)->tv_nsec;	\
		if ((vsp)->tv_nsec < 0) {				\
			(vsp)->tv_sec--;				\
			(vsp)->tv_nsec += 1000000000L;			\
		}							\
	} while (/* CONSTCOND */ 0)

/*
 * Names of the interval timers, and structure
 * defining a timer setting.
 */
#define	ITIMER_REAL	0
#define	ITIMER_VIRTUAL	1
#define	ITIMER_PROF	2

struct	itimerval {
	struct	timeval it_interval;	/* timer interval */
	struct	timeval it_value;	/* current value */
};

/*
 * Structure defined by POSIX.1b to be like a itimerval, but with
 * timespecs. Used in the timer_*() system calls.
 */
struct	itimerspec {
	struct	timespec it_interval;
	struct	timespec it_value;
};

/*
 * Getkerninfo clock information structure
 */
struct clockinfo {
	int	hz;		/* clock frequency */
	int	tick;		/* micro-seconds per hz tick */
	int	tickadj;	/* clock skew rate for adjtime() */
	int	stathz;		/* statistics clock frequency */
	int	profhz;		/* profiling clock frequency */
};

#define	CLOCK_REALTIME	0
#define	CLOCK_VIRTUAL	1
#define	CLOCK_PROF	2
#define	CLOCK_MONOTONIC	3

#define	TIMER_RELTIME	0x0	/* relative timer */
#define	TIMER_ABSTIME	0x1	/* absolute timer */

#ifdef _KERNEL
/*
 * Structure used to manage timers in a process.
 */
struct 	ptimer {
	union {
		struct	callout	pt_ch;
		struct {
			LIST_ENTRY(ptimer)	pt_list;
			int	pt_active;
		} pt_nonreal;
	} pt_data;
	struct	sigevent pt_ev;
	struct	itimerval pt_time;
	struct	ksiginfo pt_info;
	int	pt_overruns;	/* Overruns currently accumulating */
	int	pt_poverruns;	/* Overruns associated w/ a delivery */
	int	pt_type;
	int	pt_entry;
	struct proc *pt_proc;
};

#define pt_ch	pt_data.pt_ch
#define pt_list	pt_data.pt_nonreal.pt_list
#define pt_active	pt_data.pt_nonreal.pt_active

#define	TIMER_MAX	32	/* See ptimers->pts_fired if you enlarge this */
#define	TIMERS_ALL	0
#define	TIMERS_POSIX	1

LIST_HEAD(ptlist, ptimer);

struct	ptimers {
	struct ptlist pts_virtual;
	struct ptlist pts_prof;
	struct ptimer *pts_timers[TIMER_MAX];
	int pts_fired;
};

int	itimerfix(struct timeval *tv);
int	itimerdecr(struct ptimer *, int);
void	itimerfire(struct ptimer *);
void	microtime(struct timeval *);
struct timespec	*nanotime(struct timespec *);
int	settime(struct proc *p, struct timespec *);
int	ratecheck(struct timeval *, const struct timeval *);
int	ppsratecheck(struct timeval *, int *, int);
int	settimeofday1(const struct timeval *, const struct timezone *,
	    struct proc *);
int	adjtime1(const struct timeval *, struct timeval *, struct proc *);
int	clock_settime1(struct proc *, clockid_t, const struct timespec *);
void	timer_settime(struct ptimer *);
void	timer_gettime(struct ptimer *, struct itimerval *);
void	timers_alloc(struct proc *);
void	timers_free(struct proc *, int);
void	realtimerexpire(void *);

#else /* !_KERNEL */

#ifndef _STANDALONE
#if (_POSIX_C_SOURCE - 0) >= 200112L || \
    (defined(_XOPEN_SOURCE) && defined(_XOPEN_SOURCE_EXTENDED)) || \
    (_XOPEN_SOURCE - 0) >= 500 || defined(_NETBSD_SOURCE)
#include <sys/select.h>
#endif

#include <time.h>

#if defined(_XOPEN_SOURCE) || defined(_NETBSD_SOURCE)
#include <sys/cdefs.h>

__BEGIN_DECLS
int	adjtime(const struct timeval *, struct timeval *);
int	futimes(int, const struct timeval [2]);
int	getitimer(int, struct itimerval *);
int	gettimeofday(struct timeval * __restrict, void * __restrict);
int	lutimes(const char *, const struct timeval [2]);
int	setitimer(int, const struct itimerval * __restrict,
	    struct itimerval * __restrict);
int	settimeofday(const struct timeval * __restrict,
	    const void * __restrict);
int	utimes(const char *, const struct timeval [2]);
__END_DECLS
#endif /* _XOPEN_SOURCE || _NETBSD_SOURCE */

#endif	/* !_STANDALONE */

#endif /* !_KERNEL */

#endif /* !_SYS_TIME_H_ */
