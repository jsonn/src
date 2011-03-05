/*	$NetBSD: mach_exec.h,v 1.33.8.1 2011/03/05 20:52:53 rmind Exp $	 */

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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

#ifndef	_MACH_EXEC_H_
#define	_MACH_EXEC_H_

#include <uvm/uvm_extern.h>

#include <compat/mach/mach_types.h>
#include <compat/mach/mach_message.h>
#include <compat/mach/mach_port.h>
#include <compat/mach/mach_exception.h>


struct mach_emuldata {
	int med_inited;			/* Is this structure initialized? */
	int med_thpri;			/* Saved priority */
	LIST_HEAD(med_right, mach_right) med_right;
	krwlock_t med_rightlock;	/* process right list and lock */
	mach_port_t med_nextright;	/* next unused right */

	struct mach_port *med_bootstrap;/* task bootstrap port */
	struct mach_port *med_kernel;	/* task kernel port */
	struct mach_port *med_host;	/* task host port */
	struct mach_port *med_exc[MACH_EXC_MAX + 1];	/* Exception ports */

	int med_dirty_thid;		/* Thread id not yet initialized */
	int med_suspend;		/* Suspend semaphore */
	krwlock_t med_exclock;		/* Process exception handler lock */
};

struct mach_lwp_emuldata {
	struct mach_port *mle_kernel;	/* Thread's kernel port */
};

int exec_mach_copyargs(struct lwp *, struct exec_package *,
    struct ps_strings *, char **, void *);
int exec_mach_probe(const char **);
void mach_e_proc_init(struct proc *);
void mach_e_proc_exit(struct proc *);
void mach_e_proc_exec(struct proc *, struct exec_package *);
void mach_e_proc_fork(struct proc *, struct lwp *, int);
void mach_e_proc_fork1(struct proc *, struct lwp *, int);
void mach_e_lwp_fork(struct lwp *, struct lwp *);
void mach_e_lwp_exit(struct lwp *);

extern struct emul emul_mach;

#endif /* !_MACH_EXEC_H_ */
