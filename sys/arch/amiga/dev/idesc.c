/*	$NetBSD: idesc.c,v 1.53.2.1 2004/08/03 10:31:52 skrll Exp $ */

/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Don Ahn.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)wd.c	7.4 (Berkeley) 5/25/91
 */
/*
 * Copyright (c) 1994 Michael L. Hitch
 * Copyright (c) 1993, 1994 Charles M. Hannum.
 *
 * This code is derived from software contributed to Berkeley by
 * Don Ahn.
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
 *	@(#)wd.c	7.4 (Berkeley) 5/25/91
 */
/*
 * Copyright (c) 1994 Michael L. Hitch
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
 *      This product includes software developed by Brad Pepers
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
__KERNEL_RCSID(0, "$NetBSD: idesc.c,v 1.53.2.1 2004/08/03 10:31:52 skrll Exp $");

/*
 * A4000 IDE interface, emulating a SCSI controller
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsipi_disk.h>
#include <dev/scsipi/scsi_disk.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/ata/atareg.h>
#include <amiga/amiga/device.h>
#include <amiga/amiga/cia.h>
#include <amiga/amiga/custom.h>
#ifndef __powerpc__
#include <amiga/amiga/isr.h>
#endif
#include <amiga/dev/zbusvar.h>

#include <machine/bswap.h>

#include "atapibus.h"
#include "idesc.h"

/* defines */

struct regs {
	volatile u_short	ide_data;		/* 00 */
	char	____pad0[4];
	volatile u_char		ide_error;		/* 06 */
#define ide_precomp		ide_error
	char	____pad1[3];
	volatile u_char		ide_seccnt;		/* 0a */
#define ide_ireason		ide_seccnt	/* interrupt reason (ATAPI) */
	char	____pad2[3];
	volatile u_char		ide_sector;		/* 0e */
	char	____pad3[3];
	volatile u_char		ide_cyl_lo;		/* 12 */
	char	____pad4[3];
	volatile u_char		ide_cyl_hi;		/* 16 */
	char	____pad5[3];
	volatile u_char		ide_sdh;		/* 1a */
	char	____pad6[3];
	volatile u_char		ide_command;		/* 1e */
#define ide_status		ide_command
	char	____pad7;
	char	____pad8[0xfe0];
	volatile short		ide_intpnd;		/* 1000 */
	char	____pad9[24];
	volatile u_char		ide_altsts;		/* 101a */
#define ide_ctlr		ide_altsts
};
typedef volatile struct regs *ide_regmap_p;

#define	IDES_BUSY	0x80	/* controller busy bit */
#define	IDES_READY	0x40	/* selected drive is ready */
#define	IDES_WRTFLT	0x20	/* Write fault */
#define	IDES_SEEKCMPLT	0x10	/* Seek complete */
#define	IDES_DRQ	0x08	/* Data request bit */
#define	IDES_ECCCOR	0x04	/* ECC correction made in data */
#define	IDES_INDEX	0x02	/* Index pulse from selected drive */
#define	IDES_ERR	0x01	/* Error detect bit */

#define	IDEC_RESTORE	0x10

#define	IDEC_READ	0x20
#define	IDEC_WRITE	0x30

#define	IDEC_XXX	0x40
#define	IDEC_FORMAT	0x50
#define	IDEC_XXXX	0x70
#define	IDEC_DIAGNOSE	0x90
#define	IDEC_IDC	0x91

#define	IDEC_READP	0xec

#define IDECTL_IDS	0x02		/* Interrupt disable */
#define IDECTL_RST	0x04		/* Controller reset */

/* ATAPI commands */
#define ATAPI_NOP	0x00
#define ATAPI_SOFT_RST	0x08
#define ATAPI_PACKET	0xa0
#define ATAPI_IDENTIFY	0xa1

/* ATAPI ireason */
#define IDEI_CMD	0x01		/* command(1) or data(0) */
#define IDEI_IN		0x02		/* transfer to(1) to from(0) host */
#define	IDEI_RELEASE	0x04		/* bus released until finished */

#define	PHASE_CMDOUT	(IDES_DRQ | IDEI_CMD)
#define	PHASE_DATAIN	(IDES_DRQ | IDEI_IN)
#define	PHASE_DATAOUT	(IDES_DRQ)
#define	PHASE_COMPLETED	(IDEI_IN | IDEI_CMD)
#define	PHASE_ABORTED	(0)

struct ideparams {
	/* drive info */
	short	idep_config;		/* general configuration */
	short	idep_fixedcyl;		/* number of non-removable cylinders */
	short	idep_removcyl;		/* number of removable cylinders */
	short	idep_heads;		/* number of heads */
	short	idep_unfbytespertrk;	/* number of unformatted bytes/track */
	short	idep_unfbytes;		/* number of unformatted bytes/sector */
	short	idep_sectors;		/* number of sectors */
	short	idep_minisg;		/* minimum bytes in inter-sector gap*/
	short	idep_minplo;		/* minimum bytes in postamble */
	short	idep_vendstat;		/* number of words of vendor status */
	/* controller info */
	char	idep_cnsn[20];		/* controller serial number */
	short	idep_cntype;		/* controller type */
#define	IDETYPE_SINGLEPORTSECTOR	1	 /* single port, single sector buffer */
#define	IDETYPE_DUALPORTMULTI	2	 /* dual port, multiple sector buffer */
#define	IDETYPE_DUALPORTMULTICACHE 3	 /* above plus track cache */
	short	idep_cnsbsz;		/* sector buffer size, in sectors */
	short	idep_necc;		/* ecc bytes appended */
	char	idep_rev[8];		/* firmware revision */
	char	idep_model[40];		/* model name */
	short	idep_nsecperint;		/* sectors per interrupt */
	short	idep_usedmovsd;		/* can use double word read/write? */
};

/*
 * Per drive structure.
 * N per controller (presently 2) (DRVS_PER_CTLR)
 */
struct ide_softc {
	struct device sc_dev;
	long	sc_bcount;	/* byte count left */
	long	sc_mbcount;	/* total byte count left */
	short	sc_skip;	/* blocks already transferred */
	short	sc_mskip;	/* blocks already transfereed for multi */
	long	sc_blknum;	/* starting block of active request */
	u_char	*sc_buf;	/* buffer address of active request */
	long	sc_blkcnt;	/* block count of active request */
	int	sc_flags;
#define	IDEF_ALIVE	0x01	/* it's a valid device	*/
#define IDEF_ATAPI	0x02	/* it's an ATAPI device */
#define IDEF_ACAPLEN	0x04
#define	IDEF_ACAPDRQ	0x08
#define	IDEF_SENSE	0x10	/* Doing a request sense command */
	short	sc_error;
	char	sc_drive;
	char	sc_state;
	long	sc_secpercyl;
	long	sc_sectors;
	struct buf sc_dq;
	struct ideparams sc_params;
};

/*
 * Per controller structure.
 */
struct idec_softc
{
	struct device sc_dev;
	struct isr sc_isr;

