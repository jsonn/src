/*      $NetBSD: ata.c,v 1.18.2.2 2004/08/12 11:41:22 skrll Exp $      */

/*
 * Copyright (c) 1998, 2001 Manuel Bouyer.  All rights reserved.
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
 *  This product includes software developed by Manuel Bouyer.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ata.c,v 1.18.2.2 2004/08/12 11:41:22 skrll Exp $");

#ifndef WDCDEBUG
#define WDCDEBUG
#endif /* WDCDEBUG */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/errno.h>
#include <sys/ataio.h>

#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/ata/atareg.h>
#include <dev/ata/atavar.h>
#include <dev/ic/wdcreg.h>
#include <dev/ic/wdcvar.h>

#include "locators.h"

#define DEBUG_FUNCS  0x08
#define DEBUG_PROBE  0x10
#define DEBUG_DETACH 0x20
#ifdef WDCDEBUG
extern int wdcdebug_mask; /* init'ed in wdc.c */
#define WDCDEBUG_PRINT(args, level) \
        if (wdcdebug_mask & (level)) \
		printf args
#else
#define WDCDEBUG_PRINT(args, level)
#endif

/*****************************************************************************
 * ATA bus layer.
 *
 * ATA controllers attach an atabus instance, which handles probing the bus
 * for drives, etc.
 *****************************************************************************/

dev_type_open(atabusopen);
dev_type_close(atabusclose);
dev_type_ioctl(atabusioctl);

const struct cdevsw atabus_cdevsw = {
	atabusopen, atabusclose, noread, nowrite, atabusioctl,
	nostop, notty, nopoll, nommap, nokqfilter,
};

extern struct cfdriver atabus_cd;


/*
 * atabusprint:
 *
 *	Autoconfiguration print routine used by ATA controllers when
 *	attaching an atabus instance.
 */
int
atabusprint(void *aux, const char *pnp)
{
	struct wdc_channel *chan = aux;
	
	if (pnp)
		aprint_normal("atabus at %s", pnp);
	aprint_normal(" channel %d", chan->ch_channel);

	return (UNCONF);
}

/*
 * ataprint:
 *
 *	Autoconfiguration print routine.
 */
int
ataprint(void *aux, const char *pnp)
{
	struct ata_device *adev = aux;

	if (pnp)
		aprint_normal("wd at %s", pnp);
	aprint_normal(" drive %d", adev->adev_drv_data->drive);

	return (UNCONF);
}

/*
 * atabus_thread:
 *
 *	Worker thread for the ATA bus.
 */
static void
atabus_thread(void *arg)
{
	struct atabus_softc *sc = arg;
	struct wdc_channel *chp = sc->sc_chan;
	struct ata_xfer *xfer;
	int s;

	s = splbio();
	chp->ch_flags |= WDCF_TH_RUN;
	splx(s);

	/* Configure the devices on the bus. */
	atabusconfig(sc);

	for (;;) {
		s = splbio();
		if ((chp->ch_flags & (WDCF_TH_RESET | WDCF_SHUTDOWN)) == 0 &&
		    (chp->ch_queue->active_xfer == NULL ||
		     chp->ch_queue->queue_freeze == 0)) {
			chp->ch_flags &= ~WDCF_TH_RUN;
			(void) tsleep(&chp->ch_thread, PRIBIO, "atath", 0);
			chp->ch_flags |= WDCF_TH_RUN;
		}
		splx(s);
		if (chp->ch_flags & WDCF_SHUTDOWN)
			break;
		s = splbio();
		if (chp->ch_flags & WDCF_TH_RESET) {
			/*
			 * wdc_reset_channel() will freeze 2 times, so
			 * unfreeze one time. Not a problem as we're at splbio
			 */
			chp->ch_queue->queue_freeze--;
			wdc_reset_channel(chp, AT_WAIT | chp->ch_reset_flags);
		} else if (chp->ch_queue->active_xfer != NULL &&
			   chp->ch_queue->queue_freeze == 1) {
			/*
			 * Caller has bumped queue_freeze, decrease it.
			 */
			chp->ch_queue->queue_freeze--;
			xfer = chp->ch_queue->active_xfer;
			KASSERT(xfer != NULL);
			(*xfer->c_start)(chp, xfer);
		} else if (chp->ch_queue->queue_freeze > 1)
			panic("ata_thread: queue_freeze");
		splx(s);
	}
	chp->ch_thread = NULL;
	wakeup((void *)&chp->ch_flags);
	kthread_exit(0);
}

/*
 * atabus_create_thread:
 *
 *	Helper routine to create the ATA bus worker thread.
 */
