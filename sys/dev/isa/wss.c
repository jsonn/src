/*	$NetBSD: wss.c,v 1.29.2.4 1997/10/14 10:24:07 thorpej Exp $	*/

/*
 * Copyright (c) 1994 John Brezak
 * Copyright (c) 1991-1993 Regents of the University of California.
 * All rights reserved.
 *
 * MAD support:
 * Copyright (c) 1996 Lennart Augustsson
 * Based on code which is
 * Copyright (c) 1995 Hannu Savolainen
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
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
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
 *
 */
/*
 * Copyright by Hannu Savolainen 1994
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer. 2.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/buf.h>

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/pio.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/ic/ad1848reg.h>
#include <dev/isa/ad1848var.h>
#include <dev/isa/wssreg.h>
#include <dev/isa/madreg.h>

/*
 * Mixer devices
 */
#define WSS_MIC_IN_LVL		0
#define WSS_LINE_IN_LVL		1
#define WSS_DAC_LVL		2
#define WSS_REC_LVL		3
#define WSS_MON_LVL		4
#define WSS_MIC_IN_MUTE		5
#define WSS_LINE_IN_MUTE	6
#define WSS_DAC_MUTE		7

#define WSS_RECORD_SOURCE	8

/* Classes */
#define WSS_INPUT_CLASS		9
#define WSS_RECORD_CLASS	10
#define WSS_MONITOR_CLASS	11

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (wssdebug) printf x
int	wssdebug = 0;
#else
#define DPRINTF(x)
#endif

struct wss_softc {
	struct	device sc_dev;		/* base device */
	struct	isadev sc_id;		/* ISA device */
	void	*sc_ih;			/* interrupt vectoring */
	bus_space_tag_t sc_iot;		/* tag */
	bus_space_handle_t sc_ioh;	/* handle */

	struct  ad1848_softc sc_ad1848;
#define wss_irq    sc_ad1848.sc_irq
#define wss_drq    sc_ad1848.sc_drq

	int 	mic_mute, cd_mute, dac_mute;
	int	mad_chip_type;		/* chip type if MAD emulation of WSS */
	bus_space_handle_t sc_mad_ioh;	/* MAD handle */
	bus_space_handle_t sc_mad_ioh1, sc_mad_ioh2, sc_mad_ioh3;
};

struct audio_device wss_device = {
	"wss,ad1848",
	"",
	"WSS"
};

int	wss_getdev __P((void *, struct audio_device *));

int	wss_set_out_port __P((void *, int));
int	wss_get_out_port __P((void *));
int	wss_set_in_port __P((void *, int));
int	wss_get_in_port __P((void *));
int	wss_mixer_set_port __P((void *, mixer_ctrl_t *));
int	wss_mixer_get_port __P((void *, mixer_ctrl_t *));
int	wss_query_devinfo __P((void *, mixer_devinfo_t *));

static int wss_to_vol __P((mixer_ctrl_t *, struct ad1848_volume *));
static int wss_from_vol __P((mixer_ctrl_t *, struct ad1848_volume *));

static int	wssfind __P((struct device *, struct wss_softc *, struct isa_attach_args *));

static int 	madprobe __P((struct wss_softc *, int));
static void	madattach __P((struct wss_softc *));
static void	madunmap __P((struct wss_softc *));

/*
 * Define our interface to the higher level audio driver.
 */

struct audio_hw_if wss_hw_if = {
	ad1848_open,
	ad1848_close,
	NULL,
	ad1848_query_encoding,
	ad1848_set_params,
	ad1848_round_blocksize,
	wss_set_out_port,
	wss_get_out_port,
	wss_set_in_port,
	wss_get_in_port,
	ad1848_commit_settings,
	ad1848_dma_init_output,
	ad1848_dma_init_input,
	ad1848_dma_output,
	ad1848_dma_input,
	ad1848_halt_out_dma,
	ad1848_halt_in_dma,
	ad1848_cont_out_dma,
	ad1848_cont_in_dma,
	NULL,
	wss_getdev,
	NULL,
	wss_mixer_set_port,
	wss_mixer_get_port,
	wss_query_devinfo,
	ad1848_malloc,
	ad1848_free,
	ad1848_round,
        ad1848_mappage,
	ad1848_get_props,
};

