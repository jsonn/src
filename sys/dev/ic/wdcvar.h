/*	$NetBSD: wdcvar.h,v 1.30.2.1 2002/01/10 19:55:10 thorpej Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum, by Onno van der Linden and by Manuel Bouyer.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
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

/* XXX For scsipi_adapter and scsipi_channel. */
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/atapiconf.h>

#include <sys/callout.h>

#define	WAITTIME    (10 * hz)    /* time to wait for a completion */
	/* this is a lot for hard drives, but not for cdroms */

struct channel_queue {  /* per channel queue (may be shared) */
	TAILQ_HEAD(xferhead, wdc_xfer) sc_xfer;
};

struct channel_softc { /* Per channel data */
	/* Our timeout callout */
	struct callout ch_callout;
	/* Our location */
	int channel;
	/* Our controller's softc */
	struct wdc_softc *wdc;
	/* Our registers */
	bus_space_tag_t       cmd_iot;
	bus_space_handle_t    cmd_ioh;
	bus_space_tag_t       ctl_iot;
	bus_space_handle_t    ctl_ioh;
	/* data32{iot,ioh} are only used for 32 bit xfers */
	bus_space_tag_t         data32iot;
	bus_space_handle_t      data32ioh;
	/* Our state */
	int ch_flags;
#define WDCF_ACTIVE   0x01	/* channel is active */
#define WDCF_IRQ_WAIT 0x10	/* controller is waiting for irq */
#define WDCF_DMA_WAIT 0x20	/* controller is waiting for DMA */
	u_int8_t ch_status;         /* copy of status register */
	u_int8_t ch_error;          /* copy of error register */
	/* per-drive infos */
	struct ata_drive_datas ch_drive[2];

	struct device *atapibus;
	struct scsipi_channel ch_atapi_channel;

	/*
	 * channel queues. May be the same for all channels, if hw channels
	 * are not independants
	 */
	struct channel_queue *ch_queue;
};

struct wdc_softc { /* Per controller state */
	struct device sc_dev;
	/* mandatory fields */
	int           cap;
/* Capabilities supported by the controller */
#define	WDC_CAPABILITY_DATA16 0x0001    /* can do  16-bit data access */
#define	WDC_CAPABILITY_DATA32 0x0002    /* can do 32-bit data access */
#define WDC_CAPABILITY_MODE   0x0004	/* controller knows its PIO/DMA modes */
#define	WDC_CAPABILITY_DMA    0x0008	/* DMA */
#define	WDC_CAPABILITY_UDMA   0x0010	/* Ultra-DMA/33 */
#define	WDC_CAPABILITY_HWLOCK 0x0020	/* Needs to lock HW */
#define	WDC_CAPABILITY_ATA_NOSTREAM 0x0040 /* Don't use stream funcs on ATA */
#define	WDC_CAPABILITY_ATAPI_NOSTREAM 0x0080 /* Don't use stream f on ATAPI */
#define WDC_CAPABILITY_NO_EXTRA_RESETS 0x0100 /* only reset once */
#define WDC_CAPABILITY_PREATA 0x0200 /* ctrl can be a pre-ata one */
#define WDC_CAPABILITY_IRQACK 0x0400 /* callback to ack interrupt */
#define WDC_CAPABILITY_SINGLE_DRIVE 0x0800 /* Don't probe second drive */
#define WDC_CAPABILITY_NOIRQ  0x1000	/* Controller never interrupts */
#define WDC_CAPABILITY_SELECT  0x2000	/* Controller selects target */
	u_int8_t      PIO_cap; /* highest PIO mode supported */
	u_int8_t      DMA_cap; /* highest DMA mode supported */
	u_int8_t      UDMA_cap; /* highest UDMA mode supported */
	int nchannels;	/* Number of channels on this controller */
	struct channel_softc **channels;  /* channels-specific datas (array) */

	/*
	 * The reference count here is used for both IDE and ATAPI devices.
	 */
	struct atapi_adapter sc_atapi_adapter;

	/* if WDC_CAPABILITY_DMA set in 'cap' */
	void            *dma_arg;
	int            (*dma_init) __P((void *, int, int, void *, size_t,
	                int));
	void           (*dma_start) __P((void *, int, int));
	int            (*dma_finish) __P((void *, int, int, int));
/* flags passed to dma_init */
#define WDC_DMA_READ 0x01
#define WDC_DMA_IRQW 0x02
	int		dma_status; /* status returned from dma_finish() */
#define WDC_DMAST_NOIRQ	0x01	/* missing IRQ */
#define WDC_DMAST_ERR	0x02	/* DMA error */
#define WDC_DMAST_UNDER	0x04	/* DMA underrun */

