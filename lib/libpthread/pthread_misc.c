/*	$NetBSD: pthread_misc.c,v 1.1.2.1 2007/09/03 10:14:15 skrll Exp $	*/

/*-
 * Copyright (c) 2001, 2006, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Nathan J. Williams.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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
__RCSID("$NetBSD: pthread_misc.c,v 1.1.2.1 2007/09/03 10:14:15 skrll Exp $");

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/time.h>

#include <errno.h>
#include <lwp.h>

#include "pthread.h"
#include "pthread_int.h"

int	_sys___sigprocmask14(int, const sigset_t *, sigset_t *);
int	_sys_nanosleep(const struct timespec *, struct timespec *);

__strong_alias(_nanosleep, nanosleep)
__strong_alias(__libc_thr_sigsetmask,pthread_sigmask)
__strong_alias(__sigprocmask14,pthread_sigmask)
__strong_alias(__libc_thr_yield,_sys_sched_yield)

/*ARGSUSED*/
int
pthread_getschedparam(pthread_t thread, int *policy, struct sched_param *param)
{
	if (param == NULL || policy == NULL)
		return EINVAL;
	param->sched_priority = 0;
	*policy = SCHED_RR;
	return 0;
}

/*ARGSUSED*/
int
pthread_setschedparam(pthread_t thread, int policy, 
    const struct sched_param *param)
{
	if (param == NULL || policy < SCHED_FIFO || policy > SCHED_RR)
		return EINVAL;
	if (param->sched_priority > 0 || policy != SCHED_RR)
		return ENOTSUP;
	return 0;
}

int
pthread_kill(pthread_t thread, int sig)
{

	if ((sig < 0) || (sig >= _NSIG))
		return EINVAL;
	if (pthread__find(thread) != 0)
		return ESRCH;

	return _lwp_kill(thread->pt_lid, sig);
}

int
pthread_sigmask(int how, const sigset_t *set, sigset_t *oset)
{

	return _sys___sigprocmask14(how, set, oset);
}

int
nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{

	/*
	 * For now, just nanosleep.  In the future, maybe pass a ucontext_t
	 * to _lwp_nanosleep() and allow it to recycle our kernel stack.
	 */
	return  _sys_nanosleep(rqtp, rmtp);
}
