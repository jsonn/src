/*-
 * Copyright (c) 1984, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)ptrace.h	8.2 (Berkeley) 1/4/94
 *	$NetBSD: freebsd_ptrace.h,v 1.3.16.1 2007/09/03 14:31:59 yamt Exp $
 */

#ifndef	_FREEBSD_PTRACE_H_
#define	_FREEBSD_PTRACE_H_

#define	FREEBSD_PT_TRACE_ME	0	/* child declares it's being traced */
#define	FREEBSD_PT_READ_I	1	/* read word in child's I space */
#define	FREEBSD_PT_READ_D	2	/* read word in child's D space */
#define	FREEBSD_PT_READ_U	3	/* read word in child's user structure */
#define	FREEBSD_PT_WRITE_I	4	/* write word in child's I space */
#define	FREEBSD_PT_WRITE_D	5	/* write word in child's D space */
#define	FREEBSD_PT_WRITE_U	6	/* write word in child's user structure */
#define	FREEBSD_PT_CONTINUE	7	/* continue the child */
#define	FREEBSD_PT_KILL		8	/* kill the child process */
#define	FREEBSD_PT_STEP		9	/* single step the child */

#ifdef notdef
#define	FREEBSD_PT_ATTACH	10	/* trace some running process */
#define	FREEBSD_PT_DETACH	11	/* stop tracing a process */
#endif

#define	FREEBSD_PT_FIRSTMACH	32	/* for machine-specific requests */

void netbsd_to_freebsd_ptrace_regs __P((struct reg *, struct fpreg *,
					struct freebsd_ptrace_reg *));
void freebsd_to_netbsd_ptrace_regs __P((struct freebsd_ptrace_reg *,
					struct reg *, struct fpreg *));
int freebsd_ptrace_getregs __P((struct freebsd_ptrace_reg *, void *,
				register_t *));
int freebsd_ptrace_setregs __P((struct freebsd_ptrace_reg *, void *,
				int));

#endif	/* !_FREEBSD_PTRACE_H_ */
