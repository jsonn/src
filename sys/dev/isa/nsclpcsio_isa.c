/* $NetBSD: nsclpcsio_isa.c,v 1.1.6.3 2002/10/10 18:39:46 jdolecek Exp $ */

/*
 * Copyright (c) 2002
 * 	Matthias Drochner.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: nsclpcsio_isa.c,v 1.1.6.3 2002/10/10 18:39:46 jdolecek Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <machine/bus.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/sysmon/sysmonvar.h>

static int nsclpcsio_isa_match __P((struct device *, struct cfdata *, void *));
static void nsclpcsio_isa_attach __P((struct device *, struct device *,
				      void *));

struct nsclpcsio_softc {
	struct device sc_dev;
	bus_space_tag_t sc_iot, sc_tms_iot;
	bus_space_handle_t sc_ioh, sc_tms_ioh;

	struct envsys_tre_data sc_data[3];
	struct envsys_basic_info sc_info[3];
	struct sysmon_envsys sc_sysmon;
};

CFATTACH_DECL(nsclpcsio_isa, sizeof(struct nsclpcsio_isa_softc),
    nsclpcsio_isa_match, nsclpcsio_isa_attach, NULL, NULL);
};

static const struct envsys_range tms_ranges[] = {
	{ 0, 2, ENVSYS_STEMP },
};

static u_int8_t nsread(bus_space_tag_t, bus_space_handle_t, int);
static void nswrite(bus_space_tag_t, bus_space_handle_t, int, u_int8_t);
static int nscheck(bus_space_tag_t, int);

static void tms_update(struct nsclpcsio_softc *, int);
static int tms_gtredata(struct sysmon_envsys *, struct envsys_tre_data *);
static int tms_streinfo(struct sysmon_envsys *, struct envsys_basic_info *);

static u_int8_t
nsread(iot, ioh, idx)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int idx;
{

	bus_space_write_1(iot, ioh, 0, idx);
	return (bus_space_read_1(iot, ioh, 1));
}

static void
nswrite(iot, ioh, idx, data)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int idx;
	u_int8_t data;
{

	bus_space_write_1(iot, ioh, 0, idx);
	bus_space_write_1(iot, ioh, 1, data);
}

static int
nscheck(iot, base)
	bus_space_tag_t iot;
	int base;
{
	bus_space_handle_t ioh;
	int rv = 0;

	if (bus_space_map(iot, base, 2, 0, &ioh))
		return (0);

	/* XXX this is for PC87366 only for now */
	if (nsread(iot, ioh, 0x20) == 0xe9)
		rv = 1;

	bus_space_unmap(iot, ioh, 2);
	return (rv);
}

static int
nsclpcsio_isa_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	int iobase;

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	if (ia->ia_nio > 0 && ia->ia_io[0].ir_addr != ISACF_PORT_DEFAULT) {
		/* XXX check for legal iobase ??? */
		if (nscheck(ia->ia_iot, ia->ia_io[0].ir_addr)) {
			iobase = ia->ia_io[0].ir_addr;
			goto found;
		}
	}

	/* PC87366 has two possible locations depending on wiring */
	if (nscheck(ia->ia_iot, 0x2e)) {
		iobase = 0x2e;
		goto found;
	}
	if (nscheck(ia->ia_iot, 0x4e)) {
		iobase = 0x4e;
		goto found;
	}
	return (0);

found:
	ia->ia_nio = 1;
	ia->ia_io[0].ir_addr = iobase;
	ia->ia_io[0].ir_size = 2;
	ia->ia_niomem = 0;
	ia->ia_nirq = 0;
	ia->ia_ndrq = 0;
	return (1);
}

