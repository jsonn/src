/*	$NetBSD: aha.c,v 1.7.4.2 1997/11/04 06:04:32 thorpej Exp $	*/

#undef AHADIAG
#ifdef DDB
#define	integrate
#else
#define	integrate	static inline
#endif

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

/*
 * Copyright (c) 1994, 1996, 1997 Charles M. Hannum.  All rights reserved.
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

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/ic/ahareg.h>
#include <dev/ic/ahavar.h>

#ifndef DDB
#define Debugger() panic("should call debugger here (aha1542.c)")
#endif /* ! DDB */

#define	AHA_MAXXFER	((AHA_NSEG - 1) << PGSHIFT)

#ifdef AHADEBUG
int	aha_debug = 1;
#endif /* AHADEBUG */

int aha_cmd __P((bus_space_tag_t, bus_space_handle_t, struct aha_softc *, int,
    u_char *, int, u_char *));
integrate void aha_finish_ccbs __P((struct aha_softc *));
integrate void aha_reset_ccb __P((struct aha_softc *, struct aha_ccb *));
void aha_free_ccb __P((struct aha_softc *, struct aha_ccb *));
integrate int aha_init_ccb __P((struct aha_softc *, struct aha_ccb *));
struct aha_ccb *aha_get_ccb __P((struct aha_softc *, int));
struct aha_ccb *aha_ccb_phys_kv __P((struct aha_softc *, u_long));
void aha_queue_ccb __P((struct aha_softc *, struct aha_ccb *));
void aha_collect_mbo __P((struct aha_softc *));
void aha_start_ccbs __P((struct aha_softc *));
void aha_done __P((struct aha_softc *, struct aha_ccb *));
void aha_init __P((struct aha_softc *));
void aha_inquire_setup_information __P((struct aha_softc *));
void ahaminphys __P((struct buf *));
int aha_scsi_cmd __P((struct scsipi_xfer *));
int aha_poll __P((struct aha_softc *, struct scsipi_xfer *, int));
void aha_timeout __P((void *arg));
int aha_create_ccbs __P((struct aha_softc *, void *, size_t, int));
void aha_enqueue __P((struct aha_softc *, struct scsipi_xfer *, int));
struct scsipi_xfer *aha_dequeue __P((struct aha_softc *));

struct scsipi_adapter aha_switch = {
	aha_scsi_cmd,
	ahaminphys,
	0,
	0,
};

/* the below structure is so we have a default dev struct for out link struct */
struct scsipi_device aha_dev = {
	NULL,			/* Use default error handler */
	NULL,			/* have a queue, served by this */
	NULL,			/* have no async handler */
	NULL,			/* Use default 'done' routine */
};

struct cfdriver aha_cd = {
	NULL, "aha", DV_DULL
};

#define AHA_RESET_TIMEOUT	2000	/* time to wait for reset (mSec) */
#define	AHA_ABORT_TIMEOUT	2000	/* time to wait for abort (mSec) */

/* XXX Should put this in a better place. */
#define	offsetof(type, member)	((size_t)(&((type *)0)->member))

/*
 * Insert a scsipi_xfer into the software queue.  We overload xs->free_list
 * to avoid having to allocate additional resources (since we're used
 * only during resource shortages anyhow.
 */
void
aha_enqueue(sc, xs, infront)
	struct aha_softc *sc;
	struct scsipi_xfer *xs;
	int infront;
{

	if (infront || sc->sc_queue.lh_first == NULL) {
		if (sc->sc_queue.lh_first == NULL)
			sc->sc_queuelast = xs;
		LIST_INSERT_HEAD(&sc->sc_queue, xs, free_list);
		return;
	}

	LIST_INSERT_AFTER(sc->sc_queuelast, xs, free_list);
	sc->sc_queuelast = xs;
}

/*
 * Pull a scsipi_xfer off the front of the software queue.
 */
struct scsipi_xfer *
aha_dequeue(sc)
	struct aha_softc *sc;
{
	struct scsipi_xfer *xs;

	xs = sc->sc_queue.lh_first;
	LIST_REMOVE(xs, free_list);

	if (sc->sc_queue.lh_first == NULL)
		sc->sc_queuelast = NULL;

	return (xs);
}

/*
 * aha_cmd(iot, ioh, sc, icnt, ibuf, ocnt, obuf)
 *
 * Activate Adapter command
 *    icnt:   number of args (outbound bytes including opcode)
 *    ibuf:   argument buffer
 *    ocnt:   number of expected returned bytes
 *    obuf:   result buffer
 *    wait:   number of seconds to wait for response
 *
 * Performs an adapter command through the ports.  Not to be confused with a
 * scsi command, which is read in via the dma; one of the adapter commands
 * tells it to read in a scsi command.
 */
