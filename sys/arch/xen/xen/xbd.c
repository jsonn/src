/* $NetBSD: xbd.c,v 1.12.6.1 2005/02/13 10:20:50 yamt Exp $ */

/*
 *
 * Copyright (c) 2004 Christian Limpach.
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
 *      This product includes software developed by Christian Limpach.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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


#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: xbd.c,v 1.12.6.1 2005/02/13 10:20:50 yamt Exp $");

#include "xbd.h"
#include "rnd.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/lock.h>
#include <sys/conf.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/kernel.h>
#include <sys/kthread.h>

#include <uvm/uvm.h>

#if NRND > 0
#include <sys/rnd.h>
#endif

#include <dev/dkvar.h>
#include <machine/xbdvar.h>
#include <machine/xen.h>
#include <machine/hypervisor.h>
#include <machine/hypervisor-ifs/hypervisor-if.h>
#include <machine/hypervisor-ifs/vbd.h>
#include <machine/events.h>


static void xbd_attach(struct device *, struct device *, void *);
static int xbd_detach(struct device *, int);

#if NXBD > 0
int xbd_match(struct device *, struct cfdata *, void *);
CFATTACH_DECL(xbd, sizeof(struct xbd_softc),
    xbd_match, xbd_attach, xbd_detach, NULL);

extern struct cfdriver xbd_cd;
#endif

#if NWD > 0
int xbd_wd_match(struct device *, struct cfdata *, void *);
CFATTACH_DECL(wd, sizeof(struct xbd_softc),
    xbd_wd_match, xbd_attach, xbd_detach, NULL);

extern struct cfdriver wd_cd;
#endif

#if NSD > 0
int xbd_sd_match(struct device *, struct cfdata *, void *);
CFATTACH_DECL(sd, sizeof(struct xbd_softc),
    xbd_sd_match, xbd_attach, xbd_detach, NULL);

extern struct cfdriver sd_cd;
#endif

#if NCD > 0
int xbd_cd_match(struct device *, struct cfdata *, void *);
CFATTACH_DECL(cd, sizeof(struct xbd_softc),
    xbd_cd_match, xbd_attach, xbd_detach, NULL);

extern struct cfdriver cd_cd;
#endif


dev_type_open(xbdopen);
dev_type_close(xbdclose);
dev_type_read(xbdread);
dev_type_write(xbdwrite);
dev_type_ioctl(xbdioctl);
dev_type_ioctl(xbdioctl_cdev);
dev_type_strategy(xbdstrategy);
dev_type_dump(xbddump);
dev_type_size(xbdsize);

#if NXBD > 0
const struct bdevsw xbd_bdevsw = {
	xbdopen, xbdclose, xbdstrategy, xbdioctl,
	xbddump, xbdsize, D_DISK
};

const struct cdevsw xbd_cdevsw = {
	xbdopen, xbdclose, xbdread, xbdwrite, xbdioctl_cdev,
	nostop, notty, nopoll, nommap, nokqfilter, D_DISK
};

static dev_t xbd_major;
#endif

#if NWD > 0
const struct bdevsw wd_bdevsw = {
	xbdopen, xbdclose, xbdstrategy, xbdioctl,
	xbddump, xbdsize, D_DISK
};

const struct cdevsw wd_cdevsw = {
	xbdopen, xbdclose, xbdread, xbdwrite, xbdioctl_cdev,
	nostop, notty, nopoll, nommap, nokqfilter, D_DISK
};

static dev_t xbd_wd_major;
static dev_t xbd_wd_cdev_major;
#endif

#if NSD > 0
const struct bdevsw sd_bdevsw = {
	xbdopen, xbdclose, xbdstrategy, xbdioctl,
	xbddump, xbdsize, D_DISK
};

const struct cdevsw sd_cdevsw = {
	xbdopen, xbdclose, xbdread, xbdwrite, xbdioctl_cdev,
	nostop, notty, nopoll, nommap, nokqfilter, D_DISK
};

static dev_t xbd_sd_major;
static dev_t xbd_sd_cdev_major;
#endif

#if NCD > 0
const struct bdevsw cd_bdevsw = {
	xbdopen, xbdclose, xbdstrategy, xbdioctl,
	xbddump, xbdsize, D_DISK
};

const struct cdevsw cd_cdevsw = {
	xbdopen, xbdclose, xbdread, xbdwrite, xbdioctl_cdev,
	nostop, notty, nopoll, nommap, nokqfilter, D_DISK
};

static dev_t xbd_cd_major;
static dev_t xbd_cd_cdev_major;
#endif


static int	xbdstart(struct dk_softc *, struct buf *);
static int	xbd_response_handler(void *);
static void	xbd_update_create_kthread(void *);
static void	xbd_update_kthread(void *);
static int	xbd_update_handler(void *);

static int	xbdinit(struct xbd_softc *, xen_disk_t *, struct dk_intf *);

/* Pseudo-disk Interface */
static struct dk_intf dkintf_esdi = {
	DTYPE_ESDI,
	"Xen Virtual ESDI",
	xbdopen,
	xbdclose,
	xbdstrategy,
	xbdstart,
};
#if NSD > 0
static struct dk_intf dkintf_scsi = {
	DTYPE_SCSI,
	"Xen Virtual SCSI",
	xbdopen,
	xbdclose,
	xbdstrategy,
	xbdstart,
};
#endif

#if NXBD > 0
static struct xbd_attach_args xbd_ata = {
	.xa_device = "xbd",
	.xa_dkintf = &dkintf_esdi,
};
#endif

#if NWD > 0
static struct xbd_attach_args wd_ata = {
	.xa_device = "wd",
	.xa_dkintf = &dkintf_esdi,
};
#endif

#if NSD > 0
static struct xbd_attach_args sd_ata = {
	.xa_device = "sd",
	.xa_dkintf = &dkintf_scsi,
};
#endif

