/*	$NetBSD: vidcaudio.c,v 1.14.2.4 2004/11/02 07:50:22 skrll Exp $	*/

/*
 * Copyright (c) 1995 Melvin Tang-Richardson
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
 *	This product includes software developed by the RiscBSD team.
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

/*-
 * Copyright (c) 2003 Ben Harris
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
 * audio driver for the RiscPC 8/16 bit sound
 *
 * Interfaces with the NetBSD generic audio driver to provide SUN
 * /dev/audio (partial) compatibility.
 */

#include <sys/param.h>	/* proc.h */

__KERNEL_RCSID(0, "$NetBSD: vidcaudio.c,v 1.14.2.4 2004/11/02 07:50:22 skrll Exp $");

#include <sys/audioio.h>
#include <sys/conf.h>   /* autoconfig functions */
#include <sys/device.h> /* device calls */
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/proc.h>	/* device calls */
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#include <dev/audio_if.h>
#include <dev/audiobellvar.h>
#include <dev/mulaw.h>

#include <machine/intr.h>
#include <machine/machdep.h>
#include <arm/arm32/katelib.h>

#include <arm/iomd/vidcaudiovar.h>
#include <arm/iomd/iomdreg.h>
#include <arm/iomd/iomdvar.h>
#include <arm/iomd/vidc.h>
#include <arm/mainbus/mainbus.h>

#include "pckbd.h"
#if NPCKBD > 0
#include <dev/pckbport/pckbdvar.h>
#endif

#include "rpckbd.h"
#if NRPCKBD > 0
#include <arm/iomd/rpckbdvar.h>
#endif

#include "vt.h"
#if NVT > 0
extern void vt_hookup_bell(void (*)(void *, u_int, u_int, u_int, int), void *);
#endif

extern int *vidc_base;

#ifdef VIDCAUDIO_DEBUG
#define DPRINTF(x)	printf x
#else
#define DPRINTF(x)
#endif

struct vidcaudio_softc {
	struct	device sc_dev;

	irqhandler_t	sc_ih;
	int	sc_dma_intr;

	int	sc_is16bit;

	size_t	sc_pblksize;
	vm_offset_t	sc_poffset;
	vm_offset_t	sc_pbufsize;
	paddr_t	*sc_ppages;
	void	(*sc_pintr)(void *);
	void	*sc_parg;
	int	sc_pcountdown;
};

static int  vidcaudio_probe(struct device *, struct cfdata *, void *);
static void vidcaudio_attach(struct device *, struct device *, void *);
static int  vidcaudio_open(void *, int);
static void vidcaudio_close(void *);

static int vidcaudio_intr(void *);
static void vidcaudio_rate(int);
static void vidcaudio_ctrl(int);
static void vidcaudio_stereo(int, int);
static void mulaw_to_vidc(void *, u_char *, int);
static void mulaw_to_vidc_stereo(void *, u_char *, int);

CFATTACH_DECL(vidcaudio, sizeof(struct vidcaudio_softc),
    vidcaudio_probe, vidcaudio_attach, NULL, NULL);

static int    vidcaudio_query_encoding(void *, struct audio_encoding *);
static int    vidcaudio_set_params(void *, int, int, struct audio_params *,
    struct audio_params *);
static int    vidcaudio_round_blocksize(void *, int);
static int    vidcaudio_trigger_output(void *, void *, void *, int,
    void (*)(void *), void *, struct audio_params *);
static int    vidcaudio_trigger_input(void *, void *, void *, int,
    void (*)(void *), void *, struct audio_params *);
static int    vidcaudio_halt_output(void *);
static int    vidcaudio_halt_input(void *);
static int    vidcaudio_getdev(void *, struct audio_device *);
static int    vidcaudio_set_port(void *, mixer_ctrl_t *);
static int    vidcaudio_get_port(void *, mixer_ctrl_t *);
static int    vidcaudio_query_devinfo(void *, mixer_devinfo_t *);
static int    vidcaudio_get_props(void *);

static struct audio_device vidcaudio_device = {
	"ARM VIDC",
	"",
	"vidcaudio"
};

static const struct audio_hw_if vidcaudio_hw_if = {
	vidcaudio_open,
	vidcaudio_close,
	NULL,
	vidcaudio_query_encoding,
	vidcaudio_set_params,
	vidcaudio_round_blocksize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	vidcaudio_halt_output,
	vidcaudio_halt_input,
	NULL,
	vidcaudio_getdev,
	NULL,
	vidcaudio_set_port,
	vidcaudio_get_port,
	vidcaudio_query_devinfo,
	NULL,
	NULL,
	NULL,
	NULL,
	vidcaudio_get_props,
	vidcaudio_trigger_output,
	vidcaudio_trigger_input,
	NULL,
};

