/*	$NetBSD: hil.c,v 1.79.2.1 2008/03/24 07:14:56 keiichi Exp $	*/

/*
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: hil.c 1.38 92/01/21$
 *
 *	@(#)hil.c	8.2 (Berkeley) 1/12/94
 */
/*
 * Copyright (c) 1988 University of Utah.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: hil.c 1.38 92/01/21$
 *
 *	@(#)hil.c	8.2 (Berkeley) 1/12/94
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: hil.c,v 1.79.2.1 2008/03/24 07:14:56 keiichi Exp $");

#include "ite.h"
#include "rnd.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/uio.h>
#include <sys/user.h>
#include <sys/kauth.h>

#include <uvm/uvm_extern.h>

#if NRND > 0
#include <sys/rnd.h>
#endif

#include <hp300/dev/intiovar.h>

#include <hp300/dev/hilreg.h>
#include <hp300/dev/hilioctl.h>
#include <hp300/dev/hilvar.h>
#include <hp300/dev/itevar.h>
#include <hp300/dev/kbdmap.h>

#include <machine/bus.h>
#include <machine/cpu.h>

#include "ioconf.h"

static int	hilmatch(struct device *, struct cfdata *, void *);
static void	hilattach(struct device *, struct device *, void *);

CFATTACH_DECL(hil, sizeof(struct hil_softc),
    hilmatch, hilattach, NULL, NULL);

static struct	_hilbell default_bell = { BELLDUR, BELLFREQ };

#ifdef DEBUG
int	hildebug = 0;
#define HDB_FOLLOW	0x01
#define HDB_MMAP	0x02
#define HDB_MASK	0x04
#define HDB_CONFIG	0x08
#define HDB_KEYBOARD	0x10
#define HDB_IDMODULE	0x20
#define HDB_EVENTS	0x80
#endif

extern struct kbdmap kbd_map[];

/* symbolic sleep message strings */
static const char hilin[] = "hilin";

static dev_type_open(hilopen);
static dev_type_close(hilclose);
static dev_type_read(hilread);
static dev_type_ioctl(hilioctl);
static dev_type_poll(hilpoll);
static dev_type_kqfilter(hilkqfilter);

const struct cdevsw hil_cdevsw = {
	hilopen, hilclose, hilread, nullwrite, hilioctl,
	nostop, notty, hilpoll, nommap, hilkqfilter,
};

static void	hilattach_deferred(struct device *);

static void	hilinfo(struct hil_softc *);
static void	hilconfig(struct hil_softc *);
static void	hilreset(struct hil_softc *);
static void	hilbeep(struct hil_softc *, const struct _hilbell *);
static int	hiliddev(struct hil_softc *);

static int	hilint(void *);
static void	hil_process_int(struct hil_softc *, u_char, u_char);
static void	hilevent(struct hil_softc *);
static void	hpuxhilevent(struct hil_softc *, struct hilloopdev *);

static int	hilqalloc(struct hil_softc *, struct hilqinfo *, struct proc *);
static int	hilqfree(struct hil_softc *, int, struct proc *);
static int	hilqmap(struct hil_softc *, int, int, struct lwp *);
static int	hilqunmap(struct hil_softc *, int, int, struct proc *);

#ifdef DEBUG
static void	printhilpollbuf(struct hil_softc *);
static void	printhilcmdbuf(struct hil_softc *);
static void	hilreport(struct hil_softc *);
#endif /* DEBUG */

static int
hilmatch(struct device *parent, struct cfdata *match, void *aux)
{
	struct intio_attach_args *ia = aux;

	if (strcmp("hil", ia->ia_modname) != 0)
		return 0;

	return 1;
}

static void
hilattach(struct device *parent, struct device *self, void *aux)
{
	struct hil_softc *hilp = (struct hil_softc *)self;
	struct intio_attach_args *ia = aux;
	int i;

	printf("\n");

#ifdef DEBUG
	if (hildebug & HDB_FOLLOW)
		printf("hilsoftinit(%p, %p)\n", hilp, (void *)ia->ia_addr);
#endif
	/*
	 * Initialize loop information
	 */
	hilp->hl_addr = (struct hil_dev *)ia->ia_addr;
	hilp->hl_cmdending = false;
	hilp->hl_actdev = hilp->hl_cmddev = 0;
	hilp->hl_cmddone = false;
	hilp->hl_cmdbp = hilp->hl_cmdbuf;
	hilp->hl_pollbp = hilp->hl_pollbuf;
	hilp->hl_kbddev = 0;
	hilp->hl_kbdflags = 0;
	/*
	 * Clear all queues and device associations with queues
	 */
	for (i = 0; i < NHILQ; i++) {
		hilp->hl_queue[i].hq_eventqueue = NULL;
		hilp->hl_queue[i].hq_procp = NULL;
		hilp->hl_queue[i].hq_devmask = 0;
	}
	for (i = 0; i < NHILD; i++) {
		selinit(&hilp->hl_device[i].hd_selr);
		hilp->hl_device[i].hd_qmask = 0;
	}
	hilp->hl_device[HILLOOPDEV].hd_flags = (HIL_ALIVE|HIL_PSEUDO);

	/*
	 * Set up default keyboard language.  We always default
	 * to US ASCII - it seems to work OK for non-recognized
	 * keyboards.
	 */

	hilp->hl_kbdlang = KBD_DEFAULT;
#if NITE > 0
	{
		struct kbdmap *km;
		for (km = kbd_map; km->kbd_code; km++) {
			if (km->kbd_code == KBD_US)
				iteinstallkeymap(km);
		}
	}
#endif

	(void) intio_intr_establish(hilint, hilp, ia->ia_ipl, IPL_TTY);

	config_interrupts(self, hilattach_deferred);
}

static void
hilattach_deferred(struct device *self)
{
	struct hil_softc *hilp = (struct hil_softc *)self;

#ifdef DEBUG
	if (hildebug & HDB_FOLLOW)
		printf("hilinit(%p, %p)\n", hilp, hilp->hl_addr);
#endif
	/*
	 * Initialize hardware.
	 * Reset the loop hardware, and collect keyboard/id info
	 */
	hilreset(hilp);
	hilinfo(hilp);
	hilkbdenable(hilp);
}

