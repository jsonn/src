/*	$NetBSD: yds.c,v 1.17.2.2 2004/09/18 14:49:06 skrll Exp $	*/

/*
 * Copyright (c) 2000, 2001 Kazuki Sakamoto and Minoura Makoto.
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
 * Yamaha YMF724[B-F]/740[B-C]/744/754
 *
 * Documentation links:
 * - ftp://ftp.alsa-project.org/pub/manuals/yamaha/
 * - ftp://ftp.alsa-project.org/pub/manuals/yamaha/pci/
 *
 * TODO:
 * - FM synth volume (difficult: mixed before ac97)
 * - Digital in/out (SPDIF) support
 * - Effect??
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: yds.c,v 1.17.2.2 2004/09/18 14:49:06 skrll Exp $");

#include "mpu.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/mulaw.h>
#include <dev/auconv.h>
#include <dev/ic/ac97reg.h>
#include <dev/ic/ac97var.h>
#include <dev/ic/mpuvar.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/microcode/yds/yds_hwmcode.h>
#include <dev/pci/ydsreg.h>
#include <dev/pci/ydsvar.h>

/* Debug */
#undef YDS_USE_REC_SLOT
#define YDS_USE_P44

#ifdef AUDIO_DEBUG
# define DPRINTF(x)	if (ydsdebug) printf x
# define DPRINTFN(n,x)	if (ydsdebug>(n)) printf x
int	ydsdebug = 0;
#else
# define DPRINTF(x)
# define DPRINTFN(n,x)
#endif
#ifdef YDS_USE_REC_SLOT
# define YDS_INPUT_SLOT 0	/* REC slot = ADC + loopbacks */
#else
# define YDS_INPUT_SLOT 1	/* ADC slot */
#endif

int	yds_match __P((struct device *, struct cfdata *, void *));
void	yds_attach __P((struct device *, struct device *, void *));
int	yds_intr __P((void *));

#define DMAADDR(p) ((p)->map->dm_segs[0].ds_addr)
#define KERNADDR(p) ((void *)((p)->addr))

int	yds_allocmem __P((struct yds_softc *, size_t, size_t,
			  struct yds_dma *));
int	yds_freemem __P((struct yds_softc *, struct yds_dma *));

#ifndef AUDIO_DEBUG
#define YWRITE1(sc, r, x) bus_space_write_1((sc)->memt, (sc)->memh, (r), (x))
#define YWRITE2(sc, r, x) bus_space_write_2((sc)->memt, (sc)->memh, (r), (x))
#define YWRITE4(sc, r, x) bus_space_write_4((sc)->memt, (sc)->memh, (r), (x))
#define YREAD1(sc, r) bus_space_read_1((sc)->memt, (sc)->memh, (r))
#define YREAD2(sc, r) bus_space_read_2((sc)->memt, (sc)->memh, (r))
#define YREAD4(sc, r) bus_space_read_4((sc)->memt, (sc)->memh, (r))
#else

u_int16_t YREAD2(struct yds_softc *sc,bus_size_t r);
u_int32_t YREAD4(struct yds_softc *sc,bus_size_t r);
void YWRITE1(struct yds_softc *sc,bus_size_t r,u_int8_t x);
void YWRITE2(struct yds_softc *sc,bus_size_t r,u_int16_t x);
void YWRITE4(struct yds_softc *sc,bus_size_t r,u_int32_t x);

u_int16_t YREAD2(struct yds_softc *sc,bus_size_t r)
{
  DPRINTFN(5, (" YREAD2(0x%lX)\n",(unsigned long)r));
  return bus_space_read_2(sc->memt,sc->memh,r);
}
u_int32_t YREAD4(struct yds_softc *sc,bus_size_t r)
{
  DPRINTFN(5, (" YREAD4(0x%lX)\n",(unsigned long)r));
  return bus_space_read_4(sc->memt,sc->memh,r);
}
void YWRITE1(struct yds_softc *sc,bus_size_t r,u_int8_t x)
{
  DPRINTFN(5, (" YWRITE1(0x%lX,0x%lX)\n",(unsigned long)r,(unsigned long)x));
  bus_space_write_1(sc->memt,sc->memh,r,x);
}
void YWRITE2(struct yds_softc *sc,bus_size_t r,u_int16_t x)
{
  DPRINTFN(5, (" YWRITE2(0x%lX,0x%lX)\n",(unsigned long)r,(unsigned long)x));
  bus_space_write_2(sc->memt,sc->memh,r,x);
}
void YWRITE4(struct yds_softc *sc,bus_size_t r,u_int32_t x)
{
  DPRINTFN(5, (" YWRITE4(0x%lX,0x%lX)\n",(unsigned long)r,(unsigned long)x));
  bus_space_write_4(sc->memt,sc->memh,r,x);
}
#endif

#define	YWRITEREGION4(sc, r, x, c)	\
	bus_space_write_region_4((sc)->memt, (sc)->memh, (r), (x), (c) / 4)

CFATTACH_DECL(yds, sizeof(struct yds_softc),
    yds_match, yds_attach, NULL, NULL);

int	yds_open __P((void *, int));
void	yds_close __P((void *));
int	yds_query_encoding __P((void *, struct audio_encoding *));
int	yds_set_params __P((void *, int, int,
			    struct audio_params *, struct audio_params *));
int	yds_round_blocksize __P((void *, int));
int	yds_trigger_output __P((void *, void *, void *, int, void (*)(void *),
				void *, struct audio_params *));
int	yds_trigger_input __P((void *, void *, void *, int, void (*)(void *),
			       void *, struct audio_params *));
int	yds_halt_output __P((void *));
int	yds_halt_input __P((void *));
int	yds_getdev __P((void *, struct audio_device *));
int	yds_mixer_set_port __P((void *, mixer_ctrl_t *));
int	yds_mixer_get_port __P((void *, mixer_ctrl_t *));
void   *yds_malloc __P((void *, int, size_t, struct malloc_type *, int));
void	yds_free __P((void *, void *, struct malloc_type *));
size_t	yds_round_buffersize __P((void *, int, size_t));
paddr_t yds_mappage __P((void *, void *, off_t, int));
int	yds_get_props __P((void *));
int	yds_query_devinfo __P((void *addr, mixer_devinfo_t *dip));

int     yds_attach_codec __P((void *sc, struct ac97_codec_if *));
int	yds_read_codec __P((void *sc, u_int8_t a, u_int16_t *d));
int	yds_write_codec __P((void *sc, u_int8_t a, u_int16_t d));
void    yds_reset_codec __P((void *sc));
int     yds_get_portnum_by_name __P((struct yds_softc *, char *, char *,
				     char *));

static u_int yds_get_dstype __P((int));
static int yds_download_mcode __P((struct yds_softc *));
static int yds_allocate_slots __P((struct yds_softc *));
static void yds_configure_legacy __P((struct device *arg));
static void yds_enable_dsp __P((struct yds_softc *));
static int yds_disable_dsp __P((struct yds_softc *));
static int yds_ready_codec __P((struct yds_codec_softc *));
static int yds_halt __P((struct yds_softc *));
static u_int32_t yds_get_lpfq __P((u_int));
static u_int32_t yds_get_lpfk __P((u_int));
static struct yds_dma *yds_find_dma __P((struct yds_softc *, void *));

static int yds_init __P((struct yds_softc *));
static void yds_powerhook __P((int, void *));

#ifdef AUDIO_DEBUG
static void yds_dump_play_slot __P((struct yds_softc *, int));
#define	YDS_DUMP_PLAY_SLOT(n,sc,bank) \
	if (ydsdebug > (n)) yds_dump_play_slot(sc, bank)
#else
#define	YDS_DUMP_PLAY_SLOT(n,sc,bank)
#endif /* AUDIO_DEBUG */

static struct audio_hw_if yds_hw_if = {
	yds_open,
	yds_close,
	NULL,
	yds_query_encoding,
	yds_set_params,
	yds_round_blocksize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	yds_halt_output,
	yds_halt_input,
	NULL,
	yds_getdev,
	NULL,
	yds_mixer_set_port,
	yds_mixer_get_port,
	yds_query_devinfo,
	yds_malloc,
	yds_free,
	yds_round_buffersize,
	yds_mappage,
	yds_get_props,
	yds_trigger_output,
	yds_trigger_input,
	NULL,
};

