/*	$NetBSD: pthread_run.c,v 1.18.12.4 2007/11/04 04:26:58 wrstuden Exp $	*/

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
__RCSID("$NetBSD: pthread_run.c,v 1.18.12.4 2007/11/04 04:26:58 wrstuden Exp $");

#include <ucontext.h>
#include <errno.h>

#include "pthread.h"
#include "pthread_int.h"

#ifdef PTHREAD_RUN_DEBUG
#define SDPRINTF(x) DPRINTF(x)
#else
#define SDPRINTF(x)
#endif

extern pthread_spin_t pthread__runqueue_lock;
extern struct pthread_queue_t pthread__runqueue;
extern struct pthread_queue_t pthread__idlequeue;
extern struct pthread_queue_t pthread__suspqueue;

extern pthread_spin_t pthread__deadqueue_lock;
extern struct pthread_queue_t *pthread__reidlequeue;

extern int pthread__concurrency, pthread__maxconcurrency;

__strong_alias(__libc_thr_yield,sched_yield)

int
sched_yield(void)
{
	pthread_t self, next;
	extern int pthread__started;

	if (pthread__started) {
		self = pthread__self();
		SDPRINTF(("(sched_yield %p) yielding\n", self));
		pthread_spinlock(self, &pthread__runqueue_lock);
		pthread_spinlock(self, &self->pt_statelock);
		while (pthread_check_defsig(self)) {
			pthread_spinunlock(self, &self->pt_statelock);
			pthread_spinunlock(self, &pthread__runqueue_lock);
			pthread__signal_deferred(self, self);
			pthread_spinlock(self, &pthread__runqueue_lock);
			pthread_spinlock(self, &self->pt_statelock);
		}
		self->pt_state = PT_STATE_RUNNABLE;
		pthread_spinunlock(self, &self->pt_statelock);
		PTQ_INSERT_TAIL(&pthread__runqueue, self, pt_runq);
		/*
		 * There will always be at least one thread on the runqueue,
		 * because we just put ourselves there.
		 */
	        next = PTQ_FIRST(&pthread__runqueue);
		PTQ_REMOVE(&pthread__runqueue, next, pt_runq);
		pthread_spinlock(self, &next->pt_statelock);
		next->pt_state = PT_STATE_RUNNING;
		if (next != self) {
			next->pt_vpid = self->pt_vpid;
			next->pt_lastlwp = self->pt_lastlwp;
			pthread_spinunlock(self, &next->pt_statelock);
			pthread__locked_switch(self, next,
			    &pthread__runqueue_lock);
		} else {
			pthread_spinunlock(self, &pthread__runqueue_lock);
			pthread_spinunlock(self, &next->pt_statelock);
		}
	}

	return 0;
}


/* Go do another thread, without putting us on the run queue. */
void
pthread__block(pthread_t self, pthread_spin_t *queuelock)
{
	pthread_t next;

	next = pthread__next(self);
	SDPRINTF(("(calling locked_switch %p, %p) spinlock %d, %d\n",
 		self, next, self->pt_spinlocks, next->pt_spinlocks));
	pthread__locked_switch(self, next, queuelock);
	SDPRINTF(("(back from locked_switch %p, %p) spinlock %d, %d\n",
 		self, next, self->pt_spinlocks, next->pt_spinlocks));
}


/*
 * Get the next thread to switch to. Will never return NULL.
 * Will lock pthread__runqueue_lock. Expects we are about
 * to switch to the returned thread.
 */
