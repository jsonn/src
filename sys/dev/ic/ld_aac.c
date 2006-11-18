/*	$NetBSD: ld_aac.c,v 1.11.8.1 2006/11/18 21:34:13 ad Exp $	*/

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ld_aac.c,v 1.11.8.1 2006/11/18 21:34:13 ad Exp $");

#include "rnd.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/endian.h>
#include <sys/dkio.h>
#include <sys/disk.h>
#if NRND > 0
#include <sys/rnd.h>
#endif

#include <machine/bus.h>

#include <uvm/uvm_extern.h>

#include <dev/ldvar.h>

#include <dev/ic/aacreg.h>
#include <dev/ic/aacvar.h>

struct ld_aac_softc {
	struct	ld_softc sc_ld;
	int	sc_hwunit;
};

static void	ld_aac_attach(struct device *, struct device *, void *);
static void	ld_aac_intr(struct aac_ccb *);
static int	ld_aac_dobio(struct ld_aac_softc *, void *, int, int, int,
			     struct buf *);
static int	ld_aac_dump(struct ld_softc *, void *, int, int);
static int	ld_aac_match(struct device *, struct cfdata *, void *);
static int	ld_aac_start(struct ld_softc *, struct buf *);

CFATTACH_DECL(ld_aac, sizeof(struct ld_aac_softc),
    ld_aac_match, ld_aac_attach, NULL, NULL);

static int
ld_aac_match(struct device *parent, struct cfdata *match,
    void *aux)
{

	return (1);
}

static void
ld_aac_attach(struct device *parent, struct device *self, void *aux)
{
	struct aac_attach_args *aaca;
	struct aac_drive *hdr;
	struct ld_aac_softc *sc;
	struct ld_softc *ld;
	struct aac_softc *aac;

	aaca = aux;
	aac = (struct aac_softc *)parent;
	sc = (struct ld_aac_softc *)self;
	ld = &sc->sc_ld;
	hdr = &aac->sc_hdr[aaca->aaca_unit];

	sc->sc_hwunit = aaca->aaca_unit;
	ld->sc_flags = LDF_ENABLED;
	ld->sc_maxxfer = AAC_MAX_XFER;
	ld->sc_secperunit = hdr->hd_size;
	ld->sc_secsize = AAC_SECTOR_SIZE;
	ld->sc_maxqueuecnt = (AAC_NCCBS - AAC_NCCBS_RESERVE) / aac->sc_nunits;
	ld->sc_start = ld_aac_start;
	ld->sc_dump = ld_aac_dump;

	aprint_normal(": %s\n",
	    aac_describe_code(aac_container_types, hdr->hd_devtype));
	ldattach(ld);
}

