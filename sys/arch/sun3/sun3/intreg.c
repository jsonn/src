/*	$NetBSD: intreg.c,v 1.13.8.2 2001/01/18 09:23:06 bouyer Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass and Gordon W. Ross.
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
 * This handles multiple attach of autovectored interrupts,
 * and the handy software interrupt request register.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/vmmeter.h>

#include <uvm/uvm_extern.h>

#include <m68k/asm_single.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/mon.h>

#include <sun3/sun3/interreg.h>
#include <sun3/sun3/machdep.h>

struct intreg_softc {
	struct device sc_dev;
	volatile u_char *sc_reg;
};

static int  intreg_match __P((struct device *, struct cfdata *, void *));
static void intreg_attach __P((struct device *, struct device *, void *));
static int soft1intr __P((void *));

struct cfattach intreg_ca = {
	sizeof(struct intreg_softc), intreg_match, intreg_attach
};

volatile u_char *interrupt_reg;


/* called early (by internal_configure) */
void
intreg_init()
{
	interrupt_reg = obio_find_mapping(IREG_ADDR, 1);
	if (!interrupt_reg) {
		mon_printf("intreg_init\n");
		sunmon_abort();
	}
	/* Turn off all interrupts until clock_attach */
	*interrupt_reg = 0;
}


static int
intreg_match(parent, cf, args)
    struct device *parent;
	struct cfdata *cf;
    void *args;
{
	struct confargs *ca = args;

	/* This driver only supports one unit. */
	if (cf->cf_unit != 0)
		return (0);

	/* Validate the given address. */
	if (ca->ca_paddr != IREG_ADDR)
		return (0);

	return (1);
}


static void
intreg_attach(parent, self, args)
	struct device *parent;
	struct device *self;
	void *args;
{
	struct intreg_softc *sc = (void *)self;

	printf("\n");

	sc->sc_reg = interrupt_reg;

	/* Install handler for our "soft" interrupt. */
	isr_add_autovect(soft1intr, (void *)sc, 1);
}


/*
 * Level 1 software interrupt.
 * Possible reasons:
 *	Network software interrupt
 *	Soft clock interrupt
 */
static int
soft1intr(arg)
	void *arg;
{
	union sun3sir sir;
	int s;

	s = splhigh();
	sir.sir_any = sun3sir.sir_any;
	sun3sir.sir_any = 0;
	isr_soft_clear(1);
	splx(s);

	if (sir.sir_any) {
		uvmexp.softs++;
		if (sir.sir_which[SIR_NET]) {
			sir.sir_which[SIR_NET] = 0;
			netintr();
		}
		if (sir.sir_which[SIR_CLOCK]) {
			sir.sir_which[SIR_CLOCK] = 0;
			softclock(NULL);
		}
		if (sir.sir_which[SIR_SPARE2]) {
			sir.sir_which[SIR_SPARE2] = 0;
			/* spare2intr(); */
		}
		if (sir.sir_which[SIR_SPARE3]) {
			sir.sir_which[SIR_SPARE3] = 0;
			/* spare3intr(); */
		}
		return (1);
	}
	return(0);
}


void isr_soft_request(level)
	int level;
{
	register u_char bit;

	if ((level < 1) || (level > 3))
		return;

	bit = 1 << level;
	single_inst_bset_b(*interrupt_reg, bit);
}

void isr_soft_clear(level)
	int level;
{
	register u_char bit;

	if ((level < 1) || (level > 3))
		return;

	bit = 1 << level;
	single_inst_bclr_b(*interrupt_reg, bit);
}

