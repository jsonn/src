/*	$NetBSD: smsc.c,v 1.1.2.5 2007/10/09 13:41:38 ad Exp $ */

/*-
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Brett Lymn.
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

/*
 * This is a driver for the Standard Microsystems Corp (SMSC)
 * LPC47B397 "super i/o" chip.  This driver only handles the environment
 * monitoring capabilities of the chip, the other functions will be
 * probed/matched as "normal" PC hardware devices (serial ports, fdc, so on).
 * SMSC has not deigned to release a datasheet for this particular chip
 * (though they do for others they make) so this driver was written from
 * information contained in the comment block for the Linux driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: smsc.c,v 1.1.2.5 2007/10/09 13:41:38 ad Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/time.h>

#include <machine/bus.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/sysmon/sysmonvar.h>

#include <dev/isa/smscvar.h>

#include <machine/intr.h>

#if defined(LMDEBUG)
#define DPRINTF(x)	do { printf x; } while (0)
#else
#define DPRINTF(x)
#endif

int smsc_probe(struct device *, struct cfdata *, void *);
void smsc_attach(struct device *, struct device *, void *);
int smsc_detach(struct device *, int);
static uint8_t smsc_readreg(struct smsc_softc *, int);
/*static void smsc_writereg(struct smsc_softc *, int, int);*/
void smsc_setup(struct smsc_softc *);

static int smsc_gtredata(struct sysmon_envsys *, envsys_data_t *);

CFATTACH_DECL(smsc, sizeof(struct smsc_softc),
    smsc_probe, smsc_attach, smsc_detach, NULL);

struct smsc_sysmon {
	struct sysmon_envsys sme;
	struct smsc_softc *sc;
	envsys_data_t smsc_sensor[];
};

/*
 * Probe for the SMSC Super I/O chip
 */
int
smsc_probe(struct device *parent, struct cfdata *match, void *aux)
{
	bus_space_handle_t ioh;
	struct isa_attach_args *ia = aux;
	int rv;
	uint8_t cr;

	/* Must supply an address */
	if (ia->ia_nio < 1)
		return 0;

	if (ISA_DIRECT_CONFIG(ia))
		return 0;

	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return 0;

	if (bus_space_map(ia->ia_iot, ia->ia_io[0].ir_addr, 2, 0, &ioh))
		return 0;

	/* To get the device ID we must enter config mode... */
	bus_space_write_1(ia->ia_iot, ioh, SMSC_ADDR, SMSC_CONFIG_START);

	/* Then select the device id register */
	bus_space_write_1(ia->ia_iot, ioh, SMSC_ADDR, SMSC_DEVICE_ID);

	/* Finally, read the id from the chip */
	cr = bus_space_read_1(ia->ia_iot, ioh, SMSC_DATA);

	/* Exit config mode, apparently this is important to do */
	bus_space_write_1(ia->ia_iot, ioh, SMSC_ADDR, SMSC_CONFIG_END);

	if (cr == SMSC_ID)
		rv = 1;
	else
		rv = 0;

	DPRINTF(("smsc: rv = %d, cr = %x\n", rv, cr));

	bus_space_unmap(ia->ia_iot, ioh, 2);

	if (rv) {
		ia->ia_nio = 1;
		ia->ia_io[0].ir_size = 2;

		ia->ia_niomem = 0;
		ia->ia_nirq = 0;
		ia->ia_ndrq = 0;
	}

	return rv;
}

/*
 * Get the base address for the monitoring registers and set up the
 * env sysmon framework.
 */
void
smsc_attach(struct device *parent, struct device *self, void *aux)
{
	struct smsc_softc *smsc_sc = (void *)self;
	struct isa_attach_args *ia = aux;
	bus_space_handle_t ioh;
	uint8_t rev, msb, lsb;
	unsigned address;

	smsc_sc->smsc_iot = ia->ia_iot;

	/* To attach we need to find the actual i/o register space,
	   map the base registers in first. */
	if (bus_space_map(ia->ia_iot, ia->ia_io[0].ir_addr, 2, 0,
	    &ioh)) {
		aprint_error(": can't map base i/o space\n");
		return;
	}

	/* Enter config mode. */
	bus_space_write_1(ia->ia_iot, ioh, SMSC_ADDR, SMSC_CONFIG_START);

	/* While we have the base registers mapped, grab the chip revision */
	bus_space_write_1(ia->ia_iot, ioh, SMSC_ADDR, SMSC_DEVICE_REVISION);
	rev = bus_space_read_1(ia->ia_iot, ioh, SMSC_DATA);

	/* Select the correct logical device */
	bus_space_write_1(ia->ia_iot, ioh, SMSC_ADDR, SMSC_LOGICAL_DEV_SEL);
	bus_space_write_1(ia->ia_iot, ioh, SMSC_DATA, SMSC_LOGICAL_DEVICE);

	/* Read the base address for the registers. */
	bus_space_write_1(ia->ia_iot, ioh, SMSC_ADDR, SMSC_IO_BASE_MSB);
	msb = bus_space_read_1(ia->ia_iot, ioh, SMSC_DATA);
	bus_space_write_1(ia->ia_iot, ioh, SMSC_ADDR, SMSC_IO_BASE_LSB);
	lsb = bus_space_read_1(ia->ia_iot, ioh, SMSC_DATA);
	address = (msb << 8) | lsb;

	/* Exit config mode */
	bus_space_write_1(ia->ia_iot, ioh, SMSC_ADDR, SMSC_CONFIG_END);

	bus_space_unmap(ia->ia_iot, ioh, 2);

	/* Map the i/o space for the registers. */
	if (bus_space_map(ia->ia_iot, address, 2, 0, &smsc_sc->smsc_ioh)) {
		aprint_error(": can't map register i/o space\n");
		return;
	}

	aprint_normal(": monitor registers at 0x%04x  (rev. %u)\n",
	       address, rev);

	smsc_setup(smsc_sc);
}

