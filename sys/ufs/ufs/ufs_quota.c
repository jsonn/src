/*	$NetBSD: ufs_quota.c,v 1.38.4.1 2006/03/08 01:39:12 elad Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1990, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Robert Elz at The University of Melbourne.
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
 *	@(#)ufs_quota.c	8.5 (Berkeley) 5/20/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ufs_quota.c,v 1.38.4.1 2006/03/08 01:39:12 elad Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/mount.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

/*
 * Quota name to error message mapping.
 */
static const char *quotatypes[] = INITQFNAMES;

/*
 * Set up the quotas for an inode.
 *
 * This routine completely defines the semantics of quotas.
 * If other criterion want to be used to establish quotas, the
 * MAXQUOTAS value in quotas.h should be increased, and the
 * additional dquots set up here.
 */
int
getinoquota(struct inode *ip)
{
	struct ufsmount *ump = ip->i_ump;
	struct vnode *vp = ITOV(ip);
	int error;

	/*
	 * Set up the user quota based on file uid.
	 * EINVAL means that quotas are not enabled.
	 */
	if (ip->i_dquot[USRQUOTA] == NODQUOT &&
	    (error =
		dqget(vp, ip->i_uid, ump, USRQUOTA, &ip->i_dquot[USRQUOTA])) &&
	    error != EINVAL)
		return (error);
	/*
	 * Set up the group quota based on file gid.
	 * EINVAL means that quotas are not enabled.
	 */
	if (ip->i_dquot[GRPQUOTA] == NODQUOT &&
	    (error =
		dqget(vp, ip->i_gid, ump, GRPQUOTA, &ip->i_dquot[GRPQUOTA])) &&
	    error != EINVAL)
		return (error);
	return (0);
}

/*
 * Update disk usage, and take corrective action.
 */
int
chkdq(struct inode *ip, int64_t change, kauth_cred_t cred, int flags)
{
	struct dquot *dq;
	int i;
	int ncurblocks, error;

#ifdef DIAGNOSTIC
	if ((flags & CHOWN) == 0)
		chkdquot(ip);
#endif
	if (change == 0)
		return (0);
	if (change < 0) {
		for (i = 0; i < MAXQUOTAS; i++) {
			if ((dq = ip->i_dquot[i]) == NODQUOT)
				continue;
			while (dq->dq_flags & DQ_LOCK) {
				dq->dq_flags |= DQ_WANT;
				(void) tsleep(dq, PINOD+1, "chkdq", 0);
			}
			ncurblocks = dq->dq_curblocks + change;
			if (ncurblocks >= 0)
				dq->dq_curblocks = ncurblocks;
			else
				dq->dq_curblocks = 0;
			dq->dq_flags &= ~DQ_BLKS;
			dq->dq_flags |= DQ_MOD;
		}
		return (0);
	}
	if ((flags & FORCE) == 0 &&
	    (cred != NOCRED && kauth_cred_geteuid(cred) != 0)) {
		for (i = 0; i < MAXQUOTAS; i++) {
			if ((dq = ip->i_dquot[i]) == NODQUOT)
				continue;
			if ((error = chkdqchg(ip, change, cred, i)) != 0)
				return (error);
		}
	}
	for (i = 0; i < MAXQUOTAS; i++) {
		if ((dq = ip->i_dquot[i]) == NODQUOT)
			continue;
		while (dq->dq_flags & DQ_LOCK) {
			dq->dq_flags |= DQ_WANT;
			(void) tsleep(dq, PINOD+1, "chkdq", 0);
		}
		dq->dq_curblocks += change;
		dq->dq_flags |= DQ_MOD;
	}
	return (0);
}

/*
 * Check for a valid change to a users allocation.
 * Issue an error message if appropriate.
 */
