/*	$NetBSD: ahb.c,v 1.7.2.2 1996/12/11 05:06:56 mycroft Exp $	*/

#undef	AHBDEBUG
#ifdef DDB
#define	integrate
#else
#define	integrate	static inline
#endif

/*
 * Copyright (c) 1994, 1996 Charles M. Hannum.  All rights reserved.
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
 * Originally written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
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

#include <machine/bus.h>
#include <machine/intr.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/eisa/eisareg.h>
#include <dev/eisa/eisavar.h>
#include <dev/eisa/eisadevs.h>
#include <dev/eisa/ahbreg.h>

#ifndef DDB
#define Debugger() panic("should call debugger here (aha1742.c)")
#endif /* ! DDB */

#define AHB_ECB_MAX	32	/* store up to 32 ECBs at one time */
#define	ECB_HASH_SIZE	32	/* hash table size for phystokv */
#define	ECB_HASH_SHIFT	9
#define ECB_HASH(x)	((((long)(x))>>ECB_HASH_SHIFT) & (ECB_HASH_SIZE - 1))

#define	KVTOPHYS(x)	vtophys(x)

struct ahb_softc {
	struct device sc_dev;
	bus_chipset_tag_t sc_bc;

	bus_io_handle_t sc_ioh;
	int sc_irq;
	void *sc_ih;

	struct ahb_ecb *sc_ecbhash[ECB_HASH_SIZE];
	TAILQ_HEAD(, ahb_ecb) sc_free_ecb;
	struct ahb_ecb *sc_immed_ecb;	/* an outstanding immediete command */
	int sc_numecbs;
	int sc_scsi_dev;		/* our scsi id */
	struct scsi_link sc_link;
};

void ahb_send_mbox __P((struct ahb_softc *, int, struct ahb_ecb *));
void ahb_send_immed __P((struct ahb_softc *, u_long, struct ahb_ecb *));
int ahbintr __P((void *));
void ahb_free_ecb __P((struct ahb_softc *, struct ahb_ecb *));
struct ahb_ecb *ahb_get_ecb __P((struct ahb_softc *, int));
struct ahb_ecb *ahb_ecb_phys_kv __P((struct ahb_softc *, physaddr));
void ahb_done __P((struct ahb_softc *, struct ahb_ecb *));
int ahb_find __P((bus_chipset_tag_t, bus_io_handle_t, struct ahb_softc *));
void ahb_init __P((struct ahb_softc *));
void ahbminphys __P((struct buf *));
int ahb_scsi_cmd __P((struct scsi_xfer *));
int ahb_poll __P((struct ahb_softc *, struct scsi_xfer *, int));
void ahb_timeout __P((void *));

integrate void ahb_reset_ecb __P((struct ahb_softc *, struct ahb_ecb *));
integrate void ahb_init_ecb __P((struct ahb_softc *, struct ahb_ecb *));

struct scsi_adapter ahb_switch = {
	ahb_scsi_cmd,
	ahbminphys,
	0,
	0,
};

/* the below structure is so we have a default dev struct for our link struct */
struct scsi_device ahb_dev = {
	NULL,			/* Use default error handler */
	NULL,			/* have a queue, served by this */
	NULL,			/* have no async handler */
	NULL,			/* Use default 'done' routine */
};

int	ahbmatch __P((struct device *, void *, void *));
void	ahbattach __P((struct device *, struct device *, void *));

struct cfattach ahb_ca = {
	sizeof(struct ahb_softc), ahbmatch, ahbattach
};

struct cfdriver ahb_cd = {
	NULL, "ahb", DV_DULL
};

#define	AHB_ABORT_TIMEOUT	2000	/* time to wait for abort (mSec) */

/*
 * Check the slots looking for a board we recognise
 * If we find one, note it's address (slot) and call
 * the actual probe routine to check it out.
 */
