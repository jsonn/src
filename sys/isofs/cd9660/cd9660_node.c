/*	$NetBSD: cd9660_node.c,v 1.5.2.2 1994/07/20 03:17:45 cgd Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1989, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley
 * by Pace Willisson (pace@blitz.com).  The Rock Ridge Extension
 * Support code is derived from software contributed to Berkeley
 * by Atsushi Murai (amurai@spec.co.jp).
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
 *	@(#)cd9660_node.c	8.2 (Berkeley) 1/23/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/stat.h>

#include <isofs/cd9660/iso.h>
#include <isofs/cd9660/cd9660_node.h>
#include <isofs/cd9660/iso_rrip.h>

/*
 * Structures associated with iso_node caching.
 */
struct iso_node **isohashtbl;
u_long isohash;
#define	INOHASH(device, inum)	(((device) + ((inum)>>12)) & isohash)

#ifdef ISODEVMAP
struct iso_node **idvhashtbl;
u_long idvhash;
#define	DNOHASH(device, inum)	(((device) + ((inum)>>12)) & idvhash)
#endif

int prtactive;	/* 1 => print out reclaim of active vnodes */

/*
 * Initialize hash links for inodes and dnodes.
 */
cd9660_init()
{

	isohashtbl = hashinit(desiredvnodes, M_ISOFSMNT, &isohash);
#ifdef ISODEVMAP
	idvhashtbl = hashinit(desiredvnodes / 8, M_ISOFSMNT, &idvhash);
#endif
}

#ifdef ISODEVMAP
/*
 * Enter a new node into the device hash list
 */
struct iso_dnode *
iso_dmap(device, inum, create)
	dev_t	device;
	ino_t	inum;
	int	create;
{
	register struct iso_dnode **dpp, *dp, *dq;

	dpp = &idvhashtbl[DNOHASH(device, inum)];
	for (dp = *dpp;; dp = dp->d_next) {
		if (dp == NULL)
			return (NULL);
		if (inum == dp->i_number && device == dp->i_dev)
			return (dp);

	if (!create)
		return (NULL);

	MALLOC(dp, struct iso_dnode *, sizeof(struct iso_dnode), M_CACHE,
	       M_WAITOK);
	dp->i_dev = dev;
	dp->i_number = ino;

	if (dq = *dpp)
		dq->d_prev = dp->d_next;
	dp->d_next = dq;
	dp->d_prev = dpp;
	*dpp = dp;

	return (dp);
}

void
iso_dunmap(device)
	dev_t device;
{
	struct iso_dnode **dpp, *dp, *dq;
	
	for (dpp = idvhashtbl; dpp <= idvhashtbl + idvhash; dpp++) {
		for (dp = *dpp; dp != NULL; dp = dq)
			dq = dp->d_next;
			if (device == dp->i_dev) {
				if (dq)
					dq->d_prev = dp->d_prev;
				*dp->d_prev = dq;
				FREE(dp, M_CACHE);
			}
		}
	}
}
#endif

/*
 * Use the device/inum pair to find the incore inode, and return a pointer
 * to it. If it is in core, but locked, wait for it.
 */
struct vnode *
cd9660_ihashget(device, inum)
	dev_t device;
	ino_t inum;
{
	register struct iso_node *ip;
	struct vnode *vp;

	for (;;)
		for (ip = isohashtbl[INOHASH(device, inum)];; ip = ip->i_next) {
			if (ip == NULL)
				return (NULL);
			if (inum == ip->i_number && device == ip->i_dev) {
				if (ip->i_flag & IN_LOCKED) {
					ip->i_flag |= IN_WANTED;
					sleep(ip, PINOD);
					break;
				}
				vp = ITOV(ip);
				if (!vget(vp, 1))
					return (vp);
				break;
			}
		}
	/* NOTREACHED */
}

/*
 * Insert the inode into the hash table, and return it locked.
 */
void
cd9660_ihashins(ip)
	struct iso_node *ip;
{
	struct iso_node **ipp, *iq;

	ipp = &isohashtbl[INOHASH(ip->i_dev, ip->i_number)];
	if (iq = *ipp)
		iq->i_prev = &ip->i_next;
	ip->i_next = iq;
	ip->i_prev = ipp;
	*ipp = ip;
	if (ip->i_flag & IN_LOCKED)
		panic("cd9660_ihashins: already locked");
	if (curproc)
		ip->i_lockholder = curproc->p_pid;
	else
		ip->i_lockholder = -1;
	ip->i_flag |= IN_LOCKED;
}

