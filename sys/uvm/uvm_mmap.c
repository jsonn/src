/*	$NetBSD: uvm_mmap.c,v 1.35 1999/07/17 21:35:50 thorpej Exp $	*/

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * Copyright (c) 1991, 1993 The Regents of the University of California.  
 * Copyright (c) 1988 University of Utah.
 * 
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *      This product includes software developed by the Charles D. Cranor,
 *	Washington University, University of California, Berkeley and 
 *	its contributors.
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
 * from: Utah $Hdr: vm_mmap.c 1.6 91/10/21$
 *      @(#)vm_mmap.c   8.5 (Berkeley) 5/19/94
 * from: Id: uvm_mmap.c,v 1.1.2.14 1998/01/05 21:04:26 chuck Exp
 */

/*
 * uvm_mmap.c: system call interface into VM system, plus kernel vm_mmap
 * function.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/resourcevar.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/conf.h>
#include <sys/stat.h>

#include <miscfs/specfs/specdev.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>

#include <sys/syscallargs.h>

#include <uvm/uvm.h>
#include <uvm/uvm_device.h>
#include <uvm/uvm_vnode.h>


/*
 * unimplemented VM system calls:
 */

/*
 * sys_sbrk: sbrk system call.
 */

/* ARGSUSED */
int
sys_sbrk(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
#if 0
	struct sys_sbrk_args /* {
		syscallarg(intptr_t) incr;
	} */ *uap = v;
#endif

	return (ENOSYS);
}

/*
 * sys_sstk: sstk system call.
 */

/* ARGSUSED */
int
sys_sstk(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
#if 0
	struct sys_sstk_args /* {
		syscallarg(int) incr;
	} */ *uap = v;
#endif

	return (ENOSYS);
}

/*
 * sys_mincore: determine if pages are in core or not.
 */

/* ARGSUSED */
int
sys_mincore(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_mincore_args /* {
		syscallarg(void *) addr;
		syscallarg(size_t) len;
		syscallarg(char *) vec;
	} */ *uap = v;
	vm_page_t m;
	char *vec, pgi;
	struct uvm_object *uobj;
	struct vm_amap *amap;
	struct vm_anon *anon;
	vm_map_entry_t entry;
	vaddr_t start, end, lim;
	vm_map_t map;
	vsize_t len;
	int error = 0, npgs;

	map = &p->p_vmspace->vm_map;

	start = (vaddr_t)SCARG(uap, addr);
	len = SCARG(uap, len);
	vec = SCARG(uap, vec);

	if (start & PAGE_MASK)
		return (EINVAL);
	len = round_page(len);
	end = start + len;
	if (end <= start)
		return (EINVAL);

	npgs = len >> PAGE_SHIFT;

	if (uvm_useracc(vec, npgs, B_WRITE) == FALSE)
		return (EFAULT);

	/*
	 * Lock down vec, so our returned status isn't outdated by
	 * storing the status byte for a page.
	 */
	uvm_vslock(p, vec, npgs, VM_PROT_WRITE);

	vm_map_lock_read(map);

	if (uvm_map_lookup_entry(map, start, &entry) == FALSE) {
		error = ENOMEM;
		goto out;
	}

	for (/* nothing */;
	     entry != &map->header && entry->start < end;
	     entry = entry->next) {
#ifdef DIAGNOSTIC
		if (UVM_ET_ISSUBMAP(entry))
			panic("mincore: user map has submap");
		if (start < entry->start)
			panic("mincore: hole");
#endif
		/* Make sure there are no holes. */
		if (entry->end < end &&
		     (entry->next == &map->header ||
		      entry->next->start > entry->end)) {
			error = ENOMEM;
			goto out;
		}

		lim = end < entry->end ? end : entry->end;

		/*
		 * Special case for objects with no "real" pages.  Those
		 * are always considered resident (mapped devices).
		 */
		if (UVM_ET_ISOBJ(entry)) {
#ifdef DIAGNOSTIC
			if (UVM_OBJ_IS_KERN_OBJECT(entry->object.uvm_obj))
				panic("mincore: user map has kernel object");
#endif
			if (entry->object.uvm_obj->pgops->pgo_releasepg
			    == NULL) {
				for (/* nothing */; start < lim;
				     start += PAGE_SIZE, vec++)
					subyte(vec, 1);
				continue;
			}
		}

		amap = entry->aref.ar_amap;	/* top layer */
		uobj = entry->object.uvm_obj;	/* bottom layer */

		if (amap != NULL)
			amap_lock(amap);
		if (uobj != NULL)
			simple_lock(&uobj->vmobjlock);

		for (/* nothing */; start < lim; start += PAGE_SIZE, vec++) {
			pgi = 0;
			if (amap != NULL) {
				/* Check the top layer first. */
				anon = amap_lookup(&entry->aref,
				    start - entry->start);
				/* Don't need to lock anon here. */
				if (anon != NULL && anon->u.an_page != NULL) {
					/*
					 * Anon has the page for this entry
					 * offset.
					 */
					pgi = 1;
				}
			}

			if (uobj != NULL && pgi == 0) {
				/* Check the bottom layer. */
				m = uvm_pagelookup(uobj,
				    entry->offset + (start - entry->start));
				if (m != NULL) {
					/*
					 * Object has the page for this entry
					 * offset.
					 */
					pgi = 1;
				}
			}

			(void) subyte(vec, pgi);
		}

		if (uobj != NULL)
			simple_unlock(&uobj->vmobjlock);
		if (amap != NULL)
			amap_unlock(amap);
	}

 out:
	vm_map_unlock_read(map);
	uvm_vsunlock(p, SCARG(uap, vec), npgs);
	return (error);
}

#if 0
/*
 * munmapfd: unmap file descriptor
 *
 * XXX: is this acutally a useful function?   could it be useful?
 */

void
munmapfd(p, fd)
	struct proc *p;
	int fd;
{

	/*
	 * XXX should vm_deallocate any regions mapped to this file
	 */
	p->p_fd->fd_ofileflags[fd] &= ~UF_MAPPED;
}
#endif

/*
 * sys_mmap: mmap system call.
 *
 * => file offest and address may not be page aligned
 *    - if MAP_FIXED, offset and address must have remainder mod PAGE_SIZE
 *    - if address isn't page aligned the mapping starts at trunc_page(addr)
 *      and the return value is adjusted up by the page offset.
 */

int
sys_mmap(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_mmap_args /* {
		syscallarg(caddr_t) addr;
		syscallarg(size_t) len;
		syscallarg(int) prot;
		syscallarg(int) flags;
		syscallarg(int) fd;
		syscallarg(long) pad;
		syscallarg(off_t) pos;
	} */ *uap = v;
	vaddr_t addr;
	struct vattr va;
	off_t pos;
	vsize_t size, pageoff;
	vm_prot_t prot, maxprot;
	int flags, fd;
	vaddr_t vm_min_address = VM_MIN_ADDRESS;
	register struct filedesc *fdp = p->p_fd;
	register struct file *fp;
	struct vnode *vp;
	caddr_t handle;
	int error;

	/*
	 * first, extract syscall args from the uap.
	 */

	addr = (vaddr_t) SCARG(uap, addr);
	size = (vsize_t) SCARG(uap, len);
	prot = SCARG(uap, prot) & VM_PROT_ALL;
	flags = SCARG(uap, flags);
	fd = SCARG(uap, fd);
	pos = SCARG(uap, pos);

	/*
	 * Fixup the old deprecated MAP_COPY into MAP_PRIVATE, and
	 * validate the flags.
	 */
	if (flags & MAP_COPY)
		flags = (flags & ~MAP_COPY) | MAP_PRIVATE;
	if ((flags & (MAP_SHARED|MAP_PRIVATE)) == (MAP_SHARED|MAP_PRIVATE))
		return (EINVAL);

	/*
	 * make sure that the newsize fits within a vaddr_t
	 * XXX: need to revise addressing data types
	 */
	if (pos + size > (vaddr_t)-PAGE_SIZE) {
#ifdef DEBUG
		printf("mmap: pos=%qx, size=%lx too big\n", (long long)pos,
		       (long)size);
#endif
		return (EINVAL);
	}

	/*
	 * align file position and save offset.  adjust size.
	 */

	pageoff = (pos & PAGE_MASK);
	pos  -= pageoff;
	size += pageoff;			/* add offset */
	size = (vsize_t) round_page(size);	/* round up */
	if ((ssize_t) size < 0)
		return (EINVAL);			/* don't allow wrap */

	/*
	 * now check (MAP_FIXED) or get (!MAP_FIXED) the "addr" 
	 */

	if (flags & MAP_FIXED) {

		/* ensure address and file offset are aligned properly */
		addr -= pageoff;
		if (addr & PAGE_MASK)
			return (EINVAL);

		if (VM_MAXUSER_ADDRESS > 0 &&
		    (addr + size) > VM_MAXUSER_ADDRESS)
			return (EINVAL);
		if (vm_min_address > 0 && addr < vm_min_address)
			return (EINVAL);
		if (addr > addr + size)
			return (EINVAL);		/* no wrapping! */

	} else {

		/*
		 * not fixed: make sure we skip over the largest possible heap.
		 * we will refine our guess later (e.g. to account for VAC, etc)
		 */
		if (addr < round_page(p->p_vmspace->vm_daddr + MAXDSIZ))
			addr = round_page(p->p_vmspace->vm_daddr + MAXDSIZ);
	}

	/*
	 * check for file mappings (i.e. not anonymous) and verify file.
	 */

	if ((flags & MAP_ANON) == 0) {

		if (fd < 0 || fd >= fdp->fd_nfiles)
			return(EBADF);		/* failed range check? */
		fp = fdp->fd_ofiles[fd];	/* convert to file pointer */
		if (fp == NULL)
			return(EBADF);

		if (fp->f_type != DTYPE_VNODE)
			return (ENODEV);		/* only mmap vnodes! */
		vp = (struct vnode *)fp->f_data;	/* convert to vnode */

		if (vp->v_type != VREG && vp->v_type != VCHR &&
		    vp->v_type != VBLK)
			return (ENODEV);  /* only REG/CHR/BLK support mmap */

		/* special case: catch SunOS style /dev/zero */
		if (vp->v_type == VCHR && iszerodev(vp->v_rdev)) {
			flags |= MAP_ANON;
			goto is_anon;
		}

		/*
		 * Old programs may not select a specific sharing type, so
		 * default to an appropriate one.
		 *
		 * XXX: how does MAP_ANON fit in the picture?
		 */
		if ((flags & (MAP_SHARED|MAP_PRIVATE)) == 0) {
#if defined(DEBUG)
			printf("WARNING: defaulted mmap() share type to "
			   "%s (pid %d comm %s)\n", vp->v_type == VCHR ?
			   "MAP_SHARED" : "MAP_PRIVATE", p->p_pid,
			    p->p_comm);
#endif
			if (vp->v_type == VCHR)
				flags |= MAP_SHARED;	/* for a device */
			else
				flags |= MAP_PRIVATE;	/* for a file */
		}

		/* 
		 * MAP_PRIVATE device mappings don't make sense (and aren't
		 * supported anyway).  However, some programs rely on this,
		 * so just change it to MAP_SHARED.
		 */
		if (vp->v_type == VCHR && (flags & MAP_PRIVATE) != 0) {
#if defined(DIAGNOSTIC)
			printf("WARNING: converted MAP_PRIVATE device mapping "
			    "to MAP_SHARED (pid %d comm %s)\n", p->p_pid,
			    p->p_comm);
#endif
			flags = (flags & ~MAP_PRIVATE) | MAP_SHARED;
		}

		/*
		 * now check protection
		 */

		maxprot = VM_PROT_EXECUTE;

		/* check read access */
		if (fp->f_flag & FREAD)
			maxprot |= VM_PROT_READ;
		else if (prot & PROT_READ)
			return (EACCES);

		/* check write access, shared case first */
		if (flags & MAP_SHARED) {
			/*
			 * if the file is writable, only add PROT_WRITE to
			 * maxprot if the file is not immutable, append-only.
			 * otherwise, if we have asked for PROT_WRITE, return
			 * EPERM.
			 */
			if (fp->f_flag & FWRITE) {
				if ((error =
				    VOP_GETATTR(vp, &va, p->p_ucred, p)))
					return (error);
				if ((va.va_flags & (IMMUTABLE|APPEND)) == 0)
					maxprot |= VM_PROT_WRITE;
				else if (prot & PROT_WRITE)
					return (EPERM);
			}
			else if (prot & PROT_WRITE)
				return (EACCES);
		} else {
			/* MAP_PRIVATE mappings can always write to */
			maxprot |= VM_PROT_WRITE;
		}

		/*
		 * set handle to vnode
		 */

		handle = (caddr_t)vp;

	} else {		/* MAP_ANON case */
		/*
		 * XXX What do we do about (MAP_SHARED|MAP_PRIVATE) == 0?
		 */
		if (fd != -1)
			return (EINVAL);

 is_anon:		/* label for SunOS style /dev/zero */
		handle = NULL;
		maxprot = VM_PROT_ALL;
		pos = 0;
	}

	/*
	 * XXX (in)sanity check.  We don't do proper datasize checking
	 * XXX for anonymous (or private writable) mmap().  However,
	 * XXX know that if we're trying to allocate more than the amount
	 * XXX remaining under our current data size limit, _that_ should
	 * XXX be disallowed.
	 */
	if ((flags & MAP_ANON) != 0 ||
	    ((flags & MAP_PRIVATE) != 0 && (prot & PROT_WRITE) != 0)) {
		if (size >
		    (p->p_rlimit[RLIMIT_DATA].rlim_cur - ctob(p->p_vmspace->vm_dsize))) {
			return (ENOMEM);
		}
	}

	/*
	 * now let kernel internal function uvm_mmap do the work.
	 */

	error = uvm_mmap(&p->p_vmspace->vm_map, &addr, size, prot, maxprot,
	    flags, handle, pos, p->p_rlimit[RLIMIT_MEMLOCK].rlim_cur);

	if (error == 0)
		/* remember to add offset */
		*retval = (register_t)(addr + pageoff);

	return (error);
}

/*
 * sys___msync13: the msync system call (a front-end for flush)
 */

int
sys___msync13(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys___msync13_args /* {
		syscallarg(caddr_t) addr;
		syscallarg(size_t) len;
		syscallarg(int) flags;
	} */ *uap = v;
	vaddr_t addr;
	vsize_t size, pageoff;
	vm_map_t map;
	int rv, flags, uvmflags;

	/*
	 * extract syscall args from the uap
	 */

	addr = (vaddr_t)SCARG(uap, addr);
	size = (vsize_t)SCARG(uap, len);
	flags = SCARG(uap, flags);

	/* sanity check flags */
	if ((flags & ~(MS_ASYNC | MS_SYNC | MS_INVALIDATE)) != 0 ||
			(flags & (MS_ASYNC | MS_SYNC | MS_INVALIDATE)) == 0 ||
			(flags & (MS_ASYNC | MS_SYNC)) == (MS_ASYNC | MS_SYNC))
	  return (EINVAL);
	if ((flags & (MS_ASYNC | MS_SYNC)) == 0)
	  flags |= MS_SYNC;

	/*
	 * align the address to a page boundary, and adjust the size accordingly
	 */

	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vsize_t) round_page(size);

	/* disallow wrap-around. */
	if (addr + size < addr)
		return (EINVAL);

	/*
	 * get map
	 */

	map = &p->p_vmspace->vm_map;

	/*
	 * XXXCDC: do we really need this semantic?
	 *
	 * XXX Gak!  If size is zero we are supposed to sync "all modified
	 * pages with the region containing addr".  Unfortunately, we
	 * don't really keep track of individual mmaps so we approximate
	 * by flushing the range of the map entry containing addr.
	 * This can be incorrect if the region splits or is coalesced
	 * with a neighbor.
	 */
	if (size == 0) {
		vm_map_entry_t entry;
		
		vm_map_lock_read(map);
		rv = uvm_map_lookup_entry(map, addr, &entry);
		if (rv == TRUE) {
			addr = entry->start;
			size = entry->end - entry->start;
		}
		vm_map_unlock_read(map);
		if (rv == FALSE)
			return (EINVAL);
	}

	/*
	 * translate MS_ flags into PGO_ flags
	 */
	uvmflags = PGO_CLEANIT;
	if (flags & MS_INVALIDATE)
		uvmflags |= PGO_FREE;
	if (flags & MS_SYNC)
		uvmflags |= PGO_SYNCIO;
	else
		uvmflags |= PGO_SYNCIO;	 /* XXXCDC: force sync for now! */

	/*
	 * doit!
	 */
	rv = uvm_map_clean(map, addr, addr+size, uvmflags);

	/*
	 * and return... 
	 */
	switch (rv) {
	case KERN_SUCCESS:
		return(0);
	case KERN_INVALID_ADDRESS:
		return (ENOMEM);
	case KERN_FAILURE:
		return (EIO);
	case KERN_PAGES_LOCKED:	/* XXXCDC: uvm doesn't return this */
		return (EBUSY);
	default:
		return (EINVAL);
	}
	/*NOTREACHED*/
}