struct audio_device yds_device = {
	"Yamaha DS-1",
	"",
	"yds"
};

const static struct {
	u_int	id;
	u_int	flags;
#define YDS_CAP_MCODE_1			0x0001
#define YDS_CAP_MCODE_1E		0x0002
#define YDS_CAP_LEGACY_SELECTABLE	0x0004
#define YDS_CAP_LEGACY_FLEXIBLE		0x0008
#define YDS_CAP_HAS_P44			0x0010
} yds_chip_capabliity_list[] = {
	{ PCI_PRODUCT_YAMAHA_YMF724,
	  YDS_CAP_MCODE_1|YDS_CAP_LEGACY_SELECTABLE },
	/* 740[C] has only 32 slots.  But anyway we use only 2 */
	{ PCI_PRODUCT_YAMAHA_YMF740,
	  YDS_CAP_MCODE_1|YDS_CAP_LEGACY_SELECTABLE },	/* XXX NOT TESTED */
	{ PCI_PRODUCT_YAMAHA_YMF740C,
	  YDS_CAP_MCODE_1E|YDS_CAP_LEGACY_SELECTABLE },
	{ PCI_PRODUCT_YAMAHA_YMF724F,
	  YDS_CAP_MCODE_1E|YDS_CAP_LEGACY_SELECTABLE },
	{ PCI_PRODUCT_YAMAHA_YMF744B, 
	  YDS_CAP_MCODE_1E|YDS_CAP_LEGACY_FLEXIBLE },
	{ PCI_PRODUCT_YAMAHA_YMF754,
	  YDS_CAP_MCODE_1E|YDS_CAP_LEGACY_FLEXIBLE|YDS_CAP_HAS_P44 },
	{ 0, 0 }
};
#ifdef AUDIO_DEBUG
#define YDS_CAP_BITS	"\020\005P44\004LEGFLEX\003LEGSEL\002MCODE1E\001MCODE1"
#endif

#ifdef AUDIO_DEBUG
static void
yds_dump_play_slot(sc, bank)
	struct yds_softc *sc;
	int bank;
{
	int i, j;
	u_int32_t *p;
	u_int32_t num;
	char *pa;

	for (i = 0; i < N_PLAY_SLOTS; i++) {
		printf("pbankp[%d] = %p,", i*2, sc->pbankp[i*2]);
		printf("pbankp[%d] = %p\n", i*2+1, sc->pbankp[i*2+1]);
	}

	pa = (char *)DMAADDR(&sc->sc_ctrldata) + sc->pbankoff;
	p = (u_int32_t *)sc->ptbl;
	printf("ptbl + 0: %d\n", *p++);
	for (i = 0; i < N_PLAY_SLOTS; i++) {
		printf("ptbl + %d: 0x%x, should be %p\n",
		       i+1, *p,
		       pa + i * sizeof(struct play_slot_ctrl_bank) *
			        N_PLAY_SLOT_CTRL_BANK);
		p++;
	}

	num = le32toh(*(u_int32_t*)sc->ptbl);
	printf("numofplay = %d\n", num);

	for (i = 0; i < num; i++) {
		p = (u_int32_t *)sc->pbankp[i*2];

		printf("  pbankp[%d], bank 0 : %p\n", i*2, p);
		for (j = 0; 
		     j < sizeof(struct play_slot_ctrl_bank) / sizeof(u_int32_t);
		     j++) {
			printf("    0x%02x: 0x%08x\n",
			       (unsigned)(j * sizeof(u_int32_t)),
			       (unsigned)*p++);
		}

		p = (u_int32_t *)sc->pbankp[i*2 + 1];
		printf("  pbankp[%d], bank 1 : %p\n", i*2 + 1, p);
		for (j = 0;
		     j < sizeof(struct play_slot_ctrl_bank) / sizeof(u_int32_t);
		     j++) {
			printf("    0x%02x: 0x%08x\n",
			       (unsigned)(j * sizeof(u_int32_t)),
			       (unsigned)*p++);
		}	
	}
}
#endif /* AUDIO_DEBUG */

static u_int
yds_get_dstype(id)
	int id;
{
	int i;

	for (i = 0; yds_chip_capabliity_list[i].id; i++) {
		if (PCI_PRODUCT(id) == yds_chip_capabliity_list[i].id)
			return yds_chip_capabliity_list[i].flags;
	}

	return -1;
}

static int
yds_download_mcode(sc)
	struct yds_softc *sc;
{
	u_int ctrl;
	const u_int32_t *p;
	size_t size;
	int dstype;

	static struct {
		const u_int32_t *mcode;
		size_t size;
	} ctrls[] = {
		{yds_ds1_ctrl_mcode, sizeof(yds_ds1_ctrl_mcode)},
		{yds_ds1e_ctrl_mcode, sizeof(yds_ds1e_ctrl_mcode)},
	};

	if (sc->sc_flags & YDS_CAP_MCODE_1)
		dstype = YDS_DS_1;
	else if (sc->sc_flags & YDS_CAP_MCODE_1E)
		dstype = YDS_DS_1E;
	else
		return 1;	/* unknown */

	if (yds_disable_dsp(sc))
		return 1;

	/* Software reset */
        YWRITE4(sc, YDS_MODE, YDS_MODE_RESET);
        YWRITE4(sc, YDS_MODE, 0);

        YWRITE4(sc, YDS_MAPOF_REC, 0);
        YWRITE4(sc, YDS_MAPOF_EFFECT, 0);
        YWRITE4(sc, YDS_PLAY_CTRLBASE, 0);
        YWRITE4(sc, YDS_REC_CTRLBASE, 0);
        YWRITE4(sc, YDS_EFFECT_CTRLBASE, 0);
        YWRITE4(sc, YDS_WORK_BASE, 0);

        ctrl = YREAD2(sc, YDS_GLOBAL_CONTROL);
        YWRITE2(sc, YDS_GLOBAL_CONTROL, ctrl & ~0x0007);

	/* Download DSP microcode. */
	p = yds_dsp_mcode;
	size = sizeof(yds_dsp_mcode);
	YWRITEREGION4(sc, YDS_DSP_INSTRAM, p, size);

	/* Download CONTROL microcode. */
	p = ctrls[dstype].mcode;
	size = ctrls[dstype].size;
	YWRITEREGION4(sc, YDS_CTRL_INSTRAM, p, size);

	yds_enable_dsp(sc);
	delay(10 * 1000);		/* nessesary on my 724F (??) */

	return 0;
}

static int
yds_allocate_slots(sc)
	struct yds_softc *sc;
{
	size_t pcs, rcs, ecs, ws, memsize;
	void *mp;
	u_int32_t da;		/* DMA address */
	char *va;		/* KVA */
	off_t cb;
	int i;
	struct yds_dma *p;

	/* Alloc DSP Control Data */
	pcs = YREAD4(sc, YDS_PLAY_CTRLSIZE) * sizeof(u_int32_t);
	rcs = YREAD4(sc, YDS_REC_CTRLSIZE) * sizeof(u_int32_t);
	ecs = YREAD4(sc, YDS_EFFECT_CTRLSIZE) * sizeof(u_int32_t);
	ws = WORK_SIZE;
	YWRITE4(sc, YDS_WORK_SIZE, ws / sizeof(u_int32_t));

	DPRINTF(("play control size : %d\n", (unsigned int)pcs));
	DPRINTF(("rec control size : %d\n", (unsigned int)rcs));
	DPRINTF(("eff control size : %d\n", (unsigned int)ecs));
	DPRINTF(("work size : %d\n", (unsigned int)ws));
#ifdef DIAGNOSTIC
	if (pcs != sizeof(struct play_slot_ctrl_bank)) {
		printf("%s: invalid play slot ctrldata %d != %d\n",
		       sc->sc_dev.dv_xname, (unsigned int)pcs,
		       (unsigned int)sizeof(struct play_slot_ctrl_bank));
	if (rcs != sizeof(struct rec_slot_ctrl_bank))
		printf("%s: invalid rec slot ctrldata %d != %d\n",
		       sc->sc_dev.dv_xname, (unsigned int)rcs,
		       (unsigned int)sizeof(struct rec_slot_ctrl_bank));
	}
#endif