int	wssprobe __P((struct device *, void *, void *));
void	wssattach __P((struct device *, struct device *, void *));

struct cfattach wss_ca = {
	sizeof(struct wss_softc), wssprobe, wssattach
};

struct cfdriver wss_cd = {
	NULL, "wss", DV_DULL
};

/*
 * Probe for the Microsoft Sound System hardware.
 */
int
wssprobe(parent, match, aux)
    struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
    void *match;
#else
    struct cfdata *match;
#endif
    void *aux;
{
    struct wss_softc probesc, *sc = &probesc;

    bzero(sc, sizeof *sc);
#ifdef __BROKEN_INDIRECT_CONFIG
    sc->sc_dev.dv_cfdata = ((struct device *)match)->dv_cfdata;
#else
    sc->sc_dev.dv_cfdata = match;
#endif
    if (wssfind(parent, sc, aux)) {
        bus_space_unmap(sc->sc_iot, sc->sc_ioh, WSS_CODEC);
        ad1848_unmap(&sc->sc_ad1848);
        madunmap(sc);
        return 1;
    } else
        /* Everything is already unmapped */
        return 0;
}

static int
wssfind(parent, sc, ia)
    struct device *parent;
    struct wss_softc *sc;
    struct isa_attach_args *ia;
{
    static u_char interrupt_bits[12] = {
	-1, -1, -1, -1, -1, -1, -1, 0x08, -1, 0x10, 0x18, 0x20
    };
    static u_char dma_bits[4] = {1, 2, 0, 3};
    
    sc->sc_iot = ia->ia_iot;
    if (sc->sc_dev.dv_cfdata->cf_flags & 1)
	sc->mad_chip_type = madprobe(sc, ia->ia_iobase);
    else
	sc->mad_chip_type = MAD_NONE;

    if (!WSS_BASE_VALID(ia->ia_iobase)) {
	DPRINTF(("wss: configured iobase %x invalid\n", ia->ia_iobase));
	goto bad1;
    }

    /* Map the ports upto the AD1848 port */
    if (bus_space_map(sc->sc_iot, ia->ia_iobase, WSS_CODEC, 0, &sc->sc_ioh))
	goto bad1;

    sc->sc_ad1848.sc_iot = sc->sc_iot;
    sc->sc_ad1848.sc_iobase = ia->ia_iobase + WSS_CODEC;

    /* Is there an ad1848 chip at (WSS iobase + WSS_CODEC)? */
    if (ad1848_probe(&sc->sc_ad1848) == 0)
	goto bad;
	
    ia->ia_iosize = WSS_NPORT;

    /* Setup WSS interrupt and DMA */
    if (!WSS_DRQ_VALID(ia->ia_drq)) {
	DPRINTF(("wss: configured dma chan %d invalid\n", ia->ia_drq));
	goto bad;
    }
    sc->wss_drq = ia->ia_drq;

    /* XXX reqdrq? */
    if (sc->wss_drq != -1 && isa_drq_isfree(parent, sc->wss_drq) == 0)
	    goto bad;

#ifdef NEWCONFIG
    /*
     * If the IRQ wasn't compiled in, auto-detect it.
     */
    if (ia->ia_irq == IRQUNK) {
	ia->ia_irq = isa_discoverintr(ad1848_forceintr, &sc->sc_ad1848);
	if (!WSS_IRQ_VALID(ia->ia_irq)) {
	    printf("wss: couldn't auto-detect interrupt\n");
	    goto bad;
	}
    }
    else
#endif
    if (!WSS_IRQ_VALID(ia->ia_irq)) {
	DPRINTF(("wss: configured interrupt %d invalid\n", ia->ia_irq));
	goto bad;
    }

    sc->wss_irq = ia->ia_irq;

