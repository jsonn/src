/*	$NetBSD: pcmcom.c,v 1.14.2.5 2005/03/04 16:49:39 skrll Exp $	*/

/*-
 * Copyright (c) 1998, 2000, 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by RedBack Networks, Inc.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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
 * Device driver for multi-port PCMCIA serial cards, written by
 * Jason R. Thorpe for RedBack Networks, Inc.
 *
 * Most of these cards are simply multiple UARTs sharing a single interrupt
 * line, and rely on the fact that PCMCIA level-triggered interrupts can
 * be shared.  There are no special interrupt registers on them, as there
 * are on most ISA multi-port serial cards.
 *
 * If there are other cards that have interrupt registers, they should not
 * be glued into this driver.  Rather, separate drivers should be written
 * for those devices, as we have in the ISA multi-port serial card case.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pcmcom.c,v 1.14.2.5 2005/03/04 16:49:39 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/termios.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>

#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciadevs.h>

#include "com.h"
#include "pcmcom.h"

#include "locators.h"

struct pcmcom_softc {
	struct device sc_dev;			/* generic device glue */

	struct pcmcia_function *sc_pf;		/* our PCMCIA function */
	void *sc_ih;				/* interrupt handle */
	int sc_enabled_count;			/* enabled count */

#define	NSLAVES			8
	struct device *sc_slaves[NSLAVES];	/* slave info */
	int sc_nslaves;				/* slave count */

	int sc_state;
#define	PCMCOM_ATTACHED		3
};

struct pcmcom_attach_args {
	bus_space_tag_t pca_iot;		/* I/O tag */
	bus_space_handle_t pca_ioh;		/* I/O handle */
	int pca_slave;				/* slave # */
};

int	pcmcom_match(struct device *, struct cfdata *, void *);
int	pcmcom_validate_config(struct pcmcia_config_entry *);
void	pcmcom_attach(struct device *, struct device *, void *);
int	pcmcom_detach(struct device *, int);
int	pcmcom_activate(struct device *, enum devact);

CFATTACH_DECL(pcmcom, sizeof(struct pcmcom_softc),
    pcmcom_match, pcmcom_attach, pcmcom_detach, pcmcom_activate);

const struct pcmcia_product pcmcom_products[] = {
	{ PCMCIA_VENDOR_SOCKET, PCMCIA_PRODUCT_SOCKET_DUAL_RS232,
	  PCMCIA_CIS_INVALID },
};
const size_t pcmcom_nproducts =
    sizeof(pcmcom_products) / sizeof(pcmcom_products[0]);

int	pcmcom_print(void *, const char *);
int	pcmcom_submatch(struct device *, struct cfdata *,
			     const locdesc_t *, void *);

int	pcmcom_enable(struct pcmcom_softc *);
void	pcmcom_disable(struct pcmcom_softc *);

int	pcmcom_intr(void *);

int
pcmcom_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct pcmcia_attach_args *pa = aux;

	if (pcmcia_product_lookup(pa, pcmcom_products, pcmcom_nproducts,
	    sizeof(pcmcom_products[0]), NULL))
		return (2);	/* beat com_pcmcia */
	return (0);
}

int
pcmcom_validate_config(cfe)
	struct pcmcia_config_entry *cfe;
{
	if (cfe->iftype != PCMCIA_IFTYPE_IO ||
	    cfe->num_iospace < 1 || cfe->num_iospace > NSLAVES)
		return (EINVAL);
	return (0);
}

void
pcmcom_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pcmcom_softc *sc = (void *)self;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
	int slave;
	int error;
	int help[2];
	locdesc_t *ldesc = (void *)help; /* XXX */

	sc->sc_pf = pa->pf;

	error = pcmcia_function_configure(pa->pf, pcmcom_validate_config);
	if (error) {
		aprint_error("%s: configure failed, error=%d\n", self->dv_xname,
		    error);
		return;
	}

	cfe = pa->pf->cfe;
	sc->sc_nslaves = cfe->num_iospace;

	error = pcmcom_enable(sc);
	if (error)
		goto fail;

	/* Attach the children. */
	for (slave = 0; slave < sc->sc_nslaves; slave++) {
		struct pcmcom_attach_args pca;

		printf("%s: slave %d\n", self->dv_xname, slave);

		pca.pca_iot = cfe->iospace[slave].handle.iot;
		pca.pca_ioh = cfe->iospace[slave].handle.ioh;
		pca.pca_slave = slave;

		ldesc->len = 1;
		ldesc->locs[PCMCOMCF_SLAVE] = slave;

		sc->sc_slaves[slave] = config_found_sm_loc(&sc->sc_dev,
			"pcmcom", ldesc,
			&pca, pcmcom_print, pcmcom_submatch);
	}

	pcmcom_disable(sc);
	sc->sc_state = PCMCOM_ATTACHED;
	return;

fail:
	pcmcia_function_unconfigure(pa->pf);
}

