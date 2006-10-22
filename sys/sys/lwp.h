/* 	$NetBSD: lwp.h,v 1.41.6.1 2006/10/22 06:07:47 yamt Exp $	*/

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

#ifndef _SYS_LWP_H_
#define _SYS_LWP_H_


#if defined(_KERNEL)
#include <machine/cpu.h>		/* curcpu() and cpu_info */
#endif
#include <machine/proc.h>		/* Machine-dependent proc substruct. */
#include <sys/queue.h>
#include <sys/callout.h>
#include <sys/specificdata.h>

struct	lwp {
	struct	lwp *l_forw;		/* Doubly-linked run/sleep queue. */
	struct	lwp *l_back;
	LIST_ENTRY(lwp) l_list;		/* Entry on list of all LWPs. */

	struct proc *l_proc;	/* Process with which we are associated. */

	LIST_ENTRY(lwp) l_sibling;	/* Entry on process's list of LWPs. */

	struct cpu_info * volatile l_cpu; /* CPU we're running on if
					       SONPROC */
	specificdata_reference
		l_specdataref;	/* subsystem lwp-specific data */

	int	l_flag;
	int	l_stat;
	lwpid_t	l_lid;		/* LWP identifier; local to process. */

#define l_startzero l_cred
	struct kauth_cred *l_cred;	/* Cached credentials */
	u_short	l_acflag;	/* Accounting flags */
	u_int	l_swtime;	/* Time swapped in or out. */
	u_int	l_slptime;	/* Time since last blocked. */

	volatile const void *l_wchan;	/* Sleep address. */
	struct callout l_tsleep_ch;	/* callout for tsleep */
	const char *l_wmesg;	/* Reason for sleep. */
	int	l_holdcnt;	/* If non-zero, don't swap. */
	void	*l_ctxlink;	/* uc_link {get,set}context */
	int	l_dupfd;	/* Sideways return value from cloning devices XXX */
	struct sadata_vp *l_savp; /* SA "virtual processor" */

	int	l_locks;       	/* DEBUG: lockmgr count of held locks */
	void	*l_private;	/* svr4-style lwp-private data */

#define l_endzero l_priority

#define l_startcopy l_priority

	u_char	l_priority;	/* Process priority. */
	u_char	l_usrpri;	/* User-priority based on p_cpu and p_nice. */

#define l_endcopy l_emuldata

	void	*l_emuldata;	/* kernel lwp-private data */

	struct	user *l_addr;	/* Kernel virtual addr of u-area (PROC ONLY). */
	struct	mdlwp l_md;	/* Any machine-dependent fields. */
};

#if !defined(USER_TO_UAREA)
#if !defined(UAREA_USER_OFFSET)
#define	UAREA_USER_OFFSET	0
#endif /* !defined(UAREA_USER_OFFSET) */
#define	USER_TO_UAREA(user)	((vaddr_t)(user) - UAREA_USER_OFFSET)
#define	UAREA_TO_USER(uarea)	((struct user *)((uarea) + UAREA_USER_OFFSET))
#endif /* !defined(UAREA_TO_USER) */

LIST_HEAD(lwplist, lwp);		/* a list of LWPs */

#ifdef _KERNEL
extern struct lwplist alllwp;		/* List of all LWPs. */

extern struct pool lwp_uc_pool;		/* memory pool for LWP startup args */

extern struct lwp lwp0;			/* LWP for proc0 */
#endif

/* These flags are kept in l_flag. [*] is shared with p_flag */
#define	L_INMEM		0x00000004 /* [*] Loaded into memory. */
#define	L_SELECT	0x00000040 /* [*] Selecting; wakeup/waiting danger. */
#define	L_SINTR		0x00000080 /* [*] Sleep is interruptible. */
#define	L_SA		0x00000400 /* [*] Scheduler activations LWP */
#define	L_SA_UPCALL	0x00200000 /* SA upcall is pending */
#define	L_SA_BLOCKING	0x00400000 /* Blocking in tsleep() */
#define	L_DETACHED	0x00800000 /* Won't be waited for. */
#define	L_CANCELLED	0x02000000 /* tsleep should not sleep */
#define	L_SA_PAGEFAULT	0x04000000 /* SA LWP in pagefault handler */
#define	L_TIMEOUT	0x08000000 /* Timing out during sleep. */
#define	L_SA_YIELD	0x10000000 /* LWP on VP is yielding */
#define	L_SA_IDLE	0x20000000 /* VP is idle */
#define	L_COWINPROGRESS	0x40000000 /* UFS: doing copy on write */
#define	L_SA_SWITCHING	0x80000000 /* SA LWP in context switch */

