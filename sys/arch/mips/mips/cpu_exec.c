/*	$NetBSD: cpu_exec.c,v 1.19.4.1 1999/06/21 00:52:05 thorpej Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by Ralph
 * Campbell.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)machdep.c	8.3 (Berkeley) 1/12/94
 */

#include "opt_compat_netbsd.h"
#include "opt_compat_ultrix.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/exec.h>
#include <sys/resourcevar.h>
#include <vm/vm.h>

#include <sys/exec_ecoff.h>
#include <sys/exec_elf.h>
#ifdef COMPAT_09
#include <machine/bsd-aout.h>
#endif
#include <machine/reg.h>
#include <mips/regnum.h>			/* symbolic register indices */

#include <machine/elf.h>
/*XXX*/
int	exec_elf_mips32_makecmds __P((struct proc *, struct exec_package *));


/*
 * cpu_exec_aout_makecmds():
 *	cpu-dependent a.out format hook for execve().
 *
 * Determine of the given exec package refers to something which we
 * understand and, if so, set up the vmcmds for it.
 *
 */
int
cpu_exec_aout_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	int error;

	/* If COMPAT_09 is defined, allow loading of old-style 4.4bsd a.out
	   executables. */
#ifdef COMPAT_09
	struct bsd_aouthdr *hdr = (struct bsd_aouthdr *)epp -> ep_hdr;

	/* Only handle paged files (laziness). */
	if (hdr -> a_magic != BSD_ZMAGIC)
#endif
	{
		/* If that failed, try old NetBSD-1.1 elf format */
		error = mips_elf_makecmds (p, epp);
		return error;
	}



#ifdef COMPAT_09
	epp -> ep_taddr = 0x1000;
	epp -> ep_entry = hdr -> a_entry;
	epp -> ep_tsize = hdr -> a_text;
	epp -> ep_daddr = epp -> ep_taddr + hdr -> a_text;
	epp -> ep_dsize = hdr -> a_data + hdr -> a_bss;

	/*
	 * check if vnode is in open for writing, because we want to
	 * demand-page out of it.  if it is, don't do it, for various
	 * reasons
	 */
	if ((hdr -> a_text != 0 || hdr -> a_data != 0)
	    && epp->ep_vp->v_writecount != 0) {
#ifdef DIAGNOSTIC
		if (epp->ep_vp->v_flag & VTEXT)
			panic("exec: a VTEXT vnode has writecount != 0\n");
#endif
		return ETXTBSY;
	}
	epp->ep_vp->v_flag |= VTEXT;

	/* set up command for text segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_pagedvn, hdr -> a_text,
	    epp->ep_taddr, epp->ep_vp, 0, VM_PROT_READ|VM_PROT_EXECUTE);

	/* set up command for data segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_pagedvn, hdr -> a_data,
	    epp->ep_daddr, epp->ep_vp, hdr -> a_text,
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	/* set up command for bss segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero, hdr -> a_bss,
	    epp->ep_daddr + hdr -> a_data, NULLVP, 0,
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	return exec_aout_setup_stack(p, epp);
#endif
}

#ifdef COMPAT_ULTRIX
extern struct emul emul_ultrix;

void
cpu_exec_ecoff_setregs(p, epp, stack)
	struct proc *p;
	struct exec_package *epp;
	u_long stack;
{
	struct ecoff_exechdr *execp = (struct ecoff_exechdr *)epp->ep_hdr;
	struct frame *f = (struct frame *)p->p_md.md_regs;

	setregs(p, epp, stack);
	f->f_regs[GP] = (register_t)execp->a.gp_value;
}

/*
 * cpu_exec_ecoff_hook():
 *	cpu-dependent ECOFF format hook for execve().
 *
 * Do any machine-dependent diddling of the exec package when doing ECOFF.
 *
 */
int
cpu_exec_ecoff_hook(p, epp)
	struct proc *p;
	struct exec_package *epp;
{

	epp->ep_emul = &emul_ultrix;
	return 0;
}
#endif

/*
 * mips_elf_makecmds (p, epp)
 *
 * Test if an executable is a MIPS ELF executable.   If it is,
 * try to load it.
 */

int
mips_elf_makecmds (p, epp)
        struct proc *p;
        struct exec_package *epp;
{
	struct ehdr *ex = (struct ehdr *)epp -> ep_hdr;
	struct phdr ph;
	int i, error;
	size_t resid;

	/* Make sure we got enough data to check magic numbers... */
	if (epp -> ep_hdrvalid < sizeof (struct ehdr)) {
#ifdef DIAGNOSTIC
	    if (epp -> ep_hdrlen < sizeof (struct ehdr))
		printf ("mips_elf_makecmds: execsw hdrsize too short!\n");
#endif
	    return ENOEXEC;
	}

