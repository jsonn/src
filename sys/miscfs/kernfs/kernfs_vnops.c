/*	$NetBSD: kernfs_vnops.c,v 1.75.2.5 2002/08/13 02:20:09 nathanw Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)kernfs_vnops.c	8.15 (Berkeley) 5/21/95
 */

/*
 * Kernel parameter filesystem (/kern)
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kernfs_vnops.c,v 1.75.2.5 2002/08/13 02:20:09 nathanw Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/vmmeter.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/buf.h>
#include <sys/dirent.h>
#include <sys/msgbuf.h>

#include <miscfs/genfs/genfs.h>
#include <miscfs/kernfs/kernfs.h>

#include <uvm/uvm_extern.h>

#define KSTRING	256		/* Largest I/O available via this filesystem */
#define	UIO_MX 32

#define	READ_MODE	(S_IRUSR|S_IRGRP|S_IROTH)
#define	WRITE_MODE	(S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH)
#define DIR_MODE	(S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)

const struct kern_target kern_targets[] = {
/* NOTE: The name must be less than UIO_MX-16 chars in length */
#define N(s) sizeof(s)-1, s
     /*        name            data          tag           type  ro/rw */
     { DT_DIR, N("."),         0,            KTT_NULL,     VDIR, DIR_MODE   },
     { DT_DIR, N(".."),        0,            KTT_NULL,     VDIR, DIR_MODE   },
     { DT_REG, N("boottime"),  &boottime.tv_sec, KTT_INT,  VREG, READ_MODE  },
			/* XXX cast away const */
     { DT_REG, N("copyright"), (void *)copyright,
     					     KTT_STRING,   VREG, READ_MODE  },
     { DT_REG, N("hostname"),  0,            KTT_HOSTNAME, VREG, WRITE_MODE },
     { DT_REG, N("hz"),        &hz,          KTT_INT,      VREG, READ_MODE  },
     { DT_REG, N("loadavg"),   0,            KTT_AVENRUN,  VREG, READ_MODE  },
     { DT_REG, N("msgbuf"),    0,	     KTT_MSGBUF,   VREG, READ_MODE  },
     { DT_REG, N("pagesize"),  &uvmexp.pagesize, KTT_INT,  VREG, READ_MODE  },
     { DT_REG, N("physmem"),   &physmem,     KTT_INT,      VREG, READ_MODE  },
#if 0
     { DT_DIR, N("root"),      0,            KTT_NULL,     VDIR, DIR_MODE   },
#endif
     { DT_BLK, N("rootdev"),   &rootdev,     KTT_DEVICE,   VBLK, READ_MODE  },
     { DT_CHR, N("rrootdev"),  &rrootdev,    KTT_DEVICE,   VCHR, READ_MODE  },
     { DT_REG, N("time"),      0,            KTT_TIME,     VREG, READ_MODE  },
			/* XXX cast away const */
     { DT_REG, N("version"),   (void *)version,
     					     KTT_STRING,   VREG, READ_MODE  },
#undef N
};
static int nkern_targets = sizeof(kern_targets) / sizeof(kern_targets[0]);

