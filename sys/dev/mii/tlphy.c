/*	$NetBSD: tlphy.c,v 1.18.2.2 2001/04/21 17:49:05 bouyer Exp $	*/

/*-
 * Copyright (c) 1998, 1999, 2000 The NetBSD Foundation, Inc.
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
 * Driver for Texas Instruments's ThunderLAN PHYs
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/errno.h>

#include <machine/bus.h>

#include <net/if.h>
#include <net/if_media.h>

#include <net/if_ether.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/miidevs.h>

#include <dev/i2c/i2c_bus.h>

#include <dev/mii/tlphyreg.h>
#include <dev/mii/tlphyvar.h>

/* ThunderLAN PHY can only be on a ThunderLAN */
#include <dev/pci/if_tlvar.h>

struct tlphy_softc {
	struct mii_softc sc_mii;		/* generic PHY */
	int sc_tlphycap;
	int sc_need_acomp;
};

int	tlphymatch __P((struct device *, struct cfdata *, void *));
void	tlphyattach __P((struct device *, struct device *, void *));

struct cfattach tlphy_ca = {
	sizeof(struct tlphy_softc), tlphymatch, tlphyattach, mii_phy_detach,
	    mii_phy_activate
};

int	tlphy_service __P((struct mii_softc *, struct mii_data *, int));
int	tlphy_auto __P((struct tlphy_softc *, int));
void	tlphy_acomp __P((struct tlphy_softc *));
void	tlphy_status __P((struct mii_softc *));

const struct mii_phy_funcs tlphy_funcs = {
	tlphy_service, tlphy_status, mii_phy_reset,
};

int
tlphymatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct mii_attach_args *ma = aux;       

	if (MII_OUI(ma->mii_id1, ma->mii_id2) == MII_OUI_TI &&
	    MII_MODEL(ma->mii_id2) == MII_MODEL_TI_TLAN10T)
		return (10);

	return (0);
}

void
tlphyattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct tlphy_softc *sc = (struct tlphy_softc *)self;
	struct tl_softc *tlsc = (struct tl_softc *)self->dv_parent;
	struct mii_attach_args *ma = aux;
	struct mii_data *mii = ma->mii_data;
	const char *sep = "";

	printf(": %s, rev. %d\n", MII_STR_TI_TLAN10T,
	    MII_REV(ma->mii_id2));

	sc->sc_mii.mii_inst = mii->mii_instance;
	sc->sc_mii.mii_phy = ma->mii_phyno;
	sc->sc_mii.mii_funcs = &tlphy_funcs;
	sc->sc_mii.mii_pdata = mii;
	sc->sc_mii.mii_flags = mii->mii_flags;

	PHY_RESET(&sc->sc_mii);

	/*
	 * Note that if we're on a device that also supports 100baseTX,
	 * we are not going to want to use the built-in 10baseT port,
	 * since there will be another PHY on the MII wired up to the
	 * UTP connector.  The parent indicates this to us by specifying
	 * the TLPHY_MEDIA_NO_10_T bit.
	 */
	sc->sc_tlphycap = tlsc->tl_product->tp_tlphymedia;
	if ((sc->sc_tlphycap & TLPHY_MEDIA_NO_10_T) == 0)
		sc->sc_mii.mii_capabilities =
		    PHY_READ(&sc->sc_mii, MII_BMSR) & ma->mii_capmask;
	else
		sc->sc_mii.mii_capabilities = 0;


#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)
#define	PRINT(s)	printf("%s%s", sep, s); sep = ", "

	printf("%s: ", sc->sc_mii.mii_dev.dv_xname);
	if (sc->sc_tlphycap) {
		if (sc->sc_tlphycap & TLPHY_MEDIA_10_2) {
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_2, 0,
			    sc->sc_mii.mii_inst), 0);
			PRINT("10base2");
		} else if (sc->sc_tlphycap & TLPHY_MEDIA_10_5) {
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_5, 0,
			    sc->sc_mii.mii_inst), 0);
			PRINT("10base5");
		}
	}
	if (sc->sc_mii.mii_capabilities & BMSR_MEDIAMASK) {
		printf("%s", sep);
		mii_phy_add_media(&sc->sc_mii);
	} else if ((sc->sc_tlphycap &
		    (TLPHY_MEDIA_10_2 | TLPHY_MEDIA_10_5)) == 0)
		printf("no media present");
	printf("\n");
#undef ADD
#undef PRINT
}

