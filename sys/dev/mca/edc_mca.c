/*	$NetBSD: edc_mca.c,v 1.9.2.3 2001/11/14 19:15:00 nathanw Exp $	*/

/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jaromir Dolecek.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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
 * Driver for MCA ESDI controllers and disks conforming to IBM DASD
 * spec.
 *
 * The driver was written with DASD Storage Interface Specification
 * for MCA rev. 2.2 in hands, thanks to Scott Telford <st@epcc.ed.ac.uk>.
 *
 * TODO:
 * - move the MCA DMA controller (edc_setup_dma()) goo to device driver
 *   independant location
 * - improve error recovery
 *   add any soft resets when anything gets stuck?
 * - test with > 1 disk (this is supported by some controllers), eliminate
 *   any remaining devno=0 assumptions if there are any still
 * - test with > 1 ESDI controller in machine; shared interrupts
 *   necessary for this to work should be supported - edc_intr() specifically
 *   checks if the interrupt is for this controller
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: edc_mca.c,v 1.9.2.3 2001/11/14 19:15:00 nathanw Exp $");

#include "rnd.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/endian.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/syslog.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/kernel.h>
#if NRND > 0
#include <sys/rnd.h>
#endif

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/mca/mcareg.h>
#include <dev/mca/mcavar.h>
#include <dev/mca/mcadevs.h>

#include <dev/mca/edcreg.h>
#include <dev/mca/edvar.h>
#include <dev/mca/edcvar.h>

#define EDC_ATTN_MAXTRIES	10000	/* How many times check for unbusy */

struct edc_mca_softc {
	struct device sc_dev;

	bus_space_tag_t	sc_iot;
	bus_space_handle_t sc_ioh;

	bus_dma_tag_t sc_dmat;		/* DMA tag as passed by parent */
	bus_space_handle_t sc_dmaextcmdh;
	bus_space_handle_t sc_dmaexech;

	void	*sc_ih;				/* interrupt handle */
	int	sc_drq;				/* DRQ number */
	int	sc_cmd_async;			/* asynchronous cmd pending */

	int	sc_flags;
#define	DASD_QUIET	0x01		/* don't dump cmd error info */
#define DASD_MAXDEVS	8
	struct ed_softc *sc_ed[DASD_MAXDEVS];
	struct ed_softc sc_controller;
};

int	edc_mca_probe	__P((struct device *, struct cfdata *, void *));
void	edc_mca_attach	__P((struct device *, struct device *, void *));

struct cfattach edc_mca_ca = {
	sizeof(struct edc_mca_softc), edc_mca_probe, edc_mca_attach
};

#define DMA_EXTCMD	0x18
#define DMA_EXEC	0x1A

static int	edc_intr __P((void *));
static void	edc_dump_status_block __P((struct edc_mca_softc *, int, int));
static int	edc_setup_dma __P((struct edc_mca_softc *, int,
			bus_addr_t, bus_size_t));
static int	edc_do_attn __P((struct edc_mca_softc *, int, int, int));
static int	edc_cmd_wait __P((struct edc_mca_softc *, int, int, int));

int
edc_mca_probe(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct mca_attach_args *ma = aux;

	switch (ma->ma_id) {
	case MCA_PRODUCT_IBM_ESDIC:
	case MCA_PRODUCT_IBM_ESDIC_IG:
		return (1);
	default:
		return (0);
	}
}

