/*	$NetBSD: sys_process.c,v 1.33.2.4 1994/09/25 05:46:15 cgd Exp $	*/

/*-
 * Copyright (c) 1994 Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	from: @(#)sys_process.c	8.1 (Berkeley) 6/10/93
 */

/*
 * References:
 *	(1) Bach's "The Design of the UNIX Operating System",
 *	(2) sys/miscfs/procfs from UCB's 4.4BSD-Lite distribution,
 *	(3) the "4.4BSD Programmer's Reference Manual" published
 *		by USENIX and O'Reilly & Associates.
 * The 4.4BSD PRM does a reasonably good job of documenting what the various
 * ptrace() requests should actually do, and its text is quoted several times
 * in this file.
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/ptrace.h>
#include <sys/uio.h>
#include <sys/user.h>

#include <machine/reg.h>

/* Macros to clear/set/test flags. */
#define	SET(t, f)	(t) |= (f)
#define	CLR(t, f)	(t) &= ~(f)
#define	ISSET(t, f)	((t) & (f))

/*
 * Process debugging system call.
 */
struct ptrace_args {
	int	req;
	pid_t	pid;
	caddr_t	addr;
	int	data;
};
ptrace(p, uap, retval)
	struct proc *p;
	struct ptrace_args *uap;
	int *retval;
{
	struct proc *t;				/* target process */
	struct uio uio;
	struct iovec iov;
	int error, step, write;

	/* "A foolish consistency..." XXX */
	if (uap->req == PT_TRACE_ME)
		t = p;
	else {
		/* Find the process we're supposed to be operating on. */
		if ((t = pfind(uap->pid)) == NULL)
			return (ESRCH);
	}

	/* Make sure we can operate on it. */
	switch (uap->req) {
	case  PT_TRACE_ME:
		/* Saying that you're being traced is always legal. */
		break;

	case  PT_ATTACH:
		/*
		 * You can't attach to a process if:
		 *	(1) it's the process that's doing the attaching,
		 */
		if (t->p_pid == p->p_pid)
			return (EINVAL);

		/*
		 *	(2) it's already being traced, or
		 */
		if (ISSET(t->p_flag, P_TRACED))
			return (EBUSY);

		/*
		 *	(3) it's not owned by you, or is set-id on exec
		 *	    (unless you're root).
		 */
		if ((t->p_cred->p_ruid != p->p_cred->p_ruid ||
			ISSET(t->p_flag, P_SUGID)) &&
		    (error = suser(p->p_ucred, &p->p_acflag)) != 0)
			return (error);
		break;

	case  PT_READ_I:
	case  PT_READ_D:
	case  PT_READ_U:
	case  PT_WRITE_I:
	case  PT_WRITE_D:
	case  PT_WRITE_U:
	case  PT_CONTINUE:
	case  PT_KILL:
	case  PT_DETACH:
#ifdef PT_STEP
	case  PT_STEP:
#endif
#ifdef PT_GETREGS
	case  PT_GETREGS:
#endif
#ifdef PT_SETREGS
	case  PT_SETREGS:
#endif
#ifdef PT_GETFPREGS
	case  PT_GETFPREGS:
#endif
#ifdef PT_SETFPREGS
	case  PT_SETFPREGS:
#endif
		/*
		 * You can't do what you want to the process if:
		 *	(1) It's not being traced at all,
		 */
		if (!ISSET(t->p_flag, P_TRACED))
			return (EPERM);

		/*
		 *	(2) it's not being traced by _you_, or
		 */
		if (t->p_pptr != p)
			return (EBUSY);

		/*
		 *	(3) it's not currently stopped.
		 */
		if (t->p_stat != SSTOP || !ISSET(t->p_flag, P_WAITED))
			return (EBUSY);
		break;

	default:			/* It was not a legal request. */
		return (EINVAL);
	}

	/* Do single-step fixup if needed. */
	FIX_SSTEP(t);

	/* Now do the operation. */
	step = write = 0;
	*retval = 0;

	switch (uap->req) {
	case  PT_TRACE_ME:
		/* Just set the trace flag. */
		SET(t->p_flag, P_TRACED);
		return (0);

	case  PT_WRITE_I:		/* XXX no seperate I and D spaces */
	case  PT_WRITE_D:
		write = 1;
	case  PT_READ_I:		/* XXX no seperate I and D spaces */
	case  PT_READ_D:
		/* write = 0 done above. */
		iov.iov_base = write ? (caddr_t)&uap->data : (caddr_t)retval;
		iov.iov_len = sizeof(int);
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_offset = (off_t)uap->addr;
		uio.uio_resid = sizeof(int);
		uio.uio_segflg = UIO_SYSSPACE;
		uio.uio_rw = write ? UIO_WRITE : UIO_READ;
		uio.uio_procp = p;
		return (procfs_domem(p, t, NULL, &uio));

	case  PT_READ_U:
		/*
		 * The 4.4BSD PRM says that only the first 512 bytes
		 * of the user area are accessible for reading.  This
		 * disagrees with common practice, and would render PT_READ_U
		 * almost worthless.  Additionally, we _always_ require
		 * that the address be int-aligned.
		 */
		if ((u_long)uap->addr > (ctob(UPAGES) - sizeof(int)) ||
#ifdef m68k /* XXX */
		    ((u_long)uap->addr & 1) != 0)
#else /* !m68k XXX */
		    ((u_long)uap->addr & (sizeof(int) - 1)) != 0)
#endif /* !m68k XXX */
			return (EINVAL);

		/*
		 * Fill in eproc in user area, because user.h says that
		 * it's valid for ptrace().  Do it the same way as coredump().
		 */
		bcopy(t, &t->p_addr->u_kproc.kp_proc, sizeof(struct proc));
		fill_eproc(t, &t->p_addr->u_kproc.kp_eproc);

		/* Finally, pull the appropriate int out of the user area. */
		*retval = *(int *)((caddr_t)t->p_addr + (u_long)uap->addr);
		return (0);

	case  PT_WRITE_U:
		/*
		 * Mostly the same as PT_READ_U, but write data instead of
		 * reading it.  Don't bother filling in the eproc, because
		 * it won't be used for anything anyway.
		 */
		if ((u_long)uap->addr > (ctob(UPAGES) - sizeof(int)) ||
#ifdef m68k /* XXX */
		    ((u_long)uap->addr & 1) != 0)
#else /* !m68k XXX */
		    ((u_long)uap->addr & (sizeof(int) - 1)) != 0)