	memsize = N_PLAY_SLOTS*N_PLAY_SLOT_CTRL_BANK*pcs +
		  N_REC_SLOT_CTRL*N_REC_SLOT_CTRL_BANK*rcs + ws;
	memsize += (N_PLAY_SLOTS+1)*sizeof(u_int32_t);

	p = &sc->sc_ctrldata;
	if (KERNADDR(p) == NULL) {
		i = yds_allocmem(sc, memsize, 16, p);
		if (i) {
			printf("%s: couldn't alloc/map DSP DMA buffer, reason %d\n",
				sc->sc_dev.dv_xname, i);
			free(p, M_DEVBUF);
			return 1;
		}
	}
	mp = KERNADDR(p);
	da = DMAADDR(p);

	DPRINTF(("mp:%p, DMA addr:%p\n",
		 mp, (void *)sc->sc_ctrldata.map->dm_segs[0].ds_addr));

	memset(mp, 0, memsize);

	/* Work space */
        cb = 0;
	va = (u_int8_t *)mp;
	YWRITE4(sc, YDS_WORK_BASE, da + cb);
        cb += ws;

	/* Play control data table */
        sc->ptbl = (u_int32_t *)(va + cb);
	sc->ptbloff = cb;
        YWRITE4(sc, YDS_PLAY_CTRLBASE, da + cb);
        cb += (N_PLAY_SLOT_CTRL + 1) * sizeof(u_int32_t);

	/* Record slot control data */
        sc->rbank = (struct rec_slot_ctrl_bank *)(va + cb);
        YWRITE4(sc, YDS_REC_CTRLBASE, da + cb);
	sc->rbankoff = cb;
        cb += N_REC_SLOT_CTRL * N_REC_SLOT_CTRL_BANK * rcs;

#if 0
	/* Effect slot control data -- unused */
        YWRITE4(sc, YDS_EFFECT_CTRLBASE, da + cb);
        cb += N_EFFECT_SLOT_CTRL * N_EFFECT_SLOT_CTRL_BANK * ecs;
#endif

	/* Play slot control data */
        sc->pbankoff = cb;
        for (i=0; i < N_PLAY_SLOT_CTRL; i++) {
		sc->pbankp[i*2] = (struct play_slot_ctrl_bank *)(va + cb);
		*(sc->ptbl + i+1) = htole32(da + cb);
                cb += pcs;

                sc->pbankp[i*2+1] = (struct play_slot_ctrl_bank *)(va + cb);
                cb += pcs;
        }
	/* Sync play control data table */
	bus_dmamap_sync(sc->sc_dmatag, p->map,
			sc->ptbloff, (N_PLAY_SLOT_CTRL+1) * sizeof(u_int32_t),
			BUS_DMASYNC_PREWRITE);			

	return 0;
}

static void
yds_enable_dsp(sc)
	struct yds_softc *sc;
{
	YWRITE4(sc, YDS_CONFIG, YDS_DSP_SETUP);
}

static int
yds_disable_dsp(sc)
	struct yds_softc *sc;
{
	int to;
	u_int32_t data;

	data = YREAD4(sc, YDS_CONFIG);
	if (data)
		YWRITE4(sc, YDS_CONFIG, YDS_DSP_DISABLE);

	for (to = 0; to < YDS_WORK_TIMEOUT; to++) {
		if ((YREAD4(sc, YDS_STATUS) & YDS_STAT_WORK) == 0)
			return 0;
		delay(1);
	}

	return 1;
}

int
yds_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	switch (PCI_VENDOR(pa->pa_id)) {
	case PCI_VENDOR_YAMAHA:
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_YAMAHA_YMF724:
		case PCI_PRODUCT_YAMAHA_YMF740:
		case PCI_PRODUCT_YAMAHA_YMF740C:
		case PCI_PRODUCT_YAMAHA_YMF724F:
		case PCI_PRODUCT_YAMAHA_YMF744B:
		case PCI_PRODUCT_YAMAHA_YMF754:
			return (1);
		}
		break;
	}

	return (0);
}

/*
 * This routine is called after all the ISA devices are configured,
 * to avoid conflict.
 */
static void
yds_configure_legacy (arg)
	struct device *arg;
#define FLEXIBLE	(sc->sc_flags & YDS_CAP_LEGACY_FLEXIBLE)
#define SELECTABLE	(sc->sc_flags & YDS_CAP_LEGACY_SELECTABLE)
{
	struct yds_softc *sc = (struct yds_softc*) arg;
	pcireg_t reg;
	struct device *dev;
	int i;
	bus_addr_t opl_addrs[] = {0x388, 0x398, 0x3A0, 0x3A8};
	bus_addr_t mpu_addrs[] = {0x330, 0x300, 0x332, 0x334};

	if (!FLEXIBLE && !SELECTABLE)
		return;

	reg = pci_conf_read(sc->sc_pc, sc->sc_pcitag, YDS_PCI_LEGACY);
	reg &= ~0x8133c03f;	/* these bits are out of interest */
	reg |= ((YDS_PCI_EX_LEGACY_IMOD) |
		(YDS_PCI_LEGACY_FMEN |
		 YDS_PCI_LEGACY_MEN /*| YDS_PCI_LEGACY_MIEN*/));
	reg |= YDS_PCI_EX_LEGACY_SMOD_DISABLE;
	if (FLEXIBLE) {
		pci_conf_write(sc->sc_pc, sc->sc_pcitag, YDS_PCI_LEGACY, reg);
		delay(100*1000);
	}

	/* Look for OPL */
	dev = 0;
	for (i = 0; i < sizeof(opl_addrs) / sizeof(bus_addr_t); i++) {
		if (SELECTABLE) {
			pci_conf_write(sc->sc_pc, sc->sc_pcitag,
				       YDS_PCI_LEGACY, reg | (i << (0+16)));
			delay(100*1000);	/* wait 100ms */
		} else
			pci_conf_write(sc->sc_pc, sc->sc_pcitag,
				       YDS_PCI_FM_BA, opl_addrs[i]);
		if (bus_space_map(sc->sc_opl_iot,
				  opl_addrs[i], 4, 0, &sc->sc_opl_ioh) == 0) {
			struct audio_attach_args aa; 

			aa.type = AUDIODEV_TYPE_OPL;
			aa.hwif = aa.hdl = NULL;
			dev = config_found(&sc->sc_dev, &aa, audioprint);
			if (dev == 0)
				bus_space_unmap(sc->sc_opl_iot,
						sc->sc_opl_ioh, 4);
			else {
				if (SELECTABLE)
					reg |= (i << (0+16));
				break;
			}
		} 
	}
	if (dev == 0) {
		reg &= ~YDS_PCI_LEGACY_FMEN;
		pci_conf_write(sc->sc_pc, sc->sc_pcitag,
			       YDS_PCI_LEGACY, reg);
	} else {
		/* Max. volume */
		YWRITE4(sc, YDS_LEGACY_OUT_VOLUME, 0x3fff3fff);
		YWRITE4(sc, YDS_LEGACY_REC_VOLUME, 0x3fff3fff);
	}

	/* Look for MPU */
	dev = 0;
	for (i = 0; i < sizeof(mpu_addrs) / sizeof(bus_addr_t); i++) {
		if (SELECTABLE)
			pci_conf_write(sc->sc_pc, sc->sc_pcitag,
				       YDS_PCI_LEGACY, reg | (i << (4+16)));
		else
			pci_conf_write(sc->sc_pc, sc->sc_pcitag,
				       YDS_PCI_MPU_BA, mpu_addrs[i]);
		if (bus_space_map(sc->sc_mpu_iot,
				  mpu_addrs[i], 2, 0, &sc->sc_mpu_ioh) == 0) {
			struct audio_attach_args aa; 

			aa.type = AUDIODEV_TYPE_MPU;
			aa.hwif = aa.hdl = NULL;
			dev = config_found(&sc->sc_dev, &aa, audioprint);
			if (dev == 0)
				bus_space_unmap(sc->sc_mpu_iot,
						sc->sc_mpu_ioh, 2);
			else {
				if (SELECTABLE)
					reg |= (i << (4+16));
				break;
			}
		}
	}
	if (dev == 0) {
		reg &= ~(YDS_PCI_LEGACY_MEN | YDS_PCI_LEGACY_MIEN);
		pci_conf_write(sc->sc_pc, sc->sc_pcitag, YDS_PCI_LEGACY, reg);
	}
	sc->sc_mpu = dev;
} 
#undef FLEXIBLE
#undef SELECTABLE

