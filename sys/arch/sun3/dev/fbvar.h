/*	$NetBSD: fbvar.h,v 1.5.14.1 2000/11/20 20:27:51 bouyer Exp $	*/

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
 *	@(#)fbvar.h	8.1 (Berkeley) 6/11/93
 */

/*
 * Frame buffer variables.  All frame buffer drivers must provide the
 * following in order to participate.
 */

struct fbcmap;

struct fbdevice {
	struct	fbtype fb_fbtype;	/* see fbio.h */
	struct	fbdriver *fb_driver;	/* pointer to driver */
	void *fb_private;		/* for fb driver use */
	char *fb_name;			/* i.e. sc_dev.dx_name */

	caddr_t	fb_pixels;		/* display RAM */
	int	fb_linebytes;		/* bytes per display line */

	int	fb_flags;		/* copy of cf_flags */

	/* This points to the P4 register if the FB has one. */
	volatile u_int32_t *fb_pfour;

	/*
	 * XXX - The "Raster console" stuff could be stored
	 * in the driver specific structure at fb_private
	 * if needed.
	 */
};

struct fbdriver {
	/* These avoid the need to know our major number. */
	int 	(*fbd_open) __P((dev_t, int, int, struct proc *));
	int 	(*fbd_close) __P((dev_t, int, int, struct proc *));
	paddr_t	(*fbd_mmap) __P((dev_t, off_t, int));
	/* These are the internal ioctl functions */
	int 	(*fbd_gattr) __P((struct fbdevice *,  void *));
	int 	(*fbd_gvideo) __P((struct fbdevice *, void *));
	int 	(*fbd_svideo) __P((struct fbdevice *, void *));
	int 	(*fbd_getcmap) __P((struct fbdevice *, void *));
	int 	(*fbd_putcmap) __P((struct fbdevice *, void *));
};

int 	fbioctlfb __P((struct fbdevice *, u_long, caddr_t));

void	fb_attach __P((struct fbdevice *, int));
int 	fb_noioctl __P((struct fbdevice *, void *));
void	fb_unblank __P((void));

void	fb_eeprom_setsize  __P((struct fbdevice *));

int 	fb_pfour_id __P((void *));
int 	fb_pfour_get_video __P((struct fbdevice *));
void	fb_pfour_set_video __P((struct fbdevice *, int));

void	fb_pfour_setsize __P((struct fbdevice *));

/* This comes from enable.c */
void	enable_video __P((int));
