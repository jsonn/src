/*	$NetBSD: null_vnops.c,v 1.2.2.1 1994/07/20 20:16:42 cgd Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * John Heidemann of the UCLA Ficus project.
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
 *	@(#)null_vnops.c	8.1 (Berkeley) 6/10/93
 *
 * Ancestors:
 *	@(#)lofs_vnops.c	1.2 (Berkeley) 6/18/92
 *	Id: lofs_vnops.c,v 1.11 1992/05/30 10:05:43 jsp Exp
 *	...and...
 *	@(#)null_vnodeops.c 1.20 92/07/07 UCLA Ficus project
 */

/*
 * Null Layer
 *
 * (See mount_null(8) for more information.)
 *
 * The null layer duplicates a portion of the file system
 * name space under a new name.  In this respect, it is
 * similar to the loopback file system.  It differs from
 * the loopback fs in two respects:  it is implemented using
 * a stackable layers techniques, and it's "null-node"s stack above
 * all lower-layer vnodes, not just over directory vnodes.
 *
 * The null layer has two purposes.  First, it serves as a demonstration
 * of layering by proving a layer which does nothing.  (It actually
 * does everything the loopback file system does, which is slightly
 * more than nothing.)  Second, the null layer can serve as a prototype
 * layer.  Since it provides all necessary layer framework,
 * new file system layers can be created very easily be starting
 * with a null layer.
 *
 * The remainder of this man page examines the null layer as a basis
 * for constructing new layers.
 *
 *
 * INSTANTIATING NEW NULL LAYERS
 *
 * New null layers are created with mount_null(8).
 * Mount_null(8) takes two arguments, the pathname
 * of the lower vfs (target-pn) and the pathname where the null
 * layer will appear in the namespace (alias-pn).  After
 * the null layer is put into place, the contents
 * of target-pn subtree will be aliased under alias-pn.
 *
 *
 * OPERATION OF A NULL LAYER
 *
 * The null layer is the minimum file system layer,
 * simply bypassing all possible operations to the lower layer
 * for processing there.  The majority of its activity centers
 * on the bypass routine, though which nearly all vnode operations
 * pass.
 *
 * The bypass routine accepts arbitrary vnode operations for
 * handling by the lower layer.  It begins by examing vnode
 * operation arguments and replacing any null-nodes by their
 * lower-layer equivlants.  It then invokes the operation
 * on the lower layer.  Finally, it replaces the null-nodes
 * in the arguments and, if a vnode is return by the operation,
 * stacks a null-node on top of the returned vnode.
 *
 * Although bypass handles most operations, 
 * vop_getattr, _inactive, _reclaim, and _print are not bypassed.
 * Vop_getattr must change the fsid being returned.
 * Vop_inactive and vop_reclaim are not bypassed so that
 * they can handle freeing null-layer specific data.
 * Vop_print is not bypassed to avoid excessive debugging
 * information.
 *
 *
 * INSTANTIATING VNODE STACKS
 *
 * Mounting associates the null layer with a lower layer,
 * effect stacking two VFSes.  Vnode stacks are instead
 * created on demand as files are accessed.
 *
 * The initial mount creates a single vnode stack for the
 * root of the new null layer.  All other vnode stacks
 * are created as a result of vnode operations on
 * this or other null vnode stacks.
 *
 * New vnode stacks come into existance as a result of
 * an operation which returns a vnode.  
 * The bypass routine stacks a null-node above the new
 * vnode before returning it to the caller.
 *
 * For example, imagine mounting a null layer with
 * "mount_null /usr/include /dev/layer/null".
 * Changing directory to /dev/layer/null will assign
 * the root null-node (which was created when the null layer was mounted).
 * Now consider opening "sys".  A vop_lookup would be
 * done on the root null-node.  This operation would bypass through
 * to the lower layer which would return a vnode representing 
 * the UFS "sys".  Null_bypass then builds a null-node
 * aliasing the UFS "sys" and returns this to the caller.
 * Later operations on the null-node "sys" will repeat this
 * process when constructing other vnode stacks.
 *
 *
 * CREATING OTHER FILE SYSTEM LAYERS
 *
 * One of the easiest ways to construct new file system layers is to make
 * a copy of the null layer, rename all files and variables, and
 * then begin modifing the copy.  Sed can be used to easily rename
 * all variables.
 *
 * The umap layer is an example of a layer descended from the 
 * null layer.
 *
 *
 * INVOKING OPERATIONS ON LOWER LAYERS
 *
 * There are two techniques to invoke operations on a lower layer 
 * when the operation cannot be completely bypassed.  Each method
 * is appropriate in different situations.  In both cases,
 * it is the responsibility of the aliasing layer to make
 * the operation arguments "correct" for the lower layer
 * by mapping an vnode arguments to the lower layer.
 *
 * The first approach is to call the aliasing layer's bypass routine.
 * This method is most suitable when you wish to invoke the operation
 * currently being hanldled on the lower layer.  It has the advantage
 * that the bypass routine already must do argument mapping.
 * An example of this is null_getattrs in the null layer.
 *
 * A second approach is to directly invoked vnode operations on
 * the lower layer with the VOP_OPERATIONNAME interface.
 * The advantage of this method is that it is easy to invoke
 * arbitrary operations on the lower layer.  The disadvantage
 * is that vnodes arguments must be manualy mapped.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <miscfs/nullfs/null.h>


int null_bug_bypass = 0;   /* for debugging: enables bypass printf'ing */

