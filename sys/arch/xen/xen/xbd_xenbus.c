/*      $NetBSD: xbd_xenbus.c,v 1.7.2.4.2.3 2007/01/22 13:45:13 tron Exp $      */

/*
 * Copyright (c) 2006 Manuel Bouyer.
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
 *      This product includes software developed by Manuel Bouyer.
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
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: xbd_xenbus.c,v 1.7.2.4.2.3 2007/01/22 13:45:13 tron Exp $");

#include "opt_xen.h"
#include "rnd.h"

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/stat.h>
#include <sys/vnode.h>

#include <dev/dkvar.h>

#include <uvm/uvm.h>

#include <machine/xen3-public/io/ring.h>
#include <machine/xen3-public/io/blkif.h>

#include <machine/granttables.h>
#include <machine/xenbus.h>
#include "locators.h"

#undef XBD_DEBUG
#ifdef XBD_DEBUG
#define DPRINTF(x) printf x;
#else
#define DPRINTF(x)
#endif

#define GRANT_INVALID_REF -1

#define XBD_RING_SIZE __RING_SIZE((blkif_sring_t *)0, PAGE_SIZE)

#define XEN_BSHIFT      9               /* log2(XEN_BSIZE) */
#define XEN_BSIZE       (1 << XEN_BSHIFT) 

struct xbd_req {
	SLIST_ENTRY(xbd_req) req_next;
	uint16_t req_id; /* ID passed to backed */
	grant_ref_t req_gntref[BLKIF_MAX_SEGMENTS_PER_REQUEST];
	int req_nr_segments; /* number of segments in this request */
	struct buf *req_bp; /* buffer associated with this request */
	void *req_data; /* pointer to the data buffer */
};

struct xbd_xenbus_softc {
	struct device sc_dev;
	struct dk_softc sc_dksc;
	struct dk_intf *sc_di;
	struct xenbus_device *sc_xbusd;

	blkif_front_ring_t sc_ring;

	unsigned int sc_evtchn;

	grant_ref_t sc_ring_gntref;

	struct xbd_req sc_reqs[XBD_RING_SIZE];
	SLIST_HEAD(,xbd_req) sc_xbdreq_head; /* list of free requests */

	int sc_backend_status; /* our status with backend */
#define BLKIF_STATE_DISCONNECTED 0
#define BLKIF_STATE_CONNECTED    1
#define BLKIF_STATE_SUSPENDED    2
	int sc_shutdown;

	u_long sc_sectors; /* number of sectors for this device */
	u_long sc_secsize; /* sector size */
	u_long sc_info; /* VDISK_* */
	u_long sc_handle; /* from backend */
};

#if 0
/* too big to be on stack */
static multicall_entry_t rq_mcl[XBD_RING_SIZE+1];
static paddr_t rq_pages[XBD_RING_SIZE];
#endif

static int  xbd_xenbus_match(struct device *, struct cfdata *, void *);
static void xbd_xenbus_attach(struct device *, struct device *, void *);
static int  xbd_xenbus_detach(struct device *, int);

static int  xbd_xenbus_resume(void *);
static int  xbd_handler(void *);
static int  xbdstart(struct dk_softc *, struct buf *);
static void xbd_backend_changed(struct device *, XenbusState);
static void xbd_connect(struct xbd_xenbus_softc *);

static int  xbd_map_align(struct xbd_req *);
static void xbd_unmap_align(struct xbd_req *);

CFATTACH_DECL(xbd_xenbus, sizeof(struct xbd_xenbus_softc),
   xbd_xenbus_match, xbd_xenbus_attach, xbd_xenbus_detach, NULL);

dev_type_open(xbdopen);
dev_type_close(xbdclose);
dev_type_read(xbdread);
dev_type_write(xbdwrite);
dev_type_ioctl(xbdioctl);
dev_type_strategy(xbdstrategy);
dev_type_dump(xbddump);
dev_type_size(xbdsize);

const struct bdevsw xbd_bdevsw = {
	xbdopen, xbdclose, xbdstrategy, xbdioctl,
	xbddump, xbdsize, D_DISK
};

const struct cdevsw xbd_cdevsw = {
	xbdopen, xbdclose, xbdread, xbdwrite, xbdioctl,
	nostop, notty, nopoll, nommap, nokqfilter, D_DISK
};