static void
atabus_create_thread(void *arg)
{
	struct atabus_softc *sc = arg;
	struct wdc_channel *chp = sc->sc_chan;
	int error;

	if ((error = kthread_create1(atabus_thread, sc, &chp->ch_thread,
				     "%s", sc->sc_dev.dv_xname)) != 0)
		aprint_error("%s: unable to create kernel thread: error %d\n",
		    sc->sc_dev.dv_xname, error);
}

/*
 * atabus_match:
 *
 *	Autoconfiguration match routine.
 */
static int
atabus_match(struct device *parent, struct cfdata *cf, void *aux)
{
	struct wdc_channel *chp = aux;

	if (chp == NULL)
		return (0);
	
	if (cf->cf_loc[ATACF_CHANNEL] != chp->ch_channel &&
	    cf->cf_loc[ATACF_CHANNEL] != ATACF_CHANNEL_DEFAULT)
	    	return (0);
	
	return (1);
}

/*
 * atabus_attach:
 *
 *	Autoconfiguration attach routine.
 */
static void
atabus_attach(struct device *parent, struct device *self, void *aux)
{
	struct atabus_softc *sc = (void *) self;
	struct wdc_channel *chp = aux;
	struct atabus_initq *initq;

	sc->sc_chan = chp;

	aprint_normal("\n");
	aprint_naive("\n");

        if (wdc_addref(chp))
                return;

	initq = malloc(sizeof(*initq), M_DEVBUF, M_WAITOK);
	initq->atabus_sc = sc;
	TAILQ_INSERT_TAIL(&atabus_initq_head, initq, atabus_initq);
	config_pending_incr();
	kthread_create(atabus_create_thread, sc);
}

/*
 * atabus_activate:
 *
 *	Autoconfiguration activation routine.
 */
static int
atabus_activate(struct device *self, enum devact act)
{
	struct atabus_softc *sc = (void *) self;
	struct wdc_channel *chp = sc->sc_chan;
	struct device *dev = NULL;
	int s, i, error = 0;

	s = splbio();
	switch (act) {
	case DVACT_ACTIVATE:
		error = EOPNOTSUPP;
		break;
	
	case DVACT_DEACTIVATE:
		/*
		 * We might deactivate the children of atapibus twice
		 * (once bia atapibus, once directly), but since the
		 * generic autoconfiguration code maintains the DVF_ACTIVE
		 * flag, it's safe.
		 */
		if ((dev = chp->atapibus) != NULL) {
			error = config_deactivate(dev);
			if (error)
				goto out;
		}

		for (i = 0; i < 2; i++) {
			if ((dev = chp->ch_drive[i].drv_softc) != NULL) {
				WDCDEBUG_PRINT(("atabus_activate: %s: "
				    "deactivating %s\n", sc->sc_dev.dv_xname,
				    dev->dv_xname),
				    DEBUG_DETACH);
				error = config_deactivate(dev);
				if (error)
					goto out;
			}
		}
		break;
	}
 out:
	splx(s);

#ifdef WDCDEBUG
	if (dev != NULL && error != 0)
		WDCDEBUG_PRINT(("atabus_activate: %s: "
		    "error %d deactivating %s\n", sc->sc_dev.dv_xname,
		    error, dev->dv_xname), DEBUG_DETACH);
#endif /* WDCDEBUG */

	return (error);
}

/*
 * atabus_detach:
 *
 *	Autoconfiguration detach routine.
 */
static int
atabus_detach(struct device *self, int flags)
{
	struct atabus_softc *sc = (void *) self;
	struct wdc_channel *chp = sc->sc_chan;
	struct device *dev = NULL;
	int i, error = 0;

	/* Shutdown the channel. */
	/* XXX NEED AN INTERLOCK HERE. */
	chp->ch_flags |= WDCF_SHUTDOWN;
	wakeup(&chp->ch_thread);
	while (chp->ch_thread != NULL)
		(void) tsleep((void *)&chp->ch_flags, PRIBIO, "atadown", 0);
	
	/*
	 * Detach atapibus and its children.
	 */
	if ((dev = chp->atapibus) != NULL) {
		WDCDEBUG_PRINT(("atabus_detach: %s: detaching %s\n",
		    sc->sc_dev.dv_xname, dev->dv_xname), DEBUG_DETACH);
		error = config_detach(dev, flags);
		if (error)
			goto out;
	}

	/*
	 * Detach our other children.
	 */
	for (i = 0; i < 2; i++) {
		if (chp->ch_drive[i].drive_flags & DRIVE_ATAPI)
			continue;
		if ((dev = chp->ch_drive[i].drv_softc) != NULL) {
			WDCDEBUG_PRINT(("atabus_detach: %s: detaching %s\n",
			    sc->sc_dev.dv_xname, dev->dv_xname),
			    DEBUG_DETACH);
			error = config_detach(dev, flags);
			if (error)
				goto out;
		}
	}

 out:
#ifdef WDCDEBUG
	if (dev != NULL && error != 0)
		WDCDEBUG_PRINT(("atabus_detach: %s: error %d detaching %s\n",
		    sc->sc_dev.dv_xname, error, dev->dv_xname),
		    DEBUG_DETACH);
#endif /* WDCDEBUG */

	return (error);
}

