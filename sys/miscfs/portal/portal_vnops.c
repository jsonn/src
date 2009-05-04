/*	$NetBSD: portal_vnops.c,v 1.79.2.1 2009/05/04 08:14:05 yamt Exp $	*/

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
 * 3. Neither the name of the University nor the names of its contributors
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
 *	from: Id: portal_vnops.c,v 1.4 1992/05/30 10:05:24 jsp Exp
 *	@(#)portal_vnops.c	8.14 (Berkeley) 5/21/95
 */

/*
 * Portal Filesystem
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: portal_vnops.c,v 1.79.2.1 2009/05/04 08:14:05 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/filedesc.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/socketvar.h>
#include <sys/un.h>
#include <sys/unpcb.h>
#include <sys/syscallargs.h>
#include <sys/kauth.h>

#include <miscfs/genfs/genfs.h>
#include <miscfs/portal/portal.h>

static int portal_fileid = PORTAL_ROOTFILEID+1;

static void	portal_closefd(struct lwp *, int);
static int	portal_connect(struct socket *, struct socket *);

int	portal_lookup(void *);
#define	portal_create	genfs_eopnotsupp
#define	portal_mknod	genfs_eopnotsupp
int	portal_open(void *);
#define	portal_close	genfs_nullop
#define	portal_access	genfs_nullop
int	portal_getattr(void *);
int	portal_setattr(void *);
#define	portal_read	genfs_eopnotsupp
#define	portal_write	genfs_eopnotsupp
#define	portal_fcntl	genfs_fcntl
#define	portal_ioctl	genfs_enoioctl
#define	portal_poll	genfs_eopnotsupp
#define portal_revoke	genfs_revoke
#define	portal_fsync	genfs_nullop
#define	portal_seek	genfs_seek
#define	portal_remove	genfs_eopnotsupp
int	portal_link(void *);
#define	portal_rename	genfs_eopnotsupp
#define	portal_mkdir	genfs_eopnotsupp
#define	portal_rmdir	genfs_eopnotsupp
int	portal_symlink(void *);
int	portal_readdir(void *);
#define	portal_readlink	genfs_eopnotsupp
#define	portal_abortop	genfs_abortop
int	portal_inactive(void *);
int	portal_reclaim(void *);
#define	portal_lock	genfs_lock
#define	portal_unlock	genfs_unlock
#define	portal_bmap	genfs_badop
#define	portal_strategy	genfs_badop
int	portal_print(void *);
#define	portal_islocked	genfs_islocked
int	portal_pathconf(void *);
#define	portal_advlock	genfs_eopnotsupp
#define	portal_bwrite	genfs_eopnotsupp
#define	portal_putpages	genfs_null_putpages

int (**portal_vnodeop_p)(void *);
const struct vnodeopv_entry_desc portal_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, portal_lookup },		/* lookup */
	{ &vop_create_desc, portal_create },		/* create */
	{ &vop_mknod_desc, portal_mknod },		/* mknod */
	{ &vop_open_desc, portal_open },		/* open */
	{ &vop_close_desc, portal_close },		/* close */
	{ &vop_access_desc, portal_access },		/* access */
	{ &vop_getattr_desc, portal_getattr },		/* getattr */
	{ &vop_setattr_desc, portal_setattr },		/* setattr */
	{ &vop_read_desc, portal_read },		/* read */
	{ &vop_write_desc, portal_write },		/* write */
	{ &vop_fcntl_desc, portal_fcntl },		/* fcntl */
	{ &vop_ioctl_desc, portal_ioctl },		/* ioctl */
	{ &vop_poll_desc, portal_poll },		/* poll */
	{ &vop_revoke_desc, portal_revoke },		/* revoke */
	{ &vop_fsync_desc, portal_fsync },		/* fsync */
	{ &vop_seek_desc, portal_seek },		/* seek */
	{ &vop_remove_desc, portal_remove },		/* remove */
	{ &vop_link_desc, portal_link },		/* link */
	{ &vop_rename_desc, portal_rename },		/* rename */
	{ &vop_mkdir_desc, portal_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, portal_rmdir },		/* rmdir */
	{ &vop_symlink_desc, portal_symlink },		/* symlink */
	{ &vop_readdir_desc, portal_readdir },		/* readdir */
	{ &vop_readlink_desc, portal_readlink },	/* readlink */
	{ &vop_abortop_desc, portal_abortop },		/* abortop */
	{ &vop_inactive_desc, portal_inactive },	/* inactive */
	{ &vop_reclaim_desc, portal_reclaim },		/* reclaim */
	{ &vop_lock_desc, portal_lock },		/* lock */
	{ &vop_unlock_desc, portal_unlock },		/* unlock */
	{ &vop_bmap_desc, portal_bmap },		/* bmap */
	{ &vop_strategy_desc, portal_strategy },	/* strategy */
	{ &vop_print_desc, portal_print },		/* print */
	{ &vop_islocked_desc, portal_islocked },	/* islocked */
	{ &vop_pathconf_desc, portal_pathconf },	/* pathconf */
	{ &vop_advlock_desc, portal_advlock },		/* advlock */
	{ &vop_bwrite_desc, portal_bwrite },		/* bwrite */
	{ &vop_putpages_desc, portal_putpages },	/* putpages */
	{ NULL, NULL }
};
const struct vnodeopv_desc portal_vnodeop_opv_desc =
	{ &portal_vnodeop_p, portal_vnodeop_entries };