#if NCD > 0
static struct xbd_attach_args cd_ata = {
	.xa_device = "cd",
	.xa_dkintf = &dkintf_esdi,
};
#endif

static struct sysctlnode *diskcookies;


#if defined(XBDDEBUG) && !defined(DEBUG)
#define DEBUG
#endif

#ifdef DEBUG
int xbddebug = 0;

#define XBDB_FOLLOW	0x1
#define XBDB_IO		0x2
#define XBDB_SETUP	0x4
#define XBDB_HOTPLUG	0x8

#define IFDEBUG(x,y)		if (xbddebug & (x)) y
#define DPRINTF(x,y)		IFDEBUG(x, printf y)
#define DPRINTF_FOLLOW(y)	DPRINTF(XBDB_FOLLOW, y)
#define	DEBUG_MARK_UNUSED(_xr)	(_xr)->xr_sc = (void *)0xdeadbeef

struct xbdreq *xbd_allxr;
#else
#define IFDEBUG(x,y)
#define DPRINTF(x,y)
#define DPRINTF_FOLLOW(y)
#define	DEBUG_MARK_UNUSED(_xr)
#endif

#ifdef DIAGNOSTIC
#define DIAGPANIC(x)		panic x 
#define DIAGCONDPANIC(x,y)	if (x) panic y
#else
#define DIAGPANIC(x)
#define DIAGCONDPANIC(x,y)
#endif


struct xbdreq {
	union {
		SLIST_ENTRY(xbdreq) _unused;	/* ptr. to next free xbdreq */
		SIMPLEQ_ENTRY(xbdreq) _suspended;
					/* link when on suspended queue. */
	} _link;
	struct xbdreq		*xr_parent;	/* ptr. to parent xbdreq */
	struct buf		*xr_bp;		/* ptr. to original I/O buf */
	daddr_t			xr_bn;		/* block no. to process */
	long			xr_bqueue;	/* bytes left to queue */
	long			xr_bdone;	/* bytes left */
	vaddr_t			xr_data;	/* ptr. to data to be proc. */
	vaddr_t			xr_aligned;	/* ptr. to aligned data */
	long			xr_breq;	/* bytes in this req. */
	struct xbd_softc	*xr_sc;		/* ptr. to xbd softc */
};
#define	xr_unused	_link._unused
#define	xr_suspended	_link._suspended

SLIST_HEAD(,xbdreq) xbdreqs =
	SLIST_HEAD_INITIALIZER(xbdreqs);
static SIMPLEQ_HEAD(, xbdreq) xbdr_suspended =
	SIMPLEQ_HEAD_INITIALIZER(xbdr_suspended);

#define	CANGET_XBDREQ() (!SLIST_EMPTY(&xbdreqs))

#define	GET_XBDREQ(_xr) do {				\
	(_xr) = SLIST_FIRST(&xbdreqs);			\
	if (__predict_true(_xr))			\
		SLIST_REMOVE_HEAD(&xbdreqs, xr_unused);	\
} while (/*CONSTCOND*/0)

#define	PUT_XBDREQ(_xr) do {				\
	DEBUG_MARK_UNUSED(_xr);				\
	SLIST_INSERT_HEAD(&xbdreqs, _xr, xr_unused);	\
} while (/*CONSTCOND*/0)

static struct bufq_state bufq;
static int bufq_users = 0;

#define XEN_MAJOR(_dev)	((_dev) >> 8)
#define XEN_MINOR(_dev)	((_dev) & 0xff)

#define	XEN_SCSI_DISK0_MAJOR	8
#define	XEN_SCSI_DISK1_MAJOR	65
#define	XEN_SCSI_DISK2_MAJOR	66
#define	XEN_SCSI_DISK3_MAJOR	67
#define	XEN_SCSI_DISK4_MAJOR	68
#define	XEN_SCSI_DISK5_MAJOR	69
#define	XEN_SCSI_DISK6_MAJOR	70
#define	XEN_SCSI_DISK7_MAJOR	71
#define	XEN_SCSI_DISK8_MAJOR	128
#define	XEN_SCSI_DISK9_MAJOR	129
#define	XEN_SCSI_DISK10_MAJOR	130
#define	XEN_SCSI_DISK11_MAJOR	131
#define	XEN_SCSI_DISK12_MAJOR	132
#define	XEN_SCSI_DISK13_MAJOR	133
#define	XEN_SCSI_DISK14_MAJOR	134
#define	XEN_SCSI_DISK15_MAJOR	135
#define	XEN_SCSI_CDROM_MAJOR	11

#define	XEN_IDE0_MAJOR		3
#define	XEN_IDE1_MAJOR		22
#define	XEN_IDE2_MAJOR		33
#define	XEN_IDE3_MAJOR		34
#define	XEN_IDE4_MAJOR		56
#define	XEN_IDE5_MAJOR		57
#define	XEN_IDE6_MAJOR		88
#define	XEN_IDE7_MAJOR		89
#define	XEN_IDE8_MAJOR		90
#define	XEN_IDE9_MAJOR		91

#define	XEN_BSHIFT	9		/* log2(XEN_BSIZE) */
#define	XEN_BSIZE	(1 << XEN_BSHIFT)

#define MAX_VBDS 64
static int nr_vbds;
static xen_disk_t *vbd_info;

static blk_ring_t *blk_ring = NULL;
static BLK_RING_IDX resp_cons; /* Response consumer for comms ring. */
static BLK_RING_IDX req_prod;  /* Private request producer.         */
static BLK_RING_IDX last_req_prod;  /* Request producer at last trap. */

#define STATE_ACTIVE    0
#define STATE_SUSPENDED 1
#define STATE_CLOSED    2
static unsigned int state = STATE_SUSPENDED;


