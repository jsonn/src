/*	$NetBSD: rtc.c,v 1.1.8.2 2002/10/18 02:33:29 nathanw Exp $	*/

/*
 * Copyright (c) 2000 Ben Harris
 * Copyright (c) 1994-1996 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * rtc.c - Routines to read and write the RTC and CMOS RAM
 */
/*
 * This driver supports the following chip:
 * Philips PCF8583	Clock/calendar with 240 x 8-bit RAM
 */

#include <sys/param.h>

__RCSID("$NetBSD: rtc.c,v 1.1.8.2 2002/10/18 02:33:29 nathanw Exp $");

#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <machine/machdep.h>

#include <dev/clock_subr.h>

#include <acorn26/ioc/iic.h>
#include <acorn26/ioc/pcf8583reg.h>

struct rtc_softc {
	struct device	sc_dev;
	int		sc_flags;
#define RTC_BROKEN	1
#define RTC_OPEN	2
	int		sc_addr;
	struct todr_chip_handle sc_ct;
};

static int rtcmatch(struct device *parent, struct cfdata *cf, void *aux);
static void rtcattach(struct device *parent, struct device *self, void *aux);
static int rtc_gettime(todr_chip_handle_t, struct timeval *);
static int rtc_settime(todr_chip_handle_t, struct timeval *);
static int rtc_getcal(todr_chip_handle_t, int *);
static int rtc_setcal(todr_chip_handle_t, int);

#define RTC_ADDR_YEAR     	0xc0
#define RTC_ADDR_CENT     	0xc1

extern struct cfdriver rtc_cd;

struct rtc_softc *the_rtc;

/* device and attach structures */

CFATTACH_DECL(rtc, sizeof(struct rtc_softc),
    rtcmatch, rtcattach, NULL, NULL);

/*
 * rtcmatch()
 *
 * Validate the IIC address to make sure its an RTC we understand
 */

int
rtcmatch(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct iicbus_attach_args *ib = aux;
	char buf[1];

	if ((ib->ib_addr & PCF8583_MASK) == PCF8583_ADDR &&
	    iic_control(parent, ib->ib_addr | IIC_READ, buf, 1) == 0)
		return 1;
	return 0;
}

/*
 * rtcattach()
 *
 * Attach the rtc device
 */

void
rtcattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct rtc_softc *sc = (struct rtc_softc *)self;
	struct iicbus_attach_args *ib = aux;
	u_char buff[1];

	sc->sc_flags |= RTC_BROKEN;
	sc->sc_addr = ib->ib_addr;
	if ((ib->ib_addr & PCF8583_MASK) == PCF8583_ADDR) {
		printf(": PCF8583");

		/* Read RTC register 0 and report info found */

		buff[0] = PCF8583_REG_CSR;

		if (iic_control(self->dv_parent, sc->sc_addr | IIC_WRITE,
				buff, 1))
			goto out;

		if (iic_control(self->dv_parent, sc->sc_addr | IIC_READ,
				buff, 1))
			goto out;

		switch (buff[0] & PCF8583_CSR_FN_MASK) {
		case PCF8583_CSR_FN_32768HZ:
			printf(", 32.768 kHz clock");
			break;
		case PCF8583_CSR_FN_50HZ:
			printf(", 50 Hz clock");
			break;
		case PCF8583_CSR_FN_EVENT:
			printf(", event counter");
			break;
		case PCF8583_CSR_FN_TEST:
			printf(", test mode");
			break;
		}

		if (buff[0] & PCF8583_CSR_STOP)
			printf(", stopped");
		if (buff[0] & PCF8583_CSR_ALARMENABLE)
			printf(", alarm enabled");
		sc->sc_flags &= ~RTC_BROKEN;
	}

	/* Set up MI todr(9) stuff (not really used) */
	sc->sc_ct.cookie = sc;
	sc->sc_ct.todr_settime = rtc_settime;
	sc->sc_ct.todr_gettime = rtc_gettime;
	sc->sc_ct.todr_getcal = rtc_getcal;
	sc->sc_ct.todr_setcal = rtc_setcal;
	if (the_rtc == NULL)
		the_rtc = sc;
 out:
	printf("\n");
}

/* Read a byte from CMOS RAM */

int
cmos_read(int location)
{
	u_char buff;
	struct rtc_softc *sc = the_rtc;

	KASSERT(sc != NULL);
	buff = location;

	if (iic_control(sc->sc_dev.dv_parent, sc->sc_addr | IIC_WRITE,
	    &buff, 1))
		return(-1);
	if (iic_control(sc->sc_dev.dv_parent, sc->sc_addr | IIC_READ,
	    &buff, 1))
		return(-1);

	return(buff);
}


/* Write a byte to CMOS RAM */

int
cmos_write(int location, int value)
{
	u_char buff[2];
	struct rtc_softc *sc = the_rtc;

	KASSERT(sc != NULL);
	buff[0] = location;
	buff[1] = value;

	if (iic_control(sc->sc_dev.dv_parent, sc->sc_addr | IIC_WRITE,
	    buff, 2))
		return(-1);

	return(0);
}

