/*	$NetBSD: grf.c,v 1.20.4.1 2001/10/10 11:56:47 fvdl Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: grf.c 1.36 93/08/13$
 *
 *	@(#)grf.c	8.4 (Berkeley) 1/12/94
 */

/*
 * Graphics display driver for the X68K machines.
 * This is the hardware-independent portion of the driver.
 * Hardware access is through the machine dependent grf switch routines.
 */

#include "opt_compat_hpux.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/conf.h>
#include <sys/vnode.h>

#include <machine/grfioctl.h>

#include <x68k/dev/grfvar.h>
#include <x68k/dev/itevar.h>

#include <machine/cpu.h>

#ifdef COMPAT_HPUX
#include <compat/hpux/hpux.h>
extern struct emul emul_hpux;
#endif

#include <uvm/uvm_extern.h>
#include <uvm/uvm_map.h>

#include <miscfs/specfs/specdev.h>

#include "ite.h"
#if NITE == 0
#define	iteon(u,f)
#define	iteoff(u,f)
#define	ite_reinit(u)
#endif

#ifdef DEBUG
int grfdebug = 0;
#define GDB_DEVNO	0x01
#define GDB_MMAP	0x02
#define GDB_IOMAP	0x04
#define GDB_LOCK	0x08
#endif

cdev_decl(grf);
int grfon __P((struct vnode *));
int grfoff __P((struct vnode *));
off_t grfaddr __P((struct grf_softc *, off_t));
int grfmap __P((struct vnode *, caddr_t *, struct proc *));
int grfunmap __P((struct vnode *, caddr_t, struct proc *));

extern struct cfdriver grf_cd;

/*ARGSUSED*/
int
grfopen(devvp, flags, mode, p)
	struct vnode *devvp;
	int flags, mode;
	struct proc *p;
{
	dev_t dev = vdev_rdev(devvp);
	int unit = GRFUNIT(dev);
	register struct grf_softc *gp;
	int error = 0;

	if (unit >= grf_cd.cd_ndevs ||
	    (gp = grf_cd.cd_devs[unit]) == NULL ||
	    (gp->g_flags & GF_ALIVE) == 0)
		return (ENXIO);

	if ((gp->g_flags & (GF_OPEN|GF_EXCLUDE)) == (GF_OPEN|GF_EXCLUDE))
		return(EBUSY);
#ifdef COMPAT_HPUX
	/*
	 * XXX: cannot handle both HPUX and BSD processes at the same time
	 */
	if (p->p_emul == &emul_hpux)
		if (gp->g_flags & GF_BSDOPEN)
			return(EBUSY);
		else
			gp->g_flags |= GF_HPUXOPEN;
	else
		if (gp->g_flags & GF_HPUXOPEN)
			return(EBUSY);
		else
			gp->g_flags |= GF_BSDOPEN;
#endif
	vdev_setprivdata(devvp, gp);
	/*
	 * First open.
	 * XXX: always put in graphics mode.
	 */
	error = 0;
	if ((gp->g_flags & GF_OPEN) == 0) {
		gp->g_flags |= GF_OPEN;
		error = grfon(devvp);
	}
	return(error);
}

/*ARGSUSED*/
int
grfclose(devvp, flags, mode, p)
	struct vnode *devvp;
	int flags, mode;
	struct proc *p;
{
	register struct grf_softc *gp = vdev_privdata(devvp);

	if ((gp->g_flags & GF_ALIVE) == 0)
		return (ENXIO);

	(void) grfoff(devvp);
#ifdef COMPAT_HPUX
	(void) grfunlock(gp);
#endif
	gp->g_flags &= GF_ALIVE;
	return(0);
}

/*ARGSUSED*/
int
grfioctl(devvp, cmd, data, flag, p)
	struct vnode *devvp;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	register struct grf_softc *gp = vdev_privdata(devvp);
	dev_t dev = vdev_rdev(devvp);
	int error, unit;

	if ((gp->g_flags & GF_ALIVE) == 0)
		return (ENXIO);

