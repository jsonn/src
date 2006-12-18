/*	$NetBSD: zts.c,v 1.1.2.2 2006/12/18 11:42:09 yamt Exp $	*/
/* $OpenBSD: zts.c,v 1.9 2005/04/24 18:55:49 uwe Exp $ */

/*
 * Copyright (c) 2005 Dale Rahn <drahn@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: zts.c,v 1.1.2.2 2006/12/18 11:42:09 yamt Exp $");

#include "lcd.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/callout.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>
#include <dev/wscons/wsdisplayvar.h>

#include <arm/xscale/pxa2x0reg.h>
#include <arm/xscale/pxa2x0var.h>
#include <arm/xscale/pxa2x0_lcd.h>
#include <arm/xscale/pxa2x0_gpio.h>

#include <zaurus/dev/zsspvar.h>

/*
 * ADS784x touch screen controller
 */
#define ADSCTRL_PD0_SH          0       /* PD0 bit */
#define ADSCTRL_PD1_SH          1       /* PD1 bit */
#define ADSCTRL_DFR_SH          2       /* SER/DFR bit */
#define ADSCTRL_MOD_SH          3       /* Mode bit */
#define ADSCTRL_ADR_SH          4       /* Address setting */
#define ADSCTRL_STS_SH          7       /* Start bit */

#define GPIO_TP_INT_C3K		11
#define GPIO_HSYNC_C3K		22

#define POLL_TIMEOUT_RATE0	((hz * 150)/1000)
#define POLL_TIMEOUT_RATE1	(hz / 100) /* XXX every tick */

#define CCNT_HS_400_VGA_C3K 6250	/* 15.024us */

/* XXX need to ask zaurus_lcd.c for the screen dimension */
#define CURRENT_DISPLAY (&sharp_zaurus_C3000)
extern const struct lcd_panel_geometry sharp_zaurus_C3000;

/* Settable via sysctl. */
int	zts_rawmode;
struct ztsscale {
	int ts_minx;
	int ts_maxx;
	int ts_miny;
	int ts_maxy;
} zts_scale = {
	/* C3000 */
	209, 3620, 312, 3780
};

struct zts_softc {
	struct device sc_dev;
	struct callout sc_ts_poll;
	void *sc_gh;
	void *sc_powerhook;
	int sc_enabled;
	int sc_buttons; /* button emulation ? */
	struct device *sc_wsmousedev;
	int sc_oldx;
	int sc_oldy;
	int sc_resx;
	int sc_resy;
};

static int	zts_match(struct device *, struct cfdata *, void *);
static void	zts_attach(struct device *, struct device *, void *);

CFATTACH_DECL(zts, sizeof(struct zts_softc),
	zts_match, zts_attach, NULL, NULL);

static int	zts_enable(void *);
static void	zts_disable(void *);
static void	zts_power(int, void *);
static void	zts_poll(void *);
static int	zts_irq(void *);
static int	zts_ioctl(void *, u_long, caddr_t, int, struct lwp *);

const struct wsmouse_accessops zts_accessops = {
        zts_enable,
	zts_ioctl,
	zts_disable
};

static int
zts_match(struct device *parent, struct cfdata *cf, void *aux)
{

	return 1;
}

static void
zts_attach(struct device *parent, struct device *self, void *aux)
{
	struct zts_softc *sc = (struct zts_softc *)self;
	struct wsmousedev_attach_args a;  

	callout_init(&sc->sc_ts_poll);
	callout_setfunc(&sc->sc_ts_poll, zts_poll, sc);

	/* Initialize ADS7846 Difference Reference mode */
	(void)zssp_ic_send(ZSSP_IC_ADS7846,
	    (1<<ADSCTRL_ADR_SH) | (1<<ADSCTRL_STS_SH));
	delay(5000);
	(void)zssp_ic_send(ZSSP_IC_ADS7846,
	    (3<<ADSCTRL_ADR_SH) | (1<<ADSCTRL_STS_SH));
	delay(5000);
	(void)zssp_ic_send(ZSSP_IC_ADS7846,
	    (4<<ADSCTRL_ADR_SH) | (1<<ADSCTRL_STS_SH));
	delay(5000);
	(void)zssp_ic_send(ZSSP_IC_ADS7846,
	    (5<<ADSCTRL_ADR_SH) | (1<<ADSCTRL_STS_SH));
	delay(5000);

	a.accessops = &zts_accessops;
	a.accesscookie = sc;
	printf("\n");

#if NLCD > 0
	sc->sc_resx = CURRENT_DISPLAY->panel_height;
	sc->sc_resy = CURRENT_DISPLAY->panel_width;
#else
	sc->sc_resx = 480;	/* XXX */
	sc->sc_resy = 640;	/* XXX */
#endif

	sc->sc_wsmousedev = config_found(self, &a, wsmousedevprint);
}

