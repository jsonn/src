/*	$NetBSD: nfs_vfsops.c,v 1.32.2.2 1994/08/23 09:31:01 pk Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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
 *	@(#)nfs_vfsops.c	8.3 (Berkeley) 1/4/94
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <sys/proc.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsv2.h>
#include <nfs/nfsnode.h>
#include <nfs/nfsmount.h>
#include <nfs/nfs.h>
#include <nfs/xdr_subs.h>
#include <nfs/nfsm_subs.h>
#include <nfs/nfsdiskless.h>
#include <nfs/nqnfs.h>

/*
 * nfs vfs operations.
 */
struct vfsops nfs_vfsops = {
	MOUNT_NFS,
	nfs_mount,
	nfs_start,
	nfs_unmount,
	nfs_root,
	nfs_quotactl,
	nfs_statfs,
	nfs_sync,
	nfs_vget,
	nfs_fhtovp,
	nfs_vptofh,
	nfs_init,
};

extern u_long nfs_procids[NFS_NPROCS];
extern u_long nfs_prog, nfs_vers;
void nfs_disconnect __P((struct nfsmount *));

static struct mount *
nfs_mount_diskless __P((struct nfs_dlmount *, char *, int, struct vnode **));

#define TRUE	1
#define	FALSE	0

/*
 * nfs statfs call
 */
int
nfs_statfs(mp, sbp, p)
	struct mount *mp;
	register struct statfs *sbp;
	struct proc *p;
{
	register struct vnode *vp;
	register struct nfsv2_statfs *sfp;
	register caddr_t cp;
	register long t1;
	caddr_t bpos, dpos, cp2;
	int error = 0, isnq;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	struct nfsmount *nmp;
	struct ucred *cred;
	struct nfsnode *np;

	nmp = VFSTONFS(mp);
	isnq = (nmp->nm_flag & NFSMNT_NQNFS);
	if (error = nfs_nget(mp, &nmp->nm_fh, &np))
		return (error);
	vp = NFSTOV(np);
	nfsstats.rpccnt[NFSPROC_STATFS]++;
	cred = crget();
	cred->cr_ngroups = 1;
	nfsm_reqhead(vp, NFSPROC_STATFS, NFSX_FH);
	nfsm_fhtom(vp);
	nfsm_request(vp, NFSPROC_STATFS, p, cred);
	nfsm_dissect(sfp, struct nfsv2_statfs *, NFSX_STATFS(isnq));
#ifdef COMPAT_09
	sbp->f_type = 2;
#else
	sbp->f_type = 0;
#endif
	sbp->f_flags = nmp->nm_flag;
	sbp->f_iosize = NFS_MAXDGRAMDATA;
	sbp->f_bsize = fxdr_unsigned(long, sfp->sf_bsize);
	sbp->f_blocks = fxdr_unsigned(long, sfp->sf_blocks);
	sbp->f_bfree = fxdr_unsigned(long, sfp->sf_bfree);
	sbp->f_bavail = fxdr_unsigned(long, sfp->sf_bavail);
	if (isnq) {
		sbp->f_files = fxdr_unsigned(long, sfp->sf_files);
		sbp->f_ffree = fxdr_unsigned(long, sfp->sf_ffree);
	} else {
		sbp->f_files = 0;
		sbp->f_ffree = 0;
	}
	if (sbp != &mp->mnt_stat) {
		bcopy(mp->mnt_stat.f_mntonname, sbp->f_mntonname, MNAMELEN);
		bcopy(mp->mnt_stat.f_mntfromname, sbp->f_mntfromname, MNAMELEN);
	}
	strncpy(&sbp->f_fstypename[0], mp->mnt_op->vfs_name, MFSNAMELEN);
	sbp->f_fstypename[MFSNAMELEN] = '\0';
	nfsm_reqdone;
	vrele(vp);
	crfree(cred);
	return (error);
}

/*
 * Mount a remote root fs via. NFS.  It goes like this:
 * - Call nfs_boot_init() to fill in the nfs_diskless struct
 *   (using RARP, bootparam RPC, mountd RPC)
 * - hand craft the swap nfs vnode hanging off a fake mount point
 *	if swdevt[0].sw_dev == NODEV
 * - build the rootfs mount point and call mountnfs() to do the rest.
 */