static void
portal_closefd(struct lwp *l, int fd)
{
	struct sys_close_args /* {
		syscallarg(int) fd;
	} */ ua;
	register_t retval[2];
	int error;

	SCARG(&ua, fd) = fd;
	error = sys_close(l, &ua, retval);
	/*
	 * We should never get an error, and there isn't anything
	 * we could do if we got one, so just print a message.
	 */
	if (error)
		printf("portal_closefd: error = %d\n", error);
}

/*
 * vp is the current namei directory
 * cnp is the name to locate in that directory...
 */
int
portal_lookup(void *v)
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
	struct portalnode *pt;
	int error;
	struct vnode *fvp = 0;
	const char *path;
	int size;

	*vpp = NULLVP;

	if (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME)
		return (EROFS);

	if (cnp->cn_namelen == 1 && *pname == '.') {
		*vpp = dvp;
		VREF(dvp);
		return (0);
	}

	error = getnewvnode(VT_PORTAL, dvp->v_mount, portal_vnodeop_p, &fvp);
	if (error)
		goto bad;
	fvp->v_type = VREG;
	uvm_vnp_setsize(fvp, 0);
	fvp->v_data = malloc(sizeof(struct portalnode), M_TEMP, M_WAITOK);

	pt = VTOPORTAL(fvp);

	/*
	 * Save all of the remaining pathname and
	 * advance the namei next pointer to the end
	 * of the string.
	 */
	for (size = 0, path = pname; *path; path++)
		size++;
	cnp->cn_consume = size - cnp->cn_namelen;
	cnp->cn_flags &= ~REQUIREDIR;

	pt->pt_arg = malloc(size+1, M_TEMP, M_WAITOK);
	pt->pt_size = size+1;
	memcpy(pt->pt_arg, pname, pt->pt_size);
	pt->pt_fileid = portal_fileid++;

	vn_lock(fvp, LK_EXCLUSIVE | LK_RETRY);
	*vpp = fvp;
	return (0);

bad:;
	if (fvp)
		vrele(fvp);
	return (error);
}

static int
portal_connect(struct socket *so, struct socket *so2)
{
	/* from unp_connect, bypassing the namei stuff... */
	struct socket *so3;
	struct unpcb *unp2;
	struct unpcb *unp3;

	KASSERT(solocked2(so, so2));

	if (so->so_type != so2->so_type)
		return (EPROTOTYPE);

	if ((so2->so_options & SO_ACCEPTCONN) == 0 ||
	    (so3 = sonewconn(so2, 0)) == 0)
		return (ECONNREFUSED);

	unp2 = sotounpcb(so2);
	unp3 = sotounpcb(so3);
	if (unp2->unp_addr) {
		unp3->unp_addr = malloc(unp2->unp_addrlen,
		    M_SONAME, M_WAITOK);
		memcpy(unp3->unp_addr, unp2->unp_addr,
		    unp2->unp_addrlen);
		unp3->unp_addrlen = unp2->unp_addrlen;
	}

	so2 = so3;


	return (unp_connect2(so, so2, PRU_CONNECT2));
}

