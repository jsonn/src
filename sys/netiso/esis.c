/*	$NetBSD: esis.c,v 1.25.6.2 2002/06/20 03:49:45 nathanw Exp $	*/

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
 *	@(#)esis.c	8.3 (Berkeley) 3/20/95
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
__KERNEL_RCSID(0, "$NetBSD: esis.c,v 1.25.6.2 2002/06/20 03:49:45 nathanw Exp $");

#include "opt_iso.h"
#ifdef ISO

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/proc.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/raw_cb.h>

#include <netiso/iso.h>
#include <netiso/iso_pcb.h>
#include <netiso/iso_var.h>
#include <netiso/iso_snpac.h>
#include <netiso/clnl.h>
#include <netiso/clnp.h>
#include <netiso/clnp_stat.h>
#include <netiso/esis.h>
#include <netiso/argo_debug.h>

#include <machine/stdarg.h>

/*
 *	Global variables to esis implementation
 *
 *	esis_holding_time - the holding time (sec) parameter for outgoing pdus
 *	esis_config_time  - the frequency (sec) that hellos are generated
 *	esis_esconfig_time - suggested es configuration time placed in the ish.
 *
 */
LIST_HEAD(, rawcb) esis_pcb;
struct esis_stat esis_stat;
int             esis_sendspace = 2048;
int             esis_recvspace = 2048;
short           esis_holding_time = ESIS_HT;
short           esis_config_time = ESIS_CONFIG;
short           esis_esconfig_time = ESIS_CONFIG;
struct sockaddr_dl esis_dl = {sizeof(esis_dl), AF_LINK};

struct callout	esis_config_ch;

#define EXTEND_PACKET(m, mhdr, cp)\
	if (((m)->m_next = m_getclr(M_DONTWAIT, MT_HEADER)) == NULL) {\
		esis_stat.es_nomem++;\
		m_freem(mhdr);\
		return;\
	} else {\
		(m) = (m)->m_next;\
		(cp) = mtod((m), caddr_t);\
		(m)->m_len = 0;\
	}

/*
 * FUNCTION:		esis_init
 *
 * PURPOSE:		Initialize the kernel portion of esis protocol
 *
 * RETURNS:		nothing
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 */
void
esis_init()
{
	extern struct clnl_protosw clnl_protox[256];

	LIST_INIT(&esis_pcb);

	callout_init(&snpac_age_ch);
	callout_init(&esis_config_ch);

	callout_reset(&snpac_age_ch, hz, snpac_age, NULL);
	callout_reset(&esis_config_ch, hz, esis_config, NULL);

	clnl_protox[ISO9542_ESIS].clnl_input = esis_input;
	clnl_protox[ISO10589_ISIS].clnl_input = isis_input;
#ifdef	ISO_X25ESIS
	clnl_protox[ISO9542X25_ESIS].clnl_input = x25esis_input;
#endif				/* ISO_X25ESIS */
}

/*
 * FUNCTION:		esis_usrreq
 *
 * PURPOSE:		Handle user level esis requests
 *
 * RETURNS:		0 or appropriate errno
 *
 * SIDE EFFECTS:
 *
 */
/* ARGSUSED */
int
esis_usrreq(so, req, m, nam, control, p)
	struct socket *so;
	int req;
	struct mbuf *m, *nam, *control;
	struct proc *p;
{
	struct rawcb *rp;
	int error = 0;

	if (req == PRU_CONTROL)
		return (EOPNOTSUPP);

	rp = sotorawcb(so);
#ifdef DIAGNOSTIC
	if (req != PRU_SEND && req != PRU_SENDOOB && control)
		panic("esis_usrreq: unexpected control mbuf");
#endif
	if (rp == 0 && req != PRU_ATTACH) {
		error = EINVAL;
		goto release;
	}

	switch (req) {

	case PRU_ATTACH:
		if (rp != 0) {
			error = EISCONN;
			break;
		}
		if (p == 0 || (error = suser(p->p_ucred, &p->p_acflag))) {
			error = EACCES;
			break;
		}
		if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
			error = soreserve(so, esis_sendspace, esis_recvspace);
			if (error)
				break;
		}
		MALLOC(rp, struct rawcb *, sizeof(*rp), M_PCB, M_WAITOK);
		if (rp == 0) {
			error = ENOBUFS;
			break;
		}
		bzero(rp, sizeof(*rp));
		rp->rcb_socket = so;
		LIST_INSERT_HEAD(&esis_pcb, rp, rcb_list);
		so->so_pcb = rp;
		break;

	case PRU_SEND:
		if (control && control->m_len) {
			m_freem(control);
			m_freem(m);
			error = EINVAL;
			break;
		}
		if (nam == NULL) {
			m_freem(m);
			error = EINVAL;
			break;
		}
		/* error checking here */
		error = isis_output(m, mtod(nam, struct sockaddr_dl *));
		break;

	case PRU_SENDOOB:
		m_freem(control);
		m_freem(m);
		error = EOPNOTSUPP;
		break;

	case PRU_DETACH:
		raw_detach(rp);
		break;

	case PRU_SHUTDOWN:
		socantsendmore(so);
		break;

	case PRU_SENSE:
		/*
		 * stat: don't bother with a blocksize.
		 */
		return (0);

	default:
		error = EOPNOTSUPP;
		break;
	}

release:
	return (error);
}