int
nfs_mountroot()
{
	struct nfs_diskless nd;
	struct vattr attr;
	struct mount *mp;
	struct vnode *vp;
	struct proc *procp;
	struct ucred *cred;
	long n;
	int error;

	procp = curproc; /* XXX */

	/*
	 * XXX time must be non-zero when we init the interface or else
	 * the arp code will wedge.  [Fixed now in if_ether.c]
	 * However, the NFS attribute cache gives false "hits" when
	 * time.tv_sec < NFS_ATTRTIMEO(np) so keep this in for now.
	 */
	if (time.tv_sec < NFS_MAXATTRTIMO)
		time.tv_sec = NFS_MAXATTRTIMO;

	/*
	 * Call nfs_boot_init() to fill in the nfs_diskless struct.
	 * Side effect:  Finds and configures a network interface.
	 */
	bzero((caddr_t) &nd, sizeof(nd));
	nfs_boot_init(&nd, procp);

	/*
	 * Create the root mount point.
	 */
	mp = nfs_mount_diskless(&nd.nd_root, "/", MNT_RDONLY, &vp);

	/*
	 * Link it into the mount list.
	 */
	if (vfs_lock(mp))
		panic("nfs_mountroot: vfs_lock");
	TAILQ_INSERT_TAIL(&mountlist, mp, mnt_list);
	mp->mnt_flag |= MNT_ROOTFS;
	mp->mnt_vnodecovered = NULLVP;
	vfs_unlock(mp);
	rootvp = vp;


	/* Get root attributes (for the time). */
	error = VOP_GETATTR(vp, &attr, procp->p_ucred, procp);
	if (error) panic("nfs_mountroot: getattr for root");
	n = attr.va_mtime.ts_sec;
#ifdef	DEBUG
	printf("root time: 0x%x\n", n);
#endif
	inittodr(n);

#ifdef notyet
	/* Set up swap credentials. */
	proc0.p_ucred->cr_uid = ntohl(nd.swap_ucred.cr_uid);
	proc0.p_ucred->cr_gid = ntohl(nd.swap_ucred.cr_gid);
	if ((proc0.p_ucred->cr_ngroups = ntohs(nd.swap_ucred.cr_ngroups)) >
		NGROUPS)
		proc0.p_ucred->cr_ngroups = NGROUPS;
	for (i = 0; i < proc0.p_ucred->cr_ngroups; i++)
	    proc0.p_ucred->cr_groups[i] = ntohl(nd.swap_ucred.cr_groups[i]);
#endif

	/*
	 * "Mount" the swap device.
	 *
	 * On a "dataless" configuration (swap on disk) we will have:
	 *	(swdevt[0].sw_dev != NODEV) identifying the swap device.
	 */
	if (bdevvp(swapdev, &swapdev_vp))
		panic("nfs_mountroot: can't setup swap vp");
	if (swdevt[0].sw_dev != NODEV)
		return (0);

	/*
	 * If swapping to an nfs node:  (swdevt[0].sw_dev == NODEV)
	 * Create a fake mount point just for the swap vnode so that the
	 * swap file can be on a different server from the rootfs.
	 */
	mp = nfs_mount_diskless(&nd.nd_swap, "/swap", 0, &vp);

	/*
	 * Since the swap file is not the root dir of a file system,
	 * hack it to a regular file.
	 */
	vp->v_type = VREG;
	vp->v_flag = 0;
	swdevt[0].sw_vp = vp;

	/*
	 * Find out how large the swap file is.
	 */
	error = VOP_GETATTR(vp, &attr, procp->p_ucred, procp);
	if (error)
		panic("nfs_mountroot: getattr for swap");
	n = (long) (attr.va_size >> DEV_BSHIFT);
#ifdef	DEBUG
	printf("swap size: 0x%x (blocks)\n", n);
#endif
	swdevt[0].sw_nblks = n;

	return (0);
}

/*
 * Internal version of mount system call for diskless setup.
 */
static struct mount *
nfs_mount_diskless(ndmntp, mntname, mntflag, vpp)
	struct nfs_dlmount *ndmntp;
	char *mntname;
	int mntflag;
	struct vnode **vpp;
{
	struct nfs_args args;
	struct mount *mp;
	struct mbuf *m;
	int error;

