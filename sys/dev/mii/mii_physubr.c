/*	$NetBSD: mii_physubr.c,v 1.17.4.1 2000/07/04 04:11:13 thorpej Exp $	*/

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
 * Subroutines common to all PHYs.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/errno.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/route.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

/*
 * Media to register setting conversion table.  Order matters.
 */
const struct mii_media mii_media_table[] = {
	{ BMCR_ISO,		ANAR_CSMA },		/* None */
	{ 0,			ANAR_CSMA|ANAR_10 },	/* 10baseT */
	{ BMCR_FDX,		ANAR_CSMA|ANAR_10_FD },	/* 10baseT-FDX */
	{ BMCR_S100,		ANAR_CSMA|ANAR_T4 },	/* 100baseT4 */
	{ BMCR_S100,		ANAR_CSMA|ANAR_TX },	/* 100baseTX */
	{ BMCR_S100|BMCR_FDX,	ANAR_CSMA|ANAR_TX_FD },	/* 100baseTX-FDX */
};

void	mii_phy_auto_timeout __P((void *));

void
mii_phy_setmedia(sc)
	struct mii_softc *sc;
{
	struct mii_data *mii = sc->mii_pdata;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int bmcr, anar;

	if (IFM_SUBTYPE(ife->ifm_media) == IFM_AUTO) {
		if ((PHY_READ(sc, MII_BMCR) & BMCR_AUTOEN) == 0)
			(void) mii_phy_auto(sc, 1);
		return;
	}

	/*
	 * Table index is stored in the media entry.
	 */

#ifdef DIAGNOSTIC
	if (ife->ifm_data < 0 || ife->ifm_data >= MII_NMEDIA)
		panic("mii_phy_setmedia");
#endif

	anar = mii_media_table[ife->ifm_data].mm_anar;
	bmcr = mii_media_table[ife->ifm_data].mm_bmcr;

	if (ife->ifm_media & IFM_LOOP)
		bmcr |= BMCR_LOOP;

	PHY_WRITE(sc, MII_ANAR, anar);
	PHY_WRITE(sc, MII_BMCR, bmcr);
}

int
mii_phy_auto(sc, waitfor)
	struct mii_softc *sc;
	int waitfor;
{
	int bmsr, i;

	if ((sc->mii_flags & MIIF_DOINGAUTO) == 0) {
		PHY_WRITE(sc, MII_ANAR,
		    BMSR_MEDIA_TO_ANAR(sc->mii_capabilities) | ANAR_CSMA);
		PHY_WRITE(sc, MII_BMCR, BMCR_AUTOEN | BMCR_STARTNEG);
	}

	if (waitfor) {
		/* Wait 500ms for it to complete. */
		for (i = 0; i < 500; i++) {
			if ((bmsr = PHY_READ(sc, MII_BMSR)) & BMSR_ACOMP)
				return (0);
			delay(1000);
		}

		/*
		 * Don't need to worry about clearing MIIF_DOINGAUTO.
		 * If that's set, a timeout is pending, and it will
		 * clear the flag.
		 */
		return (EIO);
	}

	/*
	 * Just let it finish asynchronously.  This is for the benefit of
	 * the tick handler driving autonegotiation.  Don't want 500ms
	 * delays all the time while the system is running!
	 */
	if ((sc->mii_flags & MIIF_DOINGAUTO) == 0) {
		sc->mii_flags |= MIIF_DOINGAUTO;
		callout_reset(&sc->mii_nway_ch, hz >> 1,
		    mii_phy_auto_timeout, sc);
	}
	return (EJUSTRETURN);
}

void
mii_phy_auto_timeout(arg)
	void *arg;
{
	struct mii_softc *sc = arg;
	int s, bmsr;

	if ((sc->mii_dev.dv_flags & DVF_ACTIVE) == 0)
		return;

	s = splnet();
	sc->mii_flags &= ~MIIF_DOINGAUTO;
	bmsr = PHY_READ(sc, MII_BMSR);

	/* Update the media status. */
	(void) PHY_SERVICE(sc, sc->mii_pdata, MII_POLLSTAT);
	splx(s);
}

int
mii_phy_tick(sc)
	struct mii_softc *sc;
{
	struct mii_data *mii = sc->mii_pdata;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int reg;

	/* Just bail now if the interface is down. */
	if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
		return (EJUSTRETURN);

	/*
	 * If we're not doing autonegotiation, we don't need to do
	 * any extra work here.  However, we need to check the link
	 * status so we can generate an announcement if the status
	 * changes.
	 */
	if (IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO)
		return (0);

	/* Read the status register twice; BMSR_LINK is latch-low. */
	reg = PHY_READ(sc, MII_BMSR) | PHY_READ(sc, MII_BMSR);
	if (reg & BMSR_LINK) {
		/*
		 * See above.
		 */
		return (0);
	}

	/*
	 * Only retry autonegotiation every 5 seconds.
	 */
	if (++sc->mii_ticks != 5)
		return (EJUSTRETURN);

	sc->mii_ticks = 0;
	PHY_RESET(sc);

	if (mii_phy_auto(sc, 0) == EJUSTRETURN)
		return (EJUSTRETURN);

	/*
	 * Might need to generate a status message if autonegotiation
	 * failed.
	 */
	return (0);
}

void
mii_phy_reset(sc)
	struct mii_softc *sc;
{
	int reg, i;

	if (sc->mii_flags & MIIF_NOISOLATE)
		reg = BMCR_RESET;
	else
		reg = BMCR_RESET | BMCR_ISO;
	PHY_WRITE(sc, MII_BMCR, reg);

