/*	$NetBSD: ciss.c,v 1.3.12.2 2006/12/10 07:17:05 yamt Exp $	*/
/*	$OpenBSD: ciss.c,v 1.14 2006/03/13 16:02:23 mickey Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ciss.c,v 1.3.12.2 2006/12/10 07:17:05 yamt Exp $");

/* #define CISS_DEBUG */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/kthread.h>

#include <uvm/uvm_extern.h>

#include <machine/bus.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsi_disk.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/ic/cissreg.h>
#include <dev/ic/cissvar.h>

#ifdef CISS_DEBUG
#define	CISS_DPRINTF(m,a)	if (ciss_debug & (m)) printf a
#define	CISS_D_CMD	0x0001
#define	CISS_D_INTR	0x0002
#define	CISS_D_MISC	0x0004
#define	CISS_D_DMA	0x0008
#define	CISS_D_IOCTL	0x0010
#define	CISS_D_ERR	0x0020
int ciss_debug = 0
	| CISS_D_CMD
	| CISS_D_INTR
	| CISS_D_MISC
	| CISS_D_DMA
	| CISS_D_IOCTL
	| CISS_D_ERR
	;
#else
#define	CISS_DPRINTF(m,a)	/* m, a */
#endif

static void	ciss_scsi_cmd(struct scsipi_channel *chan,
			scsipi_adapter_req_t req, void *arg);
static int	ciss_scsi_ioctl(struct scsipi_channel *chan, u_long cmd,
	    caddr_t addr, int flag, struct proc *p);
static void	cissminphys(struct buf *bp);

#if 0
static void	ciss_scsi_raw_cmd(struct scsipi_channel *chan,
			scsipi_adapter_req_t req, void *arg);
#endif

#if NBIO > 0
static int	ciss_ioctl(struct device *, u_long, caddr_t);
#endif
static int	ciss_sync(struct ciss_softc *sc);
static void	ciss_heartbeat(void *v);
static void	ciss_shutdown(void *v);
#if 0
static void	ciss_kthread(void *v);
#endif

static struct ciss_ccb *ciss_get_ccb(struct ciss_softc *sc);
static void	ciss_put_ccb(struct ciss_ccb *ccb);
static int	ciss_cmd(struct ciss_ccb *ccb, int flags, int wait);
static int	ciss_done(struct ciss_ccb *ccb);
static int	ciss_error(struct ciss_ccb *ccb);
static int	ciss_inq(struct ciss_softc *sc, struct ciss_inquiry *inq);
static int	ciss_ldmap(struct ciss_softc *sc);

static struct ciss_ccb *
ciss_get_ccb(struct ciss_softc *sc)
{
	struct ciss_ccb *ccb;

	if ((ccb = TAILQ_LAST(&sc->sc_free_ccb, ciss_queue_head))) {
		TAILQ_REMOVE(&sc->sc_free_ccb, ccb, ccb_link);
		ccb->ccb_state = CISS_CCB_READY;
	}
	return ccb;
}

static void
ciss_put_ccb(struct ciss_ccb *ccb)
{
	struct ciss_softc *sc = ccb->ccb_sc;

	ccb->ccb_state = CISS_CCB_FREE;
	TAILQ_INSERT_TAIL(&sc->sc_free_ccb, ccb, ccb_link);
}

