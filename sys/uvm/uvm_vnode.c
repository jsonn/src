/*	$NetBSD: uvm_vnode.c,v 1.17.2.1 1998/11/09 06:06:40 chs Exp $	*/

/*
 * XXXCDC: "ROUGH DRAFT" QUALITY UVM PRE-RELEASE FILE!   
 *         >>>USE AT YOUR OWN RISK, WORK IS NOT FINISHED<<<
 */
/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * Copyright (c) 1991, 1993
 *      The Regents of the University of California.  
 * Copyright (c) 1990 University of Utah.
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
 *      This product includes software developed by Charles D. Cranor,
 *	Washington University, the University of California, Berkeley and 
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
 *      @(#)vnode_pager.c       8.8 (Berkeley) 2/13/94
 * from: Id: uvm_vnode.c,v 1.1.2.26 1998/02/02 20:38:07 chuck Exp
 */

#include "fs_nfs.h"
#include "opt_uvmhist.h"

/*
 * uvm_vnode.c: the vnode pager.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/conf.h>

#include <miscfs/specfs/specdev.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>

#include <uvm/uvm.h>
#include <uvm/uvm_vnode.h>

/*
 * private global data structure
 *
 * we keep a list of writeable active vnode-backed VM objects for sync op.
 * we keep a simpleq of vnodes that are currently being sync'd.
 */

LIST_HEAD(uvn_list_struct, uvm_vnode);
static struct uvn_list_struct uvn_wlist;	/* writeable uvns */
static simple_lock_data_t uvn_wl_lock;		/* locks uvn_wlist */

SIMPLEQ_HEAD(uvn_sq_struct, uvm_vnode);
static struct uvn_sq_struct uvn_sync_q;		/* sync'ing uvns */
lock_data_t uvn_sync_lock;			/* locks sync operation */

/*
 * functions
 */

static int		   uvn_asyncget __P((struct uvm_object *, vaddr_t,
					    int));
struct uvm_object 	  *uvn_attach __P((void *, vm_prot_t));
static void		   uvn_cluster __P((struct uvm_object *, vaddr_t,
					   vaddr_t *, vaddr_t *));
static void                uvn_detach __P((struct uvm_object *));
static boolean_t           uvn_flush __P((struct uvm_object *, vaddr_t, 
					 vaddr_t, int));
static int                 uvn_get __P((struct uvm_object *, vaddr_t,
					vm_page_t *, int *, int, 
					vm_prot_t, int, int));
static void		   uvn_init __P((void));
static int		   uvn_put __P((struct uvm_object *, vm_page_t *,
					int, boolean_t));
static void                uvn_reference __P((struct uvm_object *));
static boolean_t	   uvn_releasepg __P((struct vm_page *, 
					      struct vm_page **));

/*
 * master pager structure
 */

struct uvm_pagerops uvm_vnodeops = {
	uvn_init,
	uvn_attach,
	uvn_reference,
	uvn_detach,
	NULL,			/* no specialized fault routine required */
	uvn_flush,
	uvn_get,
	uvn_asyncget,
	uvn_put,
	uvn_cluster,
	uvm_mk_pcluster, /* use generic version of this: see uvm_pager.c */
	uvm_shareprot,	 /* !NULL: allow us in share maps */
	NULL,		 /* AIO-DONE function (not until we have asyncio) */
	uvn_releasepg,
};

/*
 * the ops!
 */

/*
 * uvn_init
 *
 * init pager private data structures.
 */

static void
uvn_init()
{

	LIST_INIT(&uvn_wlist);
	simple_lock_init(&uvn_wl_lock);
	/* note: uvn_sync_q init'd in uvm_vnp_sync() */
	lockinit(&uvn_sync_lock, PVM, "uvnsync", 0, 0);
}

/*
 * uvn_attach
 *
 * attach a vnode structure to a VM object.  if the vnode is already
 * attached, then just bump the reference count by one and return the
 * VM object.   if not already attached, attach and return the new VM obj.
 * the "accessprot" tells the max access the attaching thread wants to
 * our pages.
 *
 * => caller must _not_ already be holding the lock on the uvm_object.
 * => in fact, nothing should be locked so that we can sleep here.
 * => note that uvm_object is first thing in vnode structure, so their
 *    pointers are equiv.
 */

struct uvm_object *
uvn_attach(arg, accessprot)
	void *arg;
	vm_prot_t accessprot;
{
	struct vnode *vp = arg;
	struct uvm_vnode *uvn = &vp->v_uvm;
	struct vattr vattr;
	int oldflags, result;
	struct partinfo pi;
	off_t used_vnode_size;
	UVMHIST_FUNC("uvn_attach"); UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist, "(vn=0x%x)", arg,0,0,0);

	used_vnode_size = (u_quad_t)0;	/* XXX gcc -Wuninitialized */

	/*
	 * first get a lock on the uvn.
	 */
	simple_lock(&uvn->u_obj.vmobjlock);
	while (uvn->u_flags & UVM_VNODE_BLOCKED) {
		uvn->u_flags |= UVM_VNODE_WANTED;
		UVMHIST_LOG(maphist, "  SLEEPING on blocked vn",0,0,0,0);
		UVM_UNLOCK_AND_WAIT(uvn, &uvn->u_obj.vmobjlock, FALSE,
		    "uvn_attach", 0);
		simple_lock(&uvn->u_obj.vmobjlock);
		UVMHIST_LOG(maphist,"  WOKE UP",0,0,0,0);
	}

	/*
	 * if we're mapping a BLK device, make sure it is a disk.
	 */
	if (vp->v_type == VBLK && bdevsw[major(vp->v_rdev)].d_type != D_DISK) {
		simple_unlock(&uvn->u_obj.vmobjlock);
		UVMHIST_LOG(maphist,"<- done (VBLK not D_DISK!)", 0,0,0,0);
		return(NULL);
	}

#ifdef UBC
	oldflags = 0;


#ifdef DIAGNOSTIC
	if (vp->v_type != VREG) {
		panic("uvn_attach: vp %p not VREG", vp);
	}
#endif

	/*
	 * set up our idea of the size
	 * if this hasn't been done already.
	 */
	if (uvn->u_size == VSIZENOTSET) {

	uvn->u_flags = UVM_VNODE_ALOCK;
	simple_unlock(&uvn->u_obj.vmobjlock); /* drop lock in case we sleep */
		/* XXX: curproc? */
	if (vp->v_type == VBLK) {
		/*
		 * We could implement this as a specfs getattr call, but:
		 *
		 *	(1) VOP_GETATTR() would get the file system
		 *	    vnode operation, not the specfs operation.
		 *
		 *	(2) All we want is the size, anyhow.
		 */
		result = (*bdevsw[major(vp->v_rdev)].d_ioctl)(vp->v_rdev,
		    DIOCGPART, (caddr_t)&pi, FREAD, curproc);
		if (result == 0) {
			/* XXX should remember blocksize */
			used_vnode_size = (u_quad_t)pi.disklab->d_secsize *
			    (u_quad_t)pi.part->p_size;
		}
	} else {
		result = VOP_GETATTR(vp, &vattr, curproc->p_ucred, curproc);
		if (result == 0)
			used_vnode_size = vattr.va_size;
	}


	/*
	 * make sure that the newsize fits within a vaddr_t
	 * XXX: need to revise addressing data types
	 */
	if (used_vnode_size > (vaddr_t) -PAGE_SIZE) {
#ifdef DEBUG
		printf("uvn_attach: vn %p size truncated %qx->%x\n", vp,
		    used_vnode_size, -PAGE_SIZE);
#endif    
		used_vnode_size = (vaddr_t) -PAGE_SIZE;
	}

	/* relock object */
	simple_lock(&uvn->u_obj.vmobjlock);

	if (uvn->u_flags & UVM_VNODE_WANTED)
		wakeup(uvn);
	uvn->u_flags = 0;

	if (result != 0) {
		simple_unlock(&uvn->u_obj.vmobjlock); /* drop lock */
		UVMHIST_LOG(maphist,"<- done (VOP_GETATTR FAILED!)", 0,0,0,0);
		return(NULL);
	}
	uvn->u_size = used_vnode_size;

	}

		/* check for new writeable uvn */
		if ((accessprot & VM_PROT_WRITE) != 0 && 
		    (uvn->u_flags & UVM_VNODE_WRITEABLE) == 0) {
			simple_lock(&uvn_wl_lock);

			if (uvn->u_wlist.le_next != NULL) {
				printf("already on wlist vp %p\n", uvn);
				Debugger();
			}

			LIST_INSERT_HEAD(&uvn_wlist, uvn, u_wlist);
			simple_unlock(&uvn_wl_lock);
			/* we are now on wlist! */
			uvn->u_flags |= UVM_VNODE_WRITEABLE;
		}

		/* unlock and return */
		simple_unlock(&uvn->u_obj.vmobjlock);
		UVMHIST_LOG(maphist,"<- done, refcnt=%d", uvn->u_obj.uo_refs,
		    0, 0, 0);
		return (&uvn->u_obj);
#else
	/*
	 * now we have lock and uvn must not be in a blocked state.
	 * first check to see if it is already active, in which case
	 * we can bump the reference count, check to see if we need to
	 * add it to the writeable list, and then return.
	 */
	if (uvn->u_flags & UVM_VNODE_VALID) {	/* already active? */

		/* regain VREF if we were persisting */
		if (uvn->u_obj.uo_refs == 0) {
			VREF(vp);
			UVMHIST_LOG(maphist," VREF (reclaim persisting vnode)",
			    0,0,0,0);
		}
		uvn->u_obj.uo_refs++;		/* bump uvn ref! */

		/* check for new writeable uvn */
		if ((accessprot & VM_PROT_WRITE) != 0 && 
		    (uvn->u_flags & UVM_VNODE_WRITEABLE) == 0) {
			simple_lock(&uvn_wl_lock);
			LIST_INSERT_HEAD(&uvn_wlist, uvn, u_wlist);
			simple_unlock(&uvn_wl_lock);
			/* we are now on wlist! */
			uvn->u_flags |= UVM_VNODE_WRITEABLE;
		}

		/* unlock and return */
		simple_unlock(&uvn->u_obj.vmobjlock);
		UVMHIST_LOG(maphist,"<- done, refcnt=%d", uvn->u_obj.uo_refs,
		    0, 0, 0);
		return (&uvn->u_obj);
	} 

	/*
	 * need to call VOP_GETATTR() to get the attributes, but that could
	 * block (due to I/O), so we want to unlock the object before calling.
	 * however, we want to keep anyone else from playing with the object
	 * while it is unlocked.   to do this we set UVM_VNODE_ALOCK which
	 * prevents anyone from attaching to the vnode until we are done with
	 * it.
	 */
	uvn->u_flags = UVM_VNODE_ALOCK;
	simple_unlock(&uvn->u_obj.vmobjlock); /* drop lock in case we sleep */
		/* XXX: curproc? */

	if (vp->v_type == VBLK) {
		/*
		 * We could implement this as a specfs getattr call, but:
		 *
		 *	(1) VOP_GETATTR() would get the file system
		 *	    vnode operation, not the specfs operation.
		 *
		 *	(2) All we want is the size, anyhow.
		 */
		result = (*bdevsw[major(vp->v_rdev)].d_ioctl)(vp->v_rdev,
		    DIOCGPART, (caddr_t)&pi, FREAD, curproc);
		if (result == 0) {
			/* XXX should remember blocksize */
			used_vnode_size = (u_quad_t)pi.disklab->d_secsize *
			    (u_quad_t)pi.part->p_size;
		}
	} else {
		result = VOP_GETATTR(vp, &vattr, curproc->p_ucred, curproc);
		if (result == 0)
			used_vnode_size = vattr.va_size;
	}

	/* relock object */
	simple_lock(&uvn->u_obj.vmobjlock); 

	if (result != 0) {
		if (uvn->u_flags & UVM_VNODE_WANTED)
			wakeup(uvn);
		uvn->u_flags = 0;
		simple_unlock(&uvn->u_obj.vmobjlock); /* drop lock */
		UVMHIST_LOG(maphist,"<- done (VOP_GETATTR FAILED!)", 0,0,0,0);
		return(NULL);
	}

	/*
	 * make sure that the newsize fits within a vaddr_t
	 * XXX: need to revise addressing data types
	 */
if (vp->v_type == VBLK) printf("used_vnode_size = %qu\n", used_vnode_size);
	if (used_vnode_size > (vaddr_t) -PAGE_SIZE) {
#ifdef DEBUG
		printf("uvn_attach: vn %p size truncated %qx->%x\n", vp,
		    used_vnode_size, -PAGE_SIZE);
#endif    
		used_vnode_size = (vaddr_t) -PAGE_SIZE;
	}

	/*
	 * now set up the uvn.
	 */
	uvn->u_obj.pgops = &uvm_vnodeops;
	TAILQ_INIT(&uvn->u_obj.memq);
	uvn->u_obj.uo_npages = 0;
	uvn->u_obj.uo_refs = 1;			/* just us... */
	oldflags = uvn->u_flags;
	uvn->u_flags = UVM_VNODE_VALID|UVM_VNODE_CANPERSIST;
	uvn->u_nio = 0;
	uvn->u_size = used_vnode_size;

	/* if write access, we need to add it to the wlist */
	if (accessprot & VM_PROT_WRITE) {
		simple_lock(&uvn_wl_lock);
		LIST_INSERT_HEAD(&uvn_wlist, uvn, u_wlist);
		simple_unlock(&uvn_wl_lock);
		uvn->u_flags |= UVM_VNODE_WRITEABLE;	/* we are on wlist! */
	}

	/*
	 * add a reference to the vnode.   this reference will stay as long
	 * as there is a valid mapping of the vnode.   dropped when the
	 * reference count goes to zero [and we either free or persist].
	 */
	VREF(vp);
	simple_unlock(&uvn->u_obj.vmobjlock);
	if (oldflags & UVM_VNODE_WANTED)
		wakeup(uvn);

	UVMHIST_LOG(maphist,"<- done/VREF, ret 0x%x", &uvn->u_obj,0,0,0);
	return(&uvn->u_obj);
#endif
}


