/*	$NetBSD: pthread_sa.c,v 1.1.2.32 2002/11/01 17:06:33 thorpej Exp $	*/

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
#include <err.h>
#include <errno.h>
#include <lwp.h>
#include <sa.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>
#include <sys/time.h>

#include "pthread.h"
#include "pthread_int.h"

#define PTHREAD_SA_DEBUG

#ifdef PTHREAD_SA_DEBUG
#define SDPRINTF(x) DPRINTF(x)
#else
#define SDPRINTF(x)
#endif

extern struct pthread_queue_t allqueue;

stack_t recyclable[2][(PT_UPCALLSTACKS/2)+1];
int	recycle_count;
int	recycle_threshold;
int	recycle_side;
pthread_spin_t recycle_lock;

#define	PTHREAD_RRTIMER_INTERVAL_DEFAULT	100
static pthread_mutex_t rrtimer_mutex = PTHREAD_MUTEX_INITIALIZER;
static timer_t pthread_rrtimer;
static int pthread_rrtimer_interval = PTHREAD_RRTIMER_INTERVAL_DEFAULT;

int pthread__maxlwps;

extern struct pthread_queue_t runqueue;

#define pthread__sa_id(sap) (pthread__id((sap)->sa_context))

void pthread__upcall(int type, struct sa_t *sas[], int ev, int intr, 
    void *arg);
int pthread__find_interrupted(struct sa_t *sas[], int nsas, pthread_t *qhead,
    pthread_t self);
void pthread__resolve_locks(pthread_t self, pthread_t *interrupted);
void pthread__recycle_bulk(pthread_t self, pthread_t qhead);

void
pthread__upcall(int type, struct sa_t *sas[], int ev, int intr, void *arg)
{
	pthread_t t, self, next, intqueue;
	int first = 1;
	int deliversig = 0, runalarms = 0;
	siginfo_t *si;

	PTHREADD_ADD(PTHREADD_UPCALLS);

	self = pthread__self();
	self->pt_state = PT_STATE_RUNNING;

	if (sas[0]->sa_id > pthread__maxlwps)
		pthread__maxlwps = sas[0]->sa_id;
	SDPRINTF(("(up %p) type %d LWP %d ev %d intr %d\n", self, 
	    type, sas[0]->sa_id, ev ? sas[1]->sa_id : 0, 
	    intr ? sas[ev+intr]->sa_id : 0));
	switch (type) {
	case SA_UPCALL_BLOCKED:
		t = pthread__sa_id(sas[1]);
		pthread_spinlock(self, &t->pt_statelock);
		t->pt_state = PT_STATE_BLOCKED_SYS;
		pthread_spinunlock(self, &t->pt_statelock);
#ifdef PTHREAD__DEBUG
		t->blocks++;
#endif
		t->pt_blockedlwp = sas[1]->sa_id;
		if (t->pt_cancel)
			_lwp_wakeup(t->pt_blockedlwp);
		t->pt_uc = sas[1]->sa_context;
		first++; /* Don't handle this SA in the usual processing. */
		PTHREADD_ADD(PTHREADD_UP_BLOCK);
		break;
	case SA_UPCALL_NEWPROC:
		PTHREADD_ADD(PTHREADD_UP_NEW);
		break;
	case SA_UPCALL_PREEMPTED:
		PTHREADD_ADD(PTHREADD_UP_PREEMPT);
		break;
	case SA_UPCALL_UNBLOCKED:
		PTHREADD_ADD(PTHREADD_UP_UNBLOCK);
		break;
	case SA_UPCALL_SIGNAL:
		PTHREADD_ADD(PTHREADD_UP_SIGNAL);
		deliversig = 1;
		break;
	case SA_UPCALL_SIGEV:
		PTHREADD_ADD(PTHREADD_UP_SIGEV);
		si = arg;
		if (si->si_value.sival_int == PT_ALARMTIMER_MAGIC)
			runalarms = 1;
		/*
		 * PT_RRTIMER_MAGIC doesn't need explicit handling;
		 * the per-thread work below will put the interrupted
		 * thread on the back of the run queue, and
		 * pthread_next() will get one from the front.
		 */
		break;
	case SA_UPCALL_USER:
		/* We don't send ourselves one of these. */
	default:
		assert(0);
	}

	/*
	 * Do per-thread work, including saving the context.
	 * Briefly run any threads that were in a critical section.
	 * This includes any upcalls that have been interupted, so
	 * they can do their own version of this dance.
	 */
	intqueue = NULL;
	if ((ev + intr) >= first) {
		if (pthread__find_interrupted(sas + first, ev + intr,
		    &intqueue, self) > 0)
			pthread__resolve_locks(self, &intqueue);
	}
	pthread__sched_idle2(self);
	if (intqueue)
		pthread__sched_bulk(self, intqueue);


	/*
	 * Run the alarm queue (handled after lock resolution since
	 * alarm handling requires locks).
	 */
	if (runalarms)
		pthread__alarm_process(self, arg);

	/*
	 * Note that we handle signals after handling
	 * spinlock preemption. This is because spinlocks are only
	 * used internally to the thread library and we don't want to
	 * expose the middle of them to a signal.  While this means
	 * that synchronous instruction traps that occur inside
	 * critical sections in this library (SIGFPE, SIGILL, SIGBUS,
	 * SIGSEGV) won't be handled at the precise location where
	 * they occured, that's okay, because (1) we don't use any FP
	 * and (2) SIGILL/SIGBUS/SIGSEGV should really just core dump.
	 *
	 * This also means that a thread that was interrupted to take
	 * a signal will be on a run queue, and not in upcall limbo.
	 */
	if (deliversig) {
		si = arg;
		if (ev)
			pthread__signal(pthread__sa_id(sas[1]), si->si_signo,
			    si->si_code);
		else
			pthread__signal(NULL, si->si_signo, si->si_code);
	}
	
	/*
	 * At this point everything on our list should be scheduled
	 * (or was an upcall).
	 */
	assert(self->pt_spinlocks == 0);
	next = pthread__next(self);
	next->pt_state = PT_STATE_RUNNING;
	SDPRINTF(("(up %p) switching to %p (uc: %p pc: %lx)\n", 
	    self, next, next->pt_uc, pthread__uc_pc(next->pt_uc)));
	pthread__upcall_switch(self, next);
	/* NOTREACHED */
	assert(0);
}