int
ciss_attach(struct ciss_softc *sc)
{
	struct ciss_ccb *ccb;
	struct ciss_cmd *cmd;
	struct ciss_inquiry *inq;
	bus_dma_segment_t seg[1];
	int error, i, total, rseg, maxfer;
	ciss_lock_t lock;
	paddr_t pa;

	bus_space_read_region_4(sc->sc_iot, sc->cfg_ioh, sc->cfgoff,
	    (u_int32_t *)&sc->cfg, sizeof(sc->cfg) / 4);

	if (sc->cfg.signature != CISS_SIGNATURE) {
		printf(": bad sign 0x%08x\n", sc->cfg.signature);
		return -1;
	}

	if (!(sc->cfg.methods & CISS_METH_SIMPL)) {
		printf(": not simple 0x%08x\n", sc->cfg.methods);
		return -1;
	}

	sc->cfg.rmethod = CISS_METH_SIMPL;
	sc->cfg.paddr_lim = 0;			/* 32bit addrs */
	sc->cfg.int_delay = 0;			/* disable coalescing */
	sc->cfg.int_count = 0;
	strlcpy(sc->cfg.hostname, "HUMPPA", sizeof(sc->cfg.hostname));
	sc->cfg.driverf |= CISS_DRV_PRF;	/* enable prefetch */
	if (!sc->cfg.maxsg)
		sc->cfg.maxsg = MAXPHYS / PAGE_SIZE + 1;

	bus_space_write_region_4(sc->sc_iot, sc->cfg_ioh, sc->cfgoff,
	    (u_int32_t *)&sc->cfg, sizeof(sc->cfg) / 4);
	bus_space_barrier(sc->sc_iot, sc->cfg_ioh, sc->cfgoff, sizeof(sc->cfg),
	    BUS_SPACE_BARRIER_READ|BUS_SPACE_BARRIER_WRITE);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, CISS_IDB, CISS_IDB_CFG);
	bus_space_barrier(sc->sc_iot, sc->sc_ioh, CISS_IDB, 4,
	    BUS_SPACE_BARRIER_WRITE);
	for (i = 1000; i--; DELAY(1000)) {
		/* XXX maybe IDB is really 64bit? - hp dl380 needs this */
		(void)bus_space_read_4(sc->sc_iot, sc->sc_ioh, CISS_IDB + 4);
		if (!(bus_space_read_4(sc->sc_iot, sc->sc_ioh, CISS_IDB) & CISS_IDB_CFG))
			break;
		bus_space_barrier(sc->sc_iot, sc->sc_ioh, CISS_IDB, 4,
		    BUS_SPACE_BARRIER_READ);
	}

	if (bus_space_read_4(sc->sc_iot, sc->sc_ioh, CISS_IDB) & CISS_IDB_CFG) {
		printf(": cannot set config\n");
		return -1;
	}

	bus_space_read_region_4(sc->sc_iot, sc->cfg_ioh, sc->cfgoff,
	    (u_int32_t *)&sc->cfg, sizeof(sc->cfg) / 4);

	if (!(sc->cfg.amethod & CISS_METH_SIMPL)) {
		printf(": cannot simplify 0x%08x\n", sc->cfg.amethod);
		return -1;
	}

	/* i'm ready for you and i hope you're ready for me */
	for (i = 30000; i--; DELAY(1000)) {
		if (bus_space_read_4(sc->sc_iot, sc->cfg_ioh, sc->cfgoff +
		    offsetof(struct ciss_config, amethod)) & CISS_METH_READY)
			break;
		bus_space_barrier(sc->sc_iot, sc->cfg_ioh, sc->cfgoff +
		    offsetof(struct ciss_config, amethod), 4,
		    BUS_SPACE_BARRIER_READ);
	}

	if (!(bus_space_read_4(sc->sc_iot, sc->cfg_ioh, sc->cfgoff +
	    offsetof(struct ciss_config, amethod)) & CISS_METH_READY)) {
		printf(": she never came ready for me 0x%08x\n",
		    sc->cfg.amethod);
		return -1;
	}

	sc->maxcmd = sc->cfg.maxcmd;
	sc->maxsg = sc->cfg.maxsg;
	if (sc->maxsg > MAXPHYS / PAGE_SIZE + 1)
		sc->maxsg = MAXPHYS / PAGE_SIZE + 1;
	i = sizeof(struct ciss_ccb) +
	    sizeof(ccb->ccb_cmd.sgl[0]) * (sc->maxsg - 1);
	for (sc->ccblen = 0x10; sc->ccblen < i; sc->ccblen <<= 1);

	total = sc->ccblen * sc->maxcmd;
	if ((error = bus_dmamem_alloc(sc->sc_dmat, total, PAGE_SIZE, 0,
	    sc->cmdseg, 1, &rseg, BUS_DMA_NOWAIT))) {
		printf(": cannot allocate CCBs (%d)\n", error);
		return -1;
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, sc->cmdseg, rseg, total,
	    (caddr_t *)&sc->ccbs, BUS_DMA_NOWAIT))) {
		printf(": cannot map CCBs (%d)\n", error);
		return -1;
	}
	bzero(sc->ccbs, total);

	if ((error = bus_dmamap_create(sc->sc_dmat, total, 1,
	    total, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &sc->cmdmap))) {
		printf(": cannot create CCBs dmamap (%d)\n", error);
		bus_dmamem_free(sc->sc_dmat, sc->cmdseg, 1);
		return -1;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat, sc->cmdmap, sc->ccbs, total,
	    NULL, BUS_DMA_NOWAIT))) {
		printf(": cannot load CCBs dmamap (%d)\n", error);
		bus_dmamem_free(sc->sc_dmat, sc->cmdseg, 1);
		bus_dmamap_destroy(sc->sc_dmat, sc->cmdmap);
		return -1;
	}

	TAILQ_INIT(&sc->sc_ccbq);
	TAILQ_INIT(&sc->sc_ccbdone);
	TAILQ_INIT(&sc->sc_free_ccb);

	maxfer = sc->maxsg * PAGE_SIZE;
	for (i = 0; total > 0 && i < sc->maxcmd; i++, total -= sc->ccblen) {
		ccb = (struct ciss_ccb *) (sc->ccbs + i * sc->ccblen);
		cmd = &ccb->ccb_cmd;
		pa = sc->cmdseg[0].ds_addr + i * sc->ccblen;

		ccb->ccb_sc = sc;
		ccb->ccb_cmdpa = pa + offsetof(struct ciss_ccb, ccb_cmd);
		ccb->ccb_state = CISS_CCB_FREE;

		cmd->id = htole32(i << 2);
		cmd->id_hi = htole32(0);
		cmd->sgin = sc->maxsg;
		cmd->sglen = htole16((u_int16_t)cmd->sgin);
		cmd->err_len = htole32(sizeof(ccb->ccb_err));
		pa += offsetof(struct ciss_ccb, ccb_err);
		cmd->err_pa = htole64((u_int64_t)pa);

		if ((error = bus_dmamap_create(sc->sc_dmat, maxfer, sc->maxsg,
		    maxfer, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &ccb->ccb_dmamap)))
			break;

		TAILQ_INSERT_TAIL(&sc->sc_free_ccb, ccb, ccb_link);
	}

	if (i < sc->maxcmd) {
		printf(": cannot create ccb#%d dmamap (%d)\n", i, error);
		if (i == 0) {
			/* TODO leaking cmd's dmamaps and shitz */
			bus_dmamem_free(sc->sc_dmat, sc->cmdseg, 1);
			bus_dmamap_destroy(sc->sc_dmat, sc->cmdmap);
			return -1;
		}
	}

	if ((error = bus_dmamem_alloc(sc->sc_dmat, PAGE_SIZE, PAGE_SIZE, 0,
	    seg, 1, &rseg, BUS_DMA_NOWAIT))) {
		printf(": cannot allocate scratch buffer (%d)\n", error);
		return -1;
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, seg, rseg, PAGE_SIZE,
	    (caddr_t *)&sc->scratch, BUS_DMA_NOWAIT))) {
		printf(": cannot map scratch buffer (%d)\n", error);
		return -1;
	}
	bzero(sc->scratch, PAGE_SIZE);

	lock = CISS_LOCK_SCRATCH(sc);
	inq = sc->scratch;
	if (ciss_inq(sc, inq)) {
		printf(": adapter inquiry failed\n");
		CISS_UNLOCK_SCRATCH(sc, lock);
		bus_dmamem_free(sc->sc_dmat, sc->cmdseg, 1);
		bus_dmamap_destroy(sc->sc_dmat, sc->cmdmap);
		return -1;
	}

	if (!(inq->flags & CISS_INQ_BIGMAP)) {
		printf(": big map is not supported, flags=0x%x\n",
		    inq->flags);
		CISS_UNLOCK_SCRATCH(sc, lock);
		bus_dmamem_free(sc->sc_dmat, sc->cmdseg, 1);
		bus_dmamap_destroy(sc->sc_dmat, sc->cmdmap);
		return -1;
	}

	sc->maxunits = inq->numld;
	sc->nbus = inq->nscsi_bus;
	sc->ndrives = inq->buswidth;
	printf(": %d LD%s, HW rev %d, FW %4.4s/%4.4s\n",
	    inq->numld, inq->numld == 1? "" : "s",
	    inq->hw_rev, inq->fw_running, inq->fw_stored);

	CISS_UNLOCK_SCRATCH(sc, lock);

	callout_init(&sc->sc_hb);
	callout_setfunc(&sc->sc_hb, ciss_heartbeat, sc);
	callout_schedule(&sc->sc_hb, hz * 3);

	/* map LDs */
	if (ciss_ldmap(sc)) {
		printf("%s: adapter LD map failed\n", sc->sc_dev.dv_xname);
		bus_dmamem_free(sc->sc_dmat, sc->cmdseg, 1);
		bus_dmamap_destroy(sc->sc_dmat, sc->cmdmap);
		return -1;
	}

