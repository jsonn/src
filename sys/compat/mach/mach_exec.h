/*	$NetBSD: mach_exec.h,v 1.2.4.6 2002/12/19 00:44:32 thorpej Exp $	 */

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

#ifndef	_MACH_EXEC_H_
#define	_MACH_EXEC_H_

#include <uvm/uvm_extern.h>

#include <compat/mach/mach_types.h>

struct mach_emuldata {
	mach_cproc_t med_p;		/* Thread id */
	int med_thpri;			/* Saved priority */
	/* 
	 * Lists for the receive, send ans send-once rights of
	 * this process. There is also a right list for all 
	 * process, which is protected by a lock. Theses lists
	 * are protected by the same global lock.
	 */
	LIST_HEAD(med_recv, mach_right) med_recv;
	LIST_HEAD(med_send, mach_right) med_send;
	LIST_HEAD(med_sendonce, mach_right) med_sendonce;
	struct mach_port *med_bootstrap;/* task bootstrap port */
	struct mach_port *med_kernel;	/* task kernel port */
	struct mach_port *med_host;	/* task host port */
};

int exec_mach_copyargs(struct proc *, struct exec_package *, 
    struct ps_strings *, char **, void *);
int exec_mach_probe(char **);
void mach_e_proc_init(struct proc *, struct vmspace *);

extern const struct emul emul_mach;

#endif /* !_MACH_EXEC_H_ */
