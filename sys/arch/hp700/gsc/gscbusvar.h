/*	$NetBSD: gscbusvar.h,v 1.1.2.2 2002/06/23 17:36:21 jdolecek Exp $	*/

/*	$OpenBSD: gscbusvar.h,v 1.3 1999/08/16 02:48:39 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <hp700/hp700/intr.h>

struct gscbus_ic {
	enum {gsc_unknown = 0, gsc_lasi, gsc_wax, gsc_asp} gsc_type;
	void *gsc_dv;

	struct hp700_int_reg gsc_int_reg;
};

struct gsc_attach_args {
	struct confargs ga_ca;
#define	ga_name		ga_ca.ca_name
#define	ga_iot		ga_ca.ca_iot
#define	ga_mod		ga_ca.ca_mod
#define	ga_type		ga_ca.ca_type
#define	ga_hpa		ga_ca.ca_hpa
#define	ga_dmatag	ga_ca.ca_dmatag
#define	ga_irq		ga_ca.ca_irq
/*#define	ga_pdc_iodc_read	ga_ca.ca_pdc_iodc_read */
	struct gscbus_ic *ga_ic;	/* IC pointer */
}; 

struct gscbus_intr {
	int pri;
	int (*handler) __P((void *));
	void *arg;
	struct evcnt evcnt;
};

struct gsc_softc {
	struct  device sc_dev;
	void *sc_ih;

	bus_space_tag_t sc_iot;
	struct gscbus_ic *sc_ic;
	struct hppa_bus_dma_tag sc_dmatag;

	/* interrupt vectors */
	struct gscbus_intr sc_intrvs[32];
	u_int32_t sc_intrmask;
};

void *gsc_intr_establish __P((struct gsc_softc *sc, int pri, int irq,
			       int (*handler) __P((void *v)), void *arg,
			       struct device *name));
void gsc_intr_disestablish __P((struct gsc_softc *sc, void *v));
int gsc_intr __P((void *));

int gscprint __P((void *, const char *));

