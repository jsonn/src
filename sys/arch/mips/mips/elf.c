/*	$NetBSD: elf.c,v 1.2.4.2 1996/06/24 22:38:50 jtc Exp $	*/
/* from: NetBSD: exec_elf.c,v 1.3 1995/09/16 00:28:08 thorpej Exp 	*/

/*       mips elf shared-library support from Per Fogelstrom's OpenBSD code */

/*
 * Copyright (c) 1994 Christos Zoulas
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/exec.h>
#include <sys/exec_elf.h>
#include <sys/syscall.h>
#include <sys/signalvar.h>

#include <sys/mman.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_map.h>

#include <machine/cpu.h>
#include <machine/reg.h>

#ifdef COMPAT_LINUX
#include <compat/linux/linux_exec.h>
#endif

#ifdef COMPAT_SVR4
#include <compat/svr4/svr4_exec.h>
#endif

int (*elf_probe_funcs[])() = {
#ifdef COMPAT_SVR4
	svr4_elf_probe,
#endif
#ifdef COMPAT_LINUX
	linux_elf_probe
#endif
};

static int elf_set_segment __P((struct exec_package *, u_long, u_long,
	int));
static int elf_read_from __P((struct proc *, struct vnode *, u_long,
	caddr_t, int));
static void elf_load_psection __P((struct exec_vmcmd_set *,
	struct vnode *, Elf32_Phdr *, u_long *, u_long *, int *));

#define ELF_ALIGN(a, b) ((a) & ~((b) - 1))

/*
 * This is the basic elf emul.
 */

extern char sigcode[], esigcode[];
#ifdef SYSCALL_DEBUG  
extern char *syscallnames[];
#endif

struct emul emul_elf = {
	"netbsd",
	NULL,
	sendsig,
	SYS_syscall,
	SYS_MAXSYSCALL,
	sysent,
#ifdef SYSCALL_DEBUG
	syscallnames,
#else
	NULL,
#endif
	sizeof(AuxInfo) * 8,
	elf_copyargs,
	setregs, 
	sigcode,
	esigcode,
};

/*
 * Copy arguments onto the stack in the normal way, but add some
 * extra information in case of dynamic binding.
 */
void *
elf_copyargs(pack, arginfo, stack, argp)
	struct exec_package *pack;
	struct ps_strings *arginfo;
	void *stack;
	void *argp;
{
	char **cpp = stack;
	char *dp, *sp;
	size_t len;
	void *nullp = NULL;
	int argc = arginfo->ps_nargvstr;
	int envc = arginfo->ps_nenvstr;
	AuxInfo *a;
	struct elf_args *ap;

	if (copyout(&argc, cpp++, sizeof(argc)))
		return NULL;

	dp = (char *) (cpp + argc + envc + 2 + pack->ep_emul->e_arglen);
	sp = argp;

	/* XXX don't copy them out, remap them! */
	arginfo->ps_argvstr = cpp; /* remember location of argv for later */

	for (; --argc >= 0; sp += len, dp += len)
		if (copyout(&dp, cpp++, sizeof(dp)) ||
		    copyoutstr(sp, dp, ARG_MAX, &len))
			return NULL;

	if (copyout(&nullp, cpp++, sizeof(nullp)))
		return NULL;

	arginfo->ps_envstr = cpp; /* remember location of envp for later */

	for (; --envc >= 0; sp += len, dp += len)
		if (copyout(&dp, cpp++, sizeof(dp)) ||
		    copyoutstr(sp, dp, ARG_MAX, &len))
			return NULL;

	if (copyout(&nullp, cpp++, sizeof(nullp)))
		return NULL;

	/*
	 * Push extra arguments on the stack needed by dynamically
	 * linked binaries
	 */
	a = (AuxInfo *) cpp;
	if ((ap = (struct elf_args *) pack->ep_emul_arg)) {

		a->au_id = AUX_phdr;
		a->au_v = ap->arg_phaddr;
		a++;

		a->au_id = AUX_phent;
		a->au_v = ap->arg_phentsize;
		a++;

		a->au_id = AUX_phnum;
		a->au_v = ap->arg_phnum;
		a++;

		a->au_id = AUX_pagesz;
		a->au_v = NBPG;
		a++;

		a->au_id = AUX_base;
		a->au_v = ap->arg_interp;
		a++;

		a->au_id = AUX_flags;
		a->au_v = 0;
		a++;

		a->au_id = AUX_entry;
		a->au_v = ap->arg_entry;
		a++;

		a->au_id = AUX_null;
		a->au_v = 0;
		a++;

		free((char *) ap, M_TEMP);
	}
	return a;
}

