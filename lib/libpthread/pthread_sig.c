/*	$NetBSD: pthread_sig.c,v 1.1.2.15 2002/10/14 23:43:00 nathanw Exp $	*/

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

/* We're interposing a specific version of the signal interface. */
#define	__LIBC12_SOURCE__

#include <assert.h>
#include <errno.h>
#include <lwp.h>
#include <stdlib.h>
#include <stdio.h>
#include <ucontext.h>
#include <unistd.h>
#include <sys/cdefs.h>
#include <sys/syscall.h>

#include "sched.h"
#include "pthread.h"
#include "pthread_int.h"

#undef PTHREAD_SIG_DEBUG

#ifdef PTHREAD_SIG_DEBUG
#define SDPRINTF(x) DPRINTF(x)
#else
#define SDPRINTF(x)
#endif

static pthread_spin_t	pt_sigacts_lock;
static struct sigaction pt_sigacts[_NSIG];

static pthread_spin_t	pt_process_siglock;
static sigset_t	pt_process_sigmask;
static sigset_t	pt_process_siglist;

/* Queue of threads that are waiting in sigsuspend(). */
static struct pthread_queue_t pt_sigsuspended;
static pthread_spin_t pt_sigsuspended_lock;

/*
 * Nothing actually signals or waits on this lock, but the sleepobj
 * needs to point to something.
 */
static pthread_cond_t pt_sigsuspended_cond = PTHREAD_COND_INITIALIZER;


static void 
pthread__signal_tramp(int, int, struct sigaction *, ucontext_t *, sigset_t *,
    int, struct pthread_queue_t *, pthread_spin_t *);

void
pthread__signal_init(void)
{
	SDPRINTF(("(signal_init) setting process sigmask\n"));
	__sigprocmask14(0, NULL, &pt_process_sigmask);
}

int
pthread_kill(pthread_t thread, int sig)
{

	/* We only let the thread handle this signal if the action for
	 * the signal is an explicit handler. Otherwise we feed it to
	 * the kernel so that it can affect the whole process.
	 */

	/* XXX implement me */
	return -1;
}


/*
 * Interpositioning is our friend. We need to intercept sigaction() and
 * sigsuspend().
 */

int
__sigaction14(int sig, const struct sigaction *act, struct sigaction *oact)
{
	pthread_t self;
	struct sigaction realact;
	
	self = pthread__self();
	if (act != NULL) {
		/* Save the information for our internal dispatch. */
		pthread_spinlock(self, &pt_sigacts_lock);
		pt_sigacts[sig] = *act;
		pthread_spinunlock(self, &pt_sigacts_lock);
		/*
		 * We want to handle all signals ourself, and not have
		 * the kernel mask them. Therefore, we clear the
		 * sa_mask field before passing this to the kernel. We
		 * do not set SA_NODEFER, which seems like it might be
		 * appropriate, because that would permit a continuous
		 * stream of signals to exhaust the supply of upcalls.
		 */
		realact = *act;
		__sigemptyset14(&realact.sa_mask);
		act = &realact;
	}

	return (__libc_sigaction14(sig, act, oact));
}

int
__sigsuspend14(const sigset_t *sigmask)
{
	pthread_t self;
	sigset_t oldmask;

	self = pthread__self();

	pthread_spinlock(self, &pt_sigsuspended_lock);
	pthread_spinlock(self, &self->pt_statelock);
	if (self->pt_cancel) {
		pthread_spinunlock(self, &self->pt_statelock);
		pthread_spinunlock(self, &pt_sigsuspended_lock);
		pthread_exit(PTHREAD_CANCELED);
	}
	pthread_sigmask(SIG_SETMASK, sigmask, &oldmask);

	self->pt_flags |= PT_FLAG_SIGCATCH;
	self->pt_state = PT_STATE_BLOCKED_QUEUE;
	self->pt_sleepobj = &pt_sigsuspended_cond;
	self->pt_sleepq = &pt_sigsuspended;
	self->pt_sleeplock = &pt_sigsuspended_lock;
	pthread_spinunlock(self, &self->pt_statelock);

	PTQ_INSERT_TAIL(&pt_sigsuspended, self, pt_sleep);
	pthread__block(self, &pt_sigsuspended_lock);

	pthread__testcancel(self);
	self->pt_flags &= ~PT_FLAG_SIGCATCH;

	pthread_sigmask(SIG_SETMASK, &oldmask, NULL);

	errno = EINTR;
	return -1;
}

