/*	$NetBSD: if_ae_nubus.c,v 1.6.2.2 1997/03/12 15:08:37 is Exp $	*/

/*
 * Copyright (C) 1997 Scott Reynolds
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
 *      This product includes software developed by Scott Reynolds for
 *      the NetBSD Project.
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

#include <sys/param.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/if_ether.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#endif

#include <machine/bus.h>
#include <machine/viareg.h>

#include "nubus.h"
#include <dev/ic/dp8390reg.h>
#include "if_aereg.h"
#include "if_aevar.h"

static int	ae_nubus_match __P((struct device *, struct cfdata *, void *));
static void	ae_nubus_attach __P((struct device *, struct device *, void *));
static int	ae_nb_card_vendor __P((struct nubus_attach_args *));
static int	ae_nb_get_enaddr __P((struct nubus_attach_args *, u_int8_t *));
static void	ae_nb_watchdog __P((struct ifnet *));

struct cfattach ae_nubus_ca = {
	sizeof(struct ae_softc), ae_nubus_match, ae_nubus_attach
};

static int
ae_nubus_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct nubus_attach_args *na = (struct nubus_attach_args *) aux;
	bus_space_handle_t bsh;
	int rv;

	if (bus_space_map(na->na_tag, NUBUS_SLOT2PA(na->slot), NBMEMSIZE,
	    0, &bsh))
		return (0);

	rv = 0;

	if (na->category == NUBUS_CATEGORY_NETWORK &&
	    na->type == NUBUS_TYPE_ETHERNET) {
		switch (ae_nb_card_vendor(na)) {
		case AE_VENDOR_APPLE:
		case AE_VENDOR_ASANTE:
		case AE_VENDOR_FARALLON:
		case AE_VENDOR_INTERLAN:
		case AE_VENDOR_KINETICS:
			rv = 1;
			break;
		case AE_VENDOR_DAYNA:
		case AE_VENDOR_FOCUS:
			rv = UNSUPP;
			break;
		default:
			break;
		}
	}

	bus_space_unmap(na->na_tag, bsh, NBMEMSIZE);

	return rv;
}

/*
 * Install interface into kernel networking data structures
 */