int
chkdqchg(struct inode *ip, int64_t change, kauth_cred_t cred, int type)
{
	struct dquot *dq = ip->i_dquot[type];
	long ncurblocks = dq->dq_curblocks + change;

	/*
	 * If user would exceed their hard limit, disallow space allocation.
	 */
	if (ncurblocks >= dq->dq_bhardlimit && dq->dq_bhardlimit) {
		if ((dq->dq_flags & DQ_BLKS) == 0 &&
		    ip->i_uid == kauth_cred_geteuid(cred)) {
			uprintf("\n%s: write failed, %s disk limit reached\n",
			    ITOV(ip)->v_mount->mnt_stat.f_mntonname,
			    quotatypes[type]);
			dq->dq_flags |= DQ_BLKS;
		}
		return (EDQUOT);
	}
	/*
	 * If user is over their soft limit for too long, disallow space
	 * allocation. Reset time limit as they cross their soft limit.
	 */
	if (ncurblocks >= dq->dq_bsoftlimit && dq->dq_bsoftlimit) {
		if (dq->dq_curblocks < dq->dq_bsoftlimit) {
			dq->dq_btime = time.tv_sec + ip->i_ump->um_btime[type];
			if (ip->i_uid == kauth_cred_geteuid(cred))
				uprintf("\n%s: warning, %s %s\n",
				    ITOV(ip)->v_mount->mnt_stat.f_mntonname,
				    quotatypes[type], "disk quota exceeded");
			return (0);
		}
		if (time.tv_sec > dq->dq_btime) {
			if ((dq->dq_flags & DQ_BLKS) == 0 &&
			    ip->i_uid == kauth_cred_geteuid(cred)) {
				uprintf("\n%s: write failed, %s %s\n",
				    ITOV(ip)->v_mount->mnt_stat.f_mntonname,
				    quotatypes[type],
				    "disk quota exceeded for too long");
				dq->dq_flags |= DQ_BLKS;
			}
			return (EDQUOT);
		}
	}
	return (0);
}

/*
 * Check the inode limit, applying corrective action.
 */
int
chkiq(struct inode *ip, int32_t change, kauth_cred_t cred, int flags)
{
	struct dquot *dq;
	int i;
	int ncurinodes, error;

#ifdef DIAGNOSTIC
	if ((flags & CHOWN) == 0)
		chkdquot(ip);
#endif
	if (change == 0)
		return (0);
	if (change < 0) {
		for (i = 0; i < MAXQUOTAS; i++) {
			if ((dq = ip->i_dquot[i]) == NODQUOT)
				continue;
			while (dq->dq_flags & DQ_LOCK) {
				dq->dq_flags |= DQ_WANT;
				(void) tsleep(dq, PINOD+1, "chkiq", 0);
			}
			ncurinodes = dq->dq_curinodes + change;
			if (ncurinodes >= 0)
				dq->dq_curinodes = ncurinodes;
			else
				dq->dq_curinodes = 0;
			dq->dq_flags &= ~DQ_INODS;
			dq->dq_flags |= DQ_MOD;
		}
		return (0);
	}
	if ((flags & FORCE) == 0 && kauth_cred_geteuid(cred) != 0) {
		for (i = 0; i < MAXQUOTAS; i++) {
			if ((dq = ip->i_dquot[i]) == NODQUOT)
				continue;
			if ((error = chkiqchg(ip, change, cred, i)) != 0)
				return (error);
		}
	}
	for (i = 0; i < MAXQUOTAS; i++) {
		if ((dq = ip->i_dquot[i]) == NODQUOT)
			continue;
		while (dq->dq_flags & DQ_LOCK) {
			dq->dq_flags |= DQ_WANT;
			(void) tsleep(dq, PINOD+1, "chkiq", 0);
		}
		dq->dq_curinodes += change;
		dq->dq_flags |= DQ_MOD;
	}
	return (0);
}

/*
 * Check for a valid change to a users allocation.
 * Issue an error message if appropriate.
 */
