/*	$NetBSD: mb86960var.h,v 1.27.2.1 2002/10/10 18:39:07 jdolecek Exp $	*/

/*
 * All Rights Reserved, Copyright (C) Fujitsu Limited 1995
 *
 * This software may be used, modified, copied, distributed, and sold, in
 * both source and binary form provided that the above copyright, these
 * terms and the following disclaimer are retained.  The name of the author
 * and/or the contributor may not be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND THE CONTRIBUTOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR THE CONTRIBUTOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION.
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Portions copyright (C) 1993, David Greenman.  This software may be used,
 * modified, copied, distributed, and sold, in both source and binary form
 * provided that the above copyright and these terms are retained.  Under no
 * circumstances is the author responsible for the proper functioning of this
 * software, nor does the author assume any responsibility for damages
 * incurred with its use.
 */

/*
 * Device driver for Fujitsu MB86960A/MB86965A based Ethernet cards.
 * Contributed by M.S. <seki@sysrap.cs.fujitsu.co.jp>
 *
 * This version is intended to be a generic template for various
 * MB86960A/MB86965A based Ethernet cards.  It currently supports
 * Fujitsu FMV-180 series (i.e., FMV-181 and FMV-182) and Allied-
 * Telesis AT1700 series and RE2000 series.  There are some
 * unnecessary hooks embedded, which are primarily intended to support
 * other types of Ethernet cards, but the author is not sure whether
 * they are useful.
 */

#include "rnd.h"

#if NRND > 0
#include <sys/rnd.h>
#endif

/*
 * Default settings for fe driver specific options.
 * They can be set in config file by "options" statements.
 */

/*
 * Debug control.
 * 0: No debug at all.  All debug specific codes are stripped off.
 * 1: Silent.  No debug messages are logged except emergent ones.
 * 2: Brief.  Lair events and/or important information are logged.
 * 3: Detailed.  Logs all information which *may* be useful for debugging.
 * 4: Trace.  All actions in the driver is logged.  Super verbose.
 */
#ifndef FE_DEBUG
#define FE_DEBUG		1
#endif

/*
 * Delay padding of short transmission packets to minimum Ethernet size.
 * This may or may not gain performance.  An EXPERIMENTAL option.
 */
#ifndef FE_DELAYED_PADDING
#define FE_DELAYED_PADDING	0
#endif

/*
 * Transmit just one packet per a "send" command to 86960.
 * This option is intended for performance test.  An EXPERIMENTAL option.
 */
#ifndef FE_SINGLE_TRANSMISSION
#define FE_SINGLE_TRANSMISSION	0
#endif

/*
 * Device configuration flags.
 */

/* DLCR6 settings. */
#define FE_FLAGS_DLCR6_VALUE	0x007F

/* Force DLCR6 override. */
#define FE_FLAGS_OVERRIDE_DLCR6	0x0080

/* A cludge for PCMCIA support. */
#define FE_FLAGS_PCMCIA		0x8000

/*
 * Supported hardware (Ethernet card) types
 * This information is currently used only for debugging
 */
enum fe_type {
	/* For cards which are successfully probed but not identified. */
	FE_TYPE_UNKNOWN = 0,

	/* Fujitsu FMV-180 series. */
	FE_TYPE_FMV181,
	FE_TYPE_FMV182,

	/* Allied-Telesis AT1700 series and RE2000 series. */
	FE_TYPE_AT1700T,
	FE_TYPE_AT1700BT,
	FE_TYPE_AT1700FT,
	FE_TYPE_AT1700AT,
	FE_TYPE_RE2000,

	/* PCMCIA by Fujitsu. */
	FE_TYPE_MBH10302,
	FE_TYPE_MBH10304
};

enum mb86960_type {
	MB86960_TYPE_86960,
	MB86960_TYPE_86965
};

/*
 * fe_softc: per line info and status
 */
struct mb86960_softc {
	struct device sc_dev;
	struct ethercom sc_ec;		/* ethernet common */
	struct ifmedia sc_media;	/* supported media information */

	bus_space_tag_t sc_bst;		/* bus space */
	bus_space_handle_t sc_bsh;

	/* Set by probe() and not modified in later phases. */
	enum	mb86960_type type;	/* controller type */

	u_char	proto_dlcr4;		/* DLCR4 prototype. */
	u_char	proto_dlcr5;		/* DLCR5 prototype. */
	u_char	proto_dlcr6;		/* DLCR6 prototype. */
	u_char	proto_dlcr7;		/* DLCR7 prototype. */
	u_char	proto_bmpr13;		/* BMPR13 prototype. */

	/* Vendor specific hooks. */
	void	(*init_card) __P((struct mb86960_softc *));
	void	(*stop_card) __P((struct mb86960_softc *));

	/* Transmission buffer management. */
	u_short	txb_size;	/* total bytes in TX buffer */
	u_short	txb_free;	/* free bytes in TX buffer */
	u_char	txb_count;	/* number of packets in TX buffer */
	u_char	txb_sched;	/* number of scheduled packets */
	u_char	txb_padding;	/* number of delayed padding bytes */

	/* Multicast address filter management. */
	u_char	filter_change;	/* MARs must be changed ASAP. */
	u_char	filter[FE_FILTER_LEN];	/* new filter value. */

	u_int8_t sc_enaddr[ETHER_ADDR_LEN];

#if NRND > 0
	rndsource_element_t rnd_source;
#endif

	u_int32_t	sc_flags;	/* misc. flags */
#define FE_FLAGS_ENABLED	0x0001	/* power enabled on interface */
#define FE_FLAGS_ATTACHED	0x0002	/* attach has succeeded */

	int	(*sc_enable) __P((struct mb86960_softc *));
	void	(*sc_disable) __P((struct mb86960_softc *));

	int	(*sc_mediachange) __P((struct mb86960_softc *));
	void	(*sc_mediastatus) __P((struct mb86960_softc *,
		    struct ifmediareq *));
};

/*
 * Fe driver specific constants which relate to 86960/86965.
 */

/* Interrupt masks. */
#define FE_TMASK (FE_D2_COLL16 | FE_D2_TXDONE)
#define FE_RMASK (FE_D3_OVRFLO | FE_D3_CRCERR | \
		  FE_D3_ALGERR | FE_D3_SRTPKT | FE_D3_PKTRDY)

/* Maximum number of iterrations for a receive interrupt. */
#define FE_MAX_RECV_COUNT ((65536 - 2048 * 2) / 64)
	/* Maximum size of SRAM is 65536,
	 * minimum size of transmission buffer in fe is 2x2KB,
	 * and minimum amount of received packet including headers
	 * added by the chip is 64 bytes.
	 * Hence FE_MAX_RECV_COUNT is the upper limit for number
	 * of packets in the receive buffer. */

void	mb86960_attach	__P((struct mb86960_softc *, enum mb86960_type,
	    u_int8_t *));
void	mb86960_config	__P((struct mb86960_softc *, int *, int, int));
int	mb86960_intr	__P((void *));
int	mb86960_enable	__P((struct mb86960_softc *));
void	mb86960_disable	__P((struct mb86960_softc *));
int	mb86960_activate __P((struct device *, enum devact));
int	mb86960_detach	__P((struct mb86960_softc *));
