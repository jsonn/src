/*	$NetBSD: if_mbe_pcmcia.c,v 1.30.6.1 2004/08/12 11:42:00 skrll Exp $	*/

/*-
 * Copyright (c) 1998, 2000, 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Enami Tsugutomo.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_mbe_pcmcia.c,v 1.30.6.1 2004/08/12 11:42:00 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/ic/mb86960reg.h>
#include <dev/ic/mb86960var.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

int	mbe_pcmcia_match __P((struct device *, struct cfdata *, void *));
int	mbe_pcmcia_validate_config __P((struct pcmcia_config_entry *));
void	mbe_pcmcia_attach __P((struct device *, struct device *, void *));
int	mbe_pcmcia_detach __P((struct device *, int));

struct mbe_pcmcia_softc {
	struct	mb86960_softc sc_mb86960;	/* real "mb" softc */

	struct	pcmcia_function *sc_pf;		/* our PCMCIA function */
	void	*sc_ih;				/* interrupt cookie */

	int	sc_state;
#define	MBE_PCMCIA_ATTACHED	3
};

CFATTACH_DECL(mbe_pcmcia, sizeof(struct mbe_pcmcia_softc),
    mbe_pcmcia_match, mbe_pcmcia_attach, mbe_pcmcia_detach, mb86960_activate);

int	mbe_pcmcia_enable __P((struct mb86960_softc *));
void	mbe_pcmcia_disable __P((struct mb86960_softc *));

struct mbe_pcmcia_get_enaddr_args {
	u_int8_t enaddr[ETHER_ADDR_LEN];
	int maddr;
};
int	mbe_pcmcia_get_enaddr_from_cis __P((struct pcmcia_tuple *, void *));
int	mbe_pcmcia_get_enaddr_from_mem __P((struct mbe_pcmcia_softc *,
	    struct mbe_pcmcia_get_enaddr_args *));
int	mbe_pcmcia_get_enaddr_from_io __P((struct mbe_pcmcia_softc *,
	    struct mbe_pcmcia_get_enaddr_args *));

static const struct mbe_pcmcia_product {
	struct pcmcia_product mpp_product;
	int		mpp_enet_maddr;
	int		mpp_flags;
#define MBH10302	0x0001			/* FUJITSU MBH10302 */
} mbe_pcmcia_products[] = {
	{ { PCMCIA_VENDOR_TDK, PCMCIA_PRODUCT_TDK_LAK_CD021BX,
	    PCMCIA_CIS_TDK_LAK_CD021BX },
	  -1 }, 

	{ { PCMCIA_VENDOR_TDK, PCMCIA_PRODUCT_TDK_LAK_CF010,
	    PCMCIA_CIS_TDK_LAK_CF010 },
	  -1 },

#if 0 /* XXX 86960-based? */
	{ { PCMCIA_VENDOR_TDK, PCMCIA_PRODUCT_TDK_LAK_DFL9610,
	    PCMCIA_CIS_TDK_DFL9610 },
	  -1, MBH10302 /* XXX */ },
#endif

	{ { PCMCIA_VENDOR_CONTEC, PCMCIA_PRODUCT_CONTEC_CNETPC,
	    PCMCIA_CIS_CONTEC_CNETPC },
	  -1 },

	{ { PCMCIA_VENDOR_FUJITSU, PCMCIA_PRODUCT_FUJITSU_LA501,
	    PCMCIA_CIS_FUJITSU_LA501 },
	  -1 },

	{ { PCMCIA_VENDOR_FUJITSU, PCMCIA_PRODUCT_FUJITSU_FMV_J181,
	    PCMCIA_CIS_FUJITSU_FMV_J181 },
	  -1, MBH10302 },

	{ { PCMCIA_VENDOR_FUJITSU, PCMCIA_PRODUCT_FUJITSU_FMV_J182,
	    PCMCIA_CIS_FUJITSU_FMV_J182 },
	  0xf2c },

	{ { PCMCIA_VENDOR_FUJITSU, PCMCIA_PRODUCT_FUJITSU_FMV_J182A,
	    PCMCIA_CIS_FUJITSU_FMV_J182A },
	  0x1cc },

	{ { PCMCIA_VENDOR_FUJITSU, PCMCIA_PRODUCT_FUJITSU_ITCFJ182A,
	    PCMCIA_CIS_FUJITSU_ITCFJ182A },
	  0x1cc },

	{ { PCMCIA_VENDOR_FUJITSU, PCMCIA_PRODUCT_FUJITSU_LA10S,
	    PCMCIA_CIS_FUJITSU_LA10S },
	  -1 },

	{ { PCMCIA_VENDOR_RATOC, PCMCIA_PRODUCT_RATOC_REX_R280,
	    PCMCIA_CIS_RATOC_REX_R280 },
	  0x1fc },
};
static const size_t mbe_pcmcia_nproducts =
    sizeof(mbe_pcmcia_products) / sizeof(mbe_pcmcia_products[0]);