int
aha_cmd(iot, ioh, sc, icnt, ibuf, ocnt, obuf)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct aha_softc *sc;
	int icnt, ocnt;
	u_char *ibuf, *obuf;
{
	const char *name;
	register int i;
	int wait;
	u_char sts;
	u_char opcode = ibuf[0];

	if (sc != NULL)
		name = sc->sc_dev.dv_xname;
	else
		name = "(aha probe)";

	/*
	 * Calculate a reasonable timeout for the command.
	 */
	switch (opcode) {
	case AHA_INQUIRE_DEVICES:
		wait = 90 * 20000;
		break;
	default:
		wait = 1 * 20000;
		break;
	}

	/*
	 * Wait for the adapter to go idle, unless it's one of
	 * the commands which don't need this
	 */
	if (opcode != AHA_MBO_INTR_EN) {
		for (i = 20000; i; i--) {	/* 1 sec? */
			sts = bus_space_read_1(iot, ioh, AHA_STAT_PORT);
			if (sts & AHA_STAT_IDLE)
				break;
			delay(50);
		}
		if (!i) {
			printf("%s: aha_cmd, host not idle(0x%x)\n",
			    name, sts);
			return (1);
		}
	}
	/*
	 * Now that it is idle, if we expect output, preflush the
	 * queue feeding to us.
	 */
	if (ocnt) {
		while ((bus_space_read_1(iot, ioh, AHA_STAT_PORT)) & AHA_STAT_DF)
			bus_space_read_1(iot, ioh, AHA_DATA_PORT);
	}
	/*
	 * Output the command and the number of arguments given
	 * for each byte, first check the port is empty.
	 */
	while (icnt--) {
		for (i = wait; i; i--) {
			sts = bus_space_read_1(iot, ioh, AHA_STAT_PORT);
			if (!(sts & AHA_STAT_CDF))
				break;
			delay(50);
		}
		if (!i) {
			if (opcode != AHA_INQUIRE_REVISION)
				printf("%s: aha_cmd, cmd/data port full\n", name);
			bus_space_write_1(iot, ioh, AHA_CTRL_PORT, AHA_CTRL_SRST);
			return (1);
		}
		bus_space_write_1(iot, ioh, AHA_CMD_PORT, *ibuf++);
	}
	/*
	 * If we expect input, loop that many times, each time,
	 * looking for the data register to have valid data
	 */
	while (ocnt--) {
		for (i = wait; i; i--) {
			sts = bus_space_read_1(iot, ioh, AHA_STAT_PORT);
			if (sts & AHA_STAT_DF)
				break;
			delay(50);
		}
		if (!i) {
			if (opcode != AHA_INQUIRE_REVISION)
				printf("%s: aha_cmd, cmd/data port empty %d\n",
				    name, ocnt);
			bus_space_write_1(iot, ioh, AHA_CTRL_PORT, AHA_CTRL_SRST);
			return (1);
		}
		*obuf++ = bus_space_read_1(iot, ioh, AHA_DATA_PORT);
	}
	/*
	 * Wait for the board to report a finished instruction.
	 * We may get an extra interrupt for the HACC signal, but this is
	 * unimportant.
	 */
	if (opcode != AHA_MBO_INTR_EN) {
		for (i = 20000; i; i--) {	/* 1 sec? */
			sts = bus_space_read_1(iot, ioh, AHA_INTR_PORT);
			/* XXX Need to save this in the interrupt handler? */
			if (sts & AHA_INTR_HACC)
				break;
			delay(50);
		}
		if (!i) {
			printf("%s: aha_cmd, host not finished(0x%x)\n",
			    name, sts);
			return (1);
		}
	}
	bus_space_write_1(iot, ioh, AHA_CTRL_PORT, AHA_CTRL_IRST);
	return (0);
}

void
aha_attach(sc, apd)
	struct aha_softc *sc;
	struct aha_probe_data *apd;
{

	TAILQ_INIT(&sc->sc_free_ccb);
	TAILQ_INIT(&sc->sc_waiting_ccb);
	LIST_INIT(&sc->sc_queue);

	/*
	 * fill in the prototype scsipi_link.
	 */
	sc->sc_link.scsipi_scsi.channel = SCSI_CHANNEL_ONLY_ONE;
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.scsipi_scsi.adapter_target = apd->sc_scsi_dev;
	sc->sc_link.adapter = &aha_switch;
	sc->sc_link.device = &aha_dev;
	sc->sc_link.openings = 2;
	sc->sc_link.scsipi_scsi.max_target = 7;
	sc->sc_link.type = BUS_SCSI;

	aha_inquire_setup_information(sc);
	aha_init(sc);

	/*
	 * ask the adapter what subunits are present
	 */
	config_found(&sc->sc_dev, &sc->sc_link, scsiprint);
}

integrate void
aha_finish_ccbs(sc)
	struct aha_softc *sc;
{
	struct aha_mbx_in *wmbi;
	struct aha_ccb *ccb;
	int i;

	wmbi = wmbx->tmbi;

	if (wmbi->stat == AHA_MBI_FREE) {
		for (i = 0; i < AHA_MBX_SIZE; i++) {
			if (wmbi->stat != AHA_MBI_FREE) {
				printf("%s: mbi not in round-robin order\n",
				    sc->sc_dev.dv_xname);
				goto AGAIN;
			}
			aha_nextmbx(wmbi, wmbx, mbi);
		}
#ifdef AHADIAGnot
		printf("%s: mbi interrupt with no full mailboxes\n",
		    sc->sc_dev.dv_xname);
#endif
		return;
	}

AGAIN:
	do {
		ccb = aha_ccb_phys_kv(sc, phystol(wmbi->ccb_addr));
		if (!ccb) {
			printf("%s: bad mbi ccb pointer; skipping\n",
			    sc->sc_dev.dv_xname);
			goto next;
		}

#ifdef AHADEBUG
		if (aha_debug) {
			u_char *cp = &ccb->scsi_cmd;
			printf("op=%x %x %x %x %x %x\n",
			    cp[0], cp[1], cp[2], cp[3], cp[4], cp[5]);
			printf("stat %x for mbi addr = 0x%08x, ",
			    wmbi->stat, wmbi);
			printf("ccb addr = 0x%x\n", ccb);
		}
#endif /* AHADEBUG */

		switch (wmbi->stat) {
		case AHA_MBI_OK:
		case AHA_MBI_ERROR:
			if ((ccb->flags & CCB_ABORT) != 0) {
				/*
				 * If we already started an abort, wait for it
				 * to complete before clearing the CCB.  We
				 * could instead just clear CCB_SENDING, but
				 * what if the mailbox was already received?
				 * The worst that happens here is that we clear
				 * the CCB a bit later than we need to.  BFD.
				 */
				goto next;
			}
			break;

		case AHA_MBI_ABORT:
		case AHA_MBI_UNKNOWN:
			/*
			 * Even if the CCB wasn't found, we clear it anyway.
			 * See preceeding comment.
			 */
			break;

		default:
			printf("%s: bad mbi status %02x; skipping\n",
			    sc->sc_dev.dv_xname, wmbi->stat);
			goto next;
		}

		untimeout(aha_timeout, ccb);
		aha_done(sc, ccb);

	next:
		wmbi->stat = AHA_MBI_FREE;
		aha_nextmbx(wmbi, wmbx, mbi);
	} while (wmbi->stat != AHA_MBI_FREE);