/* ARGSUSED */
static int
hilopen(dev_t dev, int flags, int mode, struct lwp *l)
{
	struct hil_softc *hilp;
	struct hilloopdev *dptr;
	int s;
#ifdef DEBUG
	struct proc *p = l->l_proc;
#endif

	hilp = device_lookup(&hil_cd, HILLOOP(dev));

#ifdef DEBUG
	if (hildebug & HDB_FOLLOW)
		printf("hilopen(%d): loop %x device %x\n",
		    p->p_pid, HILLOOP(dev), HILUNIT(dev));
#endif

	if ((hilp->hl_device[HILLOOPDEV].hd_flags & HIL_ALIVE) == 0)
		return ENXIO;

	dptr = &hilp->hl_device[HILUNIT(dev)];
	if ((dptr->hd_flags & HIL_ALIVE) == 0)
		return ENODEV;

	/*
	 * Pseudo-devices cannot be read, nothing more to do.
	 */
	if (dptr->hd_flags & HIL_PSEUDO)
		return 0;

	/*
	 * Open semantics:
	 * 1.	Open devices have only one of HIL_READIN/HIL_QUEUEIN.
	 * 2.	HPUX processes always get read syscall interface and
	 *	must have exclusive use of the device.
	 * 3.	BSD processes default to shared queue interface.
	 *	Multiple processes can open the device.
	 */
	if (dptr->hd_flags & HIL_READIN)
		return EBUSY;
	dptr->hd_flags |= HIL_QUEUEIN;
	if (flags & FNONBLOCK)
		dptr->hd_flags |= HIL_NOBLOCK;
	/*
	 * It is safe to flush the read buffer as we are guaranteed
	 * that no one else is using it.
	 */
	if ((dptr->hd_flags & HIL_OPENED) == 0) {
		dptr->hd_flags |= HIL_OPENED;
		clalloc(&dptr->hd_queue, HILMAXCLIST, 0);
	}

	send_hil_cmd(hilp->hl_addr, HIL_INTON, NULL, 0, NULL);
	/*
	 * Opened the keyboard, put in raw mode.
	 */
	s = splhil();
	if (HILUNIT(dev) == hilp->hl_kbddev) {
		u_char mask = 0;
		send_hil_cmd(hilp->hl_addr, HIL_WRITEKBDSADR, &mask, 1, NULL);
		hilp->hl_kbdflags |= KBD_RAW;
#ifdef DEBUG
		if (hildebug & HDB_KEYBOARD)
			printf("hilopen: keyboard %d raw\n", hilp->hl_kbddev);
#endif
	}
	splx(s);
	return 0;
}

/* ARGSUSED */
static int
hilclose(dev_t dev, int flags, int mode, struct lwp *l)
{
	struct hil_softc *hilp;
	struct hilloopdev *dptr;
	int i;
	char mask, lpctrl;
	int s;
	extern struct emul emul_netbsd;
#ifdef DEBUG
	struct proc *p = l->l_proc;
#endif

	hilp = device_lookup(&hil_cd, HILLOOP(dev));

#ifdef DEBUG
	if (hildebug & HDB_FOLLOW)
		printf("hilclose(%d): device %x\n", p->p_pid, HILUNIT(dev));
#endif

	dptr = &hilp->hl_device[HILUNIT(dev)];
	if (HILUNIT(dev) && (dptr->hd_flags & HIL_PSEUDO))
		return 0;

	if (l && l->l_proc->p_emul == &emul_netbsd) {
		/*
		 * If this is the loop device,
		 * free up all queues belonging to this process.
		 */
		if (HILUNIT(dev) == 0) {
			for (i = 0; i < NHILQ; i++)
				if (hilp->hl_queue[i].hq_procp == l->l_proc)
					(void) hilqfree(hilp, i, l->l_proc);
		} else {
			mask = ~hildevmask(HILUNIT(dev));
			s = splhil();
			for (i = 0; i < NHILQ; i++)
				if (hilp->hl_queue[i].hq_procp == l->l_proc) {
					dptr->hd_qmask &= ~hilqmask(i);
					hilp->hl_queue[i].hq_devmask &= mask;
				}
			splx(s);
		}
	}
	/*
	 * The read buffer can go away.
	 */
	dptr->hd_flags &= ~(HIL_QUEUEIN|HIL_READIN|HIL_NOBLOCK|HIL_OPENED);
	clfree(&dptr->hd_queue);
	/*
	 * Set keyboard back to cooked mode when closed.
	 */
	s = splhil();
	if (HILUNIT(dev) && HILUNIT(dev) == hilp->hl_kbddev) {
		mask = 1 << (hilp->hl_kbddev - 1);
		send_hil_cmd(hilp->hl_addr, HIL_WRITEKBDSADR, &mask, 1, NULL);
		hilp->hl_kbdflags &= ~(KBD_RAW|KBD_AR1|KBD_AR2);
		/*
		 * XXX: We have had trouble with keyboards remaining raw
		 * after close due to the LPC_KBDCOOK bit getting cleared
		 * somewhere along the line.  Hence we check and reset
		 * LPCTRL if necessary.
		 */
		send_hil_cmd(hilp->hl_addr, HIL_READLPCTRL, NULL, 0, &lpctrl);
		if ((lpctrl & LPC_KBDCOOK) == 0) {
			printf("hilclose: bad LPCTRL %x, reset to %x\n",
			    lpctrl, lpctrl|LPC_KBDCOOK);
			lpctrl |= LPC_KBDCOOK;
			send_hil_cmd(hilp->hl_addr, HIL_WRITELPCTRL,
					&lpctrl, 1, NULL);
		}
#ifdef DEBUG
		if (hildebug & HDB_KEYBOARD)
			printf("hilclose: keyboard %d cooked\n",
			    hilp->hl_kbddev);
#endif
		hilkbdenable(hilp);
	}
	splx(s);
	return 0;
}

/*
 * Read interface to HIL device.
 */
/* ARGSUSED */
static int
hilread(dev_t dev, struct uio *uio, int flag)
{
	struct hil_softc *hilp;
	struct hilloopdev *dptr;
	int cc;
	u_char buf[HILBUFSIZE];
	int error, s;

	hilp = device_lookup(&hil_cd, HILLOOP(dev));

#if 0
	/*
	 * XXX: Don't do this since HP-UX doesn't.
	 *
	 * Check device number.
	 * This check is necessary since loop can reconfigure.
	 */
	if (HILUNIT(dev) > hilp->hl_maxdev)
		return ENODEV;
#endif

	dptr = &hilp->hl_device[HILUNIT(dev)];
	if ((dptr->hd_flags & HIL_READIN) == 0)
		return ENODEV;

	s = splhil();
	while (dptr->hd_queue.c_cc == 0) {
		if (dptr->hd_flags & HIL_NOBLOCK) {
			spl0();
			return EWOULDBLOCK;
		}
		dptr->hd_flags |= HIL_ASLEEP;
		if ((error = tsleep((void *)dptr,
		    TTIPRI | PCATCH, hilin, 0))) {
			(void)spl0();
			return error;
		}
	}
	splx(s);

	error = 0;
	while (uio->uio_resid > 0 && error == 0) {
		cc = q_to_b(&dptr->hd_queue, buf,
		    min(uio->uio_resid, HILBUFSIZE));
		if (cc <= 0)
			break;
		error = uiomove(buf, cc, uio);
	}
	return error;
}

static int
hilioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct hil_softc *hilp;
	struct hilloopdev *dptr;
	uint8_t *buf;
	int i;
	u_char hold;
	int error;

	hilp = device_lookup(&hil_cd, HILLOOP(dev));

#ifdef DEBUG
	if (hildebug & HDB_FOLLOW)
		printf("hilioctl(%d): dev %x cmd %lx\n",
		    l->l_proc->p_pid, HILUNIT(dev), cmd);
