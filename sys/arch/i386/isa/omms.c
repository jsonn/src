/*	$NetBSD: omms.c,v 1.4.2.2 2002/01/07 21:47:03 thorpej Exp $	*/

/*-
 * Copyright (c) 1993, 1994 Charles M. Hannum.
 * Copyright (c) 1992, 1993 Erik Forsberg.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * THIS SOFTWARE IS PROVIDED BY ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL I BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: omms.c,v 1.4.2.2 2002/01/07 21:47:03 thorpej Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/device.h>
#include <sys/poll.h>

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/mouse.h>
#include <machine/conf.h>

#include <dev/isa/isavar.h>

#define	MMS_ADDR	0	/* offset for register select */
#define	MMS_DATA	1	/* offset for InPort data */
#define	MMS_IDENT	2	/* offset for identification register */
#define	MMS_NPORTS	4

#define	MMS_CHUNK	128	/* chunk size for read */
#define	MMS_BSIZE	1020	/* buffer size */

struct omms_softc {		/* driver status information */
	struct device sc_dev;
	void *sc_ih;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;

	struct clist sc_q;
	struct selinfo sc_rsel;
	u_char sc_state;	/* mouse driver state */
#define	MMS_OPEN	0x01	/* device is open */
#define	MMS_ASLP	0x02	/* waiting for mouse data */
	u_char sc_status;	/* mouse button status */
	int sc_x, sc_y;		/* accumulated motion in the X,Y axis */
};

int ommsprobe __P((struct device *, struct cfdata *, void *));
void ommsattach __P((struct device *, struct device *, void *));
int ommsintr __P((void *));

struct cfattach omms_ca = {
	sizeof(struct omms_softc), ommsprobe, ommsattach
};

extern struct cfdriver omms_cd;

#define	MMSUNIT(dev)	(minor(dev))

int
ommsprobe(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	int rv;

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	/* Disallow wildcarded i/o address. */
	if (ia->ia_io[0].ir_addr == ISACF_PORT_DEFAULT)
		return 0;
	if (ia->ia_irq[0].ir_irq == ISACF_IRQ_DEFAULT)
		return 0;

	/* Map the i/o space. */
	if (bus_space_map(iot, ia->ia_io[0].ir_addr, MMS_NPORTS, 0, &ioh))
		return 0;

	rv = 0;

	/* Read identification register to see if present */
	if (bus_space_read_1(iot, ioh, MMS_IDENT) != 0xde)
		goto out;

	/* Seems it was there; reset. */
	bus_space_write_1(iot, ioh, MMS_ADDR, 0x87);

	rv = 1;
	ia->ia_nio = 1;
	ia->ia_io[0].ir_size = MMS_NPORTS;

	ia->ia_nirq = 1;

	ia->ia_niomem = 0;
	ia->ia_ndrq = 0;

out:
	bus_space_unmap(iot, ioh, MMS_NPORTS);
	return rv;
}

void
ommsattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct omms_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;

	printf("\n");

	if (bus_space_map(iot, ia->ia_io[0].ir_addr, MMS_NPORTS, 0, &ioh)) {
		printf("%s: can't map i/o space\n", sc->sc_dev.dv_xname);
		return;
	}

	/* Other initialization was done by ommsprobe. */
	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->sc_state = 0;

	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq[0].ir_irq,
	    IST_PULSE, IPL_TTY, ommsintr, sc);
}

int
mmsopen(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	int unit = MMSUNIT(dev);
	struct omms_softc *sc;

	if (unit >= omms_cd.cd_ndevs)
		return ENXIO;
	sc = omms_cd.cd_devs[unit];
	if (!sc)
		return ENXIO;

	if (sc->sc_state & MMS_OPEN)
		return EBUSY;

	if (clalloc(&sc->sc_q, MMS_BSIZE, 0) == -1)
		return ENOMEM;

	sc->sc_state |= MMS_OPEN;
	sc->sc_status = 0;
	sc->sc_x = sc->sc_y = 0;

	/* Enable interrupts. */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, MMS_ADDR, 0x07);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, MMS_DATA, 0x09);

	return 0;
}

int
mmsclose(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	struct omms_softc *sc = omms_cd.cd_devs[MMSUNIT(dev)];

	/* Disable interrupts. */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, MMS_ADDR, 0x87);

	sc->sc_state &= ~MMS_OPEN;

	clfree(&sc->sc_q);

	return 0;
}

