/*	$NetBSD: aic6360.c,v 1.52.10.1 1997/08/27 23:31:15 thorpej Exp $	*/

#ifdef DDB
#define	integrate
#else
#define	integrate	static inline
#endif

/*
 * Copyright (c) 1994, 1995, 1996 Charles M. Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Copyright (c) 1994 Jarle Greipsland
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Acknowledgements: Many of the algorithms used in this driver are
 * inspired by the work of Julian Elischer (julian@tfs.com) and
 * Charles Hannum (mycroft@duality.gnu.ai.mit.edu).  Thanks a million!
 */

/* TODO list:
 * 1) Get the DMA stuff working.
 * 2) Get the iov/uio stuff working. Is this a good thing ???
 * 3) Get the synch stuff working.
 * 4) Rewrite it to use malloc for the acb structs instead of static alloc.?
 */

/*
 * A few customizable items:
 */

/* Use doubleword transfers to/from SCSI chip.  Note: This requires
 * motherboard support.  Basicly, some motherboard chipsets are able to
 * split a 32 bit I/O operation into two 16 bit I/O operations,
 * transparently to the processor.  This speeds up some things, notably long
 * data transfers.
 */
#define AIC_USE_DWORDS		0

/* Synchronous data transfers? */
#define AIC_USE_SYNCHRONOUS	0
#define AIC_SYNC_REQ_ACK_OFS 	8

/* Wide data transfers? */
#define	AIC_USE_WIDE		0
#define	AIC_MAX_WIDTH		0

/* Max attempts made to transmit a message */
#define AIC_MSG_MAX_ATTEMPT	3 /* Not used now XXX */

/* Use DMA (else we do programmed I/O using string instructions) (not yet!)*/
#define AIC_USE_EISA_DMA	0
#define AIC_USE_ISA_DMA		0

/* How to behave on the (E)ISA bus when/if DMAing (on<<4) + off in us */
#define EISA_BRST_TIM ((15<<4) + 1)	/* 15us on, 1us off */

/* Some spin loop parameters (essentially how long to wait some places)
 * The problem(?) is that sometimes we expect either to be able to transmit a
 * byte or to get a new one from the SCSI bus pretty soon.  In order to avoid
 * returning from the interrupt just to get yanked back for the next byte we
 * may spin in the interrupt routine waiting for this byte to come.  How long?
 * This is really (SCSI) device and processor dependent.  Tuneable, I guess.
 */
#define AIC_MSGIN_SPIN		1 	/* Will spinwait upto ?ms for a new msg byte */
#define AIC_MSGOUT_SPIN		1

/* Include debug functions?  At the end of this file there are a bunch of
 * functions that will print out various information regarding queued SCSI
 * commands, driver state and chip contents.  You can call them from the
 * kernel debugger.  If you set AIC_DEBUG to 0 they are not included (the
 * kernel uses less memory) but you lose the debugging facilities.
 */
#define AIC_DEBUG		1

#define	AIC_ABORT_TIMEOUT	2000	/* time to wait for abort */

/* End of customizable parameters */

#if AIC_USE_EISA_DMA || AIC_USE_ISA_DMA
#error "I said not yet! Start paying attention... grumble"
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/queue.h>

#include <machine/intr.h>
#include <machine/pio.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsi_message.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/isa/isavar.h>

/* Definitions, most of them has turned out to be unneccesary, but here they
 * are anyway.
 */

/* AIC6360 definitions */
#define SCSISEQ		0x00	/* SCSI sequence control */
#define SXFRCTL0	0x01	/* SCSI transfer control 0 */
#define SXFRCTL1	0x02	/* SCSI transfer control 1 */
#define SCSISIG		0x03	/* SCSI signal in/out */
#define SCSIRATE	0x04	/* SCSI rate control */
#define SCSIID		0x05	/* SCSI ID */
#define SELID		0x05	/* Selection/Reselection ID */
#define SCSIDAT		0x06	/* SCSI Latched Data */
#define SCSIBUS		0x07	/* SCSI Data Bus*/
#define STCNT0		0x08	/* SCSI transfer count */
#define STCNT1		0x09
#define STCNT2		0x0a
#define CLRSINT0	0x0b	/* Clear SCSI interrupts 0 */
#define SSTAT0		0x0b	/* SCSI interrupt status 0 */
#define CLRSINT1	0x0c	/* Clear SCSI interrupts 1 */
#define SSTAT1		0x0c	/* SCSI status 1 */
#define SSTAT2		0x0d	/* SCSI status 2 */
#define SCSITEST	0x0e	/* SCSI test control */
#define SSTAT3		0x0e	/* SCSI status 3 */
#define CLRSERR		0x0f	/* Clear SCSI errors */
#define SSTAT4		0x0f	/* SCSI status 4 */
#define SIMODE0		0x10	/* SCSI interrupt mode 0 */
#define SIMODE1		0x11	/* SCSI interrupt mode 1 */
#define DMACNTRL0	0x12	/* DMA control 0 */
#define DMACNTRL1	0x13	/* DMA control 1 */
#define DMASTAT		0x14	/* DMA status */
#define FIFOSTAT	0x15	/* FIFO status */
#define DMADATA		0x16	/* DMA data */
#define DMADATAL	0x16	/* DMA data low byte */
#define DMADATAH	0x17	/* DMA data high byte */
#define BRSTCNTRL	0x18	/* Burst Control */
#define DMADATALONG	0x18
#define PORTA		0x1a	/* Port A */
#define PORTB		0x1b	/* Port B */
#define REV		0x1c	/* Revision (001 for 6360) */
#define STACK		0x1d	/* Stack */
#define TEST		0x1e	/* Test register */
#define ID		0x1f	/* ID register */

#define IDSTRING "(C)1991ADAPTECAIC6360           "

/* What all the bits do */

/* SCSISEQ */
#define TEMODEO		0x80
#define ENSELO		0x40
#define ENSELI		0x20
#define ENRESELI	0x10
#define ENAUTOATNO	0x08
#define ENAUTOATNI	0x04
#define ENAUTOATNP	0x02
#define SCSIRSTO	0x01

/* SXFRCTL0 */
#define SCSIEN		0x80
#define DMAEN		0x40
#define CHEN		0x20
#define CLRSTCNT	0x10
#define SPIOEN		0x08
#define CLRCH		0x02

/* SXFRCTL1 */
#define BITBUCKET	0x80
#define SWRAPEN		0x40
#define ENSPCHK		0x20
#define STIMESEL1	0x10
#define STIMESEL0	0x08
#define STIMO_256ms	0x00
#define STIMO_128ms	0x08
#define STIMO_64ms	0x10
#define STIMO_32ms	0x18
#define ENSTIMER	0x04
#define BYTEALIGN	0x02

/* SCSISIG (in) */
#define CDI		0x80
#define IOI		0x40
#define MSGI		0x20
#define ATNI		0x10
#define SELI		0x08
#define BSYI		0x04
#define REQI		0x02
#define ACKI		0x01

/* Important! The 3 most significant bits of this register, in initiator mode,
 * represents the "expected" SCSI bus phase and can be used to trigger phase
 * mismatch and phase change interrupts.  But more important:  If there is a
 * phase mismatch the chip will not transfer any data!  This is actually a nice
 * feature as it gives us a bit more control over what is happening when we are
 * bursting data (in) through the FIFOs and the phase suddenly changes from
 * DATA IN to STATUS or MESSAGE IN.  The transfer will stop and wait for the
 * proper phase to be set in this register instead of dumping the bits into the
 * FIFOs.
 */
/* SCSISIG (out) */
#define CDO		0x80
#define IOO		0x40
#define MSGO		0x20
#define ATNO		0x10
#define SELO		0x08
#define BSYO		0x04
#define REQO		0x02
#define ACKO		0x01

/* Information transfer phases */
#define PH_DATAOUT	(0)
#define PH_DATAIN	(IOI)
#define PH_CMD		(CDI)
#define PH_STAT		(CDI | IOI)
#define PH_MSGOUT	(MSGI | CDI)
#define PH_MSGIN	(MSGI | CDI | IOI)

#define PH_MASK		(MSGI | CDI | IOI)

#define	PH_INVALID	0xff

/* SCSIRATE */
#define SXFR2		0x40
#define SXFR1		0x20
#define SXFR0		0x10
#define SOFS3		0x08
#define SOFS2		0x04
#define SOFS1		0x02
#define SOFS0		0x01

/* SCSI ID */
#define OID2		0x40
#define OID1		0x20
#define OID0		0x10
#define OID_S		4	/* shift value */
#define TID2		0x04
#define TID1		0x02
#define TID0		0x01
#define SCSI_ID_MASK	0x7

/* SCSI selection/reselection ID (both target *and* initiator) */
#define SELID7		0x80
#define SELID6		0x40
#define SELID5		0x20
#define SELID4		0x10
#define SELID3		0x08
#define SELID2		0x04
#define SELID1		0x02
#define SELID0		0x01

/* CLRSINT0                      Clears what? (interrupt and/or status bit) */
#define SETSDONE	0x80
#define CLRSELDO	0x40	/* I */
#define CLRSELDI	0x20	/* I+ */
#define CLRSELINGO	0x10	/* I */
#define CLRSWRAP	0x08	/* I+S */
#define CLRSDONE	0x04	/* I+S */
#define CLRSPIORDY	0x02	/* I */
#define CLRDMADONE	0x01	/* I */

/* SSTAT0                          Howto clear */
#define TARGET		0x80
#define SELDO		0x40	/* Selfclearing */
#define SELDI		0x20	/* Selfclearing when CLRSELDI is set */
#define SELINGO		0x10	/* Selfclearing */
#define SWRAP		0x08	/* CLRSWAP */
#define SDONE		0x04	/* Not used in initiator mode */
#define SPIORDY		0x02	/* Selfclearing (op on SCSIDAT) */
#define DMADONE		0x01	/* Selfclearing (all FIFOs empty & T/C */

/* CLRSINT1                      Clears what? */
#define CLRSELTIMO	0x80	/* I+S */
#define CLRATNO		0x40
#define CLRSCSIRSTI	0x20	/* I+S */
#define CLRBUSFREE	0x08	/* I+S */
#define CLRSCSIPERR	0x04	/* I+S */
#define CLRPHASECHG	0x02	/* I+S */
#define CLRREQINIT	0x01	/* I+S */

/* SSTAT1                       How to clear?  When set?*/
#define SELTO		0x80	/* C		select out timeout */
#define ATNTARG		0x40	/* Not used in initiator mode */
#define SCSIRSTI	0x20	/* C		RST asserted */
#define PHASEMIS	0x10	/* Selfclearing */
#define BUSFREE		0x08	/* C		bus free condition */
#define SCSIPERR	0x04	/* C		parity error on inbound data */
#define PHASECHG	0x02	/* C	     phase in SCSISIG doesn't match */
#define REQINIT		0x01	/* C or ACK	asserting edge of REQ */

/* SSTAT2 */
#define SOFFSET		0x20
#define SEMPTY		0x10
#define SFULL		0x08
#define SFCNT2		0x04
#define SFCNT1		0x02
#define SFCNT0		0x01