/*
 * uvn_reference
 *
 * duplicate a reference to a VM object.  Note that the reference
 * count must already be at least one (the passed in reference) so 
 * there is no chance of the uvn being killed or locked out here.
 *
 * => caller must call with object unlocked.  
 * => caller must be using the same accessprot as was used at attach time
 */


static void
uvn_reference(uobj)
	struct uvm_object *uobj;
{
#ifdef UBC
#else
#ifdef DIAGNOSTIC
	struct uvm_vnode *uvn = (struct uvm_vnode *) uobj;
#endif
#endif
	UVMHIST_FUNC("uvn_reference"); UVMHIST_CALLED(maphist);

#ifdef UBC
	VREF((struct vnode *)uobj);
#else
	simple_lock(&uobj->vmobjlock);
#ifdef DIAGNOSTIC
	if ((uvn->u_flags & UVM_VNODE_VALID) == 0) {
		printf("uvn_reference: ref=%d, flags=0x%x\n", uvn->u_flags,
		    uobj->uo_refs);
		panic("uvn_reference: invalid state");
	}
#endif
	uobj->uo_refs++;
	UVMHIST_LOG(maphist, "<- done (uobj=0x%x, ref = %d)", 
	uobj, uobj->uo_refs,0,0);
	simple_unlock(&uobj->vmobjlock);
#endif
}

/*
 * uvn_detach
 *
 * remove a reference to a VM object.
 *
 * => caller must call with object unlocked and map locked.
 * => this starts the detach process, but doesn't have to finish it
 *    (async i/o could still be pending).
 */
static void
uvn_detach(uobj)
	struct uvm_object *uobj;
{
#ifdef UBC
#else
	struct uvm_vnode *uvn;
	struct vnode *vp;
	int oldflags;
#endif
	UVMHIST_FUNC("uvn_detach"); UVMHIST_CALLED(maphist);

#ifdef UBC
	vrele((struct vnode *)uobj);
#else
	simple_lock(&uobj->vmobjlock);

	UVMHIST_LOG(maphist,"  (uobj=0x%x)  ref=%d", uobj,uobj->uo_refs,0,0);
	uobj->uo_refs--;			/* drop ref! */
	if (uobj->uo_refs) {			/* still more refs */
		simple_unlock(&uobj->vmobjlock);
		UVMHIST_LOG(maphist, "<- done (rc>0)", 0,0,0,0);
		return;
	}

	/*
	 * get other pointers ...
	 */

	uvn = (struct uvm_vnode *) uobj;
	vp = (struct vnode *) uobj;

	/*
	 * clear VTEXT flag now that there are no mappings left (VTEXT is used
	 * to keep an active text file from being overwritten).
	 */
	vp->v_flag &= ~VTEXT;

	/*
	 * we just dropped the last reference to the uvn.   see if we can
	 * let it "stick around".
	 */

	if (uvn->u_flags & UVM_VNODE_CANPERSIST) {
		/* won't block */
		uvn_flush(uobj, 0, 0, PGO_DEACTIVATE|PGO_ALLPAGES);
		simple_unlock(&uobj->vmobjlock);
		vrele(vp);			/* drop vnode reference */
		UVMHIST_LOG(maphist,"<- done/vrele!  (persist)", 0,0,0,0);
		return;
	}