	wmbx->tmbi = wmbi;
}

/*
 * Catch an interrupt from the adaptor
 */
int
aha_intr(arg)
	void *arg;
{
	struct aha_softc *sc = arg;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_char sts;

#ifdef AHADEBUG
	printf("%s: aha_intr ", sc->sc_dev.dv_xname);
#endif /*AHADEBUG */

	/*
	 * First acknowlege the interrupt, Then if it's not telling about
	 * a completed operation just return.
	 */
	sts = bus_space_read_1(iot, ioh, AHA_INTR_PORT);
	if ((sts & AHA_INTR_ANYINTR) == 0)
		return (0);
	bus_space_write_1(iot, ioh, AHA_CTRL_PORT, AHA_CTRL_IRST);

#ifdef AHADIAG
	/* Make sure we clear CCB_SENDING before finishing a CCB. */
	aha_collect_mbo(sc);
#endif

	/* Mail box out empty? */
	if (sts & AHA_INTR_MBOA) {
		struct aha_toggle toggle;

		toggle.cmd.opcode = AHA_MBO_INTR_EN;
		toggle.cmd.enable = 0;
		aha_cmd(iot, ioh, sc,
		    sizeof(toggle.cmd), (u_char *)&toggle.cmd,
		    0, (u_char *)0);
		aha_start_ccbs(sc);
	}

	/* Mail box in full? */
	if (sts & AHA_INTR_MBIF)
		aha_finish_ccbs(sc);

	return (1);
}

integrate void
aha_reset_ccb(sc, ccb)
	struct aha_softc *sc;
	struct aha_ccb *ccb;
{

	ccb->flags = 0;
}

/*
 * A ccb is put onto the free list.
 */
void
aha_free_ccb(sc, ccb)
	struct aha_softc *sc;
	struct aha_ccb *ccb;
{
	int s;

	s = splbio();

	aha_reset_ccb(sc, ccb);
	TAILQ_INSERT_HEAD(&sc->sc_free_ccb, ccb, chain);

	/*
	 * If there were none, wake anybody waiting for one to come free,
	 * starting with queued entries.
	 */
	if (ccb->chain.tqe_next == 0)
		wakeup(&sc->sc_free_ccb);

	splx(s);
}

integrate int
aha_init_ccb(sc, ccb)
	struct aha_softc *sc;
	struct aha_ccb *ccb;
{
	bus_dma_tag_t dmat = sc->sc_dmat;
	int hashnum, error;

	/*
	 * XXX Should we put a DIAGNOSTIC check for multiple
	 * XXX CCB inits here?
	 */

	bzero(ccb, sizeof(struct aha_ccb));

	/*
	 * Create DMA maps for this CCB.
	 */
	error = bus_dmamap_create(dmat, sizeof(struct aha_ccb), 1,
	    sizeof(struct aha_ccb), 0, BUS_DMA_NOWAIT, &ccb->dmamap_self);
	if (error) {
		printf("%s: can't create ccb dmamap_self\n",
		    sc->sc_dev.dv_xname);
		return (error);
	}

	error = bus_dmamap_create(dmat, AHA_MAXXFER, AHA_NSEG, AHA_MAXXFER,
	    0, BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW, &ccb->dmamap_xfer);
	if (error) {
		printf("%s: can't create ccb dmamap_xfer\n",
		    sc->sc_dev.dv_xname);
		bus_dmamap_destroy(dmat, ccb->dmamap_self);
		return (error);
	}

	/*
	 * Load the permanent DMA maps.
	 */
	error = bus_dmamap_load(dmat, ccb->dmamap_self, ccb,
	    sizeof(struct aha_ccb), NULL, BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: can't load ccb dmamap_self\n",
		    sc->sc_dev.dv_xname);
		bus_dmamap_destroy(dmat, ccb->dmamap_self);
		bus_dmamap_destroy(dmat, ccb->dmamap_xfer);
		return (error);
	}

	/*
	 * put in the phystokv hash table
	 * Never gets taken out.
	 */
	ccb->hashkey = ccb->dmamap_self->dm_segs[0].ds_addr;
	hashnum = CCB_HASH(ccb->hashkey);
	ccb->nexthash = sc->sc_ccbhash[hashnum];
	sc->sc_ccbhash[hashnum] = ccb;
	aha_reset_ccb(sc, ccb);
	return (0);
}

/*
 * Create a set of ccbs and add them to the free list.
 */