extern struct cfdriver xbd_cd;

/* Pseudo-disk Interface */
static struct dk_intf dkintf_esdi = {
        DTYPE_ESDI,
	"Xen Virtual ESDI",
	xbdopen,
	xbdclose,
	xbdstrategy,
	xbdstart,
};

static struct dkdriver xbddkdriver = {
        .d_strategy = xbdstrategy,
	.d_minphys = minphys,
};

static int
xbd_xenbus_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct xenbusdev_attach_args *xa = aux;

	if (strcmp(xa->xa_type, "vbd") != 0)
		return 0;

	if (match->cf_loc[XENBUSCF_ID] != XENBUSCF_ID_DEFAULT &&
	    match->cf_loc[XENBUSCF_ID] != xa->xa_id)
		return 0;

	return 1;
}

static void
xbd_xenbus_attach(struct device *parent, struct device *self, void *aux)
{
	struct xbd_xenbus_softc *sc = (void *)self;
	struct xenbusdev_attach_args *xa = aux;
	RING_IDX i;
#ifdef XBD_DEBUG
	char **dir, *val;
	int dir_n = 0;
	char id_str[20];
	int err;
#endif

	config_pending_incr();
	printf(": Xen Virtual Block Device Interface\n");
#ifdef XBD_DEBUG
	printf("path: %s\n", xa->xa_xbusd->xbusd_path);
	snprintf(id_str, sizeof(id_str), "%d", xa->xa_id);
	err = xenbus_directory(NULL, "device/vbd", id_str, &dir_n, &dir);
	if (err) {
		printf("%s: xenbus_directory err %d\n",
		    sc->sc_dev.dv_xname, err);
	} else {
		printf("%s/\n", xa->xa_xbusd->xbusd_path);
		for (i = 0; i < dir_n; i++) {
			printf("\t/%s", dir[i]);
			err = xenbus_read(NULL, xa->xa_xbusd->xbusd_path, dir[i],
			    NULL, &val);
			if (err) {
				printf("%s: xenbus_read err %d\n",
		    		sc->sc_dev.dv_xname, err);
			} else {
				printf(" = %s\n", val);
				free(val, M_DEVBUF);
			}
		}
	}
#endif /* XBD_DEBUG */
	sc->sc_xbusd = xa->xa_xbusd;
	sc->sc_xbusd->xbusd_otherend_changed = xbd_backend_changed;

	dk_sc_init(&sc->sc_dksc, sc, sc->sc_dev.dv_xname);
	sc->sc_dksc.sc_dkdev.dk_driver = &xbddkdriver;
	sc->sc_di = &dkintf_esdi;
	/* initialize free requests list */
	SLIST_INIT(&sc->sc_xbdreq_head);
	for (i = 0; i < XBD_RING_SIZE; i++) {
		sc->sc_reqs[i].req_id = i;
		SLIST_INSERT_HEAD(&sc->sc_xbdreq_head, &sc->sc_reqs[i],
		    req_next);
	}

	sc->sc_backend_status = BLKIF_STATE_DISCONNECTED;
	sc->sc_shutdown = 1;
	/* initialise shared structures and tell backend that we are ready */
	xbd_xenbus_resume(sc);
}