/*
 * FUNCTION:		esis_input
 *
 * PURPOSE:		Process an incoming esis packet
 *
 * RETURNS:		nothing
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 */
void
#if __STDC__
esis_input(struct mbuf *m0, ...)
#else
esis_input(m0, va_alist)
	struct mbuf    *m0;
	va_dcl
#endif
{
	struct snpa_hdr *shp;	/* subnetwork header */
	struct esis_fixed *pdu = mtod(m0, struct esis_fixed *);
	int    type;
	struct ifaddr *ifa;
	va_list ap;

	va_start(ap, m0);
	shp = va_arg(ap, struct snpa_hdr *);
	va_end(ap);

	for (ifa = shp->snh_ifp->if_addrlist.tqh_first; ifa != 0;
	     ifa = ifa->ifa_list.tqe_next)
		if (ifa->ifa_addr->sa_family == AF_ISO)
			break;
	/* if we have no iso address just send it to the sockets */
	if (ifa == 0)
		goto bad;

	/*
	 *	check checksum if necessary
	 */
	if (ESIS_CKSUM_REQUIRED(pdu) &&
	    iso_check_csum(m0, (int) pdu->esis_hdr_len)) {
		esis_stat.es_badcsum++;
		goto bad;
	}
	/* check version */
	if (pdu->esis_vers != ESIS_VERSION) {
		esis_stat.es_badvers++;
		goto bad;
	}
	type = pdu->esis_type & 0x1f;
	switch (type) {
	case ESIS_ESH:
		esis_eshinput(m0, shp);
		break;

	case ESIS_ISH:
		esis_ishinput(m0, shp);
		break;

	case ESIS_RD:
		esis_rdinput(m0, shp);
		break;

	default:
		esis_stat.es_badtype++;
	}

bad:
	if (esis_pcb.lh_first != 0)
		isis_input(m0, shp);
	else
		m_freem(m0);
}

/*
 * FUNCTION:		esis_rdoutput
 *
 * PURPOSE:		Transmit a redirect pdu
 *
 * RETURNS:		nothing
 *
 * SIDE EFFECTS:
 *
 * NOTES:		Assumes there is enough space for fixed part of header,
 *			DA, BSNPA and NET in first mbuf.
 */
void
esis_rdoutput(inbound_shp, inbound_m, inbound_oidx, rd_dstnsap, rt)
	struct snpa_hdr *inbound_shp;	/* snpa hdr from incoming packet */
	struct mbuf    *inbound_m;	/* incoming pkt itself */
	struct clnp_optidx *inbound_oidx;	/* clnp options assoc with
						 * incoming pkt */
	struct iso_addr *rd_dstnsap;	/* ultimate destination of pkt */
	struct rtentry *rt;	/* snpa cache info regarding next hop of pkt */
{
	struct mbuf    *m, *m0;
	caddr_t         cp;
	struct esis_fixed *pdu;
	int             len;
	struct sockaddr_iso siso;
	struct ifnet   *ifp = inbound_shp->snh_ifp;
	struct sockaddr_dl *sdl;
	struct iso_addr *rd_gwnsap;

	if (rt->rt_flags & RTF_GATEWAY) {
		rd_gwnsap = &satosiso(rt->rt_gateway)->siso_addr;
		rt = rtalloc1(rt->rt_gateway, 0);
	} else
		rd_gwnsap = &satosiso(rt_key(rt))->siso_addr;
	if (rt == 0 || (sdl = (struct sockaddr_dl *) rt->rt_gateway) == 0 ||
	    sdl->sdl_family != AF_LINK) {
		/*
		 * maybe we should have a function that you could put in the
		 * iso_ifaddr structure which could translate iso_addrs into
		 * snpa's where there is a known mapping for that address
		 * type
		 */
		esis_stat.es_badtype++;
		return;
	}
	esis_stat.es_rdsent++;
#ifdef ARGO_DEBUG
	if (argo_debug[D_ESISOUTPUT]) {
		printf(
		    "esis_rdoutput: ifp %p (%s), ht %d, m %p, oidx %p\n",
		    ifp, ifp->if_xname, esis_holding_time,
		    inbound_m, inbound_oidx);
		printf("\tdestination: %s\n", clnp_iso_addrp(rd_dstnsap));
		printf("\tredirected toward:%s\n", clnp_iso_addrp(rd_gwnsap));
	}
#endif

