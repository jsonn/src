/*	$NetBSD: wsmux.c,v 1.9.8.4 2001/09/26 15:28:20 fvdl Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Author: Lennart Augustsson <augustss@carlstedt.se>
 *         Carlstedt Research & Technology
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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

#include "wsmux.h"
#include "wsdisplay.h"
#include "wskbd.h"

#if NWSMUX > 0 || (NWSDISPLAY > 0 && NWSKBD > 0)

/*
 * wscons mux device.
 *
 * The mux device is a collection of real mice and keyboards and acts as 
 * a merge point for all the events from the different real devices.
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <sys/signalvar.h>
#include <sys/device.h>
#include <sys/vnode.h>

#include <miscfs/specfs/specdev.h>

#include "opt_wsdisplay_compat.h"

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wseventvar.h>
#include <dev/wscons/wscons_callbacks.h>
#include <dev/wscons/wsmuxvar.h>

#ifdef WSMUX_DEBUG
#define DPRINTF(x)	if (wsmuxdebug) printf x
int	wsmuxdebug = 0;
#else
#define DPRINTF(x)
#endif

struct wsplink {
	LIST_ENTRY(wsplink) next;
	int type;
	int pmajor;
	struct wsmux_softc *mux; /* our mux device */
	/* The rest of the fields reflect a value in the multiplexee. */
	struct device *sc;	/* softc */
	struct wseventvar *sc_mevents; /* event var */
	struct wsmux_softc **sc_muxp; /* pointer to us */
	struct wsmuxops *sc_ops;
	struct vnode *sc_pdevvp;
};

int wsmuxdoclose __P((struct device *, int, int, struct proc *));
int wsmux_set_display __P((struct device *, struct wsmux_softc *));

#if NWSMUX > 0
cdev_decl(wsmux);

void wsmuxattach __P((int));

struct wsmuxops wsmux_muxops = {
	wsmuxopen, wsmuxdoclose, wsmuxdoioctl, wsmux_displayioctl,
	wsmux_set_display
};

void wsmux_setmax __P((int n));

int nwsmux = 0;
struct wsmux_softc **wsmuxdevs;

int	wsmux_major = -1;

void
wsmux_setmax(n)
	int n;
{
	int i;

	if (wsmux_major == -1) {
		int maj;

		for (maj = 0; maj < nchrdev; maj++)
			if (cdevsw[maj].d_open == wsmuxopen)
				break;
		wsmux_major = maj;
	}

	if (n >= nwsmux) {
		i = nwsmux;
		nwsmux = n + 1;
		if (nwsmux != 0)
			wsmuxdevs = realloc(wsmuxdevs, 
					    nwsmux * sizeof (*wsmuxdevs), 
					    M_DEVBUF, M_NOWAIT);
		else
			wsmuxdevs = malloc(nwsmux * sizeof (*wsmuxdevs), 
					   M_DEVBUF, M_NOWAIT);
		if (wsmuxdevs == 0)
			panic("wsmux_setmax: no memory\n");
		for (; i < nwsmux; i++)
			wsmuxdevs[i] = 0;
	}
}

/* From upper level */
void
wsmuxattach(n)
	int n;
{
	int i;

	wsmux_setmax(n);	/* Make sure we have room for all muxes. */

	/* Make sure all muxes are there. */
	for (i = 0; i < nwsmux; i++)
		if (!wsmuxdevs[i])
			wsmuxdevs[i] = wsmux_create("wsmux", i);
}

/* From mouse or keyboard. */
void
wsmux_attach(n, type, dsc, ev, psp, ops, pmajor)
	int n;
	int type;
        struct device *dsc;
	struct wseventvar *ev;
	struct wsmux_softc **psp;
	struct wsmuxops *ops;
	int pmajor;
{
	struct wsmux_softc *sc;
	int error;

	DPRINTF(("wsmux_attach: n=%d\n", n));
	wsmux_setmax(n);
	sc = wsmuxdevs[n];
	if (sc == 0) {
		sc = wsmux_create("wsmux", n);
		if (sc == 0) {
			printf("wsmux: attach out of memory\n");
			return;
		}
		wsmuxdevs[n] = sc;
	}
	error = wsmux_attach_sc(sc, type, dsc, ev, psp, ops, pmajor);
	if (error)
		printf("wsmux_attach: error=%d\n", error);
}