#ifdef COMPAT_HPUX
	if (p->p_emul == &emul_hpux)
		return(hpuxgrfioctl(dev, cmd, data, flag, p));
#endif
	unit = GRFUNIT(dev);
	error = 0;
	switch (cmd) {

	case GRFIOCGINFO:
		bcopy((caddr_t)&gp->g_display, data, sizeof(struct grfinfo));
		break;

	case GRFIOCON:
		error = grfon(devvp);
		break;

	case GRFIOCOFF:
		error = grfoff(devvp);
		break;

	case GRFIOCMAP:
		error = grfmap(devvp, (caddr_t *)data, p);
		break;

	case GRFIOCUNMAP:
		error = grfunmap(devvp, *(caddr_t *)data, p);
		break;

	case GRFSETVMODE:
		error = (*gp->g_sw->gd_mode)(gp, GM_GRFSETVMODE, data);
		if (error == 0)
			ite_reinit(unit);
		break;

	default:
		error = EINVAL;
		break;

	}
	return(error);
}

/*ARGSUSED*/
int
grfpoll(devvp, events, p)
	struct vnode *devvp;
	int events;
	struct proc *p;
{

	return (events & (POLLOUT | POLLWRNORM));
}

/*ARGSUSED*/
paddr_t
grfmmap(devvp, off, prot)
	struct vnode *devvp;
	off_t off;
	int prot;
{

	return (grfaddr(vdev_privdata(devvp), off));
}

int
grfon(devvp)
	struct vnode *devvp;
{
	struct grf_softc *gp = vdev_privdata(devvp);
	dev_t dev = vdev_rdev(devvp);
	int unit = GRFUNIT(dev);

	/*
	 * XXX: iteoff call relies on devices being in same order
	 * as ITEs and the fact that iteoff only uses the minor part
	 * of the dev arg.
	 */
	iteoff(unit, 2);
	return((*gp->g_sw->gd_mode)(gp,
				    (dev&GRFOVDEV) ? GM_GRFOVON : GM_GRFON,
				    (caddr_t)0));
}

int
grfoff(devvp)
	struct vnode *devvp;
{
	struct grf_softc *gp = vdev_privdata(devvp);
	dev_t dev = vdev_rdev(devvp);
	int unit = GRFUNIT(dev);
	int error;

	(void) grfunmap(devvp, (caddr_t)0, curproc);
	error = (*gp->g_sw->gd_mode)(gp,
				     (dev&GRFOVDEV) ? GM_GRFOVOFF : GM_GRFOFF,
				     (caddr_t)0);
	/* XXX: see comment for iteoff above */
	iteon(unit, 2);
	return(error);
}

off_t
grfaddr(gp, off)
	struct grf_softc *gp;
	off_t off;
{
	register struct grfinfo *gi = &gp->g_display;

	/* control registers */
	if (off >= 0 && off < gi->gd_regsize)
		return(((u_int)gi->gd_regaddr + off) >> PGSHIFT);

	/* frame buffer */
	if (off >= gi->gd_regsize && off < gi->gd_regsize+gi->gd_fbsize) {
		off -= gi->gd_regsize;
		return(((u_int)gi->gd_fbaddr + off) >> PGSHIFT);
	}
	/* bogus */
	return(-1);
}

/*
 * HP-UX compatibility routines
 */
#ifdef COMPAT_HPUX

