/*	$NetBSD: wds.c,v 1.7.4.2 1996/12/10 06:07:57 mycroft Exp $	*/

#undef	WDSDIAG
#define	integrate

/*
 * XXX
 * sense data
 * aborts
 * resets
 */

/*
 * Copyright (c) 1994, 1995 Julian Highfield.  All rights reserved.
 * Portions copyright (c) 1994, 1996 Charles M. Hannum.  All rights reserved.
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
 *	This product includes software developed by Julian Highfield.
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

/*
 * This driver is for the WD7000 family of SCSI controllers:
 *   the WD7000-ASC, a bus-mastering DMA controller,
 *   the WD7000-FASST2, an -ASC with new firmware and scatter-gather,
 *   and the WD7000-ASE, which was custom manufactured for Apollo
 *      workstations and seems to include an -ASC as well as floppy
 *      and ESDI interfaces.
 *
 * Loosely based on Theo Deraadt's unfinished attempt.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>

#include <machine/intr.h>
#include <machine/pio.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>
#include <dev/isa/wdsreg.h>

#ifndef DDB
#define Debugger() panic("should call debugger here (wds.c)")
#endif /* ! DDB */

#define WDS_MBX_SIZE	16

#define WDS_SCB_MAX	32
#define	SCB_HASH_SIZE	32	/* hash table size for phystokv */
#define	SCB_HASH_SHIFT	9
#define	SCB_HASH(x)	((((long)(x))>>SCB_HASH_SHIFT) & (SCB_HASH_SIZE - 1))

#define	wds_nextmbx(wmb, mbx, mbio) \
	if ((wmb) == &(mbx)->mbio[WDS_MBX_SIZE - 1])	\
		(wmb) = &(mbx)->mbio[0];		\
	else						\
		(wmb)++;

struct wds_mbx {
	struct wds_mbx_out mbo[WDS_MBX_SIZE];
	struct wds_mbx_in mbi[WDS_MBX_SIZE];
	struct wds_mbx_out *cmbo;	/* Collection Mail Box out */
	struct wds_mbx_out *tmbo;	/* Target Mail Box out */
	struct wds_mbx_in *tmbi;	/* Target Mail Box in */
};

#define	KVTOPHYS(x)	vtophys(x)

struct wds_softc {
	struct device sc_dev;
	struct isadev sc_id;
	void *sc_ih;

	int sc_iobase;
	int sc_irq, sc_drq;

	int sc_revision;

	struct wds_mbx sc_mbx;
#define	wmbx	(&sc->sc_mbx)
	struct wds_scb *sc_scbhash[SCB_HASH_SIZE];
	TAILQ_HEAD(, wds_scb) sc_free_scb, sc_waiting_scb;
	int sc_numscbs, sc_mbofull;
	int sc_scsi_dev;
	struct scsi_link sc_link;	/* prototype for subdevs */
};

/* Define the bounce buffer length... */
#define BUFLEN (64*1024)
/* ..and how many there are. One per device! Non-FASST boards need these. */
#define BUFCNT 8
/* The macro for deciding whether the board needs a buffer. */
#define NEEDBUFFER(sc)	(sc->sc_revision < 0x800)

struct wds_buf {
	u_char data[BUFLEN];
	int    busy;
	TAILQ_ENTRY(wds_buf) chain;
} wds_buffer[BUFCNT];

TAILQ_HEAD(, wds_buf) wds_free_buffer;

integrate void    wds_wait __P((int, int, int));
int     wds_cmd __P((int, u_char *, int));
integrate void wds_finish_scbs __P((struct wds_softc *));
int     wdsintr __P((void *));
integrate void wds_reset_scb __P((struct wds_softc *, struct wds_scb *));
void    wds_free_scb __P((struct wds_softc *, struct wds_scb *));
void	wds_free_buf __P((struct wds_softc *, struct wds_buf *));
integrate void wds_init_scb __P((struct wds_softc *, struct wds_scb *));
struct	wds_scb *wds_get_scb __P((struct wds_softc *, int, int));
struct	wds_buf *wds_get_buf __P((struct wds_softc *, int));
struct	wds_scb *wds_scb_phys_kv __P((struct wds_softc *, u_long));
void	wds_queue_scb __P((struct wds_softc *, struct wds_scb *));
void	wds_collect_mbo __P((struct wds_softc *));
void	wds_start_scbs __P((struct wds_softc *));
void    wds_done __P((struct wds_softc *, struct wds_scb *, u_char));
int	wds_find __P((struct isa_attach_args *, struct wds_softc *));
void	wds_init __P((struct wds_softc *));
void	wds_inquire_setup_information __P((struct wds_softc *));
void    wdsminphys __P((struct buf *));
int     wds_scsi_cmd __P((struct scsi_xfer *));
void	wds_sense  __P((struct wds_softc *, struct wds_scb *));
int	wds_poll __P((struct wds_softc *, struct scsi_xfer *, int));
int	wds_ipoll __P((struct wds_softc *, struct wds_scb *, int));
void	wds_timeout __P((void *));