int
aha_create_ccbs(sc, mem, size, max_ccbs)
	struct aha_softc *sc;
	void *mem;
	size_t size;
	int max_ccbs;
{
	bus_dma_segment_t seg;
	struct aha_ccb *ccb;
	int rseg, error;

	if (sc->sc_numccbs >= AHA_CCB_MAX)
		return (0);

	if (max_ccbs > AHA_CCB_MAX)
		max_ccbs = AHA_CCB_MAX;

	if ((ccb = mem) != NULL)
		goto have_mem;

	size = NBPG;
	error = bus_dmamem_alloc(sc->sc_dmat, size, NBPG, 0, &seg, 1, &rseg,
	    BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: can't allocate memory for ccbs\n",
		    sc->sc_dev.dv_xname);
		return (error);
	}

	error = bus_dmamem_map(sc->sc_dmat, &seg, rseg, size,
	    (caddr_t *)&ccb, BUS_DMA_NOWAIT|BUS_DMAMEM_NOSYNC);
	if (error) {
		printf("%s: can't map memory for ccbs\n",
		    sc->sc_dev.dv_xname);
		bus_dmamem_free(sc->sc_dmat, &seg, rseg);
		return (error);
	}

 have_mem:
	bzero(ccb, size);
	while (size > sizeof(struct aha_ccb) && sc->sc_numccbs < max_ccbs) {
		error = aha_init_ccb(sc, ccb);
		if (error) {
			printf("%s: can't initialize ccb\n",
			    sc->sc_dev.dv_xname);
			return (error);
		}
		TAILQ_INSERT_TAIL(&sc->sc_free_ccb, ccb, chain);
		(caddr_t)ccb += ALIGN(sizeof(struct aha_ccb));
		size -= ALIGN(sizeof(struct aha_ccb));
		sc->sc_numccbs++;
	}

	return (0);
}

/*
 * Get a free ccb
 *
 * If there are none, see if we can allocate a new one.  If so, put it in
 * the hash table too otherwise either return an error or sleep.
 */
struct aha_ccb *
aha_get_ccb(sc, flags)
	struct aha_softc *sc;
	int flags;
{
	struct aha_ccb *ccb;
	int s;

	s = splbio();

	/*
	 * If we can and have to, sleep waiting for one to come free
	 * but only if we can't allocate a new one.
	 */
	for (;;) {
		ccb = sc->sc_free_ccb.tqh_first;
		if (ccb) {
			TAILQ_REMOVE(&sc->sc_free_ccb, ccb, chain);
			break;
		}
		if (sc->sc_numccbs < AHA_CCB_MAX) {
			/*
			 * aha_create_ccbs() might have managed to create
			 * one before it failed.  If so, don't abort,
			 * just grab it and continue to hobble along.
			 */
			if (aha_create_ccbs(sc, NULL, 0, AHA_CCB_MAX) != 0 &&
			    sc->sc_free_ccb.tqh_first == NULL) {
				printf("%s: can't allocate ccbs\n",
				    sc->sc_dev.dv_xname);
				goto out;
			}
			continue;
		}
		if ((flags & SCSI_NOSLEEP) != 0)
			goto out;
		tsleep(&sc->sc_free_ccb, PRIBIO, "ahaccb", 0);
	}

	ccb->flags |= CCB_ALLOC;

out:
	splx(s);
	return (ccb);
}

/*
 * Given a physical address, find the ccb that it corresponds to.
 */
struct aha_ccb *
aha_ccb_phys_kv(sc, ccb_phys)
	struct aha_softc *sc;
	u_long ccb_phys;
{
	int hashnum = CCB_HASH(ccb_phys);
	struct aha_ccb *ccb = sc->sc_ccbhash[hashnum];

	while (ccb) {
		if (ccb->hashkey == ccb_phys)
			break;
		ccb = ccb->nexthash;
	}
	return (ccb);
}

/*
 * Queue a CCB to be sent to the controller, and send it if possible.
 */
void
aha_queue_ccb(sc, ccb)
	struct aha_softc *sc;
	struct aha_ccb *ccb;
{

	TAILQ_INSERT_TAIL(&sc->sc_waiting_ccb, ccb, chain);
	aha_start_ccbs(sc);
}

/*
 * Garbage collect mailboxes that are no longer in use.
 */
void
aha_collect_mbo(sc)
	struct aha_softc *sc;
{
	struct aha_mbx_out *wmbo;	/* Mail Box Out pointer */
#ifdef AHADIAG
	struct aha_ccb *ccb;
#endif

	wmbo = wmbx->cmbo;

	while (sc->sc_mbofull > 0) {
		if (wmbo->cmd != AHA_MBO_FREE)
			break;

#ifdef AHADIAG
		ccb = aha_ccb_phys_kv(sc, phystol(wmbo->ccb_addr));
		ccb->flags &= ~CCB_SENDING;
#endif

		--sc->sc_mbofull;
		aha_nextmbx(wmbo, wmbx, mbo);
	}

	wmbx->cmbo = wmbo;
}

/*
 * Send as many CCBs as we have empty mailboxes for.
 */
void
aha_start_ccbs(sc)
	struct aha_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct aha_mbx_out *wmbo;	/* Mail Box Out pointer */
	struct aha_ccb *ccb;

	wmbo = wmbx->tmbo;

	while ((ccb = sc->sc_waiting_ccb.tqh_first) != NULL) {
		if (sc->sc_mbofull >= AHA_MBX_SIZE) {
			aha_collect_mbo(sc);
			if (sc->sc_mbofull >= AHA_MBX_SIZE) {
				struct aha_toggle toggle;

				toggle.cmd.opcode = AHA_MBO_INTR_EN;
				toggle.cmd.enable = 1;
				aha_cmd(iot, ioh, sc,
				    sizeof(toggle.cmd), (u_char *)&toggle.cmd,
				    0, (u_char *)0);
				break;
			}
		}

		TAILQ_REMOVE(&sc->sc_waiting_ccb, ccb, chain);
#ifdef AHADIAG
		ccb->flags |= CCB_SENDING;
#endif

		/* Link ccb to mbo. */
		ltophys(ccb->dmamap_self->dm_segs[0].ds_addr, wmbo->ccb_addr);
		if (ccb->flags & CCB_ABORT)
			wmbo->cmd = AHA_MBO_ABORT;
		else
			wmbo->cmd = AHA_MBO_START;

		/* Tell the card to poll immediately. */
		bus_space_write_1(iot, ioh, AHA_CMD_PORT, AHA_START_SCSI);

		if ((ccb->xs->flags & SCSI_POLL) == 0)
			timeout(aha_timeout, ccb, (ccb->timeout * hz) / 1000);

		++sc->sc_mbofull;
		aha_nextmbx(wmbo, wmbx, mbo);
	}

	wmbx->tmbo = wmbo;
}