#endif

	dptr = &hilp->hl_device[HILUNIT(dev)];
	if ((dptr->hd_flags & HIL_ALIVE) == 0)
		return ENODEV;

	/*
	 * Don't allow hardware ioctls on virtual devices.
	 * Note that though these are the BSD names, they have the same
	 * values as the HP-UX equivalents so we catch them as well.
	 */
	if (dptr->hd_flags & HIL_PSEUDO) {
		switch (cmd) {
		case HILIOCSC:
		case HILIOCID:
		case OHILIOCID:
		case HILIOCRN:
		case HILIOCRS:
		case HILIOCED:
			return ENODEV;

		/*
		 * XXX: should also return ENODEV but HP-UX compat
		 * breaks if we do.  They work ok right now because
		 * we only recognize one keyboard on the loop.  This
		 * will have to change if we remove that restriction.
		 */
		case HILIOCAROFF:
		case HILIOCAR1:
		case HILIOCAR2:
			break;

		default:
			break;
		}
	}

	hilp->hl_cmdbp = hilp->hl_cmdbuf;
	memset((void *)hilp->hl_cmdbuf, 0, HILBUFSIZE);
	hilp->hl_cmddev = HILUNIT(dev);
	error = 0;
	switch (cmd) {

	case HILIOCSBP:
		/* Send four data bytes to the tone gererator. */
		send_hil_cmd(hilp->hl_addr, HIL_STARTCMD, data, 4, NULL);
		/* Send the trigger beeper command to the 8042. */
		send_hil_cmd(hilp->hl_addr, (cmd & 0xFF), NULL, 0, NULL);
		break;

	case OHILIOCRRT:
	case HILIOCRRT:
		/* Transfer the real time to the 8042 data buffer */
		send_hil_cmd(hilp->hl_addr, (cmd & 0xFF), NULL, 0, NULL);
		/* Read each byte of the real time */
		buf = data;
		for (i = 0; i < 5; i++) {
			send_hil_cmd(hilp->hl_addr, HIL_READTIME + i, NULL,
					0, &hold);
			buf[4 - i] = hold;
		}
		break;

	case HILIOCRT:
		buf = data;
		for (i = 0; i < 4; i++) {
			send_hil_cmd(hilp->hl_addr, (cmd & 0xFF) + i,
					NULL, 0, &hold);
			buf[i] = hold;
		}
		break;

	case HILIOCID:
	case OHILIOCID:
	case HILIOCSC:
	case HILIOCRN:
	case HILIOCRS:
	case HILIOCED:
		send_hildev_cmd(hilp, HILUNIT(dev), (cmd & 0xFF));
		memcpy(data, hilp->hl_cmdbuf, hilp->hl_cmdbp-hilp->hl_cmdbuf);
		break;

	case HILIOCAROFF:
	case HILIOCAR1:
	case HILIOCAR2:
		if (hilp->hl_kbddev) {
			hilp->hl_cmddev = hilp->hl_kbddev;
			send_hildev_cmd(hilp, hilp->hl_kbddev, (cmd & 0xFF));
			hilp->hl_kbdflags &= ~(KBD_AR1|KBD_AR2);
			if (cmd == HILIOCAR1)
				hilp->hl_kbdflags |= KBD_AR1;
			else if (cmd == HILIOCAR2)
				hilp->hl_kbdflags |= KBD_AR2;
		}
		break;

	case HILIOCBEEP:
		hilbeep(hilp, (struct _hilbell *)data);
		break;

	case FIONBIO:
		dptr = &hilp->hl_device[HILUNIT(dev)];
		if (*(int *)data)
			dptr->hd_flags |= HIL_NOBLOCK;
		else
			dptr->hd_flags &= ~HIL_NOBLOCK;
		break;

	/*
	 * FIOASYNC must be present for FIONBIO above to work!
	 * (See fcntl in kern_descrip.c).
	 */
	case FIOASYNC:
		break;

	case HILIOCALLOCQ:
		error = hilqalloc(hilp, (struct hilqinfo *)data, l->l_proc);
		break;

	case HILIOCFREEQ:
		error = hilqfree(hilp, ((struct hilqinfo *)data)->qid, l->l_proc);
		break;

	case HILIOCMAPQ:
		error = hilqmap(hilp, *(int *)data, HILUNIT(dev), l);
		break;

	case HILIOCUNMAPQ:
		error = hilqunmap(hilp, *(int *)data, HILUNIT(dev), l->l_proc);
		break;

	case HILIOCHPUX:
		dptr = &hilp->hl_device[HILUNIT(dev)];
		dptr->hd_flags |= HIL_READIN;
		dptr->hd_flags &= ~HIL_QUEUEIN;
		break;

	case HILIOCRESET:
		hilreset(hilp);
		break;

#ifdef DEBUG
	case HILIOCTEST:
		hildebug = *(int *) data;
		break;
#endif

	default:
		error = EINVAL;
		break;

	}
	hilp->hl_cmddev = 0;
	return error;
}

/*ARGSUSED*/
static int
hilpoll(dev_t dev, int events, struct lwp *l)
{
	struct hil_softc *hilp;
	struct hilloopdev *dptr;
	struct hiliqueue *qp;
	int mask;
	int s, revents;

	hilp = device_lookup(&hil_cd, HILLOOP(dev));

	revents = events & (POLLOUT | POLLWRNORM);

	/* Attempt to save some work. */
	if ((events & (POLLIN | POLLRDNORM)) == 0)
		return revents;

	/*
	 * Read interface.
	 * Return 1 if there is something in the queue, 0 ow.
	 */
	dptr = &hilp->hl_device[HILUNIT(dev)];
	if (dptr->hd_flags & HIL_READIN) {
		s = splhil();
		if (dptr->hd_queue.c_cc > 0)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(l, &dptr->hd_selr);
		splx(s);
		return revents;
	}

	/*
	 * Make sure device is alive and real (or the loop device).
	 * Note that we do not do this for the read interface.
	 * This is primarily to be consistant with HP-UX.
	 */
	if (HILUNIT(dev) &&
	    (dptr->hd_flags & (HIL_ALIVE|HIL_PSEUDO)) != HIL_ALIVE)
		return revents | (events & (POLLIN | POLLRDNORM));

	/*
	 * Select on loop device is special.
	 * Check to see if there are any data for any loop device
	 * provided it is associated with a queue belonging to this user.
	 */
	if (HILUNIT(dev) == 0)
		mask = -1;
	else
		mask = hildevmask(HILUNIT(dev));
	/*
	 * Must check everybody with interrupts blocked to prevent races.
	 */
	s = splhil();
	for (qp = hilp->hl_queue; qp < &hilp->hl_queue[NHILQ]; qp++)
		if (qp->hq_procp == l->l_proc && (mask & qp->hq_devmask) &&
		    qp->hq_eventqueue->hil_evqueue.head !=
		    qp->hq_eventqueue->hil_evqueue.tail) {
			splx(s);
			return revents | (events & (POLLIN | POLLRDNORM));
		}

	selrecord(l, &dptr->hd_selr);
	splx(s);
	return revents;
}

