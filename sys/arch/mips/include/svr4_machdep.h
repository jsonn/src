/*	$NetBSD: svr4_machdep.h,v 1.3.32.1 2006/09/09 02:41:26 rpaulo Exp $ */

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Emmanuel Dreyfus.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

/* 
 * This does not implement COMPAT_SVR4 for MIPS yet. For now we only 
 * have enough definitions to get some svr4_* files needed by COMPAT_IRIX 
 * to build.
 */

#ifndef _MIPS_SVR4_MACHDEP_H_
#define _MIPS_SVR4_MACHDEP_H_

#include <sys/cdefs.h>
#include <sys/proc.h>

/* From Irix's sys/ucontext.h */

typedef unsigned int svr4_greg_t;
#if IRIX_ABIAPI
#define SVR4_NGREG   36
typedef svr4_greg_t svr4_gregset_t[SVR4_NGREG];
#else
typedef svr4_greg_t svr4_gregset_t[36];
#endif  /* _ABIAPI */

typedef struct svr4___fpregset {
	union {
		double		svr4___fp_dregs[16];
		float	  	svr4___fp_fregs[32];
		unsigned int    svr4___fp_regs[32];
	} svr4___fp_r;
	unsigned int    svr4___fp_csr;
	unsigned int    svr4___fp_pad;
} svr4_fpregset_t;

typedef struct svr4_mcontext {
	svr4_gregset_t       svr4___gregs;
	svr4_fpregset_t      svr4___fpregs;
} svr4_mcontext_t;

#endif /* _MIPS_SVR4_MACHDEP_H_ */