struct scsi_adapter wds_switch = {
	wds_scsi_cmd,
	wdsminphys,
	0,
	0,
};

/* the below structure is so we have a default dev struct for our link struct */
struct scsi_device wds_dev = {
	NULL,			/* Use default error handler */
	NULL,			/* have a queue, served by this */
	NULL,			/* have no async handler */
	NULL,			/* Use default 'done' routine */
};

int	wdsprobe __P((struct device *, void *, void *));
void	wdsattach __P((struct device *, struct device *, void *));
int	wdsprint __P((void *, char *));

struct cfattach wds_ca = {
	sizeof(struct wds_softc), wdsprobe, wdsattach
};

struct cfdriver wds_cd = {
	NULL, "wds", DV_DULL
};

#define	WDS_ABORT_TIMEOUT	2000	/* time to wait for abort (mSec) */

integrate void
wds_wait(port, mask, val)
	int port;
	int mask;
	int val;
{

	while ((inb(port) & mask) != val)
		;
}

/*
 * Write a command to the board's I/O ports.
 */
int
wds_cmd(iobase, ibuf, icnt)
	int iobase;
	u_char *ibuf;
	int icnt;
{
	u_char c;

	wds_wait(iobase + WDS_STAT, WDSS_RDY, WDSS_RDY);

	while (icnt--) {
		outb(iobase + WDS_CMD, *ibuf++);
		wds_wait(iobase + WDS_STAT, WDSS_RDY, WDSS_RDY);
		c = inb(iobase + WDS_STAT);
		if (c & WDSS_REJ)
			return 1;
	}

	return 0;
}

/*
 * Check for the presence of a WD7000 SCSI controller.
 */
int
wdsprobe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	register struct isa_attach_args *ia = aux;

#ifdef NEWCONFIG
	if (ia->ia_iobase == IOBASEUNK)
		return 0;
#endif

	/* See if there is a unit at this location. */
	if (wds_find(ia, NULL) != 0)
		return 0;

	ia->ia_msize = 0;
	ia->ia_iosize = 8;
	return 1;
}

int
wdsprint(aux, name)
	void *aux;
	char *name;
{

	if (name != NULL)
		printf("%s: scsibus ", name);
	return UNCONF;
}

/*
 * Attach all available units.
 */
void
wdsattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	struct wds_softc *sc = (void *)self;

	if (wds_find(ia, sc) != 0)
		panic("wdsattach: wds_find of %s failed", self->dv_xname);
	sc->sc_iobase = ia->ia_iobase;

	wds_init(sc);

	if (sc->sc_drq != DRQUNK)
		isa_dmacascade(sc->sc_drq);

	TAILQ_INIT(&sc->sc_free_scb);
	TAILQ_INIT(&sc->sc_waiting_scb);
	wds_inquire_setup_information(sc);

	/*
	 * fill in the prototype scsi_link.
	 */
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = sc->sc_scsi_dev;
	sc->sc_link.adapter = &wds_switch;
	sc->sc_link.device = &wds_dev;
	/* XXX */
	/* I don't think the -ASE can handle openings > 1. */
	/* It gives Vendor Error 26 whenever I try it.     */
	sc->sc_link.openings = 1;

#ifdef NEWCONFIG
	isa_establish(&sc->sc_id, &sc->sc_dev);
#endif
	sc->sc_ih = isa_intr_establish(ia->ia_ic, sc->sc_irq, IST_EDGE,
	    IPL_BIO, wdsintr, sc);

	/*
	 * ask the adapter what subunits are present
	 */
	config_found(self, &sc->sc_link, wdsprint);
}

integrate void
wds_finish_scbs(sc)
	struct wds_softc *sc;
{
	struct wds_mbx_in *wmbi;
	struct wds_scb *scb;
	int i;

	wmbi = wmbx->tmbi;

	if (wmbi->stat == WDS_MBI_FREE) {
		for (i = 0; i < WDS_MBX_SIZE; i++) {
			if (wmbi->stat != WDS_MBI_FREE) {
				printf("%s: mbi not in round-robin order\n",
				    sc->sc_dev.dv_xname);
				goto AGAIN;
			}
			wds_nextmbx(wmbi, wmbx, mbi);
		}
#ifdef WDSDIAGnot
		printf("%s: mbi interrupt with no full mailboxes\n",
		    sc->sc_dev.dv_xname);
#endif
		return;
	}

AGAIN:
	do {
		scb = wds_scb_phys_kv(sc, phystol(wmbi->scb_addr));
		if (!scb) {
			printf("%s: bad mbi scb pointer; skipping\n",
			    sc->sc_dev.dv_xname);
			goto next;
		}

#ifdef WDSDEBUG
		if (wds_debug) {
			u_char *cp = &scb->scsi_cmd;
			printf("op=%x %x %x %x %x %x\n",
			    cp[0], cp[1], cp[2], cp[3], cp[4], cp[5]);
			printf("stat %x for mbi addr = 0x%08x, ",
			    wmbi->stat, wmbi);
			printf("scb addr = 0x%x\n", scb);
		}
#endif /* WDSDEBUG */

		untimeout(wds_timeout, scb);
		wds_done(sc, scb, wmbi->stat);

	next:
		wmbi->stat = WDS_MBI_FREE;
		wds_nextmbx(wmbi, wmbx, mbi);
	} while (wmbi->stat != WDS_MBI_FREE);