    bus_space_write_1(sc->sc_iot, sc->sc_ioh, WSS_CONFIG,
		      (interrupt_bits[ia->ia_irq] | dma_bits[ia->ia_drq]));

    if (sc->sc_ad1848.mode <= 1)
	ia->ia_drq2 = -1;
    return 1;

bad:
    bus_space_unmap(sc->sc_iot, sc->sc_ioh, WSS_CODEC);
bad1:
    madunmap(sc);
    return 0;
}

/*
 * Attach hardware to driver, attach hardware driver to audio
 * pseudo-device driver .
 */
void
wssattach(parent, self, aux)
    struct device *parent, *self;
    void *aux;
{
    struct wss_softc *sc = (struct wss_softc *)self;
    struct isa_attach_args *ia = (struct isa_attach_args *)aux;
    int version;
    
    if (!wssfind(parent, sc, ia)) {
        printf("%s: wssfind failed\n", sc->sc_dev.dv_xname);
        return;
    }

    madattach(sc);

    sc->sc_ad1848.sc_recdrq = sc->sc_ad1848.mode > 1 && ia->ia_drq2 != -1 ? ia->ia_drq2 : ia->ia_drq;
    sc->sc_ad1848.sc_isa = parent;

#ifdef NEWCONFIG
    isa_establish(&sc->sc_id, &sc->sc_dev);
#endif
    sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE, IPL_AUDIO,
        ad1848_intr, &sc->sc_ad1848);

    ad1848_attach(&sc->sc_ad1848);
    
    version = bus_space_read_1(sc->sc_iot, sc->sc_ioh, WSS_STATUS) & WSS_VERSMASK;
    printf(" (vers %d)", version);
    if (sc->mad_chip_type != MAD_NONE)
        printf(", %s",
               sc->mad_chip_type == MAD_82C929 ? "82C929" :
               sc->mad_chip_type == MAD_82C928 ? "82C928" :
               "OTI-601D");
    printf("\n");

    sc->sc_ad1848.parent = sc;

    audio_attach_mi(&wss_hw_if, 0, &sc->sc_ad1848, &sc->sc_dev);
}

static int
wss_to_vol(cp, vol)
    mixer_ctrl_t *cp;
    struct ad1848_volume *vol;
{
    if (cp->un.value.num_channels == 1) {
	vol->left = vol->right = cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
	return(1);
    }
    else if (cp->un.value.num_channels == 2) {
	vol->left  = cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
	vol->right = cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
	return(1);
    }
    return(0);
}

static int
wss_from_vol(cp, vol)
    mixer_ctrl_t *cp;
    struct ad1848_volume *vol;
{
    if (cp->un.value.num_channels == 1) {
	cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] = vol->left;
	return(1);
    }
    else if (cp->un.value.num_channels == 2) {
	cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] = vol->left;
	cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] = vol->right;
	return(1);
    }
    return(0);
}

int
wss_getdev(addr, retp)
    void *addr;
    struct audio_device *retp;
{
    *retp = wss_device;
    return 0;
}

int
wss_set_out_port(addr, port)
    void *addr;
    int port;
{
    DPRINTF(("wss_set_out_port:\n"));
    return(EINVAL);
}

int
wss_get_out_port(addr)
    void *addr;
{
    DPRINTF(("wss_get_out_port:\n"));
    return(WSS_DAC_LVL);
}

int
wss_set_in_port(addr, port)
    void *addr;
    int port;
{
    struct ad1848_softc *ac = addr;
	
    DPRINTF(("wss_set_in_port: %d\n", port));

    switch(port) {
    case WSS_MIC_IN_LVL:
	port = MIC_IN_PORT;
	break;
    case WSS_LINE_IN_LVL:
	port = LINE_IN_PORT;
	break;
    case WSS_DAC_LVL:
	port = DAC_IN_PORT;
	break;
    default:
	return(EINVAL);
	/*NOTREACHED*/
    }
    
    return(ad1848_set_rec_port(ac, port));
}

