/*	$NetBSD: rtc.c,v 1.1.1.1.8.1 1999/12/27 18:32:15 wrstuden Exp $	*/

/*-
 * Copyright (c) 1999 Shin Takemura. All rights reserved.
 * Copyright (c) 1999 SATO Kazumi. All rights reserved.
 * Copyright (c) 1999 PocketBSD Project. All rights reserved.
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
 *	This product includes software developed by the PocketBSD project
 *	and its contributors.
 * 4. Neither the name of the project nor the names of its contributors
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>

#include <machine/bus.h>
#include <machine/clock_machdep.h>
#include <machine/cpu.h>

#include <hpcmips/vr/vr.h>
#include <hpcmips/vr/vripvar.h>
#include <hpcmips/vr/rtcreg.h>
#include <dev/dec/clockvar.h>

#if 0
#define RTCDEBUG	/* rtc debugging infomation */
#define RTC_HEARTBEAT	/* HEARTBEAT print */
#define RECALC_CPUSPEED	/* cpuspeed recalculaton */
#define RECALC_CPUSPEED_DEBUG	/* XXX */
#endif


struct vrrtc_softc {
	struct device sc_dev;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	void *sc_ih;
};

void	clock_init __P((struct device *));
void	clock_get __P((struct device *, time_t, struct clocktime *));
void	clock_set __P((struct device *, struct clocktime *));

static const struct clockfns clockfns = {
	clock_init, clock_get, clock_set,
};

int	vrrtc_match __P((struct device *, struct cfdata *, void *));
void	vrrtc_attach __P((struct device *, struct device *, void *));
int	vrrtc_intr __P((void*, u_int32_t, u_int32_t));

struct cfattach vrrtc_ca = {
	sizeof(struct vrrtc_softc), vrrtc_match, vrrtc_attach
};

void	vrrtc_write __P((struct vrrtc_softc *, int, unsigned short));
unsigned short	vrrtc_read __P((struct vrrtc_softc *, int));
void	cvt_timehl_ct __P((u_long, u_long, struct clocktime *));
int	vrrtc_recalc_cpuspeed __P((struct device *));


extern int rtc_offset;

int
vrrtc_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	return(1);
}

inline void
vrrtc_write(sc, port, val)
	struct vrrtc_softc *sc;
	int port;
	unsigned short val;
{
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, port, val);
}

inline unsigned short
vrrtc_read(sc, port)
	struct vrrtc_softc *sc;
	int port;
{
	return bus_space_read_2(sc->sc_iot, sc->sc_ioh, port);
}

void
vrrtc_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct vrip_attach_args *va = aux;
	struct vrrtc_softc *sc = (void*)self;
    
	sc->sc_iot = va->va_iot;
	if (bus_space_map(sc->sc_iot, va->va_addr, va->va_size,
			  0 /* no flags */, &sc->sc_ioh)) {
		printf("vrrtc_attach: can't map i/o space\n");
		return;
	}
	/* RTC interrupt handler is directly dispatched from CPU intr */
	vr_intr_establish(VR_INTR1, vrrtc_intr, sc);
	/* But need to set level 1 interupt mask register, 
	 * so regsiter fake interrurpt handler
	 */
	if (!(sc->sc_ih = vrip_intr_establish(va->va_vc, va->va_intr, 
						IPL_CLOCK, 0, 0))) {
		printf (":can't map interrupt.\n");
		return;
	}
	/*
	 *  Rtc is attached to call this routine
	 *  before cpu_initclock() calls clock_init().
	 *  So we must disable all interrupt for now.
	 */
	/*
	 * Disable all rtc interrupts
	 */
	/* Disable Elapse compare intr */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, ECMP_H_REG_W, 0);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, ECMP_M_REG_W, 0);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, ECMP_L_REG_W, 0);
	/* Disable RTC Long1 intr */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, RTCL1_H_REG_W, 0);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, RTCL1_L_REG_W, 0);
	/* Disable RTC Long2 intr */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, RTCL2_H_REG_W, 0);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, RTCL2_L_REG_W, 0);
	/* Disable RTC TCLK intr */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, TCLK_H_REG_W, 0);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, TCLK_L_REG_W, 0);
	/*
	 * Clear all rtc intrrupts.
	 */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, RTCINT_REG_W, RTCINT_ALL);

	clockattach(&sc->sc_dev, &clockfns);
}