int	kernfs_lookup	__P((void *));
#define	kernfs_create	genfs_eopnotsupp_rele
#define	kernfs_mknod	genfs_eopnotsupp_rele
#define	kernfs_open	genfs_nullop
#define	kernfs_close	genfs_nullop
int	kernfs_access	__P((void *));
int	kernfs_getattr	__P((void *));
int	kernfs_setattr	__P((void *));
int	kernfs_read	__P((void *));
int	kernfs_write	__P((void *));
#define	kernfs_fcntl	genfs_fcntl
#define	kernfs_ioctl	genfs_enoioctl
#define	kernfs_poll	genfs_poll
#define kernfs_revoke	genfs_revoke
#define	kernfs_fsync	genfs_nullop
#define	kernfs_seek	genfs_nullop
#define	kernfs_remove	genfs_eopnotsupp_rele
int	kernfs_link	__P((void *));
#define	kernfs_rename	genfs_eopnotsupp_rele
#define	kernfs_mkdir	genfs_eopnotsupp_rele
#define	kernfs_rmdir	genfs_eopnotsupp_rele
int	kernfs_symlink	__P((void *));
int	kernfs_readdir	__P((void *));
#define	kernfs_readlink	genfs_eopnotsupp
#define	kernfs_abortop	genfs_abortop
int	kernfs_inactive	__P((void *));
int	kernfs_reclaim	__P((void *));
#define	kernfs_lock	genfs_lock
#define	kernfs_unlock	genfs_unlock
#define	kernfs_bmap	genfs_badop
#define	kernfs_strategy	genfs_badop
int	kernfs_print	__P((void *));
#define	kernfs_islocked	genfs_islocked
int	kernfs_pathconf	__P((void *));
#define	kernfs_advlock	genfs_einval
#define	kernfs_blkatoff	genfs_eopnotsupp
#define	kernfs_valloc	genfs_eopnotsupp
#define	kernfs_vfree	genfs_nullop
#define	kernfs_truncate	genfs_eopnotsupp
#define	kernfs_update	genfs_nullop
#define	kernfs_bwrite	genfs_eopnotsupp
#define	kernfs_putpages	genfs_putpages

static int	kernfs_xread __P((const struct kern_target *, int, char **, size_t, size_t *));
static int	kernfs_xwrite __P((const struct kern_target *, char *, size_t));

int (**kernfs_vnodeop_p) __P((void *));
const struct vnodeopv_entry_desc kernfs_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, kernfs_lookup },		/* lookup */
	{ &vop_create_desc, kernfs_create },		/* create */
	{ &vop_mknod_desc, kernfs_mknod },		/* mknod */
	{ &vop_open_desc, kernfs_open },		/* open */
	{ &vop_close_desc, kernfs_close },		/* close */
	{ &vop_access_desc, kernfs_access },		/* access */
	{ &vop_getattr_desc, kernfs_getattr },		/* getattr */
	{ &vop_setattr_desc, kernfs_setattr },		/* setattr */
	{ &vop_read_desc, kernfs_read },		/* read */
	{ &vop_write_desc, kernfs_write },		/* write */
	{ &vop_fcntl_desc, kernfs_fcntl },		/* fcntl */
	{ &vop_ioctl_desc, kernfs_ioctl },		/* ioctl */
	{ &vop_poll_desc, kernfs_poll },		/* poll */
	{ &vop_revoke_desc, kernfs_revoke },		/* revoke */
	{ &vop_fsync_desc, kernfs_fsync },		/* fsync */
	{ &vop_seek_desc, kernfs_seek },		/* seek */
	{ &vop_remove_desc, kernfs_remove },		/* remove */
	{ &vop_link_desc, kernfs_link },		/* link */
	{ &vop_rename_desc, kernfs_rename },		/* rename */
	{ &vop_mkdir_desc, kernfs_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, kernfs_rmdir },		/* rmdir */
	{ &vop_symlink_desc, kernfs_symlink },		/* symlink */
	{ &vop_readdir_desc, kernfs_readdir },		/* readdir */
	{ &vop_readlink_desc, kernfs_readlink },	/* readlink */
	{ &vop_abortop_desc, kernfs_abortop },		/* abortop */
	{ &vop_inactive_desc, kernfs_inactive },	/* inactive */
	{ &vop_reclaim_desc, kernfs_reclaim },		/* reclaim */
	{ &vop_lock_desc, kernfs_lock },		/* lock */
	{ &vop_unlock_desc, kernfs_unlock },		/* unlock */
	{ &vop_bmap_desc, kernfs_bmap },		/* bmap */
	{ &vop_strategy_desc, kernfs_strategy },	/* strategy */
	{ &vop_print_desc, kernfs_print },		/* print */
	{ &vop_islocked_desc, kernfs_islocked },	/* islocked */
	{ &vop_pathconf_desc, kernfs_pathconf },	/* pathconf */
	{ &vop_advlock_desc, kernfs_advlock },		/* advlock */
	{ &vop_blkatoff_desc, kernfs_blkatoff },	/* blkatoff */
	{ &vop_valloc_desc, kernfs_valloc },		/* valloc */
	{ &vop_vfree_desc, kernfs_vfree },		/* vfree */
	{ &vop_truncate_desc, kernfs_truncate },	/* truncate */
	{ &vop_update_desc, kernfs_update },		/* update */
	{ &vop_bwrite_desc, kernfs_bwrite },		/* bwrite */
	{ &vop_putpages_desc, kernfs_putpages },	/* putpages */
	{ NULL, NULL }
};
const struct vnodeopv_desc kernfs_vnodeop_opv_desc =
	{ &kernfs_vnodeop_p, kernfs_vnodeop_entries };