static int
zts_enable(void *v)
{
	struct zts_softc *sc = (struct zts_softc *)v;

	if (sc->sc_enabled)
		return EBUSY;

	callout_stop(&sc->sc_ts_poll);

	sc->sc_powerhook = powerhook_establish(sc->sc_dev.dv_xname, zts_power,
	    sc);
	if (sc->sc_powerhook == NULL) {
		printf("%s: enable failed\n", sc->sc_dev.dv_xname);
		return ENOMEM;
	}

	pxa2x0_gpio_set_function(GPIO_TP_INT_C3K, GPIO_IN);

	/* XXX */
	if (sc->sc_gh == NULL) {
		sc->sc_gh = pxa2x0_gpio_intr_establish(GPIO_TP_INT_C3K,
		    IST_EDGE_FALLING, IPL_TTY, zts_irq, sc);
	} else {
		pxa2x0_gpio_intr_unmask(sc->sc_gh);
	}

	/* enable interrupts */
	sc->sc_enabled = 1;
	sc->sc_buttons = 0;

	return 0;
}

static void
zts_disable(void *v)
{
	struct zts_softc *sc = (struct zts_softc *)v;

	callout_stop(&sc->sc_ts_poll);

	if (sc->sc_powerhook != NULL) {
		powerhook_disestablish(sc->sc_powerhook);
		sc->sc_powerhook = NULL;
	}

	if (sc->sc_gh != NULL) {
#if 0
		pxa2x0_gpio_intr_disestablish(sc->sc_gh);
		sc->sc_gh = NULL;
#endif
	}

	/* disable interrupts */
	sc->sc_enabled = 0;
}

static void
zts_power(int why, void *v)
{
	struct zts_softc *sc = (struct zts_softc *)v;

	switch (why) {
	case PWR_STANDBY:
	case PWR_SUSPEND:
		sc->sc_enabled = 0;
#if 0
		pxa2x0_gpio_intr_disestablish(sc->sc_gh);
#endif
		callout_stop(&sc->sc_ts_poll);

		pxa2x0_gpio_intr_mask(sc->sc_gh);

		/* Turn off reference voltage but leave ADC on. */
		(void)zssp_ic_send(ZSSP_IC_ADS7846, (1 << ADSCTRL_PD1_SH) |
		    (1 << ADSCTRL_ADR_SH) | (1 << ADSCTRL_STS_SH));

		pxa2x0_gpio_set_function(GPIO_TP_INT_C3K,
		    GPIO_OUT | GPIO_SET);
		break;

	case PWR_RESUME:
		pxa2x0_gpio_set_function(GPIO_TP_INT_C3K, GPIO_IN);
		pxa2x0_gpio_intr_mask(sc->sc_gh);

		/* Enable automatic low power mode. */
		(void)zssp_ic_send(ZSSP_IC_ADS7846,
		    (4 << ADSCTRL_ADR_SH) | (1 << ADSCTRL_STS_SH));

#if 0
		sc->sc_gh = pxa2x0_gpio_intr_establish(GPIO_TP_INT_C3K,
		    IST_EDGE_FALLING, IPL_TTY, zts_irq, sc,
		    sc->sc_dev.dv_xname);
#else
		pxa2x0_gpio_intr_unmask(sc->sc_gh);
#endif
		sc->sc_enabled = 1;
		break;
	}
}

struct zts_pos {
	int x;
	int y;
	int z;			/* touch pressure */
};

#define NSAMPLES 3
static struct zts_pos zts_samples[NSAMPLES];
static int	ztsavgloaded = 0;

static int	zts_readpos(struct zts_pos *);
static void	zts_avgpos(struct zts_pos *);

#define HSYNC()								\
	do {								\
		while (pxa2x0_gpio_get_bit(GPIO_HSYNC_C3K) == 0)	\
			continue;					\
		while (pxa2x0_gpio_get_bit(GPIO_HSYNC_C3K) != 0)	\
			continue;					\
	} while (/*CONSTCOND*/0)

static int	pxa2x0_ccnt_enable(int);
static uint32_t	pxa2x0_read_ccnt(void);
static uint32_t	zts_sync_ads784x(int, int, uint32_t);
static void	zts_sync_send(uint32_t);

static int
pxa2x0_ccnt_enable(int on)
{
	uint32_t rv;

	on = on ? 0x1 : 0x0;
	__asm volatile("mrc p14, 0, %0, c0, c1, 0" : "=r" (rv));
	__asm volatile("mcr p14, 0, %0, c0, c1, 0" : : "r" (on));
	return ((int)(rv & 0x1));
}

