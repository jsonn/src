/*	$NetBSD: dptvar.h,v 1.4.2.1 1999/10/19 17:47:36 thorpej Exp $	*/

/*
 * Copyright (c) 1999 Andy Doran <ad@NetBSD.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _IC_DPTVAR_H_
#define _IC_DPTVAR_H_ 1
#ifdef _KERNEL

#define	CCB_OFF(sc,m)	((u_long)(m) - (u_long)((sc)->sc_ccbs))

#define CCB_ALLOC	0x01	/* CCB allocated */
#define CCB_ABORT	0x02	/* abort has been issued on this CCB */
#define CCB_INTR	0x04	/* HBA interrupted for this CCB */
#define CCB_PRIVATE	0x08	/* ours; don't talk to scsipi when done */ 
#define CCB_SYNC	0x10	/* write data synchronously */

struct dpt_ccb {
	struct eata_cp	ccb_eata_cp;		/* EATA command packet */
	struct eata_sg	ccb_sg[DPT_SG_SIZE];	/* SG element list */
	volatile int	ccb_flg;		/* CCB flags */
	int		ccb_timeout;		/* timeout in ms */
	u_int32_t	ccb_ccbpa;		/* physical addr of this CCB */
	bus_dmamap_t	ccb_dmamap_xfer;	/* dmamap for data xfers */
	int		ccb_hba_status;		/* from status packet */
	int		ccb_scsi_status;	/* from status packet */
	int		ccb_id;			/* unique ID of this CCB */
	TAILQ_ENTRY(dpt_ccb) ccb_chain;		/* link to next CCB */
	struct scsipi_sense_data ccb_sense;	/* SCSI sense data on error */
	struct scsipi_xfer *ccb_xs;		/* initiating SCSI command */
};

struct dpt_softc {
	struct device sc_dv;		/* generic device data */
	bus_space_handle_t sc_ioh;	/* bus space handle */
	struct scsipi_adapter sc_adapter;/* scsipi adapter */
	struct scsipi_channel sc_channels[3]; /* each channel */
	struct eata_cfg sc_ec;		/* EATA configuration data */
	bus_space_tag_t	sc_iot;		/* bus space tag */
	bus_dma_tag_t	sc_dmat;	/* bus DMA tag */
	bus_dmamap_t	sc_dmamap_ccb;	/* maps the CCBs */
	void	 	*sc_ih;		/* interrupt handler cookie */
	void		*sc_sdh;	/* shutdown hook */
	struct dpt_ccb	*sc_ccbs;	/* all our CCBs */
	struct eata_sp	*sc_statpack;	/* EATA status packet */
	int		sc_spoff;	/* status packet offset in dmamap */
	u_int32_t	sc_sppa;	/* status packet physical address */
	caddr_t		sc_scr;		/* scratch area */
	int		sc_scrlen;	/* scratch area length */
	int		sc_scroff;	/* scratch area offset in dmamap */
	u_int32_t	sc_scrpa;	/* scratch area physical address */
	int		sc_hbaid[3];	/* ID of HBA on each channel */
	int		sc_nccbs;	/* number of CCBs available */
	TAILQ_HEAD(, dpt_ccb) sc_free_ccb;/* free ccb list */
};

int	dpt_intr __P((void *));
int	dpt_readcfg __P((struct dpt_softc *));
void	dpt_init __P((struct dpt_softc *, const char *));

#endif	/* _KERNEL */
#endif	/* !defined _IC_DPTVAR_H_ */
