/*	$NetBSD: irframevar.h,v 1.12.2.1 2003/07/02 15:26:08 darrenr Exp $	*/

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net).
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

struct irframe_methods {
	int (*im_open)(void *h, int flag, int mode, struct lwp *l);
	int (*im_close)(void *h, int flag, int mode, struct lwp *l);
	int (*im_read)(void *h, struct uio *uio, int flag);
	int (*im_write)(void *h, struct uio *uio, int flag);
	int (*im_poll)(void *h, int events, struct lwp *l);
	int (*im_kqfilter)(void *h, struct knote *kn);
	int (*im_set_params)(void *h, struct irda_params *params);
	int (*im_get_speeds)(void *h, int *speeds);
	int (*im_get_turnarounds)(void *h, int *times);
};

struct irframe_softc {
	struct	device			sc_dev;
	const struct irframe_methods	*sc_methods;
	void				*sc_handle;
	char				sc_open;
	struct irda_params		sc_params;
	u_int				sc_speedmask;
#ifdef DIAGNOSTIC
	u_int				sc_speed;
#endif
};

#define IRDA_DEFAULT_SPEED	9600
#define IRDA_DEFAULT_EBOFS	12
#define IRDA_DEFAULT_SIZE	386

#define IRDA_MAX_FRAME_SIZE	2048
#define IRDA_MAX_EBOFS		64

struct device *irframe_alloc(size_t, const struct irframe_methods *, void *);
void irframe_dealloc(struct device *);

