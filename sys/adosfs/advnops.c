/*	$NetBSD: advnops.c,v 1.35.4.1 1997/10/14 08:16:35 thorpej Exp $	*/

/*
 * Copyright (c) 1994 Christian E. Hopps
 * Copyright (c) 1996 Matthias Scheler
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
 *      This product includes software developed by Christian E. Hopps.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <sys/namei.h>
#include <sys/buf.h>
#include <sys/dirent.h>
#include <sys/malloc.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <sys/proc.h>

#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h>
#include <adosfs/adosfs.h>

extern struct vnodeops adosfs_vnodeops;

#define	adosfs_open	genfs_nullop
int	adosfs_getattr	__P((void *));
int	adosfs_read	__P((void *));
int	adosfs_write	__P((void *));
int	adosfs_ioctl	__P((void *));
#define	adosfs_poll	genfs_poll
int	adosfs_strategy	__P((void *));
int	adosfs_link	__P((void *));
int	adosfs_symlink	__P((void *));
#define	adosfs_abortop	genfs_abortop
int	adosfs_lock	__P((void *));
int	adosfs_unlock	__P((void *));
int	adosfs_bmap	__P((void *));
int	adosfs_print	__P((void *));
int	adosfs_readdir	__P((void *));
int	adosfs_access	__P((void *));
int	adosfs_readlink	__P((void *));
int	adosfs_inactive	__P((void *));
int	adosfs_islocked	__P((void *));
int	adosfs_reclaim	__P((void *));
int	adosfs_pathconf	__P((void *));

#define adosfs_close 	genfs_nullop
#define adosfs_fsync 	genfs_nullop
#ifdef NFSSERVER
int	lease_check __P((void *));
#define	adosfs_lease_check	lease_check
#else
#define adosfs_lease_check 	genfs_nullop
#endif
#define adosfs_seek 	genfs_seek
#define adosfs_vfree 	genfs_nullop

#define adosfs_advlock 	genfs_eopnotsupp
#define adosfs_blkatoff	genfs_eopnotsupp
#define adosfs_bwrite 	genfs_eopnotsupp
#define adosfs_create 	genfs_eopnotsupp
#define adosfs_mkdir 	genfs_eopnotsupp
#define adosfs_mknod 	genfs_eopnotsupp
#define adosfs_mmap 	genfs_eopnotsupp
#define adosfs_remove 	genfs_eopnotsupp
#define adosfs_rename 	genfs_eopnotsupp
#define adosfs_rmdir 	genfs_eopnotsupp
#define adosfs_setattr 	genfs_eopnotsupp
#define adosfs_truncate	genfs_eopnotsupp
#define adosfs_update 	genfs_nullop
#define adosfs_valloc 	genfs_eopnotsupp

struct vnodeopv_entry_desc adosfs_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, adosfs_lookup },		/* lookup */
	{ &vop_create_desc, adosfs_create },		/* create */
	{ &vop_mknod_desc, adosfs_mknod },		/* mknod */
	{ &vop_open_desc, adosfs_open },		/* open */
	{ &vop_close_desc, adosfs_close },		/* close */
	{ &vop_access_desc, adosfs_access },		/* access */
	{ &vop_getattr_desc, adosfs_getattr },		/* getattr */
	{ &vop_setattr_desc, adosfs_setattr },		/* setattr */
	{ &vop_read_desc, adosfs_read },		/* read */
	{ &vop_write_desc, adosfs_write },		/* write */
	{ &vop_lease_desc, adosfs_lease_check },	/* lease */
	{ &vop_ioctl_desc, adosfs_ioctl },		/* ioctl */
	{ &vop_poll_desc, adosfs_poll },		/* poll */
	{ &vop_mmap_desc, adosfs_mmap },		/* mmap */
	{ &vop_fsync_desc, adosfs_fsync },		/* fsync */
	{ &vop_seek_desc, adosfs_seek },		/* seek */
	{ &vop_remove_desc, adosfs_remove },		/* remove */
	{ &vop_link_desc, adosfs_link },		/* link */
	{ &vop_rename_desc, adosfs_rename },		/* rename */
	{ &vop_mkdir_desc, adosfs_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, adosfs_rmdir },		/* rmdir */
	{ &vop_symlink_desc, adosfs_symlink },		/* symlink */
	{ &vop_readdir_desc, adosfs_readdir },		/* readdir */
	{ &vop_readlink_desc, adosfs_readlink },	/* readlink */
	{ &vop_abortop_desc, adosfs_abortop },		/* abortop */
	{ &vop_inactive_desc, adosfs_inactive },	/* inactive */
	{ &vop_reclaim_desc, adosfs_reclaim },		/* reclaim */
	{ &vop_lock_desc, adosfs_lock },		/* lock */
	{ &vop_unlock_desc, adosfs_unlock },		/* unlock */
	{ &vop_bmap_desc, adosfs_bmap },		/* bmap */
	{ &vop_strategy_desc, adosfs_strategy },	/* strategy */
	{ &vop_print_desc, adosfs_print },		/* print */
	{ &vop_islocked_desc, adosfs_islocked },	/* islocked */
	{ &vop_pathconf_desc, adosfs_pathconf },	/* pathconf */
	{ &vop_advlock_desc, adosfs_advlock },		/* advlock */
	{ &vop_blkatoff_desc, adosfs_blkatoff },	/* blkatoff */
	{ &vop_valloc_desc, adosfs_valloc },		/* valloc */
	{ &vop_vfree_desc, adosfs_vfree },		/* vfree */
	{ &vop_truncate_desc, adosfs_truncate },	/* truncate */
	{ &vop_update_desc, adosfs_update },		/* update */
	{ &vop_bwrite_desc, adosfs_bwrite },		/* bwrite */
	{ (struct vnodeop_desc*)NULL, (int(*) __P((void *)))NULL }
};