pthread_t
pthread__next(pthread_t self)
{
	pthread_t next;

	pthread_spinlock(self, &pthread__runqueue_lock);
	next = PTQ_FIRST(&pthread__runqueue);
	if (next) {
		pthread__assert(next->pt_type == PT_THREAD_NORMAL);
		PTQ_REMOVE(&pthread__runqueue, next, pt_runq);
		SDPRINTF(("(next %p) returning thread %p\n", self, next));
		if (pthread__maxconcurrency > pthread__concurrency &&
		    PTQ_FIRST(&pthread__runqueue))
			pthread__setconcurrency(1);
	} else {
		next = PTQ_FIRST(&pthread__idlequeue);
		pthread__assert(next != 0);
		PTQ_REMOVE(&pthread__idlequeue, next, pt_runq);
		pthread__assert(next->pt_type == PT_THREAD_IDLE);
		SDPRINTF(("(next %p) returning idle thread %p\n", self, next));
	}
	/* Now set this thread up to run on our VP */
	pthread_spinlock(self, &next->pt_statelock);
	next->pt_state = PT_STATE_RUNNING;
	next->pt_vpid = self->pt_vpid;
	next->pt_lastlwp = self->pt_lastlwp;
	pthread_spinunlock(self, &next->pt_statelock);

	pthread_spinunlock(self, &pthread__runqueue_lock);

	return next;
}


/*
 * Put a thread on the suspended queue
 *
 * Called and returns with both thread->pt_statelock and
 * pthread__runqueue_lock locked.
 */
void
pthread__suspend(pthread_t self, pthread_t thread)
{

	SDPRINTF(("(sched %p) suspending %p\n", self, thread));
	thread->pt_state = PT_STATE_SUSPENDED;
	pthread__assert(thread->pt_type == PT_THREAD_NORMAL);
	pthread__assert(thread->pt_spinlocks == 0);
	PTQ_INSERT_TAIL(&pthread__suspqueue, thread, pt_runq);
	/* XXX flaglock? */
	thread->pt_flags &= ~PT_FLAG_SUSPENDED;
}

/*
 * Put a thread back on the run queue. Will lock and unlock the
 * run queue unless rq_locked is non-zero.
 */
void
pthread__sched(pthread_t self, pthread_t thread, int rq_locked)
{

	SDPRINTF(("(sched %p) scheduling %p\n", self, thread));
	thread->pt_state = PT_STATE_RUNNABLE;
	pthread__assert(thread->pt_type == PT_THREAD_NORMAL);
	pthread__assert(thread->pt_spinlocks == 0);
#ifdef PTHREAD__DEBUG
	thread->rescheds++;
#endif
	pthread__assert(PTQ_LAST(&pthread__runqueue, pthread_queue_t) != thread);
	pthread__assert(PTQ_FIRST(&pthread__runqueue) != thread);
	if (rq_locked == 0)
		pthread_spinlock(self, &pthread__runqueue_lock);
	PTQ_INSERT_TAIL(&pthread__runqueue, thread, pt_runq);
	if (rq_locked == 0)
		pthread_spinunlock(self, &pthread__runqueue_lock);
}

/* Put a bunch of sleeping threads on the run queue */
void
pthread__sched_sleepers(pthread_t self, struct pthread_queue_t *threadq)
{
	pthread_t thread;

	pthread_spinlock(self, &pthread__runqueue_lock);
	PTQ_FOREACH(thread, threadq, pt_sleep) {
		SDPRINTF(("(sched_sleepers %p) scheduling %p\n", self, thread));
		pthread_spinlock(self, &thread->pt_statelock);
		thread->pt_state = PT_STATE_RUNNABLE;
		pthread_spinunlock(self, &thread->pt_statelock);
		pthread__assert(thread->pt_type == PT_THREAD_NORMAL);
		pthread__assert(thread->pt_spinlocks == 0);
#ifdef PTHREAD__DEBUG
		thread->rescheds++;
#endif
		PTQ_INSERT_TAIL(&pthread__runqueue, thread, pt_runq);
	}
	pthread_spinunlock(self, &pthread__runqueue_lock);
}


/*
 * Make a thread a candidate idle thread. Locks??
 */
void
pthread__sched_idle(pthread_t self, pthread_t thread)
{

	SDPRINTF(("(sched_idle %p) idling %p\n", self, thread));
	thread->pt_state = PT_STATE_RUNNABLE;

	thread->pt_flags &= ~PT_FLAG_IDLED;
	thread->pt_trapuc = NULL;
	_INITCONTEXT_U(thread->pt_uc);
	thread->pt_uc->uc_stack = thread->pt_stack;
	thread->pt_uc->uc_link = NULL;
	makecontext(thread->pt_uc, pthread__idle, 0);

	pthread_spinlock(self, &pthread__runqueue_lock);
	PTQ_INSERT_TAIL(&pthread__idlequeue, thread, pt_runq);
	pthread_spinunlock(self, &pthread__runqueue_lock);
}