	struct	scsipi_adapter sc_adapter;
	struct	scsipi_channel sc_channel;
	ide_regmap_p	sc_cregs;	/* driver specific regs */
	volatile u_char *sc_a1200;	/* A1200 interrupt control */
	struct	scsipi_xfer *sc_xs;	/* transfer from high level code */
	int	sc_flags;
#define	IDECF_ALIVE	0x01	/* Controller is alive */
#define	IDECF_ACTIVE	0x02
#define	IDECF_SINGLE	0x04	/* sector at a time mode */
#define	IDECF_READ	0x08	/* Current operation is read */
#define	IDECF_A1200	0x10	/* A1200 IDE */
	struct ide_softc *sc_cur; /* drive we are currently doing work for */
	int	state;
	int	saved;
	int	retry;
	char	sc_status;
	char	sc_error;
	char	sc_stat[2];
	struct ide_softc	sc_ide[2];
};

void ide_scsipi_request(struct scsipi_channel *, scsipi_adapter_req_t, void *);

void idescattach(struct device *, struct device *, void *);
int idescmatch(struct device *, struct cfdata *, void *);

int  ideicmd(struct idec_softc *, int, void *, int, void *, int);
int  idego(struct idec_softc *, struct scsipi_xfer *);
int  idegetsense(struct idec_softc *, struct scsipi_xfer *);
void ideabort(struct idec_softc *, ide_regmap_p, char *);
void ideerror(struct idec_softc *, ide_regmap_p, u_char);
int idestart(struct idec_softc *);
int idereset(struct idec_softc *);
void idesetdelay(int);
void ide_scsidone(struct idec_softc *, int);
void ide_donextcmd(struct idec_softc *);
int  idesc_intr(void *);
int  ide_atapi_icmd(struct idec_softc *, int, void *, int, void *, int);

int ide_atapi_start(struct idec_softc *);
int ide_atapi_intr(struct idec_softc *);
void ide_atapi_done(struct idec_softc *);

CFATTACH_DECL(idesc, sizeof(struct idec_softc),
    idescmatch, idescattach, NULL, NULL);

struct {
	short	ide_err;
	char	scsi_sense_key;
	char	scsi_sense_qual;
} sense_convert[] = {
	{ 0x0001, 0x03, 0x13},	/* Data address mark not found */
	{ 0x0002, 0x04, 0x06},	/* Reference position not found */
	{ 0x0004, 0x05, 0x20},	/* Invalid command */
	{ 0x0010, 0x03, 0x12},	/* ID address mark not found */
	{ 0x0020, 0x06, 0x00},	/* Media changed */
	{ 0x0040, 0x03, 0x11},	/* Unrecovered read error */
	{ 0x0080, 0x03, 0x11},	/* Bad block mark detected */
	{ 0x0000, 0x05, 0x00}	/* unknown */
};

/*
 * protos.
 */

int idecommand(struct ide_softc *, int, int, int, int, int);
int idewait(struct idec_softc *, int);
int idegetctlr(struct ide_softc *);
int ideiread(struct ide_softc *, long, u_char *, int);
int ideiwrite(struct ide_softc *, long, u_char *, int);

#define wait_for_drq(ide) idewait(ide, IDES_DRQ)
#define wait_for_ready(ide) idewait(ide, IDES_READY | IDES_SEEKCMPLT)
#define wait_for_unbusy(ide) idewait(ide,0)

int ide_no_int = 0;

#ifdef DEBUG
void ide_dump_regs(ide_regmap_p);

int ide_debug = 0;

#define TRACE0(arg) if (ide_debug > 1) printf(arg)
#define TRACE1(arg1,arg2) if (ide_debug > 1) printf(arg1,arg2)
#define QPRINTF(a) if (ide_debug > 1) printf a

#else	/* !DEBUG */

#define TRACE0(arg)
#define TRACE1(arg1,arg2)
#define QPRINTF(a)

#endif	/* !DEBUG */


/*
 * if we are an A4000 we are here.
 */
int
idescmatch(struct device *pdp, struct cfdata *cfp, void *auxp)
{
	char *mbusstr;

	mbusstr = auxp;
	if ((is_a4000() || is_a1200()) && matchname(auxp, "idesc"))
		return(1);
	return(0);
}