struct vnodeopv_desc adosfs_vnodeop_opv_desc =
	{ &adosfs_vnodeop_p, adosfs_vnodeop_entries };

int
adosfs_getattr(v)
	void *v;
{
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *sp = v;
	struct vattr *vap;
	struct adosfsmount *amp;
	struct anode *ap;
	u_long fblks;

#ifdef ADOSFS_DIAGNOSTIC
	advopprint(sp);
#endif
	vap = sp->a_vap;
	ap = VTOA(sp->a_vp);
	amp = ap->amp;
	vattr_null(vap);
	vap->va_uid = ap->uid;
	vap->va_gid = ap->gid;
	vap->va_fsid = sp->a_vp->v_mount->mnt_stat.f_fsid.val[0];
	vap->va_atime.tv_sec = vap->va_mtime.tv_sec = vap->va_ctime.tv_sec =
		ap->mtime.days * 24 * 60 * 60 + ap->mtime.mins * 60 +
		ap->mtime.ticks / 50 + (8 * 365 + 2) * 24 * 60 * 60;
	vap->va_atime.tv_nsec = vap->va_mtime.tv_nsec = vap->va_ctime.tv_nsec = 0;
	vap->va_gen = 0;
	vap->va_flags = 0;
	vap->va_rdev = NODEV;
	vap->va_fileid = ap->block;
	vap->va_type = sp->a_vp->v_type;
	vap->va_mode = adunixprot(ap->adprot) & amp->mask;
	if (sp->a_vp->v_type == VDIR) {
		vap->va_nlink = 1;	/* XXX bogus, oh well */
		vap->va_bytes = amp->bsize;
		vap->va_size = amp->bsize;
	} else {
		/* 
		 * XXX actually we can track this if we were to walk the list
		 * of links if it exists.
		 * XXX for now, just set nlink to 2 if this is a hard link
		 * to a file, or a file with a hard link.
		 */
		vap->va_nlink = 1 + (ap->linkto != 0);
		/*
		 * round up to nearest blocks add number of file list 
		 * blocks needed and mutiply by number of bytes per block.
		 */
		fblks = howmany(ap->fsize, amp->dbsize);
		fblks += howmany(fblks, ANODENDATBLKENT(ap));
		vap->va_bytes = fblks * amp->dbsize;
		vap->va_size = ap->fsize;

		vap->va_blocksize = amp->dbsize;
	}
#ifdef ADOSFS_DIAGNOSTIC
	printf(" 0)");
#endif
	return(0);
}
/*
 * are things locked??? they need to be to avoid this being 
 * deleted or changed (data block pointer blocks moving about.)
 */
