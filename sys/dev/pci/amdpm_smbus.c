/*	$NetBSD: amdpm_smbus.c,v 1.12.14.1 2007/09/03 10:20:57 skrll Exp $ */

/*
 * Copyright (c) 2005 Anil Gopinath (anil_public@yahoo.com)
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* driver for SMBUS 1.0 host controller found in the
 * AMD-8111 HyperTransport I/O Hub
 */
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: amdpm_smbus.c,v 1.12.14.1 2007/09/03 10:20:57 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/rnd.h>
#include <sys/rwlock.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/i2c/i2cvar.h>
#include <dev/i2c/i2c_bitbang.h>

#include <dev/pci/amdpmreg.h>
#include <dev/pci/amdpmvar.h>

#include <dev/pci/amdpm_smbusreg.h>

#ifdef __i386__
#include "opt_xbox.h"
#endif

#ifdef XBOX
extern int arch_i386_is_xbox;
#endif

static int       amdpm_smbus_acquire_bus(void *, int);
static void      amdpm_smbus_release_bus(void *, int);
static int       amdpm_smbus_exec(void *, i2c_op_t, i2c_addr_t, const void *,
				  size_t, void *, size_t, int);
static int       amdpm_smbus_check_done(struct amdpm_softc *, i2c_op_t);
static void      amdpm_smbus_clear_gsr(struct amdpm_softc *);
static uint16_t	amdpm_smbus_get_gsr(struct amdpm_softc *);
static int       amdpm_smbus_send_1(struct amdpm_softc *, uint8_t, i2c_op_t);
static int       amdpm_smbus_write_1(struct amdpm_softc *, uint8_t,
				     uint8_t, i2c_op_t);
static int       amdpm_smbus_receive_1(struct amdpm_softc *, i2c_op_t);
static int       amdpm_smbus_read_1(struct amdpm_softc *sc, uint8_t, i2c_op_t);

#ifdef XBOX
static int	 amdpm_smbus_intr(void *);
#endif

void
amdpm_smbus_attach(struct amdpm_softc *sc)
{
        struct i2cbus_attach_args iba;
#ifdef XBOX
	pci_intr_handle_t ih;
	const char *intrstr;
#endif
	
	/* register with iic */
	sc->sc_i2c.ic_cookie = sc; 
	sc->sc_i2c.ic_acquire_bus = amdpm_smbus_acquire_bus; 
	sc->sc_i2c.ic_release_bus = amdpm_smbus_release_bus;
	sc->sc_i2c.ic_send_start = NULL;
	sc->sc_i2c.ic_send_stop = NULL;
	sc->sc_i2c.ic_initiate_xfer = NULL;
	sc->sc_i2c.ic_read_byte = NULL;
	sc->sc_i2c.ic_write_byte = NULL;
	sc->sc_i2c.ic_exec = amdpm_smbus_exec;

	rw_init(&sc->sc_rwlock);

#ifdef XBOX
#define XBOX_SMBA	0x8000
#define XBOX_SMSIZE	256
#define XBOX_INTRLINE	12
#define XBOX_REG_ACPI_PM1a_EN		0x02
#define XBOX_REG_ACPI_PM1a_EN_TIMER		0x01
	/* XXX pci0 dev 1 function 2 "System Management" doesn't probe */
	if (arch_i386_is_xbox) {
		uint16_t val;
		sc->sc_pa->pa_intrline = XBOX_INTRLINE;

		if (bus_space_map(sc->sc_iot, XBOX_SMBA, XBOX_SMSIZE,
		    0, &sc->sc_sm_ioh) == 0) {
			aprint_normal("%s: system management at 0x%04x\n",
			    sc->sc_dev.dv_xname, XBOX_SMBA);

			/* Disable PM ACPI timer SCI interrupt */
			val = bus_space_read_2(sc->sc_iot, sc->sc_sm_ioh,
			    XBOX_REG_ACPI_PM1a_EN);
			bus_space_write_2(sc->sc_iot, sc->sc_sm_ioh,
			    XBOX_REG_ACPI_PM1a_EN,
			    val & ~XBOX_REG_ACPI_PM1a_EN_TIMER);
		}
	}

	if (pci_intr_map(sc->sc_pa, &ih))
		aprint_error("%s: couldn't map interrupt\n",
		    sc->sc_dev.dv_xname);
	else {
		intrstr = pci_intr_string(sc->sc_pc, ih);
		sc->sc_ih = pci_intr_establish(sc->sc_pc, ih, IPL_BIO,
		    amdpm_smbus_intr, sc);
		if (sc->sc_ih != NULL)
			aprint_normal("%s: interrupting at %s\n",
			    sc->sc_dev.dv_xname, intrstr);
	}
#endif

	iba.iba_tag = &sc->sc_i2c;
	(void)config_found_ia(&sc->sc_dev, "i2cbus", &iba, iicbus_print);
}