	if ((m0 = m = m_gethdr(M_DONTWAIT, MT_HEADER)) == NULL) {
		esis_stat.es_nomem++;
		return;
	}
	bzero(mtod(m, caddr_t), MHLEN);

	pdu = mtod(m, struct esis_fixed *);
	cp = (caddr_t) (pdu + 1);	/* pointer arith.; 1st byte after
					 * header */
	len = sizeof(struct esis_fixed);

	/*
	 *	Build fixed part of header
	 */
	pdu->esis_proto_id = ISO9542_ESIS;
	pdu->esis_vers = ESIS_VERSION;
	pdu->esis_type = ESIS_RD;
	HTOC(pdu->esis_ht_msb, pdu->esis_ht_lsb, esis_holding_time);

	/* Insert destination address */
	(void) esis_insert_addr(&cp, &len, rd_dstnsap, m, 0);

	/* Insert the snpa of better next hop */
	*cp++ = sdl->sdl_alen;
	bcopy(LLADDR(sdl), cp, sdl->sdl_alen);
	cp += sdl->sdl_alen;
	len += (sdl->sdl_alen + 1);

	/*
	 * If the next hop is not the destination, then it ought to be an IS
	 * and it should be inserted next. Else, set the NETL to 0
	 */
	/* PHASE2 use mask from ifp of outgoing interface */
	if (!iso_addrmatch1(rd_dstnsap, rd_gwnsap)) {
#if 0
		/* this should not happen: */
		if ((nhop_sc->sc_flags & SNPA_IS) == 0) {
			printf(
		    "esis_rdoutput: next hop is not dst and not an IS\n");
			m_freem(m0);
			return;
		}
#endif
		(void) esis_insert_addr(&cp, &len, rd_gwnsap, m, 0);
	} else {
		*cp++ = 0;	/* NETL */
		len++;
	}
	m->m_len = len;

	/*
	 * PHASE2
	 * If redirect is to an IS, add an address mask. The mask to be
	 * used should be the mask present in the routing entry used to
	 * forward the original data packet.
	 */

	/*
	 * Copy Qos, priority, or security options present in original npdu
	 */
	if (inbound_oidx) {
		/* THIS CODE IS CURRENTLY (mostly) UNTESTED */
		int             optlen = 0;
		if (inbound_oidx->cni_qos_formatp)
			optlen += (inbound_oidx->cni_qos_len + 2);
		if (inbound_oidx->cni_priorp)	/* priority option is 1 byte
						 * long */
			optlen += 3;
		if (inbound_oidx->cni_securep)
			optlen += (inbound_oidx->cni_secure_len + 2);
		if (M_TRAILINGSPACE(m) < optlen) {
			EXTEND_PACKET(m, m0, cp);
			m->m_len = 0;
			/* assumes MLEN > optlen */
		}
		/* assume MLEN-len > optlen */
		/*
		 * When copying options, copy from ptr - 2 in order to grab
		 * the option code and length
		 */
		if (inbound_oidx->cni_qos_formatp) {
			bcopy(mtod(inbound_m, caddr_t) +
				inbound_oidx->cni_qos_formatp - 2,
			      cp, (unsigned) (inbound_oidx->cni_qos_len + 2));
			cp += inbound_oidx->cni_qos_len + 2;
		}
		if (inbound_oidx->cni_priorp) {
			bcopy(mtod(inbound_m, caddr_t) +
				inbound_oidx->cni_priorp - 2, cp, 3);
			cp += 3;
		}
		if (inbound_oidx->cni_securep) {
			bcopy(mtod(inbound_m, caddr_t) +
				inbound_oidx->cni_securep - 2, cp,
			      (unsigned) (inbound_oidx->cni_secure_len + 2));
			cp += inbound_oidx->cni_secure_len + 2;
		}
		m->m_len += optlen;
		len += optlen;
	}
	pdu->esis_hdr_len = m0->m_pkthdr.len = len;
	iso_gen_csum(m0, ESIS_CKSUM_OFF, (int) pdu->esis_hdr_len);

	bzero((caddr_t) & siso, sizeof(siso));
	siso.siso_family = AF_ISO;
	siso.siso_data[0] = AFI_SNA;
	siso.siso_nlen = 6 + 1;	/* should be taken from snpa_hdr */
	/* +1 is for AFI */
	bcopy(inbound_shp->snh_shost, siso.siso_data + 1, 6);
	(ifp->if_output) (ifp, m0, sisotosa(&siso), 0);
}

