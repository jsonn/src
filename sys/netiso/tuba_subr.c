/*	$NetBSD: tuba_subr.c,v 1.8.10.1 1997/07/16 18:43:49 thorpej Exp $	*/

/*
 * Copyright (c) 1992, 1993
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
 *	@(#)tuba_subr.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/errno.h>

#include <net/route.h>
#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_debug.h>

#include <netiso/argo_debug.h>
#include <netiso/iso.h>
#include <netiso/clnp.h>
#include <netiso/iso_pcb.h>
#include <netiso/iso_var.h>
#include <netiso/tuba_table.h>

#include <machine/stdarg.h>

static struct sockaddr_iso null_siso = {sizeof(null_siso), AF_ISO,};
extern int      tuba_table_size, tcp_keepidle, tcp_keepintvl, tcp_maxidle;
extern int      tcppcbcachemiss, tcppredack, tcppreddat, tcprexmtthresh;
extern struct tcpiphdr tcp_saveti;
struct inpcbtable tuba_inpcb;
struct isopcb   tuba_isopcb;

#ifndef TUBA_INPCBHASHSIZE
#define	TUBA_INPCBHASHSIZE	128
#endif
int	tuba_inpcbhashsize = TUBA_INPCBHASHSIZE;


struct addr_arg {
	int             error;
	int             offset;
	u_long          sum;
};

static void tuba_getaddr __P((struct addr_arg *, struct sockaddr_iso **,
			      u_long));
/*
 * Tuba initialization
 */
void
tuba_init()
{
#define TUBAHDRSIZE (3 /*LLC*/ + 9 /*CLNP Fixed*/ + 42 /*Addresses*/ \
		     + 6 /*CLNP Segment*/ + 20 /*TCP*/)

	in_pcbinit(&tuba_inpcb, tuba_inpcbhashsize, tuba_inpcbhashsize);
	tuba_isopcb.isop_next = tuba_isopcb.isop_prev = &tuba_isopcb;
	tuba_isopcb.isop_faddr = &tuba_isopcb.isop_sfaddr;
	tuba_isopcb.isop_laddr = &tuba_isopcb.isop_sladdr;
	if (max_protohdr < TUBAHDRSIZE)
		max_protohdr = TUBAHDRSIZE;
	if (max_linkhdr + TUBAHDRSIZE > MHLEN)
		panic("tuba_init");
}

/*
 * Calculate contribution to fudge factor for TCP checksum,
 * and coincidentally set pointer for convenience of clnp_output
 * if we are are responding when there is no isopcb around.
 */
static void
tuba_getaddr(arg, siso, index)
	register struct addr_arg *arg;
	struct sockaddr_iso **siso;
	u_long          index;
{
	register struct tuba_cache *tc;
	if (index <= tuba_table_size && (tc = tuba_table[index])) {
		if (siso)
			*siso = &tc->tc_siso;
		arg->sum += (arg->offset & 1 ? tc->tc_ssum : tc->tc_sum)
			+ (0xffff ^ index);
		arg->offset += tc->tc_siso.siso_nlen + 1;
	} else
		arg->error = 1;
}

int
tuba_output(m, tp)
	register struct mbuf *m;
	struct tcpcb   *tp;
{
	register struct tcpiphdr *n;
	struct isopcb  *isop;
	struct addr_arg arg;

	if (tp == 0 || (n = tp->t_template) == 0 ||
	    (isop = (struct isopcb *) tp->t_tuba_pcb) == 0) {
		isop = &tuba_isopcb;
		n = mtod(m, struct tcpiphdr *);
		arg.error = arg.sum = arg.offset = 0;
		tuba_getaddr(&arg, &tuba_isopcb.isop_faddr, n->ti_dst.s_addr);
		tuba_getaddr(&arg, &tuba_isopcb.isop_laddr, n->ti_src.s_addr);
		REDUCE(arg.sum, arg.sum);
		goto adjust;
	}
	if (n->ti_sum == 0) {
		arg.error = arg.sum = arg.offset = 0;
		tuba_getaddr(&arg, (struct sockaddr_iso **) 0, n->ti_dst.s_addr);
		tuba_getaddr(&arg, (struct sockaddr_iso **) 0, n->ti_src.s_addr);
		REDUCE(arg.sum, arg.sum);
		n->ti_sum = arg.sum;
		n = mtod(m, struct tcpiphdr *);
adjust:
		if (arg.error) {
			m_freem(m);
			return (EADDRNOTAVAIL);
		}
		REDUCE(n->ti_sum, n->ti_sum + (0xffff ^ arg.sum));
	}
	m->m_len -= sizeof(struct ip);
	m->m_pkthdr.len -= sizeof(struct ip);
	m->m_data += sizeof(struct ip);
	return (clnp_output(m, isop, m->m_pkthdr.len, 0));
}