int
chkiqchg(struct inode *ip, int32_t change, kauth_cred_t cred, int type)
{
	struct dquot *dq = ip->i_dquot[type];
	long ncurinodes = dq->dq_curinodes + change;

	/*
	 * If user would exceed their hard limit, disallow inode allocation.
	 */
	if (ncurinodes >= dq->dq_ihardlimit && dq->dq_ihardlimit) {
		if ((dq->dq_flags & DQ_INODS) == 0 &&
		    ip->i_uid == kauth_cred_geteuid(cred)) {
			uprintf("\n%s: write failed, %s inode limit reached\n",
			    ITOV(ip)->v_mount->mnt_stat.f_mntonname,
			    quotatypes[type]);
			dq->dq_flags |= DQ_INODS;
		}
		return (EDQUOT);
	}
	/*
	 * If user is over their soft limit for too long, disallow inode
	 * allocation. Reset time limit as they cross their soft limit.
	 */
	if (ncurinodes >= dq->dq_isoftlimit && dq->dq_isoftlimit) {
		if (dq->dq_curinodes < dq->dq_isoftlimit) {
			dq->dq_itime = time.tv_sec + ip->i_ump->um_itime[type];
			if (ip->i_uid == kauth_cred_geteuid(cred))
				uprintf("\n%s: warning, %s %s\n",
				    ITOV(ip)->v_mount->mnt_stat.f_mntonname,
				    quotatypes[type], "inode quota exceeded");
			return (0);
		}
		if (time.tv_sec > dq->dq_itime) {
			if ((dq->dq_flags & DQ_INODS) == 0 &&
			    ip->i_uid == kauth_cred_geteuid(cred)) {
				uprintf("\n%s: write failed, %s %s\n",
				    ITOV(ip)->v_mount->mnt_stat.f_mntonname,
				    quotatypes[type],
				    "inode quota exceeded for too long");
				dq->dq_flags |= DQ_INODS;
			}
			return (EDQUOT);
		}
	}
	return (0);
}

#ifdef DIAGNOSTIC
/*
 * On filesystems with quotas enabled, it is an error for a file to change
 * size and not to have a dquot structure associated with it.
 */
void
chkdquot(struct inode *ip)
{
	struct ufsmount *ump = ip->i_ump;
	int i;

	for (i = 0; i < MAXQUOTAS; i++) {
		if (ump->um_quotas[i] == NULLVP ||
		    (ump->um_qflags[i] & (QTF_OPENING|QTF_CLOSING)))
			continue;
		if (ip->i_dquot[i] == NODQUOT) {
			vprint("chkdquot: missing dquot", ITOV(ip));
			panic("missing dquot");
		}
	}
}
#endif

/*
 * Code to process quotactl commands.
 */

/*
 * Q_QUOTAON - set up a quota file for a particular file system.
 */
int
quotaon(struct lwp *l, struct mount *mp, int type, caddr_t fname)
{
	struct ufsmount *ump = VFSTOUFS(mp);
	struct vnode *vp, **vpp;
	struct vnode *nextvp;
	struct dquot *dq;
	struct proc *p;
	int error;
	struct nameidata nd;

	p = l->l_proc;
	vpp = &ump->um_quotas[type];
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, fname, l);
	if ((error = vn_open(&nd, FREAD|FWRITE, 0)) != 0)
		return (error);
	vp = nd.ni_vp;
	VOP_UNLOCK(vp, 0);
	if (vp->v_type != VREG) {
		(void) vn_close(vp, FREAD|FWRITE, p->p_cred, l);
		return (EACCES);
	}
	if (*vpp != vp)
		quotaoff(l, mp, type);
	ump->um_qflags[type] |= QTF_OPENING;
	mp->mnt_flag |= MNT_QUOTA;
	vp->v_flag |= VSYSTEM;
	*vpp = vp;
	/*
	 * Save the credential of the process that turned on quotas.
	 * Set up the time limits for this quota.
	 */
	kauth_cred_hold(p->p_cred);
	ump->um_cred[type] = p->p_cred;
	ump->um_btime[type] = MAX_DQ_TIME;
	ump->um_itime[type] = MAX_IQ_TIME;
	if (dqget(NULLVP, 0, ump, type, &dq) == 0) {
		if (dq->dq_btime > 0)
			ump->um_btime[type] = dq->dq_btime;
		if (dq->dq_itime > 0)
			ump->um_itime[type] = dq->dq_itime;
		dqrele(NULLVP, dq);
	}
	/*
	 * Search vnodes associated with this mount point,
	 * adding references to quota file being opened.
	 * NB: only need to add dquot's for inodes being modified.
	 */
