/*	$NetBSD: cd9660_node.h,v 1.4.2.4 2004/09/18 14:52:37 skrll Exp $	*/

/*-
 * Copyright (c) 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley
 * by Pace Willisson (pace@blitz.com).  The Rock Ridge Extension
 * Support code is derived from software contributed to Berkeley
 * by Atsushi Murai (amurai@spec.co.jp).
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
 *	@(#)cd9660_node.h	8.6 (Berkeley) 5/14/95
 */

#include <miscfs/genfs/genfs_node.h>

/*
 * Theoretically, directories can be more than 2Gb in length,
 * however, in practice this seems unlikely. So, we define
 * the type doff_t as a long to keep down the cost of doing
 * lookup on a 32-bit machine. If you are porting to a 64-bit
 * architecture, you should make doff_t the same as off_t.
 */
#define doff_t	long

typedef	struct	{
	struct timespec	iso_atime;	/* time of last access */
	struct timespec	iso_mtime;	/* time of last modification */
	struct timespec	iso_ctime;	/* time file changed */
	u_short		iso_mode;	/* files access mode and type */
	uid_t		iso_uid;	/* owner user id */
	gid_t		iso_gid;	/* owner group id */
	short		iso_links;	/* links of file */
	dev_t		iso_rdev;	/* Major/Minor number for special */
} ISO_RRIP_INODE;

#ifdef ISODEVMAP
/*
 * FOr device# (major,minor) translation table
 */
struct iso_dnode {
	LIST_ENTRY(iso_dnode) d_hash;
	dev_t		i_dev;		/* device where dnode resides */
	ino_t		i_number;	/* the identity of the inode */
	dev_t		d_dev;		/* device # for translation */
};
#endif

struct iso_node {
	struct	genfs_node i_gnode;
	LIST_ENTRY(iso_node) i_hash;
	struct	vnode *i_vnode;	/* vnode associated with this inode */
	struct	vnode *i_devvp;	/* vnode for block I/O */
	u_long	i_flag;		/* see below */
	dev_t	i_dev;		/* device where inode resides */
	ino_t	i_number;	/* the identity of the inode */
				/* we use the actual starting block of the file */
	struct	iso_mnt *i_mnt;	/* filesystem associated with this inode */
	struct	lockf *i_lockf;	/* head of byte-level lock list */
	doff_t	i_endoff;	/* end of useful stuff in directory */
	doff_t	i_diroff;	/* offset in dir, where we found last entry */
	doff_t	i_offset;	/* offset of free space in directory */
	ino_t	i_ino;		/* inode number of found directory */

	unsigned long iso_extent;	/* extent of file */
	unsigned long i_size;
	unsigned long iso_start;		/* actual start of data of file (may be different */
				/* from iso_extent, if file has extended attributes) */
	ISO_RRIP_INODE  inode;
};

#define	i_forw		i_chain[0]
#define	i_back		i_chain[1]

/* flags */
#define	IN_ACCESS	0x0020		/* inode access time to be updated */

#define VTOI(vp) ((struct iso_node *)(vp)->v_data)
#define ITOV(ip) ((ip)->i_vnode)

/*
 * Prototypes for ISOFS vnode operations
 */
int	cd9660_lookup	__P((void *));
#define	cd9660_open	genfs_nullop
#define	cd9660_close	genfs_nullop
int	cd9660_access	__P((void *));
int	cd9660_getattr	__P((void *));
int	cd9660_read	__P((void *));
#define	cd9660_ioctl	genfs_enoioctl
#define	cd9660_poll	genfs_poll
#define	cd9660_mmap	genfs_mmap
#define	cd9660_seek	genfs_seek
int	cd9660_readdir	__P((void *));
int	cd9660_readlink	__P((void *));
#define	cd9660_abortop	genfs_abortop
int	cd9660_inactive	__P((void *));
int	cd9660_reclaim	__P((void *));
int	cd9660_link	__P((void *));
int	cd9660_symlink	__P((void *));
int	cd9660_bmap	__P((void *));
int	cd9660_lock	__P((void *));
int	cd9660_unlock	__P((void *));
int	cd9660_strategy	__P((void *));
int	cd9660_print	__P((void *));
int	cd9660_islocked	__P((void *));
int	cd9660_pathconf	__P((void *));
int	cd9660_setattr	__P((void *));
int	cd9660_blkatoff	__P((void *));

void	cd9660_defattr __P((struct iso_directory_record *,
			struct iso_node *, struct buf *));
void	cd9660_deftstamp __P((struct iso_directory_record *,
			struct iso_node *, struct buf *));
struct	vnode *cd9660_ihashget __P((dev_t, ino_t));
void	cd9660_ihashins __P((struct iso_node *));
void	cd9660_ihashrem __P((struct iso_node *));
int	cd9660_tstamp_conv7 __P((u_char *, struct timespec *));
int	cd9660_tstamp_conv17 __P((u_char *, struct timespec *));
int	cd9660_vget_internal __P((struct mount *, ino_t, struct vnode **, int,
			      struct iso_directory_record *));
#ifdef	ISODEVMAP
struct iso_dnode *iso_dmap __P((dev_t, ino_t, int));
void iso_dunmap __P((dev_t));
#endif