#define XBDUNIT(x)		DISKUNIT(x)
#define GETXBD_SOFTC(_xs, x)	if (!((_xs) = getxbd_softc(x))) return ENXIO
#define GETXBD_SOFTC_CDEV(_xs, x) do {			\
	dev_t bx = devsw_chr2blk((x));			\
	if (bx == NODEV)				\
		return ENXIO;				\
	if (!((_xs) = getxbd_softc(bx)))		\
		return ENXIO;				\
} while (/*CONSTCOND*/0)

static struct xbd_softc *
getxbd_softc(dev_t dev)
{
	int	unit = XBDUNIT(dev);

	DPRINTF_FOLLOW(("getxbd_softc(0x%x): major = %d unit = %d\n", dev,
	    major(dev), unit));
#if NXBD > 0
	if (major(dev) == xbd_major)
		return device_lookup(&xbd_cd, unit);
#endif
#if NWD > 0
	if (major(dev) == xbd_wd_major || major(dev) == xbd_wd_cdev_major)
		return device_lookup(&wd_cd, unit);
#endif
#if NSD > 0
	if (major(dev) == xbd_sd_major || major(dev) == xbd_sd_cdev_major)
		return device_lookup(&sd_cd, unit);
#endif
#if NCD > 0
	if (major(dev) == xbd_cd_major || major(dev) == xbd_cd_cdev_major)
		return device_lookup(&cd_cd, unit);
#endif
	return NULL;
}

static int
get_vbd_info(xen_disk_t *disk_info)
{
	int err;
	block_io_op_t op; 

	/* Probe for disk information. */
	memset(&op, 0, sizeof(op)); 
	op.cmd = BLOCK_IO_OP_VBD_PROBE; 
	op.u.probe_params.domain = 0; 
	op.u.probe_params.xdi.max = MAX_VBDS;
	op.u.probe_params.xdi.disks = disk_info;
	op.u.probe_params.xdi.count = 0;

	err = HYPERVISOR_block_io_op(&op);
	if (err) {
		printf("WARNING: Could not probe disks (%d)\n", err);
		DIAGPANIC(("get_vbd_info: Could not probe disks (%d)", err));
		return -1;
	}

	return op.u.probe_params.xdi.count;
}

static void
reset_interface(void)
{
	block_io_op_t op; 

	op.cmd = BLOCK_IO_OP_RESET;
	if (HYPERVISOR_block_io_op(&op) != 0)
		printf("xbd: Possible blkdev trouble: couldn't reset ring\n");
}

static void
init_interface(void)
{
	block_io_op_t op; 

	reset_interface();

	if (blk_ring == NULL) {
		op.cmd = BLOCK_IO_OP_RING_ADDRESS;
		(void)HYPERVISOR_block_io_op(&op);

		blk_ring = (blk_ring_t *)uvm_km_alloc(kernel_map,
		    PAGE_SIZE, PAGE_SIZE, UVM_KMF_VAONLY);
		pmap_kenter_ma((vaddr_t)blk_ring, op.u.ring_mfn << PAGE_SHIFT,
		    VM_PROT_READ|VM_PROT_WRITE);
		DPRINTF(XBDB_SETUP, ("init_interface: "
		    "ring va %p and wired to %p\n",
		    blk_ring, (void *)(op.u.ring_mfn << PAGE_SHIFT)));

		blk_ring->req_prod = blk_ring->resp_prod =
			resp_cons = req_prod = last_req_prod = 0;

		event_set_handler(_EVENT_BLKDEV, &xbd_response_handler,
		    NULL, IPL_BIO);
		hypervisor_enable_event(_EVENT_BLKDEV);
	}

	__insn_barrier();
	state = STATE_ACTIVE;
}

static void
enable_update_events(struct device *self)
{

	kthread_create(xbd_update_create_kthread, self);
	event_set_handler(_EVENT_VBD_UPD, &xbd_update_handler, self, IPL_BIO);
	hypervisor_enable_event(_EVENT_VBD_UPD);
}

static void
signal_requests_to_xen(void)
{
	block_io_op_t op; 

	DPRINTF(XBDB_IO, ("signal_requests_to_xen: %d -> %d\n",
	    blk_ring->req_prod, MASK_BLK_IDX(req_prod)));
	blk_ring->req_prod = MASK_BLK_IDX(req_prod);
	last_req_prod = req_prod;

	op.cmd = BLOCK_IO_OP_SIGNAL; 
	HYPERVISOR_block_io_op(&op);
	return;
}

static void
setup_sysctl(void)
{
	struct sysctlnode *pnode;

	sysctl_createv(NULL, 0, NULL, NULL,
		       0,
		       CTLTYPE_NODE, "machdep", NULL,
		       NULL, 0, NULL, 0,
		       CTL_MACHDEP, CTL_EOL);

	sysctl_createv(NULL, 0, NULL, &pnode,
		       0,
		       CTLTYPE_NODE, "domain0", NULL,
		       NULL, 0, NULL, 0,
		       CTL_MACHDEP, CTL_CREATE, CTL_EOL);

	if (pnode == NULL)
		return;

	sysctl_createv(NULL, 0, &pnode, &pnode,
		       0,
		       CTLTYPE_NODE, "diskcookie", NULL,
		       NULL, 0, NULL, 0,
		       CTL_CREATE, CTL_EOL);

	if (pnode)
		diskcookies = pnode;
}