	wmbx->tmbi = wmbi;
}

/*
 * Process an interrupt.
 */
int
wdsintr(arg)
	void *arg;
{
	struct wds_softc *sc = arg;
	int iobase = sc->sc_iobase;
	u_char c;

	/* Was it really an interrupt from the board? */
	if ((inb(iobase + WDS_STAT) & WDSS_IRQ) == 0)
		return 0;

	/* Get the interrupt status byte. */
	c = inb(iobase + WDS_IRQSTAT) & WDSI_MASK;

	/* Acknowledge (which resets) the interrupt. */
	outb(iobase + WDS_IRQACK, 0x00);

	switch (c) {
	case WDSI_MSVC:
		wds_finish_scbs(sc);
		break;

	case WDSI_MFREE:
		wds_start_scbs(sc);
		break;

	default:
		printf("%s: unrecognized interrupt type %02x",
		    sc->sc_dev.dv_xname, c);
		break;
	}

	return 1;
}

integrate void
wds_reset_scb(sc, scb)
	struct wds_softc *sc;
	struct wds_scb *scb;
{

	scb->flags = 0;
}

/*
 * Free the command structure, the outgoing mailbox and the data buffer.
 */
void
wds_free_scb(sc, scb)
	struct wds_softc *sc;
	struct wds_scb *scb;
{
	int s;

	if (scb->buf != 0) {
		wds_free_buf(sc, scb->buf);
		scb->buf = 0;
	}

	s = splbio();

	wds_reset_scb(sc, scb);
	TAILQ_INSERT_HEAD(&sc->sc_free_scb, scb, chain);

	/*
	 * If there were none, wake anybody waiting for one to come free,
	 * starting with queued entries.
	 */
	if (scb->chain.tqe_next == 0)
		wakeup(&sc->sc_free_scb);

	splx(s);
}

void
wds_free_buf(sc, buf)
	struct wds_softc *sc;
	struct wds_buf *buf;
{
	int s;

	s = splbio();

	buf->busy = 0;
	TAILQ_INSERT_HEAD(&wds_free_buffer, buf, chain);

	/*
	 * If there were none, wake anybody waiting for one to come free,
	 * starting with queued entries.
	 */
	if (buf->chain.tqe_next == 0)
		wakeup(&wds_free_buffer);

	splx(s);
}

integrate void
wds_init_scb(sc, scb)
	struct wds_softc *sc;
	struct wds_scb *scb;
{
	int hashnum;

	bzero(scb, sizeof(struct wds_scb));
	/*
	 * put in the phystokv hash table
	 * Never gets taken out.
	 */
	scb->hashkey = KVTOPHYS(scb);
	hashnum = SCB_HASH(scb->hashkey);
	scb->nexthash = sc->sc_scbhash[hashnum];
	sc->sc_scbhash[hashnum] = scb;
	wds_reset_scb(sc, scb);
}

/*
 * Get a free scb
 *
 * If there are none, see if we can allocate a new one.  If so, put it in
 * the hash table too otherwise either return an error or sleep.
 */
struct wds_scb *
wds_get_scb(sc, flags, needbuffer)
	struct wds_softc *sc;
	int flags;
	int needbuffer;
{
	struct wds_scb *scb;
	int s;

	s = splbio();

	/*
	 * If we can and have to, sleep waiting for one to come free
	 * but only if we can't allocate a new one.
	 */
	for (;;) {
		scb = sc->sc_free_scb.tqh_first;
		if (scb) {
			TAILQ_REMOVE(&sc->sc_free_scb, scb, chain);
			break;
		}
		if (sc->sc_numscbs < WDS_SCB_MAX) {
			scb = (struct wds_scb *) malloc(sizeof(struct wds_scb),
			    M_TEMP, M_NOWAIT);
			if (!scb) {
				printf("%s: can't malloc scb\n",
				    sc->sc_dev.dv_xname);
				goto out;
			}
			wds_init_scb(sc, scb);
			sc->sc_numscbs++;
			break;
		}
		if ((flags & SCSI_NOSLEEP) != 0)
			goto out;
		tsleep(&sc->sc_free_scb, PRIBIO, "wdsscb", 0);
	}

	scb->flags |= SCB_ALLOC;

	if (needbuffer) {
		scb->buf = wds_get_buf(sc, flags);
		if (scb->buf == 0) {
			wds_free_scb(sc, scb);
			scb = 0;
		}
	}

out:
	splx(s);
	return (scb);
}