int
adosfs_read(v)
	void *v;
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *sp = v;
	struct adosfsmount *amp;
	struct anode *ap;
	struct uio *uio;
	struct buf *bp;
	daddr_t lbn;
	int size, diff, error;
	long n, on;

#ifdef ADOSFS_DIAGNOSTIC
	advopprint(sp);
#endif
	error = 0;
	uio = sp->a_uio;
	ap = VTOA(sp->a_vp);
	amp = ap->amp;	
	/*
	 * Return EOF for character devices, EIO for others
	 */
	if (sp->a_vp->v_type != VREG) {
		error = EIO;
		goto reterr;
	}
	if (uio->uio_resid == 0)
		goto reterr;
	if (uio->uio_offset < 0) {
		error = EINVAL;
		goto reterr;
	}

	/*
	 * to expensive to let general algorithm figure out that 
	 * we are beyond the file.  Do it now.
	 */
	if (uio->uio_offset >= ap->fsize)
		goto reterr;

	/*
	 * taken from ufs_read()
	 */
	do {
		/*
		 * we are only supporting ADosFFS currently
		 * (which have data blocks without headers)
		 */
		size = amp->dbsize;
		lbn = uio->uio_offset / size;
		on = uio->uio_offset % size;
		n = min((u_int)(size - on), uio->uio_resid);
		diff = ap->fsize - uio->uio_offset;
		/* 
		 * check for EOF
		 */
		if (diff <= 0)
			return(0);
		if (diff < n)
			n = diff;
		/*
		 * read ahead could possibly be worth something
		 * but not much as ados makes little attempt to 
		 * make things contigous
		 */
		error = bread(sp->a_vp, lbn * amp->secsperblk,
			      amp->bsize, NOCRED, &bp);
		sp->a_vp->v_lastr = lbn;

		if (!IS_FFS(amp)) {
			if (bp->b_resid > 0)
				error = EIO; /* OFS needs the complete block */
			else if (adoswordn(bp, 0) != BPT_DATA) {
#ifdef DIAGNOSTIC
				printf("adosfs: bad primary type blk %ld\n",
				    bp->b_blkno / amp->secsperblk);
#endif
				error=EINVAL;
			}
			else if ( adoscksum(bp, ap->nwords)) {
#ifdef DIAGNOSTIC
				printf("adosfs: blk %ld failed cksum.\n",
				    bp->b_blkno / amp->secsperblk);
#endif
				error=EINVAL;
			}
		}

		if (error) {
			brelse(bp);
			goto reterr;
		}
#ifdef ADOSFS_DIAGNOSTIC
		printf(" %d+%d-%d+%d", lbn, on, lbn, n);
#endif
		n = min(n, (u_int)size - bp->b_resid);
		error = uiomove(bp->b_un.b_addr + on +
				amp->bsize - amp->dbsize, (int)n, uio);
		brelse(bp);
	} while (error == 0 && uio->uio_resid > 0 && n != 0);
reterr:
#ifdef ADOSFS_DIAGNOSTIC
	printf(" %d)", error);
#endif
	return(error);
}

int
adosfs_write(v)
	void *v;
{
#ifdef ADOSFS_DIAGNOSTIC
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *sp = v;
	advopprint(sp);
	printf(" EOPNOTSUPP)");
#endif
	return(EOPNOTSUPP);
}

/*
 * Device ioctl operation.
 */
