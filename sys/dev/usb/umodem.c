/*	$NetBSD: umodem.c,v 1.15 1999/09/11 08:19:27 augustss Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@carlstedt.se) at
 * Carlstedt Research & Technology.
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

/*
 * Comm Class spec: http://www.usb.org/developers/data/usbcdc11.pdf
 */

/*
 * TODO:
 * - Add error recovery in various places; the big problem is what
 *   to do in a callback if there is an error.
 * - Implement a Call Device for modems without multiplexed commands.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/device.h>
#include <sys/poll.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbcdc.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_quirks.h>

#include <dev/usb/usbdevs.h>

#ifdef USB_DEBUG
#define DPRINTFN(n, x)	if (umodemdebug > (n)) logprintf x
int	umodemdebug = 0;
#else
#define DPRINTFN(n, x)
#endif
#define DPRINTF(x) DPRINTFN(0, x)

/* Macros to clear/set/test flags. */
#define	SET(t, f)	(t) |= (f)
#define	CLR(t, f)	(t) &= ~((unsigned)(f))
#define	ISSET(t, f)	((t) & (f))

#define	UMODEMUNIT_MASK		0x3ffff
#define	UMODEMDIALOUT_MASK	0x80000
#define	UMODEMCALLUNIT_MASK	0x40000

#define	UMODEMUNIT(x)		(minor(x) & UMODEMUNIT_MASK)
#define	UMODEMDIALOUT(x)	(minor(x) & UMODEMDIALOUT_MASK)
#define	UMODEMCALLUNIT(x)	(minor(x) & UMODEMCALLUNIT_MASK)

#define UMODEMIBUFSIZE 64

struct umodem_softc {
	USBBASEDEVICE		sc_dev;		/* base device */

	usbd_device_handle	sc_udev;	/* USB device */

	int			sc_ctl_iface_no;
	usbd_interface_handle	sc_ctl_iface;	/* control interface */
	int			sc_data_iface_no;
	usbd_interface_handle	sc_data_iface;	/* data interface */

	int			sc_bulkin_no;	/* bulk in endpoint address */
	usbd_pipe_handle	sc_bulkin_pipe;	/* bulk in pipe */
	usbd_request_handle	sc_ireqh;	/* read request */
	u_char			*sc_ibuf;	/* read buffer */

	int			sc_bulkout_no;	/* bulk out endpoint address */
	usbd_pipe_handle	sc_bulkout_pipe;/* bulk out pipe */
	usbd_request_handle	sc_oreqh;	/* read request */

	int			sc_cm_cap;	/* CM capabilities */
	int			sc_acm_cap;	/* ACM capabilities */

	int			sc_cm_over_data;

	struct tty		*sc_tty;	/* our tty */

	usb_cdc_line_state_t	sc_line_state;	/* current line state */
	u_char			sc_dtr;		/* current DTR state */

	u_char			sc_opening;	/* lock during open */
	u_char			sc_dying;	/* disconnecting */
};

cdev_decl(umodem);

void *umodem_get_desc
	__P((usbd_device_handle dev, int type, int subtype));
usbd_status umodem_set_comm_feature
	__P((struct umodem_softc *sc, int feature, int state));
usbd_status umodem_set_line_coding
	__P((struct umodem_softc *sc, usb_cdc_line_state_t *state));

void	umodem_get_caps	__P((usbd_device_handle, int *, int *));
void	umodem_cleanup	__P((struct umodem_softc *));
int	umodemparam	__P((struct tty *, struct termios *));
void	umodemstart	__P((struct tty *));
void	umodem_shutdown	__P((struct umodem_softc *));
void	umodem_modem	__P((struct umodem_softc *, int));
void	umodem_break	__P((struct umodem_softc *, int));
usbd_status umodemstartread __P((struct umodem_softc *));
void	umodemreadcb	__P((usbd_request_handle, usbd_private_handle, 
			     usbd_status status));