/*
 * firstsig is stolen from kern_sig.c
 * XXX this is an abstraction violation; except for this, all of 
 * the knowledge about the composition of sigset_t's was encapsulated
 * in signal.h.
 * Putting this function in signal.h caused problems with other parts of the
 * kernel that #included <signal.h> but didn't have a prototype for ffs.
 */

static int
firstsig(const sigset_t *ss)
{
	int sig;

	sig = ffs(ss->__bits[0]);
	if (sig != 0)
		return (sig);
#if NSIG > 33
	sig = ffs(ss->__bits[1]);
	if (sig != 0)
		return (sig + 32);
#endif
#if NSIG > 65
	sig = ffs(ss->__bits[2]);
	if (sig != 0)
		return (sig + 64);
#endif
#if NSIG > 97
	sig = ffs(ss->__bits[3]);
	if (sig != 0)
		return (sig + 96);
#endif
	return (0);
}

int
pthread_sigmask(int how, const sigset_t *set, sigset_t *oset)
{
	pthread_t self;
	sigset_t tmp;
	int i, retval, unblocked, procmaskset;
	void (*handler)(int, int, struct sigcontext *);
	struct sigaction *act;
	struct sigcontext xxxsc;
	sigset_t oldmask, takelist;

	self = pthread__self();

	retval = 0;
	unblocked = 0;
	if (set != NULL) {
		pthread_spinlock(self, &self->pt_siglock);
		if (how == SIG_BLOCK) {
			__sigplusset(set, &self->pt_sigmask);
			/*
			 * Blocking of signals that are now
			 * blocked by all threads will be done
			 * lazily, at signal delivery time.
			 */
		} else if (how == SIG_UNBLOCK) {
			pthread_spinlock(self, &pt_process_siglock);
			__sigminusset(set, &self->pt_sigmask);
			/*
			 * Unblock any signals that were blocked
			 * process-wide before this.
			 */
			tmp = pt_process_sigmask;
			__sigminusset(set, &tmp);
			if (!__sigsetequal(&tmp, &pt_process_sigmask))
				pt_process_sigmask = tmp;
			unblocked = 1;
		} else if (how == SIG_SETMASK) {
			pthread_spinlock(self, &pt_process_siglock);
			self->pt_sigmask = *set;
			SDPRINTF(("(pt_sigmask %p) (set) set thread mask to "
			    "%08x\n", self, self->pt_sigmask.__bits[0]));
			/*
			 * Unblock any signals that were blocked
			 * process-wide before this.
			 */
			tmp = pt_process_sigmask;
			__sigandset(set, &tmp);
			if (!__sigsetequal(&tmp, &pt_process_sigmask))
				pt_process_sigmask = tmp;
			unblocked = 1; /* Well, maybe */
			
		} else 
			retval = EINVAL;
	}

	if (unblocked) {
		SDPRINTF(("(pt_sigmask %p) Process mask was changed to %08x\n",
		    self, pt_process_sigmask.__bits[0]));
		/* See if there are any signals to take */
		__sigemptyset14(&takelist);
		while ((i = firstsig(&self->pt_siglist)) != 0) {
			if (!__sigismember14(&self->pt_sigmask, i)) {
				__sigaddset14(&takelist, i);
				__sigdelset14(&self->pt_siglist, i);
			}
		}
		while ((i = firstsig(&pt_process_siglist)) != 0) {
			if (!__sigismember14(&self->pt_sigmask, i)) {
				__sigaddset14(&takelist, i);
				__sigdelset14(&pt_process_siglist, i);
			}
		}

		procmaskset = 0;
		while ((i = firstsig(&takelist)) != 0) {
			if (!__sigismember14(&self->pt_sigmask, i)) {
				/* Take the signal */
				act = &pt_sigacts[i];
				oldmask = self->pt_sigmask;
				__sigplusset(&self->pt_sigmask,
				    &act->sa_mask);
				__sigaddset14(&self->pt_sigmask, i);
				pthread_spinunlock(self, &pt_process_siglock);
				pthread_spinunlock(self, &self->pt_siglock);
				SDPRINTF(("(pt_sigmask %p) taking unblocked signal %d\n", self, i));
				handler =
				    (void (*)(int, int, struct sigcontext *))
				    act->sa_handler;
				handler(i, 0, &xxxsc);
				pthread_spinlock(self, &self->pt_siglock);
				pthread_spinlock(self, &pt_process_siglock);
				__sigdelset14(&takelist, i);
				/* Reset masks */
				self->pt_sigmask = oldmask;
				tmp = pt_process_sigmask;
				__sigandset(&oldmask, &tmp);
				if (!__sigsetequal(&tmp, &pt_process_sigmask)){
					pt_process_sigmask = tmp;
					SDPRINTF(("(pt_sigmask %p) setting proc sigmask to %08x\n", pt_process_sigmask.__bits[0]));
					__sigprocmask14(SIG_SETMASK, 
					    &pt_process_sigmask, NULL);
					procmaskset = 1;
				}
			}
		}
		if (!procmaskset) {
			SDPRINTF(("(pt_sigmask %p) setting proc sigmask to %08x\n", self, pt_process_sigmask.__bits[0]));
			__sigprocmask14(SIG_SETMASK, &pt_process_sigmask, NULL);
		}
	} 
	pthread_spinunlock(self, &pt_process_siglock);
	pthread_spinunlock(self, &self->pt_siglock);

	/*
	 * While other threads may read a process's sigmask,
	 * they won't write it, so we don't need to lock our reads of it.
	 */
	if (oset != NULL) {
		*oset = self->pt_sigmask;
	}

	return retval;
}