/* ARGSUSED */
int
adosfs_ioctl(v)
	void *v;
{
#ifdef ADOSFS_DIAGNOSTIC
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		u_long a_command;
		caddr_t  a_data;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *sp = v;
	advopprint(sp);
	printf(" ENOTTY)");
#endif
	return(ENOTTY);
}

/*
 * Just call the device strategy routine
 */
int
adosfs_strategy(v)
	void *v;
{
	struct vop_strategy_args /* {
		struct buf *a_bp;
	} */ *sp = v;
	struct buf *bp;
	struct anode *ap;
	struct vnode *vp;
	int error;

#ifdef ADOSFS_DIAGNOSTIC
	advopprint(sp);
#endif
	error = 0;
	bp = sp->a_bp;
	if (bp->b_vp == NULL) {
		bp->b_flags |= B_ERROR;
		biodone(bp);
		error = EIO;
		goto reterr;
	}
	vp = bp->b_vp;
	ap = VTOA(vp);
	if (bp->b_blkno == bp->b_lblkno) {
		error = VOP_BMAP(vp, bp->b_lblkno, NULL, &bp->b_blkno, NULL);
		if (error) {
			bp->b_flags |= B_ERROR;
			biodone(bp);
			goto reterr;
		}
	}
	if ((long)bp->b_blkno == -1) {
		biodone(bp);
		error = 0;
		goto reterr;
	}
	vp = ap->amp->devvp;
	bp->b_dev = vp->v_rdev;
	VOCALL(vp->v_op, VOFFSET(vop_strategy), sp);
reterr:
#ifdef ADOSFS_DIAGNOSTIC
	printf(" %d)", error);
#endif
	return(error);
}

int
adosfs_link(v) 
	void *v;
{
	struct vop_link_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;  
		struct componentname *a_cnp;
	} */ *ap = v;
 
	VOP_ABORTOP(ap->a_dvp, ap->a_cnp);
	vput(ap->a_dvp);
	return (EROFS);
}

int
adosfs_symlink(v)
	void *v;
{
	struct vop_symlink_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		char *a_target;
	} */ *ap = v;
  
	VOP_ABORTOP(ap->a_dvp, ap->a_cnp);
	vput(ap->a_dvp);
	return (EROFS);
}

/*
 * lock the anode
 */
int
adosfs_lock(v)
	void *v;
{
	struct vop_lock_args /* {
		struct vnode *a_vp;
	} */ *sp = v;
	struct vnode *vp;
	struct anode *ap;

#ifdef ADOSFS_DIAGNOSTIC
	advopprint(sp);
#endif
	vp = sp->a_vp;
start:
	while (vp->v_flag & VXLOCK) {
		vp->v_flag |= VXWANT;
		tsleep(vp, PINOD, "adosfs_lock vp", 0);
	}
	if (vp->v_tag == VT_NON)
		return (ENOENT);
	ap = VTOA(vp);
	if (ap->flags & ALOCKED) {
		ap->flags |= AWANT;
		tsleep(ap, PINOD, "adosfs_lock ap", 0);
		goto start;
	}
	ap->flags |= ALOCKED;
#ifdef ADOSFS_DIAGNOSTIC
	printf(" 0)");
#endif
	return(0);
}

/*
 * unlock an anode
 */
int
adosfs_unlock(v)
	void *v;
{
	struct vop_unlock_args /* {
		struct vnode *a_vp;
	} */ *sp = v;
	struct anode *ap;

#ifdef ADOSFS_DIAGNOSTIC
	advopprint(sp);
#endif
	ap = VTOA(sp->a_vp);	
	ap->flags &= ~ALOCKED;
	if (ap->flags & AWANT) {
		ap->flags &= ~AWANT;
		wakeup(ap);
	}

#ifdef ADOSFS_DIAGNOSTIC
	printf(" 0)");
#endif
	return(0);
}


/*
 * Wait until the vnode has finished changing state.
 */
int
adosfs_bmap(v)
	void *v;
{
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
	} */ *sp = v;
	struct anode *ap;
	struct buf *flbp;
	long nb, flblk, flblkoff, fcnt;
	daddr_t *bnp;
	daddr_t bn;
	int error; 