	/*
	 * its a goner!
	 */

	UVMHIST_LOG(maphist,"  its a goner (flushing)!", 0,0,0,0);

	uvn->u_flags |= UVM_VNODE_DYING;

	/*
	 * even though we may unlock in flush, no one can gain a reference
	 * to us until we clear the "dying" flag [because it blocks
	 * attaches].  we will not do that until after we've disposed of all
	 * the pages with uvn_flush().  note that before the flush the only
	 * pages that could be marked PG_BUSY are ones that are in async
	 * pageout by the daemon.  (there can't be any pending "get"'s
	 * because there are no references to the object).
	 */

	(void) uvn_flush(uobj, 0, 0, PGO_CLEANIT|PGO_FREE|PGO_ALLPAGES);

	UVMHIST_LOG(maphist,"  its a goner (done flush)!", 0,0,0,0);

	/*
	 * given the structure of this pager, the above flush request will
	 * create the following state: all the pages that were in the object
	 * have either been free'd or they are marked PG_BUSY|PG_RELEASED.
	 * the PG_BUSY bit was set either by us or the daemon for async I/O.
	 * in either case, if we have pages left we can't kill the object
	 * yet because i/o is pending.  in this case we set the "relkill"
	 * flag which will cause pgo_releasepg to kill the object once all
	 * the I/O's are done [pgo_releasepg will be called from the aiodone
	 * routine or from the page daemon].
	 */

	if (uobj->uo_npages) {		/* I/O pending.  iodone will free */
#ifdef DIAGNOSTIC
		/* 
		 * XXXCDC: very unlikely to happen until we have async i/o
		 * so print a little info message in case it does.
		 */
		printf("uvn_detach: vn %p has pages left after flush - "
		    "relkill mode\n", uobj);
#endif
		uvn->u_flags |= UVM_VNODE_RELKILL;
		simple_unlock(&uobj->vmobjlock);
		UVMHIST_LOG(maphist,"<- done! (releasepg will kill obj)", 0, 0,
		    0, 0);
		return;
	}

	/*
	 * kill object now.   note that we can't be on the sync q because
	 * all references are gone.
	 */
	if (uvn->u_flags & UVM_VNODE_WRITEABLE) {
		simple_lock(&uvn_wl_lock);		/* protect uvn_wlist */
		LIST_REMOVE(uvn, u_wlist);
XXXwlist
		simple_unlock(&uvn_wl_lock);
	}
#ifdef DIAGNOSTIC
	if (uobj->memq.tqh_first != NULL)
		panic("uvn_deref: vnode VM object still has pages afer "
		    "syncio/free flush");
#endif
	oldflags = uvn->u_flags;
	uvn->u_flags = 0;
	simple_unlock(&uobj->vmobjlock);
	
	/* wake up any sleepers */
	if (oldflags & UVM_VNODE_WANTED)
		wakeup(uvn);

	/*
	 * drop our reference to the vnode.
	 */
	vrele(vp);
	UVMHIST_LOG(maphist,"<- done (vrele) final", 0,0,0,0);

	return;
#endif
}

/*
 * uvm_vnp_terminate: external hook to clear out a vnode's VM
 *
 * called in two cases:
 *  [1] when a persisting vnode vm object (i.e. one with a zero reference
 *      count) needs to be freed so that a vnode can be reused.  this
 *      happens under "getnewvnode" in vfs_subr.c.   if the vnode from
 *      the free list is still attached (i.e. not VBAD) then vgone is
 *	called.   as part of the vgone trace this should get called to
 *	free the vm object.   this is the common case.
 *  [2] when a filesystem is being unmounted by force (MNT_FORCE, 
 *	"umount -f") the vgone() function is called on active vnodes
 *	on the mounted file systems to kill their data (the vnodes become
 *	"dead" ones [see src/sys/miscfs/deadfs/...]).  that results in a
 *	call here (even if the uvn is still in use -- i.e. has a non-zero
 *	reference count).  this case happens at "umount -f" and during a
 *	"reboot/halt" operation.
 *
 * => the caller must XLOCK and VOP_LOCK the vnode before calling us
 *	[protects us from getting a vnode that is already in the DYING
 *	 state...]
 * => unlike uvn_detach, this function must not return until all the
 *	uvn's pages are disposed of.
 * => in case [2] the uvn is still alive after this call, but all I/O
 *	ops will fail (due to the backing vnode now being "dead").  this
 *	will prob. kill any process using the uvn due to pgo_get failing.
 */

void
uvm_vnp_terminate(vp)
	struct vnode *vp;
{
	struct uvm_vnode *uvn = &vp->v_uvm;
#ifdef UBC
	if (uvn->u_flags & UVM_VNODE_WRITEABLE) {
		simple_lock(&uvn_wl_lock);
		LIST_REMOVE(uvn, u_wlist);
		uvn->u_wlist.le_next = NULL;
		uvn->u_flags &= ~(UVM_VNODE_WRITEABLE);
		simple_unlock(&uvn_wl_lock);
	}
#else
	int oldflags;
	UVMHIST_FUNC("uvm_vnp_terminate"); UVMHIST_CALLED(maphist);

	/*
	 * lock object and check if it is valid
	 */
	simple_lock(&uvn->u_obj.vmobjlock);
	UVMHIST_LOG(maphist, "  vp=0x%x, ref=%d, flag=0x%x", vp,
	    uvn->u_obj.uo_refs, uvn->u_flags, 0);
	if ((uvn->u_flags & UVM_VNODE_VALID) == 0) {
		simple_unlock(&uvn->u_obj.vmobjlock);
		UVMHIST_LOG(maphist, "<- done (not active)", 0, 0, 0, 0);
		return;
	}

	/*
	 * must be a valid uvn that is not already dying (because XLOCK
	 * protects us from that).   the uvn can't in the the ALOCK state
	 * because it is valid, and uvn's that are in the ALOCK state haven't
	 * been marked valid yet.
	 */

#ifdef DEBUG
	/*
	 * debug check: are we yanking the vnode out from under our uvn?
	 */
	if (uvn->u_obj.uo_refs) {
		printf("uvm_vnp_terminate(%p): terminating active vnode "
		    "(refs=%d)\n", uvn, uvn->u_obj.uo_refs);
	} 
#endif
	
	/*
	 * it is possible that the uvn was detached and is in the relkill
	 * state [i.e. waiting for async i/o to finish so that releasepg can
	 * kill object].  we take over the vnode now and cancel the relkill.
	 * we want to know when the i/o is done so we can recycle right
	 * away.   note that a uvn can only be in the RELKILL state if it
	 * has a zero reference count.
	 */
	
	if (uvn->u_flags & UVM_VNODE_RELKILL)
		uvn->u_flags &= ~UVM_VNODE_RELKILL;	/* cancel RELKILL */

	/*
	 * block the uvn by setting the dying flag, and then flush the
	 * pages.  (note that flush may unlock object while doing I/O, but
	 * it will re-lock it before it returns control here).
	 *
	 * also, note that we tell I/O that we are already VOP_LOCK'd so
	 * that uvn_io doesn't attempt to VOP_LOCK again.
	 *
	 * XXXCDC: setting VNISLOCKED on an active uvn which is being terminated
	 *	due to a forceful unmount might not be a good idea.  maybe we
	 *	need a way to pass in this info to uvn_flush through a
	 *	pager-defined PGO_ constant [currently there are none].
	 */
	uvn->u_flags |= UVM_VNODE_DYING|UVM_VNODE_VNISLOCKED;

	(void) uvn_flush(&uvn->u_obj, 0, 0, PGO_CLEANIT|PGO_FREE|PGO_ALLPAGES);

	/*
	 * as we just did a flush we expect all the pages to be gone or in 
	 * the process of going.  sleep to wait for the rest to go [via iosync].
	 */

	while (uvn->u_obj.uo_npages) {
#ifdef DIAGNOSTIC
		struct vm_page *pp;
		for (pp = uvn->u_obj.memq.tqh_first ; pp != NULL ; 
		     pp = pp->listq.tqe_next) {
			if ((pp->flags & PG_BUSY) == 0)
				panic("uvm_vnp_terminate: detected unbusy pg");
		}
		if (uvn->u_nio == 0)
			panic("uvm_vnp_terminate: no I/O to wait for?");
		printf("uvm_vnp_terminate: waiting for I/O to fin.\n");
		/* 
		 * XXXCDC: this is unlikely to happen without async i/o so we 
		 * put a printf in just to keep an eye on it.
		 */
#endif
		uvn->u_flags |= UVM_VNODE_IOSYNC;
		UVM_UNLOCK_AND_WAIT(&uvn->u_nio, &uvn->u_obj.vmobjlock, FALSE, 
		    "uvn_term",0);
		simple_lock(&uvn->u_obj.vmobjlock);
	}

