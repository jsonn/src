/*	$NetBSD: fb.c,v 1.1.1.1.2.2 1997/01/14 20:57:03 gwr Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)fb.c	8.1 (Berkeley) 6/11/93
 */

/*
 * /dev/fb (indirect frame buffer driver).
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/proc.h>

#include <machine/fbio.h>

#include "fbvar.h"

cdev_decl(fb);

static struct fbdevice *devfb;
static int fbpriority;

/*
 * This is called by the real driver (i.e. bw2, cg3, ...)
 * to declare itself as a potential default frame buffer.
 */
void
fb_attach(fb, newpri)
	struct fbdevice *fb;
	int newpri;
{
	if (fbpriority < newpri) {
		fbpriority = newpri;
		devfb = fb;
	}
}

int
fbopen(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{

	if (devfb == NULL)
		return (ENXIO);
	return ((*devfb->fb_driver->fbd_open)(dev, flags, mode, p));
}

int
fbclose(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{

	return ((*devfb->fb_driver->fbd_close)(dev, flags, mode, p));
}

int
fbioctl(dev, cmd, data, flags, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flags;
	struct proc *p;
{
	return (fbioctlfb(devfb, cmd, data));
}

int
fbmmap(dev, off, prot)
	dev_t dev;
	int off, prot;
{
	return ((*devfb->fb_driver->fbd_mmap)(dev, off, prot));
}

/*
 * Common fb ioctl function
 */
int
fbioctlfb(fb, cmd, data)
	struct fbdevice *fb;
	u_long cmd;
	caddr_t data;
{
	struct fbdriver *fbd = fb->fb_driver;
	void *vp = (void *)data;
	int error;

	switch (cmd) {

	case FBIOGTYPE:
		*(struct fbtype *)vp = fb->fb_fbtype;
		error = 0;
		break;

	case FBIOGATTR:
		error = (*fbd->fbd_gattr)(fb, vp);
		break;

	case FBIOGVIDEO:
		error = (*fbd->fbd_gvideo)(fb, vp);
		break;

	case FBIOSVIDEO:
		error = (*fbd->fbd_svideo)(fb, vp);
		break;

	case FBIOGETCMAP:
		error = (*fbd->fbd_getcmap)(fb, vp);
		break;

	case FBIOPUTCMAP:
		error = (*fbd->fbd_putcmap)(fb, vp);
		break;

	default:
		error = ENOTTY;
	}
	return (error);
}

void
fb_unblank()
{
	int on = 1;

	if (devfb == NULL)
		return;

	(*devfb->fb_driver->fbd_svideo)(devfb, (void *)&on);
}

/*
 * Default ioctl function to put in struct fbdriver
 * for functions that are not supported.
 */
int
fb_noioctl(fbd, vp)
	struct fbdevice *fbd;
	void *vp;
{
	return ENOTTY;
}