static int
ld_aac_dobio(struct ld_aac_softc *sc, void *data, int datasize, int blkno,
	     int dowrite, struct buf *bp)
{
	struct aac_blockread_response *brr;
	struct aac_blockwrite_response *bwr;
	struct aac_ccb *ac;
	struct aac_softc *aac;
	struct aac_blockread *br;
	struct aac_blockwrite *bw;
	struct aac_sg_entry *sge;
	struct aac_sg_table *sgt;
	struct aac_fib *fib;
	bus_dmamap_t xfer;
	u_int32_t status;
	u_int16_t size;
	int s, rv, i;

	aac = (struct aac_softc *)device_parent(&sc->sc_ld.sc_dv);

	/*
	 * Allocate a command control block and map the data transfer.
	 */
	ac = aac_ccb_alloc(aac, (dowrite ? AAC_CCB_DATA_OUT : AAC_CCB_DATA_IN));
	ac->ac_data = data;
	ac->ac_datalen = datasize;

	if ((rv = aac_ccb_map(aac, ac)) != 0) {
		aac_ccb_free(aac, ac);
		return (rv);
	}

	/*
	 * Build the command.
	 */
	fib = ac->ac_fib;

        fib->Header.XferState = htole32(AAC_FIBSTATE_HOSTOWNED |
	    AAC_FIBSTATE_INITIALISED | AAC_FIBSTATE_FROMHOST |
	    AAC_FIBSTATE_REXPECTED | AAC_FIBSTATE_NORM);
	fib->Header.Command = htole16(ContainerCommand);

	if (dowrite) {
		bw = (struct aac_blockwrite *)&fib->data[0];
		bw->Command = htole32(VM_CtBlockWrite);
		bw->ContainerId = htole32(sc->sc_hwunit);
		bw->BlockNumber = htole32(blkno);
		bw->ByteCount = htole32(datasize);
		bw->Stable = htole32(CUNSTABLE); /* XXX what's appropriate here? */

		size = sizeof(struct aac_blockwrite);
		sgt = &bw->SgMap;
	} else {
		br = (struct aac_blockread *)&fib->data[0];
		br->Command = htole32(VM_CtBlockRead);
		br->ContainerId = htole32(sc->sc_hwunit);
		br->BlockNumber = htole32(blkno);
		br->ByteCount = htole32(datasize);

		size = sizeof(struct aac_blockread);
		sgt = &br->SgMap;
	}

	xfer = ac->ac_dmamap_xfer;
	sgt->SgCount = xfer->dm_nsegs;
	sge = sgt->SgEntry;

	for (i = 0; i < xfer->dm_nsegs; i++, sge++) {
		sge->SgAddress = htole32(xfer->dm_segs[i].ds_addr);
		sge->SgByteCount = htole32(xfer->dm_segs[i].ds_len);
		AAC_DPRINTF(AAC_D_IO,
		    ("#%d va %p pa %lx len %lx\n", i, data,
		    (u_long)xfer->dm_segs[i].ds_addr,
		    (u_long)xfer->dm_segs[i].ds_len));
	}

	size += xfer->dm_nsegs * sizeof(struct aac_sg_entry);
	size = htole16(sizeof(fib->Header) + size);
	fib->Header.Size = htole16(size);

	if (bp == NULL) {
		/*
		 * Polled commands must not sit on the software queue.  Wait
		 * up to 30 seconds for the command to complete.
		 */
		s = splbio();
		rv = aac_ccb_poll(aac, ac, 30000);
		aac_ccb_unmap(aac, ac);
		aac_ccb_free(aac, ac);
		splx(s);

		if (rv == 0) {
			if (dowrite) {
				bwr = (struct aac_blockwrite_response *)
				    &ac->ac_fib->data[0];
				status = le32toh(bwr->Status);
			} else {
				brr = (struct aac_blockread_response *)
				    &ac->ac_fib->data[0];
				status = le32toh(brr->Status);
			}

			if (status != ST_OK) {
				printf("%s: I/O error: %s\n",
				    sc->sc_ld.sc_dv.dv_xname,
				    aac_describe_code(aac_command_status_table,
				    status));
				rv = EIO;
			}
		}
	} else {
		ac->ac_device = (struct device *)sc;
		ac->ac_context = bp;
		ac->ac_intr = ld_aac_intr;
		aac_ccb_enqueue(aac, ac);
		rv = 0;
	}

	return (rv);
}

static int
ld_aac_start(struct ld_softc *ld, struct buf *bp)
{

	return (ld_aac_dobio((struct ld_aac_softc *)ld, bp->b_data,
	    bp->b_bcount, bp->b_rawblkno, (bp->b_flags & B_READ) == 0, bp));
}

static void
ld_aac_intr(struct aac_ccb *ac)
{
	struct aac_blockread_response *brr;
	struct aac_blockwrite_response *bwr;
	struct ld_aac_softc *sc;
	struct aac_softc *aac;
	struct buf *bp;
	u_int32_t status;

	bp = ac->ac_context;
	sc = (struct ld_aac_softc *)ac->ac_device;
	aac = (struct aac_softc *)device_parent(&sc->sc_ld.sc_dv);

	if ((bp->b_flags & B_READ) != 0) {
		brr = (struct aac_blockread_response *)&ac->ac_fib->data[0];
		status = le32toh(brr->Status);
	} else {
		bwr = (struct aac_blockwrite_response *)&ac->ac_fib->data[0];
		status = le32toh(bwr->Status);
	}

	aac_ccb_unmap(aac, ac);
	aac_ccb_free(aac, ac);

	if (status != ST_OK) {
		bp->b_flags |= B_ERROR;
		bp->b_error = EIO;
		bp->b_resid = bp->b_bcount;

		printf("%s: I/O error: %s\n", sc->sc_ld.sc_dv.dv_xname,
		    aac_describe_code(aac_command_status_table, status));
	} else
		bp->b_resid = 0;

	lddone(&sc->sc_ld, bp);
}

static int
ld_aac_dump(struct ld_softc *ld, void *data, int blkno, int blkcnt)
{

	return (ld_aac_dobio((struct ld_aac_softc *)ld, data,
	    blkcnt * ld->sc_secsize, blkno, 1, NULL));
}