int
ahbmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct eisa_attach_args *ea = aux;
	bus_chipset_tag_t bc = ea->ea_bc;
	bus_io_handle_t ioh;
	int rv;

	/* must match one of our known ID strings */
	if (strcmp(ea->ea_idstring, "ADP0000") &&
	    strcmp(ea->ea_idstring, "ADP0001") &&
	    strcmp(ea->ea_idstring, "ADP0002") &&
	    strcmp(ea->ea_idstring, "ADP0400"))
		return (0);

	if (bus_io_map(bc, EISA_SLOT_ADDR(ea->ea_slot), EISA_SLOT_SIZE, &ioh))
		return (0);

	rv = !ahb_find(bc, ioh, NULL);

	bus_io_unmap(bc, ioh, EISA_SLOT_SIZE);

	return (rv);
}

ahbprint()
{

}

/*
 * Attach all the sub-devices we can find
 */
void
ahbattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct eisa_attach_args *ea = aux;
	struct ahb_softc *sc = (void *)self;
	bus_chipset_tag_t bc = ea->ea_bc;
	bus_io_handle_t ioh;
	eisa_chipset_tag_t ec = ea->ea_ec;
	eisa_intr_handle_t ih;
	const char *model, *intrstr;

	if (!strcmp(ea->ea_idstring, "ADP0000"))
		model = EISA_PRODUCT_ADP0000;
	else if (!strcmp(ea->ea_idstring, "ADP0001"))
		model = EISA_PRODUCT_ADP0001;
	else if (!strcmp(ea->ea_idstring, "ADP0002"))
		model = EISA_PRODUCT_ADP0002;
	else if (!strcmp(ea->ea_idstring, "ADP0400"))
		model = EISA_PRODUCT_ADP0400;
	else
		model = "unknown model!";
	printf(": %s\n", model);

	if (bus_io_map(bc, EISA_SLOT_ADDR(ea->ea_slot), EISA_SLOT_SIZE, &ioh))
		panic("ahbattach: could not map I/O addresses");

	sc->sc_bc = bc;
	sc->sc_ioh = ioh;
	if (ahb_find(bc, ioh, sc))
		panic("ahbattach: ahb_find failed!");

	ahb_init(sc);
	TAILQ_INIT(&sc->sc_free_ecb);

	/*
	 * fill in the prototype scsi_link.
	 */
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = sc->sc_scsi_dev;
	sc->sc_link.adapter = &ahb_switch;
	sc->sc_link.device = &ahb_dev;
	sc->sc_link.openings = 4;

	if (eisa_intr_map(ec, sc->sc_irq, &ih)) {
		printf("%s: couldn't map interrupt (%d)\n",
		    sc->sc_dev.dv_xname, sc->sc_irq);
		return;
	}
	intrstr = eisa_intr_string(ec, ih);
	sc->sc_ih = eisa_intr_establish(ec, ih, IST_LEVEL, IPL_BIO,
	    ahbintr, sc);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt",
		    sc->sc_dev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	if (intrstr != NULL)
		printf("%s: interrupting at %s\n", sc->sc_dev.dv_xname,
		    intrstr);

	/*
	 * ask the adapter what subunits are present
	 */
	config_found(self, &sc->sc_link, ahbprint);
}

/*
 * Function to send a command out through a mailbox
 */
void
ahb_send_mbox(sc, opcode, ecb)
	struct ahb_softc *sc;
	int opcode;
	struct ahb_ecb *ecb;
{
	bus_chipset_tag_t bc = sc->sc_bc;
	bus_io_handle_t ioh = sc->sc_ioh;
	int wait = 300;	/* 1ms should be enough */

	while (--wait) {
		if ((bus_io_read_1(bc, ioh, G2STAT) & (G2STAT_BUSY | G2STAT_MBOX_EMPTY))
		    == (G2STAT_MBOX_EMPTY))
			break;
		delay(10);
	}
	if (!wait) {
		printf("%s: board not responding\n", sc->sc_dev.dv_xname);
		Debugger();
	}

	bus_io_write_4(bc, ioh, MBOXOUT0, KVTOPHYS(ecb)); /* don't know this will work */
	bus_io_write_1(bc, ioh, ATTN, opcode | ecb->xs->sc_link->target);

