/*	$NetBSD: if_lmc_media.c,v 1.10.4.1 2001/11/12 21:18:14 thorpej Exp $	*/

/*-
 * Copyright (c) 1997-1999 LAN Media Corporation (LMC)
 * All rights reserved.  www.lanmedia.com
 *
 * This code is written by Michael Graff <graff@vix.com> for LMC.
 * The code is derived from permitted modifications to software created
 * by Matt Thomas (matt@3am-software.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. All marketing or advertising materials mentioning features or
 *    use of this software must display the following acknowledgement:
 *      This product includes software developed by LAN Media Corporation
 *      and its contributors.
 * 4. Neither the name of LAN Media Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY LAN MEDIA CORPORATION AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>	/* only for declaration of wakeup() used by vm.h */
#if defined(__FreeBSD__)
#include <machine/clock.h>
#elif defined(__bsdi__) || defined(__NetBSD__)
#include <sys/device.h>
#endif

#if defined(__NetBSD__)
#include <dev/pci/pcidevs.h>
#include "rnd.h"
#if NRND > 0
#include <sys/rnd.h>
#endif
#endif

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/netisr.h>

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#if defined(__FreeBSD__) || defined(__NetBSD__)
#include <net/if_sppp.h>
#endif

#if defined(__bsdi__)
#if INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#endif

#include <net/netisr.h>
#include <net/if.h>
#include <net/netisr.h>
#include <net/if_types.h>
#include <net/if_p2p.h>
#include <net/if_c_hdlc.h>
#endif

#if defined(__FreeBSD__)
#include <vm/vm.h>
#include <vm/pmap.h>
#include <pci.h>
#if NPCI > 0
#include <pci/pcivar.h>
#include <pci/dc21040reg.h>
#endif
#endif /* __FreeBSD__ */

#if defined(__bsdi__)
#include <vm/vm.h>
#include <i386/pci/ic/dc21040.h>
#include <i386/isa/isa.h>
#include <i386/isa/icu.h>
#include <i386/isa/dma.h>
#include <i386/isa/isavar.h>
#include <i386/pci/pci.h>

#endif /* __bsdi__ */

#if defined(__NetBSD__)
#include <machine/bus.h>
#if defined(__alpha__)
#include <machine/intr.h>
#endif
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/ic/dc21040reg.h>
#endif /* __NetBSD__ */

/*
 * Sigh.  Every OS puts these in different places.
 */
#if defined(__NetBSD__)  
#include <dev/pci/if_lmc_types.h>
#include <dev/pci/if_lmcioctl.h>
#include <dev/pci/if_lmcvar.h>
#elif defined(__FreeBSD__)
#include "pci/if_lmc_types.h"
#include "pci/if_lmcioctl.h"
#include "pci/if_lmcvar.h"
#else /* BSDI */
#include "i386/pci/if_lmctypes.h"
#include "i386/pci/if_lmcioctl.h"
#include "i386/pci/if_lmcvar.h"
#endif

/*
 * For lack of a better place, put the T1 cable stuff here.
 */
char *lmc_t1_cables[] = {
	"V.10/RS423", "EIA530A", "reserved", "X.21", "V.35",
	"EIA449/EIA530/V.36", "V.28/EIA232", "none", NULL
};

/*
 * protocol independent method.
 */
static void	lmc_set_protocol(lmc_softc_t * const, lmc_ctl_t *);

/*
 * media independent methods to check on media status, link, light LEDs,
 * etc.
 */
static void	lmc_ds3_init(lmc_softc_t * const);
static void	lmc_ds3_default(lmc_softc_t * const);
static void	lmc_ds3_set_status(lmc_softc_t * const, lmc_ctl_t *);
static void	lmc_ds3_set_100ft(lmc_softc_t * const, int);
static int	lmc_ds3_get_link_status(lmc_softc_t * const);
static void	lmc_ds3_set_crc_length(lmc_softc_t * const, int);
static void	lmc_ds3_set_scram(lmc_softc_t * const, int);
static void	lmc_ds3_watchdog(lmc_softc_t * const);

static void	lmc_hssi_init(lmc_softc_t * const);
static void	lmc_hssi_default(lmc_softc_t * const);
static void	lmc_hssi_set_status(lmc_softc_t * const, lmc_ctl_t *);
static void	lmc_hssi_set_clock(lmc_softc_t * const, int);
static int	lmc_hssi_get_link_status(lmc_softc_t * const);
static void	lmc_hssi_set_link_status(lmc_softc_t * const, int);
static void	lmc_hssi_set_crc_length(lmc_softc_t * const, int);
static void	lmc_hssi_watchdog(lmc_softc_t * const);

static void	lmc_ssi_init(lmc_softc_t * const);
static void	lmc_ssi_default(lmc_softc_t * const);
static void	lmc_ssi_set_status(lmc_softc_t * const, lmc_ctl_t *);
static void	lmc_ssi_set_clock(lmc_softc_t * const, int);
static void	lmc_ssi_set_speed(lmc_softc_t * const, lmc_ctl_t *);
static int	lmc_ssi_get_link_status(lmc_softc_t * const);
static void	lmc_ssi_set_link_status(lmc_softc_t * const, int);
static void	lmc_ssi_set_crc_length(lmc_softc_t * const, int);
static void	lmc_ssi_watchdog(lmc_softc_t * const);

static void	lmc_t1_init(lmc_softc_t * const);
static void	lmc_t1_default(lmc_softc_t * const);
static void	lmc_t1_set_status(lmc_softc_t * const, lmc_ctl_t *);
static int	lmc_t1_get_link_status(lmc_softc_t * const);
static void	lmc_t1_set_circuit_type(lmc_softc_t * const, int);
static void	lmc_t1_set_crc_length(lmc_softc_t * const, int);
static void	lmc_t1_set_clock(lmc_softc_t * const, int);
static void	lmc_t1_watchdog(lmc_softc_t * const);

static void	lmc_dummy_set_1(lmc_softc_t * const, int);
static void	lmc_dummy_set2_1(lmc_softc_t * const, lmc_ctl_t *);

static inline void write_av9110_bit(lmc_softc_t *, int);
static void	write_av9110(lmc_softc_t *, u_int32_t, u_int32_t, u_int32_t,
			     u_int32_t, u_int32_t);