struct wds_buf *
wds_get_buf(sc, flags)
	struct wds_softc *sc;
	int flags;
{
	struct wds_buf *buf;
	int s;

	s = splbio();

	for (;;) {
		buf = wds_free_buffer.tqh_first;
		if (buf) {
			TAILQ_REMOVE(&wds_free_buffer, buf, chain);
			break;
		}
		if ((flags & SCSI_NOSLEEP) != 0)
			goto out;
		tsleep(&wds_free_buffer, PRIBIO, "wdsbuf", 0);
	}

	buf->busy = 1;

out:
	splx(s);
	return (buf);
}

struct wds_scb *
wds_scb_phys_kv(sc, scb_phys)
	struct wds_softc *sc;
	u_long scb_phys;
{
	int hashnum = SCB_HASH(scb_phys);
	struct wds_scb *scb = sc->sc_scbhash[hashnum];

	while (scb) {
		if (scb->hashkey == scb_phys)
			break;
		/* XXX Check to see if it matches the sense command block. */
		if (scb->hashkey == (scb_phys - sizeof(struct wds_cmd)))
			break;
		scb = scb->nexthash;
	}
	return scb;
}

/*
 * Queue a SCB to be sent to the controller, and send it if possible.
 */
void
wds_queue_scb(sc, scb)
	struct wds_softc *sc;
	struct wds_scb *scb;
{

	TAILQ_INSERT_TAIL(&sc->sc_waiting_scb, scb, chain);
	wds_start_scbs(sc);
}

/*
 * Garbage collect mailboxes that are no longer in use.
 */
void
wds_collect_mbo(sc)
	struct wds_softc *sc;
{
	struct wds_mbx_out *wmbo;	/* Mail Box Out pointer */
	struct wds_scb *scb;

	wmbo = wmbx->cmbo;

	while (sc->sc_mbofull > 0) {
		if (wmbo->cmd != WDS_MBO_FREE)
			break;

#ifdef WDSDIAG
		scb = wds_scb_phys_kv(sc, phystol(wmbo->scb_addr));
		scb->flags &= ~SCB_SENDING;
#endif

		--sc->sc_mbofull;
		wds_nextmbx(wmbo, wmbx, mbo);
	}

	wmbx->cmbo = wmbo;
}

/*
 * Send as many SCBs as we have empty mailboxes for.
 */
void
wds_start_scbs(sc)
	struct wds_softc *sc;
{
	int iobase = sc->sc_iobase;
	struct wds_mbx_out *wmbo;	/* Mail Box Out pointer */
	struct wds_scb *scb;
	u_char c;

	wmbo = wmbx->tmbo;

	while ((scb = sc->sc_waiting_scb.tqh_first) != NULL) {
		if (sc->sc_mbofull >= WDS_MBX_SIZE) {
			wds_collect_mbo(sc);
			if (sc->sc_mbofull >= WDS_MBX_SIZE) {
				c = WDSC_IRQMFREE;
				wds_cmd(iobase, &c, sizeof c);
				break;
			}
		}

		TAILQ_REMOVE(&sc->sc_waiting_scb, scb, chain);
#ifdef WDSDIAG
		scb->flags |= SCB_SENDING;
#endif

		/* Link scb to mbo. */
		if (scb->flags & SCB_SENSE)
			ltophys(KVTOPHYS(&scb->sense), wmbo->scb_addr);
		else
			ltophys(KVTOPHYS(&scb->cmd), wmbo->scb_addr);
		/* XXX What about aborts? */
		wmbo->cmd = WDS_MBO_START;

		/* Tell the card to poll immediately. */
		c = WDSC_MSTART(wmbo - wmbx->mbo);
		wds_cmd(sc->sc_iobase, &c, sizeof c);

		if ((scb->flags & SCB_POLLED) == 0)
			timeout(wds_timeout, scb, (scb->timeout * hz) / 1000);

		++sc->sc_mbofull;
		wds_nextmbx(wmbo, wmbx, mbo);
	}

	wmbx->tmbo = wmbo;
}

/*
 * Process the result of a SCSI command.
 */