int
wss_get_in_port(addr)
    void *addr;
{
    struct ad1848_softc *ac = addr;
    int port = WSS_MIC_IN_LVL;
    
    switch(ad1848_get_rec_port(ac)) {
    case MIC_IN_PORT:
	port = WSS_MIC_IN_LVL;
	break;
    case LINE_IN_PORT:
	port = WSS_LINE_IN_LVL;
	break;
    case DAC_IN_PORT:
	port = WSS_DAC_LVL;
	break;
    }

    DPRINTF(("wss_get_in_port: %d\n", port));

    return(port);
}

int
wss_mixer_set_port(addr, cp)
    void *addr;
    mixer_ctrl_t *cp;
{
    struct ad1848_softc *ac = addr;
    struct wss_softc *sc = ac->parent;
    struct ad1848_volume vol;
    int error = EINVAL;
    
    DPRINTF(("wss_mixer_set_port: dev=%d type=%d\n", cp->dev, cp->type));

    switch (cp->dev) {
    case WSS_MIC_IN_LVL:	/* Microphone */
	if (cp->type == AUDIO_MIXER_VALUE) {
	    if (wss_to_vol(cp, &vol))
		error = ad1848_set_aux2_gain(ac, &vol);
	}
	break;
	
    case WSS_MIC_IN_MUTE:	/* Microphone */
	if (cp->type == AUDIO_MIXER_ENUM) {
	    sc->mic_mute = cp->un.ord;
	    DPRINTF(("mic mute %d\n", cp->un.ord));
	    error = 0;
	}
	break;

    case WSS_LINE_IN_LVL:	/* linein/CD */
	if (cp->type == AUDIO_MIXER_VALUE) {
	    if (wss_to_vol(cp, &vol))
		error = ad1848_set_aux1_gain(ac, &vol);
	}
	break;
	
    case WSS_LINE_IN_MUTE:	/* linein/CD */
	if (cp->type == AUDIO_MIXER_ENUM) {
	    sc->cd_mute = cp->un.ord;
	    DPRINTF(("CD mute %d\n", cp->un.ord));
	    error = 0;
	}
	break;

    case WSS_DAC_LVL:		/* dac out */
	if (cp->type == AUDIO_MIXER_VALUE) {
	    if (wss_to_vol(cp, &vol))
		error = ad1848_set_out_gain(ac, &vol);
	}
	break;
	
    case WSS_DAC_MUTE:		/* dac out */
	if (cp->type == AUDIO_MIXER_ENUM) {
	    sc->dac_mute = cp->un.ord;
	    DPRINTF(("DAC mute %d\n", cp->un.ord));
	    error = 0;
	}
	break;

    case WSS_REC_LVL:		/* record level */
	if (cp->type == AUDIO_MIXER_VALUE) {
	    if (wss_to_vol(cp, &vol))
		error = ad1848_set_rec_gain(ac, &vol);
	}
	break;
	
    case WSS_RECORD_SOURCE:
	if (cp->type == AUDIO_MIXER_ENUM) {
	    error = ad1848_set_rec_port(ac, cp->un.ord);
	}
	break;

    case WSS_MON_LVL:
	if (cp->type == AUDIO_MIXER_VALUE && cp->un.value.num_channels == 1) {
	    vol.left  = cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
	    error = ad1848_set_mon_gain(ac, &vol);
	}
	break;

    default:
	    return ENXIO;
	    /*NOTREACHED*/
    }
    
    return 0;
}

