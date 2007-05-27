/*	$NetBSD: gxpcic.c,v 1.3.6.1 2007/05/27 12:27:17 ad Exp $ */
/*
 * Copyright (C) 2005, 2006 WIDE Project and SOUM Corporation.
 * All rights reserved.
 *
 * Written by Takashi Kiyohara and Susumu Miki for WIDE Project and SOUM
 * Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the name of SOUM Corporation
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT and SOUM CORPORATION ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT AND SOUM CORPORATION
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 2002, 2003, 2005  Genetec corp.  All rights reserved.
 *
 * PCMCIA/CF support for TWINTAIL (G4255EB)
 * Written by Hiroyuki Bessho for Genetec corp.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Genetec corp. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GENETEC CORP. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORP.
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <uvm/uvm.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciachip.h>

#include <arch/arm/xscale/pxa2x0var.h>
#include <arch/arm/xscale/pxa2x0reg.h>
#include <arch/arm/xscale/pxa2x0_pcic.h>
#include <arch/evbarm/gumstix/gumstixvar.h>


#ifdef DEBUG
#define DPRINTF(arg)	printf arg
#else
#define DPRINTF(arg)
#endif

#define HAVE_CARD(r)	(!((r) & GPIO_SET))

#define GXIO_GPIRQ11_CD1	11
#define GXIO_GPIRQ26_PRDY1	26
#define GXIO_GPIRQ27_PRDY2	27
#define GXIO_GPIRQ36_CD2	36


static	int  	gxpcic_match(struct device *, struct cfdata *, void *);
static	void  	gxpcic_attach(struct device *, struct device *, void *);
static	void	gxpcic_pcic_socket_setup(struct pxapcic_socket *);

static	u_int	gxpcic_read(struct pxapcic_socket *, int);
static	void	gxpcic_write(struct pxapcic_socket *, int, u_int);
static	void	gxpcic_set_power(struct pxapcic_socket *, int);
static	void	gxpcic_clear_intr(struct pxapcic_socket *);
static	void	*gxpcic_intr_establish(struct pxapcic_socket *, int,
				       int (*)(void *), void *);
static	void	gxpcic_intr_disestablish(struct pxapcic_socket *, void *);
__inline void gxpcic_cpld_clk(void);
__inline u_char gxpcic_cpld_read_bits(int bits);
static	int	gxpcic_count_slot(struct pxapcic_softc *);

CFATTACH_DECL(pxapcic_gxpcic, sizeof(struct pxapcic_softc),
    gxpcic_match, gxpcic_attach, NULL, NULL);

static struct pxapcic_tag gxpcic_pcic_functions = {
	gxpcic_read,
	gxpcic_write,
	gxpcic_set_power,
	gxpcic_clear_intr,
	gxpcic_intr_establish,
	gxpcic_intr_disestablish,
};

static struct {
	int cd;
	int prdy;
} gxpcic_slot_irqs[] = {
	{ GXIO_GPIRQ11_CD1, GXIO_GPIRQ26_PRDY1 },
	{ GXIO_GPIRQ36_CD2, GXIO_GPIRQ27_PRDY2 }
};


static int
gxpcic_match(struct device *parent, struct cfdata *cf, void *aux)
{
	struct {
		int gpio;
		u_int fn;
	} pcic_gpiomodes[] = {
		{ 48, GPIO_ALT_FN_2_OUT },		/* nPOE */
		{ 49, GPIO_ALT_FN_2_OUT },		/* nPWE */
		{ 50, GPIO_ALT_FN_2_OUT },		/* nPIOR */
		{ 51, GPIO_ALT_FN_2_OUT },		/* nPIOW */
		{ 52, GPIO_ALT_FN_2_OUT },		/* nPCE1 */
		{ 53, GPIO_ALT_FN_2_OUT },		/* nPCE2 */
		{ 54, GPIO_ALT_FN_2_OUT },		/* pSKTSEL */
		{ 55, GPIO_ALT_FN_2_OUT },		/* nPREG */
		{ 56, GPIO_ALT_FN_1_IN },		/* nPWAIT */
		{ 57, GPIO_ALT_FN_1_IN },		/* nIOIS16 */
		{ -1 }
	};
	u_int reg;
	int i;

	/*
	 * Check GPIO configuration.  If you use these, it is sure already
	 * to have been set by gxio. 
	 */
	for (i = 0; pcic_gpiomodes[i].gpio != -1; i++) {
		reg = pxa2x0_gpio_get_function(pcic_gpiomodes[i].gpio);
		if (GPIO_FN(reg) != GPIO_FN(pcic_gpiomodes[i].fn) ||
		    GPIO_FN_IS_OUT(reg) != GPIO_FN_IS_OUT(pcic_gpiomodes[i].fn))
			break;
	}
	if (pcic_gpiomodes[i].gpio != -1)
		return 0;

	return	1;	/* match */
}

static void
gxpcic_attach(struct device *parent, struct device *self, void *aux)
{
	struct pxapcic_softc *sc = (struct pxapcic_softc *)self;
	struct pxaip_attach_args *pxa = (struct pxaip_attach_args *)aux;
	int nslot, i;

	sc->sc_iot = pxa->pxa_iot;

	nslot = gxpcic_count_slot(sc);

	for (i = 0; i < nslot; i++) {
		sc->sc_irqpin[i] = gxpcic_slot_irqs[i].prdy;
		sc->sc_irqcfpin[i] = gxpcic_slot_irqs[i].cd;
	}
	sc->sc_nslots = nslot;

	pxapcic_attach_common(sc, &gxpcic_pcic_socket_setup);
}