#endif /* !m68k XXX */
			return (EINVAL);

		/* And write the data. */
		*(int *)((caddr_t)t->p_addr + (u_long)uap->addr) = uap->data;
		return (0);

#ifdef PT_STEP
	case  PT_STEP:
		/*
		 * From the 4.4BSD PRM:
		 * "Execution continues as in request PT_CONTINUE; however
		 * as soon as possible after execution of at least one
		 * instruction, execution stops again. [ ... ]"
		 */
		step = 1;
#endif
	case  PT_CONTINUE:
		/*
		 * From the 4.4BSD PRM:
		 * "The data argument is taken as a signal number and the
		 * child's execution continues at location addr as if it
		 * incurred that signal.  Nromally the signal number will
		 * be either 0 to indicate that the signal that caused the
		 * stop should be ignored, or that value fetched out of
		 * the process's image indicating which signal caused
		 * the stop.  If addr is (int *)1 then execution continues
		 * from where it stopped."
		 */
		/* step = 0 done above. */

		/* Check that uap->data is a valid signal number or zero. */
		if (uap->data < 0 || uap->data >= NSIG)
			return (EINVAL);

		/*
		 * Arrange for a single-step, if that's requested and possible.
		 */
		if (error = process_sstep(t, step))
			return (error);

		/* If the address paramter is not (int *)1, set the pc. */
		if ((int *)uap->addr != (int *)1)
			if (error = process_set_pc(t, uap->addr))
				return (error);

		/* Finally, deliver the requested signal (or none). */