static int
xbd_xenbus_detach(struct device *dev, int flags)
{
	struct xbd_xenbus_softc *sc = (void *)dev;
	int s, bmaj, cmaj, i, mn;
	s = splbio();
	DPRINTF(("%s: xbd_detach\n", dev->dv_xname));
	if (sc->sc_shutdown == 0) {
		sc->sc_shutdown = 1;
		/* wait for requests to complete */
		while (sc->sc_backend_status == BLKIF_STATE_CONNECTED &&
		    sc->sc_dksc.sc_dkdev.dk_busy > 0)
			tsleep(xbd_xenbus_detach, PRIBIO, "xbddetach", hz/2);
	}
	splx(s);

	/* locate the major number */
	bmaj = bdevsw_lookup_major(&xbd_bdevsw);
	cmaj = cdevsw_lookup_major(&xbd_cdevsw);

	/* Nuke the vnodes for any open instances. */
	for (i = 0; i < MAXPARTITIONS; i++) {
		mn = DISKMINOR(dev->dv_unit, i);
		vdevgone(bmaj, mn, mn, VBLK);
		vdevgone(cmaj, mn, mn, VCHR);
	}
	if (sc->sc_backend_status == BLKIF_STATE_CONNECTED) {
		/* Delete all of our wedges. */
		dkwedge_delall(&sc->sc_dksc.sc_dkdev);

		s = splbio();
		/* Kill off any queued buffers. */
		bufq_drain(&sc->sc_dksc.sc_bufq);
		bufq_free(&sc->sc_dksc.sc_bufq);
		splx(s);

		/* detach disk */
		disk_detach(&sc->sc_dksc.sc_dkdev);
	}

	event_remove_handler(sc->sc_evtchn, &xbd_handler, sc);
	while (xengnt_status(sc->sc_ring_gntref)) {
		tsleep(xbd_xenbus_detach, PRIBIO, "xbd_ref", hz/2);
	}
	xengnt_revoke_access(sc->sc_ring_gntref);
	uvm_km_free(kernel_map, (vaddr_t)sc->sc_ring.sring,
	    PAGE_SIZE);
	return 0;
}

static int
xbd_xenbus_resume(void *p)
{
	struct xbd_xenbus_softc *sc = p;
	struct xenbus_transaction *xbt;
	int error;
	blkif_sring_t *ring;
	paddr_t ma;
	const char *errmsg;

	sc->sc_ring_gntref = GRANT_INVALID_REF;


	/* setup device: alloc event channel and shared ring */
	ring = (void *)uvm_km_zalloc(kernel_map, PAGE_SIZE);
	if (ring == NULL)
		panic("xbd_xenbus_resume: can't alloc rings");

	SHARED_RING_INIT(ring);
	FRONT_RING_INIT(&sc->sc_ring, ring, PAGE_SIZE);

	(void)pmap_extract_ma(pmap_kernel(), (vaddr_t)ring, &ma);
	error = xenbus_grant_ring(sc->sc_xbusd, ma, &sc->sc_ring_gntref);
	if (error)
		return error;
	error = xenbus_alloc_evtchn(sc->sc_xbusd, &sc->sc_evtchn);
	if (error)
		return error;
	aprint_verbose("%s: using event channel %d\n",
	    sc->sc_dev.dv_xname, sc->sc_evtchn);
	event_set_handler(sc->sc_evtchn, &xbd_handler, sc,
	    IPL_BIO, sc->sc_dev.dv_xname);

again:
	xbt = xenbus_transaction_start();
	if (xbt == NULL)
		return ENOMEM;
	error = xenbus_printf(xbt, sc->sc_xbusd->xbusd_path,
	    "ring-ref","%u", sc->sc_ring_gntref);
	if (error) {
		errmsg = "writing ring-ref";
		goto abort_transaction;
	}
	error = xenbus_printf(xbt, sc->sc_xbusd->xbusd_path,
	    "event-channel", "%u", sc->sc_evtchn);
	if (error) {
		errmsg = "writing event channel";
		goto abort_transaction;
	}
	error = xenbus_switch_state(sc->sc_xbusd, xbt, XenbusStateInitialised);
	if (error) {
		errmsg = "writing frontend XenbusStateInitialised";
		goto abort_transaction;
	}
	error = xenbus_transaction_end(xbt, 0);
	if (error == EAGAIN)
		goto again;
	if (error) {
		xenbus_dev_fatal(sc->sc_xbusd, error, "completing transaction");
		return -1;
	}
	return 0;

abort_transaction:
	xenbus_transaction_end(xbt, 1);
	xenbus_dev_fatal(sc->sc_xbusd, error, "%s", errmsg);
	return error;
}

