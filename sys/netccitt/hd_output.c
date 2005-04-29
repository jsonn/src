/*	$NetBSD: hd_output.c,v 1.16.4.1 2005/04/29 11:29:33 kent Exp $	*/

/*
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Laboratory for Computation Vision and the Computer Science Department
 * of the University of British Columbia.
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
 *	@(#)hd_output.c	8.1 (Berkeley) 6/10/93
 */

/*
 * Copyright (c) 1984 University of British Columbia.
 *
 * This code is derived from software contributed to Berkeley by
 * the Laboratory for Computation Vision and the Computer Science Department
 * of the University of British Columbia.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)hd_output.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: hd_output.c,v 1.16.4.1 2005/04/29 11:29:33 kent Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>

#include <net/if.h>

#include <netccitt/hdlc.h>
#include <netccitt/hd_var.h>
#include <netccitt/x25.h>
#include <netccitt/pk_extern.h>

#include <machine/stdarg.h>

/*
 *      HDLC OUTPUT INTERFACE
 *
 *      This routine is called when the X.25 packet layer output routine
 *      has a information frame (iframe)  to write.   It is  also called
 *      by the input and control routines of the HDLC layer.
 */

int
hd_output(struct mbuf *m0, ...)
{
	struct hdcb *hdp;
	struct mbuf *m = m0;
	int             len;
	va_list	ap;

	va_start(ap, m0);
	hdp = va_arg(ap, struct hdcb *);
	va_end(ap);

	if (m == NULL)
		panic("hd_output");
	if ((m->m_flags & M_PKTHDR) == 0)
		panic("hd_output 2");

	if (hdp->hd_state != ABM) {
		m_freem(m);
		return 0;
	}
	/*
	 * Make room for the hdlc header either by prepending
	 * another mbuf, or by adjusting the offset and length
	 * of the first mbuf in the mbuf chain.
	 */

	M_PREPEND(m, HDHEADERLN, M_DONTWAIT);
	if (m == NULL)
		return 0;
	for (len = 0; m; m = m->m_next)
		len += m->m_len;
	m = m0;
	m->m_pkthdr.len = len;

	hd_append(&hdp->hd_txq, m);
	hd_start(hdp);
	return 0;
}

void
hd_start(hdp)
	struct hdcb *hdp;
{
	struct mbuf *m;

	/*
	 * The iframe is only transmitted if all these conditions are FALSE.
	 * The iframe remains queued (hdp->hd_txq) however and will be
	 * transmitted as soon as these conditions are cleared.
	 */

	while (!(hdp->hd_condition & (TIMER_RECOVERY_CONDITION |
				     REMOTE_RNR_CONDITION | REJ_CONDITION))) {
		if (hdp->hd_vs ==
			(hdp->hd_lastrxnr + hdp->hd_xcp->xc_lwsize) % MODULUS) {

			/*
			 * We have now exceeded the  maximum  number  of
			 * outstanding iframes. Therefore,  we must wait
			 * until  at least  one is acknowledged if this
			 * condition  is not  turned off before we are
			 * requested to write another iframe.
			 */
			hdp->hd_window_condition++;
			break;
		}
		/* hd_remove top iframe from transmit queue. */
		if ((m = hd_remove(&hdp->hd_txq)) == NULL)
			break;

		hd_send_iframe(hdp, m, POLLOFF);
	}
}

/*
 * This procedure is passed a buffer descriptor for an iframe. It builds the
 * rest of the control part of the frame and then writes it out.  It also
 * starts the  acknowledgement  timer and keeps  the iframe in the Retransmit
 * queue (Retxq) just in case  we have to do this again.
 *
 * Note: This routine is also called from hd_input.c when retransmission of old
 * frames is required.
 */
void
hd_send_iframe(hdp, buf, poll_bit)
	struct hdcb *hdp;
	struct mbuf *buf;
	int             poll_bit;
{
	struct Hdlc_iframe *iframe;
	struct mbuf    *m;

	KILL_TIMER(hdp);

	if (buf == 0) {
		printf("hd_send_iframe: zero arg\n");
#ifdef HDLCDEBUG
		hd_status(hdp);
		hd_dumptrace(hdp);
#endif
		hdp->hd_vs = (hdp->hd_vs + 7) % MODULUS;
		return;
	}
	iframe = mtod(buf, struct Hdlc_iframe *);

	iframe->hdlc_0 = 0;
	iframe->nr = hdp->hd_vr;
	iframe->pf = poll_bit;
	iframe->ns = hdp->hd_vs;
	iframe->address = ADDRESS_B;
	hdp->hd_lasttxnr = hdp->hd_vr;
	hdp->hd_rrtimer = 0;

	if (hdp->hd_vs == hdp->hd_retxqi) {
		/* Check for retransmissions. */
		/* Put iframe only once in the Retransmission queue. */
		hdp->hd_retxq[(u_char) hdp->hd_retxqi] = buf;
		hdp->hd_retxqi = (hdp->hd_retxqi + 1) % MODULUS;
		hdp->hd_iframes_out++;
	}
	hdp->hd_vs = (hdp->hd_vs + 1) % MODULUS;

	hd_trace(hdp, TX, buf);

	/* Write buffer on device. */
	m = hdp->hd_dontcopy ? buf : m_copy(buf, 0, (int) M_COPYALL);
	if (m == 0) {
		printf("hdlc: out of mbufs\n");
		return;
	}
	(*hdp->hd_output) (m, hdp);
	SET_TIMER(hdp);
}

int
hd_ifoutput(struct mbuf *m, ...)
{
	struct hdcb *hdp;
	struct ifnet *ifp;
	int             s = splnet();
	va_list	ap;

	va_start(ap, m);
	hdp = va_arg(ap, struct hdcb *);
	va_end(ap);
	ifp = hdp->hd_ifp;

	/*
	 * Queue message on interface, and start output if interface
	 * not yet active.
	 */

	if (IF_QFULL(&ifp->if_snd)) {
		IF_DROP(&ifp->if_snd);
#if 0
		printf("%s: HDLC says OK to send but queue full, may hang\n",
		    ifp->if_xname);
#endif
		m_freem(m);
	} else {
		IF_ENQUEUE(&ifp->if_snd, m);
		if ((ifp->if_flags & IFF_OACTIVE) == 0)
			(*ifp->if_start) (ifp);
	}
	splx(s);
	return 0;
}


/*
 * This routine gets control when the timer expires because we have not
 * received an acknowledgement for a iframe.
 */

void
hd_resend_iframe(hdp)
	struct hdcb *hdp;
{

	if (hdp->hd_retxcnt++ < hd_n2) {
		if (!(hdp->hd_condition & TIMER_RECOVERY_CONDITION)) {
			hdp->hd_xx = hdp->hd_vs;
			hdp->hd_condition |= TIMER_RECOVERY_CONDITION;
		}
		hdp->hd_vs = hdp->hd_lastrxnr;
		hd_send_iframe(hdp, hdp->hd_retxq[(u_char)hdp->hd_vs], POLLON);
	} else {
		/*
		 * At this point we have not received a RR even after N2
		 * retries - attempt to reset link.
		 */

		hd_initvars(hdp);
		hd_writeinternal(hdp, SABM, POLLOFF);
		hdp->hd_state = WAIT_UA;
		SET_TIMER(hdp);
		hd_message(hdp, "Timer recovery failed: link down");
		(void) pk_ctlinput(PRC_LINKDOWN,
				   (struct sockaddr *)hdp->hd_pkp, NULL);
	}
}
