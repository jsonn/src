/*	$NetBSD: fwnode.c,v 1.5.2.5 2002/02/28 04:13:34 nathanw Exp $	*/

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by James Chacon.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: fwnode.c,v 1.5.2.5 2002/02/28 04:13:34 nathanw Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#include <dev/std/ieee1212var.h>
#include <dev/std/ieee1212reg.h>

#include <dev/ieee1394/ieee1394reg.h>
#include <dev/ieee1394/fwnodereg.h>

#include <dev/ieee1394/ieee1394var.h>
#include <dev/ieee1394/fwnodevar.h>

static const char * const ieee1394_speeds[] = { IEEE1394_SPD_STRINGS };

int  fwnode_match(struct device *, struct cfdata *, void *);
void fwnode_attach(struct device *, struct device *, void *);
int  fwnode_detach(struct device *, int);
static void fwnode_configrom_input(struct ieee1394_abuf *, int);
static int  fwnode_print(void *, const char *);
static void fwnode_dump_rom(struct fwnode_softc *,u_int32_t *, u_int32_t);

#ifdef FWNODE_DEBUG
#define DPRINTF(x)      if (fwnodedebug) printf x
#define DPRINTFN(n,x)   if (fwnodedebug>(n)) printf x
int     fwnodedebug = 1;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

struct cfattach fwnode_ca = {
	sizeof(struct fwnode_softc), fwnode_match, fwnode_attach,
	fwnode_detach
};

int
fwnode_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct ieee1394_attach_args *fwa = aux;
	
	if (strcmp(fwa->name, "fwnode") == 0)
		return 1;
	return 0;
}

void
fwnode_attach(struct device *parent, struct device *self, void *aux)
{
	struct fwnode_softc *sc = (struct fwnode_softc *)self;
	struct ieee1394_softc *psc = (struct ieee1394_softc *)parent;
	struct ieee1394_attach_args *fwa = aux;
	struct ieee1394_abuf *ab;
	
	ab = malloc(sizeof(struct ieee1394_abuf), M_1394DATA, M_WAITOK|M_ZERO);
	ab->ab_data = malloc(4, M_1394DATA, M_WAITOK);
	ab->ab_data[0] = 0;
	
	sc->sc_sc1394.sc1394_node_id = fwa->nodeid;
	memcpy(sc->sc_sc1394.sc1394_guid, fwa->uid, 8);
	sc->sc1394_read = fwa->read;
	sc->sc1394_write = fwa->write;
	sc->sc1394_inreg = fwa->inreg;
	sc->sc1394_unreg = fwa->unreg;
	
	/* XXX. Fix the fw code to use the generic routines. */
	sc->sc_sc1394.sc1394_ifinreg = psc->sc1394_ifinreg;
	sc->sc_sc1394.sc1394_ifoutput = psc->sc1394_ifoutput;
	
	printf(" Node %d: UID %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n", 
	    sc->sc_sc1394.sc1394_node_id,
	    sc->sc_sc1394.sc1394_guid[0], sc->sc_sc1394.sc1394_guid[1],
	    sc->sc_sc1394.sc1394_guid[2], sc->sc_sc1394.sc1394_guid[3],
	    sc->sc_sc1394.sc1394_guid[4], sc->sc_sc1394.sc1394_guid[5],
	    sc->sc_sc1394.sc1394_guid[6], sc->sc_sc1394.sc1394_guid[7]);
	ab->ab_req = (struct ieee1394_softc *)sc;
	ab->ab_addr = CSR_BASE + CSR_CONFIG_ROM;
	ab->ab_length = 4;
	ab->ab_retlen = 0;
	ab->ab_cbarg = NULL;
	ab->ab_cb = fwnode_configrom_input;
	sc->sc1394_read(ab);
}

int
fwnode_detach(struct device *self, int flags)
{
        struct fwnode_softc *sc = (struct fwnode_softc *)self;
	struct device **children;
	
	if (sc->sc_children) {
		children = sc->sc_children;
		while (*children++)
			config_detach(*children, 0);
		free(sc->sc_children, M_DEVBUF);
	}
	
	if (sc->sc_sc1394.sc1394_configrom &&
	    sc->sc_sc1394.sc1394_configrom_len)
		free(sc->sc_sc1394.sc1394_configrom, M_1394DATA);
	
	if (sc->sc_configrom)
		p1212_free(sc->sc_configrom);
	return 0;
}

/*
 * This code is trying to build a complete image of the ROM in memory.
 * This is done all here to keep the bus_read logic/callback for the ROM in one
 * place since reading the whole ROM may require lots of small reads up front
 * and building separate callback handlers for each step would be even worse.
 */

