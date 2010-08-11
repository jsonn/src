/*	$NetBSD: intr.c,v 1.2.18.4 2010/08/11 22:55:06 yamt Exp $	*/

/*
 * Copyright (c) 2008 Antti Kantee.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: intr.c,v 1.2.18.4 2010/08/11 22:55:06 yamt Exp $");

#include <sys/param.h>
#include <sys/atomic.h>
#include <sys/cpu.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/intr.h>
#include <sys/timetc.h>

#include <rump/rumpuser.h>

#include "rump_private.h"

/*
 * Interrupt simulator.  It executes hardclock() and softintrs.
 */

#define SI_MPSAFE 0x01
#define SI_ONLIST 0x02
#define SI_KILLME 0x04

struct softint {
	void (*si_func)(void *);
	void *si_arg;
	int si_flags;
	int si_level;

	LIST_ENTRY(softint) si_entries;
};

struct softint_lev {
	struct rumpuser_cv *si_cv;
	LIST_HEAD(, softint) si_pending;
};

kcondvar_t lbolt; /* Oh Kath Ra */

static u_int ticks;

static u_int
rumptc_get(struct timecounter *tc)
{

	KASSERT(rump_threads);
	return ticks;
}

static struct timecounter rumptc = {
	.tc_get_timecount	= rumptc_get,
	.tc_poll_pps 		= NULL,
	.tc_counter_mask	= ~0,
	.tc_frequency		= 0,
	.tc_name		= "rumpclk",
	.tc_quality		= 0,
};

/*
 * clock "interrupt"
 */
static void
doclock(void *noarg)
{
	struct timespec clockbase, clockup;
	struct timespec thetick, curtime;
	struct rumpuser_cv *clockcv;
	struct rumpuser_mtx *clockmtx;
	uint64_t sec, nsec;
	int error;
	extern int hz;

	memset(&clockup, 0, sizeof(clockup));
	rumpuser_gettime(&sec, &nsec, &error);
	clockbase.tv_sec = sec;
	clockbase.tv_nsec = nsec;
	curtime = clockbase;
	thetick.tv_sec = 0;
	thetick.tv_nsec = 1000000000/hz;

	/* XXX: dummies */
	rumpuser_cv_init(&clockcv);
	rumpuser_mutex_init(&clockmtx);

	rumpuser_mutex_enter(clockmtx);
	for (;;) {
		callout_hardclock();

		/* wait until the next tick. XXX: what if the clock changes? */
		while (rumpuser_cv_timedwait(clockcv, clockmtx,
		    curtime.tv_sec, curtime.tv_nsec) == 0)
			continue;

		/* XXX: sync with a) virtual clock b) host clock */
		timespecadd(&clockup, &clockbase, &curtime);
		timespecadd(&clockup, &thetick, &clockup);

#if 0
		/* CPU_IS_PRIMARY is MD and hence unreliably correct here */
		if (!CPU_IS_PRIMARY(curcpu()))
			continue;
#else
		if (curcpu()->ci_index != 0)
			continue;
#endif

		if ((++ticks % hz) == 0) {
			cv_broadcast(&lbolt);
		}
		tc_ticktock();
	}
}

/*
 * Soft interrupt execution thread.  This thread is pinned to the
 * same CPU that scheduled the interrupt, so we don't need to do
 * lock against si_lvl.
 */
static void
sithread(void *arg)
{
	struct softint *si;
	void (*func)(void *) = NULL;
	void *funarg;
	bool mpsafe;
	int mylevel = (uintptr_t)arg;
	struct softint_lev *si_lvlp, *si_lvl;
	struct cpu_data *cd = &curcpu()->ci_data;

	si_lvlp = cd->cpu_softcpu;
	si_lvl = &si_lvlp[mylevel];

	for (;;) {
		if (!LIST_EMPTY(&si_lvl->si_pending)) {
			si = LIST_FIRST(&si_lvl->si_pending);
			func = si->si_func;
			funarg = si->si_arg;
			mpsafe = si->si_flags & SI_MPSAFE;

			si->si_flags &= ~SI_ONLIST;
			LIST_REMOVE(si, si_entries);
			if (si->si_flags & SI_KILLME) {
				softint_disestablish(si);
				continue;
			}
		} else {
			rump_schedlock_cv_wait(si_lvl->si_cv);
			continue;
		}

		if (!mpsafe)
			KERNEL_LOCK(1, curlwp);
		func(funarg);
		if (!mpsafe)
			KERNEL_UNLOCK_ONE(curlwp);
	}

	panic("sithread unreachable");
}