static int
yds_init(sc)
	struct yds_softc *sc;
{
	u_int32_t reg;

	DPRINTF(("yds_init()\n"));

	/* Download microcode */
	if (yds_download_mcode(sc)) {
		printf("%s: download microcode failed\n", sc->sc_dev.dv_xname);
		return 1;
	}

	/* Allocate DMA buffers */
	if (yds_allocate_slots(sc)) {
		printf("%s: could not allocate slots\n", sc->sc_dev.dv_xname);
		return 1;
	}

	/* Warm reset */
	reg = pci_conf_read(sc->sc_pc, sc->sc_pcitag, YDS_PCI_DSCTRL);
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, YDS_PCI_DSCTRL,
		reg | YDS_DSCTRL_WRST);
	delay(50000);

	return 0;
}

static void
yds_powerhook(why, addr)
	int why;
	void *addr;
{
	struct yds_softc *sc = addr;

	if (why == PWR_RESUME) {
		if (yds_init(sc)) {
			printf("%s: reinitialize failed\n",
				sc->sc_dev.dv_xname);
			return;
		}
		sc->sc_codec[0].codec_if->vtbl->restore_ports(sc->sc_codec[0].codec_if);
	}
}

void
yds_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct yds_softc *sc = (struct yds_softc *)self;
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	char const *intrstr;
	pci_intr_handle_t ih;
	pcireg_t reg;
	struct yds_codec_softc *codec;
	char devinfo[256];
	mixer_ctrl_t ctl;
	int i, r, to;
	int revision;
	int ac97_id2;

	pci_devinfo(pa->pa_id, pa->pa_class, 0, devinfo, sizeof(devinfo));
	revision = PCI_REVISION(pa->pa_class);
	printf(": %s (rev. 0x%02x)\n", devinfo, revision);

	/* Map register to memory */
	if (pci_mapreg_map(pa, YDS_PCI_MBA, PCI_MAPREG_TYPE_MEM, 0,
			   &sc->memt, &sc->memh, NULL, NULL)) {
		printf("%s: can't map memory space\n", sc->sc_dev.dv_xname);
		return;
	}

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa, &ih)) {
		printf("%s: couldn't map interrupt\n", sc->sc_dev.dv_xname);
		return;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_AUDIO, yds_intr, sc);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt", sc->sc_dev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf("%s: interrupting at %s\n", sc->sc_dev.dv_xname, intrstr);

	sc->sc_dmatag = pa->pa_dmat;
	sc->sc_pc = pc;
	sc->sc_pcitag = pa->pa_tag;
	sc->sc_id = pa->pa_id;
	sc->sc_revision = revision;
	sc->sc_flags = yds_get_dstype(sc->sc_id);
#ifdef AUDIO_DEBUG
	if (ydsdebug) {
		char bits[80];

		printf("%s: chip has %s\n", sc->sc_dev.dv_xname,
		       bitmask_snprintf(sc->sc_flags, YDS_CAP_BITS, bits,
					sizeof(bits)));
	}
#endif

	/* Disable legacy mode */
	reg = pci_conf_read(pc, pa->pa_tag, YDS_PCI_LEGACY);
	pci_conf_write(pc, pa->pa_tag, YDS_PCI_LEGACY,
		       reg & YDS_PCI_LEGACY_LAD);

	/* Enable the device. */
	reg = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	reg |= (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE |
		PCI_COMMAND_MASTER_ENABLE);
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, reg);
	reg = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);

	/* Mute all volumes */
	for (i = 0x80; i < 0xc0; i += 2)
		YWRITE2(sc, i, 0);

	/* Initialize the device */
	if (yds_init(sc)) {
		printf("%s: initialize failed\n", sc->sc_dev.dv_xname);
		return;
	}

	/*
	 * Detect primary/secondary AC97
	 *	YMF754 Hardware Specification Rev 1.01 page 24
	 */
	reg = pci_conf_read(pc, pa->pa_tag, YDS_PCI_DSCTRL);
	pci_conf_write(pc, pa->pa_tag, YDS_PCI_DSCTRL, reg & ~YDS_DSCTRL_CRST);
	delay(400000);		/* Needed for 740C. */

	/* Primary */
	for (to = 0; to < AC97_TIMEOUT; to++) {
		if ((YREAD2(sc, AC97_STAT_ADDR1) & AC97_BUSY) == 0)
			break;
		delay(1);
	}
	if (to == AC97_TIMEOUT) {
		printf("%s: no AC97 available\n", sc->sc_dev.dv_xname);
		return;
	}

	/* Secondary */
	/* Secondary AC97 is used for 4ch audio. Currently unused. */
	ac97_id2 = -1;
	if ((YREAD2(sc, YDS_ACTIVITY) & YDS_ACTIVITY_DOCKA) == 0)
		goto detected;
#if 0				/* reset secondary... */
	YWRITE2(sc, YDS_GPIO_OCTRL,
		YREAD2(sc, YDS_GPIO_OCTRL) & ~YDS_GPIO_GPO2);
	YWRITE2(sc, YDS_GPIO_FUNCE,
		(YREAD2(sc, YDS_GPIO_FUNCE)&(~YDS_GPIO_GPC2))|YDS_GPIO_GPE2);
#endif
	for (to = 0; to < AC97_TIMEOUT; to++) {
		if ((YREAD2(sc, AC97_STAT_ADDR2) & AC97_BUSY) == 0)
			break;
		delay(1);
	}
	if (to < AC97_TIMEOUT) {
		/* detect id */
		for (ac97_id2 = 1; ac97_id2 < 4; ac97_id2++) {
			YWRITE2(sc, AC97_CMD_ADDR,
				AC97_CMD_READ | AC97_ID(ac97_id2) | 0x28);

			for (to = 0; to < AC97_TIMEOUT; to++) {
				if ((YREAD2(sc, AC97_STAT_ADDR2) & AC97_BUSY)
				    == 0)
					goto detected;
				delay(1);
			}
		}
		if (ac97_id2 == 4)
			ac97_id2 = -1;
detected:
		;
	}

	pci_conf_write(pc, pa->pa_tag, YDS_PCI_DSCTRL, reg | YDS_DSCTRL_CRST);
	delay (20);
	pci_conf_write(pc, pa->pa_tag, YDS_PCI_DSCTRL, reg & ~YDS_DSCTRL_CRST);
	delay (400000);
	for (to = 0; to < AC97_TIMEOUT; to++) {
		if ((YREAD2(sc, AC97_STAT_ADDR1) & AC97_BUSY) == 0)
			break;
		delay(1);
	}

	/*
	 * Attach ac97 codec
	 */
	for (i = 0; i < 2; i++) {
		static struct {
			int data;
			int addr;
		} statregs[] = {
			{AC97_STAT_DATA1, AC97_STAT_ADDR1},
			{AC97_STAT_DATA2, AC97_STAT_ADDR2},
		};

		if (i == 1 && ac97_id2 == -1)
			break;		/* secondary ac97 not available */

		codec = &sc->sc_codec[i];
		memcpy(&codec->sc_dev, &sc->sc_dev, sizeof(codec->sc_dev));
		codec->sc = sc;
		codec->id = i == 1 ? ac97_id2 : 0;
		codec->status_data = statregs[i].data;
		codec->status_addr = statregs[i].addr;
		codec->host_if.arg = codec;
		codec->host_if.attach = yds_attach_codec;
		codec->host_if.read = yds_read_codec;
		codec->host_if.write = yds_write_codec;
		codec->host_if.reset = yds_reset_codec;

		if ((r = ac97_attach(&codec->host_if)) != 0) {
			printf("%s: can't attach codec (error 0x%X)\n",
			       sc->sc_dev.dv_xname, r);
			return;
		}
	}

	/* Just enable the DAC and master volumes by default */
	ctl.type = AUDIO_MIXER_ENUM;
	ctl.un.ord = 0;  /* off */
	ctl.dev = yds_get_portnum_by_name(sc, AudioCoutputs,
					  AudioNmaster, AudioNmute);
	yds_mixer_set_port(sc, &ctl);
	ctl.dev = yds_get_portnum_by_name(sc, AudioCinputs,
					  AudioNdac, AudioNmute);
	yds_mixer_set_port(sc, &ctl);
	ctl.dev = yds_get_portnum_by_name(sc, AudioCinputs,
					  AudioNcd, AudioNmute);
	yds_mixer_set_port(sc, &ctl);
	ctl.dev = yds_get_portnum_by_name(sc, AudioCrecord,
					  AudioNvolume, AudioNmute);
	yds_mixer_set_port(sc, &ctl);
	
	ctl.dev = yds_get_portnum_by_name(sc, AudioCrecord,
					  AudioNsource, NULL);
	ctl.type = AUDIO_MIXER_ENUM;
	ctl.un.ord = 0;
	yds_mixer_set_port(sc, &ctl);

	/* Set a reasonable default volume */
	ctl.type = AUDIO_MIXER_VALUE;
	ctl.un.value.num_channels = 2;
	ctl.un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
	ctl.un.value.level[AUDIO_MIXER_LEVEL_RIGHT] = 127;

	ctl.dev = sc->sc_codec[0].codec_if->vtbl->get_portnum_by_name(
	    sc->sc_codec[0].codec_if, AudioCoutputs, AudioNmaster, NULL);
	yds_mixer_set_port(sc, &ctl);

	audio_attach_mi(&yds_hw_if, sc, &sc->sc_dev);

	sc->sc_legacy_iot = pa->pa_iot;
	config_defer((struct device*) sc, yds_configure_legacy);

	powerhook_establish(yds_powerhook, sc);
}

