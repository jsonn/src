/*	$NetBSD: mcontext.h,v 1.3.2.3 2004/09/21 13:21:37 skrll Exp $	*/

/*
 * Copyright 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Steve C. Woodford for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SH5_MCONTEXT_H_
#define _SH5_MCONTEXT_H_

#ifdef _KERNEL_
#include <machine/frame.h>
#endif

/*
 * General register state
 *
 * See notes, below, about the layout of mcontext_t before changing this.
 */
#define	_NGREG		73	/* PC, USR, R0 -> R62, TR0 -> TR7 */

#define	_REG_PC		0
#define	_REG_USR	1
#define	_REG_R(n)	(2 + (n))
#define	_REG_TR(n)	(65 + (n))

#define	_REG_FP		_REG_R(14)
#define	_REG_SP		_REG_R(15)

#ifndef __ASSEMBLER__
typedef	__int64_t	__greg_t;
typedef __greg_t	__gregset_t[_NGREG];

/*
 * Floating point register state
 *
 * See notes, below, about the layout of mcontext_t before changing this.
 */
typedef float		__fpreg_single_t;
typedef double		__fpreg_double_t;

typedef struct {
	int		__fp_scr;
	int		__fp_pad;
	union {
		__fpreg_single_t __u_fp_single[64];
		__fpreg_double_t __u_fp_double[32];
	} __fpregs_u;
} __fpregset_t;
#define	__fp_single	__fpregs_u.__u_fp_single
#define	__fp_double	__fpregs_u.__u_fp_double

/*
 * SH5's mcontext_t structure.
 *
 * Note: This *exactly* matches the layout of SH5's "struct reg".
 * Please don't update one without making a similar change to the other.
 */
typedef struct {
	__gregset_t	__gregs;
	__fpregset_t	__fpregs;
} mcontext_t;

#define mcontext32_t mcontext_t
#define	cpu_getmcontext32	cpu_getmcontext
#define	cpu_setmcontext32	cpu_setmcontext

#endif 	/* !__ASSEMBLER__ */

/*
 * Note: no additional padding required in ucontext_t
 */

#define	_UC_UCONTEXT_ALIGN	(~0x7)

#define	_UC_MACHINE_SP(uc)	((uc)->uc_mcontext.__gregs[_REG_SP])
#define	_UC_MACHINE_PC(uc)	((uc)->uc_mcontext.__gregs[_REG_PC])
#define	_UC_MACHINE_INTRV(uc)	((uc)->uc_mcontext.__gregs[_REG_R(2)])

#define	_UC_MACHINE_SET_PC(uc, pc)	_UC_MACHINE_PC(uc) = (pc)

/*
 * Machine dependent uc_flags
 */
#define	_UC_SETSTACK		0x10000
#define	_UC_CLRSTACK		0x20000

#endif /* _SH5_MCONTEXT_H_ */