/*
 * FUNCTION:		esis_insert_addr
 *
 * PURPOSE:		Insert an iso_addr into a buffer
 *
 * RETURNS:		true if buffer was big enough, else false
 *
 * SIDE EFFECTS:	Increment buf & len according to size of iso_addr
 *
 * NOTES:		Plus 1 here is for length byte
 */
int
esis_insert_addr(buf, len, isoa, m, nsellen)
	caddr_t *buf;	/* ptr to buffer to put address into */
	int            *len;	/* ptr to length of buffer so far */
	struct iso_addr *isoa;	/* ptr to address */
	struct mbuf *m;/* determine if there remains space */
	int             nsellen;
{
	int    newlen, result = 0;

	isoa->isoa_len -= nsellen;
	newlen = isoa->isoa_len + 1;
	if (newlen <= M_TRAILINGSPACE(m)) {
		bcopy((caddr_t) isoa, *buf, newlen);
		*len += newlen;
		*buf += newlen;
		m->m_len += newlen;
		result = 1;
	}
	isoa->isoa_len += nsellen;
	return (result);
}

#define ESIS_EXTRACT_ADDR(d, b) { d = (struct iso_addr *)(b); b += (1 + *b); \
	    if (b > buflim) {esis_stat.es_toosmall++; goto bad;}}
#define ESIS_NEXT_OPTION(b)	{ b += (2 + b[1]); \
	    if (b > buflim) {esis_stat.es_toosmall++; goto bad;}}
int             ESHonly = 0;

/*
 * FUNCTION:		esis_eshinput
 *
 * PURPOSE:		Process an incoming ESH pdu
 *
 * RETURNS:		nothing
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 */
void
esis_eshinput(m, shp)
	struct mbuf    *m;	/* esh pdu */
	struct snpa_hdr *shp;	/* subnetwork header */
{
	struct esis_fixed *pdu = mtod(m, struct esis_fixed *);
	u_short         ht;	/* holding time */
	struct iso_addr *nsap = NULL;
	int             naddr;
	u_char         *buf = (u_char *) (pdu + 1);
	u_char         *buflim = pdu->esis_hdr_len + (u_char *) pdu;
	int             new_entry = 0;

	esis_stat.es_eshrcvd++;

	CTOH(pdu->esis_ht_msb, pdu->esis_ht_lsb, ht);

	naddr = *buf++;
	if (buf >= buflim)
		goto bad;
	if (naddr == 1) {
		ESIS_EXTRACT_ADDR(nsap, buf);
		new_entry = snpac_add(shp->snh_ifp,
				      nsap, shp->snh_shost, SNPA_ES, ht, 0);
	} else {
		int             nsellength = 0, nlen = 0;
		struct ifaddr *ifa;
		/*
		 * See if we want to compress out multiple nsaps
		 * differing only by nsel
		 */
		for (ifa = shp->snh_ifp->if_addrlist.tqh_first; ifa != 0;
		     ifa = ifa->ifa_list.tqe_next)
			if (ifa->ifa_addr->sa_family == AF_ISO) {
				nsellength =
				((struct iso_ifaddr *) ifa)->ia_addr.siso_tlen;
				break;
			}
#ifdef ARGO_DEBUG
		if (argo_debug[D_ESISINPUT]) {
			printf(
			"esis_eshinput: esh: ht %d, naddr %d nsellength %d\n",
			       ht, naddr, nsellength);
		}
#endif
		while (naddr-- > 0) {
			struct iso_addr *nsap2;
			u_char         *buf2;
			ESIS_EXTRACT_ADDR(nsap, buf);
			/*
			 * see if there is at least one more nsap in ESH
			 * differing only by nsel
			 */
			if (nsellength != 0)
				for (buf2 = buf; buf2 < buflim;) {
					ESIS_EXTRACT_ADDR(nsap2, buf2);
#ifdef ARGO_DEBUG
					if (argo_debug[D_ESISINPUT]) {
						printf(
						"esis_eshinput: comparing %s ",
						       clnp_iso_addrp(nsap));
						printf("and %s\n",
						       clnp_iso_addrp(nsap2));
					}
#endif
					if (Bcmp(nsap->isoa_genaddr,
						 nsap2->isoa_genaddr,
						 nsap->isoa_len - nsellength)
					     == 0) {
						nlen = nsellength;
						break;
					}
				}
			new_entry |= snpac_add(shp->snh_ifp,
				   nsap, shp->snh_shost, SNPA_ES, ht, nlen);
			nlen = 0;
		}
	}
#ifdef ARGO_DEBUG
	if (argo_debug[D_ESISINPUT]) {
		printf("esis_eshinput: nsap %s is %s\n",
		       clnp_iso_addrp(nsap), new_entry ? "new" : "old");
	}
#endif
	if (new_entry && (iso_systype & SNPA_IS))
		esis_shoutput(shp->snh_ifp, ESIS_ISH, esis_holding_time,
			      shp->snh_shost, 6, (struct iso_addr *) 0);
bad:
	return;
}

