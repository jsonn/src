/*
 * Copyright (c) 1994 Christian E. Hopps
 * Copyright (c) 1982, 1990 The Regents of the University of California.
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
 *	@(#)dma.c
 *	$Id: gtsc.c,v 1.6.2.2 1994/10/06 19:06:42 chopps Exp $
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <amiga/amiga/custom.h>
#include <amiga/amiga/cc.h>
#include <amiga/amiga/device.h>
#include <amiga/dev/dmavar.h>
#include <amiga/dev/sbicreg.h>
#include <amiga/dev/sbicvar.h>
#include <amiga/dev/gtscreg.h>
#include <amiga/dev/ztwobusvar.h>
#include <amiga/dev/gvpbusvar.h>

void gtscattach __P((struct device *, struct device *, void *));
int gtscmatch __P((struct device *, struct cfdata *, void *));
int gtscprint __P((void *auxp, char *));

void gtsc_dmafree __P((struct sbic_softc *));
void gtsc_dmastop __P((struct sbic_softc *));
int gtsc_dmanext __P((struct sbic_softc *));
int gtsc_dmaintr __P((void));
int gtsc_dmago __P((struct sbic_softc *, char *, int, int));

struct scsi_adapter gtsc_scsiswitch = {
	sbic_scsicmd,
	sbic_minphys,
	0,			/* no lun support */
	0,			/* no lun support */
	sbic_adinfo,
	"gtsc",
};

struct scsi_device gtsc_scsidev = {
	NULL,		/* use default error handler */
	NULL,		/* have a queue served by this ??? */
	NULL,		/* have no async handler ??? */
	NULL,		/* Use default done routine */
	"gtsc",
	0,
};

int gtsc_maxdma = 0;	/* Maximum size per DMA transfer */
int gtsc_dmamask = 0;
int gtsc_dmabounce = 0;

#ifdef DEBUG
void gtsc_dmatimeout __P((void *));
int gtsc_debug = 0;
#endif

struct cfdriver gtsccd = {
	NULL, "gtsc", gtscmatch, gtscattach, 
	DV_DULL, sizeof(struct sbic_softc), NULL, 0 };

int
gtscmatch(pdp, cdp, auxp)
	struct device *pdp;
	struct cfdata *cdp;
	void *auxp;
{
	struct gvpbus_args *gap;

	gap = auxp;
	if (gap->flags & GVP_SCSI)
		return(1);
	return(0);
}

/*
 * attach all devices on our board. 
 */
