/*	$NetBSD: if_fddisubr.c,v 1.9.4.2 1997/02/20 16:41:07 is Exp $	*/

/*
 * Copyright (c) 1995
 *	Matt Thomas.  All rights reserved.
 * Copyright (c) 1982, 1989, 1993
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
 *	@(#)if_fddisubr.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>

#include <machine/cpu.h>

#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/if_llc.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_var.h>
#endif
#if defined(__NetBSD__)
#include <net/if_ether.h>
#endif
#include <netinet/if_ether.h>
#if defined(__FreeBSD__)
#include <netinet/if_fddi.h>
#else
#include <net/if_fddi.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#ifdef DECNET
#include <netdnet/dn.h>
#endif

#ifdef ISO
#include <netiso/argo_debug.h>
#include <netiso/iso.h>
#include <netiso/iso_var.h>
#include <netiso/iso_snpac.h>
#endif

#include "bpfilter.h"

#ifdef LLC
#include <netccitt/dll.h>
#include <netccitt/llc_var.h>
#endif

#if defined(LLC) && defined(CCITT)
extern struct ifqueue pkintrq;
#endif

#define senderr(e) { error = (e); goto bad;}

/*
 * This really should be defined in if_llc.h but in case it isn't.
 */
#ifndef llc_snap
#define	llc_snap	llc_un.type_snap
#endif

#if defined(__bsdi__) || defined(__NetBSD__)
#define	RTALLOC1(a, b)			rtalloc1(a, b)
#define	ARPRESOLVE(a, b, c, d, e, f)	arpresolve(a, b, c, d, e)
#define	TYPEHTONS(t)			(t)
#elif defined(__FreeBSD__)
#define	RTALLOC1(a, b)			rtalloc1(a, b, 0UL)
#define	ARPRESOLVE(a, b, c, d, e, f)	arpresolve(a, b, c, d, e, f)
#define	TYPEHTONS(t)			(htons(t))
#endif
/*
 * FDDI output routine.
 * Encapsulate a packet of type family for the local net.
 * Assumes that ifp is actually pointer to arpcom structure.
 */
