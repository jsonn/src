/*	$NetBSD: auvia.c,v 1.11.2.7 2002/11/11 22:11:02 nathanw Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Tyler C. Sarna
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
 * VIA Technologies VT82C686A / VT8233 / VT8235 Southbridge Audio Driver
 *
 * Documentation links:
 *
 * ftp://ftp.alsa-project.org/pub/manuals/via/686a.pdf
 * ftp://ftp.alsa-project.org/pub/manuals/general/ac97r21.pdf
 * ftp://ftp.alsa-project.org/pub/manuals/ad/AD1881_0.pdf (example AC'97 codec)
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: auvia.c,v 1.11.2.7 2002/11/11 22:11:02 nathanw Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/audioio.h>

#include <uvm/uvm_extern.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>

#include <dev/audio_if.h>
#include <dev/mulaw.h>
#include <dev/auconv.h>

#include <dev/ic/ac97reg.h>
#include <dev/ic/ac97var.h>

#include <dev/pci/auviavar.h>

struct auvia_dma {
	struct auvia_dma *next;
	caddr_t addr;
	size_t size;
	bus_dmamap_t map;
	bus_dma_segment_t seg;
};

struct auvia_dma_op {
	u_int32_t ptr;
	u_int32_t flags;
#define AUVIA_DMAOP_EOL		0x80000000
#define AUVIA_DMAOP_FLAG	0x40000000
#define AUVIA_DMAOP_STOP	0x20000000
#define AUVIA_DMAOP_COUNT(x)	((x)&0x00FFFFFF)
};

int	auvia_match(struct device *, struct cfdata *, void *);
void	auvia_attach(struct device *, struct device *, void *);
int	auvia_open(void *, int);
void	auvia_close(void *);
int	auvia_query_encoding(void *, struct audio_encoding *);
void	auvia_set_params_sub(struct auvia_softc *, struct auvia_softc_chan *,
	struct audio_params *);
int	auvia_set_params(void *, int, int, struct audio_params *,
	struct audio_params *);
int	auvia_round_blocksize(void *, int);
int	auvia_halt_output(void *);
int	auvia_halt_input(void *);
int	auvia_getdev(void *, struct audio_device *);
int	auvia_set_port(void *, mixer_ctrl_t *);
int	auvia_get_port(void *, mixer_ctrl_t *);
int	auvia_query_devinfo(void *, mixer_devinfo_t *);
void *	auvia_malloc(void *, int, size_t, int, int);
void	auvia_free(void *, void *, int);
size_t	auvia_round_buffersize(void *, int, size_t);
paddr_t	auvia_mappage(void *, void *, off_t, int);
int	auvia_get_props(void *);
int	auvia_build_dma_ops(struct auvia_softc *, struct auvia_softc_chan *,
	struct auvia_dma *, void *, void *, int);
int	auvia_trigger_output(void *, void *, void *, int, void (*)(void *),
	void *, struct audio_params *);
int	auvia_trigger_input(void *, void *, void *, int, void (*)(void *),
	void *, struct audio_params *);

int	auvia_intr __P((void *));

CFATTACH_DECL(auvia, sizeof (struct auvia_softc),
    auvia_match, auvia_attach, NULL, NULL);

#define AUVIA_PCICONF_JUNK	0x40
#define		AUVIA_PCICONF_ENABLES	 0x00FF0000	/* reg 42 mask */
#define		AUVIA_PCICONF_ACLINKENAB 0x00008000	/* ac link enab */
#define		AUVIA_PCICONF_ACNOTRST	 0x00004000	/* ~(ac reset) */
#define		AUVIA_PCICONF_ACSYNC	 0x00002000	/* ac sync */
#define		AUVIA_PCICONF_ACVSR	 0x00000800	/* var. samp. rate */
#define		AUVIA_PCICONF_ACSGD	 0x00000400	/* SGD enab */
#define		AUVIA_PCICONF_ACFM	 0x00000200	/* FM enab */
#define		AUVIA_PCICONF_ACSB	 0x00000100	/* SB enab */

#define	AUVIA_PLAY_BASE			0x00
#define	AUVIA_RECORD_BASE		0x10