int
mbe_pcmcia_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pcmcia_attach_args *pa = aux;

	if (pcmcia_product_lookup(pa, mbe_pcmcia_products, mbe_pcmcia_nproducts,
	    sizeof(mbe_pcmcia_products[0]), NULL))
		return (1);
	return (0);
}

int
mbe_pcmcia_validate_config(cfe)
	struct pcmcia_config_entry *cfe;
{
	if (cfe->iftype != PCMCIA_IFTYPE_IO ||
	    cfe->num_iospace < 1)
		return (EINVAL);
	return (0);
}

void
mbe_pcmcia_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct mbe_pcmcia_softc *psc = (void *)self;
	struct mb86960_softc *sc = &psc->sc_mb86960;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
	struct mbe_pcmcia_get_enaddr_args pgea;
	const struct mbe_pcmcia_product *mpp;
	int error;

	psc->sc_pf = pa->pf;

	error = pcmcia_function_configure(pa->pf, mbe_pcmcia_validate_config);
	if (error) {
		aprint_error("%s: configure failed, error=%d\n", self->dv_xname,
		    error);
		return;
	}

	cfe = pa->pf->cfe;
	sc->sc_bst = cfe->iospace[0].handle.iot;
	sc->sc_bsh = cfe->iospace[0].handle.ioh;

	mpp = pcmcia_product_lookup(pa, mbe_pcmcia_products,
	    mbe_pcmcia_nproducts, sizeof(mbe_pcmcia_products[0]), NULL);
	if (!mpp)
		panic("mbe_pcmcia_attach: impossible");

	/* Read station address from io/mem or CIS. */
	if (mpp->mpp_enet_maddr >= 0) {
		pgea.maddr = mpp->mpp_enet_maddr;
		if (mbe_pcmcia_get_enaddr_from_mem(psc, &pgea) != 0) {
			printf("%s: couldn't get ethernet address "
			    "from memory\n", self->dv_xname);
			goto fail;
		}
	} else if (mpp->mpp_flags & MBH10302) {
		bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_MBH0 ,
				  FE_MBH0_MASK | FE_MBH0_INTR_ENABLE);
		if (mbe_pcmcia_get_enaddr_from_io(psc, &pgea) != 0) {
			printf("%s: couldn't get ethernet address from i/o\n",
			    self->dv_xname);
			goto fail;
		}
	} else {
		if (pa->pf->pf_funce_lan_nidlen != ETHER_ADDR_LEN) {
			printf("%s: couldn't get ethernet address from CIS\n",
			    self->dv_xname);
			goto fail;
		}
		memcpy(pgea.enaddr, pa->pf->pf_funce_lan_nid, ETHER_ADDR_LEN);
	}

	/* Perform generic initialization. */
	if (mpp->mpp_flags & MBH10302)
		sc->sc_flags |= FE_FLAGS_MB86960;

	sc->sc_enable = mbe_pcmcia_enable;
	sc->sc_disable = mbe_pcmcia_disable;

	error = mbe_pcmcia_enable(sc);
	if (error)
		goto fail;

	mb86960_attach(sc, pgea.enaddr);
	mb86960_config(sc, NULL, 0, 0);

	mbe_pcmcia_disable(sc);
	psc->sc_state = MBE_PCMCIA_ATTACHED;
	return;