int
fddi_output(ifp, m0, dst, rt0)
	register struct ifnet *ifp;
	struct mbuf *m0;
	struct sockaddr *dst;
	struct rtentry *rt0;
{
	u_int16_t type;
	int s, error = 0;
 	u_char edst[6];
	register struct mbuf *m = m0;
	register struct rtentry *rt;
	struct mbuf *mcopy = (struct mbuf *)0;
	register struct fddi_header *fh;
#ifndef __NetBSD__
	struct arpcom *ac = (struct arpcom *)ifp;
#endif

	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING))
		senderr(ENETDOWN);
	ifp->if_lastchange = time;
	if ((rt = rt0) != NULL) {
		if ((rt->rt_flags & RTF_UP) == 0) {
			if ((rt0 = rt = RTALLOC1(dst, 1)) != NULL)
				rt->rt_refcnt--;
			else 
				senderr(EHOSTUNREACH);
		}
		if (rt->rt_flags & RTF_GATEWAY) {
			if (rt->rt_gwroute == 0)
				goto lookup;
			if (((rt = rt->rt_gwroute)->rt_flags & RTF_UP) == 0) {
				rtfree(rt); rt = rt0;
			lookup: rt->rt_gwroute = RTALLOC1(rt->rt_gateway, 1);
				if ((rt = rt->rt_gwroute) == 0)
					senderr(EHOSTUNREACH);
			}
		}
		if (rt->rt_flags & RTF_REJECT)
			if (rt->rt_rmx.rmx_expire == 0 ||
			    time.tv_sec < rt->rt_rmx.rmx_expire)
				senderr(rt == rt0 ? EHOSTDOWN : EHOSTUNREACH);
	}
	switch (dst->sa_family) {

#ifdef INET
	case AF_INET:
#if defined(__NetBSD__)
#define SIN(x) ((struct sockaddr_in *)(x))
		if (m->m_flags & M_BCAST)
                	bcopy((caddr_t)fddibroadcastaddr, (caddr_t)edst,
				sizeof(edst));

		else if (m->m_flags & M_MCAST) {
			ETHER_MAP_IP_MULTICAST(&SIN(dst)->sin_addr,
			    (caddr_t)edst)
		} else if (!arpresolve(ifp, rt, m, dst, edst))
#else
		if (!ARPRESOLVE(ac, rt, m, dst, edst, rt0))
#endif
			return (0);	/* if not yet resolved */
		/* If broadcasting on a simplex interface, loopback a copy */
		if ((m->m_flags & M_BCAST) && (ifp->if_flags & IFF_SIMPLEX))
			mcopy = m_copy(m, 0, (int)M_COPYALL);
		type = htons(ETHERTYPE_IP);
		break;
#endif
#ifdef NS
	case AF_NS:
		type = htons(ETHERTYPE_NS);
 		bcopy((caddr_t)&(((struct sockaddr_ns *)dst)->sns_addr.x_host),
		    (caddr_t)edst, sizeof (edst));
		if (!bcmp((caddr_t)edst, (caddr_t)&ns_thishost, sizeof(edst)))
			return (looutput(ifp, m, dst, rt));
		/* If broadcasting on a simplex interface, loopback a copy */
		if ((m->m_flags & M_BCAST) && (ifp->if_flags & IFF_SIMPLEX))
			mcopy = m_copy(m, 0, (int)M_COPYALL);
		break;
#endif
#ifdef	ISO
	case AF_ISO: {
		int	snpalen;
		struct	llc *l;
		register struct sockaddr_dl *sdl;

		if (rt && (sdl = (struct sockaddr_dl *)rt->rt_gateway) &&
		    sdl->sdl_family == AF_LINK && sdl->sdl_alen > 0) {
			bcopy(LLADDR(sdl), (caddr_t)edst, sizeof(edst));
		} else if ((error =
			    iso_snparesolve(ifp, (struct sockaddr_iso *)dst,
					    (char *)edst, &snpalen)) != 0)
			goto bad; /* Not Resolved */
		/* If broadcasting on a simplex interface, loopback a copy */
		if (*edst & 1)
			m->m_flags |= (M_BCAST|M_MCAST);
		if ((m->m_flags & M_BCAST) && (ifp->if_flags & IFF_SIMPLEX) &&
		    (mcopy = m_copy(m, 0, (int)M_COPYALL))) {
			M_PREPEND(mcopy, sizeof (*fh), M_DONTWAIT);
			if (mcopy) {
				fh = mtod(mcopy, struct fddi_header *);
				bcopy((caddr_t)edst,
				      (caddr_t)fh->fddi_dhost, sizeof (edst));
#ifdef __NetBSD__
				bcopy(LLADDR(ifp->if_sadl),
				      (caddr_t)fh->fddi_shost, sizeof (edst));
#else
				bcopy((caddr_t)ac->ac_enaddr,
				      (caddr_t)fh->fddi_shost, sizeof (edst));
#endif
			}
		}
		M_PREPEND(m, 3, M_DONTWAIT);
		if (m == NULL)
			return (0);
		type = 0;
		l = mtod(m, struct llc *);
		l->llc_dsap = l->llc_ssap = LLC_ISO_LSAP;
		l->llc_control = LLC_UI;
#if defined(__FreeBSD__)
		IFDEBUG(D_ETHER)
			int i;
			printf("unoutput: sending pkt to: ");
			for (i=0; i<6; i++)
				printf("%x ", edst[i] & 0xff);
			printf("\n");
		ENDDEBUG
#endif
		} break;
#endif /* ISO */
#ifdef	LLC
/*	case AF_NSAP: */
	case AF_CCITT: {
		register struct sockaddr_dl *sdl = 
			(struct sockaddr_dl *) rt->rt_gateway;

		if (sdl && sdl->sdl_family == AF_LINK
		    && sdl->sdl_alen > 0) {
			bcopy(LLADDR(sdl), (char *)edst,
				sizeof(edst));
		} else goto bad; /* Not a link interface ? Funny ... */
		if ((ifp->if_flags & IFF_SIMPLEX) && (*edst & 1) &&
		    (mcopy = m_copy(m, 0, (int)M_COPYALL))) {
			M_PREPEND(mcopy, sizeof (*fh), M_DONTWAIT);
			if (mcopy) {
				fh = mtod(mcopy, struct fddi_header *);
				bcopy((caddr_t)edst,
				      (caddr_t)fh->fddi_dhost, sizeof (edst));
#ifdef __NetBSD__
				bcopy(LLADDR(ifp->if_sadl),
				      (caddr_t)fh->fddi_shost, sizeof (edst));
#else
				bcopy((caddr_t)ac->ac_enaddr,
				      (caddr_t)fh->fddi_shost, sizeof (edst));
#endif
				fh->fddi_fc = FDDIFC_LLC_ASYNC|FDDIFC_LLC_PRIO4;
			}
		}
		type = 0;
#ifdef LLC_DEBUG
		{
			int i;
			register struct llc *l = mtod(m, struct llc *);

			printf("fddi_output: sending LLC2 pkt to: ");
			for (i=0; i<6; i++)
				printf("%x ", edst[i] & 0xff);
			printf(" len 0x%x dsap 0x%x ssap 0x%x control 0x%x\n", 
			    m->m_pkthdr.len, l->llc_dsap & 0xff, l->llc_ssap &0xff,
			    l->llc_control & 0xff);

		}
#endif /* LLC_DEBUG */
		} break;
#endif /* LLC */	

	case AF_UNSPEC:
	{
		struct ether_header *eh;
		eh = (struct ether_header *)dst->sa_data;
 		bcopy((caddr_t)eh->ether_dhost, (caddr_t)edst, sizeof (edst));
		if (*edst & 1)
			m->m_flags |= (M_BCAST|M_MCAST);
		type = TYPEHTONS(eh->ether_type);
		break;
	}

#if NBPFILTER > 0
	case AF_IMPLINK:
	{
		fh = mtod(m, struct fddi_header *);
		error = EPROTONOSUPPORT;
		switch (fh->fddi_fc & (FDDIFC_C|FDDIFC_L|FDDIFC_F)) {
			case FDDIFC_LLC_ASYNC: {
				/* legal priorities are 0 through 7 */
				if ((fh->fddi_fc & FDDIFC_Z) > 7)
			        	goto bad;
				break;
			}
			case FDDIFC_LLC_SYNC: {
				/* FDDIFC_Z bits reserved, must be zero */
				if (fh->fddi_fc & FDDIFC_Z)
					goto bad;
				break;
			}
			case FDDIFC_SMT: {
				/* FDDIFC_Z bits must be non zero */
				if ((fh->fddi_fc & FDDIFC_Z) == 0)
					goto bad;
				break;
			}
			default: {
				/* anything else is too dangerous */
               	 		goto bad;
			}
		}
		error = 0;
		if (fh->fddi_dhost[0] & 1)
			m->m_flags |= (M_BCAST|M_MCAST);
		goto queue_it;
	}
#endif
	default:
		printf("%s: can't handle af%d\n", ifp->if_xname,
		    dst->sa_family);
		senderr(EAFNOSUPPORT);
	}

	if (mcopy)
		(void) looutput(ifp, mcopy, dst, rt);

	if (type != 0) {
		register struct llc *l;
		M_PREPEND(m, sizeof (struct llc), M_DONTWAIT);
		if (m == 0)
			senderr(ENOBUFS);
		l = mtod(m, struct llc *);
		l->llc_control = LLC_UI;
		l->llc_dsap = l->llc_ssap = LLC_SNAP_LSAP;
		l->llc_snap.org_code[0] = l->llc_snap.org_code[1] = l->llc_snap.org_code[2] = 0;
		bcopy((caddr_t) &type, (caddr_t) &l->llc_snap.ether_type,
			sizeof(u_short));
	}
	/*
	 * Add local net header.  If no space in first mbuf,
	 * allocate another.
	 */
	M_PREPEND(m, sizeof (struct fddi_header), M_DONTWAIT);
	if (m == 0)
		senderr(ENOBUFS);
	fh = mtod(m, struct fddi_header *);
	fh->fddi_fc = FDDIFC_LLC_ASYNC|FDDIFC_LLC_PRIO4;
 	bcopy((caddr_t)edst, (caddr_t)fh->fddi_dhost, sizeof (edst));
