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
 *	$Id: atzsc.c,v 1.6.2.2 1994/07/16 19:45:45 chopps Exp $
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
#include <amiga/dev/atzscreg.h>
#include <amiga/dev/ztwobusvar.h>

int atzscprint __P((void *auxp, char *));
void atzscattach __P((struct device *, struct device *, void *));
int atzscmatch __P((struct device *, struct cfdata *, void *));

void atzsc_dmafree __P((struct sbic_softc *));
void atzsc_dmastop __P((struct sbic_softc *));
int atzsc_dmanext __P((struct sbic_softc *));
int atzsc_dmaintr __P((void));
int atzsc_dmago __P((struct sbic_softc *, char *, int, int));

struct scsi_adapter atzsc_scsiswitch = {
	sbic_scsicmd,
	sbic_minphys,
	0,			/* no lun support */
	0,			/* no lun support */
	sbic_adinfo,
	"atzsc",
};

struct scsi_device atzsc_scsidev = {
	NULL,		/* use default error handler */
	NULL,		/* do not have a start functio */
	NULL,		/* have no async handler */
	NULL,		/* Use default done routine */
	"atzsc",
	0,
};


#ifdef DEBUG
void	atzsc_dmatimeout __P((void *));
int	atzsc_dmadebug = 0;
#endif

struct cfdriver atzsccd = {
	NULL, "atzsc", atzscmatch, atzscattach, 
	DV_DULL, sizeof(struct sbic_softc), NULL, 0 };

/*
 * if we are an A3000 we are here.
 */
int
atzscmatch(pdp, cdp, auxp)
	struct device *pdp;
	struct cfdata *cdp;
	void *auxp;
{
	struct ztwobus_args *zap;

	zap = auxp;

	/*
	 * Check manufacturer and product id.
	 * I was informed that older boards can be 2 also.
	 */
	if (zap->manid == 514 && (zap->prodid == 3 || zap->prodid == 2))
		return(1);
	else
		return(0);
}

void
atzscattach(pdp, dp, auxp)
	struct device *pdp, *dp;
	void *auxp;
{
	volatile struct sdmac *rp;
	struct sbic_softc *sc;
	struct ztwobus_args *zap;

	zap = auxp;
	
	sc = (struct sbic_softc *)dp;
	sc->sc_cregs = rp = zap->va;
	/*
	 * disable ints and reset bank register
	 */
	rp->CNTR = CNTR_PDMD;
	rp->DAWR = DAWR_ATZSC;
	sc->sc_dmafree = atzsc_dmafree;
	sc->sc_dmago = atzsc_dmago;
	sc->sc_dmanext = atzsc_dmanext;
	sc->sc_dmastop = atzsc_dmastop;
	sc->sc_dmacmd = 0;

#ifdef DEBUG
	/* make sure timeout is really not needed */
	timeout(atzsc_dmatimeout, 0, 30 * hz);
#endif
	/*
	 * only 24 bit mem.
	 */
	sc->sc_flags |= SBICF_BADDMA;
	sc->sc_dmamask = ~0x00ffffff;
	/* 
	 * If the users kva space is not ztwo try and allocate a bounce buffer. 
	 * XXX this needs to change if we move to multiple memory segments.
	 */
	if (kvtop(sc) & sc->sc_dmamask) {
		sc->sc_dmabuffer = (char *)alloc_z2mem(MAXPHYS);
		if (isztwomem(sc->sc_dmabuffer))
			printf(" bounce pa 0x%x", ztwopa(sc->sc_dmabuffer));
		else if (sc->sc_dmabuffer)
			printf(" bounce pa 0x%x",
			    PREP_DMA_MEM(sc->sc_dmabuffer));
	}
	sc->sc_sbicp = (sbic_regmap_p) ((int)rp + 0x91);
	sc->sc_clkfreq = sbic_clock_override ? sbic_clock_override : 77;
	
	printf(": dmamask 0x%x\n", ~sc->sc_dmamask);

	sbicreset(sc);

	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_targ = 7;
	sc->sc_link.adapter = &atzsc_scsiswitch;
	sc->sc_link.device = &atzsc_scsidev;
	TAILQ_INIT(&sc->sc_xslist);

	custom.intreq = INTF_PORTS;
	custom.intena = INTF_SETCLR | INTF_PORTS;

	/*
	 * attach all scsi units on us
	 */
	config_found(dp, &sc->sc_link, atzscprint);
}

/*
 * print diag if pnp is NULL else just extra
 */
int
atzscprint(auxp, pnp)
	void *auxp;
	char *pnp;
{
	if (pnp == NULL)
		return(UNCONF);
	return(QUIET);
}


void
atzsc_dmafree(dev)
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
		if ((dev->sc_dmacmd & (CNTR_TCEN | CNTR_DDIR)) == 0) {
			/*
			 * only FLUSH if terminal count not enabled, 
			 * and reading from peripheral
			 */
			sdp->FLUSH = 1;
			while ((sdp->ISTR & ISTR_FE_FLG) == 0)
				;
		}
		/* 
		 * clear possible interrupt and stop dma
		 */
		sdp->CINT = 1;
		sdp->SP_DMA = 1;
		dev->sc_dmacmd = 0;
	}
	/*
	 * disable interrupts
	 */
	sdp->CNTR = CNTR_PDMD;	/* disable interrupts from dma/scsi */
	dev->sc_flags &= ~SBICF_INTR;
	splx(s);
}

