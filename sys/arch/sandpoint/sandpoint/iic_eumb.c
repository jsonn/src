/* $NetBSD: iic_eumb.c,v 1.2.22.1 2008/04/03 12:42:24 mjf Exp $ */

/*-
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Tohru Nishimura.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: iic_eumb.c,v 1.2.22.1 2008/04/03 12:42:24 mjf Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/tty.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/pio.h>

#include <dev/i2c/i2cvar.h>

#include <sandpoint/sandpoint/eumbvar.h>

void iic_bootstrap_init(void);
int iic_seep_bootstrap_read(int, int, uint8_t *, size_t);

static int  iic_eumb_match(struct device *, struct cfdata *, void *);
static void iic_eumb_attach(struct device *, struct device *, void *);

struct iic_eumb_softc {
	struct device		sc_dev;
	struct i2c_controller	sc_i2c;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

CFATTACH_DECL(iic_eumb, sizeof(struct iic_eumb_softc),
    iic_eumb_match, iic_eumb_attach, NULL, NULL);

static int motoi2c_acquire_bus(void *, int);
static void motoi2c_release_bus(void *, int);
static int motoi2c_send_start(void *, int);
static int motoi2c_send_stop(void *, int);
static int motoi2c_initiate_xfer(void *, uint16_t, int);
static int motoi2c_read_byte(void *, uint8_t *, int);
static int motoi2c_write_byte(void *, uint8_t, int);
static void wait4done(void);

static struct i2c_controller motoi2c = {
	.ic_acquire_bus = motoi2c_acquire_bus,
	.ic_release_bus = motoi2c_release_bus,
	.ic_send_start	= motoi2c_send_start,
	.ic_send_stop	= motoi2c_send_stop,
	.ic_initiate_xfer = motoi2c_initiate_xfer,
	.ic_read_byte	= motoi2c_read_byte,
	.ic_write_byte	= motoi2c_write_byte,
};

/*
 * This I2C controller seems to share a common design with
 * i.MX/MC9328.  Different names in bit field definition and
 * not suffered from document error.
 */
#define I2CADR	0x0000	/* my own I2C addr to respond for an external master */
#define I2CFDR	0x0004	/* frequency devider */
#define I2CCR	0x0008	/* control */
#define	 CR_MEN   0x80	/* enable this HW */
#define	 CR_MIEN  0x40	/* enable interrupt */
#define	 CR_MSTA  0x20	/* 0->1 activates START, 1->0 makes STOP condition */
#define	 CR_MTX   0x10	/* 1 for Tx, 0 for Rx */
#define	 CR_TXAK  0x08	/* 1 makes no acknowledge when Rx */
#define	 CR_RSTA  0x04	/* generate repeated START condition */
#define I2CSR	0x000c	/* status */
#define	 SR_MCF   0x80	/* date transter has completed */
#define	 SR_MBB   0x20	/* 1 before STOP condition is detected */
#define	 SR_MAL   0x10	/* arbitration was lost */
#define	 SR_MIF   0x02	/* indicates data transter completion */
#define	 SR_RXAK  0x01
#define I2CDR	0x0010	/* data */

#define	CSR_READ(r)	in8rb(0xfc003000 + (r))
#define	CSR_WRITE(r,v)	out8rb(0xfc003000 + (r), (v))
#define	CSR_WRITE4(r,v)	out32rb(0xfc003000 + (r), (v))

static int found;

static int
iic_eumb_match(struct device *parent, struct cfdata *cf, void *aux)
{

	return (found == 0);
}

static void
iic_eumb_attach(struct device *parent, struct device *self, void *aux)
{
	struct iic_eumb_softc *sc = (void *)self;
	struct eumb_attach_args *eaa = aux;
	struct i2cbus_attach_args iba;
	bus_space_handle_t ioh;

	found = 1;
	printf("\n");

	bus_space_map(eaa->eumb_bt, 0x3000, 0x20, 0, &ioh);
	sc->sc_i2c = motoi2c;
	sc->sc_i2c.ic_cookie = sc;
	sc->sc_iot = eaa->eumb_bt;
	sc->sc_ioh = ioh;
	iba.iba_tag = &sc->sc_i2c;

	iic_bootstrap_init();
#if 0
	/* not yet */
	config_found_ia(&sc->sc_dev, "i2cbus", &iba, iicbus_print);

	intr_establish(16 + 16, IST_LEVEL, IPL_SERIAL, iic_intr, sc);
#endif
}