void
edc_mca_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct edc_mca_softc *sc = (void *) self;
	struct mca_attach_args *ma = aux;
	int pos2, pos3, pos4;
	int irq, drq, iobase;
	const char *typestr;
	struct ed_softc *ed;
	struct ed_attach_args eda;
	int devno, maxdevs;

	pos2 = mca_conf_read(ma->ma_mc, ma->ma_slot, 2);
	pos3 = mca_conf_read(ma->ma_mc, ma->ma_slot, 3);
	pos4 = mca_conf_read(ma->ma_mc, ma->ma_slot, 4);

	/*
	 * POS register 2: (adf pos0)
	 * 
	 * 7 6 5 4 3 2 1 0
	 *   \ \____/  \ \__ enable: 0=adapter disabled, 1=adapter enabled
	 *    \     \   \___ Primary/Alternate Port Adresses:
	 *     \     \		0=0x3510-3517 1=0x3518-0x351f
	 *      \     \_____ DMA Arbitration Level: 0101=5 0110=6 0111=7
	 *       \              0000=0 0001=1 0011=3 0100=4
	 *        \_________ Fairness On/Off: 1=On 0=Off
	 *
	 * POS register 3: (adf pos1)
	 * 
	 * 7 6 5 4 3 2 1 0
	 * 0 0 \_/
	 *       \__________ DMA Burst Pacing Interval: 10=24ms 11=31ms
	 *                     01=16ms 00=Burst Disabled
	 *
	 * POS register 4: (adf pos2)
	 * 
	 * 7 6 5 4 3 2 1 0
	 *           \_/ \__ DMA Pacing Control: 1=Disabled 0=Enabled
	 *             \____ Time to Release: 1X=6ms 01=3ms 00=Immediate
	 *
	 * IRQ is fixed to 14 (0x0e).
	 */

	switch (ma->ma_id) {
	case MCA_PRODUCT_IBM_ESDIC:
		typestr = "IBM ESDI Fixed Disk Controller";
		break;
	case MCA_PRODUCT_IBM_ESDIC_IG:
		typestr = "IBM Integ. ESDI Fixed Disk & Controller";
		break;
	default:
		/* never reached */
	}
		
	irq = ESDIC_IRQ;
	iobase = (pos2 & IO_IS_ALT) ? ESDIC_IOALT : ESDIC_IOPRM;
	drq = (pos2 & DRQ_MASK) >> 2;

	printf(" slot %d irq %d drq %d: %s\n", ma->ma_slot+1,
		irq, drq, typestr);

#ifdef DIAGNOSTIC
	/*
	 * It's not strictly necessary to check this, machine configuration
	 * utility uses only valid adresses.
	 */
	if (drq == 2 || drq >= 8) {
		printf("%s: invalid DMA Arbitration Level %d\n",
			sc->sc_dev.dv_xname, drq);
		return;
	}