void	umodemwritecb	__P((usbd_request_handle, usbd_private_handle, 
			     usbd_status status));

USB_DECLARE_DRIVER(umodem);

USB_MATCH(umodem)
{
	USB_MATCH_START(umodem, uaa);
	usb_interface_descriptor_t *id;
	int cm, acm;
	
	if (!uaa->iface)
		return (UMATCH_NONE);

	id = usbd_get_interface_descriptor(uaa->iface);
	if (id == 0 ||
	    id->bInterfaceClass != UCLASS_CDC ||
	    id->bInterfaceSubClass != USUBCLASS_ABSTRACT_CONTROL_MODEL ||
	    id->bInterfaceProtocol != UPROTO_CDC_AT)
		return (UMATCH_NONE);
	
	umodem_get_caps(uaa->device, &cm, &acm);
	if (!(cm & USB_CDC_CM_DOES_CM) ||
	    !(cm & USB_CDC_CM_OVER_DATA) ||
	    !(acm & USB_CDC_ACM_HAS_LINE))
		return (UMATCH_NONE);

	return (UMATCH_IFACECLASS_IFACESUBCLASS_IFACEPROTO);
}

USB_ATTACH(umodem)
{
	USB_ATTACH_START(umodem, sc, uaa);
	usbd_device_handle dev = uaa->device;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	usb_cdc_cm_descriptor_t *cmd;
	char devinfo[1024];
	usbd_status r;
	int data_ifaceno;
	int i;
	struct tty *tp;

	usbd_devinfo(uaa->device, 0, devinfo);
	USB_ATTACH_SETUP;

	sc->sc_udev = dev;

	sc->sc_ctl_iface = uaa->iface;
	id = usbd_get_interface_descriptor(sc->sc_ctl_iface);
	printf("%s: %s, iclass %d/%d\n", USBDEVNAME(sc->sc_dev),
	       devinfo, id->bInterfaceClass, id->bInterfaceSubClass);
	sc->sc_ctl_iface_no = id->bInterfaceNumber;

	umodem_get_caps(dev, &sc->sc_cm_cap, &sc->sc_acm_cap);

	/* Get the data interface no. */
	cmd = umodem_get_desc(dev, UDESC_CS_INTERFACE, UDESCSUB_CDC_CM);
	if (!cmd) {
		DPRINTF(("%s: no CM desc\n", USBDEVNAME(sc->sc_dev)));
		goto bad;
	}
	sc->sc_data_iface_no = data_ifaceno = cmd->bDataInterface;

	printf("%s: data interface %d, has %sCM over data, has %sbreak\n",
	       USBDEVNAME(sc->sc_dev), data_ifaceno,
	       sc->sc_cm_cap & USB_CDC_CM_OVER_DATA ? "" : "no ",
	       sc->sc_acm_cap & USB_CDC_ACM_HAS_BREAK ? "" : "no ");


	/* Get the data interface too. */
	for (i = 0; i < uaa->nifaces; i++) {
		if (uaa->ifaces[i]) {
			id = usbd_get_interface_descriptor(uaa->ifaces[i]);
			if (id->bInterfaceNumber == data_ifaceno) {
				sc->sc_data_iface = uaa->ifaces[i];
				uaa->ifaces[i] = 0;
			}
		}
	}
	if (!sc->sc_data_iface) {
		printf("%s: no data interface\n", USBDEVNAME(sc->sc_dev));
		goto bad;
	}

	/* 
	 * Find the bulk endpoints. 
	 * Iterate over all endpoints in the data interface and take note.
	 */
	sc->sc_bulkin_no = sc->sc_bulkout_no = -1;

	id = usbd_get_interface_descriptor(sc->sc_data_iface);
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_data_iface, i);
		if (!ed) {
			printf("%s: no endpoint descriptor for %d\n",
				USBDEVNAME(sc->sc_dev), i);
			goto bad;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    (ed->bmAttributes & UE_XFERTYPE) == UE_BULK) {
                        sc->sc_bulkin_no = ed->bEndpointAddress;
                } else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			   (ed->bmAttributes & UE_XFERTYPE) == UE_BULK) {
                        sc->sc_bulkout_no = ed->bEndpointAddress;
                }
        }

	if (sc->sc_bulkin_no == -1) {
		DPRINTF(("%s: Could not find data bulk in\n",
			USBDEVNAME(sc->sc_dev)));
		goto bad;
	}
	if (sc->sc_bulkout_no == -1) {
		DPRINTF(("%s: Could not find data bulk out\n",
			USBDEVNAME(sc->sc_dev)));
		goto bad;
	}

	if (sc->sc_cm_cap & USB_CDC_CM_OVER_DATA) {
		r = umodem_set_comm_feature(sc, UCDC_ABSTRACT_STATE,
					    UCDC_DATA_MULTIPLEXED);
		if (r != USBD_NORMAL_COMPLETION)
			goto bad;
		sc->sc_cm_over_data = 1;
	}

	tp = ttymalloc();
	tp->t_oproc = umodemstart;
	tp->t_param = umodemparam;
	sc->sc_tty = tp;
	DPRINTF(("umodem_attach: tty_attach %p\n", tp));
	tty_attach(tp);

	sc->sc_dtr = -1;

	USB_ATTACH_SUCCESS_RETURN;

 bad:
	sc->sc_dying = 1;
	USB_ATTACH_ERROR_RETURN;
}