	/* Create the mount point. */
	mp = (struct mount *)malloc((u_long)sizeof(struct mount),
	    M_MOUNT, M_NOWAIT);
	if (mp == NULL)
		panic("nfs_mountroot: malloc mount for %s", mntname);
	bzero((char *)mp, (u_long)sizeof(struct mount));
	mp->mnt_op = &nfs_vfsops;
	mp->mnt_flag = mntflag;

	/* Initialize mount args. */
	bzero((caddr_t) &args, sizeof(args));
	args.addr     = (struct sockaddr *)&ndmntp->ndm_saddr;
	args.addrlen  = args.addr->sa_len;
	args.sotype   = SOCK_DGRAM;
	args.fh       = (nfsv2fh_t *)ndmntp->ndm_fh;
	args.hostname = ndmntp->ndm_host;
	args.flags    = NFSMNT_RESVPORT;

#ifdef	NFS_BOOT_RWSIZE
	/*
	 * Reduce rsize,wsize for interfaces that consistently
	 * drop fragments of long UDP messages.  (i.e. wd8003).
	 * You can always change these later via remount.
	 */
	args.flags   |= NFSMNT_WSIZE | NFSMNT_RSIZE;
	args.wsize    = NFS_BOOT_RWSIZE;
	args.rsize    = NFS_BOOT_RWSIZE;
#endif

	/* Get mbuf for server sockaddr. */
	m = m_get(M_WAIT, MT_SONAME);
	if (m == NULL)
		panic("nfs_mountroot: mget soname for %s", mntname);
	bcopy((caddr_t)args.addr, mtod(m, caddr_t),
	      (m->m_len = args.addr->sa_len));

	if (error = mountnfs(&args, mp, m, mntname, args.hostname, vpp))
		panic("nfs_mountroot: mount %s failed: %d", mntname);

	return (mp);
}

void
nfs_decode_args(nmp, argp)
	struct nfsmount *nmp;
	struct nfs_args *argp;
{
	int s;
	int adjsock;

	s = splnet();

	/* Re-bind if rsrvd port requested and wasn't on one */
	adjsock = !(nmp->nm_flag & NFSMNT_RESVPORT)
		  && (argp->flags & NFSMNT_RESVPORT);

	/* Update flags atomically.  Don't change the lock bits. */
	nmp->nm_flag =
	    (argp->flags & ~NFSMNT_INTERNAL) | (nmp->nm_flag & NFSMNT_INTERNAL);
	splx(s);

	if ((argp->flags & NFSMNT_TIMEO) && argp->timeo > 0) {
		nmp->nm_timeo = (argp->timeo * NFS_HZ + 5) / 10;
		if (nmp->nm_timeo < NFS_MINTIMEO)
			nmp->nm_timeo = NFS_MINTIMEO;
		else if (nmp->nm_timeo > NFS_MAXTIMEO)
			nmp->nm_timeo = NFS_MAXTIMEO;
	}

	if ((argp->flags & NFSMNT_RETRANS) && argp->retrans > 1) {
		nmp->nm_retry = argp->retrans;
		if (nmp->nm_retry > NFS_MAXREXMIT)
			nmp->nm_retry = NFS_MAXREXMIT;
	}

	if ((argp->flags & NFSMNT_WSIZE) && argp->wsize > 0) {
		int osize = nmp->nm_wsize;
		nmp->nm_wsize = argp->wsize;
		/* Round down to multiple of blocksize */
		nmp->nm_wsize &= ~0x1ff;
		if (nmp->nm_wsize <= 0)
			nmp->nm_wsize = 512;
		else if (nmp->nm_wsize > NFS_MAXDATA)
			nmp->nm_wsize = NFS_MAXDATA;
		adjsock |= (nmp->nm_wsize != osize);
	}
	if (nmp->nm_wsize > MAXBSIZE)
		nmp->nm_wsize = MAXBSIZE;

	if ((argp->flags & NFSMNT_RSIZE) && argp->rsize > 0) {
		int osize = nmp->nm_rsize;
		nmp->nm_rsize = argp->rsize;
		/* Round down to multiple of blocksize */
		nmp->nm_rsize &= ~0x1ff;
		if (nmp->nm_rsize <= 0)
			nmp->nm_rsize = 512;
		else if (nmp->nm_rsize > NFS_MAXDATA)
			nmp->nm_rsize = NFS_MAXDATA;
		adjsock |= (nmp->nm_rsize != osize);
	}
	if (nmp->nm_rsize > MAXBSIZE)
		nmp->nm_rsize = MAXBSIZE;

