/*	$NetBSD: ramdisk.c,v 1.1.2.1 1995/11/17 23:34:25 gwr Exp $	*/

/*
 * Copyright (c) 1995 Gordon W. Ross, Leo Weppelman.
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 4. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by
 *			Gordon W. Ross and Leo Weppelman.
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
 * This implements a general-puspose RAM-disk.
 * See ramdisk.h for notes on the config types.
 *
 * Note that this driver provides the same functionality
 * as the MFS filesystem hack, but this is better because
 * you can use this for any filesystem type you'd like!
 *
 * Credit for most of the kmem ramdisk code goes to:
 *   Leo Weppelman (atari) and Phil Nelson (pc532)
 * Credit for the ideas behind the "user space RAM" code goes
 * to the authors of the MFS implementation.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
/* Don't want all those other VM headers... */
extern vm_offset_t	 kmem_alloc __P((vm_map_t, vm_size_t));

#include <dev/ramdisk.h>

/*
 * By default, include the user-space functionality.
 * Use:  option RAMDISK_SERVER=0 to turn it off.
 */
#ifndef RAMDISK_SERVER
#define	RAMDISK_SERVER 1
#endif

/*
 * XXX: the "control" unit is (base unit + 16).
 * We should just use the cdev as the "control", but
 * that interferes with the security stuff preventing
 * simulatneous use of raw and block devices.
 *
 * XXX Assumption: 16 RAM-disks are enough!
 */
#define RD_IS_CTRL(unit) (unit & 0x10)
#define RD_UNIT(unit)    (unit &  0xF)

/*
 * XXX -  This is just for a sanity check.  Only
 * applies to kernel-space RAM disk allocations.
 */
#define RD_KMEM_MAX_SIZE	0x100000	/* 1MB */

/* autoconfig stuff... */

struct rd_softc {
	struct device sc_dev;	/* REQUIRED first entry */
	struct rd_conf sc_rd;
	struct buf *sc_buflist;
	int sc_flags;
};
/* shorthand for fields in sc_rd: */
#define sc_addr sc_rd.rd_addr
#define sc_size sc_rd.rd_size
#define sc_type sc_rd.rd_type
/* flags */
#define RD_ISOPEN	0x01
#define RD_SERVED	0x02

static int  rd_match (struct device *, void *self, void *);
static void rd_attach(struct device *, struct device *self, void *);

struct cfdriver rdcd = {
	NULL, "rd", rd_match, rd_attach,
	DV_DULL, sizeof(struct rd_softc), NULL, 0 };

static int
rd_match(parent, self, aux)
	struct device	*parent;
	void	*self;
	void	*aux;
{
	return(1);
}

static void
rd_attach(parent, self, aux)
	struct device	*parent, *self;
	void		*aux;
{
	struct rd_softc *sc = (struct rd_softc *)self;

	/* XXX - Could accept aux info here to set the config. */
#ifdef	RAMDISK_HOOKS
	/*
	 * This external function might setup a pre-loaded disk.
	 * All it would need to do is setup the rd_conf struct.
	 * See sys/arch/sun3/dev/rd_root.c for an example.
	 */
	rd_attach_hook(sc->sc_dev.dv_unit, &sc->sc_rd);
#endif
	printf("\n");
}

/*
 * operational routines:
 * open, close, read, write, strategy,
 * ioctl, dump, size
 */

void rdstrategy __P((struct buf *bp));

#if RAMDISK_SERVER
static int rd_server_loop __P((struct rd_softc *sc));
static int rd_ioctl_server __P((struct rd_softc *sc,
		struct rd_conf *urd, struct proc *proc));
#endif

int rddump(dev, blkno, va, size)
	dev_t dev;
	daddr_t blkno;
	caddr_t va;
	size_t size;
{
	return ENODEV;
}

int rdsize(dev_t dev)
{
	int unit;
	struct rd_softc *sc;

	/* Disallow control units. */
	unit = minor(dev);
	if (unit >= rdcd.cd_ndevs)
		return 0;
	sc = rdcd.cd_devs[unit];
	if (sc == NULL)
		return 0;

	if (sc->sc_type == RD_UNCONFIGURED)
		return 0;

	return (sc->sc_size >> DEV_BSHIFT);
}