int
smsc_detach(struct device *self, int flags)
{
	struct smsc_softc *sc = device_private(self);

	sysmon_envsys_unregister(sc->smsc_sysmon);
	bus_space_unmap(sc->smsc_iot, sc->smsc_ioh, 2);
	return 0;
}

/*
 * Read the value of the given register
 */
static uint8_t
smsc_readreg(struct smsc_softc *sc, int reg)
{
	bus_space_write_1(sc->smsc_iot, sc->smsc_ioh, SMSC_ADDR, reg);
	return bus_space_read_1(sc->smsc_iot, sc->smsc_ioh, SMSC_DATA);
}


/*
 * Write the given value to the given register - here just for completeness
 * it is unused in the current code.

static void
smsc_writereg (struct smsc_softc *sc, int reg, int val)
{
	bus_space_write_1(sc->smsc_iot, sc->smsc_ioh, SMSC_ADDR, reg);
	bus_space_write_1(sc->smsc_iot, sc->smsc_ioh, SMSC_DATA, val);
} */

/* convert temperature read from the chip to micro kelvin */
static inline int
smsc_temp2muk(uint8_t t)
{
        int temp=t;

        return temp * 1000000 + 273150000U;
}

/*
 * convert register value read from chip into rpm using:
 *
 * RPM = 60/(Count * 11.111us)
 *
 * 1/1.1111us = 90kHz
 *
 */
static inline int
smsc_reg2rpm(unsigned int r)
{
	unsigned long rpm;

        if (r == 0x0)
                return 0;

	rpm = (90000 * 60) / ((unsigned long) r);
        return (int) rpm;
}

/* min and max temperatures in uK */
#define SMSC_MIN_TEMP_UK ((-127 * 1000000) + 273150000)
#define SMSC_MAX_TEMP_UK ((127 * 1000000) + 273150000)

/*
 * Set up the environment monitoring framework for all the devices
 * that we monitor.
 */
void
smsc_setup(struct smsc_softc *sc)
{
	struct smsc_sysmon *datap;
	int error, i;
	char label[8];
	envsys_data_t *edata;

	datap = malloc(sizeof(struct sysmon_envsys) + SMSC_MAX_SENSORS *
	    sizeof(envsys_data_t) + sizeof(void *),
	    M_DEVBUF, M_WAITOK | M_ZERO);

	for (i = 0; i < 4; i++) {
		edata = &datap->smsc_sensor[i];
		sprintf(label, "Temp-%d", i + 1);
		strlcpy(edata->desc, label, sizeof(edata->desc));
		edata->units = ENVSYS_STEMP;
		edata->sensor = i;
		edata->state = ENVSYS_SVALID;
		switch (i) {
		case 0:
			sc->regs[i] = SMSC_TEMP1;
			break;

		case 1:
			sc->regs[i] = SMSC_TEMP2;
			break;

		case 2:
			sc->regs[i] = SMSC_TEMP3;
			break;

		case 3:
			sc->regs[i] = SMSC_TEMP4;
			break;
		}

	}

	for (i = 4; i < SMSC_MAX_SENSORS; i++) {
		edata = &datap->smsc_sensor[i];
		sprintf(label, "Fan-%d", i - 3);
		strlcpy(edata->desc, label, sizeof(edata->desc));
		edata->units = ENVSYS_SFANRPM;
		edata->sensor = i;
		edata->units = ENVSYS_SFANRPM;
		edata->state = ENVSYS_SVALID;

		switch (i) {
		case 4:
			sc->regs[i] = SMSC_FAN1_LSB;
			break;

		case 5:
			sc->regs[i] = SMSC_FAN2_LSB;
			break;

		case 6:
			sc->regs[i] = SMSC_FAN3_LSB;
			break;

		case 7:
			sc->regs[i] = SMSC_FAN4_LSB;
			break;

		default:
			aprint_error(": more fans than expected");
			break;
		}
	}

	sc->smsc_sysmon = &datap->sme;
	datap->sme.sme_nsensors = SMSC_MAX_SENSORS;
	datap->sme.sme_sensor_data = datap->smsc_sensor;
	datap->sme.sme_name = sc->sc_dev.dv_xname;
	datap->sme.sme_cookie = sc;
	datap->sme.sme_gtredata = smsc_gtredata;

	if ((error = sysmon_envsys_register(&datap->sme)) != 0) {
		aprint_error("%s: unable to register with sysmon (%d)\n",
		    sc->sc_dev.dv_xname, error);
		return;
	}

}

/*
 * Get the data for the requested sensor, update the sysmon structure
 * with the retrieved value.
 */
static int
smsc_gtredata(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	struct smsc_softc *sc = sme->sme_cookie;
	int i, reg;
	unsigned int rpm;
	uint8_t msb, lsb;

	i = edata->sensor;
	reg = sc->regs[i];

	switch (edata->units) {
	case ENVSYS_STEMP:
		edata->value_cur = smsc_temp2muk(smsc_readreg(sc, reg));
		break;

	case ENVSYS_SFANRPM:
		/* reading lsb first locks msb... */
		lsb = smsc_readreg(sc, reg);
		msb = smsc_readreg(sc, reg + 1); /* XXX note explicit
						    assumption that msb is
						    the next register along */
		rpm = (msb << 8) | lsb;
		edata->value_cur = smsc_reg2rpm(rpm);
		break;
	}

	edata->state = ENVSYS_SVALID;
	return 0;
}
