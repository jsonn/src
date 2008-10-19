/*	$NetBSD: resourcevar.h,v 1.44.12.1 2008/10/19 22:18:09 haad Exp $	*/

/*
 * Copyright (c) 1991, 1993
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
 *	@(#)resourcevar.h	8.4 (Berkeley) 1/9/95
 */

#ifndef	_SYS_RESOURCEVAR_H_
#define	_SYS_RESOURCEVAR_H_

#include <sys/mutex.h>

/*
 * Kernel per-process accounting / statistics
 */
struct pstats {
#define	pstat_startzero	p_ru
	struct	rusage p_ru;		/* stats for this proc */
	struct	rusage p_cru;		/* sum of stats for reaped children */
#define	pstat_endzero	pstat_startcopy

#define	pstat_startcopy	p_timer
	struct	itimerval p_timer[3];	/* virtual-time timers */

	struct uprof {			/* profile arguments */
		char *	pr_base;	/* buffer base */
		size_t  pr_size;	/* buffer size */
		u_long	pr_off;		/* pc offset */
		u_int   pr_scale;	/* pc scaling */
		u_long	pr_addr;	/* temp storage for addr until AST */
		u_long	pr_ticks;	/* temp storage for ticks until AST */
	} p_prof;
#define	pstat_endcopy	p_start
	struct	timeval p_start;	/* starting time */
};

/*
 * Kernel shareable process resource limits.  Because this structure
 * is moderately large but changes infrequently, it is normally
 * shared copy-on-write after forks.  If a group of processes
 * ("threads") share modifications, the PL_SHAREMOD flag is set,
 * and a copy must be made for the child of a new fork that isn't
 * sharing modifications to the limits.
 *
 * The PL_xxx flags are never cleared, once either is set p->p_limit
 * will never be changed again.
 */
struct plimit {
	struct	rlimit pl_rlimit[RLIM_NLIMITS];
	char	*pl_corename;
#define	PL_SHAREMOD	0x01		/* modifications are shared */
#define	PL_WRITEABLE	0x02		/* private to this process */
	int	pl_flags;
	int	pl_refcnt;		/* number of references */
	kmutex_t pl_lock;		/* mutex for pl_refcnt */
	struct plimit *pl_sv_limit;	/* saved when PL_WRITEABLE set */
};

/* add user profiling from AST XXXSMP */
#define	ADDUPROF(p)							\
	do {								\
		struct proc *_p = l->l_proc;				\
		addupc_task(l,						\
		    (_p)->p_stats->p_prof.pr_addr,			\
		    (_p)->p_stats->p_prof.pr_ticks);			\
		(_p)->p_stats->p_prof.pr_ticks = 0;			\
	} while (/* CONSTCOND */ 0)

#ifdef _KERNEL
extern char defcorename[];

extern int security_setidcore_dump;
extern char security_setidcore_path[];
extern uid_t security_setidcore_owner;
extern gid_t security_setidcore_group;
extern mode_t security_setidcore_mode;

void	addupc_intr(struct lwp *, u_long);
void	addupc_task(struct lwp *, u_long, u_int);
void	calcru(struct proc *, struct timeval *, struct timeval *,
	    struct timeval *, struct timeval *);

struct plimit *lim_copy(struct plimit *lim);
void	lim_addref(struct plimit *lim);
void	lim_privatise(struct proc *p, bool set_shared);
void	limfree(struct plimit *);

void	resource_init(void);
void	ruadd(struct rusage *, struct rusage *);
void	rulwps(proc_t *, struct rusage *);
struct	pstats *pstatscopy(struct pstats *);
void 	pstatsfree(struct pstats *);
extern rlim_t maxdmap;
extern rlim_t maxsmap;
#endif
#endif	/* !_SYS_RESOURCEVAR_H_ */
