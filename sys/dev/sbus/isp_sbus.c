/* $NetBSD: isp_sbus.c,v 1.14.2.2 2000/11/20 11:43:06 bouyer Exp $ */
/*
 * This driver, which is contained in NetBSD in the files:
 *
 *	sys/dev/ic/isp.c
 *	sys/dev/ic/ic/isp.c
 *	sys/dev/ic/ic/isp_inline.h
 *	sys/dev/ic/ic/isp_netbsd.c
 *	sys/dev/ic/ic/isp_netbsd.h
 *	sys/dev/ic/ic/isp_target.c
 *	sys/dev/ic/ic/isp_target.h
 *	sys/dev/ic/ic/isp_tpublic.h
 *	sys/dev/ic/ic/ispmbox.h
 *	sys/dev/ic/ic/ispreg.h
 *	sys/dev/ic/ic/ispvar.h
 *	sys/microcode/isp/asm_sbus.h
 *	sys/microcode/isp/asm_1040.h
 *	sys/microcode/isp/asm_1080.h
 *	sys/microcode/isp/asm_12160.h
 *	sys/microcode/isp/asm_2100.h
 *	sys/microcode/isp/asm_2200.h
 *	sys/pci/isp_pci.c
 *	sys/sbus/isp_sbus.c
 *
 * Is being actively maintained by Matthew Jacob (mjacob@netbsd.org).
 * This driver also is shared source with FreeBSD, OpenBSD, Linux, Solaris,
 * Linux versions. This tends to be an interesting maintenance problem.
 *
 * Please coordinate with Matthew Jacob on changes you wish to make here.
 */
/*
 * SBus specific probe and attach routines for Qlogic ISP SCSI adapters.
 *
 * Copyright (c) 1997 by Matthew Jacob
 * NASA AMES Research Center
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/queue.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/autoconf.h>

#include <dev/ic/isp_netbsd.h>
#include <dev/microcode/isp/asm_sbus.h>
#include <dev/sbus/sbusvar.h>

static int isp_sbus_intr __P((void *));
static u_int16_t isp_sbus_rd_reg __P((struct ispsoftc *, int));
static void isp_sbus_wr_reg __P((struct ispsoftc *, int, u_int16_t));
static int isp_sbus_mbxdma __P((struct ispsoftc *));
static int isp_sbus_dmasetup __P((struct ispsoftc *, struct scsipi_xfer *,
	ispreq_t *, u_int16_t *, u_int16_t));
static void isp_sbus_dmateardown __P((struct ispsoftc *, struct scsipi_xfer *,
	u_int32_t));

#ifndef	ISP_1000_RISC_CODE
#define	ISP_1000_RISC_CODE	NULL
#endif

static struct ispmdvec mdvec = {
	isp_sbus_rd_reg,
	isp_sbus_wr_reg,
	isp_sbus_mbxdma,
	isp_sbus_dmasetup,
	isp_sbus_dmateardown,
	NULL,
	NULL,
	NULL,
	ISP_1000_RISC_CODE
};

struct isp_sbussoftc {
	struct ispsoftc	sbus_isp;
	struct sbusdev	sbus_sd;
	sdparam		sbus_dev;
	bus_space_tag_t	sbus_bustag;
	bus_dma_tag_t	sbus_dmatag;
	bus_space_handle_t sbus_reg;
	int		sbus_node;
	int		sbus_pri;
	struct ispmdvec	sbus_mdvec;
	bus_dmamap_t	*sbus_dmamap;
	bus_dmamap_t	sbus_request_dmamap;
	bus_dmamap_t	sbus_result_dmamap;
	int16_t		sbus_poff[_NREG_BLKS];
};


static int isp_match __P((struct device *, struct cfdata *, void *));
static void isp_sbus_attach __P((struct device *, struct device *, void *));
struct cfattach isp_sbus_ca = {
	sizeof (struct isp_sbussoftc), isp_match, isp_sbus_attach
};

static int
isp_match(parent, cf, aux)
        struct device *parent;
        struct cfdata *cf;
        void *aux;
{
	int rv;
#ifdef DEBUG
	static int oneshot = 1;
#endif
	struct sbus_attach_args *sa = aux;

	rv = (strcmp(cf->cf_driver->cd_name, sa->sa_name) == 0 ||
		strcmp("PTI,ptisp", sa->sa_name) == 0 ||
		strcmp("ptisp", sa->sa_name) == 0 ||
		strcmp("SUNW,isp", sa->sa_name) == 0 ||
		strcmp("QLGC,isp", sa->sa_name) == 0);
#ifdef DEBUG
	if (rv && oneshot) {
		oneshot = 0;
		printf("Qlogic ISP Driver, NetBSD (sbus) Platform Version "
		    "%d.%d Core Version %d.%d\n",
		    ISP_PLATFORM_VERSION_MAJOR, ISP_PLATFORM_VERSION_MINOR,
		    ISP_CORE_VERSION_MAJOR, ISP_CORE_VERSION_MINOR);
	}
#endif
	return (rv);
}


static void
isp_sbus_attach(parent, self, aux)
        struct device *parent, *self;
        void *aux;
{
	int freq, ispburst, sbusburst;
	struct sbus_attach_args *sa = aux;
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) self;
	struct ispsoftc *isp = &sbc->sbus_isp;

	printf(" for %s\n", sa->sa_name);

	sbc->sbus_bustag = sa->sa_bustag;
	sbc->sbus_dmatag = sa->sa_dmatag;
	if (sa->sa_nintr != 0)
		sbc->sbus_pri = sa->sa_pri;
	sbc->sbus_mdvec = mdvec;

	if (sa->sa_npromvaddrs != 0) {
		sbc->sbus_reg = (bus_space_handle_t)sa->sa_promvaddrs[0];
	} else {
		if (sbus_bus_map(sa->sa_bustag, sa->sa_slot, sa->sa_offset,
				 sa->sa_size, BUS_SPACE_MAP_LINEAR, 0,
				 &sbc->sbus_reg) != 0) {
			printf("%s: cannot map registers\n", self->dv_xname);
			return;
		}
	}
	sbc->sbus_node = sa->sa_node;

	freq = getpropint(sa->sa_node, "clock-frequency", 0);
	if (freq) {
		/*
		 * Convert from HZ to MHz, rounding up.
		 */
		freq = (freq + 500000)/1000000;
