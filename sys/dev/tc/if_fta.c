/*	$NetBSD: if_fta.c,v 1.8.6.2 1997/03/09 21:12:38 is Exp $	*/

/*-
 * Copyright (c) 1996 Matt Thomas <matt@3am-software.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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
 *
 * Id: if_fta.c,v 1.3 1996/05/17 01:15:18 thomas Exp
 *
 */

/*
 * DEC TurboChannel FDDI Controller; code for BSD derived operating systems
 *
 * Written by Matt Thomas
 *
 *   This module supports the DEC DEFTA TurboChannel FDDI Controller
 */


#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_ether.h>

#ifdef INET
#include <netinet/in.h>
#if defined(__NetBSD__)
#include <netinet/if_inarp.h>
#else
#include <netinet/if_ether.h>
#endif
#include <net/if_fddi.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_param.h>

#include <dev/tc/tcvar.h>
#include <dev/ic/pdqvar.h>
#include <dev/ic/pdqreg.h>

static int
pdq_tc_match(
    struct device *parent,
#ifdef __BROKEN_INDIRECT_CONFIG
    void *match,
#else
    struct cfdata *match,
#endif
    void *aux)
{
    struct tc_attach_args *ta = (struct tc_attach_args *) aux;

    if (strncmp("PMAF-F", ta->ta_modname, 6) == 0)
	return 1;

    return 0;
}

static void
pdq_tc_attach(
    struct device * const parent,
    struct device * const self,
    void * const aux)
{
    pdq_softc_t * const sc = (pdq_softc_t *) self;
    struct tc_attach_args * const ta = (struct tc_attach_args *) aux;

    /*
     * NOTE: sc_bc is an alias for sc_csrtag and sc_membase is an
     * alias for sc_csrhandle.  sc_iobase is not used in this front-end.
     */
    sc->sc_csrtag = ta->ta_memt;
    bcopy(sc->sc_dev.dv_xname, sc->sc_if.if_xname, IFNAMSIZ);
    sc->sc_if.if_flags = 0;
    sc->sc_if.if_softc = sc;

    if (bus_space_map(sc->sc_csrtag, ta->ta_addr + PDQ_TC_CSR_OFFSET,
		    PDQ_TC_CSR_SPACE, 0, &sc->sc_csrhandle)) {
        printf("\n%s: can't map card memory!\n", sc->sc_dev.dv_xname);
	return;
    }

    sc->sc_pdq = pdq_initialize(sc->sc_csrtag, sc->sc_csrhandle,
				sc->sc_if.if_xname, 0,
				(void *) sc, PDQ_DEFTA);
    if (sc->sc_pdq == NULL) {
	printf("%s: initialization failed\n", sc->sc_dev.dv_xname);
	return;
    }
#if !defined(__NetBSD__)
    bcopy((caddr_t) sc->sc_pdq->pdq_hwaddr.lanaddr_bytes, sc->sc_ac.ac_enaddr, 6);
#endif

    pdq_ifattach(sc, NULL);

    tc_intr_establish(parent, ta->ta_cookie, TC_IPL_NET,
		      (int (*)(void *)) pdq_interrupt, sc->sc_pdq);

    sc->sc_ats = shutdownhook_establish((void (*)(void *)) pdq_hwreset, sc->sc_pdq);
    if (sc->sc_ats == NULL)
	printf("%s: warning: couldn't establish shutdown hook\n", self->dv_xname);
}

struct cfattach fta_ca = { sizeof(pdq_softc_t), pdq_tc_match, pdq_tc_attach };
struct cfdriver fta_cd = { 0, "fta", DV_IFNET };
