/*	$NetBSD: procfs_mem.c,v 1.23.18.1 2002/01/14 10:55:14 he Exp $	*/

/*
 * Copyright (c) 1993 Jan-Simon Pendry
 * Copyright (c) 1993 Sean Eric Fagan
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry and Sean Eric Fagan.
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
 *	@(#)procfs_mem.c	8.5 (Berkeley) 6/15/94
 */

/*
 * This is a lightly hacked and merged version
 * of sef's pread/pwrite functions
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/vnode.h>

#include <miscfs/procfs/procfs.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <uvm/uvm_extern.h>

#define	ISSET(t, f)	((t) & (f))

/*
 * Copy data in and out of the target process.
 * We do this by mapping the process's page into
 * the kernel and then doing a uiomove direct
 * from the kernel address space.
 */
int
procfs_domem(curp, p, pfs, uio)
	struct proc *curp;		/* tracer */
	struct proc *p;			/* traced */
	struct pfsnode *pfs;
	struct uio *uio;
{
	int error;

	size_t len;
	vaddr_t	addr;

	len = uio->uio_resid;

	if (len == 0)
		return (0);

	addr = uio->uio_offset;

	if ((error = procfs_checkioperm(curp, p)) != 0)
		return (error);

	/* XXXCDC: how should locking work here? */
	if ((p->p_flag & P_WEXIT) || (p->p_vmspace->vm_refcnt < 1)) 
		return(EFAULT);
	PHOLD(p);
	p->p_vmspace->vm_refcnt++;  /* XXX */
	error = uvm_io(&p->p_vmspace->vm_map, uio);
	PRELE(p);
	uvmspace_free(p->p_vmspace);

#ifdef PMAP_NEED_PROCWR
	if (uio->uio_rw == UIO_WRITE)
		pmap_procwr(p, addr, len);
#endif
	return (error);
}

/*
 * Given process (p), find the vnode from which
 * it's text segment is being executed.
 *
 * It would be nice to grab this information from
 * the VM system, however, there is no sure-fire
 * way of doing that.  Instead, fork(), exec() and
 * wait() all maintain the p_textvp field in the
 * process proc structure which contains a held
 * reference to the exec'ed vnode.
 */
struct vnode *
procfs_findtextvp(p)
	struct proc *p;
{

	return (p->p_textvp);
}

/*
 * Ensure that a process has permission to perform I/O on another.
 * Arguments:
 *	p	The process wishing to do the I/O (the tracer).
 *	t	The process who's memory/registers will be read/written.
 */
int
procfs_checkioperm(p, t)
	struct proc *p, *t;
{
	int error;

	/*
	 * You cannot attach to a processes mem/regs if:
	 *
	 *	(1) It is currently exec'ing
	 */
	if (ISSET(t->p_flag, P_INEXEC))
		return (EAGAIN);

	/*
	 *	(2) it's not owned by you, or is set-id on exec
	 *	    (unless you're root), or...
	 */
	if ((t->p_cred->p_ruid != p->p_cred->p_ruid ||
		ISSET(t->p_flag, P_SUGID)) &&
	    (error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);

	/*
	 *	(3) ...it's init, which controls the security level
	 *	    of the entire system, and the system was not
	 *	    compiled with permanetly insecure mode turned on.
	 */
	if (t == initproc && securelevel > -1)
		return (EPERM);

	/*
	 *	(4) the tracer is chrooted, and its root directory is
	 * 	    not at or above the root directory of the tracee
	 */
	if (!proc_isunder(t, p))
		return (EPERM);
	
	return (0);
}

#ifdef probably_never
/*
 * Given process (p), find the vnode from which
 * it's text segment is being mapped.
 *
 * (This is here, rather than in procfs_subr in order
 * to keep all the VM related code in one place.)
 */
struct vnode *
procfs_findtextvp(p)
	struct proc *p;
{
	int error;
	vm_object_t object;
	vaddr_t pageno;		/* page number */

	/* find a vnode pager for the user address space */

	for (pageno = VM_MIN_ADDRESS;
			pageno < VM_MAXUSER_ADDRESS;
			pageno += PAGE_SIZE) {
		vm_map_t map;
		vm_map_entry_t out_entry;
		vm_prot_t out_prot;
		boolean_t wired, single_use;
		vaddr_t off;

		map = &p->p_vmspace->vm_map;
		error = vm_map_lookup(&map, pageno,
			      VM_PROT_READ,
			      &out_entry, &object, &off, &out_prot,
			      &wired, &single_use);

		if (!error) {
			vm_pager_t pager;

			printf("procfs: found vm object\n");
			vm_map_lookup_done(map, out_entry);
			printf("procfs: vm object = %p\n", object);

			/*
			 * At this point, assuming no errors, object
			 * is the VM object mapping UVA (pageno).
			 * Ensure it has a vnode pager, then grab
			 * the vnode from that pager's handle.
			 */

			pager = object->pager;
			printf("procfs: pager = %p\n", pager);
			if (pager)
				printf("procfs: found pager, type = %d\n",
				    pager->pg_type);
			if (pager && pager->pg_type == PG_VNODE) {
				struct vnode *vp;

				vp = (struct vnode *) pager->pg_handle;
				printf("procfs: vp = %p\n", vp);
				return (vp);
			}
		}
	}

	printf("procfs: text object not found\n");
	return (0);
}
#endif /* probably_never */