/* SCSITEST */
#define SCTESTU		0x08
#define SCTESTD		0x04
#define STCTEST		0x01

/* SSTAT3 */
#define SCSICNT3	0x80
#define SCSICNT2	0x40
#define SCSICNT1	0x20
#define SCSICNT0	0x10
#define OFFCNT3		0x08
#define OFFCNT2		0x04
#define OFFCNT1		0x02
#define OFFCNT0		0x01

/* CLRSERR */
#define CLRSYNCERR	0x04
#define CLRFWERR	0x02
#define CLRFRERR	0x01

/* SSTAT4 */
#define SYNCERR		0x04
#define FWERR		0x02
#define FRERR		0x01

/* SIMODE0 */
#define ENSELDO		0x40
#define ENSELDI		0x20
#define ENSELINGO	0x10
#define	ENSWRAP		0x08
#define ENSDONE		0x04
#define ENSPIORDY	0x02
#define ENDMADONE	0x01

/* SIMODE1 */
#define ENSELTIMO	0x80
#define ENATNTARG	0x40
#define ENSCSIRST	0x20
#define ENPHASEMIS	0x10
#define ENBUSFREE	0x08
#define ENSCSIPERR	0x04
#define ENPHASECHG	0x02
#define ENREQINIT	0x01

/* DMACNTRL0 */
#define ENDMA		0x80
#define B8MODE		0x40
#define DMA		0x20
#define DWORDPIO	0x10
#define WRITE		0x08
#define INTEN		0x04
#define RSTFIFO		0x02
#define SWINT		0x01

/* DMACNTRL1 */
#define PWRDWN		0x80
#define ENSTK32		0x40
#define STK4		0x10
#define STK3		0x08
#define STK2		0x04
#define STK1		0x02
#define STK0		0x01

/* DMASTAT */
#define ATDONE		0x80
#define WORDRDY		0x40
#define INTSTAT		0x20
#define DFIFOFULL	0x10
#define DFIFOEMP	0x08
#define DFIFOHF		0x04
#define DWORDRDY	0x02

/* BRSTCNTRL */
#define BON3		0x80
#define BON2		0x40
#define BON1		0x20
#define BON0		0x10
#define BOFF3		0x08
#define BOFF2		0x04
#define BOFF1		0x02
#define BOFF0		0x01

/* TEST */
#define BOFFTMR		0x40
#define BONTMR		0x20
#define STCNTH		0x10
#define STCNTM		0x08
#define STCNTL		0x04
#define SCSIBLK		0x02
#define DMABLK		0x01

#ifndef DDB
#define	Debugger() panic("should call debugger here (aic6360.c)")
#endif /* ! DDB */

typedef u_long physaddr;
typedef u_long physlen;

struct aic_dma_seg {
	physaddr seg_addr;
	physlen seg_len;
};

#define AIC_NSEG	16

/*
 * ACB. Holds additional information for each SCSI command Comments: We
 * need a separate scsi command block because we may need to overwrite it
 * with a request sense command.  Basicly, we refrain from fiddling with
 * the scsipi_xfer struct (except do the expected updating of return values).
 * We'll generally update: xs->{flags,resid,error,sense,status} and
 * occasionally xs->retries.
 */
struct aic_acb {
	struct scsi_generic scsipi_cmd;
	int scsipi_cmd_length;
	u_char *data_addr;		/* Saved data pointer */
	int data_length;		/* Residue */

	u_char target_stat;		/* SCSI status byte */

#ifdef notdef
	struct aic_dma_seg dma[AIC_NSEG]; /* Physical addresses+len */
#endif

	TAILQ_ENTRY(aic_acb) chain;
	struct scsipi_xfer *xs;	/* SCSI xfer ctrl block from above */
	int flags;
#define ACB_ALLOC	0x01
#define	ACB_NEXUS	0x02
#define ACB_SENSE	0x04
#define	ACB_ABORT	0x40
#define	ACB_RESET	0x80
	int timeout;
};

/*
 * Some info about each (possible) target on the SCSI bus.  This should
 * probably have been a "per target+lunit" structure, but we'll leave it at
 * this for now.
 */
struct aic_tinfo {
	int	cmds;		/* #commands processed */
	int	dconns;		/* #disconnects */
	int	touts;		/* #timeouts */
	int	perrs;		/* #parity errors */
	int	senses;		/* #request sense commands sent */
	ushort	lubusy;		/* What local units/subr. are busy? */
	u_char  flags;
#define DO_SYNC		0x01	/* (Re)Negotiate synchronous options */
#define	DO_WIDE		0x02	/* (Re)Negotiate wide options */
	u_char  period;		/* Period suggestion */
	u_char  offset;		/* Offset suggestion */
	u_char	width;		/* Width suggestion */
} tinfo_t;

struct aic_softc {
	struct device sc_dev;
	struct isadev sc_id;
	void *sc_ih;

	int sc_iobase;
	int sc_irq, sc_drq;

	struct scsipi_link sc_link;	/* prototype for subdevs */

	TAILQ_HEAD(, aic_acb) free_list, ready_list, nexus_list;
	struct aic_acb *sc_nexus;	/* current command */
	struct aic_acb sc_acb[8];
	struct aic_tinfo sc_tinfo[8];

	/* Data about the current nexus (updated for every cmd switch) */
	u_char	*sc_dp;		/* Current data pointer */
	size_t	sc_dleft;	/* Data bytes left to transfer */
	u_char	*sc_cp;		/* Current command pointer */
	size_t	sc_cleft;	/* Command bytes left to transfer */

	/* Adapter state */
	u_char	 sc_phase;	/* Current bus phase */
	u_char	 sc_prevphase;	/* Previous bus phase */
	u_char	 sc_state;	/* State applicable to the adapter */
#define	AIC_INIT	0
#define AIC_IDLE	1
#define AIC_SELECTING	2	/* SCSI command is arbiting  */
#define AIC_RESELECTED	3	/* Has been reselected */
#define AIC_CONNECTED	4	/* Actively using the SCSI bus */
#define	AIC_DISCONNECT	5	/* MSG_DISCONNECT received */
#define	AIC_CMDCOMPLETE	6	/* MSG_CMDCOMPLETE received */
#define AIC_CLEANING	7
	u_char	 sc_flags;
#define AIC_DROP_MSGIN	0x01	/* Discard all msgs (parity err detected) */
#define	AIC_ABORTING	0x02	/* Bailing out */
#define AIC_DOINGDMA	0x04	/* The FIFO data path is active! */
	u_char	sc_selid;	/* Reselection ID */

	/* Message stuff */
	u_char	sc_msgpriq;	/* Messages we want to send */
	u_char	sc_msgoutq;	/* Messages sent during last MESSAGE OUT */
	u_char	sc_lastmsg;	/* Message last transmitted */
	u_char	sc_currmsg;	/* Message currently ready to transmit */
#define SEND_DEV_RESET		0x01
#define SEND_PARITY_ERROR	0x02
#define SEND_INIT_DET_ERR	0x04
#define SEND_REJECT		0x08
#define SEND_IDENTIFY  		0x10
#define SEND_ABORT		0x20
#define SEND_SDTR		0x40
#define	SEND_WDTR		0x80
#define AIC_MAX_MSG_LEN 8
	u_char  sc_omess[AIC_MAX_MSG_LEN];
	u_char	*sc_omp;		/* Outgoing message pointer */
	u_char	sc_imess[AIC_MAX_MSG_LEN];
	u_char	*sc_imp;		/* Incoming message pointer */

	/* Hardware stuff */
	int	sc_initiator;		/* Our scsi id */
	int	sc_freq;		/* Clock frequency in MHz */
	int	sc_minsync;		/* Minimum sync period / 4 */
	int	sc_maxsync;		/* Maximum sync period / 4 */
};

#if AIC_DEBUG
#define AIC_SHOWACBS	0x01
#define AIC_SHOWINTS	0x02
#define AIC_SHOWCMDS	0x04
#define AIC_SHOWMISC	0x08
#define AIC_SHOWTRACE	0x10
#define AIC_SHOWSTART	0x20
#define AIC_DOBREAK	0x40
int aic_debug = 0x00; /* AIC_SHOWSTART|AIC_SHOWMISC|AIC_SHOWTRACE; */
#define	AIC_PRINT(b, s)	do {if ((aic_debug & (b)) != 0) printf s;} while (0)
#define	AIC_BREAK()	do {if ((aic_debug & AIC_DOBREAK) != 0) Debugger();} while (0)
#define	AIC_ASSERT(x)	do {if (x) {} else {printf("%s at line %d: assertion failed\n", sc->sc_dev.dv_xname, __LINE__); Debugger();}} while (0)
#else
#define	AIC_PRINT(b, s)
#define	AIC_BREAK()
#define	AIC_ASSERT(x)
#endif

#define AIC_ACBS(s)	AIC_PRINT(AIC_SHOWACBS, s)
#define AIC_INTS(s)	AIC_PRINT(AIC_SHOWINTS, s)
#define AIC_CMDS(s)	AIC_PRINT(AIC_SHOWCMDS, s)
#define AIC_MISC(s)	AIC_PRINT(AIC_SHOWMISC, s)
#define AIC_TRACE(s)	AIC_PRINT(AIC_SHOWTRACE, s)
#define AIC_START(s)	AIC_PRINT(AIC_SHOWSTART, s)

int	aicprobe	__P((struct device *, void *, void *));
void	aicattach	__P((struct device *, struct device *, void *));
void	aic_minphys	__P((struct buf *));
int	aicintr		__P((void *));
void 	aic_init	__P((struct aic_softc *));
void	aic_done	__P((struct aic_softc *, struct aic_acb *));
void	aic_dequeue	__P((struct aic_softc *, struct aic_acb *));
int	aic_scsi_cmd	__P((struct scsipi_xfer *));
int	aic_poll	__P((struct aic_softc *, struct scsipi_xfer *, int));
integrate void	aic_sched_msgout __P((struct aic_softc *, u_char));
integrate void	aic_setsync	__P((struct aic_softc *, struct aic_tinfo *));
void	aic_select	__P((struct aic_softc *, struct aic_acb *));
void	aic_timeout	__P((void *));
int	aic_find	__P((struct aic_softc *));
void	aic_sched	__P((struct aic_softc *));
void	aic_scsi_reset	__P((struct aic_softc *));
void	aic_reset	__P((struct aic_softc *));
void	aic_free_acb	__P((struct aic_softc *, struct aic_acb *, int));
struct aic_acb* aic_get_acb __P((struct aic_softc *, int));
int	aic_reselect	__P((struct aic_softc *, int));
void	aic_sense	__P((struct aic_softc *, struct aic_acb *));
void	aic_msgin	__P((struct aic_softc *));
void	aic_abort	__P((struct aic_softc *, struct aic_acb *));
void	aic_msgout	__P((struct aic_softc *));
int	aic_dataout_pio	__P((struct aic_softc *, u_char *, int));
int	aic_datain_pio	__P((struct aic_softc *, u_char *, int));
#if AIC_DEBUG
void	aic_print_acb	__P((struct aic_acb *));
void	aic_dump_driver __P((struct aic_softc *));
void	aic_dump6360	__P((struct aic_softc *));
void	aic_show_scsi_cmd __P((struct aic_acb *));
void	aic_print_active_acb __P((void));
#endif