static struct xbd_attach_args *
get_xbda(xen_disk_t *xd)
{

	switch (XEN_MAJOR(xd->device)) {
#if NSD > 0
	case XEN_SCSI_DISK0_MAJOR:
	case XEN_SCSI_DISK1_MAJOR ... XEN_SCSI_DISK7_MAJOR:
	case XEN_SCSI_DISK8_MAJOR ... XEN_SCSI_DISK15_MAJOR:
		if (xd->capacity == 0)
			return NULL;
		return &sd_ata;
	case XEN_SCSI_CDROM_MAJOR:
		return &cd_ata;
#endif
#if NWD > 0
	case XEN_IDE0_MAJOR:
	case XEN_IDE1_MAJOR:
	case XEN_IDE2_MAJOR:
	case XEN_IDE3_MAJOR:
	case XEN_IDE4_MAJOR:
	case XEN_IDE5_MAJOR:
	case XEN_IDE6_MAJOR:
	case XEN_IDE7_MAJOR:
	case XEN_IDE8_MAJOR:
	case XEN_IDE9_MAJOR:
		switch (XD_TYPE(xd->info)) {
		case XD_TYPE_CDROM:
			return &cd_ata;
		case XD_TYPE_DISK:
			if (xd->capacity == 0)
				return NULL;
			return &wd_ata;
		default:
			return NULL;
		}
		break;
#endif
	default:
		if (xd->capacity == 0)
			return NULL;
		return &xbd_ata;
	}
	return NULL;
}

int
xbd_scan(struct device *self, struct xbd_attach_args *mainbus_xbda,
    cfprint_t print)
{
	struct xbdreq *xr;
	struct xbd_attach_args *xbda;
	xen_disk_t *xd;
	int i;

	init_interface();
	if (xen_start_info.flags & SIF_PRIVILEGED)
		setup_sysctl();

#if NXBD > 0
	xbd_major = devsw_name2blk("xbd", NULL, 0);
#endif
#if NWD > 0
	xbd_wd_major = devsw_name2blk("wd", NULL, 0);
	/* XXX Also handle the cdev majors since stuff like
	 * read_sector calls strategy on the cdev.  This only works if
	 * all the majors we care about are different.
	 */
	xbd_wd_cdev_major = major(devsw_blk2chr(makedev(xbd_wd_major, 0)));
#endif
#if NSD > 0
	xbd_sd_major = devsw_name2blk("sd", NULL, 0);
	xbd_sd_cdev_major = major(devsw_blk2chr(makedev(xbd_sd_major, 0)));
#endif
#if NCD > 0
	xbd_cd_major = devsw_name2blk("cd", NULL, 0);
	xbd_cd_cdev_major = major(devsw_blk2chr(makedev(xbd_cd_major, 0)));
#endif

	MALLOC(xr, struct xbdreq *, BLK_RING_SIZE * sizeof(struct xbdreq),
	    M_DEVBUF, M_WAITOK | M_ZERO);
#ifdef DEBUG
	xbd_allxr = xr;
#endif

	/* XXX Xen1.2: We cannot use BLK_RING_SIZE many slots, since
	 * Xen 1.2 keeps indexes masked in the ring and the case where
	 * we queue all slots at once is handled wrong. 
	 */
	for (i = 0; i < BLK_RING_SIZE - 1; i++)
		PUT_XBDREQ(&xr[i]);

	MALLOC(vbd_info, xen_disk_t *, MAX_VBDS * sizeof(xen_disk_t),
	    M_DEVBUF, M_WAITOK);
	memset(vbd_info, 0, MAX_VBDS * sizeof(xen_disk_t));
	nr_vbds  = get_vbd_info(vbd_info);
	if (nr_vbds <= 0)
		goto out;

	for (i = 0; i < nr_vbds; i++) {
		xd = &vbd_info[i];
		xbda = get_xbda(xd);
		if (xbda) {
			xbda->xa_xd = xd;
			config_found(self, xbda, print);
		}
	}

	enable_update_events(self);

	return 0;

 out:
	FREE(vbd_info, M_DEVBUF);
	vbd_info = NULL;
	FREE(xr, M_DEVBUF);
#ifdef DEBUG
	xbd_allxr = NULL;
#endif
	SLIST_INIT(&xbdreqs);
	return 0;
}

#if NXBD > 0
int
xbd_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct xbd_attach_args *xa = (struct xbd_attach_args *)aux;

	if (strcmp(xa->xa_device, "xbd") == 0)
		return 1;
	return 0;
}
#endif

#if NWD > 0
int
xbd_wd_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct xbd_attach_args *xa = (struct xbd_attach_args *)aux;

	if (strcmp(xa->xa_device, "wd") == 0)
		return 1;
	return 0;
}
#endif

#if NSD > 0
int
xbd_sd_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct xbd_attach_args *xa = (struct xbd_attach_args *)aux;

	if (strcmp(xa->xa_device, "sd") == 0)
		return 1;
	return 0;
}
#endif

#if NCD > 0
int
xbd_cd_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct xbd_attach_args *xa = (struct xbd_attach_args *)aux;

	if (strcmp(xa->xa_device, "cd") == 0)
		return 1;
	return 0;
}
#endif

static void
xbd_attach(struct device *parent, struct device *self, void *aux)
{
	struct xbd_attach_args *xbda = (struct xbd_attach_args *)aux;
	struct xbd_softc *xs = (struct xbd_softc *)self;

	aprint_normal(": Xen Virtual Block Device");

	simple_lock_init(&xs->sc_slock);
	dk_sc_init(&xs->sc_dksc, xs, xs->sc_dev.dv_xname);
	xbdinit(xs, xbda->xa_xd, xbda->xa_dkintf);
	if (diskcookies) {
		/* XXX beware that xs->sc_xd_device is a long */
		sysctl_createv(NULL, 0, &diskcookies, NULL,
		    0,
		    CTLTYPE_INT, xs->sc_dev.dv_xname, NULL,
		    NULL, 0, &xs->sc_xd_device, 0,
		    CTL_CREATE, CTL_EOL);
	}
#if NRND > 0
	rnd_attach_source(&xs->rnd_source, xs->sc_dev.dv_xname,
	    RND_TYPE_DISK, 0);
#endif
}