int
portal_open(void *v)
{
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct socket *so = 0, *so2;
	struct portalnode *pt;
	struct lwp *l = curlwp;
	struct vnode *vp = ap->a_vp;
	struct uio auio;
	struct iovec aiov[2];
	int res;
	struct mbuf *cm = 0;
	struct cmsghdr *cmsg;
	int newfds;
	int *ip;
	int fd;
	int error;
	int len;
	struct portalmount *fmp;
	struct file *fp;
	struct portal_cred pcred;

	/*
	 * Nothing to do when opening the root node.
	 */
	if (vp->v_vflag & VV_ROOT)
		return (0);

	/*
	 * Can't be opened unless the caller is set up
	 * to deal with the side effects.  Check for this
	 * by testing whether the dupfd has been set.
	 */
	if (curlwp->l_dupfd >= 0)	/* XXX */
		return EDUPFD;

	pt = VTOPORTAL(vp);
	fmp = VFSTOPORTAL(vp->v_mount);
	so2 = fmp->pm_server->f_data;

	if (so2 == NULL) {
		/* XXX very fishy */
		return ECONNREFUSED;
	}

	/*
	 * Create a new socket.
	 */
	error = socreate(AF_LOCAL, &so, SOCK_STREAM, 0, l, so2);
	if (error)
		goto bad;

	/*
	 * Reserve some buffer space
	 */
	res = pt->pt_size + sizeof(pcred) + 512;	/* XXX */
	solock(so);
	error = soreserve(so, res, res);
	if (error) {
		sounlock(so);
		goto bad;
	}

	/*
	 * Kick off connection
	 */
	error = portal_connect(so, so2);
	if (error) {
		sounlock(so);
		goto bad;
	}

	/*
	 * Wait for connection to complete
	 */
	/*
	 * XXX: Since the mount point is holding a reference on the
	 * underlying server socket, it is not easy to find out whether
	 * the server process is still running.  To handle this problem
	 * we loop waiting for the new socket to be connected (something
	 * which will only happen if the server is still running) or for
	 * the reference count on the server socket to drop to 1, which
	 * will happen if the server dies.  Sleep for 5 second intervals
	 * and keep polling the reference count.   XXX.
	 */
	while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0) {
		if (fmp->pm_server->f_count == 1) {
			error = ECONNREFUSED;
			sounlock(so);
			goto bad;
		}
		sowait(so, false, 5 * hz);
	}
	if (so->so_error) {
		error = so->so_error;
		sounlock(so);
		goto bad;
	}

	/*
	 * Set miscellaneous flags
	 */
	so->so_rcv.sb_timeo = 0;
	so->so_snd.sb_timeo = 0;
	so->so_rcv.sb_flags |= SB_NOINTR;
	so->so_snd.sb_flags |= SB_NOINTR;
	sounlock(so);

	pcred.pcr_flag = ap->a_mode;
	pcred.pcr_uid = kauth_cred_geteuid(ap->a_cred);
	pcred.pcr_gid = kauth_cred_getegid(ap->a_cred);
	pcred.pcr_ngroups = kauth_cred_ngroups(ap->a_cred);
	kauth_cred_getgroups(ap->a_cred, pcred.pcr_groups, pcred.pcr_ngroups,
	    UIO_SYSSPACE);
	aiov[0].iov_base = &pcred;
	aiov[0].iov_len = sizeof(pcred);
	aiov[1].iov_base = pt->pt_arg;
	aiov[1].iov_len = pt->pt_size;
	auio.uio_iov = aiov;
	auio.uio_iovcnt = 2;
	auio.uio_rw = UIO_WRITE;
	auio.uio_offset = 0;
	auio.uio_resid = aiov[0].iov_len + aiov[1].iov_len;
	UIO_SETUP_SYSSPACE(&auio);

	error = (*so->so_send)(so, NULL, &auio, NULL, NULL, 0, l);
	if (error)
		goto bad;

	len = auio.uio_resid = sizeof(int);
	do {
		struct mbuf *m = 0;
		int flags = MSG_WAITALL;
		error = (*so->so_receive)(so, (struct mbuf **) 0, &auio,
					  &m, &cm, &flags);
		if (error)
			goto bad;

		/*
		 * Grab an error code from the mbuf.
		 */
		if (m) {
			m = m_pullup(m, sizeof(int));	/* Needed? */
			if (m) {
				error = *(mtod(m, int *));
				m_freem(m);
			} else {
				error = EINVAL;
			}
		} else {
			if (cm == 0) {
				error = ECONNRESET;	 /* XXX */
#ifdef notdef
				break;
#endif
			}
		}
	} while (cm == 0 && auio.uio_resid == len && !error);

	if (cm == 0)
		goto bad;

	if (auio.uio_resid) {
		error = 0;
#ifdef notdef
		error = EMSGSIZE;
		goto bad;
#endif
	}

	/*
	 * XXX: Break apart the control message, and retrieve the
	 * received file descriptor.  Note that more than one descriptor
	 * may have been received, or that the rights chain may have more
	 * than a single mbuf in it.  What to do?
	 */
	cmsg = mtod(cm, struct cmsghdr *);
	newfds = (cmsg->cmsg_len - CMSG_ALIGN(sizeof(*cmsg))) / sizeof(int);
	if (newfds == 0) {
		error = ECONNREFUSED;
		goto bad;
	}
	/*
	 * At this point the rights message consists of a control message
	 * header, followed by a data region containing a vector of
	 * integer file descriptors.  The fds were allocated by the action
	 * of receiving the control message.
	 */
	ip = (int *) CMSG_DATA(cmsg);
	fd = *ip++;
	if (newfds > 1) {
		/*
		 * Close extra fds.
		 */
		int i;
		printf("portal_open: %d extra fds\n", newfds - 1);
		for (i = 1; i < newfds; i++) {
			portal_closefd(l, *ip); /* XXXNJWLWP */
			ip++;
		}
	}

	/*
	 * Check that the mode the file is being opened for is a subset
	 * of the mode of the existing descriptor.
	 */
 	fp = l->l_proc->p_fd->fd_ofiles[fd]->ff_file;
	if (((ap->a_mode & (FREAD|FWRITE)) | fp->f_flag) != fp->f_flag) {
		portal_closefd(l, fd); /* XXXNJWLWP */
		error = EACCES;
		goto bad;
	}

	/*
	 * Save the dup fd in the proc structure then return the
	 * special error code (EMOVEFD) which causes magic things to
	 * happen in vn_open.  The whole concept is, well, hmmm.
	 */
	curlwp->l_dupfd = fd;
	error = EMOVEFD;