/* *_RP_* are offsets from AUVIA_PLAY_BASE or AUVIA_RECORD_BASE */
#define	AUVIA_RP_STAT			0x00
#define		AUVIA_RPSTAT_INTR		0x03
#define	AUVIA_RP_CONTROL		0x01
#define		AUVIA_RPCTRL_START		0x80
#define		AUVIA_RPCTRL_TERMINATE		0x40
#define		AUVIA_RPCTRL_AUTOSTART		0x20
/* The following are 8233 specific */
#define		AUVIA_RPCTRL_STOP		0x04
#define		AUVIA_RPCTRL_EOL		0x02
#define		AUVIA_RPCTRL_FLAG		0x01
#define	AUVIA_RP_MODE			0x02		/* 82c686 specific */
#define		AUVIA_RPMODE_INTR_FLAG		0x01
#define		AUVIA_RPMODE_INTR_EOL		0x02
#define		AUVIA_RPMODE_STEREO		0x10
#define		AUVIA_RPMODE_16BIT		0x20
#define		AUVIA_RPMODE_AUTOSTART		0x80
#define	AUVIA_RP_DMAOPS_BASE		0x04

#define	VIA8233_RP_DXS_LVOL		0x02
#define	VIA8233_RP_DXS_RVOL		0x03
#define	VIA8233_RP_RATEFMT		0x08
#define		VIA8233_RATEFMT_48K		0xfffff
#define		VIA8233_RATEFMT_STEREO		0x00100000
#define		VIA8233_RATEFMT_16BIT		0x00200000

#define	VIA_RP_DMAOPS_COUNT		0x0c

#define VIA8233_MP_BASE			0x40
	/* STAT, CONTROL, DMAOPS_BASE, DMAOPS_COUNT are valid */
#define VIA8233_OFF_MP_FORMAT		0x02
#define		VIA8233_MP_FORMAT_8BIT		0x00
#define		VIA8233_MP_FORMAT_16BIT		0x80
#define		VIA8233_MP_FORMAT_CHANNLE_MASK	0x70 /* 1, 2, 4, 6 */
#define VIA8233_OFF_MP_SCRATCH		0x03
#define VIA8233_OFF_MP_STOP		0x08

#define	AUVIA_CODEC_CTL			0x80
#define		AUVIA_CODEC_READ		0x00800000
#define		AUVIA_CODEC_BUSY		0x01000000
#define		AUVIA_CODEC_PRIVALID		0x02000000
#define		AUVIA_CODEC_INDEX(x)		((x)<<16)

#define CH_WRITE1(sc, ch, off, v)	\
	bus_space_write_1((sc)->sc_iot,	(sc)->sc_ioh, (ch)->sc_base + (off), v)
#define CH_WRITE4(sc, ch, off, v)	\
	bus_space_write_4((sc)->sc_iot,	(sc)->sc_ioh, (ch)->sc_base + (off), v)
#define CH_READ1(sc, ch, off)		\
	bus_space_read_1((sc)->sc_iot, (sc)->sc_ioh, (ch)->sc_base + (off))
#define CH_READ4(sc, ch, off)		\
	bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (ch)->sc_base + (off))

#define TIMEOUT	50

struct audio_hw_if auvia_hw_if = {
	auvia_open,
	auvia_close,
	NULL, /* drain */
	auvia_query_encoding,
	auvia_set_params,
	auvia_round_blocksize,
	NULL, /* commit_settings */
	NULL, /* init_output */
	NULL, /* init_input */
	NULL, /* start_output */
	NULL, /* start_input */
	auvia_halt_output,
	auvia_halt_input,
	NULL, /* speaker_ctl */
	auvia_getdev,
	NULL, /* setfd */
	auvia_set_port,
	auvia_get_port,
	auvia_query_devinfo,
	auvia_malloc,
	auvia_free,
	auvia_round_buffersize,
	auvia_mappage,
	auvia_get_props,
	auvia_trigger_output,
	auvia_trigger_input,
	NULL, /* dev_ioctl */
};

int	auvia_attach_codec(void *, struct ac97_codec_if *);
int	auvia_write_codec(void *, u_int8_t, u_int16_t);
int	auvia_read_codec(void *, u_int8_t, u_int16_t *);
void	auvia_reset_codec(void *);
int	auvia_waitready_codec(struct auvia_softc *sc);
int	auvia_waitvalid_codec(struct auvia_softc *sc);


int
auvia_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_VIATECH)
		return 0;
	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_VIATECH_VT82C686A_AC97:
	case PCI_PRODUCT_VIATECH_VT8233_AC97:
		break;
	default:
		return 0;
	}

	return 1;
}


