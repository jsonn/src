/*	$NetBSD: signal.h,v 1.4.30.3 2003/01/16 03:14:56 thorpej Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef	_POWERPC_SIGNAL_H_
#define	_POWERPC_SIGNAL_H_

typedef int sig_atomic_t;

#if !defined(_ANSI_SOURCE) && !defined(_POSIX_C_SOURCE) && \
    !defined(_XOPEN_SOURCE)
#include <machine/frame.h>

#if defined(__LIBC12_SOURCE__) || defined(_KERNEL)
struct sigcontext13 {
	int sc_onstack;			/* saved onstack flag */
	int sc_mask;			/* saved signal mask (old style) */
	struct trapframe sc_frame;	/* saved registers */
};
#endif /* __LIBC12_SOURCE__ || _KERNEL */

struct sigcontext {
	int sc_onstack;			/* saved onstack flag */
	int __sc_mask13;		/* saved signal mask (old style) */
	struct trapframe sc_frame;	/* saved registers */
	sigset_t sc_mask;		/* saved signal mask (new style) */
};

/*
 * The following macros are used to convert from a ucontext to sigcontext,
 * and vice-versa.  This is for building a sigcontext to deliver to old-style
 * signal handlers, and converting back (in the event the handler modifies
 * the context).
 */
#define	_MCONTEXT_TO_SIGCONTEXT(uc, sc)					\
do {									\
	memcpy((sc)->sc_frame.fixreg, &(uc)->uc_mcontext.__gregs.__r_r0,\
	    sizeof((sc)->sc_frame.fixreg));				\
	(sc)->sc_frame.lr     = (uc)->uc_mcontext.__gregs.__r_lr;	\
	(sc)->sc_frame.cr     = (uc)->uc_mcontext.__gregs.__r_cr;	\
	(sc)->sc_frame.xer    = (uc)->uc_mcontext.__gregs.__r_xer;	\
	(sc)->sc_frame.ctr    = (uc)->uc_mcontext.__gregs.__r_ctr;	\
	(sc)->sc_frame.srr0   = (uc)->uc_mcontext.__gregs.__r_pc;	\
	(sc)->sc_frame.srr1   = (uc)->uc_mcontext.__gregs.__r_msr;	\
	(sc)->sc_frame.dar    = 0;					\
	(sc)->sc_frame.dsisr  = 0;					\
	(sc)->sc_frame.exc    = 0;					\
	(sc)->sc_frame.vrsave = (uc)->uc_mcontext.__vrf.__vrsave;	\
} while (/*CONSTCOND*/0)

#define	_SIGCONTEXT_TO_MCONTEXT(sc, uc)					\
do {									\
	memcpy(&(uc)->uc_mcontext.__gregs.__r_r0, (sc)->sc_frame.fixreg,\
	    sizeof((sc)->sc_frame.fixreg));				\
	(uc)->uc_mcontext.__gregs.__r_lr  = (sc)->sc_frame.lr;		\
	(uc)->uc_mcontext.__gregs.__r_cr  = (sc)->sc_frame.cr;		\
	(uc)->uc_mcontext.__gregs.__r_xer = (sc)->sc_frame.xer;		\
	(uc)->uc_mcontext.__gregs.__r_ctr = (sc)->sc_frame.ctr;		\
	(uc)->uc_mcontext.__gregs.__r_pc  = (sc)->sc_frame.srr0;	\
	(uc)->uc_mcontext.__gregs.__r_msr = (sc)->sc_frame.srr1;	\
	(uc)->uc_mcontext.__vrf.__vrsave  = (sc)->sc_frame.vrsave;	\
} while (/*CONSTCOND*/0)

struct sigframe {
	struct sigcontext sf_sc;
};
#endif	/* !_ANSI_SOURCE && !_POSIX_C_SOURCE && !_XOPEN_SOURCE */
#endif	/* !_POWERPC_SIGNAL_H_ */