struct cfattach aic_ca = {
	sizeof(struct aic_softc), aicprobe, aicattach
};

struct cfdriver aic_cd = {
	NULL, "aic", DV_DULL
};

struct scsipi_adapter aic_switch = {
	aic_scsi_cmd,
	aic_minphys,
	0,
	0,
};

struct scsipi_device aic_dev = {
	NULL,			/* Use default error handler */
	NULL,			/* have a queue, served by this */
	NULL,			/* have no async handler */
	NULL,			/* Use default 'done' routine */
};

/*
 * INITIALIZATION ROUTINES (probe, attach ++)
 */

/*
 * aicprobe: probe for AIC6360 SCSI-controller
 * returns non-zero value if a controller is found.
 */
int
aicprobe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct aic_softc *sc = match;
	struct isa_attach_args *ia = aux;

#ifdef NEWCONFIG
	if (ia->ia_iobase == IOBASEUNK)
		return 0;
#endif

	sc->sc_iobase = ia->ia_iobase;
	if (aic_find(sc) != 0)
		return 0;

#ifdef NEWCONFIG
	if (ia->ia_irq != IRQUNK) {
		if (ia->ia_irq != sc->sc_irq) {
			printf("%s: irq mismatch; kernel configured %d != board configured %d\n",
			    sc->sc_dev.dv_xname, ia->ia_irq, sc->sc_irq);
			return 0;
		}
	} else
		ia->ia_irq = sc->sc_irq;

	if (ia->ia_drq != DRQUNK) {
		if (ia->ia_drq != sc->sc_drq) {
			printf("%s: drq mismatch; kernel configured %d != board configured %d\n",
			    sc->sc_dev.dv_xname, ia->ia_drq, sc->sc_drq);
			return 0;
		}
	} else
		ia->ia_drq = sc->sc_drq;
#endif

	ia->ia_msize = 0;
	ia->ia_iosize = 0x20;
	return 1;
}

/* Do the real search-for-device.
 * Prerequisite: sc->sc_iobase should be set to the proper value
 */
int
aic_find(sc)
	struct aic_softc *sc;
{
	int iobase = sc->sc_iobase;
	char chip_id[sizeof(IDSTRING)];	/* For chips that support it */
	int i;

	/* Remove aic6360 from possible powerdown mode */
	outb(iobase + DMACNTRL0, 0);

	/* Thanks to mark@aggregate.com for the new method for detecting
	 * whether the chip is present or not.  Bonus: may also work for
	 * the AIC-6260!
 	 */
	AIC_TRACE(("aic: probing for aic-chip at port 0x%x\n",
	    sc->sc_iobase));
 	/*
 	 * Linux also init's the stack to 1-16 and then clears it,
     	 *  6260's don't appear to have an ID reg - mpg
 	 */
	/* Push the sequence 0,1,..,15 on the stack */
#define STSIZE 16
	outb(iobase + DMACNTRL1, 0);	/* Reset stack pointer */
	for (i = 0; i < STSIZE; i++)
		outb(iobase + STACK, i);

	/* See if we can pull out the same sequence */
	outb(iobase + DMACNTRL1, 0);
 	for (i = 0; i < STSIZE && inb(iobase + STACK) == i; i++)
		;
	if (i != STSIZE) {
		AIC_START(("STACK futzed at %d.\n", i));
		return ENXIO;
	}

	/* See if we can pull the id string out of the ID register,
	 * now only used for informational purposes.
	 */
	bzero(chip_id, sizeof(chip_id));
	insb(iobase + ID, chip_id, sizeof(IDSTRING)-1);
	AIC_START(("AIC found at 0x%x ", sc->sc_iobase));
	AIC_START(("ID: %s ",chip_id));
	AIC_START(("chip revision %d\n",(int)inb(iobase + REV)));

	sc->sc_initiator = 7;
	sc->sc_freq = 20;	/* XXXX Assume 20 MHz. */

	/*
	 * These are the bounds of the sync period, based on the frequency of
	 * the chip's clock input and the size and offset of the sync period
	 * register.
	 *
	 * For a 20Mhz clock, this gives us 25, or 100nS, or 10MB/s, as a
	 * maximum transfer rate, and 112.5, or 450nS, or 2.22MB/s, as a
	 * minimum transfer rate.
	 */
	sc->sc_minsync = (2 * 250) / sc->sc_freq;
	sc->sc_maxsync = (9 * 250) / sc->sc_freq;

	return 0;
}

/*
 * Attach the AIC6360, fill out some high and low level data structures
 */
void
aicattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	struct aic_softc *sc = (void *)self;

	AIC_TRACE(("aicattach  "));
	sc->sc_state = AIC_INIT;
	aic_init(sc);	/* Init chip and driver */

	/*
	 * Fill in the prototype scsipi_link
	 */
	sc->sc_link.scsipi_scsi.channel = SCSI_CHANNEL_ONLY_ONE;
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.scsipi_scsi.adapter_target = sc->sc_initiator;
	sc->sc_link.adapter = &aic_switch;
	sc->sc_link.device = &aic_dev;
	sc->sc_link.openings = 2;
	sc->sc_link.scsipi_scsi.max_target = 7;
	sc->sc_link.type = BUS_SCSI;

	printf("\n");

#ifdef NEWCONFIG
	isa_establish(&sc->sc_id, &sc->sc_dev);
#endif
	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
	    IPL_BIO, aicintr, sc);

	config_found(self, &sc->sc_link, scsiprint);
}


/* Initialize AIC6360 chip itself
 * The following conditions should hold:
 * aicprobe should have succeeded, i.e. the iobase address in aic_softc must
 * be valid.
 */
void
aic_reset(sc)
	struct aic_softc *sc;
{
	int iobase = sc->sc_iobase;

	outb(iobase + SCSITEST, 0);	/* Doc. recommends to clear these two */
	outb(iobase + TEST, 0);		/* registers before operations commence */

	/* Reset SCSI-FIFO and abort any transfers */
	outb(iobase + SXFRCTL0, CHEN | CLRCH | CLRSTCNT);

	/* Reset DMA-FIFO */
	outb(iobase + DMACNTRL0, RSTFIFO);
	outb(iobase + DMACNTRL1, 0);

	outb(iobase + SCSISEQ, 0);	/* Disable all selection features */
	outb(iobase + SXFRCTL1, 0);

	outb(iobase + SIMODE0, 0x00);	/* Disable some interrupts */
	outb(iobase + CLRSINT0, 0x7f);	/* Clear a slew of interrupts */

	outb(iobase + SIMODE1, 0x00);	/* Disable some more interrupts */
	outb(iobase + CLRSINT1, 0xef);	/* Clear another slew of interrupts */

	outb(iobase + SCSIRATE, 0);	/* Disable synchronous transfers */

	outb(iobase + CLRSERR, 0x07);	/* Haven't seen ant errors (yet) */

	outb(iobase + SCSIID, sc->sc_initiator << OID_S); /* Set our SCSI-ID */
	outb(iobase + BRSTCNTRL, EISA_BRST_TIM);
}

/* Pull the SCSI RST line for 500 us */
void
aic_scsi_reset(sc)
	struct aic_softc *sc;
{
	int iobase = sc->sc_iobase;

	outb(iobase + SCSISEQ, SCSIRSTO);
	delay(500);
	outb(iobase + SCSISEQ, 0);
	delay(50);
}

/*
 * Initialize aic SCSI driver.
 */
void
aic_init(sc)
	struct aic_softc *sc;
{
	int iobase = sc->sc_iobase;
	struct aic_acb *acb;
	int r;

	aic_reset(sc);
	aic_scsi_reset(sc);
	aic_reset(sc);

	if (sc->sc_state == AIC_INIT) {
		/* First time through; initialize. */
		TAILQ_INIT(&sc->ready_list);
		TAILQ_INIT(&sc->nexus_list);
		TAILQ_INIT(&sc->free_list);
		sc->sc_nexus = NULL;
		acb = sc->sc_acb;
		bzero(acb, sizeof(sc->sc_acb));
		for (r = 0; r < sizeof(sc->sc_acb) / sizeof(*acb); r++) {
			TAILQ_INSERT_TAIL(&sc->free_list, acb, chain);
			acb++;
		}
		bzero(&sc->sc_tinfo, sizeof(sc->sc_tinfo));
	} else {
		/* Cancel any active commands. */
		sc->sc_state = AIC_CLEANING;
		if ((acb = sc->sc_nexus) != NULL) {
			acb->xs->error = XS_DRIVER_STUFFUP;
			untimeout(aic_timeout, acb);
			aic_done(sc, acb);
		}
		while ((acb = sc->nexus_list.tqh_first) != NULL) {
			acb->xs->error = XS_DRIVER_STUFFUP;
			untimeout(aic_timeout, acb);
			aic_done(sc, acb);
		}
	}

	sc->sc_prevphase = PH_INVALID;
	for (r = 0; r < 8; r++) {
		struct aic_tinfo *ti = &sc->sc_tinfo[r];

		ti->flags = 0;
#if AIC_USE_SYNCHRONOUS
		ti->flags |= DO_SYNC;
		ti->period = sc->sc_minsync;
		ti->offset = AIC_SYNC_REQ_ACK_OFS;
#else
		ti->period = ti->offset = 0;
#endif
#if AIC_USE_WIDE
		ti->flags |= DO_WIDE;
		ti->width = AIC_MAX_WIDTH;
#else
		ti->width = 0;
#endif
	}

	sc->sc_state = AIC_IDLE;
	outb(iobase + DMACNTRL0, INTEN);
}

void
aic_free_acb(sc, acb, flags)
	struct aic_softc *sc;
	struct aic_acb *acb;
	int flags;
{
	int s;

	s = splbio();

	acb->flags = 0;
	TAILQ_INSERT_HEAD(&sc->free_list, acb, chain);

	/*
	 * If there were none, wake anybody waiting for one to come free,
	 * starting with queued entries.
	 */
	if (acb->chain.tqe_next == 0)
		wakeup(&sc->free_list);

	splx(s);
}

struct aic_acb *
aic_get_acb(sc, flags)
	struct aic_softc *sc;
	int flags;
{
	struct aic_acb *acb;
	int s;

	s = splbio();

	while ((acb = sc->free_list.tqh_first) == NULL &&
	       (flags & SCSI_NOSLEEP) == 0)
		tsleep(&sc->free_list, PRIBIO, "aicacb", 0);
	if (acb) {
		TAILQ_REMOVE(&sc->free_list, acb, chain);
		acb->flags |= ACB_ALLOC;
	}

	splx(s);
	return acb;
}

/*
 * DRIVER FUNCTIONS CALLABLE FROM HIGHER LEVEL DRIVERS
 */