void
auvia_attach(struct device *parent, struct device *self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct auvia_softc *sc = (struct auvia_softc *) self;
	const char *intrstr = NULL;
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t pt = pa->pa_tag;
	pci_intr_handle_t ih;
	bus_size_t iosize;
	pcireg_t pr;
	int r;

	sc->sc_play.sc_base = AUVIA_PLAY_BASE;
	sc->sc_record.sc_base = AUVIA_RECORD_BASE;
	if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_VIATECH_VT8233_AC97) {
		sc->sc_flags |= AUVIA_FLAGS_VT8233;
		sc->sc_play.sc_base = VIA8233_MP_BASE;
	}

	if (pci_mapreg_map(pa, 0x10, PCI_MAPREG_TYPE_IO, 0, &sc->sc_iot,
		&sc->sc_ioh, NULL, &iosize)) {
		printf(": can't map i/o space\n");
		return;
	}

	sc->sc_dmat = pa->pa_dmat;
	sc->sc_pc = pc;
	sc->sc_pt = pt;

	r = PCI_REVISION(pa->pa_class);
	if (sc->sc_flags & AUVIA_FLAGS_VT8233) {
		sprintf(sc->sc_revision, "0x%02X", r);
		if (r < 0x50) {
			printf(": VIA VT8233 AC'97 Audio (rev %s)\n",
			       sc->sc_revision);
		} else {
			printf(": VIA VT8235 AC'97 Audio (rev %s)\n",
			       sc->sc_revision);
		}
	} else {
		sc->sc_revision[1] = '\0';
		if (r == 0x20) {
			sc->sc_revision[0] = 'H';
		} else if ((r >= 0x10) && (r <= 0x14)) {
			sc->sc_revision[0] = 'A' + (r - 0x10);
		} else {
			sprintf(sc->sc_revision, "0x%02X", r);
		}

		printf(": VIA VT82C686A AC'97 Audio (rev %s)\n",
		       sc->sc_revision);
	}

	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, iosize);
		return;
	}
	intrstr = pci_intr_string(pc, ih);

	sc->sc_ih = pci_intr_establish(pc, ih, IPL_AUDIO, auvia_intr, sc);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt",sc->sc_dev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, iosize);
		return;
	}

	printf("%s: interrupting at %s\n", sc->sc_dev.dv_xname, intrstr);

	/* disable SBPro compat & others */
	pr = pci_conf_read(pc, pt, AUVIA_PCICONF_JUNK);

	pr &= ~AUVIA_PCICONF_ENABLES; /* clear compat function enables */
	/* XXX what to do about MIDI, FM, joystick? */

	pr |= (AUVIA_PCICONF_ACLINKENAB | AUVIA_PCICONF_ACNOTRST
		| AUVIA_PCICONF_ACVSR | AUVIA_PCICONF_ACSGD);

	pr &= ~(AUVIA_PCICONF_ACFM | AUVIA_PCICONF_ACSB);

	pci_conf_write(pc, pt, AUVIA_PCICONF_JUNK, pr);

	sc->host_if.arg = sc;
	sc->host_if.attach = auvia_attach_codec;
	sc->host_if.read = auvia_read_codec;
	sc->host_if.write = auvia_write_codec;
	sc->host_if.reset = auvia_reset_codec;

	if ((r = ac97_attach(&sc->host_if)) != 0) {
		printf("%s: can't attach codec (error 0x%X)\n",
			sc->sc_dev.dv_xname, r);
		pci_intr_disestablish(pc, sc->sc_ih);
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, iosize);
		return;
	}

	audio_attach_mi(&auvia_hw_if, sc, &sc->sc_dev);
}


int
auvia_attach_codec(void *addr, struct ac97_codec_if *cif)
{
	struct auvia_softc *sc = addr;

	sc->codec_if = cif;

	return 0;
}


void
auvia_reset_codec(void *addr)
{
#ifdef notyet /* XXX seems to make codec become unready... ??? */
	struct auvia_softc *sc = addr;
	pcireg_t r;

	/* perform a codec cold reset */

	r = pci_conf_read(sc->sc_pc, sc->sc_pt, AUVIA_PCICONF_JUNK);

	r &= ~AUVIA_PCICONF_ACNOTRST;	/* enable RESET (active low) */
	pci_conf_write(sc->sc_pc, sc->sc_pt, AUVIA_PCICONF_JUNK, r);
	delay(2);

	r |= AUVIA_PCICONF_ACNOTRST;	/* disable RESET (inactive high) */
	pci_conf_write(sc->sc_pc, sc->sc_pt, AUVIA_PCICONF_JUNK, r);
	delay(200);

	auvia_waitready_codec(sc);
#endif
}