	/*
	 * done.   now we free the uvn if its reference count is zero
	 * (true if we are zapping a persisting uvn).   however, if we are
	 * terminating a uvn with active mappings we let it live ... future
	 * calls down to the vnode layer will fail.
	 */

	oldflags = uvn->u_flags;
	if (uvn->u_obj.uo_refs) {

		/*
		 * uvn must live on it is dead-vnode state until all references 
		 * are gone.   restore flags.    clear CANPERSIST state.
		 */

		uvn->u_flags &= ~(UVM_VNODE_DYING|UVM_VNODE_VNISLOCKED|
		      UVM_VNODE_WANTED|UVM_VNODE_CANPERSIST);
	
	} else {

		/*
		 * free the uvn now.   note that the VREF reference is already
		 * gone [it is dropped when we enter the persist state].
		 */
		if (uvn->u_flags & UVM_VNODE_IOSYNCWANTED)
			panic("uvm_vnp_terminate: io sync wanted bit set");

		if (uvn->u_flags & UVM_VNODE_WRITEABLE) {
			simple_lock(&uvn_wl_lock);
			LIST_REMOVE(uvn, u_wlist);
			simple_unlock(&uvn_wl_lock);
		}
		uvn->u_flags = 0;	/* uvn is history, clear all bits */
	}

	if (oldflags & UVM_VNODE_WANTED)
		wakeup(uvn);		/* object lock still held */

	simple_unlock(&uvn->u_obj.vmobjlock);
	UVMHIST_LOG(maphist, "<- done", 0, 0, 0, 0);
#endif
}

/*
 * uvn_releasepg: handled a released page in a uvn
 *
 * => "pg" is a PG_BUSY [caller owns it], PG_RELEASED page that we need
 *	to dispose of.
 * => caller must handled PG_WANTED case
 * => called with page's object locked, pageq's unlocked
 * => returns TRUE if page's object is still alive, FALSE if we
 *	killed the page's object.    if we return TRUE, then we
 *	return with the object locked.
 * => if (nextpgp != NULL) => we return pageq.tqe_next here, and return
 *				with the page queues locked [for pagedaemon]
 * => if (nextpgp == NULL) => we return with page queues unlocked [normal case]
 * => we kill the uvn if it is not referenced and we are suppose to
 *	kill it ("relkill").
 */

boolean_t
uvn_releasepg(pg, nextpgp)
	struct vm_page *pg;
	struct vm_page **nextpgp;	/* OUT */
{
	struct uvm_vnode *uvn = (struct uvm_vnode *) pg->uobject;
#ifdef DIAGNOSTIC
	if ((pg->flags & PG_RELEASED) == 0)
		panic("uvn_releasepg: page not released!");
#endif
	
	/*
	 * dispose of the page [caller handles PG_WANTED]
	 */
	pmap_page_protect(PMAP_PGARG(pg), VM_PROT_NONE);
	uvm_lock_pageq();
	if (nextpgp)
		*nextpgp = pg->pageq.tqe_next;	/* next page for daemon */
	uvm_pagefree(pg);
	if (!nextpgp)
		uvm_unlock_pageq();

#ifdef UBC
	/* XXX I'm sure we need to do something here. */
	uvn = uvn;
#else
	/*
	 * now see if we need to kill the object
	 */
	if (uvn->u_flags & UVM_VNODE_RELKILL) {
		if (uvn->u_obj.uo_refs)
			panic("uvn_releasepg: kill flag set on referenced "
			    "object!");
		if (uvn->u_obj.uo_npages == 0) {
			if (uvn->u_flags & UVM_VNODE_WRITEABLE) {
				simple_lock(&uvn_wl_lock);
				LIST_REMOVE(uvn, u_wlist);
				simple_unlock(&uvn_wl_lock);
			}
#ifdef DIAGNOSTIC
			if (uvn->u_obj.memq.tqh_first)
	panic("uvn_releasepg: pages in object with npages == 0");
#endif
			if (uvn->u_flags & UVM_VNODE_WANTED)
				/* still holding object lock */
				wakeup(uvn);

			uvn->u_flags = 0;		/* DEAD! */
			simple_unlock(&uvn->u_obj.vmobjlock);
			return (FALSE);
		}
	}
#endif
	return (TRUE);
}

/*
 * NOTE: currently we have to use VOP_READ/VOP_WRITE because they go
 * through the buffer cache and allow I/O in any size.  These VOPs use
 * synchronous i/o.  [vs. VOP_STRATEGY which can be async, but doesn't
 * go through the buffer cache or allow I/O sizes larger than a
 * block].  we will eventually want to change this.
 *
 * issues to consider:
 *   uvm provides the uvm_aiodesc structure for async i/o management.
 * there are two tailq's in the uvm. structure... one for pending async
 * i/o and one for "done" async i/o.   to do an async i/o one puts
 * an aiodesc on the "pending" list (protected by splbio()), starts the
 * i/o and returns VM_PAGER_PEND.    when the i/o is done, we expect
 * some sort of "i/o done" function to be called (at splbio(), interrupt
 * time).   this function should remove the aiodesc from the pending list
 * and place it on the "done" list and wakeup the daemon.   the daemon
 * will run at normal spl() and will remove all items from the "done"
 * list and call the "aiodone" hook for each done request (see uvm_pager.c).
 * [in the old vm code, this was done by calling the "put" routine with
 * null arguments which made the code harder to read and understand because
 * you had one function ("put") doing two things.]  
 *
 * so the current pager needs: 
 *   int uvn_aiodone(struct uvm_aiodesc *)
 *
 * => return KERN_SUCCESS (aio finished, free it).  otherwise requeue for
 *	later collection.
 * => called with pageq's locked by the daemon.
 *
 * general outline:
 * - "try" to lock object.   if fail, just return (will try again later)
 * - drop "u_nio" (this req is done!)
 * - if (object->iosync && u_naio == 0) { wakeup &uvn->u_naio }
 * - get "page" structures (atop?).
 * - handle "wanted" pages
 * - handle "released" pages [using pgo_releasepg]
 *   >>> pgo_releasepg may kill the object
 * dont forget to look at "object" wanted flag in all cases.
 */


/*
 * uvn_flush: flush pages out of a uvm object.
 *
 * => object should be locked by caller.   we may _unlock_ the object
 *	if (and only if) we need to clean a page (PGO_CLEANIT).
 *	we return with the object locked.
 * => if PGO_CLEANIT is set, we may block (due to I/O).   thus, a caller
 *	might want to unlock higher level resources (e.g. vm_map)
 *	before calling flush.
 * => if PGO_CLEANIT is not set, then we will neither unlock the object
 *	or block.
 * => if PGO_ALLPAGE is set, then all pages in the object are valid targets
 *	for flushing.
 * => NOTE: we rely on the fact that the object's memq is a TAILQ and
 *	that new pages are inserted on the tail end of the list.   thus,
 *	we can make a complete pass through the object in one go by starting
 *	at the head and working towards the tail (new pages are put in
 *	front of us).
 * => NOTE: we are allowed to lock the page queues, so the caller
 *	must not be holding the lock on them [e.g. pagedaemon had
 *	better not call us with the queues locked]
 * => we return TRUE unless we encountered some sort of I/O error
 *
 * comment on "cleaning" object and PG_BUSY pages:
 *	this routine is holding the lock on the object.   the only time
 *	that it can run into a PG_BUSY page that it does not own is if
 *	some other process has started I/O on the page (e.g. either
 *	a pagein, or a pageout).    if the PG_BUSY page is being paged
 *	in, then it can not be dirty (!PG_CLEAN) because no one has
 *	had a chance to modify it yet.    if the PG_BUSY page is being
 *	paged out then it means that someone else has already started
 *	cleaning the page for us (how nice!).    in this case, if we 
 *	have syncio specified, then after we make our pass through the
 *	object we need to wait for the other PG_BUSY pages to clear 
 *	off (i.e. we need to do an iosync).   also note that once a
 *	page is PG_BUSY it must stay in its object until it is un-busyed.
 *
 * note on page traversal:
 *	we can traverse the pages in an object either by going down the
 *	linked list in "uobj->memq", or we can go over the address range
 *	by page doing hash table lookups for each address.    depending
 *	on how many pages are in the object it may be cheaper to do one 
 *	or the other.   we set "by_list" to true if we are using memq.
 *	if the cost of a hash lookup was equal to the cost of the list
 *	traversal we could compare the number of pages in the start->stop
 *	range to the total number of pages in the object.   however, it
 *	seems that a hash table lookup is more expensive than the linked
 *	list traversal, so we multiply the number of pages in the 
 *	start->stop range by a penalty which we define below.
 */