int
mmsread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct omms_softc *sc = omms_cd.cd_devs[MMSUNIT(dev)];
	int s;
	int error = 0;
	size_t length;
	u_char buffer[MMS_CHUNK];

	/* Block until mouse activity occurred. */

	s = spltty();
	while (sc->sc_q.c_cc == 0) {
		if (flag & IO_NDELAY) {
			splx(s);
			return EWOULDBLOCK;
		}
		sc->sc_state |= MMS_ASLP;
		error = tsleep((caddr_t)sc, PZERO | PCATCH, "mmsrea", 0);
		if (error) {
			sc->sc_state &= ~MMS_ASLP;
			splx(s);
			return error;
		}
	}
	splx(s);

	/* Transfer as many chunks as possible. */

	while (sc->sc_q.c_cc > 0 && uio->uio_resid > 0) {
		length = min(sc->sc_q.c_cc, uio->uio_resid);
		if (length > sizeof(buffer))
			length = sizeof(buffer);

		/* Remove a small chunk from the input queue. */
		(void) q_to_b(&sc->sc_q, buffer, length);

		/* Copy the data to the user process. */
		if ((error = uiomove(buffer, length, uio)) != 0)
			break;
	}

	return error;
}

int
mmsioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	struct omms_softc *sc = omms_cd.cd_devs[MMSUNIT(dev)];
	struct mouseinfo info;
	int s;
	int error;

	switch (cmd) {
	case MOUSEIOCREAD:
		s = spltty();

		info.status = sc->sc_status;
		if (sc->sc_x || sc->sc_y)
			info.status |= MOVEMENT;

		if (sc->sc_x > 127)
			info.xmotion = 127;
		else if (sc->sc_x < -127)
			/* Bounding at -127 avoids a bug in XFree86. */
			info.xmotion = -127;
		else
			info.xmotion = sc->sc_x;

		if (sc->sc_y > 127)
			info.ymotion = 127;
		else if (sc->sc_y < -127)
			info.ymotion = -127;
		else
			info.ymotion = sc->sc_y;

		/* Reset historical information. */
		sc->sc_x = sc->sc_y = 0;
		sc->sc_status &= ~BUTCHNGMASK;
		ndflush(&sc->sc_q, sc->sc_q.c_cc);

		splx(s);
		error = copyout(&info, addr, sizeof(struct mouseinfo));
		break;

	default:
		error = EINVAL;
		break;
	}

	return error;
}

int
ommsintr(arg)
	void *arg;
{
	struct omms_softc *sc = arg;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_char buttons, changed, status;
	char dx, dy;
	u_char buffer[5];

	if ((sc->sc_state & MMS_OPEN) == 0)
		/* Interrupts are not expected. */
		return 0;

	/* Freeze InPort registers (disabling interrupts). */
	bus_space_write_1(iot, ioh, MMS_ADDR, 0x07);
	bus_space_write_1(iot, ioh, MMS_DATA, 0x29);

	bus_space_write_1(iot, ioh, MMS_ADDR, 0x00);
	status = bus_space_read_1(iot, ioh, MMS_DATA);

	if (status & 0x40) {
		bus_space_write_1(iot, ioh, MMS_ADDR, 1);
		dx = bus_space_read_1(iot, ioh, MMS_DATA);
		dx = (dx == -128) ? -127 : dx;
		bus_space_write_1(iot, ioh, MMS_ADDR, 2);
		dy = bus_space_read_1(iot, ioh, MMS_DATA);
		dy = (dy == -128) ? 127 : -dy;
	} else
		dx = dy = 0;

	/* Unfreeze InPort registers (reenabling interrupts). */
	bus_space_write_1(iot, ioh, MMS_ADDR, 0x07);
	bus_space_write_1(iot, ioh, MMS_DATA, 0x09);

	buttons = status & BUTSTATMASK;
	changed = status & BUTCHNGMASK;
	sc->sc_status = buttons | (sc->sc_status & ~BUTSTATMASK) | changed;

	if (dx || dy || changed) {
		/* Update accumulated movements. */
		sc->sc_x += dx;
		sc->sc_y += dy;

		/* Add this event to the queue. */
		buffer[0] = 0x80 | (buttons ^ BUTSTATMASK);
		buffer[1] = dx;
		buffer[2] = dy;
		buffer[3] = buffer[4] = 0;
		(void) b_to_q(buffer, sizeof buffer, &sc->sc_q);

		if (sc->sc_state & MMS_ASLP) {
			sc->sc_state &= ~MMS_ASLP;
			wakeup((caddr_t)sc);
		}
		selwakeup(&sc->sc_rsel);
	}

	return -1;
}

int
mmspoll(dev, events, p)
	dev_t dev;
	int events;
	struct proc *p;
{
	struct omms_softc *sc = omms_cd.cd_devs[MMSUNIT(dev)];
	int revents = 0;
	int s = spltty();

	if (events & (POLLIN | POLLRDNORM)) {
		if (sc->sc_q.c_cc > 0)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(p, &sc->sc_rsel);
	}

	splx(s);
	return (revents);
}
