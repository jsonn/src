/*	$NetBSD: vm_extern.h,v 1.38.2.1 1998/07/30 14:04:19 eeh Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)vm_extern.h	8.5 (Berkeley) 5/3/95
 */

#if defined(_KERNEL) && !defined(_LKM)
#include "opt_uvm.h"
#endif

struct buf;
struct loadavg;
struct proc;
struct pmap;
struct vmspace;
struct vmtotal;
struct mount;
struct vnode;
struct core;

#if defined(KGDB) && !defined(UVM)
void		 chgkprot __P((caddr_t, size_t, int));
#endif

#ifdef _KERNEL
#ifdef TYPEDEF_FOR_UAP
int		 compat_43_getpagesize __P((struct proc *p, void *, int *));
int		 madvise __P((struct proc *, void *, int *));
int		 mincore __P((struct proc *, void *, int *));
int		 mprotect __P((struct proc *, void *, int *));
int		 msync __P((struct proc *, void *, int *));
int		 munmap __P((struct proc *, void *, int *));
int		 obreak __P((struct proc *, void *, int *));
int		 sbrk __P((struct proc *, void *, int *));
int		 smmap __P((struct proc *, void *, int *));
int		 sstk __P((struct proc *, void *, int *));
#endif

void		 assert_wait __P((void *, boolean_t));
#if !defined(UVM)
int		 grow __P((struct proc *, vaddr_t));
#endif
void		 iprintf __P((void (*)(const char *, ...), const char *, ...));
#if !defined(UVM)
int		 kernacc __P((caddr_t, size_t, int));
#endif
int		 kinfo_loadavg __P((int, char *, int *, int, int *));
int		 kinfo_meter __P((int, caddr_t, int *, int, int *));
vaddr_t	 kmem_alloc __P((vm_map_t, vsize_t));
vaddr_t	 kmem_alloc_pageable __P((vm_map_t, vsize_t));
vaddr_t	 kmem_alloc_wait __P((vm_map_t, vsize_t));
void		 kmem_free __P((vm_map_t, vaddr_t, vsize_t));
void		 kmem_free_wakeup __P((vm_map_t, vaddr_t, vsize_t));
void		 kmem_init __P((vaddr_t, vaddr_t));
vaddr_t	 kmem_malloc __P((vm_map_t, vsize_t, boolean_t));
vm_map_t	 kmem_suballoc __P((vm_map_t, vaddr_t *, vaddr_t *,
				    vsize_t, boolean_t));
vaddr_t	 kmem_alloc_poolpage __P((void));
void		 kmem_free_poolpage __P((vaddr_t));
void		 loadav __P((struct loadavg *));
#if !defined(UVM)
void		 munmapfd __P((struct proc *, int));
#endif
int		 pager_cache __P((vm_object_t, boolean_t));
void		 sched __P((void));
#if !defined(UVM)
__dead void	 scheduler __P((void)) __attribute__((noreturn));
#endif
int		 svm_allocate __P((struct proc *, void *, int *));
int		 svm_deallocate __P((struct proc *, void *, int *));
int		 svm_inherit __P((struct proc *, void *, int *));
int		 svm_protect __P((struct proc *, void *, int *));
void		 swapinit __P((void));
#if !defined(UVM)
void		 swapout __P((struct proc *));
void		 swapout_threads __P((void));
#endif
int		 swfree __P((struct proc *, int));
void		 swstrategy __P((struct buf *));
void		 thread_block __P((char *));
void		 thread_sleep_msg __P((void *, simple_lock_t,
		     boolean_t, char *, int));

/*
 * This define replaces the thread_wakeup prototype, as thread_wakeup
 * was solely a wrapper around wakeup.
 *
 * void           thread_wakeup __P((void *));
 */
#define		 thread_wakeup wakeup
#if !defined(UVM)
int		 useracc __P((caddr_t, size_t, int));
int		 vm_allocate __P((vm_map_t, vaddr_t *, vsize_t,
				  boolean_t));
int		 vm_allocate_with_pager __P((vm_map_t, vaddr_t *,
		    vsize_t, boolean_t, vm_pager_t, vaddr_t, boolean_t));
int		 vm_coredump __P((struct proc *, struct vnode *, struct ucred *,
				  struct core *));
int		 vm_deallocate __P((vm_map_t, vaddr_t, vsize_t));
#endif
int		 vm_fault __P((vm_map_t, vaddr_t, vm_prot_t, boolean_t));
void		 vm_fault_copy_entry __P((vm_map_t,
		    vm_map_t, vm_map_entry_t, vm_map_entry_t));
void		 vm_fault_unwire __P((vm_map_t, vaddr_t, vaddr_t));
int		 vm_fault_wire __P((vm_map_t, vaddr_t, vaddr_t));
#if !defined(UVM)
void		 vm_fork __P((struct proc *, struct proc *, boolean_t));
#endif
int		 vm_inherit __P((vm_map_t,
		    vaddr_t, vsize_t, vm_inherit_t));
#if !defined(UVM)
void		 vm_init_limits __P((struct proc *));
#endif
void		 vm_mem_init __P((void));
#if !defined(UVM)
int		 vm_mmap __P((vm_map_t, vaddr_t *, vsize_t,
		    vm_prot_t, vm_prot_t, int, caddr_t, vaddr_t));
#endif
int		 vm_protect __P((vm_map_t,
		    vaddr_t, vsize_t, boolean_t, vm_prot_t));
void		 vm_set_page_size __P((void));
int		 vm_sysctl __P((int *, u_int, void *, size_t *, void *,
		    size_t, struct proc *));
void		 vmmeter __P((void));
#if !defined(UVM)
struct vmspace	*vmspace_alloc __P((vaddr_t, vaddr_t, boolean_t));
struct vmspace	*vmspace_fork __P((struct vmspace *));
void		 vmspace_exec __P((struct proc *));
void		 vmspace_free __P((struct vmspace *));
void		 vmspace_init __P((struct vmspace *, struct pmap *,
		    vaddr_t, vaddr_t, boolean_t));
void		 vmspace_share __P((struct proc *, struct proc *));
void		 vmspace_unshare __P((struct proc *));
#endif
void		 vmtotal __P((struct vmtotal *));
void		 vnode_pager_setsize __P((struct vnode *, u_quad_t));
void		 vnode_pager_sync __P((struct mount *));
void		 vnode_pager_umount __P((struct mount *));
boolean_t	 vnode_pager_uncache __P((struct vnode *));
#if !defined(UVM)
void		 vslock __P((struct proc *, caddr_t, size_t));
void		 vsunlock __P((struct proc *, caddr_t, size_t));
#endif

/* Machine dependent portion */
void		vmapbuf __P((struct buf *, vsize_t));
void		vunmapbuf __P((struct buf *, vsize_t));
void		pagemove __P((caddr_t, caddr_t, size_t));
void		cpu_fork __P((struct proc *, struct proc *));
#ifndef	cpu_swapin
void		cpu_swapin __P((struct proc *));
#endif
#ifndef	cpu_swapout
void		cpu_swapout __P((struct proc *));
#endif

#endif