static void xbd_backend_changed(struct device *dev, XenbusState new_state)
{
	struct xbd_xenbus_softc *sc = (void *)dev;
	struct dk_geom *pdg;
	char buf[9];
	int s;
	DPRINTF(("%s: new backend state %d\n", sc->sc_dev.dv_xname, new_state));

	switch (new_state) {
	case XenbusStateUnknown:
	case XenbusStateInitialising:
	case XenbusStateInitWait:
	case XenbusStateInitialised:
		break;
	case XenbusStateClosing:
		s = splbio();
		sc->sc_shutdown = 1;
		/* wait for requests to complete */
		while (sc->sc_backend_status == BLKIF_STATE_CONNECTED &&
		    sc->sc_dksc.sc_dkdev.dk_busy > 0)
			tsleep(xbd_xenbus_detach, PRIBIO, "xbddetach",
			    hz/2);
		splx(s);
		xenbus_switch_state(sc->sc_xbusd, NULL, XenbusStateClosed);
		break;
	case XenbusStateConnected:
		/*
		 * note that xbd_backend_changed() can only be called by
		 * the xenbus thread.
		 */

		if (sc->sc_backend_status == BLKIF_STATE_CONNECTED)
			/* already connected */
			return;

		xbd_connect(sc);
		sc->sc_shutdown = 0;
		hypervisor_enable_event(sc->sc_evtchn);

		sc->sc_dksc.sc_size =
		    (uint64_t)sc->sc_sectors * (uint64_t)sc->sc_secsize /
		    DEV_BSIZE;
		pdg = &sc->sc_dksc.sc_geom;
		pdg->pdg_secsize = DEV_BSIZE;
		pdg->pdg_ntracks = 1;
		pdg->pdg_nsectors = 1024 * (1024 / pdg->pdg_secsize);
		pdg->pdg_ncylinders = sc->sc_dksc.sc_size / pdg->pdg_nsectors;

		bufq_alloc(&sc->sc_dksc.sc_bufq, BUFQ_FCFS);
		sc->sc_dksc.sc_flags |= DKF_INITED;
		disk_attach(&sc->sc_dksc.sc_dkdev);

		sc->sc_backend_status = BLKIF_STATE_CONNECTED;

		/* try to read the disklabel */
		dk_getdisklabel(sc->sc_di, &sc->sc_dksc, 0 /* XXX ? */);
		format_bytes(buf, sizeof(buf), (uint64_t)sc->sc_dksc.sc_size *
		    pdg->pdg_secsize);
		printf("%s: %s, %d bytes/sect x %llu sectors\n",
		    sc->sc_dev.dv_xname, buf, (int)pdg->pdg_secsize,
		    (unsigned long long)sc->sc_dksc.sc_size);
		/* Discover wedges on this disk. */
		dkwedge_discover(&sc->sc_dksc.sc_dkdev);

		/* the disk should be working now */
		config_pending_decr();
		break;
	default:
		panic("bad backend state %d", new_state);
	}
}

static void
xbd_connect(struct xbd_xenbus_softc *sc)
{
	int err;

	err = xenbus_read_ul(NULL,
	    sc->sc_xbusd->xbusd_path, "virtual-device", &sc->sc_handle);
	if (err)
		panic("%s: can't read number from %s/virtual-device\n", 
		    sc->sc_dev.dv_xname, sc->sc_xbusd->xbusd_otherend);
	err = xenbus_read_ul(NULL,
	    sc->sc_xbusd->xbusd_otherend, "sectors", &sc->sc_sectors);
	if (err)
		panic("%s: can't read number from %s/sectors\n", 
		    sc->sc_dev.dv_xname, sc->sc_xbusd->xbusd_otherend);
	err = xenbus_read_ul(NULL,
	    sc->sc_xbusd->xbusd_otherend, "info", &sc->sc_info);
	if (err)
		panic("%s: can't read number from %s/info\n", 
		    sc->sc_dev.dv_xname, sc->sc_xbusd->xbusd_otherend);
	err = xenbus_read_ul(NULL,
	    sc->sc_xbusd->xbusd_otherend, "sector-size", &sc->sc_secsize);
	if (err)
		panic("%s: can't read number from %s/sector-size\n", 
		    sc->sc_dev.dv_xname, sc->sc_xbusd->xbusd_otherend);

	xenbus_switch_state(sc->sc_xbusd, NULL, XenbusStateConnected);
}

