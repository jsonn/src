/*      $NetBSD: scheduler.c,v 1.9.4.2 2010/03/11 15:04:38 yamt Exp $	*/

/*
 * Copyright (c) 2009 Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by
 * The Finnish Cultural Foundation.
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
__KERNEL_RCSID(0, "$NetBSD: scheduler.c,v 1.9.4.2 2010/03/11 15:04:38 yamt Exp $");

#include <sys/param.h>
#include <sys/cpu.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/queue.h>
#include <sys/select.h>

#include <rump/rumpuser.h>

#include "rump_private.h"

/* should go for MAXCPUS at some point */
static struct cpu_info rump_cpus[MAXCPUS];
static struct rumpcpu {
	struct cpu_info *rcpu_ci;
	int rcpu_flags;
	struct rumpuser_cv *rcpu_cv;
	LIST_ENTRY(rumpcpu) rcpu_entries;
} rcpu_storage[MAXCPUS];
struct cpu_info *rump_cpu = &rump_cpus[0];
int ncpu = 1;

#define RCPU_WANTED	0x01	/* someone wants this specific CPU */
#define RCPU_BUSY	0x02	/* CPU is busy */
#define RCPU_FREELIST	0x04	/* CPU is on freelist */

static LIST_HEAD(,rumpcpu) cpu_freelist = LIST_HEAD_INITIALIZER(cpu_freelist);
static struct rumpuser_mtx *schedmtx;
static struct rumpuser_cv *schedcv, *lwp0cv;

static bool lwp0busy = false;

struct cpu_info *
cpu_lookup(u_int index)
{

	return &rump_cpus[index];
}

void
rump_scheduler_init()
{
	struct rumpcpu *rcpu;
	struct cpu_info *ci;
	int i;

	rumpuser_mutex_init(&schedmtx);
	rumpuser_cv_init(&schedcv);
	rumpuser_cv_init(&lwp0cv);
	for (i = 0; i < ncpu; i++) {
		rcpu = &rcpu_storage[i];
		ci = &rump_cpus[i];
		rump_cpu_bootstrap(ci);
		ci->ci_schedstate.spc_mutex =
		    mutex_obj_alloc(MUTEX_DEFAULT, IPL_NONE);
		ci->ci_schedstate.spc_flags = SPCF_RUNNING;
		rcpu->rcpu_ci = ci;
		LIST_INSERT_HEAD(&cpu_freelist, rcpu, rcpu_entries);
		rcpu->rcpu_flags = RCPU_FREELIST;
		rumpuser_cv_init(&rcpu->rcpu_cv);
	}
}

void
rump_schedule()
{
	struct lwp *l;

	/*
	 * If there is no dedicated lwp, allocate a temp one and
	 * set it to be free'd upon unschedule().  Use lwp0 context
	 * for reserving the necessary resources.
	 */
	l = rumpuser_get_curlwp();
	if (l == NULL) {
		/* busy lwp0 */
		rumpuser_mutex_enter_nowrap(schedmtx);
		while (lwp0busy)
			rumpuser_cv_wait_nowrap(lwp0cv, schedmtx);
		lwp0busy = true;
		rumpuser_mutex_exit(schedmtx);

		/* schedule cpu and use lwp0 */
		rump_schedule_cpu(&lwp0);
		rumpuser_set_curlwp(&lwp0);
		l = rump_lwp_alloc(0, rump_nextlid());

		/* release lwp0 */
		rump_lwp_switch(l);
		rumpuser_mutex_enter_nowrap(schedmtx);
		lwp0busy = false;
		rumpuser_cv_signal(lwp0cv);
		rumpuser_mutex_exit(schedmtx);

		/* mark new lwp as dead-on-exit */
		rump_lwp_release(l);
	} else {
		rump_schedule_cpu(l);
	}
}

