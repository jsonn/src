/*	$NetBSD: kern_fork.c,v 1.30.6.1 1997/03/12 21:23:38 is Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)kern_fork.c	8.6 (Berkeley) 4/8/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/map.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/acct.h>
#include <sys/ktrace.h>

#include <sys/syscallargs.h>

#include <vm/vm.h>

int	nprocs = 1;		/* process 0 */

int fork1 __P((struct proc *, int, register_t *));

/*ARGSUSED*/
int
sys_fork(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

	return (fork1(p, 0, retval));
}

/*ARGSUSED*/
int
sys_vfork(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

	return (fork1(p, 1, retval));
}

int
fork1(p1, isvfork, retval)
	register struct proc *p1;
	int isvfork;
	register_t *retval;
{
	register struct proc *p2;
	register uid_t uid;
	struct proc *newproc;
	int count;
	static int nextpid, pidchecked = 0;

	/*
	 * Although process entries are dynamically created, we still keep
	 * a global limit on the maximum number we will create.  Don't allow
	 * a nonprivileged user to use the last process; don't let root
	 * exceed the limit. The variable nprocs is the current number of
	 * processes, maxproc is the limit.
	 */
	uid = p1->p_cred->p_ruid;
	if ((nprocs >= maxproc - 1 && uid != 0) || nprocs >= maxproc) {
		tablefull("proc");
		return (EAGAIN);
	}

	/*
	 * Increment the count of procs running with this uid. Don't allow
	 * a nonprivileged user to exceed their current limit.
	 */
	count = chgproccnt(uid, 1);
	if (uid != 0 && count > p1->p_rlimit[RLIMIT_NPROC].rlim_cur) {
		(void)chgproccnt(uid, -1);
		return (EAGAIN);
	}

	/* Allocate new proc. */
	MALLOC(newproc, struct proc *, sizeof(struct proc), M_PROC, M_WAITOK);

	/*
	 * Find an unused process ID.  We remember a range of unused IDs
	 * ready to use (from nextpid+1 through pidchecked-1).
	 */
	nextpid++;
retry:
	/*
	 * If the process ID prototype has wrapped around,
	 * restart somewhat above 0, as the low-numbered procs
	 * tend to include daemons that don't exit.
	 */
	if (nextpid >= PID_MAX) {
		nextpid = 100;
		pidchecked = 0;
	}
	if (nextpid >= pidchecked) {
		int doingzomb = 0;

		pidchecked = PID_MAX;
		/*
		 * Scan the active and zombie procs to check whether this pid
		 * is in use.  Remember the lowest pid that's greater
		 * than nextpid, so we can avoid checking for a while.
		 */
		p2 = allproc.lh_first;
again:
		for (; p2 != 0; p2 = p2->p_list.le_next) {
			while (p2->p_pid == nextpid ||
			    p2->p_pgrp->pg_id == nextpid) {
				nextpid++;
				if (nextpid >= pidchecked)
					goto retry;
			}
			if (p2->p_pid > nextpid && pidchecked > p2->p_pid)
				pidchecked = p2->p_pid;
			if (p2->p_pgrp->pg_id > nextpid && 
			    pidchecked > p2->p_pgrp->pg_id)
				pidchecked = p2->p_pgrp->pg_id;
		}
		if (!doingzomb) {
			doingzomb = 1;
			p2 = zombproc.lh_first;
			goto again;
		}
	}

	nprocs++;
	p2 = newproc;
	p2->p_stat = SIDL;			/* protect against others */
	p2->p_pid = nextpid;
	LIST_INSERT_HEAD(&allproc, p2, p_list);
	p2->p_forw = p2->p_back = NULL;		/* shouldn't be necessary */
	LIST_INSERT_HEAD(PIDHASH(p2->p_pid), p2, p_hash);

	/*
	 * Make a proc table entry for the new process.
	 * Start by zeroing the section of proc that is zero-initialized,
	 * then copy the section that is copied directly from the parent.
	 */
	bzero(&p2->p_startzero,
	    (unsigned) ((caddr_t)&p2->p_endzero - (caddr_t)&p2->p_startzero));
	bcopy(&p1->p_startcopy, &p2->p_startcopy,
	    (unsigned) ((caddr_t)&p2->p_endcopy - (caddr_t)&p2->p_startcopy));

	/*
	 * Duplicate sub-structures as needed.
	 * Increase reference counts on shared objects.
	 * The p_stats and p_sigacts substructs are set in vm_fork.
	 */
	p2->p_flag = P_INMEM | (p1->p_flag & P_SUGID);
	p2->p_emul = p1->p_emul;
	if (p1->p_flag & P_PROFIL)
		startprofclock(p2);
	MALLOC(p2->p_cred, struct pcred *, sizeof(struct pcred),
	    M_SUBPROC, M_WAITOK);
	bcopy(p1->p_cred, p2->p_cred, sizeof(*p2->p_cred));
	p2->p_cred->p_refcnt = 1;
	crhold(p1->p_ucred);

	/* bump references to the text vnode (for procfs) */
	p2->p_textvp = p1->p_textvp;
	if (p2->p_textvp)
		VREF(p2->p_textvp);

	p2->p_fd = fdcopy(p1);
	/*
	 * If p_limit is still copy-on-write, bump refcnt,
	 * otherwise get a copy that won't be modified.
	 * (If PL_SHAREMOD is clear, the structure is shared
	 * copy-on-write.)
	 */
	if (p1->p_limit->p_lflags & PL_SHAREMOD)
		p2->p_limit = limcopy(p1->p_limit);
	else {
		p2->p_limit = p1->p_limit;
		p2->p_limit->p_refcnt++;
	}

	if (p1->p_session->s_ttyvp != NULL && p1->p_flag & P_CONTROLT)
		p2->p_flag |= P_CONTROLT;
	if (isvfork)
		p2->p_flag |= P_PPWAIT;
	LIST_INSERT_AFTER(p1, p2, p_pglist);
	p2->p_pptr = p1;
	LIST_INSERT_HEAD(&p1->p_children, p2, p_sibling);
	LIST_INIT(&p2->p_children);

#ifdef KTRACE
	/*
	 * Copy traceflag and tracefile if enabled.
	 * If not inherited, these were zeroed above.
	 */
	if (p1->p_traceflag&KTRFAC_INHERIT) {
		p2->p_traceflag = p1->p_traceflag;
		if ((p2->p_tracep = p1->p_tracep) != NULL)
			VREF(p2->p_tracep);
	}
#endif

	/*
	 * This begins the section where we must prevent the parent
	 * from being swapped.
	 */
	PHOLD(p1);

#ifdef __FORK_BRAINDAMAGE
	/*
	 * Set return values for child before vm_fork,
	 * so they can be copied to child stack.
	 * We return 0, rather than the traditional behaviour of modifying the
	 * return value in the system call stub.
	 * NOTE: the kernel stack may be at a different location in the child
	 * process, and thus addresses of automatic variables (including retval)
	 * may be invalid after vm_fork returns in the child process.
	 */
	retval[0] = 0;
	retval[1] = 1;
	if (vm_fork(p1, p2))
		return (0);
#else
	/*
	 * Finish creating the child process.  It will return through a
	 * different path later.
	 */
	vm_fork(p1, p2);
#endif

	/*
	 * Make child runnable, set start time, and add to run queue.
	 */
	(void) splstatclock();
	p2->p_stats->p_start = time;
	p2->p_acflag = AFORK;
	p2->p_stat = SRUN;
	setrunqueue(p2);
	(void) spl0();

	/*
	 * Now can be swapped.
	 */
	PRELE(p1);

	/*
	 * Preserve synchronization semantics of vfork.  If waiting for
	 * child to exec or exit, set P_PPWAIT on child, and sleep on our
	 * proc (in case of exit).
	 */
	if (isvfork)
		while (p2->p_flag & P_PPWAIT)
			tsleep(p1, PWAIT, "ppwait", 0);

	/*
	 * Return child pid to parent process,
	 * marking us as parent via retval[1].
	 */
	retval[0] = p2->p_pid;
	retval[1] = 0;
	return (0);
}
