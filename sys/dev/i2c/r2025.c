/* $NetBSD: r2025.c,v 1.2.8.2 2006/06/21 15:02:51 yamt Exp $ */

/*-
 * Copyright (c) 2006 Shigeyuki Fukushima.
 * All rights reserved.
 *
 * Written by Shigeyuki Fukushima.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: r2025.c,v 1.2.8.2 2006/06/21 15:02:51 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <sys/conf.h>
#include <sys/event.h>

#include <dev/clock_subr.h>

#include <dev/i2c/i2cvar.h>
#include <dev/i2c/r2025reg.h>

struct r2025rtc_softc {
	struct device		sc_dev;
	i2c_tag_t		sc_tag;
	int			sc_address;
	int			sc_open;
	struct todr_chip_handle	sc_todr;
};

static void	r2025rtc_attach(struct device *, struct device *, void *);
static int	r2025rtc_match(struct device *, struct cfdata *, void *);

CFATTACH_DECL(r2025rtc, sizeof(struct r2025rtc_softc),
	r2025rtc_match, r2025rtc_attach, NULL, NULL);

static int	r2025rtc_gettime(struct todr_chip_handle *,
				volatile struct timeval *);
static int	r2025rtc_settime(struct todr_chip_handle *,
				volatile struct timeval *);
static int	r2025rtc_getcal(struct todr_chip_handle *, int *);
static int	r2025rtc_setcal(struct todr_chip_handle *, int);

static int	r2025rtc_reg_write(struct r2025rtc_softc *, int, uint8_t*, int);
static int	r2025rtc_reg_read(struct r2025rtc_softc *, int, uint8_t*, int);


static int
r2025rtc_match(struct device *parent, struct cfdata *cf, void *arg)
{
	struct i2c_attach_args *ia = arg;

	/* match only R2025 RTC devices */
	if (ia->ia_addr == R2025_ADDR)
		return 1;

	return 0;
}

static void
r2025rtc_attach(struct device *parent, struct device *self, void *arg)
{
	struct r2025rtc_softc *sc = device_private(self);
	struct i2c_attach_args *ia = arg;

	aprint_normal(": RICOH R2025S/D Real-time Clock\n");

	sc->sc_tag = ia->ia_tag;
	sc->sc_address = ia->ia_addr;
	sc->sc_open = 0;
	sc->sc_todr.cookie = sc;
	sc->sc_todr.todr_gettime = r2025rtc_gettime;
	sc->sc_todr.todr_settime = r2025rtc_settime;
	sc->sc_todr.todr_getcal = r2025rtc_getcal;
	sc->sc_todr.todr_setcal = r2025rtc_setcal;
	sc->sc_todr.todr_setwen = NULL;

	todr_attach(&sc->sc_todr);
}

static int
r2025rtc_gettime(struct todr_chip_handle *ch, volatile struct timeval *tv)
{
	struct r2025rtc_softc *sc = ch->cookie;
	struct clock_ymdhms dt;
	uint8_t rctrl;
	uint8_t bcd[R2025_CLK_SIZE];
	int hour;

	memset(&dt, 0, sizeof(dt));

	if (r2025rtc_reg_read(sc, R2025_REG_CTRL1, &rctrl, 1) != 0) {
		printf("%s: r2025rtc_gettime: failed to read registers.\n",
			sc->sc_dev.dv_xname);
		return -1;
	}

	if (r2025rtc_reg_read(sc, R2025_REG_SEC, &bcd[0], R2025_CLK_SIZE)
		!= 0) {
		printf("%s: r2025rtc_gettime: failed to read registers.\n",
			sc->sc_dev.dv_xname);
		return -1;
	}

	dt.dt_sec = FROMBCD(bcd[R2025_REG_SEC] & R2025_REG_SEC_MASK);
	dt.dt_min = FROMBCD(bcd[R2025_REG_MIN] & R2025_REG_MIN_MASK);
	hour = FROMBCD(bcd[R2025_REG_HOUR] & R2025_REG_HOUR_MASK);
	if (rctrl & R2025_REG_CTRL1_H1224) {
		dt.dt_hour = hour;
	} else {
		if (hour == 12) {
			dt.dt_hour = 0;
		} else if (hour == 32) {
			dt.dt_hour = 12;
		} else if (hour > 13) {
			dt.dt_hour = (hour - 8);
		} else { /* (hour < 12) */
			dt.dt_hour = hour;
		}
	}
	dt.dt_wday = FROMBCD(bcd[R2025_REG_WDAY] & R2025_REG_WDAY_MASK);
	dt.dt_day = FROMBCD(bcd[R2025_REG_DAY] & R2025_REG_DAY_MASK);
	dt.dt_mon = FROMBCD(bcd[R2025_REG_MON] & R2025_REG_MON_MASK);
	dt.dt_year = FROMBCD(bcd[R2025_REG_YEAR] & R2025_REG_YEAR_MASK)
		+ ((bcd[R2025_REG_MON] & R2025_REG_MON_Y1920) ? 2000 : 1900);

	tv->tv_sec = clock_ymdhms_to_secs(&dt);
	tv->tv_usec = 0;

	return 0;
}