void
idescattach(struct device *pdp, struct device *dp, void *auxp)
{
	ide_regmap_p rp;
	struct idec_softc *sc = (struct idec_softc *)dp;
	struct scsipi_adapter *adapt = &sc->sc_adapter;
	struct scsipi_channel *chan = &sc->sc_channel;

	int i;

	if (is_a4000()) {
		sc->sc_cregs = rp = (ide_regmap_p) ztwomap(0xdd2020);
		printf(": A4000 IDE @ %p", rp);
	} else {
		/* Let's hope the A1200 will work with the same regs */
		sc->sc_cregs = rp = (ide_regmap_p) ztwomap(0xda0000);
		sc->sc_a1200 = ztwomap(0xda8000 + 0x1000);
		sc->sc_flags |= IDECF_A1200;
		printf(": A1200 IDE @ %p:%p", rp, sc->sc_a1200);
	}

#ifdef DEBUG
	if (ide_debug)
		ide_dump_regs(rp);
#endif
	if (idereset(sc) != 0) {
#ifdef DEBUG_ATAPI
		printf("\nIDE reset failed, checking ATAPI ");
#endif
		rp->ide_sdh = 0xb0;	/* slave */
#ifdef DEBUG_ATAPI
		printf(" cyl lo %x hi %x\n", rp->ide_cyl_lo, rp->ide_cyl_hi);
#endif
		delay(500000);
		idereset(sc);
	}
#ifdef DEBUG_ATAPI
	if (rp->ide_cyl_lo == 0x14 && rp->ide_cyl_hi == 0xeb)
		printf(" ATAPI drive present?\n");
#endif
	rp->ide_error = 0x5a;
	rp->ide_cyl_lo = 0xa5;
	if (rp->ide_error == 0x5a || rp->ide_cyl_lo != 0xa5) {
		printf ("\n");
		return;
	}
	/* test if controller will reset */
	if (idereset(sc) != 0) {
		delay (500000);
		if (idereset(sc) != 0) {
			printf (" IDE controller did not reset\n");
			return;
		}
	}
	/* Dummy up the unit structures */
	sc->sc_ide[0].sc_dev.dv_parent = (void *) sc;
	sc->sc_ide[1].sc_dev.dv_parent = (void *) sc;
#if 0	/* Amiga ROM does this; it also takes a lot of time on the Seacrate */
	/* Execute a controller only command. */
	if (idecommand(&sc->sc_ide[0], 0, 0, 0, 0, IDEC_DIAGNOSE) != 0 ||
	    wait_for_unbusy(sc) != 0) {
		printf (" ide attach failed\n");
		return;
	}
#endif
#ifdef DEBUG
	if (ide_debug)
		ide_dump_regs(rp);
#endif

	idereset(sc);

	for (i = 0; i < 2; ++i) {
		rp->ide_sdh = 0xa0 | (i << 4);
		sc->sc_ide[i].sc_drive = i;
		if ((rp->ide_status & IDES_READY) == 0) {
			int len;
			struct ataparams id;
			u_short *p = (u_short *)&id;

			sc->sc_ide[i].sc_flags |= IDEF_ATAPI;
			if (idecommand(&sc->sc_ide[i], 0, 0, 0, 0, ATAPI_SOFT_RST)
			    != 0) {
#ifdef DEBUG_ATAPI
				printf("\nATAPI_SOFT_RESET failed for drive %d",
				    i);
#endif
				continue;
			}
			if (wait_for_unbusy(sc) != 0) {
#ifdef DEBUG_ATAPI
				printf("\nATAPI wait for unbusy failed");
#endif
				continue;
			}
			if (idecommand(&sc->sc_ide[i], DEV_BSIZE, 0, 0, 0,
			    ATAPI_IDENTIFY) != 0 ||
			    wait_for_drq(sc) != 0) {
#ifdef DEBUG_ATAPI
				printf("\nATAPI_IDENTIFY failed for drive %d",
				    i);
#endif
				continue;
			}
			len = DEV_BSIZE;
#ifdef DEBUG_ATAPI
			printf("\nATAPI_IDENTIFY returned %d/%d bytes",
			    rp->ide_cyl_lo + rp->ide_cyl_hi * 256, DEV_BSIZE);
#endif
			while (len) {
				if (p < (u_short *)(&id + 1))
					*p++ = rp->ide_data;
				else
					rp->ide_data;
				len -= 2;
			}

			for (i = 0; i < sizeof(id.atap_model); i += 2) {
				p = (u_short *)(id.atap_model + i);
				*p = ntohs(*p);
			}
			for (i = 0; i < sizeof(id.atap_serial); i += 2) {
				p = (u_short *)(id.atap_serial + i);
				*p = ntohs(*p);
			}
			for (i = 0; i < sizeof(id.atap_revision); i += 2) {
				p = (u_short *)(id.atap_revision + i);
				*p = ntohs(*p);
			}
			
			strncpy(sc->sc_ide[i].sc_params.idep_model, id.atap_model,
			    sizeof(sc->sc_ide[i].sc_params.idep_model));
			strncpy(sc->sc_ide[i].sc_params.idep_rev, id.atap_revision,
			    sizeof(sc->sc_ide[i].sc_params.idep_rev));
			for (len = sizeof(id.atap_model) - 1;
			    id.atap_model[len] == ' ' && len != 0; --len)
				;
			if (len < sizeof(id.atap_model) - 1)
				id.atap_model[len] = 0;
			for (len = sizeof(id.atap_serial) - 1;
			    id.atap_serial[len] == ' ' && len != 0; --len)
				;
			if (len < sizeof(id.atap_serial) - 1)
				id.atap_serial[len] = 0;
			for (len = sizeof(id.atap_revision) - 1;
			    id.atap_revision[len] == ' ' && len != 0; --len)
				;
			if (len < sizeof(id.atap_revision) - 1)
				id.atap_revision[len] = 0;
			id.atap_config = bswap16(id.atap_config);
#ifdef DEBUG_ATAPI
			printf("\nATAPI device: type %x", ATAPI_CFG_TYPE(id.atap_config));
			printf(" cyls %04x heads %04x",
			    id.atap_cylinders, id.atap_heads);
			printf(" bpt %04x bps %04x",
			    id.__retired1[0],
			    id.__retired2[0]);
			printf(" drq_rem %02x", id.atap_config& 0xff);
			printf("\n model %s rev %s ser %s", id.atap_model,
			    id.atap_revision, id.atap_serial);
			printf("\n cap %04x%04x sect %04x%04x",
			    id.atap_curcapacity[0], id.atap_curcapacity[1],
			    id.atap_capacity[0], id.atap_capacity[1]);
#endif
			if (id.atap_config & ATAPI_CFG_CMD_16)
				sc->sc_ide[i].sc_flags |= IDEF_ACAPLEN;
			if ((id.atap_config & ATAPI_CFG_DRQ_MASK) == ATAPI_CFG_IRQ_DRQ)
				sc->sc_ide[i].sc_flags |= IDEF_ACAPDRQ;
		}
		sc->sc_ide[i].sc_flags |= IDEF_ALIVE;
		rp->ide_ctlr = 0;
	}

	printf ("\n");

	/*
	 * Fill in the scsipi_adapter.
	 */
	memset(adapt, 0, sizeof(*adapt));
	adapt->adapt_dev = &sc->sc_dev;
	adapt->adapt_nchannels = 1;
	adapt->adapt_openings = 1;
	adapt->adapt_max_periph = 1;
	adapt->adapt_request = ide_scsipi_request;
	adapt->adapt_minphys = minphys;

	/*
	 * Fill in the scsipi_channel.
	 */
	memset(chan, 0, sizeof(*chan));
	chan->chan_adapter = adapt;
	chan->chan_bustype = &scsi_bustype;
	chan->chan_channel = 0;
	chan->chan_flags = 0;
	chan->chan_ntargets = 2;
	chan->chan_nluns = 1;
	chan->chan_id = 7;

	sc->sc_isr.isr_intr = idesc_intr;
	sc->sc_isr.isr_arg = sc;
	sc->sc_isr.isr_ipl = 2;
	add_isr (&sc->sc_isr);

	/*
	 * attach all "scsi" units on us
	 */
	config_found(dp, chan, scsiprint);
}

/*
 * used by specific ide controller
 *
 */
void
ide_scsipi_request(struct scsipi_channel *chan, scsipi_adapter_req_t req,
                   void *arg)
{
	struct scsipi_xfer *xs;
	struct scsipi_periph *periph;
	struct idec_softc *dev = (void *)chan->chan_adapter->adapt_dev;
	int flags, s;

	switch (req) {
	case ADAPTER_REQ_RUN_XFER:
		xs = arg;
		periph = xs->xs_periph;
		flags = xs->xs_control;

		if (flags & XS_CTL_DATA_UIO)
			panic("ide: scsi data uio requested");

		if (dev->sc_xs && (flags & XS_CTL_POLL))
			panic("ide_scsipi_request: busy");

		s = splbio();
#ifdef DIAGNOSTIC
		/*
		 * This should never happen as we track the resources
		 * in the mid-layer.
		 */
		if (dev->sc_xs) {
			splx(s);
			panic("ide_scsipi_request: busy");
		}
#endif

		dev->sc_xs = xs;
		splx(s);

		/*
		 * nothing is pending do it now.
		 */
		ide_donextcmd(dev);
		return;

	case ADAPTER_REQ_GROW_RESOURCES:
		return;

	case ADAPTER_REQ_SET_XFER_MODE:
		return;
	}

}

/*
 * entered with dev->sc_xs pointing to the next xfer to perform
 */
void
ide_donextcmd(struct idec_softc *dev)
{
	struct scsipi_xfer *xs = dev->sc_xs;
	struct scsipi_periph *periph = xs->xs_periph;
	int flags, stat;

	flags = xs->xs_control;

	if (flags & XS_CTL_RESET)
		idereset(dev);

	dev->sc_stat[0] = -1;
	/* Weed out invalid targets & LUNs here */
	if (periph->periph_target > 1 || periph->periph_lun != 0) {
		ide_scsidone(dev, -1);
		return;
	}
	if (flags & XS_CTL_POLL || ide_no_int)
		stat = ideicmd(dev, periph->periph_target, xs->cmd, xs->cmdlen,
		    xs->data, xs->datalen);
	else if (idego(dev, xs) == 0)
		return;
	else
		stat = dev->sc_stat[0];

	if (dev->sc_xs)
		ide_scsidone(dev, stat);
}

