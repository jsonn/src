/*	$NetBSD: nfsnode.h,v 1.11.2.1 1994/08/19 12:10:45 mycroft Exp $	*/

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
 *	@(#)nfsnode.h	8.4 (Berkeley) 2/13/94
 */

/*
 * Silly rename structure that hangs off the nfsnode until the name
 * can be removed by nfs_inactive()
 */
struct sillyrename {
	struct	ucred *s_cred;
	struct	vnode *s_dvp;
	long	s_namlen;
	char	s_name[20];
};

/*
 * The nfsnode is the nfs equivalent to ufs's inode. Any similarity
 * is purely coincidental.
 * There is a unique nfsnode allocated for each active file,
 * each current directory, each mounted-on file, text file, and the root.
 * An nfsnode is 'named' by its file handle. (nget/nfs_node.c)
 */

struct nfsnode {
	LIST_ENTRY(nfsnode) n_hash;	/* Hash chain */
	CIRCLEQ_ENTRY(nfsnode) n_timer;	/* Nqnfs timer chain */
	nfsv2fh_t n_fh;			/* NFS File Handle */
	long	n_flag;			/* Flag for locking.. */
	struct	vnode *n_vnode;		/* vnode associated with this node */
	struct	vattr n_vattr;		/* Vnode attribute cache */
	time_t	n_attrstamp;		/* Time stamp for cached attributes */
	struct	sillyrename *n_sillyrename; /* Ptr to silly rename struct */
	u_quad_t n_size;		/* Current size of file */
	int	n_error;		/* Save write error value */
	u_long	n_direofoffset;		/* Dir. EOF offset cache */
	time_t	n_mtime;		 /* Prev modify time. */
	time_t	n_ctime;		 /* Prev create time. */
	u_quad_t n_brev;		 /* Modify rev when cached */
	u_quad_t n_lrev;		 /* Modify rev for lease */
	time_t	n_expiry;		 /* Lease expiry time */
	struct	lockf *n_lockf;		/* Advisory lock records */
	struct	sillyrename n_silly;	/* Silly rename struct */
	struct	timeval n_atim;		/* Special file times */
	struct	timeval n_mtim;
};

/*
 * Flags for n_flag
 */
#define	NFLUSHWANT	0x0001	/* Want wakeup from a flush in prog. */
#define	NFLUSHINPROG	0x0002	/* Avoid multiple calls to vinvalbuf() */
#define	NMODIFIED	0x0004	/* Might have a modified buffer in bio */
#define	NWRITEERR	0x0008	/* Flag write errors so close will know */
#define	NQNFSNONCACHE	0x0020	/* Non-cachable lease */
#define	NQNFSWRITE	0x0040	/* Write lease */
#define	NQNFSEVICTED	0x0080	/* Has been evicted */
#define	NACC		0x0100	/* Special file accessed */
#define	NUPD		0x0200	/* Special file updated */
#define	NCHG		0x0400	/* Special file times changed */

/*
 * Convert between nfsnode pointers and vnode pointers
 */
#define VTONFS(vp)	((struct nfsnode *)(vp)->v_data)
#define NFSTOV(np)	((struct vnode *)(np)->n_vnode)

/*
 * Queue head for nfsiod's
 */
TAILQ_HEAD(, buf) nfs_bufq;

#ifdef KERNEL
/*
 * Prototypes for NFS vnode operations
 */
int	nfs_lookup __P((struct vop_lookup_args *));
int	nfs_create __P((struct vop_create_args *));
int	nfs_mknod __P((struct vop_mknod_args *));
int	nfs_open __P((struct vop_open_args *));
int	nfs_close __P((struct vop_close_args *));
int	nfsspec_close __P((struct vop_close_args *));
#ifdef FIFO
int	nfsfifo_close __P((struct vop_close_args *));
#endif
int	nfs_access __P((struct vop_access_args *));
int	nfsspec_access __P((struct vop_access_args *));
int	nfs_getattr __P((struct vop_getattr_args *));
int	nfs_setattr __P((struct vop_setattr_args *));
int	nfs_read __P((struct vop_read_args *));
int	nfs_write __P((struct vop_write_args *));
int	nfsspec_read __P((struct vop_read_args *));
int	nfsspec_write __P((struct vop_write_args *));
#ifdef FIFO
int	nfsfifo_read __P((struct vop_read_args *));
int	nfsfifo_write __P((struct vop_write_args *));
#endif
#define nfs_ioctl ((int (*) __P((struct  vop_ioctl_args *)))enoioctl)
#define nfs_select ((int (*) __P((struct  vop_select_args *)))seltrue)
int	nfs_mmap __P((struct vop_mmap_args *));
int	nfs_fsync __P((struct vop_fsync_args *));
#define nfs_seek ((int (*) __P((struct  vop_seek_args *)))nullop)
int	nfs_remove __P((struct vop_remove_args *));
int	nfs_link __P((struct vop_link_args *));
int	nfs_rename __P((struct vop_rename_args *));
int	nfs_mkdir __P((struct vop_mkdir_args *));
int	nfs_rmdir __P((struct vop_rmdir_args *));
int	nfs_symlink __P((struct vop_symlink_args *));
int	nfs_readdir __P((struct vop_readdir_args *));
int	nfs_readlink __P((struct vop_readlink_args *));
int	nfs_abortop __P((struct vop_abortop_args *));
int	nfs_inactive __P((struct vop_inactive_args *));
int	nfs_reclaim __P((struct vop_reclaim_args *));
int	nfs_lock __P((struct vop_lock_args *));
int	nfs_unlock __P((struct vop_unlock_args *));
int	nfs_bmap __P((struct vop_bmap_args *));
int	nfs_strategy __P((struct vop_strategy_args *));
int	nfs_print __P((struct vop_print_args *));
int	nfs_islocked __P((struct vop_islocked_args *));
int	nfs_pathconf __P((struct vop_pathconf_args *));
int	nfs_advlock __P((struct vop_advlock_args *));
int	nfs_blkatoff __P((struct vop_blkatoff_args *));
int	nfs_vget __P((struct mount *, ino_t, struct vnode **));
int	nfs_valloc __P((struct vop_valloc_args *));
#define nfs_reallocblks \
	((int (*) __P((struct  vop_reallocblks_args *)))eopnotsupp)
int	nfs_vfree __P((struct vop_vfree_args *));
int	nfs_truncate __P((struct vop_truncate_args *));
int	nfs_update __P((struct vop_update_args *));
int	nfs_bwrite __P((struct vop_bwrite_args *));
#endif /* KERNEL */
