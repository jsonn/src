/*	$NetBSD: ld_icp.c,v 1.2.2.2 2002/06/20 03:44:54 nathanw Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * ICP-Vortex "GDT" front-end for ld(4) driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ld_icp.c,v 1.2.2.2 2002/06/20 03:44:54 nathanw Exp $");

#include "rnd.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/endian.h>
#include <sys/dkio.h>
#include <sys/disk.h>
#if NRND > 0
#include <sys/rnd.h>
#endif

#include <uvm/uvm_extern.h>

#include <machine/bus.h>

#include <dev/ldvar.h>

#include <dev/ic/icpreg.h>
#include <dev/ic/icpvar.h>

struct ld_icp_softc {
	struct	ld_softc sc_ld;
	int	sc_hwunit;
};

void	ld_icp_attach(struct device *, struct device *, void *);
int	ld_icp_dobio(struct ld_icp_softc *, void *, int, int, int,
		     struct buf *);
int	ld_icp_dump(struct ld_softc *, void *, int, int);
int	ld_icp_flush(struct ld_softc *);
void	ld_icp_intr(struct icp_ccb *);
int	ld_icp_match(struct device *, struct cfdata *, void *);
int	ld_icp_start(struct ld_softc *, struct buf *);

struct cfattach ld_icp_ca = {
	sizeof(struct ld_icp_softc), ld_icp_match, ld_icp_attach,
};

int
ld_icp_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct icp_attach_args *icpa;

	icpa = aux;

	return (icpa->icpa_unit < ICPA_UNIT_SCSI);
}

void
ld_icp_attach(struct device *parent, struct device *self, void *aux)
{
	struct icp_attach_args *icpa;
	struct ld_icp_softc *sc;
	struct ld_softc *ld;
	struct icp_softc *icp;
	struct icp_cachedrv *cd;
	struct icp_cdevinfo *cdi;
	const char *str;
	int t;

	sc = (struct ld_icp_softc *)self;
	ld = &sc->sc_ld;
	icp = (struct icp_softc *)parent;
	icpa = aux;
	cd = &icp->icp_cdr[icpa->icpa_unit];

	sc->sc_hwunit = icpa->icpa_unit;
	ld->sc_maxxfer = ICP_MAX_XFER;
	ld->sc_secsize = ICP_SECTOR_SIZE;
	ld->sc_start = ld_icp_start;
	ld->sc_dump = ld_icp_dump;
	ld->sc_flush = ld_icp_flush;
	ld->sc_secperunit = cd->cd_size;
	ld->sc_flags = LDF_ENABLED;
	ld->sc_maxqueuecnt = icp->icp_openings;

	if (!icp_cmd(icp, ICP_CACHESERVICE, ICP_IOCTL, ICP_CACHE_DRV_INFO,
	    sc->sc_hwunit, sizeof(struct icp_cdevinfo))) {
		printf(": unable to retrieve device info\n");
		ld->sc_flags = LDF_ENABLED;
		goto out;
	}
	cdi = (struct icp_cdevinfo *)icp->icp_scr;

	printf(": <%.8s>, ", cdi->ld_name);
	t = le32toh(cdi->ld_dtype) >> 16;

	/*
	 * Print device type.
	 */
	if (le32toh(cdi->ld_dcnt) > 1 || le32toh(cdi->ld_slave) != -1)
		str = "RAID-1";
	else if (t == 0)
		str = "JBOD";
	else if (t == 1)
		str = "RAID-0";
	else if (t == 2)
		str = "Chain";
	else
		str = "unknown type";

	printf("type: %s, ", str);

	/*
	 * Print device status.
	 */
	if (t > 2)
		str = "missing";
	else if ((cdi->ld_error & 1) != 0) {
		str = "fault";
		ld->sc_flags = LDF_ENABLED;
	} else if ((cdi->ld_error & 2) != 0)
		str = "invalid";
	else {
		str = "optimal";
		ld->sc_flags = LDF_ENABLED;
	}
		
	printf("status: %s\n", str);

 out:
	ldattach(ld);
}