static void
ae_nubus_attach(parent, self, aux)
	struct device *parent, *self;
	void   *aux;
{
	struct ae_softc *sc = (struct ae_softc *) self;
	struct nubus_attach_args *na = (struct nubus_attach_args *) aux;
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	int success;
#ifdef AE_OLD_GET_ENADDR
	int i;
#endif
	u_int8_t myaddr[ETHER_ADDR_LEN];

	bst = na->na_tag;
	if (bus_space_map(bst, NUBUS_SLOT2PA(na->slot), NBMEMSIZE,
	    0, &bsh)) {
		printf(": can't map memory space\n");
		return;
	}

	sc->sc_regt = sc->sc_buft = bst;
	sc->sc_flags = self->dv_cfdata->cf_flags;
	sc->regs_rev = 0;
	sc->use16bit = 1;
	sc->vendor = ae_nb_card_vendor(na);
	strncpy(sc->type_str, nubus_get_card_name(na->fmt),
	    INTERFACE_NAME_LEN);
	sc->type_str[INTERFACE_NAME_LEN-1] = '\0';
	sc->mem_size = 0;

	success = 0;

	switch (sc->vendor) {
	case AE_VENDOR_APPLE:	/* Apple-compatible cards */
	case AE_VENDOR_ASANTE:
		sc->regs_rev = 1;
		if (bus_space_subregion(bst, bsh,
		    AE_REG_OFFSET, AE_REG_SIZE, &sc->sc_regh)) {
			printf(": failed to map register space\n");
			break;
		}
		if ((sc->mem_size = ae_size_card_memory(bst, bsh,
		    AE_DATA_OFFSET)) == 0) {
			printf(": failed to determine size of RAM.\n");
			break;
		}
		if (bus_space_subregion(bst, bsh,
		    AE_DATA_OFFSET, sc->mem_size, &sc->sc_bufh)) {
			printf(": failed to map register space\n");
			break;
		}
#ifdef AE_OLD_GET_ENADDR
		/* Get station address from on-board ROM */
		for (i = 0; i < ETHER_ADDR_LEN; ++i)
			myaddr[i] =
			    bus_space_read_1(bst, bsh, (AE_ROM_OFFSET + i * 2));
#else
		if (ae_nb_get_enaddr(na, myaddr)) {
			printf(": can't find MAC address\n");
			break;
		}
#endif

		success = 1;
		break;

	case AE_VENDOR_DAYNA:
		if (bus_space_subregion(bst, bsh,
		    DP_REG_OFFSET, AE_REG_SIZE, &sc->sc_regh)) {
			printf(": failed to map register space\n");
			break;
		}
		sc->mem_size = 8192;
		if (bus_space_subregion(bst, bsh,
		    DP_DATA_OFFSET, sc->mem_size, &sc->sc_bufh)) {
			printf(": failed to map register space\n");
			break;
		}
#ifdef AE_OLD_GET_ENADDR
		/* Get station address from on-board ROM */
		for (i = 0; i < ETHER_ADDR_LEN; ++i)
			myaddr[i] =
			    bus_space_read_1(bst, bsh, (DP_ROM_OFFSET + i * 2));
#else
		if (ae_nb_get_enaddr(na, myaddr)) {
			printf(": can't find MAC address\n");
			break;
		}
#endif

		printf(": unsupported Dayna hardware\n");
		break;

	case AE_VENDOR_FARALLON:
		sc->regs_rev = 1;
		if (bus_space_subregion(bst, bsh,
		    AE_REG_OFFSET, AE_REG_SIZE, &sc->sc_regh)) {
			printf(": failed to map register space\n");
			break;
		}
		if ((sc->mem_size = ae_size_card_memory(bst, bsh,
		    AE_DATA_OFFSET)) == 0) {
			printf(": failed to determine size of RAM.\n");
			break;
		}
		if (bus_space_subregion(bst, bsh,
		    AE_DATA_OFFSET, sc->mem_size, &sc->sc_bufh)) {
			printf(": failed to map register space\n");
			break;
		}
#ifdef AE_OLD_GET_ENADDR
		/* Get station address from on-board ROM */
		for (i = 0; i < ETHER_ADDR_LEN; ++i)
			myaddr[i] =
			    bus_space_read_1(bst, bsh, (FE_ROM_OFFSET + i));
#else
		if (ae_nb_get_enaddr(na, myaddr)) {
			printf(": can't find MAC address\n");
			break;
		}
#endif

		success = 1;
		break;

	case AE_VENDOR_FOCUS:
		printf(": unsupported Focus hardware\n");
		break;

	case AE_VENDOR_INTERLAN:
		if (bus_space_subregion(bst, bsh,
		    GC_REG_OFFSET, AE_REG_SIZE, &sc->sc_regh)) {
			printf(": failed to map register space\n");
			break;
		}
		if ((sc->mem_size = ae_size_card_memory(bst, bsh,
		    GC_DATA_OFFSET)) == 0) {
			printf(": failed to determine size of RAM.\n");
			break;
		}
		if (bus_space_subregion(bst, bsh,
		    GC_DATA_OFFSET, sc->mem_size, &sc->sc_bufh)) {
			printf(": failed to map register space\n");
			break;
		}

		/* reset the NIC chip */
		bus_space_write_1(bst, bsh, GC_RESET_OFFSET, 0);

#ifdef AE_OLD_GET_ENADDR
		/* Get station address from on-board ROM */
		for (i = 0; i < ETHER_ADDR_LEN; ++i)
			myaddr[i] =
			    bus_space_read_1(bst, bsh, (GC_ROM_OFFSET + i * 4));
#else
		if (ae_nb_get_enaddr(na, myaddr)) {
			printf(": can't find MAC address\n");
			break;
		}
#endif

		success = 1;
		break;

	case AE_VENDOR_KINETICS:
		sc->use16bit = 0;
		if (bus_space_subregion(bst, bsh,
		    KE_REG_OFFSET, AE_REG_SIZE, &sc->sc_regh)) {
			printf(": failed to map register space\n");
			break;
		}
		if ((sc->mem_size = ae_size_card_memory(bst, bsh,
		    KE_DATA_OFFSET)) == 0) {
			printf(": failed to determine size of RAM.\n");
			break;
		}
		if (bus_space_subregion(bst, bsh,
		    KE_DATA_OFFSET, sc->mem_size, &sc->sc_bufh)) {
			printf(": failed to map register space\n");
			break;
		}
		if (ae_nb_get_enaddr(na, myaddr)) {
			printf(": can't find MAC address\n");
			break;
		}

		success = 1;
		break;

	default:
		break;
	}

	if (!success) {
		bus_space_unmap(bst, bsh, NBMEMSIZE);
		return;
	}

	ifp->if_watchdog = ae_nb_watchdog;	/* Override watchdog */
	if (aesetup(sc, myaddr)) {
		bus_space_unmap(bst, bsh, NBMEMSIZE);
		return;
	}

	/* make sure interrupts are vectored to us */
	add_nubus_intr(na->slot, aeintr, sc);

#ifdef MAC68K_BROKEN_VIDEO
	/*
	 * XXX -- enable nubus interrupts here.  Should be done elsewhere,
	 *        but that currently breaks with some nubus video cards'
	 *	  interrupts.  So we only enable nubus interrupts if we
	 *	  have an ethernet card...  i.e., we do it here.
	 */
	enable_nubus_intr();
#endif
}