	if ((ecb->xs->flags & SCSI_POLL) == 0)
		timeout(ahb_timeout, ecb, (ecb->timeout * hz) / 1000);
}

/*
 * Function to  send an immediate type command to the adapter
 */
void
ahb_send_immed(sc, cmd, ecb)
	struct ahb_softc *sc;
	u_long cmd;
	struct ahb_ecb *ecb;
{
	bus_chipset_tag_t bc = sc->sc_bc;
	bus_io_handle_t ioh = sc->sc_ioh;
	int wait = 100;	/* 1 ms enough? */

	while (--wait) {
		if ((bus_io_read_1(bc, ioh, G2STAT) & (G2STAT_BUSY | G2STAT_MBOX_EMPTY))
		    == (G2STAT_MBOX_EMPTY))
			break;
		delay(10);
	}
	if (!wait) {
		printf("%s: board not responding\n", sc->sc_dev.dv_xname);
		Debugger();
	}

	bus_io_write_4(bc, ioh, MBOXOUT0, cmd);	/* don't know this will work */
	bus_io_write_1(bc, ioh, G2CNTRL, G2CNTRL_SET_HOST_READY);
	bus_io_write_1(bc, ioh, ATTN, OP_IMMED | ecb->xs->sc_link->target);

	if ((ecb->xs->flags & SCSI_POLL) == 0)
		timeout(ahb_timeout, ecb, (ecb->timeout * hz) / 1000);
}

/*
 * Catch an interrupt from the adaptor
 */
int
ahbintr(arg)
	void *arg;
{
	struct ahb_softc *sc = arg;
	bus_chipset_tag_t bc = sc->sc_bc;
	bus_io_handle_t ioh = sc->sc_ioh;
	struct ahb_ecb *ecb;
	u_char ahbstat;
	u_long mboxval;

#ifdef	AHBDEBUG
	printf("%s: ahbintr ", sc->sc_dev.dv_xname);
#endif /* AHBDEBUG */

	if ((bus_io_read_1(bc, ioh, G2STAT) & G2STAT_INT_PEND) == 0)
		return 0;

	for (;;) {
		/*
		 * First get all the information and then
		 * acknowlege the interrupt
		 */
		ahbstat = bus_io_read_1(bc, ioh, G2INTST);
		mboxval = bus_io_read_4(bc, ioh, MBOXIN0);
		bus_io_write_1(bc, ioh, G2CNTRL, G2CNTRL_CLEAR_EISA_INT);

#ifdef	AHBDEBUG
		printf("status = 0x%x ", ahbstat);
#endif /* AHBDEBUG */

		/*
		 * Process the completed operation
		 */
		switch (ahbstat & G2INTST_INT_STAT) {
		case AHB_ECB_OK:
		case AHB_ECB_RECOVERED:
		case AHB_ECB_ERR:
			ecb = ahb_ecb_phys_kv(sc, mboxval);
			if (!ecb) {
				printf("%s: BAD ECB RETURNED!\n",
				    sc->sc_dev.dv_xname);
				goto next;	/* whatever it was, it'll timeout */
			}
			break;

		case AHB_IMMED_ERR:
			ecb = sc->sc_immed_ecb;
			sc->sc_immed_ecb = 0;
			ecb->flags |= ECB_IMMED_FAIL;
			break;

		case AHB_IMMED_OK:
			ecb = sc->sc_immed_ecb;
			sc->sc_immed_ecb = 0;
			break;

		default:
			printf("%s: unexpected interrupt %x\n",
			    sc->sc_dev.dv_xname, ahbstat);
			goto next;
		}

		untimeout(ahb_timeout, ecb);
		ahb_done(sc, ecb);

	next:
		if ((bus_io_read_1(bc, ioh, G2STAT) & G2STAT_INT_PEND) == 0)
			return 1;
	}
}

integrate void
ahb_reset_ecb(sc, ecb)
	struct ahb_softc *sc;
	struct ahb_ecb *ecb;
{

	ecb->flags = 0;
}