/* TODO scan all physdev */
/* TODO scan all logdev */

	sc->sc_flush = CISS_FLUSH_ENABLE;
	if (!(sc->sc_sh = shutdownhook_establish(ciss_shutdown, sc))) {
		printf(": unable to establish shutdown hook\n");
		bus_dmamem_free(sc->sc_dmat, sc->cmdseg, 1);
		bus_dmamap_destroy(sc->sc_dmat, sc->cmdmap);
		return -1;
	}

#if 0
	if (kthread_create(ciss_kthread, sc, NULL, "%s", sc->sc_dev.dv_xname)) {
		printf(": unable to create kernel thread\n");
		shutdownhook_disestablish(sc->sc_sh);
		bus_dmamem_free(sc->sc_dmat, sc->cmdseg, 1);
		bus_dmamap_destroy(sc->sc_dmat, sc->cmdmap);
		return -1;
	}
#endif

	sc->sc_channel.chan_adapter = &sc->sc_adapter;
	sc->sc_channel.chan_bustype = &scsi_bustype;
	sc->sc_channel.chan_channel = 0;
	sc->sc_channel.chan_ntargets = sc->maxunits;
	sc->sc_channel.chan_nluns = 8;
	sc->sc_channel.chan_openings = sc->maxcmd / (sc->maxunits? sc->maxunits : 1);
	sc->sc_channel.chan_flags = 0;
	sc->sc_channel.chan_id = sc->maxunits;

	sc->sc_adapter.adapt_dev = (struct device *) sc;
	sc->sc_adapter.adapt_openings = sc->maxcmd / (sc->maxunits? sc->maxunits : 1);
	sc->sc_adapter.adapt_max_periph = sc->maxunits;
	sc->sc_adapter.adapt_request = ciss_scsi_cmd;
	sc->sc_adapter.adapt_minphys = cissminphys;
	sc->sc_adapter.adapt_ioctl = ciss_scsi_ioctl;
	sc->sc_adapter.adapt_nchannels = 1;
	config_found(&sc->sc_dev, &sc->sc_channel, scsiprint);

