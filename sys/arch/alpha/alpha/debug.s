/* $NetBSD: debug.s,v 1.10.108.2 2009/09/16 13:37:34 yamt Exp $ */

/*-
 * Copyright (c) 1999, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

__KERNEL_RCSID(6, "$NetBSD: debug.s,v 1.10.108.2 2009/09/16 13:37:34 yamt Exp $")

#include "opt_multiprocessor.h"
#include "opt_kgdb.h"

/*
 * Debugger glue.
 */

	.text
inc6:	.stabs	__FILE__,132,0,0,inc6; .loc	1 __LINE__

/*
 * Debugger stack.
 */
#define	DEBUG_STACK_SIZE	8192
BSS(debug_stack_bottom, DEBUG_STACK_SIZE)

#define debug_stack_top		(debug_stack_bottom + DEBUG_STACK_SIZE)

/*
 * alpha_debug:
 *
 *	Single debugger entry point, handling the housekeeping
 *	chores we need to deal with.
 *
 *	Arguments are:
 *
 *		a0	a0 from trap
 *		a1	a1 from trap
 *		a2	a2 from trap
 *		a3	kernel trap entry point
 *		a4	frame pointer
 */
NESTED_NOPROFILE(alpha_debug, 5, 32, ra, IM_RA|IM_S0, 0)
	br	pv, 1f
1:	LDGP(pv)
	lda	t0, FRAME_SIZE*8(a4)	/* what would sp have been? */
	stq	t0, FRAME_SP*8(a4)	/* belatedly save sp for ddb view */
	lda	sp, -32(sp)		/* set up stack frame */
	stq	ra, (32-8)(sp)		/* save ra */
	stq	s0, (32-16)(sp)		/* save s0 */

	/* Remember our current stack pointer. */
	mov	sp, s0

#if defined(MULTIPROCESSOR)
	/* Pause all other CPUs. */
	ldiq	a0, 1
	CALL(cpu_pause_resume_all)
#endif

	/*
	 * Switch to the debug stack if we're not on it already.
	 */
	lda	t0, debug_stack_bottom
	cmpule	sp, t0, t1		/* sp <= debug_stack_bottom */
	bne	t1, 2f			/* yes, switch now */

	lda	t0, debug_stack_top
	cmpule	t0, sp, t1		/* debug_stack_top <= sp? */
	bne	t1, 3f			/* yes, we're on the debug stack */

2:	lda	sp, debug_stack_top	/* sp <- debug_stack_top */

3:	/* Dispatch to the debugger - arguments are already in place. */
#if defined(KGDB)
	mov	a3, a0			/* a0 == entry (trap type) */
	mov	a4, a1			/* a1 == frame pointer */
	CALL(kgdb_trap)
	br	9f
#endif
#if defined(DDB)
	CALL(ddb_trap)
	br	9f
#endif
9:	/* Debugger return value in v0; switch back to our previous stack. */
	mov	s0, sp

#if defined(MULTIPROCESSOR)
	mov	v0, s0

	/* Resume all other CPUs. */
	mov	zero, a0
	CALL(cpu_pause_resume_all)

	mov	s0, v0
#endif

	ldq	ra, (32-8)(sp)		/* restore ra */
	ldq	s0, (32-16)(sp)		/* restore s0 */
	lda	sp, 32(sp)		/* pop stack frame */
	RET
	END(alpha_debug)