static int
vidcaudio_probe(struct device *parent, struct cfdata *cf, void *aux)
{
	int id;

	id = IOMD_ID;

	/* So far I only know about this IOMD */
	switch (id) {
	case RPC600_IOMD_ID:
	case ARM7500_IOC_ID:
	case ARM7500FE_IOC_ID:
		return 1;
	default:
		aprint_error("vidcaudio: Unknown IOMD id=%04x", id);
		return 0;
	}
}


static void
vidcaudio_attach(struct device *parent, struct device *self, void *aux)
{
	struct vidcaudio_softc *sc = (void *)self;
	struct device *beepdev;

	switch (IOMD_ID) {
#ifndef EB7500ATX
	case RPC600_IOMD_ID:
		sc->sc_is16bit = (cmos_read(0xc4) >> 5) & 1;
		sc->sc_dma_intr = IRQ_DMASCH0;
		break;
#endif
	case ARM7500_IOC_ID:
	case ARM7500FE_IOC_ID:
		sc->sc_is16bit = TRUE;
		sc->sc_dma_intr = IRQ_SDMA;
		break;
	default:
		aprint_error(": strange IOMD\n");
		return;
	}

	if (sc->sc_is16bit)
		aprint_normal(": 16-bit external DAC\n");
	else
		aprint_normal(": 8-bit internal DAC\n");

	/* Install the irq handler for the DMA interrupt */
	sc->sc_ih.ih_func = vidcaudio_intr;
	sc->sc_ih.ih_arg = sc;
	sc->sc_ih.ih_level = IPL_AUDIO;
	sc->sc_ih.ih_name = self->dv_xname;

	if (irq_claim(sc->sc_dma_intr, &sc->sc_ih) != 0) {
		aprint_error("%s: couldn't claim IRQ %d\n",
		    self->dv_xname, sc->sc_dma_intr);
		return;
	}

	disable_irq(sc->sc_dma_intr);

	beepdev = audio_attach_mi(&vidcaudio_hw_if, sc, self);
#if NPCKBD > 0
	pckbd_hookup_bell(audiobell, beepdev);
#endif
#if NRPCKBD > 0
	rpckbd_hookup_bell(audiobell, beepdev);
#endif
#if NVT > 0
	vt_hookup_bell(audiobell, beepdev);
#endif
}

static int
vidcaudio_open(void *addr, int flags)
{

	DPRINTF(("DEBUG: vidcaudio_open called\n"));
	return 0;
}
 
static void
vidcaudio_close(void *addr)
{
	struct vidcaudio_softc *sc = addr;

	DPRINTF(("DEBUG: vidcaudio_close called\n"));
	/*
	 * We do this here rather than in vidcaudio_halt_output()
	 * because the latter can be called from interrupt context
	 * (audio_pint()->audio_clear()->vidcaudio_halt_output()).
	 */
	if (sc->sc_ppages != NULL) {
		free(sc->sc_ppages, M_DEVBUF);
		sc->sc_ppages = NULL;
	}
}

/*
 * Interface to the generic audio driver
 */

static int
vidcaudio_query_encoding(void *addr, struct audio_encoding *fp)
{
	struct vidcaudio_softc *sc = addr;

	switch (fp->index) {
	case 0:
		strcpy(fp->name, AudioEmulaw);
		fp->encoding = AUDIO_ENCODING_ULAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;

	case 1:
		if (sc->sc_is16bit) {
			strcpy(fp->name, AudioEslinear_le);
			fp->encoding = AUDIO_ENCODING_SLINEAR_LE;
			fp->precision = 16;
			fp->flags = 0;
			break;
		}
		/* FALLTHROUGH */
	default:
		return EINVAL;
	}
	return 0;
}

#define MULAW_TO_VIDC(m) (~((m) << 1 | (m) >> 7))

static void
mulaw_to_vidc(void *v, u_char *p, int cc)
{

	while (--cc >= 0) {
		*p = MULAW_TO_VIDC(*p);
		++p;
	}
}

static void
mulaw_to_vidc_stereo(void *v, u_char *p, int cc)
{
	u_char *q = p;

	p += cc;
	q += cc * 2;
	while (--cc >= 0) {
		q -= 2;
		--p;
		q[0] = q[1] = MULAW_TO_VIDC(*p);
	}
}

