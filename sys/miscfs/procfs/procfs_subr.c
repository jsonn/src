/*	$NetBSD: procfs_subr.c,v 1.38.2.1 2001/11/12 21:19:12 thorpej Exp $	*/

/*
 * Copyright (c) 1994 Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1993 Jan-Simon Pendry
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
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
 *	@(#)procfs_subr.c	8.6 (Berkeley) 5/14/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: procfs_subr.c,v 1.38.2.1 2001/11/12 21:19:12 thorpej Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/stat.h>

#include <miscfs/procfs/procfs.h>

void procfs_hashins __P((struct pfsnode *));
void procfs_hashrem __P((struct pfsnode *));
struct vnode *procfs_hashget __P((pid_t, pfstype, struct mount *));

LIST_HEAD(pfs_hashhead, pfsnode) *pfs_hashtbl;
u_long	pfs_ihash;	/* size of hash table - 1 */
#define PFSPIDHASH(pid)	((pid) & pfs_ihash)

struct lock pfs_hashlock;
struct simplelock pfs_hash_slock;

#define	ISSET(t, f)	((t) & (f))

/*
 * allocate a pfsnode/vnode pair.  the vnode is
 * referenced, and locked.
 *
 * the pid, pfs_type, and mount point uniquely
 * identify a pfsnode.  the mount point is needed
 * because someone might mount this filesystem
 * twice.
 *
 * all pfsnodes are maintained on a singly-linked
 * list.  new nodes are only allocated when they cannot
 * be found on this list.  entries on the list are
 * removed when the vfs reclaim entry is called.
 *
 * a single lock is kept for the entire list.  this is
 * needed because the getnewvnode() function can block
 * waiting for a vnode to become free, in which case there
 * may be more than one process trying to get the same
 * vnode.  this lock is only taken if we are going to
 * call getnewvnode, since the kernel itself is single-threaded.
 *
 * if an entry is found on the list, then call vget() to
 * take a reference.  this is done because there may be
 * zero references to it and so it needs to removed from
 * the vnode free list.
 */
int
procfs_allocvp(mp, vpp, pid, pfs_type)
	struct mount *mp;
	struct vnode **vpp;
	long pid;
	pfstype pfs_type;
{
	struct pfsnode *pfs;
	struct vnode *vp;
	int error;

	do {
		if ((*vpp = procfs_hashget(pid, pfs_type, mp)) != NULL)
			return (0);
	} while (lockmgr(&pfs_hashlock, LK_EXCLUSIVE|LK_SLEEPFAIL, 0));

	if ((error = getnewvnode(VT_PROCFS, mp, procfs_vnodeop_p, vpp)) != 0) {
		*vpp = NULL;
		lockmgr(&pfs_hashlock, LK_RELEASE, NULL);
		return (error);
	}
	vp = *vpp;

	MALLOC(pfs, void *, sizeof(struct pfsnode), M_TEMP, M_WAITOK);
	vp->v_data = pfs;

	pfs->pfs_pid = (pid_t) pid;
	pfs->pfs_type = pfs_type;
	pfs->pfs_vnode = vp;
	pfs->pfs_flags = 0;
	pfs->pfs_fileno = PROCFS_FILENO(pid, pfs_type);

	switch (pfs_type) {
	case Proot:	/* /proc = dr-xr-xr-x */
		pfs->pfs_mode = S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH;
		vp->v_type = VDIR;
		vp->v_flag = VROOT;
		break;

	case Pcurproc:	/* /proc/curproc = lr-xr-xr-x */
	case Pself:	/* /proc/self    = lr-xr-xr-x */
		pfs->pfs_mode = S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH;
		vp->v_type = VLNK;
		break;

	case Pproc:	/* /proc/N = dr-xr-xr-x */
		pfs->pfs_mode = S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH;
		vp->v_type = VDIR;
		break;

	case Pfile:	/* /proc/N/file = -rw------- */
	case Pmem:	/* /proc/N/mem = -rw------- */
	case Pregs:	/* /proc/N/regs = -rw------- */
	case Pfpregs:	/* /proc/N/fpregs = -rw------- */
		pfs->pfs_mode = S_IRUSR|S_IWUSR;
		vp->v_type = VREG;
		break;

	case Pctl:	/* /proc/N/ctl = --w------ */
	case Pnote:	/* /proc/N/note = --w------ */
	case Pnotepg:	/* /proc/N/notepg = --w------ */
		pfs->pfs_mode = S_IWUSR;
		vp->v_type = VREG;
		break;

	case Pmap:	/* /proc/N/map = -r--r--r-- */
	case Pmaps:	/* /proc/N/maps = -r--r--r-- */
	case Pstatus:	/* /proc/N/status = -r--r--r-- */
	case Pcmdline:	/* /proc/N/cmdline = -r--r--r-- */
	case Pmeminfo:	/* /proc/meminfo = -r--r--r-- */
	case Pcpuinfo:	/* /proc/cpuinfo = -r--r--r-- */
		pfs->pfs_mode = S_IRUSR|S_IRGRP|S_IROTH;
		vp->v_type = VREG;
		break;

	default:
		panic("procfs_allocvp");
	}

	procfs_hashins(pfs);
	uvm_vnp_setsize(vp, 0);
	lockmgr(&pfs_hashlock, LK_RELEASE, NULL);

	return (error);
}