void
pthread__sched_idle2(pthread_t self)
{
	pthread_t idlethread, qhead, next;

	qhead = NULL;
	pthread_spinlock(self, &pthread__deadqueue_lock);
	idlethread = PTQ_FIRST(&pthread__reidlequeue[self->pt_vpid]);
	while (idlethread != NULL) {
		SDPRINTF(("(sched_idle2 %p) reidling %p\n", self, idlethread));
		next = PTQ_NEXT(idlethread, pt_runq);
		if ((idlethread->pt_next == NULL) &&
		    (idlethread->pt_blockgen == idlethread->pt_unblockgen)) {
			PTQ_REMOVE(&pthread__reidlequeue[self->pt_vpid],
			    idlethread, pt_runq);
			idlethread->pt_next = qhead;
			qhead = idlethread;

			pthread__concurrency++;
			SDPRINTF(("(sched_idle2 %p concurrency) now %d from %p\n",
				     self, pthread__concurrency, idlethread));
			pthread__assert(pthread__concurrency <=
			    pthread__maxconcurrency);

		} else {
		SDPRINTF(("(sched_idle2 %p) %p is in a preemption loop\n", self, idlethread));
		}
		idlethread = next;
	}
	pthread_spinunlock(self, &pthread__deadqueue_lock);

	if (qhead)
		pthread__sched_bulk(self, qhead);
}

/*
 * Put a series of threads, linked by their pt_next fields, onto the
 * run queue.
 */
void
pthread__sched_bulk(pthread_t self, pthread_t qhead)
{
	pthread_t next;

	pthread_spinlock(self, &pthread__runqueue_lock);
	for ( ; qhead && (qhead != self) ; qhead = next) {
		next = qhead->pt_next;
		pthread__assert(qhead->pt_spinlocks == 0);
		pthread__assert(qhead->pt_type != PT_THREAD_UPCALL);
		if (qhead->pt_unblockgen != qhead->pt_blockgen)
			qhead->pt_unblockgen++;
		if (qhead->pt_type == PT_THREAD_NORMAL) {
			qhead->pt_state = PT_STATE_RUNNABLE;
			qhead->pt_next = NULL;
			qhead->pt_parent = NULL;
#ifdef PTHREAD__DEBUG
			qhead->rescheds++;
#endif
			SDPRINTF(("(bulk %p) scheduling %p\n", self, qhead));
			pthread__assert(PTQ_LAST(&pthread__runqueue, pthread_queue_t) != qhead);
			pthread__assert(PTQ_FIRST(&pthread__runqueue) != qhead);
			if (qhead->pt_flags & PT_FLAG_SUSPENDED) {
				qhead->pt_state = PT_STATE_SUSPENDED;
				PTQ_INSERT_TAIL(&pthread__suspqueue, qhead, pt_runq);
			} else
				PTQ_INSERT_TAIL(&pthread__runqueue, qhead, pt_runq);
		} else if (qhead->pt_type == PT_THREAD_IDLE) {
			qhead->pt_state = PT_STATE_RUNNABLE;
			qhead->pt_flags &= ~PT_FLAG_IDLED;
			qhead->pt_next = NULL;
			qhead->pt_parent = NULL;
			qhead->pt_trapuc = NULL;
			_INITCONTEXT_U(qhead->pt_uc);
			qhead->pt_uc->uc_stack = qhead->pt_stack;
			qhead->pt_uc->uc_link = NULL;
			makecontext(qhead->pt_uc, pthread__idle, 0);
			SDPRINTF(("(bulk %p) idling %p\n", self, qhead));
			PTQ_INSERT_TAIL(&pthread__idlequeue, qhead, pt_runq);
		}
	}
	pthread_spinunlock(self, &pthread__runqueue_lock);
}


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