#endif

	printf("%s: Fairness %s, Release %s, ",
		sc->sc_dev.dv_xname,
		(pos2 & FAIRNESS_ENABLE) ? "On" : "Off",
		(pos4 & RELEASE_1) ? "6ms"
				: ((pos4 & RELEASE_2) ? "3ms" : "Immediate")
		);
	if ((pos4 & PACING_CTRL_DISABLE) == 0) {
		static const char * const pacint[] =
			{ "disabled", "16ms", "24ms", "31ms"};
		printf("DMA burst pacing interval %s\n",
			pacint[(pos3 & PACING_INT_MASK) >> 4]);
	} else
		printf("DMA pacing control disabled\n");

	sc->sc_iot = ma->ma_iot;
	sc->sc_drq = drq;

	if (bus_space_map(sc->sc_iot, iobase,
	    ESDIC_REG_NPORTS, 0, &sc->sc_ioh)) {
		printf("%s: couldn't map registers\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	if (bus_space_map(sc->sc_iot, DMA_EXTCMD, 1, 0, &sc->sc_dmaextcmdh)) {
		printf("%s: couldn't map registers\n",
		    sc->sc_dev.dv_xname);
		return;
	}
	if (bus_space_map(sc->sc_iot, DMA_EXEC, 1, 0, &sc->sc_dmaexech)) {
		printf("%s: couldn't map registers\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	sc->sc_dmat = ma->ma_dmat;

	sc->sc_ih = mca_intr_establish(ma->ma_mc, irq, IPL_BIO, edc_intr, sc);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt handler\n",
			sc->sc_dev.dv_xname);
		return;
	}

	/*
	 * Integrated ESDI controller supports only one disk, other
	 * controllers support two disks.
	 */
	if (ma->ma_id == MCA_PRODUCT_IBM_ESDIC_IG)
		maxdevs = 1;
	else
		maxdevs = 2;

	/*
	 * Initialize the controller ed softc. We could do without this,
	 * but absence of checks for controller devno simplifies code logic
	 * somewhat.
	 */
	sc->sc_ed[DASD_DEVNO_CONTROLLER] = &sc->sc_controller;
	strcpy(sc->sc_controller.sc_dev.dv_xname, sc->sc_dev.dv_xname);/*safe*/

	/*
	 * Reset controller and attach individual disks. ed attach routine
	 * uses polling so that this works with interrupts disabled.
	 */

	/* Do a reset to ensure sane state after warm boot. */
	if (bus_space_read_1(sc->sc_iot, sc->sc_ioh, BSR) & BSR_BUSY) {
		/* hard reset */
		printf("%s: controller busy, performing hardware reset ...\n",
			sc->sc_dev.dv_xname);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, BCR,
			BCR_INT_ENABLE|BCR_RESET);
	} else {
		/* "SOFT" reset */
		edc_do_attn(sc, ATN_RESET_ATTACHMENT, DASD_DEVNO_CONTROLLER,0);
	}
		
	/*
	 * Since interrupts are disabled ATM, it's necessary
	 * to detect the interrupt request and call edc_intr()
	 * explicitly. See also edc_run_cmd().
	 */
	while(bus_space_read_1(sc->sc_iot, sc->sc_ioh, BSR) & BSR_BUSY) {
		if (bus_space_read_1(sc->sc_iot, sc->sc_ioh, BSR) & BSR_INTR)
			edc_intr(sc);

		delay(100);
	}

	/*
	 * Get dummy ed_softc to be used during probe. Once a disk is
	 * found, ed_mca_attach() calls edc_add_disk() to insert the
	 * right pointer into sc->sc_ed[] array. 
	 */
	MALLOC(ed, struct ed_softc *, sizeof(struct ed_softc),
		M_TEMP, M_WAITOK);

	/* be quiet duting probes */
	sc->sc_flags |= DASD_QUIET;

	/* check for attached disks */
	for(devno=0; devno < maxdevs; devno++) {
		eda.sc_devno = devno;
		eda.sc_dmat  = sc->sc_dmat;
		sc->sc_ed[devno] = ed;
		(void *) config_found_sm(self, &eda, NULL, NULL);
	}

	/* enable full error dumps again */
	sc->sc_flags &= ~DASD_QUIET;

	/* cleanup */
	FREE(ed, M_TEMP);

	/*
	 * Check if there are any disks attached. If not, disestablish
	 * the interrupt.
	 */
	for(devno=0; devno < maxdevs; devno++) {
		if (sc->sc_ed[devno] && (sc->sc_ed[devno]->sc_flags & EDF_INIT))
			break;
	}
	if (devno == maxdevs) {
		printf("%s: disabling controller (no drives attached)\n",
			sc->sc_dev.dv_xname);
		mca_intr_disestablish(ma->ma_mc, sc->sc_ih);
	}
}

void
edc_add_disk(sc, ed, devno)
	struct edc_mca_softc *sc;
	struct ed_softc *ed;
	int devno;
{
	sc->sc_ed[devno] = ed;
}