void
gtscattach(pdp, dp, auxp)
	struct device *pdp, *dp;
	void *auxp;
{
	volatile struct sdmac *rp;
	struct gvpbus_args *gap;
	struct sbic_softc *sc;

	gap = auxp;
	sc = (struct sbic_softc *)dp;
	sc->sc_cregs = rp = gap->zargs.va;	

	/*
	 * disable ints and reset bank register
	 */
	rp->CNTR = 0;
	if ((gap->flags & GVP_NOBANK) == 0)
		rp->bank = 0;
	
	sc->sc_dmago =  gtsc_dmago;
	sc->sc_dmafree = gtsc_dmafree;
	sc->sc_dmanext = gtsc_dmanext;
	sc->sc_dmastop = gtsc_dmastop;
	sc->sc_dmacmd = 0;

#ifdef DEBUG
	/* make sure timeout is really not needed */
	timeout((void *)gtsc_dmatimeout, 0, 30 * hz);
#endif

	sc->sc_flags |= SBICF_BADDMA;
	if (gtsc_dmamask)
		sc->sc_dmamask = gtsc_dmamask;
	else if (gap->flags & GVP_24BITDMA)
		sc->sc_dmamask = ~0x00ffffff;
	else if (gap->flags & GVP_25BITDMA)
		sc->sc_dmamask = ~0x01ffffff;
	else
		sc->sc_dmamask = ~0x07ffffff;
	printf(": dmamask 0x%x", ~sc->sc_dmamask);
	
	if ((gap->flags & GVP_NOBANK) == 0)
		sc->gtsc_bankmask = (~sc->sc_dmamask >> 18) & 0x01c0;


	/*
	 * if the user requests a bounce buffer or 
	 * the users kva space is not ztwo and dma needs it
	 * try and allocate a bounce buffer.  If we allocate
	 * one and it is in ztwo space leave maxdma to user
	 * setting or default to MAXPHYS else the address must
	 * be on the chip bus so decrease it to either the users
	 * setting or 1024 bytes.
	 *
	 * XXX this needs to change if we move to multiple memory segments.
	 */
	if (gtsc_dmabounce || kvtop(sc) & sc->sc_dmamask) {
		sc->sc_dmabuffer = (char *) alloc_z2mem(MAXPHYS);
		if (isztwomem(sc->sc_dmabuffer))
			printf(" bounce pa 0x%x", kvtop(sc->sc_dmabuffer));
		else if (gtsc_maxdma == 0) {
			gtsc_maxdma = 1024;
			printf(" bounce pa 0x%x", 
			    PREP_DMA_MEM(sc->sc_dmabuffer));
		}
	}
	if (gtsc_maxdma == 0)
		gtsc_maxdma = MAXPHYS;

	printf(" maxdma %d\n", gtsc_maxdma);

	sc->sc_sbicp = (sbic_regmap_p) ((int)rp + 0x61);
	sc->sc_clkfreq = sbic_clock_override ? sbic_clock_override : 77;

	sbicreset(sc);

	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_targ = 7;
	sc->sc_link.adapter = &gtsc_scsiswitch;
	sc->sc_link.device = &gtsc_scsidev;
	TAILQ_INIT(&sc->sc_xslist);

	custom.intreq = INTF_PORTS;
	custom.intena = INTF_SETCLR | INTF_PORTS;

	/*
	 * attach all scsi units on us
	 */
	config_found(dp, &sc->sc_link, gtscprint);
}

/*
 * print diag if pnp is NULL else just extra
 */
int
gtscprint(auxp, pnp)
	void *auxp;
	char *pnp;
{
	if (pnp == NULL)
		return(UNCONF);
	return(QUIET);
}

void
gtsc_dmafree(dev)
	struct sbic_softc *dev;
{
	volatile struct sdmac *sdp;
	int s;

	sdp = dev->sc_cregs;

	s = splbio();
#ifdef DEBUG
	dev->sc_dmatimo = 0;
#endif
	if (dev->sc_dmacmd) {
		/* 
		 * clear possible interrupt and stop dma
		 */
		sdp->CNTR &= ~GVP_CNTR_INT_P;
		sdp->SP_DMA = 1;
		dev->sc_dmacmd = 0;
	}
#ifdef DEBUG
	if (gtsc_debug & (DDB_IO | DDB_FOLLOW))
		printf("gtsc_dmafree\n");
#endif
	/*
	 * disable interrupts
	 */
	sdp->CNTR = 0;	/* disable interrupts from dma/sbic */
	dev->sc_flags &= ~SBICF_INTR;
	splx(s);
}

int
gtsc_dmago(dev, addr, count, flags)
	struct sbic_softc *dev;
	char *addr;
	int count, flags;
{
	volatile struct sdmac *sdp;

	sdp = dev->sc_cregs;
	/*
	 * Set up the command word based on flags
	 */
	dev->sc_dmacmd = GVP_CNTR_INTEN;
	if ((flags & DMAGO_READ) == 0)
		dev->sc_dmacmd |= GVP_CNTR_DDIR;

#ifdef DEBUG
	if (gtsc_debug & DDB_IO)
		printf("gtsc_dmago: cmd %x\n", dev->sc_dmacmd);
	dev->sc_dmatimo = 1;
#endif
	dev->sc_flags |= SBICF_INTR;
	sdp->CNTR = dev->sc_dmacmd;
	sdp->ACR = (u_int) dev->sc_cur->dc_addr;
	if (dev->gtsc_bankmask)
		sdp->bank = 
		    dev->gtsc_bankmask & (((u_int)dev->sc_cur->dc_addr) >> 18);
	sdp->ST_DMA = 1;

	/*
	 * restrict transfer count to maximum
	 */
	if (dev->sc_tcnt > gtsc_maxdma)
		dev->sc_tcnt = gtsc_maxdma;
	return(dev->sc_tcnt);
}