/*
 * We have a ccb which has been processed by the
 * adaptor, now we look to see how the operation
 * went. Wake up the owner if waiting
 */
void
aha_done(sc, ccb)
	struct aha_softc *sc;
	struct aha_ccb *ccb;
{
	bus_dma_tag_t dmat = sc->sc_dmat;
	struct scsipi_sense_data *s1, *s2;
	struct scsipi_xfer *xs = ccb->xs;

	SC_DEBUG(xs->sc_link, SDEV_DB2, ("aha_done\n"));

	/*
	 * If we were a data transfer, unload the map that described
	 * the data buffer.
	 */
	if (xs->datalen) {
		bus_dmamap_sync(dmat, ccb->dmamap_xfer,
		    (xs->flags & SCSI_DATA_IN) ? BUS_DMASYNC_POSTREAD :
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(dmat, ccb->dmamap_xfer);
	}

	/*
	 * Otherwise, put the results of the operation
	 * into the xfer and call whoever started it
	 */
#ifdef AHADIAG
	if (ccb->flags & CCB_SENDING) {
		printf("%s: exiting ccb still in transit!\n", sc->sc_dev.dv_xname);
		Debugger();
		return;
	}
#endif
	if ((ccb->flags & CCB_ALLOC) == 0) {
		printf("%s: exiting ccb not allocated!\n", sc->sc_dev.dv_xname);
		Debugger();
		return;
	}
	if (xs->error == XS_NOERROR) {
		if (ccb->host_stat != AHA_OK) {
			switch (ccb->host_stat) {
			case AHA_SEL_TIMEOUT:	/* No response */
				xs->error = XS_SELTIMEOUT;
				break;
			default:	/* Other scsi protocol messes */
				printf("%s: host_stat %x\n",
				    sc->sc_dev.dv_xname, ccb->host_stat);
				xs->error = XS_DRIVER_STUFFUP;
				break;
			}
		} else if (ccb->target_stat != SCSI_OK) {
			switch (ccb->target_stat) {
			case SCSI_CHECK:
				s1 = (struct scsipi_sense_data *) (((char *) (&ccb->scsi_cmd)) +
				    ccb->scsi_cmd_length);
				s2 = &xs->sense.scsi_sense;
				*s2 = *s1;
				xs->error = XS_SENSE;
				break;
			case SCSI_BUSY:
				xs->error = XS_BUSY;
				break;
			default:
				printf("%s: target_stat %x\n",
				    sc->sc_dev.dv_xname, ccb->target_stat);
				xs->error = XS_DRIVER_STUFFUP;
				break;
			}
		} else
			xs->resid = 0;
	}
	aha_free_ccb(sc, ccb);
	xs->flags |= ITSDONE;
	scsipi_done(xs);

	/*
	 * If there are queue entries in the software queue, try to
	 * run the first one.  We should be more or less guaranteed
	 * to succeed, since we just freed a CCB.
	 *
	 * NOTE: aha_scsi_cmd() relies on our calling it with
	 * the first entry in the queue.
	 */
	if ((xs = sc->sc_queue.lh_first) != NULL)
		(void) aha_scsi_cmd(xs);
}

/*
 * Find the board and find its irq/drq
 */
int
aha_find(iot, ioh, sc)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct aha_probe_data *sc;
{
	int i;
	u_char sts;
	struct aha_config config;
	int irq, drq;

	/*
	 * reset board, If it doesn't respond, assume
	 * that it's not there.. good for the probe
	 */

	bus_space_write_1(iot, ioh, AHA_CTRL_PORT, AHA_CTRL_HRST | AHA_CTRL_SRST);

	delay(100);
	for (i = AHA_RESET_TIMEOUT; i; i--) {
		sts = bus_space_read_1(iot, ioh, AHA_STAT_PORT);
		if (sts == (AHA_STAT_IDLE | AHA_STAT_INIT))
			break;
		delay(1000);	/* calibrated in msec */
	}
	if (!i) {
#ifdef AHADEBUG
		if (aha_debug)
			printf("aha_find: No answer from adaptec board\n");
#endif /* AHADEBUG */
		return (0);
	}

	/*
	 * setup dma channel from jumpers and save int
	 * level
	 */
	delay(1000);		/* for Bustek 545 */
	config.cmd.opcode = AHA_INQUIRE_CONFIG;
	aha_cmd(iot, ioh, (struct aha_softc *)0,
	    sizeof(config.cmd), (u_char *)&config.cmd,
	    sizeof(config.reply), (u_char *)&config.reply);
	switch (config.reply.chan) {
	case EISADMA:
		drq = -1;
		break;
	case CHAN0:
		drq = 0;
		break;
	case CHAN5:
		drq = 5;
		break;
	case CHAN6:
		drq = 6;
		break;
	case CHAN7:
		drq = 7;
		break;
	default:
		printf("aha_find: illegal drq setting %x\n", config.reply.chan);
		return (0);
	}

	switch (config.reply.intr) {
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
		printf("aha_find: illegal irq setting %x\n", config.reply.intr);
		return (0);
	}

	if (sc) {
		sc->sc_irq = irq;
		sc->sc_drq = drq;
		sc->sc_scsi_dev = config.reply.scsi_dev;
	}

	return (1);
}

/*
 * Start the board, ready for normal operation
 */
void
aha_init(sc)
	struct aha_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	bus_dma_segment_t seg;
	struct aha_devices devices;
	struct aha_setup setup;
	struct aha_mailbox mailbox;
	int i, j, initial_ccbs, rseg;

