/*	$NetBSD: pthread_mutex.c,v 1.1.2.18 2003/01/09 19:27:52 thorpej Exp $	*/

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

#include <assert.h>
#include <errno.h>
#include <sys/cdefs.h>

#include "pthread.h"
#include "pthread_int.h"

static void pthread_mutex_lock_slow(pthread_mutex_t *);

__strong_alias(__libc_mutex_init,pthread_mutex_init)
__strong_alias(__libc_mutex_lock,pthread_mutex_lock)
__strong_alias(__libc_mutex_trylock,pthread_mutex_trylock)
__strong_alias(__libc_mutex_unlock,pthread_mutex_unlock)
__strong_alias(__libc_mutex_destroy,pthread_mutex_destroy)

__strong_alias(__libc_thr_once,pthread_once)

int
pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{

#ifdef ERRORCHECK
	if ((mutex == NULL) || 
	    (attr && (attr->ptma_magic != _PT_MUTEXATTR_MAGIC)))
		return EINVAL;
#endif

	mutex->ptm_magic = _PT_MUTEX_MAGIC;
	mutex->ptm_owner = NULL;
	pthread_lockinit(&mutex->ptm_lock);
	pthread_lockinit(&mutex->ptm_interlock);
	PTQ_INIT(&mutex->ptm_blocked);

	return 0;
}


int
pthread_mutex_destroy(pthread_mutex_t *mutex)
{

#ifdef ERRORCHECK
	if ((mutex == NULL) ||
	    (mutex->ptm_magic != _PT_MUTEX_MAGIC) ||
	    (mutex->ptm_lock != __SIMPLELOCK_UNLOCKED))
		return EINVAL;
#endif

	mutex->ptm_magic = _PT_MUTEX_DEAD;

	return 0;
}


/*
 * Note regarding memory visibility: Pthreads has rules about memory
 * visibility and mutexes. Very roughly: Memory a thread can see when
 * it unlocks a mutex can be seen by another thread that locks the
 * same mutex.
 * 
 * A memory barrier after a lock and before an unlock will provide
 * this behavior. This code relies on pthread__simple_lock_try() to issue
 * a barrier after obtaining a lock, and on pthread__simple_unlock() to
 * issue a barrier before releasing a lock.
 */

int
pthread_mutex_lock(pthread_mutex_t *mutex)
{

#ifdef ERRORCHECK
	if ((mutex == NULL) || (mutex->ptm_magic != _PT_MUTEX_MAGIC))
		return EINVAL;
#endif

	if (__predict_false(pthread__simple_lock_try(&mutex->ptm_lock) == 0))
		pthread_mutex_lock_slow(mutex);

	/* We have the lock! */
#ifdef ERRORCHECK
	mutex->ptm_owner = (pthread_t)pthread__sp();
#endif	
	return 0;
}

static void
pthread_mutex_lock_slow(pthread_mutex_t *mutex)
{
	pthread_t self;

	self = pthread__self();

	while (/*CONSTCOND*/1) {
		if (pthread__simple_lock_try(&mutex->ptm_lock))
		    break; /* got it! */
		
		/* Okay, didn't look free. Get the interlock... */
		pthread_spinlock(self, &mutex->ptm_interlock);
		/*
		 * The mutex_unlock routine will get the interlock
		 * before looking at the list of sleepers, so if the
		 * lock is held we can safely put ourselves on the
		 * sleep queue. If it's not held, we can try taking it
		 * again.
		 */
		if (mutex->ptm_lock == __SIMPLELOCK_LOCKED) {
			PTQ_INSERT_TAIL(&mutex->ptm_blocked, self, pt_sleep);
			/*
			 * Locking a mutex is not a cancellation
			 * point, so we don't need to do the
			 * test-cancellation dance. We may get woken
			 * up spuriously by pthread_cancel, though,
			 * but it's okay since we're just going to
			 * retry.
			 */
			pthread_spinlock(self, &self->pt_statelock);
			self->pt_state = PT_STATE_BLOCKED_QUEUE;
			self->pt_sleepobj = mutex;
			self->pt_sleepq = &mutex->ptm_blocked;
			self->pt_sleeplock = &mutex->ptm_interlock;
			pthread_spinunlock(self, &self->pt_statelock);

			pthread__block(self, &mutex->ptm_interlock);
			/* interlock is not held when we return */
		} else {
			pthread_spinunlock(self, &mutex->ptm_interlock);
		}
		/* Go around for another try. */
	}
}


int
pthread_mutex_trylock(pthread_mutex_t *mutex)
{

#ifdef ERRORCHECK
	if ((mutex == NULL) || (mutex->ptm_magic != _PT_MUTEX_MAGIC))
		return EINVAL;
#endif

	if (pthread__simple_lock_try(&mutex->ptm_lock) == 0)
		return EBUSY;

#ifdef ERRORCHECK
	mutex->ptm_owner = (pthread_t)pthread__sp();
#endif
	return 0;
}


int
pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	pthread_t self, blocked; 

	self = pthread__self();

#ifdef ERRORCHECK
	if ((mutex == NULL) || (mutex->ptm_magic != _PT_MUTEX_MAGIC))
		return EINVAL;

	if (mutex->ptm_lock != __SIMPLELOCK_LOCKED)
		return EPERM; /* Not exactly the right error. */
#endif

	pthread_spinlock(self, &mutex->ptm_interlock);
	blocked = PTQ_FIRST(&mutex->ptm_blocked);
	if (blocked)
		PTQ_REMOVE(&mutex->ptm_blocked, blocked, pt_sleep);
#ifdef ERRORCHECK
	mutex->ptm_owner = NULL;
#endif
	pthread__simple_unlock(&mutex->ptm_lock);
	pthread_spinunlock(self, &mutex->ptm_interlock);

	/* Give the head of the blocked queue another try. */
	if (blocked)
		pthread__sched(self, blocked);

	return 0;
}

int
pthread_mutexattr_init(pthread_mutexattr_t *attr)
{

#ifdef ERRORCHECK
	if (attr == NULL)
		return EINVAL;
#endif

	attr->ptma_magic = _PT_MUTEXATTR_MAGIC;

	return 0;
}


int
pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
{

#ifdef ERRORCHECK
	if ((attr == NULL) ||
	    (attr->ptma_magic != _PT_MUTEXATTR_MAGIC))
		return EINVAL;
#endif

	attr->ptma_magic = _PT_MUTEXATTR_DEAD;

	return 0;
}


int
pthread_once(pthread_once_t *once_control, void (*routine)(void))
{

	if (once_control->pto_done == 0) {
		pthread_mutex_lock(&once_control->pto_mutex);
		if (once_control->pto_done == 0) {
			routine();
			once_control->pto_done = 1;
		}
		pthread_mutex_unlock(&once_control->pto_mutex);
	}

	return 0;
}
