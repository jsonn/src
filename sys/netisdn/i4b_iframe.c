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
 *	i4b_iframe.c - i frame handling routines
 *	------------------------------------------
 *
 *	$Id: i4b_iframe.c,v 1.6.8.1 2005/03/04 16:53:44 skrll Exp $
 *
 * $FreeBSD$
 *
 *      last edit-date: [Fri Jan  5 11:33:47 2001]
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: i4b_iframe.c,v 1.6.8.1 2005/03/04 16:53:44 skrll Exp $");

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
#include <machine/i4b_trace.h>
#else
#include <netisdn/i4b_debug.h>
#include <netisdn/i4b_ioctl.h>
#include <netisdn/i4b_trace.h>
#endif

#include <netisdn/i4b_global.h>
#include <netisdn/i4b_l2.h>
#include <netisdn/i4b_l1l2.h>
#include <netisdn/i4b_isdnq931.h>
#include <netisdn/i4b_mbuf.h>
#include <netisdn/i4b_l2fsm.h>

/*---------------------------------------------------------------------------*
 *	process i frame
 *	implements the routine "I COMMAND" Q.921 03/93 pp 68 and pp 77
 *---------------------------------------------------------------------------*/
void
i4b_rxd_i_frame(l2_softc_t *l2sc, struct isdn_l3_driver *drv, struct mbuf *m)
{
	u_char *ptr = m->m_data;
	int nr;
	int ns;
	int p;
	int s;

	if(!((l2sc->tei_valid == TEI_VALID) &&
	     (l2sc->tei == GETTEI(*(ptr+OFF_TEI)))))
	{
		i4b_Dfreembuf(m);
		return;
	}

	if((l2sc->Q921_state != ST_MULTIFR) && (l2sc->Q921_state != ST_TIMREC))
	{
		i4b_Dfreembuf(m);
		NDBGL2(L2_I_ERR, "ERROR, state != (MF || TR)!");
		return;
	}

	s = splnet();

	l2sc->stat.rx_i++;		/* update frame count */

	nr = GETINR(*(ptr + OFF_INR));
	ns = GETINS(*(ptr + OFF_INS));
	p = GETIP(*(ptr + OFF_INR));

	i4b_rxd_ack(l2sc, drv, nr);		/* last packet ack */

	if(l2sc->own_busy)		/* own receiver busy ? */
	{
		i4b_Dfreembuf(m);	/* yes, discard information */

		if(p == 1)		/* P bit == 1 ? */
		{
			i4b_tx_rnr_response(l2sc, p); /* yes, tx RNR */
			l2sc->ack_pend = 0;	/* clear ACK pending */
		}
	}
	else	/* own receiver ready */
	{
		if(ns == l2sc->vr)	/* expected sequence number ? */
		{
			M128INC(l2sc->vr);	/* yes, update */

			l2sc->rej_excpt = 0;	/* clr reject exception */

			m_adj(m, I_HDR_LEN);	/* strip i frame header */

			l2sc->iframe_sent = 0;	/* reset i acked already */

			i4b_dl_data_ind(drv, m);	/* pass data up */

			if(!l2sc->iframe_sent)
			{
				i4b_tx_rr_response(l2sc, p); /* yes, tx RR */
				l2sc->ack_pend = 0;	/* clr ACK pending */
			}
		}
		else	/* ERROR, sequence number NOT expected */
		{
			i4b_Dfreembuf(m);	/* discard information */

			if(l2sc->rej_excpt == 1)  /* already exception ? */
			{
				if(p == 1)	/* immediate response ? */
				{
					i4b_tx_rr_response(l2sc, p); /* yes, tx RR */
					l2sc->ack_pend = 0; /* clr ack pend */
				}
			}
			else	/* not in exception cond */
			{
				l2sc->rej_excpt = 1;	/* set exception */
				i4b_tx_rej_response(l2sc, p);	/* tx REJ */
				l2sc->ack_pend = 0;	/* clr ack pending */
			}
		}
	}

	/* sequence number ranges as expected ? */

	if(i4b_l2_nr_ok(nr, l2sc->va, l2sc->vs))
	{
		if(l2sc->Q921_state == ST_TIMREC)
		{
			l2sc->va = nr;

			splx(s);

			return;
		}

		if(l2sc->peer_busy)	/* yes, other side busy ? */
		{
			l2sc->va = nr;	/* yes, update ack count */
		}
		else	/* other side ready */
		{
			if(nr == l2sc->vs)	/* count expected ? */
			{
				l2sc->va = nr;	/* update ack */
				i4b_T200_stop(l2sc);
				i4b_T203_restart(l2sc);
			}
			else
			{
				if(nr != l2sc->va)
				{
					l2sc->va = nr;
					i4b_T200_restart(l2sc);
				}
			}
		}
	}
	else
	{
		i4b_nr_error_recovery(l2sc);	/* sequence error */
		l2sc->Q921_state = ST_AW_EST;
	}

	splx(s);
}

