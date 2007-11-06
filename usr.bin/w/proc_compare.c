/*	$NetBSD: proc_compare.c,v 1.13.24.1 2007/11/06 23:36:18 matt Exp $	*/

/*-
 * Copyright (c) 1990, 1993
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
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)proc_compare.c	8.2 (Berkeley) 9/23/93";
#else
__RCSID("$NetBSD: proc_compare.c,v 1.13.24.1 2007/11/06 23:36:18 matt Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/sysctl.h>

#include "extern.h"

/*
 * Returns 1 if p2 is "better" than p1
 *
 * The algorithm for picking the "interesting" process is thus:
 *
 *	1) Only foreground processes are eligible - implied.
 *	2) Runnable processes are favored over anything else.  The runner
 *	   with the highest CPU utilization is picked (p_pctcpu).  Ties are
 *	   broken by picking the highest pid.
 *	3) The sleeper with the shortest sleep time is next.  With ties,
 *	   we pick out just "short-term" sleepers (P_SINTR == 0).
 *	4) Further ties are broken by picking the highest pid.
 *
 * If you change this, be sure to consider making the change in the kernel
 * too (^T in kern/tty.c).
 *
 * TODO - consider whether pctcpu should be used.
 */

#define ISRUN(p)	(((p)->p_stat == LSRUN) || ((p)->p_stat == SIDL) || \
			 ((p)->p_stat == LSONPROC))
#define TESTAB(a, b)    ((a)<<1 | (b))
#define ONLYA   2
#define ONLYB   1
#define BOTH    3

int
proc_compare(struct kinfo_proc2 *p1, struct kinfo_proc2 *p2)
{

	if (p1 == NULL)
		return (1);
	/*
	 * see if at least one of them is runnable
	 */
	switch (TESTAB(ISRUN(p1), ISRUN(p2))) {
	case ONLYA:
		return (0);
	case ONLYB:
		return (1);
	case BOTH:
		/*
		 * tie - favor one with highest recent CPU utilization
		 */
		if (p2->p_pctcpu > p1->p_pctcpu)
			return (1);
		if (p1->p_pctcpu > p2->p_pctcpu)
			return (0);
		return (p2->p_pid > p1->p_pid);	/* tie - return highest pid */
	}
	/*
 	 * weed out zombies
	 */
	switch (TESTAB(p1->p_stat == SZOMB, p2->p_stat == SZOMB)) {
	case ONLYA:
		return (1);
	case ONLYB:
		return (0);
	case BOTH:
		return (p2->p_pid > p1->p_pid); /* tie - return highest pid */
	}
	/*
	 * pick the one with the smallest sleep time
	 */
	if (p2->p_slptime > p1->p_slptime)
		return (0);
	if (p1->p_slptime > p2->p_slptime)
		return (1);
	/*
	 * favor one sleeping in a non-interruptible sleep
	 */
	if (p1->p_flag & L_SINTR && (p2->p_flag & L_SINTR) == 0)
		return (1);
	if (p2->p_flag & L_SINTR && (p1->p_flag & L_SINTR) == 0)
		return (0);
	return (p2->p_pid > p1->p_pid);		/* tie - return highest pid */
}
