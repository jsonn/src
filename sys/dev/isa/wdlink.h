/*	$NetBSD: wdlink.h,v 1.2.2.2 1997/08/27 23:32:14 thorpej Exp $	*/

/*
 * Copyright (c) 1994, 1995 Charles M. Hannum.  All rights reserved.
 *
 * DMA and multi-sector PIO handling are derived from code contributed by
 * Onno van der Linden.
 *
 * Atapi support added by Manuel Bouyer.
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
 *  This product includes software developed by Charles M. Hannum.
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
 */


#undef WDDEBUG
/* #undef DIAGNOSTIC */

struct wdc_softc {
	struct device sc_dev;
	void *sc_ih;
	struct wd_link *d_link[2];
	struct scsipi_link *ab_link;
	int sc_iobase;			/* I/O port base */
	int sc_drq;			/* DMA channel */

	TAILQ_HEAD(xferhead, wdc_xfer) sc_xfer;
	int sc_flags;
#define	WDCF_ACTIVE		0x01	/* controller is active */
#define	WDCF_SINGLE		0x02	/* sector at a time mode */
#define	WDCF_ERROR		0x04	/* processing a disk error */
#define	WDCF_WANTED		0x08	/* XXX locking for wd_get_parms() */
#define	WDCF_IRQ_WAIT		0x10	/* controller is waiting for irq */
#define	WDCF_ONESLAVE		0x20	/* ctrl. has one ATAPI slave attached */
	int sc_errors;			/* errors during current transfer */
	u_char sc_status;		/* copy of status register */
	u_char sc_error;		/* copy of error register */
};

struct wd_link {
	u_int8_t type;
#define ATA 0
#define ATAPI 1
	u_int8_t channel;
	u_int8_t drive;
	caddr_t wdc_softc;
	caddr_t wd_softc;
	struct wdparams sc_params;

	/* Long-term state: */
	u_int8_t openings;
	int sc_state;			/* control state */
#define RECAL		0		/* recalibrate */
#define RECAL_WAIT	1		/* done recalibrating */
#define GEOMETRY	2		/* upload geometry */
#define GEOMETRY_WAIT	3		/* done uploading geometry */
#define MULTIMODE	4		/* set multiple mode */
#define MULTIMODE_WAIT	5		/* done setting multiple mode */
#define READY		6		/* done with open */
	int sc_mode;			/* transfer mode */
#define WDM_PIOSINGLE	0		/* single-sector PIO */
#define WDM_PIOMULTI	1		/* multi-sector PIO */
#define WDM_DMA		2		/* DMA */
	int sc_multiple;		/* multiple for WDM_PIOMULTI */
	int sc_flags;			/* drive characteistics found */
#define WDF_LOCKED	0x01
#define WDF_WANTED	0x02
#define WDF_WLABEL	0x04		/* label is writable */
#define WDF_LABELLING	0x08		/* writing label */

/*
 * XXX Nothing resets this yet, but disk change sensing will when ATAPI is
 * implemented.
 */
#define WDF_LOADED	0x10		/* parameters loaded */
#define WDF_32BIT	0x20		/* can do 32-bit transfer */
#define WDF_WAIT	0x40		/* waiting for resourses */

	daddr_t sc_badsect[127];	/* 126 plus trailing -1 marker */
	struct disklabel *sc_lp;	/* label info for this disk */
};

struct wdc_xfer {
	struct wd_link *d_link;		/* drive/bus structure info */
	volatile long c_flags;		/* handle: scsi flags (0x0000ffff)
								           b_read/b_write (0x00f00000)
										   controllers flags (0x000f0000) */
#define C_INUSE		0x010000
#define C_ATAPI		0x020000
#define C_ERROR		0x040000
#define C_NEEDDONE	0x080000

	/* Information about the current transfer  */
	struct buf *c_bp;
	void *atapi_cmd;
	void *databuf;
	daddr_t c_blkno;	/* starting block number */
	int c_bcount;		/* byte count left */
	int c_skip;		/* bytes already transferred */
	int c_nblks;		/* number of blocks currently transferring */
	int c_nbytes;		/* number of bytes currently transferring */
	u_long c_p_offset;	/* offset of the partition */
	TAILQ_ENTRY(wdc_xfer) c_xferchain;
	LIST_ENTRY(wdc_xfer) free_list;
};

void	wdc_exec_xfer	__P((struct wdc_softc *, struct wd_link *,
					struct wdc_xfer *));
struct	wdc_xfer *wdc_get_xfer	__P((int));
int		wdc_get_parms	__P((struct wdc_softc *, struct wd_link *));
void	wdstart			__P((void*));
void	wderror			__P((struct wd_link* , struct buf *, char *));
void	wddone			__P((struct wd_link*, struct buf*));
int		wdccommand		__P((struct wdc_softc *, struct wd_link *, int, int,
							int, int, int, int));
int		wdccommandshort __P((struct wdc_softc *, int, int));
int		wdcwait			__P((struct wdc_softc *, int));

/*
 * ST506 spec says that if READY or SEEKCMPLT go off, then the read or write
 * command is aborted.
 */
#define	wait_for_drq(d)		wdcwait(d, WDCS_DRDY | WDCS_DSC | WDCS_DRQ)
#define	wait_for_unbusy(d)	wdcwait(d, 0)
#define	wait_for_ready(d)	wdcwait(d, WDCS_DRDY | WDCS_DSC)
#define	atapi_ready(d)		wdcwait(d, WDCS_DRQ)

#define IDE_NOSLEEP 0x01