void
wds_done(sc, scb, stat)
	struct wds_softc *sc;
	struct wds_scb *scb;
	u_char stat;
{
	struct scsi_xfer *xs = scb->xs;

	/* XXXXX */

	/* Don't release the SCB if it was an internal command. */
	if (xs == 0) {
		scb->flags |= SCB_DONE;
		return;
	}

	/* Sense handling. */
	if (xs->error == XS_SENSE) {
		bcopy(&scb->sense_data, &xs->sense, sizeof (struct scsi_sense_data));
	} else {
		if (xs->error == XS_NOERROR) {
			/* If all went well, or an error is acceptable. */
			if (stat == WDS_MBI_OK) {
				/* OK, set the result */
				xs->resid = 0;
			} else {
				/* Check the mailbox status. */
				switch (stat) {
				case WDS_MBI_OKERR:
					/* SCSI error recorded in scb, counts as WDS_MBI_OK */
					switch (scb->cmd.venderr) {
					case 0x00:
						printf("%s: Is this an error?\n", sc->sc_dev.dv_xname);
						xs->error = XS_DRIVER_STUFFUP; /* Experiment */
						break;
					case 0x01:
						/*printf("%s: OK, see SCSI error field.\n", sc->sc_dev.dv_xname);*/
						if (scb->cmd.stat == SCSI_CHECK) {
							/* Do sense. */
							wds_sense (sc, scb);
							return;
						} else if (scb->cmd.stat == SCSI_BUSY) {
							xs->error = XS_BUSY;
						}
						break;
					case 0x40:
						/*printf("%s: DMA underrun!\n", sc->sc_dev.dv_xname);*/
						/* Hits this if the target returns fewer that datalen bytes (eg my CD-ROM,
						which returns a short version string, or if DMA is turned off etc. */
						xs->resid = 0;
						break;
					default:
						printf("%s: VENDOR ERROR %02x, scsi %02x\n", sc->sc_dev.dv_xname, scb->cmd.venderr, scb->cmd.stat);
						xs->error = XS_DRIVER_STUFFUP; /* Experiment */
						break;
					}
					break;
				case WDS_MBI_ETIME:
					/*
					 * The documentation isn't clear on
					 * what conditions might generate this,
					 * but selection timeouts are the only
					 * one I can think of.
					 */
					xs->error = XS_SELTIMEOUT;
					break;
				case WDS_MBI_ERESET:
				case WDS_MBI_ETARCMD:
				case WDS_MBI_ERESEL:
				case WDS_MBI_ESEL:
				case WDS_MBI_EABORT:
				case WDS_MBI_ESRESET:
				case WDS_MBI_EHRESET:
					xs->error = XS_DRIVER_STUFFUP;
					break;
				}
			}
		} /* else sense */

		if (NEEDBUFFER(sc) && xs->datalen) {
			if (xs->flags & SCSI_DATA_IN)
				bcopy(scb->buf->data, xs->data, xs->datalen);
		}
	} /* XS_NOERROR */

	wds_free_scb(sc, scb);
	xs->flags |= ITSDONE;
	scsi_done(xs);
}

int
wds_find(ia, sc)
	struct isa_attach_args *ia;
	struct wds_softc *sc;
{
	int iobase = ia->ia_iobase;
	u_char c;
	int i;

	/* XXXXX */

	/*
	 * Sending a command causes the CMDRDY bit to clear.
 	 */
	c = inb(iobase + WDS_STAT);
	for (i = 0; i < 4; i++)
		if ((inb(iobase+WDS_STAT) & WDSS_RDY) != 0) {
			goto ready;
		delay(10);
	}
	return 1;

ready:
	outb(iobase + WDS_CMD, WDSC_NOOP);
	if (inb(iobase + WDS_STAT) & WDSS_RDY)
		return 1;

	outb(iobase + WDS_HCR, WDSH_SCSIRESET|WDSH_ASCRESET);
	delay(10000);
	outb(iobase + WDS_HCR, 0x00);
	delay(500000);
	wds_wait(iobase + WDS_STAT, WDSS_RDY, WDSS_RDY);
	if (inb(iobase + WDS_IRQSTAT) != 1)
		if (inb(iobase + WDS_IRQSTAT) != 7)
			printf("%s: failed reset!!! %2x\n", sc->sc_dev.dv_xname, inb(iobase + WDS_IRQSTAT));

	if ((inb(iobase + WDS_STAT) & (WDSS_RDY)) != WDSS_RDY) {
		printf("%s: waiting for controller to become ready.", sc->sc_dev.dv_xname);
		for (i = 0; i < 20; i++) {
			if ((inb(iobase + WDS_STAT) & (WDSS_RDY)) == WDSS_RDY)
				break;
			printf(".");
			delay(10000);
		}
		if ((inb(iobase + WDS_STAT) & (WDSS_RDY)) != WDSS_RDY) {
			printf(" failed\n");
			return 1;
		}
		printf("\n");
	}

	if (sc != NULL) {
		/* XXX Can we do this better? */
		/* who are we on the scsi bus? */
		sc->sc_scsi_dev = 7;

		sc->sc_iobase = iobase;
		sc->sc_irq = ia->ia_irq;
		sc->sc_drq = ia->ia_drq;
	}

	return 0;
}

/*
 * Initialise the board and driver.
 */
void
wds_init(sc)
	struct wds_softc *sc;
{
	int iobase = sc->sc_iobase;
	struct wds_setup init;
	u_char c;
	int i;