/*
 * sys_munmap: unmap a users memory
 */

int
sys_munmap(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_munmap_args /* {
		syscallarg(caddr_t) addr;
		syscallarg(size_t) len;
	} */ *uap = v;
	vaddr_t addr;
	vsize_t size, pageoff;
	vm_map_t map;
	vaddr_t vm_min_address = VM_MIN_ADDRESS;
	struct vm_map_entry *dead_entries;

	/*
	 * get syscall args...
	 */

	addr = (vaddr_t) SCARG(uap, addr);
	size = (vsize_t) SCARG(uap, len);
	
	/*
	 * align the address to a page boundary, and adjust the size accordingly
	 */

	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vsize_t) round_page(size);

	if ((int)size < 0)
		return (EINVAL);
	if (size == 0)
		return (0);

	/*
	 * Check for illegal addresses.  Watch out for address wrap...
	 * Note that VM_*_ADDRESS are not constants due to casts (argh).
	 */
	if (VM_MAXUSER_ADDRESS > 0 && addr + size > VM_MAXUSER_ADDRESS)
		return (EINVAL);
	if (vm_min_address > 0 && addr < vm_min_address)
		return (EINVAL);
	if (addr > addr + size)
		return (EINVAL);
	map = &p->p_vmspace->vm_map;


	vm_map_lock(map);	/* lock map so we can checkprot */

	/*
	 * interesting system call semantic: make sure entire range is 
	 * allocated before allowing an unmap.
	 */

	if (!uvm_map_checkprot(map, addr, addr + size, VM_PROT_NONE)) {
		vm_map_unlock(map);
		return (EINVAL);
	}

	/*
	 * doit!
	 */
	(void) uvm_unmap_remove(map, addr, addr + size, &dead_entries);

	vm_map_unlock(map);	/* and unlock */

	if (dead_entries != NULL)
		uvm_unmap_detach(dead_entries, 0);

	return (0);
}