void
umodem_get_caps(dev, cm, acm)
	usbd_device_handle dev;
	int *cm, *acm;
{
	usb_cdc_cm_descriptor_t *cmd;
	usb_cdc_acm_descriptor_t *cad;

	*cm = *acm = 0;

	cmd = umodem_get_desc(dev, UDESC_CS_INTERFACE, UDESCSUB_CDC_CM);
	if (!cmd) {
		DPRINTF(("umodem_get_desc: no CM desc\n"));
		return;
	}
	*cm = cmd->bmCapabilities;

	cad = umodem_get_desc(dev, UDESC_CS_INTERFACE, UDESCSUB_CDC_ACM);
	if (!cad) {
		DPRINTF(("umodem_get_desc: no ACM desc\n"));
		return;
	}
	*acm = cad->bmCapabilities;
} 

void
umodemstart(tp)
	struct tty *tp;
{
	struct umodem_softc *sc = umodem_cd.cd_devs[UMODEMUNIT(tp->t_dev)];
	int s;
	u_char *data;
	int cnt;

	if (sc->sc_dying)
		return;

	s = spltty();
	if (ISSET(tp->t_state, TS_BUSY | TS_TIMEOUT | TS_TTSTOP)) {
		DPRINTFN(4,("umodemstart: stopped\n"));
		goto out;
	}

	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (ISSET(tp->t_state, TS_ASLEEP)) {
			CLR(tp->t_state, TS_ASLEEP);
			wakeup(&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
		if (tp->t_outq.c_cc == 0)
			goto out;
	}

	/* Grab the first contiguous region of buffer space. */
	data = tp->t_outq.c_cf;
	cnt = ndqb(&tp->t_outq, 0);

	if (cnt == 0) {
		DPRINTF(("umodemstart: cnt==0\n"));
		splx(s);
		return;
	}

	SET(tp->t_state, TS_BUSY);

	DPRINTFN(4,("umodemstart: %d chars\n", cnt));
	/* XXX what can we do on error? */
	usbd_setup_request(sc->sc_oreqh, sc->sc_bulkout_pipe, 
			   (usbd_private_handle)sc, data, cnt,
			   0, USBD_NO_TIMEOUT, umodemwritecb);
	(void)usbd_transfer(sc->sc_oreqh);

out:
	splx(s);
}

void
umodemwritecb(reqh, p, status)
	usbd_request_handle reqh;
	usbd_private_handle p;
	usbd_status status;
{
	struct umodem_softc *sc = (struct umodem_softc *)p;
	struct tty *tp = sc->sc_tty;
	u_int32_t cc;
	int s;

	DPRINTFN(5,("umodemwritecb: status=%d\n", status));

	if (status == USBD_CANCELLED)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF(("umodemwritecb: status=%d\n", status));
		usbd_clear_endpoint_stall_async(sc->sc_bulkin_pipe);
		/* XXX we should restart after some delay. */
		return;
	}