/*
 * Build a chain of the threads that were interrupted by the upcall. 
 * Determine if any of them were upcalls or lock-holders that
 * need to be continued early.
 */
int
pthread__find_interrupted(struct sa_t *sas[], int nsas, pthread_t *qhead,
    pthread_t self)
{
	int i, resume;
	pthread_t victim, next;

	resume = 0;
	next = self;

	for (i = 0; i < nsas; i++) {
		victim = pthread__sa_id(sas[i]);
#ifdef PTHREAD__DEBUG
		victim->preempts++;
#endif
		victim->pt_uc = sas[i]->sa_context;
		SDPRINTF(("(fi %p) victim %d %p(%d)", self, i, victim,
		    victim->pt_type));
		if (victim->pt_type == PT_THREAD_UPCALL) {
			/* Case 1: Upcall. Must be resumed. */
				SDPRINTF((" upcall"));
			resume = 1;
			if (victim->pt_next) {
				/*
				 * Case 1A: Upcall in a chain.
				 *
				 * Already part of a chain. We want to
				 * splice this chain into our chain, so
				 * we have to find the root.
				 */
				SDPRINTF((" chain"));
				for ( ; victim->pt_parent != NULL; 
				      victim = victim->pt_parent) {
					SDPRINTF((" parent %p", victim->pt_parent));
					assert(victim->pt_parent != victim);
				}
			}
		} else {
			/* Case 2: Normal or idle thread. */
			if (victim->pt_spinlocks > 0) {
				/* Case 2A: Lockholder. Must be resumed. */
				SDPRINTF((" lockholder %d",
				    victim->pt_spinlocks));
				resume = 1;
				if (victim->pt_next) {
					/*
					 * Case 2A1: Lockholder on a chain.
					 * Same deal as 1A.
					 */
					SDPRINTF((" chain"));
					for ( ; victim->pt_parent != NULL; 
					      victim = victim->pt_parent) {
						SDPRINTF((" parent %p", victim->pt_parent));
						assert(victim->pt_parent != victim);
					}


				}
			} else {
				/* Case 2B: Non-lockholder. */
					SDPRINTF((" nonlockholder"));
				if (victim->pt_next) {
					/*
					 * Case 2B1: Non-lockholder on a chain
					 * (must have just released a lock).
					 */
					SDPRINTF((" chain"));
					resume = 1;
					for ( ; victim->pt_parent != NULL; 
					      victim = victim->pt_parent) {
						SDPRINTF((" parent %p", victim->pt_parent));
						assert(victim->pt_parent != victim);
					}
				} else if (victim->pt_flags & PT_FLAG_IDLED) {
					/*
					 * Idle threads that have already 
					 * idled must be skipped so 
					 * that we don't (a) idle-queue them
					 * twice and (b) get the pt_next
					 * queue of threads to put on the run 
					 * queue mangled by 
					 * pthread__sched_idle2()
					 */
					SDPRINTF(("\n"));
					continue;
			        }
					
			}
		}
		assert (victim != self);
		victim->pt_parent = self;
		victim->pt_next = next;
		next = victim;
		SDPRINTF(("\n"));
	}

	*qhead = next;

	return resume;
}