static int
xbd_detach(struct device *dv, int flags)
{
	struct	xbd_softc *xs = (struct	xbd_softc *)dv;

	/* 
	 * Mark disk about to be removed (between now and when the xs
	 * will be freed).
	 */
	xs->sc_shutdown = 1;

	/* And give it some time to settle if it's busy. */
	if (xs->sc_dksc.sc_dkdev.dk_busy > 0)
		tsleep(&xs, PWAIT, "xbdetach", hz);

	/* Detach the disk. */
	disk_detach(&xs->sc_dksc.sc_dkdev);

	/* XXX decrement bufq_users and free? */

	/* XXX no need to remove sysctl nodes since they only exist
	 * in domain0 and domain0's devices are never removed.
	 */

	return 0;
}

int
xbdopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct	xbd_softc *xs;

	DPRINTF_FOLLOW(("xbdopen(0x%04x, %d)\n", dev, flags));
	switch (fmt) {
	case S_IFCHR:
		GETXBD_SOFTC_CDEV(xs, dev);
		break;
	case S_IFBLK:
		GETXBD_SOFTC(xs, dev);
		break;
	default:
		return ENXIO;
	}
	return dk_open(xs->sc_di, &xs->sc_dksc, dev, flags, fmt, p);
}

int
xbdclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct	xbd_softc *xs;

	DPRINTF_FOLLOW(("xbdclose(%d, %d)\n", dev, flags));
	switch (fmt) {
	case S_IFCHR:
		GETXBD_SOFTC_CDEV(xs, dev);
		break;
	case S_IFBLK:
		GETXBD_SOFTC(xs, dev);
		break;
	default:
		return ENXIO;
	}
	return dk_close(xs->sc_di, &xs->sc_dksc, dev, flags, fmt, p);
}

void
xbdstrategy(struct buf *bp)
{
	struct	xbd_softc *xs = getxbd_softc(bp->b_dev);

	DPRINTF_FOLLOW(("xbdstrategy(%p): b_bcount = %ld\n", bp,
	    (long)bp->b_bcount));

	if (xs == NULL || xs->sc_shutdown) {
		bp->b_flags |= B_ERROR;
		bp->b_error = EIO;
		biodone(bp);
		return;
	}

	dk_strategy(xs->sc_di, &xs->sc_dksc, bp);
	return;
}

int
xbdsize(dev_t dev)
{
	struct xbd_softc *xs = getxbd_softc(dev);

	DPRINTF_FOLLOW(("xbdsize(%d)\n", dev));
	if (xs == NULL || xs->sc_shutdown)
		return -1;
	return dk_size(xs->sc_di, &xs->sc_dksc, dev);
}

static void
map_align(struct xbdreq *xr)
{
	int s;

	s = splvm();
	xr->xr_aligned = uvm_km_alloc(kmem_map, xr->xr_bqueue, XEN_BSIZE,
	    UVM_KMF_WIRED);
	splx(s);
	DPRINTF(XBDB_IO, ("map_align(%p): bp %p addr %p align 0x%08lx "
	    "size 0x%04lx\n", xr, xr->xr_bp, xr->xr_bp->b_data,
	    xr->xr_aligned, xr->xr_bqueue));
	xr->xr_data = xr->xr_aligned;
	if ((xr->xr_bp->b_flags & B_READ) == 0)
		memcpy((void *)xr->xr_aligned, xr->xr_bp->b_data,
		    xr->xr_bqueue);
}

static void
unmap_align(struct xbdreq *xr)
{
	int s;

	if (xr->xr_bp->b_flags & B_READ)
		memcpy(xr->xr_bp->b_data, (void *)xr->xr_aligned,
		    xr->xr_bp->b_bcount);
	DPRINTF(XBDB_IO, ("unmap_align(%p): bp %p addr %p align 0x%08lx "
	    "size 0x%04x\n", xr, xr->xr_bp, xr->xr_bp->b_data,
	    xr->xr_aligned, xr->xr_bp->b_bcount));
	s = splvm();
	uvm_km_free(kmem_map, xr->xr_aligned, xr->xr_bp->b_bcount,
	    UVM_KMF_WIRED);
	splx(s);
	xr->xr_aligned = (vaddr_t)0;
}

static void
fill_ring(struct xbdreq *xr)
{
	struct xbdreq *pxr = xr->xr_parent;
	paddr_t pa;
	unsigned long ma;
	vaddr_t addr, off;
	blk_ring_req_entry_t *ring_req;
	int breq, nr_sectors;

	/* Fill out a communications ring structure. */
	ring_req = &blk_ring->ring[MASK_BLK_IDX(req_prod)].req;
	ring_req->id = (unsigned long)xr;
	ring_req->operation = pxr->xr_bp->b_flags & B_READ ? XEN_BLOCK_READ :
		XEN_BLOCK_WRITE;
	ring_req->sector_number = (xen_sector_t)pxr->xr_bn;
	ring_req->device = pxr->xr_sc->sc_xd_device;

	DPRINTF(XBDB_IO, ("fill_ring(%d): bp %p sector %llu pxr %p xr %p\n",
	    MASK_BLK_IDX(req_prod), pxr->xr_bp, (unsigned long long)pxr->xr_bn,
	    pxr, xr));

	xr->xr_breq = 0;
	ring_req->nr_segments = 0;
	addr = trunc_page(pxr->xr_data);
	off = pxr->xr_data - addr;
	while (pxr->xr_bqueue > 0) {
#if 0
		pmap_extract(vm_map_pmap(&bp->b_proc->p_vmspace->vm_map),
		    addr, &pa);
#else
		pmap_extract(pmap_kernel(), addr, &pa);
#endif
		ma = xpmap_ptom_masked(pa) + off;
		DIAGCONDPANIC((ma & (XEN_BSIZE - 1)) != 0,
		    ("xbd request ma not sector aligned"));

		if (pxr->xr_bqueue > PAGE_SIZE - off)
			breq = PAGE_SIZE - off;
		else
			breq = pxr->xr_bqueue;
		nr_sectors = breq >> XEN_BSHIFT;
		DIAGCONDPANIC(nr_sectors >= XEN_BSIZE,
		    ("xbd request nr_sectors >= XEN_BSIZE"));

		DPRINTF(XBDB_IO, ("fill_ring(%d): va 0x%08lx pa 0x%08lx "
		    "ma 0x%08lx, sectors %d, left %ld/%ld\n",
		    MASK_BLK_IDX(req_prod), addr, pa, ma, nr_sectors,
		    pxr->xr_bqueue >> XEN_BSHIFT, pxr->xr_bqueue));

		ring_req->buffer_and_sects[ring_req->nr_segments++] =
			ma | nr_sectors;
		addr += PAGE_SIZE;
		pxr->xr_bqueue -= breq;
		pxr->xr_bn += nr_sectors;
		xr->xr_breq += breq;
		off = 0;
		if (ring_req->nr_segments == MAX_BLK_SEGS)
			break;
	}
	pxr->xr_data = addr;

	req_prod++;
}

