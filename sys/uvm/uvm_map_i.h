/*	$NetBSD: uvm_map_i.h,v 1.18.2.1 2001/04/09 01:59:18 nathanw Exp $	*/

/* 
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * Copyright (c) 1991, 1993, The Regents of the University of California.  
 *
 * All rights reserved.
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
 *	This product includes software developed by Charles D. Cranor,
 *      Washington University, the University of California, Berkeley and 
 *      its contributors.
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
 *	@(#)vm_map.c    8.3 (Berkeley) 1/12/94
 * from: Id: uvm_map_i.h,v 1.1.2.1 1997/08/14 19:10:50 chuck Exp
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
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

#ifndef _UVM_UVM_MAP_I_H_
#define _UVM_UVM_MAP_I_H_

/*
 * uvm_map_i.h
 */

/*
 * inline functions [maybe]
 */

#if defined(UVM_MAP_INLINE) || defined(UVM_MAP)

/*
 * uvm_map_create: create map
 */

MAP_INLINE vm_map_t
uvm_map_create(pmap, min, max, flags)
	pmap_t pmap;
	vaddr_t min, max;
	int flags;
{
	vm_map_t result;

	MALLOC(result, vm_map_t,
	    (flags & VM_MAP_INTRSAFE) ? sizeof(struct vm_map_intrsafe) :
					sizeof(struct vm_map),
	    M_VMMAP, M_WAITOK);
	uvm_map_setup(result, min, max, flags);
	result->pmap = pmap;
	return(result);
}

/*
 * uvm_map_setup: init map
 *
 * => map must not be in service yet.
 */

MAP_INLINE void
uvm_map_setup(map, min, max, flags)
	vm_map_t map;
	vaddr_t min, max;
	int flags;
{

	map->header.next = map->header.prev = &map->header;
	map->nentries = 0;
	map->size = 0;
	map->ref_count = 1;
	map->min_offset = min;
	map->max_offset = max;
	map->flags = flags;
	map->first_free = &map->header;
	map->hint = &map->header;
	map->timestamp = 0;
	lockinit(&map->lock, PVM, "vmmaplk", 0, 0);
	simple_lock_init(&map->ref_lock);
	simple_lock_init(&map->hint_lock);
	simple_lock_init(&map->flags_lock);

	/*
	 * If the map is interrupt safe, place it on the list
	 * of interrupt safe maps, for uvm_fault().
	 *
	 * We almost never set up an interrupt-safe map, but we set
	 * up quite a few regular ones (at every fork!), so put
	 * interrupt-safe map setup in the slow path.
	 */
	if (__predict_false(flags & VM_MAP_INTRSAFE)) {
		struct vm_map_intrsafe *vmi = (struct vm_map_intrsafe *)map;
		int s;

		s = vmi_list_lock();
		LIST_INSERT_HEAD(&vmi_list, vmi, vmi_list);
		vmi_list_unlock(s);
	}
}


/*
 *   U N M A P   -   m a i n   e n t r y   p o i n t
 */

/*
 * uvm_unmap: remove mappings from a vm_map (from "start" up to "stop")
 *
 * => caller must check alignment and size 
 * => map must be unlocked (we will lock it)
 */

MAP_INLINE void
uvm_unmap(map, start, end)
	vm_map_t map;
	vaddr_t start,end;
{
	vm_map_entry_t dead_entries;
	UVMHIST_FUNC("uvm_unmap"); UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist, "  (map=0x%x, start=0x%x, end=0x%x)",
	    map, start, end, 0);
	/*
	 * work now done by helper functions.   wipe the pmap's and then
	 * detach from the dead entries...
	 */
	vm_map_lock(map);
	uvm_unmap_remove(map, start, end, &dead_entries);
	vm_map_unlock(map);

	if (dead_entries != NULL)
		uvm_unmap_detach(dead_entries, 0);

	UVMHIST_LOG(maphist, "<- done", 0,0,0,0);
}


/*
 * uvm_map_reference: add reference to a map
 *
 * => map need not be locked (we use ref_lock).
 */

MAP_INLINE void
uvm_map_reference(map)
	vm_map_t map;
{
	simple_lock(&map->ref_lock);
	map->ref_count++; 
	simple_unlock(&map->ref_lock);
}

/*
 * uvm_map_deallocate: drop reference to a map
 *
 * => caller must not lock map
 * => we will zap map if ref count goes to zero
 */

MAP_INLINE void
uvm_map_deallocate(map)
	vm_map_t map;
{
	int c;

	simple_lock(&map->ref_lock);
	c = --map->ref_count;
	simple_unlock(&map->ref_lock);
	if (c > 0) {
		return;
	}

	/*
	 * all references gone.   unmap and free.
	 */

	uvm_unmap(map, map->min_offset, map->max_offset);
	pmap_destroy(map->pmap);
	FREE(map, M_VMMAP);
}

#endif /* defined(UVM_MAP_INLINE) || defined(UVM_MAP) */

#endif /* _UVM_UVM_MAP_I_H_ */