/*
 * FUNCTION:		esis_ishinput
 *
 * PURPOSE:		process an incoming ISH pdu
 *
 * RETURNS:
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 */
void
esis_ishinput(m, shp)
	struct mbuf    *m;	/* esh pdu */
	struct snpa_hdr *shp;	/* subnetwork header */
{
	struct esis_fixed *pdu = mtod(m, struct esis_fixed *);
	u_short         ht, newct;	/* holding time */
	struct iso_addr *nsap;	/* Network Entity Title */
	u_char *buf = (u_char *) (pdu + 1);
	u_char *buflim = pdu->esis_hdr_len + (u_char *) pdu;
	int             new_entry;

	esis_stat.es_ishrcvd++;
	CTOH(pdu->esis_ht_msb, pdu->esis_ht_lsb, ht);

#ifdef ARGO_DEBUG
	if (argo_debug[D_ESISINPUT]) {
		printf("esis_ishinput: ish: ht %d\n", ht);
	}
#endif
	if (ESHonly)
		goto bad;

	ESIS_EXTRACT_ADDR(nsap, buf);

	while (buf < buflim) {
		switch (*buf) {
		case ESISOVAL_ESCT:
			if (iso_systype & SNPA_IS)
				break;
			if (buf[1] != 2)
				goto bad;
			CTOH(buf[2], buf[3], newct);
			if ((u_short) esis_config_time != newct) {
				callout_stop(&esis_config_ch);
				esis_config_time = newct;
				esis_config(NULL);
			}
			break;

		default:
			printf("Unknown ISH option: %x\n", *buf);
		}
		ESIS_NEXT_OPTION(buf);
	}
	new_entry = snpac_add(shp->snh_ifp, nsap, shp->snh_shost, SNPA_IS,
			      ht, 0);
#ifdef ARGO_DEBUG
	if (argo_debug[D_ESISINPUT]) {
		printf("esis_ishinput: nsap %s is %s\n",
		   clnp_iso_addrp(nsap), new_entry ? "new" : "old");
	}
#endif

	if (new_entry)
		esis_shoutput(shp->snh_ifp,
			      iso_systype & SNPA_ES ? ESIS_ESH : ESIS_ISH,
		esis_holding_time, shp->snh_shost, 6, (struct iso_addr *) 0);
bad:
	return;
}

/*
 * FUNCTION:		esis_rdinput
 *
 * PURPOSE:		Process an incoming RD pdu
 *
 * RETURNS:
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 */
void
esis_rdinput(m0, shp)
	struct mbuf    *m0;	/* esh pdu */
	struct snpa_hdr *shp;	/* subnetwork header */
{
	struct esis_fixed *pdu = mtod(m0, struct esis_fixed *);
	u_short         ht;	/* holding time */
	struct iso_addr *da, *net = 0, *netmask = 0, *snpamask = 0;
	struct iso_addr *bsnpa;
	u_char *buf = (u_char *) (pdu + 1);
	u_char *buflim = pdu->esis_hdr_len + (u_char *) pdu;

	esis_stat.es_rdrcvd++;

	/* intermediate systems ignore redirects */
	if (iso_systype & SNPA_IS)
		return;
	if (ESHonly)
		return;

	CTOH(pdu->esis_ht_msb, pdu->esis_ht_lsb, ht);
	if (buf >= buflim)
		return;

	/* Extract DA */
	ESIS_EXTRACT_ADDR(da, buf);

	/* Extract better snpa */
	ESIS_EXTRACT_ADDR(bsnpa, buf);

