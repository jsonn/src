/*	$NetBSD: pthread_md.h,v 1.1.2.4 2002/12/05 06:35:35 thorpej Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Nathan J. Williams.
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

#ifndef _LIB_PTHREAD_MIPS_MD_H
#define _LIB_PTHREAD_MIPS_MD_H

static __inline long
pthread__sp(void)
{
	long ret;

	__asm("move %0, $sp" : "=r" (ret));

	return ret;
}

#define pthread__uc_sp(ucp) ((ucp)->uc_mcontext.__gregs[_REG_SP])
#define pthread__uc_pc(ucp) ((ucp)->uc_mcontext.__gregs[_REG_EPC])

/*
 * Usable stack space below the ucontext_t.
 *    For a good time, see comments in pthread_switch.S and
 *    ../i386/pthread_switch.S about STACK_SWITCH.
 */
#define STACKSPACE	16		/* 4 integer valuses */

/*
 * Conversions between struct reg and struct mcontext. Used by
 * libpthread_dbg.  Note that in the "reg" structure, the indices
 * are the same as are used in the "frame" structure in the kernel.
 * These do NOT, in all cases, match the indices used in the
 * "mcontext" structure.
 *
 * Thankfully, the "fpreg" and mcontext have the same FP layout.
 */
#include <mips/regnum.h>

#define PTHREAD_UCONTEXT_TO_REG(reg, uc)				\
do {									\
	memcpy(&(reg)->r_regs[AST], &(uc)->uc_mcontext.__gregs[_REG_AT],\
	    sizeof(__greg_t) * 31);					\
	(reg)->r_regs[MULLO] = (uc)->uc_mcontext.__gregs[_REG_MDLO];	\
	(reg)->r_regs[MULHI] = (uc)->uc_mcontext.__gregs[_REG_MDHI];	\
	(reg)->r_regs[CAUSE] = (uc)->uc_mcontext.__gregs[_REG_CAUSE];	\
	(reg)->r_regs[PC] = (uc)->uc_mcontext.__gregs[_REG_EPC];	\
} while (/*CONSTCOND*/0)

#define PTHREAD_REG_TO_UCONTEXT(uc, reg)				\
do {									\
	memcpy(&(uc)->uc_mcontext.__gregs[_REG_AT], &(reg)->r_regs[AST],\
	    sizeof(__greg_t) * 31);					\
	(uc)->uc_mcontext.__gregs[_REG_MDLO] = (reg)->r_regs[MULLO];	\
	(uc)->uc_mcontext.__gregs[_REG_MDHI] = (reg)->r_regs[MULHI];	\
	(uc)->uc_mcontext.__gregs[_REG_CAUSE] = (reg)->r_regs[CAUSE];	\
	(uc)->uc_mcontext.__gregs[_REG_EPC] = (reg)->r_regs[PC];	\
									\
	(uc)->uc_flags = ((uc)->uc_flags | _UC_CPU) & ~_UC_USER;       	\
} while (/*CONSTCOND*/0)

#define PTHREAD_UCONTEXT_TO_FPREG(freg, uc)       			\
	memcpy((freg), &(uc)->uc_mcontext.__fpregs,			\
	    sizeof(struct fpreg))					\

#define PTHREAD_FPREG_TO_UCONTEXT(uc, freg)				\
do {						       	       		\
	memcpy(&(uc)->uc_mcontext.__fpregs, (freg),			\
	    sizeof(struct fpreg));					\
	(uc)->uc_flags = ((uc)->uc_flags | _UC_FPU) & ~_UC_USER;       	\
} while (/*CONSTCOND*/0)

#endif /* !_LIB_PTHREAD_MIPS_MD_H */