/*
 * Remove the inode from the hash table.
 */
void
cd9660_ihashrem(ip)
	register struct iso_node *ip;
{
	register struct iso_node *iq;

	if (iq = ip->i_next)
		iq->i_prev = ip->i_prev;
	*ip->i_prev = iq;
#ifdef DIAGNOSTIC
	ip->i_next = NULL;
	ip->i_prev = NULL;
#endif
}

/*
 * Last reference to an inode, write the inode out and if necessary,
 * truncate and deallocate the file.
 */
int
cd9660_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	register struct iso_node *ip = VTOI(vp);
	int mode, error = 0;
	
	if (prtactive && vp->v_usecount != 0)
		vprint("cd9660_inactive: pushing active", vp);
	
	ip->i_flag = 0;
	/*
	 * If we are done with the inode, reclaim it
	 * so that it can be reused immediately.
	 */
	if (vp->v_usecount == 0 && ip->inode.iso_mode == 0)
		vgone(vp);
	return error;
}

/*
 * Reclaim an inode so that it can be used for other purposes.
 */
int
cd9660_reclaim(ap)
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct iso_node *ip = VTOI(vp);
	int i;
	
	if (prtactive && vp->v_usecount != 0)
		vprint("cd9660_reclaim: pushing active", vp);
	/*
	 * Remove the inode from its hash chain.
	 */
	cd9660_ihashrem(ip);
	/*
	 * Purge old data structures associated with the inode.
	 */
	cache_purge(vp);
	if (ip->i_devvp) {
		vrele(ip->i_devvp);
		ip->i_devvp = 0;
	}
	FREE(vp->v_data, M_ISOFSNODE);
	vp->v_data = NULL;
	return (0);
}

/*
 * File attributes
 */
void
cd9660_defattr(isodir, inop, bp)
	struct iso_directory_record *isodir;
	struct iso_node *inop;
	struct buf *bp;
{
	struct buf *bp2 = NULL;
	struct iso_mnt *imp;
	struct iso_extended_attributes *ap = NULL;
	int off;
	
	if (isonum_711(isodir->flags)&2) {
		inop->inode.iso_mode = S_IFDIR;
		/*
		 * If we return 2, fts() will assume there are no subdirectories
		 * (just links for the path and .), so instead we return 1.
		 */
		inop->inode.iso_links = 1;
	} else {
		inop->inode.iso_mode = S_IFREG;
		inop->inode.iso_links = 1;
	}
	if (!bp
	    && ((imp = inop->i_mnt)->im_flags & ISOFSMNT_EXTATT)
	    && (off = isonum_711(isodir->ext_attr_length))) {
		VOP_BLKATOFF(ITOV(inop), (off_t)-(off << imp->im_bshift), NULL,
			     &bp2);
		bp = bp2;
	}
	if (bp) {
		ap = (struct iso_extended_attributes *)bp->b_data;
		
		if (isonum_711(ap->version) == 1) {
			if (!(ap->perm[0]&0x40))
				inop->inode.iso_mode |= VEXEC >> 6;
			if (!(ap->perm[0]&0x10))
				inop->inode.iso_mode |= VREAD >> 6;
			if (!(ap->perm[0]&4))
				inop->inode.iso_mode |= VEXEC >> 3;
			if (!(ap->perm[0]&1))
				inop->inode.iso_mode |= VREAD >> 3;
			if (!(ap->perm[1]&0x40))
				inop->inode.iso_mode |= VEXEC;
			if (!(ap->perm[1]&0x10))
				inop->inode.iso_mode |= VREAD;
			inop->inode.iso_uid = isonum_723(ap->owner); /* what about 0? */
			inop->inode.iso_gid = isonum_723(ap->group); /* what about 0? */
		} else
			ap = NULL;
	}
	if (!ap) {
		inop->inode.iso_mode |= VREAD|VEXEC|(VREAD|VEXEC)>>3|(VREAD|VEXEC)>>6;
		inop->inode.iso_uid = (uid_t)0;
		inop->inode.iso_gid = (gid_t)0;
	}
	if (bp2)
		brelse(bp2);
}

/*
 * Time stamps
 */
void
cd9660_deftstamp(isodir,inop,bp)
	struct iso_directory_record *isodir;
	struct iso_node *inop;
	struct buf *bp;
{
	struct buf *bp2 = NULL;
	struct iso_mnt *imp;
	struct iso_extended_attributes *ap = NULL;
	int off;
	