#define UVN_HASH_PENALTY 4	/* XXX: a guess */

static boolean_t
uvn_flush(uobj, start, stop, flags)
	struct uvm_object *uobj;
	vaddr_t start, stop;
	int flags;
{
	struct uvm_vnode *uvn = (struct uvm_vnode *) uobj;
	struct vm_page *pp, *ppnext, *ptmp;
	struct vm_page *pps[MAXBSIZE >> PAGE_SHIFT], **ppsp;
	int npages, result, lcv;
	boolean_t retval, need_iosync, by_list, needs_clean;
	vaddr_t curoff;
	u_short pp_version;
	UVMHIST_FUNC("uvn_flush"); UVMHIST_CALLED(maphist);

#ifdef UBC
	if (uvn->u_size == VSIZENOTSET) {
		void vp_name(void *);
	      
		printf("uvn_flush: size not set vp %p\n", uvn);
		if ((flags & PGO_ALLPAGES) == 0)
			printf("... and PGO_ALLPAGES not set: "
			       "start 0x%lx end 0x%lx flags 0x%x\n",
			       start, stop, flags);
		vp_name(uvn);
		flags |= PGO_ALLPAGES;
	}
#if 0
	/* XXX unfortunately this is legitimate */
	if (flags & PGO_FREE && uobj->uo_refs) {
		printf("uvn_flush: PGO_FREE on ref'd vp %p\n", uobj);
		Debugger();
	}
#endif
#endif

	curoff = 0;	/* XXX: shut up gcc */
	/*
	 * get init vals and determine how we are going to traverse object
	 */

	need_iosync = FALSE;
	retval = TRUE;		/* return value */
	if (flags & PGO_ALLPAGES) {
		start = 0;
#ifdef UBC
		stop = -1;
#else
		stop = round_page(uvn->u_size);
#endif
		by_list = TRUE;		/* always go by the list */
	} else {
		start = trunc_page(start);
		stop = round_page(stop);
		if (stop > round_page(uvn->u_size)) {
			printf("uvn_flush: out of range flush (fixed)\n");
			printf("  vp %p stop 0x%x\n", uvn, (int)stop);
		}

		by_list = (uobj->uo_npages <= 
		    ((stop - start) >> PAGE_SHIFT) * UVN_HASH_PENALTY);
	}

	UVMHIST_LOG(maphist,
	    " flush start=0x%x, stop=0x%x, by_list=%d, flags=0x%x",
	    start, stop, by_list, flags);

	/*
	 * PG_CLEANCHK: this bit is used by the pgo_mk_pcluster function as
	 * a _hint_ as to how up to date the PG_CLEAN bit is.   if the hint
	 * is wrong it will only prevent us from clustering... it won't break
	 * anything.   we clear all PG_CLEANCHK bits here, and pgo_mk_pcluster
	 * will set them as it syncs PG_CLEAN.   This is only an issue if we
	 * are looking at non-inactive pages (because inactive page's PG_CLEAN
	 * bit is always up to date since there are no mappings).
	 * [borrowed PG_CLEANCHK idea from FreeBSD VM]
	 */

	if ((flags & PGO_CLEANIT) != 0 &&
	    uobj->pgops->pgo_mk_pcluster != NULL) {
		if (by_list) {
			for (pp = TAILQ_FIRST(&uobj->memq);
			     pp != NULL ;
			     pp = TAILQ_NEXT(pp, listq)) {
				if (pp->offset < start ||
				    (pp->offset >= stop && stop != -1))
					continue;
				pp->flags &= ~PG_CLEANCHK;
			}

		} else {   /* by hash */
			for (curoff = start ; curoff < stop;
			    curoff += PAGE_SIZE) {
				pp = uvm_pagelookup(uobj, curoff);
				if (pp)
					pp->flags &= ~PG_CLEANCHK;
			}
		}
	}

	/*
	 * now do it.   note: we must update ppnext in body of loop or we
	 * will get stuck.  we need to use ppnext because we may free "pp"
	 * before doing the next loop.
	 */

	if (by_list) {
		pp = TAILQ_FIRST(&uobj->memq);
	} else {
		curoff = start;
		pp = uvm_pagelookup(uobj, curoff);
	}

	ppnext = NULL;	/* XXX: shut up gcc */ 
	ppsp = NULL;		/* XXX: shut up gcc */
	uvm_lock_pageq();	/* page queues locked */

	/* locked: both page queues and uobj */
	for ( ; (by_list && pp != NULL) || 
	  (!by_list && curoff < stop) ; pp = ppnext) {

		if (by_list) {

			/*
			 * range check
			 */

			if (pp->offset < start || pp->offset >= stop) {
				ppnext = TAILQ_NEXT(pp, listq);
				continue;
			}

		} else {

			/*
			 * null check
			 */

			curoff += PAGE_SIZE;
			if (pp == NULL) {
				if (curoff < stop)
					ppnext = uvm_pagelookup(uobj, curoff);
				continue;
			}

		}

		/*
		 * handle case where we do not need to clean page (either
		 * because we are not clean or because page is not dirty or
		 * is busy):
		 * 
		 * NOTE: we are allowed to deactivate a non-wired active
		 * PG_BUSY page, but once a PG_BUSY page is on the inactive
		 * queue it must stay put until it is !PG_BUSY (so as not to
		 * confuse pagedaemon).
		 */

		if ((flags & PGO_CLEANIT) == 0 || (pp->flags & PG_BUSY) != 0) {
			needs_clean = FALSE;
			if ((pp->flags & PG_BUSY) != 0 &&
			    (flags & (PGO_CLEANIT|PGO_SYNCIO)) ==
			             (PGO_CLEANIT|PGO_SYNCIO))
				need_iosync = TRUE;
		} else {
			/*
			 * freeing: nuke all mappings so we can sync
			 * PG_CLEAN bit with no race
			 */
			if ((pp->flags & PG_CLEAN) != 0 && 
			    (flags & PGO_FREE) != 0 &&
			    (pp->pqflags & PQ_ACTIVE) != 0)
				pmap_page_protect(PMAP_PGARG(pp), VM_PROT_NONE);
			if ((pp->flags & PG_CLEAN) != 0 &&
			    pmap_is_modified(PMAP_PGARG(pp)))
				pp->flags &= ~(PG_CLEAN);
			pp->flags |= PG_CLEANCHK;	/* update "hint" */

			needs_clean = ((pp->flags & PG_CLEAN) == 0);
		}

		/*
		 * if we don't need a clean... load ppnext and dispose of pp
		 */
		if (!needs_clean) {
			/* load ppnext */
			if (by_list)
				ppnext = pp->listq.tqe_next;
			else {
				if (curoff < stop)
					ppnext = uvm_pagelookup(uobj, curoff);
			}

			/* now dispose of pp */
			if (flags & PGO_DEACTIVATE) {
				if ((pp->pqflags & PQ_INACTIVE) == 0 &&
				    pp->wire_count == 0) {
					pmap_page_protect(PMAP_PGARG(pp),
					    VM_PROT_NONE);
					uvm_pagedeactivate(pp);
				}

			} else if (flags & PGO_FREE) {
				if (pp->flags & PG_BUSY) {
					/* release busy pages */
					pp->flags |= PG_RELEASED;
				} else {
					pmap_page_protect(PMAP_PGARG(pp),
					    VM_PROT_NONE);
					/* removed page from object */
					uvm_pagefree(pp);
				}
			}
			/* ppnext is valid so we can continue... */
			continue;
		}

		/*
		 * pp points to a page in the locked object that we are
		 * working on.  if it is !PG_CLEAN,!PG_BUSY and we asked
		 * for cleaning (PGO_CLEANIT).  we clean it now.
		 *
		 * let uvm_pager_put attempted a clustered page out.
		 * note: locked: uobj and page queues.
		 */

		pp->flags |= PG_BUSY;	/* we 'own' page now */
		UVM_PAGE_OWN(pp, "uvn_flush");
		pmap_page_protect(PMAP_PGARG(pp), VM_PROT_READ);
		pp_version = pp->version;
ReTry:
		ppsp = pps;
		npages = sizeof(pps) / sizeof(struct vm_page *);

		/* locked: page queues, uobj */
		result = uvm_pager_put(uobj, pp, &ppsp, &npages, 
			   flags | PGO_DOACTCLUST, start, stop);
		/* unlocked: page queues, uobj */

		/*
		 * at this point nothing is locked.   if we did an async I/O
		 * it is remotely possible for the async i/o to complete and 
		 * the page "pp" be freed or what not before we get a chance 
		 * to relock the object.   in order to detect this, we have
		 * saved the version number of the page in "pp_version".
		 */

		/* relock! */
		simple_lock(&uobj->vmobjlock);
		uvm_lock_pageq();

		/*
		 * VM_PAGER_AGAIN: given the structure of this pager, this 
		 * can only happen when  we are doing async I/O and can't
		 * map the pages into kernel memory (pager_map) due to lack
		 * of vm space.   if this happens we drop back to sync I/O.
		 */

		if (result == VM_PAGER_AGAIN) {
			/* 
			 * it is unlikely, but page could have been released
			 * while we had the object lock dropped.   we ignore
			 * this now and retry the I/O.  we will detect and
			 * handle the released page after the syncio I/O
			 * completes.
			 */
#ifdef DIAGNOSTIC
			if (flags & PGO_SYNCIO)
	panic("uvn_flush: PGO_SYNCIO return 'try again' error (impossible)");
#endif
			flags |= PGO_SYNCIO;
			goto ReTry;
		}

		/*
		 * the cleaning operation is now done.   finish up.  note that
		 * on error (!OK, !PEND) uvm_pager_put drops the cluster for us.
		 * if success (OK, PEND) then uvm_pager_put returns the cluster
		 * to us in ppsp/npages.
		 */

		/*
		 * for pending async i/o if we are not deactivating/freeing
		 * we can move on to the next page.
		 */

		if (result == VM_PAGER_PEND) {

			if ((flags & (PGO_DEACTIVATE|PGO_FREE)) == 0) {
				/*
				 * no per-page ops: refresh ppnext and continue
				 */
				if (by_list) {
					if (pp->version == pp_version)
						ppnext = pp->listq.tqe_next;
					else
						/* reset */
						ppnext = uobj->memq.tqh_first;
				} else {
					if (curoff < stop)
						ppnext = uvm_pagelookup(uobj,
						    curoff);
				}
				continue;
			}

			/* need to do anything here? */
		}

		/*
		 * need to look at each page of the I/O operation.  we defer 
		 * processing "pp" until the last trip through this "for" loop 
		 * so that we can load "ppnext" for the main loop after we
		 * play with the cluster pages [thus the "npages + 1" in the 
		 * loop below].
		 */

		for (lcv = 0 ; lcv < npages + 1 ; lcv++) {

			/*
			 * handle ppnext for outside loop, and saving pp
			 * until the end.
			 */
			if (lcv < npages) {
				if (ppsp[lcv] == pp)
					continue; /* skip pp until the end */
				ptmp = ppsp[lcv];
			} else {
				ptmp = pp;

				/* set up next page for outer loop */
				if (by_list) {
					if (pp->version == pp_version)
						ppnext = pp->listq.tqe_next;
					else
						/* reset */
						ppnext = uobj->memq.tqh_first;
				} else {
					if (curoff < stop)
					ppnext = uvm_pagelookup(uobj, curoff);
				}
			}

			/*
			 * verify the page didn't get moved while obj was
			 * unlocked
			 */
			if (result == VM_PAGER_PEND && ptmp->uobject != uobj)
				continue;

			/*
			 * unbusy the page if I/O is done.   note that for
			 * pending I/O it is possible that the I/O op
			 * finished before we relocked the object (in
			 * which case the page is no longer busy).
			 */

			if (result != VM_PAGER_PEND) {
				if (ptmp->flags & PG_WANTED)
					/* still holding object lock */
					thread_wakeup(ptmp);

				ptmp->flags &= ~(PG_WANTED|PG_BUSY);
				UVM_PAGE_OWN(ptmp, NULL);
				if (ptmp->flags & PG_RELEASED) {

					/* pgo_releasepg wants this */
					uvm_unlock_pageq();
					if (!uvn_releasepg(ptmp, NULL))
						return (TRUE);

					uvm_lock_pageq();	/* relock */
					continue;		/* next page */

				} else {
					ptmp->flags |= (PG_CLEAN|PG_CLEANCHK);
					if ((flags & PGO_FREE) == 0)
						pmap_clear_modify(
						    PMAP_PGARG(ptmp));
				}
			}
	  
			/*
			 * dispose of page
			 */

			if (flags & PGO_DEACTIVATE) {
				if ((pp->pqflags & PQ_INACTIVE) == 0 &&
				    pp->wire_count == 0) {
					pmap_page_protect(PMAP_PGARG(ptmp),
					    VM_PROT_NONE);
					uvm_pagedeactivate(ptmp);
				}

			} else if (flags & PGO_FREE) {
				if (result == VM_PAGER_PEND) {
					if ((ptmp->flags & PG_BUSY) != 0)
						/* signal for i/o done */
						ptmp->flags |= PG_RELEASED;
				} else {
					if (result != VM_PAGER_OK) {
						printf("uvn_flush: obj=%p, "
						   "offset=0x%lx.  error %d\n",
						    pp->uobject, pp->offset,
						    result);
						printf("uvn_flush: WARNING: "
						    "changes to page may be "
						    "lost!\n");
						retval = FALSE;
					}
					pmap_page_protect(PMAP_PGARG(ptmp),
					    VM_PROT_NONE);
					uvm_pagefree(ptmp);
				}
			}

		}		/* end of "lcv" for loop */

	}		/* end of "pp" for loop */

	/*
	 * done with pagequeues: unlock
	 */
	uvm_unlock_pageq();

	/*
	 * now wait for all I/O if required.
	 */
#ifdef UBC
	/*
	 * XXX currently not needed since all i/o is sync.
	 * merge this with VBWAIT.
	 */
#else
	if (need_iosync) {

		UVMHIST_LOG(maphist,"  <<DOING IOSYNC>>",0,0,0,0);
		while (uvn->u_nio != 0) {
			uvn->u_flags |= UVM_VNODE_IOSYNC;
			UVM_UNLOCK_AND_WAIT(&uvn->u_nio, &uvn->u_obj.vmobjlock, 
			  FALSE, "uvn_flush",0);
			simple_lock(&uvn->u_obj.vmobjlock);
		}
		if (uvn->u_flags & UVM_VNODE_IOSYNCWANTED)
			wakeup(&uvn->u_flags);
		uvn->u_flags &= ~(UVM_VNODE_IOSYNC|UVM_VNODE_IOSYNCWANTED);
	}
#endif

	/* return, with object locked! */
	UVMHIST_LOG(maphist,"<- done (retval=0x%x)",retval,0,0,0);
	return(retval);
}

