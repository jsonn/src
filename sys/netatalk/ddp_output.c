/*	$NetBSD: ddp_output.c,v 1.7.2.3 2004/09/21 13:36:56 skrll Exp $	 */

/*
 * Copyright (c) 1990,1991 Regents of The University of Michigan.
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation, and that the name of The University
 * of Michigan not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. This software is supplied as is without expressed or
 * implied warranties of any kind.
 *
 * This product includes software developed by the University of
 * California, Berkeley and its contributors.
 *
 *	Research Systems Unix Group
 *	The University of Michigan
 *	c/o Wesley Craig
 *	535 W. William Street
 *	Ann Arbor, Michigan
 *	+1-313-764-2278
 *	netatalk@umich.edu
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ddp_output.c,v 1.7.2.3 2004/09/21 13:36:56 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_ether.h>

#include <netinet/in.h>
#undef s_net

#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/ddp.h>
#include <netatalk/ddp_var.h>
#include <netatalk/at_extern.h>

#include <machine/stdarg.h>

int ddp_cksum = 1;

int
ddp_output(struct mbuf *m,...)
{
	struct ddpcb   *ddp;
	struct ddpehdr *deh;
	va_list         ap;

	va_start(ap, m);
	ddp = va_arg(ap, struct ddpcb *);
	va_end(ap);

	M_PREPEND(m, sizeof(struct ddpehdr), M_DONTWAIT);
	if (!m)
		return (ENOBUFS);

	deh = mtod(m, struct ddpehdr *);
	deh->deh_pad = 0;
	deh->deh_hops = 0;

	deh->deh_len = m->m_pkthdr.len;

	deh->deh_dnet = ddp->ddp_fsat.sat_addr.s_net;
	deh->deh_dnode = ddp->ddp_fsat.sat_addr.s_node;
	deh->deh_dport = ddp->ddp_fsat.sat_port;
	deh->deh_snet = ddp->ddp_lsat.sat_addr.s_net;
	deh->deh_snode = ddp->ddp_lsat.sat_addr.s_node;
	deh->deh_sport = ddp->ddp_lsat.sat_port;

	/*
         * The checksum calculation is done after all of the other bytes have
         * been filled in.
         */
	if (ddp_cksum) {
		deh->deh_sum = at_cksum(m, sizeof(int));
	} else {
		deh->deh_sum = 0;
	}
	deh->deh_bytes = htonl(deh->deh_bytes);

	return (ddp_route(m, &ddp->ddp_route));
}

u_short
at_cksum(m, skip)
	struct mbuf *m;
	int skip;
{
	u_char         *data, *end;
	u_long          cksum = 0;

	for (; m; m = m->m_next) {
		for (data = mtod(m, u_char *), end = data + m->m_len;
		    data < end; data++) {
			if (skip) {
				skip--;
				continue;
			}
			cksum = (cksum + *data) << 1;
			if (cksum & 0x00010000) {
				cksum++;
			}
			cksum &= 0x0000ffff;
		}
	}

	if (cksum == 0) {
		cksum = 0x0000ffff;
	}
	return ((u_short) cksum);
}

int
ddp_route(m, ro)
	struct mbuf *m;
	struct route *ro;
{
	struct sockaddr_at gate;
	struct elaphdr *elh;
	struct at_ifaddr *aa = NULL;
	struct ifnet   *ifp = NULL;
	u_short         net;

	if (ro->ro_rt && (ifp = ro->ro_rt->rt_ifp)) {
		net = satosat(ro->ro_rt->rt_gateway)->sat_addr.s_net;
		for (aa = at_ifaddr.tqh_first; aa; aa = aa->aa_list.tqe_next) {
			if (aa->aa_ifp == ifp &&
			    ntohs(net) >= ntohs(aa->aa_firstnet) &&
			    ntohs(net) <= ntohs(aa->aa_lastnet)) {
				break;
			}
		}
	}
	if (aa == NULL) {
		printf("ddp_route: oops\n");
		m_freem(m);
		return (EINVAL);
	}
	/*
         * There are several places in the kernel where data is added to
         * an mbuf without ensuring that the mbuf pointer is aligned.
         * This is bad for transition routing, since phase 1 and phase 2
         * packets end up poorly aligned due to the three byte elap header.
         */
	if (!(aa->aa_flags & AFA_PHASE2)) {
		M_PREPEND(m, SZ_ELAPHDR, M_DONTWAIT);
		if (!m)
			return (ENOBUFS);

		elh = mtod(m, struct elaphdr *);
		elh->el_snode = satosat(&aa->aa_addr)->sat_addr.s_node;
		elh->el_type = ELAP_DDPEXTEND;
		if (ntohs(satosat(&ro->ro_dst)->sat_addr.s_net) >=
		    ntohs(aa->aa_firstnet) &&
		    ntohs(satosat(&ro->ro_dst)->sat_addr.s_net) <=
		    ntohs(aa->aa_lastnet)) {
			elh->el_dnode = satosat(&ro->ro_dst)->sat_addr.s_node;
		} else {
			elh->el_dnode =
			    satosat(ro->ro_rt->rt_gateway)->sat_addr.s_node;
		}
	}
	if (ntohs(satosat(&ro->ro_dst)->sat_addr.s_net) >=
	    ntohs(aa->aa_firstnet) &&
	    ntohs(satosat(&ro->ro_dst)->sat_addr.s_net) <=
	    ntohs(aa->aa_lastnet)) {
		gate = *satosat(&ro->ro_dst);
	} else {
		gate = *satosat(ro->ro_rt->rt_gateway);
	}
	ro->ro_rt->rt_use++;

#if IFA_STATS
	aa->aa_ifa.ifa_data.ifad_outbytes += m->m_pkthdr.len;
#endif

	/* XXX */
	return ((*ifp->if_output) (ifp, m, (struct sockaddr *) &gate, NULL));
}