void
rump_schedule_cpu(struct lwp *l)
{
	struct rumpcpu *rcpu;

	rumpuser_mutex_enter_nowrap(schedmtx);
	if (l->l_pflag & LP_BOUND) {
		KASSERT(l->l_cpu != NULL);
		rcpu = &rcpu_storage[l->l_cpu-&rump_cpus[0]];
		if (rcpu->rcpu_flags & RCPU_BUSY) {
			KASSERT((rcpu->rcpu_flags & RCPU_FREELIST) == 0);
			while (rcpu->rcpu_flags & RCPU_BUSY) {
				rcpu->rcpu_flags |= RCPU_WANTED;
				rumpuser_cv_wait_nowrap(rcpu->rcpu_cv,
				    schedmtx);
			}
			rcpu->rcpu_flags &= ~RCPU_WANTED;
		} else {
			KASSERT(rcpu->rcpu_flags & (RCPU_FREELIST|RCPU_WANTED));
		}
		if (rcpu->rcpu_flags & RCPU_FREELIST) {
			LIST_REMOVE(rcpu, rcpu_entries);
			rcpu->rcpu_flags &= ~RCPU_FREELIST;
		}
	} else {
		while ((rcpu = LIST_FIRST(&cpu_freelist)) == NULL) {
			rumpuser_cv_wait_nowrap(schedcv, schedmtx);
		}
		KASSERT(rcpu->rcpu_flags & RCPU_FREELIST);
		LIST_REMOVE(rcpu, rcpu_entries);
		rcpu->rcpu_flags &= ~RCPU_FREELIST;
		KASSERT(l->l_cpu == NULL);
		l->l_cpu = rcpu->rcpu_ci;
	}
	rcpu->rcpu_flags |= RCPU_BUSY;
	rumpuser_mutex_exit(schedmtx);
	l->l_mutex = rcpu->rcpu_ci->ci_schedstate.spc_mutex;
}

void
rump_unschedule()
{
	struct lwp *l;

	l = rumpuser_get_curlwp();
	KASSERT(l->l_mutex == l->l_cpu->ci_schedstate.spc_mutex);
	rump_unschedule_cpu(l);
	l->l_mutex = NULL;

	/*
	 * If we're using a temp lwp, need to take lwp0 for rump_lwp_free().
	 * (we could maybe cache idle lwp's to avoid constant bouncing)
	 */
	if (l->l_flag & LW_WEXIT) {
		rumpuser_set_curlwp(NULL);

		/* busy lwp0 */
		rumpuser_mutex_enter_nowrap(schedmtx);
		while (lwp0busy)
			rumpuser_cv_wait_nowrap(lwp0cv, schedmtx);
		lwp0busy = true;
		rumpuser_mutex_exit(schedmtx);

		rump_schedule_cpu(&lwp0);
		rumpuser_set_curlwp(&lwp0);
		rump_lwp_free(l);
		rump_unschedule_cpu(&lwp0);
		rumpuser_set_curlwp(NULL);

		rumpuser_mutex_enter_nowrap(schedmtx);
		lwp0busy = false;
		rumpuser_cv_signal(lwp0cv);
		rumpuser_mutex_exit(schedmtx);
	}
}

void
rump_unschedule_cpu(struct lwp *l)
{

	if ((l->l_pflag & LP_INTR) == 0)
		rump_softint_run(l->l_cpu);
	rump_unschedule_cpu1(l);
}

void
rump_unschedule_cpu1(struct lwp *l)
{
	struct rumpcpu *rcpu;
	struct cpu_info *ci;

	ci = l->l_cpu;
	if ((l->l_pflag & LP_BOUND) == 0) {
		l->l_cpu = NULL;
	}
	rcpu = &rcpu_storage[ci-&rump_cpus[0]];
	KASSERT(rcpu->rcpu_ci == ci);
	KASSERT(rcpu->rcpu_flags & RCPU_BUSY);

	rumpuser_mutex_enter_nowrap(schedmtx);
	if (rcpu->rcpu_flags & RCPU_WANTED) {
		/*
		 * The assumption is that there will usually be max 1
		 * thread waiting on the rcpu_cv, so broadcast is fine.
		 * (and the current structure requires it because of
		 * only a bitmask being used for wanting).
		 */
		rumpuser_cv_broadcast(rcpu->rcpu_cv);
	} else {
		LIST_INSERT_HEAD(&cpu_freelist, rcpu, rcpu_entries);
		rcpu->rcpu_flags |= RCPU_FREELIST;
		rumpuser_cv_signal(schedcv);
	}
	rcpu->rcpu_flags &= ~RCPU_BUSY;
	rumpuser_mutex_exit(schedmtx);
}

/* Give up and retake CPU (perhaps a different one) */
void
yield()
{
	struct lwp *l = curlwp;
	int nlocks;

	KERNEL_UNLOCK_ALL(l, &nlocks);
	rump_unschedule_cpu(l);
	rump_schedule_cpu(l);
	KERNEL_LOCK(nlocks, l);
}

void
preempt()
{

	yield();
}