static void
filt_hilrdetach(struct knote *kn)
{
	dev_t dev = (intptr_t) kn->kn_hook;
	struct hil_softc *hilp = hil_cd.cd_devs[HILLOOP(dev)];
	struct hilloopdev *dptr = &hilp->hl_device[HILUNIT(dev)];
	int s;

	s = splhil();
	SLIST_REMOVE(&dptr->hd_selr.sel_klist, kn, knote, kn_selnext);
	splx(s);
}

static int
filt_hilread(struct knote *kn, long hint)
{
	dev_t dev = (intptr_t) kn->kn_hook;
	int device = HILUNIT(dev);
	struct hil_softc *hilp = hil_cd.cd_devs[HILLOOP(dev)];
	struct hilloopdev *dptr = &hilp->hl_device[device];
	struct hiliqueue *qp;
	int mask;

	if (dptr->hd_flags & HIL_READIN) {
		kn->kn_data = dptr->hd_queue.c_cc;
		return kn->kn_data > 0;
	}

	/*
	 * Make sure device is alive and real (or the loop device).
	 * Note that we do not do this for the read interface.
	 * This is primarily to be consistant with HP-UX.
	 */
	if (device && (dptr->hd_flags & (HIL_ALIVE|HIL_PSEUDO)) != HIL_ALIVE) {
		kn->kn_data = 0; /* XXXLUKEM (thorpej): what to put here? */
		return 1;
	}

	/*
	 * Select on loop device is special.
	 * Check to see if there are any data for any loop device
	 * provided it is associated with a queue belonging to this user.
	 */
	if (device == 0)
		mask = -1;
	else
		mask = hildevmask(device);
	/*
	 * Must check everybody with interrupts blocked to prevent races.
	 * (Interrupts are already blocked.)
	 */
	for (qp = hilp->hl_queue; qp < &hilp->hl_queue[NHILQ]; qp++) {
		/* XXXLUKEM (thorpej): PROCESS CHECK! */
		if (/*qp->hq_procp == l->l_proc &&*/ (mask & qp->hq_devmask) &&
		    qp->hq_eventqueue->hil_evqueue.head !=
		    qp->hq_eventqueue->hil_evqueue.tail) {
			/* XXXLUKEM (thorpej): what to put here? */
			kn->kn_data = 0;
			return 1;
		}
	}

	return 0;
}

static const struct filterops hilread_filtops =
	{ 1, NULL, filt_hilrdetach, filt_hilread };

static const struct filterops hil_seltrue_filtops =
	{ 1, NULL, filt_hilrdetach, filt_seltrue };

static int
hilkqfilter(dev_t dev, struct knote *kn)
{
	struct hil_softc *hilp = hil_cd.cd_devs[HILLOOP(dev)];
	struct hilloopdev *dptr = &hilp->hl_device[HILUNIT(dev)];
	struct klist *klist;
	int s;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		klist = &dptr->hd_selr.sel_klist;
		kn->kn_fop = &hilread_filtops;
		break;

	case EVFILT_WRITE:
		klist = &dptr->hd_selr.sel_klist;
		kn->kn_fop = &hil_seltrue_filtops;
		break;

	default:
		return 1;
	}

	kn->kn_hook = (void *)(intptr_t) dev; /* XXX yuck */

	s = splhil();
	SLIST_INSERT_HEAD(klist, kn, kn_selnext);
	splx(s);

	return 0;
}

/*ARGSUSED*/
static int
hilint(void *v)
{
	struct hil_softc *hilp = v;
	struct hil_dev *hildevice = hilp->hl_addr;
	u_char c, stat;

	stat = READHILSTAT(hildevice);
	c = READHILDATA(hildevice);		/* clears interrupt */
	hil_process_int(hilp, stat, c);
#if NRND > 0
	rnd_add_uint32(&hilp->rnd_source, (stat<<8)|c);
#endif
	return 1;
}

static void
hil_process_int(struct hil_softc *hilp, u_char stat, u_char c)
{
#ifdef DEBUG
	if (hildebug & HDB_EVENTS)
		printf("hilint: %x %x\n", stat, c);
#endif

	/* the shift enables the compiler to generate a jump table */
	switch ((stat>>HIL_SSHIFT) & HIL_SMASK) {

#if NITE > 0
	case HIL_KEY:
	case HIL_SHIFT:
	case HIL_CTRL:
	case HIL_CTRLSHIFT:
		itefilter(stat, c);
		return;
#endif

	case HIL_STATUS:			/* The status info. */
		if (c & HIL_ERROR) {
			hilp->hl_cmddone = true;
			if (c == HIL_RECONFIG)
				hilconfig(hilp);
			break;
		}
		if (c & HIL_COMMAND) {
			if (c & HIL_POLLDATA)	/* End of data */
				hilevent(hilp);
			else			/* End of command */
				hilp->hl_cmdending = true;
			hilp->hl_actdev = 0;
		} else {
			if (c & HIL_POLLDATA) {	/* Start of polled data */
				if (hilp->hl_actdev != 0)
					hilevent(hilp);
				hilp->hl_actdev = (c & HIL_DEVMASK);
				hilp->hl_pollbp = hilp->hl_pollbuf;
			} else {		/* Start of command */
				if (hilp->hl_cmddev == (c & HIL_DEVMASK)) {
					hilp->hl_cmdbp = hilp->hl_cmdbuf;
					hilp->hl_actdev = 0;
				}
			}
		}
		return;

	case HIL_DATA:
		if (hilp->hl_actdev != 0)	/* Collecting poll data */
			*hilp->hl_pollbp++ = c;
		else {
			if (hilp->hl_cmddev != 0) {  /* Collecting cmd data */
				if (hilp->hl_cmdending) {
					hilp->hl_cmddone = true;
					hilp->hl_cmdending = false;
				} else
					*hilp->hl_cmdbp++ = c;
			}
		}
		return;

	case 0:		/* force full jump table */
	default:
		return;
	}

}

/*
 * Optimized macro to compute:
 *	eq->head == (eq->tail + 1) % eq->size
 * i.e. has tail caught up with head.  We do this because 32 bit long
 * remaidering is expensive (a function call with our compiler).
 */
#define HQFULL(eq)	(((eq)->head?(eq)->head:(eq)->size) == (eq)->tail+1)
#define HQVALID(eq) \
	((eq)->size == HEVQSIZE && (eq)->tail >= 0 && (eq)->tail < HEVQSIZE)