/*
 * Expected sequence:
 * 1) Command inserted into ready list
 * 2) Command selected for execution
 * 3) Command won arbitration and has selected target device
 * 4) Send message out (identify message, eventually also sync.negotiations)
 * 5) Send command
 * 5a) Receive disconnect message, disconnect.
 * 5b) Reselected by target
 * 5c) Receive identify message from target.
 * 6) Send or receive data
 * 7) Receive status
 * 8) Receive message (command complete etc.)
 * 9) If status == SCSI_CHECK construct a synthetic request sense SCSI cmd.
 *    Repeat 2-8 (no disconnects please...)
 */

/*
 * Start a SCSI-command
 * This function is called by the higher level SCSI-driver to queue/run
 * SCSI-commands.
 */
int
aic_scsi_cmd(xs)
	struct scsipi_xfer *xs;
{
	struct scsipi_link *sc_link = xs->sc_link;
	struct aic_softc *sc = sc_link->adapter_softc;
	struct aic_acb *acb;
	int s, flags;

	AIC_TRACE(("aic_scsi_cmd  "));
	AIC_CMDS(("[0x%x, %d]->%d ", (int)xs->cmd->opcode, xs->cmdlen,
	    sc_link->scsipi_scsi.target));

	flags = xs->flags;
	if ((acb = aic_get_acb(sc, flags)) == NULL) {
		xs->error = XS_DRIVER_STUFFUP;
		return TRY_AGAIN_LATER;
	}

	/* Initialize acb */
	acb->xs = xs;
	acb->timeout = xs->timeout;

	if (xs->flags & SCSI_RESET) {
		acb->flags |= ACB_RESET;
		acb->scsipi_cmd_length = 0;
		acb->data_length = 0;
	} else {
		bcopy(xs->cmd, &acb->scsipi_cmd, xs->cmdlen);
		acb->scsipi_cmd_length = xs->cmdlen;
		acb->data_addr = xs->data;
		acb->data_length = xs->datalen;
	}
	acb->target_stat = 0;

	s = splbio();

	TAILQ_INSERT_TAIL(&sc->ready_list, acb, chain);
	if (sc->sc_state == AIC_IDLE)
		aic_sched(sc);

	splx(s);

	if ((flags & SCSI_POLL) == 0)
		return SUCCESSFULLY_QUEUED;

	/* Not allowed to use interrupts, use polling instead */
	if (aic_poll(sc, xs, acb->timeout)) {
		aic_timeout(acb);
		if (aic_poll(sc, xs, acb->timeout))
			aic_timeout(acb);
	}
	return COMPLETE;
}

/*
 * Adjust transfer size in buffer structure
 */
void
aic_minphys(bp)
	struct buf *bp;
{

	AIC_TRACE(("aic_minphys  "));
	if (bp->b_bcount > (AIC_NSEG << PGSHIFT))
		bp->b_bcount = (AIC_NSEG << PGSHIFT);
	minphys(bp);
}

/*
 * Used when interrupt driven I/O isn't allowed, e.g. during boot.
 */
int
aic_poll(sc, xs, count)
	struct aic_softc *sc;
	struct scsipi_xfer *xs;
	int count;
{
	int iobase = sc->sc_iobase;

	AIC_TRACE(("aic_poll  "));
	while (count) {
		/*
		 * If we had interrupts enabled, would we
		 * have got an interrupt?
		 */
		if ((inb(iobase + DMASTAT) & INTSTAT) != 0)
			aicintr(sc);
		if ((xs->flags & ITSDONE) != 0)
			return 0;
		delay(1000);
		count--;
	}
	return 1;
}

/*
 * LOW LEVEL SCSI UTILITIES
 */

integrate void
aic_sched_msgout(sc, m)
	struct aic_softc *sc;
	u_char m;
{
	int iobase = sc->sc_iobase;

	if (sc->sc_msgpriq == 0)
		outb(iobase + SCSISIG, sc->sc_phase | ATNO);
	sc->sc_msgpriq |= m;
}

/*
 * Set synchronous transfer offset and period.
 */
integrate void
aic_setsync(sc, ti)
	struct aic_softc *sc;
	struct aic_tinfo *ti;
{
#if AIC_USE_SYNCHRONOUS
	int iobase = sc->sc_iobase;

	if (ti->offset != 0)
		outb(iobase + SCSIRATE,
		    ((ti->period * sc->sc_freq) / 250 - 2) << 4 | ti->offset);
	else
		outb(iobase + SCSIRATE, 0);
#endif
}

/*
 * Start a selection.  This is used by aic_sched() to select an idle target,
 * and by aic_done() to immediately reselect a target to get sense information.
 */
void
aic_select(sc, acb)
	struct aic_softc *sc;
	struct aic_acb *acb;
{
	struct scsipi_link *sc_link = acb->xs->sc_link;
	int target = sc_link->scsipi_scsi.target;
	struct aic_tinfo *ti = &sc->sc_tinfo[target];
	int iobase = sc->sc_iobase;

	outb(iobase + SCSIID, sc->sc_initiator << OID_S | target);
	aic_setsync(sc, ti);
	outb(iobase + SXFRCTL1, STIMO_256ms | ENSTIMER);

	/* Always enable reselections. */
	outb(iobase + SIMODE0, ENSELDI | ENSELDO);
	outb(iobase + SIMODE1, ENSCSIRST | ENSELTIMO);
	outb(iobase + SCSISEQ, ENRESELI | ENSELO | ENAUTOATNO);

	sc->sc_state = AIC_SELECTING;
}

int
aic_reselect(sc, message)
	struct aic_softc *sc;
	int message;
{
	u_char selid, target, lun;
	struct aic_acb *acb;
	struct scsipi_link *sc_link;
	struct aic_tinfo *ti;

	/*
	 * The SCSI chip made a snapshot of the data bus while the reselection
	 * was being negotiated.  This enables us to determine which target did
	 * the reselect.
	 */
	selid = sc->sc_selid & ~(1 << sc->sc_initiator);
	if (selid & (selid - 1)) {
		printf("%s: reselect with invalid selid %02x; sending DEVICE RESET\n",
		    sc->sc_dev.dv_xname, selid);
		AIC_BREAK();
		goto reset;
	}

	/* Search wait queue for disconnected cmd
	 * The list should be short, so I haven't bothered with
	 * any more sophisticated structures than a simple
	 * singly linked list.
	 */
	target = ffs(selid) - 1;
	lun = message & 0x07;
	for (acb = sc->nexus_list.tqh_first; acb != NULL;
	     acb = acb->chain.tqe_next) {
		sc_link = acb->xs->sc_link;
		if (sc_link->scsipi_scsi.target == target && sc_link->scsipi_scsi.lun == lun)
			break;
	}
	if (acb == NULL) {
		printf("%s: reselect from target %d lun %d with no nexus; sending ABORT\n",
		    sc->sc_dev.dv_xname, target, lun);
		AIC_BREAK();
		goto abort;
	}

	/* Make this nexus active again. */
	TAILQ_REMOVE(&sc->nexus_list, acb, chain);
	sc->sc_state = AIC_CONNECTED;
	sc->sc_nexus = acb;
	ti = &sc->sc_tinfo[target];
	ti->lubusy |= (1 << lun);
	aic_setsync(sc, ti);

	if (acb->flags & ACB_RESET)
		aic_sched_msgout(sc, SEND_DEV_RESET);
	else if (acb->flags & ACB_ABORT)
		aic_sched_msgout(sc, SEND_ABORT);

	/* Do an implicit RESTORE POINTERS. */
	sc->sc_dp = acb->data_addr;
	sc->sc_dleft = acb->data_length;
	sc->sc_cp = (u_char *)&acb->scsipi_cmd;
	sc->sc_cleft = acb->scsipi_cmd_length;

	return (0);

reset:
	aic_sched_msgout(sc, SEND_DEV_RESET);
	return (1);

abort:
	aic_sched_msgout(sc, SEND_ABORT);
	return (1);
}

/*
 * Schedule a SCSI operation.  This has now been pulled out of the interrupt
 * handler so that we may call it from aic_scsi_cmd and aic_done.  This may
 * save us an unecessary interrupt just to get things going.  Should only be
 * called when state == AIC_IDLE and at bio pl.
 */
void
aic_sched(sc)
	register struct aic_softc *sc;
{
	struct aic_acb *acb;
	struct scsipi_link *sc_link;
	struct aic_tinfo *ti;
	int iobase = sc->sc_iobase;

	/*
	 * Find first acb in ready queue that is for a target/lunit pair that
	 * is not busy.
	 */
	outb(iobase + CLRSINT1, CLRSELTIMO | CLRBUSFREE | CLRSCSIPERR);
	for (acb = sc->ready_list.tqh_first; acb != NULL;
	    acb = acb->chain.tqe_next) {
		sc_link = acb->xs->sc_link;
		ti = &sc->sc_tinfo[sc_link->scsipi_scsi.target];
		if ((ti->lubusy & (1 << sc_link->scsipi_scsi.lun)) == 0) {
			AIC_MISC(("selecting %d:%d  ",
			    sc_link->scsipi_scsi.target, sc_link->scsipi_scsi.lun));
			TAILQ_REMOVE(&sc->ready_list, acb, chain);
			sc->sc_nexus = acb;
			aic_select(sc, acb);
			return;
		} else
			AIC_MISC(("%d:%d busy\n",
			    sc_link->scsipi_scsi.target, sc_link->scsipi_scsi.lun));
	}
	AIC_MISC(("idle  "));
	/* Nothing to start; just enable reselections and wait. */
	outb(iobase + SIMODE0, ENSELDI);
	outb(iobase + SIMODE1, ENSCSIRST);
	outb(iobase + SCSISEQ, ENRESELI);
}

void
aic_sense(sc, acb)
	struct aic_softc *sc;
	struct aic_acb *acb;
{
	struct scsipi_xfer *xs = acb->xs;
	struct scsipi_link *sc_link = xs->sc_link;
	struct aic_tinfo *ti = &sc->sc_tinfo[sc_link->scsipi_scsi.target];
	struct scsipi_sense *ss = (void *)&acb->scsipi_cmd;

	AIC_MISC(("requesting sense  "));
	/* Next, setup a request sense command block */
	bzero(ss, sizeof(*ss));
	ss->opcode = REQUEST_SENSE;
	ss->byte2 = sc_link->scsipi_scsi.lun << 5;
	ss->length = sizeof(struct scsipi_sense_data);
	acb->scsipi_cmd_length = sizeof(*ss);
	acb->data_addr = (char *)&xs->sense.scsi_sense;
	acb->data_length = sizeof(struct scsipi_sense_data);
	acb->flags |= ACB_SENSE;
	ti->senses++;
	if (acb->flags & ACB_NEXUS)
		ti->lubusy &= ~(1 << sc_link->scsipi_scsi.lun);
	if (acb == sc->sc_nexus) {
		aic_select(sc, acb);
	} else {
		aic_dequeue(sc, acb);
		TAILQ_INSERT_HEAD(&sc->ready_list, acb, chain);
		if (sc->sc_state == AIC_IDLE)
			aic_sched(sc);
	}
}

/*
 * POST PROCESSING OF SCSI_CMD (usually current)
 */
void
aic_done(sc, acb)
	struct aic_softc *sc;
	struct aic_acb *acb;
{
	struct scsipi_xfer *xs = acb->xs;
	struct scsipi_link *sc_link = xs->sc_link;
	struct aic_tinfo *ti = &sc->sc_tinfo[sc_link->scsipi_scsi.target];