int
pcmcom_detach(self, flags)
	struct device *self;
	int flags;
{
	struct pcmcom_softc *sc = (void *)self;
	int slave, error;

	if (sc->sc_state != PCMCOM_ATTACHED)
		return (0);

	for (slave = sc->sc_nslaves - 1; slave >= 0; slave--) {
		if (sc->sc_slaves[slave]) {
			/* Detach the child. */
			error = config_detach(sc->sc_slaves[slave], flags);
			if (error)
				return (error);
			sc->sc_slaves[slave] = 0;
		}
	}

	pcmcia_function_unconfigure(sc->sc_pf);

	return (0);
}

int
pcmcom_activate(self, act)
	struct device *self;
	enum devact act;
{
	struct pcmcom_softc *sc = (void *)self;
	int slave, error = 0, s;

	s = splserial();
	switch (act) {
	case DVACT_ACTIVATE:
		error = EOPNOTSUPP;
		break;

	case DVACT_DEACTIVATE:
		for (slave = sc->sc_nslaves - 1; slave >= 0; slave--) {
			if (sc->sc_slaves[slave]) {
				/*
				 * Deactivate the child.  Doing so will cause
				 * our own enabled count to drop to 0, once all
				 * children are deactivated.
				 */
				error = config_deactivate(sc->sc_slaves[slave]);
				if (error)
					break;
			}
		}
		break;
	}
	splx(s);
	return (error);
}

int
pcmcom_print(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct pcmcom_attach_args *pca = aux;

	/* only com's can attach to pcmcom's; easy... */
	if (pnp)
		aprint_normal("com at %s", pnp);

	aprint_normal(" slave %d", pca->pca_slave);

	return (UNCONF);
}

int
pcmcom_submatch(parent, cf, ldesc, aux)
	struct device *parent;
	struct cfdata *cf;
	const locdesc_t *ldesc;
	void *aux;
{

	if (cf->cf_loc[PCMCOMCF_SLAVE] != PCMCOMCF_SLAVE_DEFAULT &&
	    cf->cf_loc[PCMCOMCF_SLAVE] != ldesc->locs[PCMCOMCF_SLAVE]);
		return (0);

	return (config_match(parent, cf, aux));
}

int
pcmcom_intr(arg)
	void *arg;
{
#if NCOM > 0
	struct pcmcom_softc *sc = arg;
	int i, rval = 0;

	if (sc->sc_enabled_count == 0)
		return (0);

	for (i = 0; i < sc->sc_nslaves; i++) {
		if (sc->sc_slaves[i])
			rval |= comintr(sc->sc_slaves[i]);
	}

	return (rval);
#else
	return (0);
#endif /* NCOM > 0 */
}

int
pcmcom_enable(sc)
	struct pcmcom_softc *sc;
{
	int error;

	if (sc->sc_enabled_count++ != 0)
		return (0);

	/* Establish the interrupt. */
	sc->sc_ih = pcmcia_intr_establish(sc->sc_pf, IPL_SERIAL,
	    pcmcom_intr, sc);
	if (!sc->sc_ih)
		return (EIO);

	error = pcmcia_function_enable(sc->sc_pf);
	if (error) {
		pcmcia_intr_disestablish(sc->sc_pf, sc->sc_ih);
		sc->sc_ih = 0;
	}

	return (error);
}

void
pcmcom_disable(sc)
	struct pcmcom_softc *sc;
{

	if (--sc->sc_enabled_count != 0)
		return;

	pcmcia_function_disable(sc->sc_pf);
	pcmcia_intr_disestablish(sc->sc_pf, sc->sc_ih);
	sc->sc_ih = 0;
}

/****** Here begins the com attachment code. ******/

#if NCOM_PCMCOM > 0
int	com_pcmcom_match(struct device *, struct cfdata *, void *);
void	com_pcmcom_attach(struct device *, struct device *, void *);

/* No pcmcom-specific goo in the softc; it's all in the parent. */
CFATTACH_DECL(com_pcmcom, sizeof(struct com_softc),
    com_pcmcom_match, com_pcmcom_attach, com_detach, com_activate);

int	com_pcmcom_enable(struct com_softc *);
void	com_pcmcom_disable(struct com_softc *);

int
com_pcmcom_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{

	/* Device is always present. */
	return (1);
}

void
com_pcmcom_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct com_softc *sc = (struct com_softc *)self;
	struct pcmcom_attach_args *pca = aux;

	sc->sc_iot = pca->pca_iot;
	sc->sc_ioh = pca->pca_ioh;

	sc->enabled = 1;

	sc->sc_iobase = -1;
	sc->sc_frequency = COM_FREQ;

	sc->enable = com_pcmcom_enable;
	sc->disable = com_pcmcom_disable;

	com_attach_subr(sc);

	sc->enabled = 0;
}

int
com_pcmcom_enable(sc)
	struct com_softc *sc;
{

	return (pcmcom_enable((struct pcmcom_softc *)sc->sc_dev.dv_parent));
}

void
com_pcmcom_disable(sc)
	struct com_softc *sc;
{

	pcmcom_disable((struct pcmcom_softc *)sc->sc_dev.dv_parent);
}
#endif /* NCOM_PCMCOM > 0 */