/*
 * sys_mprotect: the mprotect system call
 */

int
sys_mprotect(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_mprotect_args /* {
		syscallarg(caddr_t) addr;
		syscallarg(int) len;
		syscallarg(int) prot;
	} */ *uap = v;
	vaddr_t addr;
	vsize_t size, pageoff;
	vm_prot_t prot;
	int rv;

	/*
	 * extract syscall args from uap
	 */

	addr = (vaddr_t)SCARG(uap, addr);
	size = (vsize_t)SCARG(uap, len);
	prot = SCARG(uap, prot) & VM_PROT_ALL;

	/*
	 * align the address to a page boundary, and adjust the size accordingly
	 */
	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vsize_t) round_page(size);
	if ((int)size < 0)
		return (EINVAL);

	/*
	 * doit
	 */

	rv = uvm_map_protect(&p->p_vmspace->vm_map, 
			   addr, addr+size, prot, FALSE);

	if (rv == KERN_SUCCESS)
		return (0);
	if (rv == KERN_PROTECTION_FAILURE)
		return (EACCES);
	return (EINVAL);
}

/*
 * sys_minherit: the minherit system call
 */

int
sys_minherit(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_minherit_args /* {
		syscallarg(caddr_t) addr;
		syscallarg(int) len;
		syscallarg(int) inherit;
	} */ *uap = v;
	vaddr_t addr;
	vsize_t size, pageoff;
	register vm_inherit_t inherit;
	
	addr = (vaddr_t)SCARG(uap, addr);
	size = (vsize_t)SCARG(uap, len);
	inherit = SCARG(uap, inherit);
	/*
	 * align the address to a page boundary, and adjust the size accordingly
	 */

	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vsize_t) round_page(size);

	if ((int)size < 0)
		return (EINVAL);
	
	switch (uvm_map_inherit(&p->p_vmspace->vm_map, addr, addr+size,
			 inherit)) {
	case KERN_SUCCESS:
		return (0);
	case KERN_PROTECTION_FAILURE:
		return (EACCES);
	}
	return (EINVAL);
}

