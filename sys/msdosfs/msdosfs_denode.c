/*	$NetBSD: msdosfs_denode.c,v 1.23.2.1 1997/11/18 23:49:52 mellon Exp $	*/

/*-
 * Copyright (C) 1994, 1995, 1997 Wolfgang Solfrank.
 * Copyright (C) 1994, 1995, 1997 TooLs GmbH.
 * All rights reserved.
 * Original code by Paul Popelka (paulp@uts.amdahl.com) (see below).
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Written by Paul Popelka (paulp@uts.amdahl.com)
 *
 * You can do anything you want with this software, just don't say you wrote
 * it, and don't remove this notice.
 *
 * This software is provided "as is".
 *
 * The author supplies this software to be publicly redistributed on the
 * understanding that the author is not responsible for the correct
 * functioning of this software in any circumstances and is not liable for
 * any damages caused by this software.
 *
 * October 1992
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/kernel.h>		/* defines "time" */
#include <sys/dirent.h>
#include <sys/namei.h>

#include <vm/vm.h>

#include <msdosfs/bpb.h>
#include <msdosfs/msdosfsmount.h>
#include <msdosfs/direntry.h>
#include <msdosfs/denode.h>
#include <msdosfs/fat.h>

struct denode **dehashtbl;
u_long dehash;			/* size of hash table - 1 */
#define	DEHASH(dev, dcl, doff)	(((dev) + (dcl) + (doff) / sizeof(struct direntry)) \
				 & dehash)

static struct denode *msdosfs_hashget __P((dev_t, u_long, u_long));
static void msdosfs_hashins __P((struct denode *));
static void msdosfs_hashrem __P((struct denode *));

void
msdosfs_init()
{
	dehashtbl = hashinit(desiredvnodes/2, M_MSDOSFSMNT, &dehash);
}

static struct denode *
msdosfs_hashget(dev, dirclust, diroff)
	dev_t dev;
	u_long dirclust;
	u_long diroff;
{
	struct denode *dep;

	for (;;)
		for (dep = dehashtbl[DEHASH(dev, dirclust, diroff)];;
		     dep = dep->de_next) {
			if (dep == NULL)
				return (NULL);
			if (dirclust == dep->de_dirclust &&
			    diroff == dep->de_diroffset &&
			    dev == dep->de_dev &&
			    dep->de_refcnt != 0) {
				if (dep->de_flag & DE_LOCKED) {
					dep->de_flag |= DE_WANTED;
					sleep(dep, PINOD);
					break;
				}
				if (!vget(DETOV(dep), 1))
					return (dep);
				break;
			}
		}
	/* NOTREACHED */
}

static void
msdosfs_hashins(dep)
	struct denode *dep;
{
	struct denode **depp, *deq;

	depp = &dehashtbl[DEHASH(dep->de_dev, dep->de_dirclust, dep->de_diroffset)];
	if ((deq = *depp) != NULL)
		deq->de_prev = &dep->de_next;
	dep->de_next = deq;
	dep->de_prev = depp;
	*depp = dep;
}

static void
msdosfs_hashrem(dep)
	struct denode *dep;
{
	struct denode *deq;

	if ((deq = dep->de_next) != NULL)
		deq->de_prev = dep->de_prev;
	*dep->de_prev = deq;
#ifdef DIAGNOSTIC
	dep->de_next = NULL;
	dep->de_prev = NULL;
#endif
}

/*
 * If deget() succeeds it returns with the gotten denode locked().
 *
 * pmp	     - address of msdosfsmount structure of the filesystem containing
 *	       the denode of interest.  The pm_dev field and the address of
 *	       the msdosfsmount structure are used.
 * dirclust  - which cluster bp contains, if dirclust is 0 (root directory)
 *	       diroffset is relative to the beginning of the root directory,
 *	       otherwise it is cluster relative.
 * diroffset - offset past begin of cluster of denode we want
 * depp	     - returns the address of the gotten denode.
 */