again:
	for (vp = LIST_FIRST(&mp->mnt_vnodelist); vp != NULL; vp = nextvp) {
		nextvp = LIST_NEXT(vp, v_mntvnodes);
		if (vp->v_type == VNON ||vp->v_writecount == 0)
			continue;
		if (vget(vp, LK_EXCLUSIVE))
			goto again;
		if ((error = getinoquota(VTOI(vp))) != 0) {
			vput(vp);
			break;
		}
		vput(vp);
		if (LIST_NEXT(vp, v_mntvnodes) != nextvp || vp->v_mount != mp)
			goto again;
	}
	ump->um_qflags[type] &= ~QTF_OPENING;
	if (error)
		quotaoff(l, mp, type);
	return (error);
}

/*
 * Q_QUOTAOFF - turn off disk quotas for a filesystem.
 */
int
quotaoff(struct lwp *l, struct mount *mp, int type)
{
	struct vnode *vp;
	struct vnode *qvp, *nextvp;
	struct ufsmount *ump = VFSTOUFS(mp);
	struct dquot *dq;
	struct inode *ip;
	int error;

	if ((qvp = ump->um_quotas[type]) == NULLVP)
		return (0);
	ump->um_qflags[type] |= QTF_CLOSING;
	/*
	 * Search vnodes associated with this mount point,
	 * deleting any references to quota file being closed.
	 */
again:
	for (vp = LIST_FIRST(&mp->mnt_vnodelist); vp != NULL; vp = nextvp) {
		nextvp = LIST_NEXT(vp, v_mntvnodes);
		if (vp->v_type == VNON)
			continue;
		if (vget(vp, LK_EXCLUSIVE))
			goto again;
		ip = VTOI(vp);
		dq = ip->i_dquot[type];
		ip->i_dquot[type] = NODQUOT;
		dqrele(vp, dq);
		vput(vp);
		if (LIST_NEXT(vp, v_mntvnodes) != nextvp || vp->v_mount != mp)
			goto again;
	}
	dqflush(qvp);
	qvp->v_flag &= ~VSYSTEM;
	error = vn_close(qvp, FREAD|FWRITE, l->l_proc->p_cred, l);
	ump->um_quotas[type] = NULLVP;
	kauth_cred_free(ump->um_cred[type]);
	ump->um_cred[type] = NOCRED;
	ump->um_qflags[type] &= ~QTF_CLOSING;
	for (type = 0; type < MAXQUOTAS; type++)
		if (ump->um_quotas[type] != NULLVP)
			break;
	if (type == MAXQUOTAS)
		mp->mnt_flag &= ~MNT_QUOTA;
	return (error);
}

/*
 * Q_GETQUOTA - return current values in a dqblk structure.
 */
int
getquota(struct mount *mp, u_long id, int type, caddr_t addr)
{
	struct dquot *dq;
	int error;

	if ((error = dqget(NULLVP, id, VFSTOUFS(mp), type, &dq)) != 0)
		return (error);
	error = copyout((caddr_t)&dq->dq_dqb, addr, sizeof (struct dqblk));
	dqrele(NULLVP, dq);
	return (error);
}

/*
 * Q_SETQUOTA - assign an entire dqblk structure.
 */
