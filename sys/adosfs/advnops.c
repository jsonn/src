/*	$NetBSD: advnops.c,v 1.11.2.2 1994/10/06 18:41:27 chopps Exp $	*/

/*
 * Copyright (c) 1994 Christian E. Hopps
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
#include <miscfs/specfs/specdev.h>
#include <adosfs/adosfs.h>

extern struct vnodeops adosfs_vnodeops;

int
adosfs_open(sp)
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *sp;
{
#ifdef ADOSFS_DIAGNOSTIC
	advopprint(sp);
	printf(" 0)");
#endif
	return(0);
}

int
adosfs_getattr(sp)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *sp;
{
	struct vattr *vap;
	struct amount *amp;
	struct anode *ap;
	u_long fblks;

#ifdef ADOSFS_DIAGNOSTIC
	advopprint(sp);
#endif
	vap = sp->a_vap;
	ap = VTOA(sp->a_vp);
	amp = ap->amp;
	vattr_null(vap);
	vap->va_uid = amp->uid;
	vap->va_gid = amp->gid;
	vap->va_fsid = sp->a_vp->v_mount->mnt_stat.f_fsid.val[0];
	microtime(&vap->va_atime);
	vap->va_mtime = vap->va_atime;
	vap->va_ctime = vap->va_ctime;
	vap->va_gen = 0;
	vap->va_flags = 0;
	vap->va_rdev = NODEV;
	vap->va_fileid = ap->block;
	vap->va_type = sp->a_vp->v_type;
	vap->va_mode = amp->mask & adunixprot(ap->adprot);
	if (sp->a_vp->v_type == VDIR) {
		vap->va_nlink = 1;	/* XXX bogus, oh well */
		vap->va_bytes = amp->bsize;
		vap->va_size = amp->bsize;
	} else {
		/* 
		 * XXX actually we can track this if we were to walk the list
		 * of links if it exists.
		 */
		vap->va_nlink = 1;
		/*
		 * round up to nearest blocks add number of file list 
		 * blocks needed and mutiply by number of bytes per block.
		 */
		fblks = howmany(ap->fsize, amp->bsize);
		fblks += howmany(fblks, ANODENDATBLKENT(ap));
		vap->va_bytes = fblks * amp->bsize;
		vap->va_size = ap->fsize;
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
adosfs_read(sp)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *sp;
{
	struct amount *amp;
	struct anode *ap;
	struct uio *uio;
	struct buf *bp;
	struct fs *fs;
	daddr_t lbn, bn;
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
		 * (which have data blocks of 512 bytes)
		 */
		size = amp->bsize;
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
		error = bread(sp->a_vp, lbn, size, NOCRED, &bp);
		sp->a_vp->v_lastr = lbn;
		n = min(n, (u_int)size - bp->b_resid);
		if (error) {
			brelse(bp);
			goto reterr;
		}
#ifdef ADOSFS_DIAGNOSTIC
	printf(" %d+%d-%d+%d", lbn, on, lbn, n);
#endif
		error = uiomove(bp->b_un.b_addr + on, (int)n, uio);
		brelse(bp);
	} while (error == 0 && uio->uio_resid > 0 && n != 0);
reterr:
#ifdef ADOSFS_DIAGNOSTIC
	printf(" %d)", error);
#endif
	return(error);
}

int
adosfs_write(sp)
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *sp;
{
#ifdef ADOSFS_DIAGNOSTIC
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
adosfs_ioctl(sp)
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		int  a_command;
		caddr_t  a_data;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *sp;
{
#ifdef ADOSFS_DIAGNOSTIC
	advopprint(sp);
	printf(" ENOTTY)");
#endif
	return(ENOTTY);
}

/* ARGSUSED */
int
adosfs_select(sp)
	struct vop_select_args /* {
		struct vnode *a_vp;
		int  a_which;
		int  a_fflags;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *sp;
{
	/*
	 * sure there's something to read...
	 */
#ifdef ADOSFS_DIAGNOSTIC
	advopprint(sp);
	printf(" 1)");
#endif
	return(1);
}

/*
 * Just call the device strategy routine
 */