int
deget(pmp, dirclust, diroffset, depp)
	struct msdosfsmount *pmp;	/* so we know the maj/min number */
	u_long dirclust;		/* cluster this dir entry came from */
	u_long diroffset;		/* index of entry within the cluster */
	struct denode **depp;		/* returns the addr of the gotten denode */
{
	int error;
	extern int (**msdosfs_vnodeop_p) __P((void *));
	struct direntry *direntptr;
	struct denode *ldep;
	struct vnode *nvp;
	struct buf *bp;

#ifdef MSDOSFS_DEBUG
	printf("deget(pmp %p, dirclust %lu, diroffset %lx, depp %p)\n",
	    pmp, dirclust, diroffset, depp);
#endif

	/*
	 * On FAT32 filesystems, root is a (more or less) normal
	 * directory
	 */
	if (FAT32(pmp) && dirclust == MSDOSFSROOT)
		dirclust = pmp->pm_rootdirblk;

	/*
	 * See if the denode is in the denode cache. Use the location of
	 * the directory entry to compute the hash value. For subdir use
	 * address of "." entry. For root dir (if not FAT32) use cluster
	 * MSDOSFSROOT, offset MSDOSFSROOT_OFS
	 *
	 * NOTE: The check for de_refcnt > 0 below insures the denode being
	 * examined does not represent an unlinked but still open file.
	 * These files are not to be accessible even when the directory
	 * entry that represented the file happens to be reused while the
	 * deleted file is still open.
	 */
	ldep = msdosfs_hashget(pmp->pm_dev, dirclust, diroffset);
	if (ldep) {
		*depp = ldep;
		return (0);
	}

	/*
	 * Directory entry was not in cache, have to create a vnode and
	 * copy it from the passed disk buffer.
	 */
	/* getnewvnode() does a VREF() on the vnode */
	error = getnewvnode(VT_MSDOSFS, pmp->pm_mountp,
			    msdosfs_vnodeop_p, &nvp);
	if (error) {
		*depp = 0;
		return (error);
	}
	MALLOC(ldep, struct denode *, sizeof(struct denode), M_MSDOSFSNODE, M_WAITOK);
	bzero((caddr_t)ldep, sizeof *ldep);
	nvp->v_data = ldep;
	ldep->de_vnode = nvp;
	ldep->de_flag = 0;
	ldep->de_devvp = 0;
	ldep->de_lockf = 0;
	ldep->de_dev = pmp->pm_dev;
	ldep->de_dirclust = dirclust;
	ldep->de_diroffset = diroffset;
	fc_purge(ldep, 0);	/* init the fat cache for this denode */

	/*
	 * Insert the denode into the hash queue and lock the denode so it
	 * can't be accessed until we've read it in and have done what we
	 * need to it.
	 */
	VOP_LOCK(nvp);
	msdosfs_hashins(ldep);

	ldep->de_pmp = pmp;
	ldep->de_devvp = pmp->pm_devvp;
	ldep->de_refcnt = 1;
	/*
	 * Copy the directory entry into the denode area of the vnode.
	 */
	if ((dirclust == MSDOSFSROOT
	     || (FAT32(pmp) && dirclust == pmp->pm_rootdirblk))
	    && diroffset == MSDOSFSROOT_OFS) {
		/*
		 * Directory entry for the root directory. There isn't one,
		 * so we manufacture one. We should probably rummage
		 * through the root directory and find a label entry (if it
		 * exists), and then use the time and date from that entry
		 * as the time and date for the root denode.
		 */
		nvp->v_flag |= VROOT; /* should be further down		XXX */

		ldep->de_Attributes = ATTR_DIRECTORY;
		if (FAT32(pmp))
			ldep->de_StartCluster = pmp->pm_rootdirblk;
			/* de_FileSize will be filled in further down */
		else {
			ldep->de_StartCluster = MSDOSFSROOT;
			ldep->de_FileSize = pmp->pm_rootdirsize * pmp->pm_BytesPerSec;
		}
		/*
		 * fill in time and date so that dos2unixtime() doesn't
		 * spit up when called from msdosfs_getattr() with root
		 * denode
		 */
		ldep->de_CHun = 0;
		ldep->de_CTime = 0x0000;	/* 00:00:00	 */
		ldep->de_CDate = (0 << DD_YEAR_SHIFT) | (1 << DD_MONTH_SHIFT)
		    | (1 << DD_DAY_SHIFT);
		/* Jan 1, 1980	 */
		ldep->de_ADate = ldep->de_CDate;
		ldep->de_MTime = ldep->de_CTime;
		ldep->de_MDate = ldep->de_CDate;
		/* leave the other fields as garbage */
	} else {
		error = readep(pmp, dirclust, diroffset, &bp, &direntptr);
		if (error)
			return (error);
		DE_INTERNALIZE(ldep, direntptr);
		brelse(bp);
	}