/*
 * elf_check_header():
 *
 * Check header for validity; return 0 of ok ENOEXEC if error
 *
 * XXX machine type needs to be moved to <machine/param.h> so
 * just one comparison can be done. Unfortunately, there is both
 * em_486 and em_386, so this would not work on the i386.
 */
int
elf_check_header(eh, type)
	Elf32_Ehdr *eh;
	int type;
{

	if (bcmp(eh->e_ident, Elf32_e_ident, Elf32_e_siz) != 0)
		return ENOEXEC;

	switch (eh->e_machine) {
	/* XXX */
#ifdef i386
	case Elf32_em_386:
	case Elf32_em_486:
#endif
#ifdef sparc
	case Elf32_em_sparc:
#endif
#ifdef mips
	case Elf32_em_mips:
#endif
		break;

	default:
		return ENOEXEC;
	}

	if (eh->e_type != type)
		return ENOEXEC;

	return 0;
}

/*
 * elf_load_psection():
 * 
 * Load a psection at the appropriate address
 */
static void
elf_load_psection(vcset, vp, ph, addr, size, prot)
	struct exec_vmcmd_set *vcset;
	struct vnode *vp;
	Elf32_Phdr *ph;
	u_long *addr;
	u_long *size;
	int *prot;
{
	u_long uaddr, msize, psize, rm, rf;
	long diff, offset;

	/*
         * If the user specified an address, then we load there.
         */
	if (*addr != ELF32_NO_ADDR) {
		if (ph->p_align > 1) {
			*addr = ELF_ALIGN(*addr + ph->p_align, ph->p_align);
			uaddr = ELF_ALIGN(ph->p_vaddr, ph->p_align);
		} else
			uaddr = ph->p_vaddr;
		diff = ph->p_vaddr - uaddr;
	} else {
		*addr = uaddr = ph->p_vaddr;
		if (ph->p_align > 1)
			*addr = ELF_ALIGN(uaddr, ph->p_align);
		diff = uaddr - *addr;
	}

	*prot |= (ph->p_flags & Elf32_pf_r) ? VM_PROT_READ : 0;
	*prot |= (ph->p_flags & Elf32_pf_w) ? VM_PROT_WRITE : 0;
	*prot |= (ph->p_flags & Elf32_pf_x) ? VM_PROT_EXECUTE : 0;

	offset = ph->p_offset - diff;
	*size = ph->p_filesz + diff;
	msize = ph->p_memsz + diff;
	psize = round_page(*size);

	if(ph->p_flags & Elf32_pf_w) {
		psize = trunc_page(*size);

#ifdef ELF_DEBUG
/*XXX*/		printf("mi elf: NEW_VNCMD len %x va %x off %x prot %x\n",
			psize, *addr, offset, *prot);
#endif	/*ELF_DEBUG*/

		NEW_VMCMD(vcset, vmcmd_map_pagedvn, psize, *addr, vp, offset, *prot);
		if(psize != *size) {

			NEW_VMCMD(vcset, vmcmd_map_readvn, *size - psize, *addr + psize, vp, offset + psize, *prot);

#ifdef ELF_DEBUG
/*XXX*/		printf("mi elf: NEW_VNCMD, size!=psize,  len %x va %x off %x prot %x\n",

			*size - psize, *addr + psize, offset + psize, *prot);
#endif	/*ELF_DEBUG*/
		}
	}
	else {
		NEW_VMCMD(vcset, vmcmd_map_pagedvn, psize, *addr, vp, offset, *prot);
	}

	/*
         * Check if we need to extend the size of the segment
         */
	rm = round_page(*addr + msize);
	rf = round_page(*addr + *size);

	if (rm != rf) {

#ifdef ELFDEBUG
/*XXX*/		printf("mi elf: rounding VNCMD len %x va %x off %x prot %x\n",
			rm - rf, rf, offset, *prot);
#endif /*ELF_DEBUG*/
		NEW_VMCMD(vcset, vmcmd_map_zero, rm - rf, rf, NULLVP, 0, *prot);
		*size = msize;
	}
}

/*
 * elf_set_segment():
 *
 * Decide if the segment is text or data, depending on the protection
 * and set it appropriately
 */