int
yds_attach_codec(sc_, codec_if)
	void *sc_;
	struct ac97_codec_if *codec_if;
{
	struct yds_codec_softc *sc = sc_;

	sc->codec_if = codec_if;
	return 0;
}

static int
yds_ready_codec(sc)
	struct yds_codec_softc *sc;
{
	int to;

	for (to = 0; to < AC97_TIMEOUT; to++) {
		if ((YREAD2(sc->sc, sc->status_addr) & AC97_BUSY) == 0)
			return 0;
		delay(1);
	}

	return 1;
}

int
yds_read_codec(sc_, reg, data)
	void *sc_;
	u_int8_t reg;
	u_int16_t *data;
{
	struct yds_codec_softc *sc = sc_;

	YWRITE2(sc->sc, AC97_CMD_ADDR, AC97_CMD_READ | AC97_ID(sc->id) | reg);

	if (yds_ready_codec(sc)) {
		printf("%s: yds_read_codec timeout\n",
		       sc->sc->sc_dev.dv_xname);
		return EIO;
	}

	if (PCI_PRODUCT(sc->sc->sc_id) == PCI_PRODUCT_YAMAHA_YMF744B &&
	    sc->sc->sc_revision < 2) {
		int i;
		for (i=0; i<600; i++)
			YREAD2(sc->sc, sc->status_data);
	}

	*data = YREAD2(sc->sc, sc->status_data);

	return 0;
}

int
yds_write_codec(sc_, reg, data)
	void *sc_;
	u_int8_t reg;
	u_int16_t data;
{
	struct yds_codec_softc *sc = sc_;

	YWRITE2(sc->sc, AC97_CMD_ADDR, AC97_CMD_WRITE | AC97_ID(sc->id) | reg);
	YWRITE2(sc->sc, AC97_CMD_DATA, data);

	if (yds_ready_codec(sc)) {
		printf("%s: yds_write_codec timeout\n",
			sc->sc->sc_dev.dv_xname);
		return EIO;
	}

	return 0;
}

/*
 * XXX: Must handle the secondary differntly!!
 */
void
yds_reset_codec(sc_)
	void *sc_;
{
	struct yds_codec_softc *codec = sc_;
	struct yds_softc *sc = codec->sc;
	pcireg_t reg;

	/* reset AC97 codec */
	reg = pci_conf_read(sc->sc_pc, sc->sc_pcitag, YDS_PCI_DSCTRL);
	if (reg & 0x03) {
		pci_conf_write(sc->sc_pc, sc->sc_pcitag,
			       YDS_PCI_DSCTRL, reg & ~0x03);
		pci_conf_write(sc->sc_pc, sc->sc_pcitag,
			       YDS_PCI_DSCTRL, reg | 0x03);
		pci_conf_write(sc->sc_pc, sc->sc_pcitag,
			       YDS_PCI_DSCTRL, reg & ~0x03);
		delay(50000);
	}

	yds_ready_codec(sc_);
}

int
yds_intr(p)
	void *p;
{
	struct yds_softc *sc = p;
	u_int status;

	status = YREAD4(sc, YDS_STATUS);
	DPRINTFN(1, ("yds_intr: status=%08x\n", status));
	if ((status & (YDS_STAT_INT|YDS_STAT_TINT)) == 0) {
#if NMPU > 0
		if (sc->sc_mpu)
			return mpu_intr(sc->sc_mpu);
#endif
		return 0;
	}

	if (status & YDS_STAT_TINT) {
		YWRITE4(sc, YDS_STATUS, YDS_STAT_TINT);
		printf ("yds_intr: timeout!\n");
	}

	if (status & YDS_STAT_INT) {
		int nbank = (YREAD4(sc, YDS_CONTROL_SELECT) == 0);

		/* Clear interrupt flag */
		YWRITE4(sc, YDS_STATUS, YDS_STAT_INT);

		/* Buffer for the next frame is always ready. */
		YWRITE4(sc, YDS_MODE, YREAD4(sc, YDS_MODE) | YDS_MODE_ACTV2);

		if (sc->sc_play.intr) {
			u_int dma, cpu, blk, len;

			/* Sync play slot control data */
			bus_dmamap_sync(sc->sc_dmatag, sc->sc_ctrldata.map,
					sc->pbankoff,
					sizeof(struct play_slot_ctrl_bank)*
					    le32toh(*sc->ptbl)*
					    N_PLAY_SLOT_CTRL_BANK,
					BUS_DMASYNC_POSTWRITE|
					BUS_DMASYNC_POSTREAD);
			dma = le32toh(sc->pbankp[nbank]->pgstart) * sc->sc_play.factor;
			cpu = sc->sc_play.offset;
			blk = sc->sc_play.blksize;
			len = sc->sc_play.length;

			if (((dma > cpu) && (dma - cpu > blk * 2)) ||
			    ((cpu > dma) && (dma + len - cpu > blk * 2))) {
				/* We can fill the next block */
				/* Sync ring buffer for previous write */
				bus_dmamap_sync(sc->sc_dmatag,
						sc->sc_play.dma->map,
						cpu, blk,
						BUS_DMASYNC_POSTWRITE);
				sc->sc_play.intr(sc->sc_play.intr_arg);
				sc->sc_play.offset += blk;
				if (sc->sc_play.offset >= len) {
					sc->sc_play.offset -= len;
#ifdef DIAGNOSTIC
					if (sc->sc_play.offset != 0)
						printf ("Audio ringbuffer botch\n");
#endif
				}
				/* Sync ring buffer for next write */
				bus_dmamap_sync(sc->sc_dmatag,
						sc->sc_play.dma->map,
						cpu, blk,
						BUS_DMASYNC_PREWRITE);
			}
		}
		if (sc->sc_rec.intr) {
			u_int dma, cpu, blk, len;

			/* Sync rec slot control data */
			bus_dmamap_sync(sc->sc_dmatag, sc->sc_ctrldata.map,
					sc->rbankoff,
					sizeof(struct rec_slot_ctrl_bank)*
					    N_REC_SLOT_CTRL*
					    N_REC_SLOT_CTRL_BANK,
					BUS_DMASYNC_POSTWRITE|
					BUS_DMASYNC_POSTREAD);
			dma = le32toh(sc->rbank[YDS_INPUT_SLOT*2 + nbank].pgstartadr);
			cpu = sc->sc_rec.offset;
			blk = sc->sc_rec.blksize;
			len = sc->sc_rec.length;

			if (((dma > cpu) && (dma - cpu > blk * 2)) ||
			    ((cpu > dma) && (dma + len - cpu > blk * 2))) {
				/* We can drain the current block */
				/* Sync ring buffer first */
				bus_dmamap_sync(sc->sc_dmatag,
						sc->sc_rec.dma->map,
						cpu, blk,
						BUS_DMASYNC_POSTREAD);
				sc->sc_rec.intr(sc->sc_rec.intr_arg);
				sc->sc_rec.offset += blk;
				if (sc->sc_rec.offset >= len) {
					sc->sc_rec.offset -= len;
#ifdef DIAGNOSTIC
					if (sc->sc_rec.offset != 0)
						printf ("Audio ringbuffer botch\n");
#endif
				}
				/* Sync ring buffer for next read */
				bus_dmamap_sync(sc->sc_dmatag,
						sc->sc_rec.dma->map,
						cpu, blk,
						BUS_DMASYNC_PREREAD);
			}
		}
	}

	return 1;
}