static int
kernfs_xread(kt, off, bufp, len, wrlen)
	const struct kern_target *kt;
	int off;
	char **bufp;
	size_t len;
	size_t *wrlen;
{

	switch (kt->kt_tag) {
	case KTT_TIME: {
		struct timeval tv;

		microtime(&tv);
		sprintf(*bufp, "%ld %ld\n", tv.tv_sec, tv.tv_usec);
		break;
	}

	case KTT_INT: {
		int *ip = kt->kt_data;

		sprintf(*bufp, "%d\n", *ip);
		break;
	}

	case KTT_STRING: {
		char *cp = kt->kt_data;

		*bufp = cp;
		break;
	}

	case KTT_MSGBUF: {
		long n;

		/*
		 * deal with cases where the message buffer has
		 * become corrupted.
		 */
		if (!msgbufenabled || msgbufp->msg_magic != MSG_MAGIC) {
			msgbufenabled = 0;
			return (ENXIO);
		}

		/*
		 * Note that reads of /kern/msgbuf won't necessarily yield
		 * consistent results, if the message buffer is modified
		 * while the read is in progress.  The worst that can happen
		 * is that incorrect data will be read.  There's no way
		 * that this can crash the system unless the values in the
		 * message buffer header are corrupted, but that'll cause
		 * the system to die anyway.
		 */
		if (off >= msgbufp->msg_bufs) {
			*wrlen = 0;
			return (0);
		}
		n = msgbufp->msg_bufx + off;
		if (n >= msgbufp->msg_bufs)
			n -= msgbufp->msg_bufs;
		len = min(msgbufp->msg_bufs - n, msgbufp->msg_bufs - off);
		*bufp = msgbufp->msg_bufc + n;
		*wrlen = len;
		return (0);
	}

	case KTT_HOSTNAME: {
		char *cp = hostname;
		int xlen = hostnamelen;

		if (xlen >= (len-2))
			return (EINVAL);

		memcpy(*bufp, cp, xlen);
		(*bufp)[xlen] = '\n';
		(*bufp)[xlen+1] = '\0';
		break;
	}

	case KTT_AVENRUN:
		averunnable.fscale = FSCALE;
		sprintf(*bufp, "%d %d %d %ld\n",
		    averunnable.ldavg[0], averunnable.ldavg[1],
		    averunnable.ldavg[2], averunnable.fscale);
		break;

	default:
		*wrlen = 0;
		return (0);
	}

	len = strlen(*bufp);
	if (len <= off)
		*wrlen = 0;
	else {
		*bufp += off;
		*wrlen = len - off;
	}
	return (0);
}

static int
kernfs_xwrite(kt, buf, len)
	const struct kern_target *kt;
	char *buf;
	size_t len;
{

	switch (kt->kt_tag) {
	case KTT_HOSTNAME:
		if (buf[len-1] == '\n')
			--len;
		memcpy(hostname, buf, len);
		hostname[len] = '\0';
		hostnamelen = (size_t) len;
		return (0);

	default:
		return (EIO);
	}
}