/*
 * Dispatch a signal to thread t, if it is non-null, and to any
 * willing thread, if t is null.
 */
extern pthread_spin_t runqueue_lock;
extern struct pthread_queue_t runqueue;

void
pthread__signal(pthread_t t, int sig, int code)
{
	pthread_t self, target, good, okay;
	sigset_t oldmask, *maskp;
	ucontext_t *uc;

	extern pthread_spin_t allqueue_lock;
	extern struct pthread_queue_t allqueue;

	self = pthread__self();
	if (t)
		target = t;
	else {
		/*
		 * Pick a thread that doesn't have the signal blocked
		 * and can be reasonably forced to run. 
		 */
		okay = good = NULL;
		pthread_spinlock(self, &allqueue_lock);
		PTQ_FOREACH(target, &allqueue, pt_allq) {
			/*
			 * Changing to PT_STATE_ZOMBIE is protected
			 * by the allqueue lock, so we can just test for
			 * it here.
			 */
			if ((target->pt_state == PT_STATE_ZOMBIE) ||
			    (target->pt_type != PT_THREAD_NORMAL))
				continue;
			pthread_spinlock(self, &target->pt_siglock);
			SDPRINTF((
				"(pt_signal %p) target %p: state %d, mask %08x\n", 
				self, target, target->pt_state, target->pt_sigmask.__bits[0]));
			if (!__sigismember14(&target->pt_sigmask, sig)) {
				if (target->pt_state != PT_STATE_BLOCKED_SYS) {
					good = target;
					/* Leave target locked */
					break;
				} else if (okay == NULL) {
					okay = target;
					/* Leave target locked */
					continue;
				}
			}
			pthread_spinunlock(self, &target->pt_siglock);
		}
		pthread_spinunlock(self, &allqueue_lock);
		if (good) {
			target = good;
			if (okay)
				pthread_spinunlock(self, &okay->pt_siglock);
		} else {
			target = okay;
		}

		if (target == NULL) {
			/*
			 * They all have it blocked. Note that in our
			 * process-wide signal mask, and stash the signal
			 * for later unblocking.  
			 */
			pthread_spinlock(self, &pt_process_siglock);
			__sigaddset14(&pt_process_sigmask, sig);
			SDPRINTF(("(pt_signal %p) lazily setting proc sigmask to "
			    "%08x\n", self, pt_process_sigmask.__bits[0]));
			__sigprocmask14(SIG_SETMASK, &pt_process_sigmask, NULL);
			__sigaddset14(&pt_process_siglist, sig);
			pthread_spinunlock(self, &pt_process_siglock);
			return;
		}
	}

	/*
	 * We now have a signal and a victim. 
	 * The victim's pt_siglock is locked.
	 */

	/*
	 * Reset the process signal mask so we can take another
	 * signal.  We will not exhaust our supply of upcalls; if
	 * another signal is delivered after this, the resolve_locks
	 * dance will permit us to finish and recycle before the next
	 * upcall reaches this point.
	 */
	pthread_spinlock(self, &pt_process_siglock);
	SDPRINTF(("(pt_signal %p) setting proc sigmask to "
	    "%08x\n", self, pt_process_sigmask.__bits[0]));
	__sigprocmask14(SIG_SETMASK, &pt_process_sigmask, NULL);
	pthread_spinunlock(self, &pt_process_siglock);

	if (t && __sigismember14(&target->pt_sigmask, sig)) {
		/* Record the signal for later delivery. */
		__sigaddset14(&target->pt_siglist, sig);
		pthread_spinunlock(self, &target->pt_siglock);
		return;
	}

	/*
	 * Ensure the victim is not running.
	 * In a MP world, it could be on another processor somewhere.
	 *
	 * XXX As long as this is uniprocessor, encountering a running
	 * target process is a bug.
	 */
	assert(target->pt_state != PT_STATE_RUNNING);
	/*
	 * Prevent anyone else from considering this thread for handling
	 * more instances of this signal.
	 */
	oldmask = target->pt_sigmask;
	__sigplusset(&target->pt_sigmask, &pt_sigacts[sig].sa_mask);
	__sigaddset14(&target->pt_sigmask, sig);
	pthread_spinunlock(self, &target->pt_siglock);

	/*
	 * Holding the state lock blocks out cancellation and any other
	 * attempts to set this thread up to take a signal.
	 */
	pthread_spinlock(self, &target->pt_statelock);
	switch (target->pt_state) {
	case PT_STATE_RUNNABLE:
		pthread_spinlock(self, &runqueue_lock);
		PTQ_REMOVE(&runqueue, target, pt_runq);
		pthread_spinunlock(self, &runqueue_lock);
		break;
	case PT_STATE_BLOCKED_QUEUE:
		pthread_spinlock(self, target->pt_sleeplock);
		PTQ_REMOVE(target->pt_sleepq, target, pt_sleep);
		pthread_spinunlock(self, target->pt_sleeplock);
		break;
	case PT_STATE_BLOCKED_SYS:
		/*
		 * The target is not on a queue at all, and won't run
		 * again for a while. Try to wake it from its torpor.
		 */
		_lwp_wakeup(target->pt_blockedlwp);
		break;
	default:
		;
	}

	/*
	 * We'd like to just pass oldmask to the
	 * pthread__signal_tramp(), but makecontext() can't reasonably
	 * pass structures, just word-size things or smaller. We also
	 * don't want to involve malloc() here, inside the upcall
	 * handler. So we borrow a bit of space from the target's
	 * stack, which we were adjusting anyway.
	 */
	maskp = (sigset_t *)((char *)target->pt_uc - sizeof(sigset_t));
	*maskp = oldmask;

	/*
	 * XXX We are blatantly ignoring SIGALTSTACK. It would screw
	 * with our notion of stack->thread mappings.
	 */
	uc = (ucontext_t *)((char *)maskp - 
	    STACKSPACE - sizeof(ucontext_t));

	_getcontext_u(uc);
	uc->uc_flags &= ~UC_USER;

	uc->uc_stack.ss_sp = maskp;
	uc->uc_stack.ss_size = 0;
	SDPRINTF(("(makecontext %p): target %p: sig: %d %d uc: %p oldmask: %08x\n",
	    self, target, sig, code, target->pt_uc, maskp->__bits[0]));
	makecontext(uc, pthread__signal_tramp, 8,
	    sig, code, &pt_sigacts[sig], target->pt_uc, maskp,
	    target->pt_state, target->pt_sleepq, target->pt_sleeplock);
	target->pt_uc = uc;

	if (target->pt_state != PT_STATE_BLOCKED_SYS)
		pthread__sched(self, target);
	pthread_spinunlock(self, &target->pt_statelock);
}


