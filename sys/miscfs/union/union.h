/*	$NetBSD: union.h,v 1.12.6.1 2002/10/06 00:15:37 thorpej Exp $	*/

/*
 * Copyright (c) 1994 The Regents of the University of California.
 * Copyright (c) 1994 Jan-Simon Pendry.
 * All rights reserved.
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
 *	@(#)union.h	8.9 (Berkeley) 12/10/94
 */

struct union_args {
	char		*target;	/* Target of loopback  */
	int		mntflags;	/* Options on the mount */
};

#define UNMNT_ABOVE	0x0001		/* Target appears below mount point */
#define UNMNT_BELOW	0x0002		/* Target appears below mount point */
#define UNMNT_REPLACE	0x0003		/* Target replaces mount point */
#define UNMNT_OPMASK	0x0003

#define UNMNT_BITS "\177\20" \
    "b\00above\0b\01below\0b\02replace"

struct union_mount {
	struct vnode	*um_uppervp;
	struct vnode	*um_lowervp;
	struct ucred	*um_cred;	/* Credentials of user calling mount */
	int		um_cmode;	/* cmask from mount process */
	int		um_op;		/* Operation mode */
};

#ifdef _KERNEL

/*
 * DEFDIRMODE is the mode bits used to create a shadow directory.
 */
#define	UN_DIRMODE	(S_IRWXU|S_IRWXG|S_IRWXO)
#define	UN_FILEMODE	(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)

/*
 * A cache of vnode references
 */
struct union_node {
	LIST_ENTRY(union_node)	un_cache;	/* Hash chain */
	struct vnode		*un_vnode;	/* Back pointer */
	struct vnode	        *un_uppervp;	/* overlaying object */
	struct vnode	        *un_lowervp;	/* underlying object */
	struct vnode		*un_dirvp;	/* Parent dir of uppervp */
	struct vnode		*un_pvp;	/* Parent vnode */
	char			*un_path;	/* saved component name */
	int			un_hash;	/* saved un_path hash value */
	int			un_openl;	/* # of opens on lowervp */
	unsigned int		un_flags;
	struct vnode		**un_dircache;	/* cached union stack */
	off_t			un_uppersz;	/* size of upper object */
	off_t			un_lowersz;	/* size of lower object */
#ifdef DIAGNOSTIC
	pid_t			un_pid;
#endif
};

#define UN_WANTED	0x01
#define UN_LOCKED	0x02
#define UN_ULOCK	0x04		/* Upper node is locked */
#define UN_KLOCK	0x08		/* Keep upper node locked on vput */
#define UN_CACHED	0x10		/* In union cache */
#define UN_DRAINING	0x20		/* upper node lock is draining */
#define UN_DRAINED	0x40		/* upper node lock is drained */

extern int union_allocvp __P((struct vnode **, struct mount *,
				struct vnode *, struct vnode *,
				struct componentname *, struct vnode *,
				struct vnode *, int));
extern int union_copyfile __P((struct vnode *, struct vnode *,
					struct ucred *, struct proc *));
extern int union_copyup __P((struct union_node *, int, struct ucred *,
				struct proc *));
extern void union_diruncache __P((struct union_node *));
extern int union_dowhiteout __P((struct union_node *, struct ucred *,
					struct proc *));
extern int union_mkshadow __P((struct union_mount *, struct vnode *,
				struct componentname *, struct vnode **));
extern int union_mkwhiteout __P((struct union_mount *, struct vnode *,
				struct componentname *, char *));
extern int union_vn_create __P((struct vnode **, struct union_node *,
				struct proc *));
extern int union_cn_close __P((struct vnode *, int, struct ucred *,
				struct proc *));
extern void union_removed_upper __P((struct union_node *un));
extern struct vnode *union_lowervp __P((struct vnode *));
extern void union_newlower __P((struct union_node *, struct vnode *));
extern void union_newupper __P((struct union_node *, struct vnode *));
extern void union_newsize __P((struct vnode *, off_t, off_t));

#define	MOUNTTOUNIONMOUNT(mp) ((struct union_mount *)((mp)->mnt_data))
#define	VTOUNION(vp) ((struct union_node *)(vp)->v_data)
#define	UNIONTOV(un) ((un)->un_vnode)
#define	LOWERVP(vp) (VTOUNION(vp)->un_lowervp)
#define	UPPERVP(vp) (VTOUNION(vp)->un_uppervp)
#define OTHERVP(vp) (UPPERVP(vp) ? UPPERVP(vp) : LOWERVP(vp))

extern int (**union_vnodeop_p) __P((void *));
extern struct vfsops union_vfsops;

void union_init __P((void));
void union_done __P((void));
int union_freevp __P((struct vnode *));

#endif /* _KERNEL */