	if ((argp->flags & NFSMNT_MAXGRPS) && argp->maxgrouplist >= 0 &&
		argp->maxgrouplist <= NFS_MAXGRPS)
		nmp->nm_numgrps = argp->maxgrouplist;
	if ((argp->flags & NFSMNT_READAHEAD) && argp->readahead >= 0 &&
		argp->readahead <= NFS_MAXRAHEAD)
		nmp->nm_readahead = argp->readahead;
	if ((argp->flags & NFSMNT_LEASETERM) && argp->leaseterm >= 2 &&
		argp->leaseterm <= NQ_MAXLEASE)
		nmp->nm_leaseterm = argp->leaseterm;
	if ((argp->flags & NFSMNT_DEADTHRESH) && argp->deadthresh >= 1 &&
		argp->deadthresh <= NQ_NEVERDEAD)
		nmp->nm_deadthresh = argp->deadthresh;

	if (nmp->nm_so && adjsock) {
		nfs_disconnect(nmp);
		if (nmp->nm_sotype == SOCK_DGRAM)
			while (nfs_connect(nmp, (struct nfsreq *)0)) {
				printf("nfs_args: retrying connect\n");
				(void) tsleep((caddr_t)&lbolt,
					      PSOCK, "nfscon", 0);
			}
	}
}

/*
 * VFS Operations.
 *
 * mount system call
 * It seems a bit dumb to copyinstr() the host and path here and then
 * bcopy() them in mountnfs(), but I wanted to detect errors before
 * doing the sockargs() call because sockargs() allocates an mbuf and
 * an error after that means that I have to release the mbuf.
 */
/* ARGSUSED */
int
nfs_mount(mp, path, data, ndp, p)
	struct mount *mp;
	char *path;
	caddr_t data;
	struct nameidata *ndp;
	struct proc *p;
{
	int error;
	struct nfs_args args;
	struct mbuf *nam;
	struct vnode *vp;
	char pth[MNAMELEN], hst[MNAMELEN];
	u_int len;
	nfsv2fh_t nfh;

	if (error = copyin(data, (caddr_t)&args, sizeof (struct nfs_args)))
		return (error);
	if (mp->mnt_flag & MNT_UPDATE) {
		register struct nfsmount *nmp = VFSTONFS(mp);

		if (nmp == NULL)
			return (EIO);
		nfs_decode_args(nmp, &args);
		return (0);
	}
	if (error = copyin((caddr_t)args.fh, (caddr_t)&nfh, sizeof (nfsv2fh_t)))
		return (error);
	if (error = copyinstr(path, pth, MNAMELEN-1, &len))
		return (error);
	bzero(&pth[len], MNAMELEN - len);
	if (error = copyinstr(args.hostname, hst, MNAMELEN-1, &len))
		return (error);
	bzero(&hst[len], MNAMELEN - len);
	/* sockargs() call must be after above copyin() calls */
	if (error = sockargs(&nam, (caddr_t)args.addr,
		args.addrlen, MT_SONAME))
		return (error);
	args.fh = &nfh;
	error = mountnfs(&args, mp, nam, pth, hst, &vp);
	return (error);
}

/*
 * Common code for mount and mountroot
 */
int
mountnfs(argp, mp, nam, pth, hst, vpp)
	register struct nfs_args *argp;
	register struct mount *mp;
	struct mbuf *nam;
	char *pth, *hst;
	struct vnode **vpp;
{
	register struct nfsmount *nmp;
	struct nfsnode *np;
	int error;