bad:;
	/*
	 * And discard the control message.
	 */
	if (cm) {
		m_freem(cm);
	}

	if (so) {
		solock(so);
		soshutdown(so, 2);
		sounlock(so);
		soclose(so);
	}
	return (error);
}

int
portal_getattr(void *v)
{
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct vattr *vap = ap->a_vap;

	memset(vap, 0, sizeof(*vap));
	vattr_null(vap);
	vap->va_uid = 0;
	vap->va_gid = 0;
	vap->va_fsid = vp->v_mount->mnt_stat.f_fsidx.__fsid_val[0];
	vap->va_size = DEV_BSIZE;
	vap->va_blocksize = DEV_BSIZE;
	/* Make all times be current TOD. */
	getnanotime(&vap->va_ctime);
	vap->va_atime = vap->va_mtime = vap->va_ctime;
	vap->va_gen = 0;
	vap->va_flags = 0;
	vap->va_rdev = 0;
	/* vap->va_qbytes = 0; */
	vap->va_bytes = 0;
	/* vap->va_qsize = 0; */
	if (vp->v_vflag & VV_ROOT) {
		vap->va_type = VDIR;
		vap->va_mode = S_IRUSR|S_IWUSR|S_IXUSR|
				S_IRGRP|S_IWGRP|S_IXGRP|
				S_IROTH|S_IWOTH|S_IXOTH;
		vap->va_nlink = 2;
		vap->va_fileid = 2;
	} else {
		vap->va_type = VREG;
		vap->va_mode = S_IRUSR|S_IWUSR|
				S_IRGRP|S_IWGRP|
				S_IROTH|S_IWOTH;
		vap->va_nlink = 1;
		vap->va_fileid = VTOPORTAL(vp)->pt_fileid;
	}
	return (0);
}

int
portal_setattr(void *v)
{
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
	} */ *ap = v;

	/*
	 * Can't mess with the root vnode
	 */
	if (ap->a_vp->v_vflag & VV_ROOT)
		return (EACCES);

	return (0);
}

/*
 * Fake readdir, just return empty directory.
 * It is hard to deal with '.' and '..' so don't bother.
 */
/*ARGSUSED*/
int
portal_readdir(void *v)
{

	return (0);
}

/*ARGSUSED*/
int
portal_inactive(void *v)
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
	} */ *ap = v;

	VOP_UNLOCK(ap->a_vp, 0);
	return (0);
}

int
portal_reclaim(void *v)
{
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct portalnode *pt = VTOPORTAL(ap->a_vp);

	if (pt->pt_arg) {
		free(pt->pt_arg, M_TEMP);
		pt->pt_arg = NULL;
	}
	free(ap->a_vp->v_data, M_TEMP);
	ap->a_vp->v_data = NULL;

	return (0);
}

/*
 * Return POSIX pathconf information applicable to special devices.
 */
int
portal_pathconf(void *v)
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
 * Print out the contents of a Portal vnode.
 */
/* ARGSUSED */
int
portal_print(void *v)
{
	printf("tag VT_PORTAL, portal vnode\n");
	return (0);
}

int
portal_link(void *v)
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
portal_symlink(void *v)
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
