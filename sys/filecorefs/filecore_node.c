/*	$NetBSD: filecore_node.c,v 1.4.2.2 2000/11/22 16:05:13 bouyer Exp $	*/

/*-
 * Copyright (c) 1998 Andrew McMurry
 * Copyright (c) 1982, 1986, 1989, 1994 
 *           The Regents of the University of California.
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
 *	filecore_node.c		1.0	1998/6/4
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/stat.h>

#include <filecorefs/filecore.h>
#include <filecorefs/filecore_extern.h>
#include <filecorefs/filecore_node.h>
#include <filecorefs/filecore_mount.h>

/*
 * Structures associated with filecore_node caching.
 */
struct filecore_node **filecorehashtbl;
u_long filecorehash;
#define	INOHASH(device, inum)	(((device) + ((inum)>>12)) & filecorehash)
struct simplelock filecore_ihash_slock;

struct pool filecore_node_pool;

int prtactive;	/* 1 => print out reclaim of active vnodes */

/*
 * Initialize hash links for inodes and dnodes.
 */
void
filecore_init()
{
	filecorehashtbl = hashinit(desiredvnodes, HASH_LIST, M_FILECOREMNT,
	    M_WAITOK, &filecorehash);
	simple_lock_init(&filecore_ihash_slock);
	pool_init(&filecore_node_pool, sizeof(struct filecore_node),
	    0, 0, 0, "filecrnopl", 0, pool_page_alloc_nointr,
	    pool_page_free_nointr, M_FILECORENODE);
}

/*
 * Destroy node pool and hash table.
 */
void
filecore_done()
{
	pool_destroy(&filecore_node_pool);
	hashdone(filecorehashtbl, M_FILECOREMNT);
}

/*
 * Use the device/inum pair to find the incore inode, and return a pointer
 * to it. If it is in core, but locked, wait for it.
 */
struct vnode *
filecore_ihashget(dev, inum)
	dev_t dev;
	ino_t inum;
{
	struct filecore_node *ip;
	struct vnode *vp;

loop:
	simple_lock(&filecore_ihash_slock);
	for (ip = filecorehashtbl[INOHASH(dev, inum)]; ip; ip = ip->i_next) {
		if (inum == ip->i_number && dev == ip->i_dev) {
			vp = ITOV(ip);
			simple_lock(&vp->v_interlock);
			simple_unlock(&filecore_ihash_slock);
			if (vget(vp, LK_EXCLUSIVE | LK_INTERLOCK))
				goto loop;
			return (vp);
		}
	}
	simple_unlock(&filecore_ihash_slock);
	return (NULL);
}

/*
 * Insert the inode into the hash table, and return it locked.
 */
void
filecore_ihashins(ip)
	struct filecore_node *ip;
{
	struct filecore_node **ipp, *iq;
	struct vnode *vp;

	simple_lock(&filecore_ihash_slock);
	ipp = &filecorehashtbl[INOHASH(ip->i_dev, ip->i_number)];
	if ((iq = *ipp) != NULL)
		iq->i_prev = &ip->i_next;
	ip->i_next = iq;
	ip->i_prev = ipp;
	*ipp = ip;
	simple_unlock(&filecore_ihash_slock);

	vp = ip->i_vnode;
	lockmgr(&vp->v_lock, LK_EXCLUSIVE, &vp->v_interlock);
}

/*
 * Remove the inode from the hash table.
 */
void
filecore_ihashrem(ip)
	struct filecore_node *ip;
{
	struct filecore_node *iq;

	simple_lock(&filecore_ihash_slock);
	if ((iq = ip->i_next) != NULL)
		iq->i_prev = ip->i_prev;
	*ip->i_prev = iq;
#ifdef DIAGNOSTIC
	ip->i_next = NULL;
	ip->i_prev = NULL;
#endif
	simple_unlock(&filecore_ihash_slock);
}

/*
 * Last reference to an inode, write the inode out and if necessary,
 * truncate and deallocate the file.
 */
int
filecore_inactive(v)
	void *v;
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct proc *p = ap->a_p;
	struct filecore_node *ip = VTOI(vp);
	int error = 0;
	
	if (prtactive && vp->v_usecount != 0)
		vprint("filecore_inactive: pushing active", vp);
	
	ip->i_flag = 0;
	VOP_UNLOCK(vp, 0);
	/*
	 * If we are done with the inode, reclaim it
	 * so that it can be reused immediately.
	 */
	if (filecore_staleinode(ip))
		vrecycle(vp, (struct simplelock *)0, p);
	return error;
}

/*
 * Reclaim an inode so that it can be used for other purposes.
 */
int
filecore_reclaim(v)
	void *v;
{
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct filecore_node *ip = VTOI(vp);
	
	if (prtactive && vp->v_usecount != 0)
		vprint("filecore_reclaim: pushing active", vp);
	/*
	 * Remove the inode from its hash chain.
	 */
	filecore_ihashrem(ip);
	/*
	 * Purge old data structures associated with the inode.
	 */
	cache_purge(vp);
	if (ip->i_devvp) {
		vrele(ip->i_devvp);
		ip->i_devvp = 0;
	}
	pool_put(&filecore_node_pool, vp->v_data);
	vp->v_data = NULL;
	return (0);
}
