/*	$NetBSD: i80321_gpio.c,v 1.1.4.2 2004/08/03 10:32:58 skrll Exp $	*/

/*
 * Copyright (c) 2001, 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Intel i80321 I/O Processor general purpose I/O support.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <arm/xscale/i80321reg.h>
#include <arm/xscale/i80321var.h>

/*
 * i80321_gpio_set_direction:
 *
 *	Set the direction of the indicated GPIO pins (1 == output).
 */
void
i80321_gpio_set_direction(uint8_t which, uint8_t val)
{
	struct i80321_softc *sc = i80321_softc;

	sc->sc_gpio_dir = (sc->sc_gpio_dir & ~which) | val;
	bus_space_write_1(sc->sc_st, sc->sc_sh, ICU_GPOE, ~sc->sc_gpio_dir);
}

/*
 * i80321_gpio_set_val:
 *
 *	Set the value of the indicated GPIO pins.
 */
void
i80321_gpio_set_val(uint8_t which, uint8_t val)
{
	struct i80321_softc *sc = i80321_softc; 

	sc->sc_gpio_val = (sc->sc_gpio_val & ~which) | val;
	bus_space_write_1(sc->sc_st, sc->sc_sh, ICU_GPOD, sc->sc_gpio_val);
}

/*
 * i80321_gpio_get_val:
 *
 *	Get the current state of the GPIO pins.
 */
uint8_t
i80321_gpio_get_val(void)
{
	struct i80321_softc *sc = i80321_softc;

	return (bus_space_read_1(sc->sc_st, sc->sc_sh, ICU_GPID));
}
