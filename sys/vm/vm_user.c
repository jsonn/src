/*	$NetBSD: vm_user.c,v 1.13.20.1 1998/07/30 14:04:26 eeh Exp $	*/

/* 
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
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
 *	@(#)vm_user.c	8.2 (Berkeley) 1/12/94
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Avadis Tevanian, Jr., Michael Wayne Young
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 *	User-exported virtual memory functions.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <vm/vm.h>

simple_lock_data_t	vm_alloc_lock;	/* XXX */

#ifdef MACHVMCOMPAT
/*
 * BSD style syscall interfaces to MACH calls
 * All return MACH return values.
 */
struct svm_allocate_args {
	vm_map_t map;
	vaddr_t *addr;
	vsize_t size;
	boolean_t anywhere;
};
/* ARGSUSED */
int
svm_allocate(p, uap, retval)
	struct proc *p;
	struct svm_allocate_args *uap;
	register_t *retval;
{
	vaddr_t addr;
	int rv;

	SCARG(uap, map) = p->p_map;		/* XXX */

	if (copyin((caddr_t)SCARG(uap, addr), (caddr_t)&addr, sizeof (addr)))
		rv = KERN_INVALID_ARGUMENT;
	else
		rv = vm_allocate(SCARG(uap, map), &addr, SCARG(uap, size),
		    SCARG(uap, anywhere));
	if (rv == KERN_SUCCESS) {
		if (copyout((caddr_t)&addr, (caddr_t)SCARG(uap, addr),
		    sizeof(addr)))
			rv = KERN_INVALID_ARGUMENT;
	}
	return((int)rv);
}

struct svm_deallocate_args {
	vm_map_t map;
	vaddr_t addr;
	vsize_t size;
};
/* ARGSUSED */
int
svm_deallocate(p, uap, retval)
	struct proc *p;
	struct svm_deallocate_args *uap;
	register_t *retval;
{
	int rv;

	SCARG(uap, map) = p->p_map;		/* XXX */
	rv = vm_deallocate(SCARG(uap, map), SCARG(uap, addr), SCARG(uap, size));
	return((int)rv);
}

struct svm_inherit_args {
	vm_map_t map;
	vaddr_t addr;
	vsize_t size;
	vm_inherit_t inherit;
};
/* ARGSUSED */
int
svm_inherit(p, uap, retval)
	struct proc *p;
	struct svm_inherit_args *uap;
	register_t *retval;
{
	int rv;

	SCARG(uap, map) = p->p_map;		/* XXX */
	rv = vm_inherit(SCARG(uap, map), SCARG(uap, addr), SCARG(uap, size),
	    SCARG(uap, inherit));
	return((int)rv);
}

struct svm_protect_args {
	vm_map_t map;
	vaddr_t addr;
	vsize_t size;
	boolean_t setmax;
	vm_prot_t prot;
};
/* ARGSUSED */
int
svm_protect(p, uap, retval)
	struct proc *p;
	struct svm_protect_args *uap;
	register_t *retval;
{
	int rv;

	SCARG(uap, map) = p->p_map;		/* XXX */
	rv = vm_protect(SCARG(uap, map), SCARG(uap, addr), SCARG(uap, size),
	    SCARG(uap, setmax), SCARG(uap, prot));
	return((int)rv);
}

/*
 *	vm_inherit sets the inheritence of the specified range in the
 *	specified map.
 */
int
vm_inherit(map, start, size, new_inheritance)
	register vm_map_t	map;
	vaddr_t		start;
	vsize_t		size;
	vm_inherit_t		new_inheritance;
{
	if (map == NULL)
		return(KERN_INVALID_ARGUMENT);

	return(vm_map_inherit(map, trunc_page(start), round_page(start+size), new_inheritance));
}

/*
 *	vm_protect sets the protection of the specified range in the
 *	specified map.
 */

int
vm_protect(map, start, size, set_maximum, new_protection)
	register vm_map_t	map;
	vaddr_t		start;
	vsize_t		size;
	boolean_t		set_maximum;
	vm_prot_t		new_protection;
{
	if (map == NULL)
		return(KERN_INVALID_ARGUMENT);

	return(vm_map_protect(map, trunc_page(start), round_page(start+size), new_protection, set_maximum));
}
#endif

/*
 *	vm_allocate allocates "zero fill" memory in the specfied
 *	map.
 */
int
vm_allocate(map, addr, size, anywhere)
	register vm_map_t	map;
	register vaddr_t	*addr;
	register vsize_t	size;
	boolean_t		anywhere;
{
	int	result;

	if (map == NULL)
		return(KERN_INVALID_ARGUMENT);
	if (size == 0) {
		*addr = 0;
		return(KERN_SUCCESS);
	}

	if (anywhere)
		*addr = vm_map_min(map);
	else
		*addr = trunc_page(*addr);
	size = round_page(size);

	result = vm_map_find(map, NULL, (vaddr_t) 0, addr, size, anywhere);

	return(result);
}

/*
 *	vm_deallocate deallocates the specified range of addresses in the
 *	specified address map.
 */
int
vm_deallocate(map, start, size)
	register vm_map_t	map;
	vaddr_t		start;
	vsize_t		size;
{
	if (map == NULL)
		return(KERN_INVALID_ARGUMENT);

	if (size == (vaddr_t) 0)
		return(KERN_SUCCESS);

	return(vm_map_remove(map, trunc_page(start), round_page(start+size)));
}

/*
 * Similar to vm_allocate but assigns an explicit pager.
 */
int
vm_allocate_with_pager(map, addr, size, anywhere, pager, poffset, internal)
	register vm_map_t	map;
	register vaddr_t	*addr;
	register vsize_t	size;
	boolean_t		anywhere;
	vm_pager_t		pager;
	vaddr_t		poffset;
	boolean_t		internal;
{
	register vm_object_t	object;
	register int		result;
	vaddr_t		start;

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
	else {
		object->flags &= ~OBJ_INTERNAL;
		cnt.v_nzfod -= atop(size);
	}

	start = *addr;
	vm_map_lock(map);
	if (anywhere) {
	again:
		if (vm_map_findspace(map, start, size, addr))
			result = KERN_NO_SPACE;
		else {
#ifdef	PMAP_PREFER
			PMAP_PREFER(poffset, addr);
#endif
			start = *addr;
			result = vm_map_insert(map, object, poffset,
					       start, start + size);
			if (result == KERN_NO_SPACE)
				goto again;
		}
	} else
		result = vm_map_insert(map, object, poffset,
				       start, start + size);
	vm_map_unlock(map);

	if (result != KERN_SUCCESS)
		vm_object_deallocate(object);
	else if (pager != NULL)
		vm_object_setpager(object, pager, (vaddr_t) 0, TRUE);
	return(result);
}