static int
rtc_settime(todr_chip_handle_t handle, struct timeval *tv)
{
	struct rtc_softc *sc = handle->cookie;
	u_char buff[8];
	struct clock_ymdhms ymdhms;

	clock_secs_to_ymdhms(tv->tv_sec, &ymdhms);

	buff[0] = PCF8583_REG_CENTI;

	buff[PCF8583_REG_CENTI] = TOBCD(tv->tv_usec / 10000);
	buff[PCF8583_REG_SEC]   = TOBCD(ymdhms.dt_sec);
	buff[PCF8583_REG_MIN]   = TOBCD(ymdhms.dt_min);
	buff[PCF8583_REG_HOUR]  = TOBCD(ymdhms.dt_hour);
	buff[PCF8583_REG_YEARDATE] = TOBCD(ymdhms.dt_day) |
	    ((ymdhms.dt_year % 4) << PCF8583_YEAR_SHIFT);
	buff[PCF8583_REG_WKDYMON] = TOBCD(ymdhms.dt_mon) |
	    ((ymdhms.dt_wday % 4) << PCF8583_WKDY_SHIFT);

	if (iic_control(sc->sc_dev.dv_parent,
			sc->sc_addr | IIC_WRITE, buff, 7))
		return EIO;

	if (cmos_write(RTC_ADDR_YEAR, ymdhms.dt_year % 100))
		return EIO;
	if (cmos_write(RTC_ADDR_CENT, ymdhms.dt_year / 100))
		return EIO;
	return 0;
}

void
inittodr(time_t base)
{
	int check;
	todr_chip_handle_t chip;
	struct timeval todrtime;

	check = 0;
	if (the_rtc == NULL) {
		printf("inittodr: rtc0 not present");
		time.tv_sec = base;
		time.tv_usec = 0;
		check = 1;
	} else {
		chip = &the_rtc->sc_ct;
		if (todr_gettime(chip, &todrtime) != 0) {
			printf("inittodr: Error reading clock");
			time.tv_sec = base;
			time.tv_usec = 0;
			check = 1;
		} else {
			time = todrtime;
			if (time.tv_sec > base + 3 * SECDAY) {
				printf("inittodr: Clock has gained %ld days",
				       (time.tv_sec - base) / SECDAY);
				check = 1;
			} else if (time.tv_sec + SECDAY < base) {
				printf("inittodr: Clock has lost %ld day(s)",
				       (base - time.tv_sec) / SECDAY);
				check = 1;
			}
		}
	}
	if (check)
		printf(" - CHECK AND RESET THE DATE.\n");
}


static int
rtc_gettime(todr_chip_handle_t handle, struct timeval *tv)
{
	u_char buff[8];
	int byte, centi;
	struct rtc_softc *sc = handle->cookie;
	struct clock_ymdhms ymdhms;
    
	buff[0] = 0;

	if (iic_control(sc->sc_dev.dv_parent, sc->sc_addr | IIC_WRITE,buff, 1))
		return EIO;

	if (iic_control(sc->sc_dev.dv_parent, sc->sc_addr | IIC_READ, buff, 8))
		return EIO;

	centi          = FROMBCD(buff[PCF8583_REG_CENTI]);
	ymdhms.dt_sec  = FROMBCD(buff[PCF8583_REG_SEC]);
	ymdhms.dt_min  = FROMBCD(buff[PCF8583_REG_MIN]);
	ymdhms.dt_hour = FROMBCD(buff[PCF8583_REG_HOUR] & PCF8583_HOUR_MASK);

	/* If in 12 hour mode need to look at the AM/PM flag */
	
	if (buff[PCF8583_REG_HOUR] & PCF8583_HOUR_12H) {
		ymdhms.dt_hour %= 12; /* 12AM -> 0, 12PM -> 12 */
		if (buff[PCF8583_REG_HOUR] & PCF8583_HOUR_PM)
			ymdhms.dt_hour += 12;
	}

	ymdhms.dt_day = FROMBCD(buff[PCF8583_REG_YEARDATE] &
				PCF8583_DATE_MASK);
	ymdhms.dt_mon = FROMBCD(buff[PCF8583_REG_WKDYMON] &
				PCF8583_MON_MASK);

	byte = cmos_read(RTC_ADDR_YEAR);
	if (byte == -1)
		return EIO;
	ymdhms.dt_year = byte;
	byte = cmos_read(RTC_ADDR_CENT);
	if (byte == -1)
		return EIO;
	ymdhms.dt_year += 100 * byte;

	/* Try to notice if the year's rolled over. */
	if (buff[PCF8583_REG_CSR] & PCF8583_CSR_MASK)
		printf("%s: cannot check year in mask mode\n",
		       sc->sc_dev.dv_xname);
	else
		while (ymdhms.dt_year % 4 !=
		       (buff[PCF8583_REG_YEARDATE] &
			PCF8583_YEAR_MASK) >> PCF8583_YEAR_SHIFT)
			ymdhms.dt_year++;
	
	tv->tv_sec = clock_ymdhms_to_secs(&ymdhms);
	tv->tv_usec = centi * 10000;
	return 0;
}

static int
rtc_getcal(todr_chip_handle_t handle, int *vp)
{

	return EOPNOTSUPP;
}

static int
rtc_setcal(todr_chip_handle_t handle, int v)
{

	return EOPNOTSUPP;
}

/* End of rtc.c */