#if NBPFILTER > 0
  queue_it:
#endif
#ifdef __NetBSD__
 	bcopy(LLADDR(ifp->if_sadl), (caddr_t)fh->fddi_shost,
	    sizeof(fh->fddi_shost));
#else
 	bcopy((caddr_t)ac->ac_enaddr, (caddr_t)fh->fddi_shost,
	    sizeof(fh->fddi_shost));
#endif
	s = splimp();
	/*
	 * Queue message on interface, and start output if interface
	 * not yet active.
	 */
	if (IF_QFULL(&ifp->if_snd)) {
		IF_DROP(&ifp->if_snd);
		splx(s);
		senderr(ENOBUFS);
	}
	ifp->if_obytes += m->m_pkthdr.len;
	IF_ENQUEUE(&ifp->if_snd, m);
	if ((ifp->if_flags & IFF_OACTIVE) == 0)
		(*ifp->if_start)(ifp);
	splx(s);
	if (m->m_flags & M_MCAST)
		ifp->if_omcasts++;
	return (error);

bad:
	if (m)
		m_freem(m);
	return (error);
}

/*
 * Process a received FDDI packet;
 * the packet is in the mbuf chain m without
 * the fddi header, which is provided separately.
 */
void
fddi_input(ifp, fh, m)
	struct ifnet *ifp;
	register struct fddi_header *fh;
	struct mbuf *m;
{
	register struct ifqueue *inq;
	register struct llc *l;
#if defined(ISO) && !defined(__NetBSD__)
	struct arpcom *ac = (struct arpcom *)ifp;
#endif
	int s;