void
ide_scsidone(struct idec_softc *dev, int stat)
{
	struct scsipi_xfer *xs;

	xs = dev->sc_xs;
#ifdef DIAGNOSTIC
	if (xs == NULL)
		panic("ide_scsidone");
#endif
	/*
	 * is this right?
	 */
	xs->status = stat;

	if (stat == 0)
		xs->resid = 0;
	else {
		switch(stat) {
		case SCSI_CHECK:
			if ((stat = idegetsense(dev, xs)) != 0)
				goto bad_sense;
			xs->error = XS_SENSE;
			break;
		case SCSI_BUSY:
			xs->error = XS_BUSY;
			break;
		bad_sense:
		default:
			xs->error = XS_DRIVER_STUFFUP;
			QPRINTF(("ide_scsicmd() bad %x\n", stat));
			break;
		}
	}

	dev->sc_xs = NULL; /* Not busy anymore */

	scsipi_done(xs);
}

int
idegetsense(struct idec_softc *dev, struct scsipi_xfer *xs)
{
	struct scsipi_sense rqs;
	struct scsipi_periph *periph = xs->xs_periph;

	if (dev->sc_cur->sc_flags & IDEF_ATAPI)
		return (0);
	rqs.opcode = REQUEST_SENSE;
	rqs.byte2 = periph->periph_lun << 5;
#ifdef not_yet
	rqs.length = xs->req_sense_length ? xs->req_sense_length :
	    sizeof(xs->sense.scsi_sense);
#else
	rqs.length = sizeof(xs->sense.scsi_sense);
#endif

	rqs.unused[0] = rqs.unused[1] = rqs.control = 0;

	return(ideicmd(dev, periph->periph_target, &rqs, sizeof(rqs),
		&xs->sense.scsi_sense, rqs.length));
}

#ifdef DEBUG
void
ide_dump_regs(ide_regmap_p regs)
{
	printf ("ide regs: %04x %02x %02x %02x %02x %02x %02x %02x\n",
	    regs->ide_data, regs->ide_error, regs->ide_seccnt,
	    regs->ide_sector, regs->ide_cyl_lo, regs->ide_cyl_hi,
	    regs->ide_sdh, regs->ide_command);
}
#endif

int
idereset(struct idec_softc *sc)
{
	ide_regmap_p regs=sc->sc_cregs;

	regs->ide_ctlr = IDECTL_RST | IDECTL_IDS;
	delay(1000);
	regs->ide_ctlr = IDECTL_IDS;
	delay(1000);
	(void) regs->ide_error;
	if (wait_for_unbusy(sc) < 0) {
		printf("%s: reset failed\n", sc->sc_dev.dv_xname);
		return (1);
	}
	return (0);
}

int
idewait(struct idec_softc *sc, int mask)
{
	ide_regmap_p regs = sc->sc_cregs;
	int timeout = 0;
	int status = sc->sc_status = regs->ide_status;

	if ((status & IDES_BUSY) == 0 && (status & mask) == mask)
		return (0);
#ifdef DEBUG
	if (ide_debug)
		printf ("idewait busy: %02x\n", status);
#endif
	for (;;) {
		status = sc->sc_status = regs->ide_status;
		if ((status & IDES_BUSY) == 0 && (status & mask) == mask)
			break;
#if 0
		if (status & IDES_ERR)
			break;
#endif
		if (++timeout > 10000) {
#ifdef DEBUG_ATAPI
			printf ("idewait timeout status %02x error %02x\n",
			    status, regs->ide_error);
#endif
			return (-1);
		}
		delay (1000);
	}
	if (status & IDES_ERR) {
		sc->sc_error = regs->ide_error;
#ifdef DEBUG
		if (ide_debug)
			printf ("idewait: status %02x error %02x\n", status,
			    sc->sc_error);
#endif
	}
#ifdef DEBUG
	else if (ide_debug)
		printf ("idewait delay %d %02x\n", timeout, status);
#endif
	return (status & IDES_ERR);
}

int
idecommand(struct ide_softc *ide, int cylin, int head, int sector, int count,
           int cmd)
{
	struct idec_softc *idec = (void *)ide->sc_dev.dv_parent;
	ide_regmap_p regs = idec->sc_cregs;
	int stat;

#ifdef DEBUG
	if (ide_debug)
		printf ("idecommand: cmd = %02x\n", cmd);
#endif
	if (wait_for_unbusy(idec) < 0)
		return (-1);
	regs->ide_sdh = 0xa0 | (ide->sc_drive << 4) | head;
	if (cmd == IDEC_DIAGNOSE || cmd == IDEC_IDC || ide->sc_flags & IDEF_ATAPI)
		stat = wait_for_unbusy(idec);
	else
		stat = idewait(idec, IDES_READY);
	if (stat < 0) printf ("idecommand:%d stat %d\n", ide->sc_drive, stat);
	if (stat < 0)
		return (-1);
	regs->ide_precomp = 0;
	regs->ide_cyl_lo = cylin;
	regs->ide_cyl_hi = cylin >> 8;
	regs->ide_sector = sector;
	regs->ide_seccnt = count;
	regs->ide_command = cmd;
	return (0);
}

int
idegetctlr(struct ide_softc *dev)
{
	struct idec_softc *idec = (void *)dev->sc_dev.dv_parent;
	ide_regmap_p regs = idec->sc_cregs;
	char tb[DEV_BSIZE];
	short *tbp = (short *) tb;
	int i;

	if (idecommand(dev, 0, 0, 0, 0, IDEC_READP) != 0 ||
	    wait_for_drq(idec) != 0) {
		return (-1);
	} else {
		for (i = 0; i < DEV_BSIZE / 2; ++i)
			*tbp++ = ntohs(regs->ide_data);
		for (i = 0; i < DEV_BSIZE; i += 2) {
			char temp;
			temp = tb[i];
			tb[i] = tb[i + 1];
			tb[i + 1] = temp;
		}
		bcopy (tb, &dev->sc_params, sizeof (struct ideparams));
		dev->sc_sectors = dev->sc_params.idep_sectors;
		dev->sc_secpercyl = dev->sc_sectors *
		    dev->sc_params.idep_heads;
	}
	return (0);
}