static void
hilevent(struct hil_softc *hilp)
{
	struct hilloopdev *dptr = &hilp->hl_device[hilp->hl_actdev];
	int len, mask, qnum;
	u_char *cp, *pp;
	HILQ *hq;
	struct timeval ourtime;
	hil_packet *proto;
	int len0;
	long tenths;

#ifdef DEBUG
	if (hildebug & HDB_EVENTS) {
		printf("hilevent: dev %d pollbuf: ", hilp->hl_actdev);
		printhilpollbuf(hilp);
		printf("\n");
	}
#endif

	/*
	 * Note that HIL_READIN effectively "shuts off" any queues
	 * that may have been in use at the time of an HILIOCHPUX call.
	 */
	if (dptr->hd_flags & HIL_READIN) {
		hpuxhilevent(hilp, dptr);
		return;
	}

	/*
	 * If this device isn't on any queue or there are no data
	 * in the packet (can this happen?) do nothing.
	 */
	if (dptr->hd_qmask == 0 ||
	    (len0 = hilp->hl_pollbp - hilp->hl_pollbuf) <= 0)
		return;

	/*
	 * Everybody gets the same time stamp
	 */
	microtime(&ourtime);
	tenths = (ourtime.tv_sec * 100) + (ourtime.tv_usec / 10000);

	proto = NULL;
	mask = dptr->hd_qmask;
	for (qnum = 0; mask; qnum++) {
		if ((mask & hilqmask(qnum)) == 0)
			continue;
		mask &= ~hilqmask(qnum);
		hq = hilp->hl_queue[qnum].hq_eventqueue;

		/*
		 * Ensure that queue fields that we rely on are valid
		 * and that there is space in the queue.  If either
		 * test fails, we just skip this queue.
		 */
		if (!HQVALID(&hq->hil_evqueue) || HQFULL(&hq->hil_evqueue))
			continue;

		/*
		 * Copy data to queue.
		 * If this is the first queue we construct the packet
		 * with length, timestamp and poll buffer data.
		 * For second and successive packets we just duplicate
		 * the first packet.
		 */
		pp = (u_char *) &hq->hil_event[hq->hil_evqueue.tail];
		if (proto == NULL) {
			proto = (hil_packet *)pp;
			cp = hilp->hl_pollbuf;
			len = len0;
			*pp++ = len + 6;
			*pp++ = hilp->hl_actdev;
			*(long *)pp = tenths;
			pp += sizeof(long);
			do *pp++ = *cp++; while (--len);
		} else
			*(hil_packet *)pp = *proto;

		if (++hq->hil_evqueue.tail == hq->hil_evqueue.size)
			hq->hil_evqueue.tail = 0;
	}

	/*
	 * Wake up anyone selecting on this device or the loop itself
	 */
	selnotify(&dptr->hd_selr, 0, 0);
	dptr = &hilp->hl_device[HILLOOPDEV];
	selnotify(&dptr->hd_selr, 0, 0);
}

#undef HQFULL

static void
hpuxhilevent(struct hil_softc *hilp, struct hilloopdev *dptr)
{
	int len;
	struct timeval ourtime;
	long tstamp;

	/*
	 * Everybody gets the same time stamp
	 */
	microtime(&ourtime);
	tstamp = (ourtime.tv_sec * 100) + (ourtime.tv_usec / 10000);

	/*
	 * Each packet that goes into the buffer must be preceded by the
	 * number of bytes in the packet, and the timestamp of the packet.
	 * This adds 5 bytes to the packet size. Make sure there is enough
	 * room in the buffer for it, and if not, toss the packet.
	 */
	len = hilp->hl_pollbp - hilp->hl_pollbuf;
	if (dptr->hd_queue.c_cc <= (HILMAXCLIST - (len+5))) {
		putc(len+5, &dptr->hd_queue);
		(void) b_to_q((u_char *)&tstamp, sizeof tstamp, &dptr->hd_queue);
		(void) b_to_q((u_char *)hilp->hl_pollbuf, len, &dptr->hd_queue);
	}

	/*
	 * Wake up any one blocked on a read or select
	 */
	if (dptr->hd_flags & HIL_ASLEEP) {
		dptr->hd_flags &= ~HIL_ASLEEP;
		wakeup((void *)dptr);
	}
	selnotify(&dptr->hd_selr, 0, 0);
}

/*
 * Shared queue manipulation routines
 */

static int
hilqalloc(struct hil_softc *hilp, struct hilqinfo *qip, struct proc *p)
{

#ifdef DEBUG
	if (hildebug & HDB_FOLLOW)
		printf("hilqalloc(%d): addr %p\n", p->p_pid, qip->addr);
#endif
	return EINVAL;
}

static int
hilqfree(struct hil_softc *hilp, int qnum, struct proc *p)
{

#ifdef DEBUG
	if (hildebug & HDB_FOLLOW)
		printf("hilqfree(%d): qnum %d\n", p->p_pid, qnum);
#endif
	return EINVAL;
}

static int
hilqmap(struct hil_softc *hilp, int qnum, int device, struct lwp *l)
{
	struct hilloopdev *dptr = &hilp->hl_device[device];
	int s;

#ifdef DEBUG
	if (hildebug & HDB_FOLLOW)
		printf("hilqmap(%d): qnum %d device %x\n",
		    l->l_proc->p_pid, qnum, device);
#endif
	if (qnum >= NHILQ || hilp->hl_queue[qnum].hq_procp != l->l_proc)
		return EINVAL;
	if ((dptr->hd_flags & HIL_QUEUEIN) == 0)
		return EINVAL;
	if (dptr->hd_qmask && kauth_cred_geteuid(l->l_cred) &&
	    kauth_cred_geteuid(l->l_cred) != dptr->hd_uid)
		return EPERM;

	hilp->hl_queue[qnum].hq_devmask |= hildevmask(device);
	if (dptr->hd_qmask == 0)
		dptr->hd_uid = kauth_cred_geteuid(l->l_cred);
	s = splhil();
	dptr->hd_qmask |= hilqmask(qnum);
	splx(s);
#ifdef DEBUG
	if (hildebug & HDB_MASK)
		printf("hilqmap(%d): devmask %x qmask %x\n",
		    l->l_proc->p_pid, hilp->hl_queue[qnum].hq_devmask,
		    dptr->hd_qmask);
#endif
	return 0;
}

static int
hilqunmap(struct hil_softc *hilp, int qnum, int device, struct proc *p)
{
	int s;

#ifdef DEBUG
	if (hildebug & HDB_FOLLOW)
		printf("hilqunmap(%d): qnum %d device %x\n",
		    p->p_pid, qnum, device);
#endif

	if (qnum >= NHILQ || hilp->hl_queue[qnum].hq_procp != p)
		return EINVAL;

	hilp->hl_queue[qnum].hq_devmask &= ~hildevmask(device);
	s = splhil();
	hilp->hl_device[device].hd_qmask &= ~hilqmask(qnum);
	splx(s);
#ifdef DEBUG
	if (hildebug & HDB_MASK)
		printf("hilqunmap(%d): devmask %x qmask %x\n",
		    p->p_pid, hilp->hl_queue[qnum].hq_devmask,
		    hilp->hl_device[device].hd_qmask);
#endif
	return 0;
}

/*
 * Cooked keyboard functions for ite driver.
 * There is only one "cooked" ITE keyboard (the first keyboard found)
 * per loop.  There may be other keyboards, but they will always be "raw".
 */

void
hilkbdbell(void *v)
{
	hilbeep(v, &default_bell);
}