int
atzsc_dmago(dev, addr, count, flags)
	struct sbic_softc *dev;
	char *addr;
	int count, flags;
{
	volatile struct sdmac *sdp;

	sdp = dev->sc_cregs;
	/*
	 * Set up the command word based on flags
	 */
	dev->sc_dmacmd = CNTR_PDMD | CNTR_INTEN;
	if ((flags & DMAGO_READ) == 0)
		dev->sc_dmacmd |= CNTR_DDIR;
#ifdef DEBUG
	if (atzsc_dmadebug & DDB_IO)
		printf("atzsc_dmago: cmd %x\n", dev->sc_dmacmd);
	dev->sc_dmatimo = 1;
#endif

	dev->sc_flags |= SBICF_INTR;
	sdp->CNTR = dev->sc_dmacmd;
	sdp->ACR = (u_int) dev->sc_cur->dc_addr;
	sdp->ST_DMA = 1;
  
	return(dev->sc_tcnt);
}

void
atzsc_dmastop(dev)
	struct sbic_softc *dev;
{
	volatile struct sdmac *sdp;
	int s;

	sdp = dev->sc_cregs;

#ifdef DEBUG
	if (atzsc_dmadebug & DDB_FOLLOW)
		printf("atzsc_dmastop()\n");
	dev->sc_dmatimo = 0;
#endif
	if (dev->sc_dmacmd) {
		s = splbio();
		if ((dev->sc_dmacmd & (CNTR_TCEN | CNTR_DDIR)) == 0) {
			/*
			 * only FLUSH if terminal count not enabled,
			 * and reading from peripheral
			 */
			sdp->FLUSH = 1;
			while ((sdp->ISTR & ISTR_FE_FLG) == 0)
				;
		}
		/* 
		 * clear possible interrupt and stop dma
		 */
		sdp->CINT = 1;
		sdp->SP_DMA = 1;
		dev->sc_dmacmd = 0;
		splx(s);
	}
}

int
atzsc_dmaintr()
{
	volatile struct sdmac *sdp;
	struct sbic_softc *dev;
	int i, stat, found;

	found = 0;
	for (i = 0; i < atzsccd.cd_ndevs; i++) {
		dev = atzsccd.cd_devs[i];
		if (dev == NULL)
			continue;
		sdp = dev->sc_cregs;
		stat = sdp->ISTR;
      
		if ((stat & (ISTR_INT_F|ISTR_INT_P)) == 0)
			continue;
  
#ifdef DEBUG
		if (atzsc_dmadebug & DDB_FOLLOW)
			printf("atzsc_dmaintr (%d, 0x%x)\n", i, stat);
#endif

		/*
		 * both, SCSI and DMA interrupts arrive here. I chose
		 * arbitrarily that DMA interrupts should have higher
		 * precedence than SCSI interrupts.
		 */
		if (stat & ISTR_E_INT) {
			found++;
	  
			sdp->CINT = 1;	/* clear possible interrupt */
	
			/*
			 * check for SCSI ints in the same go and 
			 * eventually save an interrupt
			 */
		}

		if (dev->sc_flags & SBICF_INTR && stat & ISTR_INTS)
			found += sbicintr(dev);
	}
	return(found);
}


int
atzsc_dmanext(dev)
	struct sbic_softc *dev;
{
	volatile struct sdmac *sdp;
	int i, stat;

	sdp = dev->sc_cregs;

	if (dev->sc_cur > dev->sc_last) {
		/* shouldn't happen !! */
		printf("atzsc_dmanext at end !!!\n");
		atzsc_dmastop(dev);
		return(0);
	}
#ifdef DEBUG
	dev->sc_dmatimo = 1;
#endif
	if ((dev->sc_dmacmd & (CNTR_TCEN | CNTR_DDIR)) == 0) {
		  /* 
		   * only FLUSH if terminal count not enabled,
		   * and reading from peripheral
		   */
		sdp->FLUSH = 1;
		while ((sdp->ISTR & ISTR_FE_FLG) == 0)
			;
        }
	/* 
	 * clear possible interrupt and stop dma
	 */
	sdp->CINT = 1;	/* clear possible interrupt */
	sdp->SP_DMA = 1;	/* stop dma */
	sdp->CNTR = dev->sc_dmacmd;
	sdp->ACR = (u_int)dev->sc_cur->dc_addr;
	sdp->ST_DMA = 1;
      
	dev->sc_tcnt = dev->sc_cur->dc_count << 1;
	return(dev->sc_tcnt);
}

#ifdef DEBUG
/*ARGSUSED*/
void
atzsc_dmatimeout(arg)
	void *arg;
{
	struct sbic_softc *dev;
	int i, s;

	for (i = 0; i < atzsccd.cd_ndevs; i++) {
		dev = atzsccd.cd_devs[i];
		if (dev == NULL)
			continue;

		s = splbio();
		if (dev->sc_dmatimo) {
			if (dev->sc_dmatimo > 1)
				printf("atzsc_dma%d: timeout #%d\n", 
				    dev->sc_dev.dv_unit, dev->sc_dmatimo - 1);
			dev->sc_dmatimo++;
		}
		splx(s);
	}
	timeout(atzsc_dmatimeout, 0, 30 * hz);
}
#endif