/* From mouse or keyboard. */
void
wsmux_detach(n, dsc)
	int n;
        struct device *dsc;
{
#ifdef DIAGNOSTIC
	int error;

	if (n >= nwsmux || n < 0) {
		printf("wsmux_detach: detach is out of range\n");
		return;
	}
	if ((error = wsmux_detach_sc(wsmuxdevs[n], dsc)))
		printf("wsmux_detach: error=%d\n", error);
#else
	(void)wsmux_detach_sc(wsmuxdevs[n], dsc);
#endif
}

int
wsmuxopen(devvp, flags, mode, p)
	struct vnode *devvp;
	int flags, mode;
	struct proc *p;
{
	struct wsmux_softc *sc;
	struct wsplink *m;
	struct vnode *vp2;
	int unit, error, nopen, lasterror;

	unit = minor(vdev_rdev(devvp));
	if (unit >= nwsmux ||	/* make sure it was attached */
	    (sc = wsmuxdevs[unit]) == NULL)
		return (ENXIO);

	vdev_setprivdata(devvp, sc);

	DPRINTF(("wsmuxopen: %s: sc=%p\n", sc->sc_dv.dv_xname, sc));
	if (!(flags & FREAD)) {
		/* Not opening for read, only ioctl is available. */
		return (0);
	}

	if (sc->sc_events.io)
		return (EBUSY);

	sc->sc_events.io = p;
	sc->sc_flags = flags;
	sc->sc_mode = mode;
	sc->sc_p = p;
	wsevent_init(&sc->sc_events);		/* may cause sleep */

	nopen = 0;
	lasterror = 0;
	for (m = LIST_FIRST(&sc->sc_reals); m; m = LIST_NEXT(m, next)) {
		if (!m->sc_mevents->io && !*m->sc_muxp) {
			/* XXXDEVVP */
			DPRINTF(("wsmuxopen: %s: m=%p dev=%s\n", 
				 sc->sc_dv.dv_xname, m, m->sc->dv_xname));
			KASSERT(m->sc_pdevvp == NULL);
			error = cdevvp(makedev(m->pmajor, m->sc->dv_unit),
			    &m->sc_pdevvp);
			if (error == 0) {
				vp2 = NULL;
				vn_lock(m->sc_pdevvp, LK_EXCLUSIVE | LK_RETRY);
				error = VOP_OPEN(m->sc_pdevvp, flags,
				    p->p_ucred, p, &vp2);
				if (error == 0 && vp2 != NULL) {
					vput(m->sc_pdevvp);
					m->sc_pdevvp = vp2;
				}
			}
			if (error) {
				/* Ignore opens that fail */
				lasterror = error;
				vput(m->sc_pdevvp);
				m->sc_pdevvp = NULL;
				DPRINTF(("wsmuxopen: open failed %d\n", 
					 error));
			} else {
				VOP_UNLOCK(m->sc_pdevvp, 0);
				nopen++;
				*m->sc_muxp = sc;
			}
		}
	}

	if (nopen == 0 && lasterror != 0) {
		wsevent_fini(&sc->sc_events);
		sc->sc_events.io = NULL;
		return (lasterror);
	}

	return (0);
}

int
wsmuxclose(devvp, flags, mode, p)
	struct vnode *devvp;
	int flags, mode;
	struct proc *p;
{
	return wsmuxdoclose(vdev_privdata(devvp), flags, mode, p);
}

int
wsmuxread(devvp, uio, flags)
	struct vnode *devvp;
	struct uio *uio;
	int flags;
{
	struct wsmux_softc *sc;

	sc = vdev_privdata(devvp);

	if (!sc->sc_events.io)
		return (EACCES);

	return (wsevent_read(&sc->sc_events, uio, flags));
}

int
wsmuxioctl(devvp, cmd, data, flag, p)
	struct vnode *devvp;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	return wsmuxdoioctl(vdev_privdata(devvp), cmd, data, flag, p);
}

int
wsmuxpoll(devvp, events, p)
	struct vnode *devvp;
	int events;
	struct proc *p;
{
	struct wsmux_softc *sc;