#if	0
		printf("%s: %d MHz\n", self->dv_xname, freq);
#endif
	}
	sbc->sbus_mdvec.dv_clock = freq;

	/*
	 * Now figure out what the proper burst sizes, etc., to use.
	 * Unfortunately, there is no ddi_dma_burstsizes here which
	 * walks up the tree finding the limiting burst size node (if
	 * any).
	 */
	sbusburst = ((struct sbus_softc *)parent)->sc_burst;
	if (sbusburst == 0)
		sbusburst = SBUS_BURST_32 - 1;
	ispburst = getpropint(sa->sa_node, "burst-sizes", -1);
	if (ispburst == -1) {
		ispburst = sbusburst;
	}
	ispburst &= sbusburst;
	ispburst &= ~(1 << 7);
	ispburst &= ~(1 << 6);
	sbc->sbus_mdvec.dv_conf1 =  0;
	if (ispburst & (1 << 5)) {
		sbc->sbus_mdvec.dv_conf1 = BIU_SBUS_CONF1_FIFO_32;
	} else if (ispburst & (1 << 4)) {
		sbc->sbus_mdvec.dv_conf1 = BIU_SBUS_CONF1_FIFO_16;
	} else if (ispburst & (1 << 3)) {
		sbc->sbus_mdvec.dv_conf1 =
		    BIU_SBUS_CONF1_BURST8 | BIU_SBUS_CONF1_FIFO_8;
	}
	if (sbc->sbus_mdvec.dv_conf1) {
		sbc->sbus_mdvec.dv_conf1 |= BIU_BURST_ENABLE;
	}

	/*
	 * Some early versions of the PTI SBus adapter
	 * would fail in trying to download (via poking)
	 * FW. We give up on them.
	 */
	if (strcmp("PTI,ptisp", sa->sa_name) == 0 ||
	    strcmp("ptisp", sa->sa_name) == 0) {
		sbc->sbus_mdvec.dv_ispfw = NULL;
	}

	isp->isp_mdvec = &sbc->sbus_mdvec;
	isp->isp_bustype = ISP_BT_SBUS;
	isp->isp_type = ISP_HA_SCSI_UNKNOWN;
	isp->isp_param = &sbc->sbus_dev;
	bzero(isp->isp_param, sizeof (sdparam));

	sbc->sbus_poff[BIU_BLOCK >> _BLK_REG_SHFT] = BIU_REGS_OFF;
	sbc->sbus_poff[MBOX_BLOCK >> _BLK_REG_SHFT] = SBUS_MBOX_REGS_OFF;
	sbc->sbus_poff[SXP_BLOCK >> _BLK_REG_SHFT] = SBUS_SXP_REGS_OFF;
	sbc->sbus_poff[RISC_BLOCK >> _BLK_REG_SHFT] = SBUS_RISC_REGS_OFF;
	sbc->sbus_poff[DMA_BLOCK >> _BLK_REG_SHFT] = DMA_REGS_OFF;

	/*
	 * Set up logging levels.
	 */
