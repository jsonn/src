/*	$NetBSD: freebsd_misc.c,v 1.19.2.3 2004/09/18 14:43:15 skrll Exp $	*/

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: freebsd_misc.c,v 1.19.2.3 2004/09/18 14:43:15 skrll Exp $");

#if defined(_KERNEL_OPT)
#include "opt_ntp.h"
#include "opt_ktrace.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <sys/sa.h>
#include <sys/syscallargs.h>

#include <compat/freebsd/freebsd_syscallargs.h>
#include <compat/common/compat_util.h>
#include <compat/freebsd/freebsd_rtprio.h>
#include <compat/freebsd/freebsd_timex.h>
#include <compat/freebsd/freebsd_signal.h>
#include <compat/freebsd/freebsd_mman.h>

int
freebsd_sys_msync(l, v, retval)
	struct lwp *l;
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
	return sys___msync13(l, &bma, retval);
}

int
freebsd_sys_mmap(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_mmap_args /* {
		syscallarg(caddr_t) addr;
		syscallarg(size_t) len;
		syscallarg(int) prot;
		syscallarg(int) flags;
		syscallarg(int) fd;
		syscallarg(long) pad;
		syscallarg(off_t) pos;
	} */ *uap = v;
	struct sys_mmap_args bma;
	int flags, prot, fd;
	off_t pos;

	prot = SCARG(uap, prot);
	flags = SCARG(uap, flags);
	fd = SCARG(uap, fd);
	pos = SCARG(uap, pos);

	/*
	 * If using MAP_STACK on FreeBSD:
	 *
	 * + fd has to be -1
	 * + prot must have read and write
	 * + MAP_STACK implies MAP_ANON
	 * + MAP_STACK implies offset of 0
	 */
	if (flags & FREEBSD_MAP_STACK) {
		if ((fd != -1)
		    ||((prot & (PROT_READ|PROT_WRITE))!=(PROT_READ|PROT_WRITE)))
			return (EINVAL);

		flags |= (MAP_ANON | MAP_FIXED);
		flags &= ~FREEBSD_MAP_STACK;

		pos = 0;
	}

	SCARG(&bma, addr) = SCARG(uap, addr);
	SCARG(&bma, len) = SCARG(uap, len);
	SCARG(&bma, prot) = prot;
	SCARG(&bma, flags) = flags;
	SCARG(&bma, fd) = fd;
	SCARG(&bma, pos) = pos;

	return sys_mmap(l, &bma, retval);
}

/* just a place holder */

int
freebsd_sys_rtprio(l, v, retval)
	struct lwp *l;
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

#ifdef NTP
int
freebsd_ntp_adjtime(l, v, retval)
	struct lwp *l;
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
#endif

int
freebsd_sys_sigaction4(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_sigaction4_args /* {
		syscallarg(int) signum;
		syscallarg(const struct freebsd_sigaction4 *) nsa;
		syscallarg(struct freebsd_sigaction4 *) osa;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	struct freebsd_sigaction4 nesa, oesa;
	struct sigaction nbsa, obsa;
	int error;

	if (SCARG(uap, nsa)) {
		error = copyin(SCARG(uap, nsa), &nesa, sizeof(nesa));
		if (error)
			return (error);
		nbsa.sa_handler = nesa.freebsd_sa_handler;
		nbsa.sa_mask    = nesa.freebsd_sa_mask;
		nbsa.sa_flags   = nesa.freebsd_sa_flags;
	}
	error = sigaction1(p, SCARG(uap, signum),
	    SCARG(uap, nsa) ? &nbsa : 0, SCARG(uap, osa) ? &obsa : 0,
	    NULL, 0);
	if (error)
		return (error);
	if (SCARG(uap, osa)) {
		oesa.freebsd_sa_handler = obsa.sa_handler;
		oesa.freebsd_sa_mask    = obsa.sa_mask;
		oesa.freebsd_sa_flags   = obsa.sa_flags;
		error = copyout(&oesa, SCARG(uap, osa), sizeof(oesa));
		if (error)
			return (error);
	}
	return (0);
}

int
freebsd_sys_utrace(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
#ifdef KTRACE
	struct freebsd_sys_utrace_args /* {
		syscallarg(void *) addr;
		syscallarg(size_t) len;
	} */ *uap = v;
	struct proc *p = l->l_proc;

	if (KTRPOINT(p, KTR_USER))
		ktruser(p, "FreeBSD utrace", SCARG(uap, addr), SCARG(uap, len),
			0);
	
	return (0);
#else
	return (ENOSYS);
#endif
}