void
pthread__resolve_locks(pthread_t self, pthread_t *intqueuep)
{
	pthread_t victim, prev, next, switchto, runq, recycleq, intqueue;
	pthread_spin_t *lock;

	PTHREADD_ADD(PTHREADD_RESOLVELOCKS);

	recycleq = NULL;
	runq = NULL;
	intqueue = *intqueuep;
	switchto = NULL;
	victim = intqueue;

	SDPRINTF(("(rl %p) entered\n", self));

	while (intqueue != self) {
		/*
		 * Make a pass over the interrupted queue, cleaning out
		 * any threads that have dropped all their locks and any
		 * upcalls that have finished.
		 */
		SDPRINTF(("(rl %p) intqueue %p\n", self, intqueue));
		prev = NULL;
		for (victim = intqueue; victim != self; victim = next) {
			next = victim->pt_next;
			SDPRINTF(("(rl %p) victim %p (uc %p)", self,
			    victim, victim->pt_uc));
			if (victim->pt_switchto) {
				PTHREADD_ADD(PTHREADD_SWITCHTO);
				switchto = victim->pt_switchto;
				switchto->pt_uc = victim->pt_switchtouc;
				victim->pt_switchto = NULL;
				victim->pt_switchtouc = NULL;
				SDPRINTF((" switchto: %p", switchto));
			}

			if (victim->pt_type == PT_THREAD_NORMAL) {
				SDPRINTF((" normal"));
				if (victim->pt_spinlocks == 0) {
					/*
					 * We can remove this thread
					 * from the interrupted queue.
					 */
					if (prev)
						prev->pt_next = next;
					else
						intqueue = next;
					/*
					 * Check whether the victim was
					 * making a locked switch.
					 */
					if (victim->pt_heldlock) {
						/*
						 * Yes. Therefore, it's on
						 * some sleep queue and
						 * all we have to do is
						 * release the lock and
						 * restore the real
						 * sleep context.
						 */
						lock = victim->pt_heldlock;
						victim->pt_heldlock = NULL;
						__cpu_simple_unlock(lock);
						victim->pt_uc = 
						    victim->pt_sleepuc;
						victim->pt_sleepuc = NULL;
						victim->pt_next = NULL;
						victim->pt_parent = NULL;
						SDPRINTF((" heldlock: %p",lock));
					} else {
						/* 
						 * No. Queue it for the 
						 * run queue.
						 */
						victim->pt_next = runq;
						runq = victim;
					}
				} else {
					SDPRINTF((" spinlocks: %d", 
					    victim->pt_spinlocks));
					/*
					 * Still holding locks.
					 * Leave it in the interrupted queue.
					 */
					prev = victim;
				}
			} else if (victim->pt_type == PT_THREAD_UPCALL) {
				SDPRINTF((" upcall"));
				/* Okay, an upcall. */
				if (victim->pt_state == PT_STATE_RECYCLABLE) {
					/* We're done with you. */
					SDPRINTF((" recyclable"));
					if (prev)
						prev->pt_next = next;
					else
						intqueue = next;
					victim->pt_next = recycleq;
					recycleq = victim;
				} else {
					/*
					 * Not finished yet.
					 * Leave it in the interrupted queue.
					 */
					prev = victim;
				}
			} else {
				SDPRINTF((" idle"));
				/*
				 * Idle threads should be given an opportunity
				 * to put themselves on the reidle queue. 
				 * We know that they're done when they have no
				 * locks and PT_FLAG_IDLED is set.
				 */
				if (victim->pt_spinlocks != 0) {
					/* Still holding locks. */
					SDPRINTF((" spinlocks: %d", 
					    victim->pt_spinlocks));
					prev = victim;
				} else if (!(victim->pt_flags & PT_FLAG_IDLED)) {
					/*
					 * Hasn't yet put itself on the
					 * reidle queue. 
					 */
					SDPRINTF((" not done"));
					prev = victim;
				} else {
					/* Done! */
					if (prev)
						prev->pt_next = next;
					else
						intqueue = next;
				}
			}

			if (switchto) {
				assert(switchto->pt_spinlocks == 0);
				/*
				 * Threads can have switchto set to themselves
				 * if they hit new_preempt. Don't put them
				 * on the run queue twice.
				 */
				if (switchto != victim) {
					switchto->pt_next = runq;
					runq = switchto;
				}
				switchto = NULL;
			}
			SDPRINTF(("\n"));
		}

		if (intqueue != self) {
			/*
			 * There is a chain. Run through the elements
			 * of the chain. If one of them is preempted again,
			 * the upcall that handles it will have us on its
			 * chain, and we will continue here, having
			 * returned from the switch.
			 */
			SDPRINTF(("(rl %p) starting chain %p (pc: %lx sp: %lx)\n",
			    self, intqueue, pthread__uc_pc(intqueue->pt_uc), 
			    pthread__uc_sp(intqueue->pt_uc)));
			pthread__switch(self, intqueue);
			SDPRINTF(("(rl %p) returned from chain\n",
			    self));
		}

		if (self->pt_next) {
			/*
			 * We're on a chain ourselves. Let the other 
			 * threads in the chain run; our parent upcall
			 * will resume us here after a pass around its
			 * interrupted queue.
			 */
			SDPRINTF(("(rl %p) upcall chain switch to %p (pc: %lx sp: %lx)\n",
			    self, self->pt_next, 
			    pthread__uc_pc(self->pt_next->pt_uc), 
			    pthread__uc_sp(self->pt_next->pt_uc)));
			pthread__switch(self, self->pt_next);
		}

	}

	/* Recycle upcalls. */
	pthread__recycle_bulk(self, recycleq);
	SDPRINTF(("(rl %p) exiting\n", self));
	*intqueuep = runq;
}