#if 0
	sc->sc_link_raw.adapter_softc = sc;
	sc->sc_link.openings = sc->maxcmd / (sc->maxunits? sc->maxunits : 1);
	sc->sc_link_raw.adapter = &ciss_raw_switch;
	sc->sc_link_raw.adapter_target = sc->ndrives;
	sc->sc_link_raw.adapter_buswidth = sc->ndrives;
	config_found(&sc->sc_dev, &sc->sc_channel, scsiprint);
#endif

#if NBIO1 > 0
	if (bio_register(&sc->sc_dev, ciss_ioctl) != 0)
		printf("%s: controller registration failed",
		    sc->sc_dev.dv_xname);
#endif

	return 0;
}

static void
ciss_shutdown(void *v)
{
	struct ciss_softc *sc = v;

	sc->sc_flush = CISS_FLUSH_DISABLE;
	/* timeout_del(&sc->sc_hb); */
	ciss_sync(sc);
}

static void
cissminphys(struct buf *bp)
{
#if 0	/* TOSO */
#define	CISS_MAXFER	(PAGE_SIZE * (sc->maxsg + 1))
	if (bp->b_bcount > CISS_MAXFER)
		bp->b_bcount = CISS_MAXFER;
#endif
	minphys(bp);
}

/*
 * submit a command and optionally wait for completition.
 * wait arg abuses XS_CTL_POLL|XS_CTL_NOSLEEP flags to request
 * to wait (XS_CTL_POLL) and to allow tsleep() (!XS_CTL_NOSLEEP)
 * instead of busy loop waiting
 */
static int
ciss_cmd(struct ciss_ccb *ccb, int flags, int wait)
{
	struct ciss_softc *sc = ccb->ccb_sc;
	struct ciss_cmd *cmd = &ccb->ccb_cmd;
	struct ciss_ccb *ccb1;
	bus_dmamap_t dmap = ccb->ccb_dmamap;
	u_int32_t id;
	int i, tohz, error = 0;

	if (ccb->ccb_state != CISS_CCB_READY) {
		printf("%s: ccb %d not ready state=0x%x\n", sc->sc_dev.dv_xname,
		    cmd->id, ccb->ccb_state);
		return (EINVAL);
	}

	if (ccb->ccb_data) {
		bus_dma_segment_t *sgd;

		if ((error = bus_dmamap_load(sc->sc_dmat, dmap, ccb->ccb_data,
		    ccb->ccb_len, NULL, flags))) {
			if (error == EFBIG)
				printf("more than %d dma segs\n", sc->maxsg);
			else
				printf("error %d loading dma map\n", error);
			ciss_put_ccb(ccb);
			return (error);
		}
		cmd->sgin = dmap->dm_nsegs;

		sgd = dmap->dm_segs;
		CISS_DPRINTF(CISS_D_DMA, ("data=%p/%u<0x%lx/%lu",
		    ccb->ccb_data, ccb->ccb_len, sgd->ds_addr, sgd->ds_len));

		for (i = 0; i < dmap->dm_nsegs; sgd++, i++) {
			cmd->sgl[i].addr_lo = htole32(sgd->ds_addr);
			cmd->sgl[i].addr_hi =
			    htole32((u_int64_t)sgd->ds_addr >> 32);
			cmd->sgl[i].len = htole32(sgd->ds_len);
			cmd->sgl[i].flags = htole32(0);
			if (i) {
				CISS_DPRINTF(CISS_D_DMA,
				    (",0x%lx/%lu", sgd->ds_addr, sgd->ds_len));
			}
		}

		CISS_DPRINTF(CISS_D_DMA, ("> "));

		bus_dmamap_sync(sc->sc_dmat, dmap, 0, dmap->dm_mapsize,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	} else
		cmd->sgin = 0;
	cmd->sglen = htole16((u_int16_t)cmd->sgin);
	bzero(&ccb->ccb_err, sizeof(ccb->ccb_err));

	bus_dmamap_sync(sc->sc_dmat, sc->cmdmap, 0, sc->cmdmap->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	if ((wait & (XS_CTL_POLL|XS_CTL_NOSLEEP)) == (XS_CTL_POLL|XS_CTL_NOSLEEP))
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, CISS_IMR,
		    bus_space_read_4(sc->sc_iot, sc->sc_ioh, CISS_IMR) | sc->iem);

	TAILQ_INSERT_TAIL(&sc->sc_ccbq, ccb, ccb_link);
	ccb->ccb_state = CISS_CCB_ONQ;
	CISS_DPRINTF(CISS_D_CMD, ("submit=0x%x ", cmd->id));
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, CISS_INQ, ccb->ccb_cmdpa);

	if (wait & XS_CTL_POLL) {
		int etick;
		CISS_DPRINTF(CISS_D_CMD, ("waiting "));

		i = ccb->ccb_xs? ccb->ccb_xs->timeout : 60000;
		tohz = (i / 1000) * hz + (i % 1000) * (hz / 1000);
		if (tohz == 0)
			tohz = 1;
		for (i *= 100, etick = tick + tohz; i--; ) {
			if (!(wait & XS_CTL_NOSLEEP)) {
				ccb->ccb_state = CISS_CCB_POLL;
				CISS_DPRINTF(CISS_D_CMD, ("tsleep(%d) ", tohz));
				if (tsleep(ccb, PRIBIO + 1, "ciss_cmd",
				    tohz) == EWOULDBLOCK) {
					break;
				}
				if (ccb->ccb_state != CISS_CCB_ONQ) {
					tohz = etick - tick;
					if (tohz <= 0)
						break;
					CISS_DPRINTF(CISS_D_CMD, ("T"));
					continue;
				}
				ccb1 = ccb;
			} else {
				DELAY(10);

				if (!(bus_space_read_4(sc->sc_iot, sc->sc_ioh,
				    CISS_ISR) & sc->iem)) {
					CISS_DPRINTF(CISS_D_CMD, ("N"));
					continue;
				}

				if ((id = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
				    CISS_OUTQ)) == 0xffffffff) {
					CISS_DPRINTF(CISS_D_CMD, ("Q"));
					continue;
				}

				CISS_DPRINTF(CISS_D_CMD, ("got=0x%x ", id));
				ccb1 = (struct ciss_ccb *)
					(sc->ccbs + (id >> 2) * sc->ccblen);
				ccb1->ccb_cmd.id = htole32(id);
			}

			error = ciss_done(ccb1);
			if (ccb1 == ccb)
				break;
		}

		/* if never got a chance to be done above... */
		if (ccb->ccb_state != CISS_CCB_FREE) {
			ccb->ccb_err.cmd_stat = CISS_ERR_TMO;
			error = ciss_done(ccb);
		}

		CISS_DPRINTF(CISS_D_CMD, ("done %d:%d",
		    ccb->ccb_err.cmd_stat, ccb->ccb_err.scsi_stat));
	}

	if ((wait & (XS_CTL_POLL|XS_CTL_NOSLEEP)) == (XS_CTL_POLL|XS_CTL_NOSLEEP))
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, CISS_IMR,
		    bus_space_read_4(sc->sc_iot, sc->sc_ioh, CISS_IMR) & ~sc->iem);

	return (error);
}