int
ideiread(struct ide_softc *ide, long block, u_char *buf, int nblks)
{
	int cylin, head, sector;
	int stat;
	u_short *bufp = (u_short *) buf;
	int i;
	struct idec_softc *idec = (void *) ide->sc_dev.dv_parent;
	ide_regmap_p regs = idec->sc_cregs;

	cylin = block / ide->sc_secpercyl;
	head = (block % ide->sc_secpercyl) / ide->sc_sectors;
	sector = block % ide->sc_sectors + 1;
	stat = idecommand(ide, cylin, head, sector, nblks, IDEC_READ);
	if (stat != 0)
		return (-1);
	while (nblks--) {
		if (wait_for_drq(idec) != 0)
			return (-1);
		for (i = 0; i < DEV_BSIZE / 2 / 16; ++i) {
			*bufp++ = regs->ide_data;
			*bufp++ = regs->ide_data;
			*bufp++ = regs->ide_data;
			*bufp++ = regs->ide_data;
			*bufp++ = regs->ide_data;
			*bufp++ = regs->ide_data;
			*bufp++ = regs->ide_data;
			*bufp++ = regs->ide_data;
			*bufp++ = regs->ide_data;
			*bufp++ = regs->ide_data;
			*bufp++ = regs->ide_data;
			*bufp++ = regs->ide_data;
			*bufp++ = regs->ide_data;
			*bufp++ = regs->ide_data;
			*bufp++ = regs->ide_data;
			*bufp++ = regs->ide_data;
		}
	}
	idec->sc_stat[0] = 0;
	return (0);
}

int
ideiwrite(struct ide_softc *ide, long block, u_char *buf, int nblks)
{
	int cylin, head, sector;
	int stat;
	u_short *bufp = (u_short *) buf;
	int i;
	struct idec_softc *idec = (void *) ide->sc_dev.dv_parent;
	ide_regmap_p regs = idec->sc_cregs;

	cylin = block / ide->sc_secpercyl;
	head = (block % ide->sc_secpercyl) / ide->sc_sectors;
	sector = block % ide->sc_sectors + 1;
	stat = idecommand(ide, cylin, head, sector, nblks, IDEC_WRITE);
	if (stat != 0)
		return (-1);
	while (nblks--) {
		if (wait_for_drq(idec) != 0)
			return (-1);
		for (i = 0; i < DEV_BSIZE / 2 / 16; ++i) {
			regs->ide_data = *bufp++;
			regs->ide_data = *bufp++;
			regs->ide_data = *bufp++;
			regs->ide_data = *bufp++;
			regs->ide_data = *bufp++;
			regs->ide_data = *bufp++;
			regs->ide_data = *bufp++;
			regs->ide_data = *bufp++;
			regs->ide_data = *bufp++;
			regs->ide_data = *bufp++;
			regs->ide_data = *bufp++;
			regs->ide_data = *bufp++;
			regs->ide_data = *bufp++;
			regs->ide_data = *bufp++;
			regs->ide_data = *bufp++;
			regs->ide_data = *bufp++;
		}
		if (wait_for_unbusy(idec) != 0)
			printf ("ideiwrite: timeout waiting for unbusy\n");
	}
	idec->sc_stat[0] = 0;
	return (0);
}

int
ideicmd(struct idec_softc *dev, int target, void *cbuf, int clen, void *buf,
        int len)
{
	struct ide_softc *ide;
	int i;
	int lba;
	int nblks;
	struct scsipi_inquiry_data *inqbuf;
	struct {
		struct scsipi_mode_header header;
		struct scsi_blk_desc blk_desc;
		union scsi_disk_pages pages;
	} *mdsnbuf;

#ifdef DEBUG
	if (ide_debug > 1)
		printf ("ideicmd: target %d cmd %02x\n", target,
		    *((u_char *)cbuf));
#endif
	if (target > 1)
		return (-1);		/* invalid unit */

	ide = &dev->sc_ide[target];
	if ((ide->sc_flags & IDEF_ALIVE) == 0)
		return (-1);

	if(ide->sc_flags & IDEF_ATAPI) {
#ifdef DEBUG
		if (ide_debug)
			printf("ideicmd: atapi cmd %02x\n", *((u_char *)cbuf));
#endif
		return (ide_atapi_icmd(dev, target, cbuf, clen, buf, len));
	}

	if (*((u_char *)cbuf) != REQUEST_SENSE)
		ide->sc_error = 0;
	switch (*((u_char *)cbuf)) {
	case TEST_UNIT_READY:
		dev->sc_stat[0] = 0;
		return (0);

	case INQUIRY:
		dev->sc_stat[0] = idegetctlr(ide);
		if (dev->sc_stat[0] != 0)
			return (dev->sc_stat[0]);
		inqbuf = (void *) buf;
		bzero (buf, len);
		inqbuf->device = 0;
		inqbuf->dev_qual2 = 0;	/* XXX check RMB? */
		inqbuf->version = 2;
		inqbuf->response_format = 2;
		inqbuf->additional_length = 31;
		for (i = 0; i < 8; ++i)
			inqbuf->vendor[i] = ide->sc_params.idep_model[i];
		for (i = 0; i < 16; ++i)
			inqbuf->product[i] = ide->sc_params.idep_model[i+8];
		for (i = 0; i < 4; ++i)
			inqbuf->revision[i] = ide->sc_params.idep_rev[i];
		return (0);

	case READ_CAPACITY:
		*((long *)buf) = ide->sc_params.idep_sectors *
		    ide->sc_params.idep_heads *
		    ide->sc_params.idep_fixedcyl - 1;
		*((long *)buf + 1) = ide->sc_flags & IDEF_ATAPI ?
		    512	:			/* XXX 512 byte blocks */
		    2048;			/* XXX */
		dev->sc_stat[0] = 0;
		return (0);

	case READ_BIG:
		lba = *((long *)((char *)cbuf + 2));
		nblks = *((u_short *)((char *)cbuf + 7));
		return (ideiread(ide, lba, buf, nblks));

	case SCSI_READ_COMMAND:
		lba = *((long *)cbuf) & 0x001fffff;
		nblks = *((u_char *)((char *)cbuf + 4));
		if (nblks == 0)
			nblks = 256;
		return (ideiread(ide, lba, buf, nblks));

	case WRITE_BIG:
		lba = *((long *)((char *)cbuf + 2));
		nblks = *((u_short *)((char *)cbuf + 7));
		return (ideiwrite(ide, lba, buf, nblks));

	case SCSI_WRITE_COMMAND:
		lba = *((long *)cbuf) & 0x001fffff;
		nblks = *((u_char *)((char *)cbuf + 4));
		if (nblks == 0)
			nblks = 256;
		return (ideiwrite(ide, lba, buf, nblks));

	case PREVENT_ALLOW:
	case START_STOP:	/* and LOAD */
		dev->sc_stat[0] = 0;
		return (0);

	case MODE_SENSE:
		mdsnbuf = (void*) buf;
		bzero(buf, *((u_char *)cbuf + 4));
		switch (*((u_char *)cbuf + 2) & 0x3f) {
		case 4:
			mdsnbuf->header.data_length = 27;
			mdsnbuf->header.blk_desc_len = 8;
			mdsnbuf->blk_desc.blklen[1] = 512 >> 8;
			mdsnbuf->pages.rigid_geometry.pg_code = 4;
			mdsnbuf->pages.rigid_geometry.pg_length = 16;
			_lto3b(ide->sc_params.idep_fixedcyl,
			    mdsnbuf->pages.rigid_geometry.ncyl);
			mdsnbuf->pages.rigid_geometry.nheads =
			    ide->sc_params.idep_heads;
			dev->sc_stat[0] = 0;
			return (0);
		default:
			printf ("ide: mode sense page %x not simulated\n",
			   *((u_char *)cbuf + 2) & 0x3f);
			return (-1);
		}

	case REQUEST_SENSE:
		/* convert sc_error to SCSI sense */
		bzero (buf, *((u_char *)cbuf + 4));
		*((u_char *) buf) = 0x70;
		*((u_char *) buf + 7) = 10;
		i = 0;
		while (sense_convert[i].ide_err) {
			if (sense_convert[i].ide_err & ide->sc_error)
				break;
			++i;
		}
		*((u_char *) buf + 2) = sense_convert[i].scsi_sense_key;
		*((u_char *) buf + 12) = sense_convert[i].scsi_sense_qual;
		dev->sc_stat[0] = 0;
		return (0);

	case 0x01 /*REWIND*/:
	case 0x04 /*CMD_FORMAT_UNIT*/:
	case 0x05 /*READ_BLOCK_LIMITS*/:
	case SCSI_REASSIGN_BLOCKS:
	case 0x10 /*WRITE_FILEMARKS*/:
	case 0x11 /*SPACE*/:
	case MODE_SELECT:
	default:
		printf ("ide: unhandled SCSI command %02x\n", *((u_char *)cbuf));
		ide->sc_error = 0x04;
		dev->sc_stat[0] = SCSI_CHECK;
		return (SCSI_CHECK);
	}
}