int
ld_icp_dobio(struct ld_icp_softc *sc, void *data, int datasize, int blkno,
	     int dowrite, struct buf *bp)
{
	struct icp_cachecmd *cc;
	struct icp_ccb *ic;
	struct icp_softc *icp;
	int s, rv;

	icp = (struct icp_softc *)sc->sc_ld.sc_dv.dv_parent;

	/*
	 * Allocate a command control block.
	 */
	ic = icp_ccb_alloc(icp);

	/*
	 * Map the data transfer.
	 */
	cc = &ic->ic_cmd.cmd_packet.cc;
	ic->ic_sg = cc->cc_sg;
	ic->ic_service = ICP_CACHESERVICE;

	rv = icp_ccb_map(icp, ic, data, datasize,
	    dowrite ? IC_XFER_OUT : IC_XFER_IN);
	if (rv != 0) {
		icp_ccb_free(icp, ic);
		return (rv);
	}

	/*
	 * Build the command.
	 */
	ic->ic_cmd.cmd_opcode = htole16((dowrite ? ICP_WRITE : ICP_READ));
	cc->cc_deviceno = htole16(sc->sc_hwunit);
	cc->cc_blockno = htole32(blkno);
	cc->cc_blockcnt = htole32(datasize / ICP_SECTOR_SIZE);
	cc->cc_addr = ~0;	/* scatter gather */
	cc->cc_nsgent = htole32(ic->ic_nsgent);

	ic->ic_cmdlen = (u_long)ic->ic_sg - (u_long)&ic->ic_cmd +
	    ic->ic_nsgent * sizeof(*ic->ic_sg);

	/*
	 * Fire it off to the controller.
	 */
	if (bp == NULL) {
		s = splbio();
		rv = icp_ccb_poll(icp, ic, 10000);
		icp_ccb_unmap(icp, ic);
		icp_ccb_free(icp, ic);
		splx(s);
	} else {
 		ic->ic_intr = ld_icp_intr;
		ic->ic_context = bp;
		ic->ic_dv = &sc->sc_ld.sc_dv;
		icp_ccb_enqueue(icp, ic);
	}

	return (rv);
}

int
ld_icp_start(struct ld_softc *ld, struct buf *bp)
{

	return (ld_icp_dobio((struct ld_icp_softc *)ld, bp->b_data,
	    bp->b_bcount, bp->b_rawblkno, (bp->b_flags & B_READ) == 0, bp));
}

int
ld_icp_dump(struct ld_softc *ld, void *data, int blkno, int blkcnt)
{

	return (ld_icp_dobio((struct ld_icp_softc *)ld, data,
	    blkcnt * ld->sc_secsize, blkno, 1, NULL));
}

int
ld_icp_flush(struct ld_softc *ld)
{
	struct ld_icp_softc *sc;
	struct icp_softc *icp;
	struct icp_cachecmd *cc;
	struct icp_ccb *ic;
	int rv;

	sc = (struct ld_icp_softc *)ld;
	icp = (struct icp_softc *)ld->sc_dv.dv_parent;

	ic = icp_ccb_alloc(icp);
	ic->ic_cmd.cmd_opcode = htole16(ICP_FLUSH);

	cc = &ic->ic_cmd.cmd_packet.cc;
	cc->cc_deviceno = htole16(sc->sc_hwunit);
	cc->cc_blockno = htole32(1);
	cc->cc_blockcnt = 0;
	cc->cc_addr = 0;
	cc->cc_nsgent = 0;

	ic->ic_cmdlen = (u_long)&cc->cc_sg - (u_long)&ic->ic_cmd;
	ic->ic_service = ICP_CACHESERVICE;

	rv = icp_ccb_wait(icp, ic, 30000);
	icp_ccb_free(icp, ic);

	return (rv);
}

void
ld_icp_intr(struct icp_ccb *ic)
{
	struct buf *bp;
	struct ld_icp_softc *sc;
	struct icp_softc *icp;

	bp = ic->ic_context;
	sc = (struct ld_icp_softc *)ic->ic_dv;
	icp = (struct icp_softc *)sc->sc_ld.sc_dv.dv_parent;

	if (ic->ic_status != ICP_S_OK) {
		printf("%s: request failed; status=0x%04x\n",
		    ic->ic_dv->dv_xname, ic->ic_status);
		bp->b_flags |= B_ERROR;
		bp->b_error = EIO;
		bp->b_resid = bp->b_bcount;
	} else
		bp->b_resid = 0;

	icp_ccb_unmap(icp, ic);
	icp_ccb_free(icp, ic);
	lddone(&sc->sc_ld, bp);
}