/*
 * This is the 10-Apr-92 bypass routine.
 *    This version has been optimized for speed, throwing away some
 * safety checks.  It should still always work, but it's not as
 * robust to programmer errors.
 *    Define SAFETY to include some error checking code.
 *
 * In general, we map all vnodes going down and unmap them on the way back.
 * As an exception to this, vnodes can be marked "unmapped" by setting
 * the Nth bit in operation's vdesc_flags.
 *
 * Also, some BSD vnode operations have the side effect of vrele'ing
 * their arguments.  With stacking, the reference counts are held
 * by the upper node, not the lower one, so we must handle these
 * side-effects here.  This is not of concern in Sun-derived systems
 * since there are no such side-effects.
 *
 * This makes the following assumptions:
 * - only one returned vpp
 * - no INOUT vpp's (Sun's vop_open has one of these)
 * - the vnode operation vector of the first vnode should be used
 *   to determine what implementation of the op should be invoked
 * - all mapped vnodes are of our vnode-type (NEEDSWORK:
 *   problems on rmdir'ing mount points and renaming?)
 */ 
int
null_bypass(ap)
	struct vop_generic_args /* {
		struct vnodeop_desc *a_desc;
		<other random data follows, presumably>
	} */ *ap;
{
	extern int (**null_vnodeop_p)();  /* not extern, really "forward" */
	register struct vnode **this_vp_p;
	int error;
	struct vnode *old_vps[VDESC_MAX_VPS];
	struct vnode **vps_p[VDESC_MAX_VPS];
	struct vnode ***vppp;
	struct vnodeop_desc *descp = ap->a_desc;
	int reles, i;

	if (null_bug_bypass)
		printf ("null_bypass: %s\n", descp->vdesc_name);

#ifdef SAFETY
	/*
	 * We require at least one vp.
	 */
	if (descp->vdesc_vp_offsets == NULL ||
	    descp->vdesc_vp_offsets[0] == VDESC_NO_OFFSET)
		panic ("null_bypass: no vp's in map.\n");
#endif

	/*
	 * Map the vnodes going in.
	 * Later, we'll invoke the operation based on
	 * the first mapped vnode's operation vector.
	 */
	reles = descp->vdesc_flags;
	for (i = 0; i < VDESC_MAX_VPS; reles >>= 1, i++) {
		if (descp->vdesc_vp_offsets[i] == VDESC_NO_OFFSET)
			break;   /* bail out at end of list */
		vps_p[i] = this_vp_p = 
			VOPARG_OFFSETTO(struct vnode**,descp->vdesc_vp_offsets[i],ap);
		/*
		 * We're not guaranteed that any but the first vnode
		 * are of our type.  Check for and don't map any
		 * that aren't.  (We must always map first vp or vclean fails.)
		 */
		if (i && (*this_vp_p == NULLVP ||
		    (*this_vp_p)->v_op != null_vnodeop_p)) {
			old_vps[i] = NULLVP;
		} else {
			old_vps[i] = *this_vp_p;
			*(vps_p[i]) = NULLVPTOLOWERVP(*this_vp_p);
			/*
			 * XXX - Several operations have the side effect
			 * of vrele'ing their vp's.  We must account for
			 * that.  (This should go away in the future.)
			 */
			if (reles & 1)
				VREF(*this_vp_p);
		}
			
	}

	/*
	 * Call the operation on the lower layer
	 * with the modified argument structure.
	 */
	error = VCALL(*(vps_p[0]), descp->vdesc_offset, ap);

	/*
	 * Maintain the illusion of call-by-value
	 * by restoring vnodes in the argument structure
	 * to their original value.
	 */
	reles = descp->vdesc_flags;
	for (i = 0; i < VDESC_MAX_VPS; reles >>= 1, i++) {
		if (descp->vdesc_vp_offsets[i] == VDESC_NO_OFFSET)
			break;   /* bail out at end of list */
		if (old_vps[i] != NULLVP) {
			*(vps_p[i]) = old_vps[i];
			if (reles & 1)
				vrele(*(vps_p[i]));
		}
	}

	/*
	 * Map the possible out-going vpp
	 * (Assumes that the lower layer always returns
	 * a VREF'ed vpp unless it gets an error.)
	 */
	if (descp->vdesc_vpp_offset != VDESC_NO_OFFSET &&
	    !(descp->vdesc_flags & VDESC_NOMAP_VPP) &&
	    !error) {
		/*
		 * XXX - even though some ops have vpp returned vp's,
		 * several ops actually vrele this before returning.
		 * We must avoid these ops.
		 * (This should go away when these ops are regularized.)
		 */
		if (descp->vdesc_flags & VDESC_VPP_WILLRELE)
			goto out;
		vppp = VOPARG_OFFSETTO(struct vnode***,
				 descp->vdesc_vpp_offset,ap);
		error = null_node_create(old_vps[0]->v_mount, **vppp, *vppp);
	}

 out:
	return (error);
}