sendsig:
		t->p_xstat = uap->data;
		setrunnable(t);
		return (0);

	case  PT_KILL:
		/* just send the process a KILL signal. */
		uap->data = SIGKILL;
		goto sendsig;	/* in PT_CONTINUE, above. */

	case  PT_ATTACH:
		/*
		 * As done in procfs:
		 * Go ahead and set the trace flag.
		 * Save the old parent (it's reset in
		 *   _DETACH, and also in kern_exit.c:wait4()
		 * Reparent the process so that the tracing
		 *   proc gets to see all the action.
		 * Stop the target.
		 */
		SET(t->p_flag, P_TRACED);
		t->p_xstat = 0;         /* XXX ? */
		if (t->p_pptr != p) {
			t->p_oppid = t->p_pptr->p_pid;
			proc_reparent(t, p);
		}
		psignal(t, SIGSTOP);
		return (0);

	case  PT_DETACH:
		/* Again, as done in procfs: */

#ifdef notdef /* not allowed, by checks above. */
		/* if not being traced, then this is a painless no-op */
		if (!ISSET(t->p_flag, P_TRACED))
			return (0);
#endif

		/* not being traced any more */
		CLR(t->p_flag, P_TRACED);

		/* give process back to original parent */
		if (t->p_oppid != t->p_pptr->p_pid) {
			struct proc *pp;

			pp = pfind(t->p_oppid);
			if (pp)
				proc_reparent(t, pp);
		}

		t->p_oppid = 0;
		CLR(t->p_flag, P_WAITED); /* XXX? */

		/* and deliver any signal requested by tracer. */
		if (t->p_stat == SSTOP) {
			t->p_xstat = uap->data;
				setrunnable(t);
		} else if (uap->data)
			psignal(t, uap->data);

		return (0);

#ifdef PT_SETREGS
	case  PT_SETREGS:
		write = 1;
#endif
#ifdef PT_GETREGS
	case  PT_GETREGS:
		/* write = 0 done above. */
#endif
#if defined(PT_SETREGS) || defined(PT_GETREGS)
		if (!procfs_validregs(t))
			return (EINVAL);
		else {
			iov.iov_base = uap->addr;
			iov.iov_len = sizeof(struct reg);
			uio.uio_iov = &iov;
			uio.uio_iovcnt = 1;
			uio.uio_offset = 0;
			uio.uio_resid = sizeof(struct reg);
			uio.uio_segflg = UIO_USERSPACE;
			uio.uio_rw = write ? UIO_WRITE : UIO_READ;
			uio.uio_procp = p;
			return (procfs_doregs(p, t, NULL, &uio));
		}
#endif

#ifdef PT_SETFPREGS
	case  PT_SETFPREGS:
		write = 1;
#endif
#ifdef PT_GETFPREGS
	case  PT_GETFPREGS:
		/* write = 0 done above. */
#endif
#if defined(PT_SETFPREGS) || defined(PT_GETFPREGS)
		if (!procfs_validfpregs(t))
			return (EINVAL);
		else {
			iov.iov_base = uap->addr;
			iov.iov_len = sizeof(struct fpreg);
			uio.uio_iov = &iov;
			uio.uio_iovcnt = 1;
			uio.uio_offset = 0;
			uio.uio_resid = sizeof(struct fpreg);
			uio.uio_segflg = UIO_USERSPACE;
			uio.uio_rw = write ? UIO_WRITE : UIO_READ;
			uio.uio_procp = p;
			return (procfs_dofpregs(p, t, NULL, &uio));
		}
#endif
	}

#ifdef DIAGNOSTIC
	panic("ptrace: impossible");
#endif
}

trace_req(a1)
	struct proc *a1;
{

	/* just return 1 to keep other parts of the system happy */
	return (1);
}