static int
ciss_done(struct ciss_ccb *ccb)
{
	struct ciss_softc *sc = ccb->ccb_sc;
	struct scsipi_xfer *xs = ccb->ccb_xs;
	struct ciss_cmd *cmd;
	ciss_lock_t lock;
	int error = 0;

	CISS_DPRINTF(CISS_D_CMD, ("ciss_done(%p) ", ccb));

	if (ccb->ccb_state != CISS_CCB_ONQ) {
		printf("%s: unqueued ccb %p ready, state=0x%x\n",
		    sc->sc_dev.dv_xname, ccb, ccb->ccb_state);
		return 1;
	}

	lock = CISS_LOCK(sc);
	ccb->ccb_state = CISS_CCB_READY;
	TAILQ_REMOVE(&sc->sc_ccbq, ccb, ccb_link);

	if (ccb->ccb_cmd.id & CISS_CMD_ERR)
		error = ciss_error(ccb);

	cmd = &ccb->ccb_cmd;
	if (ccb->ccb_data) {
		bus_dmamap_sync(sc->sc_dmat, ccb->ccb_dmamap, 0,
		    ccb->ccb_dmamap->dm_mapsize, (cmd->flags & CISS_CDB_IN) ?
		    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, ccb->ccb_dmamap);
		ccb->ccb_xs = NULL;
		ccb->ccb_data = NULL;
	}

	ciss_put_ccb(ccb);

	if (xs) {
		xs->resid = 0;
		CISS_DPRINTF(CISS_D_CMD, ("scsipi_done(%p) ", xs));
		scsipi_done(xs);
	}
	CISS_UNLOCK(sc, lock);

	return error;
}

static int
ciss_error(struct ciss_ccb *ccb)
{
	struct ciss_softc *sc = ccb->ccb_sc;
	struct ciss_error *err = &ccb->ccb_err;
	struct scsipi_xfer *xs = ccb->ccb_xs;
	int rv;

	switch ((rv = le16toh(err->cmd_stat))) {
	case CISS_ERR_OK:
		break;

	case CISS_ERR_INVCMD:
		printf("%s: invalid cmd 0x%x: 0x%x is not valid @ 0x%x[%d]\n",
		    sc->sc_dev.dv_xname, ccb->ccb_cmd.id,
		    err->err_info, err->err_type[3], err->err_type[2]);
		if (xs) {
			bzero(&xs->sense, sizeof(xs->sense));
			xs->sense.scsi_sense.response_code =
				SSD_RCODE_CURRENT | SSD_RCODE_VALID;
			xs->sense.scsi_sense.flags = SKEY_ILLEGAL_REQUEST;
			xs->sense.scsi_sense.asc = 0x24; /* ill field */
			xs->sense.scsi_sense.ascq = 0x0;
			xs->error = XS_SENSE;
		}
		break;

	case CISS_ERR_TMO:
		xs->error = XS_TIMEOUT;
		break;

	case CISS_ERR_UNRUN:
		/* Underrun */
		xs->resid = le32toh(err->resid);
		CISS_DPRINTF(CISS_D_CMD, (" underrun resid=0x%x ",
					  xs->resid));
		break;
	default:
		if (xs) {
			CISS_DPRINTF(CISS_D_CMD, ("scsi_stat=%x ", err->scsi_stat));
			switch (err->scsi_stat) {
			case SCSI_CHECK:
				xs->error = XS_SENSE;
				bcopy(&err->sense[0], &xs->sense,
				    sizeof(xs->sense));
				CISS_DPRINTF(CISS_D_CMD, (" sense=%02x %02x %02x %02x ",
					     err->sense[0], err->sense[1], err->sense[2], err->sense[3]));
				break;

			case XS_BUSY:
				xs->error = XS_BUSY;
				break;

			default:
				CISS_DPRINTF(CISS_D_ERR, ("%s: "
				    "cmd_stat=%x scsi_stat=0x%x resid=0x%x\n",
				    sc->sc_dev.dv_xname, rv, err->scsi_stat,
				    le32toh(err->resid)));
				printf("ciss driver stuffup in %s:%d: %s()\n",
				       __FILE__, __LINE__, __FUNCTION__);
				xs->error = XS_DRIVER_STUFFUP;
				break;
			}
			xs->resid = le32toh(err->resid);
		}
	}
	ccb->ccb_cmd.id &= htole32(~3);

	return rv;
}