void
rump_intr_init()
{

	cv_init(&lbolt, "oh kath ra");
}

void
softint_init(struct cpu_info *ci)
{
	struct cpu_data *cd = &ci->ci_data;
	struct softint_lev *slev;
	int rv, i;

	if (!rump_threads)
		return;

	/* XXX */
	if (ci->ci_index == 0) {
		rumptc.tc_frequency = hz;
		tc_init(&rumptc);
	}

	slev = kmem_alloc(sizeof(struct softint_lev) * SOFTINT_COUNT, KM_SLEEP);
	for (i = 0; i < SOFTINT_COUNT; i++) {
		rumpuser_cv_init(&slev[i].si_cv);
		LIST_INIT(&slev[i].si_pending);
	}
	cd->cpu_softcpu = slev;

	/* softint might run on different physical CPU */
	membar_sync();

	for (i = 0; i < SOFTINT_COUNT; i++) {
		rv = kthread_create(PRI_NONE,
		    KTHREAD_MPSAFE | KTHREAD_INTR, ci,
		    sithread, (void *)(uintptr_t)i,
		    NULL, "rsi%d/%d", ci->ci_index, i);
	}

	rv = kthread_create(PRI_NONE, KTHREAD_MPSAFE,
	    ci, doclock, NULL, NULL, "rumpclk%d", ci->ci_index);
	if (rv)
		panic("clock thread creation failed: %d", rv);
}

/*
 * Soft interrupts bring two choices.  If we are running with thread
 * support enabled, defer execution, otherwise execute in place.
 * See softint_schedule().
 * 
 * As there is currently no clear concept of when a thread finishes
 * work (although rump_clear_curlwp() is close), simply execute all
 * softints in the timer thread.  This is probably not the most
 * efficient method, but good enough for now.
 */
void *
softint_establish(u_int flags, void (*func)(void *), void *arg)
{
	struct softint *si;

	si = malloc(sizeof(*si), M_TEMP, M_WAITOK);
	si->si_func = func;
	si->si_arg = arg;
	si->si_flags = flags & SOFTINT_MPSAFE ? SI_MPSAFE : 0;
	si->si_level = flags & SOFTINT_LVLMASK;
	KASSERT(si->si_level < SOFTINT_COUNT);

	return si;
}

void
softint_schedule(void *arg)
{
	struct softint *si = arg;
	struct cpu_data *cd = &curcpu()->ci_data;
	struct softint_lev *si_lvl = cd->cpu_softcpu;

	if (!rump_threads) {
		si->si_func(si->si_arg);
	} else {
		if (!(si->si_flags & SI_ONLIST)) {
			LIST_INSERT_HEAD(&si_lvl[si->si_level].si_pending,
			    si, si_entries);
			si->si_flags |= SI_ONLIST;
		}
	}
}

/* flimsy disestablish: should wait for softints to finish */
void
softint_disestablish(void *cook)
{
	struct softint *si = cook;

	if (si->si_flags & SI_ONLIST) {
		si->si_flags |= SI_KILLME;
		return;
	}
	free(si, M_TEMP);
}

void
rump_softint_run(struct cpu_info *ci)
{
	struct cpu_data *cd = &ci->ci_data;
	struct softint_lev *si_lvl = cd->cpu_softcpu;
	int i;

	if (!rump_threads)
		return;

	for (i = 0; i < SOFTINT_COUNT; i++) {
		if (!LIST_EMPTY(&si_lvl[i].si_pending))
			rumpuser_cv_signal(si_lvl[i].si_cv);
	}
}

bool
cpu_intr_p(void)
{

	return false;
}

bool
cpu_softintr_p(void)
{

	return curlwp->l_pflag & LP_INTR;
}
