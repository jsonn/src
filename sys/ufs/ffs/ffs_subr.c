/*	$NetBSD: ffs_subr.c,v 1.15.6.6 2002/08/01 02:47:03 nathanw Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)ffs_subr.c	8.5 (Berkeley) 3/21/95
 */

#include <sys/cdefs.h>
#if defined(__KERNEL_RCSID)
__KERNEL_RCSID(0, "$NetBSD: ffs_subr.c,v 1.15.6.6 2002/08/01 02:47:03 nathanw Exp $");
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/param.h>

/* in ffs_tables.c */
extern int inside[], around[];
extern u_char *fragtbl[];

#ifndef _KERNEL
#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>
#include <ufs/ufs/ufs_bswap.h>
void    panic __P((const char *, ...))
    __attribute__((__noreturn__,__format__(__printf__,1,2)));

#else	/* _KERNEL */
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>
#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>
#include <ufs/ufs/ufs_bswap.h>

/*
 * Return buffer with the contents of block "offset" from the beginning of
 * directory "ip".  If "res" is non-zero, fill it in with a pointer to the
 * remaining space in the directory.
 */
int
ffs_blkatoff(v)
	void *v;
{
	struct vop_blkatoff_args /* {
		struct vnode *a_vp;
		off_t a_offset;
		char **a_res;
		struct buf **a_bpp;
	} */ *ap = v;
	struct inode *ip;
	struct fs *fs;
	struct buf *bp;
	ufs_daddr_t lbn;
	int bsize, error;

	ip = VTOI(ap->a_vp);
	fs = ip->i_fs;
	lbn = lblkno(fs, ap->a_offset);
	bsize = blksize(fs, ip, lbn);

	*ap->a_bpp = NULL;
	if ((error = bread(ap->a_vp, lbn, bsize, NOCRED, &bp)) != 0) {
		brelse(bp);
		return (error);
	}
	if (ap->a_res)
		*ap->a_res = (char *)bp->b_data + blkoff(fs, ap->a_offset);
	*ap->a_bpp = bp;
	return (0);
}
#endif	/* _KERNEL */

/*
 * Update the frsum fields to reflect addition or deletion 
 * of some frags.
 */
void
ffs_fragacct(fs, fragmap, fraglist, cnt, needswap)
	struct fs *fs;
	int fragmap;
	int32_t fraglist[];
	int cnt;
	int needswap;
{
	int inblk;
	int field, subfield;
	int siz, pos;

	inblk = (int)(fragtbl[fs->fs_frag][fragmap]) << 1;
	fragmap <<= 1;
	for (siz = 1; siz < fs->fs_frag; siz++) {
		if ((inblk & (1 << (siz + (fs->fs_frag & (NBBY - 1))))) == 0)
			continue;
		field = around[siz];
		subfield = inside[siz];
		for (pos = siz; pos <= fs->fs_frag; pos++) {
			if ((fragmap & field) == subfield) {
				fraglist[siz] = ufs_rw32(
				    ufs_rw32(fraglist[siz], needswap) + cnt,
				    needswap);
				pos += siz;
				field <<= siz;
				subfield <<= siz;
			}
			field <<= 1;
			subfield <<= 1;
		}
	}
}

#if defined(_KERNEL) && defined(DIAGNOSTIC)
void
ffs_checkoverlap(bp, ip)
	struct buf *bp;
	struct inode *ip;
{
	struct buf *ebp, *ep;
	ufs_daddr_t start, last;
	struct vnode *vp;

	ebp = &buf[nbuf];
	start = bp->b_blkno;
	last = start + btodb(bp->b_bcount) - 1;
	for (ep = buf; ep < ebp; ep++) {
		if (ep == bp || (ep->b_flags & B_INVAL) ||
		    ep->b_vp == NULLVP)
			continue;
		if (VOP_BMAP(ep->b_vp, (daddr_t)0, &vp, (daddr_t)0, NULL))
			continue;
		if (vp != ip->i_devvp)
			continue;
		/* look for overlap */
		if (ep->b_bcount == 0 || ep->b_blkno > last ||
		    ep->b_blkno + btodb(ep->b_bcount) <= start)
			continue;
		vprint("Disk overlap", vp);
		printf("\tstart %d, end %d overlap start %d, end %ld\n",
		    start, last, ep->b_blkno,
		    (long) ep->b_blkno + btodb(ep->b_bcount) - 1);
		panic("Disk buffer overlap");
	}
}
#endif /* _KERNEL && DIAGNOSTIC */

/*
 * block operations
 *
 * check if a block is available
 */
int
ffs_isblock(fs, cp, h)
	struct fs *fs;
	u_char *cp;
	ufs_daddr_t h;
{
	u_char mask;

	switch ((int)fs->fs_fragshift) {
	case 3:
		return (cp[h] == 0xff);
	case 2:
		mask = 0x0f << ((h & 0x1) << 2);
		return ((cp[h >> 1] & mask) == mask);
	case 1:
		mask = 0x03 << ((h & 0x3) << 1);
		return ((cp[h >> 2] & mask) == mask);
	case 0:
		mask = 0x01 << (h & 0x7);
		return ((cp[h >> 3] & mask) == mask);
	default:
		panic("ffs_isblock: unknown fs_fragshift %d",
		    (int)fs->fs_fragshift);
	}
}

/*
 * check if a block is free
 */
int
ffs_isfreeblock(fs, cp, h)
	struct fs *fs;
	u_char *cp;
	ufs_daddr_t h;
{

	switch ((int)fs->fs_fragshift) {
	case 3:
		return (cp[h] == 0);
	case 2:
		return ((cp[h >> 1] & (0x0f << ((h & 0x1) << 2))) == 0);
	case 1:
		return ((cp[h >> 2] & (0x03 << ((h & 0x3) << 1))) == 0);
	case 0:
		return ((cp[h >> 3] & (0x01 << (h & 0x7))) == 0);
	default:
		panic("ffs_isfreeblock: unknown fs_fragshift %d",
		    (int)fs->fs_fragshift);
	}
}

/*
 * take a block out of the map
 */
void
ffs_clrblock(fs, cp, h)
	struct fs *fs;
	u_char *cp;
	ufs_daddr_t h;
{

	switch ((int)fs->fs_fragshift) {
	case 3:
		cp[h] = 0;
		return;
	case 2:
		cp[h >> 1] &= ~(0x0f << ((h & 0x1) << 2));
		return;
	case 1:
		cp[h >> 2] &= ~(0x03 << ((h & 0x3) << 1));
		return;
	case 0:
		cp[h >> 3] &= ~(0x01 << (h & 0x7));
		return;
	default:
		panic("ffs_clrblock: unknown fs_fragshift %d",
		    (int)fs->fs_fragshift);
	}
}

/*
 * put a block into the map
 */
void
ffs_setblock(fs, cp, h)
	struct fs *fs;
	u_char *cp;
	ufs_daddr_t h;
{

	switch ((int)fs->fs_fragshift) {
	case 3:
		cp[h] = 0xff;
		return;
	case 2:
		cp[h >> 1] |= (0x0f << ((h & 0x1) << 2));
		return;
	case 1:
		cp[h >> 2] |= (0x03 << ((h & 0x3) << 1));
		return;
	case 0:
		cp[h >> 3] |= (0x01 << (h & 0x7));
		return;
	default:
		panic("ffs_setblock: unknown fs_fragshift %d",
		    (int)fs->fs_fragshift);
	}
}