static int
elf_set_segment(epp, vaddr, size, prot)
	struct exec_package *epp;
	u_long vaddr;
	u_long size;
	int prot;
{
	/*
         * Kludge: Unfortunately the current implementation of
         * exec package assumes a single text and data segment.
         * In Elf we can have more, but here we limit ourselves
         * to two and hope :-(
         * We also assume that the text is r-x, and data is rwx or rw-.
         */
	switch (prot) {
	case (VM_PROT_READ | VM_PROT_EXECUTE):
		if (epp->ep_tsize != ELF32_NO_ADDR)
			return ENOEXEC;
		epp->ep_taddr = vaddr;
		epp->ep_tsize = size;
		break;

	case (VM_PROT_READ | VM_PROT_WRITE):
	case (VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE):
		if (epp->ep_dsize != ELF32_NO_ADDR)
		  {
		  /* the following fix from christos does not work  */
#if 0
			long diff;
			/*
			 * Support for sbss extra segment, by extending
			 * the data segment appropriately
			 */
/*XXX DEBUG*/	printf("elf_set_segment(): prot %x, bad rw size  %x\n",
			prot, epp->ep_dsize);

			if (epp->ep_daddr > vaddr)	
				epp->ep_daddr = vaddr;

			diff = (vaddr + size) - (epp->ep_daddr + epp->ep_dsize);
			if (diff > 0)
			    epp->ep_dsize += diff;

			return 0;
#else
			return ENOEXEC;
#endif
		}
		epp->ep_daddr = vaddr;
		epp->ep_dsize = size;
		break;

	default:
		return ENOEXEC;
	}
	return 0;
}

/*
 * elf_read_from():
 *
 *	Read from vnode into buffer at offset.
 */
static int
elf_read_from(p, vp, off, buf, size)
	struct vnode *vp;
	u_long off;
	struct proc *p;
	caddr_t buf;
	int size;
{
	int error;
	int resid;

	if ((error = vn_rdwr(UIO_READ, vp, buf, size,
			     off, UIO_SYSSPACE, IO_NODELOCKED, p->p_ucred,
			     &resid, p)) != 0)
		return error;
	/*
         * See if we got all of it
         */
	if (resid != 0)
		return error;
	return 0;
}

/*
 * elf_load_file():
 *
 * Load a file (interpreter/library) pointed to by path
 * [stolen from coff_load_shlib()]. Made slightly generic
 * so it might be used externally.
 */
int
elf_load_file(p, path, vcset, entry, ap, last)
	struct proc *p;
	char *path;
	struct exec_vmcmd_set *vcset;
	u_long *entry;
	struct elf_args	*ap;
	u_long *last;
{
	int error, i;
	struct nameidata nd;
	Elf32_Ehdr eh;
	Elf32_Phdr *ph = NULL;
	u_long phsize;
	char *bp = NULL;
	u_long addr = *last;

	bp = path;
	/*
         * 1. open file
         * 2. read filehdr
         * 3. map text, data, and bss out of it using VM_*
         */
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, path, p);
	if ((error = namei(&nd)) != 0) {
		return error;
	}
	if ((error = elf_read_from(p, nd.ni_vp, 0, (caddr_t) &eh,
				    sizeof(eh))) != 0)
		goto bad;

	if ((error = elf_check_header(&eh, Elf32_et_dyn)) != 0)
		goto bad;

	phsize = eh.e_phnum * sizeof(Elf32_Phdr);
	ph = (Elf32_Phdr *) malloc(phsize, M_TEMP, M_WAITOK);

	if ((error = elf_read_from(p, nd.ni_vp, eh.e_phoff,
				    (caddr_t) ph, phsize)) != 0)
		goto bad;

	/*
         * Load all the necessary sections
         */
	for (i = 0; i < eh.e_phnum; i++) {
		u_long size = 0;
		int prot = 0;

		switch (ph[i].p_type) {
		case Elf32_pt_load:
			elf_load_psection(vcset, nd.ni_vp, &ph[i], &addr,
						&size, &prot);
			/* Assume that the text segment is r-x only */
			if ((prot & PROT_WRITE) == 0) {
				*entry = addr + eh.e_entry - ph[i].p_vaddr;
				ap->arg_interp = addr;
			}
			addr += size;
			break;

		case Elf32_pt_dynamic:
		case Elf32_pt_phdr:
		case Elf32_pt_note:
			break;

		default:
			break;
		}
	}

bad:
	if (ph != NULL)
		free((char *) ph, M_TEMP);

	*last = addr;
	vrele(nd.ni_vp);
	return error;
}

/*
 * exec_elf_makecmds(): Prepare an Elf binary's exec package
 *
 * First, set of the various offsets/lengths in the exec package.
 *
 * Then, mark the text image busy (so it can be demand paged) or error
 * out if this is not possible.  Finally, set up vmcmds for the
 * text, data, bss, and stack segments.
 *
 */