static uint32_t
pxa2x0_read_ccnt(void)
{
	uint32_t rv;

	__asm volatile("mrc p14, 0, %0, c1, c1, 0" : "=r" (rv));
	return rv;
}

/*
 * Communicate synchronously with the ADS784x touch screen controller.
 */
static uint32_t
zts_sync_ads784x(int dorecv/* XXX */, int dosend/* XXX */, uint32_t cmd)
{
	int ccen;
	uint32_t rv;

	/* XXX poll hsync only if LCD is enabled */

	/* start clock counter */
	ccen = pxa2x0_ccnt_enable(1);

	HSYNC();

	if (dorecv) {
		/* read SSDR and disable ADS784x */
		rv = zssp_ic_stop(ZSSP_IC_ADS7846);
	} else {
		rv = 0;
	}

	if (dosend)
		zts_sync_send(cmd);

	/* stop clock counter */
	pxa2x0_ccnt_enable(ccen);

	return rv;
}

void
zts_sync_send(uint32_t cmd)
{
	uint32_t tck;
	uint32_t a, b;

	/* XXX */
	tck = CCNT_HS_400_VGA_C3K - 151;

	/* send dummy command; discard SSDR */
	(void)zssp_ic_send(ZSSP_IC_ADS7846, cmd);

	/* wait for refresh */
	HSYNC();

	/* wait after refresh */
	a = pxa2x0_read_ccnt();
	b = pxa2x0_read_ccnt();
	while ((b - a) < tck)
		b = pxa2x0_read_ccnt();

	/* send the actual command; keep ADS784x enabled */
	zssp_ic_start(ZSSP_IC_ADS7846, cmd);
}

static int
zts_readpos(struct zts_pos *pos)
{
	int cmd;
	int t0, t1;
	int down;

	/* XXX */
	pxa2x0_gpio_set_function(GPIO_HSYNC_C3K, GPIO_IN);

	/* check that pen is down */
	cmd = (1 << ADSCTRL_PD0_SH) | (1 << ADSCTRL_PD1_SH) |
	    (3 << ADSCTRL_ADR_SH) | (1 << ADSCTRL_STS_SH);

	t0 = zssp_ic_send(ZSSP_IC_ADS7846, cmd);
	down = !(t0 < 10);
	if (down == 0)
		goto out;

	/* Y */
	cmd = (1 << ADSCTRL_PD0_SH) | (1 << ADSCTRL_PD1_SH) |
	    (1 << ADSCTRL_ADR_SH) | (1 << ADSCTRL_STS_SH);

	(void)zts_sync_ads784x(0, 1, cmd);

	/* Y */
	cmd = (1 << ADSCTRL_PD0_SH) | (1 << ADSCTRL_PD1_SH) |
	    (1 << ADSCTRL_ADR_SH) | (1 << ADSCTRL_STS_SH);

	(void)zts_sync_ads784x(1, 1, cmd);

	/* X */
	cmd = (1 << ADSCTRL_PD0_SH) | (1 << ADSCTRL_PD1_SH) |
	    (5 << ADSCTRL_ADR_SH) | (1 << ADSCTRL_STS_SH);

	pos->y = zts_sync_ads784x(1, 1, cmd);

	/* T0 */
	cmd = (1 << ADSCTRL_PD0_SH) | (1 << ADSCTRL_PD1_SH) |
	    (3 << ADSCTRL_ADR_SH) | (1 << ADSCTRL_STS_SH);

	pos->x = zts_sync_ads784x(1, 1, cmd);

	/* T1 */
	cmd = (1 << ADSCTRL_PD0_SH) | (1 << ADSCTRL_PD1_SH) |
	    (4 << ADSCTRL_ADR_SH) | (1 << ADSCTRL_STS_SH);

	t0 = zts_sync_ads784x(1, 1, cmd);
	t1 = zts_sync_ads784x(1, 0, cmd);

	/* check that pen is still down */
	/* XXX pressure sensitivity varies with X or what? */
	if (t0 == 0 || (pos->x * (t1 - t0) / t0) >= 15000)
		down = 0;
	pos->z = down;

out:
	/* Enable automatic low power mode. */
        cmd = (4 << ADSCTRL_ADR_SH) | (1 << ADSCTRL_STS_SH);
	(void)zssp_ic_send(ZSSP_IC_ADS7846, cmd);

	return down;
}

#define NAVGSAMPLES (NSAMPLES < 3 ? NSAMPLES : 3)