	/*
	 * Set up initial mail box for round-robin operation.
	 */
	for (i = 0; i < WDS_MBX_SIZE; i++) {
		wmbx->mbo[i].cmd = WDS_MBO_FREE;
		wmbx->mbi[i].stat = WDS_MBI_FREE;
	}
	wmbx->cmbo = wmbx->tmbo = &wmbx->mbo[0];
	wmbx->tmbi = &wmbx->mbi[0];
	sc->sc_mbofull = 0;

	/* Clear the buffers. */
	TAILQ_INIT(&wds_free_buffer);
	for (i = 0; i < BUFCNT; i++) {
		wds_buffer[i].busy = 0;
		TAILQ_INSERT_HEAD(&wds_free_buffer, &wds_buffer[i], chain);
	}

	init.opcode = WDSC_INIT;
	init.scsi_id = sc->sc_scsi_dev;
	/* Record scsi id of controller for use in scsi_attach */
	sc->sc_scsi_dev = init.scsi_id;
	init.buson_t = 48;
	init.busoff_t = 24;
	init.xx = 0;
	ltophys(KVTOPHYS(wmbx), init.mbaddr);
	init.nomb = init.nimb = WDS_MBX_SIZE;
	wds_cmd(iobase, (u_char *)&init, sizeof init);

	wds_wait(iobase + WDS_STAT, WDSS_INIT, WDSS_INIT);

	c = WDSC_DISUNSOL;
	wds_cmd(iobase, &c, sizeof c);

	outb(iobase + WDS_HCR, WDSH_DRQEN);
}

/*
 * Read the board's firmware revision information.
 */
void
wds_inquire_setup_information(sc)
	struct wds_softc *sc;
{
	struct wds_scb *scb;
	int iobase;
	u_char *j;
	int s;

	iobase = sc->sc_iobase;

	if ((scb = wds_get_scb(sc, SCSI_NOSLEEP, 0)) == NULL) {
		printf("%s: no request slot available in getvers()!\n", sc->sc_dev.dv_xname);
		return;
	}
	scb->xs = NULL;
	scb->timeout = 40;

	bzero(&scb->cmd, sizeof scb->cmd);
	scb->cmd.write = 0x80;
	scb->cmd.opcode = WDSX_GETFIRMREV;

	/* Will poll card, await result. */
	outb(iobase + WDS_HCR, WDSH_DRQEN);
	scb->flags |= SCB_POLLED;

	s = splbio();
	wds_queue_scb(sc, scb);
	splx(s);

	if (wds_ipoll(sc, scb, scb->timeout))
		goto out;

	/* Print the version number. */
	printf(": version %x.%02x ", scb->cmd.targ, scb->cmd.scb.opcode);
	sc->sc_revision = (scb->cmd.targ << 8) | scb->cmd.scb.opcode;
	/* Print out the version string. */
	j = 2 + &(scb->cmd.targ);
	while ((*j >= 32) && (*j < 128)) {
		printf("%c", *j);
		j++;
	}

out:
	printf("\n");
	wds_free_scb(sc, scb);
}

void
wdsminphys(bp)
	struct buf *bp;
{

	if (bp->b_bcount > ((WDS_NSEG - 1) << PGSHIFT))
		bp->b_bcount = ((WDS_NSEG - 1) << PGSHIFT);
	minphys(bp);
}

/*
 * Send a SCSI command.
 */
int
wds_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *sc_link = xs->sc_link;
	struct wds_softc *sc = sc_link->adapter_softc;
	struct wds_scb *scb;
	struct wds_scat_gath *sg;
	int seg;
	u_long thiskv, thisphys, nextphys;
	int bytes_this_seg, bytes_this_page, datalen, flags;
#ifdef TFS
	struct iovec *iovp;
