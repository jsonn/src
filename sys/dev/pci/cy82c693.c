/* $NetBSD: cy82c693.c,v 1.6.20.1 2010/04/30 14:43:32 uebayasi Exp $ */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 * Common routines to read/write control registers on the Cypress 82c693
 * hyperCache(tm) Stand-Alone PCI Peripheral Controller with USB.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cy82c693.c,v 1.6.20.1 2010/04/30 14:43:32 uebayasi Exp $");

#include "opt_multiprocessor.h"
#include "opt_lockdebug.h"

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/simplelock.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/pci/cy82c693reg.h>
#include <dev/pci/cy82c693var.h>

static struct cy82c693_handle cyhc_handle;
static int cyhc_initialized;

static struct simplelock cyhc_slock = SIMPLELOCK_INITIALIZER;

#define	CYHC_LOCK(s)							\
do {									\
	s = splhigh();							\
	simple_lock(&cyhc_slock);					\
} while (0)

#define	CYHC_UNLOCK(s)							\
do {									\
	simple_unlock(&cyhc_slock);					\
	splx(s);							\
} while (0)

const struct cy82c693_handle *
cy82c693_init(bus_space_tag_t iot)
{
	bus_space_handle_t ioh;
	int s;

	CYHC_LOCK(s);

	if (cyhc_initialized) {
		CYHC_UNLOCK(s);
		KASSERT(bus_space_is_equal(iot, cyhc_handle.cyhc_iot));
		return (&cyhc_handle);
	}

	if (bus_space_map(iot, CYHC_CONFIG_ADDR, 2, 0, &ioh) != 0) {
		CYHC_UNLOCK(s);
		return (NULL);
	}

	cyhc_handle.cyhc_iot = iot;
	cyhc_handle.cyhc_ioh = ioh;

	cyhc_initialized = 1;

	CYHC_UNLOCK(s);

	return (&cyhc_handle);
}

u_int8_t
cy82c693_read(const struct cy82c693_handle *cyhc, int reg)
{
	int s;
	u_int8_t rv;

	CYHC_LOCK(s);

	if (cyhc_initialized == 0) {
		CYHC_UNLOCK(s);
		panic("cy82c693_read");
	}

	bus_space_write_1(cyhc->cyhc_iot, cyhc->cyhc_ioh, 0, reg);
	rv = bus_space_read_1(cyhc->cyhc_iot, cyhc->cyhc_ioh, 1);

	CYHC_UNLOCK(s);

	return (rv);
}

void
cy82c693_write(const struct cy82c693_handle *cyhc, int reg, u_int8_t val)
{
	int s;

	CYHC_LOCK(s);

	if (cyhc_initialized == 0) {
		CYHC_UNLOCK(s);
		panic("cy82c693_write");
	}

	bus_space_write_1(cyhc->cyhc_iot, cyhc->cyhc_ioh, 0, reg);
	bus_space_write_1(cyhc->cyhc_iot, cyhc->cyhc_ioh, 1, val);

	CYHC_UNLOCK(s);
}