int
setquota(struct mount *mp, u_long id, int type, caddr_t addr)
{
	struct dquot *dq;
	struct dquot *ndq;
	struct ufsmount *ump = VFSTOUFS(mp);
	struct dqblk newlim;
	int error;

	error = copyin(addr, (caddr_t)&newlim, sizeof (struct dqblk));
	if (error)
		return (error);
	if ((error = dqget(NULLVP, id, ump, type, &ndq)) != 0)
		return (error);
	dq = ndq;
	while (dq->dq_flags & DQ_LOCK) {
		dq->dq_flags |= DQ_WANT;
		(void) tsleep(dq, PINOD+1, "setquota", 0);
	}
	/*
	 * Copy all but the current values.
	 * Reset time limit if previously had no soft limit or were
	 * under it, but now have a soft limit and are over it.
	 */
	newlim.dqb_curblocks = dq->dq_curblocks;
	newlim.dqb_curinodes = dq->dq_curinodes;
	if (dq->dq_id != 0) {
		newlim.dqb_btime = dq->dq_btime;
		newlim.dqb_itime = dq->dq_itime;
	}
	if (newlim.dqb_bsoftlimit &&
	    dq->dq_curblocks >= newlim.dqb_bsoftlimit &&
	    (dq->dq_bsoftlimit == 0 || dq->dq_curblocks < dq->dq_bsoftlimit))
		newlim.dqb_btime = time.tv_sec + ump->um_btime[type];
	if (newlim.dqb_isoftlimit &&
	    dq->dq_curinodes >= newlim.dqb_isoftlimit &&
	    (dq->dq_isoftlimit == 0 || dq->dq_curinodes < dq->dq_isoftlimit))
		newlim.dqb_itime = time.tv_sec + ump->um_itime[type];
	dq->dq_dqb = newlim;
	if (dq->dq_curblocks < dq->dq_bsoftlimit)
		dq->dq_flags &= ~DQ_BLKS;
	if (dq->dq_curinodes < dq->dq_isoftlimit)
		dq->dq_flags &= ~DQ_INODS;
	if (dq->dq_isoftlimit == 0 && dq->dq_bsoftlimit == 0 &&
	    dq->dq_ihardlimit == 0 && dq->dq_bhardlimit == 0)
		dq->dq_flags |= DQ_FAKE;
	else
		dq->dq_flags &= ~DQ_FAKE;
	dq->dq_flags |= DQ_MOD;
	dqrele(NULLVP, dq);
	return (0);
}

/*
 * Q_SETUSE - set current inode and block usage.
 */
int
setuse(struct mount *mp, u_long id, int type, caddr_t addr)
{
	struct dquot *dq;
	struct ufsmount *ump = VFSTOUFS(mp);
	struct dquot *ndq;
	struct dqblk usage;
	int error;

	error = copyin(addr, (caddr_t)&usage, sizeof (struct dqblk));
	if (error)
		return (error);
	if ((error = dqget(NULLVP, id, ump, type, &ndq)) != 0)
		return (error);
	dq = ndq;
	while (dq->dq_flags & DQ_LOCK) {
		dq->dq_flags |= DQ_WANT;
		(void) tsleep(dq, PINOD+1, "setuse", 0);
	}
	/*
	 * Reset time limit if have a soft limit and were
	 * previously under it, but are now over it.
	 */
	if (dq->dq_bsoftlimit && dq->dq_curblocks < dq->dq_bsoftlimit &&
	    usage.dqb_curblocks >= dq->dq_bsoftlimit)
		dq->dq_btime = time.tv_sec + ump->um_btime[type];
	if (dq->dq_isoftlimit && dq->dq_curinodes < dq->dq_isoftlimit &&
	    usage.dqb_curinodes >= dq->dq_isoftlimit)
		dq->dq_itime = time.tv_sec + ump->um_itime[type];
	dq->dq_curblocks = usage.dqb_curblocks;
	dq->dq_curinodes = usage.dqb_curinodes;
	if (dq->dq_curblocks < dq->dq_bsoftlimit)
		dq->dq_flags &= ~DQ_BLKS;
	if (dq->dq_curinodes < dq->dq_isoftlimit)
		dq->dq_flags &= ~DQ_INODS;
	dq->dq_flags |= DQ_MOD;
	dqrele(NULLVP, dq);
	return (0);
}