lmc_media_t lmc_ds3_media = {
	lmc_ds3_init,			/* special media init stuff */
	lmc_ds3_default,		/* reset to default state */
	lmc_ds3_set_status,		/* reset status to state provided */
	lmc_dummy_set_1,		/* set clock source */
	lmc_dummy_set2_1,		/* set line speed */
	lmc_ds3_set_100ft,		/* set cable length */
	lmc_ds3_set_scram,		/* set scrambler */
	lmc_ds3_get_link_status,	/* get link status */
	lmc_dummy_set_1,		/* set link status */
	lmc_ds3_set_crc_length,		/* set CRC length */
	lmc_dummy_set_1,		/* set T1 or E1 circuit type */
	lmc_ds3_watchdog
};

lmc_media_t lmc_hssi_media = {
	lmc_hssi_init,			/* special media init stuff */
	lmc_hssi_default,		/* reset to default state */
	lmc_hssi_set_status,		/* reset status to state provided */
	lmc_hssi_set_clock,		/* set clock source */
	lmc_dummy_set2_1,		/* set line speed */
	lmc_dummy_set_1,		/* set cable length */
	lmc_dummy_set_1,		/* set scrambler */
	lmc_hssi_get_link_status,	/* get link status */
	lmc_hssi_set_link_status,	/* set link status */
	lmc_hssi_set_crc_length,	/* set CRC length */
	lmc_dummy_set_1,		/* set T1 or E1 circuit type */
	lmc_hssi_watchdog
};

lmc_media_t lmc_ssi_media = {
	lmc_ssi_init,			/* special media init stuff */
	lmc_ssi_default,		/* reset to default state */
	lmc_ssi_set_status,		/* reset status to state provided */
	lmc_ssi_set_clock,		/* set clock source */
	lmc_ssi_set_speed,		/* set line speed */
	lmc_dummy_set_1,		/* set cable length */
	lmc_dummy_set_1,		/* set scrambler */
	lmc_ssi_get_link_status,	/* get link status */
	lmc_ssi_set_link_status,	/* set link status */
	lmc_ssi_set_crc_length,		/* set CRC length */
	lmc_dummy_set_1,		/* set T1 or E1 circuit type */
	lmc_ssi_watchdog
};

lmc_media_t lmc_t1_media = {
	lmc_t1_init,			/* special media init stuff */
	lmc_t1_default,			/* reset to default state */
	lmc_t1_set_status,		/* reset status to state provided */
	lmc_t1_set_clock,		/* set clock source */
	lmc_dummy_set2_1,		/* set line speed */
	lmc_dummy_set_1,		/* set cable length */
	lmc_dummy_set_1,		/* set scrambler */
	lmc_t1_get_link_status,		/* get link status */
	lmc_dummy_set_1,		/* set link status */
	lmc_t1_set_crc_length,		/* set CRC length */
	lmc_t1_set_circuit_type,	/* set T1 or E1 circuit type */
	lmc_t1_watchdog
};

static void
lmc_dummy_set_1(lmc_softc_t * const sc, int a)
{
}

static void
lmc_dummy_set2_1(lmc_softc_t * const sc, lmc_ctl_t *a)
{
}

/*
 *  HSSI methods
 */

static void
lmc_hssi_init(lmc_softc_t * const sc)
{
	sc->ictl.cardtype = LMC_CTL_CARDTYPE_LMC5200;

	lmc_gpio_mkoutput(sc, LMC_GEP_HSSI_CLOCK);
}

static void
lmc_hssi_default(lmc_softc_t * const sc)
{
	sc->lmc_miireg16 = LMC_MII16_LED_ALL;

	sc->lmc_media->set_link_status(sc, LMC_LINK_DOWN);
	sc->lmc_media->set_clock_source(sc, LMC_CTL_CLOCK_SOURCE_EXT);
	sc->lmc_media->set_crc_length(sc, LMC_CTL_CRC_LENGTH_16);
}

/*
 * Given a user provided state, set ourselves up to match it.  This will
 * always reset the card if needed.
 */
static void
lmc_hssi_set_status(lmc_softc_t * const sc, lmc_ctl_t *ctl)
{
	if (ctl == NULL) {
		sc->lmc_media->set_clock_source(sc, sc->ictl.clock_source);
		lmc_set_protocol(sc, NULL);

		return;
	}

	/*
	 * check for change in clock source
	 */
	if (ctl->clock_source && !sc->ictl.clock_source) {
		sc->lmc_media->set_clock_source(sc, LMC_CTL_CLOCK_SOURCE_INT);
		sc->lmc_timing = LMC_CTL_CLOCK_SOURCE_INT;
	} else if (!ctl->clock_source && sc->ictl.clock_source) {
		sc->lmc_timing = LMC_CTL_CLOCK_SOURCE_EXT;
		sc->lmc_media->set_clock_source(sc, LMC_CTL_CLOCK_SOURCE_EXT);
	}

	lmc_set_protocol(sc, ctl);
}

/*
 * 1 == internal, 0 == external
 */
static void
lmc_hssi_set_clock(lmc_softc_t * const sc, int ie)
{
	if (ie == LMC_CTL_CLOCK_SOURCE_EXT) {
		sc->lmc_gpio |= LMC_GEP_HSSI_CLOCK;
		LMC_CSR_WRITE(sc, csr_gp, sc->lmc_gpio);
		sc->ictl.clock_source = LMC_CTL_CLOCK_SOURCE_EXT;
		printf(LMC_PRINTF_FMT ": clock external\n",
		       LMC_PRINTF_ARGS);
	} else {
		sc->lmc_gpio &= ~(LMC_GEP_HSSI_CLOCK);
		LMC_CSR_WRITE(sc, csr_gp, sc->lmc_gpio);
		sc->ictl.clock_source = LMC_CTL_CLOCK_SOURCE_INT;
		printf(LMC_PRINTF_FMT ": clock internal\n",
		       LMC_PRINTF_ARGS);
	}
}

/*
 * return hardware link status.
 * 0 == link is down, 1 == link is up.
 */
static int
lmc_hssi_get_link_status(lmc_softc_t * const sc)
{
	u_int16_t link_status;

	link_status = lmc_mii_readreg(sc, 0, 16);

	if ((link_status & LMC_MII16_HSSI_CA) == LMC_MII16_HSSI_CA)
		return 1;
	else
		return 0;
}