static void
xbdresume(void)
{
	struct xbdreq *pxr, *xr;
	struct xbd_softc *xs;
	struct buf *bp;

	while ((pxr = SIMPLEQ_FIRST(&xbdr_suspended)) != NULL) {
		DPRINTF(XBDB_IO, ("xbdstart: resuming xbdreq %p for bp %p\n",
		    pxr, pxr->xr_bp));
		bp = pxr->xr_bp;
		xs = getxbd_softc(bp->b_dev);
		if (xs == NULL || xs->sc_shutdown) {
			bp->b_flags |= B_ERROR;
			bp->b_error = EIO;
		}
		if (bp->b_flags & B_ERROR) {
			pxr->xr_bdone -= pxr->xr_bqueue;
			pxr->xr_bqueue = 0;
			if (pxr->xr_bdone == 0) {
				bp->b_resid = bp->b_bcount;
				if (pxr->xr_aligned)
					unmap_align(pxr);
				PUT_XBDREQ(pxr);
				if (xs)
				{
					disk_unbusy(&xs->sc_dksc.sc_dkdev,
					    (bp->b_bcount - bp->b_resid),
					    (bp->b_flags & B_READ));
#if NRND > 0
					rnd_add_uint32(&xs->rnd_source,
					    bp->b_blkno);
#endif
				}
				biodone(bp);
			}
			continue;
		}
		while (__predict_true(pxr->xr_bqueue > 0)) {
			GET_XBDREQ(xr);
			if (__predict_false(xr == NULL))
				goto out;
			xr->xr_parent = pxr;
			fill_ring(xr);
		}
		DPRINTF(XBDB_IO, ("xbdstart: resumed xbdreq %p for bp %p\n",
		    pxr, bp));
		SIMPLEQ_REMOVE_HEAD(&xbdr_suspended, xr_suspended);
	}

 out:
	return;
}

static int
xbdstart(struct dk_softc *dksc, struct buf *bp)
{
	struct	xbd_softc *xs;
	struct xbdreq *pxr, *xr;
	struct	partition *pp;
	daddr_t	bn;
	int ret, runqueue;

	DPRINTF_FOLLOW(("xbdstart(%p, %p)\n", dksc, bp));

	runqueue = 1;
	ret = -1;

	xs = getxbd_softc(bp->b_dev);
	if (xs == NULL || xs->sc_shutdown) {
		bp->b_flags |= B_ERROR;
		bp->b_error = EIO;
		biodone(bp);
		return 0;
	}
	dksc = &xs->sc_dksc;

	/* XXXrcd:
	 * Translate partition relative blocks to absolute blocks,
	 * this probably belongs (somehow) in dksubr.c, since it
	 * is independant of the underlying code...  This will require
	 * that the interface be expanded slightly, though.
	 */
	bn = bp->b_blkno;
	if (DISKPART(bp->b_dev) != RAW_PART) {
		pp = &xs->sc_dksc.sc_dkdev.dk_label->
			d_partitions[DISKPART(bp->b_dev)];
		bn += pp->p_offset;
	}

	DPRINTF(XBDB_IO, ("xbdstart: addr %p, sector %llu, "
	    "count %d [%s]\n", bp->b_data, (unsigned long long)bn,
	    bp->b_bcount, bp->b_flags & B_READ ? "read" : "write"));

	GET_XBDREQ(pxr);
	if (__predict_false(pxr == NULL))
		goto out;

	disk_busy(&dksc->sc_dkdev); /* XXX: put in dksubr.c */
	/*
	 * We have a request slot, return 0 to make dk_start remove
	 * the bp from the work queue.
	 */
	ret = 0;

	pxr->xr_bp = bp;
	pxr->xr_parent = pxr;
	pxr->xr_bn = bn;
	pxr->xr_bqueue = bp->b_bcount;
	pxr->xr_bdone = bp->b_bcount;
	pxr->xr_data = (vaddr_t)bp->b_data;
	pxr->xr_sc = xs;

	if (pxr->xr_data & (XEN_BSIZE - 1))
		map_align(pxr);

	fill_ring(pxr);

	while (__predict_false(pxr->xr_bqueue > 0)) {
		GET_XBDREQ(xr);
		if (__predict_false(xr == NULL))
			break;
		xr->xr_parent = pxr;
		fill_ring(xr);
	}

	if (__predict_false(pxr->xr_bqueue > 0)) {
		SIMPLEQ_INSERT_TAIL(&xbdr_suspended, pxr,
		    xr_suspended);
		DPRINTF(XBDB_IO, ("xbdstart: suspended xbdreq %p "
		    "for bp %p\n", pxr, bp));
	} else if (CANGET_XBDREQ() && BUFQ_PEEK(&bufq) != NULL) {
		/* 
		 * We have enough resources to start another bp and
		 * there are additional bps on the queue, dk_start
		 * will call us again and we'll run the queue then.
		 */
		runqueue = 0;
	}

 out:
	if (runqueue && last_req_prod != req_prod)
		signal_requests_to_xen();

	return ret;
}

