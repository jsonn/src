/*	$NetBSD: mfp.c,v 1.12.2.1 2005/04/29 11:28:28 kent Exp $	*/

/*-
 * Copyright (c) 1998 NetBSD Foundation, Inc.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * MC68901 MFP (multi function periferal) driver for NetBSD/x68k
 */

/*
 * MFP is used as keyboard controller, which may be used before
 * ordinary initialization.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mfp.c,v 1.12.2.1 2005/04/29 11:28:28 kent Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/cpu.h>

#include <arch/x68k/dev/intiovar.h>
#include <arch/x68k/dev/mfp.h>

static int mfp_match(struct device *, struct cfdata *, void *);
static void mfp_attach(struct device *, struct device *, void *);
static void mfp_init(void);
static void mfp_calibrate_delay(void);

CFATTACH_DECL(mfp, sizeof(struct mfp_softc),
    mfp_match, mfp_attach, NULL, NULL);

static int mfp_attached;

static int 
mfp_match(struct device *parent, struct cfdata *cf, void *aux)
{
	struct intio_attach_args *ia = aux;

	/* mfp0 */
	if (strcmp (ia->ia_name, "mfp") != 0)
		return 0;
	if (mfp_attached)
		return (0);

	if (ia->ia_addr == INTIOCF_ADDR_DEFAULT)
		ia->ia_addr = MFP_ADDR;
	if (ia->ia_intr == INTIOCF_INTR_DEFAULT)
		ia->ia_addr = MFP_INTR;

	/* fixed address */
	if (ia->ia_addr != MFP_ADDR)
		return (0);
	if (ia->ia_intr != MFP_INTR)
		return (0);

	return (1);
}


static void 
mfp_attach(struct device *parent, struct device *self, void *aux)
{
	struct mfp_softc *sc = (struct mfp_softc *)self;
	struct intio_attach_args *ia = aux;

	mfp_init ();

	if (sc != NULL) {
		/* realconfig */
		int r;

		printf ("\n");

		mfp_attached = 1;
		sc->sc_bst = ia->ia_bst;
		sc->sc_intr = ia->ia_intr;
		ia->ia_size = 0x30;
		r = intio_map_allocate_region (parent, ia, INTIO_MAP_ALLOCATE);
#ifdef DIAGNOSTIC
		if (r)
			panic ("IO map for MFP corruption??");
#endif
		bus_space_map(ia->ia_bst, ia->ia_addr, 0x2000, 0, &sc->sc_bht);
		config_found (self, "kbd", NULL);
		config_found (self, "clock", NULL);
		config_found (self, "pow", NULL);
	} else {
		/*
		 * Called from config_console;
		 * calibrate the DELAY loop counter
		 */
		mfp_calibrate_delay();
	}
}

static void
mfp_init(void)
{
#if 0				/* done in x68k_init.c::intr_reset() */
	mfp_set_vr(MFP_INTR);

	/* stop all interrupts */
	mfp_set_iera(0);
	mfp_set_ierb(0);
#endif

	/* Timer A settings */
	mfp_set_tacr(MFP_TIMERA_RESET | MFP_TIMERA_STOP);

	/* Timer B settings: used for USART clock */
	mfp_set_tbcr(MFP_TIMERB_RESET | MFP_TIMERB_STOP);

	/* Timer C/D settings */
	mfp_set_tcdcr(0);
}

extern int delay_divisor;
void	_delay(u_int);

static void
mfp_calibrate_delay(void)
{
	/*
	 * Stolen from mvme68k.
	 */
	/*
	 * X68k provides 4MHz clock (= 0.25usec) for MFP timer C.
	 * 10000usec = 0.25usec * 200 * 200
	 * Our slowest clock is 20MHz (?).  Its delay_divisor value
	 * should be about 102.  Start from 140 here.
	 */
	for (delay_divisor = 140; delay_divisor > 0; delay_divisor--) {
		mfp_set_tcdr(255-0);
		mfp_set_tcdcr(0x70); /* 1/200 delay mode */
		_delay(10000 << 8);
		mfp_set_tcdcr(0); /* stop timer */
		if ((255 - mfp_get_tcdr()) > 200)
			break;	/* got it! */
		/* retry! */
	}
}

/*
 * MFP utility functions
 */

/*
 * wait for built-in display hsync.
 * should be called before writing to frame buffer.
 * might be called before realconfig.
 */
void
mfp_wait_for_hsync(void)
{
	/* wait for CRT HSYNC */
	while (mfp_get_gpip() & MFP_GPIP_HSYNC)
		asm("nop");
	while (!(mfp_get_gpip() & MFP_GPIP_HSYNC))
		asm("nop");
}

/*
 * send COMMAND to the MFP USART.
 * USART is attached to the keyboard.
 * might be called before realconfig.
 */
int 
mfp_send_usart(int command)
{
	while (!(mfp_get_tsr() & MFP_TSR_BE));
	mfp_set_udr(command);

	return 0;
}

int
mfp_receive_usart(void)
{
	while (!(mfp_get_rsr() & MFP_RSR_BF))
		asm("nop");
	return mfp_get_udr();
}