static void
nsclpcsio_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct nsclpcsio_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int8_t val;
	int tms_iobase;
	int i;

	sc->sc_iot = iot = ia->ia_iot;
	if (bus_space_map(ia->ia_iot, ia->ia_io[0].ir_addr, 2, 0, &ioh)) {
		printf(": can't map i/o space\n");
		return;
	}
	sc->sc_ioh = ioh;
	printf(": NSC PC87366 rev. %d\n", nsread(iot, ioh, 0x27));

	nswrite(iot, ioh, 0x07, 0x0e); /* select tms */

	val = nsread(iot, ioh, 0x30); /* control register */
	if (!(val & 1)) {
		printf("%s: TMS disabled\n", sc->sc_dev.dv_xname);
		return;
	}

	tms_iobase = (nsread(iot, ioh, 0x60) << 8) | nsread(iot, ioh, 0x61);
	sc->sc_tms_iot = iot;
	if (bus_space_map(iot, tms_iobase, 16, 0, &sc->sc_tms_ioh)) {
		printf("%s: can't map TMS i/o space\n", sc->sc_dev.dv_xname);
		return;
	}
	printf("%s: TMS at 0x%x\n", sc->sc_dev.dv_xname, tms_iobase);

	if (bus_space_read_1(sc->sc_tms_iot, sc->sc_tms_ioh, 0x08) & 1) {
		printf("%s: TMS in standby mode\n", sc->sc_dev.dv_xname);
		/* XXX awake it ??? */
		return;
	}

	/* Initialize sensor meta data */
	for (i = 0; i < 3; i++) {
		sc->sc_data[i].sensor = sc->sc_info[i].sensor = i;
		sc->sc_data[i].units = sc->sc_info[i].units = ENVSYS_STEMP;
	}
	strcpy(sc->sc_info[0].desc, "TSENS1");
	strcpy(sc->sc_info[1].desc, "TSENS2");
	strcpy(sc->sc_info[2].desc, "TNSC");

	/* Get initial set of sensor values. */
	for (i = 0; i < 3; i++)
		tms_update(sc, i);

	/*
	 * Hook into the System Monitor.
	 */
	sc->sc_sysmon.sme_ranges = tms_ranges;
	sc->sc_sysmon.sme_sensor_info = sc->sc_info;
	sc->sc_sysmon.sme_sensor_data = sc->sc_data;
	sc->sc_sysmon.sme_cookie = sc;

	sc->sc_sysmon.sme_gtredata = tms_gtredata;
	sc->sc_sysmon.sme_streinfo = tms_streinfo;

	sc->sc_sysmon.sme_nsensors = 3;
	sc->sc_sysmon.sme_envsys_version = 1000;

	if (sysmon_envsys_register(&sc->sc_sysmon))
		printf("%s: unable to register with sysmon\n",
		    sc->sc_dev.dv_xname);
}

static void
tms_update(sc, chan)
	struct nsclpcsio_softc *sc;
	int chan;
{
	bus_space_tag_t iot = sc->sc_tms_iot;
	bus_space_handle_t ioh = sc->sc_tms_ioh;
	u_int8_t status;
	int8_t temp, ctemp; /* signed!! */

	bus_space_write_1(iot, ioh, 0x09, chan); /* select */

	status = bus_space_read_1(iot, ioh, 0x0a); /* config/status */
	if (status & 0x01) {
		/* enabled */
		sc->sc_info[chan].validflags = ENVSYS_FVALID;
	}else {
		sc->sc_info[chan].validflags = 0;
		return;
	}

	/*
	 * If the channel is enabled, it is considered valid.
	 * An "open circuit" might be temporary.
	 */
	sc->sc_data[chan].validflags = ENVSYS_FVALID;
	if (status & 0x40) {
		/*
		 * open circuit
		 * XXX should have a warning for it
		 */
		sc->sc_data[chan].warnflags = ENVSYS_WARN_OK; /* XXX */
		return;
	}

	/* get current temperature in signed degree celsius */
	temp = bus_space_read_1(iot, ioh, 0x0b);
	sc->sc_data[chan].cur.data_us = (int)temp * 1000000 + 273150000;
	sc->sc_data[chan].validflags |= ENVSYS_FCURVALID;

	if (status & 0x0e) { /* any temperature warning? */
		/*
		 * XXX the chip documentation is a bit fuzzy - it doesn't state
		 * that the hardware OTS output depends on the "overtemp"
		 * warning bit.
		 * It seems the output gets cleared if the warning bit is reset.
		 * This sucks.
		 * The hardware might do something useful with output pins, eg
		 * throttling the CPU, so we must do the comparision in
		 * software, and only reset the bits if the reason is gone.
		 */
		if (status & 0x02) { /* low limit */
			sc->sc_data[chan].warnflags = ENVSYS_WARN_UNDER;
			/* read low limit */
			ctemp = bus_space_read_1(iot, ioh, 0x0d);
			if (temp <= ctemp) /* still valid, don't reset */
				status &= ~0x02;
		}
		if (status & 0x04) { /* high limit */
			sc->sc_data[chan].warnflags = ENVSYS_WARN_OVER;
			/* read high limit */
			ctemp = bus_space_read_1(iot, ioh, 0x0c);
			if (temp >= ctemp) /* still valid, don't reset */
				status &= ~0x04;
		}
		if (status & 0x08) { /* overtemperature */
			sc->sc_data[chan].warnflags = ENVSYS_WARN_CRITOVER;
			/* read overtemperature limit */
			ctemp = bus_space_read_1(iot, ioh, 0x0e);
			if (temp >= ctemp) /* still valid, don't reset */
				status &= ~0x08;
		}

		/* clear outdated warnings */
		if (status & 0x0e)
			bus_space_write_1(iot, ioh, 0x0a, status);
	}
}

static int
tms_gtredata(sme, data)
	struct sysmon_envsys *sme;
	struct envsys_tre_data *data;
{
	struct nsclpcsio_softc *sc = sme->sme_cookie;

	tms_update(sc, data->sensor);

	*data = sc->sc_data[data->sensor];
	return (0);
}

static int
tms_streinfo(sme, info)
	struct sysmon_envsys *sme;
	struct envsys_basic_info *info;
{
#if 0
	struct nsclpcsio_softc *sc = sme->sme_cookie;
#endif
	/* XXX Not implemented */
	info->validflags = 0;
	
	return (0);
}