int
auvia_waitready_codec(struct auvia_softc *sc)
{
	int i;

	/* poll until codec not busy */
	for (i = 0; (i < TIMEOUT) && (bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		AUVIA_CODEC_CTL) & AUVIA_CODEC_BUSY); i++)
		delay(1);
	if (i >= TIMEOUT) {
		printf("%s: codec busy\n", sc->sc_dev.dv_xname);
		return 1;
	}

	return 0;
}


int
auvia_waitvalid_codec(struct auvia_softc *sc)
{
	int i;

	/* poll until codec valid */
	for (i = 0; (i < TIMEOUT) && !(bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		AUVIA_CODEC_CTL) & AUVIA_CODEC_PRIVALID); i++)
			delay(1);
	if (i >= TIMEOUT) {
		printf("%s: codec invalid\n", sc->sc_dev.dv_xname);
		return 1;
	}

	return 0;
}


int
auvia_write_codec(void *addr, u_int8_t reg, u_int16_t val)
{
	struct auvia_softc *sc = addr;

	if (auvia_waitready_codec(sc))
		return 1;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, AUVIA_CODEC_CTL,
		AUVIA_CODEC_PRIVALID | AUVIA_CODEC_INDEX(reg) | val);

	return 0;
}


int
auvia_read_codec(void *addr, u_int8_t reg, u_int16_t *val)
{
	struct auvia_softc *sc = addr;

	if (auvia_waitready_codec(sc))
		return 1;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, AUVIA_CODEC_CTL,
		AUVIA_CODEC_PRIVALID | AUVIA_CODEC_READ | AUVIA_CODEC_INDEX(reg));

	if (auvia_waitready_codec(sc))
		return 1;

	if (auvia_waitvalid_codec(sc))
		return 1;

	*val = bus_space_read_2(sc->sc_iot, sc->sc_ioh, AUVIA_CODEC_CTL);

	return 0;
}


int
auvia_open(void *addr, int flags)
{
	return 0;
}


void
auvia_close(void *addr)
{
	struct auvia_softc *sc = addr;

	auvia_halt_output(sc);
	auvia_halt_input(sc);

	sc->sc_play.sc_intr = NULL;
	sc->sc_record.sc_intr = NULL;
}


int
auvia_query_encoding(void *addr, struct audio_encoding *fp)
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

void
auvia_set_params_sub(struct auvia_softc *sc, struct auvia_softc_chan *ch,
		     struct audio_params *p)
{
	u_int32_t v;
	u_int16_t regval;

	if (!(sc->sc_flags & AUVIA_FLAGS_VT8233)) {
		regval = (p->channels == 2 ? AUVIA_RPMODE_STEREO : 0)
			| (p->precision * p->factor == 16 ?
				AUVIA_RPMODE_16BIT : 0)
			| AUVIA_RPMODE_INTR_FLAG | AUVIA_RPMODE_INTR_EOL
			| AUVIA_RPMODE_AUTOSTART;
		ch->sc_reg = regval;
	} else if (ch->sc_base != VIA8233_MP_BASE) {
		v = CH_READ4(sc, ch, VIA8233_RP_RATEFMT);
		v &= ~(VIA8233_RATEFMT_48K | VIA8233_RATEFMT_STEREO
			| VIA8233_RATEFMT_16BIT);

		v |= VIA8233_RATEFMT_48K * (p->sample_rate / 20)
			/ (48000 / 20);
		if (p->channels == 2)
			v |= VIA8233_RATEFMT_STEREO;
		if (p->precision == 16)
			v |= VIA8233_RATEFMT_16BIT;

		CH_WRITE4(sc, ch, VIA8233_RP_RATEFMT, v);
	} else {
		static const u_int32_t slottab[7] =
			{ 0, 0xff000011, 0xff000021, 0,
			  0xff004321, 0, 0xff436521};

		regval = (p->hw_precision == 16
			? VIA8233_MP_FORMAT_16BIT : VIA8233_MP_FORMAT_8BIT)
			| (p->hw_channels << 4);
		CH_WRITE1(sc, ch, VIA8233_OFF_MP_FORMAT, regval);
		CH_WRITE4(sc, ch, VIA8233_OFF_MP_STOP, slottab[p->hw_channels]);
	}
}