CFATTACH_DECL(atabus, sizeof(struct atabus_softc),
    atabus_match, atabus_attach, atabus_detach, atabus_activate);

/*****************************************************************************
 * Common ATA bus operations.
 *****************************************************************************/

/* Get the disk's parameters */
int
ata_get_params(struct ata_drive_datas *drvp, u_int8_t flags,
    struct ataparams *prms)
{
	char tb[DEV_BSIZE];
	struct ata_command ata_c;

#if BYTE_ORDER == LITTLE_ENDIAN
	int i;
	u_int16_t *p;
#endif

	WDCDEBUG_PRINT(("ata_get_parms\n"), DEBUG_FUNCS);

	memset(tb, 0, DEV_BSIZE);
	memset(prms, 0, sizeof(struct ataparams));
	memset(&ata_c, 0, sizeof(struct ata_command));

	if (drvp->drive_flags & DRIVE_ATA) {
		ata_c.r_command = WDCC_IDENTIFY;
		ata_c.r_st_bmask = WDCS_DRDY;
		ata_c.r_st_pmask = 0;
		ata_c.timeout = 3000; /* 3s */
	} else if (drvp->drive_flags & DRIVE_ATAPI) {
		ata_c.r_command = ATAPI_IDENTIFY_DEVICE;
		ata_c.r_st_bmask = 0;
		ata_c.r_st_pmask = 0;
		ata_c.timeout = 10000; /* 10s */
	} else {
		WDCDEBUG_PRINT(("ata_get_parms: no disks\n"),
		    DEBUG_FUNCS|DEBUG_PROBE);
		return CMD_ERR;
	}
	ata_c.flags = AT_READ | flags;
	ata_c.data = tb;
	ata_c.bcount = DEV_BSIZE;
	if (wdc_exec_command(drvp, &ata_c) != ATACMD_COMPLETE) {
		WDCDEBUG_PRINT(("ata_get_parms: wdc_exec_command failed\n"),
		    DEBUG_FUNCS|DEBUG_PROBE);
		return CMD_AGAIN;
	}
	if (ata_c.flags & (AT_ERROR | AT_TIMEOU | AT_DF)) {
		WDCDEBUG_PRINT(("ata_get_parms: ata_c.flags=0x%x\n",
		    ata_c.flags), DEBUG_FUNCS|DEBUG_PROBE);
		return CMD_ERR;
	} else {
		/* if we didn't read any data something is wrong */
		if ((ata_c.flags & AT_XFDONE) == 0)
			return CMD_ERR;
		/* Read in parameter block. */
		memcpy(prms, tb, sizeof(struct ataparams));
#if BYTE_ORDER == LITTLE_ENDIAN
		/*
		 * Shuffle string byte order.
		 * ATAPI Mitsumi and NEC drives don't need this.
		 */
		if ((prms->atap_config & WDC_CFG_ATAPI_MASK) ==
		    WDC_CFG_ATAPI &&
		    ((prms->atap_model[0] == 'N' &&
			prms->atap_model[1] == 'E') ||
		     (prms->atap_model[0] == 'F' &&
			 prms->atap_model[1] == 'X')))
			return 0;
		for (i = 0; i < sizeof(prms->atap_model); i += 2) {
			p = (u_short *)(prms->atap_model + i);
			*p = ntohs(*p);
		}
		for (i = 0; i < sizeof(prms->atap_serial); i += 2) {
			p = (u_short *)(prms->atap_serial + i);
			*p = ntohs(*p);
		}
		for (i = 0; i < sizeof(prms->atap_revision); i += 2) {
			p = (u_short *)(prms->atap_revision + i);
			*p = ntohs(*p);
		}
#endif
		return CMD_OK;
	}
}

int
ata_set_mode(struct ata_drive_datas *drvp, u_int8_t mode, u_int8_t flags)
{
	struct ata_command ata_c;

	WDCDEBUG_PRINT(("ata_set_mode=0x%x\n", mode), DEBUG_FUNCS);
	memset(&ata_c, 0, sizeof(struct ata_command));

	ata_c.r_command = SET_FEATURES;
	ata_c.r_st_bmask = 0;
	ata_c.r_st_pmask = 0;
	ata_c.r_features = WDSF_SET_MODE;
	ata_c.r_count = mode;
	ata_c.flags = flags;
	ata_c.timeout = 1000; /* 1s */
	if (wdc_exec_command(drvp, &ata_c) != ATACMD_COMPLETE)
		return CMD_AGAIN;
	if (ata_c.flags & (AT_ERROR | AT_TIMEOU | AT_DF)) {
		return CMD_ERR;
	}
	return CMD_OK;
}

