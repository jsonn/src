/*	$NetBSD: inode.h,v 1.13.8.1 1999/12/27 18:36:41 wrstuden Exp $	*/

/*
 * Copyright (c) 1982, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)inode.h	8.9 (Berkeley) 5/14/95
 */

#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ext2fs/ext2fs_dinode.h>

/*
 * Per-filesystem inode extensions.
 */
struct ext2fs_inode_ext {
	ufs_daddr_t ext2fs_last_lblk;	/* last logical block allocated */
	ufs_daddr_t ext2fs_last_blk;	/* last block allocated on disk */
};

struct ffs_inode_ext {
	int ffs_effnlink;	/* i_nlink when I/O completes */
};

/*
 * The inode is used to describe each active (or recently active) file in the
 * UFS filesystem. It is composed of two types of information. The first part
 * is the information that is needed only while the file is active (such as
 * the identity of the file and linkage to speed its lookup). The second part
 * is * the permanent meta-data associated with the file which is read in
 * from the permanent dinode from long term storage when the file becomes
 * active, and is put back when the file is no longer being used.
 */
struct inode {
	LIST_ENTRY(inode) i_hash;/* Hash chain. */
	struct	vnode *i_vnode;	/* Vnode associated with this inode. */
	struct	vnode *i_devvp;	/* Vnode for block I/O. */
	u_int32_t i_flag;	/* flags, see below */
	dev_t	  i_dev;	/* Device associated with the inode. */
	ino_t	  i_number;	/* The identity of the inode. */

	union {			/* Associated filesystem. */
		struct	fs *fs;		/* FFS */
		struct	lfs *lfs;	/* LFS */
		struct	m_ext2fs *e2fs;	/* EXT2FS */
	} inode_u;
#define	i_fs	inode_u.fs
#define	i_lfs	inode_u.lfs
#define	i_e2fs	inode_u.e2fs

	struct	 dquot *i_dquot[MAXQUOTAS]; /* Dquot structures. */
	u_quad_t i_modrev;	/* Revision level for NFS lease. */
	struct	 lockf *i_lockf;/* Head of byte-level lock list. */
	/*
	 * Side effects; used during directory lookup.
	 */
	int32_t	  i_count;	/* Size of free slot in directory. */
	doff_t	  i_endoff;	/* End of useful stuff in directory. */
	doff_t	  i_diroff;	/* Offset in dir, where we found last entry. */
	doff_t	  i_offset;	/* Offset of free space in directory. */
	ino_t	  i_ino;	/* Inode number of found directory. */
	u_int32_t i_reclen;	/* Size of found directory entry. */
	/*
	 * Inode extensions
	 */
	union {
		/* Other extensions could go here... */
		struct	ext2fs_inode_ext e2fs;
		struct	ffs_inode_ext ffs;
	} inode_ext;
#define	i_e2fs_last_lblk	inode_ext.e2fs.ext2fs_last_lblk
#define	i_e2fs_last_blk		inode_ext.e2fs.ext2fs_last_blk
#define	i_ffs_effnlink		inode_ext.ffs.ffs_effnlink

	/*
	 * The on-disk dinode itself.
	 */
	union {
		struct	dinode ffs_din;	/* 128 bytes of the on-disk dinode. */
		struct	ext2fs_dinode e2fs_din; /* 128 bytes of the on-disk
						   dinode. */
	} i_din;
};

#define	i_ffs_atime		i_din.ffs_din.di_atime
#define	i_ffs_atimensec		i_din.ffs_din.di_atimensec
#define	i_ffs_blocks		i_din.ffs_din.di_blocks
#define	i_ffs_ctime		i_din.ffs_din.di_ctime
#define	i_ffs_ctimensec		i_din.ffs_din.di_ctimensec
#define	i_ffs_db		i_din.ffs_din.di_db
#define	i_ffs_flags		i_din.ffs_din.di_flags
#define	i_ffs_gen		i_din.ffs_din.di_gen
#define	i_ffs_gid		i_din.ffs_din.di_gid
#define	i_ffs_ib		i_din.ffs_din.di_ib
#define	i_ffs_mode		i_din.ffs_din.di_mode
#define	i_ffs_mtime		i_din.ffs_din.di_mtime
#define	i_ffs_mtimensec		i_din.ffs_din.di_mtimensec
#define	i_ffs_nlink		i_din.ffs_din.di_nlink
#define	i_ffs_rdev		i_din.ffs_din.di_rdev
#define	i_ffs_shortlink		i_din.ffs_din.di_shortlink
#define	i_ffs_size		i_din.ffs_din.di_size
#define	i_ffs_uid		i_din.ffs_din.di_uid