static int
xbd_response_handler(void *arg)
{
	struct buf *bp;
	struct xbd_softc *xs;
	blk_ring_resp_entry_t *ring_resp;
	struct xbdreq *pxr, *xr;
	int i;

	for (i = resp_cons; i != blk_ring->resp_prod; i = BLK_RING_INC(i)) {
		ring_resp = &blk_ring->ring[MASK_BLK_IDX(i)].resp;
		xr = (struct xbdreq *)ring_resp->id;
		pxr = xr->xr_parent;

		DPRINTF(XBDB_IO, ("xbd_response_handler(%d): pxr %p xr %p "
		    "bdone %04lx breq %04lx\n", i, pxr, xr, pxr->xr_bdone,
		    xr->xr_breq));
		pxr->xr_bdone -= xr->xr_breq;
		DIAGCONDPANIC(pxr->xr_bdone < 0,
		    ("xbd_response_handler: pxr->xr_bdone < 0"));

		if (__predict_false(ring_resp->status)) {
			pxr->xr_bp->b_flags |= B_ERROR;
			pxr->xr_bp->b_error = EIO;
		}

		if (xr != pxr) {
			PUT_XBDREQ(xr);
			if (!SIMPLEQ_EMPTY(&xbdr_suspended))
				xbdresume();
		}

		if (pxr->xr_bdone == 0) {
			bp = pxr->xr_bp;
			xs = getxbd_softc(bp->b_dev);
			if (xs == NULL) { /* don't fail bp if we're shutdown */
				bp->b_flags |= B_ERROR;
				bp->b_error = EIO;
			}
			DPRINTF(XBDB_IO, ("xbd_response_handler(%d): "
			    "completed bp %p\n", i, bp));
			if (bp->b_flags & B_ERROR)
				bp->b_resid = bp->b_bcount;
			else
				bp->b_resid = 0;

			if (pxr->xr_aligned)
				unmap_align(pxr);

			PUT_XBDREQ(pxr);
			if (xs) {
				disk_unbusy(&xs->sc_dksc.sc_dkdev,
				    (bp->b_bcount - bp->b_resid),
				    (bp->b_flags & B_READ));
#if NRND > 0
				rnd_add_uint32(&xs->rnd_source,
				    bp->b_blkno);
#endif
			}
			biodone(bp);
			if (!SIMPLEQ_EMPTY(&xbdr_suspended))
				xbdresume();
			/* XXX possible lockup if this was the only
			 * active device and requests were held back in
			 * the queue.
			 */
			if (xs)
				dk_iodone(xs->sc_di, &xs->sc_dksc);
		}
	}
	resp_cons = i;
	/* check if xbdresume queued any requests */
	if (last_req_prod != req_prod)
		signal_requests_to_xen();
	return 0;
}

static struct device *
find_device(xen_disk_t *xd)
{
	struct device *dv;
	struct xbd_softc *xs;

	for (dv = alldevs.tqh_first; dv != NULL; dv = dv->dv_list.tqe_next) {
		if (dv->dv_cfattach == NULL ||
		    dv->dv_cfattach->ca_attach != xbd_attach)
			continue;
		xs = (struct xbd_softc *)dv;
		if (xs->sc_xd_device == xd->device)
			break;
	}
	return dv;
}

static void
xbd_update_create_kthread(void *arg)
{

	kthread_create1(xbd_update_kthread, arg, NULL, "xbdupdate");
}

static void
xbd_update_kthread(void *arg)
{
	struct device *parent = arg;
	struct xbd_attach_args *xbda;
	struct device *dev;
	xen_disk_t *xd;
	xen_disk_t *vbd_info_update, *vbd_info_old;
	int i, j, new_nr_vbds;
	extern int hypervisor_print(void *, const char *);

	MALLOC(vbd_info_update, xen_disk_t *, MAX_VBDS *
	    sizeof(xen_disk_t), M_DEVBUF, M_WAITOK);

	for (;;) {
		memset(vbd_info_update, 0, MAX_VBDS * sizeof(xen_disk_t));
		new_nr_vbds  = get_vbd_info(vbd_info_update);

		if (memcmp(vbd_info, vbd_info_update, MAX_VBDS *
		    sizeof(xen_disk_t)) == 0) {
			FREE(vbd_info_update, M_DEVBUF);
			tsleep(parent, PWAIT, "xbdupd", 0);
			MALLOC(vbd_info_update, xen_disk_t *, MAX_VBDS *
			    sizeof(xen_disk_t), M_DEVBUF, M_WAITOK);
			continue;
		}

		j = 0;
		for (i = 0; i < new_nr_vbds; i++) {
			while (j < nr_vbds &&
			    vbd_info[j].device < vbd_info_update[i].device) {
				DPRINTF(XBDB_HOTPLUG,
				    ("delete device %x size %lx\n",
					vbd_info[j].device,
					vbd_info[j].capacity));
				xd = &vbd_info[j];
				dev = find_device(xd);
				if (dev)
					config_detach(dev, DETACH_FORCE);
				j++;
			}
			if (j < nr_vbds &&
			    vbd_info[j].device == vbd_info_update[i].device) {
				DPRINTF(XBDB_HOTPLUG,
				    ("update device %x size %lx size %lx\n",
					vbd_info_update[i].device,
					vbd_info[j].capacity,
					vbd_info_update[i].capacity));
				j++;
			} else {
				DPRINTF(XBDB_HOTPLUG,
				    ("add device %x size %lx\n",
					vbd_info_update[i].device,
					vbd_info_update[i].capacity));
				xd = &vbd_info_update[i];
				xbda = get_xbda(xd);
				if (xbda) {
					xbda->xa_xd = xd;
					config_found(parent, xbda, hypervisor_print);
				}
			}
		}

		while (j < nr_vbds) {
			DPRINTF(XBDB_HOTPLUG, ("delete device %x\n",
			    vbd_info[j].device));
			xd = &vbd_info[j];
			dev = find_device(xd);
			if (dev)
				config_detach(dev, DETACH_FORCE);
			j++;
		}

		nr_vbds = new_nr_vbds;

		vbd_info_old = vbd_info;
		vbd_info = vbd_info_update;
		vbd_info_update = vbd_info_old;
	}
}