int
procfs_freevp(vp)
	struct vnode *vp;
{
	struct pfsnode *pfs = VTOPFS(vp);

	procfs_hashrem(pfs);

	FREE(vp->v_data, M_TEMP);
	vp->v_data = 0;
	return (0);
}

int
procfs_rw(v)
	void *v;
{
	struct vop_read_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	struct proc *curp = uio->uio_procp;
	struct pfsnode *pfs = VTOPFS(vp);
	struct proc *p;

	p = PFIND(pfs->pfs_pid);
	if (p == 0)
		return (EINVAL);

	switch (pfs->pfs_type) {
	case Pregs:
	case Pfpregs:
	case Pmem:
		/*
		 * Do not allow init to be modified while in secure mode; it
		 * could be duped into changing the security level.
		 */
		if (uio->uio_rw == UIO_WRITE &&
		    p == initproc && securelevel > -1)
			return (EPERM);
		break;

	default:
		break;
	}

	switch (pfs->pfs_type) {
	case Pnote:
	case Pnotepg:
		return (procfs_donote(curp, p, pfs, uio));

	case Pregs:
		return (procfs_doregs(curp, p, pfs, uio));

	case Pfpregs:
		return (procfs_dofpregs(curp, p, pfs, uio));

	case Pctl:
		return (procfs_doctl(curp, p, pfs, uio));

	case Pstatus:
		return (procfs_dostatus(curp, p, pfs, uio));

	case Pmap:
		return (procfs_domap(curp, p, pfs, uio, 0));

	case Pmaps:
		return (procfs_domap(curp, p, pfs, uio, 1));

	case Pmem:
		return (procfs_domem(curp, p, pfs, uio));

	case Pcmdline:
		return (procfs_docmdline(curp, p, pfs, uio));

	case Pmeminfo:
		return (procfs_domeminfo(curp, p, pfs, uio));
	case Pcpuinfo:
		return (procfs_docpuinfo(curp, p, pfs, uio));

	default:
		return (EOPNOTSUPP);
	}
}

/*
 * Get a string from userland into (buf).  Strip a trailing
 * nl character (to allow easy access from the shell).
 * The buffer should be *buflenp + 1 chars long.  vfs_getuserstr
 * will automatically add a nul char at the end.
 *
 * Returns 0 on success or the following errors
 *
 * EINVAL:    file offset is non-zero.
 * EMSGSIZE:  message is longer than kernel buffer
 * EFAULT:    user i/o buffer is not addressable
 */
int
vfs_getuserstr(uio, buf, buflenp)
	struct uio *uio;
	char *buf;
	int *buflenp;
{
	int xlen;
	int error;

	if (uio->uio_offset != 0)
		return (EINVAL);

	xlen = *buflenp;

	/* must be able to read the whole string in one go */
	if (xlen < uio->uio_resid)
		return (EMSGSIZE);
	xlen = uio->uio_resid;

	if ((error = uiomove(buf, xlen, uio)) != 0)
		return (error);

	/* allow multiple writes without seeks */
	uio->uio_offset = 0;

	/* cleanup string and remove trailing newline */
	buf[xlen] = '\0';
	xlen = strlen(buf);
	if (xlen > 0 && buf[xlen-1] == '\n')
		buf[--xlen] = '\0';
	*buflenp = xlen;

	return (0);
}