int
adosfs_strategy(sp)
	struct vop_strategy_args /* {
		struct buf *a_bp;
	} */ *sp;
{
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
		if (error = 
		    VOP_BMAP(vp, bp->b_lblkno, NULL, &bp->b_blkno, NULL)) {
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

/*
 * lock the anode
 */
int
adosfs_lock(sp)
	struct vop_lock_args /* {
		struct vnode *a_vp;
	} */ *sp;
{
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
adosfs_unlock(sp)
	struct vop_unlock_args /* {
		struct vnode *a_vp;
	} */ *sp;
{
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
adosfs_bmap(sp)
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
	} */ *sp;
{
	struct anode *ap;
	struct buf *flbp;
	long nb, flblk, flblkoff, fcnt;
	daddr_t *bnp;
	daddr_t bn;
	int error; 

#ifdef ADOSFS_DIAGNOSTIC
	advopprint(sp);
#endif
	bn = sp->a_bn;
	bnp = sp->a_bnp;
	error = 0;
	ap = VTOA(sp->a_vp);

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
	if (flblk > ap->lastlindblk) 
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
		if (error = bread(ap->amp->devvp, nb, ap->amp->bsize, 
		    NOCRED, &flbp))
			goto reterr;
		if (adoscksum(flbp, ap->nwords)) {
#ifdef DIAGNOSTIC
			printf("adosfs: blk %d failed cksum.\n", nb);
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
		*bnp = adoswordn(flbp, flblkoff);
	} else {
#ifdef DIAGNOSTIC
		printf("flblk offset %d too large in lblk %d blk %d\n", 
		    flblkoff, bn, flbp->b_blkno);
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
adosfs_print(sp)
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *sp;
{
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
adosfs_readdir(sp)
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		u_long *a_cookies;
		int a_ncookies;
	} */ *sp;
{
	int error, useri, chainc, hashi, scanned, uavail;
	struct adirent ad, *adp;
	struct anode *pap, *ap;
	struct amount *amp;
	struct vnode *vp;
	struct uio *uio;
	u_long nextbn, resid;
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
			if (error = VFS_VGET(amp->mp, (ino_t)nextbn, &vp))
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

		error = uiomove(adp, sizeof(struct adirent), uio);
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
adosfs_access(sp)
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *sp;
{
	struct anode *ap;
	struct ucred *ucp;
	gid_t *gp;
	mode_t mode, mask;
	int i, error;

#ifdef ADOSFS_DIAGNOSTIC
	advopprint(sp);
#endif

	mask = error = 0;
	ucp = sp->a_cred;
	mode = sp->a_mode;
	ap = VTOA(sp->a_vp);
#ifdef DIAGNOSTIC
	if (!VOP_ISLOCKED(sp->a_vp)) {
		vprint("adosfs_access: not locked", sp->a_vp);
		panic("adosfs_access: not locked");
	}
#endif
#ifdef QUOTA
#endif
	/*
	 * super-user always gets a go ahead (suser()?)
	 */
	if (suser(ucp, NULL) == 0)
		return(0);

	/*
	 * check owner
	 */
	if (ucp->cr_uid == ap->amp->uid)  {
		if (mode & VEXEC)
			mask |= S_IXUSR;
		if (mode & VREAD)
			mask |= S_IRUSR;
		if (mode & VWRITE)
			mask |= S_IWUSR;
		goto found;
	}

	/*
	 * check groups
	 */
	for (i = 0, gp = ucp->cr_groups; i < ucp->cr_ngroups; i++, gp++) {
		if (ap->amp->gid != *gp)
			continue;
		if (mode & VEXEC)
			mask |= S_IXGRP;
		if (mode & VREAD)
			mask |= S_IRGRP;
		if (mode & VWRITE)
			mask |= S_IWGRP;
		goto found;
	}

	/*
	 * check other
	 */
	if (mode & VEXEC)
		mask |= S_IXOTH;
	if (mode & VREAD)
		mask |= S_IROTH;
	if (mode & VWRITE)
		mask |= S_IWOTH;
found:
	if ((adunixprot(ap->adprot) & ap->amp->mask & mask) != mask)
		error = EACCES;
#ifdef ADOSFS_DIAGNOSTIC
	printf(" %d)", error);
#endif
	return(error);
}

/*ARGSUSED*/
int
adosfs_readlink(sp)
	struct vop_readlink_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
	} */ *sp;
{
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
adosfs_inactive(sp)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
	} */ *sp;
{
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
adosfs_islocked(sp)
	struct vop_islocked_args /* {
		struct vnode *a_vp;
	} */ *sp;
{
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
adosfs_reclaim(sp)
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *sp;
{
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
adosfs_pathconf(sp)
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		int *a_retval;
	} */ *sp;
{

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

int
adenotsup(sp)
	void *sp;
{
#ifdef ADOSFS_DIAGNOSTIC
	advopprint(sp);
	printf(" EOPNOTSUPP)");
#endif
	return(EOPNOTSUPP);
}

int
adnullop(sp)
	void *sp;
{
#ifdef ADOSFS_DIAGNOSTIC
	advopprint(sp);
	printf(" NULL)");
#endif
	return(0);
}

#define adosfs_close ((int (*) __P((struct  vop_close_args *)))adnullop)
#define adosfs_fsync ((int (*) __P((struct  vop_fsync_args *)))adnullop)
#define adosfs_seek ((int (*) __P((struct  vop_seek_args *)))adnullop)
#define adosfs_vfree ((int (*) __P((struct vop_vfree_args *)))adnullop)

#define adosfs_abortop ((int (*) __P((struct  vop_abortop_args *)))adenotsup)
#define adosfs_advlock ((int (*) __P((struct vop_advlock_args *)))adenotsup)
#define adosfs_blkatoff ((int (*) __P((struct vop_blkatoff_args *)))adenotsup)
#define adosfs_bwrite ((int (*) __P((struct vop_bwrite_args *)))adenotsup)
#define adosfs_create ((int (*) __P((struct vop_create_args *)))adenotsup)
#define adosfs_link ((int (*) __P((struct vop_link_args *)))adenotsup)
#define adosfs_mkdir ((int (*) __P((struct vop_mkdir_args *)))adenotsup)
#define adosfs_mknod ((int (*) __P((struct vop_mknod_args *)))adenotsup)
#define adosfs_mmap ((int (*) __P((struct vop_mmap_args *)))adenotsup)
#define adosfs_remove ((int (*) __P((struct vop_remove_args *)))adenotsup)
#define adosfs_rename ((int (*) __P((struct vop_rename_args *)))adenotsup)
#define adosfs_rmdir ((int (*) __P((struct vop_rmdir_args *)))adenotsup)
#define adosfs_setattr ((int (*) __P((struct vop_setattr_args *)))adenotsup)
#define adosfs_symlink ((int (*) __P((struct vop_symlink_args *)))adenotsup)
#define adosfs_truncate ((int (*) __P((struct vop_truncate_args *)))adenotsup)
#define adosfs_update ((int (*) __P((struct vop_update_args *)))adenotsup)
#define adosfs_valloc ((int (*) __P((struct vop_valloc_args *)))adenotsup)

struct vnodeopv_entry_desc adosfs_vnodeop_entries[] = {
	{ &vop_default_desc,	vn_default_error },
	{ &vop_lookup_desc,	adosfs_lookup },	/* lookup */
	{ &vop_create_desc,	adosfs_create },	/* create */
	{ &vop_mknod_desc,	adosfs_mknod },		/* mknod */
	{ &vop_open_desc,	adosfs_open },		/* open */
	{ &vop_close_desc,	adosfs_close },		/* close */
	{ &vop_access_desc,	adosfs_access },	/* access */
	{ &vop_getattr_desc,	adosfs_getattr },	/* getattr */
	{ &vop_setattr_desc,	adosfs_setattr },	/* setattr */
	{ &vop_read_desc,	adosfs_read },		/* read */
	{ &vop_write_desc,	adosfs_write },		/* write */
	{ &vop_ioctl_desc,	adosfs_ioctl },		/* ioctl */
	{ &vop_select_desc,	adosfs_select },	/* select */
	{ &vop_mmap_desc,	adosfs_mmap },		/* mmap */
	{ &vop_fsync_desc,	adosfs_fsync },		/* fsync */
	{ &vop_seek_desc,	adosfs_seek },		/* seek */
	{ &vop_remove_desc,	adosfs_remove },	/* remove */
	{ &vop_link_desc,	adosfs_link },		/* link */
	{ &vop_rename_desc,	adosfs_rename },	/* rename */
	{ &vop_mkdir_desc,	adosfs_mkdir },		/* mkdir */
	{ &vop_rmdir_desc,	adosfs_rmdir },		/* rmdir */
	{ &vop_symlink_desc,	adosfs_symlink },	/* symlink */
	{ &vop_readdir_desc,	adosfs_readdir },	/* readdir */
	{ &vop_readlink_desc,	adosfs_readlink },	/* readlink */
	{ &vop_abortop_desc,	adosfs_abortop },	/* abortop */
	{ &vop_inactive_desc,	adosfs_inactive },	/* inactive */
	{ &vop_reclaim_desc,	adosfs_reclaim },	/* reclaim */
	{ &vop_lock_desc,	adosfs_lock },		/* lock */
	{ &vop_unlock_desc,	adosfs_unlock },	/* unlock */
	{ &vop_bmap_desc,	adosfs_bmap },		/* bmap */
	{ &vop_strategy_desc,	adosfs_strategy },	/* strategy */
	{ &vop_print_desc,	adosfs_print },		/* print */
	{ &vop_islocked_desc,	adosfs_islocked },	/* islocked */
	{ &vop_pathconf_desc,	adosfs_pathconf },	/* pathconf */
	{ &vop_advlock_desc,	adosfs_advlock },	/* advlock */
	{ &vop_blkatoff_desc,	adosfs_blkatoff },	/* blkatoff */
	{ &vop_valloc_desc,	adosfs_valloc },	/* valloc */
	{ &vop_vfree_desc,	adosfs_vfree },		/* vfree */
	{ &vop_truncate_desc,	adosfs_truncate },	/* truncate */
	{ &vop_update_desc,	adosfs_update },	/* update */
	{ &vop_bwrite_desc,	adosfs_bwrite },	/* bwrite */
	{ (struct vnodeop_desc*)NULL, (int(*)())NULL }
};
struct vnodeopv_desc adosfs_vnodeop_opv_desc =
	{ &adosfs_vnodeop_p, adosfs_vnodeop_entries };