void
gtsc_dmastop(dev)
	struct sbic_softc *dev;
{
	volatile struct sdmac *sdp;
	int s;

	sdp = dev->sc_cregs;

#ifdef DEBUG
	if (gtsc_debug & DDB_FOLLOW)
		printf("gtsc_dmastop()\n");
	dev->sc_dmatimo = 0;
#endif
	if (dev->sc_dmacmd) {
		/* 
		 * clear possible interrupt and stop dma
		 */
		s = splbio();
		sdp->CNTR &= ~GVP_CNTR_INT_P;
		sdp->SP_DMA = 1;
		dev->sc_dmacmd = 0;
		splx(s);
	}
}

int
gtsc_dmaintr()
{
	volatile struct sdmac *sdp;
	struct sbic_softc *dev;
	int i, stat, found;

	found = 0;
	for (i = 0; i < gtsccd.cd_ndevs; i++) {
		dev = gtsccd.cd_devs[i];
		if (dev == NULL)
			continue;
		sdp = dev->sc_cregs;
		stat = sdp->CNTR;
		if ((stat & GVP_CNTR_INT_P) == 0)
			continue;
#ifdef DEBUG
		if (gtsc_debug & DDB_FOLLOW)
			printf("gtsc_dmaintr(%d, 0x%x) ", i, stat);
#endif
		if (dev->sc_flags & SBICF_INTR)
			found += sbicintr(dev);
	}
	return(found);
}


int
gtsc_dmanext(dev)
	struct sbic_softc *dev;
{
	volatile struct sdmac *sdp;
	int i, stat;

	sdp = dev->sc_cregs;

	if (dev->sc_cur > dev->sc_last) {
		/* shouldn't happen !! */
		printf("gtsc_dmanext at end !!!\n");
		gtsc_dmastop(dev);
		return(0);
	}
#ifdef DEBUG
	dev->sc_dmatimo = 1;
#endif
	/* 
	 * clear possible interrupt and stop dma
	 */
	sdp->CNTR &= ~GVP_CNTR_INT_P;
	sdp->SP_DMA = 1;

	sdp->CNTR = dev->sc_dmacmd;
	sdp->ACR = (u_int) dev->sc_cur->dc_addr;
	if (dev->gtsc_bankmask)
		sdp->bank = 
		    dev->gtsc_bankmask & ((u_int)dev->sc_cur->dc_addr >> 18);
	sdp->ST_DMA = 1;

	dev->sc_tcnt = dev->sc_cur->dc_count << 1;
	if (dev->sc_tcnt > gtsc_maxdma)
		dev->sc_tcnt = gtsc_maxdma;
#ifdef DEBUG
	if (gtsc_debug & DDB_FOLLOW)
		printf("gtsc_dmanext ret: %d\n", dev->sc_tcnt);
#endif
	return(dev->sc_tcnt);
}

#ifdef DEBUG
void
gtsc_dmatimeout(arg)
	void *arg;
{
	struct sbic_softc *dev;
	int i, s;

	for (i = 0; i < gtsccd.cd_ndevs; i++) {
		dev = gtsccd.cd_devs[i];
		if (dev == NULL)
			continue;
		s = splbio();
		if (dev->sc_dmatimo) {
			if (dev->sc_dmatimo > 1)
				printf("gtsc_dma%d: timeout #%d\n",
				    dev->sc_dev.dv_unit, dev->sc_dmatimo - 1);
			dev->sc_dmatimo++;
		}
		splx(s);
	}
	timeout(gtsc_dmatimeout, 0, 30 * hz);
}
#endif