int
vrrtc_intr(arg, pc, statusReg)
        void *arg;
	u_int32_t pc;
	u_int32_t statusReg;
{
	struct vrrtc_softc *sc = arg;
	struct clockframe cf;
    
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, RTCINT_REG_W, RTCINT_ALL);
	cf.pc = pc;
	cf.sr = statusReg;
	hardclock(&cf);
	intrcnt[HARDCLOCK]++;

#ifdef RTC_HEARTBEAT
	if ((intrcnt[HARDCLOCK] % (CLOCK_RATE * 5)) == 0) {
		struct clocktime ct;
		clock_get((struct device *)sc, NULL, &ct);
		printf("%s(%d): rtc_intr: %2d.%2d.%2d %02d:%02d:%02d\n",
		       __FILE__, __LINE__,
		       ct.year, ct.mon, ct.day,
		       ct.hour, ct.min, ct.sec);
	}
#endif
	return 0;
}


int
vrrtc_recalc_cpuspeed(dev)
	struct device *dev;
{
	struct vrrtc_softc *sc = (struct vrrtc_softc *)dev;
	u_long otimeh;
	u_long otimel;
	u_long timeh;
	u_long timel;

	otimeh = bus_space_read_2(sc->sc_iot, sc->sc_ioh, ETIME_H_REG_W);
	otimel = bus_space_read_2(sc->sc_iot, sc->sc_ioh, ETIME_M_REG_W);
	otimel = (otimel << 16) 
		| bus_space_read_2(sc->sc_iot, sc->sc_ioh, ETIME_L_REG_W);

#define MSEC 1000
	/* wait 1msec */
	DELAY(MSEC);

	timeh = bus_space_read_2(sc->sc_iot, sc->sc_ioh, ETIME_H_REG_W);
	timel = bus_space_read_2(sc->sc_iot, sc->sc_ioh, ETIME_M_REG_W);
	timel = (timel << 16) 
		| bus_space_read_2(sc->sc_iot, sc->sc_ioh, ETIME_L_REG_W);

	if (timeh-otimeh > 0){
		/* cpuspeed is too large (> 2 sec)*/
		cpuspeed = cpuspeed/((timeh-otimeh)*2*MSEC);
		cpuspeed +=1;
		return 0;
	}
	if (timel-otimel < (ETIME_L_HZ/MSEC/10)) {
		/* cpuspeed is too small (< 0.1msec) */ 
		cpuspeed *=10;
		return -1;
	}
	cpuspeed = cpuspeed * (ETIME_L_HZ/MSEC) / (timel-otimel);
	return 0;
}

void
clock_init(dev)
	struct device *dev;
{
	struct vrrtc_softc *sc = (struct vrrtc_softc *)dev;
#ifdef RTCDEBUG
	int timeh;
	int timel;
#endif /* RTCDEBUG */
#ifdef RECALC_CPUSPEED
	int maxrecalc = 3;
#endif /* RECALC_CPUSPEED */

#ifdef RTCDEBUG
	timeh = bus_space_read_2(sc->sc_iot, sc->sc_ioh, ETIME_H_REG_W);
	timel = bus_space_read_2(sc->sc_iot, sc->sc_ioh, ETIME_M_REG_W);
	timel = (timel << 16) 
		| bus_space_read_2(sc->sc_iot, sc->sc_ioh, ETIME_L_REG_W);
	printf("clock_init()  Elapse Time %04x%04x\n", timeh, timel);

	timeh = bus_space_read_2(sc->sc_iot, sc->sc_ioh, ECMP_H_REG_W);
	timel = bus_space_read_2(sc->sc_iot, sc->sc_ioh, ECMP_M_REG_W);
	timel = (timel << 16) 
		| bus_space_read_2(sc->sc_iot, sc->sc_ioh, ECMP_L_REG_W);
	printf("clock_init()  Elapse Compare %04x%04x\n", timeh, timel);