const vfs_namemap_t *
vfs_findname(nm, buf, buflen)
	const vfs_namemap_t *nm;
	const char *buf;
	int buflen;
{

	for (; nm->nm_name; nm++)
		if (memcmp(buf, nm->nm_name, buflen+1) == 0)
			return (nm);

	return (0);
}

/*
 * Initialize pfsnode hash table.
 */
void
procfs_hashinit()
{
	lockinit(&pfs_hashlock, PINOD, "pfs_hashlock", 0, 0);
	pfs_hashtbl = hashinit(desiredvnodes / 4, HASH_LIST, M_UFSMNT,
	    M_WAITOK, &pfs_ihash);
	simple_lock_init(&pfs_hash_slock);
}

void
procfs_hashreinit()
{
	struct pfsnode *pp;
	struct pfs_hashhead *oldhash, *hash;
	u_long oldmask, mask, val;
	int i;

	hash = hashinit(desiredvnodes / 4, HASH_LIST, M_UFSMNT, M_WAITOK,
	    &mask);

	simple_lock(&pfs_hash_slock);
	oldhash = pfs_hashtbl;
	oldmask = pfs_ihash;
	pfs_hashtbl = hash;
	pfs_ihash = mask;
	for (i = 0; i <= oldmask; i++) {
		while ((pp = LIST_FIRST(&oldhash[i])) != NULL) {
			LIST_REMOVE(pp, pfs_hash);
			val = PFSPIDHASH(pp->pfs_pid);
			LIST_INSERT_HEAD(&hash[val], pp, pfs_hash);
		}
	}
	simple_unlock(&pfs_hash_slock);
	hashdone(oldhash, M_UFSMNT);
}

/*
 * Free pfsnode hash table.
 */
void
procfs_hashdone()
{
	hashdone(pfs_hashtbl, M_UFSMNT);
}

struct vnode *
procfs_hashget(pid, type, mp)
	pid_t pid;
	pfstype type;
	struct mount *mp;
{
	struct pfs_hashhead *ppp;
	struct pfsnode *pp;
	struct vnode *vp;

loop:
	simple_lock(&pfs_hash_slock);
	ppp = &pfs_hashtbl[PFSPIDHASH(pid)];
	LIST_FOREACH(pp, ppp, pfs_hash) {
		vp = PFSTOV(pp);
		if (pid == pp->pfs_pid && pp->pfs_type == type &&
		    vp->v_mount == mp) {
			simple_lock(&vp->v_interlock);
			simple_unlock(&pfs_hash_slock);
			if (vget(vp, LK_EXCLUSIVE | LK_INTERLOCK))
				goto loop;
			return (vp);
		}
	}
	simple_unlock(&pfs_hash_slock);
	return (NULL);
}

/*
 * Insert the pfsnode into the hash table and lock it.
 */
void
procfs_hashins(pp)
	struct pfsnode *pp;
{
	struct pfs_hashhead *ppp;

	/* lock the pfsnode, then put it on the appropriate hash list */
	lockmgr(&pp->pfs_vnode->v_lock, LK_EXCLUSIVE, (struct simplelock *)0);

	simple_lock(&pfs_hash_slock);
	ppp = &pfs_hashtbl[PFSPIDHASH(pp->pfs_pid)];
	LIST_INSERT_HEAD(ppp, pp, pfs_hash);
	simple_unlock(&pfs_hash_slock);
}

/*
 * Remove the pfsnode from the hash table.
 */
void
procfs_hashrem(pp)
	struct pfsnode *pp;
{
	simple_lock(&pfs_hash_slock);
	LIST_REMOVE(pp, pfs_hash);
	simple_unlock(&pfs_hash_slock);
}

void
procfs_revoke_vnodes(p, arg)
	struct proc *p;
	void *arg;
{
	struct pfsnode *pfs, *pnext;
	struct vnode *vp;
	struct mount *mp = (struct mount *)arg;
	struct pfs_hashhead *ppp;

	if (!(p->p_flag & P_SUGID))
		return;

	ppp = &pfs_hashtbl[PFSPIDHASH(p->p_pid)];
	for (pfs = LIST_FIRST(ppp); pfs; pfs = pnext) {
		vp = PFSTOV(pfs);
		pnext = LIST_NEXT(pfs, pfs_hash);
		if (vp->v_usecount > 0 && pfs->pfs_pid == p->p_pid &&
		    vp->v_mount == mp)
			VOP_REVOKE(vp, REVOKEALL);
	}
}
