/*	$NetBSD: frame.h,v 1.15.2.5 2002/07/02 21:06:12 nathanw Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	@(#)frame.h	5.2 (Berkeley) 1/18/91
 */

#ifndef _I386_FRAME_H_
#define _I386_FRAME_H_

#include <sys/signal.h>
#include <sys/sa.h>

/*
 * System stack frames.
 */

/*
 * Exception/Trap Stack Frame
 */
struct trapframe {
	int	tf_gs;
	int	tf_fs;
	int	tf_es;
	int	tf_ds;
	int	tf_edi;
	int	tf_esi;
	int	tf_ebp;
	int	tf_ebx;
	int	tf_edx;
	int	tf_ecx;
	int	tf_eax;
	int	tf_trapno;
	/* below portion defined in 386 hardware */
	int	tf_err;
	int	tf_eip;
	int	tf_cs;
	int	tf_eflags;
	/* below used when transitting rings (e.g. user to kernel) */
	int	tf_esp;
	int	tf_ss;
	/* below used when switching out of VM86 mode */
	int	tf_vm86_es;
	int	tf_vm86_ds;
	int	tf_vm86_fs;
	int	tf_vm86_gs;
};

/*
 * Interrupt stack frame
 */
struct intrframe {
	int	if_ppl;
	int	if_gs;
	int	if_fs;
	int	if_es;
	int	if_ds;
	int	if_edi;
	int	if_esi;
	int	if_ebp;
	int	if_ebx;
	int	if_edx;
	int	if_ecx;
	int	if_eax;
	u_int32_t __if_trapno:32; /* for compat with trap frame - trapno */
	u_int32_t __if_err:32;	/* for compat with trap frame - err */
	/* below portion defined in 386 hardware */
	int	if_eip;
	int	if_cs;
	int	if_eflags;
	/* below only when transitting rings (e.g. user to kernel) */
	int	if_esp;
	int	if_ss;
};

/*
 * Stack frame inside cpu_switch()
 */
struct switchframe {
	int	sf_ppl;
	int	sf_edi;
	int	sf_esi;
	int	sf_ebx;
	int	sf_eip;
};

/*
 * Signal frame
 */
struct sigframe {
	int	sf_signum;
	int	sf_code;
	struct	sigcontext *sf_scp;
	sig_t	sf_handler;
	struct	sigcontext sf_sc;
};

/*
 * Scheduler activations upcall frame
 */
struct saframe {
	int		sa_type;
	struct sa_t**	sa_sas;
	int		sa_events;
	int		sa_interrupted;
	void*		sa_arg;
	sa_upcall_t	sa_upcall;
};

#endif  /* _I386_FRAME_H_ */
