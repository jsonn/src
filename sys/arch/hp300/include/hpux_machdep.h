/*	$NetBSD: hpux_machdep.h,v 1.16.2.2 2004/08/03 10:34:37 skrll Exp $	*/

/*-
 * Copyright (c) 1996, 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

#ifndef _MACHINE_HPUX_MACHDEP_H_
#define _MACHINE_HPUX_MACHDEP_H_

/*
 * Information pushed on stack when a signal is delivered.
 * This is used by the kernel to restore state following
 * execution of the signal handler.  It is also made available
 * to the handler to allow it to restore state properly if
 * a non-standard exit is performed.
 */
struct hpuxsigcontext {
	int	hsc_syscall;		/* ??? (syscall number?) */
	char	hsc_action;		/* ??? */
	char	hsc_pad1;
	char	hsc_pad2;
	char	hsc_onstack;		/* sigstack state to restore */
	int	hsc_mask;		/* signal mask to restore */
	int	hsc_sp;			/* sp to restore */
	short	hsc_ps;			/* psl to restore */
	int	hsc_pc;			/* pc to restore */

	/*
	 * The following are not actually used by HP-UX.  They exist
	 * for the convenience of the compatibility code.
	 */
	short	_hsc_pad;
	int	_hsc_ap;		/* pointer to hpuxsigstate */
};

#ifdef _KERNEL
struct exec_package;
struct exec_vmcmd;

int	hpux_cpu_makecmds __P((struct lwp *, struct exec_package *));
int	hpux_cpu_vmcmd __P((struct proc *, struct exec_vmcmd *));
int	hpux_cpu_sysconf_arch __P((void));
int	hpux_to_bsd_uoff __P((int *, int *, struct lwp *));

void	hpux_sendsig __P((const ksiginfo_t *, const sigset_t *));
void	hpux_setregs __P((struct lwp *, struct exec_package *,
	    u_long));
#endif /* _KERNEL */

#endif /* ! _MACHINE_HPUX_MACHDEP_H_ */