/*
 * uvn_cluster
 *
 * we are about to do I/O in an object at offset.   this function is called
 * to establish a range of offsets around "offset" in which we can cluster
 * I/O.
 *
 * - currently doesn't matter if obj locked or not.
 */

static void
uvn_cluster(uobj, offset, loffset, hoffset)
	struct uvm_object *uobj;
	vaddr_t offset;
	vaddr_t *loffset, *hoffset; /* OUT */
{
	struct uvm_vnode *uvn = (struct uvm_vnode *) uobj;
	*loffset = offset;

	if (*loffset >= uvn->u_size)
#ifdef UBC
	{
		/* XXX nfs writes cause trouble with this */
		*loffset = *hoffset = offset;
		printf("uvn_cluster: offset out of range: vp %p loffset 0x%x\n",
		      uobj, (int) *loffset);
		return;
	}
#else
		panic("uvn_cluster: offset out of range: vp %p loffset 0x%x",
		      uobj, (int) *loffset);
#endif

	/*
	 * XXX: old pager claims we could use VOP_BMAP to get maxcontig value.
	 */
	*hoffset = *loffset + MAXBSIZE;
	if (*hoffset > round_page(uvn->u_size))	/* past end? */
		*hoffset = round_page(uvn->u_size);

	return;
}

/*
 * uvn_put: flush page data to backing store.
 *
 * => prefer map unlocked (not required)
 * => object must be locked!   we will _unlock_ it before starting I/O.
 * => flags: PGO_SYNCIO -- use sync. I/O
 * => note: caller must set PG_CLEAN and pmap_clear_modify (if needed)
 * => XXX: currently we use VOP_READ/VOP_WRITE which are only sync.
 *	[thus we never do async i/o!  see iodone comment]
 */

static int
uvn_put(uobj, pps, npages, flags)
	struct uvm_object *uobj;
	struct vm_page **pps;
	int npages, flags;
{
	int retval;

	/* note: object locked */
	simple_lock_assert(&uobj->vmobjlock, 1);
	retval = VOP_PUTPAGES((struct vnode *)uobj, pps, npages, 1, &retval);
	/* note: object unlocked */
	simple_lock_assert(&uobj->vmobjlock, 0);

	return(retval);
}