void
tuba_refcnt(isop, delta)
	struct isopcb  *isop;
	int delta;
{
	register struct tuba_cache *tc;
	unsigned        index;

	if (delta != 1)
		delta = -1;
	if (isop == 0 || isop->isop_faddr == 0 || isop->isop_laddr == 0 ||
	    (delta == -1 && isop->isop_tuba_cached == 0) ||
	    (delta == 1 && isop->isop_tuba_cached != 0))
		return;
	isop->isop_tuba_cached = (delta == 1);
	if ((index = tuba_lookup(isop->isop_faddr, M_DONTWAIT)) != 0 &&
	 (tc = tuba_table[index]) != 0 && (delta == 1 || tc->tc_refcnt > 0))
		tc->tc_refcnt += delta;
	if ((index = tuba_lookup(isop->isop_laddr, M_DONTWAIT)) != 0 &&
	 (tc = tuba_table[index]) != 0 && (delta == 1 || tc->tc_refcnt > 0))
		tc->tc_refcnt += delta;
}

void
tuba_pcbdetach(v)
	void *v;
{
	struct isopcb  *isop = v;
	if (isop == 0)
		return;
	tuba_refcnt(isop, -1);
	isop->isop_socket = 0;
	iso_pcbdetach(isop);
}

/*
 * Avoid  in_pcbconnect in faked out tcp_input()
 */
int
tuba_pcbconnect(v, nam)
	void *v;
	struct mbuf    *nam;
{
	register struct inpcb *inp = v;
	register struct sockaddr_iso *siso;
	struct sockaddr_in *sin = mtod(nam, struct sockaddr_in *);
	struct tcpcb   *tp = intotcpcb(inp);
	struct isopcb  *isop = (struct isopcb *) tp->t_tuba_pcb;
	int             error;

	/* hardwire iso_pcbbind() here */
	siso = isop->isop_laddr = &isop->isop_sladdr;
	*siso = tuba_table[inp->inp_laddr.s_addr]->tc_siso;
	siso->siso_tlen = sizeof(inp->inp_lport);
	bcopy((caddr_t) & inp->inp_lport, TSEL(siso), sizeof(inp->inp_lport));

	/* hardwire in_pcbconnect() here without assigning route */
	inp->inp_fport = sin->sin_port;
	inp->inp_faddr = sin->sin_addr;

	/* reuse nam argument to call iso_pcbconnect() */
	nam->m_len = sizeof(*siso);
	siso = mtod(nam, struct sockaddr_iso *);
	*siso = tuba_table[inp->inp_faddr.s_addr]->tc_siso;
	siso->siso_tlen = sizeof(inp->inp_fport);
	bcopy((caddr_t) & inp->inp_fport, TSEL(siso), sizeof(inp->inp_fport));

	if ((error = iso_pcbconnect(isop, nam)) == 0)
		tuba_refcnt(isop, 1);
	return (error);
}

/*
 * CALLED FROM:
 * 	clnp's input routine, indirectly through the protosw.
 * FUNCTION and ARGUMENTS:
 * Take a packet (m) from clnp, strip off the clnp header
 * and do tcp input processing.
 * No return value.
 */
void
#if __STDC__
tuba_tcpinput(struct mbuf *m, ...)
#else
tuba_tcpinput(m, va_alist)
	struct mbuf    *m;	/* ptr to first mbuf of pkt */
	va_dcl
