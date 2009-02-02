/*	$NetBSD: cpu.h,v 1.23.4.3 2009/02/02 03:19:32 snj Exp $	*/

/*-
 * Copyright (c) 2007 YAMAMOTO Takashi,
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _SYS_CPU_H_
#define _SYS_CPU_H_

#ifndef _LOCORE

#include <machine/cpu.h>

#include <sys/lwp.h>

struct cpu_info;

#ifndef cpu_idle
void cpu_idle(void);
#endif

/*
 * cpu_need_resched() must always be called with the target CPU
 * locked (via spc_lock() or another route), unless called locally.
 * If called locally, the caller need only be at IPL_SCHED.
 */
#ifndef cpu_need_resched
void cpu_need_resched(struct cpu_info *, int);
#endif

#ifndef cpu_did_resched
#define	cpu_did_resched(l)	/* nothing */
#endif

#ifndef CPU_INFO_ITERATOR
#define	CPU_INFO_ITERATOR		int
#define	CPU_INFO_FOREACH(cii, ci)	\
    (void)cii, ci = curcpu(); ci != NULL; ci = NULL
#endif

#ifndef CPU_IS_PRIMARY
#define	CPU_IS_PRIMARY(ci)	((void)ci, 1)
#endif

#ifdef __HAVE_MD_CPU_OFFLINE
void	cpu_offline_md(void);
#endif

lwp_t	*cpu_switchto(lwp_t *, lwp_t *, bool);
struct	cpu_info *cpu_lookup(u_int);
int	cpu_setstate(struct cpu_info *, bool);
bool	cpu_intr_p(void);
bool	cpu_kpreempt_enter(uintptr_t, int);
void	cpu_kpreempt_exit(uintptr_t);
bool	cpu_kpreempt_disabled(void);

CIRCLEQ_HEAD(cpuqueue, cpu_info);

extern kmutex_t cpu_lock;
extern u_int maxcpus;
extern struct cpuqueue cpu_queue;
  
static inline u_int
cpu_index(struct cpu_info *ci)
{
	return ci->ci_index;
}

#endif	/* !_LOCORE */

/* flags for cpu_need_resched */
#define	RESCHED_LAZY		0x01
#define	RESCHED_IMMED		0x02
#define	RESCHED_KPREEMPT	0x04

#endif	/* !_SYS_CPU_H_ */