/*
 * vp is the current namei directory
 * ndp is the name to locate in that directory...
 */
int
kernfs_lookup(v)
	void *v;
{
	struct vop_lookup_args /* {
		struct vnode * a_dvp;
		struct vnode ** a_vpp;
		struct componentname * a_cnp;
	} */ *ap = v;
	struct componentname *cnp = ap->a_cnp;
	struct vnode **vpp = ap->a_vpp;
	struct vnode *dvp = ap->a_dvp;
	const char *pname = cnp->cn_nameptr;
	const struct kern_target *kt;
	struct vnode *fvp;
	int error, i, wantpunlock;

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_lookup(%p)\n", ap);
	printf("kernfs_lookup(dp = %p, vpp = %p, cnp = %p)\n", dvp, vpp, ap->a_cnp);
	printf("kernfs_lookup(%s)\n", pname);
#endif

	*vpp = NULLVP;
	cnp->cn_flags &= ~PDIRUNLOCK;

	if (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME)
		return (EROFS);

	if (cnp->cn_namelen == 1 && *pname == '.') {
		*vpp = dvp;
		VREF(dvp);
		return (0);
	}

	/*
	 * This code only supports a flat directory, so we don't
	 * need to worry about ..
	 */

#if 0
	if (cnp->cn_namelen == 4 && memcmp(pname, "root", 4) == 0) {
		*vpp = rootdir;
		VREF(rootdir);
		vn_lock(rootdir, LK_SHARED | LK_RETRY);
		return (0);
	}
#endif

	wantpunlock = (~cnp->cn_flags & (LOCKPARENT | ISLASTCN));

	for (kt = kern_targets, i = 0; i < nkern_targets; kt++, i++) {
		if (cnp->cn_namelen == kt->kt_namlen &&
		    memcmp(kt->kt_name, pname, cnp->cn_namelen) == 0)
			goto found;
	}

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_lookup: i = %d, failed", i);
#endif

	return (cnp->cn_nameiop == LOOKUP ? ENOENT : EROFS);

found:
	if (kt->kt_tag == KTT_DEVICE) {
		dev_t *dp = kt->kt_data;
	loop:
		if (*dp == NODEV || !vfinddev(*dp, kt->kt_vtype, &fvp)) {
			return (ENOENT);
		}
		*vpp = fvp;
		if (vget(fvp, LK_EXCLUSIVE))
			goto loop;
		if (wantpunlock) {
			VOP_UNLOCK(dvp, 0);
			cnp->cn_flags |= PDIRUNLOCK;
		}
		return (0);
	}

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_lookup: allocate new vnode\n");
#endif
	error = getnewvnode(VT_KERNFS, dvp->v_mount, kernfs_vnodeop_p, &fvp);
	if (error) {
		return (error);
	}

	MALLOC(fvp->v_data, void *, sizeof(struct kernfs_node), M_TEMP,
	    M_WAITOK);
	VTOKERN(fvp)->kf_kt = kt;
	fvp->v_type = kt->kt_vtype;
	vn_lock(fvp, LK_EXCLUSIVE | LK_RETRY);
	*vpp = fvp;

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_lookup: newvp = %p\n", fvp);
#endif
	if (wantpunlock) {
		VOP_UNLOCK(dvp, 0);
		cnp->cn_flags |= PDIRUNLOCK;
	}
	return (0);
}