	sc = vdev_privdata(devvp);

	if (!sc->sc_events.io)
		return (EACCES);

	return (wsevent_poll(&sc->sc_events, events, p));
}

int
wsmux_add_mux(unit, muxsc)
	int unit;
	struct wsmux_softc *muxsc;
{
	struct wsmux_softc *sc, *m;

	if (unit < 0 || unit >= nwsmux || (sc = wsmuxdevs[unit]) == NULL)
		return (ENXIO);

	DPRINTF(("wsmux_add_mux: %s to %s\n", sc->sc_dv.dv_xname,
		 muxsc->sc_dv.dv_xname));

	if (sc->sc_mux || sc->sc_events.io)
		return (EBUSY);

	/* The mux we are adding must not be an ancestor of it. */
	for (m = muxsc->sc_mux; m; m = m->sc_mux)
		if (m == sc)
			return (EINVAL);

	return (wsmux_attach_sc(muxsc, WSMUX_MUX, &sc->sc_dv, &sc->sc_events, 
				&sc->sc_mux, &wsmux_muxops, wsmux_major));
}

int
wsmux_rem_mux(unit, muxsc)
	int unit;
	struct wsmux_softc *muxsc;
{
	struct wsmux_softc *sc;

	if (unit < 0 || unit >= nwsmux || (sc = wsmuxdevs[unit]) == NULL)
		return (ENXIO);
	
	DPRINTF(("wsmux_rem_mux: %s from %s\n", sc->sc_dv.dv_xname,
		 muxsc->sc_dv.dv_xname));

	return (wsmux_detach_sc(muxsc, &sc->sc_dv));
}

#endif /* NWSMUX > 0 */

struct wsmux_softc *
wsmux_create(name, unit)
	const char *name;
	int unit;
{
	struct wsmux_softc *sc;

	DPRINTF(("wsmux_create: allocating\n"));
	sc = malloc(sizeof *sc, M_DEVBUF, M_NOWAIT);
	if (!sc)
		return (0);
	memset(sc, 0, sizeof *sc);
	LIST_INIT(&sc->sc_reals);
	snprintf(sc->sc_dv.dv_xname, sizeof sc->sc_dv.dv_xname,
		 "%s%d", name, unit);
	sc->sc_dv.dv_unit = unit;
	return (sc);
}

int
wsmux_attach_sc(sc, type, dsc, ev, psp, ops, pmajor)
	struct wsmux_softc *sc;
	int type;
        struct device *dsc;
	struct wseventvar *ev;
	struct wsmux_softc **psp;
	struct wsmuxops *ops;
	int pmajor;
{
	struct wsplink *m;
	struct vnode *vp2;
	int error;

	DPRINTF(("wsmux_attach_sc: %s: type=%d dsc=%p, *psp=%p\n",
		 sc->sc_dv.dv_xname, type, dsc, *psp));
	m = malloc(sizeof *m, M_DEVBUF, M_NOWAIT);
	if (m == 0)
		return (ENOMEM);
	m->type = type;
	m->pmajor = pmajor;
	m->mux = sc;
	m->sc = dsc;
	m->sc_mevents = ev;
	m->sc_muxp = psp;
	m->sc_ops = ops;
	m->sc_pdevvp = NULL;
	LIST_INSERT_HEAD(&sc->sc_reals, m, next);

	if (sc->sc_displaydv) {
		/* This is a display mux, so attach the new device to it. */
		DPRINTF(("wsmux_attach_sc: %s: set display %p\n", 
			 sc->sc_dv.dv_xname, sc->sc_displaydv));
		error = 0;
		if (m->sc_ops->dsetdisplay) {
			error = m->sc_ops->dsetdisplay(m->sc, sc);
			/* Ignore that the console already has a display. */
			if (error == EBUSY)
				error = 0;
			if (!error) {
				*m->sc_muxp = sc;
#ifdef WSDISPLAY_COMPAT_RAWKBD
				DPRINTF(("wsmux_attach_sc: on %s set rawkbd=%d\n",
					 m->sc->dv_xname, sc->sc_rawkbd));
				(void)m->sc_ops->dioctl(m->sc, 
					     WSKBDIO_SETMODE, 
					     (caddr_t)&sc->sc_rawkbd,
					     0, 0);
#endif
			}
		}
	} else if (sc->sc_events.io) {
		/* XXXDEVVP */
		/* Mux is open, so open the new subdevice */
		DPRINTF(("wsmux_attach_sc: %s: calling open of %s\n",
			 sc->sc_dv.dv_xname, m->sc->dv_xname));
		/* mux already open, join in */
		error = cdevvp(makedev(m->pmajor, m->sc->dv_unit),
		    &m->sc_pdevvp);
		if (error == 0) {
			vn_lock(m->sc_pdevvp, LK_EXCLUSIVE | LK_RETRY);
			vp2 = NULL;
			error = VOP_OPEN(m->sc_pdevvp, sc->sc_flags,
			    sc->sc_p->p_ucred, sc->sc_p, &vp2);
			if (error == 0 && vp2 != NULL) {
				vput(m->sc_pdevvp);
				m->sc_pdevvp = vp2;
			}
		}
		if (error) {
			vput(m->sc_pdevvp);
			m->sc_pdevvp = NULL;
		} else {
			VOP_UNLOCK(m->sc_pdevvp, 0);
			*m->sc_muxp = sc;
		}
	} else {
		DPRINTF(("wsmux_attach_sc: %s not open\n",
			 sc->sc_dv.dv_xname));
		error = 0;
	}
	DPRINTF(("wsmux_attach_sc: done sc=%p psp=%p *psp=%p\n", 
		 sc, psp, *psp));

	return (error);
}