#define	i_e2fs_mode		i_din.e2fs_din.e2di_mode
#define	i_e2fs_uid		i_din.e2fs_din.e2di_uid
#define	i_e2fs_size		i_din.e2fs_din.e2di_size
#define	i_e2fs_atime		i_din.e2fs_din.e2di_atime
#define	i_e2fs_ctime		i_din.e2fs_din.e2di_ctime
#define	i_e2fs_mtime		i_din.e2fs_din.e2di_mtime
#define	i_e2fs_dtime		i_din.e2fs_din.e2di_dtime
#define	i_e2fs_gid		i_din.e2fs_din.e2di_gid
#define	i_e2fs_nlink		i_din.e2fs_din.e2di_nlink
#define	i_e2fs_nblock		i_din.e2fs_din.e2di_nblock
#define	i_e2fs_flags		i_din.e2fs_din.e2di_flags
#define	i_e2fs_blocks		i_din.e2fs_din.e2di_blocks
#define	i_e2fs_gen		i_din.e2fs_din.e2di_gen
#define	i_e2fs_facl		i_din.e2fs_din.e2di_facl
#define	i_e2fs_dacl		i_din.e2fs_din.e2di_dacl
#define	i_e2fs_faddr		i_din.e2fs_din.e2di_faddr
#define	i_e2fs_nfrag		i_din.e2fs_din.e2di_nfrag
#define	i_e2fs_fsize		i_din.e2fs_din.e2di_fsize
#define	i_e2fs_rdev		i_din.e2fs_din.e2di_rdev

/* These flags are kept in i_flag. */
#define	IN_ACCESS	0x0001		/* Access time update request. */
#define	IN_CHANGE	0x0002		/* Inode change time update request. */
#define	IN_UPDATE	0x0004		/* Modification time update request. */
#define	IN_MODIFIED	0x0008		/* Inode has been modified. */
#define	IN_RENAME	0x0010		/* Inode is being renamed. */
#define	IN_SHLOCK	0x0020		/* File has shared lock. */
#define	IN_EXLOCK	0x0040		/* File has exclusive lock. */
#define	IN_CLEANING	0x0080		/* LFS: file is being cleaned */

#ifdef _KERNEL
/*
 * Structure used to pass around logical block paths generated by
 * ufs_getlbns and used by truncate and bmap code.
 */
struct indir {
	ufs_daddr_t in_lbn;		/* Logical block number. */
	int	in_off;			/* Offset in buffer. */
	int	in_exists;		/* Flag if the block exists. */
};

/* Convert between inode pointers and vnode pointers. */
#define	VTOI(vp)	((struct inode *)(vp)->v_data)
#define	ITOV(ip)	((ip)->i_vnode)

#define	FFS_ITIMES(ip, acc, mod, cre) {					\
	if ((ip)->i_flag & (IN_ACCESS | IN_CHANGE | IN_UPDATE)) {	\
		(ip)->i_flag |= IN_MODIFIED;				\
		if ((ip)->i_flag & IN_ACCESS) {				\
			(ip)->i_ffs_atime = (acc)->tv_sec;		\
			(ip)->i_ffs_atimensec = (acc)->tv_nsec;		\
		}							\
		if ((ip)->i_flag & IN_UPDATE) {				\
			(ip)->i_ffs_mtime = (mod)->tv_sec;		\
			(ip)->i_ffs_mtimensec = (mod)->tv_nsec;		\
			(ip)->i_modrev++;				\
		}							\
		if ((ip)->i_flag & IN_CHANGE) {				\
			(ip)->i_ffs_ctime = (cre)->tv_sec;		\
			(ip)->i_ffs_ctimensec = (cre)->tv_nsec;		\
		}							\
		(ip)->i_flag &= ~(IN_ACCESS | IN_CHANGE | IN_UPDATE);	\
	}								\
}

#define	EXT2FS_ITIMES(ip, acc, mod, cre) {				\
	if ((ip)->i_flag & (IN_ACCESS | IN_CHANGE | IN_UPDATE)) {	\
		(ip)->i_flag |= IN_MODIFIED;				\
		if ((ip)->i_flag & IN_ACCESS) {				\
			(ip)->i_e2fs_atime = (acc)->tv_sec;		\
		}							\
		if ((ip)->i_flag & IN_UPDATE) {				\
			(ip)->i_e2fs_mtime = (mod)->tv_sec;		\
			(ip)->i_modrev++;				\
		}							\
		if ((ip)->i_flag & IN_CHANGE) {				\
			(ip)->i_e2fs_ctime = (cre)->tv_sec;		\
		}							\
		(ip)->i_flag &= ~(IN_ACCESS | IN_CHANGE | IN_UPDATE);	\
	}								\
}

#define	ITIMES(ip, acc, mod, cre) {			\
	if (IS_EXT2_VNODE((ip)->i_vnode))		\
		EXT2FS_ITIMES(ip, acc, mod, cre)	\
	else						\
		FFS_ITIMES(ip, acc, mod, cre)		\
}

/* Determine if soft dependencies are being done */
#define	DOINGSOFTDEP(vp)	((vp)->v_mount->mnt_flag & MNT_SOFTDEP)

/* This overlays the fid structure (see mount.h). */
struct ufid {
	u_int16_t ufid_len;	/* Length of structure. */
	u_int16_t ufid_pad;	/* Force 32-bit alignment. */
	ino_t	  ufid_ino;	/* File number (ino). */
	int32_t	  ufid_gen;	/* Generation number. */
};
#endif /* _KERNEL */