	/* Extract NET if present */
	if (buf < buflim) {
		if (*buf == 0)
			buf++;	/* no NET present, skip NETL anyway */
		else
			ESIS_EXTRACT_ADDR(net, buf);
	}
	/* process options */
	while (buf < buflim) {
		switch (*buf) {
		case ESISOVAL_SNPAMASK:
			if (snpamask)	/* duplicate */
				return;
			snpamask = (struct iso_addr *) (buf + 1);
			break;

		case ESISOVAL_NETMASK:
			if (netmask)	/* duplicate */
				return;
			netmask = (struct iso_addr *) (buf + 1);
			break;

		default:
			printf("Unknown option in ESIS RD (0x%x)\n", buf[-1]);
		}
		ESIS_NEXT_OPTION(buf);
	}

#ifdef ARGO_DEBUG
	if (argo_debug[D_ESISINPUT]) {
		printf("esis_rdinput: rd: ht %d, da %s\n", ht,
		    clnp_iso_addrp(da));
		if (net)
			printf("\t: net %s\n", clnp_iso_addrp(net));
	}
#endif
	/*
	 * If netl is zero, then redirect is to an ES. We need to add an entry
	 * to the snpa cache for (destination, better snpa).
	 * If netl is not zero, then the redirect is to an IS. In this
	 * case, add an snpa cache entry for (net, better snpa).
	 *
	 * If the redirect is to an IS, add a route entry towards that
	 * IS.
	 */
	if (net == 0 || net->isoa_len == 0 || snpamask) {
		/* redirect to an ES */
		snpac_add(shp->snh_ifp, da,
			  bsnpa->isoa_genaddr, SNPA_ES, ht, 0);
	} else {
		snpac_add(shp->snh_ifp, net,
			  bsnpa->isoa_genaddr, SNPA_IS, ht, 0);
		snpac_addrt(shp->snh_ifp, da, net, netmask);
	}
bad:	;	/* Needed by ESIS_NEXT_OPTION */
}

/*
 * FUNCTION:		esis_config
 *
 * PURPOSE:		Report configuration
 *
 * RETURNS:
 *
 * SIDE EFFECTS:
 *
 * NOTES:		Called every esis_config_time seconds
 */
/*ARGSUSED*/
void
esis_config(v)
	void *v;
{
	struct ifnet *ifp;

	callout_reset(&esis_config_ch, hz * esis_config_time,
	    esis_config, NULL);

	/*
	 * Report configuration for each interface that - is UP - has
	 * BROADCAST capability - has an ISO address
	 */
	/*
	 * Todo: a better way would be to construct the esh or ish once and
	 * copy it out for all devices, possibly calling a method in the
	 * iso_ifaddr structure to encapsulate and transmit it.  This could
	 * work to advantage for non-broadcast media
	 */

	for (ifp = ifnet.tqh_first; ifp != 0; ifp = ifp->if_list.tqe_next) {
		if ((ifp->if_flags & IFF_UP) &&
		    (ifp->if_flags & IFF_BROADCAST)) {
			/* search for an ISO address family */
			struct ifaddr  *ifa;

			for (ifa = ifp->if_addrlist.tqh_first; ifa != 0;
			     ifa = ifa->ifa_list.tqe_next) {
				if (ifa->ifa_addr->sa_family == AF_ISO) {
					esis_shoutput(ifp,
			      iso_systype & SNPA_ES ? ESIS_ESH : ESIS_ISH,
			      esis_holding_time,
			      (caddr_t) (iso_systype & SNPA_ES ? all_is_snpa :
				     all_es_snpa), 6, (struct iso_addr *) 0);
					break;
				}
			}
		}
	}
}

/*
 * FUNCTION:		esis_shoutput
 *
 * PURPOSE:		Transmit an esh or ish pdu
 *
 * RETURNS:		nothing
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 */
void
esis_shoutput(ifp, type, ht, sn_addr, sn_len, isoa)
	struct ifnet   *ifp;
	int             type;
	short           ht;
	caddr_t         sn_addr;
	int             sn_len;
	struct iso_addr *isoa;
{
	struct mbuf    *m, *m0;
	caddr_t         cp, naddrp;
	int             naddr = 0;
	struct esis_fixed *pdu;
	struct iso_ifaddr *ia;
	int             len;
	struct sockaddr_iso siso;

	if (type == ESIS_ESH)
		esis_stat.es_eshsent++;
	else if (type == ESIS_ISH)
		esis_stat.es_ishsent++;
	else {
		printf("esis_shoutput: bad pdu type\n");
		return;
	}

#ifdef ARGO_DEBUG
	if (argo_debug[D_ESISOUTPUT]) {
		int             i;
		printf("esis_shoutput: ifp %p (%s), %s, ht %d, to: [%d] ",
		    ifp, ifp->if_xname,
		    type == ESIS_ESH ? "esh" : "ish",
		    ht, sn_len);
		for (i = 0; i < sn_len; i++)
			printf("%x%c", *(sn_addr + i),
			    i < (sn_len - 1) ? ':' : ' ');
		printf("\n");
	}
#endif

	if ((m0 = m = m_gethdr(M_DONTWAIT, MT_HEADER)) == NULL) {
		esis_stat.es_nomem++;
		return;
	}
	bzero(mtod(m, caddr_t), MHLEN);

	pdu = mtod(m, struct esis_fixed *);
	naddrp = cp = (caddr_t) (pdu + 1);
	len = sizeof(struct esis_fixed);

	/*
	 *	Build fixed part of header
	 */
	pdu->esis_proto_id = ISO9542_ESIS;
	pdu->esis_vers = ESIS_VERSION;
	pdu->esis_type = type;
	HTOC(pdu->esis_ht_msb, pdu->esis_ht_lsb, ht);