/*
 * uvn_get: get pages (synchronously) from backing store
 *
 * => prefer map unlocked (not required)
 * => object must be locked!  we will _unlock_ it before starting any I/O.
 * => flags: PGO_ALLPAGES: get all of the pages
 *           PGO_LOCKED: fault data structures are locked
 * => NOTE: offset is the offset of pps[0], _NOT_ pps[centeridx]
 * => NOTE: caller must check for released pages!!
 */
 
static int
uvn_get(uobj, offset, pps, npagesp, centeridx, access_type, advice, flags)
	struct uvm_object *uobj;
	vaddr_t offset;
	struct vm_page **pps;		/* IN/OUT */
	int *npagesp;			/* IN (OUT if PGO_LOCKED) */
	int centeridx, advice, flags;
	vm_prot_t access_type;
{
	struct vnode *vp = (struct vnode *)uobj;
	int error;

	simple_lock_assert(&uobj->vmobjlock, 1);
	error = VOP_GETPAGES(vp, offset, pps, npagesp, centeridx,
			     access_type, advice, flags);
	simple_lock_assert(&uobj->vmobjlock, flags & PGO_LOCKED ? 1 : 0);

	return error ? VM_PAGER_ERROR : VM_PAGER_OK;
}

/*
 * uvn_findpage:
 * return the page for the uobj and offset requested, allocating if needed.
 * => uobj must be locked.
 * => returned page will be BUSY.
 */

void
uvn_findpage(uobj, offset, pps)
	struct uvm_object *uobj;
	vaddr_t offset;
	struct vm_page **pps;
{
	struct vm_page *ptmp;
	UVMHIST_FUNC("uvn_findpage"); UVMHIST_CALLED(maphist);

	for (;;) {
		/* look for a current page */
		ptmp = uvm_pagelookup(uobj, offset);

		/* nope?   allocate one now */
		if (ptmp == NULL) {
			ptmp = uvm_pagealloc(uobj, offset, NULL);
			if (ptmp == NULL) {
				simple_unlock(&uobj->vmobjlock);
				uvm_wait("uvn_fp1");
				simple_lock(&uobj->vmobjlock);
				continue;
			}

			/*
			 * XXX for now, always zero new pages.
			 */
			pmap_zero_page(VM_PAGE_TO_PHYS(ptmp));

			break;
		}

		/* page is there, see if we need to wait on it */
		if ((ptmp->flags & (PG_BUSY|PG_RELEASED)) != 0) {
			ptmp->flags |= PG_WANTED;
			UVM_UNLOCK_AND_WAIT(ptmp, &uobj->vmobjlock, 0,
					    "uvn_fp2",0);
			simple_lock(&uobj->vmobjlock);
			continue;
		}
			
		/* BUSY the page and we're done. */
		ptmp->flags |= PG_BUSY;
		UVM_PAGE_OWN(ptmp, "uvn_findpage");
		break;
	}
	*pps = ptmp;
}

/*
 * uvn_asyncget: start async I/O to bring pages into ram
 *
 * => caller must lock object(???XXX: see if this is best)
 * => could be called from uvn_get or a madvise() fault-ahead.
 * => if it fails, it doesn't matter.
 */

static int
uvn_asyncget(uobj, offset, npages)
	struct uvm_object *uobj;
	vaddr_t offset;
	int npages;
{

	/*
	 * XXXCDC: we can't do async I/O yet
	 */
	printf("uvn_asyncget called\n");
	return (KERN_SUCCESS);
}

/*
 * uvm_vnp_uncache: disable "persisting" in a vnode... when last reference
 * is gone we will kill the object (flushing dirty pages back to the vnode
 * if needed).
 *
 * => returns TRUE if there was no uvm_object attached or if there was
 *	one and we killed it [i.e. if there is no active uvn]
 * => called with the vnode VOP_LOCK'd [we will unlock it for I/O, if
 *	needed]
 *
 * => XXX: given that we now kill uvn's when a vnode is recycled (without
 *	having to hold a reference on the vnode) and given a working
 *	uvm_vnp_sync(), how does that effect the need for this function?
 *      [XXXCDC: seems like it can die?]
 *
 * => XXX: this function should DIE once we merge the VM and buffer 
 *	cache.
 *
 * research shows that this is called in the following places:
 * ext2fs_truncate, ffs_truncate, detrunc[msdosfs]: called when vnode
 *	changes sizes
 * ext2fs_write, WRITE [ufs_readwrite], msdosfs_write: called when we
 *	are written to
 * ex2fs_chmod, ufs_chmod: called if VTEXT vnode and the sticky bit
 *	is off
 * ffs_realloccg: when we can't extend the current block and have 
 *	to allocate a new one we call this [XXX: why?]
 * nfsrv_rename, rename_files: called when the target filename is there
 *	and we want to remove it
 * nfsrv_remove, sys_unlink: called on file we are removing
 * nfsrv_access: if VTEXT and we want WRITE access and we don't uncache
 *	then return "text busy"
 * nfs_open: seems to uncache any file opened with nfs
 * vn_writechk: if VTEXT vnode and can't uncache return "text busy"
 */

boolean_t
uvm_vnp_uncache(vp)
	struct vnode *vp;
{
#ifdef UBC
#else
	struct uvm_vnode *uvn = &vp->v_uvm;

	/*
	 * lock uvn part of the vnode and check to see if we need to do anything
	 */

	simple_lock(&uvn->u_obj.vmobjlock);
	if ((uvn->u_flags & UVM_VNODE_VALID) == 0 || 
			(uvn->u_flags & UVM_VNODE_BLOCKED) != 0) {
		simple_unlock(&uvn->u_obj.vmobjlock);
		return(TRUE);
	}

	/*
	 * we have a valid, non-blocked uvn.   clear persist flag.
	 * if uvn is currently active we can return now.
	 */

	uvn->u_flags &= ~UVM_VNODE_CANPERSIST;
	if (uvn->u_obj.uo_refs) {
		simple_unlock(&uvn->u_obj.vmobjlock);
		return(FALSE);
	}

	/*
	 * uvn is currently persisting!   we have to gain a reference to
	 * it so that we can call uvn_detach to kill the uvn.
	 */

	VREF(vp);			/* seems ok, even with VOP_LOCK */
	uvn->u_obj.uo_refs++;		/* value is now 1 */
	simple_unlock(&uvn->u_obj.vmobjlock);


#ifdef DEBUG
	/*
	 * carry over sanity check from old vnode pager: the vnode should
	 * be VOP_LOCK'd, and we confirm it here.
	 */
	if (!VOP_ISLOCKED(vp)) {
		boolean_t is_ok_anyway = FALSE;
#ifdef NFS
		extern int (**nfsv2_vnodeop_p) __P((void *));
		extern int (**spec_nfsv2nodeop_p) __P((void *));
		extern int (**fifo_nfsv2nodeop_p) __P((void *));

		/* vnode is NOT VOP_LOCKed: some vnode types _never_ lock */
		if (vp->v_op == nfsv2_vnodeop_p ||
		    vp->v_op == spec_nfsv2nodeop_p) {
			is_ok_anyway = TRUE;
		}
		if (vp->v_op == fifo_nfsv2nodeop_p) {
			is_ok_anyway = TRUE;
		}
#endif	/* NFS */
		if (!is_ok_anyway)
			panic("uvm_vnp_uncache: vnode not locked!");
	}
#endif	/* DEBUG */

	/*
	 * now drop our reference to the vnode.   if we have the sole 
	 * reference to the vnode then this will cause it to die [as we
	 * just cleared the persist flag].   we have to unlock the vnode 
	 * while we are doing this as it may trigger I/O.
	 *
	 * XXX: it might be possible for uvn to get reclaimed while we are
	 * unlocked causing us to return TRUE when we should not.   we ignore
	 * this as a false-positive return value doesn't hurt us.
	 */
	VOP_UNLOCK(vp, 0);
	uvn_detach(&uvn->u_obj);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	
	/*
	 * and return...
	 */
#endif
	return(TRUE);
}

/*
 * uvm_vnp_setsize: grow or shrink a vnode uvn
 *
 * grow   => just update size value
 * shrink => toss un-needed pages
 *
 * => we assume that the caller has a reference of some sort to the 
 *	vnode in question so that it will not be yanked out from under
 *	us.
 *
 * called from:
 *  => truncate fns (ext2fs_truncate, ffs_truncate, detrunc[msdos])
 *  => "write" fns (ext2fs_write, WRITE [ufs/ufs], msdosfs_write, nfs_write)
 *  => ffs_balloc [XXX: why? doesn't WRITE handle?]
 *  => NFS: nfs_loadattrcache, nfs_getattrcache, nfs_setattr
 *  => union fs: union_newsize
 */