static int
vidcaudio_set_params(void *addr, int setmode, int usemode,
    struct audio_params *p, struct audio_params *r)
{
	struct vidcaudio_softc *sc = addr;
	int sample_period, ch;

	if ((setmode & AUMODE_PLAY) == 0)
		return 0;

	if (sc->sc_is16bit) {
		/* ARM7500ish, 16-bit, two-channel */
		if (p->encoding == AUDIO_ENCODING_ULAW && p->precision == 8) {
			p->sw_code = mulaw_to_slinear16_le;
			p->factor = 2; p->factor_denom = 1;
		} else if (p->encoding != AUDIO_ENCODING_SLINEAR_LE ||
		    p->precision != 16)
			return EINVAL;
		p->hw_channels = 2;
		sample_period = 705600 / 4 / p->sample_rate;
		if (sample_period < 3) sample_period = 3;
		p->hw_sample_rate = 705600 / 4 / sample_period;
		vidcaudio_rate(sample_period - 2);
		vidcaudio_ctrl(SCR_SERIAL);
		p->hw_encoding = AUDIO_ENCODING_SLINEAR_LE;
		p->hw_precision = 16;
	} else {
		/* VIDC20ish, u-law, 8-channel */
		if (p->encoding != AUDIO_ENCODING_ULAW || p->precision != 8)
			return EINVAL;
		/*
		 * We always use two hardware channels, because using
		 * one at 8kHz gives a nasty whining sound from the
		 * speaker.  The aurateconv mechanism doesn't support
		 * ulaw, so we do the channel duplication ourselves,
		 * and don't try to do rate conversion.
		 */
		switch (p->channels) {
		case 1:
			p->sw_code = mulaw_to_vidc_stereo;
			p->factor = 2; p->factor_denom = 1;
			break;
		case 2:
			p->sw_code = mulaw_to_vidc;
			p->factor = 1; p->factor_denom = 1;
			break;
		default:
			return EINVAL;
		}
		p->hw_channels = p->channels;
		sample_period = 1000000 / 2 / p->sample_rate;
		if (sample_period < 3) sample_period = 3;
		p->hw_sample_rate = p->sample_rate =
		    1000000 / 2 / sample_period;
		p->hw_encoding = AUDIO_ENCODING_NONE;
		p->hw_precision = 8;
		vidcaudio_rate(sample_period - 2);
		vidcaudio_ctrl(SCR_SDAC | SCR_CLKSEL);
		if (p->hw_channels == 1)
			for (ch = 0; ch < 8; ch++)
				vidcaudio_stereo(ch, SIR_CENTRE);
		else {
			for (ch = 0; ch < 8; ch += 2)
				vidcaudio_stereo(ch, SIR_LEFT_100);
			for (ch = 1; ch < 8; ch += 2)
				vidcaudio_stereo(ch, SIR_RIGHT_100);
		}
	}
	return 0;
}

static int
vidcaudio_round_blocksize(void *addr, int wantblk)
{
	int blk;

	/* 
	 * Find the smallest power of two that's larger than the
	 * requested block size, but don't allow < 32 (DMA burst is 16
	 * bytes, and single bursts are tricky) or > PAGE_SIZE (DMA is
	 * confined to a page).
	 */

	for (blk = 32; blk < PAGE_SIZE; blk <<= 1)
		if (blk >= wantblk) return blk;
	return blk;
}

static int
vidcaudio_trigger_output(void *addr, void *start, void *end, int blksize,
    void (*intr)(void *), void *arg, struct audio_params *params)
{
	struct vidcaudio_softc *sc = addr;
	size_t npages, i;

	DPRINTF(("vidcaudio_trigger_output %p-%p/0x%x\n",
	    start, end, blksize));

	KASSERT(blksize == vidcaudio_round_blocksize(addr, blksize));
	KASSERT((vaddr_t)start % blksize == 0);

	sc->sc_pblksize = blksize;
	sc->sc_pbufsize = (char *)end - (char *)start;
	npages = sc->sc_pbufsize >> PGSHIFT;
	if (sc->sc_ppages != NULL)
		free(sc->sc_ppages, M_DEVBUF);
	sc->sc_ppages = malloc(npages * sizeof(paddr_t), M_DEVBUF, M_WAITOK);
	if (sc->sc_ppages == NULL) return ENOMEM;
	for (i = 0; i < npages; i++)
		if (!pmap_extract(pmap_kernel(),
		    (vaddr_t)start + i * PAGE_SIZE, &sc->sc_ppages[i]))
			return EIO;
	sc->sc_poffset = 0;
	sc->sc_pintr = intr;
	sc->sc_parg = arg;

	IOMD_WRITE_WORD(IOMD_SD0CR, IOMD_DMACR_CLEAR | IOMD_DMACR_QUADWORD);
	IOMD_WRITE_WORD(IOMD_SD0CR, IOMD_DMACR_ENABLE | IOMD_DMACR_QUADWORD);

	/*
	 * Queue up the first two blocks, but don't tell audio code
	 * we're finished with them yet.
	 */
	sc->sc_pcountdown = 2;

	enable_irq(sc->sc_dma_intr);

	return 0;
}

