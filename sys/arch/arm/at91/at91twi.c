/*	$Id: at91twi.c,v 1.1.2.1 2007/11/10 02:56:38 matt Exp $	*/
/*	$NetBSD: at91twi.c,v 1.1.2.1 2007/11/10 02:56:38 matt Exp $	*/

/*-
 * Copyright (c) 2007 Embedtronics Oy. All rights reserved.
 *
 * Based on arch/macppc/dev/ki2c.c,
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: at91twi.c,v 1.1.2.1 2007/11/10 02:56:38 matt Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <sys/lock.h>

#include <arm/at91/at91var.h>
#include <arm/at91/at91reg.h>

#include <dev/i2c/i2cvar.h>
#include <arm/at91/at91twivar.h>
#include <arm/at91/at91twireg.h>

int at91twi_match(struct device *, struct cfdata *, void *);
void at91twi_attach(struct device *, struct device *, void *);
inline u_int at91twi_readreg(struct at91twi_softc *, int);
inline void at91twi_writereg(struct at91twi_softc *, int, u_int);
int at91twi_intr(void *);
int at91twi_poll(struct at91twi_softc *, int, int);
int at91twi_start(struct at91twi_softc *, int, void *, int, int);
int at91twi_read(struct at91twi_softc *, int, void *, int, int);
int at91twi_write(struct at91twi_softc *, int, void *, int, int);

/* I2C glue */
static int at91twi_i2c_acquire_bus(void *, int);
static void at91twi_i2c_release_bus(void *, int);
static int at91twi_i2c_exec(void *, i2c_op_t, i2c_addr_t, const void *, size_t,
		    void *, size_t, int);


const struct cfattach at91twi_ca = {
	"at91twi", {}, sizeof(struct at91twi_softc), at91twi_match, at91twi_attach
};

int
at91twi_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	if (strcmp(match->cf_name, "at91twi") == 0)
		return 2;
	return 0;
}

void
at91twi_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct at91twi_softc *sc = (struct at91twi_softc *)self;
	struct at91bus_attach_args *sa = aux;
	struct i2cbus_attach_args iba;
	unsigned ckdiv, cxdiv;

	// gather attach data:
	sc->sc_iot = sa->sa_iot;
	sc->sc_pid = sa->sa_pid;

	if (bus_space_map(sa->sa_iot, sa->sa_addr, sa->sa_size, 0, &sc->sc_ioh))
		panic("%s: Cannot map registers", self->dv_xname);

	printf(": I2C controller\n");

	/* initialize I2C controller */
	at91_peripheral_clock(sc->sc_pid, 1);

	at91twi_writereg(sc, TWI_CR, TWI_CR_SWRST);
	delay(1000);
#if 1
	// target to 100 kHz
	for (ckdiv = 0; ckdiv < 8; ckdiv++) {
		if ((cxdiv = (AT91_MSTCLK / (1U << ckdiv)) / (2 * 50000U)) < 256) {
			goto found_ckdiv;
		}
	}
	panic("%s: Cannot calculate clock divider!", __FUNCTION__);

found_ckdiv:
#else
	ckdiv = 5; cxdiv = 0xFF;
#endif
	at91twi_writereg(sc, TWI_CWGR, (ckdiv << 16) | (cxdiv << 8) | cxdiv);
	at91twi_writereg(sc, TWI_CR, TWI_CR_MSEN);

//#ifdef AT91TWI_DEBUG
	printf("%s: ckdiv=%d cxdiv=%d CWGR=0x%08X SR=0x%08X\n", self->dv_xname, ckdiv, cxdiv, at91twi_readreg(sc, TWI_CWGR), at91twi_readreg(sc, TWI_SR));
