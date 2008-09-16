/*	$NetBSD: pthread_cond.c,v 1.18.10.1 2008/09/16 18:49:32 bouyer Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
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
__RCSID("$NetBSD: pthread_cond.c,v 1.18.10.1 2008/09/16 18:49:32 bouyer Exp $");

#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>

#include "pthread.h"
#include "pthread_int.h"

#ifdef PTHREAD_COND_DEBUG
#define SDPRINTF(x) DPRINTF(x)
#else
#define SDPRINTF(x)
#endif

int	_sys_nanosleep(const struct timespec *, struct timespec *);

extern int pthread__started;

static void pthread_cond_wait__callback(void *);
static int pthread_cond_wait_nothread(pthread_t, pthread_mutex_t *,
    const struct timespec *);

__strong_alias(__libc_cond_init,pthread_cond_init)
__strong_alias(__libc_cond_signal,pthread_cond_signal)
__strong_alias(__libc_cond_broadcast,pthread_cond_broadcast)
__strong_alias(__libc_cond_wait,pthread_cond_wait)
__strong_alias(__libc_cond_timedwait,pthread_cond_timedwait)
__strong_alias(__libc_cond_destroy,pthread_cond_destroy)

int
pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{

	pthread__error(EINVAL, "Invalid condition variable attribute",
	    (attr == NULL) || (attr->ptca_magic == _PT_CONDATTR_MAGIC));

	cond->ptc_magic = _PT_COND_MAGIC;
	pthread_lockinit(&cond->ptc_lock);
	PTQ_INIT(&cond->ptc_waiters);
	cond->ptc_mutex = NULL;

	return 0;
}


int
pthread_cond_destroy(pthread_cond_t *cond)
{

	pthread__error(EINVAL, "Invalid condition variable",
	    cond->ptc_magic == _PT_COND_MAGIC);
	pthread__error(EBUSY, "Destroying condition variable in use",
	    cond->ptc_mutex == NULL);

	cond->ptc_magic = _PT_COND_DEAD;

	return 0;
}


int
pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
	pthread_t self;

	pthread__error(EINVAL, "Invalid condition variable",
	    cond->ptc_magic == _PT_COND_MAGIC);
	pthread__error(EINVAL, "Invalid mutex",
	    mutex->ptm_magic == _PT_MUTEX_MAGIC);
	pthread__error(EPERM, "Mutex not locked in condition wait",
	    mutex->ptm_lock == __SIMPLELOCK_LOCKED);

	self = pthread__self();
	PTHREADD_ADD(PTHREADD_COND_WAIT);

	/* Just hang out for a while if threads aren't running yet. */
	if (__predict_false(pthread__started == 0))
		return pthread_cond_wait_nothread(self, mutex, NULL);

	pthread_spinlock(self, &cond->ptc_lock);
	SDPRINTF(("(cond wait %p) Waiting on %p, mutex %p\n",
	    self, cond, mutex));
	pthread_spinlock(self, &self->pt_statelock);
	if (__predict_false(self->pt_cancel)) {
		pthread_spinunlock(self, &self->pt_statelock);
		pthread_spinunlock(self, &cond->ptc_lock);
		pthread_exit(PTHREAD_CANCELED);
	}
#ifdef ERRORCHECK
	if (cond->ptc_mutex == NULL)
		cond->ptc_mutex = mutex;
	else
		pthread__error(EINVAL,
		    "Multiple mutexes used for condition wait", 
		    cond->ptc_mutex == mutex);
#endif
	if (pthread_check_defsig(self)) {
		pthread_spinunlock(self, &self->pt_statelock);
		pthread_spinunlock(self, &cond->ptc_lock);
		SDPRINTF(("(cond wait %p) Caught defsig on %p, mutex %p\n",
		    self, cond, mutex));
		/* mutex still locked. Isn't supposed to matter for
		 * signal hanlders. */
		pthread__signal_deferred(self, self);
		/*
		 * We can return from spurious wakeups. So to make life simple,
		 * do so. Coveres anything that may have happened while we
		 * were delivering signals
		 */
		return 0;
	}
	self->pt_state = PT_STATE_BLOCKED_QUEUE;
	self->pt_sleepobj = cond;
	self->pt_sleepq = &cond->ptc_waiters;
	self->pt_sleeplock = &cond->ptc_lock;
	pthread_spinunlock(self, &self->pt_statelock);
	PTQ_INSERT_HEAD(&cond->ptc_waiters, self, pt_sleep);
	pthread_mutex_unlock(mutex);

	pthread__block(self, &cond->ptc_lock);
	/* Spinlock is unlocked on return */
	pthread_mutex_lock(mutex);