	usbd_get_request_status(reqh, 0, 0, &cc, 0);
	DPRINTFN(5,("umodemwritecb: cc=%d\n", cc));

	s = spltty();
	CLR(tp->t_state, TS_BUSY);
	if (ISSET(tp->t_state, TS_FLUSH))
		CLR(tp->t_state, TS_FLUSH);
	else
		ndflush(&tp->t_outq, cc);
	(*linesw[tp->t_line].l_start)(tp);
	splx(s);
}

int
umodemparam(tp, t)
	struct tty *tp;
	struct termios *t;
{
	struct umodem_softc *sc = umodem_cd.cd_devs[UMODEMUNIT(tp->t_dev)];
	usb_cdc_line_state_t ls;

	if (sc->sc_dying)
		return (EIO);

	/* Check requested parameters. */
	if (t->c_ospeed < 0)
		return (EINVAL);
	if (t->c_ispeed && t->c_ispeed != t->c_ospeed)
		return (EINVAL);

	/*
	 * If there were no changes, don't do anything.  This avoids dropping
	 * input and improves performance when all we did was frob things like
	 * VMIN and VTIME.
	 */
	if (tp->t_ospeed == t->c_ospeed &&
	    tp->t_cflag == t->c_cflag)
		return (0);

	/* And copy to tty. */
	tp->t_ispeed = 0;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;

	USETDW(ls.dwDTERate, t->c_ospeed);
	if (ISSET(t->c_cflag, CSTOPB))
		ls.bCharFormat = UCDC_STOP_BIT_2;
	else
		ls.bCharFormat = UCDC_STOP_BIT_1;
	if (ISSET(t->c_cflag, PARENB)) {
		if (ISSET(t->c_cflag, PARODD))
			ls.bParityType = UCDC_PARITY_ODD;
		else
			ls.bParityType = UCDC_PARITY_EVEN;
	} else
		ls.bParityType = UCDC_PARITY_NONE;
	switch (ISSET(t->c_cflag, CSIZE)) {
	case CS5:
		ls.bDataBits = 5;
		break;
	case CS6:
		ls.bDataBits = 6;
		break;
	case CS7:
		ls.bDataBits = 7;
		break;
	case CS8:
		ls.bDataBits = 8;
		break;
	}
	/* XXX what can we if it fails? */
	(void)umodem_set_line_coding(sc, &ls);

	/*
	 * Update the tty layer's idea of the carrier bit, in case we changed
	 * CLOCAL or MDMBUF.  We don't hang up here; we only do that by
	 * explicit request.
	 */
	(void) (*linesw[tp->t_line].l_modem)(tp, 1 /* XXX carrier */ );

	return (0);
}