	timeh = bus_space_read_2(sc->sc_iot, sc->sc_ioh, RTCL1_H_REG_W);
	timel = bus_space_read_2(sc->sc_iot, sc->sc_ioh, RTCL1_L_REG_W);
	printf("clock_init()  LONG1 %04x%04x\n", timeh, timel);

	timeh = bus_space_read_2(sc->sc_iot, sc->sc_ioh, RTCL1_CNT_H_REG_W);
	timel = bus_space_read_2(sc->sc_iot, sc->sc_ioh, RTCL1_CNT_L_REG_W);
	printf("clock_init()  LONG1 CNTL %04x%04x\n", timeh, timel);

	timeh = bus_space_read_2(sc->sc_iot, sc->sc_ioh, RTCL2_H_REG_W);
	timel = bus_space_read_2(sc->sc_iot, sc->sc_ioh, RTCL2_L_REG_W);
	printf("clock_init()  LONG2 %04x%04x\n", timeh, timel);

	timeh = bus_space_read_2(sc->sc_iot, sc->sc_ioh, RTCL2_CNT_H_REG_W);
	timel = bus_space_read_2(sc->sc_iot, sc->sc_ioh, RTCL2_CNT_L_REG_W);
	printf("clock_init()  LONG2 CNTL %04x%04x\n", timeh, timel);

	timeh = bus_space_read_2(sc->sc_iot, sc->sc_ioh, TCLK_H_REG_W);
	timel = bus_space_read_2(sc->sc_iot, sc->sc_ioh, TCLK_L_REG_W);
	printf("clock_init()  TCLK %04x%04x\n", timeh, timel);

	timeh = bus_space_read_2(sc->sc_iot, sc->sc_ioh, TCLK_CNT_H_REG_W);
	timel = bus_space_read_2(sc->sc_iot, sc->sc_ioh, TCLK_CNT_L_REG_W);
	printf("clock_init()  TCLK CNTL %04x%04x\n", timeh, timel);
#endif /* RTCDEBUG */
	/*
	 * Set tick (CLOCK_RATE)
	 */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, RTCL1_H_REG_W, 0);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, 
			  RTCL1_L_REG_W, RTCL1_L_HZ/CLOCK_RATE);

#ifdef RECALC_CPUSPEED
	/* calcurate cpu speed */
	while (maxrecalc-- > 0 && vrrtc_recalc_cpuspeed(dev))
		;
#ifdef RECALC_CPUSPEED_DEBUG
	printf("clock_init() cpuspeed = %d\n", cpuspeed);
#endif /* RECALC_CPUSPEED_DEBUG */
#endif /* RECALC_CPUSPEED */
}

