/*	$NetBSD: freebsd_misc.c,v 1.3.2.1 1998/09/30 18:16:38 cgd Exp $	*/

/*
 * Copyright (c) 1995 Frank van der Linden
 * All rights reserved.
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
 *      This product includes software developed for the NetBSD Project
 *      by Frank van der Linden
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * FreeBSD compatibility module. Try to deal with various FreeBSD system calls.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mount.h>

#include <sys/syscallargs.h>

#include <compat/freebsd/freebsd_syscallargs.h>
#include <compat/freebsd/freebsd_util.h>
#include <compat/freebsd/freebsd_rtprio.h>
#include <compat/freebsd/freebsd_timex.h>

int
freebsd_sys_msync(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_msync_args /* {
		syscallarg(caddr_t) addr;
		syscallarg(size_t) len;
		syscallarg(int) flags;
	} */ *uap = v;
	struct sys___msync13_args bma;

	/*
	 * FreeBSD-2.0-RELEASE's msync(2) is compatible with NetBSD's.
	 * FreeBSD-2.0.5-RELEASE's msync(2) has addtional argument `flags',
	 * but syscall number is not changed. :-<
	 */
	SCARG(&bma, addr) = SCARG(uap, addr);
	SCARG(&bma, len) = SCARG(uap, len);
	SCARG(&bma, flags) = SCARG(uap, flags);
	return sys___msync13(p, &bma, retval);
}

/* just a place holder */

int
freebsd_sys_rtprio(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
#ifdef notyet
	struct freebsd_sys_rtprio_args /* {
		syscallarg(int) function;
		syscallarg(pid_t) pid;
		syscallarg(struct freebsd_rtprio *) rtp;
	} */ *uap = v;
#endif

	return ENOSYS;	/* XXX */
}

int
freebsd_ntp_adjtime(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
#ifdef notyet
	struct freebsd_ntp_adjtime_args /* {
		syscallarg(struct freebsd_timex *) tp;
	} */ *uap = v;
#endif

	return ENOSYS;	/* XXX */
}

int
freebsd_sys_issetugid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	/*
	 * Note: OpenBSD sets a P_SUGIDEXEC flag set at execve() time,
	 * we use P_SUGID because we consider changing the owners as
	 * "tainting" as well.
	 * This is significant for procs that start as root and "become"
	 * a user without an exec - programs cannot know *everything*
	 * that libc *might* have put in their data segment.
	 */
	*retval = (p->p_flag & P_SUGID) != 0;
	return 0;
}