int
wsmux_detach_sc(sc, dsc)
	struct wsmux_softc *sc;
        struct device *dsc;
{
	struct wsplink *m;
	int error = 0;

	DPRINTF(("wsmux_detach_sc: %s: dsc=%p\n", sc->sc_dv.dv_xname, dsc));
#ifdef DIAGNOSTIC
	if (sc == 0) {
		printf("wsmux_detach_sc: not allocated\n");
		return (ENXIO);
	}
#endif

	for (m = LIST_FIRST(&sc->sc_reals); m; m = LIST_NEXT(m, next)) {
		if (m->sc == dsc)
			break;
	}
#ifdef DIAGNOSTIC
	if (!m) {
		printf("wsmux_detach_sc: not found\n");
		return (ENXIO);
	}
#endif
	if (sc->sc_displaydv) {
		if (m->sc_ops->dsetdisplay)
			error = m->sc_ops->dsetdisplay(m->sc, 0);
		if (error)
			return (error);
		*m->sc_muxp = 0;
	} else if (*m->sc_muxp) {
		DPRINTF(("wsmux_detach_sc: close\n"));
		/* mux device is open, so close multiplexee */
		m->sc_ops->dclose(m->sc, FREAD, 0, 0);
		*m->sc_muxp = 0;
		vrele(m->sc_pdevvp);
		m->sc_pdevvp = NULL;
	}

	LIST_REMOVE(m, next);

	free(m, M_DEVBUF);
	DPRINTF(("wsmux_detach_sc: done sc=%p\n", sc));
	return (0);
}

int wsmuxdoclose(dv, flags, mode, p)
	struct device *dv;
	int flags, mode;
	struct proc *p;
{
	struct wsmux_softc *sc = (struct wsmux_softc *)dv;
	struct wsplink *m;

	DPRINTF(("wsmuxclose: %s: sc=%p\n", sc->sc_dv.dv_xname, sc));
	if (!(flags & FREAD)) {
		/* Nothing to do, because open didn't do anything. */
		return (0);
	}

	for (m = LIST_FIRST(&sc->sc_reals); m; m = LIST_NEXT(m, next)) {
		if (*m->sc_muxp == sc) {
			DPRINTF(("wsmuxclose %s: m=%p dev=%s\n", 
				 sc->sc_dv.dv_xname, m, m->sc->dv_xname));
			m->sc_ops->dclose(m->sc, flags, mode, p);
			*m->sc_muxp = 0;
			vrele(m->sc_pdevvp);
			m->sc_pdevvp = NULL;
		}
	}