#ifdef XBOX
static int
amdpm_smbus_intr(void *cookie)
{
	struct amdpm_softc *sc;
	uint32_t status;

	sc = (struct amdpm_softc *)cookie;

	if (arch_i386_is_xbox) {
		status = bus_space_read_4(sc->sc_iot, sc->sc_sm_ioh, 0x20);
		bus_space_write_4(sc->sc_iot, sc->sc_sm_ioh, 0x20, status);
	
		if (status & 2)
			return iic_smbus_intr(&sc->sc_i2c);
	}

	return 0;
}
#endif

static int
amdpm_smbus_acquire_bus(void *cookie, int flags)
{
	struct amdpm_softc *sc = cookie;

	rw_enter(&sc->sc_rwlock, RW_WRITER);
	return 0;
}

static void
amdpm_smbus_release_bus(void *cookie, int flags)
{
	struct amdpm_softc *sc = cookie;

	rw_exit(&sc->sc_rwlock);
}

static int
amdpm_smbus_exec(void *cookie, i2c_op_t op, i2c_addr_t addr, const void *cmd,
		 size_t cmdlen, void *vbuf, size_t buflen, int flags)
{
        struct amdpm_softc *sc  = (struct amdpm_softc *) cookie;
	sc->sc_smbus_slaveaddr  = addr;
	uint8_t *p = vbuf;
	int rv;
	
	if (I2C_OP_READ_P(op) && (cmdlen == 0) && (buflen == 1)) {
		rv = amdpm_smbus_receive_1(sc, op);
		if (rv == -1)
			return -1;
		*p = (uint8_t)rv;
		return 0;
	}
	
	if ((I2C_OP_READ_P(op)) && (cmdlen == 1) && (buflen == 1)) {
		rv = amdpm_smbus_read_1(sc, *(const uint8_t *)cmd, op);
		if (rv == -1)
			return -1;
		*p = (uint8_t)rv;
		return 0;
	}
	
	if ((I2C_OP_WRITE_P(op)) && (cmdlen == 0) && (buflen == 1))
		return amdpm_smbus_send_1(sc, *(uint8_t*)vbuf, op);
	
	if ((I2C_OP_WRITE_P(op)) && (cmdlen == 1) && (buflen == 1))
		return amdpm_smbus_write_1(sc,
					   *(const uint8_t*)cmd,
					   *(uint8_t*)vbuf,
					   op);
	
	return -1;  
}

static int 
amdpm_smbus_check_done(struct amdpm_softc *sc, i2c_op_t op)
{  
        int i;

	for (i = 0; i < 1000; i++) {
	/* check gsr and wait till cycle is done */
		uint16_t data = amdpm_smbus_get_gsr(sc);
		if (data & AMDPM_8111_GSR_CYCLE_DONE)
			return 0;	
	}

	if (!(op & I2C_F_POLL))
	    delay(1);

	return -1;    
}


static void
amdpm_smbus_clear_gsr(struct amdpm_softc *sc)
{
        /* clear register */
        uint16_t data = 0xFFFF;
	int off = (sc->sc_nforce ? 0xe0 : 0);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh,
	    AMDPM_8111_SMBUS_STAT - off, data);
}

static uint16_t
amdpm_smbus_get_gsr(struct amdpm_softc *sc)
{
	int off = (sc->sc_nforce ? 0xe0 : 0);
        return bus_space_read_2(sc->sc_iot, sc->sc_ioh,
	    AMDPM_8111_SMBUS_STAT - off);
}