int
idego(struct idec_softc *dev, struct scsipi_xfer *xs)
{
	struct ide_softc *ide = &dev->sc_ide[xs->xs_periph->periph_target];
	long lba;
	int nblks;

#if 0
	cdb->cdb[1] |= unit << 5;
#endif

	ide->sc_buf = xs->data;
	ide->sc_bcount = xs->datalen;
#ifdef DEBUG
	if (ide_debug > 1)
		printf ("ide_go: %02x\n", xs->cmd->opcode);
#endif
	if ((ide->sc_flags & IDEF_ALIVE) == 0)
		return (dev->sc_stat[0] = -1);
	if(ide->sc_flags & IDEF_ATAPI) {
#ifdef DEBUG
		if (ide_debug)
			printf("idego: atapi cmd %02x\n", xs->cmd->opcode);
#endif
		dev->sc_cur = ide;
		ide->sc_flags &= ~IDEF_SENSE;
		return (idestart(dev));
	}
	if (xs->cmd->opcode != SCSI_READ_COMMAND && xs->cmd->opcode != READ_BIG &&
	    xs->cmd->opcode != SCSI_WRITE_COMMAND && xs->cmd->opcode != WRITE_BIG) {
		ideicmd (dev, xs->xs_periph->periph_target, xs->cmd, xs->cmdlen,
		    xs->data, xs->datalen);
		return (1);
	}
	switch (xs->cmd->opcode) {
	case SCSI_READ_COMMAND:
	case SCSI_WRITE_COMMAND:
		lba = *((long *)xs->cmd) & 0x001fffff;
		nblks = xs->cmd->bytes[3];
		if (nblks == 0)
			nblks = 256;
		break;
	case READ_BIG:
	case WRITE_BIG:
		lba = *((long *)&xs->cmd->bytes[1]);
		nblks = *((short *)&xs->cmd->bytes[6]);
		break;
	default:
		panic ("idego bad SCSI command");
	}
	ide->sc_blknum = lba;
	ide->sc_blkcnt = nblks;
	ide->sc_skip = ide->sc_mskip = 0;
	dev->sc_flags &= ~IDECF_READ;
	if (xs->cmd->opcode == SCSI_READ_COMMAND || xs->cmd->opcode == READ_BIG)
		dev->sc_flags |= IDECF_READ;
	dev->sc_cur = ide;
	return (idestart (dev));
}

int
idestart(struct idec_softc *dev)
{
	long blknum, cylin, head, sector;
	int command, count;
	struct ide_softc *ide = dev->sc_cur;
	short *bf;
	int i;
	ide_regmap_p regs = dev->sc_cregs;

	dev->sc_flags |= IDECF_ACTIVE;
	if (ide->sc_flags & IDEF_ATAPI)
		return(ide_atapi_start(dev));
	blknum = ide->sc_blknum + ide->sc_skip;
	if (ide->sc_mskip == 0) {
		ide->sc_mbcount = ide->sc_bcount;
	}
	cylin = blknum / ide->sc_secpercyl;
	head = (blknum % ide->sc_secpercyl) / ide->sc_sectors;
	sector = blknum % ide->sc_sectors;
	++sector;
	if (ide->sc_mskip == 0 || dev->sc_flags & IDECF_SINGLE) {
		count = howmany(ide->sc_mbcount, DEV_BSIZE);
		command = (dev->sc_flags & IDECF_READ) ?
		    IDEC_READ : IDEC_WRITE;
		if (idecommand(ide, cylin, head, sector, count, command) != 0) {
			printf ("idestart: timeout waiting for unbusy\n");
#if 0
			bp->b_error = EINVAL;
			bp->b_flags |= B_ERROR;
			idfinish(&dev->sc_ide[0], bp);
#endif
			ide_scsidone(dev, dev->sc_stat[0]);
			return (1);
		}
	}
	dev->sc_stat[0] = 0;
	if (dev->sc_flags & IDECF_READ)
		return (0);
	if (wait_for_drq(dev) < 0) {
		printf ("idestart: timeout waiting for drq\n");
	}
#define W1	(regs->ide_data = *bf++)
	for (i = 0, bf = (short *) (ide->sc_buf + ide->sc_skip * DEV_BSIZE);
	    i < DEV_BSIZE / 2 / 16; ++i) {
		W1; W1; W1; W1; W1; W1; W1; W1;
		W1; W1; W1; W1; W1; W1; W1; W1;
	}
	return (0);
}


int
idesc_intr(void *arg)
{
	struct idec_softc *dev = arg;
	ide_regmap_p regs;
	struct ide_softc *ide;
	short dummy;
	short *bf;
	int i;

	regs = dev->sc_cregs;
	if (dev->sc_flags & IDECF_A1200) {
		if (*dev->sc_a1200 & 0x80) {
#if 0
			printf ("idesc_intr: A1200 interrupt %x\n", *dev->sc_a1200);
#endif
			dummy = regs->ide_status;	/* XXX */
			*dev->sc_a1200 = 0x7c | (*dev->sc_a1200 & 0x03);
		}
		else
			return (0);
	} else {
		if (regs->ide_intpnd >= 0)
			return (0);
		dummy = regs->ide_status;
	}
#ifdef DEBUG
	if (ide_debug)
		printf ("idesc_intr: %02x\n", dummy);
#endif
	if ((dev->sc_flags & IDECF_ACTIVE) == 0)
		return (1);
	if (dev->sc_cur->sc_flags & IDEF_ATAPI)
		return (ide_atapi_intr(dev));
	dev->sc_flags &= ~IDECF_ACTIVE;
	if (wait_for_unbusy(dev) < 0)
		printf ("idesc_intr: timeout waiting for unbusy\n");
	ide = dev->sc_cur;
	if (dummy & IDES_ERR) {
		dev->sc_stat[0] = SCSI_CHECK;
		ide->sc_error = regs->ide_error;
#ifdef DEBUG
		printf("idesc_intr: error %02x, %02x\n", ide->sc_error, dummy);
#endif
		ide_scsidone(dev, dev->sc_stat[0]);
	}
	if (dev->sc_flags & IDECF_READ) {
#define R2 (*bf++ = regs->ide_data)
		bf = (short *) (ide->sc_buf + ide->sc_skip * DEV_BSIZE);
		if (wait_for_drq(dev) != 0)
			printf ("idesc_intr: read error detected late\n");
		for (i = 0; i < DEV_BSIZE / 2 / 16; ++i) {
			R2; R2; R2; R2; R2; R2; R2; R2;
			R2; R2; R2; R2; R2; R2; R2; R2;
		}
	}
	ide->sc_skip++;
	ide->sc_mskip++;
	ide->sc_bcount -= DEV_BSIZE;
	ide->sc_mbcount -= DEV_BSIZE;
#ifdef DEBUG
	if (ide_debug)
		printf ("idesc_intr: sc_bcount %ld\n", ide->sc_bcount);
#endif
	if (ide->sc_bcount == 0)
		ide_scsidone(dev, dev->sc_stat[0]);
	else
		/* Check return value here? */
		idestart (dev);
	return (1);
}

