/*	$NetBSD: if_ate_mca.c,v 1.2.2.1 2002/01/10 19:55:59 thorpej Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jaromir Dolecek.
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
 * Driver for ATI AT1720X MCA cards based on Fujitsu MB8696xA controller.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_ate_mca.c,v 1.2.2.1 2002/01/10 19:55:59 thorpej Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/ic/mb86960reg.h>
#include <dev/ic/mb86960var.h>
#include <dev/ic/ate_subr.h>

#include <dev/mca/mcavar.h>
#include <dev/mca/mcadevs.h>
#include <dev/isa/if_fereg.h>	/* XXX */

int	ate_mca_match __P((struct device *, struct cfdata *, void *));
void	ate_mca_attach __P((struct device *, struct device *, void *));
static void ate_mca_detect __P((bus_space_tag_t, bus_space_handle_t,
    u_int8_t enaddr[ETHER_ADDR_LEN]));

#define ATE_NPORTS 0x20

struct ate_softc {
	struct	mb86960_softc sc_mb86960;	/* real "mb86960" softc */

	/* MCA-specific goo. */
	void	*sc_ih;				/* interrupt cookie */
};

struct cfattach ate_mca_ca = {
	sizeof(struct ate_softc), ate_mca_match, ate_mca_attach
};

static const struct ate_mca_product {
	u_int32_t	at_prodid;	/* MCA product ID */
	const char	*at_name;	/* device name */
	int		at_type;	/* device type */
} ate_mca_products[] = {
	{ MCA_PRODUCT_AT1720T,	"ATI AT1720T",	FE_TYPE_AT1700T		},
	{ MCA_PRODUCT_AT1720BT,	"ATI AT1720BT",	FE_TYPE_AT1700BT	},
	{ MCA_PRODUCT_AT1720AT, "ATI AT1720AT",	FE_TYPE_AT1700AT	},
	{ MCA_PRODUCT_AT1720FT, "ATI AT1720FT",	FE_TYPE_AT1700FT	},
	{ 0, 	},
};

static const struct ate_mca_product *ate_mca_lookup __P((u_int32_t));

static const struct ate_mca_product *
ate_mca_lookup(id)
	u_int32_t id;
{
	const struct ate_mca_product *atp;

	for (atp = ate_mca_products; atp->at_name != NULL; atp++)
		if (id == atp->at_prodid)
			return (atp);

	return (NULL);
}

int
ate_mca_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct mca_attach_args *ma = (struct mca_attach_args *) aux;

	if (ate_mca_lookup(ma->ma_id) != NULL)
		return (1);

	return (0);
}

/* see POS diagrams below for explanation of these arrays' contents */
static const int ats_iobase[] = {
	0x400, 0x2400, 0x4400, 0x6400, 0x1400, 0x3400, 0x5400, 0x7400
};
static const int ats_irq[] = {
	3, 4, 5, 9, 10, 11, 12, 15
};

void
ate_mca_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ate_softc *isc = (struct ate_softc *)self;
	struct mb86960_softc *sc = &isc->sc_mb86960;
	struct mca_attach_args *ma = aux;
	bus_space_tag_t iot = ma->ma_iot;
	bus_space_handle_t ioh;
	u_int8_t myea[ETHER_ADDR_LEN];
	int type, pos3, pos4;
	int iobase, irq;
	const struct ate_mca_product *atp;

	pos3 = mca_conf_read(ma->ma_mc, ma->ma_slot, 3);
	pos4 = mca_conf_read(ma->ma_mc, ma->ma_slot, 4);

	/*
	 * POS register 2: (adf pos0)
	 * 7 6 5 4 3 2 1 0
	 *               \__ enable: 0=adapter disabled, 1=adapter enabled
	 *
	 * POS register 3: (adf pos1)
	 * 
	 * 7 6 5 4 3 2 1 0
	 * \_/ \___/ \___/
	 *   \     \     \__ I/O Port Addresses: 000=0x400-0x4FF 100=0x1400-
	 *    \     \          001=0x2400 101=0x3400 010=0x4400 110=0x5400
	 *     \     \         011=0x6400 111=0x7400
	 *      \     \_____ Boot ROM Memory Address
	 *       \
	 *        \_________ Lower 2 bit of Interrupt Request Number
	 *
	 * POS register 4: (adf pos2)
	 * 
	 * 7 6 5 4 3 2 1 0
	 *   \      \_______ Twisted Pair Type: 0=100 ohm, Unshielded
	 *    \                1=150 ohm, Shielded
	 *     \____________ Higher 1 bit of Interrupt Request Number:
	 *                   000=3 001=4 010=5 011=9 100=10 101=11 110=12 111=15
	 */

	atp = ate_mca_lookup(ma->ma_id);
#ifdef DIAGNOSTIC
	if (atp == NULL) {
		printf("\n%s: where did the card go?\n", sc->sc_dev.dv_xname);
		return;
	}
#endif

	iobase = ats_iobase[pos3 & 0x7];
	irq = ats_irq[((pos4 & 0x40) >> 4) | ((pos3 & 0xc0) >> 6)];

	printf(" slot %d irq %d: %s\n", ma->ma_slot + 1, irq, atp->at_name);

	/* Map i/o space. */
	if (bus_space_map(iot, iobase, ATE_NPORTS, 0, &ioh)) {
		printf("%s: can't map i/o space\n", sc->sc_dev.dv_xname);
		return;
	}

	sc->sc_bst = iot;
	sc->sc_bsh = ioh;

	/* Determine the card type and get ethernet address. */
	ate_mca_detect(iot, ioh, myea);
	type = atp->at_type;

	/* This interface is always enabled. */
	sc->sc_flags |= FE_FLAGS_ENABLED;

	/*
	 * Do generic MB86960 attach.
	 */
	mb86960_attach(sc, MB86960_TYPE_86965, myea);

	mb86960_config(sc, NULL, 0, 0);

	/* Establish the interrupt handler. */
	isc->sc_ih = mca_intr_establish(ma->ma_mc, irq, IPL_NET,
			mb86960_intr, sc);
	if (isc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt handler\n",
		    sc->sc_dev.dv_xname);
		return;
	}
}

/*
 * Very simplified ate_detect() from dev/isa/if_ate.c, only
 * to determine ethernet address.
 */
static void
ate_mca_detect(iot, ioh, enaddr)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int8_t enaddr[ETHER_ADDR_LEN];
{
	u_char eeprom[FE_EEPROM_SIZE];

	/* Get our station address from EEPROM. */
	ate_read_eeprom(iot, ioh, eeprom);
	bcopy(eeprom + FE_ATI_EEP_ADDR, enaddr, ETHER_ADDR_LEN);
}