int
auvia_set_params(void *addr, int setmode, int usemode,
	struct audio_params *play, struct audio_params *rec)
{
	struct auvia_softc *sc = addr;
	struct auvia_softc_chan *ch;
	struct audio_params *p;
	struct ac97_codec_if* codec;
	int reg, mode;
	u_int16_t ext_id;

	codec = sc->codec_if;
	/* for mode in (RECORD, PLAY) */
	for (mode = AUMODE_RECORD; mode != -1;
	     mode = mode == AUMODE_RECORD ? AUMODE_PLAY : -1) {
		if ((setmode & mode) == 0)
			continue;

		if (mode == AUMODE_PLAY ) {
			p = play;
			ch = &sc->sc_play;
			reg = AC97_REG_PCM_FRONT_DAC_RATE;
		} else {
			p = rec;
			ch = &sc->sc_record;
			reg = AC97_REG_PCM_LR_ADC_RATE;
		}

		if (ch->sc_base == VIA8233_MP_BASE && mode == AUMODE_PLAY) {
			ext_id = codec->vtbl->get_extcaps(codec);
			if (p->channels == 1) {
				/* ok */
			} else if (p->channels == 2) {
				/* ok */
			} else if (p->channels == 4
				&& ext_id & AC97_EXT_AUDIO_SDAC) {
				/* ok */
#define BITS_6CH	(AC97_EXT_AUDIO_SDAC | AC97_EXT_AUDIO_CDAC | AC97_EXT_AUDIO_LDAC)
			} else if (p->channels == 6
				&& (ext_id & BITS_6CH) == BITS_6CH) {
				/* ok */
			} else {
				return (EINVAL);
			}
		} else {
			if (p->channels != 1 && p->channels != 2)
				return (EINVAL);
		}
		if (p->sample_rate < 4000 || p->sample_rate > 48000 ||
		    (p->precision != 8 && p->precision != 16))
			return (EINVAL);

		if (IS_FIXED_RATE(codec)) {
			/* Enable aurateconv */
			p->hw_sample_rate = AC97_SINGLE_RATE;
		} else {
			if (codec->vtbl->set_rate(codec, reg, &p->sample_rate))
				return (EINVAL);
			reg = AC97_REG_PCM_SURR_DAC_RATE;
			if (p->channels >= 4
			    && codec->vtbl->set_rate(codec, reg,
						     &p->sample_rate))
				return (EINVAL);
			reg = AC97_REG_PCM_LFE_DAC_RATE;
			if (p->channels == 6
			    && codec->vtbl->set_rate(codec, reg,
						     &p->sample_rate))
				return (EINVAL);
		}

		p->factor = 1;
		p->sw_code = 0;
		switch (p->encoding) {
		case AUDIO_ENCODING_SLINEAR_BE:
			if (p->precision == 16) {
				p->sw_code = swap_bytes;
				p->hw_encoding = AUDIO_ENCODING_SLINEAR_LE;
			} else {
				p->sw_code = change_sign8;
				p->hw_encoding = AUDIO_ENCODING_ULINEAR;
			}
			break;
		case AUDIO_ENCODING_SLINEAR_LE:
			if (p->precision != 16) {
				p->sw_code = change_sign8;
				p->hw_encoding = AUDIO_ENCODING_ULINEAR;
			}
			break;
		case AUDIO_ENCODING_ULINEAR_BE:
			if (p->precision == 16) {
				if (mode == AUMODE_PLAY)
					p->sw_code = swap_bytes_change_sign16_le;
				else
					p->sw_code = change_sign16_swap_bytes_le;
				p->hw_encoding = AUDIO_ENCODING_SLINEAR_LE;
			}
			break;
		case AUDIO_ENCODING_ULINEAR_LE:
			if (p->precision == 16) {
				p->sw_code = change_sign16_le;
				p->hw_encoding = AUDIO_ENCODING_SLINEAR_LE;
			}
			break;
		case AUDIO_ENCODING_ULAW:
			if (p->precision != 8)
				return (EINVAL);
			if (mode == AUMODE_PLAY) {
				p->factor = 2;
				p->sw_code = mulaw_to_slinear16_le;
				p->hw_encoding = AUDIO_ENCODING_SLINEAR_LE;
				p->hw_precision = 16;
			} else if (!IS_FIXED_RATE(codec)) {
				p->sw_code = ulinear8_to_mulaw;
				p->hw_encoding = AUDIO_ENCODING_ULINEAR;
			} else {
				/* aurateconv supports no 8 bit PCM */
				p->factor = 2;
				p->sw_code = slinear16_to_mulaw_le;
				p->hw_encoding = AUDIO_ENCODING_SLINEAR_LE;
				p->hw_precision = 16;
			}
			break;
		case AUDIO_ENCODING_ALAW:
			if (p->precision != 8)
				return (EINVAL);
			if (mode == AUMODE_PLAY) {
				p->factor = 2;
				p->sw_code = alaw_to_slinear16_le;
				p->hw_encoding = AUDIO_ENCODING_SLINEAR_LE;
				p->hw_precision = 16;
			} else if (!IS_FIXED_RATE(codec)) {
				p->sw_code = ulinear8_to_alaw;
				p->hw_encoding = AUDIO_ENCODING_ULINEAR;
			} else {
				/* aurateconv supports no 8 bit PCM */
				p->factor = 2;
				p->sw_code = slinear16_to_alaw_le;
				p->hw_encoding = AUDIO_ENCODING_SLINEAR_LE;
				p->hw_precision = 16;
			}
			break;
		default:
			return (EINVAL);
		}
		auvia_set_params_sub(sc, ch, p);
	}