#endif
{
	unsigned long   lindex, findex;
	register struct tcpiphdr *ti;
	register struct inpcb *inp;
	caddr_t         optp = NULL;
	int             optlen = 0;
	int             len, tlen, off;
	register struct tcpcb *tp = 0;
	int             tiflags;
	struct socket  *so = NULL;
	int             todrop, acked, ourfinisacked, needoutput = 0;
	short           ostate = 0;
	int             iss = 0;
	u_long          tiwin;
	struct tcp_opt_info opti;
	struct sockaddr_iso *src, *dst;
	va_list 	ap;

	va_start(ap, m);
	src = va_arg(ap, struct sockaddr_iso *);
	dst = va_arg(ap, struct sockaddr_iso *);
	va_end(ap);

	opti.ts_present = 0;
	opti.maxseg = 0;

	if ((m->m_flags & M_PKTHDR) == 0)
		panic("tuba_tcpinput");
	/*
	 * Do some housekeeping looking up CLNP addresses.
	 * If we are out of space might as well drop the packet now.
	 */
	tcpstat.tcps_rcvtotal++;
	lindex = tuba_lookup(dst, M_DONTWAIT);
	findex = tuba_lookup(src, M_DONTWAIT);
	if (lindex == 0 || findex == 0)
		goto drop;
	/*
	 * CLNP gave us an mbuf chain WITH the clnp header pulled up,
	 * but the data pointer pushed past it.
	 */
	len = m->m_len;
	tlen = m->m_pkthdr.len;
	m->m_data -= sizeof(struct ip);
	m->m_len += sizeof(struct ip);
	m->m_pkthdr.len += sizeof(struct ip);
	m->m_flags &= ~(M_MCAST | M_BCAST);	/* XXX should do this in
						 * clnp_input */
	/*
	 * The reassembly code assumes it will be overwriting a useless
	 * part of the packet, which is why we need to have it point
	 * into the packet itself.
	 *
	 * Check to see if the data is properly alligned
	 * so that we can save copying the tcp header.
	 * This code knows way too much about the structure of mbufs!
	 */
	off = ((sizeof(long) - 1) & ((m->m_flags & M_EXT) ?
	       (m->m_data - m->m_ext.ext_buf) : (m->m_data - m->m_pktdat)));
	if (off || len < sizeof(struct tcphdr)) {
		struct mbuf    *m0 = m;

		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == 0) {
			m = m0;
			goto drop;
		}
		m->m_next = m0;
		m->m_data += max_linkhdr;
		m->m_pkthdr = m0->m_pkthdr;
		m->m_flags = m0->m_flags & M_COPYFLAGS;
		if (len < sizeof(struct tcphdr)) {
			m->m_len = 0;
			if ((m = m_pullup(m, sizeof(struct tcpiphdr))) == 0) {
				tcpstat.tcps_rcvshort++;
				return;
			}
		} else {
			bcopy(mtod(m0, caddr_t) + sizeof(struct ip),
			      mtod(m, caddr_t) + sizeof(struct ip),
			      sizeof(struct tcphdr));
			m0->m_len -= sizeof(struct tcpiphdr);
			m0->m_data += sizeof(struct tcpiphdr);
			m->m_len = sizeof(struct tcpiphdr);
		}
	}
	/*
	 * Calculate checksum of extended TCP header and data,
	 * replacing what would have been IP addresses by
	 * the IP checksum of the CLNP addresses.
	 */
	ti = mtod(m, struct tcpiphdr *);
	ti->ti_dst.s_addr = tuba_table[lindex]->tc_sum;
	if (dst->siso_nlen & 1)
		ti->ti_src.s_addr = tuba_table[findex]->tc_sum;
	else
		ti->ti_src.s_addr = tuba_table[findex]->tc_ssum;
	bzero(ti->ti_x1, sizeof ti->ti_x1);
	ti->ti_pr = ISOPROTO_TCP;
	ti->ti_len = htons((u_short) tlen);
	if ((ti->ti_sum = in_cksum(m, m->m_pkthdr.len)) != 0) {
		tcpstat.tcps_rcvbadsum++;
		goto drop;
	}
	ti->ti_src.s_addr = findex;
	ti->ti_dst.s_addr = lindex;
	/*
	 * Now include the rest of TCP input
	 */
#define TUBA_INCLUDE
#define	in_pcbconnect	tuba_pcbconnect
#define	tcb		tuba_inpcb

#include <netinet/tcp_input.c>
}

#define tcp_slowtimo	tuba_slowtimo
#define tcp_fasttimo	tuba_fasttimo

#include <netinet/tcp_timer.c>