/*
 * Q_SYNC - sync quota files to disk.
 */
int
qsync(struct mount *mp)
{
	struct ufsmount *ump = VFSTOUFS(mp);
	struct vnode *vp, *nextvp;
	struct dquot *dq;
	int i, error;

	/*
	 * Check if the mount point has any quotas.
	 * If not, simply return.
	 */
	for (i = 0; i < MAXQUOTAS; i++)
		if (ump->um_quotas[i] != NULLVP)
			break;
	if (i == MAXQUOTAS)
		return (0);
	/*
	 * Search vnodes associated with this mount point,
	 * synchronizing any modified dquot structures.
	 */
	simple_lock(&mntvnode_slock);
again:
	for (vp = LIST_FIRST(&mp->mnt_vnodelist); vp != NULL; vp = nextvp) {
		if (vp->v_mount != mp)
			goto again;
		nextvp = LIST_NEXT(vp, v_mntvnodes);
		if (vp->v_type == VNON)
			continue;
		simple_lock(&vp->v_interlock);
		simple_unlock(&mntvnode_slock);
		error = vget(vp, LK_EXCLUSIVE | LK_NOWAIT | LK_INTERLOCK);
		if (error) {
			simple_lock(&mntvnode_slock);
			if (error == ENOENT)
				goto again;
			continue;
		}
		for (i = 0; i < MAXQUOTAS; i++) {
			dq = VTOI(vp)->i_dquot[i];
			if (dq != NODQUOT && (dq->dq_flags & DQ_MOD))
				dqsync(vp, dq);
		}
		vput(vp);
		simple_lock(&mntvnode_slock);
		if (LIST_NEXT(vp, v_mntvnodes) != nextvp)
			goto again;
	}
	simple_unlock(&mntvnode_slock);
	return (0);
}

/*
 * Code pertaining to management of the in-core dquot data structures.
 */
#define DQHASH(dqvp, id) \
	(((((long)(dqvp)) >> 8) + id) & dqhash)
static LIST_HEAD(dqhashhead, dquot) *dqhashtbl;
static u_long dqhash;

/*
 * Dquot free list.
 */
#define	DQUOTINC	5	/* minimum free dquots desired */
static TAILQ_HEAD(dqfreelist, dquot) dqfreelist;
static long numdquot, desireddquot = DQUOTINC;

MALLOC_DEFINE(M_DQUOT, "UFS quota", "UFS quota entries");

/*
 * Initialize the quota system.
 */
void
dqinit(void)
{
	dqhashtbl =
	    hashinit(desiredvnodes, HASH_LIST, M_DQUOT, M_WAITOK, &dqhash);
	TAILQ_INIT(&dqfreelist);
}

void
dqreinit(void)
{
	struct dquot *dq;
	struct dqhashhead *oldhash, *hash;
	struct vnode *dqvp;
	u_long oldmask, mask, hashval;
	int i;

	hash = hashinit(desiredvnodes, HASH_LIST, M_DQUOT, M_WAITOK, &mask);
	oldhash = dqhashtbl;
	oldmask = dqhash;
	dqhashtbl = hash;
	dqhash = mask;
	for (i = 0; i <= oldmask; i++) {
		while ((dq = LIST_FIRST(&oldhash[i])) != NULL) {
			dqvp = dq->dq_ump->um_quotas[dq->dq_type];
			LIST_REMOVE(dq, dq_hash);
			hashval = DQHASH(dqvp, dq->dq_id);
			LIST_INSERT_HEAD(&dqhashtbl[hashval], dq, dq_hash);
		}
	}
	hashdone(oldhash, M_DQUOT);
}

/*
 * Free resources held by quota system.
 */
void
dqdone(void)
{
	hashdone(dqhashtbl, M_DQUOT);
}

/*
 * Obtain a dquot structure for the specified identifier and quota file
 * reading the information from the file if necessary.
 */