	/*
	 * XXX
	 * If we are a 1542C or later, disable the extended BIOS so that the
	 * mailbox interface is unlocked.
	 * No need to check the extended BIOS flags as some of the
	 * extensions that cause us problems are not flagged in that byte.
	 */
	if (!strncmp(sc->sc_model, "1542C", 5)) {
		struct aha_extbios extbios;
		struct aha_unlock unlock;

		printf("%s: unlocking mailbox interface\n", sc->sc_dev.dv_xname);
		extbios.cmd.opcode = AHA_EXT_BIOS;
		aha_cmd(iot, ioh, sc,
		    sizeof(extbios.cmd), (u_char *)&extbios.cmd,
		    sizeof(extbios.reply), (u_char *)&extbios.reply);

#ifdef AHADEBUG
		printf("%s: flags=%02x, mailboxlock=%02x\n",
		    sc->sc_dev.dv_xname,
		    extbios.reply.flags, extbios.reply.mailboxlock);
#endif /* AHADEBUG */

		unlock.cmd.opcode = AHA_MBX_ENABLE;
		unlock.cmd.junk = 0;
		unlock.cmd.magic = extbios.reply.mailboxlock;
		aha_cmd(iot, ioh, sc,
		    sizeof(unlock.cmd), (u_char *)&unlock.cmd,
		    0, (u_char *)0);
	}

#if 0
	/*
	 * Change the bus on/off times to not clash with other dma users.
	 */
	aha_cmd(iot, ioh, 1, 0, 0, 0, AHA_BUS_ON_TIME_SET, 7);
	aha_cmd(iot, ioh, 1, 0, 0, 0, AHA_BUS_OFF_TIME_SET, 4);
#endif

	/* Inquire Installed Devices (to force synchronous negotiation). */
	devices.cmd.opcode = AHA_INQUIRE_DEVICES;
	aha_cmd(iot, ioh, sc,
	    sizeof(devices.cmd), (u_char *)&devices.cmd,
	    sizeof(devices.reply), (u_char *)&devices.reply);

	/* Count installed units */
	initial_ccbs = 0;
	for (i = 0; i < 8; i++) {
		for (j = 0; j < 8; j++) {
			if (((devices.reply.lun_map[i] >> j) & 1) == 1)
				initial_ccbs += 1;
		}
	}
	initial_ccbs *= sc->sc_link.openings;

	/* Obtain setup information from. */
	setup.cmd.opcode = AHA_INQUIRE_SETUP;
	setup.cmd.len = sizeof(setup.reply);
	aha_cmd(iot, ioh, sc,
	    sizeof(setup.cmd), (u_char *)&setup.cmd,
	    sizeof(setup.reply), (u_char *)&setup.reply);

	printf("%s: %s, %s\n",
	    sc->sc_dev.dv_xname,
	    setup.reply.sync_neg ? "sync" : "async",
	    setup.reply.parity ? "parity" : "no parity");

	for (i = 0; i < 8; i++) {
		if (!setup.reply.sync[i].valid ||
		    (!setup.reply.sync[i].offset && !setup.reply.sync[i].period))
			continue;
		printf("%s targ %d: sync, offset %d, period %dnsec\n",
		    sc->sc_dev.dv_xname, i,
		    setup.reply.sync[i].offset, setup.reply.sync[i].period * 50 + 200);
	}

	/*
	 * Allocate the mailbox.
	 */
	if (bus_dmamem_alloc(sc->sc_dmat, NBPG, NBPG, 0, &seg, 1,
	    &rseg, BUS_DMA_NOWAIT) ||
	    bus_dmamem_map(sc->sc_dmat, &seg, rseg, NBPG,
	    (caddr_t *)&wmbx, BUS_DMA_NOWAIT|BUS_DMAMEM_NOSYNC))
		panic("aha_init: can't create or map mailbox");

	/*
	 * Since DMA memory allocation is always rounded up to a
	 * page size, create some ccbs from the leftovers.
	 */
	if (aha_create_ccbs(sc, ((caddr_t)wmbx) +
	    ALIGN(sizeof(struct aha_mbx)),
	    NBPG - ALIGN(sizeof(struct aha_mbx)), initial_ccbs))
		panic("aha_init: can't create ccbs");

	/*
	 * Create and load the mailbox DMA map.
	 */
	if (bus_dmamap_create(sc->sc_dmat, sizeof(struct aha_mbx), 1,
	    sizeof(struct aha_mbx), 0, BUS_DMA_NOWAIT, &sc->sc_dmamap_mbox) ||
	    bus_dmamap_load(sc->sc_dmat, sc->sc_dmamap_mbox, wmbx,
	    sizeof(struct aha_mbx), NULL, BUS_DMA_NOWAIT))
		panic("aha_init: can't create or load mailbox dma map");

	/*
	 * Set up initial mail box for round-robin operation.
	 */
	for (i = 0; i < AHA_MBX_SIZE; i++) {
		wmbx->mbo[i].cmd = AHA_MBO_FREE;
		wmbx->mbi[i].stat = AHA_MBI_FREE;
	}
	wmbx->cmbo = wmbx->tmbo = &wmbx->mbo[0];
	wmbx->tmbi = &wmbx->mbi[0];
	sc->sc_mbofull = 0;