static void
lmc_hssi_set_link_status(lmc_softc_t * const sc, int state)
{
	if (state)
		sc->lmc_miireg16 |= LMC_MII16_HSSI_TA;
	else
		sc->lmc_miireg16 &= ~LMC_MII16_HSSI_TA;

	lmc_mii_writereg(sc, 0, 16, sc->lmc_miireg16);
}

/*
 * 0 == 16bit, 1 == 32bit
 */
static void
lmc_hssi_set_crc_length(lmc_softc_t * const sc, int state)
{
	if (state == LMC_CTL_CRC_LENGTH_32) {
		/* 32 bit */
		sc->lmc_miireg16 |= LMC_MII16_HSSI_CRC;
		sc->ictl.crc_length = LMC_CTL_CRC_LENGTH_32;
	} else {
		/* 16 bit */
		sc->lmc_miireg16 &= ~LMC_MII16_HSSI_CRC;
		sc->ictl.crc_length = LMC_CTL_CRC_LENGTH_16;
	}

	lmc_mii_writereg(sc, 0, 16, sc->lmc_miireg16);
}

static void
lmc_hssi_watchdog (lmc_softc_t * const sc)
{
	/* HSSI is blank */
}

static void
lmc_ds3_watchdog (lmc_softc_t * const sc)
{
	sc->lmc_miireg16 = lmc_mii_readreg (sc, 0, 16);
	if (sc->lmc_miireg16 & 0x0018)
	{
#if 1
		printf("%s: AIS Received\n", sc->lmc_xname);
#endif
		lmc_led_on (sc, LMC_DS3_LED1 | LMC_DS3_LED2);
	}
}


/*
 *  DS3 methods
 */

/*
 * Set cable length
 */
static void
lmc_ds3_set_100ft(lmc_softc_t * const sc, int ie)
{
	if (ie == LMC_CTL_CABLE_LENGTH_GT_100FT) {
		sc->lmc_miireg16 &= ~LMC_MII16_DS3_ZERO;
		sc->ictl.cable_length = LMC_CTL_CABLE_LENGTH_GT_100FT;
	} else if (ie == LMC_CTL_CABLE_LENGTH_LT_100FT) {
		sc->lmc_miireg16 |= LMC_MII16_DS3_ZERO;
		sc->ictl.cable_length = LMC_CTL_CABLE_LENGTH_LT_100FT;
	}
	lmc_mii_writereg(sc, 0, 16, sc->lmc_miireg16);
}

static void
lmc_ds3_default(lmc_softc_t * const sc)
{
	sc->lmc_miireg16 = LMC_MII16_LED_ALL;

	sc->lmc_media->set_link_status(sc, LMC_LINK_DOWN);
	sc->lmc_media->set_cable_length(sc, LMC_CTL_CABLE_LENGTH_LT_100FT);
	sc->lmc_media->set_scrambler(sc, LMC_CTL_OFF);
	sc->lmc_media->set_crc_length(sc, LMC_CTL_CRC_LENGTH_16);
}

/*
 * Given a user provided state, set ourselves up to match it.  This will
 * always reset the card if needed.
 */
static void
lmc_ds3_set_status(lmc_softc_t * const sc, lmc_ctl_t *ctl)
{
	if (ctl == NULL) {
		sc->lmc_media->set_cable_length(sc, sc->ictl.cable_length);
		sc->lmc_media->set_scrambler(sc, sc->ictl.scrambler_onoff);
		lmc_set_protocol(sc, NULL);

		return;
	}

	/*
	 * check for change in cable length setting
	 */
	if (ctl->cable_length && !sc->ictl.cable_length)
		lmc_ds3_set_100ft(sc, LMC_CTL_CABLE_LENGTH_GT_100FT);
	else if (!ctl->cable_length && sc->ictl.cable_length)
		lmc_ds3_set_100ft(sc, LMC_CTL_CABLE_LENGTH_LT_100FT);

	/*
	 * Check for change in scrambler setting (requires reset)
	 */
	if (ctl->scrambler_onoff && !sc->ictl.scrambler_onoff)
		lmc_ds3_set_scram(sc, LMC_CTL_ON);
	else if (!ctl->scrambler_onoff && sc->ictl.scrambler_onoff)
		lmc_ds3_set_scram(sc, LMC_CTL_OFF);

	lmc_set_protocol(sc, ctl);
}

static void
lmc_ds3_init(lmc_softc_t * const sc)
{
	int i;

	sc->ictl.cardtype = LMC_CTL_CARDTYPE_LMC5245;

	/* writes zeros everywhere */
	for (i = 0 ; i < 21 ; i++) {
		lmc_mii_writereg(sc, 0, 17, i);
		lmc_mii_writereg(sc, 0, 18, 0);
	}

	/* set some essential bits */
	lmc_mii_writereg(sc, 0, 17, 1);
	lmc_mii_writereg(sc, 0, 18, 0x05);	/* ser, xtx */

	lmc_mii_writereg(sc, 0, 17, 5);
	lmc_mii_writereg(sc, 0, 18, 0x80);	/* emode */

	lmc_mii_writereg(sc, 0, 17, 14);
	lmc_mii_writereg(sc, 0, 18, 0x30);	/* rcgen, tcgen */

	/* clear counters and latched bits */
	for (i = 0 ; i < 21 ; i++) {
		lmc_mii_writereg(sc, 0, 17, i);
		lmc_mii_readreg(sc, 0, 18);
	}
}

/*
 * 1 == DS3 payload scrambled, 0 == not scrambled
 */
static void
lmc_ds3_set_scram(lmc_softc_t * const sc, int ie)
{
	if (ie == LMC_CTL_ON) {
		sc->lmc_miireg16 |= LMC_MII16_DS3_SCRAM;
		sc->ictl.scrambler_onoff = LMC_CTL_ON;
	} else {
		sc->lmc_miireg16 &= ~LMC_MII16_DS3_SCRAM;
		sc->ictl.scrambler_onoff = LMC_CTL_OFF;
	}
	lmc_mii_writereg(sc, 0, 16, sc->lmc_miireg16);
}

/*
 * return hardware link status.
 * 0 == link is down, 1 == link is up.
 */
static int
lmc_ds3_get_link_status(lmc_softc_t * const sc)
{
	u_int16_t link_status;

	lmc_mii_writereg(sc, 0, 17, 7);
	link_status = lmc_mii_readreg(sc, 0, 18);
// printf("lmc_ds3_get_link_status: %x\n", link_status);
	if ((link_status & LMC_FRAMER_REG0_DLOS) == 0)
		return 1;
	else
		return 0;
}