int
umodemopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = UMODEMUNIT(dev);
	usbd_status r;
	struct umodem_softc *sc;
	struct tty *tp;
	int s;
	int error;
 
	if (unit >= umodem_cd.cd_ndevs)
		return (ENXIO);
	sc = umodem_cd.cd_devs[unit];
	if (sc == 0)
		return (ENXIO);

	if (sc->sc_dying)
		return (EIO);

	if (ISSET(sc->sc_dev.dv_flags, DVF_ACTIVE) == 0)
		return (ENXIO);

	tp = sc->sc_tty;

	DPRINTF(("umodemopen: unit=%d, tp=%p\n", unit, tp));

	if (ISSET(tp->t_state, TS_ISOPEN) &&
	    ISSET(tp->t_state, TS_XCLUDE) &&
	    p->p_ucred->cr_uid != 0)
		return (EBUSY);

	/*
	 * Do the following iff this is a first open.
	 */
	s = spltty();
	while (sc->sc_opening)
		tsleep(&sc->sc_opening, PRIBIO, "umdmop", 0);
	sc->sc_opening = 1;
	
	if (!ISSET(tp->t_state, TS_ISOPEN) && tp->t_wopen == 0) {
		struct termios t;

		tp->t_dev = dev;

		/*
		 * Initialize the termios status to the defaults.  Add in the
		 * sticky bits from TIOCSFLAGS.
		 */
		t.c_ispeed = 0;
		t.c_ospeed = TTYDEF_SPEED;
		t.c_cflag = TTYDEF_CFLAG;
		/* Make sure umodemparam() will do something. */
		tp->t_ospeed = 0;
		(void) umodemparam(tp, &t);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		ttychars(tp);
		ttsetwater(tp);

		/*
		 * Turn on DTR.  We must always do this, even if carrier is not
		 * present, because otherwise we'd have to use TIOCSDTR
		 * immediately after setting CLOCAL, which applications do not
		 * expect.  We always assert DTR while the device is open
		 * unless explicitly requested to deassert it.
		 */
		umodem_modem(sc, 1);

		DPRINTF(("umodemopen: open pipes\n"));

		/* Open the bulk pipes */
		r = usbd_open_pipe(sc->sc_data_iface, sc->sc_bulkin_no, 0,
				   &sc->sc_bulkin_pipe);
		if (r != USBD_NORMAL_COMPLETION) {
			DPRINTF(("%s: cannot open bulk out pipe (addr %d)\n",
				 USBDEVNAME(sc->sc_dev), sc->sc_bulkin_no));
			return (EIO);
		}
		r = usbd_open_pipe(sc->sc_data_iface, sc->sc_bulkout_no,
				   USBD_EXCLUSIVE_USE, &sc->sc_bulkout_pipe);
		if (r != USBD_NORMAL_COMPLETION) {
			DPRINTF(("%s: cannot open bulk in pipe (addr %d)\n",
				 USBDEVNAME(sc->sc_dev), sc->sc_bulkout_no));
			usbd_close_pipe(sc->sc_bulkin_pipe);
			return (EIO);
		}
		
		/* Allocate a request and an input buffer and start reading. */
		sc->sc_ireqh = usbd_alloc_request(sc->sc_udev);
		if (sc->sc_ireqh == 0) {
			usbd_close_pipe(sc->sc_bulkin_pipe);
			usbd_close_pipe(sc->sc_bulkout_pipe);
			return (ENOMEM);
		}
		sc->sc_oreqh = usbd_alloc_request(sc->sc_udev);
		if (sc->sc_oreqh == 0) {
			usbd_close_pipe(sc->sc_bulkin_pipe);
			usbd_close_pipe(sc->sc_bulkout_pipe);
			usbd_free_request(sc->sc_ireqh);
			return (ENOMEM);
		}
		sc->sc_ibuf = malloc(UMODEMIBUFSIZE, M_USBDEV, M_WAITOK);
		umodemstartread(sc);
	}
	sc->sc_opening = 0;
	wakeup(&sc->sc_opening);
	splx(s);

	error = ttyopen(tp, UMODEMDIALOUT(dev), ISSET(flag, O_NONBLOCK));
	if (error)
		goto bad;

	error = (*linesw[tp->t_line].l_open)(dev, tp);
	if (error)
		goto bad;

	return (0);

bad:
	if (!ISSET(tp->t_state, TS_ISOPEN) && tp->t_wopen == 0) {
		/*
		 * We failed to open the device, and nobody else had it opened.
		 * Clean up the state as appropriate.
		 */
		umodem_cleanup(sc);
	}

	return (error);
}