//#endif

	/* initialize rest */
	lockinit(&sc->sc_buslock, PRIBIO|PCATCH, sc->sc_dev.dv_xname, 0, 0);
	sc->sc_ih = at91_intr_establish(sc->sc_pid, IPL_SERIAL, INTR_HIGH_LEVEL,
					at91twi_intr, sc);

	/* fill in the i2c tag */
	sc->sc_i2c.ic_cookie = sc;
	sc->sc_i2c.ic_acquire_bus = at91twi_i2c_acquire_bus;
	sc->sc_i2c.ic_release_bus = at91twi_i2c_release_bus;
	sc->sc_i2c.ic_send_start = NULL;
	sc->sc_i2c.ic_send_stop = NULL;
	sc->sc_i2c.ic_initiate_xfer = NULL;
	sc->sc_i2c.ic_read_byte = NULL;
	sc->sc_i2c.ic_write_byte = NULL;
	sc->sc_i2c.ic_exec = at91twi_i2c_exec;

	iba.iba_tag = &sc->sc_i2c;
	(void) config_found_ia(&sc->sc_dev, "i2cbus", &iba, iicbus_print);
}

u_int
at91twi_readreg(sc, reg)
	struct at91twi_softc *sc;
	int reg;
{
	return bus_space_read_4(sc->sc_iot, sc->sc_ioh, reg);
}

void
at91twi_writereg(sc, reg, val)
	struct at91twi_softc *sc;
	int reg;
	u_int val;
{
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, reg, val);
}

int
at91twi_intr(void *arg)
{
	struct at91twi_softc *sc = arg;
	u_int sr, isr, imr;

	sr = at91twi_readreg(sc, TWI_SR);
	imr = at91twi_readreg(sc, TWI_IMR);
	isr = sr & imr;

	if (!isr) {
#ifdef AT91TWI_DEBUG
//		printf("%s(%s): interrupts are disabled (sr=%08X imr=%08X)\n", __FUNCTION__, sc->sc_dev.dv_xname, sr, imr);
#endif
		return 0;
	}

	if (isr & TWI_SR_TXCOMP) {
		// transmission has completed!
		if (sr & (TWI_SR_NACK | TWI_SR_UNRE | TWI_SR_OVRE)) {
			// failed!
#ifdef AT91TWI_DEBUG
			printf("%s(%s): FAILED (sr=%08X)\n", __FUNCTION__, 
			       sc->sc_dev.dv_xname, sr);
#endif
			sc->sc_flags |= I2C_ERROR;
		} else {
#ifdef AT91TWI_DEBUG
			printf("%s(%s): SUCCESS (sr=%08X)\n", __FUNCTION__, 
			       sc->sc_dev.dv_xname, sr);
#endif
		}
		if (sc->sc_flags & I2C_READING && sr & TWI_SR_RXRDY) {
			*sc->sc_data++ = at91twi_readreg(sc, TWI_RHR);
			sc->sc_resid--;
		}
		sc->sc_flags &= ~I2C_BUSY;
		at91twi_writereg(sc, TWI_IDR, -1);
		goto out;
	}

	if (isr & TWI_SR_TXRDY) {
		if (--sc->sc_resid > 0)
			at91twi_writereg(sc, TWI_THR, *sc->sc_data++);
	}

	if (isr & TWI_SR_RXRDY) {
		// data has been received
		*sc->sc_data++ = at91twi_readreg(sc, TWI_RHR);
		sc->sc_resid--;
	}

	if (isr & (TWI_SR_TXRDY | TWI_SR_RXRDY) && sc->sc_resid <= 0) {
		// all bytes have been transmitted, send stop condition
		at91twi_writereg(sc, TWI_IDR, TWI_SR_RXRDY | TWI_SR_TXRDY);
		at91twi_writereg(sc, TWI_CR, TWI_CR_STOP);
	}
out:
	return 1;
}

int
at91twi_poll(sc, timo, flags)
	struct at91twi_softc *sc;
	int timo, flags;
{

	timo = 1000000U;

	while (sc->sc_flags & I2C_BUSY) {
		if (timo < 0) {
			printf("i2c_poll: timeout\n");
			return -1;
		}
		if (flags & I2C_F_POLL) {
			at91_intr_poll(sc->sc_ih, 1);
			delay(1);
			timo--;
		} else {
			delay(100); // @@@ sleep!?
			timo -= 100;
		}
	}
	return 0;
}