/*
 * A ecb (and hence a mbx-out is put onto the
 * free list.
 */
void
ahb_free_ecb(sc, ecb)
	struct ahb_softc *sc;
	struct ahb_ecb *ecb;
{
	int s;

	s = splbio();

	ahb_reset_ecb(sc, ecb);
	TAILQ_INSERT_HEAD(&sc->sc_free_ecb, ecb, chain);

	/*
	 * If there were none, wake anybody waiting for one to come free,
	 * starting with queued entries.
	 */
	if (ecb->chain.tqe_next == 0)
		wakeup(&sc->sc_free_ecb);

	splx(s);
}

integrate void
ahb_init_ecb(sc, ecb)
	struct ahb_softc *sc;
	struct ahb_ecb *ecb;
{
	int hashnum;

	bzero(ecb, sizeof(struct ahb_ecb));
	/*
	 * put in the phystokv hash table
	 * Never gets taken out.
	 */
	ecb->hashkey = KVTOPHYS(ecb);
	hashnum = ECB_HASH(ecb->hashkey);
	ecb->nexthash = sc->sc_ecbhash[hashnum];
	sc->sc_ecbhash[hashnum] = ecb;
	ahb_reset_ecb(sc, ecb);
}

/*
 * Get a free ecb
 *
 * If there are none, see if we can allocate a new one. If so, put it in the
 * hash table too otherwise either return an error or sleep.
 */
struct ahb_ecb *
ahb_get_ecb(sc, flags)
	struct ahb_softc *sc;
	int flags;
{
	struct ahb_ecb *ecb;
	int s;

	s = splbio();

	/*
	 * If we can and have to, sleep waiting for one to come free
	 * but only if we can't allocate a new one.
	 */
	for (;;) {
		ecb = sc->sc_free_ecb.tqh_first;
		if (ecb) {
			TAILQ_REMOVE(&sc->sc_free_ecb, ecb, chain);
			break;
		}
		if (sc->sc_numecbs < AHB_ECB_MAX) {
			ecb = (struct ahb_ecb *) malloc(sizeof(struct ahb_ecb),
			    M_TEMP, M_NOWAIT);
			if (!ecb) {
				printf("%s: can't malloc ecb\n",
				    sc->sc_dev.dv_xname);
				goto out;
			}
			ahb_init_ecb(sc, ecb);
			sc->sc_numecbs++;
			break;
		}
		if ((flags & SCSI_NOSLEEP) != 0)
			goto out;
		tsleep(&sc->sc_free_ecb, PRIBIO, "ahbecb", 0);
	}

	ecb->flags |= ECB_ALLOC;

out:
	splx(s);
	return ecb;
}

/*
 * given a physical address, find the ecb that it corresponds to.
 */
struct ahb_ecb *
ahb_ecb_phys_kv(sc, ecb_phys)
	struct ahb_softc *sc;
	physaddr ecb_phys;
{
	int hashnum = ECB_HASH(ecb_phys);
	struct ahb_ecb *ecb = sc->sc_ecbhash[hashnum];

	while (ecb) {
		if (ecb->hashkey == ecb_phys)
			break;
		ecb = ecb->nexthash;
	}
	return ecb;
}

/*
 * We have a ecb which has been processed by the adaptor, now we look to see
 * how the operation went.
 */
void
ahb_done(sc, ecb)
	struct ahb_softc *sc;
	struct ahb_ecb *ecb;
{
	struct scsi_sense_data *s1, *s2;
	struct scsi_xfer *xs = ecb->xs;

