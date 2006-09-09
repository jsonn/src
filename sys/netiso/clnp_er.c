/*	$NetBSD: clnp_er.c,v 1.17.4.1 2006/09/09 02:59:08 rpaulo Exp $	*/

/*-
 * Copyright (c) 1991, 1993
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
 *	@(#)clnp_er.c	8.1 (Berkeley) 6/10/93
 */

/***********************************************************
		Copyright IBM Corporation 1987

                      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of IBM not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
IBM BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

/*
 * ARGO Project, Computer Sciences Dept., University of Wisconsin - Madison
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: clnp_er.c,v 1.17.4.1 2006/09/09 02:59:08 rpaulo Exp $");

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/route.h>

#include <netiso/iso.h>
#include <netiso/iso_var.h>
#include <netiso/iso_pcb.h>
#define CLNP_ER_CODES
#include <netiso/clnp.h>
#include <netiso/clnp_stat.h>
#include <netiso/argo_debug.h>
#include <netiso/tp_param.h>
#include <netiso/tp_var.h>

static const struct clnp_fixed er_template = {
	ISO8473_CLNP,		/* network identifier */
	0,			/* length */
	ISO8473_V1,		/* version */
	CLNP_TTL,		/* ttl */
	CLNP_ER,		/* type */
	0,			/* segment length msb */
	0,			/* segment length lsb */
	0,			/* checksum msb */
	0,			/* checksum lmsb */
};

/*
 * FUNCTION:		clnp_er_input
 *
 * PURPOSE:			Process an ER pdu.
 *
 * RETURNS:
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 */
void
clnp_er_input(
	struct mbuf    *m,	/* ptr to packet itself */
	struct iso_addr *src,	/* ptr to src of er */
	u_int           reason)	/* reason code of er */
{
	int             cmd = -1;

#ifdef ARGO_DEBUG
	if (argo_debug[D_CTLINPUT]) {
		printf("clnp_er_input: m %p, src %s, reason x%x\n",
		    m, clnp_iso_addrp(src), reason);
	}
#endif

	INCSTAT(cns_er_inhist[clnp_er_index(reason)]);
	switch (reason) {
	case GEN_NOREAS:
	case GEN_PROTOERR:
		break;
	case GEN_BADCSUM:
		cmd = PRC_PARAMPROB;
		break;
	case GEN_CONGEST:
		cmd = PRC_QUENCH;
		break;
	case GEN_HDRSYNTAX:
		cmd = PRC_PARAMPROB;
		break;
	case GEN_SEGNEEDED:
		cmd = PRC_MSGSIZE;
		break;
	case GEN_INCOMPLETE:
		cmd = PRC_PARAMPROB;
		break;
	case GEN_DUPOPT:
		cmd = PRC_PARAMPROB;
		break;
	case ADDR_DESTUNREACH:
		cmd = PRC_UNREACH_HOST;
		break;
	case ADDR_DESTUNKNOWN:
		cmd = PRC_UNREACH_PROTOCOL;
		break;
	case SRCRT_UNSPECERR:
	case SRCRT_SYNTAX:
	case SRCRT_UNKNOWNADDR:
	case SRCRT_BADPATH:
		cmd = PRC_UNREACH_SRCFAIL;
		break;
	case TTL_EXPTRANSIT:
		cmd = PRC_TIMXCEED_INTRANS;
		break;
	case TTL_EXPREASS:
		cmd = PRC_TIMXCEED_REASS;
		break;
	case DISC_UNSUPPOPT:
	case DISC_UNSUPPVERS:
	case DISC_UNSUPPSECURE:
	case DISC_UNSUPPSRCRT:
	case DISC_UNSUPPRECRT:
		cmd = PRC_PARAMPROB;
		break;
	case REASS_INTERFERE:
		cmd = PRC_TIMXCEED_REASS;
		break;
	}

	/*
	 *	tpclnp_ctlinput1 is called directly so that we don't
	 *	have to build an iso_sockaddr out of src.
	 */
	if (cmd >= 0)
		tpclnp_ctlinput1(cmd, src);

	m_freem(m);
}

/*
 * FUNCTION:		clnp_discard
 *
 * PURPOSE:		Discard a clnp datagram
 *
 * RETURNS:		nothing
 *
 * SIDE EFFECTS:	Will emit an ER pdu if possible
 *
 * NOTES:		This code assumes that we have previously tried to pull
 *			up the header of the datagram into one mbuf.
 */
void
clnp_discard(m, reason)
	struct mbuf    *m;	/* header of packet to discard */
	u_int           reason;	/* reason for discard */
{
#ifdef ARGO_DEBUG
	if (argo_debug[D_DISCARD]) {
		printf("clnp_discard: m %p, reason x%x\n", m, reason);
	}
#endif

	if (m != NULL) {
		if (m->m_len >= sizeof(struct clnp_fixed)) {
			struct clnp_fixed *clnp =
				mtod(m, struct clnp_fixed *);

			if (((clnp->cnf_type & CNF_TYPE) != CLNP_ER) &&
			    (clnp->cnf_type & CNF_ERR_OK)) {
				clnp_emit_er(m, reason);
				return;
			}
		}
		m_freem(m);
	}
}

/*
 * FUNCTION:		clnp_emit_er
 *
 * PURPOSE:		Send an ER pdu.
 *			The src of the of the ER pdu is the host that is sending
 *			the ER (ie. us), *not* the original destination of the
 *			packet.
 *
 * RETURNS:		nothing
 *
 * SIDE EFFECTS:
 *
 * NOTES:		Takes responsibility for freeing mbuf passed
 *			This function may be called with a packet that
 *			was created by us; in this case, do not send
 *			an ER.
 */