static int m2d[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

void
cvt_timehl_ct(timeh, timel, ct)
	u_long timeh; /* 2 sec */
	u_long timel; /* 1/32768 sec */
	struct clocktime *ct;
{
	u_long year, month, date, hour, min, sec, sec2;

	timeh -= EPOCHOFF;

	timeh += (rtc_offset*SEC2MIN);

	year = EPOCHYEAR;
	sec2 = LEAPYEAR4(year)?SEC2YR+SEC2DAY:SEC2YR;
	while (timeh > sec2) {
		year++;
		timeh -= sec2;
		sec2 = LEAPYEAR4(year)?SEC2YR+SEC2DAY:SEC2YR;
	}

#ifdef RTCDEBUG
	printf("cvt_timehl_ct: timeh %08lx year %ld yrref %ld\n", 
		timeh, year, sec2);
#endif /* RTCDEBUG */

	month = 0; /* now month is 0..11 */
	sec2 = SEC2DAY * m2d[month];
	while (timeh > sec2) {
		timeh -= sec2;
		month++;
		sec2 = SEC2DAY * m2d[month];
		if (month == 1 && LEAPYEAR4(year)) /* feb. and leapyear */
			sec2 += SEC2DAY;
	}
	month +=1; /* now month is 1..12 */

#ifdef RTCDEBUG
	printf("cvt_timehl_ct: timeh %08lx month %ld mref %ld\n", 
		timeh, month, sec2);
#endif /* RTCDEBUG */

	sec2 = SEC2DAY;
	date = timeh/sec2+1; /* date is 1..31 */
	timeh -= (date-1)*sec2;

#ifdef RTCDEBUG
	printf("cvt_timehl_ct: timeh %08lx date %ld dref %ld\n", 
		timeh, date, sec2);
#endif /* RTCDEBUG */

	sec2 = SEC2HOUR;
	hour = timeh/sec2;
	timeh -= hour*sec2;

	sec2 = SEC2MIN;
	min = timeh/sec2;
	timeh -= min*sec2;

	sec = timeh*2 + timel/ETIME_L_HZ;	

#ifdef RTCDEBUG
	printf("cvt_timehl_ct: hour %ld min %ld sec %ld\n", hour, min, sec);
#endif /* RTCDEBUG */

	if (ct) {
		ct->year = year - YBASE; /* base 1900 */
		ct->mon = month;
		ct->day = date;
		ct->hour = hour;
		ct->min = min;
		ct->sec = sec;
	}
}

void
clock_get(dev, base, ct)
	struct device *dev;
	time_t base;
	struct clocktime *ct;
{

	struct vrrtc_softc *sc = (struct vrrtc_softc *)dev;
	u_long timeh;	/* elapse time (2*timeh sec) */
	u_long timel;	/* timel/32768 sec */

	timeh = bus_space_read_2(sc->sc_iot, sc->sc_ioh, ETIME_H_REG_W);
	timeh = (timeh << 16) 
		| bus_space_read_2(sc->sc_iot, sc->sc_ioh, ETIME_M_REG_W);
	timel = bus_space_read_2(sc->sc_iot, sc->sc_ioh, ETIME_L_REG_W);

#ifdef RTCDEBUG
	printf("clock_get: timeh %08lx timel %08lx\n", timeh, timel);
#endif /* RTCDEBUG */

	cvt_timehl_ct(timeh, timel, ct);

#ifdef RTCDEBUG
	printf("clock_get: %d/%d/%d/%d/%d/%d\n",
		 ct->year, ct->mon, ct->day, ct->hour, ct->min, ct->sec);
#endif /* RTCDEBUG */
}


void
clock_set(dev, ct)
	struct device *dev;
	struct clocktime *ct;
{
	struct vrrtc_softc *sc = (struct vrrtc_softc *)dev;
	u_long timeh;	/* elapse time (2*timeh sec) */
	u_long timel;	/* timel/32768 sec */
	int year, month, sec2;

	timeh = 0;
	timel = 0;

#ifdef RTCDEBUG
	printf("clock_set: %d/%d/%d/%d/%d/%d\n", 
		ct->year, ct->mon, ct->day, ct->hour, ct->min, ct->sec);
#endif /* RTCDEBUG */
	ct->year += YBASE;
#ifdef RTCDEBUG
	printf("clock_set: %d/%d/%d/%d/%d/%d\n", 
		ct->year, ct->mon, ct->day, ct->hour, ct->min, ct->sec);
#endif /* RTCDEBUG */
	year = EPOCHYEAR;
	sec2 = LEAPYEAR4(year)?SEC2YR+SEC2DAY:SEC2YR;
	while (year < ct->year) {
		year++;
		timeh += sec2;
		sec2 = LEAPYEAR4(year)?SEC2YR+SEC2DAY:SEC2YR;
	}
	month = 1; /* now month is 1..12 */
	sec2 = SEC2DAY * m2d[month-1];
	while (month < ct->mon) {
		month++;
		timeh += sec2;
		sec2 = SEC2DAY * m2d[month-1];
		if (month == 2 && LEAPYEAR4(year)) /* feb. and leapyear */
			sec2 += SEC2DAY;
	}

	timeh += (ct->day - 1)*SEC2DAY;

	timeh += ct->hour*SEC2HOUR;

	timeh += ct->min*SEC2MIN;

	timeh += ct->sec/2;
	timel += (ct->sec%2)*ETIME_L_HZ;

	timeh += EPOCHOFF;
	timeh -= (rtc_offset*SEC2MIN);

#ifdef RTCDEBUG
	cvt_timehl_ct(timeh, timel, NULL);
#endif /* RTCDEBUG */

	bus_space_write_2(sc->sc_iot, sc->sc_ioh, 
			  ETIME_H_REG_W, (timeh>>16)&0xffff);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, ETIME_M_REG_W, timeh&0xffff);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, ETIME_L_REG_W, timel);

}