static int
xbd_handler(void *arg)
{
	struct xbd_xenbus_softc *sc = arg;
	struct buf *bp;
	RING_IDX resp_prod, i;
	int more_to_do;
	int seg;

	DPRINTF(("xbd_handler(%s)\n", sc->sc_dev.dv_xname));

	if (__predict_false(sc->sc_backend_status != BLKIF_STATE_CONNECTED))
		return 0;
again:
	resp_prod = sc->sc_ring.sring->rsp_prod;
	x86_lfence(); /* ensure we see replies up to resp_prod */
	for (i = sc->sc_ring.rsp_cons; i != resp_prod; i++) {
		blkif_response_t *rep = RING_GET_RESPONSE(&sc->sc_ring, i);
		struct xbd_req *xbdreq = &sc->sc_reqs[rep->id];
		bp = xbdreq->req_bp;
		DPRINTF(("xbd_handler(%p): b_bcount = %ld\n",
		    bp, (long)bp->b_bcount));
		for (seg = xbdreq->req_nr_segments - 1; seg >= 0; seg--) {
			if (__predict_false(
			    xengnt_status(xbdreq->req_gntref[seg]))) {
				printf("%s: grant still used by backend\n",
				    sc->sc_dev.dv_xname);
				sc->sc_ring.rsp_cons = i;
				xbdreq->req_nr_segments = seg + 1;
				goto done;
			}
			xengnt_revoke_access(
			    xbdreq->req_gntref[seg]);
			xbdreq->req_nr_segments--;
		}
		if (rep->operation != BLKIF_OP_READ &&
		    rep->operation != BLKIF_OP_WRITE) {
			printf("%s: bad operation %d from backend\n",
			     sc->sc_dev.dv_xname, rep->operation);
				bp->b_flags |= B_ERROR;
				bp->b_error = EIO;
				bp->b_resid = bp->b_bcount;
				goto next;
		}
		if (rep->status != BLKIF_RSP_OKAY) {
				bp->b_flags |= B_ERROR;
				bp->b_error = EIO;
				bp->b_resid = bp->b_bcount;
				goto next;
		}
		/* b_resid was set in xbdstart */
next:
		if (bp->b_data != xbdreq->req_data)
			xbd_unmap_align(xbdreq);
		disk_unbusy(&sc->sc_dksc.sc_dkdev,
		    (bp->b_bcount - bp->b_resid),
		    (bp->b_flags & B_READ));
		biodone(bp);
		SLIST_INSERT_HEAD(&sc->sc_xbdreq_head, xbdreq, req_next);
	}
	x86_lfence();
	sc->sc_ring.rsp_cons = i;
	RING_FINAL_CHECK_FOR_RESPONSES(&sc->sc_ring, more_to_do);
	if (more_to_do)
		goto again;
done:
	dk_iodone(sc->sc_di, &sc->sc_dksc);
	return 1;
}

int
xbdopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct	xbd_xenbus_softc *sc;

	if (DISKUNIT(dev) > xbd_cd.cd_ndevs)
		return (ENXIO);
	sc = xbd_cd.cd_devs[DISKUNIT(dev)];
	if (sc == NULL)
		return (ENXIO);
	if ((flags & FWRITE) && (sc->sc_info & VDISK_READONLY))
		return EROFS;

	DPRINTF(("xbdopen(0x%04x, %d)\n", dev, flags));
	return dk_open(sc->sc_di, &sc->sc_dksc, dev, flags, fmt, p);
}

int
xbdclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct	xbd_xenbus_softc *sc = xbd_cd.cd_devs[DISKUNIT(dev)];

	DPRINTF(("xbdclose(%d, %d)\n", dev, flags));
	return dk_close(sc->sc_di, &sc->sc_dksc, dev, flags, fmt, p);
}

void
xbdstrategy(struct buf *bp)
{
	struct	xbd_xenbus_softc *sc = xbd_cd.cd_devs[DISKUNIT(bp->b_dev)];

	DPRINTF(("xbdstrategy(%p): b_bcount = %ld\n", bp,
	    (long)bp->b_bcount));

	if (sc == NULL || sc->sc_shutdown) {
		bp->b_flags |= B_ERROR;
		bp->b_error = EIO;
		biodone(bp);
		return;
	}
	if (__predict_false((sc->sc_info & VDISK_READONLY) &&
	    (bp->b_flags & B_READ) == 0)) {
		bp->b_flags |= B_ERROR;
		bp->b_error = EROFS;
		biodone(bp);
		return;
	}

	dk_strategy(sc->sc_di, &sc->sc_dksc, bp);
	return;
}