int
kernfs_access(v)
	void *v;
{
	struct vop_access_args /* {
		struct vnode *a_vp;
		int a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	mode_t mode;

	if (vp->v_flag & VROOT) {
		mode = DIR_MODE;
	} else {
		const struct kern_target *kt = VTOKERN(vp)->kf_kt;
		mode = kt->kt_mode;
	}

	return (vaccess(vp->v_type, mode, (uid_t)0, (gid_t)0, ap->a_mode,
	    ap->a_cred));
}

int
kernfs_getattr(v)
	void *v;
{
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct vattr *vap = ap->a_vap;
	int error = 0;
	char strbuf[KSTRING], *buf;

	memset((caddr_t) vap, 0, sizeof(*vap));
	vattr_null(vap);
	vap->va_uid = 0;
	vap->va_gid = 0;
	vap->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
	vap->va_size = 0;
	vap->va_blocksize = DEV_BSIZE;
	/*
	 * Make all times be current TOD.  Avoid microtime(9), it's slow.
	 * We don't guard the read from time(9) with splclock(9) since we
	 * don't actually need to be THAT sure the access is atomic. 
	 */
	TIMEVAL_TO_TIMESPEC(&time, &vap->va_ctime);
	vap->va_atime = vap->va_mtime = vap->va_ctime;
	vap->va_gen = 0;
	vap->va_flags = 0;
	vap->va_rdev = 0;
	vap->va_bytes = 0;

	if (vp->v_flag & VROOT) {
#ifdef KERNFS_DIAGNOSTIC
		printf("kernfs_getattr: stat rootdir\n");
#endif
		vap->va_type = VDIR;
		vap->va_mode = DIR_MODE;
		vap->va_nlink = 2;
		vap->va_fileid = 2;
		vap->va_size = DEV_BSIZE;
	} else {
		const struct kern_target *kt = VTOKERN(vp)->kf_kt;
		size_t total;
#ifdef KERNFS_DIAGNOSTIC
		printf("kernfs_getattr: stat target %s\n", kt->kt_name);
#endif
		vap->va_type = kt->kt_vtype;
		vap->va_mode = kt->kt_mode;
		vap->va_nlink = 1;
		vap->va_fileid = 1 + (kt - kern_targets);
		buf = strbuf;
		if (0 == (error = kernfs_xread(kt, 0, &buf,
				sizeof(strbuf), &total)))
			vap->va_size = total;
	}

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_getattr: return error %d\n", error);
#endif
	return (error);
}

/*ARGSUSED*/
int
kernfs_setattr(v)
	void *v;
{
	/*
	 * Silently ignore attribute changes.
	 * This allows for open with truncate to have no
	 * effect until some data is written.  I want to
	 * do it this way because all writes are atomic.
	 */
	return (0);
}

int
kernfs_read(v)
	void *v;
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	const struct kern_target *kt;
	char strbuf[KSTRING], *buf;
	off_t off;
	size_t len;
	int error;

	if (vp->v_type == VDIR)
		return (EOPNOTSUPP);

	kt = VTOKERN(vp)->kf_kt;

#ifdef KERNFS_DIAGNOSTIC
	printf("kern_read %s\n", kt->kt_name);
#endif

	off = uio->uio_offset;
	buf = strbuf;
	if ((error = kernfs_xread(kt, off, &buf, sizeof(strbuf), &len)) == 0)
		error = uiomove(buf, len, uio);
	return (error);
}

int
kernfs_write(v)
	void *v;
{
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	const struct kern_target *kt;
	int error, xlen;
	char strbuf[KSTRING];

	if (vp->v_type == VDIR)
		return (EOPNOTSUPP);

	kt = VTOKERN(vp)->kf_kt;

	if (uio->uio_offset != 0)
		return (EINVAL);

	xlen = min(uio->uio_resid, KSTRING-1);
	if ((error = uiomove(strbuf, xlen, uio)) != 0)
		return (error);

	if (uio->uio_resid != 0)
		return (EIO);

	strbuf[xlen] = '\0';
	xlen = strlen(strbuf);
	return (kernfs_xwrite(kt, strbuf, xlen));
}