usbd_status
umodemstartread(sc)
	struct umodem_softc *sc;
{
	usbd_status r;

	DPRINTFN(5,("umodemstartread: start\n"));
	usbd_setup_request(sc->sc_ireqh, sc->sc_bulkin_pipe, 
			   (usbd_private_handle)sc, 
			   sc->sc_ibuf,  UMODEMIBUFSIZE, USBD_SHORT_XFER_OK, 
			   USBD_NO_TIMEOUT, umodemreadcb);
	r = usbd_transfer(sc->sc_ireqh);
	if (r != USBD_IN_PROGRESS)
		return (r);
	return (USBD_NORMAL_COMPLETION);
}
 
void
umodemreadcb(reqh, p, status)
	usbd_request_handle reqh;
	usbd_private_handle p;
	usbd_status status;
{
	struct umodem_softc *sc = (struct umodem_softc *)p;
	struct tty *tp = sc->sc_tty;
	int (*rint) __P((int c, struct tty *tp)) = linesw[tp->t_line].l_rint;
	usbd_status r;
	u_int32_t cc;
	u_char *cp;
	int s;

	if (status == USBD_CANCELLED)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF(("umodemreadcb: status=%d\n", status));
		usbd_clear_endpoint_stall_async(sc->sc_bulkin_pipe);
		/* XXX we should restart after some delay. */
		return;
	}

	usbd_get_request_status(reqh, 0, (void **)&cp, &cc, 0);
	DPRINTFN(5,("umodemreadcb: got %d chars, tp=%p\n", cc, tp));
	s = spltty();
	/* Give characters to tty layer. */
	while (cc-- > 0) {
		DPRINTFN(7,("umodemreadcb: char=0x%02x\n", *cp));
		if ((*rint)(*cp++, tp) == -1) {
			/* XXX what should we do? */
			break;
		}
	}
	splx(s);

	r = umodemstartread(sc);
	if (r != USBD_NORMAL_COMPLETION) {
		printf("%s: read start failed\n", USBDEVNAME(sc->sc_dev));
		/* XXX what should we dow now? */
	}
}

int
umodemclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct umodem_softc *sc = umodem_cd.cd_devs[UMODEMUNIT(dev)];
	struct tty *tp = sc->sc_tty;

	DPRINTF(("umodemclose: unit=%d\n", UMODEMUNIT(dev)));
	if (!ISSET(tp->t_state, TS_ISOPEN))
		return (0);

	(*linesw[tp->t_line].l_close)(tp, flag);
	ttyclose(tp);

	if (sc->sc_dying)
		return (0);

	if (!ISSET(tp->t_state, TS_ISOPEN) && tp->t_wopen == 0) {
		/*
		 * Although we got a last close, the device may still be in
		 * use; e.g. if this was the dialout node, and there are still
		 * processes waiting for carrier on the non-dialout node.
		 */
		umodem_cleanup(sc);
	}

	return (0);
}
 
void
umodem_cleanup(sc)
	struct umodem_softc *sc;
{
	umodem_shutdown(sc);
	DPRINTF(("umodem_cleanup: closing pipes\n"));
	usbd_abort_pipe(sc->sc_bulkin_pipe);
	usbd_close_pipe(sc->sc_bulkin_pipe);
	usbd_abort_pipe(sc->sc_bulkout_pipe);
	usbd_close_pipe(sc->sc_bulkout_pipe);
	usbd_free_request(sc->sc_ireqh);
	usbd_free_request(sc->sc_oreqh);
	free(sc->sc_ibuf, M_USBDEV);
}

int
umodemread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct umodem_softc *sc = umodem_cd.cd_devs[UMODEMUNIT(dev)];
	struct tty *tp = sc->sc_tty;

	if (sc->sc_dying)
		return (EIO);
 
	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}
 
int
umodemwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct umodem_softc *sc = umodem_cd.cd_devs[UMODEMUNIT(dev)];
	struct tty *tp = sc->sc_tty;

	if (sc->sc_dying)
		return (EIO);
 
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

