/*	$NetBSD: sdmmc_mem.c,v 1.2.2.3 2009/12/02 17:39:16 sborrill Exp $	*/
/*	$OpenBSD: sdmmc_mem.c,v 1.10 2009/01/09 10:55:22 jsg Exp $	*/

/*
 * Copyright (c) 2006 Uwe Stuehler <uwe@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*-
 * Copyright (c) 2007-2009 NONAKA Kimihiro <nonaka@netbsd.org>
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
 */

/* Routines for SD/MMC memory cards. */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sdmmc_mem.c,v 1.2.2.3 2009/12/02 17:39:16 sborrill Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <uvm/uvm_extern.h>

#include <dev/sdmmc/sdmmcchip.h>
#include <dev/sdmmc/sdmmcreg.h>
#include <dev/sdmmc/sdmmcvar.h>

#ifdef SDMMC_DEBUG
#define DPRINTF(s)	do { printf s; } while (/*CONSTCOND*/0)
#else
#define DPRINTF(s)	do {} while (/*CONSTCOND*/0)
#endif

static int sdmmc_mem_send_op_cond(struct sdmmc_softc *, uint32_t, uint32_t *);
static int sdmmc_mem_send_if_cond(struct sdmmc_softc *, uint32_t, uint32_t *);
static int sdmmc_mem_set_blocklen(struct sdmmc_softc *,
    struct sdmmc_function *);
#ifdef SDMMC_DUMP_CSD
static void sdmmc_print_csd(sdmmc_response, struct sdmmc_csd *);
#endif
static int sdmmc_mem_read_block_subr(struct sdmmc_function *, uint32_t,
    u_char *, size_t);
static int sdmmc_mem_write_block_subr(struct sdmmc_function *, uint32_t,
    u_char *, size_t);

/*
 * Initialize SD/MMC memory cards and memory in SDIO "combo" cards.
 */
int
sdmmc_mem_enable(struct sdmmc_softc *sc)
{
	uint32_t host_ocr;
	uint32_t card_ocr;
	int error;

	SDMMC_LOCK(sc);

	/* Set host mode to SD "combo" card or SD memory-only. */
	SET(sc->sc_flags, SMF_SD_MODE|SMF_MEM_MODE);

	/* Reset memory (*must* do that before CMD55 or CMD1). */
	sdmmc_go_idle_state(sc);

	/*
	 * Read the SD/MMC memory OCR value by issuing CMD55 followed
	 * by ACMD41 to read the OCR value from memory-only SD cards.
	 * MMC cards will not respond to CMD55 or ACMD41 and this is
	 * how we distinguish them from SD cards.
	 */
mmc_mode:
	error = sdmmc_mem_send_op_cond(sc, 0, &card_ocr);
	if (error) {
		if (ISSET(sc->sc_flags, SMF_SD_MODE) &&
		    !ISSET(sc->sc_flags, SMF_IO_MODE)) {
			/* Not a SD card, switch to MMC mode. */
			DPRINTF(("%s: switch to MMC mode\n", SDMMCDEVNAME(sc)));
			CLR(sc->sc_flags, SMF_SD_MODE);
			goto mmc_mode;
		}
		if (!ISSET(sc->sc_flags, SMF_SD_MODE)) {
			DPRINTF(("%s: couldn't read memory OCR\n",
			    SDMMCDEVNAME(sc)));
			goto out;
		} else {
			/* Not a "combo" card. */
			CLR(sc->sc_flags, SMF_MEM_MODE);
			error = 0;
			goto out;
		}
	}

	/* Set the lowest voltage supported by the card and host. */
	host_ocr = sdmmc_chip_host_ocr(sc->sc_sct, sc->sc_sch);
	error = sdmmc_set_bus_power(sc, host_ocr, card_ocr);
	if (error) {
		DPRINTF(("%s: couldn't supply voltage requested by card\n",
		    SDMMCDEVNAME(sc)));
		goto out;
	}

	/* Tell the card(s) to enter the idle state (again). */
	sdmmc_go_idle_state(sc);

	error = sdmmc_mem_send_if_cond(sc, 0x1aa, &card_ocr);
	if (error == 0 && card_ocr == 0x1aa)
		SET(host_ocr, MMC_OCR_HCS);

	/* Send the new OCR value until all cards are ready. */
	error = sdmmc_mem_send_op_cond(sc, host_ocr, NULL);
	if (error) {
		DPRINTF(("%s: couldn't send memory OCR\n", SDMMCDEVNAME(sc)));
		goto out;
	}

out:
	SDMMC_UNLOCK(sc);

	return error;
}