static int
amdpm_smbus_send_1(struct amdpm_softc *sc, uint8_t val, i2c_op_t op)
{
	uint16_t data = 0;
	int off = (sc->sc_nforce ? 0xe0 : 0);

	/* first clear gsr */
	amdpm_smbus_clear_gsr(sc);

	/* write smbus slave address to register */
	data = sc->sc_smbus_slaveaddr;
	data <<= 1;
	data |= AMDPM_8111_SMBUS_SEND;    
	bus_space_write_1(sc->sc_iot, sc->sc_ioh,
	    AMDPM_8111_SMBUS_HOSTADDR - off, data);
	
	data = val;    
	/* store data */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh,
	    AMDPM_8111_SMBUS_HOSTDATA - off, data);
	/* host start */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh,
	    AMDPM_8111_SMBUS_CTRL - off,
	    AMDPM_8111_SMBUS_GSR_SB);	

	return amdpm_smbus_check_done(sc, op);
}

  
static int
amdpm_smbus_write_1(struct amdpm_softc *sc, uint8_t cmd, uint8_t val,
		    i2c_op_t op)
{
	uint16_t data = 0;
	int off = (sc->sc_nforce ? 0xe0 : 0);

	/* first clear gsr */
	amdpm_smbus_clear_gsr(sc);  
  
	data = sc->sc_smbus_slaveaddr;
	data <<= 1;
	data |= AMDPM_8111_SMBUS_WRITE;    
	bus_space_write_1(sc->sc_iot, sc->sc_ioh,
	    AMDPM_8111_SMBUS_HOSTADDR - off, data);
	
	data = val;    
	/* store cmd */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh,
	    AMDPM_8111_SMBUS_HOSTCMD - off, cmd);
	/* store data */    
	bus_space_write_2(sc->sc_iot, sc->sc_ioh,
	    AMDPM_8111_SMBUS_HOSTDATA - off, data);    
	/* host start */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh,
	    AMDPM_8111_SMBUS_CTRL - off, AMDPM_8111_SMBUS_GSR_WB);
	
	return amdpm_smbus_check_done(sc, op);
}

static int
amdpm_smbus_receive_1(struct amdpm_softc *sc, i2c_op_t op)
{
	uint16_t data = 0;
	int off = (sc->sc_nforce ? 0xe0 : 0);

	/* first clear gsr */
	amdpm_smbus_clear_gsr(sc);  

	/* write smbus slave address to register */
	data = sc->sc_smbus_slaveaddr;
	data <<= 1;
	data |= AMDPM_8111_SMBUS_RX;    
	bus_space_write_1(sc->sc_iot, sc->sc_ioh,
	    AMDPM_8111_SMBUS_HOSTADDR - off, data);
	
	/* start smbus cycle */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh,
	    AMDPM_8111_SMBUS_CTRL - off, AMDPM_8111_SMBUS_GSR_RXB);
	
	/* check for errors */
	if (amdpm_smbus_check_done(sc, op) < 0)
		return -1;
	
	/* read data */
	data = bus_space_read_2(sc->sc_iot, sc->sc_ioh,
	    AMDPM_8111_SMBUS_HOSTDATA - off);
	uint8_t ret = (uint8_t)(data & 0x00FF);
	return ret;
}

static int
amdpm_smbus_read_1(struct amdpm_softc *sc, uint8_t cmd, i2c_op_t op)
{
	uint16_t data = 0;
	uint8_t ret;
	int off = (sc->sc_nforce ? 0xe0 : 0);

	/* first clear gsr */
	amdpm_smbus_clear_gsr(sc);  

	/* write smbus slave address to register */
	data = sc->sc_smbus_slaveaddr;
	data <<= 1;
	data |= AMDPM_8111_SMBUS_READ;    
	bus_space_write_1(sc->sc_iot, sc->sc_ioh,
	    AMDPM_8111_SMBUS_HOSTADDR - off, data);
	
	/* store cmd */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh,
	    AMDPM_8111_SMBUS_HOSTCMD - off, cmd);
	/* host start */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh,
	    AMDPM_8111_SMBUS_CTRL - off, AMDPM_8111_SMBUS_GSR_RB);
	
	/* check for errors */
	if (amdpm_smbus_check_done(sc, op) < 0)
		return -1;
	
	/* store data */    
	data = bus_space_read_2(sc->sc_iot, sc->sc_ioh,
	    AMDPM_8111_SMBUS_HOSTDATA - off);
	ret = (uint8_t)(data & 0x00FF);
	return ret;
}