#ifdef ADOSFS_DIAGNOSTIC
	advopprint(sp);
#endif
	ap = VTOA(sp->a_vp);
	bn = sp->a_bn / ap->amp->secsperblk;
	bnp = sp->a_bnp;
	error = 0;

	if (sp->a_vpp != NULL)
		*sp->a_vpp = ap->amp->devvp;
	if (bnp == NULL)
		goto reterr;
	if (bn < 0) {
		error = EFBIG;
		goto reterr;
	}
	if (sp->a_vp->v_type != VREG) {
		error = EINVAL;
		goto reterr;
	}

	/*
	 * walk the chain of file list blocks until we find
	 * the one that will yield the block pointer we need.
	 */
	if (ap->type == AFILE)
		nb = ap->block;			/* pointer to ourself */
	else if (ap->type == ALFILE)
		nb = ap->linkto;		/* pointer to real file */
	else {
		error = EINVAL;
		goto reterr;
	}

	flblk = bn / ANODENDATBLKENT(ap);
	flbp = NULL;

	/*
	 * check last indirect block cache
	 */
	if (flblk < ap->lastlindblk) 
		fcnt = 0;
	else {
		flblk -= ap->lastlindblk;
		fcnt = ap->lastlindblk;
		nb = ap->lastindblk;
	}
	while (flblk >= 0) {
		if (flbp)
			brelse(flbp);
		if (nb == 0) {
#ifdef DIAGNOSTIC
			printf("adosfs: bad file list chain.\n");
#endif
			error = EINVAL;
			goto reterr;
		}
		error = bread(ap->amp->devvp, nb * ap->amp->secsperblk,
			      ap->amp->bsize, NOCRED, &flbp);
		if (error)
			goto reterr;
		if (adoscksum(flbp, ap->nwords)) {
#ifdef DIAGNOSTIC
			printf("adosfs: blk %ld failed cksum.\n", nb);
#endif
			brelse(flbp);
			error = EINVAL;
			goto reterr;
		}
		/*
		 * update last indirect block cache
		 */
		ap->lastlindblk = fcnt++;
		ap->lastindblk = nb;

		nb = adoswordn(flbp, ap->nwords - 2);
		flblk--;
	}
	/* 
	 * calculate offset of block number in table.  The table starts
	 * at nwords - 51 and goes to offset 6 or less if indicated by the
	 * valid table entries stored at offset ADBI_NBLKTABENT.
	 */
	flblkoff = bn % ANODENDATBLKENT(ap);
	if (flblkoff < adoswordn(flbp, 2 /* ADBI_NBLKTABENT */)) {
		flblkoff = (ap->nwords - 51) - flblkoff;
		*bnp = adoswordn(flbp, flblkoff) * ap->amp->secsperblk;
	} else {
#ifdef DIAGNOSTIC
		printf("flblk offset %ld too large in lblk %ld blk %d\n", 
		    flblkoff, bn / ap->amp->secsperblk , flbp->b_blkno);
#endif
		error = EINVAL;
	}
	brelse(flbp);
reterr:
#ifdef ADOSFS_DIAGNOSTIC
	if (error == 0 && bnp)
		printf(" %d => %d", bn, *bnp);
	printf(" %d)", error);
#endif
	return(error);
}

/*
 * Print out the contents of a adosfs vnode.
 */
/* ARGSUSED */
int
adosfs_print(v)
	void *v;
{
#if 0
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *sp = v;
#endif
	return(0);
}

struct adirent {
	u_long  fileno;
	u_short reclen;
	char    type;
	char    namlen;
	char    name[32];	/* maxlen of 30 plus 2 NUL's */
};
	
int 
adosfs_readdir(v)
	void *v;
{
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		off_t *a_cookies;
		int a_ncookies;
	} */ *sp = v;
	int error, useri, chainc, hashi, scanned, uavail;
	struct adirent ad, *adp;
	struct anode *pap, *ap;
	struct adosfsmount *amp;
	struct vnode *vp;
	struct uio *uio;
	u_long nextbn;
	off_t uoff;