static void
zts_avgpos(struct zts_pos *pos)
{
	struct zts_pos *tpp = zts_samples;
	int diff[NAVGSAMPLES];
	int mindiff, mindiffv;
	int n;
	int i;
	static int tail;

	if (ztsavgloaded < NAVGSAMPLES) {
		tpp[(tail + ztsavgloaded) % NSAMPLES] = *pos;
		ztsavgloaded++;
		return;
	}

	tpp[tail] = *pos;
	tail = (tail+1) % NSAMPLES;

	/* X */
	i = tail;
	for (n = 0 ; n < NAVGSAMPLES; n++) {
		int alt;
		alt = (i+1) % NSAMPLES;
		diff[n] = tpp[i].x - tpp[alt].x;
		if (diff[n] < 0)
			diff[n] = -diff[n]; /* ABS */
		i = alt;
	}
	mindiffv = diff[0];
	mindiff = 0;
	for (n = 1; n < NAVGSAMPLES; n++) {
		if (diff[n] < mindiffv) {
			mindiffv = diff[n];
			mindiff = n;
		}
	}
	pos->x = (tpp[(tail + mindiff) % NSAMPLES].x +
	    tpp[(tail + mindiff + 1) % NSAMPLES].x) / 2;

	/* Y */
	i = tail;
	for (n = 0 ; n < NAVGSAMPLES; n++) {
		int alt;
		alt = (i+1) % NSAMPLES;
		diff[n] = tpp[i].y - tpp[alt].y;
		if (diff[n] < 0)
			diff[n] = -diff[n]; /* ABS */
		i = alt;
	}
	mindiffv = diff[0];
	mindiff = 0;
	for (n = 1; n < NAVGSAMPLES; n++) {
		if (diff[n] < mindiffv) {
			mindiffv = diff[n];
			mindiff = n;
		}
	}
	pos->y = (tpp[(tail + mindiff) % NSAMPLES].y +
	    tpp[(tail + mindiff + 1) % NSAMPLES].y) / 2;
}

static void
zts_poll(void *v)
{
	int s;

	s = spltty();
	(void)zts_irq(v);
	splx(s);
}

#define TS_STABLE 8
static int
zts_irq(void *v)
{
	struct zts_softc *sc = (struct zts_softc *)v;
	struct zts_pos tp = { 0, };
	int s;
	int pindown;
	int down;
	extern int zkbd_modstate;

	if (!sc->sc_enabled)
		return 0;

	s = splhigh();
	pindown = pxa2x0_gpio_get_bit(GPIO_TP_INT_C3K) ? 0 : 1;
	if (pindown) {
		pxa2x0_gpio_intr_mask(sc->sc_gh);
		callout_schedule(&sc->sc_ts_poll, POLL_TIMEOUT_RATE1);
	}

	down = zts_readpos(&tp);

	if (!pindown) {
		pxa2x0_gpio_intr_unmask(sc->sc_gh);
		callout_schedule(&sc->sc_ts_poll, POLL_TIMEOUT_RATE0);
		ztsavgloaded = 0;
	}
	pxa2x0_gpio_clear_intr(GPIO_TP_INT_C3K);
	splx(s);
	
	if (down) {
		zts_avgpos(&tp);
		if (!zts_rawmode) {
			struct ztsscale *tsp = &zts_scale;

			/* Scale down to the screen resolution. */
			tp.x = ((tp.x - tsp->ts_minx) * sc->sc_resx) /
			    (tsp->ts_maxx - tsp->ts_minx);
			tp.y = ((tp.y - tsp->ts_miny) * sc->sc_resy) /
			    (tsp->ts_maxy - tsp->ts_miny);
		}
	}

	if (zkbd_modstate != 0 && down) {
		if(zkbd_modstate & (1 << 1)) {
			/* Fn */
			down = 2;
		}
		if(zkbd_modstate & (1 << 2)) {
			/* 'Alt' */
			down = 4;
		}
	}
	if (!down) {
		/* x/y values are not reliable when pen is up */
		tp.x = sc->sc_oldx;
		tp.y = sc->sc_oldy;
	}

	if (down || sc->sc_buttons != down) {
		wsmouse_input(sc->sc_wsmousedev, down, tp.x, tp.y, 0 /* z */,
		    WSMOUSE_INPUT_ABSOLUTE_X | WSMOUSE_INPUT_ABSOLUTE_Y |
		    WSMOUSE_INPUT_ABSOLUTE_Z);
		sc->sc_buttons = down;
		sc->sc_oldx = tp.x;
		sc->sc_oldy = tp.y;
	}

	return 1;
}

static int
zts_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct lwp *p)
{

	switch (cmd) {
	case WSMOUSEIO_GTYPE:
		*(u_int *)data = WSMOUSE_TYPE_TPANEL;
		return 0;
	}
	return -1;
}