/*
 * Read the CSD and CID from all cards and assign each card a unique
 * relative card address (RCA).  CMD2 is ignored by SDIO-only cards.
 */
void
sdmmc_mem_scan(struct sdmmc_softc *sc)
{
	struct sdmmc_command cmd;
	struct sdmmc_function *sf;
	uint16_t next_rca;
	int error;
	int retry;

	SDMMC_LOCK(sc);

	/*
	 * CMD2 is a broadcast command understood by SD cards and MMC
	 * cards.  All cards begin to respond to the command, but back
	 * off if another card drives the CMD line to a different level.
	 * Only one card will get its entire response through.  That
	 * card remains silent once it has been assigned a RCA.
	 */
	for (retry = 0; retry < 100; retry++) {
		memset(&cmd, 0, sizeof cmd);
		cmd.c_opcode = MMC_ALL_SEND_CID;
		cmd.c_flags = SCF_CMD_BCR | SCF_RSP_R2;

		error = sdmmc_mmc_command(sc, &cmd);
		if (error == ETIMEDOUT) {
			/* No more cards there. */
			break;
		} else if (error) {
			DPRINTF(("%s: couldn't read CID\n", SDMMCDEVNAME(sc)));
			break;
		}

		/* In MMC mode, find the next available RCA. */
		next_rca = 1;
		if (!ISSET(sc->sc_flags, SMF_SD_MODE)) {
			SIMPLEQ_FOREACH(sf, &sc->sf_head, sf_list)
				next_rca++;
		}

		/* Allocate a sdmmc_function structure. */
		sf = sdmmc_function_alloc(sc);
		sf->rca = next_rca;

		/*
		 * Remember the CID returned in the CMD2 response for
		 * later decoding.
		 */
		memcpy(sf->raw_cid, cmd.c_resp, sizeof(sf->raw_cid));

		/*
		 * Silence the card by assigning it a unique RCA, or
		 * querying it for its RCA in the case of SD.
		 */
		if (sdmmc_set_relative_addr(sc, sf) != 0) {
			aprint_error_dev(sc->sc_dev, "couldn't set mem RCA\n");
			sdmmc_function_free(sf);
			break;
		}

#if 0
		/* Verify that the RCA has been set by selecting the card. */
		if (sdmmc_select_card(sc, sf) != 0) {
			printf("%s: can't select mem RCA %d (verify)\n",
			    SDMMCDEVNAME(sc), sf->rca);
			sdmmc_function_free(sf);
			break;
		}

		/* Deselect. */
		(void)sdmmc_select_card(sc, NULL);
#endif

		/*
		 * If this is a memory-only card, the card responding
		 * first becomes an alias for SDIO function 0.
		 */
		if (sc->sc_fn0 == NULL)
			sc->sc_fn0 = sf;

		SIMPLEQ_INSERT_TAIL(&sc->sf_head, sf, sf_list);
	}

	/*
	 * All cards are either inactive or awaiting further commands.
	 * Read the CSDs and decode the raw CID for each card.
	 */
	SIMPLEQ_FOREACH(sf, &sc->sf_head, sf_list) {
		memset(&cmd, 0, sizeof cmd);
		cmd.c_opcode = MMC_SEND_CSD;
		cmd.c_arg = MMC_ARG_RCA(sf->rca);
		cmd.c_flags = SCF_CMD_AC | SCF_RSP_R2;

		if (sdmmc_mmc_command(sc, &cmd) != 0) {
			SET(sf->flags, SFF_ERROR);
			continue;
		}

		if (sdmmc_decode_csd(sc, cmd.c_resp, sf) != 0 ||
		    sdmmc_decode_cid(sc, sf->raw_cid, sf) != 0) {
			SET(sf->flags, SFF_ERROR);
			continue;
		}

#ifdef SDMMC_DEBUG
		printf("%s: CID: ", SDMMCDEVNAME(sc));
		sdmmc_print_cid(&sf->cid);
#endif
	}

	SDMMC_UNLOCK(sc);
}