void
hilkbdenable(void *v)
{
	struct hil_softc *hilp = v;
	struct hil_dev *hildevice = HILADDR;
	char db;

	if (hilp != NULL)
		hildevice = hilp->hl_addr;

	/* Set the autorepeat rate */
	db = ar_format(KBD_ARR);
	send_hil_cmd(hildevice, HIL_SETARR, &db, 1, NULL);

	/* Set the autorepeat delay */
	db = ar_format(KBD_ARD);
	send_hil_cmd(hildevice, HIL_SETARD, &db, 1, NULL);

	/* Enable interrupts */
	send_hil_cmd(hildevice, HIL_INTON, NULL, 0, NULL);
}

void
hilkbddisable(void *v)
{
}

#if NITE > 0
/*
 * The following chunk of code implements HIL console keyboard
 * support.
 */

static struct hil_dev *hilkbd_cn_device;
static struct ite_kbdmap hilkbd_cn_map;
static struct ite_kbdops hilkbd_cn_ops = {
	hilkbdcngetc,
	hilkbdenable,
	hilkbdbell,
	NULL,
};

extern char us_keymap[], us_shiftmap[], us_ctrlmap[];

/*
 * XXX: read keyboard directly and return code.
 * Used by console getchar routine.  Could really screw up anybody
 * reading from the keyboard in the normal, interrupt driven fashion.
 */
int
hilkbdcngetc(int *statp)
{
	int c, stat;
	int s;

	if (hilkbd_cn_device == NULL)
		return 0;

	/*
	 * XXX needs to be splraise because we could be called
	 * XXX at splhigh, e.g. in DDB.
	 */
	s = splhil();
	while (((stat = READHILSTAT(hilkbd_cn_device)) & HIL_DATA_RDY) == 0)
		;
	c = READHILDATA(hilkbd_cn_device);
	splx(s);
	*statp = stat;
	return c;
}

/*
 * Perform basic initialization of the HIL keyboard, suitable
 * for early console use.
 */
int
hilkbdcnattach(bus_space_tag_t bst, bus_addr_t addr)
{
	void *va;
	struct kbdmap *km;
	bus_space_handle_t bsh;
	u_char lang;

	if (bus_space_map(bst, addr, PAGE_SIZE, 0, &bsh))
		return 1;

	va = bus_space_vaddr(bst, bsh);
	hilkbd_cn_device = (struct hil_dev *)va;

	/* Default to US-ASCII keyboard. */
	hilkbd_cn_map.keymap = us_keymap;
	hilkbd_cn_map.shiftmap = us_shiftmap;
	hilkbd_cn_map.ctrlmap = us_ctrlmap;

	HILWAIT(hilkbd_cn_device);
	WRITEHILCMD(hilkbd_cn_device, HIL_SETARR);
	HILWAIT(hilkbd_cn_device);
	WRITEHILDATA(hilkbd_cn_device, ar_format(KBD_ARR));
	HILWAIT(hilkbd_cn_device);
	WRITEHILCMD(hilkbd_cn_device, HIL_READKBDLANG);
	HILDATAWAIT(hilkbd_cn_device);
	lang = READHILDATA(hilkbd_cn_device);
	for (km = kbd_map; km->kbd_code; km++) {
		if (km->kbd_code == lang) {
			hilkbd_cn_map.keymap = km->kbd_keymap;
			hilkbd_cn_map.shiftmap = km->kbd_shiftmap;
			hilkbd_cn_map.ctrlmap = km->kbd_ctrlmap;
		}
	}
	HILWAIT(hilkbd_cn_device);
	WRITEHILCMD(hilkbd_cn_device, HIL_INTON);

	hilkbd_cn_ops.arg = NULL;
	itekbdcnattach(&hilkbd_cn_ops, &hilkbd_cn_map);

	return 0;
}

#endif /* End of HIL console keyboard code. */

/*
 * Recognize and clear keyboard generated NMIs.
 * Returns 1 if it was ours, 0 otherwise.  Note that we cannot use
 * send_hil_cmd() to issue the clear NMI command as that would actually
 * lower the priority to splvm() and it doesn't wait for the completion
 * of the command.  Either of these conditions could result in the
 * interrupt reoccuring.  Note that we issue the CNMT command twice.
 * This seems to be needed, once is not always enough!?!
 */
int
kbdnmi(void)
{
	struct hil_dev *hl_addr = HILADDR;

	if ((*KBDNMISTAT & KBDNMI) == 0)
		return 0;

	HILWAIT(hl_addr);
	WRITEHILCMD(hl_addr, HIL_CNMT);
	HILWAIT(hl_addr);
	WRITEHILCMD(hl_addr, HIL_CNMT);
	HILWAIT(hl_addr);
	return 1;
}

#define HILSECURITY	0x33
#define HILIDENTIFY	0x03
#define HILSCBIT	0x04

/*
 * Called at boot time to print out info about interesting devices
 */
void
hilinfo(struct hil_softc *hilp)
{
	int id, len;
	struct kbdmap *km;

	/*
	 * Keyboard info.
	 */
	if (hilp->hl_kbddev) {
		printf("%s device %d: ", hilp->hl_dev.dv_xname,
		    hilp->hl_kbddev);
		for (km = kbd_map; km->kbd_code; km++)
			if (km->kbd_code == hilp->hl_kbdlang) {
				printf("%s ", km->kbd_desc);
				break;
			}
		printf("keyboard\n");
	}
	/*
	 * ID module.
	 * Attempt to locate the first ID module and print out its
	 * security code.  Is this a good idea??
	 */
	id = hiliddev(hilp);
	if (id) {
		hilp->hl_cmdbp = hilp->hl_cmdbuf;
		hilp->hl_cmddev = id;
		send_hildev_cmd(hilp, id, HILSECURITY);
		len = hilp->hl_cmdbp - hilp->hl_cmdbuf;
		hilp->hl_cmdbp = hilp->hl_cmdbuf;
		hilp->hl_cmddev = 0;
		printf("hil%d: security code", id);
		for (id = 0; id < len; id++)
			printf(" %x", hilp->hl_cmdbuf[id]);
		while (id++ < 16)
			printf(" 0");
		printf("\n");
	}
#if NRND > 0
	/*
	 * attach the device into the random source list
	 * except from ID module (no point)
	 */
	if (!id) {
		char buf[10];
		sprintf(buf, "%s", hilp->hl_dev.dv_xname);
		rnd_attach_source(&hilp->rnd_source, buf, RND_TYPE_TTY, 0);
	}
#endif
}

#define HILAR1	0x3E
#define HILAR2	0x3F

/*
 * Called after the loop has reconfigured.  Here we need to:
 *	- determine how many devices are on the loop
 *	  (some may have been added or removed)
 *	- locate the ITE keyboard (if any) and ensure
 *	  that it is in the proper state (raw or cooked)
 *	  and is set to use the proper language mapping table
 *	- ensure all other keyboards are raw
 * Note that our device state is now potentially invalid as
 * devices may no longer be where they were.  What we should
 * do here is either track where the devices went and move
 * state around accordingly or, more simply, just mark all
 * devices as HIL_DERROR and don't allow any further use until
 * they are closed.  This is a little too brutal for my tastes,
 * we prefer to just assume people won't move things around.
 */