static void
gxpcic_pcic_socket_setup(struct pxapcic_socket *so)
{
	struct pxapcic_softc *sc = so->sc;

	/* 3.3V only? */
	so->power_capability = PXAPCIC_POWER_3V;
	so->pcictag_cookie = NULL;
	so->pcictag = &gxpcic_pcic_functions;

	bus_space_write_4(sc->sc_iot, sc->sc_memctl_ioh,
	    MEMCTL_MCMEM(so->socket), MC_TIMING_VAL(9 ,9, 29));
	bus_space_write_4(sc->sc_iot, sc->sc_memctl_ioh,
	    MEMCTL_MCATT(so->socket), MC_TIMING_VAL(9 ,9, 29));
	bus_space_write_4(sc->sc_iot, sc->sc_memctl_ioh,
	    MEMCTL_MCIO(so->socket), MC_TIMING_VAL(5 ,5, 16));
}

static u_int
gxpcic_read(struct pxapcic_socket *so, int which)
{
	int reg;

	switch (which) {
	case PXAPCIC_CARD_STATUS:
		reg = pxa2x0_gpio_get_function(gxpcic_slot_irqs[so->socket].cd);
		return (HAVE_CARD(reg) ?
		    PXAPCIC_CARD_VALID : PXAPCIC_CARD_INVALID);

	case PXAPCIC_CARD_READY:
		reg = pxa2x0_gpio_get_function(
		    gxpcic_slot_irqs[so->socket].prdy);
		return (reg & GPIO_SET ? 1 : 0);

	default:
		panic("%s: bogus register", __FUNCTION__);
	}
	/* NOTREACHED */
}

/* ARGSUSED */
static void
gxpcic_write(struct pxapcic_socket *so, int which, u_int arg)
{

	switch (which) {
	case PXAPCIC_CARD_POWER:
	case PXAPCIC_CARD_RESET:
		/* We can't */
		break;

	default:
		panic("%s: bogus register", __FUNCTION__);
	}
	/* NOTREACHED */
}

static void
gxpcic_set_power(struct pxapcic_socket *__so, int arg)
{

	if(arg != PXAPCIC_POWER_OFF && arg != PXAPCIC_POWER_3V)
		panic("%s: bogus arg\n", __FUNCTION__);

	/* 3.3V only? */
}

/* ARGSUSED */
static void
gxpcic_clear_intr(struct pxapcic_socket *so)
{

	/* nothing to do */
}

static void *
gxpcic_intr_establish(struct pxapcic_socket *so, int level,
    int (* ih_fun)(void *), void *ih_arg)
{

	return pxa2x0_gpio_intr_establish(so->irqpin, IST_EDGE_FALLING,
	    level, ih_fun, ih_arg);
}

/* ARGSUSED */
static void
gxpcic_intr_disestablish(struct pxapcic_socket *so, void *ih)
{

	pxa2x0_gpio_intr_disestablish(ih);
}


/*
 * XXXXX: slot count functions from Linux
 */
__inline void
gxpcic_cpld_clk()
{

	pxa2x0_gpio_set_function(48, GPIO_OUT | GPIO_CLR);
	pxa2x0_gpio_set_function(48, GPIO_OUT | GPIO_SET);
}

__inline u_char
gxpcic_cpld_read_bits(int bits)
{
	u_int shift = 0, gpio;
	u_char result = 0;

	while (bits--) {
		gpio = pxa2x0_gpio_get_function(11);
		result |= ((gpio & GPIO_SET) == GPIO_SET) << shift;
		shift++;
		gxpcic_cpld_clk();
	}
	return result;
}

/*
 * We use the CPLD on the CF-CF card to read a value from a shift register.
 * If we can read that magic sequence, then we have 2 CF cards; otherwise
 * we assume just one.  The CPLD will send the value of the shift register
 * on GPIO11 (the CD line for slot 0) when RESET is held in reset.  We use
 * GPIO48 (nPWE) as a clock signal, GPIO52/53 (card enable for both cards)
 * to control read/write to the shift register.
 */
static int
gxpcic_count_slot(struct pxapcic_softc *sc)
{
	u_int poe, pce1, pce2;
	int nslot;

	poe = pxa2x0_gpio_get_function(48);
	pce1 = pxa2x0_gpio_get_function(52);
	pce2 = pxa2x0_gpio_get_function(53);

	/* Reset */
	pxa2x0_gpio_set_function(8, GPIO_OUT | GPIO_SET);

	/* Setup the shift register */
	pxa2x0_gpio_set_function(52, GPIO_OUT | GPIO_SET);
	pxa2x0_gpio_set_function(53, GPIO_OUT | GPIO_CLR);

	/* Tick the clock to program the shift register */
	gxpcic_cpld_clk();

	/* Now set shift register into read mode */
	pxa2x0_gpio_set_function(52, GPIO_OUT | GPIO_CLR);
	pxa2x0_gpio_set_function(53, GPIO_OUT | GPIO_SET);

	/* We can read the bits now -- 0xc2 means "Dual compact flash" */
	if (gxpcic_cpld_read_bits(8) != 0xc2)
		/* We do not have 2 CF slots */
		nslot = 1;
	else
		/* We have 2 CF slots */
		nslot = 2;

	delay(50);
	pxa2x0_gpio_set_function(8, GPIO_OUT | GPIO_CLR);	/* clr RESET */

	pxa2x0_gpio_set_function(48, poe);
	pxa2x0_gpio_set_function(52, pce1);
	pxa2x0_gpio_set_function(53, pce2);

	return nslot;
}