/*
 * sys_madvise: give advice about memory usage.
 */

/* ARGSUSED */
int
sys_madvise(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_madvise_args /* {
		syscallarg(caddr_t) addr;
		syscallarg(size_t) len;
		syscallarg(int) behav;
	} */ *uap = v;
	vaddr_t addr;
	vsize_t size, pageoff;
	int advice, rv;;
	
	addr = (vaddr_t)SCARG(uap, addr);
	size = (vsize_t)SCARG(uap, len);
	advice = SCARG(uap, behav);

	/*
	 * align the address to a page boundary, and adjust the size accordingly
	 */
	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vsize_t) round_page(size);

	if ((ssize_t)size <= 0)
		return (EINVAL);

	switch (advice) {
	case MADV_NORMAL:
	case MADV_RANDOM:
	case MADV_SEQUENTIAL:
		rv = uvm_map_advice(&p->p_vmspace->vm_map, addr, addr + size,
		    advice);
		break;

	case MADV_WILLNEED:
		/*
		 * Activate all these pages, pre-faulting them in if
		 * necessary.
		 */
		/*
		 * XXX IMPLEMENT ME.
		 * Should invent a "weak" mode for uvm_fault()
		 * which would only do the PGO_LOCKED pgo_get().
		 */
		return (0);

	case MADV_DONTNEED:
		/*
		 * Deactivate all these pages.  We don't need them
		 * any more.  We don't, however, toss the data in
		 * the pages.
		 */
		rv = uvm_map_clean(&p->p_vmspace->vm_map, addr, addr + size,
		    PGO_DEACTIVATE);
		break;

	case MADV_FREE:
		/*
		 * These pages contain no valid data, and may be
		 * grbage-collected.  Toss all resources, including
		 * any swap space in use.
		 */
		rv = uvm_map_clean(&p->p_vmspace->vm_map, addr, addr + size,
		    PGO_FREE);
		break;

	case MADV_SPACEAVAIL:
		/*
		 * XXXMRG What is this?  I think it's:
		 *
		 *	Ensure that we have allocated backing-store
		 *	for these pages.
		 *
		 * This is going to require changes to the page daemon,
		 * as it will free swap space allocated to pages in core.
		 * There's also what to do for device/file/anonymous memory.
		 */
		return (EINVAL);

	default:
		return (EINVAL);
	}

	switch (rv) {
	case KERN_SUCCESS:
		return (0);
	case KERN_NO_SPACE:
		return (EAGAIN);
	case KERN_INVALID_ADDRESS:
		return (ENOMEM);
	case KERN_FAILURE:
		return (EIO);
	}

	return (EINVAL);
}