void
umodemstop(tp, flag)
	struct tty *tp;
	int flag;
{
	/*struct umodem_softc *sc = umodem_cd.cd_devs[UMODEMUNIT(tp->t_dev)];*/
	int s;

	DPRINTF(("umodemstop: %d\n", flag));
	s = spltty();
	if (ISSET(tp->t_state, TS_BUSY)) {
		DPRINTF(("umodemstop: XXX\n"));
		/* XXX do what? */
		if (!ISSET(tp->t_state, TS_TTSTOP))
			SET(tp->t_state, TS_FLUSH);
	}
	splx(s);
}

struct tty *
umodemtty(dev)
	dev_t dev;
{
	struct umodem_softc *sc = umodem_cd.cd_devs[UMODEMUNIT(dev)];
	struct tty *tp = sc->sc_tty;

	return (tp);
}

int
umodemioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct umodem_softc *sc = umodem_cd.cd_devs[UMODEMUNIT(dev)];
	struct tty *tp = sc->sc_tty;
	int error;
	int s;

	if (sc->sc_dying)
		return (EIO);
 
	DPRINTF(("umodemioctl: cmd=0x%08lx\n", cmd));

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);

	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);

	error = 0;

	DPRINTF(("umodemioctl: our cmd=0x%08lx\n", cmd));
	s = spltty();

	switch (cmd) {
	case TIOCSBRK:
		umodem_break(sc, 1);
		break;

	case TIOCCBRK:
		umodem_break(sc, 0);
		break;

	case TIOCSDTR:
		umodem_modem(sc, 1);
		break;

	case TIOCCDTR:
		umodem_modem(sc, 0);
		break;

	case USB_GET_CM_OVER_DATA:
		*(int *)data = sc->sc_cm_over_data;
		break;

	case USB_SET_CM_OVER_DATA:
		if (*(int *)data != sc->sc_cm_over_data) {
			/* XXX change it */
		}
		break;

	default:
		DPRINTF(("umodemioctl: unknown\n"));
		error = ENOTTY;
		break;
	}

	splx(s);

	return (error);
}

void
umodem_shutdown(sc)
	struct umodem_softc *sc;
{
	struct tty *tp = sc->sc_tty;

	DPRINTF(("umodem_shutdown\n"));
	/*
	 * Hang up if necessary.  Wait a bit, so the other side has time to
	 * notice even if we immediately open the port again.
	 */
	if (ISSET(tp->t_cflag, HUPCL)) {
		umodem_modem(sc, 0);
		(void) tsleep(sc, TTIPRI, ttclos, hz);
	}
}

void
umodem_modem(sc, onoff)
	struct umodem_softc *sc;
	int onoff;
{
	usb_device_request_t req;

	DPRINTF(("umodem_modem: onoff=%d\n", onoff));

	if (sc->sc_dtr == onoff)
		return;

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SET_CONTROL_LINE_STATE;
	USETW(req.wValue, onoff ? UCDC_LINE_DTR : 0);
	USETW(req.wIndex, sc->sc_ctl_iface_no);
	USETW(req.wLength, 0);

	(void)usbd_do_request(sc->sc_udev, &req, 0);

	sc->sc_dtr = onoff;
}

void
umodem_break(sc, onoff)
	struct umodem_softc *sc;
	int onoff;
{
	usb_device_request_t req;

	DPRINTF(("umodem_break: onoff=%d\n", onoff));

	if (!(sc->sc_acm_cap & USB_CDC_ACM_HAS_BREAK))
		return;

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SEND_BREAK;
	USETW(req.wValue, onoff ? UCDC_BREAK_ON : UCDC_BREAK_OFF);
	USETW(req.wIndex, sc->sc_ctl_iface_no);
	USETW(req.wLength, 0);

	(void)usbd_do_request(sc->sc_udev, &req, 0);
}