#ifdef ERRORCHECK
	pthread_spinlock(self, &cond->ptc_lock);
	if (PTQ_EMPTY(&cond->ptc_waiters))
		cond->ptc_mutex = NULL;
	pthread_spinunlock(self, &cond->ptc_lock);
#endif		
	if (__predict_false(self->pt_cancel))
		pthread_exit(PTHREAD_CANCELED);

	SDPRINTF(("(cond wait %p) Woke up on %p, mutex %p\n",
	    self, cond, mutex));

	return 0;
}


struct pthread_cond__waitarg {
	pthread_t ptw_thread;
	pthread_cond_t *ptw_cond;
};

int
pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
    const struct timespec *abstime)
{
	pthread_t self;
	struct pthread_cond__waitarg wait;
	struct pt_alarm_t alarm;
	int retval;

	pthread__error(EINVAL, "Invalid condition variable",
	    cond->ptc_magic == _PT_COND_MAGIC);
	pthread__error(EINVAL, "Invalid mutex",
	    mutex->ptm_magic == _PT_MUTEX_MAGIC);
	pthread__error(EPERM, "Mutex not locked in condition wait",
	    mutex->ptm_lock == __SIMPLELOCK_LOCKED);
	pthread__error(EINVAL, "Invalid wait time", 
	    (abstime->tv_sec >= 0) &&
	    (abstime->tv_nsec >= 0) && (abstime->tv_nsec < 1000000000));

	self = pthread__self();
	PTHREADD_ADD(PTHREADD_COND_TIMEDWAIT);

	/* Just hang out for a while if threads aren't running yet. */
	if (__predict_false(pthread__started == 0))
		return pthread_cond_wait_nothread(self, mutex, abstime);

	pthread_spinlock(self, &cond->ptc_lock);
	wait.ptw_thread = self;
	wait.ptw_cond = cond;
	retval = 0;
	SDPRINTF(("(cond timed wait %p) Waiting on %p until %d.%06ld\n",
	    self, cond, abstime->tv_sec, abstime->tv_nsec/1000));

	pthread_spinlock(self, &self->pt_statelock);
	if (__predict_false(self->pt_cancel)) {
		pthread_spinunlock(self, &self->pt_statelock);
		pthread_spinunlock(self, &cond->ptc_lock);
		pthread_exit(PTHREAD_CANCELED);
	}
#ifdef ERRORCHECK
	if (cond->ptc_mutex == NULL)
		cond->ptc_mutex = mutex;
	else
		pthread__error(EINVAL,
		    "Multiple mutexes used for condition wait",
		    cond->ptc_mutex == mutex);
#endif
	if (pthread_check_defsig(self)) {
		pthread_spinunlock(self, &self->pt_statelock);
		pthread_spinunlock(self, &cond->ptc_lock);
		/* mutex still locked. Isn't supposed to matter for
		 * signal hanlders. */
		SDPRINTF(("(cond timed wait %p) Caught defsig on %p, mutex %p"
		    "\n", self, cond));
		pthread__signal_deferred(self, self);
		/*
		 * We can return from spurious wakeups. So to make life simple,
		 * do so. Coveres anything that may have happened while we
		 * were delivering signals
		 */
		return 0;
	}
	
	pthread__alarm_add(self, &alarm, abstime, pthread_cond_wait__callback,
	    &wait);
	self->pt_state = PT_STATE_BLOCKED_QUEUE;
	self->pt_sleepobj = cond;
	self->pt_sleepq = &cond->ptc_waiters;
	self->pt_sleeplock = &cond->ptc_lock;
	pthread_spinunlock(self, &self->pt_statelock);

	PTQ_INSERT_HEAD(&cond->ptc_waiters, self, pt_sleep);
	pthread_mutex_unlock(mutex);

	pthread__block(self, &cond->ptc_lock);
	/* Spinlock is unlocked on return */
	SDPRINTF(("(cond timed wait %p) Woke up on %p, mutex %p\n",
	    self, cond));
	pthread__alarm_del(self, &alarm);
	if (pthread__alarm_fired(&alarm))
		retval = ETIMEDOUT;
	SDPRINTF(("(cond timed wait %p) %s\n",
	    self, (retval == ETIMEDOUT) ? "(timed out)" : ""));
	pthread_mutex_lock(mutex);
#ifdef ERRORCHECK
	pthread_spinlock(self, &cond->ptc_lock);
	if (PTQ_EMPTY(&cond->ptc_waiters))
		cond->ptc_mutex = NULL;
	pthread_spinunlock(self, &cond->ptc_lock);
#endif		
	if (__predict_false(self->pt_cancel))
		pthread_exit(PTHREAD_CANCELED);

	return retval;
}