static void 
pthread__signal_tramp(int sig, int code, struct sigaction *act, 
    ucontext_t *uc, sigset_t *oldmask, int oldstate,
    struct pthread_queue_t *oldsleepq, pthread_spin_t *oldsleeplock)
{
	void (*handler)(int, int, struct sigcontext *);
	struct sigcontext xxxsc;
	pthread_t self, next;

	self = pthread__self();

	SDPRINTF(("(tramp %p): sig: %d %d  uc: %p maskp: %p\n", 
	    self, sig, code, uc, oldmask));

	/*
	 * We should only ever get here if a handler is set. Signal
	 * actions are process-global; a signal set to SIG_DFL or
	 * SIG_IGN should be handled in the kernel (by being ignored
	 * or killing the process) and never get this far.
	 */
	handler = (void (*)(int, int, struct sigcontext *)) act->sa_handler;

	/*
	 * XXX we don't support real sigcontext or siginfo here yet.
	 * Ironically, siginfo is actually easier to deal with.
	 */
       	handler(sig, code, &xxxsc);

	/*
	 * We've finished the handler, so this thread can restore the
	 * original signal mask.
	 */
       	pthread_sigmask(SIG_SETMASK, oldmask, NULL);

	/*
	 * Go back to whatever queue we were found on, unless SIGCATCH
	 * is set.  When we are continued, the first thing we do will
	 * be to jump back to the previous context.
	 */
	if (self->pt_flags & PT_FLAG_SIGCATCH)
		_setcontext_u(uc);
		