int
yds_allocmem(sc, size, align, p)
	struct yds_softc *sc;
	size_t size;
	size_t align;
	struct yds_dma *p;
{
	int error;

	p->size = size;
	error = bus_dmamem_alloc(sc->sc_dmatag, p->size, align, 0,
				 p->segs, sizeof(p->segs)/sizeof(p->segs[0]),
				 &p->nsegs, BUS_DMA_NOWAIT);
	if (error)
		return (error);

	error = bus_dmamem_map(sc->sc_dmatag, p->segs, p->nsegs, p->size, 
			       &p->addr, BUS_DMA_NOWAIT|BUS_DMA_COHERENT);
	if (error)
		goto free;

	error = bus_dmamap_create(sc->sc_dmatag, p->size, 1, p->size,
				  0, BUS_DMA_NOWAIT, &p->map);
	if (error)
		goto unmap;

	error = bus_dmamap_load(sc->sc_dmatag, p->map, p->addr, p->size, NULL, 
				BUS_DMA_NOWAIT);
	if (error)
		goto destroy;
	return (0);

destroy:
	bus_dmamap_destroy(sc->sc_dmatag, p->map);
unmap:
	bus_dmamem_unmap(sc->sc_dmatag, p->addr, p->size);
free:
	bus_dmamem_free(sc->sc_dmatag, p->segs, p->nsegs);
	return (error);
}

int
yds_freemem(sc, p)
	struct yds_softc *sc;
	struct yds_dma *p;
{
	bus_dmamap_unload(sc->sc_dmatag, p->map);
	bus_dmamap_destroy(sc->sc_dmatag, p->map);
	bus_dmamem_unmap(sc->sc_dmatag, p->addr, p->size);
	bus_dmamem_free(sc->sc_dmatag, p->segs, p->nsegs);
	return 0;
}

int
yds_open(addr, flags)
	void *addr;
	int flags;
{
	struct yds_softc *sc = addr;
	u_int32_t mode;

	/* Select bank 0. */
	YWRITE4(sc, YDS_CONTROL_SELECT, 0);

	/* Start the DSP operation. */
	mode = YREAD4(sc, YDS_MODE);
	mode |= YDS_MODE_ACTV;
	mode &= ~YDS_MODE_ACTV2;
	YWRITE4(sc, YDS_MODE, mode);

	return 0;
}

/*
 * Close function is called at splaudio().
 */
void
yds_close(addr)
	void *addr;
{
	struct yds_softc *sc = addr;

	yds_halt(sc);
}

int
yds_query_encoding(addr, fp)
	void *addr;
	struct audio_encoding *fp;
{
	switch (fp->index) {
	case 0:
		strcpy(fp->name, AudioEulinear);
		fp->encoding = AUDIO_ENCODING_ULINEAR;
		fp->precision = 8;
		fp->flags = 0;
		return (0);
	case 1:
		strcpy(fp->name, AudioEmulaw);
		fp->encoding = AUDIO_ENCODING_ULAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 2:
		strcpy(fp->name, AudioEalaw);
		fp->encoding = AUDIO_ENCODING_ALAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 3:
		strcpy(fp->name, AudioEslinear);
		fp->encoding = AUDIO_ENCODING_SLINEAR;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 4:
		strcpy(fp->name, AudioEslinear_le);
		fp->encoding = AUDIO_ENCODING_SLINEAR_LE;
		fp->precision = 16;
		fp->flags = 0;
		return (0);
	case 5:
		strcpy(fp->name, AudioEulinear_le);
		fp->encoding = AUDIO_ENCODING_ULINEAR_LE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 6:
		strcpy(fp->name, AudioEslinear_be);
		fp->encoding = AUDIO_ENCODING_SLINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 7:
		strcpy(fp->name, AudioEulinear_be);
		fp->encoding = AUDIO_ENCODING_ULINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	default:
		return (EINVAL);
	}
}

int
yds_set_params(addr, setmode, usemode, play, rec)
	void *addr;
	int setmode, usemode;
	struct audio_params *play, *rec;
{
	struct audio_params *p;
	int mode;

	for (mode = AUMODE_RECORD; mode != -1; 
	     mode = mode == AUMODE_RECORD ? AUMODE_PLAY : -1) {
		if ((setmode & mode) == 0)
			continue;

		p = mode == AUMODE_PLAY ? play : rec;

		if (p->sample_rate < 4000 || p->sample_rate > 48000 ||
		    (p->precision != 8 && p->precision != 16) ||
		    (p->channels != 1 && p->channels != 2))
			return (EINVAL);

		p->factor = 1;
		p->sw_code = 0;
		switch (p->encoding) {
		case AUDIO_ENCODING_SLINEAR_BE:
			if (p->precision == 16)
				p->sw_code = swap_bytes;
			else
				p->sw_code = change_sign8;
			break;
		case AUDIO_ENCODING_SLINEAR_LE:
			if (p->precision != 16)
				p->sw_code = change_sign8;
			break;
		case AUDIO_ENCODING_ULINEAR_BE:
			if (p->precision == 16) {
				if (mode == AUMODE_PLAY)
					p->sw_code = swap_bytes_change_sign16_le;
				else
					p->sw_code = change_sign16_swap_bytes_le;
			}
			break;
		case AUDIO_ENCODING_ULINEAR_LE:
			if (p->precision == 16)
				p->sw_code = change_sign16_le;
			break;
		case AUDIO_ENCODING_ULAW:
			if (mode == AUMODE_PLAY) {
				p->factor = 2;
				p->precision = 16;
				p->sw_code = mulaw_to_slinear16_le;
			} else
				p->sw_code = ulinear8_to_mulaw;
			break;
		case AUDIO_ENCODING_ALAW:
			if (mode == AUMODE_PLAY) {
				p->factor = 2;
				p->precision = 16;
				p->sw_code = alaw_to_slinear16_le;
			} else
				p->sw_code = ulinear8_to_alaw;
			break;
		default:
			return (EINVAL);
		}
	}

	return 0;
}

int
yds_round_blocksize(addr, blk)
	void *addr;
	int blk;
{
	/*
	 * Block size must be bigger than a frame.
	 * That is 1024bytes at most, i.e. for 48000Hz, 16bit, 2ch.
	 */
	if (blk < 1024)
		blk = 1024;

	return blk & ~4;
}

static u_int32_t
yds_get_lpfq(sample_rate)
	u_int sample_rate;
{
	int i;
	static struct lpfqt {
		u_int rate;
		u_int32_t lpfq;
	} lpfqt[] = {
		{8000,  0x32020000},
		{11025, 0x31770000},
		{16000, 0x31390000},
		{22050, 0x31c90000},
		{32000, 0x33d00000},
		{48000, 0x40000000},
		{0, 0}
	};

	if (sample_rate == 44100)		/* for P44 slot? */
		return 0x370A0000;

	for (i = 0; lpfqt[i].rate != 0; i++)
		if (sample_rate <= lpfqt[i].rate)
			break;

	return lpfqt[i].lpfq;
}