/*
 *  We handle getattr only to change the fsid.
 */
int
null_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	int error;
	if (error = null_bypass(ap))
		return (error);
	/* Requires that arguments be restored. */
	ap->a_vap->va_fsid = ap->a_vp->v_mount->mnt_stat.f_fsid.val[0];
	return (0);
}


int
null_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	/*
	 * Do nothing (and _don't_ bypass).
	 * Wait to vrele lowervp until reclaim,
	 * so that until then our null_node is in the
	 * cache and reusable.
	 *
	 * NEEDSWORK: Someday, consider inactive'ing
	 * the lowervp and then trying to reactivate it
	 * with capabilities (v_id)
	 * like they do in the name lookup cache code.
	 * That's too much work for now.
	 */
	return (0);
}

int
null_reclaim(ap)
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct null_node *xp = VTONULL(vp);
	struct vnode *lowervp = xp->null_lowervp;

	/*
	 * Note: in vop_reclaim, vp->v_op == dead_vnodeop_p,
	 * so we can't call VOPs on ourself.
	 */
	/* After this assignment, this node will not be re-used. */
	xp->null_lowervp = NULL;
	remque(xp);
	FREE(vp->v_data, M_TEMP);
	vp->v_data = NULL;
	vrele (lowervp);
	return (0);
}


int
null_print(ap)
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	printf ("\ttag VT_NULLFS, vp=%x, lowervp=%x\n", vp, NULLVPTOLOWERVP(vp));
	return (0);
}


/*
 * XXX - vop_strategy must be hand coded because it has no
 * vnode in its arguments.
 * This goes away with a merged VM/buffer cache.
 */
int
null_strategy(ap)
	struct vop_strategy_args /* {
		struct buf *a_bp;
	} */ *ap;
{
	struct buf *bp = ap->a_bp;
	int error;
	struct vnode *savedvp;

	savedvp = bp->b_vp;
	bp->b_vp = NULLVPTOLOWERVP(bp->b_vp);

	error = VOP_STRATEGY(bp);

	bp->b_vp = savedvp;

	return (error);
}


/*
 * XXX - like vop_strategy, vop_bwrite must be hand coded because it has no
 * vnode in its arguments.
 * This goes away with a merged VM/buffer cache.
 */
int
null_bwrite(ap)
	struct vop_bwrite_args /* {
		struct buf *a_bp;
	} */ *ap;
{
	struct buf *bp = ap->a_bp;
	int error;
	struct vnode *savedvp;

	savedvp = bp->b_vp;
	bp->b_vp = NULLVPTOLOWERVP(bp->b_vp);

	error = VOP_BWRITE(bp);

	bp->b_vp = savedvp;

	return (error);
}

/*
 * Global vfs data structures
 */
int (**null_vnodeop_p)();
struct vnodeopv_entry_desc null_vnodeop_entries[] = {
	{ &vop_default_desc, null_bypass },

	{ &vop_getattr_desc, null_getattr },
	{ &vop_inactive_desc, null_inactive },
	{ &vop_reclaim_desc, null_reclaim },
	{ &vop_print_desc, null_print },

	{ &vop_strategy_desc, null_strategy },
	{ &vop_bwrite_desc, null_bwrite },

	{ (struct vnodeop_desc*)NULL, (int(*)())NULL }
};
struct vnodeopv_desc null_vnodeop_opv_desc =
	{ &null_vnodeop_p, null_vnodeop_entries };
