/*	$NetBSD: signal.h,v 1.3.4.2 2002/09/06 08:40:21 jdolecek Exp $	*/

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

/*
 * SH-5 Signal Data Structures.
 *
 * XXX: SH-5 Signals are still very much in a state of flux.
 */

#ifndef _SH5_SIGNAL_H_
#define	_SH5_SIGNAL_H_

#ifndef _LP64
typedef long long	sig_atomic_t;
#else
typedef long	sig_atomic_t;
#endif

#if !defined(_ANSI_SOURCE) && !defined(_POSIX_C_SOURCE) && \
    !defined(_XOPEN_SOURCE)

#include <machine/reg.h>

/*
 * Information pushed on stack when a signal is delivered.
 * This is used by the kernel to restore state following
 * execution of the signal handler.  It is also made available
 * to the handler to allow it to restore state properly if
 * a non-standard exit is performed.
 */

struct sigcontext {
	int	sc_onstack;		/* sigstack state to restore */
	int	__sc_mask13;		/* signal mask to restore (old style) */
	int	sc_fpstate;		/* saved fp state flags */
	int	sc_pad;
	struct reg sc_regs;		/* saved register state */
	sigset_t sc_mask;		/* signal mask to restore (new style) */
};

#endif /* !_ANSI_SOURCE && !_POSIX_C_SOURCE && !_XOPEN_SOURCE */
#endif /* !_SH5_SIGNAL_H_*/