int
kernfs_readdir(v)
	void *v;
{
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		off_t **a_cookies;
		int a_*ncookies;
	} */ *ap = v;
	struct uio *uio = ap->a_uio;
	struct dirent d;
	const struct kern_target *kt;
	off_t i;
	int error;
	off_t *cookies = NULL;
	int ncookies = 0, nc = 0;

	if (ap->a_vp->v_type != VDIR)
		return (ENOTDIR);

	if (uio->uio_resid < UIO_MX)
		return (EINVAL);
	if (uio->uio_offset < 0)
		return (EINVAL);

	error = 0;
	i = uio->uio_offset;

	if (i >= nkern_targets)
		return 0;
	  
	memset((caddr_t)&d, 0, UIO_MX);
	d.d_reclen = UIO_MX;

	if (ap->a_ncookies) {
		nc = uio->uio_resid / UIO_MX;
		nc = min(nc, (nkern_targets - i));
		cookies = malloc(nc * sizeof(off_t), M_TEMP, M_WAITOK);
		*ap->a_cookies = cookies;
	}

	for (kt = &kern_targets[i];
	     uio->uio_resid >= UIO_MX && i < nkern_targets; kt++, i++) {
#ifdef KERNFS_DIAGNOSTIC
		printf("kernfs_readdir: i = %d\n", (int)i);
#endif

		if (kt->kt_tag == KTT_DEVICE) {
			dev_t *dp = kt->kt_data;
			struct vnode *fvp;

			if (*dp == NODEV || !vfinddev(*dp, kt->kt_vtype, &fvp))
				continue;
		}

		d.d_fileno = i + 3;
		d.d_namlen = kt->kt_namlen;
		memcpy(d.d_name, kt->kt_name, kt->kt_namlen + 1);
		d.d_type = kt->kt_type;

		if ((error = uiomove((caddr_t)&d, UIO_MX, uio)) != 0)
			break;
		if (cookies) {
			*cookies++ = i + 1;
			ncookies++;
		}
	}

	if (ap->a_ncookies) {
		if (error) {
			free(*ap->a_cookies, M_TEMP);
			*ap->a_ncookies = 0;
			*ap->a_cookies = NULL;
		} else
			*ap->a_ncookies = ncookies;
	}

	uio->uio_offset = i;
	return (error);
}

int
kernfs_inactive(v)
	void *v;
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_inactive(%p)\n", vp);
#endif
	/*
	 * Clear out the v_type field to avoid
	 * nasty things happening in vgone().
	 */
	VOP_UNLOCK(vp, 0);
	vp->v_type = VNON;
	return (0);
}

int
kernfs_reclaim(v)
	void *v;
{
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_reclaim(%p)\n", vp);
#endif
	if (vp->v_data) {
		FREE(vp->v_data, M_TEMP);
		vp->v_data = 0;
	}
	return (0);
}

/*
 * Return POSIX pathconf information applicable to special devices.
 */
int
kernfs_pathconf(v)
	void *v;
{
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		register_t *a_retval;
	} */ *ap = v;

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = LINK_MAX;
		return (0);
	case _PC_MAX_CANON:
		*ap->a_retval = MAX_CANON;
		return (0);
	case _PC_MAX_INPUT:
		*ap->a_retval = MAX_INPUT;
		return (0);
	case _PC_PIPE_BUF:
		*ap->a_retval = PIPE_BUF;
		return (0);
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		return (0);
	case _PC_VDISABLE:
		*ap->a_retval = _POSIX_VDISABLE;
		return (0);
	case _PC_SYNC_IO:
		*ap->a_retval = 1;
		return (0);
	default:
		return (EINVAL);
	}
	/* NOTREACHED */
}

/*
 * Print out the contents of a /dev/fd vnode.
 */
/* ARGSUSED */
int
kernfs_print(v)
	void *v;
{

	printf("tag VT_KERNFS, kernfs vnode\n");
	return (0);
}

int
kernfs_link(v) 
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
kernfs_symlink(v)
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
