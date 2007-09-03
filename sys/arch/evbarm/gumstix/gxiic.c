/*	$NetBSD: gxiic.c,v 1.1.4.2 2007/09/03 10:18:27 skrll Exp $ */
/*
 * Copyright (c) 2007 KIYOHARA Takashi
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: gxiic.c,v 1.1.4.2 2007/09/03 10:18:27 skrll Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/lock.h>

#include <arm/xscale/pxa2x0_i2c.h>

#include <evbarm/gumstix/gumstixvar.h>

#include <dev/i2c/i2cvar.h>


struct gxiic_softc {
	struct pxa2x0_i2c_softc sc_pxa_i2c;

	struct i2c_controller sc_i2c;
	struct lock sc_lock;
};


static int gxiicmatch(struct device *, struct cfdata *, void *);
static void gxiicattach(struct device *, struct device *, void *);

/* fuctions for i2c_controller */
static int gxiic_acquire_bus(void *, int);
static void gxiic_release_bus(void *, int);
static int gxiic_exec(void *cookie, i2c_op_t, i2c_addr_t, const void *, size_t,
    void *, size_t, int);


CFATTACH_DECL(gxiic, sizeof(struct gxiic_softc),
    gxiicmatch, gxiicattach, NULL, NULL);


/* ARGSUSED */
static int
gxiicmatch(struct device *parent, struct cfdata *match, void *aux)
{

	return 1;
}

/* ARGSUSED */
static void
gxiicattach(struct device *parent, struct device *self, void *aux)
{
	struct gxiic_softc *sc = device_private(self);
	struct gxio_attach_args *gxa = aux;
	struct i2cbus_attach_args iba;

	aprint_normal("\n");
	aprint_naive("\n");

	sc->sc_pxa_i2c.sc_iot = gxa->gxa_iot;
	sc->sc_pxa_i2c.sc_size = PXA2X0_I2C_SIZE;
	if (pxa2x0_i2c_attach_sub(&sc->sc_pxa_i2c)) {
		aprint_error(": unable to attach PXA I2C\n");
		return;
	}

	lockinit(&sc->sc_lock, PZERO, "gxiic", 0, 0);

	/* Initialize i2c_controller  */
	sc->sc_i2c.ic_cookie = sc;
	sc->sc_i2c.ic_acquire_bus = gxiic_acquire_bus;
	sc->sc_i2c.ic_release_bus = gxiic_release_bus;
	sc->sc_i2c.ic_send_start = NULL;
	sc->sc_i2c.ic_send_stop = NULL;
	sc->sc_i2c.ic_initiate_xfer = NULL;
	sc->sc_i2c.ic_read_byte = NULL;
	sc->sc_i2c.ic_write_byte = NULL;
	sc->sc_i2c.ic_exec = gxiic_exec;

	iba.iba_tag = &sc->sc_i2c;
	pxa2x0_i2c_open(&sc->sc_pxa_i2c);
	config_found_ia(&sc->sc_pxa_i2c.sc_dev, "i2cbus", &iba, iicbus_print);
	pxa2x0_i2c_close(&sc->sc_pxa_i2c);
}

static int
gxiic_acquire_bus(void *cookie, int flags)
{
	struct gxiic_softc *sc = cookie;
	int err;

	if ((err = lockmgr(&sc->sc_lock, LK_EXCLUSIVE, NULL)) == 0)
		pxa2x0_i2c_open(&sc->sc_pxa_i2c);

	return err;
}

static void
gxiic_release_bus(void *cookie, int flags)
{
	struct gxiic_softc *sc = cookie;

	lockmgr(&sc->sc_lock, LK_RELEASE, NULL);
	pxa2x0_i2c_close(&sc->sc_pxa_i2c);
	return;
}

static int
gxiic_exec(void *cookie, i2c_op_t op, i2c_addr_t addr, const void *vcmd,
	   size_t cmdlen, void *vbuf, size_t buflen, int flags)
{
	struct gxiic_softc *sc = cookie;
	int rv = -1;

	if (I2C_OP_READ_P(op) && (cmdlen == 0) && (buflen == 1))
		rv = pxa2x0_i2c_read(&sc->sc_pxa_i2c, addr, (u_char *)vbuf);

	if ((I2C_OP_READ_P(op)) && (cmdlen == 1) && (buflen == 1)) {
		rv = pxa2x0_i2c_write(&sc->sc_pxa_i2c, addr, *(u_char *)vbuf);
		if (rv == 0)
			rv = pxa2x0_i2c_read(&sc->sc_pxa_i2c,
			    addr, (u_char *)vbuf);
	}

	if ((I2C_OP_READ_P(op)) && (cmdlen == 1) && (buflen == 2)) {
		rv = pxa2x0_i2c_write(&sc->sc_pxa_i2c, addr, *(u_char *)vbuf);
		if (rv == 0)
			rv = pxa2x0_i2c_read(&sc->sc_pxa_i2c,
			    addr, (u_char *)vbuf);
		if (rv == 0)
			rv = pxa2x0_i2c_read(&sc->sc_pxa_i2c,
			    addr, (u_char *)(vbuf) + 1);
	}

	if ((I2C_OP_WRITE_P(op)) && (cmdlen == 0) && (buflen == 1))
		rv = pxa2x0_i2c_write(&sc->sc_pxa_i2c, addr, *(u_char *)vbuf);

	if ((I2C_OP_WRITE_P(op)) && (cmdlen == 1) && (buflen == 1)) {
		rv = pxa2x0_i2c_write(&sc->sc_pxa_i2c,
		    addr, *(const u_char *)vcmd);
		if (rv == 0)
			rv = pxa2x0_i2c_write(&sc->sc_pxa_i2c,
			    addr, *(u_char *)vbuf);
	}

	if ((I2C_OP_WRITE_P(op)) && (cmdlen == 1) && (buflen == 2)) {
		rv = pxa2x0_i2c_write(&sc->sc_pxa_i2c,
		    addr, *(const u_char *)vcmd);
		if (rv == 0)
			rv = pxa2x0_i2c_write_2(&sc->sc_pxa_i2c,
			    addr, *(u_short *)vbuf);
	}

	return rv;
}