/*
 * sys_mlock: memory lock
 */

int
sys_mlock(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_mlock_args /* {
		syscallarg(const void *) addr;
		syscallarg(size_t) len;
	} */ *uap = v;
	vaddr_t addr;
	vsize_t size, pageoff;
	int error;

	/*
	 * extract syscall args from uap
	 */
	addr = (vaddr_t)SCARG(uap, addr);
	size = (vsize_t)SCARG(uap, len);

	/*
	 * align the address to a page boundary and adjust the size accordingly
	 */
	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vsize_t) round_page(size);
	
	/* disallow wrap-around. */
	if (addr + (int)size < addr)
		return (EINVAL);

	if (atop(size) + uvmexp.wired > uvmexp.wiredmax)
		return (EAGAIN);

#ifdef pmap_wired_count
	if (size + ptoa(pmap_wired_count(vm_map_pmap(&p->p_vmspace->vm_map))) >
			p->p_rlimit[RLIMIT_MEMLOCK].rlim_cur)
		return (EAGAIN);
#else
	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);
#endif

	error = uvm_map_pageable(&p->p_vmspace->vm_map, addr, addr+size, FALSE,
	    0);
	return (error == KERN_SUCCESS ? 0 : ENOMEM);
}

/*
 * sys_munlock: unlock wired pages
 */