	return 0;
}


int
auvia_round_blocksize(void *addr, int blk)
{
	return (blk & -32);
}


int
auvia_halt_output(void *addr)
{
	struct auvia_softc *sc = addr;
	struct auvia_softc_chan *ch = &(sc->sc_play);

	CH_WRITE1(sc, ch, AUVIA_RP_CONTROL, AUVIA_RPCTRL_TERMINATE);
	return 0;
}


int
auvia_halt_input(void *addr)
{
	struct auvia_softc *sc = addr;
	struct auvia_softc_chan *ch = &(sc->sc_record);

	CH_WRITE1(sc, ch, AUVIA_RP_CONTROL, AUVIA_RPCTRL_TERMINATE);
	return 0;
}


int
auvia_getdev(void *addr, struct audio_device *retp)
{
	struct auvia_softc *sc = addr;

	if (retp) {
		if (sc->sc_flags & AUVIA_FLAGS_VT8233) {
			strncpy(retp->name, "VIA VT8233/8235",
				sizeof(retp->name));
		} else {
			strncpy(retp->name, "VIA VT82C686A",
				sizeof(retp->name));
		}
		strncpy(retp->version, sc->sc_revision, sizeof(retp->version));
		strncpy(retp->config, "auvia", sizeof(retp->config));
	}

	return 0;
}


int
auvia_set_port(void *addr, mixer_ctrl_t *cp)
{
	struct auvia_softc *sc = addr;

	return (sc->codec_if->vtbl->mixer_set_port(sc->codec_if, cp));
}


int
auvia_get_port(void *addr, mixer_ctrl_t *cp)
{
	struct auvia_softc *sc = addr;

	return (sc->codec_if->vtbl->mixer_get_port(sc->codec_if, cp));
}


int
auvia_query_devinfo(void *addr, mixer_devinfo_t *dip)
{
	struct auvia_softc *sc = addr;

	return (sc->codec_if->vtbl->query_devinfo(sc->codec_if, dip));
}


void *
auvia_malloc(void *addr, int direction, size_t size, int pool, int flags)
{
	struct auvia_softc *sc = addr;
	struct auvia_dma *p;
	int error;
	int rseg;

	p = malloc(sizeof(*p), pool, flags);
	if (!p)
		return 0;

	p->size = size;
	if ((error = bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0, &p->seg,
				      1, &rseg, BUS_DMA_NOWAIT)) != 0) {
		printf("%s: unable to allocate dma, error = %d\n",
		       sc->sc_dev.dv_xname, error);
		goto fail_alloc;
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, &p->seg, rseg, size, &p->addr,
				    BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) != 0) {
		printf("%s: unable to map dma, error = %d\n",
		       sc->sc_dev.dv_xname, error);
		goto fail_map;
	}

	if ((error = bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
				       BUS_DMA_NOWAIT, &p->map)) != 0) {
		printf("%s: unable to create dma map, error = %d\n",
		       sc->sc_dev.dv_xname, error);
		goto fail_create;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat, p->map, p->addr, size, NULL,
				     BUS_DMA_NOWAIT)) != 0) {
		printf("%s: unable to load dma map, error = %d\n",
		       sc->sc_dev.dv_xname, error);
		goto fail_load;
	}

	p->next = sc->sc_dmas;
	sc->sc_dmas = p;

	return p->addr;


