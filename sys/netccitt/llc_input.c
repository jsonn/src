/*	$NetBSD: llc_input.c,v 1.11.16.2 2004/09/18 14:54:40 skrll Exp $	*/

/* 
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 * 
 * This code is derived from software contributed to Berkeley by
 * Dirk Husemann and the Computer Science Department (IV) of
 * the University of Erlangen-Nuremberg, Germany.
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
 *	@(#)llc_input.c	8.1 (Berkeley) 6/10/93
 */

/* 
 * Copyright (c) 1990, 1991, 1992
 *		Dirk Husemann, Computer Science Department IV, 
 * 		University of Erlangen-Nuremberg, Germany.
 * 
 * This code is derived from software contributed to Berkeley by
 * Dirk Husemann and the Computer Science Department (IV) of
 * the University of Erlangen-Nuremberg, Germany.
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
 *	@(#)llc_input.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: llc_input.c,v 1.11.16.2 2004/09/18 14:54:40 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_llc.h>
#include <net/route.h>

#include <netccitt/dll.h>
#include <netccitt/llc_var.h>

#include <machine/stdarg.h>

struct ifqueue llcintrq;

/*
 * This module implements LLC as specified by ISO 8802-2.
 */


/*
 * llcintr() handles all LLC frames (except ISO CLNS ones for the time being)
 *           and tries to pass them on to the appropriate network layer entity.
 */