	SC_DEBUG(xs->sc_link, SDEV_DB2, ("ahb_done\n"));
	/*
	 * Otherwise, put the results of the operation
	 * into the xfer and call whoever started it
	 */
	if ((ecb->flags & ECB_ALLOC) == 0) {
		printf("%s: exiting ecb not allocated!\n", sc->sc_dev.dv_xname);
		Debugger();
	}
	if (ecb->flags & ECB_IMMED) {
		if (ecb->flags & ECB_IMMED_FAIL)
			xs->error = XS_DRIVER_STUFFUP;
		goto done;
	}
	if (xs->error == XS_NOERROR) {
		if (ecb->ecb_status.host_stat != HS_OK) {
			switch (ecb->ecb_status.host_stat) {
			case HS_TIMED_OUT:	/* No response */
				xs->error = XS_SELTIMEOUT;
				break;
			default:	/* Other scsi protocol messes */
				printf("%s: host_stat %x\n",
				    sc->sc_dev.dv_xname, ecb->ecb_status.host_stat);
				xs->error = XS_DRIVER_STUFFUP;
			}
		} else if (ecb->ecb_status.target_stat != SCSI_OK) {
			switch (ecb->ecb_status.target_stat) {
			case SCSI_CHECK:
				s1 = &ecb->ecb_sense;
				s2 = &xs->sense;
				*s2 = *s1;
				xs->error = XS_SENSE;
				break;
			case SCSI_BUSY:
				xs->error = XS_BUSY;
				break;
			default:
				printf("%s: target_stat %x\n",
				    sc->sc_dev.dv_xname, ecb->ecb_status.target_stat);
				xs->error = XS_DRIVER_STUFFUP;
			}
		} else
			xs->resid = 0;
	}
done:
	ahb_free_ecb(sc, ecb);
	xs->flags |= ITSDONE;
	scsi_done(xs);
}

/*
 * Start the board, ready for normal operation
 */
int
ahb_find(bc, ioh, sc)
	bus_chipset_tag_t bc;
	bus_io_handle_t ioh;
	struct ahb_softc *sc;
{
	u_char intdef;
	int i, irq, busid;
	int wait = 1000;	/* 1 sec enough? */

	bus_io_write_1(bc, ioh, PORTADDR, PORTADDR_ENHANCED);

#define	NO_NO 1
#ifdef NO_NO
	/*
	 * reset board, If it doesn't respond, assume
	 * that it's not there.. good for the probe
	 */
	bus_io_write_1(bc, ioh, G2CNTRL, G2CNTRL_HARD_RESET);
	delay(1000);
	bus_io_write_1(bc, ioh, G2CNTRL, 0);
	delay(10000);
	while (--wait) {
		if ((bus_io_read_1(bc, ioh, G2STAT) & G2STAT_BUSY) == 0)
			break;
		delay(1000);
	}
	if (!wait) {
#ifdef	AHBDEBUG
		printf("ahb_find: No answer from aha1742 board\n");
#endif /* AHBDEBUG */
		return ENXIO;
	}
	i = bus_io_read_1(bc, ioh, MBOXIN0);
	if (i) {
		printf("self test failed, val = 0x%x\n", i);
		return EIO;
	}

	/* Set it again, just to be sure. */
	bus_io_write_1(bc, ioh, PORTADDR, PORTADDR_ENHANCED);
#endif

	while (bus_io_read_1(bc, ioh, G2STAT) & G2STAT_INT_PEND) {
		printf(".");
		bus_io_write_1(bc, ioh, G2CNTRL, G2CNTRL_CLEAR_EISA_INT);
		delay(10000);
	}

	intdef = bus_io_read_1(bc, ioh, INTDEF);
	switch (intdef & 0x07) {
	case INT9:
		irq = 9;
		break;
	case INT10:
		irq = 10;
		break;
	case INT11:
		irq = 11;
		break;
	case INT12:
		irq = 12;
		break;
	case INT14:
		irq = 14;
		break;
	case INT15:
		irq = 15;
		break;
	default:
		printf("illegal int setting %x\n", intdef);
		return EIO;
	}

	bus_io_write_1(bc, ioh, INTDEF, (intdef | INTEN));	/* make sure we can interrupt */

	/* who are we on the scsi bus? */
	busid = (bus_io_read_1(bc, ioh, SCSIDEF) & HSCSIID);

	/* if we want to fill in softc, do so now */
	if (sc != NULL) {
		sc->sc_irq = irq;
		sc->sc_scsi_dev = busid;
	}

	/*
	 * Note that we are going and return (to probe)
	 */
	return 0;
}