	wsevent_fini(&sc->sc_events);
	sc->sc_events.io = NULL;

	return (0);
}

int
wsmuxdoioctl(dv, cmd, data, flag, p)
	struct device *dv;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct wsmux_softc *sc = (struct wsmux_softc *)dv;
	struct wsplink *m;
	int error, ok;
	int s, put, get, n;
	struct wseventvar *evar;
	struct wscons_event *ev;
	struct timeval xxxtime;
	struct wsmux_device_list *l;

	DPRINTF(("wsmuxdoioctl: %s: sc=%p, cmd=%08lx\n", 
		 sc->sc_dv.dv_xname, sc, cmd));

	switch (cmd) {
	case WSMUX_INJECTEVENT:
		/* Inject an event, e.g., from moused. */
		if (!sc->sc_events.io)
			return (EACCES);

		evar = &sc->sc_events;
		s = spltty();
		get = evar->get;
		put = evar->put;
		if (++put % WSEVENT_QSIZE == get) {
			put--;
			splx(s);
			return (ENOSPC);
		}
		if (put >= WSEVENT_QSIZE)
			put = 0;
		ev = &evar->q[put];
		*ev = *(struct wscons_event *)data;
		microtime(&xxxtime);
		TIMEVAL_TO_TIMESPEC(&xxxtime, &ev->time);
		evar->put = put;
		WSEVENT_WAKEUP(evar);
		splx(s);
		return (0);
	case WSMUX_ADD_DEVICE:
#define d ((struct wsmux_device *)data)
		switch (d->type) {
#if NWSMOUSE > 0
		case WSMUX_MOUSE:
			return (wsmouse_add_mux(d->idx, sc));
#endif
#if NWSKBD > 0
		case WSMUX_KBD:
			return (wskbd_add_mux(d->idx, sc));
#endif
#if NWSMUX > 0
		case WSMUX_MUX:
			return (wsmux_add_mux(d->idx, sc));
#endif
		default:
			return (EINVAL);
		}
	case WSMUX_REMOVE_DEVICE:
		switch (d->type) {
#if NWSMOUSE > 0
		case WSMUX_MOUSE:
			return (wsmouse_rem_mux(d->idx, sc));
#endif
#if NWSKBD > 0
		case WSMUX_KBD:
			return (wskbd_rem_mux(d->idx, sc));
#endif
#if NWSMUX > 0
		case WSMUX_MUX:
			return (wsmux_rem_mux(d->idx, sc));
#endif
		default:
			return (EINVAL);
		}
#undef d
	case WSMUX_LIST_DEVICES:
		l = (struct wsmux_device_list *)data;
		for (n = 0, m = LIST_FIRST(&sc->sc_reals);
		     n < WSMUX_MAXDEV && m != NULL;
		     m = LIST_NEXT(m, next)) {
			l->devices[n].type = m->type;
			l->devices[n].idx = m->sc->dv_unit;
			n++;
		}
		l->ndevices = n;
		return (0);
#ifdef WSDISPLAY_COMPAT_RAWKBD
	case WSKBDIO_SETMODE:
		sc->sc_rawkbd = *(int *)data;
		DPRINTF(("wsmuxdoioctl: save rawkbd = %d\n", sc->sc_rawkbd));
		break;
#endif
	case FIOASYNC:
		sc->sc_events.async = *(int *)data != 0;
		return (0);
	case TIOCSPGRP:
		if (*(int *)data != sc->sc_events.io->p_pgid)
			return (EPERM);
		return (0);
	default:
		break;
	}

	if (sc->sc_events.io == NULL && sc->sc_displaydv == NULL)
		return (EACCES);

	/* Return 0 if any of the ioctl() succeeds, otherwise the last error */
	error = 0;
	ok = 0;
	for (m = LIST_FIRST(&sc->sc_reals); m; m = LIST_NEXT(m, next)) {
		DPRINTF(("wsmuxdoioctl: m=%p *m->sc_muxp=%p sc=%p\n",
			 m, *m->sc_muxp, sc));
		if (*m->sc_muxp == sc) {
			DPRINTF(("wsmuxdoioctl: %s: m=%p dev=%s\n", 
				 sc->sc_dv.dv_xname, m, m->sc->dv_xname));
			error = m->sc_ops->dioctl(m->sc, cmd, data, flag, p);
			if (!error)
				ok = 1;
		}
	}
	if (ok)
		error = 0;

	return (error);
}