int
wss_mixer_get_port(addr, cp)
    void *addr;
    mixer_ctrl_t *cp;
{
    struct ad1848_softc *ac = addr;
    struct wss_softc *sc = ac->parent;
    struct ad1848_volume vol;
    int error = EINVAL;
    
    DPRINTF(("wss_mixer_get_port: port=%d\n", cp->dev));

    switch (cp->dev) {
    case WSS_MIC_IN_LVL:	/* Microphone */
	if (cp->type == AUDIO_MIXER_VALUE) {
	    error = ad1848_get_aux2_gain(ac, &vol);
	    if (!error)
		wss_from_vol(cp, &vol);
	}
	break;

    case WSS_MIC_IN_MUTE:
	if (cp->type == AUDIO_MIXER_ENUM) {
	    cp->un.ord = sc->mic_mute;
	    error = 0;
	}
	break;

    case WSS_LINE_IN_LVL:	/* linein/CD */
	if (cp->type == AUDIO_MIXER_VALUE) {
	    error = ad1848_get_aux1_gain(ac, &vol);
	    if (!error)
		wss_from_vol(cp, &vol);
	}
	break;

    case WSS_LINE_IN_MUTE:
	if (cp->type == AUDIO_MIXER_ENUM) {
	    cp->un.ord = sc->cd_mute;
	    error = 0;
	}
	break;

    case WSS_DAC_LVL:		/* dac out */
	if (cp->type == AUDIO_MIXER_VALUE) {
	    error = ad1848_get_out_gain(ac, &vol);
	    if (!error)
		wss_from_vol(cp, &vol);
	}
	break;

    case WSS_DAC_MUTE:
	if (cp->type == AUDIO_MIXER_ENUM) {
	    cp->un.ord = sc->dac_mute;
	    error = 0;
	}
	break;

    case WSS_REC_LVL:		/* record level */
	if (cp->type == AUDIO_MIXER_VALUE) {
	    error = ad1848_get_rec_gain(ac, &vol);
	    if (!error)
		wss_from_vol(cp, &vol);
	}
	break;

    case WSS_RECORD_SOURCE:
	if (cp->type == AUDIO_MIXER_ENUM) {
	    cp->un.ord = ad1848_get_rec_port(ac);
	    error = 0;
	}
	break;

    case WSS_MON_LVL:		/* monitor level */
	if (cp->type == AUDIO_MIXER_VALUE && cp->un.value.num_channels == 1) {
	    error = ad1848_get_mon_gain(ac, &vol);
	    if (!error)
		cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] = vol.left;
	}
	break;

    default:
	error = ENXIO;
	break;
    }

    return(error);
}