	/* if WDC_CAPABILITY_HWLOCK set in 'cap' */
	int            (*claim_hw) __P((void *, int));
	void            (*free_hw) __P((void *));

	/* if WDC_CAPABILITY_MODE set in 'cap' */
	void 		(*set_modes) __P((struct channel_softc *));

	/* if WDC_CAPABILITY_SELECT set in 'cap' */
	void		(*select) __P((struct channel_softc *,int));

	/* if WDC_CAPABILITY_IRQACK set in 'cap' */
	void		(*irqack) __P((struct channel_softc *));
};

 /*
  * Description of a command to be handled by a controller.
  * These commands are queued in a list.
  */
struct wdc_xfer {
	volatile u_int c_flags;    
#define C_ATAPI  	0x0001 /* xfer is ATAPI request */
#define C_TIMEOU  	0x0002 /* xfer processing timed out */
#define C_POLL		0x0004 /* cmd is polled */
#define C_DMA		0x0008 /* cmd uses DMA */
#define C_SENSE		0x0010 /* cmd is a internal command */
#define	C_FORCEPIO	0x0020 /* cmd must use PIO */

	/* Informations about our location */
	struct channel_softc *chp;
	u_int8_t drive;

	/* Information about the current transfer  */
	void *cmd; /* wdc, ata or scsipi command structure */
	void *databuf;
	int c_bcount;      /* byte count left */
	int c_skip;        /* bytes already transferred */
	int c_dscpoll;	   /* counter for dsc polling (ATAPI) */
	TAILQ_ENTRY(wdc_xfer) c_xferchain;
	void (*c_start) __P((struct channel_softc *, struct wdc_xfer *));
	int  (*c_intr)  __P((struct channel_softc *, struct wdc_xfer *, int));
	void (*c_kill_xfer) __P((struct channel_softc *, struct wdc_xfer *));
};

/*
 * Public functions which can be called by ATA or ATAPI specific parts,
 * or bus-specific backends.
 */

int   wdcprobe __P((struct channel_softc *));
void  wdcattach __P((struct channel_softc *));
int   wdcdetach __P((struct device *, int));
int   wdcactivate __P((struct device *, enum devact));
int   wdcintr __P((void *));
void  wdc_exec_xfer __P((struct channel_softc *, struct wdc_xfer *));
struct wdc_xfer *wdc_get_xfer __P((int)); /* int = WDC_NOSLEEP/CANSLEEP */
#define WDC_CANSLEEP 0x00
#define WDC_NOSLEEP 0x01
void   wdc_free_xfer  __P((struct channel_softc *, struct wdc_xfer *));
void  wdcstart __P((struct channel_softc *));
void  wdcrestart __P((void*));
int   wdcreset	__P((struct channel_softc *, int));
#define VERBOSE 1 
#define SILENT 0 /* wdcreset will not print errors */
int   wdcwait __P((struct channel_softc *, int, int, int));
int   wdc_dmawait __P((struct channel_softc *, struct wdc_xfer *, int));
void  wdcbit_bucket __P(( struct channel_softc *, int));
void  wdccommand __P((struct channel_softc *, u_int8_t, u_int8_t, u_int16_t,
	                  u_int8_t, u_int8_t, u_int8_t, u_int8_t));
void   wdccommandshort __P((struct channel_softc *, int, int));
void  wdctimeout	__P((void *arg));
void wdc_reset_channel __P((struct ata_drive_datas *));
int wdc_exec_command __P((struct ata_drive_datas *, struct wdc_command*));
#define WDC_COMPLETE 0x01
#define WDC_QUEUED   0x02
#define WDC_TRY_AGAIN 0x03

int	wdc_addref __P((struct channel_softc *));
void	wdc_delref __P((struct channel_softc *));
void	wdc_kill_pending __P((struct channel_softc *));

void	wdc_print_modes (struct channel_softc *);
void	wdc_probe_caps __P((struct ata_drive_datas*));

/*	
 * ST506 spec says that if READY or SEEKCMPLT go off, then the read or write
 * command is aborted.
 */   
#define wait_for_drq(chp, timeout) wdcwait((chp), WDCS_DRQ, WDCS_DRQ, (timeout))
#define wait_for_unbusy(chp, timeout)	wdcwait((chp), 0, 0, (timeout))
#define wait_for_ready(chp, timeout) wdcwait((chp), WDCS_DRDY, \
	WDCS_DRDY, (timeout))
/* ATA/ATAPI specs says a device can take 31s to reset */
#define WDC_RESET_WAIT 31000

void wdc_atapibus_attach __P((struct channel_softc *));