int
dqget(struct vnode *vp, u_long id, struct ufsmount *ump, int type,
    struct dquot **dqp)
{
	struct dquot *dq;
	struct dqhashhead *dqh;
	struct vnode *dqvp;
	struct iovec aiov;
	struct uio auio;
	int error;

	dqvp = ump->um_quotas[type];
	if (dqvp == NULLVP || (ump->um_qflags[type] & QTF_CLOSING)) {
		*dqp = NODQUOT;
		return (EINVAL);
	}
	/*
	 * Check the cache first.
	 */
	dqh = &dqhashtbl[DQHASH(dqvp, id)];
	LIST_FOREACH(dq, dqh, dq_hash) {
		if (dq->dq_id != id ||
		    dq->dq_ump->um_quotas[dq->dq_type] != dqvp)
			continue;
		/*
		 * Cache hit with no references.  Take
		 * the structure off the free list.
		 */
		if (dq->dq_cnt == 0)
			TAILQ_REMOVE(&dqfreelist, dq, dq_freelist);
		dqref(dq);
		*dqp = dq;
		return (0);
	}
	/*
	 * Not in cache, allocate a new one.
	 */
	if (dqfreelist.tqh_first == NODQUOT &&
	    numdquot < MAXQUOTAS * desiredvnodes)
		desireddquot += DQUOTINC;
	if (numdquot < desireddquot) {
		dq = (struct dquot *)malloc(sizeof *dq, M_DQUOT, M_WAITOK);
		memset((char *)dq, 0, sizeof *dq);
		numdquot++;
	} else {
		if ((dq = dqfreelist.tqh_first) == NULL) {
			tablefull("dquot",
			    "increase kern.maxvnodes or NVNODE");
			*dqp = NODQUOT;
			return (EUSERS);
		}
		if (dq->dq_cnt || (dq->dq_flags & DQ_MOD))
			panic("free dquot isn't");
		TAILQ_REMOVE(&dqfreelist, dq, dq_freelist);
		LIST_REMOVE(dq, dq_hash);
	}
	/*
	 * Initialize the contents of the dquot structure.
	 */
	if (vp != dqvp)
		vn_lock(dqvp, LK_EXCLUSIVE | LK_RETRY);
	LIST_INSERT_HEAD(dqh, dq, dq_hash);
	dqref(dq);
	dq->dq_flags = DQ_LOCK;
	dq->dq_id = id;
	dq->dq_ump = ump;
	dq->dq_type = type;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	aiov.iov_base = (caddr_t)&dq->dq_dqb;
	aiov.iov_len = sizeof (struct dqblk);
	auio.uio_resid = sizeof (struct dqblk);
	auio.uio_offset = (off_t)(id * sizeof (struct dqblk));
	auio.uio_rw = UIO_READ;
	UIO_SETUP_SYSSPACE(&auio);
	error = VOP_READ(dqvp, &auio, 0, ump->um_cred[type]);
	if (auio.uio_resid == sizeof(struct dqblk) && error == 0)
		memset((caddr_t)&dq->dq_dqb, 0, sizeof(struct dqblk));
	if (vp != dqvp)
		VOP_UNLOCK(dqvp, 0);
	if (dq->dq_flags & DQ_WANT)
		wakeup((caddr_t)dq);
	dq->dq_flags = 0;
	/*
	 * I/O error in reading quota file, release
	 * quota structure and reflect problem to caller.
	 */
	if (error) {
		LIST_REMOVE(dq, dq_hash);
		dqrele(vp, dq);
		*dqp = NODQUOT;
		return (error);
	}
	/*
	 * Check for no limit to enforce.
	 * Initialize time values if necessary.
	 */
	if (dq->dq_isoftlimit == 0 && dq->dq_bsoftlimit == 0 &&
	    dq->dq_ihardlimit == 0 && dq->dq_bhardlimit == 0)
		dq->dq_flags |= DQ_FAKE;
	if (dq->dq_id != 0) {
		if (dq->dq_btime == 0)
			dq->dq_btime = time.tv_sec + ump->um_btime[type];
		if (dq->dq_itime == 0)
			dq->dq_itime = time.tv_sec + ump->um_itime[type];
	}
	*dqp = dq;
	return (0);
}

