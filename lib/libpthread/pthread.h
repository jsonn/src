/*	$NetBSD: pthread.h,v 1.1.2.21 2003/01/16 03:35:44 thorpej Exp $	*/

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

#ifndef _LIB_PTHREAD_H
#define _LIB_PTHREAD_H

#include <sys/cdefs.h>

#include <time.h>	/* For timespec */
#include <sched.h>

#include "pthread_types.h"

__BEGIN_DECLS
int	pthread_create(pthread_t *thread, const pthread_attr_t *attr, 
	    void *(*startfunc)(void *), void *arg);
void	pthread_exit(void *retval) __attribute__((__noreturn__));
int	pthread_join(pthread_t thread, void **valptr);
int	pthread_equal(pthread_t t1, pthread_t t2);
pthread_t	pthread_self(void);
int	pthread_detach(pthread_t thread);

int	pthread_kill(pthread_t thread, int sig);

int	pthread_getrrtimer_np(void);
int	pthread_setrrtimer_np(int);

int	pthread_attr_destroy(pthread_attr_t *attr);
int	pthread_attr_getdetachstate(pthread_attr_t *attr, int *detachstate);
int	pthread_attr_init(pthread_attr_t *attr);
int	pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate);
int	pthread_attr_setschedparam(pthread_attr_t *attr,
	    const struct sched_param *param);
int	pthread_attr_getschedparam(pthread_attr_t *attr,
	    struct sched_param *param);

int	pthread_mutex_init(pthread_mutex_t *mutex,
	    const pthread_mutexattr_t *attr);
int	pthread_mutex_destroy(pthread_mutex_t *mutex);
int	pthread_mutex_lock(pthread_mutex_t *mutex);
int	pthread_mutex_trylock(pthread_mutex_t *mutex);
int	pthread_mutex_unlock(pthread_mutex_t *mutex);
int	pthread_mutexattr_init(pthread_mutexattr_t *attr);
int	pthread_mutexattr_destroy(pthread_mutexattr_t *attr);
int	pthread_mutexattr_gettype(const pthread_mutexattr_t *attr, int *type);
int	pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type);

int	pthread_cond_init(pthread_cond_t *cond,
	    const pthread_condattr_t *attr);
int	pthread_cond_destroy(pthread_cond_t *cond);
int	pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int	pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
	    const struct timespec *abstime);
int	pthread_cond_signal(pthread_cond_t *cond);
int	pthread_cond_broadcast(pthread_cond_t *cond);
int	pthread_condattr_init(pthread_condattr_t *attr);
int	pthread_condattr_destroy(pthread_condattr_t *attr);

int	pthread_once(pthread_once_t *once_control, void (*routine)(void));

int	pthread_key_create(pthread_key_t *key, void (*destructor)(void *));
int	pthread_key_delete(pthread_key_t key);
int	pthread_setspecific(pthread_key_t key, const void *value);
void*	pthread_getspecific(pthread_key_t key);

int	pthread_cancel(pthread_t thread);
int	pthread_setcancelstate(int state, int *oldstate);
int	pthread_setcanceltype(int type, int *oldtype);
void	pthread_testcancel(void);

struct pthread_cleanup_store {
	void	*pad[4];
};

#define pthread_cleanup_push(routine,arg)			\
        {							\
		struct pthread_cleanup_store __store;	\
		pthread__cleanup_push((routine),(arg), &__store);

#define pthread_cleanup_pop(execute)				\
		pthread__cleanup_pop((execute), &__store);	\
	}

void	pthread__cleanup_push(void (*routine)(void *), void *arg, void *store);
void	pthread__cleanup_pop(int execute, void *store);

int	pthread_spin_init(pthread_spinlock_t *lock, int pshared);
int	pthread_spin_destroy(pthread_spinlock_t *lock);
int	pthread_spin_lock(pthread_spinlock_t *lock);
int	pthread_spin_trylock(pthread_spinlock_t *lock);
int	pthread_spin_unlock(pthread_spinlock_t *lock);

int	pthread_rwlock_init(pthread_rwlock_t *rwlock,
	    const pthread_rwlockattr_t *attr);
int	pthread_rwlock_destroy(pthread_rwlock_t *rwlock);
int	pthread_rwlock_rdlock(pthread_rwlock_t *rwlock);
int	pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock);
int	pthread_rwlock_wrlock(pthread_rwlock_t *rwlock);
int	pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock);
int	pthread_rwlock_timedrdlock(pthread_rwlock_t *rwlock,
	    const struct timespec *abs_timeout);
int	pthread_rwlock_timedwrlock(pthread_rwlock_t *rwlock,
	    const struct timespec *abs_timeout);
int	pthread_rwlock_unlock(pthread_rwlock_t *rwlock);
int	pthread_rwlockattr_init(pthread_rwlockattr_t *attr);
int	pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr);

int	pthread_barrier_init(pthread_barrier_t *barrier,
	    const pthread_barrierattr_t *attr, unsigned int count);
int	pthread_barrier_wait(pthread_barrier_t *barrier);
int	pthread_barrier_destroy(pthread_barrier_t *barrier);
int	pthread_barrierattr_init(pthread_barrierattr_t *attr);
int	pthread_barrierattr_destroy(pthread_barrierattr_t *attr);

int 	*pthread__errno(void);
__END_DECLS

#define	PTHREAD_CREATE_JOINABLE	0
#define	PTHREAD_CREATE_DETACHED	1

#define PTHREAD_PROCESS_PRIVATE	0
#define PTHREAD_PROCESS_SHARED	1

#define PTHREAD_CANCEL_DEFERRED		0
#define PTHREAD_CANCEL_ASYNCHRONOUS	1

#define PTHREAD_CANCEL_ENABLE		0
#define PTHREAD_CANCEL_DISABLE		1

#define PTHREAD_BARRIER_SERIAL_THREAD	1234567

/*
 * POSIX 1003.1-2001, section 2.5.9.3: "The symbolic constant
 * PTHREAD_CANCELED expands to a constant expression of type (void *)
 * whose value matches no pointer to an object in memory nor the value
 * NULL."
 */
#define PTHREAD_CANCELED	((void *) 1)

#define	_POSIX_THREADS

#define PTHREAD_DESTRUCTOR_ITERATIONS	4	/* Min. required */
#define PTHREAD_KEYS_MAX	256
#define PTHREAD_STACK_MIN	4096 /* XXX Pulled out of my butt */
#define PTHREAD_THREADS_MAX	64		/* Min. required */

/*
 * Mutex attributes.
 */
#define	PTHREAD_MUTEX_NORMAL		0
#define	PTHREAD_MUTEX_ERRORCHECK	1
#define	PTHREAD_MUTEX_RECURSIVE		2
#define	PTHREAD_MUTEX_DEFAULT		PTHREAD_MUTEX_NORMAL

#endif /* _LIB_PTHREAD_H */