	if ((ifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		return;
	}
	ifp->if_lastchange = time;
	ifp->if_ibytes += m->m_pkthdr.len + sizeof (*fh);
	if (bcmp((caddr_t)fddibroadcastaddr, (caddr_t)fh->fddi_dhost,
	    sizeof(fddibroadcastaddr)) == 0)
		m->m_flags |= M_BCAST;
	else if (fh->fddi_dhost[0] & 1)
		m->m_flags |= M_MCAST;
	if (m->m_flags & (M_BCAST|M_MCAST))
		ifp->if_imcasts++;

	l = mtod(m, struct llc *);
	switch (l->llc_dsap) {
#if defined(INET) || defined(NS) || defined(DECNET)
	case LLC_SNAP_LSAP:
	{
		u_int16_t etype;
		if (l->llc_control != LLC_UI || l->llc_ssap != LLC_SNAP_LSAP)
			goto dropanyway;
		if (l->llc_snap.org_code[0] != 0 || l->llc_snap.org_code[1] != 0|| l->llc_snap.org_code[2] != 0)
			goto dropanyway;
		etype = ntohs(l->llc_snap.ether_type);
		m_adj(m, 8);
		switch (etype) {
#ifdef INET
		case ETHERTYPE_IP:
			schednetisr(NETISR_IP);
			inq = &ipintrq;
			break;

		case ETHERTYPE_ARP:
			schednetisr(NETISR_ARP);
			inq = &arpintrq;
			break;
#endif
#ifdef NS
		case ETHERTYPE_NS:
			schednetisr(NETISR_NS);
			inq = &nsintrq;
			break;
#endif
#ifdef DECNET
		case ETHERTYPE_DECENT:
			schednetisr(NETISR_DECNET);
			inq = &decnetintrq;
			break;
#endif
		default:
#if 0
			printf("fddi_input: unknown protocol 0x%x\n", etype);
#endif
			ifp->if_noproto++;
			goto dropanyway;
		}
		break;
	}
#endif /* INET || NS */
#ifdef	ISO
	case LLC_ISO_LSAP: 
		switch (l->llc_control) {
		case LLC_UI:
			/* LLC_UI_P forbidden in class 1 service */
			if ((l->llc_dsap == LLC_ISO_LSAP) &&
			    (l->llc_ssap == LLC_ISO_LSAP)) {
				/* LSAP for ISO */
				m->m_data += 3;		/* XXX */
				m->m_len -= 3;		/* XXX */
				m->m_pkthdr.len -= 3;	/* XXX */
				M_PREPEND(m, sizeof *fh, M_DONTWAIT);
				if (m == 0)
					return;
				*mtod(m, struct fddi_header *) = *fh;
#if defined(__FreeBSD__)
				IFDEBUG(D_ETHER)
					printf("clnp packet");
				ENDDEBUG
#endif
				schednetisr(NETISR_ISO);
				inq = &clnlintrq;
				break;
			}
			goto dropanyway;
			
		case LLC_XID:
		case LLC_XID_P:
			if(m->m_len < 6)
				goto dropanyway;
			l->llc_window = 0;
			l->llc_fid = 9;
			l->llc_class = 1;
			l->llc_dsap = l->llc_ssap = 0;
			/* Fall through to */
		case LLC_TEST:
		case LLC_TEST_P:
		{
			struct sockaddr sa;
			register struct ether_header *eh;
			int i;
			u_char c = l->llc_dsap;

			l->llc_dsap = l->llc_ssap;
			l->llc_ssap = c;
			if (m->m_flags & (M_BCAST | M_MCAST))
#ifdef __NetBSD__
				bcopy(LLADDR(ifp->if_sadl),
				      (caddr_t)fh->fddi_dhost, 6);
#else
				bcopy((caddr_t)ac->ac_enaddr,
				      (caddr_t)fh->fddi_dhost, 6);
#endif
			sa.sa_family = AF_UNSPEC;
			sa.sa_len = sizeof(sa);
			eh = (struct ether_header *)sa.sa_data;
			for (i = 0; i < 6; i++) {
				eh->ether_shost[i] = c = fh->fddi_dhost[i];
				eh->ether_dhost[i] = 
					eh->ether_dhost[i] = fh->fddi_shost[i];
				eh->ether_shost[i] = c;
			}
			eh->ether_type = 0;
			ifp->if_output(ifp, m, &sa, NULL);
			return;
		}
		default:
			m_freem(m);
			return;
		}
		break;
#endif /* ISO */
#ifdef LLC
	case LLC_X25_LSAP:
	{
		M_PREPEND(m, sizeof(struct sdl_hdr) , M_DONTWAIT);
		if (m == 0)
			return;
		if ( !sdl_sethdrif(ifp, fh->fddi_shost, LLC_X25_LSAP,
				    fh->fddi_dhost, LLC_X25_LSAP, 6, 
				    mtod(m, struct sdl_hdr *)))
			panic("ETHER cons addr failure");
		mtod(m, struct sdl_hdr *)->sdlhdr_len = m->m_pkthdr.len - sizeof(struct sdl_hdr);
#ifdef LLC_DEBUG
		printf("llc packet\n");
#endif /* LLC_DEBUG */
		schednetisr(NETISR_CCITT);
		inq = &llcintrq;
		break;
	}
#endif /* LLC */
		
	default:
#if 0
		printf("fddi_input: unknown dsap 0x%x\n", l->llc_dsap);
#endif
		ifp->if_noproto++;
	dropanyway:
		m_freem(m);
		return;
	}

	s = splimp();
	if (IF_QFULL(inq)) {
		IF_DROP(inq);
		m_freem(m);
	} else
		IF_ENQUEUE(inq, m);
	splx(s);
}
/*
 * Perform common duties while attaching to interface list
 */
void
#ifdef __NetBSD__
fddi_ifattach(ifp, lla)
	register struct ifnet *ifp;
	caddr_t lla;
{
#else
fddi_ifattach(ifp)
	register struct ifnet *ifp;
{
	register struct ifaddr *ifa;
#endif
	register struct sockaddr_dl *sdl;

	ifp->if_type = IFT_FDDI;
	ifp->if_addrlen = 6;
	ifp->if_hdrlen = 21;
	ifp->if_mtu = FDDIMTU;
	ifp->if_output = fddi_output;
#ifdef __NetBSD__
	if ((sdl = ifp->if_sadl) &&
	    sdl->sdl_family == AF_LINK) {
		sdl->sdl_type = IFT_FDDI;
		sdl->sdl_alen = ifp->if_addrlen;
		bcopy(lla, LLADDR(sdl), ifp->if_addrlen);
	}
#else
	for (ifa = ifp->if_addrlist; ifa; ifa = ifa->ifa_next)
		if ((sdl = (struct sockaddr_dl *)ifa->ifa_addr) &&
		    sdl->sdl_family == AF_LINK) {
			sdl->sdl_type = IFT_FDDI;
			sdl->sdl_alen = ifp->if_addrlen;
			bcopy((caddr_t)((struct arpcom *)ifp)->ac_enaddr,
			      LLADDR(sdl), ifp->if_addrlen);
			break;
		}
#endif
}