static int
r2025rtc_settime(struct todr_chip_handle *ch, volatile struct timeval *tv)
{
	struct r2025rtc_softc *sc = ch->cookie;
	struct clock_ymdhms dt;
	uint8_t rctrl;
	uint8_t bcd[R2025_CLK_SIZE];

	clock_secs_to_ymdhms(tv->tv_sec, &dt);

	/* Y3K problem */
	if (dt.dt_year >= 3000) {
		printf("%s: r2025rtc_settime: "
			"RTC does not support year 3000 or over.\n",
			sc->sc_dev.dv_xname);
		return -1;
	}

	if (r2025rtc_reg_read(sc, R2025_REG_CTRL1, &rctrl, 1) != 0) {
		printf("%s: r2025rtc_settime: failed to read register.\n",
			sc->sc_dev.dv_xname);
		return -1;
	}
	rctrl |= R2025_REG_CTRL1_H1224;

	/* setup registers 0x00-0x06 (7 byte) */
	bcd[R2025_REG_SEC] = TOBCD(dt.dt_sec) & R2025_REG_SEC_MASK;
	bcd[R2025_REG_MIN] = TOBCD(dt.dt_min) & R2025_REG_MIN_MASK;
	bcd[R2025_REG_HOUR] = TOBCD(dt.dt_hour) & R2025_REG_HOUR_MASK;
	bcd[R2025_REG_WDAY] = TOBCD(dt.dt_wday) & R2025_REG_WDAY_MASK;
	bcd[R2025_REG_DAY] = TOBCD(dt.dt_day) & R2025_REG_DAY_MASK;
	bcd[R2025_REG_MON] = (TOBCD(dt.dt_mon) & R2025_REG_MON_MASK)
		| ((dt.dt_year >= 2000) ? R2025_REG_MON_Y1920 : 0);
	bcd[R2025_REG_YEAR] = TOBCD(dt.dt_year % 100) & R2025_REG_YEAR_MASK;

	/* Write RTC register */
	if (r2025rtc_reg_write(sc, R2025_REG_CTRL1, &rctrl, 1) != 0) {
		printf("%s: r2025rtc_settime: failed to write registers.\n",
			sc->sc_dev.dv_xname);
		return -1;
	}
	if (r2025rtc_reg_write(sc, R2025_REG_SEC, bcd, R2025_CLK_SIZE) != 0) {
		printf("%s: r2025rtc_settime: failed to write registers.\n",
			sc->sc_dev.dv_xname);
		return -1;
	}

	return 0;
}

static int
r2025rtc_setcal(struct todr_chip_handle *ch, int cal)
{
	return EOPNOTSUPP;
}

static int
r2025rtc_getcal(struct todr_chip_handle *ch, int *cal)
{
	return EOPNOTSUPP;
}

static int
r2025rtc_reg_write(struct r2025rtc_softc *sc, int reg, uint8_t *val, int len)
{
	int i;
	uint8_t buf[1];
	uint8_t cmdbuf[1];

	if (iic_acquire_bus(sc->sc_tag, I2C_F_POLL)) {
		printf("%s: r2025rtc_clock_write: failed to acquire I2C bus\n",
			sc->sc_dev.dv_xname);
		return -1;
	}

	for (i = 0 ; i < len ; i++) {
		cmdbuf[0] = (((reg + i) << 4) & 0xf0);
		buf[0] = val[i];
		if (iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP, sc->sc_address,
				cmdbuf, 1, buf, 1, I2C_F_POLL)) {
			iic_release_bus(sc->sc_tag, I2C_F_POLL);
			printf("%s: r2025rtc_reg_write: "
				"failed to write registers\n",
				sc->sc_dev.dv_xname);
			return -1;
		}
	}

	iic_release_bus(sc->sc_tag, I2C_F_POLL);

	return 0;
}

static int
r2025rtc_reg_read(struct r2025rtc_softc *sc, int reg, uint8_t *val, int len)
{
	int i;
	uint8_t buf[1];
	uint8_t cmdbuf[1];

	if (iic_acquire_bus(sc->sc_tag, I2C_F_POLL)) {
		printf("%s: r2025rtc_clock_read: failed to acquire I2C bus\n",
			sc->sc_dev.dv_xname);
		return -1;
	}

	for (i = 0 ; i < len ; i++) {
		cmdbuf[0] = (((reg + i) << 4) & 0xf0);
		buf[0] = 0;
		if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP, sc->sc_address,
				cmdbuf, 1, buf, 1, I2C_F_POLL)) {
			iic_release_bus(sc->sc_tag, I2C_F_POLL);
			printf("%s: r2025rtc_reg_read: "
				"failed to write registers\n",
				sc->sc_dev.dv_xname);
			return -1;
		}

		*(val + i) = buf[0];
	}

	iic_release_bus(sc->sc_tag, I2C_F_POLL);

	return 0;
}
