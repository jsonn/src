/*	$NetBSD: linux_time.c,v 1.14.8.1 2007/01/30 13:51:33 ad Exp $ */

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Emmanuel Dreyfus.
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
 *      This product includes software developed by the NetBSD
 *      Foundation, Inc. and its contributors.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: linux_time.c,v 1.14.8.1 2007/01/30 13:51:33 ad Exp $");

#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#include <sys/signal.h>
#include <sys/stdint.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/syscallargs.h>
#include <sys/lwp.h>
#include <sys/proc.h>

#include <compat/linux/common/linux_types.h>
#include <compat/linux/common/linux_signal.h>
#include <compat/linux/common/linux_machdep.h>
#include <compat/linux/common/linux_sched.h>

#include <compat/linux/linux_syscallargs.h>

#include <compat/common/compat_util.h>

static void native_to_linux_timespec(struct linux_timespec *,
				     struct timespec *);
static void linux_to_native_timespec(struct timespec *,
				     struct linux_timespec *);
static int linux_to_native_clockid(clockid_t *, clockid_t);

/*
 * This is not implemented for alpha yet
 */
#if defined (__i386__) || defined (__m68k__) || \
    defined (__powerpc__) || defined (__mips__) || \
    defined(__arm__) || defined(__amd64__)

/*
 * Linux keeps track of a system timezone in the kernel. It is readen
 * by gettimeofday and set by settimeofday. This emulates this behavior
 * See linux/kernel/time.c
 */
struct timezone linux_sys_tz;

int
linux_sys_gettimeofday(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct linux_sys_gettimeofday_args /* {
		syscallarg(struct timeval *) tz;
		syscallarg(struct timezone *) tzp;
	} */ *uap = v;
	int error = 0;

	if (SCARG(uap, tp)) {
		error = sys_gettimeofday (l, v, retval);
		if (error)
			return (error);
	}

	if (SCARG(uap, tzp)) {
		error = copyout(&linux_sys_tz, SCARG(uap, tzp), sizeof(linux_sys_tz));
		if (error)
			return (error);
   }

	return (0);
}

int
linux_sys_settimeofday(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct linux_sys_settimeofday_args /* {
		syscallarg(struct timeval *) tz;
		syscallarg(struct timezone *) tzp;
	} */ *uap = v;
	int error = 0;

	if (SCARG(uap, tp)) {
		error = sys_settimeofday(l, v, retval);
		if (error)
			return (error);
	}

	/*
	 * If user is not the superuser, we returned
	 * after the sys_settimeofday() call.
	 */
	if (SCARG(uap, tzp)) {
		error = copyin(SCARG(uap, tzp), &linux_sys_tz, sizeof(linux_sys_tz));
		if (error)
			return (error);
   }

	return (0);
}

#endif /* __i386__ || __m68k__ || __powerpc__ || __mips__ || __arm__ */

static void
native_to_linux_timespec(struct linux_timespec *ltp, struct timespec *ntp)
{
	ltp->tv_sec = ntp->tv_sec;
	ltp->tv_nsec = ntp->tv_nsec;
}

static void
linux_to_native_timespec(struct timespec *ntp, struct linux_timespec *ltp)
{
	ntp->tv_sec = ltp->tv_sec;
	ntp->tv_nsec = ltp->tv_nsec;
}

static int
linux_to_native_clockid(clockid_t *n, clockid_t l)
{
	switch (l) {
	case LINUX_CLOCK_REALTIME:
		*n = CLOCK_REALTIME;
		break;
	case LINUX_CLOCK_MONOTONIC:
		*n = CLOCK_MONOTONIC;
		break;
	case LINUX_CLOCK_PROCESS_CPUTIME_ID:
	case LINUX_CLOCK_THREAD_CPUTIME_ID:
	case LINUX_CLOCK_REALTIME_HR:
	case LINUX_CLOCK_MONOTONIC_HR:
		return EINVAL;
	}

	return 0;
}

int
linux_sys_clock_gettime(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct linux_sys_clock_gettime_args /* {
		syscallarg(clockid_t) which;
		syscallarg(struct linux_timespec *)tp;
	} */ *uap = v;
	caddr_t sg;
	struct proc *p = l->l_proc;
	struct timespec *tp, ts;
	struct linux_timespec lts;
	int error;
	clockid_t nwhich = 0;	/* XXX: GCC */
	struct sys_clock_gettime_args sga;

	error = linux_to_native_clockid(&nwhich, SCARG(uap, which));
	if (error != 0)
		return error;
	sg = stackgap_init(p, 0);
	tp = stackgap_alloc(p, &sg, sizeof *tp);

	SCARG(&sga, clock_id) = nwhich;
	SCARG(&sga, tp) = tp;

	error = sys_clock_gettime(l, &sga, retval);
	if (error != 0)
		return error;

	error = copyin(tp, &ts, sizeof ts);
	if (error != 0)
		return error;

	native_to_linux_timespec(&lts, &ts);

	return copyout(&lts, SCARG(uap, tp), sizeof lts);
}