static int
vidcaudio_trigger_input(void *addr, void *start, void *end, int blksize,
    void (*intr)(void *), void *arg, struct audio_params *params)
{

	return ENODEV;
}

static int
vidcaudio_halt_output(void *addr)
{
	struct vidcaudio_softc *sc = addr;

	DPRINTF(("vidcaudio_halt_output\n"));
	disable_irq(sc->sc_dma_intr);
	IOMD_WRITE_WORD(IOMD_SD0CR, IOMD_DMACR_CLEAR | IOMD_DMACR_QUADWORD);
	return 0;
}

static int
vidcaudio_halt_input(void *addr)
{

	return ENODEV;
}

static int
vidcaudio_getdev(void *addr, struct audio_device *retp)
{

	*retp = vidcaudio_device;
	return 0;
}


static int
vidcaudio_set_port(void *addr, mixer_ctrl_t *cp)
{

	return EINVAL;
}

static int
vidcaudio_get_port(void *addr, mixer_ctrl_t *cp)
{

	return EINVAL;
}

static int
vidcaudio_query_devinfo(void *addr, mixer_devinfo_t *dip)
{

	return ENXIO;
}

static int
vidcaudio_get_props(void *addr)
{

	return 0;
}

static void
vidcaudio_rate(int rate)
{

	WriteWord(vidc_base, VIDC_SFR | rate);
}

static void
vidcaudio_ctrl(int ctrl)
{

	WriteWord(vidc_base, VIDC_SCR | ctrl);
}

static void
vidcaudio_stereo(int channel, int position)
{

	channel = channel << 24 | VIDC_SIR0;
	WriteWord(vidc_base, channel | position);
}

static int
vidcaudio_intr(void *arg)
{
	struct vidcaudio_softc *sc = arg;
	int status;
	paddr_t pnext, pend;

	status = IOMD_READ_BYTE(IOMD_SD0ST);
	DPRINTF(("I[%x]", status));
	if ((status & IOMD_DMAST_INT) == 0)
		return 0;

	pnext = sc->sc_ppages[sc->sc_poffset >> PGSHIFT] |
	    (sc->sc_poffset & PGOFSET);
	pend = (pnext + sc->sc_pblksize - 16) & IOMD_DMAEND_OFFSET;

	switch (status &
	    (IOMD_DMAST_OVERRUN | IOMD_DMAST_INT | IOMD_DMAST_BANKB)) {

	case (IOMD_DMAST_INT | IOMD_DMAST_BANKA):
	case (IOMD_DMAST_OVERRUN | IOMD_DMAST_INT | IOMD_DMAST_BANKB):
		DPRINTF(("B<0x%08lx,0x%03lx>", pnext, pend));
		IOMD_WRITE_WORD(IOMD_SD0CURB, pnext);
		IOMD_WRITE_WORD(IOMD_SD0ENDB, pend);
		break;

	case (IOMD_DMAST_INT | IOMD_DMAST_BANKB):
	case (IOMD_DMAST_OVERRUN | IOMD_DMAST_INT | IOMD_DMAST_BANKA):
		DPRINTF(("A<0x%08lx,0x%03lx>", pnext, pend));
		IOMD_WRITE_WORD(IOMD_SD0CURA, pnext);
		IOMD_WRITE_WORD(IOMD_SD0ENDA, pend);
		break;
	}

	sc->sc_poffset += sc->sc_pblksize;
	if (sc->sc_poffset >= sc->sc_pbufsize)
		sc->sc_poffset = 0;
	
	if (sc->sc_pcountdown > 0)
		sc->sc_pcountdown--;
	else
		(*sc->sc_pintr)(sc->sc_parg);

	return 1;
}