static int
ciss_inq(struct ciss_softc *sc, struct ciss_inquiry *inq)
{
	struct ciss_ccb *ccb;
	struct ciss_cmd *cmd;

	ccb = ciss_get_ccb(sc);
	ccb->ccb_len = sizeof(*inq);
	ccb->ccb_data = inq;
	ccb->ccb_xs = NULL;
	cmd = &ccb->ccb_cmd;
	cmd->tgt = htole32(CISS_CMD_MODE_PERIPH);
	cmd->tgt2 = 0;
	cmd->cdblen = 10;
	cmd->flags = CISS_CDB_CMD | CISS_CDB_SIMPL | CISS_CDB_IN;
	cmd->tmo = htole16(0);
	bzero(&cmd->cdb[0], sizeof(cmd->cdb));
	cmd->cdb[0] = CISS_CMD_CTRL_GET;
	cmd->cdb[6] = CISS_CMS_CTRL_CTRL;
	cmd->cdb[7] = sizeof(*inq) >> 8;	/* biiiig endian */
	cmd->cdb[8] = sizeof(*inq) & 0xff;

	return ciss_cmd(ccb, BUS_DMA_NOWAIT, XS_CTL_POLL|XS_CTL_NOSLEEP);
}

static int
ciss_ldmap(struct ciss_softc *sc)
{
	struct ciss_ccb *ccb;
	struct ciss_cmd *cmd;
	struct ciss_ldmap *lmap;
	ciss_lock_t lock;
	int total, rv;

	lock = CISS_LOCK_SCRATCH(sc);
	lmap = sc->scratch;
	lmap->size = htobe32(sc->maxunits * sizeof(lmap->map));
	total = sizeof(*lmap) + (sc->maxunits - 1) * sizeof(lmap->map);

	ccb = ciss_get_ccb(sc);
	ccb->ccb_len = total;
	ccb->ccb_data = lmap;
	ccb->ccb_xs = NULL;
	cmd = &ccb->ccb_cmd;
	cmd->tgt = CISS_CMD_MODE_PERIPH;
	cmd->tgt2 = 0;
	cmd->cdblen = 12;
	cmd->flags = CISS_CDB_CMD | CISS_CDB_SIMPL | CISS_CDB_IN;
	cmd->tmo = htole16(30);
	bzero(&cmd->cdb[0], sizeof(cmd->cdb));
	cmd->cdb[0] = CISS_CMD_LDMAP;
	cmd->cdb[8] = total >> 8;	/* biiiig endian */
	cmd->cdb[9] = total & 0xff;

	rv = ciss_cmd(ccb, BUS_DMA_NOWAIT, XS_CTL_POLL|XS_CTL_NOSLEEP);
	CISS_UNLOCK_SCRATCH(sc, lock);

	if (rv)
		return rv;

	CISS_DPRINTF(CISS_D_MISC, ("lmap %x:%x\n",
	    lmap->map[0].tgt, lmap->map[0].tgt2));

	return 0;
}

static int
ciss_sync(struct ciss_softc *sc)
{
	struct ciss_ccb *ccb;
	struct ciss_cmd *cmd;
	struct ciss_flush *flush;
	ciss_lock_t lock;
	int rv;

	lock = CISS_LOCK_SCRATCH(sc);
	flush = sc->scratch;
	bzero(flush, sizeof(*flush));
	flush->flush = sc->sc_flush;

	ccb = ciss_get_ccb(sc);
	ccb->ccb_len = sizeof(*flush);
	ccb->ccb_data = flush;
	ccb->ccb_xs = NULL;
	cmd = &ccb->ccb_cmd;
	cmd->tgt = CISS_CMD_MODE_PERIPH;
	cmd->tgt2 = 0;
	cmd->cdblen = 10;
	cmd->flags = CISS_CDB_CMD | CISS_CDB_SIMPL | CISS_CDB_OUT;
	cmd->tmo = 0;
	bzero(&cmd->cdb[0], sizeof(cmd->cdb));
	cmd->cdb[0] = CISS_CMD_CTRL_SET;
	cmd->cdb[6] = CISS_CMS_CTRL_FLUSH;
	cmd->cdb[7] = sizeof(*flush) >> 8;	/* biiiig endian */
	cmd->cdb[8] = sizeof(*flush) & 0xff;

	rv = ciss_cmd(ccb, BUS_DMA_NOWAIT, XS_CTL_POLL|XS_CTL_NOSLEEP);
	CISS_UNLOCK_SCRATCH(sc, lock);

	return rv;
}