	next = pthread__next(self);
	next->pt_state = PT_STATE_RUNNING;
	pthread_spinlock(self, &self->pt_statelock);
	if (oldstate == PT_STATE_RUNNABLE) {
		pthread_spinlock(self, &runqueue_lock);
		pthread_spinunlock(self, &self->pt_statelock);
		self->pt_state = PT_STATE_RUNNABLE;
		PTQ_INSERT_TAIL(&runqueue, self, pt_runq);
		pthread__locked_switch(self, next, &runqueue_lock);
	} else if (oldstate == PT_STATE_BLOCKED_QUEUE) {
		pthread_spinlock(self, oldsleeplock);
		self->pt_state = PT_STATE_BLOCKED_QUEUE;
		self->pt_sleepq = oldsleepq;
		self->pt_sleeplock = oldsleeplock;
		pthread_spinunlock(self, &self->pt_statelock);
		PTQ_INSERT_TAIL(oldsleepq, self, pt_sleep);
		pthread__locked_switch(self, next, oldsleeplock);
	} else if (oldstate == PT_STATE_BLOCKED_SYS) {
		/*
		 * We weren't really doing anything before. Just go to
		 * the next thread. 
		 */
		self->pt_state = PT_STATE_BLOCKED_SYS;
		pthread_spinunlock(self, &self->pt_statelock);
		pthread__switch(self, next);
	} else {
		assert(0);
	}
	/*
	 * Jump back to what we were doing before we were interrupted
	 * by a signal.
	 */
	_setcontext_u(uc);
       	/* NOTREACHED */
}