int
wss_query_devinfo(addr, dip)
    void *addr;
    mixer_devinfo_t *dip;
{
    DPRINTF(("wss_query_devinfo: index=%d\n", dip->index));

    switch(dip->index) {
    case WSS_MIC_IN_LVL:	/* Microphone */
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = WSS_INPUT_CLASS;
	dip->prev = AUDIO_MIXER_LAST;
	dip->next = WSS_MIC_IN_MUTE;
	strcpy(dip->label.name, AudioNmicrophone);
	dip->un.v.num_channels = 2;
	strcpy(dip->un.v.units.name, AudioNvolume);
	break;

    case WSS_LINE_IN_LVL:	/* line/CD */
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = WSS_INPUT_CLASS;
	dip->prev = AUDIO_MIXER_LAST;
	dip->next = WSS_LINE_IN_MUTE;
	strcpy(dip->label.name, AudioNcd);
	dip->un.v.num_channels = 2;
	strcpy(dip->un.v.units.name, AudioNvolume);
	break;

    case WSS_DAC_LVL:		/*  dacout */
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = WSS_INPUT_CLASS;
	dip->prev = AUDIO_MIXER_LAST;
	dip->next = WSS_DAC_MUTE;
	strcpy(dip->label.name, AudioNdac);
	dip->un.v.num_channels = 2;
	strcpy(dip->un.v.units.name, AudioNvolume);
	break;

    case WSS_REC_LVL:	/* record level */
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = WSS_RECORD_CLASS;
	dip->prev = AUDIO_MIXER_LAST;
	dip->next = WSS_RECORD_SOURCE;
	strcpy(dip->label.name, AudioNrecord);
	dip->un.v.num_channels = 2;
	strcpy(dip->un.v.units.name, AudioNvolume);
	break;

    case WSS_MON_LVL:	/* monitor level */
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = WSS_MONITOR_CLASS;
	dip->next = dip->prev = AUDIO_MIXER_LAST;
	strcpy(dip->label.name, AudioNmonitor);
	dip->un.v.num_channels = 1;
	strcpy(dip->un.v.units.name, AudioNvolume);
	break;

    case WSS_INPUT_CLASS:			/* input class descriptor */
	dip->type = AUDIO_MIXER_CLASS;
	dip->mixer_class = WSS_INPUT_CLASS;
	dip->next = dip->prev = AUDIO_MIXER_LAST;
	strcpy(dip->label.name, AudioCinputs);
	break;

    case WSS_MONITOR_CLASS:			/* monitor class descriptor */
	dip->type = AUDIO_MIXER_CLASS;
	dip->mixer_class = WSS_MONITOR_CLASS;
	dip->next = dip->prev = AUDIO_MIXER_LAST;
	strcpy(dip->label.name, AudioCmonitor);
	break;
	    
    case WSS_RECORD_CLASS:			/* record source class */
	dip->type = AUDIO_MIXER_CLASS;
	dip->mixer_class = WSS_RECORD_CLASS;
	dip->next = dip->prev = AUDIO_MIXER_LAST;
	strcpy(dip->label.name, AudioCrecord);
	break;
	
    case WSS_MIC_IN_MUTE:
	dip->mixer_class = WSS_INPUT_CLASS;
	dip->type = AUDIO_MIXER_ENUM;
	dip->prev = WSS_MIC_IN_LVL;
	dip->next = AUDIO_MIXER_LAST;
	goto mute;
	
    case WSS_LINE_IN_MUTE:
	dip->mixer_class = WSS_INPUT_CLASS;
	dip->type = AUDIO_MIXER_ENUM;
	dip->prev = WSS_LINE_IN_LVL;
	dip->next = AUDIO_MIXER_LAST;
	goto mute;
	
    case WSS_DAC_MUTE:
	dip->mixer_class = WSS_INPUT_CLASS;
	dip->type = AUDIO_MIXER_ENUM;
	dip->prev = WSS_DAC_LVL;
	dip->next = AUDIO_MIXER_LAST;
    mute:
	strcpy(dip->label.name, AudioNmute);
	dip->un.e.num_mem = 2;
	strcpy(dip->un.e.member[0].label.name, AudioNoff);
	dip->un.e.member[0].ord = 0;
	strcpy(dip->un.e.member[1].label.name, AudioNon);
	dip->un.e.member[1].ord = 1;
	break;

    case WSS_RECORD_SOURCE:
	dip->mixer_class = WSS_RECORD_CLASS;
	dip->type = AUDIO_MIXER_ENUM;
	dip->prev = WSS_REC_LVL;
	dip->next = AUDIO_MIXER_LAST;
	strcpy(dip->label.name, AudioNsource);
	dip->un.e.num_mem = 3;
	strcpy(dip->un.e.member[0].label.name, AudioNmicrophone);
	dip->un.e.member[0].ord = WSS_MIC_IN_LVL;
	strcpy(dip->un.e.member[1].label.name, AudioNcd);
	dip->un.e.member[1].ord = WSS_LINE_IN_LVL;
	strcpy(dip->un.e.member[2].label.name, AudioNdac);
	dip->un.e.member[2].ord = WSS_DAC_LVL;
	break;

    default:
	return ENXIO;
	/*NOTREACHED*/
    }
    DPRINTF(("AUDIO_MIXER_DEVINFO: name=%s\n", dip->label.name));

    return 0;
}

/*
 * Initialization code for OPTi MAD16 compatible audio chips. Including
 *
 *      OPTi 82C928     MAD16           (replaced by C929)
 *      OAK OTI-601D    Mozart
 *      OPTi 82C929     MAD16 Pro
 *
 */
static unsigned int mad_read __P((struct wss_softc *, int, int));
static void mad_write __P((struct wss_softc *, int, int, int));
static int detect_mad16 __P((struct wss_softc *, int));