/*
 * 0 == 16bit, 1 == 32bit
 */
static void
lmc_ds3_set_crc_length(lmc_softc_t * const sc, int state)
{
	if (state == LMC_CTL_CRC_LENGTH_32) {
		/* 32 bit */
		sc->lmc_miireg16 |= LMC_MII16_DS3_CRC;
		sc->ictl.crc_length = LMC_CTL_CRC_LENGTH_32;
	} else {
		/* 16 bit */
		sc->lmc_miireg16 &= ~LMC_MII16_DS3_CRC;
		sc->ictl.crc_length = LMC_CTL_CRC_LENGTH_16;
	}

	lmc_mii_writereg(sc, 0, 16, sc->lmc_miireg16);
}


/*
 *  SSI methods
 */

static void
lmc_ssi_init(lmc_softc_t * const sc)
{
	u_int16_t mii17;
	int cable;

	sc->ictl.cardtype = LMC_CTL_CARDTYPE_LMC1000;

	mii17 = lmc_mii_readreg(sc, 0, 17);

	cable = (mii17 & LMC_MII17_SSI_CABLE_MASK) >> LMC_MII17_SSI_CABLE_SHIFT;
	sc->ictl.cable_type = cable;

	lmc_gpio_mkoutput(sc, LMC_GEP_SSI_TXCLOCK);
}

static void
lmc_ssi_default(lmc_softc_t * const sc)
{
	sc->lmc_miireg16 = LMC_MII16_LED_ALL;

	/*
	 * make TXCLOCK always be an output
	 */
	lmc_gpio_mkoutput(sc, LMC_GEP_SSI_TXCLOCK);

	sc->lmc_media->set_link_status(sc, LMC_LINK_DOWN);
	sc->lmc_media->set_clock_source(sc, LMC_CTL_CLOCK_SOURCE_EXT);
	sc->lmc_media->set_speed(sc, NULL);
	sc->lmc_media->set_crc_length(sc, LMC_CTL_CRC_LENGTH_16);
}

/*
 * Given a user provided state, set ourselves up to match it.  This will
 * always reset the card if needed.
 */
static void
lmc_ssi_set_status(lmc_softc_t * const sc, lmc_ctl_t *ctl)
{
	if (ctl == NULL) {
		sc->lmc_media->set_clock_source(sc, sc->ictl.clock_source);
		sc->lmc_media->set_speed(sc, &sc->ictl);
		lmc_set_protocol(sc, NULL);

		return;
	}

	/*
	 * check for change in clock source
	 */
	if (ctl->clock_source == LMC_CTL_CLOCK_SOURCE_INT &&
	    sc->ictl.clock_source == LMC_CTL_CLOCK_SOURCE_EXT) {
		sc->lmc_media->set_clock_source(sc, LMC_CTL_CLOCK_SOURCE_INT);
		sc->lmc_timing = LMC_CTL_CLOCK_SOURCE_INT;
	} else if (ctl->clock_source == LMC_CTL_CLOCK_SOURCE_EXT &&
	    sc->ictl.clock_source == LMC_CTL_CLOCK_SOURCE_INT) {
		sc->lmc_media->set_clock_source(sc, LMC_CTL_CLOCK_SOURCE_EXT);
		sc->lmc_timing = LMC_CTL_CLOCK_SOURCE_EXT;
	}

	if (ctl->clock_rate != sc->ictl.clock_rate)
		sc->lmc_media->set_speed(sc, ctl);

	lmc_set_protocol(sc, ctl);
}

/*
 * 1 == internal, 0 == external
 */
static void
lmc_ssi_set_clock(lmc_softc_t * const sc, int ie)
{
	if (ie == LMC_CTL_CLOCK_SOURCE_EXT) {
		sc->lmc_gpio &= ~(LMC_GEP_SSI_TXCLOCK);
		LMC_CSR_WRITE(sc, csr_gp, sc->lmc_gpio);
		sc->ictl.clock_source = LMC_CTL_CLOCK_SOURCE_EXT;
		printf(LMC_PRINTF_FMT ": clock external\n",
		       LMC_PRINTF_ARGS);
	} else {
		sc->lmc_gpio |= LMC_GEP_SSI_TXCLOCK;
		LMC_CSR_WRITE(sc, csr_gp, sc->lmc_gpio);
		sc->ictl.clock_source = LMC_CTL_CLOCK_SOURCE_INT;
		printf(LMC_PRINTF_FMT ": clock internal\n",
		       LMC_PRINTF_ARGS);
	}
}

static void
lmc_ssi_set_speed(lmc_softc_t * const sc, lmc_ctl_t *ctl)
{
	lmc_ctl_t *ictl = &sc->ictl;
	lmc_av9110_t *av;

	/*
	 * original settings for clock rate of:
	 *  100 Khz (8,25,0,0,2) were incorrect
	 *  they should have been 80,125,1,3,3
	 *  There are 17 param combinations to produce this freq.
	 *  For 1.5 Mhz use 120,100,1,1,2 (226 param. combinations)
	 */
	if (ctl == NULL) {
		av = &ictl->cardspec.ssi;
		ictl->clock_rate = 1500000;
		av->f = ictl->clock_rate;
		av->n = 120;
		av->m = 100;
		av->v = 1;
		av->x = 1;
		av->r = 2;

		write_av9110(sc, av->n, av->m, av->v, av->x, av->r);
		return;
	}

	av = &ctl->cardspec.ssi;

	if (av->f == 0)
		return;

	ictl->clock_rate = av->f;  /* really, this is the rate we are */
	ictl->cardspec.ssi = *av;

	write_av9110(sc, av->n, av->m, av->v, av->x, av->r);
}

/*
 * return hardware link status.
 * 0 == link is down, 1 == link is up.
 */
static int
lmc_ssi_get_link_status(lmc_softc_t * const sc)
{
	u_int16_t link_status;

	/*
	 * missing CTS?  Hmm.  If we require CTS on, we may never get the
	 * link to come up, so omit it in this test.
	 *
	 * Also, it seems that with a loopback cable, DCD isn't asserted,
	 * so just check for things like this:
	 *	DSR _must_ be asserted.
	 *	One of DCD or CTS must be asserted.
	 */

#ifdef CONFIG_LMC_IGNORE_HARDWARE_HANDSHAKE
	link_status = LMC_CSR_READ(sc, csr_gp_timer);
	link_status = 0x0000ffff - (link_status & 0x0000ffff);

	return(link_status);
#else

	link_status = lmc_mii_readreg(sc, 0, 16);

	if ((link_status & LMC_MII16_SSI_DSR) == 0)
		return (0);

	if ((link_status & (LMC_MII16_SSI_CTS | LMC_MII16_SSI_DCD)) == 0)
		return (0);

	return (1);
#endif
}