int rdopen(dev, flag, fmt, proc)
	dev_t   dev;
	int     flag, fmt;
	struct proc *proc;
{
	int md, unit;
	struct rd_softc *sc;

	md = minor(dev);
	unit = RD_UNIT(md);
	if (unit >= rdcd.cd_ndevs)
		return ENXIO;
	sc = rdcd.cd_devs[unit];
	if (sc == NULL)
		return ENXIO;

	/*
	 * The control device is not exclusive, and can
	 * open uninitialized units (so you can setconf).
	 */
	if (RD_IS_CTRL(md))
		return 0;

#ifdef	RAMDISK_HOOKS
	/* Call the open hook to allow loading the device. */
	rd_open_hook(unit, &sc->sc_rd);
#endif

	/*
	 * This is a normal, "slave" device, so
	 * enforce initialized, exclusive open.
	 */
	if (sc->sc_type == RD_UNCONFIGURED)
		return ENXIO;
	if (sc->sc_flags & RD_ISOPEN)
		return EBUSY;

	return 0;
}

int rdclose(dev, flag, fmt, proc)
	dev_t   dev;
	int     flag, fmt;
	struct proc *proc;
{
	int md, unit;
	struct rd_softc *sc;

	md = minor(dev);
	unit = RD_UNIT(md);
	sc = rdcd.cd_devs[unit];

	if (RD_IS_CTRL(md))
		return 0;

	/* Normal device. */
	sc->sc_flags = 0;

	return 0;
}

int
rdread(dev, uio)
	dev_t		dev;
	struct uio	*uio;
{
	return (physio(rdstrategy, NULL, dev, B_READ, minphys, uio));
}

int
rdwrite(dev, uio)
	dev_t		dev;
	struct uio	*uio;
{
	return (physio(rdstrategy, NULL, dev, B_WRITE, minphys, uio));
}

/*
 * Handle I/O requests, either directly, or
 * by passing them to the server process.
 */
void
rdstrategy(bp)
	struct buf *bp;
{
	int md, unit;
	struct rd_softc *sc;
	caddr_t addr;
	size_t  off, xfer;

	md = minor(bp->b_dev);
	unit = RD_UNIT(md);
	sc = rdcd.cd_devs[unit];

	switch (sc->sc_type) {
#if RAMDISK_SERVER
	case RD_UMEM_SERVER:
		/* Just add this job to the server's queue. */
		bp->b_actf = sc->sc_buflist;
		sc->sc_buflist = bp;
		if (bp->b_actf == NULL) {
			/* server queue was empty. */
			wakeup((caddr_t)sc);
			/* see rd_server_loop() */
		}
		/* no biodone in this case */
		return;
#endif	/* RAMDISK_SERVER */

	case RD_KMEM_FIXED:
	case RD_KMEM_ALLOCATED:
		/* These are in kernel space.  Access directly. */
		bp->b_resid = bp->b_bcount;
		off = (bp->b_blkno << DEV_BSHIFT);
		if (off >= sc->sc_size) {
			if (bp->b_flags & B_READ)
				break;	/* EOF */
			goto set_eio;
		}
		xfer = bp->b_resid;
		if (xfer > (sc->sc_size - off))
			xfer = (sc->sc_size - off);
		addr = sc->sc_addr + off;
		if (bp->b_flags & B_READ)
			bcopy(addr, bp->b_data, xfer);
		else
			bcopy(bp->b_data, addr, xfer);
		bp->b_resid -= xfer;
		break;

	default:
		bp->b_resid = bp->b_bcount;
	set_eio:
		bp->b_error = EIO;
		bp->b_flags |= B_ERROR;
		break;
	}
	biodone(bp);
}

int
rdioctl(dev, cmd, data, flag, proc)
	dev_t	dev;
	u_long	cmd;
	int		flag;
	caddr_t	data;
	struct proc	*proc;
{
	int md, unit;
	struct rd_softc *sc;
	struct rd_conf *urd;

	md = minor(dev);
	unit = RD_UNIT(md);
	sc = rdcd.cd_devs[unit];

	/* If this is not the control device, punt! */
	if (RD_IS_CTRL(md) == 0)
		return ENOTTY;