int
xbdsize(dev_t dev)
{
	struct	xbd_xenbus_softc *sc;

	DPRINTF(("xbdsize(%d)\n", dev));
	if (DISKUNIT(dev) > xbd_cd.cd_ndevs)
		return (ENXIO);
	sc = xbd_cd.cd_devs[DISKUNIT(dev)];

	if (sc->sc_shutdown)
		return -1;
	return dk_size(sc->sc_di, &sc->sc_dksc, dev);
}

int
xbdread(dev_t dev, struct uio *uio, int flags)
{
	struct	xbd_xenbus_softc *sc = xbd_cd.cd_devs[DISKUNIT(dev)];
	struct  dk_softc *dksc = &sc->sc_dksc;

	if ((dksc->sc_flags & DKF_INITED) == 0)
		return ENXIO;
	return physio(xbdstrategy, NULL, dev, B_READ, minphys, uio);
}

int
xbdwrite(dev_t dev, struct uio *uio, int flags)
{
	struct	xbd_xenbus_softc *sc = xbd_cd.cd_devs[DISKUNIT(dev)];
	struct  dk_softc *dksc = &sc->sc_dksc;

	if ((dksc->sc_flags & DKF_INITED) == 0)
		return ENXIO;
	if (__predict_false(sc->sc_info & VDISK_READONLY))
		return EROFS;
	return physio(xbdstrategy, NULL, dev, B_WRITE, minphys, uio);
}

int
xbdioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct	xbd_xenbus_softc *sc = xbd_cd.cd_devs[DISKUNIT(dev)];
	struct	dk_softc *dksc;
	int	error;
	struct	disk *dk;

	DPRINTF(("xbdioctl(%d, %08lx, %p, %d, %p)\n",
	    dev, cmd, data, flag, l));
	dksc = &sc->sc_dksc;
	dk = &dksc->sc_dkdev;

	error = dk_ioctl(sc->sc_di, dksc, dev, cmd, data, flag, p);
	return error;
}

int
xbddump(dev_t dev, daddr_t blkno, caddr_t va, size_t size)
{
	struct	xbd_xenbus_softc *sc = xbd_cd.cd_devs[DISKUNIT(dev)];

	if (DISKUNIT(dev) > xbd_cd.cd_ndevs)
		return (ENXIO);
	sc = xbd_cd.cd_devs[DISKUNIT(dev)];
	if (sc == NULL)
		return (ENXIO);

	DPRINTF(("xbddump(%d, %" PRId64 ", %p, %lu)\n", dev, blkno, va,
	    (unsigned long)size));
	return dk_dump(sc->sc_di, &sc->sc_dksc, dev, blkno, va, size);
}