int
sdmmc_decode_csd(struct sdmmc_softc *sc, sdmmc_response resp,
    struct sdmmc_function *sf)
{
	/* TRAN_SPEED(2:0): transfer rate exponent */
	static const int speed_exponent[8] = {
		100 *    1,	/* 100 Kbits/s */
		  1 * 1000,	/*   1 Mbits/s */
		 10 * 1000,	/*  10 Mbits/s */
		100 * 1000,	/* 100 Mbits/s */
		         0,
		         0,
		         0,
		         0,
	};
	/* TRAN_SPEED(6:3): time mantissa */
	static const int speed_mantissa[16] = {
		0, 10, 12, 13, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80,
	};
	struct sdmmc_csd *csd = &sf->csd;
	int e, m;

	if (ISSET(sc->sc_flags, SMF_SD_MODE)) {
		/*
		 * CSD version 1.0 corresponds to SD system
		 * specification version 1.0 - 1.10. (SanDisk, 3.5.3)
		 */
		csd->csdver = SD_CSD_CSDVER(resp);
		switch (csd->csdver) {
		case SD_CSD_CSDVER_2_0:
			DPRINTF(("%s: SD Ver.2.0\n", SDMMCDEVNAME(sc)));
			SET(sf->flags, SFF_SDHC);
			csd->capacity = SD_CSD_V2_CAPACITY(resp);
			csd->read_bl_len = SD_CSD_V2_BL_LEN;
			break;

		case SD_CSD_CSDVER_1_0:
			DPRINTF(("%s: SD Ver.1.0\n", SDMMCDEVNAME(sc)));
			csd->capacity = SD_CSD_CAPACITY(resp);
			csd->read_bl_len = SD_CSD_READ_BL_LEN(resp);
			break;

		default:
			aprint_error_dev(sc->sc_dev,
			    "unknown SD CSD structure version 0x%x\n",
			    csd->csdver);
			return 1;
		}

		csd->mmcver = SD_CSD_MMCVER(resp);
		csd->write_bl_len = SD_CSD_WRITE_BL_LEN(resp);
		csd->r2w_factor = SD_CSD_R2W_FACTOR(resp);
		e = SD_CSD_SPEED_EXP(resp);
		m = SD_CSD_SPEED_MANT(resp);
		csd->tran_speed = speed_exponent[e] * speed_mantissa[m] / 10;
	} else {
		csd->csdver = MMC_CSD_CSDVER(resp);
		if (csd->csdver != MMC_CSD_CSDVER_1_0 &&
		    csd->csdver != MMC_CSD_CSDVER_2_0) {
			aprint_error_dev(sc->sc_dev,
			    "unknown MMC CSD structure version 0x%x\n",
			    csd->csdver);
			return 1;
		}

		csd->mmcver = MMC_CSD_MMCVER(resp);
		csd->capacity = MMC_CSD_CAPACITY(resp);
		csd->read_bl_len = MMC_CSD_READ_BL_LEN(resp);
		csd->write_bl_len = MMC_CSD_WRITE_BL_LEN(resp);
		csd->r2w_factor = MMC_CSD_R2W_FACTOR(resp);
		e = MMC_CSD_TRAN_SPEED_EXP(resp);
		m = MMC_CSD_TRAN_SPEED_MANT(resp);
		csd->tran_speed = speed_exponent[e] * speed_mantissa[m] / 10;
	}
	if ((1 << csd->read_bl_len) > SDMMC_SECTOR_SIZE)
		csd->capacity *= (1 << csd->read_bl_len) / SDMMC_SECTOR_SIZE;

	if (sc->sc_busclk > csd->tran_speed)
		sc->sc_busclk = csd->tran_speed;

#ifdef SDMMC_DUMP_CSD
	sdmmc_print_csd(resp, csd);
#endif

	return 0;
}

int
sdmmc_decode_cid(struct sdmmc_softc *sc, sdmmc_response resp,
    struct sdmmc_function *sf)
{
	struct sdmmc_cid *cid = &sf->cid;