	urd = (struct rd_conf *)data;
	switch (cmd) {
	case RD_GETCONF:
		*urd = sc->sc_rd;
		return 0;

	case RD_SETCONF:
		/* Can only set it once. */
		if (sc->sc_type != RD_UNCONFIGURED)
			break;
		switch (urd->rd_type) {
		case RD_KMEM_ALLOCATED:
			return rd_ioctl_kalloc(sc, urd, proc);
#if RAMDISK_SERVER
		case RD_UMEM_SERVER:
			return rd_ioctl_server(sc, urd, proc);
#endif
		default:
			break;
		}
		break;
	}
	return EINVAL;
}

/*
 * Handle ioctl RD_SETCONF for (sc_type == RD_KMEM_ALLOCATED)
 * Just allocate some kernel memory and return.
 */
int
rd_ioctl_kalloc(sc, urd, proc)
	struct rd_softc *sc;
	struct rd_conf *urd;
	struct proc	*proc;
{
	vm_offset_t addr;
	vm_size_t  size;

	/* Sanity check the size. */
	size = urd->rd_size;
	if (size > RD_KMEM_MAX_SIZE)
		return EINVAL;
	addr = kmem_alloc(kernel_map, size);
	if (!addr)
		return ENOMEM;

	/* This unit is now configured. */
	sc->sc_addr = (caddr_t)addr; 	/* kernel space */
	sc->sc_size = (size_t)size;
	sc->sc_type = RD_KMEM_ALLOCATED;
	return 0;
}	

#if RAMDISK_SERVER

/*
 * Handle ioctl RD_SETCONF for (sc_type == RD_UMEM_SERVER)
 * Set config, then become the I/O server for this unit.
 */
int
rd_ioctl_server(sc, urd, proc)
	struct rd_softc *sc;
	struct rd_conf *urd;
	struct proc	*proc;
{
	vm_offset_t end;
	int error;

	/* Sanity check addr, size. */
	end = (vm_offset_t) (urd->rd_addr + urd->rd_size);

	if ((end >= VM_MAXUSER_ADDRESS) ||
		(end < ((vm_offset_t) urd->rd_addr)) )
		return EINVAL;

	/* This unit is now configured. */
	sc->sc_addr = urd->rd_addr; 	/* user space */
	sc->sc_size = urd->rd_size;
	sc->sc_type = RD_UMEM_SERVER;

	/* Become the server daemon */
	error = rd_server_loop(sc);

	/* This server is now going away! */
	sc->sc_type = RD_UNCONFIGURED;
	sc->sc_addr = 0;
	sc->sc_size = 0;

	return (error);
}	

int	rd_sleep_pri = PWAIT | PCATCH;

static int
rd_server_loop(sc)
	struct rd_softc *sc;
{
	struct buf *bp;
	caddr_t addr;	/* user space address */
	size_t  off;	/* offset into "device" */
	size_t  xfer;	/* amount to transfer */
	int error;

	for (;;) {
		/* Wait for some work to arrive. */
		while (sc->sc_buflist == NULL) {
			error = tsleep((caddr_t)sc, rd_sleep_pri, "rd_idle", 0);
			if (error)
				return error;
		}

		/* Unlink buf from head of list. */
		bp = sc->sc_buflist;
		sc->sc_buflist = bp->b_actf;
		bp->b_actf = NULL;

		/* Do the transfer to/from user space. */
		error = 0;
		bp->b_resid = bp->b_bcount;
		off = (bp->b_blkno << DEV_BSHIFT);
		if (off >= sc->sc_size) {
			if (bp->b_flags & B_READ)
				goto done;	/* EOF (not an error) */
			error = EIO;
			goto done;
		}
		xfer = bp->b_resid;
		if (xfer > (sc->sc_size - off))
			xfer = (sc->sc_size - off);
		addr = sc->sc_addr + off;
		if (bp->b_flags & B_READ)
			error = copyin(addr, bp->b_data, xfer);
		else
			error = copyout(bp->b_data, addr, xfer);
		if (!error)
			bp->b_resid -= xfer;

	done:
		if (error) {
			bp->b_error = error;
			bp->b_flags |= B_ERROR;
		}
		biodone(bp);
	}
}

#endif	/* RAMDISK_SERVER */