void
ata_dmaerr(struct ata_drive_datas *drvp, int flags)
{
	/*
	 * Downgrade decision: if we get NERRS_MAX in NXFER.
	 * We start with n_dmaerrs set to NERRS_MAX-1 so that the
	 * first error within the first NXFER ops will immediatly trigger
	 * a downgrade.
	 * If we got an error and n_xfers is bigger than NXFER reset counters.
	 */
	drvp->n_dmaerrs++;
	if (drvp->n_dmaerrs >= NERRS_MAX && drvp->n_xfers <= NXFER) {
		wdc_downgrade_mode(drvp, flags);
		drvp->n_dmaerrs = NERRS_MAX-1;
		drvp->n_xfers = 0;
		return;
	}
	if (drvp->n_xfers > NXFER) {
		drvp->n_dmaerrs = 1; /* just got an error */
		drvp->n_xfers = 1; /* restart counting from this error */
	}
}

/* management of the /dev/atabus* devices */
int atabusopen(dev, flag, fmt, l)
	dev_t dev;
	int flag, fmt;
	struct lwp *l;
{
        struct atabus_softc *sc;
        int error, unit = minor(dev);
   
        if (unit >= atabus_cd.cd_ndevs ||
            (sc = atabus_cd.cd_devs[unit]) == NULL)
                return (ENXIO);
 
        if (sc->sc_flags & ATABUSCF_OPEN)
                return (EBUSY);

        if ((error = wdc_addref(sc->sc_chan)) != 0)
                return (error);

        sc->sc_flags |= ATABUSCF_OPEN;

        return (0);
}


int
atabusclose(dev, flag, fmt, l)
        dev_t dev;
        int flag, fmt;
        struct lwp *l;
{
        struct atabus_softc *sc = atabus_cd.cd_devs[minor(dev)];

        wdc_delref(sc->sc_chan);

        sc->sc_flags &= ~ATABUSCF_OPEN;

        return (0);
}

int
atabusioctl(dev, cmd, addr, flag, l)
        dev_t dev;
        u_long cmd;
        caddr_t addr;
        int flag;
        struct lwp *l;
{
        struct atabus_softc *sc = atabus_cd.cd_devs[minor(dev)];
	struct wdc_channel *chp = sc->sc_chan;
	int min_drive, max_drive, drive;
        int error;
	int s;

        /*
         * Enforce write permission for ioctls that change the
         * state of the bus.  Host adapter specific ioctls must
         * be checked by the adapter driver.
         */
        switch (cmd) {
        case ATABUSIOSCAN:
        case ATABUSIODETACH:
        case ATABUSIORESET:
                if ((flag & FWRITE) == 0)
                        return (EBADF);
        }

        switch (cmd) {
        case ATABUSIORESET:
		s = splbio();
		wdc_reset_channel(sc->sc_chan, AT_WAIT | AT_POLL);
		splx(s);
		error = 0;
		break;
	case ATABUSIOSCAN:
	{
#if 0
		struct atabusioscan_args *a=
		    (struct atabusioscan_args *)addr;
#endif
		if ((chp->ch_drive[0].drive_flags & DRIVE_OLD) ||
		    (chp->ch_drive[1].drive_flags & DRIVE_OLD))
			return (EOPNOTSUPP);
		return (EOPNOTSUPP);
	}
	case ATABUSIODETACH:
	{
		struct atabusioscan_args *a=
		    (struct atabusioscan_args *)addr;
		if ((chp->ch_drive[0].drive_flags & DRIVE_OLD) ||
		    (chp->ch_drive[1].drive_flags & DRIVE_OLD))
			return (EOPNOTSUPP);
		switch (a->at_dev) {
		case -1:
			min_drive = 0;
			max_drive = 1;
			break;
		case 0:
		case 1:
			min_drive = max_drive = a->at_dev;
			break;
		default:
			return (EINVAL);
		}
		for (drive = min_drive; drive <= max_drive; drive++) {
			if (chp->ch_drive[drive].drv_softc != NULL) {
				error = config_detach(
				    chp->ch_drive[drive].drv_softc, 0);
				if (error)
					return (error);
				chp->ch_drive[drive].drv_softc = NULL;
			}
		}
		error = 0;
		break;
	}
	default:
		error = ENOTTY;
	}
	return (error);
};
