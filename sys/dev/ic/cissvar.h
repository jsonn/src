/*	$NetBSD: cissvar.h,v 1.1.8.2 2006/04/11 12:20:52 yamt Exp $	*/
/*	$OpenBSD: cissvar.h,v 1.2 2005/09/07 04:00:16 mickey Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

struct ciss_softc {
	/* Generic device info. */
	struct device		sc_dev;
	bus_space_handle_t	sc_ioh;
	bus_space_tag_t		sc_iot;
	bus_dma_tag_t		sc_dmat;
	void			*sc_ih;
	void			*sc_sh;		/* shutdown hook */
	struct proc		*sc_thread;
	int			sc_flush;

	struct scsipi_channel	sc_channel;
	struct scsipi_channel	*sc_channel_raw;
	struct scsipi_adapter	sc_adapter;
	struct scsipi_adapter	*sc_adapter_raw;
	struct callout		sc_hb;

	u_int	sc_flags;
	int ccblen, maxcmd, maxsg, nbus, ndrives, maxunits;
	ciss_queue_head	sc_free_ccb, sc_ccbq, sc_ccbdone;

	bus_dmamap_t		cmdmap;
	bus_dma_segment_t	cmdseg[1];
	caddr_t			ccbs;
	void			*scratch;

	bus_space_handle_t	cfg_ioh;

	struct ciss_config cfg;
	int cfgoff;
	u_int32_t iem;
	u_int32_t heartbeat;
};

struct ciss_rawsoftc {
	struct ciss_softc *sc_softc;
	u_int8_t	sc_channel;
};

/* XXX These have to become spinlocks in case of fine SMP */
#define	CISS_LOCK(sc) splbio()
#define	CISS_UNLOCK(sc, lock) splx(lock)
#define	CISS_LOCK_SCRATCH(sc) splbio()
#define	CISS_UNLOCK_SCRATCH(sc, lock) splx(lock)
typedef	int ciss_lock_t;

int	ciss_attach(struct ciss_softc *sc);
int	ciss_intr(void *v);
