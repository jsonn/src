/*	$NetBSD: mach_exec.c,v 1.2.4.8 2002/12/11 06:37:28 thorpej Exp $	 */

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mach_exec.c,v 1.2.4.8 2002/12/11 06:37:28 thorpej Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/exec.h>
#include <sys/exec_macho.h>
#include <sys/malloc.h>

#include <sys/syscall.h>

#include <uvm/uvm_extern.h>
#include <uvm/uvm_param.h>

#include <compat/mach/mach_types.h>
#include <compat/mach/mach_exec.h>

static void mach_e_proc_exec(struct proc *, struct exec_package *);
static void mach_e_proc_fork(struct proc *, struct proc *);
static void mach_e_proc_exit(struct proc *);

extern char sigcode[], esigcode[];
extern struct sysent sysent[];
#ifdef SYSCALL_DEBUG
extern const char * const syscallnames[];
#endif
#ifndef __HAVE_SYSCALL_INTERN
void syscall(void);
#else
void mach_syscall_intern(struct proc *);
#endif

const struct emul emul_mach = {
	"mach",
	"/emul/mach",
#ifndef __HAVE_MINIMAL_EMUL
	0,
	0,
	SYS_syscall,
	SYS_NSYSENT,
#endif
	sysent,
#ifdef SYSCALL_DEBUG
	syscallnames,
#else
	NULL,
#endif
	sendsig,
	trapsignal,
	sigcode,
	esigcode,
	setregs,
	mach_e_proc_exec,
	mach_e_proc_fork,
	mach_e_proc_exit,
#ifdef __HAVE_SYSCALL_INTERN
	mach_syscall_intern,
#else
	syscall,
#endif
	NULL,
	NULL,
};

/*
 * Copy arguments onto the stack in the normal way, but add some
 * extra information in case of dynamic binding.
 * XXX This needs a cleanup: it is not used anymore by the Darwin 
 * emulation, and it probably contains Darwin specific bits. 
 */
int
exec_mach_copyargs(p, pack, arginfo, stackp, argp)
	struct proc *p;
	struct exec_package *pack;
	struct ps_strings *arginfo;
	char **stackp;
	void *argp;
{
	struct exec_macho_emul_arg *emea;
	size_t len;
	size_t zero = 0;
	int pagelen = PAGE_SIZE;
	int error;
	
	emea = (struct exec_macho_emul_arg *)pack->ep_emul_arg;
	
	*stackp -= 16;

	if ((error = copyout(&pagelen, *stackp, sizeof(pagelen))) != 0) {
		DPRINTF(("mach: copyout pagelen failed\n"));
		return error;
	}
	*stackp += sizeof(pagelen);

	if ((error = copyargs(p, pack, arginfo, stackp, argp)) != 0) {
		DPRINTF(("mach: copyargs failed\n"));
		return error;
	}

	if ((error = copyout(&zero, *stackp, sizeof(zero))) != 0) {
		DPRINTF(("mach: copyout first zero failed\n"));
		return error;
	}
	*stackp += sizeof(zero);

	if ((error = copyoutstr(emea->filename, 
	    *stackp, MAXPATHLEN, &len)) != 0) {
		DPRINTF(("mach: copyout path failed\n"));
		return error;
	}
	*stackp += len + 1;

	/* We don't need this anymore */
	free(pack->ep_emul_arg, M_EXEC);
	pack->ep_emul_arg = NULL;

	len = len % sizeof(zero);
	if (len) {
		if ((error = copyout(&zero, *stackp, len)) != 0) {
			DPRINTF(("mach: zero align %d failed\n", len));
			return error;
		}
		*stackp += len;
	}

	if ((error = copyout(&zero, *stackp, sizeof(zero))) != 0) {
		DPRINTF(("mach: copyout second zero failed\n"));
		return error;
	}
	*stackp += sizeof(zero);

	return 0;
}

int
exec_mach_probe(path)
	char **path;
{
	*path = (char *)emul_mach.e_path;
	return 0;
}

static void 
mach_e_proc_exec(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	mach_e_proc_init(p, p->p_vmspace);

	return;
}

static void 
mach_e_proc_fork(p, parent)
	struct proc *p;
	struct proc *parent;
{
	struct mach_emuldata *med1;
	struct mach_emuldata *med2;

	p->p_emuldata = NULL;

	/* Use parent's vmspace because our vmspace may not be setup yet */
	mach_e_proc_init(p, parent->p_vmspace);

	med1 = p->p_emuldata;
	med2 = parent->p_emuldata;

	(void)memcpy(med1, med2, sizeof(struct mach_emuldata));

	return;
}

void 
mach_e_proc_init(p, vmspace)
	struct proc *p;
	struct vmspace *vmspace;
{
	struct mach_emuldata *med;

	if (!p->p_emuldata)
		p->p_emuldata = malloc(sizeof(struct mach_emuldata),
		    M_EMULDATA, M_WAITOK | M_ZERO);

	med = (struct mach_emuldata *)p->p_emuldata;
	med->med_p = 0;

	return;
}

static void 
mach_e_proc_exit(p)
	struct proc *p;
{
	free(p->p_emuldata, M_EMULDATA);
	p->p_emuldata = NULL;

	return;
}