int
tlphy_service(self, mii, cmd)
	struct mii_softc *self;
	struct mii_data *mii;
	int cmd;
{
	struct tlphy_softc *sc = (struct tlphy_softc *)self;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int reg;

	if ((sc->sc_mii.mii_dev.dv_flags & DVF_ACTIVE) == 0)
		return (ENXIO);

	if ((sc->sc_mii.mii_flags & MIIF_DOINGAUTO) == 0 && sc->sc_need_acomp)
		tlphy_acomp(sc);

	switch (cmd) {
	case MII_POLLSTAT:
		/*
		 * If we're not polling our PHY instance, just return.
		 */
		if (IFM_INST(ife->ifm_media) != sc->sc_mii.mii_inst)
			return (0);
		break;

	case MII_MEDIACHG:
		/*
		 * If the media indicates a different PHY instance,
		 * isolate ourselves.
		 */
		if (IFM_INST(ife->ifm_media) != sc->sc_mii.mii_inst) {
			reg = PHY_READ(&sc->sc_mii, MII_BMCR);
			PHY_WRITE(&sc->sc_mii, MII_BMCR, reg | BMCR_ISO);
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
			 * The ThunderLAN PHY doesn't self-configure after
			 * an autonegotiation cycle, so there's no such
			 * thing as "already in auto mode".
			 */
			(void) tlphy_auto(sc, 1);
			break;
		case IFM_10_2:
		case IFM_10_5:
			PHY_WRITE(&sc->sc_mii, MII_BMCR, 0);
			PHY_WRITE(&sc->sc_mii, MII_TLPHY_CTRL, CTRL_AUISEL);
			delay(100000);
			break;
		default:
			PHY_WRITE(&sc->sc_mii, MII_TLPHY_CTRL, 0);
			delay(100000);
			mii_phy_setmedia(&sc->sc_mii);
		}
		break;

	case MII_TICK:
		/*
		 * If we're not currently selected, just return.
		 */
		if (IFM_INST(ife->ifm_media) != sc->sc_mii.mii_inst)
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
		 * XXX WHAT ABOUT CHECKING LINK ON THE BNC/AUI?!
		 */

		if (mii_phy_tick(&sc->sc_mii) == EJUSTRETURN)
			return (0);
		break;

	case MII_DOWN:
		mii_phy_down(&sc->sc_mii);
		return (0);
	}

	/* Update the media status. */
	mii_phy_status(&sc->sc_mii);

	/* Callback if something changed. */
	mii_phy_update(&sc->sc_mii, cmd);
	return (0);
}

void
tlphy_status(physc)
	struct mii_softc *physc;
{
	struct tlphy_softc *sc = (void *) physc;
	struct mii_data *mii = sc->sc_mii.mii_pdata;
	struct tl_softc *tlsc = (struct tl_softc *)sc->sc_mii.mii_dev.dv_parent;
	int bmsr, bmcr, tlctrl;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmcr = PHY_READ(&sc->sc_mii, MII_BMCR);
	if (bmcr & BMCR_ISO) {
		mii->mii_media_active |= IFM_NONE;
		mii->mii_media_status = 0;  
		return;
	}

	tlctrl = PHY_READ(&sc->sc_mii, MII_TLPHY_CTRL);
	if (tlctrl & CTRL_AUISEL) {
		if (sc->sc_tlphycap & TLPHY_MEDIA_10_2)
			mii->mii_media_active |= IFM_10_2;
		else if (sc->sc_tlphycap & TLPHY_MEDIA_10_5)
			mii->mii_media_active |= IFM_10_5;
		else
			printf("%s: AUI selected with no matching media !\n",
			    sc->sc_mii.mii_dev.dv_xname);
		if (tlsc->tl_flags & TL_IFACT)
			mii->mii_media_status |= IFM_ACTIVE;
		return;
	}

	bmsr = PHY_READ(&sc->sc_mii, MII_BMSR) |
	    PHY_READ(&sc->sc_mii, MII_BMSR);
	if (bmsr & BMSR_LINK)   
		mii->mii_media_status |= IFM_ACTIVE;

	if (bmcr & BMCR_LOOP)
		mii->mii_media_active |= IFM_LOOP;

	/*
	 * Grr, braindead ThunderLAN PHY doesn't have any way to
	 * tell which media is actually active.  (Note it also
	 * doesn't self-configure after autonegotiation.)  We
	 * just have to report what's in the BMCR.
	 */
	if (bmcr & BMCR_FDX)
		mii->mii_media_active |= IFM_FDX;
	mii->mii_media_active |= IFM_10_T;
}

int
tlphy_auto(sc, waitfor)
	struct tlphy_softc *sc;
	int waitfor;
{
	int error;

	switch ((error = mii_phy_auto(&sc->sc_mii, waitfor))) {
	case EIO:
		/*
		 * Just assume we're not in full-duplex mode.
		 * XXX Check link and try AUI/BNC?
		 */
		PHY_WRITE(&sc->sc_mii, MII_BMCR, 0);
		break;

	case EJUSTRETURN:
		/* Flag that we need to program when it completes. */
		sc->sc_need_acomp = 1;
		break;

	default:
		tlphy_acomp(sc);
	}

	return (error);
}

void
tlphy_acomp(sc)
	struct tlphy_softc *sc;
{
	int aner, anlpar;

	sc->sc_need_acomp = 0;

	/*
	 * Grr, braindead ThunderLAN PHY doesn't self-configure
	 * after autonegotiation.  We have to do it ourselves
	 * based on the link partner status.
	 */

	aner = PHY_READ(&sc->sc_mii, MII_ANER);
	if (aner & ANER_LPAN) {
		anlpar = PHY_READ(&sc->sc_mii, MII_ANLPAR) &
		    PHY_READ(&sc->sc_mii, MII_ANAR);
		if (anlpar & ANAR_10_FD) {
			PHY_WRITE(&sc->sc_mii, MII_BMCR, BMCR_FDX);
			return;
		}
	}
	PHY_WRITE(&sc->sc_mii, MII_BMCR, 0);
}