	if (type == ESIS_ESH) {
		cp++;
		len++;
	}
	m->m_len = len;
	if (isoa) {
		/*
		 * Here we are responding to a clnp packet sent to an NSAP
		 * that is ours which was sent to the MAC addr all_es's.
		 * It is possible that we did not specifically advertise this
		 * NSAP, even though it is ours, so we will respond
		 * directly to the sender that we are here.  If we do have
		 * multiple NSEL's we'll tack them on so he can compress
		 * them out.
		 */
		(void) esis_insert_addr(&cp, &len, isoa, m, 0);
		naddr = 1;
	}
	for (ia = iso_ifaddr.tqh_first; ia != 0; ia = ia->ia_list.tqe_next) {
		int nsellen = (type == ESIS_ISH ? ia->ia_addr.siso_tlen : 0);
		int n = ia->ia_addr.siso_nlen;
		struct iso_ifaddr *ia2;

		if (type == ESIS_ISH && naddr > 0)
			break;
		for (ia2 = iso_ifaddr.tqh_first; ia2 != ia;
		     ia2 = ia2->ia_list.tqe_next)
			if (Bcmp(ia->ia_addr.siso_data,
				 ia2->ia_addr.siso_data, n) == 0)
				break;
		if (ia2 != ia)
			continue;	/* Means we have previously copied
					 * this nsap */
		if (isoa && Bcmp(ia->ia_addr.siso_data,
				 isoa->isoa_genaddr, n) == 0) {
			isoa = 0;
			continue;	/* Ditto */
		}
#ifdef ARGO_DEBUG
		if (argo_debug[D_ESISOUTPUT]) {
			printf("esis_shoutput: adding NSAP %s\n",
			    clnp_iso_addrp(&ia->ia_addr.siso_addr));
		}
#endif
		if (!esis_insert_addr(&cp, &len,
				      &ia->ia_addr.siso_addr, m, nsellen)) {
			EXTEND_PACKET(m, m0, cp);
			(void) esis_insert_addr(&cp, &len,
						&ia->ia_addr.siso_addr, m,
						nsellen);
		}
		naddr++;
	}

	if (type == ESIS_ESH)
		*naddrp = naddr;
	else {
		/* add suggested es config timer option to ISH */
		if (M_TRAILINGSPACE(m) < 4) {
			printf("esis_shoutput: extending packet\n");
			EXTEND_PACKET(m, m0, cp);
		}
		*cp++ = ESISOVAL_ESCT;
		*cp++ = 2;
		HTOC(*cp, *(cp + 1), esis_esconfig_time);
		len += 4;
		m->m_len += 4;
#ifdef ARGO_DEBUG
		if (argo_debug[D_ESISOUTPUT]) {
			printf("m0 %p, m %p, data %p, len %d, cp %p\n",
			    m0, m, m->m_data, m->m_len, cp);
		}
#endif
	}

	m0->m_pkthdr.len = len;
	pdu->esis_hdr_len = len;
	iso_gen_csum(m0, ESIS_CKSUM_OFF, (int) pdu->esis_hdr_len);

	bzero((caddr_t) & siso, sizeof(siso));
	siso.siso_family = AF_ISO;
	siso.siso_data[0] = AFI_SNA;
	siso.siso_nlen = sn_len + 1;
	bcopy(sn_addr, siso.siso_data + 1, (unsigned) sn_len);
	(ifp->if_output) (ifp, m0, sisotosa(&siso), 0);
}

/*
 * FUNCTION:		isis_input
 *
 * PURPOSE:		Process an incoming isis packet
 *
 * RETURNS:		nothing
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 */
void
#if __STDC__
isis_input(struct mbuf *m0, ...)
#else
isis_input(m0, va_alist)
	struct mbuf    *m0;
	va_dcl
