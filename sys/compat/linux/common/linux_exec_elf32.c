/*	$NetBSD: linux_exec_elf32.c,v 1.60.2.1 2002/12/18 01:05:49 gmcgarry Exp $	*/

/*-
 * Copyright (c) 1995, 1998, 2000, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas, Frank van der Linden, Eric Haszlakiewicz and
 * Emmanuel Dreyfus.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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
 * based on exec_aout.c, sunos_exec.c and svr4_exec.c
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: linux_exec_elf32.c,v 1.60.2.1 2002/12/18 01:05:49 gmcgarry Exp $");

#ifndef ELFSIZE
/* XXX should die */
#define	ELFSIZE		32
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/exec.h>
#include <sys/exec_elf.h>
#include <sys/stat.h>

#include <sys/mman.h>
#include <sys/syscallargs.h>

#include <machine/cpu.h>
#include <machine/reg.h>

#include <compat/linux/common/linux_types.h>
#include <compat/linux/common/linux_signal.h>
#include <compat/linux/common/linux_util.h>
#include <compat/linux/common/linux_exec.h>
#include <compat/linux/common/linux_machdep.h>

#include <compat/linux/linux_syscallargs.h>
#include <compat/linux/linux_syscall.h>

static int ELFNAME2(linux,signature) __P((struct proc *, struct exec_package *,
	Elf_Ehdr *, char *));
#ifdef LINUX_GCC_SIGNATURE
static int ELFNAME2(linux,gcc_signature) __P((struct proc *p,
	struct exec_package *, Elf_Ehdr *));
#endif
#ifdef LINUX_ATEXIT_SIGNATURE
static int ELFNAME2(linux,atexit_signature) __P((struct proc *p,
	struct exec_package *, Elf_Ehdr *));
#endif

#ifdef DEBUG_LINUX
#define DPRINTF(a)	uprintf a
#else
#define DPRINTF(a)
#endif

#ifdef LINUX_ATEXIT_SIGNATURE
/*
 * On the PowerPC, statically linked Linux binaries are not recognized
 * by linux_signature nor by linux_gcc_signature. Fortunately, thoses
 * binaries features a __libc_atexit ELF section. We therefore assume we
 * have a Linux binary if we find this section.
 */
static int
ELFNAME2(linux,atexit_signature)(p, epp, eh)
	struct proc *p;
	struct exec_package *epp;
	Elf_Ehdr *eh;
{
	size_t shsize;
	int	strndx;
	size_t i;
	static const char signature[] = "__libc_atexit";
	char* strtable;
	Elf_Shdr *sh;
	
	int error;

	/*
	 * load the section header table 
	 */
	shsize = eh->e_shnum * sizeof(Elf_Shdr);
	sh = (Elf_Shdr *) malloc(shsize, M_TEMP, M_WAITOK);
	error = exec_read_from(p, epp->ep_vp, eh->e_shoff, sh, shsize);
	if (error)
		goto out;

	/* 
	 * Now let's find the string table. If it does not exists, give up.
	 */
	strndx = (int)(eh->e_shstrndx);
	if (strndx == SHN_UNDEF) {
		error = ENOEXEC;
		goto out;
	}

	/*
	 * strndx is the index in section header table of the string table
	 * section get the whole string table in strtable, and then we get access to the names
	 * s->sh_name is the offset of the section name in strtable.
	 */
	strtable = malloc(sh[strndx].sh_size, M_TEMP, M_WAITOK);
	error = exec_read_from(p, epp->ep_vp, sh[strndx].sh_offset, strtable,
	    sh[strndx].sh_size);
	if (error)
		goto out;

	for (i = 0; i < eh->e_shnum; i++) {
		Elf_Shdr *s = &sh[i];
		if (!memcmp((void*)(&(strtable[s->sh_name])), signature, 
				sizeof(signature))) {
			DPRINTF(("linux_atexit_sig=%s\n",
			    &(strtable[s->sh_name])));
			error = 0;
			goto out;
		}
	}
	error = ENOEXEC;

out:
	free(sh, M_TEMP);
	free(strtable, M_TEMP);
	return (error);
}
#endif