	if (ISSET(sc->sc_flags, SMF_SD_MODE)) {
		cid->mid = SD_CID_MID(resp);
		cid->oid = SD_CID_OID(resp);
		SD_CID_PNM_CPY(resp, cid->pnm);
		cid->rev = SD_CID_REV(resp);
		cid->psn = SD_CID_PSN(resp);
		cid->mdt = SD_CID_MDT(resp);
	} else {
		switch(sf->csd.mmcver) {
		case MMC_CSD_MMCVER_1_0:
		case MMC_CSD_MMCVER_1_4:
			cid->mid = MMC_CID_MID_V1(resp);
			MMC_CID_PNM_V1_CPY(resp, cid->pnm);
			cid->rev = MMC_CID_REV_V1(resp);
			cid->psn = MMC_CID_PSN_V1(resp);
			cid->mdt = MMC_CID_MDT_V1(resp);
			break;
		case MMC_CSD_MMCVER_2_0:
		case MMC_CSD_MMCVER_3_1:
		case MMC_CSD_MMCVER_4_0:
			cid->mid = MMC_CID_MID_V2(resp);
			cid->oid = MMC_CID_OID_V2(resp);
			MMC_CID_PNM_V2_CPY(resp, cid->pnm);
			cid->psn = MMC_CID_PSN_V2(resp);
			break;
		default:
			aprint_error_dev(sc->sc_dev, "unknown MMC version %d\n",
			    sf->csd.mmcver);
			return 1;
		}
	}
	return 0;
}

void
sdmmc_print_cid(struct sdmmc_cid *cid)
{

	printf("mid=0x%02x oid=0x%04x pnm=\"%s\" rev=0x%02x psn=0x%08x"
	    " mdt=%03x\n", cid->mid, cid->oid, cid->pnm, cid->rev, cid->psn,
	    cid->mdt);
}

#ifdef SDMMC_DUMP_CSD
static void
sdmmc_print_csd(sdmmc_response resp, struct sdmmc_csd *csd)
{

	printf("csdver = %d\n", csd->csdver);
	printf("mmcver = %d\n", csd->mmcver);
	printf("capacity = %08x\n", csd->capacity);
	printf("read_bl_len = %d\n", csd->read_bl_len);
	printf("write_cl_len = %d\n", csd->write_bl_len);
	printf("r2w_factor = %d\n", csd->r2w_factor);
	printf("tran_speed = %d\n", csd->tran_speed);
}
#endif

/*
 * Initialize a SD/MMC memory card.
 */
int
sdmmc_mem_init(struct sdmmc_softc *sc, struct sdmmc_function *sf)
{
	int error;

	SDMMC_LOCK(sc);

	error = sdmmc_select_card(sc, sf);
	if (error)
		goto out;

	if (!ISSET(sf->flags, SFF_SDHC)) {
		error = sdmmc_mem_set_blocklen(sc, sf);
		if (error)
			goto out;
	}

out:
	SDMMC_UNLOCK(sc);

	return error;
}

/*
 * Get or set the card's memory OCR value (SD or MMC).
 */
static int
sdmmc_mem_send_op_cond(struct sdmmc_softc *sc, uint32_t ocr, uint32_t *ocrp)
{
	struct sdmmc_command cmd;
	int error;
	int retry;

	/* Don't lock */

	/*
	 * If we change the OCR value, retry the command until the OCR
	 * we receive in response has the "CARD BUSY" bit set, meaning
	 * that all cards are ready for identification.
	 */
	for (retry = 0; retry < 100; retry++) {
		memset(&cmd, 0, sizeof(cmd));
		cmd.c_arg = ocr;
		cmd.c_flags = SCF_CMD_BCR | SCF_RSP_R3;

		if (ISSET(sc->sc_flags, SMF_SD_MODE)) {
			cmd.c_opcode = SD_APP_OP_COND;
			error = sdmmc_app_command(sc, &cmd);
		} else {
			cmd.c_opcode = MMC_SEND_OP_COND;
			error = sdmmc_mmc_command(sc, &cmd);
		}
		if (error)
			break;
		if (ISSET(MMC_R3(cmd.c_resp), MMC_OCR_MEM_READY) || ocr == 0)
			break;

		error = ETIMEDOUT;
		sdmmc_delay(10000);
	}
	if (error == 0 && ocrp != NULL)
		*ocrp = MMC_R3(cmd.c_resp);

	return error;
}