#endif
	int s;

	int iobase;

	iobase = sc->sc_iobase;

	if (xs->flags & SCSI_RESET) {
		/* XXX Fix me! */
		printf("%s: reset!\n", sc->sc_dev.dv_xname);
		wds_init(sc);
		return COMPLETE;
	}

	flags = xs->flags;
	if ((scb = wds_get_scb(sc, flags, NEEDBUFFER(sc))) == NULL) {
		xs->error = XS_DRIVER_STUFFUP;
		return TRY_AGAIN_LATER;
	}
	scb->xs = xs;
	scb->timeout = xs->timeout;

	if (xs->flags & SCSI_DATA_UIO) {
		/* XXX Fix me! */
		/* Let's not worry about UIO. There isn't any code for the *
		 * non-SG boards anyway! */
		printf("%s: UIO is untested and disabled!\n", sc->sc_dev.dv_xname);
		goto bad;
	}

	/* Zero out the command structure. */
	bzero(&scb->cmd, sizeof scb->cmd);
	bcopy(xs->cmd, &scb->cmd.scb, xs->cmdlen < 12 ? xs->cmdlen : 12);

	/* Set up some of the command fields. */
	scb->cmd.targ = (xs->sc_link->target << 5) | xs->sc_link->lun;

	/* NOTE: cmd.write may be OK as 0x40 (disable direction checking)
	 * on boards other than the WD-7000V-ASE. Need this for the ASE:
 	 */
	scb->cmd.write = (xs->flags & SCSI_DATA_IN) ? 0x80 : 0x00;

	if (!NEEDBUFFER(sc) && xs->datalen) {
		sg = scb->scat_gath;
		seg = 0;
#ifdef TFS
		if (flags & SCSI_DATA_UIO) {
			iovp = ((struct uio *)xs->data)->uio_iov;
			datalen = ((struct uio *)xs->data)->uio_iovcnt;
			xs->datalen = 0;
			while (datalen && seg < WDS_NSEG) {
				ltophys(iovp->iov_base, sg->seg_addr);
				ltophys(iovp->iov_len, sg->seg_len);
				xs->datalen += iovp->iov_len;
				SC_DEBUGN(sc_link, SDEV_DB4, ("UIO(0x%x@0x%x)",
				    iovp->iov_len, iovp->iov_base));
				sg++;
				iovp++;
				seg++;
				datalen--;
			}
		} else
#endif /* TFS */
		{
			/*
			 * Set up the scatter-gather block.
			 */
			SC_DEBUG(sc_link, SDEV_DB4,
			    ("%d @0x%x:- ", xs->datalen, xs->data));

			datalen = xs->datalen;
			thiskv = (int)xs->data;
			thisphys = KVTOPHYS(xs->data);

			while (datalen && seg < WDS_NSEG) {
				bytes_this_seg = 0;

				/* put in the base address */
				ltophys(thisphys, sg->seg_addr);

				SC_DEBUGN(sc_link, SDEV_DB4, ("0x%x", thisphys));

				/* do it at least once */
				nextphys = thisphys;
				while (datalen && thisphys == nextphys) {
					/*
					 * This page is contiguous (physically)
					 * with the the last, just extend the
					 * length
					 */
					/* check it fits on the ISA bus */
					if (thisphys > 0xFFFFFF) {
						printf("%s: DMA beyond"
							" end of ISA\n",
							sc->sc_dev.dv_xname);
						goto bad;
					}
					/* how far to the end of the page */
					nextphys = (thisphys & ~PGOFSET) + NBPG;
					bytes_this_page = nextphys - thisphys;
					/**** or the data ****/
					bytes_this_page = min(bytes_this_page,
							      datalen);
					bytes_this_seg += bytes_this_page;
					datalen -= bytes_this_page;

					/* get more ready for the next page */
					thiskv = (thiskv & ~PGOFSET) + NBPG;
					if (datalen)
						thisphys = KVTOPHYS(thiskv);
				}
				/*
				 * next page isn't contiguous, finish the seg
				 */
				SC_DEBUGN(sc_link, SDEV_DB4,
				    ("(0x%x)", bytes_this_seg));
				ltophys(bytes_this_seg, sg->seg_len);
				sg++;
				seg++;
			}
		}
		/* end of iov/kv decision */
		SC_DEBUGN(sc_link, SDEV_DB4, ("\n"));
		if (datalen) {
			/*
			 * there's still data, must have run out of segs!
			 */
			printf("%s: wds_scsi_cmd, more than %d dma segs\n",
			    sc->sc_dev.dv_xname, WDS_NSEG);
			goto bad;
		}
		scb->cmd.opcode = WDSX_SCSISG;
		ltophys(KVTOPHYS(scb->scat_gath), scb->cmd.data);
		ltophys(seg * sizeof(struct wds_scat_gath), scb->cmd.len);
	} else if (xs->datalen > 0) {
		/* The board is an ASC or ASE. Do not use scatter/gather. */
		if (xs->datalen > BUFLEN) {
			printf("%s: wds_scsi_cmd, I/O too large for bounce buffer\n",
			    sc->sc_dev.dv_xname);
			goto bad;
		}
		if (xs->flags & SCSI_DATA_OUT)
			bcopy(xs->data, scb->buf->data, xs->datalen);
		else
			bzero(scb->buf->data, xs->datalen);
		scb->cmd.opcode = WDSX_SCSICMD;
		ltophys(KVTOPHYS(scb->buf->data), scb->cmd.data);
		ltophys(xs->datalen, scb->cmd.len);
	} else {
		scb->cmd.opcode = WDSX_SCSICMD;
		ltophys(0, scb->cmd.data);
		ltophys(0, scb->cmd.len);
	}

	scb->cmd.stat = 0x00;
	scb->cmd.venderr = 0x00;
	ltophys(0, scb->cmd.link);

	/* XXX Do we really want to do this? */
	if (flags & SCSI_POLL) {
		/* Will poll card, await result. */
		outb(iobase + WDS_HCR, WDSH_DRQEN);
		scb->flags |= SCB_POLLED;
	} else {
		/* Will send command, let interrupt routine handle result. */
		outb(iobase + WDS_HCR, WDSH_IRQEN | WDSH_DRQEN);
	}

	s = splbio();
	wds_queue_scb(sc, scb);
	splx(s);

	if ((flags & SCSI_POLL) == 0)
		return SUCCESSFULLY_QUEUED;

	if (wds_poll(sc, xs, scb->timeout)) {
		wds_timeout(scb);
		if (wds_poll(sc, xs, scb->timeout))
			wds_timeout(scb);
	}
	return COMPLETE;