#ifdef LINUX_GCC_SIGNATURE
/*
 * Take advantage of the fact that all the linux binaries are compiled
 * with gcc, and gcc sticks in the comment field a signature. Note that
 * on SVR4 binaries, the gcc signature will follow the OS name signature,
 * that will not be a problem. We don't bother to read in the string table,
 * but we check all the progbits headers.
 *
 * XXX This only works in the i386.  On the alpha (at least)
 * XXX we have the same gcc signature which incorrectly identifies
 * XXX NetBSD binaries as Linux.
 */
static int
ELFNAME2(linux,gcc_signature)(p, epp, eh)
	struct proc *p;
	struct exec_package *epp;
	Elf_Ehdr *eh;
{
	size_t shsize;
	size_t i;
	static const char signature[] = "\0GCC: (GNU) ";
	char buf[sizeof(signature) - 1];
	Elf_Shdr *sh;
	int error;

	shsize = eh->e_shnum * sizeof(Elf_Shdr);
	sh = (Elf_Shdr *) malloc(shsize, M_TEMP, M_WAITOK);
	error = exec_read_from(p, epp->ep_vp, eh->e_shoff, sh, shsize);
	if (error)
		goto out;

	for (i = 0; i < eh->e_shnum; i++) {
		Elf_Shdr *s = &sh[i];

		/*
		 * Identify candidates for the comment header;
		 * Header cannot have a load address, or flags and
		 * it must be large enough.
		 */
		if (s->sh_type != SHT_PROGBITS ||
		    s->sh_addr != 0 ||
		    s->sh_flags != 0 ||
		    s->sh_size < sizeof(signature) - 1)
			continue;

		error = exec_read_from(p, epp->ep_vp, s->sh_offset, buf,
		    sizeof(signature) - 1);
		if (error)
			continue;

		/*
		 * error is 0, if the signatures match we are done.
		 */
		DPRINTF(("linux_gcc_sig: sig=%s\n", buf));
		if (!memcmp(buf, signature, sizeof(signature) - 1)) {
			error = 0;
			goto out;
		}
	}
	error = ENOEXEC;

out:
	free(sh, M_TEMP);
	return (error);
}
#endif

static int
ELFNAME2(linux,signature)(p, epp, eh, itp)
	struct proc *p;
	struct exec_package *epp;
	Elf_Ehdr *eh;
	char *itp;
{
	size_t i;
	Elf_Phdr *ph;
	size_t phsize;
	int error;
	static const char linux[] = "Linux";

	if (eh->e_ident[EI_OSABI] == 3 ||
	    memcmp(&eh->e_ident[EI_ABIVERSION], linux, sizeof(linux)) == 0)
		return 0;

	phsize = eh->e_phnum * sizeof(Elf_Phdr);
	ph = (Elf_Phdr *)malloc(phsize, M_TEMP, M_WAITOK);
	error = exec_read_from(p, epp->ep_vp, eh->e_phoff, ph, phsize);
	if (error)
		goto out;

	for (i = 0; i < eh->e_phnum; i++) {
		Elf_Phdr *ephp = &ph[i];
		Elf_Nhdr *np;
		u_int32_t *abi;

		if (ephp->p_type != PT_NOTE ||
		    ephp->p_filesz > 1024 ||
		    ephp->p_filesz < sizeof(Elf_Nhdr) + 20)
			continue;

		np = (Elf_Nhdr *)malloc(ephp->p_filesz, M_TEMP, M_WAITOK);
		error = exec_read_from(p, epp->ep_vp, ephp->p_offset, np,
		    ephp->p_filesz);
		if (error)
			goto next;

		if (np->n_type != ELF_NOTE_TYPE_ABI_TAG ||
		    np->n_namesz != ELF_NOTE_ABI_NAMESZ ||
		    np->n_descsz != ELF_NOTE_ABI_DESCSZ ||
		    memcmp((caddr_t)(np + 1), ELF_NOTE_ABI_NAME,
		    ELF_NOTE_ABI_NAMESZ))
			goto next;

		/* Make sure the OS is Linux. */
		abi = (u_int32_t *)((caddr_t)np + sizeof(Elf_Nhdr) +
		    np->n_namesz);
		if (abi[0] == ELF_NOTE_ABI_OS_LINUX)
			error = 0;
		else
			error = ENOEXEC;
		free(np, M_TEMP);
		goto out;

	next:
		free(np, M_TEMP);
		continue;
	}

	/* Check for certain intepreter names. */
	if (itp[0]) {
		if (!strncmp(itp, "/lib/ld-linux", 13) ||
		    !strncmp(itp, "/lib/ld.so.", 11))
			error = 0;
		else
			error = ENOEXEC;
		goto out;
	}

	error = ENOEXEC;
out:
	free(ph, M_TEMP);
	return (error);
}