	/* Wait 100ms for it to complete. */
	for (i = 0; i < 100; i++) {
		reg = PHY_READ(sc, MII_BMCR); 
		if ((reg & BMCR_RESET) == 0)
			break;
		delay(1000);
	}

	if (sc->mii_inst != 0 && ((sc->mii_flags & MIIF_NOISOLATE) == 0))
		PHY_WRITE(sc, MII_BMCR, reg | BMCR_ISO);
}

void
mii_phy_down(sc)
	struct mii_softc *sc;
{

	if (sc->mii_flags & MIIF_DOINGAUTO) {
		sc->mii_flags &= ~MIIF_DOINGAUTO;
		callout_stop(&sc->mii_nway_ch);
	}
}

void
mii_phy_status(sc)
	struct mii_softc *sc;
{

	PHY_STATUS(sc);
}

void
mii_phy_update(sc, cmd)
	struct mii_softc *sc;
	int cmd;
{
	struct mii_data *mii = sc->mii_pdata;

	if (sc->mii_media_active != mii->mii_media_active ||
	    sc->mii_media_status != mii->mii_media_status ||
	    cmd == MII_MEDIACHG) {
		(*mii->mii_statchg)(sc->mii_dev.dv_parent);
		mii_phy_statusmsg(sc);
		sc->mii_media_active = mii->mii_media_active;
		sc->mii_media_status = mii->mii_media_status;
	}
}

void
mii_phy_statusmsg(sc)
	struct mii_softc *sc;
{
	struct mii_data *mii = sc->mii_pdata;
	struct ifnet *ifp = mii->mii_ifp;
	int s, baudrate, link_state, announce = 0;

	if (mii->mii_media_status & IFM_AVALID) {
		if (mii->mii_media_status & IFM_ACTIVE)
			link_state = LINK_STATE_UP;
		else
			link_state = LINK_STATE_DOWN;
	} else
		link_state = LINK_STATE_UNKNOWN;

	baudrate = ifmedia_baudrate(mii->mii_media_active);

	if (link_state != ifp->if_link_state) {
		ifp->if_link_state = link_state;
		/*
		 * XXX Right here we'd like to notify protocols
		 * XXX that the link status has changed, so that
		 * XXX e.g. Duplicate Address Detection can restart.
		 */
		announce = 1;
	}

	if (baudrate != ifp->if_baudrate) {
		ifp->if_baudrate = baudrate;
		announce = 1;
	}

	if (announce) {
		s = splimp();	/* XXX Should be splnet() */
		rt_ifmsg(ifp);
		splx(s);
	}
}

/*
 * Initialize generic PHY media based on BMSR, called when a PHY is
 * attached.  We expect to be set up to print a comma-separated list
 * of media names.  Does not print a newline.
 */
void
mii_phy_add_media(sc)
	struct mii_softc *sc;
{
	struct mii_data *mii = sc->mii_pdata;
	const char *sep = "";

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)
#define	PRINT(s)	printf("%s%s", sep, s); sep = ", "

	if ((sc->mii_flags & MIIF_NOISOLATE) == 0)
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_NONE, 0, sc->mii_inst),
		    MII_MEDIA_NONE);

	if (sc->mii_capabilities & BMSR_10THDX) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, 0, sc->mii_inst),
		    MII_MEDIA_10_T);
#if 0
		if ((sc->mii_flags & MIIF_NOLOOP) == 0)
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, IFM_LOOP,
			    sc->mii_inst), MII_MEDIA_10_T);
#endif
		PRINT("10baseT");
	}
	if (sc->mii_capabilities & BMSR_10TFDX) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, IFM_FDX, sc->mii_inst),
		    MII_MEDIA_10_T_FDX);
		PRINT("10baseT-FDX");
	}
	if (sc->mii_capabilities & BMSR_100TXHDX) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, 0, sc->mii_inst),
		    MII_MEDIA_100_TX);
#if 0
		if ((sc->mii_flags & MIIF_NOLOOP) == 0)
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_LOOP,
			    sc->mii_inst), MII_MEDIA_100_TX);
#endif
		PRINT("100baseTX");
	}
	if (sc->mii_capabilities & BMSR_100TXFDX) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_FDX, sc->mii_inst),
		    MII_MEDIA_100_TX_FDX);
		PRINT("100baseTX-FDX");
	}
	if (sc->mii_capabilities & BMSR_100T4) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_T4, 0, sc->mii_inst),
		    MII_MEDIA_100_T4);
#if 0
		if ((sc->mii_flags & MIIF_NOLOOP) == 0)
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_T4, IFM_LOOP,
			    sc->mii_inst), MII_MEDIA_100_T4);
#endif
		PRINT("100baseT4");
	}
	if (sc->mii_capabilities & BMSR_ANEG) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_AUTO, 0, sc->mii_inst),
		    MII_NMEDIA);	/* intentionally invalid index */
		PRINT("auto");
	}
#undef ADD
#undef PRINT
}

void
mii_phy_delete_media(sc)
	struct mii_softc *sc;
{
	struct mii_data *mii = sc->mii_pdata;

	ifmedia_delete_instance(&mii->mii_media, sc->mii_inst);
}

int
mii_phy_activate(self, act)
	struct device *self;
	enum devact act;
{
	int rv = 0;

	switch (act) {
	case DVACT_ACTIVATE:
		rv = EOPNOTSUPP;
		break;

	case DVACT_DEACTIVATE:
		/* Nothing special to do. */
		break;
	}

	return (rv);
}

int
mii_phy_detach(self, flags)
	struct device *self;
	int flags;
{
	struct mii_softc *sc = (void *) self;

	if (sc->mii_flags & MIIF_DOINGAUTO)
		callout_stop(&sc->mii_nway_ch);

	mii_phy_delete_media(sc);

	return (0);
}