/*ARGSUSED*/
int
hpuxgrfioctl(devvp, cmd, data, flag, p)
	struct vnode *devvp;
	int cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	register struct grf_softc *gp = vdev_privdata(devvp);
	int error;

	error = 0;
	switch (cmd) {

	case GCID:
		*(int *)data = gp->g_display.gd_id;
		break;

	case GCON:
		error = grfon(devvp);
		break;

	case GCOFF:
		error = grfoff(devvp);
		break;

	case GCLOCK:
		error = grflock(gp, 1);
		break;

	case GCUNLOCK:
		error = grfunlock(gp);
		break;

	case GCAON:
	case GCAOFF:
		break;

	/* GCSTATIC is implied by our implementation */
	case GCSTATIC_CMAP:
	case GCVARIABLE_CMAP:
		break;

	/* map in control regs and frame buffer */
	case GCMAP:
		error = grfmap(devvp, (caddr_t *)data, p);
		break;

	case GCUNMAP:
		error = grfunmap(devvp, *(caddr_t *)data, p);
		/* XXX: HP-UX uses GCUNMAP to get rid of GCSLOT memory */
		if (error)
			error = grflckunmmap(devvp, *(caddr_t *)data);
		break;

	case GCSLOT:
	{
		struct grf_slot *sp = (struct grf_slot *)data;

		sp->slot = grffindpid(gp);
		if (sp->slot) {
			error = grflckmmap(devvp, (caddr_t *)&sp->addr);
			if (error && gp->g_pid) {
				free((caddr_t)gp->g_pid, M_DEVBUF);
				gp->g_pid = NULL;
			}
		} else
			error = EINVAL;		/* XXX */
		break;
	}

	case GCDESCRIBE:
		error = (*gp->g_sw->gd_mode)(gp, GM_DESCRIBE, data);
		break;

	/*
	 * XXX: only used right now to map in rbox control registers
	 * Will be replaced in the future with a real IOMAP interface.
	 */
	case IOMAPMAP:
		error = iommap(devvp, (caddr_t *)data);
#if 0
		/*
		 * It may not be worth kludging this (using p_devtmp) to
		 * make this work.  It was an undocumented side-effect
		 * in HP-UX that the mapped address was the return value
		 * of the ioctl.  The only thing I remember that counted
		 * on this behavior was the rbox X10 server.
		 */
		if (!error)
			u.u_r.r_val1 = *(int *)data;	/* XXX: this sux */
#endif
		break;

	case IOMAPUNMAP:
		error = iounmmap(devvp, *(caddr_t *)data);
		break;

	default:
		error = EINVAL;
		break;
	}
	return(error);
}

int
grflock(gp, block)
	register struct grf_softc *gp;
	int block;
{
	struct proc *p = curproc;		/* XXX */
	int error;
	extern char devioc[];

#ifdef DEBUG
	if (grfdebug & GDB_LOCK)
		printf("grflock(%d): flags %x lockpid %x\n",
		       p->p_pid, gp->g_flags,
		       gp->g_lockp ? gp->g_lockp->p_pid : -1);
#endif
	if (gp->g_pid) {
#ifdef DEBUG
		if (grfdebug & GDB_LOCK)
			printf(" lockpslot %d lockslot %d lock[lockslot] %d\n",
			       gp->g_lock->gl_lockslot, gp->g_lockpslot,
			       gp->g_lock->gl_locks[gp->g_lockpslot]);
#endif
		gp->g_lock->gl_lockslot = 0;
		if (gp->g_lock->gl_locks[gp->g_lockpslot] == 0) {
			gp->g_lockp = NULL;
			gp->g_lockpslot = 0;
		}
	}
	if (gp->g_lockp) {
		if (gp->g_lockp == p)
			return(EBUSY);
		if (!block)
			return(OEAGAIN);
		do {
			gp->g_flags |= GF_WANTED;
			if ((error = tsleep((caddr_t)&gp->g_flags,
					   (PZERO+1) | PCATCH, devioc, 0)))
				return (error);
		} while (gp->g_lockp);
	}
	gp->g_lockp = p;
	if (gp->g_pid) {
		int slot = grffindpid(gp);

#ifdef DEBUG
		if (grfdebug & GDB_LOCK)
			printf("  slot %d\n", slot);
#endif
		gp->g_lockpslot = gp->g_lock->gl_lockslot = slot;
		gp->g_lock->gl_locks[slot] = 1;
	}
	return(0);
}