void
hilconfig(struct hil_softc *hilp)
{
	u_char db;
	int s;

	s = splhil();
#ifdef DEBUG
	if (hildebug & HDB_CONFIG) {
		printf("hilconfig: reconfigured: ");
		send_hil_cmd(hilp->hl_addr, HIL_READLPSTAT, NULL, 0, &db);
		printf("LPSTAT %x, ", db);
		send_hil_cmd(hilp->hl_addr, HIL_READLPCTRL, NULL, 0, &db);
		printf("LPCTRL %x, ", db);
		send_hil_cmd(hilp->hl_addr, HIL_READKBDSADR, NULL, 0, &db);
		printf("KBDSADR %x\n", db);
		hilreport(hilp);
	}
#endif
	/*
	 * Determine how many devices are on the loop.
	 * Mark those as alive and real, all others as dead.
	 */
	db = 0;
	send_hil_cmd(hilp->hl_addr, HIL_READLPSTAT, NULL, 0, &db);
	hilp->hl_maxdev = db & LPS_DEVMASK;
#ifdef DEBUG
	if (hildebug & HDB_CONFIG)
		printf("hilconfig: %d devices found\n", hilp->hl_maxdev);
#endif
	for (db = 1; db < NHILD; db++) {
		if (db <= hilp->hl_maxdev)
			hilp->hl_device[db].hd_flags |= HIL_ALIVE;
		else
			hilp->hl_device[db].hd_flags &= ~HIL_ALIVE;
		hilp->hl_device[db].hd_flags &= ~HIL_PSEUDO;
	}
#ifdef DEBUG
	if (hildebug & (HDB_CONFIG|HDB_KEYBOARD))
		printf("hilconfig: max device %d\n", hilp->hl_maxdev);
#endif
	if (hilp->hl_maxdev == 0) {
		hilp->hl_kbddev = 0;
		splx(s);
		return;
	}
	/*
	 * Find out where the keyboards are and record the ITE keyboard
	 * (first one found).  If no keyboards found, we are all done.
	 */
	db = 0;
	send_hil_cmd(hilp->hl_addr, HIL_READKBDSADR, NULL, 0, &db);
#ifdef DEBUG
	if (hildebug & HDB_KEYBOARD)
		printf("hilconfig: keyboard: KBDSADR %x, old %d, new %d\n",
		    db, hilp->hl_kbddev, ffs((int)db));
#endif
	hilp->hl_kbddev = ffs((int)db);
	if (hilp->hl_kbddev == 0) {
		splx(s);
		return;
	}
	/*
	 * Determine if the keyboard should be cooked or raw and configure it.
	 */
	db = (hilp->hl_kbdflags & KBD_RAW) ? 0 : 1 << (hilp->hl_kbddev - 1);
	send_hil_cmd(hilp->hl_addr, HIL_WRITEKBDSADR, &db, 1, NULL);
	/*
	 * Re-enable autorepeat in raw mode, cooked mode AR is not affected.
	 */
	if (hilp->hl_kbdflags & (KBD_AR1|KBD_AR2)) {
		db = (hilp->hl_kbdflags & KBD_AR1) ? HILAR1 : HILAR2;
		hilp->hl_cmddev = hilp->hl_kbddev;
		send_hildev_cmd(hilp, hilp->hl_kbddev, db);
		hilp->hl_cmddev = 0;
	}
	/*
	 * Determine the keyboard language configuration, but don't
	 * override a user-specified setting.
	 */
	db = 0;
	send_hil_cmd(hilp->hl_addr, HIL_READKBDLANG, NULL, 0, &db);
#ifdef DEBUG
	if (hildebug & HDB_KEYBOARD)
		printf("hilconfig: language: old %x new %x\n",
		    hilp->hl_kbdlang, db);
#endif
	if (hilp->hl_kbdlang != KBD_SPECIAL) {
		struct kbdmap *km;

#if NITE > 0
		for (km = kbd_map; km->kbd_code; km++) {
			if (km->kbd_code == db) {
				hilp->hl_kbdlang = db;
				iteinstallkeymap(km);
				break;
			}
		}
#endif
		if (km->kbd_code == 0) {
			printf("hilconfig: unknown keyboard type 0x%x, "
			    "using default\n", db);
		}
	}
	splx(s);
}

void
hilreset(struct hil_softc *hilp)
{
	struct hil_dev *hildevice = hilp->hl_addr;
	u_char db;

#ifdef DEBUG
	if (hildebug & HDB_FOLLOW)
		printf("hilreset(%p)\n", hilp);
#endif
	/*
	 * Initialize the loop: reconfigure, don't report errors,
	 * cook keyboards, and enable autopolling.
	 */
	db = LPC_RECONF | LPC_KBDCOOK | LPC_NOERROR | LPC_AUTOPOLL;
	send_hil_cmd(hildevice, HIL_WRITELPCTRL, &db, 1, NULL);
	/*
	 * Delay one second for reconfiguration and then read the
	 * data to clear the interrupt (if the loop reconfigured).
	 */
	DELAY(1000000);
	if (READHILSTAT(hildevice) & HIL_DATA_RDY)
		db = READHILDATA(hildevice);
	/*
	 * The HIL loop may have reconfigured.  If so we proceed on,
	 * if not we loop until a successful reconfiguration is reported
	 * back to us.  The HIL loop will continue to attempt forever.
	 * Probably not very smart.
	 */
	do {
		send_hil_cmd(hildevice, HIL_READLPSTAT, NULL, 0, &db);
	} while ((db & (LPS_CONFFAIL|LPS_CONFGOOD)) == 0);
	/*
	 * At this point, the loop should have reconfigured.
	 * The reconfiguration interrupt has already called hilconfig()
	 * so the keyboard has been determined.
	 */
	send_hil_cmd(hildevice, HIL_INTON, NULL, 0, NULL);
}

void
hilbeep(struct hil_softc *hilp, const struct _hilbell *bp)
{
	struct hil_dev *hl_addr = HILADDR;
	u_char buf[2];

	if (hilp != NULL)
		hl_addr = hilp->hl_addr;

	buf[0] = ~((bp->duration - 10) / 10);
	buf[1] = bp->frequency;
	send_hil_cmd(hl_addr, HIL_SETTONE, buf, 2, NULL);
}

/*
 * Locate and return the address of the first ID module, 0 if none present.
 */
