/* $NetBSD: signal.h,v 1.5.24.1 2003/01/16 03:14:50 thorpej Exp $ */

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#ifndef _ALPHA_SIGNAL_H_
#define	_ALPHA_SIGNAL_H_

typedef long	sig_atomic_t;

#if !defined(_ANSI_SOURCE) && !defined(_POSIX_C_SOURCE) && \
    !defined(_XOPEN_SOURCE)
/*
 * Information pushed on stack when a signal is delivered.
 * This is used by the kernel to restore state following
 * execution of the signal handler.  It is also made available
 * to the handler to allow it to restore state properly if
 * a non-standard exit is performed.
 *
 * Note that sc_regs[] and sc_fpregs[]+sc_fpcr are inline
 * representations of 'struct reg' and 'struct fpreg', respectively.
 */
#if defined(__LIBC12_SOURCE__) || defined(_KERNEL)
struct sigcontext13 {
	long	sc_onstack;		/* sigstack state to restore */
	long	sc_mask;		/* signal mask to restore (old style) */
	long	sc_pc;			/* pc to restore */
	long	sc_ps;			/* ps to restore */
	unsigned long sc_regs[32];	/* integer register set (see above) */
#define	sc_sp	sc_regs[R_SP]
	long	sc_ownedfp;		/* fp has been used */
	unsigned long sc_fpregs[32];	/* FP register set (see above) */
	unsigned long sc_fpcr;		/* FP control register (see above) */
	unsigned long sc_fp_control;	/* FP software control word */
	long	sc_reserved[2];		/* XXX */
	long	sc_xxx[8];		/* XXX */
};
#endif /* __LIBC12_SOURCE__ || _KERNEL */

struct sigcontext {
	long	sc_onstack;		/* sigstack state to restore */
	long	__sc_mask13;		/* signal mask to restore (old style) */
	long	sc_pc;			/* pc to restore */
	long	sc_ps;			/* ps to restore */
	unsigned long sc_regs[32];	/* integer register set (see above) */
#define	sc_sp	sc_regs[R_SP]
	long	sc_ownedfp;		/* fp has been used */
	unsigned long sc_fpregs[32];	/* FP register set (see above) */
	unsigned long sc_fpcr;		/* FP control register (see above) */
	unsigned long sc_fp_control;	/* FP software control word */
	long	sc_reserved[2];		/* XXX */
	long	sc_xxx[8];		/* XXX */
	sigset_t sc_mask;		/* signal mask to restore (new style) */
};

/*
 * The following macros are used to convert from a ucontext to sigcontext,
 * and vice-versa.  This is for building a sigcontext to deliver to old-style
 * signal handlers, and converting back (in the event the handler modifies
 * the context).
 */
#define	_MCONTEXT_TO_SIGCONTEXT(uc, sc)					\
do {									\
	(sc)->sc_pc = (uc)->uc_mcontext.__gregs[_REG_PC];		\
	(sc)->sc_ps = (uc)->uc_mcontext.__gregs[_REG_PS];		\
	memcpy(&(sc)->sc_regs, &(uc)->uc_mcontext.__gregs,		\
	    31 * sizeof(unsigned long));				\
	if ((uc)->uc_flags & _UC_FPU) {					\
		(sc)->sc_ownedfp = 1;					\
		memcpy(&(sc)->sc_fpregs,				\
		    &(uc)->uc_mcontext.__fpregs.__fp_fr,		\
		    31 * sizeof(unsigned long));			\
		(sc)->sc_fpcr = (uc)->uc_mcontext.__fpregs.__fp_fpcr;	\
		/* XXX sc_fp_control */					\
	} else								\
		(sc)->sc_ownedfp = 0;					\
} while (/*CONSTCOND*/0)

#define	_SIGCONTEXT_TO_MCONTEXT(sc, uc)					\
do {									\
	(uc)->uc_mcontext.__gregs[_REG_PC] = (sc)->sc_pc;		\
	(uc)->uc_mcontext.__gregs[_REG_PS] = (sc)->sc_ps;		\
	memcpy(&(uc)->uc_mcontext.__gregs, &(sc)->sc_regs,		\
	    31 * sizeof(unsigned long));				\
	if ((sc)->sc_ownedfp) {						\
		memcpy(&(uc)->uc_mcontext.__fpregs.__fp_fr,		\
		    &(sc)->sc_fpregs, 31 * sizeof(unsigned long));	\
		(sc)->sc_fpcr = (uc)->uc_mcontext.__fpregs.__fp_fpcr;	\
		/* XXX sc_fp_control */					\
		(uc)->uc_flags |= _UC_FPU;				\
	}								\
} while (/*CONSTCOND*/0)

#endif /* !_ANSI_SOURCE && !_POSIX_C_SOURCE && !_XOPEN_SOURCE */
#endif /* !_ALPHA_SIGNAL_H_*/
