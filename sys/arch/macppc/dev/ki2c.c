/*	$NetBSD: ki2c.c,v 1.1.4.2 2004/08/03 10:37:21 skrll Exp $	*/
/*	Id: ki2c.c,v 1.7 2002/10/05 09:56:05 tsubai Exp	*/

/*-
 * Copyright (c) 2001 Tsubai Masanari.  All rights reserved.
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <dev/ofw/openfirm.h>
#include <uvm/uvm_extern.h>
#include <machine/autoconf.h>

/* Keywest I2C Register offsets */
#define MODE	0
#define CONTROL	1
#define STATUS	2
#define ISR	3
#define IER	4
#define ADDR	5
#define SUBADDR	6
#define DATA	7

/* MODE */
#define I2C_SPEED	0x03	/* Speed mask */
#define  I2C_100kHz	0x00
#define  I2C_50kHz	0x01
#define  I2C_25kHz	0x02
#define I2C_MODE	0x0c	/* Mode mask */
#define  I2C_DUMBMODE	0x00	/*  Dumb mode */
#define  I2C_STDMODE	0x04	/*  Standard mode */
#define  I2C_STDSUBMODE	0x08	/*  Standard mode + sub address */
#define  I2C_COMBMODE	0x0c	/*  Combined mode */
#define I2C_PORT	0xf0	/* Port mask */

/* CONTROL */
#define I2C_CT_AAK	0x01	/* Send AAK */
#define I2C_CT_ADDR	0x02	/* Send address(es) */
#define I2C_CT_STOP	0x04	/* Send STOP */
#define I2C_CT_START	0x08	/* Send START */

/* STATUS */
#define I2C_ST_BUSY	0x01	/* Busy */
#define I2C_ST_LASTAAK	0x02	/* Last AAK */
#define I2C_ST_LASTRW	0x04	/* Last R/W */
#define I2C_ST_SDA	0x08	/* SDA */
#define I2C_ST_SCL	0x10	/* SCL */

/* ISR/IER */
#define I2C_INT_DATA	0x01	/* Data byte sent/received */
#define I2C_INT_ADDR	0x02	/* Address sent */
#define I2C_INT_STOP	0x04	/* STOP condition sent */
#define I2C_INT_START	0x08	/* START condition sent */

/* I2C flags */
#define I2C_BUSY	0x01
#define I2C_READING	0x02
#define I2C_ERROR	0x04

struct ki2c_softc {
	struct device sc_dev;
	u_char *sc_reg;
	int sc_regstep;

	int sc_flags;
	u_char *sc_data;
	int sc_resid;
};

int ki2c_match(struct device *, struct cfdata *, void *);
void ki2c_attach(struct device *, struct device *, void *);
inline u_int ki2c_readreg(struct ki2c_softc *, int);
inline void ki2c_writereg(struct ki2c_softc *, int, u_int);
u_int ki2c_getmode(struct ki2c_softc *);
void ki2c_setmode(struct ki2c_softc *, u_int);
u_int ki2c_getspeed(struct ki2c_softc *);
void ki2c_setspeed(struct ki2c_softc *, u_int);
int ki2c_intr(struct ki2c_softc *);
int ki2c_poll(struct ki2c_softc *, int);
int ki2c_start(struct ki2c_softc *, int, int, void *, int);
int ki2c_read(struct ki2c_softc *, int, int, void *, int);
int ki2c_write(struct ki2c_softc *, int, int, const void *, int);

struct cfattach ki2c_ca = {
	"ki2c", {}, sizeof(struct ki2c_softc), ki2c_match, ki2c_attach
};

int
ki2c_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct confargs *ca = aux;

	if (strcmp(ca->ca_name, "i2c") == 0)
		return 1;

	return 0;
}

void
ki2c_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct ki2c_softc *sc = (struct ki2c_softc *)self;
	struct confargs *ca = aux;
	int node = ca->ca_node;
	int rate;

	ca->ca_reg[0] += ca->ca_baseaddr;

	if (OF_getprop(node, "AAPL,i2c-rate", &rate, 4) != 4) {
		printf(": cannot get i2c-rate\n");
		return;
	}
	if (OF_getprop(node, "AAPL,address", &sc->sc_reg, 4) != 4) {
		printf(": unable to find i2c address\n");
		return;
	}
	if (OF_getprop(node, "AAPL,address-step", &sc->sc_regstep, 4) != 4) {
		printf(": unable to find i2c address step\n");
		return;
	}

	printf("\n");

	ki2c_writereg(sc, STATUS, 0);
	ki2c_writereg(sc, ISR, 0);
	ki2c_writereg(sc, IER, 0);

	ki2c_setmode(sc, I2C_STDSUBMODE);
	ki2c_setspeed(sc, I2C_100kHz);		/* XXX rate */
}

u_int
ki2c_readreg(sc, reg)
	struct ki2c_softc *sc;
	int reg;
{
	u_char *addr = sc->sc_reg + sc->sc_regstep * reg;

	return *addr;
}

void
ki2c_writereg(sc, reg, val)
	struct ki2c_softc *sc;
	int reg;
	u_int val;
{
	u_char *addr = sc->sc_reg + sc->sc_regstep * reg;

	*addr = val;
	asm volatile ("eieio");
	delay(10);
}

