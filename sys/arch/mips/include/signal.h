/*	$NetBSD: signal.h,v 1.14.20.2 2003/01/16 03:14:55 thorpej Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 *	@(#)signal.h	8.1 (Berkeley) 6/10/93
 */

#ifndef	_MIPS_SIGNAL_H_
#define	_MIPS_SIGNAL_H_

#include <machine/cdefs.h>	/* for API selection */

#if !defined(__ASSEMBLER__)

/*
 * Machine-dependent signal definitions
 */

typedef int sig_atomic_t;

#if !defined(_ANSI_SOURCE) && !defined(_POSIX_C_SOURCE) && \
    !defined(_XOPEN_SOURCE)
/*
 * Information pushed on stack when a signal is delivered.
 * This is used by the kernel to restore state following
 * execution of the signal handler.  It is also made available
 * to the handler to allow it to restore state properly if
 * a non-standard exit is performed.
 *
 * sizeof(sigcontext) = 45 * sizeof(int) + 35 * sizeof(mips_reg_t)
 */
#if defined(__LIBC12_SOURCE__) || defined(_KERNEL)
struct sigcontext13 {
	int	sc_onstack;	/* sigstack state to restore */
	int	sc_mask;	/* signal mask to restore (old style) */
	mips_reg_t sc_pc;	/* pc at time of signal */
	mips_reg_t sc_regs[32];	/* processor regs 0 to 31 */
	mips_reg_t mullo, mulhi;/* mullo and mulhi registers... */
	int	sc_fpused;	/* fp has been used */
	int	sc_fpregs[33];	/* fp regs 0 to 31 and csr */
	int	sc_fpc_eir;	/* floating point exception instruction reg */
	int	sc_xxx[8];	/* XXX reserved */
};
#endif /* __LIBC12_SOURCE__ || _KERNEL */

struct sigcontext {
	int	sc_onstack;	/* sigstack state to restore */
	int	__sc_mask13;	/* signal mask to restore (old style) */
	mips_reg_t sc_pc;	/* pc at time of signal */
	mips_reg_t sc_regs[32];	/* processor regs 0 to 31 */
	mips_reg_t mullo, mulhi;/* mullo and mulhi registers... */
	int	sc_fpused;	/* fp has been used */
	int	sc_fpregs[33];	/* fp regs 0 to 31 and csr */
	int	sc_fpc_eir;	/* floating point exception instruction reg */
	int	sc_xxx[8];	/* XXX reserved */
	sigset_t sc_mask;	/* signal mask to restore (new style) */
};

/*
 * The following macros are used to convert from a ucontext to sigcontext,
 * and vice-versa.  This is for building a sigcontext to deliver to old-style
 * signal handlers, and converting back (in the event the handler modifies
 * the context).
 */
_MCONTEXT_TO_SIGCONTEXT(uc, sc)						\
do {									\
	(sc)->sc_pc = (uc)->uc_mcontext.__gregs[_REG_EPC];		\
	memcpy((sc)->sc_regs, (uc)->uc_mcontext.__gregs,		\
	    sizeof((sc)->sc_regs));					\
	(sc)->mullo = (uc)->uc_mcontext.__gregs[_REG_MDLO];		\
	(sc)->mulhi = (uc)->uc_mcontext.__gregs[_REG_MDHI];		\
									\
	if ((uc)->uc_flags & _UC_FPU) {					\
		memcpy((sc)->sc_fpregs,					\
		    (uc)->uc_mcontext.__fpregs.__fp_r.__fpregs32,	\
		    sizeof((uc)->uc_mcontext.__fpregs.__fp_r.__fpregs32)); \
		(sc)->sc_fpregs[32] =					\
		    (uc)->uc_mcontext.__fpregs.__fp_csr;		\
		(sc)->sc_fpc_eir = 0;	/* XXX */			\
		(sc)->sc_fpused = 1;					\
	} else								\
		(sc)->sc_fpused = 0;					\
} while (/*CONSTCOND*/0)

#define	_SIGCONTEXT_TO_MCONTEXT(sc, uc)					\
do {									\
	(uc)->uc_mcontext.__gregs[_REG_EPC] = (sc)->sc_pc;		\
	memcpy((uc)->uc_mcontext.__gregs, (sc)->sc_regs,		\
	    sizeof((sc)->sc_regs));					\
	(uc)->uc_mcontext.__gregs[_REG_MDLO] = (sc)->mullo;		\
	(uc)->uc_mcontext.__gregs[_REG_MDHI] = (sc)->mulhi;		\
									\
	if ((sc)->sc_fpused) {						\
		memcpy((uc)->uc_mcontext.__fpregs.__fp_r.__fpregs32,	\
		    (sc)->sc_fpregs,					\
		    sizeof((uc)->uc_mcontext.__fpregs.__fp_r.__fpregs32)); \
		(uc)->uc_mcontext.__fpregs.__fp_csr =			\
		    (sc)->sc_fpregs[32];				\
		(uc)->uc_flags |= _UC_FPU;				\
	} else								\
		(uc)->uc_flags &= ~_UC_FPU;				\
} while (/*CONSTCOND*/0)

#endif	/* !_ANSI_SOURCE && !_POSIX_C_SOURCE && !_XOPEN_SOURCE */

#endif	/* !_LANGUAGE_ASSEMBLY */
#if !defined(_KERNEL)
/*
 * Hard code these to make people think twice about breaking compatibility.
 * These macros are generated independently for the kernel.
 */
#if !defined(_MIPS_BSD_API) || _MIPS_BSD_API == _MIPS_BSD_API_LP32
#define _OFFSETOF_SC_REGS	12
#define _OFFSETOF_SC_FPREGS	152
#define _OFFSETOF_SC_MASK	320
#else
#define _OFFSETOF_SC_REGS	16
#define _OFFSETOF_SC_FPREGS	292
#define _OFFSETOF_SC_MASK	460
#endif
#endif	/* !_KERNEL */
#endif	/* !_MIPS_SIGNAL_H_ */