	if (mp->mnt_flag & MNT_UPDATE) {
		nmp = VFSTONFS(mp);
		/* update paths, file handles, etc, here	XXX */
		m_freem(nam);
		return (0);
	} else {
		MALLOC(nmp, struct nfsmount *, sizeof (struct nfsmount),
		    M_NFSMNT, M_WAITOK);
		bzero((caddr_t)nmp, sizeof (struct nfsmount));
		mp->mnt_data = (qaddr_t)nmp;
	}
	getnewfsid(mp, makefstype(MOUNT_NFS));
	nmp->nm_mountp = mp;
	if ((argp->flags & (NFSMNT_NQNFS | NFSMNT_MYWRITE)) ==
		(NFSMNT_NQNFS | NFSMNT_MYWRITE)) {
		error = EPERM;
		goto bad;
	}
	if (argp->flags & NFSMNT_NQNFS)
		/*
		 * We have to set mnt_maxsymlink to a non-zero value so
		 * that COMPAT_43 routines will know that we are setting
		 * the d_type field in directories (and can zero it for
		 * unsuspecting binaries).
		 */
		mp->mnt_maxsymlinklen = 1;
	nmp->nm_timeo = NFS_TIMEO;
	nmp->nm_retry = NFS_RETRANS;
	nmp->nm_wsize = NFS_WSIZE;
	nmp->nm_rsize = NFS_RSIZE;
	nmp->nm_numgrps = NFS_MAXGRPS;
	nmp->nm_readahead = NFS_DEFRAHEAD;
	nmp->nm_leaseterm = NQ_DEFLEASE;
	nmp->nm_deadthresh = NQ_DEADTHRESH;
	CIRCLEQ_INIT(&nmp->nm_timerhead);
	nmp->nm_inprog = NULLVP;
	bcopy((caddr_t)argp->fh, (caddr_t)&nmp->nm_fh, sizeof(nfsv2fh_t));
#ifdef COMPAT_09
	mp->mnt_stat.f_type = 2;
#else
	mp->mnt_stat.f_type = 0;
#endif
	strncpy(&mp->mnt_stat.f_fstypename[0], mp->mnt_op->vfs_name, MFSNAMELEN);
	mp->mnt_stat.f_fstypename[MFSNAMELEN] = '\0';
	bcopy(hst, mp->mnt_stat.f_mntfromname, MNAMELEN);
	bcopy(pth, mp->mnt_stat.f_mntonname, MNAMELEN);
	nmp->nm_nam = nam;
	nfs_decode_args(nmp, argp);

	/* Set up the sockets and per-host congestion */
	nmp->nm_sotype = argp->sotype;
	nmp->nm_soproto = argp->proto;

	/*
	 * For Connection based sockets (TCP,...) defer the connect until
	 * the first request, in case the server is not responding.
	 */
	if (nmp->nm_sotype == SOCK_DGRAM &&
		(error = nfs_connect(nmp, (struct nfsreq *)0)))
		goto bad;

	/*
	 * This is silly, but it has to be set so that vinifod() works.
	 * We do not want to do an nfs_statfs() here since we can get
	 * stuck on a dead server and we are holding a lock on the mount
	 * point.
	 */
	mp->mnt_stat.f_iosize = NFS_MAXDGRAMDATA;
	/*
	 * A reference count is needed on the nfsnode representing the
	 * remote root.  If this object is not persistent, then backward
	 * traversals of the mount point (i.e. "..") will not work if
	 * the nfsnode gets flushed out of the cache. Ufs does not have
	 * this problem, because one can identify root inodes by their
	 * number == ROOTINO (2).
	 */
	if (error = nfs_nget(mp, &nmp->nm_fh, &np))
		goto bad;
	*vpp = NFSTOV(np);

	return (0);
bad:
	nfs_disconnect(nmp);
	free((caddr_t)nmp, M_NFSMNT);
	m_freem(nam);
	return (error);
}

/*
 * unmount system call
 */
int
nfs_unmount(mp, mntflags, p)
	struct mount *mp;
	int mntflags;
	struct proc *p;
{
	register struct nfsmount *nmp;
	struct nfsnode *np;
	struct vnode *vp;
	int error, flags = 0;
	extern int doforce;

	if (mntflags & MNT_FORCE) {
		if (!doforce || (mp->mnt_flag & MNT_ROOTFS))
			return (EINVAL);
		flags |= FORCECLOSE;
	}
	nmp = VFSTONFS(mp);
	/*
	 * Goes something like this..
	 * - Check for activity on the root vnode (other than ourselves).
	 * - Call vflush() to clear out vnodes for this file system,
	 *   except for the root vnode.
	 * - Decrement reference on the vnode representing remote root.
	 * - Close the socket
	 * - Free up the data structures
	 */
	/*
	 * We need to decrement the ref. count on the nfsnode representing
	 * the remote root.  See comment in mountnfs().  The VFS unmount()
	 * has done vput on this vnode, otherwise we would get deadlock!
	 */
	if (error = nfs_nget(mp, &nmp->nm_fh, &np))
		return(error);
	vp = NFSTOV(np);
	if (vp->v_usecount > 2) {
		vput(vp);
		return (EBUSY);
	}

