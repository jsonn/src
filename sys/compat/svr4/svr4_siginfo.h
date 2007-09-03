/*	$NetBSD: svr4_siginfo.h,v 1.4.22.1 2007/09/03 14:32:54 yamt Exp $	 */

/*-
 * Copyright (c) 1994 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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

#ifndef	_SVR4_SIGINFO_H_
#define	_SVR4_SIGINFO_H_

#define	SVR4_ILL_ILLOPC		1
#define	SVR4_ILL_ILLOPN		2
#define	SVR4_ILL_ILLADR		3
#define	SVR4_ILL_ILLTRP		4
#define	SVR4_ILL_PRVOPC		5
#define	SVR4_ILL_PRVREG		6
#define	SVR4_ILL_COPROC		7
#define	SVR4_ILL_BADSTK		8

#define	SVR4_FPE_INTDIV		1
#define	SVR4_FPE_INTOVF		2
#define	SVR4_FPE_FLTDIV		3
#define	SVR4_FPE_FLTOVF		4
#define	SVR4_FPE_FLTUND		5
#define	SVR4_FPE_FLTRES		6
#define	SVR4_FPE_FLTINV		7
#define SVR4_FPE_FLTSUB		8

#define	SVR4_SEGV_MAPERR	1
#define	SVR4_SEGV_ACCERR	2

#define	SVR4_BUS_ADRALN		1
#define	SVR4_BUS_ADRERR		2
#define	SVR4_BUS_OBJERR		3

#define SVR4_TRAP_BRKPT		1
#define SVR4_TRAP_TRACE		2

#define SVR4_POLL_IN		1
#define	SVR4_POLL_OUT		2
#define	SVR4_POLL_MSG		3
#define	SVR4_POLL_ERR		4
#define	SVR4_POLL_PRI		5

#define	SVR4_CLD_EXITED		1
#define	SVR4_CLD_KILLED		2
#define	SVR4_CLD_DUMPED		3
#define	SVR4_CLD_TRAPPED	4
#define	SVR4_CLD_STOPPED	5
#define	SVR4_CLD_CONTINUED	6

#define SVR4_EMT_TAGOVF		1

typedef union svr4_siginfo {
	char	si_pad[128];	/* Total size; for future expansion */
	struct {
		int				_signo;
		int				_code;
		int				_errno;
#ifdef _LP64
		/* In _LP64 the union starts on an 8-byte boundary. */
		int				_pad;
#endif
		union {
			struct {
				svr4_pid_t	_pid;
				svr4_clock_t	_utime;
				int		_status;
				svr4_clock_t	_stime;
			} _child;

			struct {
				void *		_addr;
				int		_trap;
			} _fault;
		} _reason;
	} _info;
} svr4_siginfo_t;

#define	si_signo	_info._signo
#define	si_code		_info._code
#define	si_errno	_info._errno

#define si_pid		_info._reason._child._pid
#define si_stime	_info._reason._child._stime
#define si_status	_info._reason._child._status
#define si_utime	_info._reason._child._utime

#define si_addr		_info._reason._fault._addr
#define si_trap		_info._reason._fault._trap

#endif /* !_SVR4_SIGINFO_H_ */