int ide_atapi_start(struct idec_softc *dev)
{
	ide_regmap_p regs = dev->sc_cregs;
	struct scsipi_xfer *xs;
	int clen;

	if (wait_for_unbusy(dev) != 0) {
		printf("ide_atapi_start: not ready, st = %02x\n",
		    regs->ide_status);
		dev->sc_stat[0] = -1;
		ide_scsidone(dev, dev->sc_stat[0]);
		return (-1);
	}
	xs = dev->sc_xs;
	clen = dev->sc_cur->sc_flags & IDEF_ACAPLEN ? 16 : 12;

	if (idecommand(dev->sc_cur,
	    (xs->datalen < 0xffff) ? xs->datalen : 0xfffe, 0, 0, 0,
	    ATAPI_PACKET) != 0) {
		printf("ide_atapi_start: send packet failed\n");
		dev->sc_stat[0] = -1;
		ide_scsidone(dev, dev->sc_stat[0]);
		return(-1);
	}

	if (!(dev->sc_cur->sc_flags & IDEF_ACAPDRQ)) {
		int i;
		u_short *bf;
		union {
			struct scsipi_rw_big rw_big;
			struct scsipi_mode_sense_big md_big;
		} cmd;

		/* Wait for cmd i/o phase */
		for (i = 20000; i > 0; --i) {
			int phase;
			phase = (regs->ide_ireason & (IDEI_CMD | IDEI_IN)) |
			    (regs->ide_status & IDES_DRQ);
			if (phase == PHASE_CMDOUT)
				break;
			delay(10);
		}
#ifdef DEBUG
		if (ide_debug)
			printf("atapi_start: wait for cmd i/o phase i = %d\n", i);
#endif

		bf = (u_short *)xs->cmd;
		switch (xs->cmd->opcode) {
		case SCSI_READ_COMMAND:
		case SCSI_WRITE_COMMAND:
			bzero((char *)&cmd, sizeof(cmd.rw_big));
			cmd.rw_big.opcode = xs->cmd->opcode | 0x20;
			cmd.rw_big.addr[3] = xs->cmd->bytes[2];
			cmd.rw_big.addr[2] = xs->cmd->bytes[1];
			cmd.rw_big.addr[1] = xs->cmd->bytes[0] & 0x0f;
			cmd.rw_big.length[1] = xs->cmd->bytes[3];
			if (xs->cmd->bytes[3] == 0)
				cmd.rw_big.length[0] = 1;
			bf = (u_short *)&cmd.rw_big;
			break;
		case MODE_SENSE:
		case MODE_SELECT:
			bzero((char *)&cmd, sizeof(cmd.md_big));
			cmd.md_big.opcode = xs->cmd->opcode |= 0x40;
			cmd.md_big.byte2 = xs->cmd->bytes[0];
			cmd.md_big.page = xs->cmd->bytes[1];
			cmd.md_big.length[1] = xs->cmd->bytes[3];
			bf = (u_short *)&cmd.md_big;
			break;
		}
		for (i = 0; i < clen; i += 2)
			regs->ide_data = *bf++;
	}

	return (0);
}

int
ide_atapi_icmd(struct idec_softc *dev, int target, void *cbuf, int clen,
               void *buf, int len)
{
	struct ide_softc *ide = &dev->sc_ide[target];
	struct scsipi_xfer *xs = dev->sc_xs;
	ide_regmap_p regs = dev->sc_cregs;
	int i;
	u_short *bf;

	clen = dev->sc_flags & IDEF_ACAPLEN ? 16 : 12;
	ide->sc_buf = buf;
	ide->sc_bcount = len;

	if (wait_for_unbusy(dev) != 0) {
		printf("ide_atapi_icmd: not ready, st = %02x\n",
		    regs->ide_status);
		dev->sc_stat[0] = -1;
		return (-1);
	}

	if (idecommand(ide, (len < 0xffff) ? len : 0xfffe, 0, 0, 0,
	    ATAPI_PACKET) != 0) {
		printf("ide_atapi_icmd: send packet failed\n");
		dev->sc_stat[0] = -1;
		return(-1);
	}
	/* Wait for cmd i/o phase */
	for (i = 20000; i > 0; --i) {
		int phase;
		phase = (regs->ide_ireason & (IDEI_CMD | IDEI_IN)) |
		    (regs->ide_status & IDES_DRQ);
		if (phase == PHASE_CMDOUT)
			break;
		delay(10);
	}
#ifdef DEBUG
	if (ide_debug)
		printf("atapi_icmd: wait for cmd i/o phase i = %d\n", i);
#endif

	for (i = 0, bf = (u_short *)cbuf; i < clen; i += 2)
		regs->ide_data = *bf++;

	/* Wait for data i/o phase */
	for (i = 20000; i > 0; --i) {
		int phase;
		phase = (regs->ide_ireason & (IDEI_CMD | IDEI_IN)) |
		    (regs->ide_status & IDES_DRQ);
		if (phase != PHASE_CMDOUT)
			break;
		delay(10);
	}
#ifdef DEBUG
	if (ide_debug)
		printf("atapi_icmd: wait for data i/o phase i = %d\n", i);
#endif

	dev->sc_cur = ide;
	while ((xs->xs_status & XS_STS_DONE) == 0) {
		ide_atapi_intr(dev);
		for (i = 2000; i > 0; --i)
			if ((regs->ide_status & IDES_DRQ) == 0)
				break;
#ifdef DEBUG
		if (ide_debug)
			printf("atapi_icmd: intr i = %d\n", i);
#endif
	}
	return (1);
}

