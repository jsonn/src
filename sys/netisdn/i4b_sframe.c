/*
 * Copyright (c) 1997, 2000 Hellmuth Michaelis. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	i4b_sframe.c - s frame handling routines
 *	----------------------------------------
 *
 *	$Id: i4b_sframe.c,v 1.5.8.4 2005/03/04 16:53:46 skrll Exp $
 *
 * $FreeBSD$
 *
 *      last edit-date: [Fri Jan  5 11:33:47 2001]
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: i4b_sframe.c,v 1.5.8.4 2005/03/04 16:53:46 skrll Exp $");

#ifdef __FreeBSD__
#include "i4bq921.h"
#else
#define	NI4BQ921	1
#endif
#if NI4BQ921 > 0

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/if.h>

#if defined(__NetBSD__) && __NetBSD_Version__ >= 104230000
#include <sys/callout.h>
#endif

#ifdef __FreeBSD__
#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#else
#include <netisdn/i4b_debug.h>
#include <netisdn/i4b_ioctl.h>
#endif

#include <netisdn/i4b_l2.h>
#include <netisdn/i4b_l1l2.h>
#include <netisdn/i4b_isdnq931.h>
#include <netisdn/i4b_mbuf.h>
#include <netisdn/i4b_l2fsm.h>
#include <netisdn/i4b_l3l4.h>

/*---------------------------------------------------------------------------*
 *	process s frame
 *---------------------------------------------------------------------------*/
void
i4b_rxd_s_frame(l2_softc_t *l2sc, struct isdn_l3_driver *drv, struct mbuf *m)
{
	u_char *ptr = m->m_data;

	if(!((l2sc->tei_valid == TEI_VALID) &&
	     (l2sc->tei == GETTEI(*(ptr+OFF_TEI)))))
	{
		i4b_Dfreembuf(m);
		return;
	}

	l2sc->rxd_CR = GETCR(*(ptr + OFF_SAPI));
	l2sc->rxd_PF = GETSPF(*(ptr + OFF_SNR));
	l2sc->rxd_NR = GETSNR(*(ptr + OFF_SNR));

	i4b_rxd_ack(l2sc, drv, l2sc->rxd_NR);

	switch(*(ptr + OFF_SRCR))
	{
		case RR:
			l2sc->stat.rx_rr++; /* update statistics */
			NDBGL2(L2_S_MSG, "rx'd RR, N(R) = %d", l2sc->rxd_NR);
			i4b_next_l2state(l2sc, drv, EV_RXRR);
			break;

		case RNR:
			l2sc->stat.rx_rnr++; /* update statistics */
			NDBGL2(L2_S_MSG, "rx'd RNR, N(R) = %d", l2sc->rxd_NR);
			i4b_next_l2state(l2sc, drv, EV_RXRNR);
			break;

		case REJ:
			l2sc->stat.rx_rej++; /* update statistics */
			NDBGL2(L2_S_MSG, "rx'd REJ, N(R) = %d", l2sc->rxd_NR);
			i4b_next_l2state(l2sc, drv, EV_RXREJ);
			break;

		default:
			l2sc->stat.err_rx_bads++; /* update statistics */
			NDBGL2(L2_S_ERR, "ERROR, unknown code, frame = ");
			i4b_print_frame(m->m_len, m->m_data);
			break;
	}
	i4b_Dfreembuf(m);
}

/*---------------------------------------------------------------------------*
 *	transmit RR command
 *---------------------------------------------------------------------------*/
void
i4b_tx_rr_command(l2_softc_t *l2sc, pbit_t pbit)
{
	struct mbuf *m;

	NDBGL2(L2_S_MSG, "tx RR, isdnif = %d", l2sc->drv->isdnif);

	m = i4b_build_s_frame(l2sc, CR_CMD_TO_NT, pbit, RR);

	l2sc->driver->ph_data_req(l2sc->l1_token, m, MBUF_FREE);

	l2sc->stat.tx_rr++; /* update statistics */
}

/*---------------------------------------------------------------------------*
 *	transmit RR response
 *---------------------------------------------------------------------------*/
void
i4b_tx_rr_response(l2_softc_t *l2sc, fbit_t fbit)
{
	struct mbuf *m;

	NDBGL2(L2_S_MSG, "tx RR, isdnif = %d", l2sc->drv->isdnif);

	m = i4b_build_s_frame(l2sc, CR_RSP_TO_NT, fbit, RR);

	l2sc->driver->ph_data_req(l2sc->l1_token, m, MBUF_FREE);

	l2sc->stat.tx_rr++; /* update statistics */
}

/*---------------------------------------------------------------------------*
 *	transmit RNR command
 *---------------------------------------------------------------------------*/
void
i4b_tx_rnr_command(l2_softc_t *l2sc, pbit_t pbit)
{
	struct mbuf *m;

	NDBGL2(L2_S_MSG, "tx RNR, isdnif = %d", l2sc->drv->isdnif);

	m = i4b_build_s_frame(l2sc, CR_CMD_TO_NT, pbit, RNR);

	l2sc->driver->ph_data_req(l2sc->l1_token, m, MBUF_FREE);

	l2sc->stat.tx_rnr++; /* update statistics */
}

/*---------------------------------------------------------------------------*
 *	transmit RNR response
 *---------------------------------------------------------------------------*/
void
i4b_tx_rnr_response(l2_softc_t *l2sc, fbit_t fbit)
{
	struct mbuf *m;

	NDBGL2(L2_S_MSG, "tx RNR, isdnif = %d", l2sc->drv->isdnif);

	m = i4b_build_s_frame(l2sc, CR_RSP_TO_NT, fbit, RNR);

	l2sc->driver->ph_data_req(l2sc->l1_token, m, MBUF_FREE);

	l2sc->stat.tx_rnr++; /* update statistics */
}

/*---------------------------------------------------------------------------*
 *	transmit REJ response
 *---------------------------------------------------------------------------*/
void
i4b_tx_rej_response(l2_softc_t *l2sc, fbit_t fbit)
{
	struct mbuf *m;

	NDBGL2(L2_S_MSG, "tx REJ, isdnif = %d", l2sc->drv->isdnif);

	m = i4b_build_s_frame(l2sc, CR_RSP_TO_NT, fbit, REJ);

	l2sc->driver->ph_data_req(l2sc->l1_token, m, MBUF_FREE);

	l2sc->stat.tx_rej++; /* update statistics */
}

/*---------------------------------------------------------------------------*
 *	build S-frame for sending
 *---------------------------------------------------------------------------*/
struct mbuf *
i4b_build_s_frame(l2_softc_t *l2sc, crbit_to_nt_t crbit, pbit_t pbit, u_char type)
{
	struct mbuf *m;

	if((m = i4b_Dgetmbuf(S_FRAME_LEN)) == NULL)
		return(NULL);

	PUTSAPI(SAPI_CCP, crbit, m->m_data[OFF_SAPI]);

	PUTTEI(l2sc->tei, m->m_data[OFF_TEI]);

	m->m_data[OFF_SRCR] = type;

	m->m_data[OFF_SNR] = (l2sc->vr << 1) | (pbit & 0x01);

	return(m);
}

#endif /* NI4BQ921 > 0 */