static int
edc_intr(arg)
	void *arg;
{
	struct edc_mca_softc *sc = arg;
	u_int8_t isr, intr_id;
	u_int16_t sifr;
	int cmd=-1, devno, bioerror=0;
	struct ed_softc *ed=NULL;

	/*
	 * Check if the interrupt was for us.
	 */
	if ((bus_space_read_1(sc->sc_iot, sc->sc_ioh, BSR) & BSR_INTR) == 0)
		return (0);

	/*
	 * Read ISR to find out interrupt type. This also clears the interrupt
	 * condition and BSR_INTR flag. Accordings to docs interrupt ID of 0, 2
	 * and 4 are reserved and not used.
	 */
	isr = bus_space_read_1(sc->sc_iot, sc->sc_ioh, ISR);
	intr_id = isr & ISR_INTR_ID_MASK;

#ifdef DEBUG
	if (intr_id == 0 || intr_id == 2 || intr_id == 4) {
		printf("%s: bogus interrupt id %d\n", sc->sc_dev.dv_xname,
			(int) intr_id);
		return (0);
	}
#endif

	/* Get number of device whose intr this was */
	devno = (isr & 0xe0) >> 5;

	/*
	 * Get Status block. Higher byte always says how long the status
	 * block is, rest is device number and command code.
	 * Check the status block length against our supported maximum length
	 * and fetch the data.
	 */
	if (bus_space_read_1(sc->sc_iot, sc->sc_ioh,BSR) & BSR_SIFR_FULL) {
		size_t len;
		int i;

		sifr = le16toh(bus_space_read_2(sc->sc_iot, sc->sc_ioh, SIFR));
		len = (sifr & 0xff00) >> 8;
#ifdef DEBUG
		if (len > DASD_MAX_CMD_RES_LEN)
			panic("%s: maximum Status Length exceeded: %d > %d",
				sc->sc_dev.dv_xname,
				len, DASD_MAX_CMD_RES_LEN);
#endif

		/* Get command code */
		cmd = sifr & SIFR_CMD_MASK;

		/* Read whole status block */
		ed = sc->sc_ed[devno];
		ed->sc_status_block[0] = sifr;
		for(i=1; i < len; i++) {
			while((bus_space_read_1(sc->sc_iot, sc->sc_ioh, BSR)
				& BSR_SIFR_FULL) == 0)
				delay(1);

			ed->sc_status_block[i] = le16toh(
				bus_space_read_2(sc->sc_iot, sc->sc_ioh, SIFR));
		}
	}

	switch (intr_id) {
	case ISR_DATA_TRANSFER_RDY:
		/*
		 * Ready to do DMA, setup DMA controller and kick DASD
		 * controller to do the transfer.
		 */
		ed = sc->sc_ed[devno];
		if (!edc_setup_dma(sc, ed->sc_read,
			ed->dmamap_xfer->dm_segs[0].ds_addr,
			ed->dmamap_xfer->dm_segs[0].ds_len)) {
			/* XXX bail out? */
			printf("%s: edc_setup_dma() failed\n",
				ed->sc_dev.dv_xname);
			bus_space_write_1(sc->sc_iot, sc->sc_ioh, BCR,
				BCR_INT_ENABLE);
		} else {
			/* OK, proceed with DMA */
			bus_space_write_1(sc->sc_iot, sc->sc_ioh, BCR,
				BCR_INT_ENABLE|BCR_DMA_ENABLE);
		}
		break;
	case ISR_COMPLETED:
	case ISR_COMPLETED_WITH_ECC:
	case ISR_COMPLETED_RETRIES:
	case ISR_COMPLETED_WARNING:
		bioerror = 0;
		break;
	case ISR_RESET_COMPLETED:
	case ISR_ABORT_COMPLETED:
		/* nothing to do */
		break;
	default:
		if ((sc->sc_flags & DASD_QUIET) == 0)
			edc_dump_status_block(sc, devno, intr_id);

		bioerror = EIO;
		break;
	}
			
	/*
	 * Unless the interrupt is for Data Transfer Ready or
	 * Attention Error, finish by assertion EOI. This makes
	 * attachment aware the interrupt is processed and system
	 * is ready to accept another one.
	 */
	if (intr_id != ISR_DATA_TRANSFER_RDY && intr_id != ISR_ATTN_ERROR)
		edc_do_attn(sc, ATN_END_INT, devno, intr_id);

	/* If Read or Write Data, wakeup worker thread to finish it */
	if (intr_id != ISR_DATA_TRANSFER_RDY
	    && (cmd == CMD_READ_DATA || cmd == CMD_WRITE_DATA)) {
		sc->sc_ed[devno]->sc_error = bioerror;
		wakeup_one(&sc->sc_ed[devno]->edc_softc);
	}

	return (1);
}

/*
 * This follows the exact order for Attention Request as
 * written in DASD Storage Interface Specification MC (Rev 2.2).
 */ 
