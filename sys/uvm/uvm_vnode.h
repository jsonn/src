/*	$NetBSD: uvm_vnode.h,v 1.7.4.2 1999/07/01 23:55:18 thorpej Exp $	*/

/*
 *
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
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
 *      This product includes software developed by Charles D. Cranor and
 *      Washington University.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * from: Id: uvm_vnode.h,v 1.1.2.4 1997/10/03 21:18:24 chuck Exp
 */

#ifndef _UVM_UVM_VNODE_H_
#define _UVM_UVM_VNODE_H_

/*
 * uvm_vnode.h
 *
 * vnode handle into the VM system.
 */

/*
 * the uvm_vnode structure.   put at the top of the vnode data structure.
 * this allows:
 *   (struct vnode *) == (struct uvm_vnode *) == (struct uvm_object *)
 */

struct uvm_vnode {
	struct uvm_object u_obj;	/* the actual VM object */
	int u_flags;			/* flags */
	int u_nio;			/* number of running I/O requests */
	vsize_t u_size;			/* size of object */

	/* the following entry is locked by uvn_wl_lock */
	LIST_ENTRY(uvm_vnode) u_wlist;	/* list of writeable vnode objects */

	/* the following entry is locked by uvn_sync_lock */
	SIMPLEQ_ENTRY(uvm_vnode) u_syncq; /* vnode objects due for a "sync" */
};

/*
 * u_flags values
 */
#ifdef UBC
#define UVM_VNODE_RELKILL	VXLOCK
#define UVM_VNODE_WANTED	VXWANT
#define UVM_VNODE_IOSYNCWANTED	VBWAIT
#define UVM_VNODE_WRITEABLE	VDIRTY
#define UVM_VNODE_VNISLOCKED	VXLOCK
#define UVM_VNODE_BLOCKED	VXLOCK

#define UVM_VNODE_ALOCK		VXLOCK

#else
#define UVM_VNODE_VALID		0x001	/* we are attached to the vnode */
#define UVM_VNODE_CANPERSIST	0x002	/* we can persist after ref == 0 */
#define UVM_VNODE_ALOCK		0x004	/* uvn_attach is locked out */
#define UVM_VNODE_DYING		0x008	/* final detach/terminate in 
					   progress */
#define UVM_VNODE_RELKILL	0x010	/* uvn should be killed by releasepg
					   when final i/o is done */
#define UVM_VNODE_WANTED	0x020	/* someone is waiting for alock,
					   dying, or relkill to clear */
#define UVM_VNODE_VNISLOCKED	0x040	/* underlying vnode struct is locked
					   (valid when DYING is true) */
#define UVM_VNODE_IOSYNC	0x080	/* I/O sync in progress ... setter
					   sleeps on &uvn->u_nio */
#define UVM_VNODE_IOSYNCWANTED	0x100	/* a process is waiting for the
					   i/o sync to clear so it can do
					   i/o */
#define UVM_VNODE_WRITEABLE	0x200	/* uvn has pages that are writeable */

/*
 * UVM_VNODE_BLOCKED: any condition that should new processes from
 * touching the vnode [set WANTED and sleep to wait for it to clear]
 */
#define UVM_VNODE_BLOCKED (UVM_VNODE_ALOCK|UVM_VNODE_DYING|UVM_VNODE_RELKILL)

#endif /* _UVM_UVM_VNODE_H_ */