void
pthread__recycle_bulk(pthread_t self, pthread_t qhead)
{
	int do_recycle, my_side, ret;
	pthread_t upcall;

	while(qhead != NULL) {
		pthread_spinlock(self, &recycle_lock);
		my_side = recycle_side;
		do_recycle = 0;
		while ((qhead != NULL) && 
		    (recycle_count < recycle_threshold)) {
			upcall = qhead; 
			qhead = qhead->pt_next;
			upcall->pt_state = PT_STATE_RUNNABLE;
			upcall->pt_next = NULL;
			upcall->pt_parent = NULL;
			recyclable[my_side][recycle_count] = upcall->pt_stack;
			recycle_count++;
		}
		SDPRINTF(("(recycle_bulk %p) count %d\n", self, recycle_count));
		if (recycle_count == recycle_threshold) {
			recycle_side = 1 - recycle_side;
			recycle_count = 0;
			do_recycle = 1;
		}
		pthread_spinunlock(self, &recycle_lock);
		if (do_recycle) {
			SDPRINTF(("(recycle_bulk %p) recycled %d stacks\n", self, recycle_threshold));

			ret = sa_stacks(recycle_threshold, 
			    recyclable[my_side]);
			if (ret != recycle_threshold) {
				printf("Error: recycle_threshold\n");
				printf("ret: %d  threshold: %d\n",
				    ret, recycle_threshold);

				assert(0);
			}
		}
	}
	
}

/*
 * Stash away an upcall and its stack, possibly recycling it to the kernel.
 * Must be running in the context of "new".
 */
void
pthread__sa_recycle(pthread_t old, pthread_t new)
{
	int do_recycle, my_side, ret;

	do_recycle = 0;

	old->pt_next = NULL;
	old->pt_parent = NULL;
	old->pt_state = PT_STATE_RUNNABLE;

	pthread_spinlock(new, &recycle_lock);

	my_side = recycle_side;
	recyclable[my_side][recycle_count] = old->pt_stack;
	recycle_count++;
	SDPRINTF(("(recycle %p) count %d\n", new, recycle_count));

	if (recycle_count == recycle_threshold) {
		/* Switch */
		recycle_side = 1 - recycle_side;
		recycle_count = 0;
		do_recycle = 1;
	}
	pthread_spinunlock(new, &recycle_lock);

	if (do_recycle) {
		ret = sa_stacks(recycle_threshold, recyclable[my_side]);
		SDPRINTF(("(recycle %p) recycled %d stacks\n", new, recycle_threshold));
		if (ret != recycle_threshold)
			assert(0);
	}
}