int
hiliddev(struct hil_softc *hilp)
{
	int i, len;

#ifdef DEBUG
	if (hildebug & HDB_IDMODULE)
		printf("hiliddev(%p): max %d, looking for idmodule...",
		    hilp, hilp->hl_maxdev);
#endif
	for (i = 1; i <= hilp->hl_maxdev; i++) {
		hilp->hl_cmdbp = hilp->hl_cmdbuf;
		hilp->hl_cmddev = i;
		send_hildev_cmd(hilp, i, HILIDENTIFY);
		/*
		 * XXX: the final condition checks to ensure that the
		 * device ID byte is in the range of the ID module (0x30-0x3F)
		 */
		len = hilp->hl_cmdbp - hilp->hl_cmdbuf;
		if (len > 1 && (hilp->hl_cmdbuf[1] & HILSCBIT) &&
		    (hilp->hl_cmdbuf[0] & 0xF0) == 0x30) {
			hilp->hl_cmdbp = hilp->hl_cmdbuf;
			hilp->hl_cmddev = i;
			send_hildev_cmd(hilp, i, HILSECURITY);
			break;
		}
	}
	hilp->hl_cmdbp = hilp->hl_cmdbuf;
	hilp->hl_cmddev = 0;
#ifdef DEBUG
	if (hildebug & HDB_IDMODULE) {
		if (i <= hilp->hl_maxdev)
			printf("found at %d\n", i);
		else
			printf("not found\n");
	}
#endif
	return i <= hilp->hl_maxdev ? i : 0;
}

/*
 * Low level routines which actually talk to the 8042 chip.
 */

/*
 * Send a command to the 8042 with zero or more bytes of data.
 * If rdata is non-null, wait for and return a byte of data.
 * We run at splvm() to make the transaction as atomic as
 * possible without blocking the clock (is this necessary?)
 */
void
send_hil_cmd(struct hil_dev *hildevice, u_char cmd, u_char *data, u_char dlen,
    u_char *rdata)
{
	u_char status;
	int s = splvm();

	HILWAIT(hildevice);
	WRITEHILCMD(hildevice, cmd);
	while (dlen--) {
		HILWAIT(hildevice);
		WRITEHILDATA(hildevice, *data++);
	}
	if (rdata) {
		do {
			HILDATAWAIT(hildevice);
			status = READHILSTAT(hildevice);
			*rdata = READHILDATA(hildevice);
		} while (((status >> HIL_SSHIFT) & HIL_SMASK) != HIL_68K);
	}
	splx(s);
}

/*
 * Send a command to a device on the loop.
 * Since only one command can be active on the loop at any time,
 * we must ensure that we are not interrupted during this process.
 * Hence we mask interrupts to prevent potential access from most
 * interrupt routines and turn off auto-polling to disable the
 * internally generated poll commands.
 *
 * splhigh is extremely conservative but insures atomic operation,
 * splvm (clock only interrupts) seems to be good enough in practice.
 */
void
send_hildev_cmd(struct hil_softc *hilp, char device, char cmd)
{
	struct hil_dev *hildevice = hilp->hl_addr;
	u_char status, c;
	int s = splvm();

	polloff(hildevice);

	/*
	 * Transfer the command and device info to the chip
	 */
	HILWAIT(hildevice);
	WRITEHILCMD(hildevice, HIL_STARTCMD);
	HILWAIT(hildevice);
	WRITEHILDATA(hildevice, 8 + device);
	HILWAIT(hildevice);
	WRITEHILDATA(hildevice, cmd);
	HILWAIT(hildevice);
	WRITEHILDATA(hildevice, HIL_TIMEOUT);
	/*
	 * Trigger the command and wait for completion
	 */
	HILWAIT(hildevice);
	WRITEHILCMD(hildevice, HIL_TRIGGER);
	hilp->hl_cmddone = false;
	do {
		HILDATAWAIT(hildevice);
		status = READHILSTAT(hildevice);
		c = READHILDATA(hildevice);
		hil_process_int(hilp, status, c);
	} while (!hilp->hl_cmddone);

	pollon(hildevice);
	splx(s);
}

/*
 * Turn auto-polling off and on.
 * Also disables and enable auto-repeat.  Why?
 */
void
polloff(struct hil_dev *hildevice)
{
	char db;

	/*
	 * Turn off auto repeat
	 */
	HILWAIT(hildevice);
	WRITEHILCMD(hildevice, HIL_SETARR);
	HILWAIT(hildevice);
	WRITEHILDATA(hildevice, 0);
	/*
	 * Turn off auto-polling
	 */
	HILWAIT(hildevice);
	WRITEHILCMD(hildevice, HIL_READLPCTRL);
	HILDATAWAIT(hildevice);
	db = READHILDATA(hildevice);
	db &= ~LPC_AUTOPOLL;
	HILWAIT(hildevice);
	WRITEHILCMD(hildevice, HIL_WRITELPCTRL);
	HILWAIT(hildevice);
	WRITEHILDATA(hildevice, db);
	/*
	 * Must wait til polling is really stopped
	 */
	do {
		HILWAIT(hildevice);
		WRITEHILCMD(hildevice, HIL_READBUSY);
		HILDATAWAIT(hildevice);
		db = READHILDATA(hildevice);
	} while (db & BSY_LOOPBUSY);
}

void
pollon(struct hil_dev *hildevice)
{
	char db;

	/*
	 * Turn on auto polling
	 */
	HILWAIT(hildevice);
	WRITEHILCMD(hildevice, HIL_READLPCTRL);
	HILDATAWAIT(hildevice);
	db = READHILDATA(hildevice);
	db |= LPC_AUTOPOLL;
	HILWAIT(hildevice);
	WRITEHILCMD(hildevice, HIL_WRITELPCTRL);
	HILWAIT(hildevice);
	WRITEHILDATA(hildevice, db);
	/*
	 * Turn on auto repeat
	 */
	HILWAIT(hildevice);
	WRITEHILCMD(hildevice, HIL_SETARR);
	HILWAIT(hildevice);
	WRITEHILDATA(hildevice, ar_format(KBD_ARR));
}

#ifdef DEBUG
static void
printhilpollbuf(struct hil_softc *hilp)
{
	u_char *cp;
	int i, len;

	cp = hilp->hl_pollbuf;
	len = hilp->hl_pollbp - cp;
	for (i = 0; i < len; i++)
		printf("%x ", hilp->hl_pollbuf[i]);
	printf("\n");
}

static void
printhilcmdbuf(struct hil_softc *hilp)
{
	u_char *cp;
	int i, len;

	cp = hilp->hl_cmdbuf;
	len = hilp->hl_cmdbp - cp;
	for (i = 0; i < len; i++)
		printf("%x ", hilp->hl_cmdbuf[i]);
	printf("\n");
}

static void
hilreport(struct hil_softc *hilp)
{
	int i, len;
	int s = splhil();

	for (i = 1; i <= hilp->hl_maxdev; i++) {
		hilp->hl_cmdbp = hilp->hl_cmdbuf;
		hilp->hl_cmddev = i;
		send_hildev_cmd(hilp, i, HILIDENTIFY);
		printf("hil%d: id: ", i);
		printhilcmdbuf(hilp);
		len = hilp->hl_cmdbp - hilp->hl_cmdbuf;
		if (len > 1 && (hilp->hl_cmdbuf[1] & HILSCBIT)) {
			hilp->hl_cmdbp = hilp->hl_cmdbuf;
			hilp->hl_cmddev = i;
			send_hildev_cmd(hilp, i, HILSECURITY);
			printf("hil%d: sc: ", i);
			printhilcmdbuf(hilp);
		}
	}
	hilp->hl_cmdbp = hilp->hl_cmdbuf;
	hilp->hl_cmddev = 0;
	splx(s);
}
#endif