	AIC_TRACE(("aic_done  "));

	/*
	 * Now, if we've come here with no error code, i.e. we've kept the
	 * initial XS_NOERROR, and the status code signals that we should
	 * check sense, we'll need to set up a request sense cmd block and
	 * push the command back into the ready queue *before* any other
	 * commands for this target/lunit, else we lose the sense info.
	 * We don't support chk sense conditions for the request sense cmd.
	 */
	if (xs->error == XS_NOERROR) {
		if (acb->flags & ACB_ABORT) {
			xs->error = XS_DRIVER_STUFFUP;
		} else if (acb->flags & ACB_SENSE) {
			xs->error = XS_SENSE;
		} else if (acb->target_stat == SCSI_CHECK) {
			/* First, save the return values */
			xs->resid = acb->data_length;
			xs->status = acb->target_stat;
			aic_sense(sc, acb);
			return;
		} else {
			xs->resid = acb->data_length;
		}
	}

	xs->flags |= ITSDONE;

#if AIC_DEBUG
	if ((aic_debug & AIC_SHOWMISC) != 0) {
		if (xs->resid != 0)
			printf("resid=%d ", xs->resid);
		if (xs->error == XS_SENSE)
			printf("sense=0x%02x\n", xs->sense.scsi_sense.error_code);
		else
			printf("error=%d\n", xs->error);
	}
#endif

	/*
	 * Remove the ACB from whatever queue it happens to be on.
	 */
	if (acb->flags & ACB_NEXUS)
		ti->lubusy &= ~(1 << sc_link->scsipi_scsi.lun);
	if (acb == sc->sc_nexus) {
		sc->sc_nexus = NULL;
		sc->sc_state = AIC_IDLE;
		aic_sched(sc);
	} else
		aic_dequeue(sc, acb);

	aic_free_acb(sc, acb, xs->flags);
	ti->cmds++;
	scsipi_done(xs);
}

void
aic_dequeue(sc, acb)
	struct aic_softc *sc;
	struct aic_acb *acb;
{

	if (acb->flags & ACB_NEXUS) {
		TAILQ_REMOVE(&sc->nexus_list, acb, chain);
	} else {
		TAILQ_REMOVE(&sc->ready_list, acb, chain);
	}
}

/*
 * INTERRUPT/PROTOCOL ENGINE
 */

#define IS1BYTEMSG(m) (((m) != 0x01 && (m) < 0x20) || (m) >= 0x80)
#define IS2BYTEMSG(m) (((m) & 0xf0) == 0x20)
#define ISEXTMSG(m) ((m) == 0x01)

/*
 * Precondition:
 * The SCSI bus is already in the MSGI phase and there is a message byte
 * on the bus, along with an asserted REQ signal.
 */
void
aic_msgin(sc)
	register struct aic_softc *sc;
{
	int iobase = sc->sc_iobase;
	u_char sstat1;
	int n;

	AIC_TRACE(("aic_msgin  "));

	if (sc->sc_prevphase == PH_MSGIN) {
		/* This is a continuation of the previous message. */
		n = sc->sc_imp - sc->sc_imess;
		goto nextbyte;
	}

	/* This is a new MESSAGE IN phase.  Clean up our state. */
	sc->sc_flags &= ~AIC_DROP_MSGIN;

nextmsg:
	n = 0;
	sc->sc_imp = &sc->sc_imess[n];

nextbyte:
	/*
	 * Read a whole message, but don't ack the last byte.  If we reject the
	 * message, we have to assert ATN during the message transfer phase
	 * itself.
	 */
	for (;;) {
		for (;;) {
			sstat1 = inb(iobase + SSTAT1);
			if ((sstat1 & (REQINIT | PHASECHG | BUSFREE)) != 0)
				break;
			/* Wait for REQINIT.  XXX Need timeout. */
		}
		if ((sstat1 & (PHASECHG | BUSFREE)) != 0) {
			/*
			 * Target left MESSAGE IN, probably because it
			 * a) noticed our ATN signal, or
			 * b) ran out of messages.
			 */
			goto out;
		}

		/* If parity error, just dump everything on the floor. */
		if ((sstat1 & SCSIPERR) != 0) {
			sc->sc_flags |= AIC_DROP_MSGIN;
			aic_sched_msgout(sc, SEND_PARITY_ERROR);
		}

		/* Gather incoming message bytes if needed. */
		if ((sc->sc_flags & AIC_DROP_MSGIN) == 0) {
			if (n >= AIC_MAX_MSG_LEN) {
				(void) inb(iobase + SCSIDAT);
				sc->sc_flags |= AIC_DROP_MSGIN;
				aic_sched_msgout(sc, SEND_REJECT);
			} else {
				*sc->sc_imp++ = inb(iobase + SCSIDAT);
				n++;
				/*
				 * This testing is suboptimal, but most
				 * messages will be of the one byte variety, so
				 * it should not affect performance
				 * significantly.
				 */
				if (n == 1 && IS1BYTEMSG(sc->sc_imess[0]))
					break;
				if (n == 2 && IS2BYTEMSG(sc->sc_imess[0]))
					break;
				if (n >= 3 && ISEXTMSG(sc->sc_imess[0]) &&
				    n == sc->sc_imess[1] + 2)
					break;
			}
		} else
			(void) inb(iobase + SCSIDAT);

		/*
		 * If we reach this spot we're either:
		 * a) in the middle of a multi-byte message, or
		 * b) dropping bytes.
		 */
		outb(iobase + SXFRCTL0, CHEN | SPIOEN);
		/* Ack the last byte read. */
		(void) inb(iobase + SCSIDAT);
		outb(iobase + SXFRCTL0, CHEN);
		while ((inb(iobase + SCSISIG) & ACKI) != 0)
			;
	}

	AIC_MISC(("n=%d imess=0x%02x  ", n, sc->sc_imess[0]));

	/* We now have a complete message.  Parse it. */
	switch (sc->sc_state) {
		struct aic_acb *acb;
		struct scsipi_link *sc_link;
		struct aic_tinfo *ti;

	case AIC_CONNECTED:
		AIC_ASSERT(sc->sc_nexus != NULL);
		acb = sc->sc_nexus;
		ti = &sc->sc_tinfo[acb->xs->sc_link->scsipi_scsi.target];

		switch (sc->sc_imess[0]) {
		case MSG_CMDCOMPLETE:
			if (sc->sc_dleft < 0) {
				sc_link = acb->xs->sc_link;
				printf("%s: %d extra bytes from %d:%d\n",
				    sc->sc_dev.dv_xname, -sc->sc_dleft,
				    sc_link->scsipi_scsi.target, sc_link->scsipi_scsi.lun);
				acb->data_length = 0;
			}
			acb->xs->resid = acb->data_length = sc->sc_dleft;
			sc->sc_state = AIC_CMDCOMPLETE;
			break;

		case MSG_PARITY_ERROR:
			/* Resend the last message. */
			aic_sched_msgout(sc, sc->sc_lastmsg);
			break;

		case MSG_MESSAGE_REJECT:
			AIC_MISC(("message rejected %02x  ", sc->sc_lastmsg));
			switch (sc->sc_lastmsg) {
#if AIC_USE_SYNCHRONOUS + AIC_USE_WIDE
			case SEND_IDENTIFY:
				ti->flags &= ~(DO_SYNC | DO_WIDE);
				ti->period = ti->offset = 0;
				aic_setsync(sc, ti);
				ti->width = 0;
				break;
#endif
#if AIC_USE_SYNCHRONOUS
			case SEND_SDTR:
				ti->flags &= ~DO_SYNC;
				ti->period = ti->offset = 0;
				aic_setsync(sc, ti);
				break;
#endif
#if AIC_USE_WIDE
			case SEND_WDTR:
				ti->flags &= ~DO_WIDE;
				ti->width = 0;
				break;
#endif
			case SEND_INIT_DET_ERR:
				aic_sched_msgout(sc, SEND_ABORT);
				break;
			}
			break;

		case MSG_NOOP:
			break;

		case MSG_DISCONNECT:
			ti->dconns++;
			sc->sc_state = AIC_DISCONNECT;
			break;

		case MSG_SAVEDATAPOINTER:
			acb->data_addr = sc->sc_dp;
			acb->data_length = sc->sc_dleft;
			break;

		case MSG_RESTOREPOINTERS:
			sc->sc_dp = acb->data_addr;
			sc->sc_dleft = acb->data_length;
			sc->sc_cp = (u_char *)&acb->scsipi_cmd;
			sc->sc_cleft = acb->scsipi_cmd_length;
			break;

		case MSG_EXTENDED:
			switch (sc->sc_imess[2]) {
#if AIC_USE_SYNCHRONOUS
			case MSG_EXT_SDTR:
				if (sc->sc_imess[1] != 3)
					goto reject;
				ti->period = sc->sc_imess[3];
				ti->offset = sc->sc_imess[4];
				ti->flags &= ~DO_SYNC;
				if (ti->offset == 0) {
				} else if (ti->period < sc->sc_minsync ||
					   ti->period > sc->sc_maxsync ||
					   ti->offset > 8) {
					ti->period = ti->offset = 0;
					aic_sched_msgout(sc, SEND_SDTR);
				} else {
					scsi_print_addr(acb->xs->sc_link);
					printf("sync, offset %d, period %dnsec\n",
					    ti->offset, ti->period * 4);
				}
				aic_setsync(sc, ti);
				break;
#endif

#if AIC_USE_WIDE
			case MSG_EXT_WDTR:
				if (sc->sc_imess[1] != 2)
					goto reject;
				ti->width = sc->sc_imess[3];
				ti->flags &= ~DO_WIDE;
				if (ti->width == 0) {
				} else if (ti->width > AIC_MAX_WIDTH) {
					ti->width = 0;
					aic_sched_msgout(sc, SEND_WDTR);
				} else {
					scsi_print_addr(acb->xs->sc_link);
					printf("wide, width %d\n",
					    1 << (3 + ti->width));
				}
				break;
#endif

			default:
				printf("%s: unrecognized MESSAGE EXTENDED; sending REJECT\n",
				    sc->sc_dev.dv_xname);
				AIC_BREAK();
				goto reject;
			}
			break;

		default:
			printf("%s: unrecognized MESSAGE; sending REJECT\n",
			    sc->sc_dev.dv_xname);
			AIC_BREAK();
		reject:
			aic_sched_msgout(sc, SEND_REJECT);
			break;
		}
		break;

	case AIC_RESELECTED:
		if (!MSG_ISIDENTIFY(sc->sc_imess[0])) {
			printf("%s: reselect without IDENTIFY; sending DEVICE RESET\n",
			    sc->sc_dev.dv_xname);
			AIC_BREAK();
			goto reset;
		}

		(void) aic_reselect(sc, sc->sc_imess[0]);
		break;

	default:
		printf("%s: unexpected MESSAGE IN; sending DEVICE RESET\n",
		    sc->sc_dev.dv_xname);
		AIC_BREAK();
	reset:
		aic_sched_msgout(sc, SEND_DEV_RESET);
		break;

#ifdef notdef
	abort:
		aic_sched_msgout(sc, SEND_ABORT);
		break;
#endif
	}

	outb(iobase + SXFRCTL0, CHEN | SPIOEN);
	/* Ack the last message byte. */
	(void) inb(iobase + SCSIDAT);
	outb(iobase + SXFRCTL0, CHEN);
	while ((inb(iobase + SCSISIG) & ACKI) != 0)
		;

	/* Go get the next message, if any. */
	goto nextmsg;

out:
	AIC_MISC(("n=%d imess=0x%02x  ", n, sc->sc_imess[0]));
}

