/* $NetBSD: linux_exec_powerpc.c,v 1.12.4.2 2004/06/19 04:40:13 grant Exp $ */

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
 *      This product includes software developed by the NetBSD
 *      Foundation, Inc. and its contributors.
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
 * From NetBSD's sys/compat/arch/alpha/linux_exec_alpha.c, with some 
 * powerpc add-ons (ifdef LINUX_SHIFT and LINUX_SP_WRAP).
 * 
 * This code is to be common to alpha and powerpc. If it works on alpha, it 
 * should be moved to common/linux_exec_elf32.c. Beware that it needs 
 * LINUX_ELF_AUX_ENTRIES in arch/<arch>/linux_exec.h to also be moved to common
 * 
 * Emmanuel Dreyfus <p99dreyf@criens.u-psud.fr>
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: linux_exec_powerpc.c,v 1.12.4.2 2004/06/19 04:40:13 grant Exp $");

#if defined (__alpha__)
#define ELFSIZE 64
#elif defined (__powerpc__)
#define ELFSIZE 32
#else
#error Unified linux_elf_{32|64}copyargs not tested for this platform 
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/exec.h>
#include <sys/exec_elf.h>
#include <sys/resourcevar.h>

#include <uvm/uvm_extern.h>

#include <compat/linux/common/linux_exec.h>

#ifdef LINUX_SP_WRAP
extern int linux_sp_wrap_start;
extern int linux_sp_wrap_end;
extern int linux_sp_wrap_entry;
#endif
/*
 * Alpha and PowerPC specific linux copyargs function.
 */
int
ELFNAME2(linux,copyargs)(p, pack, arginfo, stackp, argp)
	struct proc *p;
	struct exec_package *pack;
	struct ps_strings *arginfo;
	char **stackp;
	void *argp;
{
	size_t len;
	AuxInfo ai[LINUX_ELF_AUX_ENTRIES], *a;
	struct elf_args *ap;
#ifdef LINUX_SP_WRAP
	AuxInfo *prog_entry = NULL;
	char	linux_sp_wrap_code[LINUX_SP_WRAP];
	unsigned long*	cga;
#endif
	int error;

#ifdef LINUX_SHIFT
	/* 
	 * Seems that PowerPC Linux binaries expect argc to start on a 16 bytes
	 * aligned address. And we need one more 16 byte shift if it was already
	 * 16 bytes aligned,	
	 */
	*stackp = (char *)(((unsigned long)*stackp - 1) & ~LINUX_SHIFT);
#endif

	if ((error = copyargs(p, pack, arginfo, stackp, argp)) != 0)
		return error;

#ifdef LINUX_SHIFT
	/* 
	 * From Linux's arch/ppc/kernel/process.c:shove_aux_table(). GNU ld.so 
	 * expects the ELF auxiliary table to start on a 16 bytes boundary on
	 * the PowerPC.
	 */
	*stackp = (char *)(((unsigned long)(*stackp) + LINUX_SHIFT)
	    & ~LINUX_SHIFT);
#endif 

	memset(ai, 0, sizeof(AuxInfo) * LINUX_ELF_AUX_ENTRIES);

	a = ai;

	/*
	 * Push extra arguments on the stack needed by dynamically
	 * linked binaries.
	 */
	if ((ap = (struct elf_args *)pack->ep_emul_arg)) {
#ifdef LINUX_SP_WRAP
		memset(linux_sp_wrap_code, 0, LINUX_SP_WRAP);
		bcopy(&linux_sp_wrap_start, linux_sp_wrap_code, 
		    (unsigned long)(&linux_sp_wrap_end) 
		    - (unsigned long)(&linux_sp_wrap_start));
		(unsigned long)cga = ((unsigned long)linux_sp_wrap_code) 
		    + ((unsigned long)(&linux_sp_wrap_entry))
		    - ((unsigned long)(&linux_sp_wrap_start));	
		(*cga) = (unsigned long)(ap->arg_entry); 
#endif
#if 1
		/*
		 * The exec_package doesn't have a proc pointer and it's not
		 * exactly trivial to add one since the credentials are
		 * changing. XXX Linux uses curlwp's credentials.
		 * Why can't we use them too?
		 */
		a->a_type = LINUX_AT_EGID;
		a->a_v = p->p_ucred->cr_gid;
		a++;

		a->a_type = LINUX_AT_GID;
		a->a_v = p->p_cred->p_rgid;
		a++;

		a->a_type = LINUX_AT_EUID;
		a->a_v = p->p_ucred->cr_uid;
		a++;

		a->a_type = LINUX_AT_UID;
		a->a_v = p->p_cred->p_ruid;
		a++;
#endif

		a->a_type = AT_ENTRY;
		a->a_v = ap->arg_entry;
#ifdef LINUX_SP_WRAP
		prog_entry = a;
#endif
		a++;

		a->a_type = AT_FLAGS;
		a->a_v = 0;
		a++;

		a->a_type = AT_BASE;
		a->a_v = ap->arg_interp;
		a++;

		a->a_type = AT_PHNUM;
		a->a_v = ap->arg_phnum;
		a++;

		a->a_type = AT_PHENT;
		a->a_v = ap->arg_phentsize;
		a++;

		a->a_type = AT_PHDR;
		a->a_v = ap->arg_phaddr;
		a++;

		a->a_type = LINUX_AT_CLKTCK;
		a->a_v = LINUX_CLOCKS_PER_SEC;
		a++;

		a->a_type = AT_PAGESZ;
		a->a_v = PAGE_SIZE;
		a++;

		a->a_type = LINUX_AT_HWCAP;
		a->a_v = LINUX_ELF_HWCAP;
		a++;

		free((char *)ap, M_TEMP);
		pack->ep_emul_arg = NULL;
	}

	a->a_type = AT_NULL;
	a->a_v = 0;
	a++;

	len = (a - ai) * sizeof(AuxInfo);

#ifdef LINUX_SP_WRAP
	if (prog_entry != NULL) 
		prog_entry->a_v = (unsigned long)(*stackp) + len;
#endif

	if ((error = copyout(ai, *stackp, len)) != 0)
		return error;
	*stackp += len; 

#ifdef LINUX_SP_WRAP
	if (prog_entry != NULL) {
		if ((error = copyout(linux_sp_wrap_code, *stackp,
		    LINUX_SP_WRAP)) != 0)
			return error;
		*stackp += LINUX_SP_WRAP;
	}
#endif

	return 0;
}