#endif
{
	struct snpa_hdr *shp;	/* subnetwork header */
	struct rawcb *rp, *first_rp = 0;
	struct ifnet   *ifp;
	struct mbuf    *mm;
	va_list ap;

	va_start(ap, m0);
	shp = va_arg(ap, struct snpa_hdr *);
	va_end(ap);
	ifp = shp->snh_ifp;

#ifdef ARGO_DEBUG
	if (argo_debug[D_ISISINPUT]) {
		int             i;

		printf("isis_input: pkt on ifp %p (%s): from:",
		    ifp, ifp->if_xname);
		for (i = 0; i < 6; i++)
			printf("%x%c", shp->snh_shost[i] & 0xff,
			    (i < 5) ? ':' : ' ');
		printf(" to:");
		for (i = 0; i < 6; i++)
			printf("%x%c", shp->snh_dhost[i] & 0xff, 
			    (i < 5) ? ':' : ' ');
		printf("\n");
	}
#endif
	esis_dl.sdl_alen = ifp->if_addrlen;
	esis_dl.sdl_index = ifp->if_index;
	bcopy(shp->snh_shost, (caddr_t) esis_dl.sdl_data, esis_dl.sdl_alen);
	for (rp = esis_pcb.lh_first; rp != 0; rp = rp->rcb_list.le_next) {
		if (first_rp == 0) {
			first_rp = rp;
			continue;
		}
		/* can't block at interrupt level */
		if ((mm = m_copy(m0, 0, M_COPYALL)) != NULL) {
			if (sbappendaddr(&rp->rcb_socket->so_rcv,
					 (struct sockaddr *) &esis_dl, mm,
					 (struct mbuf *) 0) != 0) {
				sorwakeup(rp->rcb_socket);
			} else {
#ifdef ARGO_DEBUG
				if (argo_debug[D_ISISINPUT]) {
					printf(
				    "Error in sbappenaddr, mm = %p\n", mm);
				}
#endif
				m_freem(mm);
			}
		}
	}
	if (first_rp && sbappendaddr(&first_rp->rcb_socket->so_rcv,
	       (struct sockaddr *) &esis_dl, m0, (struct mbuf *) 0) != 0) {
		sorwakeup(first_rp->rcb_socket);
		return;
	}
	m_freem(m0);
}

int
#if __STDC__
isis_output(struct mbuf *m, ...)
#else
isis_output(m, va_alist)
	struct mbuf    *m;
	va_dcl
#endif
{
	struct sockaddr_dl *sdl;
	struct ifnet *ifp;
	struct ifaddr  *ifa;
	struct sockaddr_iso siso;
	int             error = 0;
	unsigned        sn_len;
	va_list ap;

	va_start(ap, m);
	sdl = va_arg(ap, struct sockaddr_dl *);
	va_end(ap);

	ifa = ifa_ifwithnet((struct sockaddr *) sdl);	/* get ifp from sdl */
	if (ifa == 0) {
#ifdef ARGO_DEBUG
		if (argo_debug[D_ISISOUTPUT]) {
			printf("isis_output: interface not found\n");
		}
#endif
		error = EINVAL;
		goto release;
	}
	ifp = ifa->ifa_ifp;
	sn_len = sdl->sdl_alen;
#ifdef ARGO_DEBUG
	if (argo_debug[D_ISISOUTPUT]) {
		u_char *cp = (u_char *) LLADDR(sdl), *cplim = cp + sn_len;
		printf("isis_output: ifp %p (%s), to: ",
		    ifp, ifp->if_xname);
		while (cp < cplim) {
			printf("%x", *cp++);
			printf("%c", (cp < cplim) ? ':' : ' ');
		}
		printf("\n");
	}
#endif
	bzero((caddr_t) & siso, sizeof(siso));
	siso.siso_family = AF_ISO;	/* This convention may be useful for
					 * X.25 */
	if (sn_len == 0)
		siso.siso_nlen = 0;
	else {
		siso.siso_data[0] = AFI_SNA;
		siso.siso_nlen = sn_len + 1;
		bcopy(LLADDR(sdl), siso.siso_data + 1, sn_len);
	}
	error = (ifp->if_output) (ifp, m, sisotosa(&siso), 0);
	if (error) {
#ifdef ARGO_DEBUG
		if (argo_debug[D_ISISOUTPUT]) {
			printf("isis_output: error from if_output is %d\n",
			    error);
		}
#endif
	}
	return (error);

release:
	if (m != NULL)
		m_freem(m);
	return (error);
}


/*
 * FUNCTION:		esis_ctlinput
 *
 * PURPOSE:		Handle the PRC_IFDOWN transition
 *
 * RETURNS:		nothing
 *
 * SIDE EFFECTS:
 *
 * NOTES:		Calls snpac_flush for interface specified.
 *			The loop through iso_ifaddr is stupid because
 *			back in if_down, we knew the ifp...
 */
void *
esis_ctlinput(req, siso, dummy)
	int             req;	/* request: we handle only PRC_IFDOWN */
	struct sockaddr *siso;	/* address of ifp */
	void *dummy;
{
	struct iso_ifaddr *ia;	/* scan through interface addresses */

	/*XXX correct? */
	if (siso->sa_family != AF_ISO)
		return NULL;
	if (req == PRC_IFDOWN)
		for (ia = iso_ifaddr.tqh_first; ia != 0;
		     ia = ia->ia_list.tqe_next) {
			if (iso_addrmatch(IA_SIS(ia),
					  (struct sockaddr_iso *) siso))
				snpac_flushifp(ia->ia_ifp);
		}
	return NULL;
}

#endif /* ISO */
