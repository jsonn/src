/*	$NetBSD: ffs_extern.h,v 1.12.8.2 2000/12/08 09:20:11 bouyer Exp $	*/

/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)ffs_extern.h	8.6 (Berkeley) 3/30/95
 */

/*
 * Sysctl values for the fast filesystem.
 */
#define FFS_CLUSTERREAD		1	/* cluster reading enabled */
#define FFS_CLUSTERWRITE	2	/* cluster writing enabled */
#define FFS_REALLOCBLKS		3	/* block reallocation enabled */
#define FFS_ASYNCFREE		4	/* asynchronous block freeing enabled */
#define FFS_LOG_CHANGEOPT	5	/* log optimalization strategy change */
#define FFS_MAXID		6	/* number of valid ffs ids */

#define FFS_NAMES { \
	{ 0, 0 }, \
	{ "doclusterread", CTLTYPE_INT }, \
	{ "doclusterwrite", CTLTYPE_INT }, \
	{ "doreallocblks", CTLTYPE_INT }, \
	{ "doasyncfree", CTLTYPE_INT }, \
	{ "log_changeopt", CTLTYPE_INT }, \
}

struct buf;
struct fid;
struct fs;
struct inode;
struct dinode;
struct mount;
struct nameidata;
struct proc;
struct statfs;
struct timeval;
struct ucred;
struct ufsmount;
struct uio;
struct vnode;
struct mbuf;
struct cg;

extern struct pool ffs_inode_pool;		/* memory pool for inodes */

__BEGIN_DECLS

/* ffs_alloc.c */
int ffs_alloc __P((struct inode *, ufs_daddr_t, ufs_daddr_t , int, struct ucred *,
		   ufs_daddr_t *));
int ffs_realloccg __P((struct inode *, ufs_daddr_t, ufs_daddr_t, int, int ,
		       struct ucred *, struct buf **, ufs_daddr_t *));
int ffs_reallocblks __P((void *));
int ffs_valloc __P((void *));
ufs_daddr_t ffs_blkpref __P((struct inode *, ufs_daddr_t, int, ufs_daddr_t *));
void ffs_blkfree __P((struct inode *, ufs_daddr_t, long));
int ffs_vfree __P((void *));
void ffs_clusteracct __P((struct fs *, struct cg *, ufs_daddr_t, int));

/* ffs_balloc.c */
int ffs_balloc __P((void *));
int ffs_ballocn __P((void *));

/* ffs_bswap.c */
void ffs_sb_swap __P((struct fs*, struct fs *, int));
void ffs_dinode_swap __P((struct dinode *, struct dinode *));
void ffs_csum_swap __P((struct csum *, struct csum *, int));

/* ffs_inode.c */
int ffs_update __P((void *));
int ffs_truncate __P((void *));

/* ffs_subr.c */
int ffs_blkatoff __P((void *));
int ffs_freefile __P((void *));
void ffs_fragacct __P((struct fs *, int, int32_t[], int, int));
#ifdef DIAGNOSTIC
void	ffs_checkoverlap __P((struct buf *, struct inode *));
#endif
int ffs_isblock __P((struct fs *, unsigned char *, ufs_daddr_t));
int ffs_isfreeblock __P((struct fs *, unsigned char *, ufs_daddr_t));
void ffs_clrblock __P((struct fs *, u_char *, ufs_daddr_t));
void ffs_setblock __P((struct fs *, unsigned char *, ufs_daddr_t));

/* ffs_vfsops.c */
void ffs_init __P((void));
void ffs_done __P((void));
int ffs_mountroot __P((void));
int ffs_mount __P((struct mount *, const char *, void *, struct nameidata *,
		   struct proc *));
int ffs_reload __P((struct mount *, struct ucred *, struct proc *));
int ffs_mountfs __P((struct vnode *, struct mount *, struct proc *));
int ffs_oldfscompat __P((struct fs *));
int ffs_unmount __P((struct mount *, int, struct proc *));
int ffs_flushfiles __P((struct mount *, int, struct proc *));
int ffs_statfs __P((struct mount *, struct statfs *, struct proc *));
int ffs_sync __P((struct mount *, int, struct ucred *, struct proc *));
int ffs_vget __P((struct mount *, ino_t, struct vnode **));
int ffs_fhtovp __P((struct mount *, struct fid *, struct vnode **));
int ffs_vptofh __P((struct vnode *, struct fid *));
int ffs_sysctl __P((int *, u_int, void *, size_t *, void *, size_t,
		    struct proc *));
int ffs_sbupdate __P((struct ufsmount *, int));
int ffs_cgupdate __P((struct ufsmount *, int));

/* ffs_vnops.c */
int ffs_read __P((void *));
int ffs_write __P((void *));
int ffs_fsync __P((void *));
int ffs_reclaim __P((void *));
int ffs_size __P((void *));
__END_DECLS

 
/*
 * Soft dependency function prototypes.
 */
void	softdep_initialize __P((void));
int	softdep_mount __P((struct vnode *, struct mount *, struct fs *,
	    struct ucred *));
int	softdep_flushfiles __P((struct mount *, int, struct proc *));
void	softdep_update_inodeblock __P((struct inode *, struct buf *, int));
void	softdep_load_inodeblock __P((struct inode *));
void	softdep_freefile __P((void *));
void	softdep_setup_freeblocks __P((struct inode *, off_t));
void	softdep_setup_inomapdep __P((struct buf *, struct inode *, ino_t));
void	softdep_setup_blkmapdep __P((struct buf *, struct fs *, ufs_daddr_t));
void	softdep_setup_allocdirect __P((struct inode *, ufs_lbn_t, ufs_daddr_t,
	    ufs_daddr_t, long, long, struct buf *));
void	softdep_setup_allocindir_meta __P((struct buf *, struct inode *,
	    struct buf *, int, ufs_daddr_t));
void	softdep_setup_allocindir_page __P((struct inode *, ufs_lbn_t,
	    struct buf *, int, ufs_daddr_t, ufs_daddr_t, struct buf *));
void	softdep_fsync_mountdev __P((struct vnode *));
int	softdep_sync_metadata __P((void *));

extern int (**ffs_vnodeop_p) __P((void *));
extern int (**ffs_specop_p) __P((void *));
extern int (**ffs_fifoop_p) __P((void *));