	/* Initialize mail box. */
	mailbox.cmd.opcode = AHA_MBX_INIT;
	mailbox.cmd.nmbx = AHA_MBX_SIZE;
	ltophys(sc->sc_dmamap_mbox->dm_segs[0].ds_addr, mailbox.cmd.addr);
	aha_cmd(iot, ioh, sc,
	    sizeof(mailbox.cmd), (u_char *)&mailbox.cmd,
	    0, (u_char *)0);
}

void
aha_inquire_setup_information(sc)
	struct aha_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct aha_revision revision;
	u_char sts;
	int i;
	char *p;

	strcpy(sc->sc_model, "unknown");

	/*
	 * Assume we have a board at this stage, do an adapter inquire
	 * to find out what type of controller it is.  If the command
	 * fails, we assume it's either a crusty board or an old 1542
	 * clone, and skip the board-specific stuff.
	 */
	revision.cmd.opcode = AHA_INQUIRE_REVISION;
	if (aha_cmd(iot, ioh, sc,
	    sizeof(revision.cmd), (u_char *)&revision.cmd,
	    sizeof(revision.reply), (u_char *)&revision.reply)) {
		/*
		 * aha_cmd() already started the reset.  It's not clear we
		 * even need to bother here.
		 */
		for (i = AHA_RESET_TIMEOUT; i; i--) {
			sts = bus_space_read_1(iot, ioh, AHA_STAT_PORT);
			if (sts == (AHA_STAT_IDLE | AHA_STAT_INIT))
				break;
			delay(1000);
		}
		if (!i) {
#ifdef AHADEBUG
			printf("aha_init: soft reset failed\n");
#endif /* AHADEBUG */
			return;
		}
#ifdef AHADEBUG
		printf("aha_init: inquire command failed\n");
#endif /* AHADEBUG */
		goto noinquire;
	}

#ifdef AHADEBUG
	printf("%s: inquire %x, %x, %x, %x\n",
	    sc->sc_dev.dv_xname,
	    revision.reply.boardid, revision.reply.spec_opts,
	    revision.reply.revision_1, revision.reply.revision_2);
#endif /* AHADEBUG */

	switch (revision.reply.boardid) {
	case 0x31:
		strcpy(sc->sc_model, "1540");
		break;
	case 0x41:
		strcpy(sc->sc_model, "1540A/1542A/1542B");
		break;
	case 0x42:
		strcpy(sc->sc_model, "1640");
		break;
	case 0x43:
		strcpy(sc->sc_model, "1542C");
		break;
	case 0x44:
	case 0x45:
		strcpy(sc->sc_model, "1542CF");
		break;
	case 0x46:
		strcpy(sc->sc_model, "1542CP");
		break;
	}

	p = sc->sc_firmware;
	*p++ = revision.reply.revision_1;
	*p++ = '.';
	*p++ = revision.reply.revision_2;
	*p = '\0';

noinquire:
	printf("%s: model AHA-%s, firmware %s\n",
	       sc->sc_dev.dv_xname,
	       sc->sc_model, sc->sc_firmware);
}

void
ahaminphys(bp)
	struct buf *bp;
{

	if (bp->b_bcount > AHA_MAXXFER)
		bp->b_bcount = AHA_MAXXFER;
	minphys(bp);
}

/*
 * start a scsi operation given the command and the data address. Also needs
 * the unit, target and lu.
 */
int
aha_scsi_cmd(xs)
	struct scsipi_xfer *xs;
{
	struct scsipi_link *sc_link = xs->sc_link;
	struct aha_softc *sc = sc_link->adapter_softc;
	bus_dma_tag_t dmat = sc->sc_dmat;
	struct aha_ccb *ccb;
	int error, seg, flags, s;
	int fromqueue = 0, dontqueue = 0;

	SC_DEBUG(sc_link, SDEV_DB2, ("aha_scsi_cmd\n"));

	s = splbio();		/* protect the queue */

	/*
	 * If we're running the queue from aha_done(), we've been
	 * called with the first queue entry as our argument.
	 */
	if (xs == sc->sc_queue.lh_first) {
		xs = aha_dequeue(sc);
		fromqueue = 1;
		goto get_ccb;
	}

	/* Polled requests can't be queued for later. */
	dontqueue = xs->flags & SCSI_POLL;

	/*
	 * If there are jobs in the queue, run them first.
	 */
	if (sc->sc_queue.lh_first != NULL) {
		/*
		 * If we can't queue, we have to abort, since
		 * we have to preserve order.
		 */
		if (dontqueue) {
			splx(s);
			xs->error = XS_DRIVER_STUFFUP;
			return (TRY_AGAIN_LATER);
		}

		/*
		 * Swap with the first queue entry.
		 */
		aha_enqueue(sc, xs, 0);
		xs = aha_dequeue(sc);
		fromqueue = 1;
	}

 get_ccb:
	/*
	 * get a ccb to use. If the transfer
	 * is from a buf (possibly from interrupt time)
	 * then we can't allow it to sleep
	 */
	flags = xs->flags;
	if ((ccb = aha_get_ccb(sc, flags)) == NULL) {
		/*
		 * If we can't queue, we lose.
		 */
		if (dontqueue) {
			splx(s);
			xs->error = XS_DRIVER_STUFFUP;
			return (TRY_AGAIN_LATER);
		}

		/*
		 * Stuff ourselves into the queue, in front
		 * if we came off in the first place.
		 */
		aha_enqueue(sc, xs, fromqueue);
		splx(s);
		return (SUCCESSFULLY_QUEUED);
	}

	splx(s);		/* done playing with the queue */

	ccb->xs = xs;
	ccb->timeout = xs->timeout;