	if (!bp
	    && ((imp = inop->i_mnt)->im_flags & ISOFSMNT_EXTATT)
	    && (off = isonum_711(isodir->ext_attr_length))) {
		VOP_BLKATOFF(ITOV(inop), (off_t)-(off << imp->im_bshift), NULL,
			     &bp2);
		bp = bp2;
	}
	if (bp) {
		ap = (struct iso_extended_attributes *)bp->b_data;
		
		if (isonum_711(ap->version) == 1) {
			if (!cd9660_tstamp_conv17(ap->ftime,&inop->inode.iso_atime))
				cd9660_tstamp_conv17(ap->ctime,&inop->inode.iso_atime);
			if (!cd9660_tstamp_conv17(ap->ctime,&inop->inode.iso_ctime))
				inop->inode.iso_ctime = inop->inode.iso_atime;
			if (!cd9660_tstamp_conv17(ap->mtime,&inop->inode.iso_mtime))
				inop->inode.iso_mtime = inop->inode.iso_ctime;
		} else
			ap = NULL;
	}
	if (!ap) {
		cd9660_tstamp_conv7(isodir->date,&inop->inode.iso_ctime);
		inop->inode.iso_atime = inop->inode.iso_ctime;
		inop->inode.iso_mtime = inop->inode.iso_ctime;
	}
	if (bp2)
		brelse(bp2);
}

int
cd9660_tstamp_conv7(pi,pu)
char *pi;
struct timeval *pu;
{
	int i;
	int crtime, days;
	int y, m, d, hour, minute, second, tz;
	
	y = pi[0] + 1900;
	m = pi[1];
	d = pi[2];
	hour = pi[3];
	minute = pi[4];
	second = pi[5];
	tz = pi[6];
	
	if (y < 1970) {
		pu->tv_sec  = 0;
		pu->tv_usec = 0;
		return 0;
	} else {
#ifdef	ORIGINAL
		/* computes day number relative to Sept. 19th,1989 */
		/* don't even *THINK* about changing formula. It works! */
		days = 367*(y-1980)-7*(y+(m+9)/12)/4-3*((y+(m-9)/7)/100+1)/4+275*m/9+d-100;
#else
		/*
		 * Changed :-) to make it relative to Jan. 1st, 1970
		 * and to disambiguate negative division
		 */
		days = 367*(y-1960)-7*(y+(m+9)/12)/4-3*((y+(m+9)/12-1)/100+1)/4+275*m/9+d-239;
#endif
		crtime = ((((days * 24) + hour) * 60 + minute) * 60) + second;
		
		/* timezone offset is unreliable on some disks */
		if (-48 <= tz && tz <= 52)
			crtime -= tz * 15 * 60;
	}
	pu->tv_sec  = crtime;
	pu->tv_usec = 0;
	return 1;
}

static u_int
cd9660_chars2ui(begin,len)
	u_char *begin;
	int len;
{
	u_int rc;
	
	for (rc = 0; --len >= 0;) {
		rc *= 10;
		rc += *begin++ - '0';
	}
	return rc;
}

int
cd9660_tstamp_conv17(pi,pu)
	u_char *pi;
	struct timeval *pu;
{
	u_char buf[7];
	
	/* year:"0001"-"9999" -> -1900  */
	buf[0] = cd9660_chars2ui(pi,4) - 1900;
	
	/* month: " 1"-"12"      -> 1 - 12 */
	buf[1] = cd9660_chars2ui(pi + 4,2);
	
	/* day:   " 1"-"31"      -> 1 - 31 */
	buf[2] = cd9660_chars2ui(pi + 6,2);
	
	/* hour:  " 0"-"23"      -> 0 - 23 */
	buf[3] = cd9660_chars2ui(pi + 8,2);
	
	/* minute:" 0"-"59"      -> 0 - 59 */
	buf[4] = cd9660_chars2ui(pi + 10,2);
	
	/* second:" 0"-"59"      -> 0 - 59 */
	buf[5] = cd9660_chars2ui(pi + 12,2);
	
	/* difference of GMT */
	buf[6] = pi[16];
	
	return cd9660_tstamp_conv7(buf,pu);
}

ino_t
isodirino(isodir, imp)
	struct iso_directory_record *isodir;
	struct iso_mnt *imp;
{
	ino_t ino;

	ino = (isonum_733(isodir->extent) + isonum_711(isodir->ext_attr_length))
	      << imp->im_bshift;
	return (ino);
}