void
ahb_init(sc)
	struct ahb_softc *sc;
{

}

void
ahbminphys(bp)
	struct buf *bp;
{

	if (bp->b_bcount > ((AHB_NSEG - 1) << PGSHIFT))
		bp->b_bcount = ((AHB_NSEG - 1) << PGSHIFT);
	minphys(bp);
}

/*
 * start a scsi operation given the command and the data address.  Also needs
 * the unit, target and lu.
 */
int
ahb_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *sc_link = xs->sc_link;
	struct ahb_softc *sc = sc_link->adapter_softc;
	struct ahb_ecb *ecb;
	struct ahb_dma_seg *sg;
	int seg;		/* scatter gather seg being worked on */
	u_long thiskv, thisphys, nextphys;
	int bytes_this_seg, bytes_this_page, datalen, flags;
	int s;

	SC_DEBUG(sc_link, SDEV_DB2, ("ahb_scsi_cmd\n"));
	/*
	 * get a ecb (mbox-out) to use. If the transfer
	 * is from a buf (possibly from interrupt time)
	 * then we can't allow it to sleep
	 */
	flags = xs->flags;
	if ((ecb = ahb_get_ecb(sc, flags)) == NULL) {
		xs->error = XS_DRIVER_STUFFUP;
		return TRY_AGAIN_LATER;
	}
	ecb->xs = xs;
	ecb->timeout = xs->timeout;

	/*
	 * If it's a reset, we need to do an 'immediate'
	 * command, and store its ecb for later
	 * if there is already an immediate waiting,
	 * then WE must wait
	 */
	if (flags & SCSI_RESET) {
		ecb->flags |= ECB_IMMED;
		if (sc->sc_immed_ecb)
			return TRY_AGAIN_LATER;
		sc->sc_immed_ecb = ecb;

		s = splbio();
		ahb_send_immed(sc, AHB_TARG_RESET, ecb);
		splx(s);

		if ((flags & SCSI_POLL) == 0)
			return SUCCESSFULLY_QUEUED;

		/*
		 * If we can't use interrupts, poll on completion
		 */
		if (ahb_poll(sc, xs, ecb->timeout))
			ahb_timeout(ecb);
		return COMPLETE;
	}

	/*
	 * Put all the arguments for the xfer in the ecb
	 */
	ecb->opcode = ECB_SCSI_OP;
	ecb->opt1 = ECB_SES /*| ECB_DSB*/ | ECB_ARS;
	ecb->opt2 = sc_link->lun | ECB_NRB;
	bcopy(xs->cmd, &ecb->scsi_cmd, ecb->scsi_cmd_length = xs->cmdlen);
	ecb->sense_ptr = KVTOPHYS(&ecb->ecb_sense);
	ecb->req_sense_length = sizeof(ecb->ecb_sense);
	ecb->status = KVTOPHYS(&ecb->ecb_status);
	ecb->ecb_status.host_stat = 0x00;
	ecb->ecb_status.target_stat = 0x00;

	if (xs->datalen) {
		sg = ecb->ahb_dma;
		seg = 0;
#ifdef	TFS
		if (flags & SCSI_DATA_UIO) {
			struct iovec *iovp = ((struct uio *) xs->data)->uio_iov;
			datalen = ((struct uio *) xs->data)->uio_iovcnt;
			xs->datalen = 0;
			while (datalen && seg < AHB_NSEG) {
				sg->seg_addr = (physaddr)iovp->iov_base;
				sg->seg_len = iovp->iov_len;
				xs->datalen += iovp->iov_len;
				SC_DEBUGN(sc_link, SDEV_DB4, ("(0x%x@0x%x)",
				    iovp->iov_len, iovp->iov_base));
				sg++;
				iovp++;
				seg++;
				datalen--;
			}
		}
		else
#endif /*TFS */
		{
			/*
			 * Set up the scatter gather block
			 */
			SC_DEBUG(sc_link, SDEV_DB4,
			    ("%d @0x%x:- ", xs->datalen, xs->data));
			datalen = xs->datalen;
			thiskv = (long) xs->data;
			thisphys = KVTOPHYS(thiskv);

			while (datalen && seg < AHB_NSEG) {
				bytes_this_seg = 0;

				/* put in the base address */
				sg->seg_addr = thisphys;

				SC_DEBUGN(sc_link, SDEV_DB4, ("0x%x", thisphys));

				/* do it at least once */
				nextphys = thisphys;
				while (datalen && thisphys == nextphys) {
					/*
					 * This page is contiguous (physically)
					 * with the the last, just extend the
					 * length
					 */
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
				sg->seg_len = bytes_this_seg;
				sg++;
				seg++;
			}
		}
		/*end of iov/kv decision */
		SC_DEBUGN(sc_link, SDEV_DB4, ("\n"));
		if (datalen) {
			/*
			 * there's still data, must have run out of segs!
			 */
			printf("%s: ahb_scsi_cmd, more than %d dma segs\n",
			    sc->sc_dev.dv_xname, AHB_NSEG);
			goto bad;
		}
		ecb->data_addr = KVTOPHYS(ecb->ahb_dma);
		ecb->data_length = seg * sizeof(struct ahb_dma_seg);
		ecb->opt1 |= ECB_S_G;
	} else {	/* No data xfer, use non S/G values */
		ecb->data_addr = (physaddr)0;
		ecb->data_length = 0;
	}
	ecb->link_addr = (physaddr)0;

	s = splbio();
	ahb_send_mbox(sc, OP_START_ECB, ecb);
	splx(s);

	/*
	 * Usually return SUCCESSFULLY QUEUED
	 */
	if ((flags & SCSI_POLL) == 0)
		return SUCCESSFULLY_QUEUED;

	/*
	 * If we can't use interrupts, poll on completion
	 */
	if (ahb_poll(sc, xs, ecb->timeout)) {
		ahb_timeout(ecb);
		if (ahb_poll(sc, xs, ecb->timeout))
			ahb_timeout(ecb);
	}
	return COMPLETE;

bad:
	xs->error = XS_DRIVER_STUFFUP;
	ahb_free_ecb(sc, ecb);
	return COMPLETE;
}