#if 0
static void
ciss_scsi_raw_cmd(struct scsipi_channel *chan, scsipi_adapter_req_t req,
	void *arg)				/* TODO */
{
	struct scsipi_xfer *xs = (struct scsipi_xfer *) arg;
	struct ciss_rawsoftc *rsc =
		(struct ciss_rawsoftc *) chan->chan_adapter->adapt_dev;
	struct ciss_softc *sc = rsc->sc_softc;
	struct ciss_ccb *ccb;
	struct ciss_cmd *cmd;
	ciss_lock_t lock;
	int error;

	CISS_DPRINTF(CISS_D_CMD, ("ciss_scsi_raw_cmd "));

	switch (req)
	{
	case ADAPTER_REQ_RUN_XFER:
		if (xs->cmdlen > CISS_MAX_CDB) {
			CISS_DPRINTF(CISS_D_CMD, ("CDB too big %p ", xs));
			bzero(&xs->sense, sizeof(xs->sense));
			printf("ciss driver stuffup in %s:%d: %s()\n",
			       __FILE__, __LINE__, __FUNCTION__);
			xs->error = XS_DRIVER_STUFFUP;
			scsipi_done(xs);
			break;
		}

		lock = CISS_LOCK(sc);
		error = 0;
		xs->error = XS_NOERROR;

		/* TODO check this target has not yet employed w/ any volume */

		ccb = ciss_get_ccb(sc);
		cmd = &ccb->ccb_cmd;
		ccb->ccb_len = xs->datalen;
		ccb->ccb_data = xs->data;
		ccb->ccb_xs = xs;

		cmd->cdblen = xs->cmdlen;
		cmd->flags = CISS_CDB_CMD | CISS_CDB_SIMPL;
		if (xs->xs_control & XS_CTL_DATA_IN)
			cmd->flags |= CISS_CDB_IN;
		else if (xs->xs_control & XS_CTL_DATA_OUT)
			cmd->flags |= CISS_CDB_OUT;
		cmd->tmo = xs->timeout < 1000? 1 : xs->timeout / 1000;
		bzero(&cmd->cdb[0], sizeof(cmd->cdb));
		bcopy(xs->cmd, &cmd->cdb[0], CISS_MAX_CDB);

		if (ciss_cmd(ccb, BUS_DMA_WAITOK,
		    xs->xs_control & (XS_CTL_POLL|XS_CTL_NOSLEEP))) {
			printf("ciss driver stuffup in %s:%d: %s()\n",
			       __FILE__, __LINE__, __FUNCTION__);
			xs->error = XS_DRIVER_STUFFUP;
			scsipi_done(xs);
			CISS_UNLOCK(sc, lock);
			break;
		}

		CISS_UNLOCK(sc, lock);
		break;

	case ADAPTER_REQ_GROW_RESOURCES:
		/*
		 * Not supported.
		 */
		break;

	case ADAPTER_REQ_SET_XFER_MODE:
		/*
		 * We can't change the transfer mode, but at least let
		 * scsipi know what the adapter has negociated.
		 */
		 /* Get xfer mode and return it */
		break;
	}
}
#endif

