/*	$NetBSD: com_supio.c,v 1.5.2.1 1997/11/12 01:35:15 mellon Exp $	*/

/*-
 * Copyright (c) 1993, 1994, 1995, 1996
 *	Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1991 The Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *	@(#)com.c	7.5 (Berkeley) 5/16/91
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/device.h>

#include <machine/intr.h>
#include <machine/bus.h>

/*#include <dev/isa/isavar.h>*/
#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>

#include <amiga/dev/supio.h>

struct comsupio_softc {
	struct com_softc sc_com;
	struct isr sc_isr;
};

int com_supio_match __P((struct device *, struct cfdata *, void *));
void com_supio_attach __P((struct device *, struct device *, void *));
void com_supio_cleanup __P((void *));

#if 0
static int      comconsaddr;
static bus_space_handle_t comconsioh; 
static int      comconsattached;
static bus_space_tag_t comconstag;
static int comconsrate; 
static tcflag_t comconscflag;
#endif

struct cfattach com_supio_ca = {
	sizeof(struct comsupio_softc), com_supio_match, com_supio_attach
};

int
com_supio_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	bus_space_tag_t iot;
	int iobase;
	int rv = 1;
	struct supio_attach_args *supa = aux;

	iot = supa->supio_iot;
	iobase = supa->supio_iobase;

	if (strcmp(supa->supio_name,"com"))
		return 0;
#if 0
	/* if it's in use as console, it's there. */
	if (iobase != comconsaddr || comconsattached) {
		if (bus_space_map(iot, iobase, COM_NPORTS, 0, &ioh)) {
			return 0;
		}
		rv = comprobe1(iot, ioh, iobase);
		bus_space_unmap(iot, ioh, COM_NPORTS);
	}
#endif
	return (rv);
}

void
com_supio_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct comsupio_softc *sc = (void *)self;
	struct com_softc *csc = &sc->sc_com;
	int iobase;
	bus_space_tag_t iot;
	struct supio_attach_args *supa = aux;
	u_int16_t needpsl;

	/*
	 * We're living on a superio chip.
	 */
	iobase = csc->sc_iobase = supa->supio_iobase;
	iot = csc->sc_iot = supa->supio_iot;
	printf(" port 0x%x ipl %d", iobase, supa->supio_ipl);

	if (bus_space_map(iot, iobase, COM_NPORTS, 0, &csc->sc_ioh))
		panic("comattach: io mapping failed");

	csc->sc_frequency = supa->supio_arg;

	com_attach_subr(csc);

	/* XXX this should be really in the interupt stuff */
	needpsl = PSL_S | (supa->supio_ipl << 8);

	if (amiga_ttyspl < needpsl) {
		printf("%s: raising amiga_ttyspl from 0x%x to 0x%x\n",
		    csc->sc_dev.dv_xname, amiga_ttyspl, needpsl);
		amiga_ttyspl = needpsl;
	}
	sc->sc_isr.isr_intr = comintr;
	sc->sc_isr.isr_arg = csc;
	sc->sc_isr.isr_ipl = supa->supio_ipl;
	add_isr(&sc->sc_isr);

	/*
	 * Shutdown hook for buggy BIOSs that don't recognize the UART
	 * without a disabled FIFO.
	 */
	if (shutdownhook_establish(com_supio_cleanup, csc) == NULL)
		panic("comsupio: could not establish shutdown hook");
}

void
com_supio_cleanup(arg)
	void *arg;
{
	struct com_softc *sc = arg;

	if (ISSET(sc->sc_hwflags, COM_HW_FIFO))
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, com_fifo, 0);
}