static void
lmc_ssi_set_link_status(lmc_softc_t * const sc, int state)
{
	if (state) {
		sc->lmc_miireg16 |= (LMC_MII16_SSI_DTR | LMC_MII16_SSI_RTS);
		printf(LMC_PRINTF_FMT ": asserting DTR and RTS\n",
		       LMC_PRINTF_ARGS);
	} else {
		sc->lmc_miireg16 &= ~(LMC_MII16_SSI_DTR | LMC_MII16_SSI_RTS);
		printf(LMC_PRINTF_FMT ": deasserting DTR and RTS\n",
		       LMC_PRINTF_ARGS);
	}

	lmc_mii_writereg(sc, 0, 16, sc->lmc_miireg16);

}

/*
 * 0 == 16bit, 1 == 32bit
 */
static void
lmc_ssi_set_crc_length(lmc_softc_t * const sc, int state)
{
	if (state == LMC_CTL_CRC_LENGTH_32) {
		/* 32 bit */
		sc->lmc_miireg16 |= LMC_MII16_SSI_CRC;
		sc->ictl.crc_length = LMC_CTL_CRC_LENGTH_32;
		sc->lmc_crcSize = LMC_CTL_CRC_BYTESIZE_4;
	} else {
		/* 16 bit */
		sc->lmc_miireg16 &= ~LMC_MII16_SSI_CRC;
		sc->ictl.crc_length = LMC_CTL_CRC_LENGTH_16;
		sc->lmc_crcSize = LMC_CTL_CRC_BYTESIZE_2;
	}

	lmc_mii_writereg(sc, 0, 16, sc->lmc_miireg16);
}

/*
 * These are bits to program the ssi frequency generator
 */
static inline void
write_av9110_bit(lmc_softc_t *sc, int c)
{
	/*
	 * set the data bit as we need it.
	 */
	sc->lmc_gpio &= ~(LMC_GEP_SERIALCLK);
	if (c & 0x01)
		sc->lmc_gpio |= LMC_GEP_SERIAL;
	else
		sc->lmc_gpio &= ~(LMC_GEP_SERIAL);
	LMC_CSR_WRITE(sc, csr_gp, sc->lmc_gpio);

	/*
	 * set the clock to high
	 */
	sc->lmc_gpio |= LMC_GEP_SERIALCLK;
	LMC_CSR_WRITE(sc, csr_gp, sc->lmc_gpio);

	/*
	 * set the clock to low again.
	 */
	sc->lmc_gpio &= ~(LMC_GEP_SERIALCLK);
	LMC_CSR_WRITE(sc, csr_gp, sc->lmc_gpio);
}

static void
write_av9110(lmc_softc_t *sc, u_int32_t n, u_int32_t m, u_int32_t v,
	     u_int32_t x, u_int32_t r)
{
	int i;

#if 0
	printf(LMC_PRINTF_FMT ": speed %u, %d %d %d %d %d\n",
	       LMC_PRINTF_ARGS, sc->ictl.clock_rate,
	       n, m, v, x, r);
#endif

	sc->lmc_gpio |= LMC_GEP_SSI_GENERATOR;
	sc->lmc_gpio &= ~(LMC_GEP_SERIAL | LMC_GEP_SERIALCLK);
	LMC_CSR_WRITE(sc, csr_gp, sc->lmc_gpio);

	/*
	 * Set the TXCLOCK, GENERATOR, SERIAL, and SERIALCLK
	 * as outputs.
	 */
	lmc_gpio_mkoutput(sc, (LMC_GEP_SERIAL | LMC_GEP_SERIALCLK
			       | LMC_GEP_SSI_GENERATOR));

	sc->lmc_gpio &= ~(LMC_GEP_SSI_GENERATOR);
	LMC_CSR_WRITE(sc, csr_gp, sc->lmc_gpio);

	/*
	 * a shifting we will go...
	 */
	for (i = 0 ; i < 7 ; i++)
		write_av9110_bit(sc, n >> i);
	for (i = 0 ; i < 7 ; i++)
		write_av9110_bit(sc, m >> i);
	for (i = 0 ; i < 1 ; i++)
		write_av9110_bit(sc, v >> i);
	for (i = 0 ; i < 2 ; i++)
		write_av9110_bit(sc, x >> i);
	for (i = 0 ; i < 2 ; i++)
		write_av9110_bit(sc, r >> i);
	for (i = 0 ; i < 5 ; i++)
		write_av9110_bit(sc, 0x17 >> i);

	/*
	 * stop driving serial-related signals
	 */
	lmc_gpio_mkinput(sc,
			 (LMC_GEP_SERIAL | LMC_GEP_SERIALCLK
			  | LMC_GEP_SSI_GENERATOR));
}

static void
lmc_ssi_watchdog(lmc_softc_t * const sc)
{
	u_int16_t mii17;
	struct ssicsr2 {
		unsigned short dtr:1, dsr:1, rts:1, cable:3, crc:1, led0:1,
		led1:1, led2:1, led3:1, fifo:1, ll:1, rl:1, tm:1, loop:1;
	};
	struct ssicsr2 *ssicsr;
	mii17 = lmc_mii_readreg (sc, 0, 17);
	ssicsr = (struct ssicsr2 *) &mii17;
	if (ssicsr->cable == 7) {
		lmc_led_off (sc, LMC_MII16_LED2);
	}
	else {
		lmc_led_on (sc, LMC_MII16_LED2);
	}

}

/*
 *  T1 methods
 */

/*
 * The framer regs are multiplexed through MII regs 17 & 18
 *  write the register address to MII reg 17 and the
 *  data to MII reg 18.
 */
static void
lmc_t1_write(lmc_softc_t * const sc, int a, int d)
{
	lmc_mii_writereg(sc, 0, 17, a);
	lmc_mii_writereg(sc, 0, 18, d);
}