void
llcintr()
{
	struct mbuf *m;
	int i;
	int frame_kind;
	u_char cmdrsp;
	struct llc_linkcb *linkp;
	struct npaidbentry *sapinfo = NULL;
	struct sdl_hdr *sdlhdr;
	struct llc *frame;
	long expected_len;

	struct ifnet   *ifp;
	struct rtentry *llrt;
	struct rtentry *nlrt;

	for (;;) {
		i = splnet();
		IF_DEQUEUE(&llcintrq, m);
		splx(i);
		if (m == 0)
			break;
#ifdef		DIAGNOSTIC
		if ((m->m_flags & M_PKTHDR) == 0)
			panic("llcintr no HDR");
#endif
		/*
		 * Get ifp this packet was received on
		 */
		ifp = m->m_pkthdr.rcvif;

		sdlhdr = mtod(m, struct sdl_hdr *);

		/*
		 * [Copied from net/ip_input.c]
		 *
		 * Check that the amount of data in the buffers is
		 * at least as much as the LLC header tells us.
		 * Trim mbufs if longer than expected.
		 * Drop packets if shorter than we think they are.
		 *
		 * Layout of mbuf chain at this point:
		 *
		 *  +-------------------------------+----+	-\
                 *  |  sockaddr_dl src - sdlhdr_src | 20 |	  \
                 *  +-------------------------------+----+	   |
		 *  |  sockaddr_dl dst - sdlhdr_dst | 20 |	    > sizeof(struct sdl_hdr) == 44
		 *  +-------------------------------+----+	   |
                 *  |  LLC frame len - sdlhdr_len   | 04 |	  /
		 *  +-------------------------------+----+	-/
		 * /
		 * | m_next
		 * \
                 *  +----------------------------+----+	 -\
                 *  |  llc DSAP 		 | 01 |	   \
		 *  +----------------------------+----+	    |
                 *  |  llc SSAP 		 | 01 |	    |
		 *  +----------------------------+----+	     > sdlhdr_len
                 *  |  llc control      	 | 01 |	    |
		 *  +----------------------------+----+	    |
		 *  |  ...                       |    |	   /
		 *      			      	 -/
		 *
		 * Thus the we expect to have exactly 
		 * (sdlhdr->sdlhdr_len+sizeof(struct sdl_hdr)) in the mbuf chain
		 */
		expected_len = sdlhdr->sdlhdr_len + sizeof(struct sdl_hdr);

		if (m->m_pkthdr.len < expected_len) {
			m_freem(m);
			continue;
		}
		if (m->m_pkthdr.len > expected_len) {
			if (m->m_len == m->m_pkthdr.len) {
				m->m_len = expected_len;
				m->m_pkthdr.len = expected_len;
			} else
				m_adj(m, expected_len - m->m_pkthdr.len);
		}

		/*
		 * Get llc header
		 */
		if (m->m_len > sizeof(struct sdl_hdr))
			frame = mtod((struct mbuf *)((struct sdl_hdr*)(m+1)),
			     	     struct llc *);
		else frame = mtod(m->m_next, struct llc *);
		if (frame == (struct llc *) NULL)
			panic("llcintr no llc header");

		/*
		 * Now check for bogus I/S frame, i.e. those with a control
		 * field telling us they're an I/S frame yet their length
		 * is less than the established I/S frame length (DSAP + SSAP +
		 * control + N(R)&P/F = 4) --- we drop those suckers
		 */
		if (((frame->llc_control & 0x03) != 0x03) 
		    && ((expected_len - sizeof(struct sdl_hdr)) < LLC_ISFRAMELEN)) {
			m_freem(m);
			printf("llc: hurz error\n");
			continue;
		}

		/*
		 * Get link control block for the addressed link connection.
		 * If there is none we take care of it later on.
		 */
		cmdrsp = (frame->llc_ssap & 0x01);
		frame->llc_ssap &= ~0x01;
		llrt = rtalloc1((struct sockaddr *)&sdlhdr->sdlhdr_src, 0);
		if (llrt)
			llrt->rt_refcnt--;
#ifdef notyet
		else
			llrt = npaidb_enter(&sdlhdr->sdlhdr_src, 0, 0, 0);
#endif /* notyet */
		else {
			/* 
			 * We cannot do anything currently here as we
			 * don't `know' this link --- drop it 
			 */
			m_freem(m);
			continue;
		}
		linkp = ((struct npaidbentry *)(llrt->rt_llinfo))->np_link;
		nlrt = ((struct npaidbentry *)(llrt->rt_llinfo))->np_rt;

		/*
		 * If the link is not existing right now, we can try and look up
		 * the SAP info block.
		 */
		if ((linkp == 0) && frame->llc_ssap) 
			sapinfo = llc_getsapinfo(frame->llc_dsap, ifp);

		/*
		 * Handle XID and TEST frames
		 * XID:		if DLSAP == 0, return 	type-of-services
		 *					window-0
		 *					DLSAP-0
		 *					format-identifier-?
		 * 		if DLSAP != 0, locate sapcb and return
		 *					type-of-services
		 *					SAP-window
		 *					format-identifier-?
		 * TEST:	swap (snpah_dst, snpah_src) and return frame
		 *
		 * Also toggle the CMD/RESP bit
		 *
		 * Is this behaviour correct? Check ISO 8802-2 (90)!
		 */
		frame_kind = llc_decode(frame, (struct llc_linkcb *)0);
		switch(frame_kind) {
		case LLCFT_XID:
			if (linkp || sapinfo) {
				if (linkp)
			   		frame->llc_window = linkp->llcl_window;
			   	else frame->llc_window = sapinfo->si_window;
			 	frame->llc_fid = 9;			/* XXX */
			  	frame->llc_class = sapinfo->si_class;
			 	frame->llc_ssap = frame->llc_dsap;
			} else {
			 	frame->llc_window = 0;
			     	frame->llc_fid = 9;
				frame->llc_class = 1;
				frame->llc_dsap = frame->llc_ssap = 0;
			}

			/* fall thru to */
		case LLCFT_TEST:
			sdl_swapaddr(&(mtod(m, struct sdl_hdr *)->sdlhdr_dst),
				     &(mtod(m, struct sdl_hdr *)->sdlhdr_src));

			/* Now set the CMD/RESP bit */
			frame->llc_ssap |= (cmdrsp == 0x0 ? 0x1 : 0x0);

			/* Ship it out again */
			(*ifp->if_output)(ifp, m,
					  (struct sockaddr *) &(mtod(m, struct sdl_hdr *)->sdlhdr_dst),
					  (struct rtentry *) 0);
			continue;
		}

		/*
		 * Create link control block in case it is not existing
		 */
		if (linkp == 0 && sapinfo) {
			if ((linkp = llc_newlink(&sdlhdr->sdlhdr_src, ifp, nlrt,
						     (nlrt == 0) ? 0 : nlrt->rt_llinfo,
						     llrt)) == 0) {
				printf("llcintr: couldn't create new link\n");
				m_freem(m);
				continue;
			}
			((struct npaidbentry *)llrt->rt_llinfo)->np_link = linkp;
		} else if (linkp == 0) {
			/* The link is not known to us, drop the frame and continue */
			m_freem(m);
			continue;
		}

		/*
		 * Drop SNPA header and get rid of empty mbuf at the
		 * front of the mbuf chain (I don't like 'em)
		 */
		m_adj(m, sizeof(struct sdl_hdr));
		/* 
		 * LLC_UFRAMELEN is sufficient, m_pullup() will pull up
		 * the min(m->m_len, maxprotohdr_len [=40]) thus doing
		 * the trick ...
		 */
		if ((m = m_pullup(m, LLC_UFRAMELEN)))
			/*
			 * Pass it on thru the elements of procedure
			 */
			llc_input(m, linkp, cmdrsp);
	}
	return;
}

/*
 * llc_input() --- We deal with the various incoming frames here.
 *                 Basically we (indirectly) call the appropriate
 *                 state handler function that's pointed to by
 *                 llcl_statehandler.
 * 
 *                 The statehandler returns an action code ---
 *                 further actions like 
 *                         o notify network layer
 *                         o block further sending
 *                         o deblock link
 *                         o ...
 *                 are then enacted accordingly.
 */