static unsigned int
mad_read(sc, chip_type, port)
    struct wss_softc *sc;
    int chip_type;
    int port;
{
    unsigned int tmp;
    int s = splaudio();		/* don't want an interrupt between outb&inb */
    
    switch (chip_type) {	/* Output password */
    case MAD_82C928:
    case MAD_OTI601D:
	bus_space_write_1(sc->sc_iot, sc->sc_mad_ioh, MC_PASSWD_REG, M_PASSWD_928);
	break;
    case MAD_82C929:
	bus_space_write_1(sc->sc_iot, sc->sc_mad_ioh, MC_PASSWD_REG, M_PASSWD_929);
	break;
    }
    tmp = bus_space_read_1(sc->sc_iot, sc->sc_mad_ioh, port);
    splx(s);
    return tmp;
}

static void
mad_write(sc, chip_type, port, value)
    struct wss_softc *sc;
    int chip_type;
    int port;
    int value;
{
    int s = splaudio();		/* don't want an interrupt between outb&outb */

    switch (chip_type) {	/* Output password */
    case MAD_82C928:
    case MAD_OTI601D:
	bus_space_write_1(sc->sc_iot, sc->sc_mad_ioh, MC_PASSWD_REG, M_PASSWD_928);
	break;
    case MAD_82C929:
	bus_space_write_1(sc->sc_iot, sc->sc_mad_ioh, MC_PASSWD_REG, M_PASSWD_929);
	break;
    }
    bus_space_write_1(sc->sc_iot, sc->sc_mad_ioh, port, value & 0xff);
    splx(s);
}

static int
detect_mad16(sc, chip_type)
    struct wss_softc *sc;
    int chip_type;
{
    unsigned char tmp, tmp2;

    /*
     * Check that reading a register doesn't return bus float (0xff)
     * when the card is accessed using password. This may fail in case
     * the card is in low power mode. Normally at least the power saving mode
     * bit should be 0.
     */
    if ((tmp = mad_read(sc, chip_type, MC1_PORT)) == 0xff) {
	DPRINTF(("MC1_PORT returned 0xff\n"));
	return 0;
    }

    /*
     * Now check that the gate is closed on first I/O after writing
     * the password. (This is how a MAD16 compatible card works).
     */
    if ((tmp2 = bus_space_read_1(sc->sc_iot, sc->sc_mad_ioh, MC1_PORT)) == tmp)	{ /* It didn't close */
	DPRINTF(("MC1_PORT didn't close after read (0x%02x)\n", tmp2));
	return 0;
    }

    mad_write(sc, chip_type, MC1_PORT, tmp ^ 0x80);	/* Toggle a bit */

    /* Compare the bit */
    if ((tmp2 = mad_read(sc, chip_type, MC1_PORT)) != (tmp ^ 0x80)) {
	mad_write(sc, chip_type, MC1_PORT, tmp);	/* Restore */
	DPRINTF(("Bit revert test failed (0x%02x, 0x%02x)\n", tmp, tmp2));
	return 0;
    }

    mad_write(sc, chip_type, MC1_PORT, tmp);	/* Restore */
    return 1;
}