void
iic_bootstrap_init()
{

	CSR_WRITE(I2CCR, 0);
	CSR_WRITE4(I2CFDR, 0x1031); /* XXX magic XXX */
	CSR_WRITE(I2CADR, 0);
	CSR_WRITE(I2CSR, 0);
	CSR_WRITE(I2CCR, CR_MEN);
}

int
iic_seep_bootstrap_read(int i2caddr, int offset, uint8_t *rvp, size_t len)
{
	i2c_addr_t addr;
	uint8_t cmdbuf[1];

	if (motoi2c_acquire_bus(&motoi2c, I2C_F_POLL) != 0)
		return -1;
	while (len) {
		addr = i2caddr + (offset >> 8);
		cmdbuf[0] = offset & 0xff;
		if (iic_exec(&motoi2c, I2C_OP_READ_WITH_STOP, addr,
			     cmdbuf, 1, rvp, 1, I2C_F_POLL)) {
			motoi2c_release_bus(&motoi2c, I2C_F_POLL);
			return -1;
		}
		len--;
		rvp++;
		offset++;
	}
	motoi2c_release_bus(&motoi2c, I2C_F_POLL);
	return 0;	
}

static int
motoi2c_acquire_bus(void *v, int flags)
{
	unsigned loop = 10;

	while (--loop != 0 && (CSR_READ(I2CSR) & SR_MBB))
		DELAY(1);
	if (loop == 0)
		return -1;
printf("bus acquired\n");
	return 0;
}

static void
motoi2c_release_bus(void *v, int flags)
{
	unsigned loop = 10;

	while (--loop != 0 && (CSR_READ(I2CSR) & SR_MBB))
		DELAY(1);
}

static int
motoi2c_send_start(void *v, int flags)
{
	unsigned cr, sr;

	cr = CSR_READ(I2CCR);
	cr |= CR_MSTA;
	CSR_WRITE(I2CCR, cr);
	do {
		sr = CSR_READ(I2CSR);
		if (sr & SR_MAL) {
			printf("moti2c_send_start() lost sync\n");
			sr &= ~SR_MAL;
			CSR_WRITE(I2CSR, sr);
			return -1;
		}
	} while ((sr & SR_MBB) == 0);
printf("start sent\n");
	return 0;
}

static int
motoi2c_send_stop(void *v, int flags)
{
	unsigned cr;

	cr = CSR_READ(I2CCR);
	cr &= ~CR_MSTA;
	CSR_WRITE(I2CCR, cr);
	(void)CSR_READ(I2CDR);
printf("stop sent\n");
	return 0;
}

static int
motoi2c_initiate_xfer(void *v, i2c_addr_t addr, int flags)
{
	unsigned cr;

	cr = CSR_READ(I2CCR);
	if (flags & I2C_F_READ) {
		cr &= ~(CR_MTX | CR_TXAK);
		cr |= CR_RSTA;
		CSR_WRITE(I2CCR, cr);
		cr &= ~CR_RSTA;
		CSR_WRITE(I2CCR, cr);
		(void)CSR_READ(I2CDR);
		wait4done();
		return 0;
	}
	cr |= CR_MTX;
	CSR_WRITE(I2CCR, cr);
	return 0;
}

static int
motoi2c_read_byte(void *v, uint8_t *bytep, int flags)
{
	unsigned cr, val;

	if (flags & I2C_F_LAST) {
		cr = CSR_READ(I2CCR);
		cr |= CR_TXAK;
		CSR_WRITE(I2CCR, cr);
	}
	val = CSR_READ(I2CDR);
	wait4done();
	*bytep = val;
	return 0;
}

static int
motoi2c_write_byte(void *v, uint8_t byte, int flags)
{

	CSR_WRITE(I2CDR, byte);
	wait4done();
	return 0;
}

/* busy waiting for byte data transfer completion */
static void
wait4done()
{
	unsigned sr;

	do {
		sr = CSR_READ(I2CSR);
	} while ((sr & SR_MIF) == 0);
	CSR_WRITE(I2CSR, sr &~ SR_MIF);
}