static int
sdmmc_mem_send_if_cond(struct sdmmc_softc *sc, uint32_t ocr, uint32_t *ocrp)
{
	struct sdmmc_command cmd;
	int error;

	/* Don't lock */

	memset(&cmd, 0, sizeof(cmd));
	cmd.c_arg = ocr;
	cmd.c_flags = SCF_CMD_BCR | SCF_RSP_R7;
	cmd.c_opcode = SD_SEND_IF_COND;

	error = sdmmc_mmc_command(sc, &cmd);
	if (error == 0 && ocrp != NULL)
		*ocrp = MMC_R7(cmd.c_resp);
	return error;
}

/*
 * Set the read block length appropriately for this card, according to
 * the card CSD register value.
 */
static int
sdmmc_mem_set_blocklen(struct sdmmc_softc *sc, struct sdmmc_function *sf)
{
	struct sdmmc_command cmd;
	int error;

	/* Don't lock */

	memset(&cmd, 0, sizeof(cmd));
	cmd.c_opcode = MMC_SET_BLOCKLEN;
	cmd.c_arg = SDMMC_SECTOR_SIZE;
	cmd.c_flags = SCF_CMD_AC | SCF_RSP_R1;

	error = sdmmc_mmc_command(sc, &cmd);

	DPRINTF(("%s: sdmmc_mem_set_blocklen: read_bl_len=%d sector_size=%d\n",
	    SDMMCDEVNAME(sc), 1 << sf->csd.read_bl_len, SDMMC_SECTOR_SIZE));

	return error;
}

static int
sdmmc_mem_read_block_subr(struct sdmmc_function *sf, uint32_t blkno,
    u_char *data, size_t datalen)
{
	struct sdmmc_softc *sc = sf->sc;
	struct sdmmc_command cmd;
	int error;

	error = sdmmc_select_card(sc, sf);
	if (error)
		goto out;

	memset(&cmd, 0, sizeof(cmd));
	cmd.c_data = data;
	cmd.c_datalen = datalen;
	cmd.c_blklen = SDMMC_SECTOR_SIZE;
	cmd.c_opcode = (cmd.c_datalen / cmd.c_blklen) > 1 ?
	    MMC_READ_BLOCK_MULTIPLE : MMC_READ_BLOCK_SINGLE;
	cmd.c_arg = blkno;
	if (!ISSET(sf->flags, SFF_SDHC))
		cmd.c_arg <<= SDMMC_SECTOR_SIZE_SB;
	cmd.c_flags = SCF_CMD_ADTC | SCF_CMD_READ | SCF_RSP_R1;
	if (ISSET(sc->sc_caps, SMC_CAPS_DMA))
		cmd.c_dmamap = sc->sc_dmap;

	error = sdmmc_mmc_command(sc, &cmd);
	if (error)
		goto out;

	if (!ISSET(sc->sc_caps, SMC_CAPS_AUTO_STOP)) {
		if (cmd.c_opcode == MMC_READ_BLOCK_MULTIPLE) {
			memset(&cmd, 0, sizeof cmd);
			cmd.c_opcode = MMC_STOP_TRANSMISSION;
			cmd.c_arg = MMC_ARG_RCA(sf->rca);
			cmd.c_flags = SCF_CMD_AC | SCF_RSP_R1B;
			error = sdmmc_mmc_command(sc, &cmd);
			if (error)
				goto out;
		}
	}

	do {
		memset(&cmd, 0, sizeof(cmd));
		cmd.c_opcode = MMC_SEND_STATUS;
		cmd.c_arg = MMC_ARG_RCA(sf->rca);
		cmd.c_flags = SCF_CMD_AC | SCF_RSP_R1;
		error = sdmmc_mmc_command(sc, &cmd);
		if (error)
			break;
		/* XXX time out */
	} while (!ISSET(MMC_R1(cmd.c_resp), MMC_R1_READY_FOR_DATA));

out:
	return error;
}

int
sdmmc_mem_read_block(struct sdmmc_function *sf, uint32_t blkno, u_char *data,
    size_t datalen)
{
	struct sdmmc_softc *sc = sf->sc;
	int error;

	SDMMC_LOCK(sc);

	if (!ISSET(sc->sc_caps, SMC_CAPS_DMA)) {
		error = sdmmc_mem_read_block_subr(sf, blkno, data, datalen);
		goto out;
	}

	/* DMA transfer */
	error = bus_dmamap_load(sc->sc_dmat, sc->sc_dmap, data, datalen, NULL,
	    BUS_DMA_NOWAIT|BUS_DMA_STREAMING|BUS_DMA_READ);
	if (error)
		goto out;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmap, 0, datalen,
	    BUS_DMASYNC_PREREAD);

	error = sdmmc_mem_read_block_subr(sf, blkno, data, datalen);
	if (error)
		goto unload;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmap, 0, datalen,
	    BUS_DMASYNC_POSTREAD);