static void
ciss_scsi_cmd(struct scsipi_channel *chan, scsipi_adapter_req_t req,
	void *arg)
{
	struct scsipi_xfer *xs = (struct scsipi_xfer *) arg;
	struct ciss_softc *sc =
		(struct ciss_softc *) chan->chan_adapter->adapt_dev;
	u_int8_t target;
	struct ciss_ccb *ccb;
	struct ciss_cmd *cmd;
	int error;
	ciss_lock_t lock;

	CISS_DPRINTF(CISS_D_CMD, ("ciss_scsi_cmd "));

	switch (req)
	{
	case ADAPTER_REQ_RUN_XFER:
		target = xs->xs_periph->periph_target;
		CISS_DPRINTF(CISS_D_CMD, ("targ=%d ", target));
		if (xs->cmdlen > CISS_MAX_CDB) {
			CISS_DPRINTF(CISS_D_CMD, ("CDB too big %p ", xs));
			bzero(&xs->sense, sizeof(xs->sense));
			printf("ciss driver stuffup in %s:%d: %s()\n",
			       __FILE__, __LINE__, __FUNCTION__);
			xs->error = XS_DRIVER_STUFFUP;
			scsipi_done(xs);
			break;
		}

		lock = CISS_LOCK(sc);
		error = 0;
		xs->error = XS_NOERROR;

		/* XXX emulate SYNCHRONIZE_CACHE ??? */

		ccb = ciss_get_ccb(sc);
		cmd = &ccb->ccb_cmd;
		ccb->ccb_len = xs->datalen;
		ccb->ccb_data = xs->data;
		ccb->ccb_xs = xs;
		cmd->tgt = CISS_CMD_MODE_LD | target;
		cmd->tgt2 = 0;
		cmd->cdblen = xs->cmdlen;
		cmd->flags = CISS_CDB_CMD | CISS_CDB_SIMPL;
		if (xs->xs_control & XS_CTL_DATA_IN)
			cmd->flags |= CISS_CDB_IN;
		else if (xs->xs_control & XS_CTL_DATA_OUT)
			cmd->flags |= CISS_CDB_OUT;
		cmd->tmo = xs->timeout < 1000? 1 : xs->timeout / 1000;
		bzero(&cmd->cdb[0], sizeof(cmd->cdb));
		bcopy(xs->cmd, &cmd->cdb[0], CISS_MAX_CDB);
		CISS_DPRINTF(CISS_D_CMD, ("cmd=%02x %02x %02x %02x %02x %02x ",
			     cmd->cdb[0], cmd->cdb[1], cmd->cdb[2],
			     cmd->cdb[3], cmd->cdb[4], cmd->cdb[5]));

		if (ciss_cmd(ccb, BUS_DMA_WAITOK,
		    xs->xs_control & (XS_CTL_POLL|XS_CTL_NOSLEEP))) {
			printf("ciss driver stuffup in %s:%d: %s()\n",
			       __FILE__, __LINE__, __FUNCTION__);
			xs->error = XS_DRIVER_STUFFUP;
			scsipi_done(xs);
			CISS_UNLOCK(sc, lock);
			return;
		}

		CISS_UNLOCK(sc, lock);
		break;
	case ADAPTER_REQ_GROW_RESOURCES:
		/*
		 * Not supported.
		 */
		break;
	case ADAPTER_REQ_SET_XFER_MODE:
		/*
		 * We can't change the transfer mode, but at least let
		 * scsipi know what the adapter has negociated.
		 */
		/* FIXME: get xfer mode and write it into arg */
		break;
	}
}

int
ciss_intr(void *v)
{
	struct ciss_softc *sc = v;
	struct ciss_ccb *ccb;
	ciss_lock_t lock;
	u_int32_t id;
	int hit = 0;

	CISS_DPRINTF(CISS_D_INTR, ("intr "));

	if (!(bus_space_read_4(sc->sc_iot, sc->sc_ioh, CISS_ISR) & sc->iem))
		return 0;

	lock = CISS_LOCK(sc);
	while ((id = bus_space_read_4(sc->sc_iot, sc->sc_ioh, CISS_OUTQ)) !=
	    0xffffffff) {

		ccb = (struct ciss_ccb *) (sc->ccbs + (id >> 2) * sc->ccblen);
		ccb->ccb_cmd.id = htole32(id);
		if (ccb->ccb_state == CISS_CCB_POLL) {
			ccb->ccb_state = CISS_CCB_ONQ;
			wakeup(ccb);
		} else
			ciss_done(ccb);

		hit = 1;
	}
	CISS_UNLOCK(sc, lock);

	CISS_DPRINTF(CISS_D_INTR, ("exit\n"));
	return hit;
}

static void
ciss_heartbeat(void *v)
{
	struct ciss_softc *sc = v;
	u_int32_t hb;

	hb = bus_space_read_4(sc->sc_iot, sc->cfg_ioh,
	    sc->cfgoff + offsetof(struct ciss_config, heartbeat));
	if (hb == sc->heartbeat)
		panic("ciss: dead");	/* XX reset! */
	else
		sc->heartbeat = hb;

	callout_schedule(&sc->sc_hb, hz * 3);
}

#if 0
static void
ciss_kthread(void *v)
{
	struct ciss_softc *sc = v;
	ciss_lock_t lock;

	for (;;) {
		tsleep(sc, PRIBIO, sc->sc_dev.dv_xname, 0);

		lock = CISS_LOCK(sc);



		CISS_UNLOCK(sc, lock);
	}
}
#endif

static int
ciss_scsi_ioctl(struct scsipi_channel *chan, u_long cmd,
    caddr_t addr, int flag, struct proc *p)
{
#if NBIO > 0
	return ciss_ioctl(chan->chan_adapter->adapt_dev, cmd, addr);
#else
	return ENOTTY;
#endif
}

#if NBIO > 0
static int
ciss_ioctl(struct device *dev, u_long cmd, caddr_t addr)	/* TODO */
{
	/* struct ciss_softc *sc = (struct ciss_softc *)dev; */
	ciss_lock_t lock;
	int error;

	lock = CISS_LOCK(sc);
	switch (cmd) {
	case BIOCINQ:
	case BIOCVOL:
	case BIOCDISK:
	case BIOCALARM:
	case BIOCBLINK:
	case BIOCSETSTATE:
	default:
		CISS_DPRINTF(CISS_D_IOCTL, ("%s: invalid ioctl\n",
		    sc->sc_dev.dv_xname));
		error = ENOTTY;
	}
	CISS_UNLOCK(sc, lock);

	return error;
}
#endif