fail_load:
	bus_dmamap_destroy(sc->sc_dmat, p->map);
fail_create:
	bus_dmamem_unmap(sc->sc_dmat, p->addr, size);
fail_map:
	bus_dmamem_free(sc->sc_dmat, &p->seg, 1);
fail_alloc:
	free(p, pool);
	return 0;
}


void
auvia_free(void *addr, void *ptr, int pool)
{
	struct auvia_softc *sc = addr;
	struct auvia_dma **pp, *p;

	for (pp = &(sc->sc_dmas); (p = *pp) != NULL; pp = &p->next)
		if (p->addr == ptr) {
			bus_dmamap_unload(sc->sc_dmat, p->map);
			bus_dmamap_destroy(sc->sc_dmat, p->map);
			bus_dmamem_unmap(sc->sc_dmat, p->addr, p->size);
			bus_dmamem_free(sc->sc_dmat, &p->seg, 1);

			*pp = p->next;
			free(p, pool);
			return;
		}

	panic("auvia_free: trying to free unallocated memory");
}


size_t
auvia_round_buffersize(void *addr, int direction, size_t size)
{
	return size;
}


paddr_t
auvia_mappage(void *addr, void *mem, off_t off, int prot)
{
	struct auvia_softc *sc = addr;
	struct auvia_dma *p;

	if (off < 0)
		return -1;

	for (p = sc->sc_dmas; p && p->addr != mem; p = p->next)
		;

	if (!p)
		return -1;

	return bus_dmamem_mmap(sc->sc_dmat, &p->seg, 1, off, prot,
	       BUS_DMA_WAITOK);
}


int
auvia_get_props(void *addr)
{
	struct auvia_softc *sc = addr;
	int props;

	props = AUDIO_PROP_INDEPENDENT | AUDIO_PROP_FULLDUPLEX;
	/*
	 * Even if the codec is fixed-rate, set_param() succeeds for any sample
	 * rate because of aurateconv.  Applications can't know what rate the
	 * device can process in the case of mmap().
	 */
	if (!IS_FIXED_RATE(sc->codec_if))
		props |= AUDIO_PROP_MMAP;
	return props;
}


int
auvia_build_dma_ops(struct auvia_softc *sc, struct auvia_softc_chan *ch,
	struct auvia_dma *p, void *start, void *end, int blksize)
{
	struct auvia_dma_op *op;
	struct auvia_dma *dp;
	bus_addr_t s, e;
	size_t l;
	int segs;

	s = p->map->dm_segs[0].ds_addr;
	l = ((char *)end - (char *)start);
	e = s + l;
	segs = (l + blksize - 1) / blksize;

	if (segs > (ch->sc_dma_op_count)) {
		/* if old list was too small, free it */
		if (ch->sc_dma_ops) {
			auvia_free(sc, ch->sc_dma_ops, M_DEVBUF);
		}

		ch->sc_dma_ops = auvia_malloc(sc, 0,
			sizeof(struct auvia_dma_op) * segs, M_DEVBUF, M_WAITOK);

		if (ch->sc_dma_ops == NULL) {
			printf("%s: couldn't build dmaops\n", sc->sc_dev.dv_xname);
			return 1;
		}

		for (dp = sc->sc_dmas;
			dp && dp->addr != (void *)(ch->sc_dma_ops);
			dp = dp->next)
				;

		if (!dp)
			panic("%s: build_dma_ops: where'd my memory go??? "
				"address (%p)\n", sc->sc_dev.dv_xname,
				ch->sc_dma_ops);

		ch->sc_dma_op_count = segs;
		ch->sc_dma_ops_dma = dp;
	}

	dp = ch->sc_dma_ops_dma;
	op = ch->sc_dma_ops;

	while (l) {
		op->ptr = s;
		l = l - blksize;
		if (!l) {
			/* if last block */
			op->flags = AUVIA_DMAOP_EOL | blksize;
		} else {
			op->flags = AUVIA_DMAOP_FLAG | blksize;
		}
		s += blksize;
		op++;
	}

	return 0;
}


int
auvia_trigger_output(void *addr, void *start, void *end,
	int blksize, void (*intr)(void *), void *arg,
	struct audio_params *param)
{
	struct auvia_softc *sc = addr;
	struct auvia_softc_chan *ch = &(sc->sc_play);
	struct auvia_dma *p;