static int
edc_do_attn(sc, attn_type, devno, intr_id)
	struct edc_mca_softc *sc;
	int attn_type, devno, intr_id;
{
	int tries;

	/* 1. Disable interrupts in BCR. */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, BCR, 0);

	/*
	 * 2. Assure NOT BUSY and NO INTERRUPT PENDING, unless acknowledging
	 *    a RESET COMPLETED interrupt.
	 */
	if (intr_id != ISR_RESET_COMPLETED) {
		for(tries=1; tries < EDC_ATTN_MAXTRIES; tries++) {
			if ((bus_space_read_1(sc->sc_iot, sc->sc_ioh, BSR)
			     & BSR_BUSY) == 0) {
#ifdef DEBUG
				if ((bus_space_read_1(sc->sc_iot, sc->sc_ioh,
					BSR) & BSR_INT_PENDING) && intr_id)
					panic("foobar");
#endif
				break;
			}
		}

		if (tries == EDC_ATTN_MAXTRIES) {
			printf("%s: edc_do_attn: timeout waiting for attachment to become available\n",
					sc->sc_ed[devno]->sc_dev.dv_xname);
			return (EAGAIN);
		}
	}

	/*
	 * 3. Write proper DEVICE NUMBER and Attention number to ATN.
	 */ 
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, ATN,
		attn_type | (devno << 5));

	/*
	 * 4. Enable interrupts via BCR.
	 */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, BCR, BCR_INT_ENABLE);

	return (0);
}

/*
 * Wait until command is processed, timeout after 'secs' seconds.
 * We use mono_time, since we don't need actual RTC, just time
 * interval.
 */
static int
edc_cmd_wait(sc, devno, secs, poll)
	struct edc_mca_softc *sc;
	int devno, secs, poll;
{
	int val, delayed;

	delayed = 0;
	do {
		val = bus_space_read_1(sc->sc_iot,sc->sc_ioh, BSR);
		if ((val & BSR_CMD_INPROGRESS) == 0)
			break;

		if (poll && (val & BSR_INTR))
			goto out;

		if (secs == 0)
			break;

		delay(1);

		/*
		 * This is not as accurate as checking mono_time, but
		 * it works with hardclock interrupts disabled too.
		 */
		delayed++;
		if (delayed == 1000000) {
			delayed = 0;
			secs--;
		}
#if 0
		if (delayed % 1000)
			printf("looping ...");
#endif
	} while(1);

	if (secs == 0 &&
	    bus_space_read_1(sc->sc_iot, sc->sc_ioh, BSR) & BSR_CMD_INPROGRESS){
		printf("%s: timed out waiting for previous cmd to finish\n",
			sc->sc_ed[devno]->sc_dev.dv_xname);
		return (EAGAIN);
	}

    out:
	return (0);
}
	  
int
edc_run_cmd(sc, cmd, devno, cmd_args, cmd_len, async, poll)
	struct edc_mca_softc *sc;
	int cmd;
	int devno;
	u_int16_t cmd_args[];
	int cmd_len, async, poll;
{
	int i, error, tries;
	u_int16_t cmd0;

	/*
	 * If there has been an asynchronous command executed, first wait for it
	 * to finish.
	 */
	if (sc->sc_cmd_async) {
		/* Wait maximum 15s */
		if (edc_cmd_wait(sc, devno, 15, 0))
			return (EAGAIN);	/* Busy */

		sc->sc_cmd_async = 0;
	}

	/* Do Attention Request for Command Request. */
	if ((error = edc_do_attn(sc, ATN_CMD_REQ, devno, 0)))
		return (error);

	/*
	 * Construct the command. The bits are like this:
	 *
	 * 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
	 *  \_/   0  0       1 0 \__/   \_____/      
	 *    \    \__________/     \         \_ Command Code (see CMD_*)
	 *     \              \      \__ Device: 0 common, 7 controller
	 *      \              \__ Options: reserved, bit 10=cache bypass bit
	 *       \_ Type: 00=2B, 01=4B, 10 and 11 reserved
	 *
	 * We always use device 0 or 1, so difference is made only by Command
	 * Code, Command Options and command length.
	 */
	cmd0 = ((cmd_len == 4) ? (CIFR_LONG_CMD) : 0)
		| (devno <<  5)
		| (cmd_args[0] << 8) | cmd;
	cmd_args[0] = cmd0;
	
	/*
	 * Write word of CMD to the CIFR. This sets "Command
	 * Interface Register Full (CMD IN)" in BSR. Once the attachment
	 * detects it, it reads the word and clears CMD IN.
	 */
	for(i=0; i < cmd_len; i++) {
		bus_space_write_2(sc->sc_iot, sc->sc_ioh, CIFR,
			htole16(cmd_args[i]));
			
		/*
		 * Wait until CMD IN is cleared. The 1ms delay for polling
		 * case is necessary, otherwise e.g. system dump gets stuck
		 * soon. Quirky hw ?
		 */
		tries = 0;
		while(bus_space_read_1(sc->sc_iot, sc->sc_ioh, BSR) & BSR_CIFR_FULL)
			delay(poll ? 1000 : 1);
	}

	/*
	 * Attachment is now executing the command. Unless we are executing
	 * command asynchronously, wait until it finishes.
	 */
	if (async) {
		sc->sc_cmd_async = 1;
		return (0);
	}

	/* Wait for command to complete, but maximum 15 seconds. */
	if (edc_cmd_wait(sc, devno, 15, poll))
		return (EAGAIN);

	/* If polling, call edc_intr() explicitly */
	if (poll) {
		edc_intr(sc);

		/*
		 * If got attention id DATA TRANSFER READY, wait for
		 * the transfer to finish.
		 */
		if (sc->sc_ed[devno]->sc_error == 0
		    && (cmd == CMD_READ_DATA || cmd == CMD_WRITE_DATA)) {
			if (edc_cmd_wait(sc, devno, 15, 1))
				return (EAGAIN);
			edc_intr(sc);
		}

		if (edc_cmd_wait(sc, devno, 15, 0))
			return (EAGAIN);
	}

	/* Check if the command completed successfully; if not, return error */
	switch(SB_GET_CMD_STATUS(sc->sc_ed[devno]->sc_status_block)) {
	case ISR_COMPLETED:
	case ISR_COMPLETED_WITH_ECC:
	case ISR_COMPLETED_RETRIES:
	case ISR_COMPLETED_WARNING:
		return (0);
	default:
		return (EIO);
	}
}