int
linux_sys_clock_settime(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct linux_sys_clock_settime_args /* {
		syscallarg(clockid_t) which;
		syscallarg(struct linux_timespec *)tp;
	} */ *uap = v;
	caddr_t sg;
	struct proc *p = l->l_proc;
	struct timespec *tp, ts;
	struct linux_timespec lts;
	int error;
	clockid_t nwhich = 0;	/* XXX: GCC */
	struct sys_clock_settime_args sta;

	error = linux_to_native_clockid(&nwhich, SCARG(uap, which));
	if (error != 0)
		return error;

	error = copyin(SCARG(uap, tp), &lts, sizeof lts);
	if (error != 0)
		return error;

	linux_to_native_timespec(&ts, &lts);

	sg = stackgap_init(p, 0);
	tp = stackgap_alloc(p, &sg, sizeof *tp);
	error = copyout(&ts, tp, sizeof ts);
	if (error != 0)
		return error;

	SCARG(&sta, clock_id) = nwhich;
	SCARG(&sta, tp) = tp;

	return sys_clock_settime(l, &sta, retval);
}

int
linux_sys_clock_getres(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct linux_sys_clock_gettime_args /* {
		syscallarg(clockid_t) which;
		syscallarg(struct linux_timespec *)tp;
	} */ *uap = v;
	caddr_t sg;
	struct proc *p = l->l_proc;
	struct timespec *tp, ts;
	struct linux_timespec lts;
	int error;
	clockid_t nwhich = 0;	/* XXX: GCC */
	struct sys_clock_gettime_args sga;

	error = linux_to_native_clockid(&nwhich, SCARG(uap, which));
	if (error != 0)
		return error;

	if (SCARG(uap, tp) != NULL) {
		sg = stackgap_init(p, 0);
		tp = stackgap_alloc(p, &sg, sizeof *tp);
	} else
		tp = NULL;

	SCARG(&sga, clock_id) = nwhich;
	SCARG(&sga, tp) = tp;

	error = sys_clock_getres(l, &sga, retval);
	if (error != 0)
		return error;

	if (tp != NULL) {
		error = copyin(tp, &ts, sizeof ts);
		if (error != 0)
			return error;
		native_to_linux_timespec(&lts, &ts);

		return copyout(&lts, SCARG(uap, tp), sizeof lts);
	}

	return 0;
}

int
linux_sys_clock_nanosleep(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct linux_sys_clock_nanosleep_args /* {
		syscallarg(clockid_t) which;
		syscallarg(int) flags;
		syscallarg(struct linux_timespec) *rqtp;
		syscallarg(struct linux_timespec) *rmtp;
	} */ *uap = v;
	caddr_t sg;
	struct proc *p = l->l_proc;
	struct timespec *rqtp, *rmtp;
	struct linux_timespec lrqts, lrmts;
	struct timespec rqts, rmts;
	int error;
	struct sys_nanosleep_args sna;

	if (SCARG(uap, flags) != 0)
		return EINVAL;		/* XXX deal with TIMER_ABSTIME */

	if (SCARG(uap, which) != LINUX_CLOCK_REALTIME)
		return EINVAL;

	error = copyin(SCARG(uap, rqtp), &lrqts, sizeof lrqts);
	if (error != 0)
		return error;

	linux_to_native_timespec(&rqts, &lrqts);

	sg = stackgap_init(p, 0);
	rqtp = stackgap_alloc(p, &sg, sizeof *rqtp);
	error = copyout(&rqts, rqtp, sizeof rqts);
	if (error != 0)
		return error;

	if (SCARG(uap, rmtp) != NULL)
		rmtp = stackgap_alloc(p, &sg, sizeof *rmtp);
	else
		rmtp = NULL;

	SCARG(&sna, rqtp) = rqtp;
	SCARG(&sna, rmtp) = rmtp;

	error = sys_nanosleep(l, &sna, retval);
	if (error != 0)
		return error;

	if (rmtp != NULL) {
		error = copyin(rmtp, &rmts, sizeof rmts);
		if (error != 0)
			return error;
		native_to_linux_timespec(&lrmts, &rmts);
		error = copyout(&lrmts, SCARG(uap, rmtp), sizeof lrmts);
		if (error != 0)
			return error;
	}

	return 0;
}