int
wsmux_displayioctl(dv, cmd, data, flag, p)
	struct device *dv;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct wsmux_softc *sc = (struct wsmux_softc *)dv;
	struct wsplink *m;
	int error, ok;

	DPRINTF(("wsmux_displayioctl: %s: sc=%p, cmd=%08lx\n", 
		 sc->sc_dv.dv_xname, sc, cmd));

#ifdef WSDISPLAY_COMPAT_RAWKBD
	if (cmd == WSKBDIO_SETMODE) {
		sc->sc_rawkbd = *(int *)data;
		DPRINTF(("wsmux_displayioctl: rawkbd = %d\n", sc->sc_rawkbd));
	}		
#endif

	/* 
	 * Return 0 if any of the ioctl() succeeds, otherwise the last error.
	 * Return -1 if no mux component accepts the ioctl.
	 */
	error = -1;
	ok = 0;
	for (m = LIST_FIRST(&sc->sc_reals); m; m = LIST_NEXT(m, next)) {
		DPRINTF(("wsmux_displayioctl: m=%p sc=%p sc_muxp=%p\n", 
			 m, sc, *m->sc_muxp));
		if (m->sc_ops->ddispioctl && *m->sc_muxp == sc) {
			error = m->sc_ops->ddispioctl(m->sc, cmd, data,
						      flag, p);
			DPRINTF(("wsmux_displayioctl: m=%p dev=%s ==> %d\n", 
				 m, m->sc->dv_xname, error));
			if (!error)
				ok = 1;
		}
	}
	if (ok)
		error = 0;

	return (error);
}

int
wsmux_set_display(dv, muxsc)
	struct device *dv;
	struct wsmux_softc *muxsc;
{
	struct wsmux_softc *sc = (struct wsmux_softc *)dv;
	struct wsmux_softc *nsc = muxsc ? sc : 0;
	struct device *displaydv = muxsc ? muxsc->sc_displaydv : 0;
	struct device *odisplaydv;
	struct wsplink *m;
	int error, ok;

	DPRINTF(("wsmux_set_display: %s: displaydv=%p\n",
		 sc->sc_dv.dv_xname, displaydv));

	if (displaydv) {
		if (sc->sc_displaydv)
			return (EBUSY);
	} else {
		if (sc->sc_displaydv == NULL)
			return (ENXIO);
	}

	odisplaydv = sc->sc_displaydv;
	sc->sc_displaydv = displaydv;

	if (displaydv)
		printf("%s: connecting to %s\n",
		       sc->sc_dv.dv_xname, displaydv->dv_xname);
	ok = 0;
	error = 0;
	for (m = LIST_FIRST(&sc->sc_reals); m; m = LIST_NEXT(m, next)) {
		if (m->sc_ops->dsetdisplay &&
		    (nsc ? m->sc_mevents->io == 0 && *m->sc_muxp == 0 : 
		           *m->sc_muxp == sc)) {
			error = m->sc_ops->dsetdisplay(m->sc, nsc);
			DPRINTF(("wsmux_set_display: m=%p dev=%s error=%d\n", 
				 m, m->sc->dv_xname, error));
			if (!error) {
				ok = 1;
				*m->sc_muxp = nsc;
#ifdef WSDISPLAY_COMPAT_RAWKBD
				DPRINTF(("wsmux_set_display: on %s set rawkbd=%d\n",
					 m->sc->dv_xname, sc->sc_rawkbd));
				(void)m->sc_ops->dioctl(m->sc, 
					     WSKBDIO_SETMODE, 
					     (caddr_t)&sc->sc_rawkbd,
					     0, 0);
#endif
			}
		}
	}
	if (ok)
		error = 0;

	if (displaydv == NULL)
		printf("%s: disconnecting from %s\n", 
		       sc->sc_dv.dv_xname, odisplaydv->dv_xname);

	return (error);
}

#endif /* NWSMUX > 0 || (NWSDISPLAY > 0 && NWSKBD > 0) */
