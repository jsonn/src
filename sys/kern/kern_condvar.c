/*	$NetBSD: kern_condvar.c,v 1.1.2.4 2006/12/29 20:27:43 ad Exp $	*/

/*-
 * Copyright (c) 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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
 * Kernel condition variable implementation, modeled after those found in
 * Solaris, a description of which can be found in:
 *
 *	Solaris Internals: Core Kernel Architecture, Jim Mauro and
 *	    Richard McDougall.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_condvar.c,v 1.1.2.4 2006/12/29 20:27:43 ad Exp $");

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/systm.h>
#include <sys/condvar.h>
#include <sys/sleepq.h>

void	cv_unsleep(struct lwp *);

syncobj_t cv_syncobj = {
	SOBJ_SLEEPQ_SORTED,
	cv_unsleep,
	sleepq_changepri
};

/*
 * cv_init:
 *
 *	Initialize a condition variable for use.
 */
void
cv_init(kcondvar_t *cv, const char *wmesg)
{

	KASSERT(wmesg != NULL);

	cv->cv_wmesg = wmesg;
	cv->cv_waiters = 0;
}

/*
 * cv_destroy:
 *
 *	Tear down a condition variable.
 */
void
cv_destroy(kcondvar_t *cv)
{

#ifdef DIAGNOSTIC
	KASSERT(cv->cv_waiters == 0 && cv->cv_wmesg != NULL);
	cv->cv_wmesg = NULL;
#endif
}

/*
 * cv_enter:
 *
 *	Look up and lock the sleep queue corresponding to the given
 *	condition variable, and increment the number of waiters.
 */
static inline sleepq_t *
cv_enter(kcondvar_t *cv, kmutex_t *mtx, struct lwp *l)
{
	sleepq_t *sq;

	KASSERT(cv->cv_wmesg != NULL);

	sq = sleeptab_lookup(&sleeptab, cv);
	cv->cv_waiters++;
	sleepq_enter(sq, l);
	mutex_exit(mtx);

	return sq;
}

/*
 * cv_unsleep:
 *
 *	Remove an LWP from the condition variable and sleep queue.  This
 *	is called when the LWP has not been awoken normally but instead
 *	interrupted: for example, when a signal is received.  Must be called
 *	with the LWP locked, and must return it unlocked.
 */
void
cv_unsleep(struct lwp *l)
{
	uintptr_t addr;

	KASSERT(l->l_wchan != NULL);
	LOCK_ASSERT(lwp_locked(l, l->l_sleepq->sq_mutex));

	addr = (uintptr_t)l->l_wchan;
	((kcondvar_t *)addr)->cv_waiters--;

	sleepq_unsleep(l);
}

/*
 * cv_wait:
 *
 *	Wait non-interruptably on a condition variable until awoken.
 */
void
cv_wait(kcondvar_t *cv, kmutex_t *mtx)
{
	struct lwp *l = curlwp;
	sleepq_t *sq;

	LOCK_ASSERT(mutex_owned(mtx));

	if (sleepq_dontsleep(l)) {
		(void)sleepq_abort(mtx, 0);
		return;
	}

	sq = cv_enter(cv, mtx, l);
	sleepq_block(sq, sched_kpri(l), cv, cv->cv_wmesg, 0, 0,
	    &cv_syncobj);
	(void)sleepq_unblock(0, 0);
	mutex_enter(mtx);
}

/*
 * cv_wait_sig:
 *
 *	Wait on a condition variable until a awoken or a signal is received. 
 *	Will also return early if the process is exiting.  Returns zero if
 *	awoken normallly, ERESTART if a signal was received and the system
 *	call is restartable, or EINTR otherwise.
 */
int
cv_wait_sig(kcondvar_t *cv, kmutex_t *mtx)
{
	struct lwp *l = curlwp;
	sleepq_t *sq;
	int error;

	LOCK_ASSERT(mutex_owned(mtx));

	if (sleepq_dontsleep(l))
		return sleepq_abort(mtx, 0);

	sq = cv_enter(cv, mtx, l);
	sleepq_block(sq, sched_kpri(l), cv, cv->cv_wmesg, 0, 1,
	    &cv_syncobj);
	error = sleepq_unblock(0, 1);
	mutex_enter(mtx);

	return error;
}

/*
 * cv_timedwait:
 *
 *	Wait on a condition variable until awoken or the specified timeout
 *	expires.  Returns zero if awoken normally or EWOULDBLOCK if the
 *	timeout expired.
 */
int
cv_timedwait(kcondvar_t *cv, kmutex_t *mtx, int timo)
{
	struct lwp *l = curlwp;
	sleepq_t *sq;
	int error;

	LOCK_ASSERT(mutex_owned(mtx));

	if (sleepq_dontsleep(l))
		return sleepq_abort(mtx, 0);

	sq = cv_enter(cv, mtx, l);
	sleepq_block(sq, sched_kpri(l), cv, cv->cv_wmesg, timo, 0,
	    &cv_syncobj);
	error = sleepq_unblock(timo, 0);
	mutex_enter(mtx);

 	return error;
}

/*
 * cv_timedwait_sig:
 *
 *	Wait on a condition variable until a timeout expires, awoken or a
 *	signal is received.  Will also return early if the process is
 *	exiting.  Returns zero if awoken normallly, EWOULDBLOCK if the
 *	timeout expires, ERESTART if a signal was received and the system
 *	call is restartable, or EINTR otherwise.
 */
int
cv_timedwait_sig(kcondvar_t *cv, kmutex_t *mtx, int timo)
{
	struct lwp *l = curlwp;
	sleepq_t *sq;
	int error;

	LOCK_ASSERT(mutex_owned(mtx));

	if (sleepq_dontsleep(l))
		return sleepq_abort(mtx, 0);

	sq = cv_enter(cv, mtx, l);
	sleepq_block(sq, sched_kpri(l), cv, cv->cv_wmesg, timo, 1,
	    &cv_syncobj);
	error = sleepq_unblock(timo, 1);
	mutex_enter(mtx);

 	return error;
}

/*
 * cv_signal:
 *
 *	Wake the highest priority LWP waiting on a condition variable.
 */
void
cv_signal(kcondvar_t *cv)
{
	sleepq_t *sq;

	sq = sleeptab_lookup(&sleeptab, cv);
	if (cv->cv_waiters != 0) {
		cv->cv_waiters--;
		sleepq_wake(sq, cv, 1);
	} else
		sleepq_unlock(sq);
}

/*
 * cv_broadcast:
 *
 *	Wake all LWPs waiting on a condition variable.
 */
void
cv_broadcast(kcondvar_t *cv)
{
	sleepq_t *sq;
	u_int cnt;

	sq = sleeptab_lookup(&sleeptab, cv);
	if ((cnt = cv->cv_waiters) != 0) {
		cv->cv_waiters = 0;
		sleepq_wake(sq, cv, cnt);
	} else
		sleepq_unlock(sq);
}

/*
 * cv_has_waiters:
 *
 *	For diagnostic assertions: return non-zero if a condition
 *	variable has waiters.
 */
int
cv_has_waiters(kcondvar_t *cv)
{

	/* No need to interlock here */
	return (int)cv->cv_waiters;
}
