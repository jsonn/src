/*	$NetBSD: vm_swap.c,v 1.37 1997/04/17 00:12:50 thorpej Exp $	*/

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
 *	@(#)vm_swap.c	8.5 (Berkeley) 2/17/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/map.h>
#include <sys/file.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <vm/vm.h>
#include <vm/vm_conf.h>

#include <miscfs/specfs/specdev.h>

/*
 * Indirect driver for multi-controller paging.
 */

int	nswap, nswdev;
#ifdef SEQSWAP
int	niswdev;		/* number of interleaved swap devices */
int	niswap;			/* size of interleaved swap area */
#endif
extern	int dmmax;

int swfree __P((struct proc *, int));

/*
 * Set up swap devices.
 * Initialize linked list of free swap
 * headers. These do not actually point
 * to buffers, but rather to pages that
 * are being swapped in and out.
 */
void
swapinit()
{
	register int i;
	register struct buf *sp = swbuf;
	register struct proc *p = &proc0;	/* XXX */
	struct swdevt *swp;
	int error;

	/*
	 * Count swap devices, and adjust total swap space available.
	 * Some of the space will not be countable until later (dynamically
	 * configurable devices) and some of the counted space will not be
	 * available until a swapon() system call is issued, both usually
	 * happen when the system goes multi-user.
	 *
	 * If using NFS for swap, swdevt[0] will already be bdevvp'd.	XXX
	 */
#ifdef SEQSWAP
	nswdev = niswdev = 0;
	nswap = niswap = 0;
	/*
	 * All interleaved devices must come first
	 */
	for (swp = swdevt; swp->sw_dev != NODEV || swp->sw_vp != NULL; swp++) {
		if (swp->sw_flags & SW_SEQUENTIAL)
			break;
		niswdev++;
		if (swp->sw_nblks > niswap)
			niswap = swp->sw_nblks;
	}
	niswap = roundup(niswap, dmmax);
	niswap *= niswdev;
	if (niswdev != 0 && swdevt[0].sw_vp == NULL &&
	    bdevvp(swdevt[0].sw_dev, &swdevt[0].sw_vp))
		panic("swapvp");
	/*
	 * The remainder must be sequential
	 */
	for ( ; swp->sw_dev != NODEV; swp++) {
		if ((swp->sw_flags & SW_SEQUENTIAL) == 0)
			panic("binit: mis-ordered swap devices");
		nswdev++;
		if (swp->sw_nblks > 0) {
			if (swp->sw_nblks % dmmax)
				swp->sw_nblks -= (swp->sw_nblks % dmmax);
			nswap += swp->sw_nblks;
		}
	}
	nswdev += niswdev;
	nswap += niswap;
#else
	nswdev = 0;
	nswap = 0;
	for (swp = swdevt; swp->sw_dev != NODEV || swp->sw_vp != NULL; swp++) {
		nswdev++;
		if (swp->sw_nblks > nswap)
			nswap = swp->sw_nblks;
	}
	if (nswdev > 1)
		nswap = ((nswap + dmmax - 1) / dmmax) * dmmax;
	nswap *= nswdev;
	if (nswdev != 0 && swdevt[0].sw_vp == NULL &&
	    bdevvp(swdevt[0].sw_dev, &swdevt[0].sw_vp))
		panic("swapvp");
#endif
	if (nswap == 0)
		printf("WARNING: no swap space found\n");
	else if ((error = swfree(p, 0)) == ENXIO)
		printf("WARNING: primary swap device not configured\n");
	else if (error) {
		printf("swfree errno %d\n", error);	/* XXX */
		panic("swapinit swfree 0");
	}

	/*
	 * Now set up swap buffer headers.
	 */
	bswlist.b_actf = sp;
	for (i = 0; i < nswbuf - 1; i++, sp++) {
		sp->b_actf = sp + 1;
		sp->b_rcred = sp->b_wcred = p->p_ucred;
		sp->b_vnbufs.le_next = NOLIST;
	}
	sp->b_rcred = sp->b_wcred = p->p_ucred;
	sp->b_vnbufs.le_next = NOLIST;
	sp->b_actf = NULL;
}

void
swstrategy(bp)
	register struct buf *bp;
{
	int sz, off, seg, index;
	register struct swdevt *sp;
	struct vnode *vp;

	sz = howmany(bp->b_bcount, DEV_BSIZE);
	if (bp->b_blkno + sz > nswap) {
		bp->b_error = EINVAL;
		bp->b_flags |= B_ERROR;
		biodone(bp);
		return;
	}
	if (nswdev > 1) {
#ifdef SEQSWAP
		if (bp->b_blkno < niswap) {
			if (niswdev > 1) {
				off = bp->b_blkno % dmmax;
				if (off+sz > dmmax) {
					bp->b_error = EINVAL;
					bp->b_flags |= B_ERROR;
					biodone(bp);
					return;
				}
				seg = bp->b_blkno / dmmax;
				index = seg % niswdev;
				seg /= niswdev;
				bp->b_blkno = seg*dmmax + off;
			} else
				index = 0;
		} else {
			register struct swdevt *swp;

			bp->b_blkno -= niswap;
			for (index = niswdev, swp = &swdevt[niswdev];
			     swp->sw_dev != NODEV;
			     swp++, index++) {
				if (bp->b_blkno < swp->sw_nblks)
					break;
				bp->b_blkno -= swp->sw_nblks;
			}
			if (swp->sw_dev == NODEV ||
			    bp->b_blkno+sz > swp->sw_nblks) {
				bp->b_error = swp->sw_dev == NODEV ?
					ENODEV : EINVAL;
				bp->b_flags |= B_ERROR;
				biodone(bp);
				return;
			}
		}
#else
		off = bp->b_blkno % dmmax;
		if (off+sz > dmmax) {
			bp->b_error = EINVAL;
			bp->b_flags |= B_ERROR;
			biodone(bp);
			return;
		}
		seg = bp->b_blkno / dmmax;
		index = seg % nswdev;
		seg /= nswdev;
		bp->b_blkno = seg*dmmax + off;
#endif
	} else
		index = 0;
	sp = &swdevt[index];
	if (sp->sw_vp == NULL) {
		bp->b_error = ENODEV;
		bp->b_flags |= B_ERROR;
		biodone(bp);
		return;
	}
	if ((bp->b_dev = sp->sw_dev) == NODEV && sp->sw_vp->v_type != VREG)
		panic("swstrategy");
	VHOLD(sp->sw_vp);
	if ((bp->b_flags & B_READ) == 0) {
		if ((vp = bp->b_vp) != NULL) {
			vp->v_numoutput--;
			if ((vp->v_flag & VBWAIT) && vp->v_numoutput <= 0) {
				vp->v_flag &= ~VBWAIT;
				wakeup((caddr_t)&vp->v_numoutput);
			}
		}
		sp->sw_vp->v_numoutput++;
	}
	if (bp->b_vp != NULL)
		brelvp(bp);
	bp->b_vp = sp->sw_vp;
	VOP_STRATEGY(bp);
}

/*ARGSUSED*/
int
swread(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{

	return (physio(swstrategy, NULL, dev, B_READ, minphys, uio));
}

/*ARGSUSED*/
int
swwrite(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{

	return (physio(swstrategy, NULL, dev, B_WRITE, minphys, uio));
}

/*
 * System call swapon(name) enables swapping on device name,
 * which must be in the swdevsw.  Return EBUSY
 * if already swapping on this device.
 */
/* ARGSUSED */
int
sys_swapon(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_swapon_args /* {
		syscallarg(char *) name;
	} */ *uap = v;
	register struct vnode *vp;
	register struct swdevt *sp;
	dev_t dev;
	int error;
	struct nameidata nd;

	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, name), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	vp = nd.ni_vp;
	if (vp->v_type != VBLK) {
		vrele(vp);
		return (ENOTBLK);
	}
	dev = (dev_t)vp->v_rdev;
	if (major(dev) >= nblkdev) {
		vrele(vp);
		return (ENXIO);
	}
	for (sp = &swdevt[0]; sp->sw_dev != NODEV; sp++) {
		if (sp->sw_dev == dev) {
			if (sp->sw_flags & SW_FREED) {
				vrele(vp);
				return (EBUSY);
			}
			sp->sw_vp = vp;
			if ((error = swfree(p, sp - swdevt)) != 0) {
				vrele(vp);
				return (error);
			}
			return (0);
		}
#ifdef SEQSWAP
		/*
		 * If we have reached a non-freed sequential device without
		 * finding what we are looking for, it is an error.
		 * That is because all interleaved devices must come first
		 * and sequential devices must be freed in order.
		 */
		if ((sp->sw_flags & (SW_SEQUENTIAL|SW_FREED)) == SW_SEQUENTIAL)
			break;
#endif
	}
	vrele(vp);
	return (EINVAL);
}

/*
 * Swfree(index) frees the index'th portion of the swap map.
 * Each of the nswdev devices provides 1/nswdev'th of the swap
 * space, which is laid out with blocks of dmmax pages circularly
 * among the devices.
 */
int
swfree(p, index)
	struct proc *p;
	int index;
{
	register struct swdevt *sp;
	register swblk_t vsbase;
	register long blk;
	struct vnode *vp;
	register swblk_t dvbase;
	register int nblks;
	int error;

	sp = &swdevt[index];
	vp = sp->sw_vp;
	/* If root on swap, then the skip open/close operations. */
	if (vp != rootvp) {
		if ((error = VOP_OPEN(vp, FREAD|FWRITE, p->p_ucred, p)) != 0)
			return (error);
	}
	sp->sw_flags |= SW_FREED;
	nblks = sp->sw_nblks;
	/*
	 * Some devices may not exist til after boot time.
	 * If so, their nblk count will be 0.
	 */
	if (nblks <= 0) {
		int perdev;
		dev_t dev = sp->sw_dev;

		if (bdevsw[major(dev)].d_psize == 0 ||
		    (nblks = (*bdevsw[major(dev)].d_psize)(dev)) == -1) {
			if (vp != rootvp)
				(void) VOP_CLOSE(vp, FREAD|FWRITE, p->p_ucred, p);
			sp->sw_flags &= ~SW_FREED;
			return (ENXIO);
		}
#ifdef SEQSWAP
		if (index < niswdev) {
			perdev = niswap / niswdev;
			if (nblks > perdev)
				nblks = perdev;
		} else {
			if (nblks % dmmax)
				nblks -= (nblks % dmmax);
			nswap += nblks;
		}
#else
		perdev = nswap / nswdev;
		if (nblks > perdev)
			nblks = perdev;
#endif
		sp->sw_nblks = nblks;
	}
	if (nblks == 0) {
		if (vp != rootvp)
			(void) VOP_CLOSE(vp, FREAD|FWRITE, p->p_ucred, p);
		sp->sw_flags &= ~SW_FREED;
		return (0);	/* XXX error? */
	}
#ifdef SEQSWAP
	if (sp->sw_flags & SW_SEQUENTIAL) {
		register struct swdevt *swp;

		blk = niswap;
		for (swp = &swdevt[niswdev]; swp != sp; swp++)
			blk += swp->sw_nblks;
		rmfree(swapmap, nblks, blk);
		return (0);
	}
#endif
	for (dvbase = 0; dvbase < nblks; dvbase += dmmax) {
		blk = nblks - dvbase;
#ifdef SEQSWAP
		if ((vsbase = index*dmmax + dvbase*niswdev) >= niswap)
			panic("swfree");
#else
		if ((vsbase = index*dmmax + dvbase*nswdev) >= nswap)
			panic("swfree");
#endif
		if (blk > dmmax)
			blk = dmmax;
		if (vsbase == 0) {
			/*
			 * First of all chunks... initialize the swapmap.
			 * Don't use the first cluster of the device
			 * in case it starts with a label or boot block.
			 */
			rminit(swapmap, blk - ctod(CLSIZE),
			    vsbase + ctod(CLSIZE), "swap", nswapmap);
		} else if (dvbase == 0) {
			/*
			 * Don't use the first cluster of the device
			 * in case it starts with a label or boot block.
			 */
			rmfree(swapmap, blk - ctod(CLSIZE),
			    vsbase + ctod(CLSIZE));
		} else
			rmfree(swapmap, blk, vsbase);
	}

	/*
	 * Preserve the mini-root if appropriate:
	 * Note: this requires !SEQSWAP && nswdev==1
	 *
	 * A mini-root gets copied into the front of the swap
	 * and we run over top of the swap area just long
	 * enough for us to do a mkfs and restor of the real
	 * root (sure beats rewriting standalone restor).
	 */
	if (vp == rootvp) {
#ifndef	MINIROOTSIZE
		struct mount *mp;
		struct statfs *sp;
#endif
		long firstblk;
		int rootblks;

#ifdef	MINIROOTSIZE
		rootblks = MINIROOTSIZE;
#else
		/* Get size from root FS (mountroot did statfs) */
		mp = rootvnode->v_mount;
		sp = &mp->mnt_stat;
		rootblks = sp->f_blocks * (sp->f_bsize / DEV_BSIZE);
#endif
		if (rootblks > nblks)
			panic("swfree miniroot size");
		/* First ctod(CLSIZE) blocks are not in the map. */
		firstblk = rmalloc(swapmap, rootblks - ctod(CLSIZE));
		if (firstblk != ctod(CLSIZE))
			panic("swfree miniroot save");
		printf("Preserved %d blocks of miniroot leaving %d pages of swap\n",
		    rootblks, dtoc(nblks - rootblks));
	}

	return (0);
}