void
clnp_emit_er(m, reason)
	struct mbuf    *m;	/* header of packet to discard */
	u_int           reason;	/* reason for discard */
{
	struct clnp_fixed *clnp = mtod(m, struct clnp_fixed *);
	struct clnp_fixed *er;
	struct route_iso route;
	struct ifnet   *ifp;
	struct sockaddr *first_hop;
	struct iso_addr src, dst, *our_addr;
	caddr_t         hoff, hend;
	int             total_len;	/* total len of dg */
	struct iso_ifaddr *ia = 0;

#ifdef ARGO_DEBUG
	if (argo_debug[D_DISCARD]) {
		printf("clnp_emit_er: m %p, hdr len %d\n",
		       m, clnp->cnf_hdr_len);
	}
#endif

	bzero((caddr_t) & route, sizeof(route));

	/*
	 * If header length is incorrect, or entire header is not contained
	 * in this mbuf, we punt
	 */
	if ((clnp->cnf_hdr_len < CLNP_HDR_MIN) ||
	    (clnp->cnf_hdr_len > CLNP_HDR_MAX) ||
	    (clnp->cnf_hdr_len > m->m_len))
		goto bad;

	/* extract src, dest address */
	hend = (caddr_t) clnp + clnp->cnf_hdr_len;
	hoff = (caddr_t) clnp + sizeof(struct clnp_fixed);
	CLNP_EXTRACT_ADDR(dst, hoff, hend);
	if (hoff == (caddr_t) 0) {
		goto bad;
	}
	CLNP_EXTRACT_ADDR(src, hoff, hend);
	if (hoff == (caddr_t) 0) {
		goto bad;
	}
	/*
	 * Do not send ER if we generated the packet.
	 */
	if (clnp_ours(&src))
		goto bad;

	/*
	 * Trim mbuf to hold only the header. This mbuf will be the 'data' of
	 * the er pdu
	 */
	if (m->m_next != NULL) {
		m_freem(m->m_next);
		m->m_next = NULL;
	}
	if (m->m_len > clnp->cnf_hdr_len)
		m_adj(m, (int) -(m->m_len - (int) clnp->cnf_hdr_len));

	/* route er pdu: note we send pkt to src of original packet  */
	if (clnp_route(&src, &route, /* flags */ 0, &first_hop, &ia) != 0)
		goto bad;

	/* compute our address based upon firsthop/ifp */
	if (ia)
		our_addr = &ia->ia_addr.siso_addr;
	else
		goto bad;
	ifp = ia->ia_ifp;

#ifdef ARGO_DEBUG
	if (argo_debug[D_DISCARD]) {
		printf("clnp_emit_er: to %s", clnp_iso_addrp(&src));
		printf(" from %s\n", clnp_iso_addrp(our_addr));
	}
#endif

#ifdef ARGO_DEBUG
	if (argo_debug[D_DISCARD]) {
		printf("clnp_emit_er: packet routed to %s\n",
		    clnp_iso_addrp(&satosiso(first_hop)->siso_addr));
	}
#endif

	/* allocate mbuf for er pdu header: punt on no space */
	/*
	 * fixed part, two addresses and their length bytes, and a
	 * 4-byte option
	 */

	M_PREPEND(m, sizeof(struct clnp_fixed) + 4 + 1 + 1 +
			src.isoa_len + our_addr->isoa_len, M_DONTWAIT);
	if (m == 0)
		goto bad;

	er = mtod(m, struct clnp_fixed *);
	*er = er_template;

	/* setup src/dst on er pdu */
	/* NOTE REVERSAL OF SRC/DST */
	hoff = (caddr_t) er + sizeof(struct clnp_fixed);
	CLNP_INSERT_ADDR(hoff, src);
	CLNP_INSERT_ADDR(hoff, *our_addr);

	/*
	 *	TODO: if complete src rt was specified, then reverse path, and
	 *	copy into er as option.
	 */

	/* add er option */
	*hoff++ = CLNPOVAL_ERREAS;	/* code */
	*hoff++ = 2;		/* length */
	*hoff++ = reason;	/* discard reason */
	*hoff++ = 0;		/* error localization = not specified */

	/* set length */
	er->cnf_hdr_len = (u_char) (hoff - (caddr_t) er);
	total_len = m->m_pkthdr.len;
	HTOC(er->cnf_seglen_msb, er->cnf_seglen_lsb, total_len);

	/* compute checksum (on header only) */
	iso_gen_csum(m, CLNP_CKSUM_OFF, (int) er->cnf_hdr_len);

	/* trim packet if too large for interface */
	if (total_len > ifp->if_mtu)
		m_adj(m, -(total_len - ifp->if_mtu));

	/* send packet */
	INCSTAT(cns_er_outhist[clnp_er_index(reason)]);
	(void) (*ifp->if_output) (ifp, m, first_hop, route.ro_rt);
	goto done;

bad:
	m_freem(m);

done:
	/* free route if it is a temp */
	if (route.ro_rt != NULL)
		RTFREE(route.ro_rt);
}

int
clnp_er_index(u_int p)
{
	u_char *cp = clnp_er_codes + CLNP_ERRORS;
	while (cp > clnp_er_codes) {
		cp--;
		if (*cp == p)
			return (cp - clnp_er_codes);
	}
	return (CLNP_ERRORS + 1);
}