/* 
 * This is copied from sys/kern/exec_subr.c:exec_setup_stack()
 * We need a Linux version only to avoid the non executable
 * mappings. They will probably break signal delivery on Linux,
 * and they surely break the stack fixup hack.
 */
int
linux_exec_setup_stack(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	u_long max_stack_size;
	u_long access_linear_min, access_size;
	u_long noaccess_linear_min, noaccess_size;

#ifndef	USRSTACK32
#define USRSTACK32	(0x00000000ffffffffL&~PGOFSET)
#endif

	if (epp->ep_flags & EXEC_32) {
		epp->ep_minsaddr = USRSTACK32;
		max_stack_size = MAXSSIZ;
	} else {
		epp->ep_minsaddr = USRSTACK;
		max_stack_size = MAXSSIZ;
	}
	epp->ep_maxsaddr = (u_long)STACK_GROW(epp->ep_minsaddr, 
		max_stack_size);
	epp->ep_ssize = p->p_rlimit[RLIMIT_STACK].rlim_cur;

	/*
	 * set up commands for stack.  note that this takes *two*, one to
	 * map the part of the stack which we can access, and one to map
	 * the part which we can't.
	 *
	 * arguably, it could be made into one, but that would require the
	 * addition of another mapping proc, which is unnecessary
	 */
	access_size = epp->ep_ssize;
	access_linear_min = (u_long)STACK_ALLOC(epp->ep_minsaddr, access_size);
	noaccess_size = max_stack_size - access_size;
	noaccess_linear_min = (u_long)STACK_ALLOC(STACK_GROW(epp->ep_minsaddr, 
	    access_size), noaccess_size);
	if (noaccess_size > 0) {
		NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero, noaccess_size,
		    noaccess_linear_min, NULL, 0, VM_PROT_NONE);
	}
	KASSERT(access_size > 0);
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero, access_size,
	    access_linear_min, NULL, 0, 
	    VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE);

	return 0;
}