static u_int32_t
yds_get_lpfk(sample_rate)
	u_int sample_rate;
{
	int i;
	static struct lpfkt {
		u_int rate;
		u_int32_t lpfk;
	} lpfkt[] = {
		{8000,  0x18b20000},
		{11025, 0x20930000},
		{16000, 0x2b9a0000},
		{22050, 0x35a10000},
		{32000, 0x3eaa0000},
		{48000, 0x40000000},
		{0, 0}
	};

	if (sample_rate == 44100)		/* for P44 slot? */
		return 0x46460000;

	for (i = 0; lpfkt[i].rate != 0; i++)
		if (sample_rate <= lpfkt[i].rate)
			break;

	return lpfkt[i].lpfk;
}

int
yds_trigger_output(addr, start, end, blksize, intr, arg, param)
	void *addr;
	void *start, *end;
	int blksize;
	void (*intr) __P((void *));
	void *arg;
	struct audio_params *param;
#define P44		(sc->sc_flags & YDS_CAP_HAS_P44)
{
	struct yds_softc *sc = addr;
	struct yds_dma *p;
	struct play_slot_ctrl_bank *psb;
	const u_int gain = 0x40000000;
	bus_addr_t s;
	size_t l;
	int i;
	int p44, channels;
	u_int32_t format;

#ifdef DIAGNOSTIC
	if (sc->sc_play.intr)
		panic("yds_trigger_output: already running");
#endif

	sc->sc_play.intr = intr;
	sc->sc_play.intr_arg = arg;
	sc->sc_play.offset = 0;
	sc->sc_play.blksize = blksize;

	DPRINTFN(1, ("yds_trigger_output: sc=%p start=%p end=%p "
	    "blksize=%d intr=%p(%p)\n", addr, start, end, blksize, intr, arg));

	p = yds_find_dma(sc, start);
	if (!p) {
		printf("yds_trigger_output: bad addr %p\n", start);
		return (EINVAL);
	}
	sc->sc_play.dma = p;

#ifdef YDS_USE_P44
	/* The document says the P44 SRC supports only stereo, 16bit PCM. */
	if (P44)
		p44 = ((param->sample_rate == 44100) &&
		       (param->channels == 2) &&
		       (param->precision == 16));
	else
#endif
		p44 = 0;
	channels = p44 ? 1 : param->channels;

	s = DMAADDR(p);
	l = ((char *)end - (char *)start);
	sc->sc_play.length = l;

	*sc->ptbl = htole32(channels);	/* Num of play */

	sc->sc_play.factor = 1;
	if (param->channels == 2)
		sc->sc_play.factor *= 2;
	if (param->precision != 8)
		sc->sc_play.factor *= 2;
	l /= sc->sc_play.factor;

	format = ((channels == 2 ? PSLT_FORMAT_STEREO : 0) |
		  (param->precision == 8 ? PSLT_FORMAT_8BIT : 0) |
		  (p44 ? PSLT_FORMAT_SRC441 : 0));

	psb = sc->pbankp[0];
	memset(psb, 0, sizeof(*psb));
	psb->format = htole32(format);
	psb->pgbase = htole32(s);
	psb->pgloopend = htole32(l);
	if (!p44) {
		psb->pgdeltaend = htole32((param->sample_rate * 65536 / 48000) << 12);
		psb->lpfkend = htole32(yds_get_lpfk(param->sample_rate));
		psb->eggainend = htole32(gain);
		psb->lpfq = htole32(yds_get_lpfq(param->sample_rate));
		psb->pgdelta = htole32(psb->pgdeltaend);
		psb->lpfk = htole32(yds_get_lpfk(param->sample_rate));
		psb->eggain = htole32(gain);
	}

	for (i = 0; i < channels; i++) {
		/* i == 0: left or mono, i == 1: right */
		psb = sc->pbankp[i*2];
		if (i)
			/* copy from left */
			*psb = *(sc->pbankp[0]);
		if (channels == 2) {
			/* stereo */
			if (i == 0) {
				psb->lchgain = psb->lchgainend = htole32(gain);
			} else {
				psb->lchgain = psb->lchgainend = 0;
				psb->rchgain = psb->rchgainend = htole32(gain);
				psb->format |= htole32(PSLT_FORMAT_RCH);
			}
		} else if (!p44) {
			/* mono */
			psb->lchgain = psb->rchgain = htole32(gain);
			psb->lchgainend = psb->rchgainend = htole32(gain);
		}
		/* copy to the other bank */
		*(sc->pbankp[i*2+1]) = *psb;
	}

	YDS_DUMP_PLAY_SLOT(5, sc, 0);
	YDS_DUMP_PLAY_SLOT(5, sc, 1);

	if (p44)
		YWRITE4(sc, YDS_P44_OUT_VOLUME, 0x3fff3fff);
	else
		YWRITE4(sc, YDS_DAC_OUT_VOLUME, 0x3fff3fff);

	/* Now the play slot for the next frame is set up!! */
	/* Sync play slot control data for both directions */
	bus_dmamap_sync(sc->sc_dmatag, sc->sc_ctrldata.map,
			sc->ptbloff,
			sizeof(struct play_slot_ctrl_bank) *
			    channels * N_PLAY_SLOT_CTRL_BANK,
			BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);
	/* Sync ring buffer */
	bus_dmamap_sync(sc->sc_dmatag, p->map, 0, blksize,
			BUS_DMASYNC_PREWRITE);
	/* HERE WE GO!! */
	YWRITE4(sc, YDS_MODE,
		YREAD4(sc, YDS_MODE) | YDS_MODE_ACTV | YDS_MODE_ACTV2);

	return 0;
}
#undef P44

int
yds_trigger_input(addr, start, end, blksize, intr, arg, param)
	void *addr;
	void *start, *end;
	int blksize;
	void (*intr) __P((void *));
	void *arg;
	struct audio_params *param;
{
	struct yds_softc *sc = addr;
	struct yds_dma *p;
	u_int srate, format;
	struct rec_slot_ctrl_bank *rsb;
	bus_addr_t s;
	size_t l;

#ifdef DIAGNOSTIC
	if (sc->sc_rec.intr)
		panic("yds_trigger_input: already running");
#endif
	sc->sc_rec.intr = intr;
	sc->sc_rec.intr_arg = arg;
	sc->sc_rec.offset = 0;
	sc->sc_rec.blksize = blksize;

	DPRINTFN(1, ("yds_trigger_input: "
	    "sc=%p start=%p end=%p blksize=%d intr=%p(%p)\n", 
	    addr, start, end, blksize, intr, arg));
	DPRINTFN(1, (" parameters: rate=%lu, precision=%u, channels=%u\n",
	    param->sample_rate, param->precision, param->channels));

	p = yds_find_dma(sc, start);
	if (!p) {
		printf("yds_trigger_input: bad addr %p\n", start);
		return (EINVAL);
	}
	sc->sc_rec.dma = p;

	s = DMAADDR(p);
	l = ((char *)end - (char *)start);
	sc->sc_rec.length = l;

	sc->sc_rec.factor = 1;
	if (param->channels == 2)
		sc->sc_rec.factor *= 2;
	if (param->precision != 8)
		sc->sc_rec.factor *= 2;

	rsb = &sc->rbank[0];
	memset(rsb, 0, sizeof(*rsb));
	rsb->pgbase = htole32(s);
	rsb->pgloopendadr = htole32(l);
	/* Seems all 4 banks must be set up... */
	sc->rbank[1] = *rsb;
	sc->rbank[2] = *rsb;
	sc->rbank[3] = *rsb;