void *
umodem_get_desc(dev, type, subtype)
	usbd_device_handle dev;
	int type;
	int subtype;
{
	usb_descriptor_t *desc;
	usb_config_descriptor_t *cd = usbd_get_config_descriptor(dev);
        uByte *p = (uByte *)cd;
        uByte *end = p + UGETW(cd->wTotalLength);

	while (p < end) {
		desc = (usb_descriptor_t *)p;
		if (desc->bDescriptorType == type &&
		    desc->bDescriptorSubtype == subtype)
			return (desc);
		p += desc->bLength;
	}

	return (0);
}

usbd_status
umodem_set_comm_feature(sc, feature, state)
	struct umodem_softc *sc;
	int feature;
	int state;
{
	usb_device_request_t req;
	usbd_status r;
	usb_cdc_abstract_state_t ast;

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SET_COMM_FEATURE;
	USETW(req.wValue, feature);
	USETW(req.wIndex, sc->sc_ctl_iface_no);
	USETW(req.wLength, UCDC_ABSTRACT_STATE_LENGTH);
	USETW(ast.wState, state);

	r = usbd_do_request(sc->sc_udev, &req, &ast);
	if (r != USBD_NORMAL_COMPLETION) {
		DPRINTF(("umodem_set_comm_feature: feature=%d failed, r=%d\n",
			 feature, r));
		return (r);
	}

	return (USBD_NORMAL_COMPLETION);
}

usbd_status
umodem_set_line_coding(sc, state)
	struct umodem_softc *sc;
	usb_cdc_line_state_t *state;
{
	usb_device_request_t req;
	usbd_status r;

	DPRINTF(("umodem_set_line_coding: rate=%d fmt=%d parity=%d bits=%d\n",
		 UGETDW(state->dwDTERate), state->bCharFormat,
		 state->bParityType, state->bDataBits));

	if (memcmp(state, &sc->sc_line_state, UCDC_LINE_STATE_LENGTH) == 0) {
		DPRINTF(("umodem_set_line_coding: already set\n"));
		return (USBD_NORMAL_COMPLETION);
	}

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SET_LINE_CODING;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_ctl_iface_no);
	USETW(req.wLength, UCDC_LINE_STATE_LENGTH);

	r = usbd_do_request(sc->sc_udev, &req, state);
	if (r != USBD_NORMAL_COMPLETION) {
		DPRINTF(("umodem_set_line_coding: failed, r=%d\n", r));
		return (r);
	}

	sc->sc_line_state = *state;

	return (USBD_NORMAL_COMPLETION);
}

int
umodem_activate(self, act)
	device_ptr_t self;
	enum devact act;
{
	struct umodem_softc *sc = (struct umodem_softc *)self;

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
		break;

	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		break;
	}
	return (0);
}

int
umodem_detach(self, flags)
	device_ptr_t self;
	int flags;
{
	struct umodem_softc *sc = (struct umodem_softc *)self;
	int maj, mn;

	DPRINTF(("umodem_detach: sc=%p flags=%d tp=%p\n", 
		 sc, flags, sc->sc_tty));

	sc->sc_dying = 1;

#ifdef DIAGNOSTIC
	if (sc->sc_tty == 0) {
		DPRINTF(("umodem_detach: no tty\n"));
		return (0);
	}
#endif

	/* use refernce count? XXX */

	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == umodemopen)
			break;

	/* Nuke the vnodes for any open instances. */
	mn = self->dv_unit;
	vdevgone(maj, mn, mn, VCHR);
	vdevgone(maj, mn, mn | UMODEMDIALOUT_MASK, VCHR);
	vdevgone(maj, mn, mn | UMODEMCALLUNIT_MASK, VCHR);

	/* Detach and free the tty. */
	tty_detach(sc->sc_tty);
	ttyfree(sc->sc_tty);
	sc->sc_tty = 0;

	return (0);
}