int
sys_munlock(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_munlock_args /* {
		syscallarg(const void *) addr;
		syscallarg(size_t) len;
	} */ *uap = v;
	vaddr_t addr;
	vsize_t size, pageoff;
	int error;

	/*
	 * extract syscall args from uap
	 */

	addr = (vaddr_t)SCARG(uap, addr);
	size = (vsize_t)SCARG(uap, len);

	/*
	 * align the address to a page boundary, and adjust the size accordingly
	 */
	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vsize_t) round_page(size);

	/* disallow wrap-around. */
	if (addr + (int)size < addr)
		return (EINVAL);

#ifndef pmap_wired_count
	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);
#endif

	error = uvm_map_pageable(&p->p_vmspace->vm_map, addr, addr+size, TRUE,
	    0);
	return (error == KERN_SUCCESS ? 0 : ENOMEM);
}

/*
 * sys_mlockall: lock all pages mapped into an address space.
 */

int
sys_mlockall(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_mlockall_args /* {
		syscallarg(int) flags;
	} */ *uap = v;
	int error, flags;

	flags = SCARG(uap, flags);

	if (flags == 0 ||
	    (flags & ~(MCL_CURRENT|MCL_FUTURE)) != 0)
		return (EINVAL);

#ifndef pmap_wired_count
	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);
