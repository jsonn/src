/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1991 The Regents of the University of California.
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
 *	from: Utah Hdr: vm_mmap.c 1.3 90/01/21
 *	from: @(#)vm_mmap.c	7.5 (Berkeley) 6/28/91
 *	$Id: vm_mmap.c,v 1.18.2.3 1994/04/15 01:54:43 cgd Exp $
 */

/*
 * Mapped file (mmap) interface to VM
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <miscfs/specfs/specdev.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/conf.h>

#include <vm/vm.h>
#include <vm/vm_pager.h>
#include <vm/vm_prot.h>
#include <vm/vm_user.h>

#ifdef DEBUG
int mmapdebug = 0;
#define MDB_FOLLOW	0x01
#define MDB_SYNC	0x02
#define MDB_MAPIT	0x04
#endif

static boolean_t vm_map_is_allocated
		    __P((vm_map_t, vm_offset_t, vm_offset_t, boolean_t));

/* ARGSUSED */
int
getpagesize(p, uap, retval)
	struct proc *p;
	void *uap;
	int *retval;
{

	*retval = NBPG * CLSIZE;
	return (0);
}

struct sbrk_args {
	int	incr;
};

/* ARGSUSED */
int
sbrk(p, uap, retval)
	struct proc *p;
	struct sbrk_args *uap;
	int *retval;
{

	/* Not yet implemented */
	return (EOPNOTSUPP);
}

struct sstk_args {
	int	incr;
};

/* ARGSUSED */
int
sstk(p, uap, retval)
	struct proc *p;
	struct sstk_args *uap;
	int *retval;
{

	/* Not yet implemented */
	return (EOPNOTSUPP);
}

struct mmap_args {
	caddr_t	addr;
	int	len;
	int	prot;
	int	flags;
	int	fd;
	int	pad;
	off_t	pos;
};

int
smmap(p, uap, retval)				/* XXX SHOULD BE mmap() */
	struct proc *p;
	register struct mmap_args *uap;
	int *retval;
{
	register struct filedesc *fdp = p->p_fd;
	register struct file *fp;
	struct vnode *vp;
	vm_offset_t addr;
	vm_size_t size;
	vm_prot_t prot;
	vm_prot_t maxprot;
	caddr_t handle;
	int error;
	int flags = uap->flags;

#ifdef DEBUG
	if (mmapdebug & MDB_FOLLOW)
		printf("mmap(%d): addr %x len %x pro %x flg %x fd %d pos %qx\n",
		    p->p_pid, uap->addr, uap->len, uap->prot,
		    flags, uap->fd, uap->pos);
#endif
        if (uap->pos > (quad_t)0x07fffffff ||
            uap->pos < -(quad_t)0x080000000)
                return (EINVAL);  /* XXX */

	/*
	 * Address (if FIXED) must be page aligned.
	 * Size is implicitly rounded to a page boundary.
	 */
	addr = (vm_offset_t) uap->addr;
	if ((flags & MAP_FIXED) && (addr & page_mask) || uap->len < 0)
		return(EINVAL);
	size = (vm_size_t) round_page(uap->len);
	if ((flags & MAP_FIXED) && (addr + size > VM_MAXUSER_ADDRESS))
		return(EINVAL);
	/*
	 * XXX if no hint provided for a non-fixed mapping place it after
	 * the end of the largest possible heap.
	 *
	 * There should really be a pmap call to determine a reasonable
	 * location.
	 */
	if (addr == 0 && (flags & MAP_FIXED) == 0)
		addr = round_page(p->p_vmspace->vm_daddr + MAXDSIZ);
	/*
	 * Mapping file or named anonymous, get fp for validation
	 */
	if ((flags & MAP_ANON) == 0 || uap->fd != -1) {
		if (((unsigned)uap->fd) >= fdp->fd_nfiles ||
		    (fp = fdp->fd_ofiles[uap->fd]) == NULL)
			return(EBADF);
	}
	/*
	 * Map current protections to MACH style
	 */
	prot = VM_PROT_NONE;
	if (uap->prot & PROT_READ)
		prot |= VM_PROT_READ;
	if (uap->prot & PROT_WRITE)
		prot |= VM_PROT_WRITE;
	if (uap->prot & PROT_EXEC)
		prot |= VM_PROT_EXECUTE;
	/*
	 * If we are mapping a file we need to check various
	 * file/vnode related things.
	 */
	if ((flags & MAP_ANON) == 0) {
		/*
		 * Obtain vnode and make sure it is of appropriate type
		 */
		if (fp->f_type != DTYPE_VNODE)
			return(EINVAL);
		vp = (struct vnode *)fp->f_data;
		if (vp->v_type != VREG && vp->v_type != VCHR)
			return(EINVAL);
		/*
		 * Set the maximum protection according to the file
		 * protection.
		 */
		maxprot = VM_PROT_NONE;
		if (fp->f_flag & FREAD)
			maxprot |= VM_PROT_READ|VM_PROT_EXECUTE;
		if (flags & MAP_SHARED) {
			if (fp->f_flag & FWRITE)
				maxprot |= VM_PROT_WRITE;
		} else
			maxprot |= VM_PROT_WRITE;
		/*
		 * Ensure that the maximum protection (based on the file
		 * protection) and the desired protection are compatible.
		 */
		if ((maxprot & prot) != prot)
			return(EACCES);
		handle = (caddr_t)vp;
	} else if (uap->fd != -1) {
		maxprot = VM_PROT_ALL;
		handle = (caddr_t)fp;
	} else {
		maxprot = VM_PROT_ALL;
		handle = NULL;
	}
	error = vm_mmap(&p->p_vmspace->vm_map, &addr, size, prot, maxprot,
			flags, handle, (vm_offset_t)uap->pos);
	if (error == 0)
		*retval = (int) addr;
	return(error);
}

#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
struct onmmap_args {
	caddr_t	addr;
	int	len;
	int	prot;
	int	flags;
	int	fd;
	off_t	pos;
};

int
onmmap(p, uap, retval)
	struct proc *p;
	register struct onmmap_args *uap;
	int *retval;
{
	struct mmap_args ma;

	ma.addr = uap->addr;
	ma.len = uap->len;
	ma.prot = uap->prot;
	ma.flags = uap->flags;
	ma.fd = uap->fd;
	ma.pos = uap->pos;
	return (smmap(p, &ma, retval));
}

struct ommap_args {
	caddr_t	addr;
	int	len;
	int	prot;
	int	flags;
	int	fd;
	long	pos;
};

int
ommap(p, uap, retval)
	struct proc *p;
	register struct ommap_args *uap;
	int *retval;
{
	struct mmap_args ma;

	ma.len = uap->len;
	/* prot and flags below */
	ma.fd = uap->fd;
	ma.pos = uap->pos;

#define	OPROT_EXEC		0x01
#define	OPROT_WRITE		0x02
#define	OPROT_READ		0x04

	ma.prot = 0;
	if (uap->prot & OPROT_EXEC)
		ma.prot |= PROT_EXEC;
	if (uap->prot & OPROT_WRITE)
		ma.prot |= PROT_WRITE;
	if (uap->prot & OPROT_READ)
		ma.prot |= PROT_READ;

#define	OMAP_ANON		0x0002
#define	OMAP_SHARED		0x0010
#define	OMAP_COPY		0x0020
#define	OMAP_FIXED		0x0100
#define	OMAP_NOEXTEND		0x0200
#define	OMAP_HASSEMAPHORE	0x0400
#define	OMAP_INHERIT		0x0800

	ma.addr = uap->addr;
	ma.flags = 0;
	if (uap->flags & OMAP_ANON)
		ma.flags |= MAP_ANON;	/* else MAP_FILE, the default */
	if (uap->flags & OMAP_COPY)
		ma.flags |= MAP_COPY;
	else if (uap->flags & OMAP_SHARED)
		ma.flags |= MAP_SHARED;
	else
		ma.flags |= MAP_PRIVATE;
	if (uap->flags & OMAP_FIXED)
		ma.flags |= MAP_FIXED;
	if (uap->flags & OMAP_NOEXTEND)
		ma.flags |= MAP_NOEXTEND;
	if (uap->flags & OMAP_HASSEMAPHORE)
		ma.flags |= MAP_HASSEMAPHORE;
	if (uap->flags & OMAP_INHERIT)
		ma.flags |= MAP_INHERIT;

	return(smmap(p, &ma, retval));
}
#endif /* defined(COMPAT_43) || defined(COMPAT_SUNOS) */

struct msync_args {
	caddr_t	addr;
	int	len;
};

int
msync(p, uap, retval)
	struct proc *p;
	struct msync_args *uap;
	int *retval;
{
	vm_offset_t addr, objoff, oaddr;
	vm_size_t size, osize;
	vm_prot_t prot, mprot;
	vm_inherit_t inherit;
	vm_object_t object;
	boolean_t shared;
	int rv;

#ifdef DEBUG
	if (mmapdebug & (MDB_FOLLOW|MDB_SYNC))
		printf("msync(%d): addr %x len %x\n",
		       p->p_pid, uap->addr, uap->len);
#endif
	if (((int)uap->addr & page_mask) || uap->len < 0)
		return(EINVAL);
	addr = oaddr = (vm_offset_t)uap->addr;
	osize = (vm_size_t)uap->len;
	/*
	 * Region must be entirely contained in a single entry
	 */
	if (!vm_map_is_allocated(&p->p_vmspace->vm_map, addr, addr+osize,
	    TRUE))
		return(EINVAL);
	/*
	 * Determine the object associated with that entry
	 * (object is returned locked on KERN_SUCCESS)
	 */
	rv = vm_region(&p->p_vmspace->vm_map, &addr, &size, &prot, &mprot,
		       &inherit, &shared, &object, &objoff);
	if (rv != KERN_SUCCESS)
		return(EINVAL);
#ifdef DEBUG
	if (mmapdebug & MDB_SYNC)
		printf("msync: region: object %x addr %x size %d objoff %d\n",
		       object, addr, size, objoff);
#endif
	/*
	 * Do not msync non-vnoded backed objects.
	 */
	if ((object->flags & OBJ_INTERNAL) || object->pager == NULL ||
	    object->pager->pg_type != PG_VNODE) {
		vm_object_unlock(object);
		return(EINVAL);
	}
	objoff += oaddr - addr;
	if (osize == 0)
		osize = size;
#ifdef DEBUG
	if (mmapdebug & MDB_SYNC)
		printf("msync: cleaning/flushing object range [%x-%x)\n",
		       objoff, objoff+osize);
#endif
	if (prot & VM_PROT_WRITE)
		vm_object_page_clean(object, objoff, objoff+osize);
	/*
	 * (XXX)
	 * Bummer, gotta flush all cached pages to ensure
	 * consistency with the file system cache.
	 */
	vm_object_page_remove(object, objoff, objoff+osize);
	vm_object_unlock(object);
	return(0);
}

struct munmap_args {
	caddr_t	addr;
	int	len;
};

int
munmap(p, uap, retval)
	register struct proc *p;
	register struct munmap_args *uap;
	int *retval;
{
	vm_offset_t addr;
	vm_size_t size;

#ifdef DEBUG
	if (mmapdebug & MDB_FOLLOW)
		printf("munmap(%d): addr %x len %x\n",
		       p->p_pid, uap->addr, uap->len);
#endif
	addr = (vm_offset_t) uap->addr;
	if ((addr & page_mask) || uap->len < 0)
		return(EINVAL);
	size = (vm_size_t) round_page(uap->len);
	if (size == 0)
		return(0);
	if (addr + size >= VM_MAXUSER_ADDRESS)
	    return(EINVAL);
	if (!vm_map_is_allocated(&p->p_vmspace->vm_map, addr, addr+size,
	    FALSE))
		return(EINVAL);
	/* returns nothing but KERN_SUCCESS anyway */
	(void) vm_map_remove(&p->p_vmspace->vm_map, addr, addr+size);
	return(0);
}

void
munmapfd(p, fd)
	register struct proc *p;
	int fd;
{
#ifdef DEBUG
	if (mmapdebug & MDB_FOLLOW)
		printf("munmapfd(%d): fd %d\n", p->p_pid, fd);
#endif

	/*
	 * XXX -- should vm_deallocate any regions mapped to this file
	 */
	p->p_fd->fd_ofileflags[fd] &= ~UF_MAPPED;
}

struct mprotect_args {
	caddr_t	addr;
	int	len;
	int	prot;
};

int
mprotect(p, uap, retval)
	struct proc *p;
	struct mprotect_args *uap;
	int *retval;
{
	vm_offset_t addr;
	vm_size_t size;
	register vm_prot_t prot;

#ifdef DEBUG
	if (mmapdebug & MDB_FOLLOW)
		printf("mprotect(%d): addr %x len %x prot %d\n",
		       p->p_pid, uap->addr, uap->len, uap->prot);
#endif
	addr = (vm_offset_t) uap->addr;
	if ((addr & page_mask) || uap->len < 0)
		return(EINVAL);
	size = (vm_size_t) uap->len;
	/*
	 * Map protections
	 */
	prot = VM_PROT_NONE;
	if (uap->prot & PROT_READ)
		prot |= VM_PROT_READ;
	if (uap->prot & PROT_WRITE)
		prot |= VM_PROT_WRITE;
	if (uap->prot & PROT_EXEC)
		prot |= VM_PROT_EXECUTE;
	switch (vm_map_protect(&p->p_vmspace->vm_map, addr, addr+size, prot,
	    FALSE)) {
	case KERN_SUCCESS:
		return (0);
	case KERN_PROTECTION_FAILURE:
		return (EACCES);
	}
	return(EINVAL);
}

struct madvise_args {
	caddr_t	addr;
	int	len;
	int	behav;
};

/* ARGSUSED */
int
madvise(p, uap, retval)
	struct proc *p;
	struct madvise_args *uap;
	int *retval;
{

	/* Not yet implemented */
	return (EOPNOTSUPP);
}

struct mincore_args {
	caddr_t	addr;
	int	len;
	char	*vec;
};

/* ARGSUSED */
int
mincore(p, uap, retval)
	struct proc *p;
	struct mincore_args *uap;
	int *retval;
{

	/* Not yet implemented */
	return (EOPNOTSUPP);
}

/*
 * Internal version of mmap.
 * Currently used by mmap, exec, and sys5 shared memory.
 * Handle is:
 *	MAP_FILE: a vnode pointer
 *	MAP_ANON: NULL or a file pointer
 */
int
vm_mmap(map, addr, size, prot, maxprot, flags, handle, foff)
	register vm_map_t map;
	register vm_offset_t *addr;
	register vm_size_t size;
	vm_prot_t prot;
	vm_prot_t maxprot;
	register int flags;
	caddr_t handle;		/* XXX should be vp */
	vm_offset_t foff;
{
	register vm_pager_t pager;
	boolean_t fitit;
	vm_object_t object;
	struct vnode *vp;
	int type;
	int rv = KERN_SUCCESS;

	if (size == 0)
		return (0);
	if ((flags & MAP_FIXED) == 0) {
		fitit = TRUE;
		*addr = round_page(*addr);
	} else {
		fitit = FALSE;
		(void) vm_deallocate(map, *addr, size);
	}
	/*
	 * Lookup/allocate pager.  All except an unnamed anonymous lookup
	 * gain a reference to ensure continued existance of the object.
	 * (XXX the exception is to appease the pageout daemon)
	 */
	if (flags & MAP_ANON)
		type = PG_DFLT;
	else {
		vp = (struct vnode *)handle;
		if (vp->v_type == VCHR) {
			type = PG_DEVICE;
			handle = (caddr_t)(long)vp->v_rdev;
		} else
			type = PG_VNODE;
	}
	pager = vm_pager_allocate(type, handle, size, prot, foff);
	if (pager == NULL)
		return (type == PG_DEVICE ? EINVAL : ENOMEM);
	/*
	 * Find object and release extra reference gained by lookup
	 */
	object = vm_object_lookup(pager);
	vm_object_deallocate(object);
	/*
	 * Anonymous memory.
	 */
	if (flags & MAP_ANON) {
		rv = vm_allocate_with_pager(map, addr, size, fitit,
					    pager, (vm_offset_t)foff, TRUE);
		if (rv != KERN_SUCCESS) {
			if (handle == NULL)
				vm_pager_deallocate(pager);
			else
				vm_object_deallocate(object);
			goto out;
		}
		/*
		 * The object of unnamed anonymous regions was just created
		 * find it for pager_cache.
		 */
		if (handle == NULL)
			object = vm_object_lookup(pager);
		/*
		 * Don't cache anonymous objects.
		 * Loses the reference gained by vm_pager_allocate.
		 */
		(void) pager_cache(object, FALSE);
#ifdef DEBUG
		if (mmapdebug & MDB_MAPIT)
			printf("vm_mmap(%d): ANON *addr %x size %x pager %x\n",
			       curproc->p_pid, *addr, size, pager);
#endif
	}
	/*
	 * Must be type MAP_FILE.
	 * Distinguish between character special and regular files.
	 */
	else if (vp->v_type == VCHR) {
		rv = vm_allocate_with_pager(map, addr, size, fitit,
					    pager, (vm_offset_t)foff, FALSE);
		/*
		 * Uncache the object and lose the reference gained
		 * by vm_pager_allocate().  If the call to
		 * vm_allocate_with_pager() was sucessful, then we
		 * gained an additional reference ensuring the object
		 * will continue to exist.  If the call failed then
		 * the deallocate call below will terminate the
		 * object which is fine.
		 */
		(void) pager_cache(object, FALSE);
		if (rv != KERN_SUCCESS)
			goto out;
	}
	/*
	 * A regular file
	 */
	else {
#ifdef DEBUG
		if (object == NULL)
			printf("vm_mmap: no object: vp %x, pager %x\n",
			       vp, pager);
#endif
		/*
		 * Map it directly.
		 * Allows modifications to go out to the vnode.
		 */
		if (flags & MAP_SHARED) {
			rv = vm_allocate_with_pager(map, addr, size,
						    fitit, pager,
						    (vm_offset_t)foff, FALSE);
			if (rv != KERN_SUCCESS) {
				vm_object_deallocate(object);
				goto out;
			}
			/*
			 * Don't cache the object.  This is the easiest way
			 * of ensuring that data gets back to the filesystem
			 * because vnode_pager_deallocate() will fsync the
			 * vnode.  pager_cache() will lose the extra ref.
			 */
			if (prot & VM_PROT_WRITE)
				pager_cache(object, FALSE);
			else
				vm_object_deallocate(object);
		}
		/*
		 * Copy-on-write of file.  Two flavors.
		 * MAP_COPY is true COW, you essentially get a snapshot of
		 * the region at the time of mapping.  MAP_PRIVATE means only
		 * that your changes are not reflected back to the object.
		 * Changes made by others will be seen.
		 */
		else {
			vm_map_t tmap;
			vm_offset_t off;

			/* locate and allocate the target address space */
			rv = vm_map_find(map, NULL, (vm_offset_t)0,
					 addr, size, fitit);
			if (rv != KERN_SUCCESS) {
				vm_object_deallocate(object);
				goto out;
			}
			tmap = vm_map_create(pmap_create(size), VM_MIN_ADDRESS,
					     VM_MIN_ADDRESS+size, TRUE);
			off = VM_MIN_ADDRESS;
			rv = vm_allocate_with_pager(tmap, &off, size,
						    TRUE, pager,
						    (vm_offset_t)foff, FALSE);
			if (rv != KERN_SUCCESS) {
				vm_object_deallocate(object);
				vm_map_deallocate(tmap);
				goto out;
			}
			/*
			 * (XXX)
			 * MAP_PRIVATE implies that we see changes made by
			 * others.  To ensure that we need to guarentee that
			 * no copy object is created (otherwise original
			 * pages would be pushed to the copy object and we
			 * would never see changes made by others).  We
			 * totally sleeze it right now by marking the object
			 * internal temporarily.
			 */
			if ((flags & MAP_COPY) == 0)
				object->flags |= OBJ_INTERNAL;
			rv = vm_map_copy(map, tmap, *addr, size, off,
					 FALSE, FALSE);
			object->flags &= ~OBJ_INTERNAL;
			/*
			 * (XXX)
			 * My oh my, this only gets worse...
			 * Force creation of a shadow object so that
			 * vm_map_fork will do the right thing.
			 */
			if ((flags & MAP_COPY) == 0) {
				vm_map_t tmap;
				vm_map_entry_t tentry;
				vm_object_t tobject;
				vm_offset_t toffset;
				vm_prot_t tprot;
				boolean_t twired, tsu;

				tmap = map;
				vm_map_lookup(&tmap, *addr, VM_PROT_WRITE,
					      &tentry, &tobject, &toffset,
					      &tprot, &twired, &tsu);
				vm_map_lookup_done(tmap, tentry);
			}
			/*
			 * (XXX)
			 * Map copy code cannot detect sharing unless a
			 * sharing map is involved.  So we cheat and write
			 * protect everything ourselves.
			 */
			vm_object_pmap_copy(object, (vm_offset_t)foff,
					    (vm_offset_t)foff+size);
			vm_object_deallocate(object);
			vm_map_deallocate(tmap);
			if (rv != KERN_SUCCESS)
				goto out;
		}
#ifdef DEBUG
		if (mmapdebug & MDB_MAPIT)
			printf("vm_mmap(%d): FILE *addr %x size %x pager %x\n",
			       curproc->p_pid, *addr, size, pager);
#endif
	}
	/*
	 * We only need to set max_protection in case it's
	 * unequal to its default, which is VM_PROT_DEFAULT.
	 */
	if (maxprot != VM_PROT_DEFAULT) {
		rv = vm_map_protect(map, *addr, *addr+size, maxprot, TRUE);
		if (rv != KERN_SUCCESS) {
			(void) vm_deallocate(map, *addr, size);
			goto out;
		}
	}
	/*
	 * We only need to set the current protection if it's different
	 * from the maximum.  If it is equal to the maximum, then the
	 * vm_map_protect() above would have set it.
	 */
	if (prot != maxprot) {
		rv = vm_map_protect(map, *addr, *addr+size, prot, FALSE);
		if (rv != KERN_SUCCESS) {
			(void) vm_deallocate(map, *addr, size);
			goto out;
		}
	}
	/*
	 * Shared memory is also shared with children.
	 */
	if (flags & MAP_SHARED) {
		rv = vm_inherit(map, *addr, size, VM_INHERIT_SHARE);
		if (rv != KERN_SUCCESS) {
			(void) vm_deallocate(map, *addr, size);
			goto out;
		}
	}
out:
#ifdef DEBUG
	if (mmapdebug & MDB_MAPIT)
		printf("vm_mmap: rv %d\n", rv);
#endif
	switch (rv) {
	case KERN_SUCCESS:
		return (0);
	case KERN_INVALID_ADDRESS:
	case KERN_NO_SPACE:
		return (ENOMEM);
	case KERN_PROTECTION_FAILURE:
		return (EACCES);
	default:
		return (EINVAL);
	}
}

/*
 * Internal bastardized version of MACHs vm_region system call.
 * Given address and size it returns map attributes as well
 * as the (locked) object mapped at that location. 
 */
int
vm_region(map, addr, size, prot, max_prot, inheritance, shared, object, objoff)
	vm_map_t	map;
	vm_offset_t	*addr;		/* IN/OUT */
	vm_size_t	*size;		/* OUT */
	vm_prot_t	*prot;		/* OUT */
	vm_prot_t	*max_prot;	/* OUT */
	vm_inherit_t	*inheritance;	/* OUT */
	boolean_t	*shared;	/* OUT */
	vm_object_t	*object;	/* OUT */
	vm_offset_t	*objoff;	/* OUT */
{
	vm_map_entry_t	tmp_entry;
	register
	vm_map_entry_t	entry;
	register
	vm_offset_t	tmp_offset;
	vm_offset_t	start;

	if (map == NULL)
		return(KERN_INVALID_ARGUMENT);
	
	start = *addr;

	vm_map_lock_read(map);
	if (!vm_map_lookup_entry(map, start, &tmp_entry)) {
		if ((entry = tmp_entry->next) == &map->header) {
			vm_map_unlock_read(map);
		   	return(KERN_NO_SPACE);
		}
		start = entry->start;
		*addr = start;
	} else
		entry = tmp_entry;

	*prot = entry->protection;
	*max_prot = entry->max_protection;
	*inheritance = entry->inheritance;

	tmp_offset = entry->offset + (start - entry->start);
	*size = (entry->end - start);

	if (entry->is_a_map) {
		register vm_map_t share_map;
		vm_size_t share_size;

		share_map = entry->object.share_map;

		vm_map_lock_read(share_map);
		(void) vm_map_lookup_entry(share_map, tmp_offset, &tmp_entry);

		if ((share_size = (tmp_entry->end - tmp_offset)) < *size)
			*size = share_size;

		vm_object_lock(tmp_entry->object);
		*object = tmp_entry->object.vm_object;
		*objoff = tmp_entry->offset + (tmp_offset - tmp_entry->start);

		*shared = (share_map->ref_count != 1);
		vm_map_unlock_read(share_map);
	} else {
		vm_object_lock(entry->object);
		*object = entry->object.vm_object;
		*objoff = tmp_offset;

		*shared = FALSE;
	}

	vm_map_unlock_read(map);

	return(KERN_SUCCESS);
}

/*
 * Yet another bastard routine.
 */
int
vm_allocate_with_pager(map, addr, size, fitit, pager, poffset, internal)
	register vm_map_t	map;
	register vm_offset_t	*addr;
	register vm_size_t	size;
	boolean_t		fitit;
	vm_pager_t		pager;
	vm_offset_t		poffset;
	boolean_t		internal;
{
	register vm_object_t	object;
	register int		result;

	if (map == NULL)
		return(KERN_INVALID_ARGUMENT);

	*addr = trunc_page(*addr);
	size = round_page(size);

	/*
	 *	Lookup the pager/paging-space in the object cache.
	 *	If it's not there, then create a new object and cache
	 *	it.
	 */
	object = vm_object_lookup(pager);
	cnt.v_lookups++;
	if (object == NULL) {
		object = vm_object_allocate(size);
		/*
		 * From Mike Hibler: "unnamed anonymous objects should never
		 * be on the hash list ... For now you can just change
		 * vm_allocate_with_pager to not do vm_object_enter if this
		 * is an internal object ..."
		 */
		if (!internal)
			vm_object_enter(object, pager);
	} else
		cnt.v_hits++;
	if (internal)
		object->flags |= OBJ_INTERNAL;
	else
		object->flags &= ~OBJ_INTERNAL;

	result = vm_map_find(map, object, poffset, addr, size, fitit);
	if (result != KERN_SUCCESS)
		vm_object_deallocate(object);
	else if (pager != NULL)
		vm_object_setpager(object, pager, (vm_offset_t) 0, TRUE);
	return(result);
}

/*
 * XXX: this routine belongs in vm_map.c.
 *
 * Returns TRUE if the range [start - end) is allocated in either
 * a single entry (single_entry == TRUE) or multiple contiguous
 * entries (single_entry == FALSE).
 *
 * start and end should be page aligned.
 */
static boolean_t
vm_map_is_allocated(map, start, end, single_entry)
	vm_map_t map;
	vm_offset_t start, end;
	boolean_t single_entry;
{
	vm_map_entry_t mapent;
	register vm_offset_t nend;

	vm_map_lock_read(map);

	/*
	 * Start address not in any entry
	 */
	if (!vm_map_lookup_entry(map, start, &mapent)) {
		vm_map_unlock_read(map);
		return (FALSE);
	}
	/*
	 * Find the maximum stretch of contiguously allocated space
	 */
	nend = mapent->end;
	if (!single_entry) {
		mapent = mapent->next;
		while (mapent != &map->header && mapent->start == nend) {
			nend = mapent->end;
			mapent = mapent->next;
		}
	}

	vm_map_unlock_read(map);
	return (end <= nend);
}