int
llc_input(struct mbuf *m, ...)
{
	int frame_kind;
	int pollfinal;
	int action = 0;
	struct llc *frame;
	struct llc_linkcb *linkp;
	u_int cmdrsp;
	va_list ap;

	va_start(ap, m);
	linkp = va_arg(ap, struct llc_linkcb *);
	cmdrsp = va_arg(ap, u_int);
	va_end(ap);


	if ((frame = mtod(m, struct llc *)) == (struct llc *) 0) {
		m_freem(m);
		return 0;
	}
	pollfinal = ((frame->llc_control & 0x03) == 0x03) ? 
		LLCGBITS(frame->llc_control, u_pf) :
			LLCGBITS(frame->llc_control_ext, s_pf);

	/*
	 * first decode the frame
	 */
	frame_kind = llc_decode(frame, linkp);

	switch (action = llc_statehandler(linkp, frame, frame_kind, cmdrsp, 
					  pollfinal)) {
	case LLC_DATA_INDICATION:
		m_adj(m, LLC_ISFRAMELEN);
		if ((m = m_pullup(m, NLHDRSIZEGUESS)) != NULL) {
			m->m_pkthdr.rcvif = (struct ifnet *)linkp->llcl_nlnext;
			(*linkp->llcl_sapinfo->si_input)(m, NULL, NULL, NULL);
		}
		break;
	}

	/* release mbuf if not an info frame */
	if (action != LLC_DATA_INDICATION && m)
		m_freem(m);

	/* try to get frames out ... */
	llc_start(linkp);

	return 0;
}

/*
 * This routine is called by configuration setup. It sets up a station control
 * block and notifies all registered upper level protocols.
 */
void *
llc_ctlinput(prc, addr, info)
	int prc;
	struct sockaddr *addr;
	void *info;
{
	struct ifnet *ifp = NULL;
	struct ifaddr *ifa;
	struct dll_ctlinfo *ctlinfo = (struct dll_ctlinfo *)info;
	u_char sap;
	struct dllconfig *config;
	caddr_t pcb;
	struct rtentry *nlrt;
	struct rtentry *llrt = NULL;
	struct llc_linkcb *linkp = NULL;
	int i;

	/* info must point to something valid at all times */
	if (info == 0)
		return 0;

	if (prc == PRC_IFUP || prc == PRC_IFDOWN) {
		/* we use either this set ... */
		ifa = ifa_ifwithaddr(addr);
		if (ifa == NULL)
			return (0);
		ifp = ifa->ifa_ifp;
		if (ifp == NULL)
			return (0);

		sap = ctlinfo->dlcti_lsap;
		config = ctlinfo->dlcti_cfg;
		pcb = (caddr_t) 0;
		nlrt = (struct rtentry *) 0;
	} else {
		/* or this one */
		sap = 0; 
		config = (struct dllconfig *) 0;
		pcb = ctlinfo->dlcti_pcb;
		nlrt = ctlinfo->dlcti_rt;

		if ((llrt = rtalloc1(nlrt->rt_gateway, 0)))
			llrt->rt_refcnt--;
		else return 0;

		linkp = ((struct npaidbentry *)llrt->rt_llinfo)->np_link;
	}
	
	switch (prc) {
	case PRC_IFUP:
		(void) llc_setsapinfo(ifp, addr->sa_family, sap, config);
		return 0;

	case PRC_IFDOWN: {
		struct llc_linkcb *linkp;
		struct llc_linkcb *nlinkp;
		int i;

		/*
		 * All links are accessible over the doubly linked list llccb_q
		 */
		if (!LQEMPTY) {
			/*
			 * A for-loop is not that great an idea as the linkp
			 * will get deleted by llc_timer()
			 */
			linkp = LQFIRST;
			while (LQVALID(linkp)) {
				nlinkp = LQNEXT(linkp);
				if ((linkp->llcl_if = ifp) != NULL) {
					i = splnet();
					(void)llc_statehandler(linkp, (struct llc *)0,
							       NL_DISCONNECT_REQUEST,
							       0, 1);
					splx(i);
				}
				linkp = nlinkp;
			}
		}
	}
	
	case PRC_CONNECT_REQUEST: 
		if (linkp == 0) {
			if ((linkp = llc_newlink((struct sockaddr_dl *) nlrt->rt_gateway, 
						 nlrt->rt_ifp, nlrt, 
						 pcb, llrt)) == 0)
				return (0);
			((struct npaidbentry *)llrt->rt_llinfo)->np_link = linkp;
			i = splnet();
			(void)llc_statehandler(linkp, (struct llc *) 0,
						NL_CONNECT_REQUEST, 0, 1);
			splx(i);
		}
		return ((caddr_t)linkp);
	
	case PRC_DISCONNECT_REQUEST:
		if (linkp == 0) 
			panic("no link control block!");

		i = splnet();
		(void)llc_statehandler(linkp, (struct llc *) 0,
				       NL_DISCONNECT_REQUEST, 0, 1);
		splx(i);

		/*
		 * The actual removal of the link control block is done by the
		 * cleaning neutrum (i.e. llc_timer()).
		 */
		break;
	
	case PRC_RESET_REQUEST:
		if (linkp == 0) 
			panic("no link control block!");

		i = splnet();
		(void)llc_statehandler(linkp, (struct llc *) 0,
				       NL_RESET_REQUEST, 0, 1);
		splx(i);

		break;

	}
	
	return 0;
}