static int
lmc_t1_read(lmc_softc_t * const sc, int a)
{
	lmc_mii_writereg(sc, 0, 17, a);
	return lmc_mii_readreg(sc, 0, 18);
}

static void
lmc_t1_init(lmc_softc_t * const sc)
{
	u_int16_t mii16;
	int     i;

	sc->ictl.cardtype = LMC_CTL_CARDTYPE_LMC1200;
	mii16 = lmc_mii_readreg(sc, 0, 16);

	mii16 &= ~LMC_MII16_T1_XOE;
	lmc_mii_writereg (sc, 0, 16, mii16);
	sc->lmc_miireg16 = mii16;

	/* reset 8370 */
	mii16 &= ~LMC_MII16_T1_RST;
	lmc_mii_writereg(sc, 0, 16, mii16 | LMC_MII16_T1_RST);
	lmc_mii_writereg(sc, 0, 16, mii16);

	/* set T1 line impedance */
	mii16 |= LMC_MII16_T1_Z;
	lmc_mii_writereg(sc, 0, 16, mii16);


	/* CR0 - Set framing to ESF + Force CRC - Set T1 */
	lmc_t1_write(sc, 0x01, 0x1b);

	/* Reset Elastic store to center - 64 bit elastic store */
	lmc_t1_write(sc, 0x02, 0x4b);

	/* Release Elastic store reset */
	lmc_t1_write(sc, 0x02, 0x43);

	/* Disable all interrupts except BOP receive */
	lmc_t1_write(sc, 0x0C, 0x00);
	lmc_t1_write(sc, 0x0D, 0x00);
	lmc_t1_write(sc, 0x0E, 0x00);
	lmc_t1_write(sc, 0x0F, 0x00);
	lmc_t1_write(sc, 0x10, 0x00);
	lmc_t1_write(sc, 0x11, 0x00);
	lmc_t1_write(sc, 0x12, 0x80);
	lmc_t1_write(sc, 0x13, 0x00);

	lmc_t1_write(sc, 0x14, 0x00);  /* LOOP    - loopback config         */
	lmc_t1_write(sc, 0x15, 0x00);  /* DL3_TS  - xtrnl datalink timeslot */
	lmc_t1_write(sc, 0x18, 0xFF);  /* PIO     - programmable I/O        */
	lmc_t1_write(sc, 0x19, 0x30);  /* POE     - programmable OE         */
	lmc_t1_write(sc, 0x1A, 0x0F);  /* CMUX    - clock input mux         */

	lmc_t1_write(sc, 0x20, 0xC1);  /* LIU_CR  - RX LIU config           */
	lmc_t1_write(sc, 0x20, 0x41);  /* LIU_CR  - RX LIU config           */

	lmc_t1_write(sc, 0x22, 0xB1);  /* RLIU_CR - RX LIU config           */
	lmc_t1_write(sc, 0x24, 0x21);  /* VGA_MAX -20db sesitivity          */
	lmc_t1_write(sc, 0x2A, 0xA6);  /* Force off the pre-equalizer       */

	/* Equalizer Gain threshholds */
	lmc_t1_write(sc, 0x38, 0x24);  /* RX_TH0  - RX gain threshold 0      */
	lmc_t1_write(sc, 0x39, 0x28);  /* RX_TH1  - RX gain threshold 0      */
	lmc_t1_write(sc, 0x3A, 0x2C);  /* RX_TH2  - RX gain threshold 0      */
	lmc_t1_write(sc, 0x3B, 0x30);  /* RX_TH3  - RX gain threshold 0      */
	lmc_t1_write(sc, 0x3C, 0x34);  /* RX_TH4  - RX gain threshold 0      */

	/* Reset LIU */
	lmc_t1_write(sc, 0x20, 0x81);  /* LIU_CR  - RX LIU (reset RLIU)      */
	lmc_t1_write(sc, 0x20, 0x01);  /* LIU_CR  - RX LIU (clear reset)     */

	lmc_t1_write(sc, 0x40, 0x03);  /* RCR0    - RX config                */
	lmc_t1_write(sc, 0x41, 0x00);  /* Zero test pattern generator        */

	lmc_t1_write(sc, 0x42, 0x09);  /* DN_LEN is 8, UP_LEN is 5           */
	lmc_t1_write(sc, 0x43, 0x08);  /* Loopback activate                  */
	lmc_t1_write(sc, 0x44, 0x24);  /* Loopback deactivate                */

	lmc_t1_write(sc, 0x45, 0x00);  /* RALM    - RX alarm config          */
	lmc_t1_write(sc, 0x46, 0x08);  /* LATCH   - RX alarm/err/cntr latch  */

	lmc_t1_write(sc, 0x68, 0x4E);  /* TLIU_CR - TX LIU config            */
	lmc_t1_write(sc, 0x70, 0x0D);  /* TCR0    - TX framer config         */
	lmc_t1_write(sc, 0x71, 0x05);  /* TCR1    - TX config                */
	lmc_t1_write(sc, 0x72, 0x0B);  /* TFRM    - TX frame format          */
	lmc_t1_write(sc, 0x73, 0x00);  /* TERROR  - TX error insert          */
	lmc_t1_write(sc, 0x74, 0x00);  /* TMAN    - TX manual Sa/FEBE config */
	lmc_t1_write(sc, 0x75, 0x00);  /* TALM    - TX alarm signal config   */
	lmc_t1_write(sc, 0x76, 0x00);  /* TPATT   - TX test pattern config   */
	lmc_t1_write(sc, 0x77, 0x00);  /* TLB     - TX inband loopback confg */
	lmc_t1_write(sc, 0x90, 0x06);  /* CLAD_CR - clock rate adapter confg */
	lmc_t1_write(sc, 0x91, 0x05);  /* CSEL    - clad freq sel            */
	lmc_t1_write(sc, 0x92, 0x00);  /* CLAD Phase Det                     */
	lmc_t1_write(sc, 0x93, 0x00);  /* No CLAD Test                       */

	/* Activate BOP */
	lmc_t1_write(sc, 0xA0, 0xea);  /* BOP  - Bit oriented protocol xcvr  */

	lmc_t1_write(sc, 0xA4, 0x40);  /* DL1_TS  - DL1 time slot enable     */
	lmc_t1_write(sc, 0xA5, 0x00);  /* DL1_BIT - DL1 bit enable           */
	lmc_t1_write(sc, 0xA6, 0x03);  /* DL1_CTL - DL1 control              */
	lmc_t1_write(sc, 0xA7, 0x00);  /* RDL1_FFC - DL1 FIFO Size           */
	lmc_t1_write(sc, 0xAB, 0x00);  /* TDL1_FFC - DL1 Empty Control       */

	lmc_t1_write(sc, 0xAA, 0x00);  /* PRM - no perf report messages      */
	lmc_t1_write(sc, 0xB1, 0x00);  /* DL2_CTL - DL2 control              */
	lmc_t1_write(sc, 0xD0, 0x47);  /* SBI_CR  - sys bus iface config     */
	lmc_t1_write(sc, 0xD1, 0x70);  /* RSB_CR  - RX sys bus config        */
	lmc_t1_write(sc, 0xD4, 0x30);  /* TSB_CR  - TX sys bus config        */

	for (i = 0; i < 32; i++) {
		lmc_t1_write(sc, 0x0E0+i, 0x00); /*SBCn sysbus perchannel ctl */
		lmc_t1_write(sc, 0x100+i, 0x00); /* TPCn - TX per-channel ctl */
		lmc_t1_write(sc, 0x180+i, 0x00); /* RPCn - RX per-channel ctl */
	}
	for (i = 1; i < 25; i++) {
		/* SBCn - sys bus per-channel ctl */
		lmc_t1_write(sc, 0x0E0+i, 0x0D);
	}

	/* PFM */
	lmc_t1_write(sc, 0xA4, 0x40);  /* DL1_TS  -  DL1 time slot enable     */
	lmc_t1_write(sc, 0xA5, 0x00);  /* DL1_BIT -  DL1 bit enable           */
	lmc_t1_write(sc, 0xA6, 0x03);  /* DL1_CTL -  DL1 control              */
	lmc_t1_write(sc, 0xA7, 0x00);  /* RDL1_FFC - DL1 FIFO Size            */
	lmc_t1_write(sc, 0xAB, 0x00);  /* TDL1_FFC - DL1 Empty Control        */


	mii16 |= LMC_MII16_T1_XOE;
	lmc_mii_writereg (sc, 0, 16, mii16);
	sc->lmc_miireg16 = mii16; 
}