static int
xbd_update_handler(void *arg)
{

	wakeup(arg);

	return 0;
}

/* XXX: we should probably put these into dksubr.c, mostly */
int
xbdread(dev_t dev, struct uio *uio, int flags)
{
	struct	xbd_softc *xs;
	struct	dk_softc *dksc;

	DPRINTF_FOLLOW(("xbdread(%d, %p, %d)\n", dev, uio, flags));
	GETXBD_SOFTC_CDEV(xs, dev);
	dksc = &xs->sc_dksc;
	if ((dksc->sc_flags & DKF_INITED) == 0)
		return ENXIO;
	/* XXX see the comments about minphys in ccd.c */
	return physio(xbdstrategy, NULL, dev, B_READ, minphys, uio);
}

/* XXX: we should probably put these into dksubr.c, mostly */
int
xbdwrite(dev_t dev, struct uio *uio, int flags)
{
	struct	xbd_softc *xs;
	struct	dk_softc *dksc;

	DPRINTF_FOLLOW(("xbdwrite(%d, %p, %d)\n", dev, uio, flags));
	GETXBD_SOFTC_CDEV(xs, dev);
	dksc = &xs->sc_dksc;
	if ((dksc->sc_flags & DKF_INITED) == 0)
		return ENXIO;
	/* XXX see the comments about minphys in ccd.c */
	return physio(xbdstrategy, NULL, dev, B_WRITE, minphys, uio);
}

int
xbdioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct	xbd_softc *xs;
	struct	dk_softc *dksc;
	int	ret;

	DPRINTF_FOLLOW(("xbdioctl(%d, %08lx, %p, %d, %p)\n",
	    dev, cmd, data, flag, p));
	GETXBD_SOFTC(xs, dev);
	dksc = &xs->sc_dksc;

	if ((ret = lockmgr(&dksc->sc_lock, LK_EXCLUSIVE, NULL)) != 0)
		return ret;

	switch (cmd) {
	default:
		ret = dk_ioctl(xs->sc_di, dksc, dev, cmd, data, flag, p);
		break;
	}

	lockmgr(&dksc->sc_lock, LK_RELEASE, NULL);
	return ret;
}

int
xbdioctl_cdev(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	dev_t bdev;

	bdev = devsw_chr2blk(dev);
	if (bdev == NODEV)
		return ENXIO;
	return xbdioctl(bdev, cmd, data, flag, p);
}

int
xbddump(dev_t dev, daddr_t blkno, caddr_t va, size_t size)
{
	struct	xbd_softc *xs;

	DPRINTF_FOLLOW(("xbddump(%d, %" PRId64 ", %p, %lu)\n", dev, blkno, va,
	    (unsigned long)size));
	GETXBD_SOFTC(xs, dev);
	return dk_dump(xs->sc_di, &xs->sc_dksc, dev, blkno, va, size);
}

static int
xbdinit(struct xbd_softc *xs, xen_disk_t *xd, struct dk_intf *dkintf)
{
	struct dk_geom *pdg;
	char buf[9];
	int ret;

	ret = 0;

	xs->sc_dksc.sc_size = xd->capacity;
	xs->sc_xd_device = xd->device;
	xs->sc_di = dkintf;
	xs->sc_shutdown = 0;

	/*
	 * XXX here we should probe the underlying device.  If we
	 *     are accessing a partition of type RAW_PART, then
	 *     we should populate our initial geometry with the
	 *     geometry that we discover from the device.
	 */
	pdg = &xs->sc_dksc.sc_geom;
	pdg->pdg_secsize = DEV_BSIZE;
	pdg->pdg_ntracks = 1;
	pdg->pdg_nsectors = 1024 * (1024 / pdg->pdg_secsize);
	pdg->pdg_ncylinders = xs->sc_dksc.sc_size / pdg->pdg_nsectors;

	/*
	 * We have one shared bufq for all devices because otherwise
	 * requests can stall if there were no free request slots
	 * available in xbdstart and this device had no requests
	 * in-flight which would trigger a dk_start from the interrupt
	 * handler.
	 * XXX this assumes that we can just memcpy struct bufq_state
	 *     to share it between devices.
	 * XXX we reference count the usage in case so we can de-alloc
	 *     the bufq if all devices are deconfigured.
	 */
	if (bufq_users == 0) {
		bufq_alloc(&bufq, BUFQ_FCFS);
		bufq_users = 1;
	}
	memcpy(&xs->sc_dksc.sc_bufq, &bufq, sizeof(struct bufq_state));

	xs->sc_dksc.sc_flags |= DKF_INITED;

	/* Attach the disk. */
	disk_attach(&xs->sc_dksc.sc_dkdev);

	/* Try and read the disklabel. */
	dk_getdisklabel(xs->sc_di, &xs->sc_dksc, 0 /* XXX ? */);

	format_bytes(buf, sizeof(buf), (uint64_t)xs->sc_dksc.sc_size *
	    pdg->pdg_secsize);
	printf(" %s\n", buf);

/*   out: */
	return ret;
}