int
exec_elf_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	Elf32_Ehdr *eh = epp->ep_hdr;
	Elf32_Phdr *ph, *pp;
	int error, i, n;
	char interp[MAXPATHLEN];
	u_long pos = 0, phsize;

	if (epp->ep_hdrvalid < sizeof(Elf32_Ehdr))
		return ENOEXEC;

	if (elf_check_header(eh, Elf32_et_exec))
		return ENOEXEC;

	/*
         * check if vnode is in open for writing, because we want to
         * demand-page out of it.  if it is, don't do it, for various
         * reasons
         */
	if (epp->ep_vp->v_writecount != 0) {
#ifdef DIAGNOSTIC
		if (epp->ep_vp->v_flag & VTEXT)
			panic("exec: a VTEXT vnode has writecount != 0\n");
#endif
		return ETXTBSY;
	}
	/*
         * Allocate space to hold all the program headers, and read them
         * from the file
         */
	phsize = eh->e_phnum * sizeof(Elf32_Phdr);
	ph = (Elf32_Phdr *) malloc(phsize, M_TEMP, M_WAITOK);

	if ((error = elf_read_from(p, epp->ep_vp, eh->e_phoff,
				    (caddr_t) ph, phsize)) != 0)
		goto bad;

	epp->ep_tsize = ELF32_NO_ADDR;
	epp->ep_dsize = ELF32_NO_ADDR;

	interp[0] = '\0';

	for (i = 0; i < eh->e_phnum; i++) {
		pp = &ph[i];
		if (pp->p_type == Elf32_pt_interp) {
			if (pp->p_filesz >= sizeof(interp))
				goto bad;
			if ((error = elf_read_from(p, epp->ep_vp, pp->p_offset,
				      (caddr_t) interp, pp->p_filesz)) != 0)
				goto bad;
			break;
		}
	}

	/*
	 * OK, we want a slightly different twist of the
         * standard emulation package for "real" elf.
         */
	epp->ep_emul = &emul_elf;
	pos = ELF32_NO_ADDR;

	/*
	 * On the same architecture, we may be emulating different systems.
	 * See which one will accept this executable. This currently only
	 * applies to Linux and SVR4 on the i386.
	 *
	 * Probe functions would normally see if the interpreter (if any)
	 * exists. Emulation packages may possibly replace the interpreter in
	 * interp[] with a changed path (/emul/xxx/<path>), and also
	 * set the ep_emul field in the exec package structure.
	 */
	if ((n = sizeof elf_probe_funcs / sizeof elf_probe_funcs[0])) {
		error = ENOEXEC;
		for (i = 0; i < n && error; i++)
			error = elf_probe_funcs[i](p, epp, interp, &pos);

		if (error)
			goto bad;
	}

	/*
         * Load all the necessary sections
         */
	for (i = 0; i < eh->e_phnum; i++) {
		u_long  addr = ELF32_NO_ADDR, size = 0;
		int prot = 0;

		pp = &ph[i];

		switch (ph[i].p_type) {
		case Elf32_pt_load:
			elf_load_psection(&epp->ep_vmcmds, epp->ep_vp,
				&ph[i], &addr, &size, &prot);
			if ((error = elf_set_segment(epp, addr, size,
						      prot)) != 0) {
#ifdef ELF_DEBUG
			/*
			 * This fails on binaries made by the old
			 * NetBSD pre-1.1 ELF toolchain, which had
			 */
				printf("exec_elf_makecmds: set_segment failed\n");
#endif /*ELF_DEBUG*/
				goto bad;

			}
			break;

		case Elf32_pt_shlib:
			error = ENOEXEC;
			goto bad;

		case Elf32_pt_interp:
			/* Already did this one */
		case Elf32_pt_dynamic:
		case Elf32_pt_phdr:
		case Elf32_pt_note:
			break;

		default:
			/*
			 * Not fatal, we don't need to understand everything
			 * :-)
			 */
			break;
		}
	}

	/*
         * Check if we found a dynamically linked binary and arrange to load
         * it's interpreter
         */
	if (interp[0]) {
		struct elf_args *ap;

		ap = (struct elf_args *) malloc(sizeof(struct elf_args),
						 M_TEMP, M_WAITOK);
		if ((error = elf_load_file(p, interp, &epp->ep_vmcmds,
				&epp->ep_entry, ap, &pos)) != 0) {
			free((char *) ap, M_TEMP);
			goto bad;
		}
		/* Arrange to load the program headers. */
		pos = ELF_ALIGN(pos + NBPG, NBPG);
		ap->arg_phaddr = pos;
		NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn, phsize,
			  pos, epp->ep_vp, eh->e_phoff,
			  VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE);
		pos += phsize;

		ap->arg_phentsize = eh->e_phentsize;
		ap->arg_phnum = eh->e_phnum;
		ap->arg_entry = eh->e_entry;

		epp->ep_emul_arg = ap;
	} else
		epp->ep_entry = eh->e_entry;

	free((char *) ph, M_TEMP);
	epp->ep_vp->v_flag |= VTEXT;
	return exec_aout_setup_stack(p, epp);

bad:
	free((char *) ph, M_TEMP);
	kill_vmcmds(&epp->ep_vmcmds);
	return ENOEXEC;
}