bad:
	xs->error = XS_DRIVER_STUFFUP;
	wds_free_scb(sc, scb);
	return COMPLETE;
}

/*
 * Send a sense request.
 */
void
wds_sense(sc, scb)
	struct wds_softc *sc;
	struct wds_scb *scb;
{
	struct scsi_xfer *xs = scb->xs;
	struct scsi_sense *ss = (void *)&scb->sense.scb;
	int s;

	/* XXXXX */

	/* Send sense request SCSI command. */
	xs->error = XS_SENSE;
	scb->flags |= SCB_SENSE;

	/* First, save the return values */
	if (NEEDBUFFER(sc) && xs->datalen) {
		if (xs->flags & SCSI_DATA_IN)
			bcopy(scb->buf->data, xs->data, xs->datalen);
	}

	/* Next, setup a request sense command block */
	bzero(ss, sizeof(*ss));
	ss->opcode = REQUEST_SENSE;
	ss->byte2 = xs->sc_link->lun << 5;
	ss->length = sizeof(struct scsi_sense_data);

	/* Set up some of the command fields. */
	scb->sense.targ = scb->cmd.targ;
	scb->sense.write = 0x80;
	scb->sense.opcode = WDSX_SCSICMD;
	ltophys(KVTOPHYS(&scb->sense_data), scb->sense.data);
	ltophys(sizeof(struct scsi_sense_data), scb->sense.len);

	s = splbio();
	wds_queue_scb(sc, scb);
	splx(s);

	/*
	 * There's no reason for us to poll here.  There are two cases:
	 * 1) If it's a polling operation, then we're called from the interrupt
	 *    handler, and we return and continue polling.
	 * 2) If it's an interrupt-driven operation, then it gets completed
	 *    later on when the REQUEST SENSE finishes.
	 */
}

/*
 * Poll a particular unit, looking for a particular scb
 */
int
wds_poll(sc, xs, count)
	struct wds_softc *sc;
	struct scsi_xfer *xs;
	int count;
{
	int iobase = sc->sc_iobase;

	/* timeouts are in msec, so we loop in 1000 usec cycles */
	while (count) {
		/*
		 * If we had interrupts enabled, would we
		 * have got an interrupt?
		 */
		if (inb(iobase + WDS_STAT) & WDSS_IRQ)
			wdsintr(sc);
		if (xs->flags & ITSDONE)
			return 0;
		delay(1000);	/* only happens in boot so ok */
		count--;
	}
	return 1;
}

/*
 * Poll a particular unit, looking for a particular scb
 */
int
wds_ipoll(sc, scb, count)
	struct wds_softc *sc;
	struct wds_scb *scb;
	int count;
{
	int iobase = sc->sc_iobase;

	/* timeouts are in msec, so we loop in 1000 usec cycles */
	while (count) {
		/*
		 * If we had interrupts enabled, would we
		 * have got an interrupt?
		 */
		if (inb(iobase + WDS_STAT) & WDSS_IRQ)
			wdsintr(sc);
		if (scb->flags & SCB_DONE)
			return 0;
		delay(1000);	/* only happens in boot so ok */
		count--;
	}
	return 1;
}

void
wds_timeout(arg)
	void *arg;
{
	struct wds_scb *scb = arg;
	struct scsi_xfer *xs = scb->xs;
	struct scsi_link *sc_link = xs->sc_link;
	struct wds_softc *sc = sc_link->adapter_softc;
	int s;

	sc_print_addr(sc_link);
	printf("timed out");

	s = splbio();

#ifdef WDSDIAG
	/*
	 * If The scb's mbx is not free, then the board has gone south?
	 */
	wds_collect_mbo(sc);
	if (scb->flags & SCB_SENDING) {
		printf("%s: not taking commands!\n", sc->sc_dev.dv_xname);
		Debugger();
	}
#endif

	/*
	 * If it has been through before, then
	 * a previous abort has failed, don't
	 * try abort again
	 */
	if (scb->flags & SCB_ABORT) {
		/* abort timed out */
		printf(" AGAIN\n");
		/* XXX Must reset! */
	} else {
		/* abort the operation that has timed out */
		printf("\n");
		scb->xs->error = XS_TIMEOUT;
		scb->timeout = WDS_ABORT_TIMEOUT;
		scb->flags |= SCB_ABORT;
		wds_queue_scb(sc, scb);
	}

	splx(s);
}