static int
ae_nb_card_vendor(na)
	struct nubus_attach_args *na;
{
	int vendor;

	switch (na->drsw) {
	case NUBUS_DRSW_3COM:
	case NUBUS_DRSW_APPLE:
	case NUBUS_DRSW_TECHWORKS:
		vendor = AE_VENDOR_APPLE;
		break;
	case NUBUS_DRSW_ASANTE:
		vendor = AE_VENDOR_ASANTE;
		break;
	case NUBUS_DRSW_FARALLON:
		vendor = AE_VENDOR_FARALLON;
		break;
	case NUBUS_DRSW_FOCUS:
		vendor = AE_VENDOR_FOCUS;
		break;
	case NUBUS_DRSW_GATOR:
		switch (na->drhw) {
		default:
		case NUBUS_DRHW_INTERLAN:
			vendor = AE_VENDOR_INTERLAN;
			break;
		case NUBUS_DRHW_KINETICS:
			if (strncmp(
			    nubus_get_card_name(na->fmt), "EtherPort", 9) == 0)
				vendor = AE_VENDOR_KINETICS;
			else
				vendor = AE_VENDOR_DAYNA;
			break;
		}
		break;
	default:
#ifdef AE_DEBUG
		printf("Unknown ethernet drsw: %x\n", na->drsw);
#endif
		vendor = AE_VENDOR_UNKNOWN;
	}
	return vendor;
}

static int
ae_nb_get_enaddr(na, ep)
	struct nubus_attach_args *na;
	u_int8_t *ep;
{
	nubus_dir dir;
	nubus_dirent dirent;

	/*
	 * XXX - note hardwired resource IDs here (0x80); these are
	 * assumed to be used by all cards, but should be fixed when
	 * we find out more about Ethernet card resources.
	 */
	nubus_get_main_dir(na->fmt, &dir);
	if (nubus_find_rsrc(na->fmt, &dir, 0x80, &dirent) <= 0)
		return 1;
	nubus_get_dir_from_rsrc(na->fmt, &dirent, &dir);
	if (nubus_find_rsrc(na->fmt, &dir, 0x80, &dirent) <= 0)
		return 1;
	if (nubus_get_ind_data(na->fmt, &dirent, ep, ETHER_ADDR_LEN) <= 0)
		return 1;

	return 0;
}

static void
ae_nb_watchdog(ifp)
	struct ifnet *ifp;
{
	struct ae_softc *sc = ifp->if_softc;

#if 1
/*
 * This is a kludge!  The via code seems to miss slot interrupts
 * sometimes.  This kludges around that by calling the handler
 * by hand if the watchdog is activated. -- XXX (akb)
 */
	(*via2itab[1])((void *) 1);
#endif

	log(LOG_ERR, "%s: device timeout\n", sc->sc_dev.dv_xname);
	++ifp->if_oerrors;

	aereset(sc);
}