	/*
	 * Fill in a few fields of the vnode and finish filling in the
	 * denode.  Then return the address of the found denode.
	 */
	if (ldep->de_Attributes & ATTR_DIRECTORY) {
		/*
		 * Since DOS directory entries that describe directories
		 * have 0 in the filesize field, we take this opportunity
		 * to find out the length of the directory and plug it into
		 * the denode structure.
		 */
		u_long size;

		nvp->v_type = VDIR;
		if (ldep->de_StartCluster != MSDOSFSROOT) {
			error = pcbmap(ldep, 0xffff, 0, &size, 0);
			if (error == E2BIG) {
				ldep->de_FileSize = de_cn2off(pmp, size);
				error = 0;
			} else
				printf("deget(): pcbmap returned %d\n", error);
		}
	} else
		nvp->v_type = VREG;
	VREF(ldep->de_devvp);
	*depp = ldep;
	return (0);
}

int
deupdat(dep, waitfor)
	struct denode *dep;
	int waitfor;
{
	struct timespec ts;

	TIMEVAL_TO_TIMESPEC(&time, &ts);
	return (VOP_UPDATE(DETOV(dep), &ts, &ts, waitfor));
}

/*
 * Truncate the file described by dep to the length specified by length.
 */
int
detrunc(dep, length, flags, cred, p)
	struct denode *dep;
	u_long length;
	int flags;
	struct ucred *cred;
	struct proc *p;
{
	int error;
	int allerror;
	int vflags;
	u_long eofentry;
	u_long chaintofree;
	daddr_t bn;
	int boff;
	int isadir = dep->de_Attributes & ATTR_DIRECTORY;
	struct buf *bp;
	struct msdosfsmount *pmp = dep->de_pmp;

#ifdef MSDOSFS_DEBUG
	printf("detrunc(): file %s, length %lu, flags %x\n", dep->de_Name, length, flags);
#endif

	/*
	 * Disallow attempts to truncate the root directory since it is of
	 * fixed size.  That's just the way dos filesystems are.  We use
	 * the VROOT bit in the vnode because checking for the directory
	 * bit and a startcluster of 0 in the denode is not adequate to
	 * recognize the root directory at this point in a file or
	 * directory's life.
	 */
	if ((DETOV(dep)->v_flag & VROOT) && !FAT32(pmp)) {
		printf("detrunc(): can't truncate root directory, clust %ld, offset %ld\n",
		    dep->de_dirclust, dep->de_diroffset);
		return (EINVAL);
	}

	vnode_pager_setsize(DETOV(dep), length);

	if (dep->de_FileSize < length)
		return (deextend(dep, length, cred));

	/*
	 * If the desired length is 0 then remember the starting cluster of
	 * the file and set the StartCluster field in the directory entry
	 * to 0.  If the desired length is not zero, then get the number of
	 * the last cluster in the shortened file.  Then get the number of
	 * the first cluster in the part of the file that is to be freed.
	 * Then set the next cluster pointer in the last cluster of the
	 * file to CLUST_EOFE.
	 */
	if (length == 0) {
		chaintofree = dep->de_StartCluster;
		dep->de_StartCluster = 0;
		eofentry = ~0;
	} else {
		error = pcbmap(dep, de_clcount(pmp, length) - 1, 0,
			       &eofentry, 0);
		if (error) {
#ifdef MSDOSFS_DEBUG
			printf("detrunc(): pcbmap fails %d\n", error);
#endif
			return (error);
		}
	}