#endif

	error = uvm_map_pageable_all(&p->p_vmspace->vm_map, flags,
	    p->p_rlimit[RLIMIT_MEMLOCK].rlim_cur);
	switch (error) {
	case KERN_SUCCESS:
		error = 0;
		break;

	case KERN_NO_SPACE:	/* XXX overloaded */
		error = ENOMEM;
		break;

	default:
		/*
		 * "Some or all of the memory could not be locked when
		 * the call was made."
		 */
		error = EAGAIN;
	}

	return (error);
}

/*
 * sys_munlockall: unlock all pages mapped into an address space.
 */

int
sys_munlockall(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

	(void) uvm_map_pageable_all(&p->p_vmspace->vm_map, 0, 0);
	return (0);
}

/*
 * uvm_mmap: internal version of mmap
 *
 * - used by sys_mmap, exec, and sysv shm
 * - handle is a vnode pointer or NULL for MAP_ANON (XXX: not true,
 *	sysv shm uses "named anonymous memory")
 * - caller must page-align the file offset
 */

int
uvm_mmap(map, addr, size, prot, maxprot, flags, handle, foff, locklimit)
	vm_map_t map;
	vaddr_t *addr;
	vsize_t size;
	vm_prot_t prot, maxprot;
	int flags;
	caddr_t handle;		/* XXX: VNODE? */
	vaddr_t foff;
	vsize_t locklimit;
{
	struct uvm_object *uobj;
	struct vnode *vp;
	int retval;
	int advice = UVM_ADV_NORMAL;
	uvm_flag_t uvmflag = 0;

	/*
	 * check params
	 */

	if (size == 0)
		return(0);
	if (foff & PAGE_MASK)
		return(EINVAL);
	if ((prot & maxprot) != prot)
		return(EINVAL);

	/*
	 * for non-fixed mappings, round off the suggested address.
	 * for fixed mappings, check alignment and zap old mappings.
	 */

	if ((flags & MAP_FIXED) == 0) {
		*addr = round_page(*addr);	/* round */
	} else {
		
		if (*addr & PAGE_MASK)
			return(EINVAL);
		uvmflag |= UVM_FLAG_FIXED;
		(void) uvm_unmap(map, *addr, *addr + size);	/* zap! */
	}

	/*
	 * handle anon vs. non-anon mappings.   for non-anon mappings attach
	 * to underlying vm object.
	 */

	if (flags & MAP_ANON) {
		
		foff = UVM_UNKNOWN_OFFSET;		
		uobj = NULL;
		if ((flags & MAP_SHARED) == 0)
			/* XXX: defer amap create */
			uvmflag |= UVM_FLAG_COPYONW;
		else
			/* shared: create amap now */
			uvmflag |= UVM_FLAG_OVERLAY;

	} else {

		vp = (struct vnode *) handle;	/* get vnode */
		if (vp->v_type != VCHR) {
			uobj = uvn_attach((void *) vp, (flags & MAP_SHARED) ?
			   maxprot : (maxprot & ~VM_PROT_WRITE));

			/*
			 * XXXCDC: hack from old code
			 * don't allow vnodes which have been mapped
			 * shared-writeable to persist [forces them to be
			 * flushed out when last reference goes].
			 * XXXCDC: interesting side effect: avoids a bug.
			 * note that in WRITE [ufs_readwrite.c] that we
			 * allocate buffer, uncache, and then do the write.
			 * the problem with this is that if the uncache causes
			 * VM data to be flushed to the same area of the file
			 * we are writing to... in that case we've got the
			 * buffer locked and our process goes to sleep forever.
			 *
			 * XXXCDC: checking maxprot protects us from the
			 * "persistbug" program but this is not a long term
			 * solution.
			 * 
			 * XXXCDC: we don't bother calling uncache with the vp
			 * VOP_LOCKed since we know that we are already
			 * holding a valid reference to the uvn (from the
			 * uvn_attach above), and thus it is impossible for
			 * the uncache to kill the uvn and trigger I/O.
			 */
			if (flags & MAP_SHARED) {
				if ((prot & VM_PROT_WRITE) ||
				    (maxprot & VM_PROT_WRITE)) {
					uvm_vnp_uncache(vp);
				}
			}

		} else {
			uobj = udv_attach((void *) &vp->v_rdev,
			    (flags & MAP_SHARED) ?
			    maxprot : (maxprot & ~VM_PROT_WRITE), foff, size);
			advice = UVM_ADV_RANDOM;
		}
		
		if (uobj == NULL)
			return((vp->v_type == VREG) ? ENOMEM : EINVAL);

		if ((flags & MAP_SHARED) == 0)
			uvmflag |= UVM_FLAG_COPYONW;
	}

	/*
	 * set up mapping flags
	 */

	uvmflag = UVM_MAPFLAG(prot, maxprot, 
			(flags & MAP_SHARED) ? UVM_INH_SHARE : UVM_INH_COPY,
			advice, uvmflag);

	/*
	 * do it!
	 */

	retval = uvm_map(map, addr, size, uobj, foff, uvmflag);

	if (retval == KERN_SUCCESS) {
		/*
		 * POSIX 1003.1b -- if our address space was configured
		 * to lock all future mappings, wire the one we just made.
		 */
		if (prot == VM_PROT_NONE) {
			/*
			 * No more work to do in this case.
			 */
			return (0);
		}
		
		vm_map_lock(map);

		if (map->flags & VM_MAP_WIREFUTURE) {
			/*
			 * uvm_map_pageable() always returns the map
			 * unlocked.
			 */
			if ((atop(size) + uvmexp.wired) > uvmexp.wiredmax
#ifdef pmap_wired_count
			    || (locklimit != 0 && (size +
			         ptoa(pmap_wired_count(vm_map_pmap(map)))) >
			        locklimit)
#endif
			) {
				retval = KERN_RESOURCE_SHORTAGE;
				/* unmap the region! */
				(void) uvm_unmap(map, *addr, *addr + size);
				goto bad;
			}
			retval = uvm_map_pageable(map, *addr, *addr + size,
			    FALSE, UVM_LK_ENTER);
			if (retval != KERN_SUCCESS) {
				/* unmap the region! */
				(void) uvm_unmap(map, *addr, *addr + size);
				goto bad;
			}
			return (0);
		}

		vm_map_unlock(map);

		return (0);
	}

	/*
	 * errors: first detach from the uobj, if any.
	 */
	
	if (uobj)
		uobj->pgops->pgo_detach(uobj);

 bad:
	switch (retval) {
	case KERN_INVALID_ADDRESS:
	case KERN_NO_SPACE:
		return(ENOMEM);
	case KERN_RESOURCE_SHORTAGE:
		return (EAGAIN);
	case KERN_PROTECTION_FAILURE:
		return(EACCES);
	}
	return(EINVAL);
}
