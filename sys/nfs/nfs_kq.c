/*	$NetBSD: nfs_kq.c,v 1.9.4.1 2006/09/09 02:59:24 rpaulo Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jaromir Dolecek.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: nfs_kq.c,v 1.9.4.1 2006/09/09 02:59:24 rpaulo Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/unistd.h>
#include <sys/file.h>
#include <sys/kthread.h>

#include <uvm/uvm_extern.h>
#include <uvm/uvm.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <nfs/nfsnode.h>
#include <nfs/nfs_var.h>

struct kevq {
	SLIST_ENTRY(kevq)	kev_link;
	struct vnode		*vp;
	u_int			usecount;
	u_int			flags;
#define KEVQ_BUSY	0x01	/* currently being processed */
#define KEVQ_WANT	0x02	/* want to change this entry */
	struct timespec		omtime;	/* old modification time */
	struct timespec		octime;	/* old change time */
	nlink_t			onlink;	/* old number of references to file */
};
SLIST_HEAD(kevqlist, kevq);

static struct lock nfskevq_lock;
static struct proc *pnfskq;
static struct kevqlist kevlist = SLIST_HEAD_INITIALIZER(kevlist);

void
nfs_kqinit(void)
{
	lockinit(&nfskevq_lock, PSOCK, "nfskqlck", 0, 0);
}

/*
 * This quite simplistic routine periodically checks for server changes
 * of any of the watched files every NFS_MINATTRTIMO/2 seconds.
 * Only changes in size, modification time, change time and nlinks
 * are being checked, everything else is ignored.
 * The routine only calls VOP_GETATTR() when it's likely it would get
 * some new data, i.e. when the vnode expires from attrcache. This
 * should give same result as periodically running stat(2) from userland,
 * while keeping CPU/network usage low, and still provide proper kevent
 * semantics.
 * The poller thread is created when first vnode is added to watch list,
 * and exits when the watch list is empty. The overhead of thread creation
 * isn't really important, neither speed of attach and detach of knote.
 */
/* ARGSUSED */
static void
nfs_kqpoll(void *arg)
{
	struct kevq *ke;
	struct vattr attr;
	struct lwp *l = curlwp;
	u_quad_t osize;

	for(;;) {
		lockmgr(&nfskevq_lock, LK_EXCLUSIVE, NULL);
		SLIST_FOREACH(ke, &kevlist, kev_link) {
			/* skip if still in attrcache */
			if (nfs_getattrcache(ke->vp, &attr) != ENOENT)
				continue;

			/*
			 * Mark entry busy, release lock and check
			 * for changes.
			 */
			ke->flags |= KEVQ_BUSY;
			lockmgr(&nfskevq_lock, LK_RELEASE, NULL);

			/* save v_size, nfs_getattr() updates it */
			osize = ke->vp->v_size;

			(void) VOP_GETATTR(ke->vp, &attr, l->l_cred, l);

			/* following is a bit fragile, but about best
			 * we can get */
			if (attr.va_size != osize) {
				int extended = (attr.va_size > osize);
				VN_KNOTE(ke->vp, NOTE_WRITE
					| (extended ? NOTE_EXTEND : 0));
				ke->omtime = attr.va_mtime;
			} else if (attr.va_mtime.tv_sec != ke->omtime.tv_sec
			    || attr.va_mtime.tv_nsec != ke->omtime.tv_nsec) {
				VN_KNOTE(ke->vp, NOTE_WRITE);
				ke->omtime = attr.va_mtime;
			}

			if (attr.va_ctime.tv_sec != ke->octime.tv_sec
			    || attr.va_ctime.tv_nsec != ke->octime.tv_nsec) {
				VN_KNOTE(ke->vp, NOTE_ATTRIB);
				ke->octime = attr.va_ctime;
			}

			if (attr.va_nlink != ke->onlink) {
				VN_KNOTE(ke->vp, NOTE_LINK);
				ke->onlink = attr.va_nlink;
			}

			lockmgr(&nfskevq_lock, LK_EXCLUSIVE, NULL);
			ke->flags &= ~KEVQ_BUSY;
			if (ke->flags & KEVQ_WANT) {
				ke->flags &= ~KEVQ_WANT;
				wakeup(ke);
			}
		}

		if (SLIST_EMPTY(&kevlist)) {
			/* Nothing more to watch, exit */
			pnfskq = NULL;
			lockmgr(&nfskevq_lock, LK_RELEASE, NULL);
			kthread_exit(0);
		}
		lockmgr(&nfskevq_lock, LK_RELEASE, NULL);

		/* wait a while before checking for changes again */
		tsleep(pnfskq, PSOCK, "nfskqpw",
			NFS_MINATTRTIMO * hz / 2);

	}
}