static void
lmc_t1_default(lmc_softc_t * const sc)
{
	sc->lmc_miireg16 = LMC_MII16_LED_ALL;
	sc->lmc_media->set_link_status(sc, LMC_LINK_DOWN);
	sc->lmc_media->set_circuit_type(sc, LMC_CTL_CIRCUIT_TYPE_T1);
	sc->lmc_media->set_crc_length(sc, LMC_CTL_CRC_LENGTH_16);
}

/*
 * Given a user provided state, set ourselves up to match it.  This will
 * always reset the card if needed.
 */

static void
lmc_t1_set_status(lmc_softc_t * const sc, lmc_ctl_t *ctl){
	if (ctl == NULL) {
		sc->lmc_media->set_circuit_type(sc, sc->ictl.circuit_type);
		lmc_set_protocol(sc, NULL);

		return;
	}

	/*
	 * check for change in circuit type
	 */

	if (ctl->circuit_type == LMC_CTL_CIRCUIT_TYPE_T1
		&& sc->ictl.circuit_type == LMC_CTL_CIRCUIT_TYPE_E1)
		sc->lmc_media->set_circuit_type(sc, LMC_CTL_CIRCUIT_TYPE_E1);
	else if (ctl->circuit_type == LMC_CTL_CIRCUIT_TYPE_E1
		&& sc->ictl.circuit_type == LMC_CTL_CIRCUIT_TYPE_T1)
		sc->lmc_media->set_circuit_type(sc, LMC_CTL_CIRCUIT_TYPE_T1);
	lmc_set_protocol(sc, ctl);
}

/*
 * return hardware link status.
 * 0 == link is down, 1 == link is up.
 */

static int
lmc_t1_get_link_status(lmc_softc_t * const sc)
{
	u_int16_t link_status;
	lmc_mii_writereg(sc, 0, 17, T1FRAMER_ALARM1_STATUS);
	link_status = lmc_mii_readreg(sc, 0, 18);

	/*
	 * LMC 1200 LED definitions
	 * led0 yellow = far-end adapter is in Red alarm condition
	 * led1 blue = received an Alarm Indication signal (upstream failure)
	 * led2 Green = power to adapter, Gate Array loaded & driver attached
	 * led3 red = Loss of Signal (LOS) or out of frame (OOF) conditions
	 * detected on T3 receive signal
	 */

	/* detect a change in Blue alarm indication signal */

	if ((sc->t1_alarm1_status & T1F_RAIS) != (link_status & T1F_RAIS)) {
		if (link_status & T1F_RAIS) {
			/* turn on blue LED */
			/* DEBUG */
			printf(" link status: RAIS turn ON Blue %x\n",
			    link_status);
			lmc_led_on(sc, LMC_DS3_LED1);
		} else {
			/* turn off blue LED */
			/* DEBUG */
			printf(" link status: RAIS turn OFF Blue %x\n",
			    link_status);
			lmc_led_off(sc, LMC_DS3_LED1);
		}       
	}
	/*
	 * T1F_RYEL wiggles quite a bit,
	 *  taking it out until I understand why -baz 6/22/99
	 */
	/* Yellow alarm indication */
	if ((sc->t1_alarm1_status & T1F_RMYEL) !=
	    (link_status & T1F_RMYEL)) {
		if ((link_status & (T1F_RYEL | T1F_RMYEL)) == 0) {
			/* turn off yellow LED */
			/* DEBUG */
			printf(" link status: RYEL turn OFF Yellow %x\n",
			    link_status);
			lmc_led_off(sc, LMC_DS3_LED0);

		} else {
			/* turn on yellow LED */
			/* DEBUG */
			printf(" link status: RYEL turn ON Yellow %x\n",
			    link_status);
			lmc_led_on(sc, LMC_DS3_LED0);
		}
	}

	sc->t1_alarm1_status = link_status;

	lmc_mii_writereg(sc, 0, 17, T1FRAMER_ALARM2_STATUS);
	sc->t1_alarm2_status = lmc_mii_readreg(sc, 0, 18);

	/*
	 * link status based upon T1 receive loss of frame or
	 * loss of signal - RED alarm indication
	 */
	if ((link_status & (T1F_RLOF | T1F_RLOS)) == 0)
		return 1;
	else
		return 0;
}

