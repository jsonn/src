/*	$NetBSD: if_ae.c,v 1.65.4.1 1997/11/05 18:51:48 thorpej Exp $	*/

/*
 * Device driver for National Semiconductor DS8390/WD83C690 based ethernet
 * adapters.
 *
 * Copyright (c) 1994, 1995 Charles M. Hannum.  All rights reserved.
 *
 * Copyright (C) 1993, David Greenman.  This software may be used, modified,
 * copied, distributed, and sold, in both source and binary form provided that
 * the above copyright and these terms are retained.  Under no circumstances is
 * the author responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with its use.
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <machine/bus.h>

#include <dev/ic/dp8390reg.h>
#include <dev/ic/dp8390var.h>
#include <mac68k/dev/if_aevar.h>

struct cfdriver ae_cd = {
	NULL, "ae", DV_IFNET
};

int
ae_size_card_memory(bst, bsh, ofs)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	int ofs;
{
	int i1, i2, i3, i4;

	/*
	 * banks; also assume it will generally mirror in upper banks
	 * if not installed.
	 */
	i1 = (8192 * 0);
	i2 = (8192 * 1);
	i3 = (8192 * 2);
	i4 = (8192 * 3);

	bus_space_write_2(bst, bsh, ofs + i1, 0x1111);
	bus_space_write_2(bst, bsh, ofs + i2, 0x2222);
	bus_space_write_2(bst, bsh, ofs + i3, 0x3333);
	bus_space_write_2(bst, bsh, ofs + i4, 0x4444);

	if (bus_space_read_2(bst, bsh, ofs + i1) == 0x1111 &&
	    bus_space_read_2(bst, bsh, ofs + i2) == 0x2222 &&
	    bus_space_read_2(bst, bsh, ofs + i3) == 0x3333 &&
	    bus_space_read_2(bst, bsh, ofs + i4) == 0x4444)
		return 8192 * 4;

	if ((bus_space_read_2(bst, bsh, ofs + i1) == 0x1111 &&
	    bus_space_read_2(bst, bsh, ofs + i2) == 0x2222) ||
	    (bus_space_read_2(bst, bsh, ofs + i1) == 0x3333 &&
	    bus_space_read_2(bst, bsh, ofs + i2) == 0x4444))
		return 8192 * 2;

	if (bus_space_read_2(bst, bsh, ofs + i1) == 0x1111 ||
	    bus_space_read_2(bst, bsh, ofs + i1) == 0x4444)
		return 8192;

	return 0;
}

/*
 * Zero memory and verify that it is clear.  The only difference between
 * this and the default test_mem function is that the DP8390-based NuBus
 * cards * apparently require word-wide writes and byte-wide reads, an
 * `interesting' combination.
 */
int
ae_test_mem(sc)
	struct dp8390_softc *sc;
{
	bus_space_tag_t buft = sc->sc_buft;
	bus_space_handle_t bufh = sc->sc_bufh;
	int i;

	bus_space_set_region_2(buft, bufh, sc->mem_start, 0,
	    sc->mem_size / 2);

	for (i = 0; i < sc->mem_size; ++i) {
		if (bus_space_read_1(sc->sc_buft, sc->sc_bufh, i)) {
			printf(": failed to clear NIC buffer at offset %x - "
			    "check configuration\n", (sc->mem_start + i));
			return 1;
		}
	}

	return 0;
}

/*
 * Copy packet from mbuf to the board memory Currently uses an extra
 * buffer/extra memory copy, unless the whole packet fits in one mbuf.
 *
 * As in the test_mem function, we use word-wide writes.
 */
int
ae_write_mbuf(sc, m, buf)
	struct dp8390_softc *sc;
	struct mbuf *m;
	int buf;
{
	u_char *data, savebyte[2];
	int len, wantbyte;
	u_short totlen = 0;

	wantbyte = 0;

	for (; m ; m = m->m_next) {
		data = mtod(m, u_char *);
		len = m->m_len;
		totlen += len;
		if (len > 0) {
			/* Finish the last word. */
			if (wantbyte) {
				savebyte[1] = *data;
				bus_space_write_region_2(sc->sc_buft,
				    sc->sc_bufh, buf, savebyte, 1);
				buf += 2;
				data++;
				len--;
				wantbyte = 0;
			}
			/* Output contiguous words. */
			if (len > 1) {
				bus_space_write_region_2(sc->sc_buft,
				    sc->sc_bufh, buf, data, len >> 1);
				buf += len & ~1;
				data += len & ~1;
				len &= 1;
			}
			/* Save last byte, if necessary. */
			if (len == 1) {
				savebyte[0] = *data;
				wantbyte = 1;
			}
		}
	}

	if (wantbyte) {
		savebyte[1] = 0;
		bus_space_write_region_2(sc->sc_buft, sc->sc_bufh,
		    buf, savebyte, 1);
	}
	return (totlen);
}