unload:
	bus_dmamap_unload(sc->sc_dmat, sc->sc_dmap);

out:
	SDMMC_UNLOCK(sc);

	return error;
}

static int
sdmmc_mem_write_block_subr(struct sdmmc_function *sf, uint32_t blkno,
    u_char *data, size_t datalen)
{
	struct sdmmc_softc *sc = sf->sc;
	struct sdmmc_command cmd;
	int error;

	error = sdmmc_select_card(sc, sf);
	if (error)
		goto out;

	memset(&cmd, 0, sizeof(cmd));
	cmd.c_data = data;
	cmd.c_datalen = datalen;
	cmd.c_blklen = SDMMC_SECTOR_SIZE;
	cmd.c_opcode = (cmd.c_datalen / cmd.c_blklen) > 1 ?
	    MMC_WRITE_BLOCK_MULTIPLE : MMC_WRITE_BLOCK_SINGLE;
	cmd.c_arg = blkno;
	if (!ISSET(sf->flags, SFF_SDHC))
		cmd.c_arg <<= SDMMC_SECTOR_SIZE_SB;
	cmd.c_flags = SCF_CMD_ADTC | SCF_RSP_R1;
	if (ISSET(sc->sc_caps, SMC_CAPS_DMA))
		cmd.c_dmamap = sc->sc_dmap;

	error = sdmmc_mmc_command(sc, &cmd);
	if (error)
		goto out;

	if (!ISSET(sc->sc_caps, SMC_CAPS_AUTO_STOP)) {
		if (cmd.c_opcode == MMC_WRITE_BLOCK_MULTIPLE) {
			memset(&cmd, 0, sizeof(cmd));
			cmd.c_opcode = MMC_STOP_TRANSMISSION;
			cmd.c_flags = SCF_CMD_AC | SCF_RSP_R1B;
			error = sdmmc_mmc_command(sc, &cmd);
			if (error)
				goto out;
		}
	}

	do {
		memset(&cmd, 0, sizeof(cmd));
		cmd.c_opcode = MMC_SEND_STATUS;
		cmd.c_arg = MMC_ARG_RCA(sf->rca);
		cmd.c_flags = SCF_CMD_AC | SCF_RSP_R1;
		error = sdmmc_mmc_command(sc, &cmd);
		if (error)
			break;
		/* XXX time out */
	} while (!ISSET(MMC_R1(cmd.c_resp), MMC_R1_READY_FOR_DATA));

out:
	return error;
}

int
sdmmc_mem_write_block(struct sdmmc_function *sf, uint32_t blkno, u_char *data,
    size_t datalen)
{
	struct sdmmc_softc *sc = sf->sc;
	int error;

	SDMMC_LOCK(sc);

	if (sdmmc_chip_write_protect(sc->sc_sct, sc->sc_sch)) {
		aprint_normal_dev(sc->sc_dev, "write-protected\n");
		error = EIO;
		goto out;
	}

	if (!ISSET(sc->sc_caps, SMC_CAPS_DMA)) {
		error = sdmmc_mem_write_block_subr(sf, blkno, data, datalen);
		goto out;
	}

	/* DMA transfer */
	error = bus_dmamap_load(sc->sc_dmat, sc->sc_dmap, data, datalen, NULL,
	    BUS_DMA_NOWAIT|BUS_DMA_STREAMING|BUS_DMA_WRITE);
	if (error)
		goto out;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmap, 0, datalen,
	    BUS_DMASYNC_PREWRITE);

	error = sdmmc_mem_write_block_subr(sf, blkno, data, datalen);
	if (error)
		goto unload;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmap, 0, datalen,
	    BUS_DMASYNC_POSTWRITE);
unload:
	bus_dmamap_unload(sc->sc_dmat, sc->sc_dmap);

out:
	SDMMC_UNLOCK(sc);

	return error;
}