int
at91twi_start(sc, addr, data, len, flags)
	struct at91twi_softc *sc;
	int addr;
	void *data;
	int len, flags;
{
	int rd = (sc->sc_flags & I2C_READING);
	int timo, s;

	KASSERT((addr & 1) == 0);

	sc->sc_data = data;
	sc->sc_resid = len;
	sc->sc_flags |= I2C_BUSY;

	timo = 1000 + len * 200;

	s = splserial();
	// if writing, queue first byte immediately
	if (!rd)
		at91twi_writereg(sc, TWI_THR, *sc->sc_data++);
	// if there's just one byte to transmit, we must set STOP-bit too
	if (sc->sc_resid == 1) {
		at91twi_writereg(sc, TWI_IER, TWI_SR_TXCOMP);
		at91twi_writereg(sc, TWI_CR, TWI_CR_START | TWI_CR_STOP);
	} else {
		at91twi_writereg(sc, TWI_IER, TWI_SR_TXCOMP 
				  | (rd ? TWI_SR_RXRDY : TWI_SR_TXRDY));
		at91twi_writereg(sc, TWI_CR, TWI_CR_START);
	}
	splx(s);

	if (at91twi_poll(sc, timo, flags))
		return -1;
	if (sc->sc_flags & I2C_ERROR) {
		printf("I2C_ERROR\n");
		return -1;
	}
	return 0;
}

int
at91twi_read(sc, addr, data, len, flags)
	struct at91twi_softc *sc;
	int addr, len;
	void *data;
	int flags;
{
	sc->sc_flags = I2C_READING;
	#ifdef AT91TWI_DEBUG
		printf("at91twi_read: %02x %d\n", addr, len);
	#endif
	return at91twi_start(sc, addr, data, len, flags);
}

int
at91twi_write(sc, addr, data, len, flags)
	struct at91twi_softc *sc;
	int addr, len;
	void *data;
	int flags;
{
	sc->sc_flags = 0;
	#ifdef AT91TWI_DEBUG
		printf("at91twi_write: %02x %d\n", addr, len);
	#endif
	return at91twi_start(sc, addr, data, len, flags);
}

static int
at91twi_i2c_acquire_bus(void *cookie, int flags)
{
	struct at91twi_softc *sc = cookie;

	if (flags & I2C_F_POLL)
		return 0;

	return (lockmgr(&sc->sc_buslock, LK_EXCLUSIVE, NULL));
}

static void
at91twi_i2c_release_bus(void *cookie, int flags)
{
	struct at91twi_softc *sc = cookie;

	if (flags & I2C_F_POLL)
		return;

	(void) lockmgr(&sc->sc_buslock, LK_RELEASE, NULL);
}

int
at91twi_i2c_exec(void *cookie, i2c_op_t op, i2c_addr_t addr, const void *vcmd,
    size_t cmdlen, void *vbuf, size_t buflen, int flags)
{
	struct at91twi_softc *sc = cookie;

	if (I2C_OP_READ_P(op))
	{
		u_int iadr = 0;
		if (vcmd) {
			const uint8_t *cmd = (const uint8_t *)vcmd;
			if (cmdlen > 3) {
				// we're in trouble..
				return -1;
			}
			iadr = cmd[0];
			if (cmdlen > 1) {
				iadr <<= 8;
				iadr |= cmd[1];
			}
			if (cmdlen > 2) {
				iadr <<= 8;
				iadr |= cmd[2];
			}
		}
		at91twi_writereg(sc, TWI_MMR, (addr << 16) | TWI_MMR_MREAD | (cmdlen << 8));
		if (cmdlen > 0) {
	#ifdef AT91TWI_DEBUG
			printf("at91twi_read: %02x iadr=%08X mmr=%08X\n", 
			       addr, iadr, at91twi_readreg(sc, TWI_MMR));
	#endif
			at91twi_writereg(sc, TWI_IADR, iadr);
		}
		if (at91twi_read(sc, addr, vbuf, buflen, flags) != 0)
			return -1;
	} else if (vcmd) {
		at91twi_writereg(sc, TWI_MMR, addr << 16);
		if (at91twi_write(sc, addr, __UNCONST(vcmd), cmdlen, flags) !=0)
			return -1;
	}
	return 0;
}
