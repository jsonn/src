/*	$NetBSD: adosfs.h,v 1.1.2.2 2002/12/29 19:55:26 thorpej Exp $	*/

/*
 * Copyright (c) 1994 Christian E. Hopps
 * Copyright (c) 1996 Matthias Scheler
 * All rights reserved.
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
 *      This product includes software developed by Christian E. Hopps.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Arguments to mount amigados filesystems.
 */
struct adosfs_args {
	char	*fspec;		/* blocks special holding the fs to mount */
	struct	export_args export;	/* network export information */
	uid_t	uid;		/* uid that owns adosfs files */
	gid_t	gid;		/* gid that owns adosfs files */
	mode_t	mask;		/* mask to be applied for adosfs perms */
};

#ifdef _KERNEL
#include <miscfs/genfs/genfs_node.h>

/*
 * Amigados datestamp. (from 1/1/1978 00:00:00 local)
 */
struct datestamp {
	u_int32_t days;
	u_int32_t mins;
	u_int32_t ticks;	/* 20000 * (ticks % 50) = useconds */
				/* ticks / 50 = seconds */
};

enum anode_type { AROOT, ADIR, AFILE, ALDIR, ALFILE, ASLINK };

/* 
 * similar to inode's, we use to represent:
 * the root dir, reg dirs, reg files and extension blocks
 * note the ``tab'' is a hash table for r/d, and a data block
 * table for f/e. it is always ANODETABSZ(ap) bytes in size.
 */
struct anode {
	struct genfs_node gnode;
	LIST_ENTRY(anode) link;
	enum anode_type type;
	char name[31];		/* (r/d/f) name for object */
	struct datestamp mtimev;	/* (r) volume modified */
	struct datestamp created;	/* (r) volume created */
	struct datestamp mtime;	/* (r/d/f) last modified */
	struct adosfsmount *amp;	/* owner file system */
	struct vnode *vp;	/* owner vnode */
	u_long fsize;		/* (f) size of file in bytes */
	u_long block;		/* block num */
	u_long pblock;		/* (d/f/e) parent block */
	u_long hashf;		/* (d/f) hash forward */
	u_long extb;		/* (f/e) extension block number */
	u_long linkto;		/* (hd/hf) header this link points at */
	u_long linknext;	/* (d/f/hd/hf) next link (or head) in chain */
	u_long lastlindblk;	/* (f/hf) last logical indirect block */
	u_long lastindblk;	/* (f/hf) last indirect block read */
	u_long *tab;		/* (r/d) hash table */
	int *tabi;		/* (r/d) table info */
	int ntabent;		/* (r/d) number of entries in table */
	int nwords;		/* size of blocks in long words */
	int adprot;		/* (d/f) amigados protection bits */
	uid_t  uid;		/* (d/f) uid of directory/file */
	gid_t  gid;		/* (d/f) gid of directory/file */
	int flags;		/* misc flags */ 
	char *slinkto;		/* name of file or dir */
};
#define VTOA(vp)		((struct anode *)(vp)->v_data)
#define ATOV(ap)		((ap)->vp)
#define ANODETABSZ(ap)		(((ap)->nwords - 56) * sizeof(long))
#define ANODETABENT(ap)		((ap)->nwords - 56)
#define ANODENDATBLKENT(ap)	((ap)->nwords - 56)

/*
 * mount data 
 */
#define ANODEHASHSZ (512)

struct adosfsmount {
	LIST_HEAD(anodechain, anode) anodetab[ANODEHASHSZ];
	struct mount *mp;	/* owner mount */
	u_int32_t dostype;	/* type of volume */
	u_long rootb;		/* root block number */
	u_long secsperblk;	/* sectors per block */
	u_long bsize;		/* size of blocks */
	u_long nwords;		/* size of blocks in long words */
	u_long dbsize;		/* data bytes per block */
	uid_t  uid;		/* uid of mounting user */
	gid_t  gid;		/* gid of mounting user */
	u_long mask;		/* mode mask */
	struct vnode *devvp;	/* blk device mounted on */
	struct vnode *rootvp;	/* out root vnode */
	struct netexport export;
	u_long *bitmap;		/* allocation bitmap */
	u_long numblks;		/* number of usable blocks */
	u_long freeblks;	/* number of free blocks */
};

#define VFSTOADOSFS(mp) ((struct adosfsmount *)(mp)->mnt_data)

#define IS_FFS(amp)	((amp)->dostype & 1)
#define IS_INTER(amp)	(((amp)->dostype & 7) > 1)

/*
 * AmigaDOS block stuff.
 */
#define BBOFF		(0)

#define BPT_SHORT	((u_int32_t)2)
#define BPT_DATA	((u_int32_t)8)
#define BPT_LIST	((u_int32_t)16)

#define BST_RDIR	((u_int32_t)1)
#define BST_UDIR	((u_int32_t)2)
#define BST_SLINK	((u_int32_t)3)
#define BST_LDIR	((u_int32_t)4)
#define BST_FILE	((u_int32_t)-3)
#define BST_LFILE	((u_int32_t)-4)

#define	OFS_DATA_OFFSET	(24)

extern struct pool adosfs_node_pool;

/*
 * utility protos
 */
#if BYTE_ORDER != BIG_ENDIAN
u_int32_t adoswordn __P((struct buf *, int));
#else
#define adoswordn(bp,wn) (*((u_int32_t *)(bp)->b_data + (wn)))
#endif

u_int32_t adoscksum __P((struct buf *, int));
int adoscaseequ __P((const u_char *, const u_char *, int, int));
int adoshash __P((const u_char *, int, int, int));
int adunixprot __P((int));
int adosfs_getblktype __P((struct adosfsmount *, struct buf *));

struct vnode *adosfs_ahashget __P((struct mount *, ino_t));
void adosfs_ainshash __P((struct adosfsmount *, struct anode *));
void adosfs_aremhash __P((struct anode *));

int adosfs_lookup __P((void *));

extern int (**adosfs_vnodeop_p) __P((void *));
#endif /* _KERNEL */