/*
 * Obtain a reference to a dquot.
 */
void
dqref(struct dquot *dq)
{

	dq->dq_cnt++;
}

/*
 * Release a reference to a dquot.
 */
void
dqrele(struct vnode *vp, struct dquot *dq)
{

	if (dq == NODQUOT)
		return;
	if (dq->dq_cnt > 1) {
		dq->dq_cnt--;
		return;
	}
	if (dq->dq_flags & DQ_MOD)
		(void) dqsync(vp, dq);
	if (--dq->dq_cnt > 0)
		return;
	TAILQ_INSERT_TAIL(&dqfreelist, dq, dq_freelist);
}

/*
 * Update the disk quota in the quota file.
 */
int
dqsync(struct vnode *vp, struct dquot *dq)
{
	struct vnode *dqvp;
	struct mount *mp;
	struct iovec aiov;
	struct uio auio;
	int error;

	if (dq == NODQUOT)
		panic("dqsync: dquot");
	if ((dq->dq_flags & DQ_MOD) == 0)
		return (0);
	if ((dqvp = dq->dq_ump->um_quotas[dq->dq_type]) == NULLVP)
		panic("dqsync: file");
	vn_start_write(dqvp, &mp, V_WAIT | V_LOWER);
	if (vp != dqvp)
		vn_lock(dqvp, LK_EXCLUSIVE | LK_RETRY);
	while (dq->dq_flags & DQ_LOCK) {
		dq->dq_flags |= DQ_WANT;
		(void) tsleep(dq, PINOD+2, "dqsync", 0);
		if ((dq->dq_flags & DQ_MOD) == 0) {
			if (vp != dqvp)
				VOP_UNLOCK(dqvp, 0);
			vn_finished_write(mp, V_LOWER);
			return (0);
		}
	}
	dq->dq_flags |= DQ_LOCK;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	aiov.iov_base = (caddr_t)&dq->dq_dqb;
	aiov.iov_len = sizeof (struct dqblk);
	auio.uio_resid = sizeof (struct dqblk);
	auio.uio_offset = (off_t)(dq->dq_id * sizeof (struct dqblk));
	auio.uio_rw = UIO_WRITE;
	UIO_SETUP_SYSSPACE(&auio);
	error = VOP_WRITE(dqvp, &auio, 0, dq->dq_ump->um_cred[dq->dq_type]);
	if (auio.uio_resid && error == 0)
		error = EIO;
	if (dq->dq_flags & DQ_WANT)
		wakeup((caddr_t)dq);
	dq->dq_flags &= ~(DQ_MOD|DQ_LOCK|DQ_WANT);
	if (vp != dqvp)
		VOP_UNLOCK(dqvp, 0);
	vn_finished_write(mp, V_LOWER);
	return (error);
}

/*
 * Flush all entries from the cache for a particular vnode.
 */
void
dqflush(struct vnode *vp)
{
	struct dquot *dq, *nextdq;
	struct dqhashhead *dqh;

	/*
	 * Move all dquot's that used to refer to this quota
	 * file off their hash chains (they will eventually
	 * fall off the head of the free list and be re-used).
	 */
	for (dqh = &dqhashtbl[dqhash]; dqh >= dqhashtbl; dqh--) {
		for (dq = LIST_FIRST(dqh); dq; dq = nextdq) {
			nextdq = LIST_NEXT(dq, dq_hash);
			if (dq->dq_ump->um_quotas[dq->dq_type] != vp)
				continue;
			if (dq->dq_cnt)
				panic("dqflush: stray dquot");
			LIST_REMOVE(dq, dq_hash);
			dq->dq_ump = NULL;
		}
	}
}