	fc_purge(dep, de_clcount(pmp, length));

	/*
	 * If the new length is not a multiple of the cluster size then we
	 * must zero the tail end of the new last cluster in case it
	 * becomes part of the file again because of a seek.
	 */
	if ((boff = length & pmp->pm_crbomask) != 0) {
		if (isadir) {
			bn = cntobn(pmp, eofentry);
			error = bread(pmp->pm_devvp, bn, pmp->pm_bpcluster,
			    NOCRED, &bp);
		} else {
			bn = de_blk(pmp, length);
			error = bread(DETOV(dep), bn, pmp->pm_bpcluster,
			    NOCRED, &bp);
		}
		if (error) {
			brelse(bp);
#ifdef MSDOSFS_DEBUG
			printf("detrunc(): bread fails %d\n", error);
#endif
			return (error);
		}
		vnode_pager_uncache(DETOV(dep));	/* what's this for? */
		/*
		 * is this the right place for it?
		 */
		bzero(bp->b_data + boff, pmp->pm_bpcluster - boff);
		if (flags & IO_SYNC)
			bwrite(bp);
		else
			bdwrite(bp);
	}

	/*
	 * Write out the updated directory entry.  Even if the update fails
	 * we free the trailing clusters.
	 */
	dep->de_FileSize = length;
	if (!isadir)
		dep->de_flag |= DE_UPDATE|DE_MODIFIED;
	vflags = (length > 0 ? V_SAVE : 0) | V_SAVEMETA;
	vinvalbuf(DETOV(dep), vflags, cred, p, 0, 0);
	allerror = deupdat(dep, 1);
#ifdef MSDOSFS_DEBUG
	printf("detrunc(): allerror %d, eofentry %lu\n",
	       allerror, eofentry);
#endif

	/*
	 * If we need to break the cluster chain for the file then do it
	 * now.
	 */
	if (eofentry != ~0) {
		error = fatentry(FAT_GET_AND_SET, pmp, eofentry,
				 &chaintofree, CLUST_EOFE);
		if (error) {
#ifdef MSDOSFS_DEBUG
			printf("detrunc(): fatentry errors %d\n", error);
#endif
			return (error);
		}
		fc_setcache(dep, FC_LASTFC, de_cluster(pmp, length - 1),
			    eofentry);
	}

	/*
	 * Now free the clusters removed from the file because of the
	 * truncation.
	 */
	if (chaintofree != 0 && !MSDOSFSEOF(pmp, chaintofree))
		freeclusterchain(pmp, chaintofree);

	return (allerror);
}

/*
 * Extend the file described by dep to length specified by length.
 */
int
deextend(dep, length, cred)
	struct denode *dep;
	u_long length;
	struct ucred *cred;
{
	struct msdosfsmount *pmp = dep->de_pmp;
	u_long count;
	int error;

	/*
	 * The root of a DOS filesystem cannot be extended.
	 */
	if ((DETOV(dep)->v_flag & VROOT) && !FAT32(pmp))
		return (EINVAL);

	/*
	 * Directories cannot be extended.
	 */
	if (dep->de_Attributes & ATTR_DIRECTORY)
		return (EISDIR);

	if (length <= dep->de_FileSize)
		panic("deextend: file too large");

	/*
	 * Compute the number of clusters to allocate.
	 */
	count = de_clcount(pmp, length) - de_clcount(pmp, dep->de_FileSize);
	if (count > 0) {
		if (count > pmp->pm_freeclustercount)
			return (ENOSPC);
		error = extendfile(dep, count, NULL, NULL, DE_CLEAR);
		if (error) {
			/* truncate the added clusters away again */
			(void) detrunc(dep, dep->de_FileSize, 0, cred, NULL);
			return (error);
		}
	}

