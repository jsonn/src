/*	$NetBSD: elink3var.h,v 1.5.4.1 1997/03/04 18:40:29 mycroft Exp $	*/

/*
 * Copyright (c) 1994 Herb Peyerl <hpeyerl@beer.org>
 * All rights reserved.
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
 *      This product includes software developed by Herb Peyerl.
 * 4. The name of Herb Peyerl may not be used to endorse or promote products
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

/*
 * Ethernet software status per interface.
 */
struct ep_softc {
	struct device sc_dev;
	void *sc_ih;

	struct arpcom sc_arpcom;	/* Ethernet common part		*/
	bus_chipset_tag_t sc_bc;	/* bus cookie			*/
	bus_io_handle_t sc_ioh;		/* bus i/o handle		*/
	char    ep_connectors;		/* Connectors on this card.	*/
#define MAX_MBS	8			/* # of mbufs we keep around	*/
	struct mbuf *mb[MAX_MBS];	/* spare mbuf storage.		*/
	int	next_mb;		/* Which mbuf to use next. 	*/
	int	last_mb;		/* Last mbuf.			*/
	int	tx_start_thresh;	/* Current TX_start_thresh.	*/
	int	tx_succ_ok;		/* # packets sent in sequence   */
					/* w/o underrun			*/
	u_char	txashift;		/* shift packet size		*/
	u_char	bustype;
#define EP_BUS_ISA	  	0x0
#define	EP_BUS_PCMCIA	  	0x1
#define	EP_BUS_EISA	  	0x2
#define EP_BUS_PCI	  	0x3

#define EP_IS_BUS_32(a)	((a) & 0x2)
};

u_int16_t epreadeeprom __P((bus_chipset_tag_t, bus_io_handle_t, int));
void	epconfig __P((struct ep_softc *, u_int16_t));
int	epintr __P((void *));
void	epstop __P((struct ep_softc *));