static int
edc_setup_dma(sc, isread, phys, cnt)
	struct edc_mca_softc *sc;
	int isread;
	bus_addr_t phys;
	bus_size_t cnt;
{
	/* XXX magic constants, should be moved to device-independant location*/
	/* The exact sequence to setup MCA DMA controller is taken from Minix */

	bus_space_write_1(sc->sc_iot, sc->sc_dmaextcmdh, 0,
		0x90 + sc->sc_drq);
	/* Disable access to dma channel       */
	bus_space_write_1(sc->sc_iot, sc->sc_dmaextcmdh, 0,
		0x20 + sc->sc_drq);
	/* Clear the address byte pointer      */
	bus_space_write_1(sc->sc_iot, sc->sc_dmaexech, 0,
		(phys >> 0) & 0xff);	/* address bits 0..7   */
	bus_space_write_1(sc->sc_iot, sc->sc_dmaexech, 0,
		(phys >> 8) & 0xff);	/* address bits 8..15  */
	bus_space_write_1(sc->sc_iot, sc->sc_dmaexech, 0,
		(phys >> 16) & 0xff);	/* address bits 16..23  */
	bus_space_write_1(sc->sc_iot, sc->sc_dmaextcmdh, 0,
		0x40 + sc->sc_drq);
	/* Clear the count byte pointer        */
	bus_space_write_1(sc->sc_iot, sc->sc_dmaexech, 0,
		((cnt - 1) >> 0) & 0xff);         /* count bits 0..7     */
	bus_space_write_1(sc->sc_iot, sc->sc_dmaexech, 0,
		((cnt - 1) >> 8) & 0xff);         /* count bits 8..15    */
	bus_space_write_1(sc->sc_iot, sc->sc_dmaextcmdh, 0,
		0x70 + sc->sc_drq);
	/* Set the transfer mode               */
	bus_space_write_1(sc->sc_iot, sc->sc_dmaexech, 0,
  		(isread) ? 0x4C : 0x44);
	bus_space_write_1(sc->sc_iot, sc->sc_dmaextcmdh, 0,
			0xA0 + sc->sc_drq);
	/* Enable access to dma channel        */

	return (1);
}

static const char * const edc_commands[] = {
	"Invalid Command",
	"Read Data",
	"Write Data",
	"Read Verify",
	"Write with Verify",
	"Seek",
	"Park Head",
	"Get Command Complete Status",
	"Get Device Status",
	"Get Device Configuration",
	"Get POS Information",
	"Translate RBA",
	"Write Attachment Buffer",
	"Read Attachment Buffer",
	"Run Diagnostic Test",
	"Get Diagnostic Status Block",
	"Get MFG Header",
	"Format Unit",
	"Format Prepare",
	"Set MAX RBA",
	"Set Power Saving Mode",
	"Power Conservation Command",
};