u_int
ki2c_getmode(sc)
	struct ki2c_softc *sc;
{
	return ki2c_readreg(sc, MODE) & I2C_MODE;
}

void
ki2c_setmode(sc, mode)
	struct ki2c_softc *sc;
	u_int mode;
{
	u_int x;

	KASSERT((mode & ~I2C_MODE) == 0);
	x = ki2c_readreg(sc, MODE);
	x &= ~I2C_MODE;
	x |= mode;
	ki2c_writereg(sc, MODE, x);
}

u_int
ki2c_getspeed(sc)
	struct ki2c_softc *sc;
{
	return ki2c_readreg(sc, MODE) & I2C_SPEED;
}

void
ki2c_setspeed(sc, speed)
	struct ki2c_softc *sc;
	u_int speed;
{
	u_int x;

	KASSERT((speed & ~I2C_SPEED) == 0);
	x = ki2c_readreg(sc, MODE);
	x &= ~I2C_SPEED;
	x |= speed;
	ki2c_writereg(sc, MODE, x);
}

int
ki2c_intr(sc)
	struct ki2c_softc *sc;
{
	u_int isr, x;

	isr = ki2c_readreg(sc, ISR);

	if (isr & I2C_INT_ADDR) {
#if 0
		if ((ki2c_readreg(sc, STATUS) & I2C_ST_LASTAAK) == 0) {
			/* No slave responded. */
			sc->sc_flags |= I2C_ERROR;
			goto out;
		}
#endif

		if (sc->sc_flags & I2C_READING) {
			if (sc->sc_resid > 1) {
				x = ki2c_readreg(sc, CONTROL);
				x |= I2C_CT_AAK;
				ki2c_writereg(sc, CONTROL, x);
			}
		} else {
			ki2c_writereg(sc, DATA, *sc->sc_data++);
			sc->sc_resid--;
		}
	}

	if (isr & I2C_INT_DATA) {
		if (sc->sc_flags & I2C_READING) {
			*sc->sc_data++ = ki2c_readreg(sc, DATA);
			sc->sc_resid--;

			if (sc->sc_resid == 0) {	/* Completed */
				ki2c_writereg(sc, CONTROL, 0);
				goto out;
			}
		} else {
#if 0
			if ((ki2c_readreg(sc, STATUS) & I2C_ST_LASTAAK) == 0) {
				/* No slave responded. */
				sc->sc_flags |= I2C_ERROR;
				goto out;
			}
#endif

			if (sc->sc_resid == 0) {
				x = ki2c_readreg(sc, CONTROL) | I2C_CT_STOP;
				ki2c_writereg(sc, CONTROL, x);
			} else {
				ki2c_writereg(sc, DATA, *sc->sc_data++);
				sc->sc_resid--;
			}
		}
	}

out:
	if (isr & I2C_INT_STOP) {
		ki2c_writereg(sc, CONTROL, 0);
		sc->sc_flags &= ~I2C_BUSY;
	}

	ki2c_writereg(sc, ISR, isr);

	return 1;
}

int
ki2c_poll(sc, timo)
	struct ki2c_softc *sc;
	int timo;
{
	while (sc->sc_flags & I2C_BUSY) {
		if (ki2c_readreg(sc, ISR))
			ki2c_intr(sc);
		timo -= 100;
		if (timo < 0) {
			printf("i2c_poll: timeout\n");
			return -1;
		}
		delay(100);
	}
	return 0;
}

int
ki2c_start(sc, addr, subaddr, data, len)
	struct ki2c_softc *sc;
	int addr, subaddr, len;
	void *data;
{
	int rw = (sc->sc_flags & I2C_READING) ? 1 : 0;
	int timo, x;

	KASSERT((addr & 1) == 0);

	sc->sc_data = data;
	sc->sc_resid = len;
	sc->sc_flags |= I2C_BUSY;

	timo = 1000 + len * 200;

	/* XXX TAS3001 sometimes takes 50ms to finish writing registers. */
	/* if (addr == 0x68) */
		timo += 100000;

	ki2c_writereg(sc, ADDR, addr | rw);
	ki2c_writereg(sc, SUBADDR, subaddr);

	x = ki2c_readreg(sc, CONTROL) | I2C_CT_ADDR;
	ki2c_writereg(sc, CONTROL, x);

	if (ki2c_poll(sc, timo))
		return -1;
	if (sc->sc_flags & I2C_ERROR) {
		printf("I2C_ERROR\n");
		return -1;
	}
	return 0;
}

int
ki2c_read(sc, addr, subaddr, data, len)
	struct ki2c_softc *sc;
	int addr, subaddr, len;
	void *data;
{
	sc->sc_flags = I2C_READING;
	return ki2c_start(sc, addr, subaddr, data, len);
}

int
ki2c_write(sc, addr, subaddr, data, len)
	struct ki2c_softc *sc;
	int addr, subaddr, len;
	const void *data;
{
	sc->sc_flags = 0;
	return ki2c_start(sc, addr, subaddr, (void *)data, len);
}