	dep->de_FileSize = length;
	dep->de_flag |= DE_UPDATE|DE_MODIFIED;
	return (deupdat(dep, 1));
}

/*
 * Move a denode to its correct hash queue after the file it represents has
 * been moved to a new directory.
 */
void
reinsert(dep)
	struct denode *dep;
{
	/*
	 * Fix up the denode cache.  If the denode is for a directory,
	 * there is nothing to do since the hash is based on the starting
	 * cluster of the directory file and that hasn't changed.  If for a
	 * file the hash is based on the location of the directory entry,
	 * so we must remove it from the cache and re-enter it with the
	 * hash based on the new location of the directory entry.
	 */
	if (dep->de_Attributes & ATTR_DIRECTORY)
		return;
	msdosfs_hashrem(dep);
	msdosfs_hashins(dep);
}

int
msdosfs_reclaim(v)
	void *v;
{
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct denode *dep = VTODE(vp);
	extern int prtactive;

#ifdef MSDOSFS_DEBUG
	printf("msdosfs_reclaim(): dep %p, file %s, refcnt %ld\n",
	    dep, dep->de_Name, dep->de_refcnt);
#endif

	if (prtactive && vp->v_usecount != 0)
		vprint("msdosfs_reclaim(): pushing active", vp);
	/*
	 * Remove the denode from its hash chain.
	 */
	msdosfs_hashrem(dep);
	/*
	 * Purge old data structures associated with the denode.
	 */
	cache_purge(vp);
	if (dep->de_devvp) {
		vrele(dep->de_devvp);
		dep->de_devvp = 0;
	}
#if 0 /* XXX */
	dep->de_flag = 0;
#endif
	FREE(dep, M_MSDOSFSNODE);
	vp->v_data = NULL;
	return (0);
}

int
msdosfs_inactive(v)
	void *v;
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct denode *dep = VTODE(vp);
	int error;
	extern int prtactive;

#ifdef MSDOSFS_DEBUG
	printf("msdosfs_inactive(): dep %p, de_Name[0] %x\n", dep, dep->de_Name[0]);
#endif

	if (prtactive && vp->v_usecount != 0)
		vprint("msdosfs_inactive(): pushing active", vp);

	/*
	 * Get rid of denodes related to stale file handles.
	 */
	if (dep->de_Name[0] == SLOT_DELETED) {
		if ((vp->v_flag & VXLOCK) == 0)
			vgone(vp);
		return (0);
	}

	error = 0;
#ifdef DIAGNOSTIC
	if (VOP_ISLOCKED(vp))
		panic("msdosfs_inactive: locked denode");
	if (curproc)
		dep->de_lockholder = curproc->p_pid;
	else
		dep->de_lockholder = -1;
#endif
	dep->de_flag |= DE_LOCKED;
	/*
	 * If the file has been deleted and it is on a read/write
	 * filesystem, then truncate the file, and mark the directory slot
	 * as empty.  (This may not be necessary for the dos filesystem.)
	 */
#ifdef MSDOSFS_DEBUG
	printf("msdosfs_inactive(): dep %p, refcnt %ld, mntflag %x, MNT_RDONLY %x\n",
	       dep, dep->de_refcnt, vp->v_mount->mnt_flag, MNT_RDONLY);
#endif
	if (dep->de_refcnt <= 0 && (vp->v_mount->mnt_flag & MNT_RDONLY) == 0) {
		error = detrunc(dep, (u_long)0, 0, NOCRED, NULL);
		dep->de_Name[0] = SLOT_DELETED;
	}
	deupdat(dep, 0);
	VOP_UNLOCK(vp);
	/*
	 * If we are done with the denode, reclaim it
	 * so that it can be reused immediately.
	 */
#ifdef MSDOSFS_DEBUG
	printf("msdosfs_inactive(): v_usecount %d, de_Name[0] %x\n", vp->v_usecount,
	       dep->de_Name[0]);
#endif
	if (vp->v_usecount == 0 && dep->de_Name[0] == SLOT_DELETED)
		vgone(vp);
	return (error);
}
