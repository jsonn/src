/*	$NetBSD: hpibvar.h,v 1.13.50.1 2004/08/03 10:34:23 skrll Exp $	*/

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
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

/*
 * Copyright (c) 1982, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)hpibvar.h	8.1 (Berkeley) 6/10/93
 */

#include <sys/queue.h>

#define	HPIB_IPL(x)	((((x) >> 4) & 0x3) + 3)

#define	HPIBA		32
#define	HPIBB		1
#define	HPIBC		8
#define	HPIBA_BA	21
#define	HPIBC_BA	30
#define	HPIBA_IPL	3

#define	CSA_BA		0x1F

#define	IDS_WDMA	0x04
#define	IDS_WRITE	0x08
#define	IDS_IR		0x40
#define	IDS_IE		0x80
#define	IDS_DMA(x)	(1 << (x))

#define	C_SDC		0x04	/* Selected device clear */
#define	C_SDC_P		0x04	/*  with odd parity */
#define	C_DCL		0x14	/* Universal device clear */
#define	C_DCL_P		0x94	/*  with odd parity */
#define	C_LAG		0x20	/* Listener address group commands */
#define	C_UNL		0x3f	/* Universal unlisten */
#define	C_UNL_P		0xbf	/*  with odd parity */
#define	C_TAG		0x40	/* Talker address group commands */
#define	C_UNA		0x5e	/* Unaddress (master talk address?) */
#define	C_UNA_P		0x5e	/*  with odd parity */
#define	C_UNT		0x5f	/* Universal untalk */
#define	C_UNT_P		0xdf	/*  with odd parity */
#define	C_SCG		0x60	/* Secondary group commands */

struct hpibbus_softc;

/*
 * Each of the HP-IB controller drivers fills in this structure, which
 * is used by the indirect driver to call controller-specific functions.
 */
struct	hpib_controller {
	void	(*hpib_reset) __P((struct hpibbus_softc *));
	int	(*hpib_send) __P((struct hpibbus_softc *,
		    int, int, void *, int));
	int	(*hpib_recv) __P((struct hpibbus_softc *,
		    int, int, void *, int));
	int	(*hpib_ppoll) __P((struct hpibbus_softc *));
	void	(*hpib_ppwatch) __P((void *));
	void	(*hpib_go) __P((struct hpibbus_softc *,
		    int, int, void *, int, int, int));
	void	(*hpib_done) __P((struct hpibbus_softc *));
	int	(*hpib_intr) __P((void *));
};

/*
 * Attach an HP-IB bus to an HP-IB controller.
 */
struct hpibdev_attach_args {
	struct	hpib_controller *ha_ops;	/* controller ops vector */
	int	ha_type;			/* XXX */
	int	ha_ba;
	struct hpibbus_softc **ha_softcpp;	/* XXX */
};

/*
 * Attach an HP-IB device to an HP-IB bus.
 */
struct hpibbus_attach_args {
	u_int16_t ha_id;		/* device id */
	int	ha_slave;		/* HP-IB bus slave */
	int	ha_punit;		/* physical unit on slave */
};

/* Locator short-hand */
#include "locators.h"

#define	hpibbuscf_slave		cf_loc[HPIBBUSCF_SLAVE]
#define	hpibbuscf_punit		cf_loc[HPIBBUSCF_PUNIT]

#define	HPIB_NSLAVES		8	/* number of slaves on a bus */
#define	HPIB_NPUNITS		2	/* number of punits per slave */

/*
 * An HP-IB job queue entry.  Slave drivers have one of these used
 * to queue requests with the controller.
 */
struct hpibqueue {
	TAILQ_ENTRY(hpibqueue) hq_list;	/* entry on queue */
	void	*hq_softc;		/* slave's softc */
	int	hq_slave;		/* slave on bus */

	/*
	 * Callbacks used to start and stop the slave driver.
	 */
	void	(*hq_start) __P((void *));
	void	(*hq_go) __P((void *));
	void	(*hq_intr) __P((void *));
};

struct dmaqueue;

/*
 * Software state per HP-IB bus.
 */
struct hpibbus_softc {
	struct	device sc_dev;		/* generic device glue */
	struct	hpib_controller *sc_ops; /* controller ops vector */
	volatile int sc_flags;		/* misc flags */
	struct	dmaqueue *sc_dq;
	TAILQ_HEAD(, hpibqueue) sc_queue;
	int	sc_ba;
	int	sc_type;
	char	*sc_addr;
	int	sc_count;
	int	sc_curcnt;

	/*
	 * HP-IB is an indirect bus; this cheezy resource map
	 * keeps track of slave/punit allocations.
	 */
	char	sc_rmap[HPIB_NSLAVES][HPIB_NPUNITS];
};

/* sc_flags */
#define	HPIBF_IO	0x1
#define	HPIBF_DONE	0x2
#define	HPIBF_PPOLL	0x4
#define	HPIBF_READ	0x8
#define	HPIBF_TIMO	0x10
#define	HPIBF_DMA16	0x8000

#ifdef _KERNEL
extern	caddr_t internalhpib;
extern	int hpibtimeout;
extern	int hpibdmathresh;

void	hpibreset __P((int));
int	hpibsend __P((int, int, int, void *, int));
int	hpibrecv __P((int, int, int, void *, int));
int	hpibustart __P((int));
void	hpibstart __P((void *));
void	hpibgo __P((int, int, int, void *, int, int, int));
void	hpibdone __P((void *));
int	hpibpptest __P((int, int));
void	hpibppclear __P((int));
void	hpibawait __P((int));
int	hpibswait __P((int, int));
int	hpibid __P((int, int));

int	hpibreq __P((struct device *, struct hpibqueue *));
void	hpibfree __P((struct device *, struct hpibqueue *));
int	hpibbus_alloc __P((struct hpibbus_softc *, int, int));
void	hpibbus_free __P((struct hpibbus_softc *, int, int));

int	hpibintr __P((void *));
int	hpibdevprint __P((void *, const char *));
#endif
