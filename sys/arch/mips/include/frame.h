/*	$NetBSD: frame.h,v 1.2.2.1 2004/08/03 10:37:39 skrll Exp $	*/

/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Wayne Knowles
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

#ifndef _MIPS_FRAME_H_
#define _MIPS_FRAME_H_

#ifndef _LOCORE

#include <sys/signal.h>
#include <sys/sa.h>

/*
 * Signal frame
 */
struct sigframe {
	struct	sigcontext sf_sc;	/* actual context */
};

/*
 * Scheduler activations upcall frame.  Pushed onto user stack before
 * calling an SA upcall.
 */

struct saframe {
	/* first 4 arguments passed in registers on entry to upcallcode */
	int		sa_type;	/* A0 */
	struct sa_t **	sa_sas;		/* A1 */
	int		sa_events;	/* A2 */
	int		sa_interrupted;	/* A3 */
	void *		sa_arg;
	sa_upcall_t	sa_upcall;
};

void *getframe(struct lwp *, int, int *);
#ifdef COMPAT_16
void sendsig_sigcontext(const ksiginfo_t *, const sigset_t *);
#endif

#endif /* _LOCORE */

#endif /* _MIPS_FRAME_H_ */
  
/* End of frame.h */