/*
 * Send the highest priority, scheduled message.
 */
void
aic_msgout(sc)
	register struct aic_softc *sc;
{
	int iobase = sc->sc_iobase;
#if AIC_USE_SYNCHRONOUS
	struct aic_tinfo *ti;
#endif
	u_char sstat1;
	int n;

	AIC_TRACE(("aic_msgout  "));

	/* Reset the FIFO. */
	outb(iobase + DMACNTRL0, RSTFIFO);
	/* Enable REQ/ACK protocol. */
	outb(iobase + SXFRCTL0, CHEN | SPIOEN);

	if (sc->sc_prevphase == PH_MSGOUT) {
		if (sc->sc_omp == sc->sc_omess) {
			/*
			 * This is a retransmission.
			 *
			 * We get here if the target stayed in MESSAGE OUT
			 * phase.  Section 5.1.9.2 of the SCSI 2 spec indicates
			 * that all of the previously transmitted messages must
			 * be sent again, in the same order.  Therefore, we
			 * requeue all the previously transmitted messages, and
			 * start again from the top.  Our simple priority
			 * scheme keeps the messages in the right order.
			 */
			AIC_MISC(("retransmitting  "));
			sc->sc_msgpriq |= sc->sc_msgoutq;
			/*
			 * Set ATN.  If we're just sending a trivial 1-byte
			 * message, we'll clear ATN later on anyway.
			 */
			outb(iobase + SCSISIG, PH_MSGOUT | ATNO);
		} else {
			/* This is a continuation of the previous message. */
			n = sc->sc_omp - sc->sc_omess;
			goto nextbyte;
		}
	}

	/* No messages transmitted so far. */
	sc->sc_msgoutq = 0;
	sc->sc_lastmsg = 0;

nextmsg:
	/* Pick up highest priority message. */
	sc->sc_currmsg = sc->sc_msgpriq & -sc->sc_msgpriq;
	sc->sc_msgpriq &= ~sc->sc_currmsg;
	sc->sc_msgoutq |= sc->sc_currmsg;

	/* Build the outgoing message data. */
	switch (sc->sc_currmsg) {
	case SEND_IDENTIFY:
		AIC_ASSERT(sc->sc_nexus != NULL);
		sc->sc_omess[0] =
		    MSG_IDENTIFY(sc->sc_nexus->xs->sc_link->scsipi_scsi.lun, 1);
		n = 1;
		break;

#if AIC_USE_SYNCHRONOUS
	case SEND_SDTR:
		AIC_ASSERT(sc->sc_nexus != NULL);
		ti = &sc->sc_tinfo[sc->sc_nexus->xs->sc_link->scsipi_scsi.target];
		sc->sc_omess[4] = MSG_EXTENDED;
		sc->sc_omess[3] = 3;
		sc->sc_omess[2] = MSG_EXT_SDTR;
		sc->sc_omess[1] = ti->period >> 2;
		sc->sc_omess[0] = ti->offset;
		n = 5;
		break;
#endif

#if AIC_USE_WIDE
	case SEND_WDTR:
		AIC_ASSERT(sc->sc_nexus != NULL);
		ti = &sc->sc_tinfo[sc->sc_nexus->xs->sc_link->scsipi_scsi.target];
		sc->sc_omess[3] = MSG_EXTENDED;
		sc->sc_omess[2] = 2;
		sc->sc_omess[1] = MSG_EXT_WDTR;
		sc->sc_omess[0] = ti->width;
		n = 4;
		break;
#endif

	case SEND_DEV_RESET:
		sc->sc_flags |= AIC_ABORTING;
		sc->sc_omess[0] = MSG_BUS_DEV_RESET;
		n = 1;
		break;

	case SEND_REJECT:
		sc->sc_omess[0] = MSG_MESSAGE_REJECT;
		n = 1;
		break;

	case SEND_PARITY_ERROR:
		sc->sc_omess[0] = MSG_PARITY_ERROR;
		n = 1;
		break;

	case SEND_INIT_DET_ERR:
		sc->sc_omess[0] = MSG_INITIATOR_DET_ERR;
		n = 1;
		break;

	case SEND_ABORT:
		sc->sc_flags |= AIC_ABORTING;
		sc->sc_omess[0] = MSG_ABORT;
		n = 1;
		break;

	default:
		printf("%s: unexpected MESSAGE OUT; sending NOOP\n",
		    sc->sc_dev.dv_xname);
		AIC_BREAK();
		sc->sc_omess[0] = MSG_NOOP;
		n = 1;
		break;
	}
	sc->sc_omp = &sc->sc_omess[n];

nextbyte:
	/* Send message bytes. */
	for (;;) {
		for (;;) {
			sstat1 = inb(iobase + SSTAT1);
			if ((sstat1 & (REQINIT | PHASECHG | BUSFREE)) != 0)
				break;
			/* Wait for REQINIT.  XXX Need timeout. */
		}
		if ((sstat1 & (PHASECHG | BUSFREE)) != 0) {
			/*
			 * Target left MESSAGE OUT, possibly to reject
			 * our message.
			 *
			 * If this is the last message being sent, then we
			 * deassert ATN, since either the target is going to
			 * ignore this message, or it's going to ask for a
			 * retransmission via MESSAGE PARITY ERROR (in which
			 * case we reassert ATN anyway).
			 */
			if (sc->sc_msgpriq == 0)
				outb(iobase + CLRSINT1, CLRATNO);
			goto out;
		}

		/* Clear ATN before last byte if this is the last message. */
		if (n == 1 && sc->sc_msgpriq == 0)
			outb(iobase + CLRSINT1, CLRATNO);
		/* Send message byte. */
		outb(iobase + SCSIDAT, *--sc->sc_omp);
		--n;
		/* Keep track of the last message we've sent any bytes of. */
		sc->sc_lastmsg = sc->sc_currmsg;
		/* Wait for ACK to be negated.  XXX Need timeout. */
		while ((inb(iobase + SCSISIG) & ACKI) != 0)
			;

		if (n == 0)
			break;
	}

	/* We get here only if the entire message has been transmitted. */
	if (sc->sc_msgpriq != 0) {
		/* There are more outgoing messages. */
		goto nextmsg;
	}

	/*
	 * The last message has been transmitted.  We need to remember the last
	 * message transmitted (in case the target switches to MESSAGE IN phase
	 * and sends a MESSAGE REJECT), and the list of messages transmitted
	 * this time around (in case the target stays in MESSAGE OUT phase to
	 * request a retransmit).
	 */

out:
	/* Disable REQ/ACK protocol. */
	outb(iobase + SXFRCTL0, CHEN);
}

/* aic_dataout_pio: perform a data transfer using the FIFO datapath in the aic6360
 * Precondition: The SCSI bus should be in the DOUT phase, with REQ asserted
 * and ACK deasserted (i.e. waiting for a data byte)
 * This new revision has been optimized (I tried) to make the common case fast,
 * and the rarer cases (as a result) somewhat more comlex
 */
int
aic_dataout_pio(sc, p, n)
	register struct aic_softc *sc;
	u_char *p;
	int n;
{
	int iobase = sc->sc_iobase;
	register u_char dmastat = 0;
	int out = 0;
#define DOUTAMOUNT 128		/* Full FIFO */

	AIC_MISC(("%02x%02x  ", inb(iobase + FIFOSTAT), inb(iobase + SSTAT2)));

	/* Clear host FIFO and counter. */
	outb(iobase + DMACNTRL0, RSTFIFO | WRITE);
	/* Enable FIFOs. */
	outb(iobase + DMACNTRL0, ENDMA | DWORDPIO | WRITE);
	outb(iobase + SXFRCTL0, SCSIEN | DMAEN | CHEN);

	/* Turn off ENREQINIT for now. */
	outb(iobase + SIMODE1,
	    ENSCSIRST | ENSCSIPERR | ENBUSFREE | ENPHASECHG);

	/* I have tried to make the main loop as tight as possible.  This
	 * means that some of the code following the loop is a bit more
	 * complex than otherwise.
	 */
	while (n > 0) {
		for (;;) {
			dmastat = inb(iobase + DMASTAT);
			if ((dmastat & (DFIFOEMP | INTSTAT)) != 0)
				break;
		}

		if ((dmastat & INTSTAT) != 0)
			goto phasechange;

		if (n >= DOUTAMOUNT) {
			n -= DOUTAMOUNT;
			out += DOUTAMOUNT;

#if AIC_USE_DWORDS
			outsl(iobase + DMADATALONG, p, DOUTAMOUNT >> 2);
#else
			outsw(iobase + DMADATA, p, DOUTAMOUNT >> 1);
#endif

			p += DOUTAMOUNT;
		} else {
			register int xfer;

			xfer = n;
			AIC_MISC(("%d> ", xfer));

			n -= xfer;
			out += xfer;

#if AIC_USE_DWORDS
			if (xfer >= 12) {
				outsl(iobase + DMADATALONG, p, xfer >> 2);
				p += xfer & ~3;
				xfer &= 3;
			}
#else
			if (xfer >= 8) {
				outsw(iobase + DMADATA, p, xfer >> 1);
				p += xfer & ~1;
				xfer &= 1;
			}
#endif

			if (xfer > 0) {
				outb(iobase + DMACNTRL0, ENDMA | B8MODE | WRITE);
				outsb(iobase + DMADATA, p, xfer);
				p += xfer;
				outb(iobase + DMACNTRL0, ENDMA | DWORDPIO | WRITE);
			}
		}
	}

	if (out == 0) {
		outb(iobase + SXFRCTL1, BITBUCKET);
		for (;;) {
			if ((inb(iobase + DMASTAT) & INTSTAT) != 0)
				break;
		}
		outb(iobase + SXFRCTL1, 0);
		AIC_MISC(("extra data  "));
	} else {
		/* See the bytes off chip */
		for (;;) {
			dmastat = inb(iobase + DMASTAT);
			if ((dmastat & INTSTAT) != 0)
				goto phasechange;
			if ((dmastat & DFIFOEMP) != 0 &&
			    (inb(iobase + SSTAT2) & SEMPTY) != 0)
				break;
		}
	}

phasechange:
	if ((dmastat & INTSTAT) != 0) {
		/* Some sort of phase change. */
		int amount;

		/* Stop transfers, do some accounting */
		amount = inb(iobase + FIFOSTAT) + (inb(iobase + SSTAT2) & 15);
		if (amount > 0) {
			out -= amount;
			outb(iobase + DMACNTRL0, RSTFIFO | WRITE);
			outb(iobase + SXFRCTL0, CHEN | CLRCH);
			AIC_MISC(("+%d ", amount));
		}
	}

	/* Turn on ENREQINIT again. */
	outb(iobase + SIMODE1,
	    ENSCSIRST | ENSCSIPERR | ENBUSFREE | ENREQINIT | ENPHASECHG);

	/* Stop the FIFO data path. */
	outb(iobase + SXFRCTL0, CHEN);
	outb(iobase + DMACNTRL0, 0);

	return out;
}