static int
xbdstart(struct dk_softc *dksc, struct buf *bp)
{
	struct xbd_xenbus_softc *sc = xbd_cd.cd_devs[DISKUNIT(bp->b_dev)];
	struct xbd_req *xbdreq;
	blkif_request_t *req;
	int ret = 0, runqueue = 1;
	size_t bcount, off;
	paddr_t ma;
	vaddr_t va;
	int nsects, nbytes, seg;
	int notify;
	daddr_t bn;
	struct  partition *pp;

	DPRINTF(("xbdstart(%p): b_bcount = %ld\n", bp, (long)bp->b_bcount));


	if (sc == NULL || sc->sc_shutdown) {
		bp->b_error = EIO;
		goto err;
	}

	bn = bp->b_blkno;
	if (DISKPART(bp->b_dev) != RAW_PART) {
		pp = &sc->sc_dksc.sc_dkdev.dk_label->d_partitions[
		    DISKPART(bp->b_dev)];
		bn += pp->p_offset;
	}

	if (bn < 0 || bn > sc->sc_dksc.sc_size) {
		/* invalid block number */
		bp->b_error = EINVAL;
		goto err;
	}

	if (bn == sc->sc_dksc.sc_size) {
		/* at end of disk; return short read */
		bp->b_resid = bp->b_bcount;
		biodone(bp);
		return 0;
	}
		

	if (RING_FULL(&sc->sc_ring)) {
		DPRINTF(("xbdstart: ring_full\n"));
		ret = -1;
		goto out;
	}

	dksc = &sc->sc_dksc;

	xbdreq = SLIST_FIRST(&sc->sc_xbdreq_head);
	if (__predict_false(xbdreq == NULL)) {
		DPRINTF(("xbdstart: no req\n"));
		ret = -1; /* dk_start should not remove bp from queue */
		goto out;
	}

	xbdreq->req_bp = bp;
	xbdreq->req_data = bp->b_data;
	if ((vaddr_t)bp->b_data & (XEN_BSIZE - 1)) {
		if (__predict_false(xbd_map_align(xbdreq) != 0)) {
			ret = -1;
			goto out;
		}
	}
	/* now we're sure we'll send this buf */
	disk_busy(&dksc->sc_dkdev);
	SLIST_REMOVE_HEAD(&sc->sc_xbdreq_head, req_next);
	req = RING_GET_REQUEST(&sc->sc_ring, sc->sc_ring.req_prod_pvt);
	req->id = xbdreq->req_id;
	req->operation = bp->b_flags & B_READ ? BLKIF_OP_READ : BLKIF_OP_WRITE;
	req->sector_number = bn;
	req->handle = sc->sc_handle;

	va = (vaddr_t)xbdreq->req_data & ~PAGE_MASK;
	off = (vaddr_t)xbdreq->req_data & PAGE_MASK;
	if (bn + bp->b_bcount / DEV_BSIZE >= sc->sc_dksc.sc_size) {
		bcount = (sc->sc_dksc.sc_size - bn) * DEV_BSIZE;
		bp->b_resid = bp->b_bcount - bcount;
	} else {
		bcount = bp->b_bcount;
		bp->b_resid = 0;
	}
	for (seg = 0, bcount = bp->b_bcount; bcount > 0;) {
		pmap_extract_ma(pmap_kernel(), va, &ma);
		KASSERT((ma & (XEN_BSIZE - 1)) == 0);
		if (bcount > PAGE_SIZE - off)
			nbytes = PAGE_SIZE - off;
		else
			nbytes = bcount;
		nsects = nbytes >> XEN_BSHIFT;
		req->seg[seg].first_sect = off >> XEN_BSHIFT;
		req->seg[seg].last_sect = (off >> XEN_BSHIFT) + nsects - 1;
		KASSERT(req->seg[seg].first_sect <= req->seg[seg].last_sect);
		KASSERT(req->seg[seg].last_sect < 8);
		if (__predict_false(xengnt_grant_access(
		    sc->sc_xbusd->xbusd_otherend_id, ma,
		    (bp->b_flags & B_READ) == 0, &xbdreq->req_gntref[seg])))
			panic("xbdstart: xengnt_grant_access"); /* XXX XXX !!! */
		req->seg[seg].gref = xbdreq->req_gntref[seg];
		seg++;
		KASSERT(seg <= BLKIF_MAX_SEGMENTS_PER_REQUEST);
		va += PAGE_SIZE;
		off = 0;
		bcount -= nbytes;
	}
	xbdreq->req_nr_segments = req->nr_segments = seg;
	sc->sc_ring.req_prod_pvt++;
	if (BUFQ_PEEK(&sc->sc_dksc.sc_bufq)) {
		 /* we will be called again; don't notify guest yet */
		runqueue = 0;
	}

out:
	if (runqueue) {
		RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(&sc->sc_ring, notify);
		if (notify)
			hypervisor_notify_via_evtchn(sc->sc_evtchn);
	}
	return ret;

err:
	bp->b_flags |= B_ERROR;
	bp->b_resid = bp->b_bcount;
	biodone(bp);
	return 0;
}

static int
xbd_map_align(struct xbd_req *req)
{
	int s = splvm();

	req->req_data = (void *)uvm_km_kmemalloc1(kmem_map, NULL,
	    req->req_bp->b_bcount, XEN_BSIZE,
	    UVM_UNKNOWN_OFFSET, UVM_KMF_NOWAIT);
	splx(s);
	if (__predict_false(req->req_data == NULL))
		return ENOMEM;
	if ((req->req_bp->b_flags & B_READ) == 0)
		memcpy(req->req_data, req->req_bp->b_data,
		    req->req_bp->b_bcount);
	return 0;
}

static void
xbd_unmap_align(struct xbd_req *req)
{
	int s;
	if (req->req_bp->b_flags & B_READ)
		memcpy(req->req_bp->b_data, req->req_data,
		    req->req_bp->b_bcount);
	s = splvm();
	uvm_km_free(kmem_map, (vaddr_t)req->req_data, req->req_bp->b_bcount);
	splx(s);
}
