/*	$NetBSD: uhavar.h,v 1.6.4.1 1997/11/04 06:04:48 thorpej Exp $	*/

/*
 * Copyright (c) 1994, 1996, 1997 Charles M. Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

#include <sys/queue.h>

#define UHA_MSCP_MAX	32	/* store up to 32 MSCPs at one time */
#define	MSCP_HASH_SIZE	32	/* hash table size for phystokv */
#define	MSCP_HASH_SHIFT	9
#define MSCP_HASH(x)	((((long)(x))>>MSCP_HASH_SHIFT) & (MSCP_HASH_SIZE - 1))

struct uha_softc {
	struct device sc_dev;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	bus_dma_tag_t	sc_dmat;
	void *sc_ih;

	int sc_dmaflags;	/* bus-specific DMA map creation flags */

	void (*start_mbox) __P((struct uha_softc *, struct uha_mscp *));
	int (*poll) __P((struct uha_softc *, struct scsipi_xfer *, int));
	void (*init) __P((struct uha_softc *));

	struct uha_mscp *sc_mscphash[MSCP_HASH_SIZE];
	TAILQ_HEAD(, uha_mscp) sc_free_mscp;
	int sc_nummscps;
	struct scsipi_link sc_link;

	LIST_HEAD(, scsipi_xfer) sc_queue;
	struct scsipi_xfer *sc_queuelast;
};

struct uha_probe_data {
	int sc_irq, sc_drq;
	int sc_scsi_dev;
};

void	uha_attach __P((struct uha_softc *, struct uha_probe_data *));
void	uha_timeout __P((void *arg));
struct	uha_mscp *uha_mscp_phys_kv __P((struct uha_softc *, u_long));
void	uha_done __P((struct uha_softc *, struct uha_mscp *));