/*
 * Function to poll for command completion when in poll mode
 */
int
ahb_poll(sc, xs, count)
	struct ahb_softc *sc;
	struct scsi_xfer *xs;
	int count;
{				/* in msec  */
	bus_chipset_tag_t bc = sc->sc_bc;
	bus_io_handle_t ioh = sc->sc_ioh;

	while (count) {
		/*
		 * If we had interrupts enabled, would we
		 * have got an interrupt?
		 */
		if (bus_io_read_1(bc, ioh, G2STAT) & G2STAT_INT_PEND)
			ahbintr(sc);
		if (xs->flags & ITSDONE)
			return 0;
		delay(1000);
		count--;
	}
	return 1;
}

void
ahb_timeout(arg)
	void *arg;
{
	struct ahb_ecb *ecb = arg;
	struct scsi_xfer *xs = ecb->xs;
	struct scsi_link *sc_link = xs->sc_link;
	struct ahb_softc *sc = sc_link->adapter_softc;
	int s;

	sc_print_addr(sc_link);
	printf("timed out");

	s = splbio();

	if (ecb->flags & ECB_IMMED) {
		printf("\n");
		ecb->flags |= ECB_IMMED_FAIL;
		/* XXX Must reset! */
	} else

	/*
	 * If it has been through before, then
	 * a previous abort has failed, don't
	 * try abort again
	 */
	if (ecb->flags & ECB_ABORT) {
		/* abort timed out */
		printf(" AGAIN\n");
		/* XXX Must reset! */
	} else {
		/* abort the operation that has timed out */
		printf("\n");
		ecb->xs->error = XS_TIMEOUT;
		ecb->timeout = AHB_ABORT_TIMEOUT;
		ecb->flags |= ECB_ABORT;
		ahb_send_mbox(sc, OP_ABORT_ECB, ecb);
	}

	splx(s);
}