int
grfunlock(gp)
	register struct grf_softc *gp;
{
#ifdef DEBUG
	if (grfdebug & GDB_LOCK)
		printf("grfunlock(%d): flags %x lockpid %d\n",
		       curproc->p_pid, gp->g_flags,
		       gp->g_lockp ? gp->g_lockp->p_pid : -1);
#endif
	if (gp->g_lockp != curproc)
		return(EBUSY);
	if (gp->g_pid) {
#ifdef DEBUG
		if (grfdebug & GDB_LOCK)
			printf(" lockpslot %d lockslot %d lock[lockslot] %d\n",
			       gp->g_lock->gl_lockslot, gp->g_lockpslot,
			       gp->g_lock->gl_locks[gp->g_lockpslot]);
#endif
		gp->g_lock->gl_locks[gp->g_lockpslot] = 0;
		gp->g_lockpslot = gp->g_lock->gl_lockslot = 0;
	}
	if (gp->g_flags & GF_WANTED) {
		wakeup((caddr_t)&gp->g_flags); 
		gp->g_flags &= ~GF_WANTED;
	}
	gp->g_lockp = NULL;
	return(0);
}

/*
 * Convert a BSD style minor devno to HPUX style.
 * We cannot just create HPUX style nodes as they require 24 bits
 * of minor device number and we only have 8.
 * XXX: This may give the wrong result for remote stats of other
 * machines where device 10 exists.
 */
int
grfdevno(dev)
	dev_t dev;
{
	int unit = GRFUNIT(dev);
	struct grf_softc *gp;
	int newdev;

	if (unit >= grf_cd.cd_ndevs ||
	    (gp = grf_cd.cd_devs[unit]) == NULL ||
	    (gp->g_flags&GF_ALIVE) == 0)
		return(bsdtohpuxdev(dev));
	/* magic major number */
	newdev = 12 << 24;
	/* now construct minor number */
	if (gp->g_display.gd_regaddr != (caddr_t)GRFIADDR) {
		int sc = patosc(gp->g_display.gd_regaddr);
		newdev |= (sc << 16) | 0x200;
	}
	if (dev & GRFIMDEV)
		newdev |= 0x02;
	else if (dev & GRFOVDEV)
		newdev |= 0x01;
#ifdef DEBUG
	if (grfdebug & GDB_DEVNO)
		printf("grfdevno: dev %x newdev %x\n", dev, newdev);
#endif
	return(newdev);
}

#endif	/* COMPAT_HPUX */

int
grfmap(devvp, addrp, p)
	struct vnode *devvp;
	caddr_t *addrp;
	struct proc *p;
{
	struct grf_softc *gp = vdev_privdata(devvp);
	int len, error;
	int flags;

#ifdef DEBUG
	if (grfdebug & GDB_MMAP)
		printf("grfmap(%d): addr %p\n", p->p_pid, *addrp);
#endif
	len = gp->g_display.gd_regsize + gp->g_display.gd_fbsize;
	flags = MAP_SHARED;
	if (*addrp)
		flags |= MAP_FIXED;
	else
		*addrp = (caddr_t)0x1000000;	/* XXX */
	error = uvm_mmap(&p->p_vmspace->vm_map, (vaddr_t *)addrp,
			 (vsize_t)len, VM_PROT_ALL, VM_PROT_ALL,
			 flags, devvp, 0,
			 p->p_rlimit[RLIMIT_MEMLOCK].rlim_cur);
	if (error == 0)
		(void) (*gp->g_sw->gd_mode)(gp, GM_MAP, *addrp);
	return(error);
}

int
grfunmap(devvp, addr, p)
	struct vnode *devvp;
	caddr_t addr;
	struct proc *p;
{
	struct grf_softc *gp = vdev_privdata(devvp);
	vsize_t size;

#ifdef DEBUG
	if (grfdebug & GDB_MMAP)
		printf("grfunmap(%d): dev %x addr %p\n", p->p_pid, dev, addr);
#endif
	if (addr == 0)
		return(EINVAL);		/* XXX: how do we deal with this? */
	(void) (*gp->g_sw->gd_mode)(gp, GM_UNMAP, 0);
	size = round_page(gp->g_display.gd_regsize + gp->g_display.gd_fbsize);
	uvm_unmap(&p->p_vmspace->vm_map, (vaddr_t)addr,
	    (vaddr_t)addr + size);
	return 0;
}