static void
filt_nfsdetach(struct knote *kn)
{
	struct vnode *vp = (struct vnode *)kn->kn_hook;
	struct kevq *ke;

	/* XXXLUKEM lock the struct? */
	SLIST_REMOVE(&vp->v_klist, kn, knote, kn_selnext);

	/* Remove the vnode from watch list */
	lockmgr(&nfskevq_lock, LK_EXCLUSIVE, NULL);
	SLIST_FOREACH(ke, &kevlist, kev_link) {
		if (ke->vp == vp) {
			while (ke->flags & KEVQ_BUSY) {
				ke->flags |= KEVQ_WANT;
				lockmgr(&nfskevq_lock, LK_RELEASE, NULL);
				(void) tsleep(ke, PSOCK, "nfskqdet", 0);
				lockmgr(&nfskevq_lock, LK_EXCLUSIVE, NULL);
			}

			if (ke->usecount > 1) {
				/* keep, other kevents need this */
				ke->usecount--;
			} else {
				/* last user, g/c */
				SLIST_REMOVE(&kevlist, ke, kevq, kev_link);
				FREE(ke, M_KEVENT);
			}
			break;
		}
	}
	lockmgr(&nfskevq_lock, LK_RELEASE, NULL);
}

static int
filt_nfsread(struct knote *kn, long hint)
{
	struct vnode *vp = (struct vnode *)kn->kn_hook;

	/*
	 * filesystem is gone, so set the EOF flag and schedule
	 * the knote for deletion.
	 */
	if (hint == NOTE_REVOKE) {
		kn->kn_flags |= (EV_EOF | EV_ONESHOT);
		return (1);
	}

	/* XXXLUKEM lock the struct? */
	kn->kn_data = vp->v_size - kn->kn_fp->f_offset;
        return (kn->kn_data != 0);
}

static int
filt_nfsvnode(struct knote *kn, long hint)
{

	if (kn->kn_sfflags & hint)
		kn->kn_fflags |= hint;
	if (hint == NOTE_REVOKE) {
		kn->kn_flags |= EV_EOF;
		return (1);
	}
	return (kn->kn_fflags != 0);
}

static const struct filterops nfsread_filtops =
	{ 1, NULL, filt_nfsdetach, filt_nfsread };
static const struct filterops nfsvnode_filtops =
	{ 1, NULL, filt_nfsdetach, filt_nfsvnode };

int
nfs_kqfilter(void *v)
{
	struct vop_kqfilter_args /* {
		struct vnode	*a_vp;
		struct knote	*a_kn;
	} */ *ap = v;
	struct vnode *vp;
	struct knote *kn;
	struct kevq *ke;
	int error = 0;
	struct vattr attr;
	struct lwp *l = curlwp;		/* XXX */

	vp = ap->a_vp;
	kn = ap->a_kn;
	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &nfsread_filtops;
		break;
	case EVFILT_VNODE:
		kn->kn_fop = &nfsvnode_filtops;
		break;
	default:
		return (1);
	}

	kn->kn_hook = vp;

	/*
	 * Put the vnode to watched list.
	 */

	/*
	 * Fetch current attributes. It's only needed when the vnode
	 * is not watched yet, but we need to do this without lock
	 * held. This is likely cheap due to attrcache, so do it now.
	 */
	memset(&attr, 0, sizeof(attr));
	(void) VOP_GETATTR(vp, &attr, l->l_cred, l);

	lockmgr(&nfskevq_lock, LK_EXCLUSIVE, NULL);

	/* ensure the poller is running */
	if (!pnfskq) {
		error = kthread_create1(nfs_kqpoll, NULL, &pnfskq,
				"nfskqpoll");
		if (error)
			goto out;
	}

	SLIST_FOREACH(ke, &kevlist, kev_link) {
		if (ke->vp == vp)
			break;
	}

	if (ke) {
		/* already watched, so just bump usecount */
		ke->usecount++;
	} else {
		/* need a new one */
		MALLOC(ke, struct kevq *, sizeof(struct kevq), M_KEVENT,
			M_WAITOK);
		ke->vp = vp;
		ke->usecount = 1;
		ke->flags = 0;
		ke->omtime = attr.va_mtime;
		ke->octime = attr.va_ctime;
		ke->onlink = attr.va_nlink;
		SLIST_INSERT_HEAD(&kevlist, ke, kev_link);
	}

	/* kick the poller */
	wakeup(pnfskq);

	/* XXXLUKEM lock the struct? */
	SLIST_INSERT_HEAD(&vp->v_klist, kn, kn_selnext);

    out:
	lockmgr(&nfskevq_lock, LK_RELEASE, NULL);

	return (error);
}
