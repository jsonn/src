/*	$NetBSD: tqphy.c,v 1.2.8.1 1999/12/27 18:35:13 wrstuden Exp $	*/

/*
 * Copyright (c) 1998, 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 * Copyright (c) 1997 Manuel Bouyer.  All rights reserved.
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
 *	This product includes software developed by Manuel Bouyer.
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

/*
 * TDK TSC78Q2120 PHY driver
 *
 * Documentation available at http://www.tsc.tdk.com/lan/78Q2120.pdf .
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/miidevs.h>

#include <dev/mii/tqphyreg.h>

int	tqphymatch __P((struct device *, struct cfdata *, void *));
void	tqphyattach __P((struct device *, struct device *, void *));

struct cfattach tqphy_ca = {
	sizeof(struct mii_softc), tqphymatch, tqphyattach
};

int	tqphy_service __P((struct mii_softc *, struct mii_data *, int));
void	tqphy_status __P((struct mii_softc *));

int
tqphymatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct mii_attach_args *ma = aux;

	if (MII_OUI(ma->mii_id1, ma->mii_id2) == MII_OUI_TSC)
		switch MII_MODEL(ma->mii_id2) {
		case MII_MODEL_TSC_78Q2120:
		/* case MII_MODEL_TSC_78Q2121: */
			return (10);
	}

	return (0);
}

void
tqphyattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct mii_softc *sc = (struct mii_softc *)self;
	struct mii_attach_args *ma = aux;
	struct mii_data *mii = ma->mii_data;

	printf(": %s, rev. %d\n", MII_STR_TSC_78Q2120,
	    MII_REV(ma->mii_id2));

	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_service = tqphy_service;
	sc->mii_pdata = mii;

	/*
	 * Apparently, we can't do loopback on this PHY.
	 */
	sc->mii_flags |= MIIF_NOLOOP;

	mii_phy_reset(sc);

	sc->mii_capabilities =
	    PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	printf("%s: ", sc->mii_dev.dv_xname);
	if ((sc->mii_capabilities & BMSR_MEDIAMASK) == 0)
		printf("no media present");
	else
		mii_add_media(sc);
	printf("\n");
}

int
tqphy_service(sc, mii, cmd)
	struct mii_softc *sc;
	struct mii_data *mii;
	int cmd;
{
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int reg;

	switch (cmd) {
	case MII_POLLSTAT:
		/*
		 * If we're not polling our PHY instance, just return.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst)
			return (0);
		break;

	case MII_MEDIACHG:
		/*
		 * If the media indicates a different PHY instance,
		 * isolate ourselves.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst) {
			reg = PHY_READ(sc, MII_BMCR);
			PHY_WRITE(sc, MII_BMCR, reg | BMCR_ISO);
			return (0);
		}

		/*
		 * If the interface is not up, don't do anything.
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			break;

		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_AUTO:
			/*
			 * If we're already in auto mode, just return.
			 */
			if (PHY_READ(sc, MII_BMCR) & BMCR_AUTOEN)
				return (0);
			(void) mii_phy_auto(sc, 1);
			break;
		default:
			mii_phy_setmedia(sc);
		}
		break;

	case MII_TICK:
		/*
		 * If we're not currently selected, just return.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst)
			return (0);

		/*
		 * Only used for autonegotiation.
		 */
		if (IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO)
			return (0);

		/*
		 * Is the interface even up?
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			return (0);

		/*
		 * Check to see if we have link.  If we do, we don't
		 * need to restart the autonegotiation process.  Read
		 * the BMSR twice in case it's latched.
		 */
		reg = PHY_READ(sc, MII_BMSR) |
		    PHY_READ(sc, MII_BMSR);
		if (reg & BMSR_LINK)
			return (0);

		/*
		 * Only retry autonegotiation every 5 seconds.
		 */
		if (++sc->mii_ticks != 5)
			return (0);

		sc->mii_ticks = 0;
		mii_phy_reset(sc);
		if (mii_phy_auto(sc, 0) == EJUSTRETURN)
			return (0);
		break;

	case MII_DOWN:
		mii_phy_down(sc);
		return (0);
	}

	/* Update the media status. */
	tqphy_status(sc);

	/* Callback if something changed. */
	if (sc->mii_active != mii->mii_media_active || cmd == MII_MEDIACHG) {
		(*mii->mii_statchg)(sc->mii_dev.dv_parent);
		sc->mii_active = mii->mii_media_active;
	}
	return (0);
}

void
tqphy_status(sc)
	struct mii_softc *sc;
{
	struct mii_data *mii = sc->mii_pdata;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int bmsr, bmcr, diag;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmsr = PHY_READ(sc, MII_BMSR) |
	    PHY_READ(sc, MII_BMSR);
	if (bmsr & BMSR_LINK)
		mii->mii_media_status |= IFM_ACTIVE;
 
	bmcr = PHY_READ(sc, MII_BMCR);
	if (bmcr & BMCR_ISO) {
		mii->mii_media_active |= IFM_NONE;
		mii->mii_media_status = 0;
		return;
	}
 
	if (bmcr & BMCR_LOOP)
		mii->mii_media_active |= IFM_LOOP;
 
	if (bmcr & BMCR_AUTOEN) {
		if ((bmsr & BMSR_ACOMP) == 0) { 
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}
		diag = PHY_READ(sc, MII_TQPHY_DIAG);
		if (diag & DIAG_RATE)
			mii->mii_media_active |= IFM_100_TX;
		else
			mii->mii_media_active |= IFM_10_T;
		if (diag & DIAG_DPLX)
			mii->mii_media_active |= IFM_FDX;
	} else 
		mii->mii_media_active = ife->ifm_media;
}