static int
madprobe(sc, iobase)
    struct wss_softc *sc;
    int iobase;
{
    static int valid_ports[M_WSS_NPORTS] = 
        { M_WSS_PORT0, M_WSS_PORT1, M_WSS_PORT2, M_WSS_PORT3 };
    int i;
    int chip_type;

    if (bus_space_map(sc->sc_iot, MAD_BASE, MAD_NPORT, 0, &sc->sc_mad_ioh))
	return MAD_NONE;

    /* Allocate bus space that the MAD chip wants */
    if (bus_space_map(sc->sc_iot, MAD_REG1, MAD_LEN1, 0, &sc->sc_mad_ioh1))
        goto bad1;
    if (bus_space_map(sc->sc_iot, MAD_REG2, MAD_LEN2, 0, &sc->sc_mad_ioh2))
        goto bad2;
    if (bus_space_map(sc->sc_iot, MAD_REG3, MAD_LEN3, 0, &sc->sc_mad_ioh3))
        goto bad3;

    DPRINTF(("mad: Detect using password = 0xE2\n"));
    if (!detect_mad16(sc, MAD_82C928)) {
	/* No luck. Try different model */
	DPRINTF(("mad: Detect using password = 0xE3\n"));
	if (!detect_mad16(sc, MAD_82C929))
	    goto bad;
	chip_type = MAD_82C929;
	DPRINTF(("mad: 82C929 detected\n"));
    } else {
	if ((mad_read(sc, MAD_82C928, MC3_PORT) & 0x03) == 0x03) {
	    DPRINTF(("mad: Mozart detected\n"));
	    chip_type = MAD_OTI601D;
	} else {
	    DPRINTF(("mad: 82C928 detected?\n"));
	    chip_type = MAD_82C928;
	}
    }

#ifdef AUDIO_DEBUG
    if (wssdebug)
	for (i = MC1_PORT; i <= MC7_PORT; i++)
	    printf("mad: port %03x = %02x\n", i, mad_read(sc, chip_type, i));
#endif

    /* Set the WSS address. */
    for (i = 0; i < M_WSS_NPORTS; i++)
	if (valid_ports[i] == iobase)
	    break;
    if (i >= M_WSS_NPORTS) {		/* Not a valid port */
	printf("mad: Bad WSS base address 0x%x\n", iobase);
	goto bad;
    }
    /* enable WSS emulation at the I/O port, no joystick */
    mad_write(sc, chip_type, MC1_PORT, M_WSS_PORT_SELECT(i) | MC1_JOYDISABLE);

    mad_write(sc, chip_type, MC2_PORT, 0x03); /* ? */
    mad_write(sc, chip_type, MC3_PORT, 0xf0); /* Disable SB */

    return chip_type;
bad:
    bus_space_unmap(sc->sc_iot, sc->sc_mad_ioh3, MAD_LEN3);
bad3:
    bus_space_unmap(sc->sc_iot, sc->sc_mad_ioh2, MAD_LEN2);
bad2:
    bus_space_unmap(sc->sc_iot, sc->sc_mad_ioh1, MAD_LEN1);
bad1:
    bus_space_unmap(sc->sc_iot, sc->sc_mad_ioh, MAD_NPORT);
    return MAD_NONE;
}

static void
madunmap(sc)
    struct wss_softc *sc;
{
    if (sc->mad_chip_type == MAD_NONE)
        return;
    bus_space_unmap(sc->sc_iot, sc->sc_mad_ioh, MAD_NPORT);
    bus_space_unmap(sc->sc_iot, sc->sc_mad_ioh1, MAD_LEN1);
    bus_space_unmap(sc->sc_iot, sc->sc_mad_ioh2, MAD_LEN2);
    bus_space_unmap(sc->sc_iot, sc->sc_mad_ioh3, MAD_LEN3);
}

static void
madattach(sc)
    struct wss_softc *sc;
{
    int chip_type = sc->mad_chip_type;
    unsigned char cs4231_mode;

    if (chip_type == MAD_NONE)
        return;

    cs4231_mode = 
	strncmp(sc->sc_ad1848.chip_name, "CS4248", 6) == 0 ||
	strncmp(sc->sc_ad1848.chip_name, "CS4231", 6) == 0 ? 0x02 : 0;

    if (chip_type == MAD_82C929) {
	mad_write(sc, chip_type, MC4_PORT, 0xa2);
	mad_write(sc, chip_type, MC5_PORT, 0xA5 | cs4231_mode);
	mad_write(sc, chip_type, MC6_PORT, 0x03);	/* Disable MPU401 */
    } else {
	mad_write(sc, chip_type, MC4_PORT, 0x02);
	mad_write(sc, chip_type, MC5_PORT, 0x30 | cs4231_mode);
    }

#ifdef AUDIO_DEBUG
    if (wssdebug) {
	int i;
	for (i = MC1_PORT; i <= MC7_PORT; i++)
	    DPRINTF(("port %03x after init = %02x\n", i, mad_read(sc, chip_type, i)));
    }
#endif
}
