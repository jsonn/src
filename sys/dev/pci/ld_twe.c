/*	$NetBSD: ld_twe.c,v 1.6.2.3 2002/06/20 03:45:35 nathanw Exp $	*/

/*-
 * Copyright (c) 2000, 2001, 2002 The NetBSD Foundation, Inc.
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
 * 3ware "Escalade" RAID controller front-end for ld(4) driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ld_twe.c,v 1.6.2.3 2002/06/20 03:45:35 nathanw Exp $");

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

#include <machine/bus.h>

#include <uvm/uvm_extern.h>

#include <dev/ldvar.h>

#include <dev/pci/twereg.h>
#include <dev/pci/twevar.h>

struct ld_twe_softc {
	struct	ld_softc sc_ld;
	int	sc_hwunit;
};

static void	ld_twe_attach(struct device *, struct device *, void *);
static int	ld_twe_dobio(struct ld_twe_softc *, void *, int, int, int,
			     struct buf *);
static int	ld_twe_dump(struct ld_softc *, void *, int, int);
static void	ld_twe_handler(struct twe_ccb *, int);
static int	ld_twe_match(struct device *, struct cfdata *, void *);
static int	ld_twe_start(struct ld_softc *, struct buf *);

struct cfattach ld_twe_ca = {
	sizeof(struct ld_twe_softc), ld_twe_match, ld_twe_attach
};

static int
ld_twe_match(struct device *parent, struct cfdata *match, void *aux)
{

	return (1);
}

static void
ld_twe_attach(struct device *parent, struct device *self, void *aux)
{
	struct twe_attach_args *twea;
	struct ld_twe_softc *sc;
	struct ld_softc *ld;
	struct twe_softc *twe;

	sc = (struct ld_twe_softc *)self;
	ld = &sc->sc_ld;
	twe = (struct twe_softc *)parent;
	twea = aux;

	sc->sc_hwunit = twea->twea_unit;
	ld->sc_flags = LDF_ENABLED;
	ld->sc_maxxfer = twe_get_maxxfer(twe_get_maxsegs());
	ld->sc_secperunit = twe->sc_dsize[twea->twea_unit];
	ld->sc_secsize = TWE_SECTOR_SIZE;
	ld->sc_maxqueuecnt = (TWE_MAX_QUEUECNT - 1) / twe->sc_nunits;
	ld->sc_start = ld_twe_start;
	ld->sc_dump = ld_twe_dump;

	printf("\n");
	ldattach(ld);
}

static int
ld_twe_dobio(struct ld_twe_softc *sc, void *data, int datasize, int blkno,
	     int dowrite, struct buf *bp)
{
	struct twe_ccb *ccb;
	struct twe_cmd *tc;
	struct twe_softc *twe;
	int s, rv, flags;

	twe = (struct twe_softc *)sc->sc_ld.sc_dv.dv_parent;

	flags = (dowrite ? TWE_CCB_DATA_OUT : TWE_CCB_DATA_IN);
	if ((rv = twe_ccb_alloc(twe, &ccb, flags)) != 0)
		return (rv);

	ccb->ccb_data = data;
	ccb->ccb_datasize = datasize;
	tc = ccb->ccb_cmd;

	/* Build the command. */
	tc->tc_size = 3;
	tc->tc_unit = sc->sc_hwunit;
	tc->tc_count = htole16(datasize / TWE_SECTOR_SIZE);
	tc->tc_args.io.lba = htole32(blkno);

	if (dowrite)
		tc->tc_opcode = TWE_OP_WRITE | (tc->tc_size << 5);
	else
		tc->tc_opcode = TWE_OP_READ | (tc->tc_size << 5);

	/* Map the data transfer. */
	if ((rv = twe_ccb_map(twe, ccb)) != 0) {
		twe_ccb_free(twe, ccb);
		return (rv);
	}

	if (bp == NULL) {
		/*
		 * Polled commands must not sit on the software queue.  Wait
		 * up to 2 seconds for the command to complete.
		 */
		s = splbio();
		rv = twe_ccb_poll(twe, ccb, 2000);
		twe_ccb_unmap(twe, ccb);
		twe_ccb_free(twe, ccb);
		splx(s);
	} else {
		ccb->ccb_tx.tx_handler = ld_twe_handler;
		ccb->ccb_tx.tx_context = bp;
		ccb->ccb_tx.tx_dv = (struct device *)sc;
		twe_ccb_enqueue(twe, ccb);
		rv = 0;
	}

	return (rv);
}

static int
ld_twe_start(struct ld_softc *ld, struct buf *bp)
{

	return (ld_twe_dobio((struct ld_twe_softc *)ld, bp->b_data,
	    bp->b_bcount, bp->b_rawblkno, (bp->b_flags & B_READ) == 0, bp));
}

static void
ld_twe_handler(struct twe_ccb *ccb, int error)
{
	struct buf *bp;
	struct twe_context *tx;
	struct ld_twe_softc *sc;
	struct twe_softc *twe;

	tx = &ccb->ccb_tx;
	bp = tx->tx_context;
	sc = (struct ld_twe_softc *)tx->tx_dv;
	twe = (struct twe_softc *)sc->sc_ld.sc_dv.dv_parent;

	twe_ccb_unmap(twe, ccb);
	twe_ccb_free(twe, ccb);

	if (error) {
		bp->b_flags |= B_ERROR;
		bp->b_error = error;
		bp->b_resid = bp->b_bcount;
	} else
		bp->b_resid = 0;

	lddone(&sc->sc_ld, bp);
}

static int
ld_twe_dump(struct ld_softc *ld, void *data, int blkno, int blkcnt)
{

	return (ld_twe_dobio((struct ld_twe_softc *)ld, data,
	    blkcnt * ld->sc_secsize, blkno, 1, NULL));
}