int
ELFNAME2(linux,probe)(p, epp, eh, itp, pos)
	struct proc *p;
	struct exec_package *epp;
	void *eh;
	char *itp;
	vaddr_t *pos;
{
	int error;

	if (((error = ELFNAME2(linux,signature)(p, epp, eh, itp)) != 0) &&
#ifdef LINUX_GCC_SIGNATURE
	    ((error = ELFNAME2(linux,gcc_signature)(p, epp, eh)) != 0) &&
#endif
#ifdef LINUX_ATEXIT_SIGNATURE
	    ((error = ELFNAME2(linux,atexit_signature)(p, epp, eh)) != 0) &&
#endif
	    1) 
			return error;

	if (itp[0]) {
		if ((error = emul_find_interp(p, epp->ep_esch->es_emul->e_path,
		    itp)))
			return (error);
	}
	*pos = ELF_NO_ADDR;
	DPRINTF(("linux_probe: returning 0\n"));
	return 0;
}

#ifndef LINUX_MACHDEP_ELF_COPYARGS
/*
 * Copy arguments onto the stack in the normal way, but add some
 * extra information in case of dynamic binding.
 */
int
ELFNAME2(linux,copyargs)(struct proc *p, struct exec_package *pack,
    struct ps_strings *arginfo, char **stackp, void *argp)
{
	size_t len;
	AuxInfo ai[LINUX_ELF_AUX_ENTRIES], *a;
	struct elf_args *ap;
	int error;
	struct vattr *vap;

	if ((error = copyargs(p, pack, arginfo, stackp, argp)) != 0)
		return error;

	a = ai;

	/*
	 * Push extra arguments used by glibc on the stack.
	 */

	a->a_type = AT_PAGESZ;
	a->a_v = PAGE_SIZE;
	a++;

	if ((ap = (struct elf_args *)pack->ep_emul_arg)) {

		a->a_type = AT_PHDR;
		a->a_v = ap->arg_phaddr;
		a++;

		a->a_type = AT_PHENT;
		a->a_v = ap->arg_phentsize;
		a++;

		a->a_type = AT_PHNUM;
		a->a_v = ap->arg_phnum;
		a++;

		a->a_type = AT_BASE;
		a->a_v = ap->arg_interp;
		a++;

		a->a_type = AT_FLAGS;
		a->a_v = 0;
		a++;

		a->a_type = AT_ENTRY;
		a->a_v = ap->arg_entry;
		a++;

		free(pack->ep_emul_arg, M_TEMP);
		pack->ep_emul_arg = NULL;
	}

	/* Linux-specific items */
	a->a_type = LINUX_AT_CLKTCK;
	a->a_v = hz;
	a++;

	vap = pack->ep_vap;

	a->a_type = LINUX_AT_UID;
	a->a_v = p->p_ucred->cr_ruid;
	a++;

	a->a_type = LINUX_AT_EUID;
	if (vap->va_mode & S_ISUID)
		a->a_v = vap->va_uid;
	else
		a->a_v = p->p_ucred->cr_uid;
	a++;

	a->a_type = LINUX_AT_GID;
	a->a_v = p->p_ucred->cr_rgid;
	a++;

	a->a_type = LINUX_AT_EGID;
	if (vap->va_mode & S_ISGID)
		a->a_v = vap->va_gid;
	else
		a->a_v = p->p_ucred->cr_gid;
	a++;

	a->a_type = AT_NULL;
	a->a_v = 0;
	a++;

	len = (a - ai) * sizeof(AuxInfo);
	if ((error = copyout(ai, *stackp, len)) != 0)
		return error;
	*stackp += len;

	return 0;
}
#endif /* !LINUX_MACHDEP_ELF_COPYARGS */
