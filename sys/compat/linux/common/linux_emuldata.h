/*	$NetBSD: linux_emuldata.h,v 1.14.40.2 2009/05/04 08:12:22 yamt Exp $	*/

/*-
 * Copyright (c) 1998,2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Eric Haszlakiewicz.
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

#include <compat/linux/common/linux_machdep.h> /* For LINUX_NPTL */
#include <compat/linux/common/linux_futex.h>

#ifndef _COMMON_LINUX_EMULDATA_H
#define _COMMON_LINUX_EMULDATA_H

/*
 * This is auxillary data the linux compat code
 * needs to do its work.  A pointer to it is
 * stored in the emuldata field of the proc
 * structure.
 */
struct linux_emuldata_shared {
	void *	p_break;	/* Processes' idea of break */
	int refs;
	pid_t group_pid;	/* PID of Linux process (group of threads) */
	/* List of Linux threads (NetBSD processes) in the Linux process */
	LIST_HEAD(, linux_emuldata) threads;
	int flags;		/* See below */
	int xstat;		/* Thread group exit code, for exit_group */
};

#define LINUX_LES_INEXITGROUP	0x1	/* thread group doing exit_group() */
#define LINUX_LES_USE_NPTL	0x2	/* Need to emulate NPTL threads */

struct linux_emuldata {
#if notyet
	sigset_t ps_siginfo;	/* Which signals have a RT handler */
#endif
	int	debugreg[8];	/* GDB information for ptrace - for use, */
				/* see ../arch/i386/linux_ptrace.c */
	struct linux_emuldata_shared *s;
#ifdef LINUX_NPTL
	void *parent_tidptr;	/* Used during clone() */
	void *child_tidptr;	/* Used during clone() */
	int clone_flags;	/* Used during clone() */
	void *clear_tid;	/* Own TID to clear on exit */
	void *set_tls;		/* Pointer to child TLS desc in user space */
#if 0
	int *child_set_tid;	/* in clone(): Child's TID to set on clone */
	int *child_clear_tid;	/* in clone(): Child's TID to clear on exit */
	int *set_tid;		/* in clone(): Own TID to set on clone */
	int flags;		/* See above */
#endif
#endif

	struct linux_robust_list_head *robust_futexes;

	/* List of Linux threads (NetBSD processes) in the Linux process */
	LIST_ENTRY(linux_emuldata) threads;
	struct proc *proc;	/* backpointer to struct proc */
};

#define LINUX_CHILD_QUIETEXIT	0x1	/* Child will have quietexit set */
#define LINUX_QUIETEXIT		0x2	/* Quiet exit (no zombie state) */

#endif /* !_COMMON_LINUX_EMULDATA_H */