#ifdef	ISP_LOGDEFAULT
	isp->isp_dblev = ISP_LOGDEFAULT;
#else
	isp->isp_dblev = ISP_LOGCONFIG|ISP_LOGWARN|ISP_LOGERR;
#ifdef	SCSIDEBUG
	isp->isp_dblev |= ISP_LOGDEBUG1|ISP_LOGDEBUG2;
#endif
#ifdef	DEBUG
	isp->isp_dblev |= ISP_LOGDEBUG0|ISP_LOGINFO;
#endif
#endif
	isp->isp_confopts = self->dv_cfdata->cf_flags;
	/*
	 * There's no tool on sparc to set NVRAM for ISPs, so ignore it.
	 */
	isp->isp_confopts |= ISP_CFG_NONVRAM;
	ISP_LOCK(isp);
	isp->isp_osinfo.no_mbox_ints = 1;
	isp_reset(isp);
	if (isp->isp_state != ISP_RESETSTATE) {
		ISP_UNLOCK(isp);
		return;
	}
	isp_init(isp);
	if (isp->isp_state != ISP_INITSTATE) {
		isp_uninit(isp);
		ISP_UNLOCK(isp);
		return;
	}
	/* Establish interrupt channel */
	bus_intr_establish(sbc->sbus_bustag, sbc->sbus_pri, IPL_BIO, 0,
	    isp_sbus_intr, sbc);
	ENABLE_INTS(isp);
	ISP_UNLOCK(isp);

	sbus_establish(&sbc->sbus_sd, &sbc->sbus_isp.isp_osinfo._dev);

	/*
	 * do generic attach.
	 */
	isp_attach(isp);
	if (isp->isp_state != ISP_RUNSTATE) {
		isp_uninit(isp);
	}
}

static int
isp_sbus_intr(arg)
	void *arg;
{
	int rv;
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *)arg;
	bus_dmamap_sync(sbc->sbus_dmatag, sbc->sbus_result_dmamap, 0,
	    sbc->sbus_result_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);
	sbc->sbus_isp.isp_osinfo.onintstack = 1;
	rv = isp_intr(arg);
	sbc->sbus_isp.isp_osinfo.onintstack = 0;
	return (rv);
}

static u_int16_t
isp_sbus_rd_reg(isp, regoff)
	struct ispsoftc *isp;
	int regoff;
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	int offset = sbc->sbus_poff[(regoff & _BLK_REG_MASK) >> _BLK_REG_SHFT];
	offset += (regoff & 0xff);
	return (bus_space_read_2(sbc->sbus_bustag, sbc->sbus_reg, offset));
}

static void
isp_sbus_wr_reg(isp, regoff, val)
	struct ispsoftc *isp;
	int regoff;
	u_int16_t val;
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	int offset = sbc->sbus_poff[(regoff & _BLK_REG_MASK) >> _BLK_REG_SHFT];
	offset += (regoff & 0xff);
	bus_space_write_2(sbc->sbus_bustag, sbc->sbus_reg, offset, val);
}