void
uvm_vnp_setsize(vp, newsize)
	struct vnode *vp;
	u_quad_t newsize;
{
	struct uvm_vnode *uvn = &vp->v_uvm;

	/*
	 * lock uvn and check for valid object, and if valid: do it!
	 */
	simple_lock(&uvn->u_obj.vmobjlock);
#ifdef UBC
#else
	if (uvn->u_flags & UVM_VNODE_VALID) {
#endif
		/*
		 * make sure that the newsize fits within a vaddr_t
		 * XXX: need to revise addressing data types
		 */

		if (newsize > (vaddr_t) -PAGE_SIZE) {
#ifdef DEBUG
			printf("uvm_vnp_setsize: vn %p size truncated "
			    "%qx->%lx\n", vp, newsize, (vaddr_t)-PAGE_SIZE);
#endif
			newsize = (vaddr_t)-PAGE_SIZE;
		}

		/*
		 * now check if the size has changed: if we shrink we had better
		 * toss some pages...
		 */

#ifdef UBC
		if (uvn->u_size > newsize && uvn->u_size != VSIZENOTSET) {
#else
/*
		if (uvn->u_size > newsize) {
*/
#endif
			(void)uvn_flush(&uvn->u_obj, (vaddr_t)newsize,
					uvn->u_size, PGO_FREE);
		}
#ifdef DEBUGxx
printf("uvm_vnp_setsize: vp %p newsize 0x%x\n", vp, (int)newsize);
		uvn->u_size = (vaddr_t)newsize;
#endif
#ifdef UBC
#else
	}
#endif
	simple_unlock(&uvn->u_obj.vmobjlock);
}

/*
 * uvm_vnp_sync: flush all dirty VM pages back to their backing vnodes.
 *
 * => called from sys_sync with no VM structures locked
 * => only one process can do a sync at a time (because the uvn
 *    structure only has one queue for sync'ing).  we ensure this
 *    by holding the uvn_sync_lock while the sync is in progress.
 *    other processes attempting a sync will sleep on this lock
 *    until we are done.
 */

void
uvm_vnp_sync(mp)
	struct mount *mp;
{
	struct uvm_vnode *uvn;
	struct vnode *vp;
	boolean_t got_lock;

	/*
	 * step 1: ensure we are only ones using the uvn_sync_q by locking
	 * our lock...
	 */
	lockmgr(&uvn_sync_lock, LK_EXCLUSIVE, (void *)0);

	/*
	 * step 2: build up a simpleq of uvns of interest based on the 
	 * write list.   we gain a reference to uvns of interest.  must 
	 * be careful about locking uvn's since we will be holding uvn_wl_lock
	 * in the body of the loop.
	 */
	SIMPLEQ_INIT(&uvn_sync_q);
	simple_lock(&uvn_wl_lock);
	for (uvn = uvn_wlist.lh_first ; uvn != NULL ;
	    uvn = uvn->u_wlist.le_next) {

		vp = (struct vnode *) uvn;
		if (mp && vp->v_mount != mp)
			continue;

		/* attempt to gain reference */
		while ((got_lock = simple_lock_try(&uvn->u_obj.vmobjlock)) ==
		    						FALSE && 
				(uvn->u_flags & UVM_VNODE_BLOCKED) == 0) 
			/* spin */ ;

		/*
		 * we will exit the loop if either if the following are true:
		 *  - we got the lock [always true if NCPU == 1]
		 *  - we failed to get the lock but noticed the vnode was
		 * 	"blocked" -- in this case the vnode must be a dying
		 *	vnode, and since dying vnodes are in the process of
		 *	being flushed out, we can safely skip this one
		 *
		 * we want to skip over the vnode if we did not get the lock,
		 * or if the vnode is already dying (due to the above logic).
		 *
		 * note that uvn must already be valid because we found it on
		 * the wlist (this also means it can't be ALOCK'd).
		 */
		if (!got_lock || (uvn->u_flags & UVM_VNODE_BLOCKED) != 0) {
			if (got_lock)
				simple_unlock(&uvn->u_obj.vmobjlock);
			continue;		/* skip it */
		}
		
		/*
		 * gain reference.   watch out for persisting uvns (need to
		 * regain vnode REF).
		 */
#ifdef UBC
/* XXX should be using a vref-like function here */
#else
		if (uvn->u_obj.uo_refs == 0)
			VREF(vp);
#endif
		uvn->u_obj.uo_refs++;
		simple_unlock(&uvn->u_obj.vmobjlock);

		/*
		 * got it!
		 */
		SIMPLEQ_INSERT_HEAD(&uvn_sync_q, uvn, u_syncq);
	}
	simple_unlock(&uvn_wl_lock);

	/*
	 * step 3: we now have a list of uvn's that may need cleaning.
	 * we are holding the uvn_sync_lock, but have dropped the uvn_wl_lock
	 * (so we can now safely lock uvn's again).
	 */

	for (uvn = uvn_sync_q.sqh_first ; uvn ; uvn = uvn->u_syncq.sqe_next) {
		simple_lock(&uvn->u_obj.vmobjlock);
#ifdef UBC
#else
#ifdef DIAGNOSTIC
		if (uvn->u_flags & UVM_VNODE_DYING) {
			printf("uvm_vnp_sync: dying vnode on sync list\n");
		}
#endif
#endif
		uvn_flush(&uvn->u_obj, 0, 0,
		    PGO_CLEANIT|PGO_ALLPAGES|PGO_DOACTCLUST);

		/*
		 * if we have the only reference and we just cleaned the uvn,
		 * then we can pull it out of the UVM_VNODE_WRITEABLE state
		 * thus allowing us to avoid thinking about flushing it again
		 * on later sync ops.
		 */
		if (uvn->u_obj.uo_refs == 1 &&
		    (uvn->u_flags & UVM_VNODE_WRITEABLE)) {
			LIST_REMOVE(uvn, u_wlist);
			uvn->u_flags &= ~UVM_VNODE_WRITEABLE;
		}

		simple_unlock(&uvn->u_obj.vmobjlock);

		/* now drop our reference to the uvn */
		uvn_detach(&uvn->u_obj);
	}

	/*
	 * done!  release sync lock
	 */
	lockmgr(&uvn_sync_lock, LK_RELEASE, (void *)0);
}


/*
 * uvm_vnp_relocate: update pages' blknos
 */

int
uvm_vnp_relocate(vp, off, len, blkno)
	struct vnode *vp;
	vaddr_t off;
	vsize_t len;
	daddr_t blkno;
{
	int npages = (len + PAGE_SIZE - 1) >> PAGE_SHIFT;
	struct vm_page *pgs[npages], *pg;
	int i, rv;

printf("relocate: vp %p off 0x%lx npages 0x%x blkno 0x%x\n",
       vp, off, npages, blkno);

#ifdef DIAGNOSTIC
	if (off & (PAGE_SIZE - 1)) {
		panic("uvm_vnp_relocate: vp %p bad off 0x%lx", vp, off);
	}
#endif

	/*
	 * get all the pages in the range, change their blknos.
	 * XXX access_type?  advice?
	 */

	bzero(pgs, sizeof pgs);

again:
	simple_lock(&vp->v_uvm.u_obj.vmobjlock);
	rv = (vp->v_uvm.u_obj.pgops->pgo_get)(&vp->v_uvm.u_obj, off,
					      pgs, &npages,
					      0, 0, 0, PGO_ALLPAGES);
	switch (rv) {
	case VM_PAGER_OK:
		break;

#ifdef DIAGNOSTIC
	case VM_PAGER_PEND:
		panic("ubc_fault: pgo_get got PENDing on non-async I/O");
#endif

	case VM_PAGER_AGAIN:
		tsleep(&lbolt, PVM, "uvn_relocate", 0);
		goto again;

	default:
		return rv;
	}

	for (i = 0; i < npages; i++) {
		pg = pgs[i];

#ifdef DIAGNOSTIC
		if (pg == NULL) {
			panic("uvm_vnp_relocate: NULL pg");
		}
#endif

		pg->blkno = blkno;
		blkno += PAGE_SIZE >> DEV_BSHIFT;

		if (pg->flags & PG_WANTED) {
			wakeup(pg);
		}

#ifdef DIAGNOSTIC
		if (pg->flags & PG_RELEASED) {
			panic("uvm_vnp_relocate: "
			      "pgo_get gave us a RELEASED page");
		}
#endif
		pg->flags &= ~PG_BUSY;
		UVM_PAGE_OWN(pg, NULL);
	}

	return 0;
}