/*
 * Set the round-robin timeslice timer.
 */
static int
pthread__setrrtimer(int msec, int startit)
{
	static int rrtimer_created;
	struct itimerspec it;

	/*
	 * This check is safe -- we will either be called before there
	 * are any threads, or with the rrtimer_mutex held.
	 */
	if (rrtimer_created == 0) {
		struct sigevent ev;

		ev.sigev_notify = SIGEV_SA;
		ev.sigev_signo = 0;
		ev.sigev_value.sival_int = (int) PT_RRTIMER_MAGIC;
		if (timer_create(CLOCK_VIRTUAL, &ev, &pthread_rrtimer) == -1)
			return (errno);

		rrtimer_created = 1;
	}

	if (startit) {
		it.it_interval.tv_sec = 0;
		it.it_interval.tv_nsec = (long)msec * 1000000;
		it.it_value = it.it_interval;
		if (timer_settime(pthread_rrtimer, 0, &it, NULL) == -1)
			return (errno);
	}

	pthread_rrtimer_interval = msec;

	return (0);
}

/* Get things rolling. */
void
pthread__sa_start(void)
{
	pthread_t self, t;
	stack_t upcall_stacks[PT_UPCALLSTACKS];
	int ret, i, errnosave, flags, rr;
	char *value;

	flags = 0;
	value = getenv("PTHREAD_PREEMPT");
	if (value && strcmp(value, "yes") == 0)
		flags |= SA_FLAG_PREEMPT;

	/*
	 * It's possible the user's program has set the round-robin
	 * interval before starting any threads.
	 *
	 * Allow the environment variable to override the default.
	 *
	 * XXX Should we just nuke the environment variable?
	 */
	rr = pthread_rrtimer_interval;
	value = getenv("PTHREAD_RRTIME");
	if (value)
		rr = atoi(value);

	ret = sa_register(pthread__upcall, NULL, flags);
	if (ret)
		abort();

	self = pthread__self();
	for (i = 0; i < PT_UPCALLSTACKS; i++) {
		if (0 != (ret = pthread__stackalloc(&t)))
			abort();
		upcall_stacks[i] = t->pt_stack;	
		pthread__initthread(self, t);
		t->pt_type = PT_THREAD_UPCALL;
		t->pt_flags = PT_FLAG_DETACHED;
		sigfillset(&t->pt_sigmask); /* XXX hmmmmmm */
		/* No locking needed, there are no threads yet. */
		PTQ_INSERT_HEAD(&allqueue, t, pt_allq);
	}

	recycle_threshold = PT_UPCALLSTACKS/2;

	ret = sa_stacks(i, upcall_stacks);
	if (ret == -1)
		abort();

	/* XXX 
	 * Calling sa_enable() can mess with errno in bizzare ways,
	 * because the kernel doesn't really return from it as a
	 * normal system call. The kernel will launch an upcall
	 * handler which will jump back to the inside of sa_enable()
	 * and permit us to continue here. However, since the kernel
	 * doesn't get a chance to set up the return-state properly,
	 * the syscall stub may interpret the unmodified register
	 * state as an error return and stuff an inappropriate value
	 * into errno.
	 *
	 * Therefore, we need to keep errno from being changed by this
	 * slightly weird control flow.
	 */
	errnosave = errno;
	sa_enable();
	errno = errnosave;

	/* Start the round-robin timer. */
	if (rr != 0 && pthread__setrrtimer(rr, 1) != 0)
		abort();
}

/*
 * Interface routines to get/set the round-robin timer interval.
 *
 * XXX Sanity check the behavior for MP systems.
 */

int
pthread_getrrtimer_np(void)
{

	return (pthread_rrtimer_interval);
}

int
pthread_setrrtimer_np(int msec)
{
	extern int pthread__started;
	int ret = 0;

	if (msec < 0)
		return (EINVAL);

	pthread_mutex_lock(&rrtimer_mutex);

	ret = pthread__setrrtimer(msec, pthread__started);

	pthread_mutex_unlock(&rrtimer_mutex);

	return (ret);
}
