/*	$NetBSD: exec_ecoff.c,v 1.10.2.2 2000/11/22 16:05:16 bouyer Exp $	*/

/*
 * Copyright (c) 1994 Adam Glass
 * Copyright (c) 1993, 1994, 1996, 1999 Christopher G. Demetriou
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christopher G. Demetriou
 *      for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/exec.h>
#include <sys/resourcevar.h>

#include <sys/exec_ecoff.h>

/*
 * exec_ecoff_makecmds(): Check if it's an ecoff-format executable.
 *
 * Given a proc pointer and an exec package pointer, see if the referent
 * of the epp is in ecoff format.  Check 'standard' magic numbers for
 * this architecture.  If that fails, return failure.
 *
 * This function is  responsible for creating a set of vmcmds which can be
 * used to build the process's vm space and inserting them into the exec
 * package.
 */
int
exec_ecoff_makecmds(struct proc *p, struct exec_package *epp)
{
	int error;
	struct ecoff_exechdr *execp = epp->ep_hdr;

	if (epp->ep_hdrvalid < ECOFF_HDR_SIZE)
		return ENOEXEC;

	if (ECOFF_BADMAG(execp))
		return ENOEXEC;

	error = (*epp->ep_esch->u.ecoff_probe_func)(p, epp);

	/*
	 * if there was an error or there are already vmcmds set up,
	 * we return.  (the latter can happen if cpu_exec_ecoff_hook()
	 * recursively invokes check_exec() to handle loading of a
	 * dynamically linked binary's shared loader.
	 */
	if (error || epp->ep_vmcmds.evs_cnt)
		return (error);

	/*
	 * prepare the exec package to map the executable.
	 */
	switch (execp->a.magic) {
	case ECOFF_OMAGIC:
		error = exec_ecoff_prep_omagic(p, epp, epp->ep_hdr,
		   epp->ep_vp);
		break;
	case ECOFF_NMAGIC:
		error = exec_ecoff_prep_nmagic(p, epp, epp->ep_hdr, 
		   epp->ep_vp);
		break;
	case ECOFF_ZMAGIC:
		error = exec_ecoff_prep_zmagic(p, epp, epp->ep_hdr,
		   epp->ep_vp);
		break;
	default:
		return ENOEXEC;
	}

	/* set up the stack */
	if (!error)
		error = exec_ecoff_setup_stack(p, epp);

	if (error)
		kill_vmcmds(&epp->ep_vmcmds);

	return error;
}

/*
 * exec_ecoff_setup_stack(): Set up the stack segment for an ecoff
 * executable.
 *
 * Note that the ep_ssize parameter must be set to be the current stack
 * limit; this is adjusted in the body of execve() to yield the
 * appropriate stack segment usage once the argument length is
 * calculated.
 *
 * This function returns an int for uniformity with other (future) formats'
 * stack setup functions.  They might have errors to return.
 */
int
exec_ecoff_setup_stack(struct proc *p, struct exec_package *epp)
{

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
	 * note that in memory, things assumed to be: 0 ... ep_maxsaddr
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

/*
 * exec_ecoff_prep_omagic(): Prepare a ECOFF OMAGIC binary's exec package
 */
int
exec_ecoff_prep_omagic(struct proc *p, struct exec_package *epp,
    struct ecoff_exechdr *execp, struct vnode *vp)
{
	struct ecoff_aouthdr *eap = &execp->a;

	epp->ep_taddr = ECOFF_SEGMENT_ALIGN(execp, eap->text_start);
	epp->ep_tsize = eap->tsize;
	epp->ep_daddr = ECOFF_SEGMENT_ALIGN(execp, eap->data_start);
	epp->ep_dsize = eap->dsize + eap->bsize;
	epp->ep_entry = eap->entry;

	/* set up command for text and data segments */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn,
	    eap->tsize + eap->dsize, epp->ep_taddr, vp,
	    ECOFF_TXTOFF(execp),
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	/* set up command for bss segment */
	if (eap->bsize > 0)
		NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero, eap->bsize,
		    ECOFF_SEGMENT_ALIGN(execp, eap->bss_start), NULLVP, 0,
		    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);
	
	return 0;
}

/*
 * exec_ecoff_prep_nmagic(): Prepare a 'native' NMAGIC ECOFF binary's exec
 *                           package.
 */
int
exec_ecoff_prep_nmagic(struct proc *p, struct exec_package *epp,
    struct ecoff_exechdr *execp, struct vnode *vp)
{
	struct ecoff_aouthdr *eap = &execp->a;

	epp->ep_taddr = ECOFF_SEGMENT_ALIGN(execp, eap->text_start);
	epp->ep_tsize = eap->tsize;
	epp->ep_daddr = ECOFF_ROUND(eap->data_start, ECOFF_LDPGSZ);
	epp->ep_dsize = eap->dsize + eap->bsize;
	epp->ep_entry = eap->entry;

	/* set up command for text segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn, epp->ep_tsize,
	    epp->ep_taddr, vp, ECOFF_TXTOFF(execp),
	    VM_PROT_READ|VM_PROT_EXECUTE);

	/* set up command for data segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn, epp->ep_dsize,
	    epp->ep_daddr, vp, ECOFF_DATOFF(execp),
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	/* set up command for bss segment */
	if (eap->bsize > 0)
		NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero, eap->bsize,
		    ECOFF_SEGMENT_ALIGN(execp, eap->bss_start), NULLVP, 0,
		    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	return 0;
}

/*
 * exec_ecoff_prep_zmagic(): Prepare a ECOFF ZMAGIC binary's exec package
 *
 * First, set the various offsets/lengths in the exec package.
 *
 * Then, mark the text image busy (so it can be demand paged) or error
 * out if this is not possible.  Finally, set up vmcmds for the
 * text, data, bss, and stack segments.
 */
int
exec_ecoff_prep_zmagic(struct proc *p, struct exec_package *epp,
    struct ecoff_exechdr *execp, struct vnode *vp)
{
	struct ecoff_aouthdr *eap = &execp->a;

	epp->ep_taddr = ECOFF_SEGMENT_ALIGN(execp, eap->text_start);
	epp->ep_tsize = eap->tsize;
	epp->ep_daddr = ECOFF_SEGMENT_ALIGN(execp, eap->data_start);
	epp->ep_dsize = eap->dsize + eap->bsize;
	epp->ep_entry = eap->entry;

	/*
	 * check if vnode is in open for writing, because we want to
	 * demand-page out of it.  if it is, don't do it, for various
	 * reasons
	 */
	if ((eap->tsize != 0 || eap->dsize != 0) &&
	    vp->v_writecount != 0) {
#ifdef DIAGNOSTIC
		if (vp->v_flag & VTEXT)
			panic("exec: a VTEXT vnode has writecount != 0\n");
#endif
		return ETXTBSY;
	}
	vn_marktext(vp);

	/* set up command for text segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_pagedvn, eap->tsize,
	    epp->ep_taddr, vp, ECOFF_TXTOFF(execp),
	    VM_PROT_READ|VM_PROT_EXECUTE);

	/* set up command for data segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_pagedvn, eap->dsize,
	    epp->ep_daddr, vp, ECOFF_DATOFF(execp),
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	/* set up command for bss segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero, eap->bsize,
	    ECOFF_SEGMENT_ALIGN(execp, eap->bss_start), NULLVP, 0,
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	return 0;
}