/*
 * Status values.
 *
 * A note about SRUN and SONPROC: SRUN indicates that a process is
 * runnable but *not* yet running, i.e. is on a run queue.  SONPROC
 * indicates that the process is actually executing on a CPU, i.e.
 * it is no longer on a run queue.
 */
#define	LSIDL	1		/* Process being created by fork. */
#define	LSRUN	2		/* Currently runnable. */
#define	LSSLEEP	3		/* Sleeping on an address. */
#define	LSSTOP	4		/* Process debugging or suspension. */
#define	LSZOMB	5		/* Awaiting collection by parent. */
#define	LSDEAD	6		/* Process is almost a zombie. */
#define	LSONPROC	7	/* Process is currently on a CPU. */
#define	LSSUSPENDED	8	/* Not running, not signalable. */

#ifdef _KERNEL
#define	PHOLD(l)							\
do {									\
	if ((l)->l_holdcnt++ == 0 && ((l)->l_flag & L_INMEM) == 0)	\
		uvm_swapin(l);						\
} while (/* CONSTCOND */ 0)
#define	PRELE(l)	(--(l)->l_holdcnt)

#define	LWP_CACHE_CREDS(l, p)						\
do {									\
	if ((l)->l_cred != (p)->p_cred)					\
		lwp_update_creds(l);					\
} while (/* CONSTCOND */ 0)

void	preempt (int);
int	mi_switch (struct lwp *, struct lwp *);
#ifndef remrunqueue
void	remrunqueue (struct lwp *);
#endif
void	resetpriority (struct lwp *);
void	setrunnable (struct lwp *);
#ifndef setrunqueue
void	setrunqueue (struct lwp *);
#endif
#ifndef nextrunqueue
struct lwp *nextrunqueue(void);
#endif
void	unsleep (struct lwp *);
#ifndef cpu_switch
int	cpu_switch (struct lwp *, struct lwp *);
#endif
#ifndef cpu_switchto
void	cpu_switchto (struct lwp *, struct lwp *);
#endif

int newlwp(struct lwp *, struct proc *, vaddr_t, int /* XXX boolean_t */, int,
    void *, size_t, void (*)(void *), void *, struct lwp **);

/* Flags for _lwp_wait1 */
#define LWPWAIT_EXITCONTROL	0x00000001
void	lwpinit(void);
int 	lwp_wait1(struct lwp *, lwpid_t, lwpid_t *, int);
void	lwp_continue(struct lwp *);
void	cpu_setfunc(struct lwp *, void (*)(void *), void *);
void	startlwp(void *);
void	upcallret(struct lwp *);
void	lwp_exit (struct lwp *);
void	lwp_exit2 (struct lwp *);
struct lwp *proc_representative_lwp(struct proc *);
__inline int lwp_suspend(struct lwp *, struct lwp *);
int	lwp_create1(struct lwp *, const void *, size_t, u_long, lwpid_t *);
void	lwp_update_creds(struct lwp *);

int	lwp_specific_key_create(specificdata_key_t *, specificdata_dtor_t);
void	lwp_specific_key_delete(specificdata_key_t);
void 	lwp_initspecific(struct lwp *);
void 	lwp_finispecific(struct lwp *);
void *	lwp_getspecific(specificdata_key_t);
void	lwp_setspecific(specificdata_key_t, void *);
#endif	/* _KERNEL */

/* Flags for _lwp_create(), as per Solaris. */

#define LWP_DETACHED    0x00000040
#define LWP_SUSPENDED   0x00000080
#define __LWP_ASLWP     0x00000100 /* XXX more icky signal semantics */

#endif	/* !_SYS_LWP_H_ */