static void
pthread_cond_wait__callback(void *arg)
{
	struct pthread_cond__waitarg *a;
	pthread_t self;

	a = arg;
	self = pthread__self();

	/*
	 * Don't dequeue and schedule the thread if it's already been
	 * queued up by a signal or broadcast (but hasn't yet run as far
	 * as pthread__alarm_del(), or we wouldn't be here, and hence can't
	 * have become blocked on some *other* queue).
	 */
	pthread_spinlock(self, &a->ptw_cond->ptc_lock);
	if (a->ptw_thread->pt_state == PT_STATE_BLOCKED_QUEUE) {
		PTQ_REMOVE(&a->ptw_cond->ptc_waiters, a->ptw_thread, pt_sleep);
#ifdef ERRORCHECK
		if (PTQ_EMPTY(&a->ptw_cond->ptc_waiters))
			a->ptw_cond->ptc_mutex = NULL;
#endif
		pthread__sched(self, a->ptw_thread, 0);
	}
	pthread_spinunlock(self, &a->ptw_cond->ptc_lock);
}

int
pthread_cond_signal(pthread_cond_t *cond)
{
	pthread_t self, signaled;

	pthread__error(EINVAL, "Invalid condition variable",
	    cond->ptc_magic == _PT_COND_MAGIC);
	PTHREADD_ADD(PTHREADD_COND_SIGNAL);

	SDPRINTF(("(cond signal %p) Signaling %p\n",
	    pthread__self(), cond));

	if (!PTQ_EMPTY(&cond->ptc_waiters)) {
		self = pthread__self();
		pthread_spinlock(self, &cond->ptc_lock);
		signaled = PTQ_FIRST(&cond->ptc_waiters);
		if (signaled != NULL) {
			PTQ_REMOVE(&cond->ptc_waiters, signaled, pt_sleep);
			pthread__sched(self, signaled, 0);
			PTHREADD_ADD(PTHREADD_COND_WOKEUP);
		}
#ifdef ERRORCHECK
		if (PTQ_EMPTY(&cond->ptc_waiters))
			cond->ptc_mutex = NULL;
#endif
		pthread_spinunlock(self, &cond->ptc_lock);
	}

	return 0;
}


int
pthread_cond_broadcast(pthread_cond_t *cond)
{
	pthread_t self;
	struct pthread_queue_t blockedq;

	pthread__error(EINVAL, "Invalid condition variable", cond->ptc_magic == _PT_COND_MAGIC);

	PTHREADD_ADD(PTHREADD_COND_BROADCAST);
	SDPRINTF(("(cond signal %p) Broadcasting %p\n",
	    pthread__self(), cond));

	if (!PTQ_EMPTY(&cond->ptc_waiters)) {
		self = pthread__self();
		pthread_spinlock(self, &cond->ptc_lock);
		blockedq = cond->ptc_waiters;
		PTQ_INIT(&cond->ptc_waiters);
#ifdef ERRORCHECK
		cond->ptc_mutex = NULL;
#endif
		pthread__sched_sleepers(self, &blockedq);
		PTHREADD_ADD(PTHREADD_COND_WOKEUP);
		pthread_spinunlock(self, &cond->ptc_lock);
	}

	return 0;

}


int
pthread_condattr_init(pthread_condattr_t *attr)
{

	attr->ptca_magic = _PT_CONDATTR_MAGIC;

	return 0;
}


int
pthread_condattr_destroy(pthread_condattr_t *attr)
{

	pthread__error(EINVAL, "Invalid condition variable attribute",
	    attr->ptca_magic == _PT_CONDATTR_MAGIC);

	attr->ptca_magic = _PT_CONDATTR_DEAD;

	return 0;
}

/* Utility routine to hang out for a while if threads haven't started yet. */
static int
pthread_cond_wait_nothread(pthread_t self, pthread_mutex_t *mutex,
    const struct timespec *abstime)
{
	struct timespec now, diff;
	int retval;

	if (abstime == NULL) {
		diff.tv_sec = 99999999;
		diff.tv_nsec = 0;
	} else {
		clock_gettime(CLOCK_REALTIME, &now);
		if  (timespeccmp(abstime, &now, <))
			timespecclear(&diff);
		else
			timespecsub(abstime, &now, &diff);
	}

	do {
		pthread__testcancel(self);
		pthread_mutex_unlock(mutex);
		retval = _sys_nanosleep(&diff, NULL);
		pthread_mutex_lock(mutex);
	} while (abstime == NULL && retval == 0);
	pthread__testcancel(self);

	if (retval == 0)
		return ETIMEDOUT;
	else
		/* spurious wakeup */
		return 0;
}