static int
isp_sbus_mbxdma(isp)
	struct ispsoftc *isp;
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	bus_dma_tag_t dmatag = sbc->sbus_dmatag;
	bus_dma_segment_t seg;
	int rs, i;
	size_t n;
	bus_size_t len;

	if (isp->isp_rquest_dma)
		return (0);

	n = sizeof (XS_T **) * isp->isp_maxcmds;
	isp->isp_xflist = (XS_T **) malloc(n, M_DEVBUF, M_WAITOK);
	if (isp->isp_xflist == NULL) {
		isp_prt(isp, ISP_LOGERR, "cannot alloc xflist array");
		return (1);
	}
	bzero(isp->isp_xflist, n);
	n = sizeof (bus_dmamap_t) * isp->isp_maxcmds;
	sbc->sbus_dmamap = (bus_dmamap_t *) malloc(n, M_DEVBUF, M_WAITOK);
	if (sbc->sbus_dmamap == NULL) {
		free(isp->isp_xflist, M_DEVBUF);
		isp->isp_xflist = NULL;
		isp_prt(isp, ISP_LOGERR, "cannot alloc dmamap array");
		return (1);
	}
	for (i = 0; i < isp->isp_maxcmds; i++) {
		/* Allocate a DMA handle */
		if (bus_dmamap_create(dmatag, MAXPHYS, 1, MAXPHYS, 0,
		    BUS_DMA_NOWAIT, &sbc->sbus_dmamap[i]) != 0) {
			isp_prt(isp, ISP_LOGERR, "cmd DMA maps create error");
			break;
		}
	}
	if (i < isp->isp_maxcmds) {
		while (--i >= 0) {
			bus_dmamap_destroy(dmatag, sbc->sbus_dmamap[i]);
		}
		free(isp->isp_xflist, M_DEVBUF);
		free(sbc->sbus_dmamap, M_DEVBUF);
		isp->isp_xflist = NULL;
		sbc->sbus_dmamap = NULL;
		return (1);
	}

	/*
	 * Allocate and map the request queue.
	 */
	len = ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp));
	/* Allocate DMA map */
	if (bus_dmamap_create(dmatag, len, 1, len, 0, BUS_DMA_NOWAIT,
	    &sbc->sbus_request_dmamap) != 0) {
		goto dmafail;
	}

	/* Allocate DMA buffer */
	if (bus_dmamem_alloc(dmatag, len, 0, 0, &seg, 1, &rs, BUS_DMA_NOWAIT)) {
		goto dmafail;
	}

	/* Load the buffer */
	if (bus_dmamap_load_raw(dmatag, sbc->sbus_request_dmamap,
	    &seg, rs, len, BUS_DMA_NOWAIT) != 0) {
		bus_dmamem_free(dmatag, &seg, rs);
		goto dmafail;
	}
	isp->isp_rquest_dma = sbc->sbus_request_dmamap->dm_segs[0].ds_addr;

	/* Map DMA buffer in CPU addressable space */
	if (bus_dmamem_map(dmatag, &seg, rs, len, (caddr_t *)&isp->isp_rquest,
	    BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) {
		bus_dmamap_unload(dmatag, sbc->sbus_request_dmamap);
		bus_dmamem_free(dmatag, &seg, rs);
		goto dmafail;
	}

	/*
	 * Allocate and map the result queue.
	 */
	len = ISP_QUEUE_SIZE(RESULT_QUEUE_LEN(isp));
	/* Allocate DMA map */
	if (bus_dmamap_create(dmatag, len, 1, len, 0, BUS_DMA_NOWAIT,
	    &sbc->sbus_result_dmamap) != 0) {
		goto dmafail;
	}

	/* Allocate DMA buffer */
	if (bus_dmamem_alloc(dmatag, len, 0, 0, &seg, 1, &rs, BUS_DMA_NOWAIT)) {
		goto dmafail;
	}

	/* Load the buffer */
	if (bus_dmamap_load_raw(dmatag, sbc->sbus_result_dmamap,
	    &seg, rs, len, BUS_DMA_NOWAIT) != 0) {
		bus_dmamem_free(dmatag, &seg, rs);
		goto dmafail;
	}

	/* Map DMA buffer in CPU addressable space */
	if (bus_dmamem_map(dmatag, &seg, rs, len, (caddr_t *)&isp->isp_result,
	    BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) {
		bus_dmamap_unload(dmatag, sbc->sbus_result_dmamap);
		bus_dmamem_free(dmatag, &seg, rs);
		goto dmafail;
	}
	isp->isp_result_dma = sbc->sbus_result_dmamap->dm_segs[0].ds_addr;

	return (0);

dmafail:
	for (i = 0; i < isp->isp_maxcmds; i++) {
		bus_dmamap_destroy(dmatag, sbc->sbus_dmamap[i]);
	}
	free(sbc->sbus_dmamap, M_DEVBUF);
	free(isp->isp_xflist, M_DEVBUF);
	isp->isp_xflist = NULL;
	sbc->sbus_dmamap = NULL;
	return (1);
}

/*
 * Map a DMA request.
 * We're guaranteed that rq->req_handle is a value from 1 to isp->isp_maxcmds.
 */

static int
isp_sbus_dmasetup(isp, xs, rq, iptrp, optr)
	struct ispsoftc *isp;
	struct scsipi_xfer *xs;
	ispreq_t *rq;
	u_int16_t *iptrp;
	u_int16_t optr;
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	bus_dmamap_t dmap;
	ispcontreq_t *crq;
	int cansleep = (xs->xs_control & XS_CTL_NOSLEEP) == 0;
	int in = (xs->xs_control & XS_CTL_DATA_IN) != 0;

	if (xs->datalen == 0) {
		rq->req_seg_count = 1;
		goto mbxsync;
	}

	dmap = sbc->sbus_dmamap[isp_handle_index(rq->req_handle)];
	if (dmap->dm_nsegs != 0) {
		panic("%s: dma map already allocated\n", isp->isp_name);
		/* NOTREACHED */
	}
	if (bus_dmamap_load(sbc->sbus_dmatag, dmap, xs->data, xs->datalen,
	    NULL, cansleep? BUS_DMA_WAITOK : BUS_DMA_NOWAIT) != 0) {
		XS_SETERR(xs, HBA_BOTCH);
		return (CMD_COMPLETE);
	}

	bus_dmamap_sync(sbc->sbus_dmatag, dmap, dmap->dm_segs[0].ds_addr,
	    xs->datalen, in? BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);

	if (in) {
		rq->req_flags |= REQFLAG_DATA_IN;
	} else {
		rq->req_flags |= REQFLAG_DATA_OUT;
	}

	if (XS_CDBLEN(xs) > 12) {
		crq = (ispcontreq_t *) ISP_QUEUE_ENTRY(isp->isp_rquest, *iptrp);
		*iptrp = ISP_NXT_QENTRY(*iptrp, RQUEST_QUEUE_LEN(isp));
		if (*iptrp == optr) {
			isp_prt(isp, ISP_LOGDEBUG0, "Request Queue Overflow++");
			bus_dmamap_unload(sbc->sbus_dmatag, dmap);
			XS_SETERR(xs, HBA_BOTCH);
			return (CMD_EAGAIN);
		}
		rq->req_seg_count = 2;
		rq->req_dataseg[0].ds_count = 0;
		rq->req_dataseg[0].ds_base =  0;
		bzero((void *)crq, sizeof (*crq));
		crq->req_header.rqs_entry_count = 1;
		crq->req_header.rqs_entry_type = RQSTYPE_DATASEG;  
		crq->req_dataseg[0].ds_count = xs->datalen;
		crq->req_dataseg[0].ds_base =  dmap->dm_segs[0].ds_addr;
		ISP_SBUSIFY_ISPHDR(isp, &crq->req_header)
	} else {
		rq->req_dataseg[0].ds_count = xs->datalen;
		rq->req_dataseg[0].ds_base = dmap->dm_segs[0].ds_addr;
		rq->req_seg_count = 1;
	}

mbxsync:
        ISP_SWIZZLE_REQUEST(isp, rq);
#if	0
	/*
	 * If we ever map cacheable memory, we need to do something like this.
	 */
        bus_dmamap_sync(sbc->sbus_dmat, sbc->sbus_rquest_dmap, 0,
            sbc->sbus_rquest_dmap->dm_mapsize, BUS_DMASYNC_PREWRITE);
#endif
	return (CMD_QUEUED);
}

static void
isp_sbus_dmateardown(isp, xs, handle)
	struct ispsoftc *isp;
	struct scsipi_xfer *xs;
	u_int32_t handle;
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	bus_dmamap_t dmap;

	dmap = sbc->sbus_dmamap[isp_handle_index(handle)];

	if (dmap->dm_nsegs == 0) {
		panic("%s: dma map not already allocated\n", isp->isp_name);
		/* NOTREACHED */
	}
	bus_dmamap_sync(sbc->sbus_dmatag, dmap, dmap->dm_segs[0].ds_addr,
	    xs->datalen, (xs->xs_control & XS_CTL_DATA_IN)?
	    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sbc->sbus_dmatag, dmap);
}