	for (p = sc->sc_dmas; p && p->addr != start; p = p->next)
		;

	if (!p)
		panic("auvia_trigger_output: request with bad start "
			"address (%p)", start);

	if (auvia_build_dma_ops(sc, ch, p, start, end, blksize)) {
		return 1;
	}

	ch->sc_intr = intr;
	ch->sc_arg = arg;

	CH_WRITE4(sc, ch, AUVIA_RP_DMAOPS_BASE,
		ch->sc_dma_ops_dma->map->dm_segs[0].ds_addr);

	if (sc->sc_flags & AUVIA_FLAGS_VT8233) {
		if (ch->sc_base != VIA8233_MP_BASE) {
			CH_WRITE1(sc, ch, VIA8233_RP_DXS_LVOL, 0);
			CH_WRITE1(sc, ch, VIA8233_RP_DXS_RVOL, 0);
		}
		CH_WRITE1(sc, ch, AUVIA_RP_CONTROL,
			AUVIA_RPCTRL_START | AUVIA_RPCTRL_AUTOSTART |
			AUVIA_RPCTRL_STOP  | AUVIA_RPCTRL_EOL | AUVIA_RPCTRL_FLAG);
	} else {
		CH_WRITE1(sc, ch, AUVIA_RP_MODE, ch->sc_reg);
		CH_WRITE1(sc, ch, AUVIA_RP_CONTROL, AUVIA_RPCTRL_START);
	}

	return 0;
}


int
auvia_trigger_input(void *addr, void *start, void *end,
	int blksize, void (*intr)(void *), void *arg,
	struct audio_params *param)
{
	struct auvia_softc *sc = addr;
	struct auvia_softc_chan *ch = &(sc->sc_record);
	struct auvia_dma *p;

	for (p = sc->sc_dmas; p && p->addr != start; p = p->next)
		;

	if (!p)
		panic("auvia_trigger_input: request with bad start "
			"address (%p)", start);

	if (auvia_build_dma_ops(sc, ch, p, start, end, blksize)) {
		return 1;
	}

	ch->sc_intr = intr;
	ch->sc_arg = arg;

	CH_WRITE4(sc, ch, AUVIA_RP_DMAOPS_BASE,
		  ch->sc_dma_ops_dma->map->dm_segs[0].ds_addr);

	if (sc->sc_flags & AUVIA_FLAGS_VT8233) {
		CH_WRITE1(sc, ch, VIA8233_RP_DXS_LVOL, 0);
		CH_WRITE1(sc, ch, VIA8233_RP_DXS_RVOL, 0);
		CH_WRITE1(sc, ch, AUVIA_RP_CONTROL,
			AUVIA_RPCTRL_START | AUVIA_RPCTRL_AUTOSTART |
			AUVIA_RPCTRL_STOP  | AUVIA_RPCTRL_EOL | AUVIA_RPCTRL_FLAG);
	} else {
		CH_WRITE1(sc, ch, AUVIA_RP_MODE, ch->sc_reg);
		CH_WRITE1(sc, ch, AUVIA_RP_CONTROL, AUVIA_RPCTRL_START);
	}

	return 0;
}


int
auvia_intr(void *arg)
{
	struct auvia_softc *sc = arg;
	struct auvia_softc_chan *ch;
	u_int8_t r;
	int rval;

	rval = 0;

	ch = &sc->sc_record;
	r = CH_READ1(sc, ch, AUVIA_RP_STAT);
	if (r & AUVIA_RPSTAT_INTR) {
		if (sc->sc_record.sc_intr)
			sc->sc_record.sc_intr(sc->sc_record.sc_arg);

		/* clear interrupts */
		CH_WRITE1(sc, ch, AUVIA_RP_STAT, AUVIA_RPSTAT_INTR);
		rval = 1;
	}

	ch = &sc->sc_play;
	r = CH_READ1(sc, ch, AUVIA_RP_STAT);
	if (r & AUVIA_RPSTAT_INTR) {
		if (sc->sc_play.sc_intr)
			sc->sc_play.sc_intr(sc->sc_play.sc_arg);

		/* clear interrupts */
		CH_WRITE1(sc, ch, AUVIA_RP_STAT, AUVIA_RPSTAT_INTR);
		rval = 1;
	}

	return rval;
}