/* aic_datain_pio: perform data transfers using the FIFO datapath in the aic6360
 * Precondition: The SCSI bus should be in the DIN phase, with REQ asserted
 * and ACK deasserted (i.e. at least one byte is ready).
 * For now, uses a pretty dumb algorithm, hangs around until all data has been
 * transferred.  This, is OK for fast targets, but not so smart for slow
 * targets which don't disconnect or for huge transfers.
 */
int
aic_datain_pio(sc, p, n)
	register struct aic_softc *sc;
	u_char *p;
	int n;
{
	int iobase = sc->sc_iobase;
	register u_char dmastat;
	int in = 0;
#define DINAMOUNT 128		/* Full FIFO */

	AIC_MISC(("%02x%02x  ", inb(iobase + FIFOSTAT), inb(iobase + SSTAT2)));

	/* Clear host FIFO and counter. */
	outb(iobase + DMACNTRL0, RSTFIFO);
	/* Enable FIFOs. */
	outb(iobase + DMACNTRL0, ENDMA | DWORDPIO);
	outb(iobase + SXFRCTL0, SCSIEN | DMAEN | CHEN);

	/* Turn off ENREQINIT for now. */
	outb(iobase + SIMODE1,
	    ENSCSIRST | ENSCSIPERR | ENBUSFREE | ENPHASECHG);

	/* We leave this loop if one or more of the following is true:
	 * a) phase != PH_DATAIN && FIFOs are empty
	 * b) SCSIRSTI is set (a reset has occurred) or busfree is detected.
	 */
	while (n > 0) {
		/* Wait for fifo half full or phase mismatch */
		for (;;) {
			dmastat = inb(iobase + DMASTAT);
			if ((dmastat & (DFIFOFULL | INTSTAT)) != 0)
				break;
		}

		if ((dmastat & DFIFOFULL) != 0) {
			n -= DINAMOUNT;
			in += DINAMOUNT;

#if AIC_USE_DWORDS
			insl(iobase + DMADATALONG, p, DINAMOUNT >> 2);
#else
			insw(iobase + DMADATA, p, DINAMOUNT >> 1);
#endif

			p += DINAMOUNT;
		} else {
			register int xfer;

			xfer = min(inb(iobase + FIFOSTAT), n);
			AIC_MISC((">%d ", xfer));

			n -= xfer;
			in += xfer;

#if AIC_USE_DWORDS
			if (xfer >= 12) {
				insl(iobase + DMADATALONG, p, xfer >> 2);
				p += xfer & ~3;
				xfer &= 3;
			}
#else
			if (xfer >= 8) {
				insw(iobase + DMADATA, p, xfer >> 1);
				p += xfer & ~1;
				xfer &= 1;
			}
#endif

			if (xfer > 0) {
				outb(iobase + DMACNTRL0, ENDMA | B8MODE);
				insb(iobase + DMADATA, p, xfer);
				p += xfer;
				outb(iobase + DMACNTRL0, ENDMA | DWORDPIO);
			}
		}

		if ((dmastat & INTSTAT) != 0)
			goto phasechange;
	}

	/* Some SCSI-devices are rude enough to transfer more data than what
	 * was requested, e.g. 2048 bytes from a CD-ROM instead of the
	 * requested 512.  Test for progress, i.e. real transfers.  If no real
	 * transfers have been performed (n is probably already zero) and the
	 * FIFO is not empty, waste some bytes....
	 */
	if (in == 0) {
		outb(iobase + SXFRCTL1, BITBUCKET);
		for (;;) {
			if ((inb(iobase + DMASTAT) & INTSTAT) != 0)
				break;
		}
		outb(iobase + SXFRCTL1, 0);
		AIC_MISC(("extra data  "));
	}

phasechange:
	/* Turn on ENREQINIT again. */
	outb(iobase + SIMODE1,
	    ENSCSIRST | ENSCSIPERR | ENBUSFREE | ENREQINIT | ENPHASECHG);

	/* Stop the FIFO data path. */
	outb(iobase + SXFRCTL0, CHEN);
	outb(iobase + DMACNTRL0, 0);

	return in;
}

/*
 * This is the workhorse routine of the driver.
 * Deficiencies (for now):
 * 1) always uses programmed I/O
 */
int
aicintr(arg)
	void *arg;
{
	register struct aic_softc *sc = arg;
	int iobase = sc->sc_iobase;
	u_char sstat0, sstat1;
	register struct aic_acb *acb;
	register struct scsipi_link *sc_link;
	struct aic_tinfo *ti;
	int n;

	/*
	 * Clear INTEN.  We enable it again before returning.  This makes the
	 * interrupt esssentially level-triggered.
	 */
	outb(iobase + DMACNTRL0, 0);

	AIC_TRACE(("aicintr  "));

loop:
	/*
	 * First check for abnormal conditions, such as reset.
	 */
	sstat1 = inb(iobase + SSTAT1);
	AIC_MISC(("sstat1:0x%02x ", sstat1));

	if ((sstat1 & SCSIRSTI) != 0) {
		printf("%s: SCSI bus reset\n", sc->sc_dev.dv_xname);
		goto reset;
	}

	/*
	 * Check for less serious errors.
	 */
	if ((sstat1 & SCSIPERR) != 0) {
		printf("%s: SCSI bus parity error\n", sc->sc_dev.dv_xname);
		outb(iobase + CLRSINT1, CLRSCSIPERR);
		if (sc->sc_prevphase == PH_MSGIN) {
			sc->sc_flags |= AIC_DROP_MSGIN;
			aic_sched_msgout(sc, SEND_PARITY_ERROR);
		} else
			aic_sched_msgout(sc, SEND_INIT_DET_ERR);
	}

	/*
	 * If we're not already busy doing something test for the following
	 * conditions:
	 * 1) We have been reselected by something
	 * 2) We have selected something successfully
	 * 3) Our selection process has timed out
	 * 4) This is really a bus free interrupt just to get a new command
	 *    going?
	 * 5) Spurious interrupt?
	 */
	switch (sc->sc_state) {
	case AIC_IDLE:
	case AIC_SELECTING:
		sstat0 = inb(iobase + SSTAT0);
		AIC_MISC(("sstat0:0x%02x ", sstat0));

		if ((sstat0 & TARGET) != 0) {
			/*
			 * We don't currently support target mode.
			 */
			printf("%s: target mode selected; going to BUS FREE\n",
			    sc->sc_dev.dv_xname);
			outb(iobase + SCSISIG, 0);

			goto sched;
		} else if ((sstat0 & SELDI) != 0) {
			AIC_MISC(("reselected  "));

			/*
			 * If we're trying to select a target ourselves,
			 * push our command back into the ready list.
			 */
			if (sc->sc_state == AIC_SELECTING) {
				AIC_MISC(("backoff selector  "));
				AIC_ASSERT(sc->sc_nexus != NULL);
				acb = sc->sc_nexus;
				sc->sc_nexus = NULL;
				TAILQ_INSERT_HEAD(&sc->ready_list, acb, chain);
			}

			/* Save reselection ID. */
			sc->sc_selid = inb(iobase + SELID);

			sc->sc_state = AIC_RESELECTED;
		} else if ((sstat0 & SELDO) != 0) {
			AIC_MISC(("selected  "));

			/* We have selected a target. Things to do:
			 * a) Determine what message(s) to send.
			 * b) Verify that we're still selecting the target.
			 * c) Mark device as busy.
			 */
			if (sc->sc_state != AIC_SELECTING) {
				printf("%s: selection out while idle; resetting\n",
				    sc->sc_dev.dv_xname);
				AIC_BREAK();
				goto reset;
			}
			AIC_ASSERT(sc->sc_nexus != NULL);
			acb = sc->sc_nexus;
			sc_link = acb->xs->sc_link;
			ti = &sc->sc_tinfo[sc_link->scsipi_scsi.target];

			sc->sc_msgpriq = SEND_IDENTIFY;
			if (acb->flags & ACB_RESET)
				sc->sc_msgpriq |= SEND_DEV_RESET;
			else if (acb->flags & ACB_ABORT)
				sc->sc_msgpriq |= SEND_ABORT;
			else {
#if AIC_USE_SYNCHRONOUS
				if ((ti->flags & DO_SYNC) != 0)
					sc->sc_msgpriq |= SEND_SDTR;
#endif
#if AIC_USE_WIDE
				if ((ti->flags & DO_WIDE) != 0)
					sc->sc_msgpriq |= SEND_WDTR;
#endif
			}

			acb->flags |= ACB_NEXUS;
			ti->lubusy |= (1 << sc_link->scsipi_scsi.lun);

			/* Do an implicit RESTORE POINTERS. */
			sc->sc_dp = acb->data_addr;
			sc->sc_dleft = acb->data_length;
			sc->sc_cp = (u_char *)&acb->scsipi_cmd;
			sc->sc_cleft = acb->scsipi_cmd_length;

			/* On our first connection, schedule a timeout. */
			if ((acb->xs->flags & SCSI_POLL) == 0)
				timeout(aic_timeout, acb, (acb->timeout * hz) / 1000);

			sc->sc_state = AIC_CONNECTED;
		} else if ((sstat1 & SELTO) != 0) {
			AIC_MISC(("selection timeout  "));

			if (sc->sc_state != AIC_SELECTING) {
				printf("%s: selection timeout while idle; resetting\n",
				    sc->sc_dev.dv_xname);
				AIC_BREAK();
				goto reset;
			}
			AIC_ASSERT(sc->sc_nexus != NULL);
			acb = sc->sc_nexus;

			outb(iobase + SXFRCTL1, 0);
			outb(iobase + SCSISEQ, ENRESELI);
			outb(iobase + CLRSINT1, CLRSELTIMO);
			delay(250);

			acb->xs->error = XS_SELTIMEOUT;
			goto finish;
		} else {
			if (sc->sc_state != AIC_IDLE) {
				printf("%s: BUS FREE while not idle; state=%d\n",
				    sc->sc_dev.dv_xname, sc->sc_state);
				AIC_BREAK();
				goto out;
			}

			goto sched;
		}

		/*
		 * Turn off selection stuff, and prepare to catch bus free
		 * interrupts, parity errors, and phase changes.
		 */
		outb(iobase + SXFRCTL0, CHEN | CLRSTCNT | CLRCH);
		outb(iobase + SXFRCTL1, 0);
		outb(iobase + SCSISEQ, ENAUTOATNP);
		outb(iobase + CLRSINT0, CLRSELDI | CLRSELDO);
		outb(iobase + CLRSINT1, CLRBUSFREE | CLRPHASECHG);
		outb(iobase + SIMODE0, 0);
		outb(iobase + SIMODE1,
		    ENSCSIRST | ENSCSIPERR | ENBUSFREE | ENREQINIT | ENPHASECHG);

		sc->sc_flags = 0;
		sc->sc_prevphase = PH_INVALID;
		goto dophase;
	}

	if ((sstat1 & BUSFREE) != 0) {
		/* We've gone to BUS FREE phase. */
		outb(iobase + CLRSINT1, CLRBUSFREE | CLRPHASECHG);

		switch (sc->sc_state) {
		case AIC_RESELECTED:
			goto sched;

		case AIC_CONNECTED:
			AIC_ASSERT(sc->sc_nexus != NULL);
			acb = sc->sc_nexus;

#if AIC_USE_SYNCHRONOUS + AIC_USE_WIDE
			if (sc->sc_prevphase == PH_MSGOUT) {
				/*
				 * If the target went to BUS FREE phase during
				 * or immediately after sending a SDTR or WDTR
				 * message, disable negotiation.
				 */
				sc_link = acb->xs->sc_link;
				ti = &sc->sc_tinfo[sc_link->scsipi_scsi.target];
				switch (sc->sc_lastmsg) {
#if AIC_USE_SYNCHRONOUS
				case SEND_SDTR:
					ti->flags &= ~DO_SYNC;
					ti->period = ti->offset = 0;
					break;
#endif
#if AIC_USE_WIDE
				case SEND_WDTR:
					ti->flags &= ~DO_WIDE;
					ti->width = 0;
					break;
#endif
				}
			}
#endif

			if ((sc->sc_flags & AIC_ABORTING) == 0) {
				/*
				 * Section 5.1.1 of the SCSI 2 spec suggests
				 * issuing a REQUEST SENSE following an
				 * unexpected disconnect.  Some devices go into
				 * a contingent allegiance condition when
				 * disconnecting, and this is necessary to
				 * clean up their state.
				 */
				printf("%s: unexpected disconnect; sending REQUEST SENSE\n",
				    sc->sc_dev.dv_xname);
				AIC_BREAK();
				aic_sense(sc, acb);
				goto out;
			}

			acb->xs->error = XS_DRIVER_STUFFUP;
			goto finish;

		case AIC_DISCONNECT:
			AIC_ASSERT(sc->sc_nexus != NULL);
			acb = sc->sc_nexus;
#if 1 /* XXXX */
			acb->data_addr = sc->sc_dp;
			acb->data_length = sc->sc_dleft;
#endif
			TAILQ_INSERT_HEAD(&sc->nexus_list, acb, chain);
			sc->sc_nexus = NULL;
			goto sched;

		case AIC_CMDCOMPLETE:
			AIC_ASSERT(sc->sc_nexus != NULL);
			acb = sc->sc_nexus;
			goto finish;
		}
	}

	outb(iobase + CLRSINT1, CLRPHASECHG);

dophase:
	if ((sstat1 & REQINIT) == 0) {
		/* Wait for REQINIT. */
		goto out;
	}

	sc->sc_phase = inb(iobase + SCSISIG) & PH_MASK;
	outb(iobase + SCSISIG, sc->sc_phase);

	switch (sc->sc_phase) {
	case PH_MSGOUT:
		if (sc->sc_state != AIC_CONNECTED &&
		    sc->sc_state != AIC_RESELECTED)
			break;
		aic_msgout(sc);
		sc->sc_prevphase = PH_MSGOUT;
		goto loop;

	case PH_MSGIN:
		if (sc->sc_state != AIC_CONNECTED &&
		    sc->sc_state != AIC_RESELECTED)
			break;
		aic_msgin(sc);
		sc->sc_prevphase = PH_MSGIN;
		goto loop;

	case PH_CMD:
		if (sc->sc_state != AIC_CONNECTED)
			break;
#if AIC_DEBUG
		if ((aic_debug & AIC_SHOWMISC) != 0) {
			AIC_ASSERT(sc->sc_nexus != NULL);
			acb = sc->sc_nexus;
			printf("cmd=0x%02x+%d ",
			    acb->scsipi_cmd.opcode, acb->scsipi_cmd_length-1);
		}
#endif
		n = aic_dataout_pio(sc, sc->sc_cp, sc->sc_cleft);
		sc->sc_cp += n;
		sc->sc_cleft -= n;
		sc->sc_prevphase = PH_CMD;
		goto loop;

	case PH_DATAOUT:
		if (sc->sc_state != AIC_CONNECTED)
			break;
		AIC_MISC(("dataout %d ", sc->sc_dleft));
		n = aic_dataout_pio(sc, sc->sc_dp, sc->sc_dleft);
		sc->sc_dp += n;
		sc->sc_dleft -= n;
		sc->sc_prevphase = PH_DATAOUT;
		goto loop;

	case PH_DATAIN:
		if (sc->sc_state != AIC_CONNECTED)
			break;
		AIC_MISC(("datain %d ", sc->sc_dleft));
		n = aic_datain_pio(sc, sc->sc_dp, sc->sc_dleft);
		sc->sc_dp += n;
		sc->sc_dleft -= n;
		sc->sc_prevphase = PH_DATAIN;
		goto loop;

	case PH_STAT:
		if (sc->sc_state != AIC_CONNECTED)
			break;
		AIC_ASSERT(sc->sc_nexus != NULL);
		acb = sc->sc_nexus;
		outb(iobase + SXFRCTL0, CHEN | SPIOEN);
		acb->target_stat = inb(iobase + SCSIDAT);
		outb(iobase + SXFRCTL0, CHEN);
		AIC_MISC(("target_stat=0x%02x  ", acb->target_stat));
		sc->sc_prevphase = PH_STAT;
		goto loop;
	}

	printf("%s: unexpected bus phase; resetting\n", sc->sc_dev.dv_xname);
	AIC_BREAK();
reset:
	aic_init(sc);
	return 1;

finish:
	untimeout(aic_timeout, acb);
	aic_done(sc, acb);
	goto out;

sched:
	sc->sc_state = AIC_IDLE;
	aic_sched(sc);
	goto out;

out:
	outb(iobase + DMACNTRL0, INTEN);
	return 1;
}

