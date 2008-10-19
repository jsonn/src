/*	$NetBSD: i2c.c,v 1.20.6.1 2008/10/19 22:16:25 haad Exp $	*/

/*
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: i2c.c,v 1.20.6.1 2008/10/19 22:16:25 haad Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/event.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/kthread.h>
#include <sys/proc.h>
#include <sys/kernel.h>

#include <dev/i2c/i2cvar.h>

#include "locators.h"
#include <opt_i2cbus.h>

struct iic_softc {
	i2c_tag_t sc_tag;
	int sc_type;
};

static void	iic_smbus_intr_thread(void *);

int
iicbus_print(void *aux, const char *pnp)
{

	if (pnp != NULL)
		aprint_normal("iic at %s", pnp);

	return (UNCONF);
}

static int
iic_print(void *aux, const char *pnp)
{
	struct i2c_attach_args *ia = aux;

	if (ia->ia_addr != (i2c_addr_t)-1)
		aprint_normal(" addr 0x%x", ia->ia_addr);

	return (UNCONF);
}

static int
iic_search(device_t parent, cfdata_t cf, const int *ldesc, void *aux)
{
	struct iic_softc *sc = device_private(parent);
	struct i2c_attach_args ia;

	ia.ia_tag = sc->sc_tag;
	ia.ia_addr = cf->cf_loc[IICCF_ADDR];
	ia.ia_size = cf->cf_loc[IICCF_SIZE];
	ia.ia_type = sc->sc_type;

	if (config_match(parent, cf, &ia) > 0)
		config_attach(parent, cf, &ia, iic_print);

	return (0);
}

static int
iic_match(device_t parent, cfdata_t cf, void *aux)
{

	return (1);
}

static void
iic_attach(device_t parent, device_t self, void *aux)
{
	struct iic_softc *sc = device_private(self);
	struct i2cbus_attach_args *iba = aux;
	i2c_tag_t ic;
	int rv;

	aprint_naive(": I2C bus\n");
	aprint_normal(": I2C bus\n");

	sc->sc_tag = iba->iba_tag;
	sc->sc_type = iba->iba_type;
	ic = sc->sc_tag;
	ic->ic_devname = device_xname(self);

	LIST_INIT(&(sc->sc_tag->ic_list));
	LIST_INIT(&(sc->sc_tag->ic_proc_list));

	rv = kthread_create(PRI_NONE, 0, NULL, iic_smbus_intr_thread,
	    ic, &ic->ic_intr_thread, "%s", ic->ic_devname);
	if (rv)
		aprint_error_dev(self, "unable to create intr thread\n");

#if I2C_SCAN
	if (sc->sc_type == I2C_TYPE_SMBUS) {
		int found = 0;
		i2c_addr_t addr;
		uint8_t cmd = 0, val;

		for (addr = 0x0; addr < 0x80; addr++) {
			/* Skip i2c Alert Response Address */
			if (addr == 0x0c)
				continue;
			iic_acquire_bus(ic, 0);
			if (iic_exec(ic, I2C_OP_READ_WITH_STOP, addr,
			    &cmd, 1, &val, 1, 0) == 0) {
				if (found == 0)
					aprint_normal("%s: devices at",
							ic->ic_devname);
				found++;
				aprint_normal(" 0x%02x", addr);
			}
			iic_release_bus(ic, 0);
		}
		if (found == 0)
			aprint_normal("%s: no devices found", ic->ic_devname);
		aprint_normal("\n");
	}
#endif

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");

	/*
	 * Attach all i2c devices described in the kernel
	 * configuration file.
	 */
	config_search_ia(iic_search, self, "iic", NULL);
}

static void
iic_smbus_intr_thread(void *aux)
{
	i2c_tag_t ic;
	struct ic_intr_list *il;
	int rv;

	ic = (i2c_tag_t)aux;
	ic->ic_running = 1;
	ic->ic_pending = 0;

	while (ic->ic_running) {
		if (ic->ic_pending == 0)
			rv = tsleep(ic, PZERO, "iicintr", hz);
		if (ic->ic_pending > 0) {
			LIST_FOREACH(il, &(ic->ic_proc_list), il_next) {
				(*il->il_intr)(il->il_intrarg);
			}
			ic->ic_pending--;
		}
	}

	kthread_exit(0);
}

void *
iic_smbus_intr_establish(i2c_tag_t ic, int (*intr)(void *), void *intrarg)
{
	struct ic_intr_list *il;

	il = malloc(sizeof(struct ic_intr_list), M_DEVBUF, M_WAITOK);
	if (il == NULL)
		return NULL;
	    
	il->il_intr = intr;
	il->il_intrarg = intrarg;

	LIST_INSERT_HEAD(&(ic->ic_list), il, il_next);

	return il;
}

void
iic_smbus_intr_disestablish(i2c_tag_t ic, void *hdl)
{
	struct ic_intr_list *il;

	il = (struct ic_intr_list *)hdl;

	LIST_REMOVE(il, il_next);
	free(il, M_DEVBUF);

	return;
}

void *
iic_smbus_intr_establish_proc(i2c_tag_t ic, int (*intr)(void *), void *intrarg)
{
	struct ic_intr_list *il;

	il = malloc(sizeof(struct ic_intr_list), M_DEVBUF, M_WAITOK);
	if (il == NULL)
		return NULL;
	    
	il->il_intr = intr;
	il->il_intrarg = intrarg;

	LIST_INSERT_HEAD(&(ic->ic_proc_list), il, il_next);

	return il;
}

void
iic_smbus_intr_disestablish_proc(i2c_tag_t ic, void *hdl)
{
	struct ic_intr_list *il;

	il = (struct ic_intr_list *)hdl;

	LIST_REMOVE(il, il_next);
	free(il, M_DEVBUF);

	return;
}

int
iic_smbus_intr(i2c_tag_t ic)
{
	struct ic_intr_list *il;

	LIST_FOREACH(il, &(ic->ic_list), il_next) {
		(*il->il_intr)(il->il_intrarg);
	}

	ic->ic_pending++;
	wakeup(ic);

	return 1;
}

CFATTACH_DECL_NEW(iic, sizeof(struct iic_softc),
    iic_match, iic_attach, NULL, NULL);