#ifdef ADOSFS_DIAGNOSTIC
	advopprint(sp);
#endif
	if (sp->a_vp->v_type != VDIR) {
		error = ENOTDIR;
		goto reterr;
	}
	uio = sp->a_uio;
	uoff = uio->uio_offset;
	if (uoff < 0) {
		error = EINVAL;
		goto reterr;
	}

	pap = VTOA(sp->a_vp);
	amp = pap->amp;
	adp = &ad;
	error = nextbn = hashi = chainc = scanned = 0;
	uavail = uio->uio_resid / sizeof(ad);
	useri = uoff / sizeof(ad);

	/*
	 * if no slots available or offset requested is not on a slot boundry
	 */
	if (uavail < 1 || uoff % sizeof(ad)) {
		error = EINVAL;
		goto reterr;
	}

	while (uavail && (sp->a_cookies == NULL || sp->a_ncookies > 0)) {
		if (hashi == pap->ntabent) {
			*sp->a_eofflag = 1;
			break;
		}
		if (pap->tab[hashi] == 0) {
			hashi++;
			continue;
		}
		if (nextbn == 0)
			nextbn = pap->tab[hashi];

		/*
		 * first determine if we can skip this chain
		 */
		if (chainc == 0) {
			int skip;

			skip = useri - scanned;
			if (pap->tabi[hashi] > 0 && pap->tabi[hashi] <= skip) {
				scanned += pap->tabi[hashi];
				hashi++;
				nextbn = 0;
				continue;
			}
		}

		/*
		 * now [continue to] walk the chain
		 */
		ap = NULL;
		do {
			error = VFS_VGET(amp->mp, (ino_t)nextbn, &vp);
			if (error)
				goto reterr;
			ap = VTOA(vp);
			scanned++;
			chainc++;
			nextbn = ap->hashf;

			/*
			 * check for end of chain.
			 */
			if (nextbn == 0) {
				pap->tabi[hashi] = chainc;
				hashi++;
				chainc = 0;
			} else if (pap->tabi[hashi] <= 0 &&
			    -chainc < pap->tabi[hashi])
				pap->tabi[hashi] = -chainc;

			if (useri >= scanned) {
				vput(vp);
				ap = NULL;
			}
		} while (ap == NULL && nextbn != 0);

		/*
		 * we left the loop but without a result so do main over.
		 */
		if (ap == NULL)
			continue;
		/*
		 * Fill in dirent record
		 */
		bzero(adp, sizeof(struct adirent));
		adp->fileno = ap->block;
		/*
		 * this deserves an function in kern/vfs_subr.c
		 */
		switch (ATOV(ap)->v_type) {
		case VREG:
			adp->type = DT_REG;
			break;
		case VDIR:
			adp->type = DT_DIR;
			break;
		case VLNK:
			adp->type = DT_LNK;
			break;
		default:
			adp->type = DT_UNKNOWN;
			break;
		}
		adp->reclen = sizeof(struct adirent);
		adp->namlen = strlen(ap->name);
		bcopy(ap->name, adp->name, adp->namlen);
		vput(vp);

		error = uiomove((caddr_t) adp, sizeof(struct adirent), uio);
		if (error)
			break;
		if (sp->a_cookies) {
			*sp->a_cookies++ = uoff;
			sp->a_ncookies--;
		}
		uoff += sizeof(struct adirent);
		useri++;
		uavail--;
	}
#if doesnt_uiomove_handle_this
	uio->uio_offset = uoff;
#endif
reterr:
#ifdef ADOSFS_DIAGNOSTIC
	printf(" %d)", error);
#endif
	return(error);
}