fail:
	pcmcia_function_unconfigure(pa->pf);
}

int
mbe_pcmcia_detach(self, flags)
	struct device *self;
	int flags;
{
	struct mbe_pcmcia_softc *psc = (void *)self;
	int error;

	if (psc->sc_state != MBE_PCMCIA_ATTACHED)
		return (0);

	error = mb86960_detach(&psc->sc_mb86960);
	if (error)
		return (error);

	pcmcia_function_unconfigure(psc->sc_pf);

	return (0);
}

int
mbe_pcmcia_enable(sc)
	struct mb86960_softc *sc;
{
	struct mbe_pcmcia_softc *psc = (void *)sc;
	int error;

	/* Establish the interrupt handler. */
	psc->sc_ih = pcmcia_intr_establish(psc->sc_pf, IPL_NET, mb86960_intr,
	    sc);
	if (!psc->sc_ih)
		return (EIO);

	error = pcmcia_function_enable(psc->sc_pf);
	if (error) {
		pcmcia_intr_disestablish(psc->sc_pf, psc->sc_ih);
		psc->sc_ih = 0;
	}

	return (error);
}

void
mbe_pcmcia_disable(sc)
	struct mb86960_softc *sc;
{
	struct mbe_pcmcia_softc *psc = (void *)sc;

	pcmcia_function_disable(psc->sc_pf);
	pcmcia_intr_disestablish(psc->sc_pf, psc->sc_ih);
	psc->sc_ih = 0;
}

int
mbe_pcmcia_get_enaddr_from_io(psc, ea)
	struct mbe_pcmcia_softc *psc;
	struct mbe_pcmcia_get_enaddr_args *ea;
{                       
	struct mb86960_softc *sc = &psc->sc_mb86960;
	int i;

	for (i = 0; i < ETHER_ADDR_LEN; i++)
		ea->enaddr[i] = bus_space_read_1(sc->sc_bst, sc->sc_bsh,
		    FE_MBH_ENADDR + i);
	return (0);
}

int
mbe_pcmcia_get_enaddr_from_mem(psc, ea)
	struct mbe_pcmcia_softc *psc;
	struct mbe_pcmcia_get_enaddr_args *ea;
{
	struct mb86960_softc *sc = &psc->sc_mb86960;
	struct pcmcia_mem_handle pcmh;
	bus_size_t offset;
	int i, mwindow, rv = 1;

	if (ea->maddr < 0)
		goto bad_memaddr;

	if (pcmcia_mem_alloc(psc->sc_pf, ETHER_ADDR_LEN * 2, &pcmh)) {
		printf("%s: can't alloc mem for enet addr\n", 
		    sc->sc_dev.dv_xname);
		goto memalloc_failed;
	}

	if (pcmcia_mem_map(psc->sc_pf, PCMCIA_MEM_ATTR, ea->maddr,
	    ETHER_ADDR_LEN * 2, &pcmh, &offset, &mwindow)) {
		printf("%s: can't map mem for enet addr\n", 
		    sc->sc_dev.dv_xname);
		goto memmap_failed;
	}

	for (i = 0; i < ETHER_ADDR_LEN; i++)
		ea->enaddr[i] = bus_space_read_1(pcmh.memt, pcmh.memh,
		    offset + (i * 2));

	rv = 0;
	pcmcia_mem_unmap(psc->sc_pf, mwindow);
memmap_failed:
	pcmcia_mem_free(psc->sc_pf, &pcmh);
memalloc_failed:
bad_memaddr:

	return (rv);
}
