/*	$NetBSD: kern_kthread.c,v 1.24.10.2 2009/02/02 22:02:24 snj Exp $	*/

/*-
 * Copyright (c) 1998, 1999, 2007, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center, and by Andrew Doran.
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
__KERNEL_RCSID(0, "$NetBSD: kern_kthread.c,v 1.24.10.2 2009/02/02 22:02:24 snj Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/kmem.h>

#include <uvm/uvm_extern.h>

/*
 * note that stdarg.h and the ansi style va_start macro is used for both
 * ansi and traditional c complers.
 * XXX: this requires that stdarg.h define: va_alist and va_dcl
 */
#include <machine/stdarg.h>

/*
 * Fork a kernel thread.  Any process can request this to be done.
 */
int
kthread_create(pri_t pri, int flag, struct cpu_info *ci,
	       void (*func)(void *), void *arg,
	       lwp_t **lp, const char *fmt, ...)
{
	lwp_t *l;
	vaddr_t uaddr;
	bool inmem;
	int error;
	va_list ap;
	int lc;

	inmem = uvm_uarea_alloc(&uaddr);
	if (uaddr == 0)
		return ENOMEM;
	if ((flag & KTHREAD_TS) != 0) {
		lc = SCHED_OTHER;
	} else {
		lc = SCHED_RR;
	}
	error = lwp_create(&lwp0, &proc0, uaddr, inmem, LWP_DETACHED, NULL,
	    0, func, arg, &l, lc);
	if (error) {
		uvm_uarea_free(uaddr, curcpu());
		return error;
	}
	uvm_lwp_hold(l);
	if (fmt != NULL) {
		l->l_name = kmem_alloc(MAXCOMLEN, KM_SLEEP);
		if (l->l_name == NULL) {
			lwp_exit(l);
			return ENOMEM;
		}
		va_start(ap, fmt);
		vsnprintf(l->l_name, MAXCOMLEN, fmt, ap);
		va_end(ap);
	}

	/*
	 * Set parameters.
	 */
	if ((flag & KTHREAD_INTR) != 0) {
		KASSERT((flag & KTHREAD_MPSAFE) != 0);
	}

	if (pri == PRI_NONE) {
		if ((flag & KTHREAD_TS) != 0) {
			/* Maximum user priority level. */
			pri = MAXPRI_USER;
		} else {
			/* Minimum kernel priority level. */
			pri = PRI_KTHREAD;
		}
	}
	mutex_enter(proc0.p_lock);
	lwp_lock(l);
	l->l_priority = pri;
	if (ci != NULL) {
		if (ci != l->l_cpu) {
			lwp_unlock_to(l, ci->ci_schedstate.spc_mutex);
			lwp_lock(l);
		}
		l->l_pflag |= LP_BOUND;
		l->l_cpu = ci;
	}
	if ((flag & KTHREAD_INTR) != 0)
		l->l_pflag |= LP_INTR;
	if ((flag & KTHREAD_MPSAFE) == 0)
		l->l_pflag &= ~LP_MPSAFE;

	/*
	 * Set the new LWP running, unless the caller has requested
	 * otherwise.
	 */
	if ((flag & KTHREAD_IDLE) == 0) {
		l->l_stat = LSRUN;
		sched_enqueue(l, false);
		lwp_unlock(l);
	} else
		lwp_unlock_to(l, ci->ci_schedstate.spc_lwplock);

	/*
	 * The LWP is not created suspended or stopped and cannot be set
	 * into those states later, so must be considered runnable.
	 */
	proc0.p_nrlwps++;
	mutex_exit(proc0.p_lock);

	/* All done! */
	if (lp != NULL)
		*lp = l;

	return (0);
}

/*
 * Cause a kernel thread to exit.  Assumes the exiting thread is the
 * current context.
 */
void
kthread_exit(int ecode)
{
	const char *name;
	lwp_t *l = curlwp;

	/* We can't do much with the exit code, so just report it. */
	if (ecode != 0) {
		if ((name = l->l_name) == NULL)
			name = "unnamed";
		printf("WARNING: kthread `%s' (%d) exits with status %d\n",
		    name, l->l_lid, ecode);
	}

	/* And exit.. */
	lwp_exit(l);

	/*
	 * XXX Fool the compiler.  Making exit1() __noreturn__ is a can
	 * XXX of worms right now.
	 */
	for (;;)
		;
}

/*
 * Destroy an inactive kthread.  The kthread must be in the LSIDL state.
 */
void
kthread_destroy(lwp_t *l)
{

	KASSERT((l->l_flag & LW_SYSTEM) != 0);
	KASSERT(l->l_stat == LSIDL);

	lwp_exit(l);
}