	YWRITE4(sc, YDS_ADC_IN_VOLUME, 0x3fff3fff);
	YWRITE4(sc, YDS_REC_IN_VOLUME, 0x3fff3fff);
	srate = 48000 * 4096 / param->sample_rate - 1;
	format = ((param->precision == 8 ? YDS_FORMAT_8BIT : 0) |
		  (param->channels == 2 ? YDS_FORMAT_STEREO : 0));
	DPRINTF(("srate=%d, format=%08x\n", srate, format));
#ifdef YDS_USE_REC_SLOT
	YWRITE4(sc, YDS_DAC_REC_VOLUME, 0x3fff3fff);
	YWRITE4(sc, YDS_P44_REC_VOLUME, 0x3fff3fff);
	YWRITE4(sc, YDS_MAPOF_REC, YDS_RECSLOT_VALID);
	YWRITE4(sc, YDS_REC_SAMPLE_RATE, srate);
	YWRITE4(sc, YDS_REC_FORMAT, format);
#else
	YWRITE4(sc, YDS_MAPOF_REC, YDS_ADCSLOT_VALID);
	YWRITE4(sc, YDS_ADC_SAMPLE_RATE, srate);
	YWRITE4(sc, YDS_ADC_FORMAT, format);
#endif
	/* Now the rec slot for the next frame is set up!! */
	/* Sync record slot control data */
	bus_dmamap_sync(sc->sc_dmatag, sc->sc_ctrldata.map,
			sc->rbankoff,
			sizeof(struct rec_slot_ctrl_bank)*
			    N_REC_SLOT_CTRL*
			    N_REC_SLOT_CTRL_BANK,
			BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);
	/* Sync ring buffer */
	bus_dmamap_sync(sc->sc_dmatag, p->map, 0, blksize,
			BUS_DMASYNC_PREREAD);
	/* HERE WE GO!! */
	YWRITE4(sc, YDS_MODE,
		YREAD4(sc, YDS_MODE) | YDS_MODE_ACTV | YDS_MODE_ACTV2);

	return 0;
}

static int
yds_halt(sc)
	struct yds_softc *sc;
{
	u_int32_t mode;

	/* Stop the DSP operation. */
	mode = YREAD4(sc, YDS_MODE);
	YWRITE4(sc, YDS_MODE, mode & ~(YDS_MODE_ACTV|YDS_MODE_ACTV2));

	/* Paranoia...  mute all */
	YWRITE4(sc, YDS_P44_OUT_VOLUME, 0);
	YWRITE4(sc, YDS_DAC_OUT_VOLUME, 0);
	YWRITE4(sc, YDS_ADC_IN_VOLUME, 0);
	YWRITE4(sc, YDS_REC_IN_VOLUME, 0);
	YWRITE4(sc, YDS_DAC_REC_VOLUME, 0);
	YWRITE4(sc, YDS_P44_REC_VOLUME, 0);

	return 0;
}

int
yds_halt_output(addr)
	void *addr;
{
	struct yds_softc *sc = addr;

	DPRINTF(("yds: yds_halt_output\n"));
	if (sc->sc_play.intr) {
		sc->sc_play.intr = 0;
		/* Sync play slot control data */
		bus_dmamap_sync(sc->sc_dmatag, sc->sc_ctrldata.map,
				sc->pbankoff,
				sizeof(struct play_slot_ctrl_bank)*
				    (*sc->ptbl)*N_PLAY_SLOT_CTRL_BANK,
				BUS_DMASYNC_POSTWRITE|BUS_DMASYNC_POSTREAD);
		/* Stop the play slot operation */
		sc->pbankp[0]->status =
		sc->pbankp[1]->status =
		sc->pbankp[2]->status =
		sc->pbankp[3]->status = 1;
		/* Sync ring buffer */
		bus_dmamap_sync(sc->sc_dmatag, sc->sc_play.dma->map,
				0, sc->sc_play.length, BUS_DMASYNC_POSTWRITE);
	}

	return 0;
}

int
yds_halt_input(addr)
	void *addr;
{
	struct yds_softc *sc = addr;

	DPRINTF(("yds: yds_halt_input\n"));
	sc->sc_rec.intr = NULL;
	if (sc->sc_rec.intr) {
		/* Stop the rec slot operation */
		YWRITE4(sc, YDS_MAPOF_REC, 0);
		sc->sc_rec.intr = 0;
		/* Sync rec slot control data */
		bus_dmamap_sync(sc->sc_dmatag, sc->sc_ctrldata.map,
				sc->rbankoff,
				sizeof(struct rec_slot_ctrl_bank)*
				    N_REC_SLOT_CTRL*N_REC_SLOT_CTRL_BANK,
				BUS_DMASYNC_POSTWRITE|BUS_DMASYNC_POSTREAD);
		/* Sync ring buffer */
		bus_dmamap_sync(sc->sc_dmatag, sc->sc_rec.dma->map,
				0, sc->sc_rec.length, BUS_DMASYNC_POSTREAD);
	}

	return 0;
}

int
yds_getdev(addr, retp)
	void *addr;
	struct audio_device *retp;
{
	*retp = yds_device;

	return 0;
}

int
yds_mixer_set_port(addr, cp)
	void *addr;
	mixer_ctrl_t *cp;
{
	struct yds_softc *sc = addr;

	return (sc->sc_codec[0].codec_if->vtbl->mixer_set_port(
	    sc->sc_codec[0].codec_if, cp));
}

int
yds_mixer_get_port(addr, cp)
	void *addr;
	mixer_ctrl_t *cp;
{
	struct yds_softc *sc = addr;

	return (sc->sc_codec[0].codec_if->vtbl->mixer_get_port(
	    sc->sc_codec[0].codec_if, cp));
}

int
yds_query_devinfo(addr, dip)
	void *addr;
	mixer_devinfo_t *dip;
{
	struct yds_softc *sc = addr;

	return (sc->sc_codec[0].codec_if->vtbl->query_devinfo(
	    sc->sc_codec[0].codec_if, dip));
}

int
yds_get_portnum_by_name(sc, class, device, qualifier)
	struct yds_softc *sc;
	char *class, *device, *qualifier;
{
	return (sc->sc_codec[0].codec_if->vtbl->get_portnum_by_name(
	    sc->sc_codec[0].codec_if, class, device, qualifier));
}

void *
yds_malloc(addr, direction, size, pool, flags)
	void *addr;
	int direction;
	size_t size;
	struct malloc_type *pool;
	int flags;
{
	struct yds_softc *sc = addr;
	struct yds_dma *p;
	int error;

	p = malloc(sizeof(*p), pool, flags);
	if (!p)
		return (0);
	error = yds_allocmem(sc, size, 16, p);
	if (error) {
		free(p, pool);
		return (0);
	}
	p->next = sc->sc_dmas;
	sc->sc_dmas = p;
	return (KERNADDR(p));
}

void
yds_free(addr, ptr, pool)
	void *addr;
	void *ptr;
	struct malloc_type *pool;
{
	struct yds_softc *sc = addr;
	struct yds_dma **pp, *p;

	for (pp = &sc->sc_dmas; (p = *pp) != NULL; pp = &p->next) {
		if (KERNADDR(p) == ptr) {
			yds_freemem(sc, p);
			*pp = p->next;
			free(p, pool);
			return;
		}
	}
}

static struct yds_dma *
yds_find_dma(sc, addr)
	struct yds_softc *sc;
	void *addr;
{
	struct yds_dma *p;

	for (p = sc->sc_dmas; p && KERNADDR(p) != addr; p = p->next)
		;

	return p;
}

size_t
yds_round_buffersize(addr, direction, size)
	void *addr;
	int direction;
	size_t size;
{
	/*
	 * Buffer size should be at least twice as bigger as a frame.
	 */
	if (size < 1024 * 3)
		size = 1024 * 3;
	return (size);
}

paddr_t
yds_mappage(addr, mem, off, prot)
	void *addr;
	void *mem;
	off_t off;
	int prot;
{
	struct yds_softc *sc = addr;
	struct yds_dma *p;

	if (off < 0)
		return (-1);
	p = yds_find_dma(sc, mem);
	if (!p)
		return (-1);
	return (bus_dmamem_mmap(sc->sc_dmatag, p->segs, p->nsegs, 
				off, prot, BUS_DMA_WAITOK));
}

int
yds_get_props(addr)
	void *addr;
{
	return (AUDIO_PROP_MMAP | AUDIO_PROP_INDEPENDENT | 
		AUDIO_PROP_FULLDUPLEX);
}