#ifdef COMPAT_HPUX
int
iommap(devvp, addrp)
	struct vnode *devvp;
	caddr_t *addrp;
{

#ifdef DEBUG
	if (grfdebug & (GDB_MMAP|GDB_IOMAP))
		printf("iommap(%d): addr %p\n", curproc->p_pid, *addrp);
#endif
	return(EINVAL);
}

int
iounmmap(devvp, addr)
	struct vnode *devvp;
	caddr_t addr;
{
#ifdef DEBUG
	int unit = minor(vdev_rdev(devvp));

	if (grfdebug & (GDB_MMAP|GDB_IOMAP))
		printf("iounmmap(%d): id %d addr %p\n",
		       curproc->p_pid, unit, addr);
#endif
	return(0);
}

/*
 * Processes involved in framebuffer mapping via GCSLOT are recorded in
 * an array of pids.  The first element is used to record the last slot used
 * (for faster lookups).  The remaining elements record up to GRFMAXLCK-1
 * process ids.  Returns a slot number between 1 and GRFMAXLCK or 0 if no
 * slot is available. 
 */
int
grffindpid(gp)
	struct grf_softc *gp;
{
	register short pid, *sp;
	register int i, limit;
	int ni;

	if (gp->g_pid == NULL) {
		gp->g_pid = (short *)
			malloc(GRFMAXLCK * sizeof(short), M_DEVBUF, M_WAITOK);
		bzero((caddr_t)gp->g_pid, GRFMAXLCK * sizeof(short));
	}
	pid = curproc->p_pid;
	ni = limit = gp->g_pid[0];
	for (i = 1, sp = &gp->g_pid[1]; i <= limit; i++, sp++) {
		if (*sp == pid)
			goto done;
		if (*sp == 0)
			ni = i;
	}
	i = ni;
	if (i < limit) {
		gp->g_pid[i] = pid;
		goto done;
	}
	if (++i == GRFMAXLCK)
		return(0);
	gp->g_pid[0] = i;
	gp->g_pid[i] = pid;
done:
#ifdef DEBUG
	if (grfdebug & GDB_LOCK)
		printf("grffindpid(%d): slot %d of %d\n",
		       pid, i, gp->g_pid[0]);
#endif
	return(i);
}

void
grfrmpid(gp)
	struct grf_softc *gp;
{
	register short pid, *sp;
	register int limit, i;
	int mi;

	if (gp->g_pid == NULL || (limit = gp->g_pid[0]) == 0)
		return;
	pid = curproc->p_pid;
	limit = gp->g_pid[0];
	mi = 0;
	for (i = 1, sp = &gp->g_pid[1]; i <= limit; i++, sp++) {
		if (*sp == pid)
			*sp = 0;
		else if (*sp)
			mi = i;
	}
	i = mi;
	if (i < limit)
		gp->g_pid[0] = i;
#ifdef DEBUG
	if (grfdebug & GDB_LOCK)
		printf("grfrmpid(%d): slot %d of %d\n",
		       pid, sp-gp->g_pid, gp->g_pid[0]);
#endif
}

int
grflckmmap(devvp, addrp)
	stryct vnode *devvp;
	caddr_t *addrp;
{
#ifdef DEBUG
	struct proc *p = curproc;		/* XXX */

	if (grfdebug & (GDB_MMAP|GDB_LOCK))
		printf("grflckmmap(%d): addr %p\n",
		       p->p_pid, *addrp);
#endif
	return(EINVAL);
}

int
grflckunmmap(devvp, addr)
	struct vnode *devvp;
	caddr_t addr;
{
#ifdef DEBUG
	int unit = minor(vdev_rdev(dev));

	if (grfdebug & (GDB_MMAP|GDB_LOCK))
		printf("grflckunmmap(%d): id %d addr %p\n",
		       curproc->p_pid, unit, addr);
#endif
	return(EINVAL);
}
#endif	/* COMPAT_HPUX */