int
ide_atapi_intr(struct idec_softc *dev)
{
	struct ide_softc *ide = dev->sc_cur;
	struct scsipi_xfer *xs = dev->sc_xs;
	ide_regmap_p regs = dev->sc_cregs;
	u_short *bf;
	int phase;
	int len;
	int status;
	int err;
	int ire;
	int retries = 0;
	union {
		struct scsipi_rw_big rw_big;
		struct scsipi_mode_sense_big md_big;
	} cmd;

	if (wait_for_unbusy(dev) < 0) {
		if ((regs->ide_status & IDES_ERR) == 0) {
			printf("atapi_intr: controller busy\n");
			return (0);
		} else {
			xs->error = XS_SHORTSENSE;
			xs->sense.atapi_sense = regs->ide_error;
			ide_atapi_done(dev);
			return (0);
		}
	}

again:
	len = regs->ide_cyl_lo + 256 * regs->ide_cyl_hi;
	status = regs->ide_status;
	err = regs->ide_error;
	ire = regs->ide_ireason;
	phase = (ire & (IDEI_CMD | IDEI_IN)) | (status & IDES_DRQ);
#ifdef DEBUG
	if (ide_debug)
		printf("ide_atapi_intr: len %d st %x err %x ire %x :",
		    len, status, err, ire);
#endif

	switch(phase) {
	case PHASE_CMDOUT:
#ifdef DEBUG
		if (ide_debug)
			printf("PHASE_CMDOUT\n");
#endif
		len = ide->sc_flags & IDEF_ACAPLEN ? 16 : 12;

		bf = (u_short *)xs->cmd;
		switch (xs->cmd->opcode) {
		case SCSI_READ_COMMAND:
		case SCSI_WRITE_COMMAND:
			bzero((char *)&cmd, sizeof(cmd.rw_big));
			cmd.rw_big.opcode = xs->cmd->opcode | 0x20;
			cmd.rw_big.addr[3] = xs->cmd->bytes[2];
			cmd.rw_big.addr[2] = xs->cmd->bytes[1];
			cmd.rw_big.addr[1] = xs->cmd->bytes[0] & 0x0f;
			cmd.rw_big.length[1] = xs->cmd->bytes[3];
			if (xs->cmd->bytes[3] == 0)
				cmd.rw_big.length[0] = 1;
			bf = (u_short *)&cmd.rw_big;
			break;
		case MODE_SENSE:
		case MODE_SELECT:
			bzero((char *)&cmd, sizeof(cmd.md_big));
			cmd.md_big.opcode = xs->cmd->opcode |= 0x40;
			cmd.md_big.byte2 = xs->cmd->bytes[0];
			cmd.md_big.page = xs->cmd->bytes[1];
			cmd.md_big.length[1] = xs->cmd->bytes[3];
			bf = (u_short *)&cmd.md_big;
			break;
		}
#ifdef DEBUG
		if (ide_debug > 1) {
			int i;
			for (i = 0; i < len; ++i)
				printf("%s%02x ", i == 0 ? "cmd: " : " ",
				    *((u_char *)bf + i));
			printf("\n");
		}
#endif
		while (len > 0) {
			regs->ide_data = *bf++;
			len -= 2;
		}
		return (1);

	case PHASE_DATAOUT:
#ifdef DEBUG
		if (ide_debug)
			printf("PHASE_DATAOUT\n");
#endif
		if (ide->sc_bcount < len) {
			printf("ide_atapi_intr: write only %ld of %d bytes\n",
			    ide->sc_bcount, len);
			len = ide->sc_bcount;	/* XXXXXXXXXXXXX */
		}
		bf = (u_short *)ide->sc_buf;
		ide->sc_buf += len;
		ide->sc_bcount -= len;
		while (len > 0) {
			regs->ide_data = *bf++;
			len -= 2;
		}
		return (1);

	case PHASE_DATAIN:
#ifdef DEBUG
		if (ide_debug)
			printf("PHASE_DATAIN\n");
#endif
		if (ide->sc_bcount < len) {
			printf("ide_atapi_intr: read only %ld of %d bytes\n",
			    ide->sc_bcount, len);
			len = ide->sc_bcount;	/* XXXXXXXXXXXXX */
		}
		bf = (u_short *)ide->sc_buf;
		ide->sc_buf += len;
		ide->sc_bcount -= len;
		while (len > DEV_BSIZE) {
			R2; R2; R2; R2; R2; R2; R2; R2;
			R2; R2; R2; R2; R2; R2; R2; R2;
			len -= 16 * 2;
		}
		while (len > 0) {
			*bf++ = regs->ide_data;
			len -= 2;
		}
		return (1);

	case PHASE_ABORTED:
	case PHASE_COMPLETED:
#ifdef DEBUG
		if (ide_debug)
			printf("PHASE_COMPLETED\n");
#endif
		if (ide->sc_flags & IDEF_SENSE) {
			ide->sc_flags &= ~IDEF_SENSE;
			if ((status & IDES_ERR) == 0)
				xs->error = XS_SENSE;
		} else if (status & IDES_ERR) {
			struct scsipi_sense rqs;

#ifdef DEBUG_ATAPI
			printf("ide_atapi_intr: error status %x err %x\n",
			    status, err);
#endif
			xs->error = XS_SHORTSENSE;
			xs->sense.atapi_sense = err;
			ide->sc_flags |= IDEF_SENSE;
			rqs.opcode = REQUEST_SENSE;
			rqs.byte2 = xs->xs_periph->periph_lun << 5;
			rqs.length = sizeof(xs->sense.scsi_sense);
			rqs.unused[0] = rqs.unused[1] = rqs.control = 0;
			ide_atapi_icmd(dev, xs->xs_periph->periph_target,
			    &rqs, sizeof(rqs), &xs->sense.scsi_sense,
			    sizeof(xs->sense.scsi_sense));
			return(1);
		}
#ifdef DEBUG_ATAPI
		if (ide->sc_bcount != 0)
			printf("ide_atapi_intr: %ld bytes remaining\n", ide->sc_bcount);
#endif
		break;
	default:
		if (++retries < 500) {
			delay(100);
			goto again;
		}
		printf("ide_atapi_intr: unknown phase %x\n", phase);
		if (status & IDES_ERR) {
			xs->error = XS_SHORTSENSE;
			xs->sense.atapi_sense = err;
		} else
			xs->error = XS_DRIVER_STUFFUP;
	}
	dev->sc_flags &= ~IDECF_ACTIVE;
	ide_atapi_done(dev);
	return (1);
}

void
ide_atapi_done(struct idec_softc *dev)
{
	struct scsipi_xfer *xs = dev->sc_xs;

	if (xs->error == XS_SHORTSENSE) {
		int atapi_sense = xs->sense.atapi_sense;

		bzero((char *)&xs->sense.scsi_sense, sizeof(xs->sense.scsi_sense));
		xs->sense.scsi_sense.error_code = 0x70;
		xs->sense.scsi_sense.flags = atapi_sense >> 4;
		if (atapi_sense & 0x01)
			xs->sense.scsi_sense.flags |= SSD_ILI;
		if (atapi_sense & 0x02)
			xs->sense.scsi_sense.flags |= SSD_EOM;
#if 0
		if (atapi_sense & 0x04)
			;		/* command aborted */
#endif
		ide_scsidone(dev, SCSI_CHECK);
		return;
	}
	if (xs->error == XS_SENSE) {
		ide_scsidone(dev, SCSI_CHECK);
		return;
	}
	ide_scsidone(dev, 0);		/* ??? */
}