	/* See if it's got the basic elf magic number leadin... */
	if (ex -> elf_magic [0] != 127
	    || bcmp ("ELF", &ex -> elf_magic [1], 3)) {
		return ENOEXEC;
	}
		/* XXX: Check other magic numbers here. */

		/* See if we got any program header information... */
	if (!ex -> phoff || !ex -> phcount) {
		return ENOEXEC;
	}

	/* Set the entry point... */
	epp -> ep_entry = ex -> entry;

	/*
	 * Check if vnode is open for writing, because we want to
	 * demand-page out of it.  If it is, don't do it.
	 */
	if (epp->ep_vp->v_writecount != 0) {
#ifdef DIAGNOSTIC
		if (epp->ep_vp->v_flag & VTEXT)
			panic("exec: a VTEXT vnode has writecount != 0\n");
#endif
		return ETXTBSY;
	}
	epp->ep_vp->v_flag |= VTEXT;

	epp->ep_taddr = 0;
	epp->ep_tsize = 0;
	epp->ep_daddr = 0;
	epp->ep_dsize = 0;

	for (i = 0; i < ex -> phcount; i++) {
#ifdef DEBUG
		/*printf("obsolete elf: mapping %x %x %x\n", resid);*/
#endif
		if ((error = vn_rdwr(UIO_READ, epp -> ep_vp, (caddr_t)&ph,
				    sizeof ph, ex -> phoff + i * sizeof ph,
				    UIO_SYSSPACE, IO_NODELOCKED,
				    p->p_ucred, &resid, p))
		    != 0)
			return error;

		if (resid != 0) {
			return ENOEXEC;
		}

		/* We only care about loadable sections... */
		if (ph.type == PT_LOAD) {
			int prot = VM_PROT_READ | VM_PROT_EXECUTE;
			int residue;
			unsigned vaddr, offset, length;

			vaddr = ph.vaddr;
			offset = ph.offset;
			length = ph.filesz;
			residue = ph.memsz - ph.filesz;

			if (ph.flags & PF_W) {
				prot |= VM_PROT_WRITE;
				if (!epp->ep_daddr || vaddr < epp -> ep_daddr)
					epp->ep_daddr = vaddr;
				epp->ep_dsize += ph.memsz;
				/* Read the data from the file... */
				NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn,
					  length, vaddr,
					  epp->ep_vp, offset, prot);
#ifdef OLD_ELF_DEBUG
/*XXX*/		printf(
	"obsolete elf: NEW_VNCMD len %x va %x off %x prot %x residue %x\n",
			length, vaddr, offset, prot, residue);
#endif /*ELF_DEBUG*/

				if (residue) {
					vaddr &= ~(NBPG - 1);
					offset &= ~(NBPG - 1);
					length = roundup (length + ph.vaddr
							  - vaddr, NBPG);
					residue = (ph.vaddr + ph.memsz)
						  - (vaddr + length);
				}
			} else {
				vaddr &= ~(NBPG - 1);
				offset &= ~(NBPG - 1);
				length = roundup (length + ph.vaddr - vaddr,
						  NBPG);
				residue = (ph.vaddr + ph.memsz)
					  - (vaddr + length);
				if (!epp->ep_taddr || vaddr < epp -> ep_taddr)
					epp->ep_taddr = vaddr;
				epp->ep_tsize += ph.memsz;
				/* Map the data from the file... */
				NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_pagedvn,
					  length, vaddr,
					  epp->ep_vp, offset, prot);
			}
			/* If part of the segment is just zeros (e.g., bss),
			   map that. */
			if (residue > 0) {
#ifdef OLD_ELF_DEBUG
/*XXX*/			printf(
	"old elf:resid NEW_VNCMD len %x va %x off %x prot %x residue %x\n",
				length, vaddr + length, offset, prot, residue);
#endif /*ELF_DEBUG*/

				NEW_VMCMD (&epp->ep_vmcmds, vmcmd_map_zero,
					   residue, vaddr + length,
					   NULLVP, 0, prot);
			}
		}
	}

	epp->ep_maxsaddr = USRSTACK - MAXSSIZ;
	epp->ep_minsaddr = USRSTACK;
	epp->ep_ssize = p->p_rlimit[RLIMIT_STACK].rlim_cur;

	/*
	 * set up commands for stack.  note that this takes *two*, one to
	 * map the part of the stack which we can access, and one to map
	 * the part which we can't.
	 *
	 * arguably, it could be made into one, but that would require the
	 * addition of another mapping proc, which is unnecessary
	 *
	 * note that in memory, things assumed to be: 0 ....... ep_maxsaddr
	 * <stack> ep_minsaddr
	 */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero,
	    ((epp->ep_minsaddr - epp->ep_ssize) - epp->ep_maxsaddr),
	    epp->ep_maxsaddr, NULLVP, 0, VM_PROT_NONE);
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero, epp->ep_ssize,
	    (epp->ep_minsaddr - epp->ep_ssize), NULLVP, 0,
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	return 0;
}
