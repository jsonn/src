/*	$NetBSD: grf_subr.c,v 1.5.30.2 2002/06/23 17:36:07 jdolecek Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * Subroutines common to all framebuffer devices.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: grf_subr.c,v 1.5.30.2 2002/06/23 17:36:07 jdolecek Exp $");                                                  

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h> 
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <hp300/dev/grfioctl.h>
#include <hp300/dev/grfvar.h>

int	grfdevprint __P((void *, const char *));

void
grfdev_attach(sc, init, regs, sw)
	struct grfdev_softc *sc;
	int (*init) __P((struct grf_data *, int, caddr_t));
	caddr_t regs;
	struct grfsw *sw;
{
	struct grfdev_attach_args ga;
	struct grf_data *gp;

	if (sc->sc_isconsole) 
		sc->sc_data = gp = &grf_cn;
	else {
		MALLOC(sc->sc_data, struct grf_data *, sizeof(struct grf_data),
		    M_DEVBUF, M_NOWAIT | M_ZERO);
		if (sc->sc_data == NULL) {
			printf("\n%s: can't allocate grf data\n",
			    sc->sc_dev.dv_xname);
			return;
		}

		/* Initialize the framebuffer hardware. */
		if ((*init)(sc->sc_data, sc->sc_scode, regs) == 0) {
			printf("\n%s: init failed\n",
			    sc->sc_dev.dv_xname);
			free(sc->sc_data, M_DEVBUF);
			return;
		}

		gp = sc->sc_data;
		gp->g_flags = GF_ALIVE;
		gp->g_sw = sw;
		gp->g_display.gd_id = gp->g_sw->gd_swid;
	}

	/* Announce ourselves. */
	printf(": %d x %d ", gp->g_display.gd_dwidth,
	    gp->g_display.gd_dheight);
	if (gp->g_display.gd_colors == 2)
		printf("monochrome");
	else
		printf("%d color", gp->g_display.gd_colors);
	printf(" %s display\n", gp->g_sw->gd_desc);

	/* Attach a grf. */
	ga.ga_scode = sc->sc_scode;	/* XXX */
	ga.ga_isconsole = sc->sc_isconsole;
	ga.ga_data = (void *)sc->sc_data;
	(void)config_found(&sc->sc_dev, &ga, grfdevprint);
}

int
grfdevprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	/* struct grfdev_attach_args *ga = aux; */

	/* Only grf's can attach to grfdev's... easy. */
	if (pnp)
		printf("grf at %s", pnp);

	return (UNCONF);
}