static const char * const edc_cmd_status[256] = {
	"Reserved",
	"Command completed successfully",
	"Reserved",
	"Command completed successfully with ECC applied",
	"Reserved",
	"Command completed successfully with retries",
	"Format Command partially completed",	/* Status available */
	"Command completed successfully with ECC and retries",
	"Command completed with Warning", 	/* Command Error is available */
	"Aborted",
	"Reset completed",
	"Data Transfer Ready",		/* No Status Block available */
	"Command terminated with failure",	/* Device Error is available */
	"DMA Error",			/* Retry entire command as recovery */
	"Command Block Error",
	"Attention Error (Illegal Attention Code)",
	/* 0x14 - 0xff reserved */
};

static const char * const edc_cmd_error[256] = {
	"No Error",
	"Invalid parameter in the command block",
	"Reserved",
	"Command not supported",
	"Command Aborted per request",
	"Reserved",
	"Command rejected",	/* Attachment diagnostic failure */
	"Format Rejected",	/* Prepare Format command is required */
	"Format Error (Primary Map is not readable)",
	"Format Error (Secondary map is not readable)",
	"Format Error (Diagnostic Failure)",
	"Format Warning (Secondary Map Overflow)",
	"Reserved"
	"Format Error (Host Checksum Error)",
	"Reserved",
	"Format Warning (Push table overflow)",
	"Format Warning (More pushes than allowed)",
	"Reserved",
	"Format Warning (Error during verifying)",
	"Invalid device number for the command",
	/* 0x14-0xff reserved */
};

static const char * const edc_dev_errors[] = {
	"No Error",
	"Seek Fault",	/* Device report */
	"Interface Fault (Parity, Attn, or Cmd Complete Error)",
	"Block not found (ID not found)",
	"Block not found (AM not found)",
	"Data ECC Error (hard error)",
	"ID CRC Error",
	"RBA Out of Range",
	"Reserved",
	"Defective Block",
	"Reserved",
	"Selection Error",
	"Reserved",
	"Write Fault",
	"No index or sector pulse",
	"Device Not Ready",
	"Seek Error",	/* Attachment report */
	"Bad Format",
	"Volume Overflow",
	"No Data AM Found",
	"Block not found (No ID AM or ID CRC error occurred)",
	"Reserved",
	"Reserved",
	"No ID found on track (ID search)",
	/* 0x19 - 0xff reserved */
};

static void
edc_dump_status_block(sc, devno, intr_id)
	struct edc_mca_softc *sc;
	int devno, intr_id;
{
	struct ed_softc *ed = sc->sc_ed[devno];
	printf("%s: Command: %s, Status: %s\n",
		ed->sc_dev.dv_xname,
		edc_commands[ed->sc_status_block[0] & 0x1f],
		edc_cmd_status[SB_GET_CMD_STATUS(ed->sc_status_block)]
		);
	printf("%s: # left blocks: %u, last processed RBA: %u\n",
		ed->sc_dev.dv_xname,
		ed->sc_status_block[SB_RESBLKCNT_IDX],
		(ed->sc_status_block[5] << 16) | ed->sc_status_block[4]);

	if (intr_id == ISR_COMPLETED_WARNING) {
		printf("%s: Command Error Code: %s\n",
			ed->sc_dev.dv_xname,
			edc_cmd_error[ed->sc_status_block[1] & 0xff]);
	}

	if (intr_id == ISR_CMD_FAILED) {
		char buf[100];

		printf("%s: Device Error Code: %s\n",
			ed->sc_dev.dv_xname,
			edc_dev_errors[ed->sc_status_block[2] & 0xff]);
		bitmask_snprintf((ed->sc_status_block[2] & 0xff00) >> 8,
			"\20"
			"\01SeekOrCmdComplete"
			"\02Track0Flag"
			"\03WriteFault"
			"\04Selected"
			"\05Ready"
			"\06Reserved0"
			"\07STANDBY"
			"\010Reserved0",
			buf, sizeof(buf));
		printf("%s: Device Status: %s\n",
			ed->sc_dev.dv_xname, buf);
	}
}