/*
 * 1 == T1 Circuit Type , 0 == E1 Circuit Type
 */
static void
lmc_t1_set_circuit_type(lmc_softc_t * const sc, int ie)
{
	if (ie == LMC_CTL_CIRCUIT_TYPE_T1) {
		sc->lmc_miireg16 |= LMC_MII16_T1_Z;
		sc->ictl.circuit_type = LMC_CTL_CIRCUIT_TYPE_T1;
	} else {
		sc->lmc_miireg16 &= ~LMC_MII16_T1_Z;
		sc->ictl.scrambler_onoff = LMC_CTL_CIRCUIT_TYPE_E1;
	}
	lmc_mii_writereg(sc, 0, 16, sc->lmc_miireg16);
}

/*
 * 0 == 16bit, 1 == 32bit
 */
static void
lmc_t1_set_crc_length(lmc_softc_t * const sc, int state)
{
	if (state == LMC_CTL_CRC_LENGTH_32) {
		/* 32 bit */
		sc->lmc_miireg16 |= LMC_MII16_T1_CRC;
		sc->ictl.crc_length = LMC_CTL_CRC_LENGTH_32;
		sc->lmc_crcSize = LMC_CTL_CRC_BYTESIZE_4;

	} else {
		/* 16 bit */
		sc->lmc_miireg16 &= ~LMC_MII16_T1_CRC;
		sc->ictl.crc_length = LMC_CTL_CRC_LENGTH_16;
		sc->lmc_crcSize = LMC_CTL_CRC_BYTESIZE_2;
	}

	lmc_mii_writereg(sc, 0, 16, sc->lmc_miireg16);
}

/*
 * 1 == internal, 0 == external
 */
static void
lmc_t1_set_clock(lmc_softc_t * const sc, int ie)
{
	if (ie == LMC_CTL_CLOCK_SOURCE_EXT) {
		sc->lmc_gpio &= ~(LMC_GEP_SSI_TXCLOCK);
		LMC_CSR_WRITE (sc, csr_gp, sc->lmc_gpio);
		sc->ictl.clock_source = LMC_CTL_CLOCK_SOURCE_EXT;
		printf (LMC_PRINTF_FMT ": clock external\n", LMC_PRINTF_ARGS);
	}
	else {
		sc->lmc_gpio |= LMC_GEP_SSI_TXCLOCK;
		LMC_CSR_WRITE (sc, csr_gp, sc->lmc_gpio);
		sc->ictl.clock_source = LMC_CTL_CLOCK_SOURCE_INT;
		printf (LMC_PRINTF_FMT ": clock internal\n", LMC_PRINTF_ARGS);
	}
}

static void
lmc_t1_watchdog(lmc_softc_t * const sc)
{
	int t1stat;

	/* read alarm 1 status (receive) */
	t1stat = lmc_t1_read (sc, 0x47);
	/* blue alarm -- RAIS */
	if (t1stat & 0x08) {
#if 0
		if (sc->lmc_blue != 1)
			printf ("%s: AIS Received\n", sc->lmc_xname);
#endif
		lmc_led_on (sc, LMC_DS3_LED1 | LMC_DS3_LED2);
		sc->lmc_blue = 1;
	} else {
#if 0
		if (sc->lmc_blue == 1)
			printf ("%s: AIS ok\n", sc->lmc_xname);
#endif
		lmc_led_off (sc, LMC_DS3_LED1);
		lmc_led_on (sc, LMC_DS3_LED2);
		sc->lmc_blue = 0;
	}

	/* Red alarm -- LOS | LOF */
	if (t1stat & 0x04) {
		/* Only print the error once */
		if (sc->lmc_red != 1)
			printf ("%s: Red Alarm\n", sc->lmc_xname);
		lmc_led_on (sc, LMC_DS3_LED2 | LMC_DS3_LED3);
		sc->lmc_red = 1;
	} else { 
		if (sc->lmc_red == 1)
			printf ("%s: Red Alarm ok\n", sc->lmc_xname);
	lmc_led_off (sc, LMC_DS3_LED3);
	lmc_led_on (sc, LMC_DS3_LED2);
	sc->lmc_red = 0;
	}

	/* check for Receive Multiframe Yellow Alarm
	 * Ignore Receive Yellow Alarm
	 */
	if (t1stat & 0x80) {
		if (sc->lmc_yel != 1) {
			printf ("%s: Receive Yellow Alarm\n", sc->lmc_xname);
		}
			lmc_led_on (sc, LMC_DS3_LED0 | LMC_DS3_LED2);
			sc->lmc_yel = 1;
	}
	else {
		if (sc->lmc_yel == 1)
		printf ("%s: Yellow Alarm ok\n", sc->lmc_xname);
		lmc_led_off (sc, LMC_DS3_LED0);
		lmc_led_on (sc, LMC_DS3_LED2);
		sc->lmc_yel = 0;
	}
}


static void
lmc_set_protocol(lmc_softc_t * const sc, lmc_ctl_t *ctl)
{
	if (ctl == 0) {
		sc->ictl.keepalive_onoff = LMC_CTL_ON;

		return;
	}

#if defined(__NetBSD__) || defined(__FreeBSD__)
	if (ctl->keepalive_onoff != sc->ictl.keepalive_onoff) {
		switch (ctl->keepalive_onoff) {
		case LMC_CTL_ON:
			printf(LMC_PRINTF_FMT ": enabling keepalive\n",
			       LMC_PRINTF_ARGS);
			sc->ictl.keepalive_onoff = LMC_CTL_ON;
			sc->lmc_sppp.pp_flags = PP_CISCO | PP_KEEPALIVE;
			break;
		case LMC_CTL_OFF:
			printf(LMC_PRINTF_FMT ": disabling keepalive\n",
			       LMC_PRINTF_ARGS);
			sc->ictl.keepalive_onoff = LMC_CTL_OFF;
			sc->lmc_sppp.pp_flags = PP_CISCO;
		}
	}

#if NBPFILTER > 0
	/* just in case we are going to change encap type */
	if ((sc->lmc_sppp.pp_flags & PP_CISCO) != 0)
		bpf_change_type(&sc->lmc_if, DLT_HDLC, PPP_HEADER_LEN);
	else
		bpf_change_type(&sc->lmc_if, DLT_PPP, PPP_HEADER_LEN);
#endif /* NBPFILTER > 0 */
#endif
}