static void
fwnode_configrom_input(struct ieee1394_abuf *ab, int rcode)
{
	struct fwnode_softc *sc = (struct fwnode_softc *)ab->ab_req;
	u_int32_t val;
	
	if (rcode != IEEE1394_RCODE_COMPLETE) {
		DPRINTF(("Aborting configrom input, rcode: %d\n", rcode));
#ifdef FWNODE_DEBUG
		fwnode_dump_rom(sc, ab->ab_data, ab->ab_length);
#endif
		free(ab->ab_data, M_1394DATA);
		free(ab, M_1394DATA);
		return;
	}
	
	if (ab->ab_cbarg)
		panic("Got an invalid abuf on callback\n");

	if (ab->ab_length != ab->ab_retlen) {
		DPRINTF(("%s: config rom short read. Expected :%d, received: "
		    "%d. Not attaching\n", sc->sc_sc1394.sc1394_dev.dv_xname,
		    ab->ab_length, ab->ab_retlen));
		free(ab->ab_data, M_1394DATA);
		free(ab, M_1394DATA);
		return;
	} 
	if (ab->ab_retlen % 4) {
		DPRINTF(("%s: configrom read of invalid length: %d\n",
		    sc->sc_sc1394.sc1394_dev.dv_xname, ab->ab_retlen));
		free(ab->ab_data, M_1394DATA);
		free(ab, M_1394DATA);
		return;
	}
	
	ab->ab_retlen = ab->ab_retlen / 4;
	if (p1212_iscomplete(ab->ab_data, &ab->ab_retlen) == -1) {
		DPRINTF(("%s: configrom parse error\n",
		    sc->sc_sc1394.sc1394_dev.dv_xname));
		free(ab->ab_data, M_1394DATA);
		free(ab, M_1394DATA);
		return;
	}

#ifdef DIAGNOSTIC
	if (ab->ab_retlen < (ab->ab_length / 4))
		panic("Configrom shrank during iscomplete check?\n");
#endif
	
	if (ab->ab_retlen > (ab->ab_length / 4)) {

		free(ab->ab_data, M_1394DATA);
		ab->ab_data = malloc(ab->ab_retlen * 4, M_1394DATA, 
		    M_WAITOK|M_ZERO);
		
		ab->ab_addr = CSR_BASE + CSR_CONFIG_ROM;
		ab->ab_length = ab->ab_retlen * 4;
		ab->ab_retlen = 0;
		ab->ab_cbarg = NULL;
		ab->ab_cb = fwnode_configrom_input;
		sc->sc1394_read(ab);
		return;
	} else {
		sc->sc_sc1394.sc1394_configrom_len = ab->ab_retlen;
		sc->sc_sc1394.sc1394_configrom = ab->ab_data;
		ab->ab_data = NULL;
		
		free(ab, M_1394DATA);
		
		/* 
		 * Set P1212_ALLOW_DEPENDENT_INFO_OFFSET_TYPE and
		 * P1212_ALLOW_DEPENDENT_INFO_IMMED_TYPE as some protocols
		 * such as SBP2 need it. 
		 */
		
		val = P1212_ALLOW_DEPENDENT_INFO_OFFSET_TYPE;
		val |= P1212_ALLOW_DEPENDENT_INFO_IMMED_TYPE;
		sc->sc_configrom =
		    p1212_parse(sc->sc_sc1394.sc1394_configrom,
		    sc->sc_sc1394.sc1394_configrom_len, val);
		if ((sc->sc_configrom == NULL) ||
		    (sc->sc_configrom->len != IEEE1394_BUSINFO_LEN)) {
#ifdef FWNODE_DEBUG
			DPRINTF(("Parse error with config rom\n"));
			fwnode_dump_rom(sc, sc->sc_sc1394.sc1394_configrom, 
				sc->sc_sc1394.sc1394_configrom_len);
#endif
			if (sc->sc_configrom)
				p1212_free(sc->sc_configrom);
			free(sc->sc_sc1394.sc1394_configrom, M_1394DATA);
			sc->sc_sc1394.sc1394_configrom = NULL;
			sc->sc_sc1394.sc1394_configrom_len = 0;
			return;
		}
		
		val = htonl(IEEE1394_SIGNATURE);
		if (memcmp(sc->sc_configrom->name, &val, 4)) {
			DPRINTF(("Invalid signature found in bus info block: "
				    "%s\n", sc->sc_configrom->name));
			p1212_free(sc->sc_configrom);
			sc->sc_sc1394.sc1394_configrom = NULL;
			sc->sc_sc1394.sc1394_configrom_len = 0;
			return;
		}

		sc->sc_sc1394.sc1394_max_receive =
		    IEEE1394_GET_MAX_REC(ntohl(sc->sc_configrom->data[0]));
		sc->sc_sc1394.sc1394_link_speed =
		    IEEE1394_GET_LINK_SPD(ntohl(sc->sc_configrom->data[0]));
		printf("%s: Link Speed: %s, max_rec: %d bytes\n",
		    sc->sc_sc1394.sc1394_dev.dv_xname, 
		    ieee1394_speeds[sc->sc_sc1394.sc1394_link_speed],
		    IEEE1394_MAX_REC(sc->sc_sc1394.sc1394_max_receive));
#ifdef FWNODE_DEBUG
		fwnode_dump_rom(sc, sc->sc_sc1394.sc1394_configrom, 
			sc->sc_sc1394.sc1394_configrom_len);
		p1212_print(sc->sc_configrom->root);
#endif
		sc->sc_children = p1212_match_units(&sc->sc_sc1394.sc1394_dev,
			sc->sc_configrom->root, fwnode_print);
	}
}

static int
fwnode_print(void *aux, const char *pnp)
{
	if (pnp)
		printf("Unknown device at %s", pnp);
	
	return UNCONF;
}

static void
fwnode_dump_rom(struct fwnode_softc *sc, u_int32_t *t, u_int32_t len)
{
	int i;
	printf("%s: Config rom dump:\n", sc->sc_sc1394.sc1394_dev.dv_xname);
	for (i = 0; i < len; i++) {
		if ((i % 4) == 0) {
			if (i)
				printf("\n");
			printf("%s: 0x%02hx: ",
			    sc->sc_sc1394.sc1394_dev.dv_xname, (short)(4 * i));
		}
		printf("0x%08x ", ntohl(t[i]));
	}
	printf("\n");
}