int
adosfs_access(v)
	void *v;
{
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *sp = v;
	struct anode *ap;
	int error;

#ifdef ADOSFS_DIAGNOSTIC
	advopprint(sp);
#endif

	ap = VTOA(sp->a_vp);
#ifdef DIAGNOSTIC
	if (!VOP_ISLOCKED(sp->a_vp)) {
		vprint("adosfs_access: not locked", sp->a_vp);
		panic("adosfs_access: not locked");
	}
#endif
#ifdef QUOTA
#endif
	error = vaccess(sp->a_vp->v_type, adunixprot(ap->adprot) & ap->amp->mask,
	    ap->uid, ap->gid, sp->a_mode, sp->a_cred);
#ifdef ADOSFS_DIAGNOSTIC
	printf(" %d)", error);
#endif
	return(error);
}

/*ARGSUSED*/
int
adosfs_readlink(v)
	void *v;
{
	struct vop_readlink_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
	} */ *sp = v;
	struct anode *ap;
	int error;

#ifdef ADOSFS_DIAGNOSTIC
	advopprint(sp);
#endif
	error = 0;
	ap = VTOA(sp->a_vp);
	if (ap->type != ASLINK)
		error = EBADF;
	/*
	 * XXX Should this be NULL terminated?
	 */
	if (error == 0)
		error = uiomove(ap->slinkto, strlen(ap->slinkto)+1, sp->a_uio);
#ifdef ADOSFS_DIAGNOSTIC
	printf(" %d)", error);
#endif
	return(error);
}

/*ARGSUSED*/
int
adosfs_inactive(v)
	void *v;
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
	} */ *sp = v;
#ifdef ADOSFS_DIAGNOSTIC
	advopprint(sp);
#endif
	if (sp->a_vp->v_usecount == 0 /* && check for file gone? */)
		vgone(sp->a_vp);

#ifdef ADOSFS_DIAGNOSTIC
	printf(" 0)");
#endif
	return(0);
}

int
adosfs_islocked(v)
	void *v;
{
	struct vop_islocked_args /* {
		struct vnode *a_vp;
	} */ *sp = v;
	int locked;

#ifdef ADOSFS_DIAGNOSTIC
	advopprint(sp);
#endif

	locked = (VTOA(sp->a_vp)->flags & ALOCKED) == ALOCKED;

#ifdef ADOSFS_DIAGNOSTIC
	printf(" %d)", locked);
#endif
	return(locked);
}

/*
 * the kernel wants its vnode back.
 * no lock needed we are being called from vclean()
 */
int
adosfs_reclaim(v)
	void *v;
{
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *sp = v;
	struct vnode *vp;
	struct anode *ap;

#ifdef ADOSFS_DIAGNOSTIC
	printf("(reclaim 0)");
#endif
	vp = sp->a_vp;
	ap = VTOA(vp);
	LIST_REMOVE(ap, link);
	cache_purge(vp);
	if (vp->v_type == VDIR && ap->tab)
		free(ap->tab, M_ANODE);
	else if (vp->v_type == VLNK && ap->slinkto)
		free(ap->slinkto, M_ANODE);
	free(ap, M_ANODE);
	vp->v_data = NULL;
	return(0);
}


/*
 * POSIX pathconf info, grabbed from kern/u fs, probably need to 
 * investigate exactly what each return type means as they are probably
 * not valid currently
 */
int
adosfs_pathconf(v)
	void *v;
{
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		register_t *a_retval;
	} */ *sp = v;

	switch (sp->a_name) {
	case _PC_LINK_MAX:
		*sp->a_retval = LINK_MAX;
		return (0);
	case _PC_MAX_CANON:
		*sp->a_retval = MAX_CANON;
		return (0);
	case _PC_MAX_INPUT:
		*sp->a_retval = MAX_INPUT;
		return (0);
	case _PC_PIPE_BUF:
		*sp->a_retval = PIPE_BUF;
		return (0);
	case _PC_CHOWN_RESTRICTED:
		*sp->a_retval = 1;
		return (0);
	case _PC_VDISABLE:
		*sp->a_retval = _POSIX_VDISABLE;
		return (0);
	default:
		return (EINVAL);
	}
	/* NOTREACHED */
}