	/*
	 * Put all the arguments for the xfer in the ccb
	 */
	if (flags & SCSI_RESET) {
		ccb->opcode = AHA_RESET_CCB;
		ccb->scsi_cmd_length = 0;
	} else {
		/* can't use S/G if zero length */
		ccb->opcode = (xs->datalen ? AHA_INIT_SCAT_GATH_CCB
					   : AHA_INITIATOR_CCB);
		bcopy(xs->cmd, &ccb->scsi_cmd,
		    ccb->scsi_cmd_length = xs->cmdlen);
	}

	if (xs->datalen) {
		/*
		 * Map the DMA transfer.
		 */
#ifdef TFS
		if (flags & SCSI_DATA_UIO) {
			error = bus_dmamap_load_uio(dmat,
			    ccb->dmamap_xfer, (struct uio *)xs->data,
			    (flags & SCSI_NOSLEEP) ? BUS_DMA_NOWAIT :
			    BUS_DMA_WAITOK);
		} else
#endif
		{
			error = bus_dmamap_load(dmat,
			    ccb->dmamap_xfer, xs->data, xs->datalen, NULL,
			    (flags & SCSI_NOSLEEP) ? BUS_DMA_NOWAIT :
			    BUS_DMA_WAITOK);
		}
		
		if (error) {
			if (error == EFBIG) {
				printf("%s: aha_scsi_cmd, more than %d"
				    " dma segments\n",
				    sc->sc_dev.dv_xname, AHA_NSEG);
			} else {
				printf("%s: aha_scsi_cmd, error %d loading"
				    " dma map\n",
				    sc->sc_dev.dv_xname, error);
			}
			goto bad;
		}

		bus_dmamap_sync(dmat, ccb->dmamap_xfer,
		    (flags & SCSI_DATA_IN) ? BUS_DMASYNC_PREREAD :
		    BUS_DMASYNC_PREWRITE);

		/*
		 * Load the hardware scatter/gather map with the
		 * contents of the DMA map.
		 */
		for (seg = 0; seg < ccb->dmamap_xfer->dm_nsegs; seg++) {
			ltophys(ccb->dmamap_xfer->dm_segs[seg].ds_addr,
			    ccb->scat_gath[seg].seg_addr);
			ltophys(ccb->dmamap_xfer->dm_segs[seg].ds_len,
			    ccb->scat_gath[seg].seg_len);
		}

		ltophys(ccb->dmamap_self->dm_segs[0].ds_addr +
		    offsetof(struct aha_ccb, scat_gath), ccb->data_addr);
		ltophys(ccb->dmamap_xfer->dm_nsegs *
		    sizeof(struct aha_scat_gath), ccb->data_length);
	} else {
		/*
		 * No data xfer, use non S/G values.
		 */
		ltophys(0, ccb->data_addr);
		ltophys(0, ccb->data_length);
	}

	ccb->data_out = 0;
	ccb->data_in = 0;
	ccb->target = sc_link->scsipi_scsi.target;
	ccb->lun = sc_link->scsipi_scsi.lun;
	ccb->req_sense_length = sizeof(ccb->scsi_sense);
	ccb->host_stat = 0x00;
	ccb->target_stat = 0x00;
	ccb->link_id = 0;
	ltophys(0, ccb->link_addr);

	s = splbio();
	aha_queue_ccb(sc, ccb);
	splx(s);

	/*
	 * Usually return SUCCESSFULLY QUEUED
	 */
	SC_DEBUG(sc_link, SDEV_DB3, ("cmd_sent\n"));
	if ((flags & SCSI_POLL) == 0)
		return (SUCCESSFULLY_QUEUED);

	/*
	 * If we can't use interrupts, poll on completion
	 */
	if (aha_poll(sc, xs, ccb->timeout)) {
		aha_timeout(ccb);
		if (aha_poll(sc, xs, ccb->timeout))
			aha_timeout(ccb);
	}
	return (COMPLETE);

bad:
	xs->error = XS_DRIVER_STUFFUP;
	aha_free_ccb(sc, ccb);
	return (COMPLETE);
}

/*
 * Poll a particular unit, looking for a particular xs
 */
int
aha_poll(sc, xs, count)
	struct aha_softc *sc;
	struct scsipi_xfer *xs;
	int count;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	/* timeouts are in msec, so we loop in 1000 usec cycles */
	while (count) {
		/*
		 * If we had interrupts enabled, would we
		 * have got an interrupt?
		 */
		if (bus_space_read_1(iot, ioh, AHA_INTR_PORT) & AHA_INTR_ANYINTR)
			aha_intr(sc);
		if (xs->flags & ITSDONE)
			return (0);
		delay(1000);	/* only happens in boot so ok */
		count--;
	}
	return (1);
}

void
aha_timeout(arg)
	void *arg;
{
	struct aha_ccb *ccb = arg;
	struct scsipi_xfer *xs = ccb->xs;
	struct scsipi_link *sc_link = xs->sc_link;
	struct aha_softc *sc = sc_link->adapter_softc;
	int s;

	scsi_print_addr(sc_link);
	printf("timed out");

	s = splbio();

#ifdef AHADIAG
	/*
	 * If The ccb's mbx is not free, then the board has gone south?
	 */
	aha_collect_mbo(sc);
	if (ccb->flags & CCB_SENDING) {
		printf("%s: not taking commands!\n", sc->sc_dev.dv_xname);
		Debugger();
	}
#endif

	/*
	 * If it has been through before, then
	 * a previous abort has failed, don't
	 * try abort again
	 */
	if (ccb->flags & CCB_ABORT) {
		/* abort timed out */
		printf(" AGAIN\n");
		/* XXX Must reset! */
	} else {
		/* abort the operation that has timed out */
		printf("\n");
		ccb->xs->error = XS_TIMEOUT;
		ccb->timeout = AHA_ABORT_TIMEOUT;
		ccb->flags |= CCB_ABORT;
		aha_queue_ccb(sc, ccb);
	}

	splx(s);
}