	/*
	 * Must handshake with nqnfs_clientd() if it is active.
	 */
	nmp->nm_flag |= NFSMNT_DISMINPROG;
	while (nmp->nm_inprog != NULLVP)
		(void) tsleep((caddr_t)&lbolt, PSOCK, "nfsdism", 0);
	if (error = vflush(mp, vp, flags)) {
		vput(vp);
		nmp->nm_flag &= ~NFSMNT_DISMINPROG;
		return (error);
	}

	/*
	 * We are now committed to the unmount.
	 * For NQNFS, let the server daemon free the nfsmount structure.
	 */
	if (nmp->nm_flag & (NFSMNT_NQNFS | NFSMNT_KERB))
		nmp->nm_flag |= NFSMNT_DISMNT;

	/*
	 * There are two reference counts to get rid of here.
	 */
	vrele(vp);
	vrele(vp);
	vgone(vp);
	nfs_disconnect(nmp);
	m_freem(nmp->nm_nam);

	if ((nmp->nm_flag & (NFSMNT_NQNFS | NFSMNT_KERB)) == 0)
		free((caddr_t)nmp, M_NFSMNT);
	return (0);
}

/*
 * Return root of a filesystem
 */
int
nfs_root(mp, vpp)
	struct mount *mp;
	struct vnode **vpp;
{
	register struct vnode *vp;
	struct nfsmount *nmp;
	struct nfsnode *np;
	int error;

	nmp = VFSTONFS(mp);
	if (error = nfs_nget(mp, &nmp->nm_fh, &np))
		return (error);
	vp = NFSTOV(np);
	vp->v_type = VDIR;
	vp->v_flag = VROOT;
	*vpp = vp;
	return (0);
}

extern int syncprt;

/*
 * Flush out the buffer cache
 */
/* ARGSUSED */
int
nfs_sync(mp, waitfor, cred, p)
	struct mount *mp;
	int waitfor;
	struct ucred *cred;
	struct proc *p;
{
	register struct vnode *vp;
	int error, allerror = 0;

	/*
	 * Force stale buffer cache information to be flushed.
	 */
loop:
	for (vp = mp->mnt_vnodelist.lh_first;
	     vp != NULL;
	     vp = vp->v_mntvnodes.le_next) {
		/*
		 * If the vnode that we are about to sync is no longer
		 * associated with this mount point, start over.
		 */
		if (vp->v_mount != mp)
			goto loop;
		if (VOP_ISLOCKED(vp) || vp->v_dirtyblkhd.lh_first == NULL)
			continue;
		if (vget(vp, 1))
			goto loop;
		if (error = VOP_FSYNC(vp, cred, waitfor, p))
			allerror = error;
		vput(vp);
	}
	return (allerror);
}

/*
 * NFS flat namespace lookup.
 * Currently unsupported.
 */
/* ARGSUSED */
int
nfs_vget(mp, ino, vpp)
	struct mount *mp;
	ino_t ino;
	struct vnode **vpp;
{

	return (EOPNOTSUPP);
}

/*
 * At this point, this should never happen
 */
/* ARGSUSED */
int
nfs_fhtovp(mp, fhp, nam, vpp, exflagsp, credanonp)
	register struct mount *mp;
	struct fid *fhp;
	struct mbuf *nam;
	struct vnode **vpp;
	int *exflagsp;
	struct ucred **credanonp;
{

	return (EINVAL);
}

/*
 * Vnode pointer to File handle, should never happen either
 */
/* ARGSUSED */
int
nfs_vptofh(vp, fhp)
	struct vnode *vp;
	struct fid *fhp;
{

	return (EINVAL);
}

/*
 * Vfs start routine, a no-op.
 */
/* ARGSUSED */
int
nfs_start(mp, flags, p)
	struct mount *mp;
	int flags;
	struct proc *p;
{

	return (0);
}

/*
 * Do operations associated with quotas, not supported
 */
/* ARGSUSED */
int
nfs_quotactl(mp, cmd, uid, arg, p)
	struct mount *mp;
	int cmd;
	uid_t uid;
	caddr_t arg;
	struct proc *p;
{

	return (EOPNOTSUPP);
}