/*---------------------------------------------------------------------------*
 *	internal I FRAME QUEUED UP routine (Q.921 03/93 p 61)
 *---------------------------------------------------------------------------*/
void
i4b_i_frame_queued_up(l2_softc_t *l2sc)
{
	struct mbuf *m;
	u_char *ptr;
	int s;

	s = splnet();

	if((l2sc->peer_busy) || (l2sc->vs == ((l2sc->va + MAX_K_VALUE) & 127)))
	{
		if(l2sc->peer_busy)
		{
			NDBGL2(L2_I_MSG, "regen IFQUP, cause: peer busy!");
		}

		if(l2sc->vs == ((l2sc->va + MAX_K_VALUE) & 127))
		{
			NDBGL2(L2_I_MSG, "regen IFQUP, cause: vs=va+k!");
		}

		/*
		 * XXX see: Q.921, page 36, 5.6.1 ".. may retransmit an I
		 * frame ...", shall we retransmit the last i frame ?
		 */

		if(!(IF_QEMPTY(&l2sc->i_queue)))
		{
			NDBGL2(L2_I_MSG, "re-scheduling IFQU call!");
			START_TIMER(l2sc->IFQU_callout, i4b_i_frame_queued_up, l2sc, IFQU_DLY);
		}
		splx(s);
		return;
	}

	IF_DEQUEUE(&l2sc->i_queue, m);    /* fetch next frame to tx */

	if(!m)
	{
		NDBGL2(L2_I_ERR, "ERROR, mbuf NULL after IF_DEQUEUE");
		splx(s);
		return;
	}

	ptr = m->m_data;

	PUTSAPI(SAPI_CCP, CR_CMD_TO_NT, *(ptr + OFF_SAPI));
	PUTTEI(l2sc->tei, *(ptr + OFF_TEI));

	*(ptr + OFF_INS) = (l2sc->vs << 1) & 0xfe; /* bit 0 = 0 */
	*(ptr + OFF_INR) = (l2sc->vr << 1) & 0xfe; /* P bit = 0 */

	l2sc->stat.tx_i++;	/* update frame counter */

	l2sc->driver->ph_data_req(l2sc->l1_token, m, MBUF_DONTFREE); /* free'd when ack'd ! */

	l2sc->iframe_sent = 1;		/* in case we ack an I frame with another I frame */

	if(l2sc->ua_num != UA_EMPTY)	/* failsafe */
	{
		NDBGL2(L2_I_ERR, "ERROR, l2sc->ua_num: %d != UA_EMPTY", l2sc->ua_num);
		i4b_print_l2var(l2sc);
		i4b_Dfreembuf(l2sc->ua_frame);
	}

	l2sc->ua_frame = m;		/* save unacked frame */
	l2sc->ua_num = l2sc->vs;	/* save frame number */

	M128INC(l2sc->vs);

	l2sc->ack_pend = 0;

	splx(s);

	if(l2sc->T200 == TIMER_IDLE)
	{
		i4b_T203_stop(l2sc);
		i4b_T200_start(l2sc);
	}
}

#endif /* NI4BQ921 > 0 */