void
aic_abort(sc, acb)
	struct aic_softc *sc;
	struct aic_acb *acb;
{

	/* 2 secs for the abort */
	acb->timeout = AIC_ABORT_TIMEOUT;
	acb->flags |= ACB_ABORT;

	if (acb == sc->sc_nexus) {
		/*
		 * If we're still selecting, the message will be scheduled
		 * after selection is complete.
		 */
		if (sc->sc_state == AIC_CONNECTED)
			aic_sched_msgout(sc, SEND_ABORT);
	} else {
		aic_dequeue(sc, acb);
		TAILQ_INSERT_HEAD(&sc->ready_list, acb, chain);
		if (sc->sc_state == AIC_IDLE)
			aic_sched(sc);
	}
}

void
aic_timeout(arg)
	void *arg;
{
	struct aic_acb *acb = arg;
	struct scsipi_xfer *xs = acb->xs;
	struct scsipi_link *sc_link = xs->sc_link;
	struct aic_softc *sc = sc_link->adapter_softc;
	int s;

	scsi_print_addr(sc_link);
	printf("timed out");

	s = splbio();

	if (acb->flags & ACB_ABORT) {
		/* abort timed out */
		printf(" AGAIN\n");
		/* XXX Must reset! */
	} else {
		/* abort the operation that has timed out */
		printf("\n");
		acb->xs->error = XS_TIMEOUT;
		aic_abort(sc, acb);
	}

	splx(s);
}

#ifdef AIC_DEBUG
/*
 * The following functions are mostly used for debugging purposes, either
 * directly called from the driver or from the kernel debugger.
 */

void
aic_show_scsi_cmd(acb)
	struct aic_acb *acb;
{
	u_char  *b = (u_char *)&acb->scsipi_cmd;
	struct scsipi_link *sc_link = acb->xs->sc_link;
	int i;

	scsi_print_addr(sc_link);
	if ((acb->xs->flags & SCSI_RESET) == 0) {
		for (i = 0; i < acb->scsipi_cmd_length; i++) {
			if (i)
				printf(",");
			printf("%x", b[i]);
		}
		printf("\n");
	} else
		printf("RESET\n");
}

void
aic_print_acb(acb)
	struct aic_acb *acb;
{

	printf("acb@%p xs=%p flags=%x", acb, acb->xs, acb->flags);
	printf(" dp=%p dleft=%d target_stat=%x\n",
	       acb->data_addr, acb->data_length, acb->target_stat);
	aic_show_scsi_cmd(acb);
}

void
aic_print_active_acb()
{
	struct aic_acb *acb;
	struct aic_softc *sc = aic_cd.cd_devs[0];

	printf("ready list:\n");
	for (acb = sc->ready_list.tqh_first; acb != NULL;
	    acb = acb->chain.tqe_next)
		aic_print_acb(acb);
	printf("nexus:\n");
	if (sc->sc_nexus != NULL)
		aic_print_acb(sc->sc_nexus);
	printf("nexus list:\n");
	for (acb = sc->nexus_list.tqh_first; acb != NULL;
	    acb = acb->chain.tqe_next)
		aic_print_acb(acb);
}

void
aic_dump6360(sc)
	struct aic_softc *sc;
{
	int iobase = sc->sc_iobase;

	printf("aic6360: SCSISEQ=%x SXFRCTL0=%x SXFRCTL1=%x SCSISIG=%x\n",
	    inb(iobase + SCSISEQ), inb(iobase + SXFRCTL0),
	    inb(iobase + SXFRCTL1), inb(iobase + SCSISIG));
	printf("         SSTAT0=%x SSTAT1=%x SSTAT2=%x SSTAT3=%x SSTAT4=%x\n",
	    inb(iobase + SSTAT0), inb(iobase + SSTAT1), inb(iobase + SSTAT2),
	    inb(iobase + SSTAT3), inb(iobase + SSTAT4));
	printf("         SIMODE0=%x SIMODE1=%x DMACNTRL0=%x DMACNTRL1=%x DMASTAT=%x\n",
	    inb(iobase + SIMODE0), inb(iobase + SIMODE1),
	    inb(iobase + DMACNTRL0), inb(iobase + DMACNTRL1),
	    inb(iobase + DMASTAT));
	printf("         FIFOSTAT=%d SCSIBUS=0x%x\n",
	    inb(iobase + FIFOSTAT), inb(iobase + SCSIBUS));
}

void
aic_dump_driver(sc)
	struct aic_softc *sc;
{
	struct aic_tinfo *ti;
	int i;

	printf("nexus=%p prevphase=%x\n", sc->sc_nexus, sc->sc_prevphase);
	printf("state=%x msgin=%x msgpriq=%x msgoutq=%x lastmsg=%x currmsg=%x\n",
	    sc->sc_state, sc->sc_imess[0],
	    sc->sc_msgpriq, sc->sc_msgoutq, sc->sc_lastmsg, sc->sc_currmsg);
	for (i = 0; i < 7; i++) {
		ti = &sc->sc_tinfo[i];
		printf("tinfo%d: %d cmds %d disconnects %d timeouts",
		    i, ti->cmds, ti->dconns, ti->touts);
		printf(" %d senses flags=%x\n", ti->senses, ti->flags);
	}
}
#endif
