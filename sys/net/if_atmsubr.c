/*      $NetBSD: if_atmsubr.c,v 1.18.10.2 1999/07/01 23:45:18 thorpej Exp $       */

/*
 *
 * Copyright (c) 1996 Charles D. Cranor and Washington University.
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
 *      This product includes software developed by Charles D. Cranor and 
 *	Washington University.
 * 4. The name of the author may not be used to endorse or promote products
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
 * if_atmsubr.c
 */

#include "opt_inet.h"
#include "opt_gateway.h"
#include "opt_natm.h"

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
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_atm.h>
#include <net/ethertypes.h> /* XXX: for ETHERTYPE_* */

#include <netinet/in.h>
#include <netinet/if_atm.h>

#if defined(INET) || defined(INET6)
#include <netinet/in_var.h>
#endif
#ifdef NATM
#include <netnatm/natm.h>
#endif

#define senderr(e) { error = (e); goto bad;}

/*
 * atm_output: ATM output routine
 *   inputs:
 *     "ifp" = ATM interface to output to
 *     "m0" = the packet to output
 *     "dst" = the sockaddr to send to (either IP addr, or raw VPI/VCI)
 *     "rt0" = the route to use
 *   returns: error code   [0 == ok]
 *
 *   note: special semantic: if (dst == NULL) then we assume "m" already
 *		has an atm_pseudohdr on it and just send it directly.
 *		[for native mode ATM output]   if dst is null, then
 *		rt0 must also be NULL.
 */

int
atm_output(ifp, m0, dst, rt0)
	register struct ifnet *ifp;
	struct mbuf *m0;
	struct sockaddr *dst;
	struct rtentry *rt0;
{
	u_int16_t etype = 0;			/* if using LLC/SNAP */
	int s, error = 0, sz;
	struct atm_pseudohdr atmdst, *ad;
	register struct mbuf *m = m0;
	register struct rtentry *rt;
	struct atmllc *atmllc;
	struct atmllc *llc_hdr = NULL;
	u_int32_t atm_flags;

	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING))
		senderr(ENETDOWN);
	ifp->if_lastchange = time;

	/*
	 * check route
	 */
	if ((rt = rt0) != NULL) {

		if ((rt->rt_flags & RTF_UP) == 0) { /* route went down! */
			if ((rt0 = rt = RTALLOC1(dst, 0)) != NULL)
				rt->rt_refcnt--;
			else 
				senderr(EHOSTUNREACH);
		}

		if (rt->rt_flags & RTF_GATEWAY) {
			if (rt->rt_gwroute == 0)
				goto lookup;
			if (((rt = rt->rt_gwroute)->rt_flags & RTF_UP) == 0) {
				rtfree(rt); rt = rt0;
			lookup: rt->rt_gwroute = RTALLOC1(rt->rt_gateway, 0);
				if ((rt = rt->rt_gwroute) == 0)
					senderr(EHOSTUNREACH);
			}
		}

		/* XXX: put RTF_REJECT code here if doing ATMARP */

	}

	/*
	 * check for non-native ATM traffic   (dst != NULL)
	 */
	if (dst) {
		switch (dst->sa_family) {
#ifdef INET
		case AF_INET:
#endif
#ifdef INET6
		case AF_INET6:
#endif
#if defined(INET) || defined(INET6)
# ifdef ATM_PVCEXT
			if (ifp->if_flags & IFF_POINTOPOINT) {
				/* pvc subinterface */
				struct pvcsif *pvcsif = (struct pvcsif *)ifp;
				atmdst = pvcsif->sif_aph;
				break;
			}
# endif
			if (!atmresolve(rt, m, dst, &atmdst)) {
				m = NULL; 
				/* XXX: atmresolve already free'd it */
				senderr(EHOSTUNREACH);
				/* XXX: put ATMARP stuff here */
				/* XXX: watch who frees m on failure */
			}
			if (dst->sa_family == AF_INET)
				etype = ETHERTYPE_IP;
			else
				etype = ETHERTYPE_IPV6;
			break;
#endif

		case AF_UNSPEC:
			/*
			 * XXX: bpfwrite or output from a pvc shadow if.
			 * assuming dst contains 12 bytes (atm pseudo
			 * header (4) + LLC/SNAP (8))
			 */
			bcopy(dst->sa_data, &atmdst, sizeof(atmdst));
			llc_hdr = (struct atmllc *)(dst->sa_data + sizeof(atmdst));
			break;
			
		default:
#if defined(__NetBSD__) || defined(__OpenBSD__)
			printf("%s: can't handle af%d\n", ifp->if_xname, 
			    dst->sa_family);
#elif defined(__FreeBSD__) || defined(__bsdi__)
			printf("%s%d: can't handle af%d\n", ifp->if_name, 
			    ifp->if_unit, dst->sa_family);
#endif
			senderr(EAFNOSUPPORT);
		}

		/*
		 * must add atm_pseudohdr to data
		 */
		sz = sizeof(atmdst);
		atm_flags = ATM_PH_FLAGS(&atmdst);
		if (atm_flags & ATM_PH_LLCSNAP) sz += 8; /* sizeof snap == 8 */
		M_PREPEND(m, sz, M_DONTWAIT);
		if (m == 0)
			senderr(ENOBUFS);
		ad = mtod(m, struct atm_pseudohdr *);
		*ad = atmdst;
		if (atm_flags & ATM_PH_LLCSNAP) {
			atmllc = (struct atmllc *)(ad + 1);
			bcopy(ATMLLC_HDR, atmllc->llchdr, 
						sizeof(atmllc->llchdr));
			ATM_LLC_SETTYPE(atmllc, etype); 
		}
	}

	/*
	 * Queue message on interface, and start output if interface
	 * not yet active.
	 */

	s = splimp();
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
	return (error);

bad:
	if (m)
		m_freem(m);
	return (error);
}

/*
 * Process a received ATM packet;
 * the packet is in the mbuf chain m.
 */
void
atm_input(ifp, ah, m, rxhand)
	struct ifnet *ifp;
	register struct atm_pseudohdr *ah;
	struct mbuf *m;
	void *rxhand;
{
	register struct ifqueue *inq;
	u_int16_t etype = ETHERTYPE_IP; /* default */
	int s;

	if ((ifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		return;
	}
	ifp->if_lastchange = time;
	ifp->if_ibytes += m->m_pkthdr.len;

	if (rxhand) {
#ifdef NATM
	  struct natmpcb *npcb = rxhand;
	  s = splimp();			/* in case 2 atm cards @ diff lvls */
	  npcb->npcb_inq++;			/* count # in queue */
	  splx(s);
	  schednetisr(NETISR_NATM);
	  inq = &natmintrq;
	  m->m_pkthdr.rcvif = rxhand; /* XXX: overload */
#else
	  printf("atm_input: NATM detected but not configured in kernel\n");
	  m_freem(m);
	  return;
#endif
	} else {
	  /*
	   * handle LLC/SNAP header, if present
	   */
	  if (ATM_PH_FLAGS(ah) & ATM_PH_LLCSNAP) {
	    struct atmllc *alc;
	    if (m->m_len < sizeof(*alc) && (m = m_pullup(m, sizeof(*alc))) == 0)
		  return; /* failed */
	    alc = mtod(m, struct atmllc *);
	    if (bcmp(alc, ATMLLC_HDR, 6)) {
#if defined(__NetBSD__) || defined(__OpenBSD__)
	      printf("%s: recv'd invalid LLC/SNAP frame [vp=%d,vc=%d]\n",
		  ifp->if_xname, ATM_PH_VPI(ah), ATM_PH_VCI(ah));
#elif defined(__FreeBSD__) || defined(__bsdi__)
	      printf("%s%d: recv'd invalid LLC/SNAP frame [vp=%d,vc=%d]\n",
		  ifp->if_name, ifp->if_unit, ATM_PH_VPI(ah), ATM_PH_VCI(ah));
#endif
	      m_freem(m);
              return;
	    }
	    etype = ATM_LLC_TYPE(alc);
	    m_adj(m, sizeof(*alc));
	  }

	  switch (etype) {
#ifdef INET
	  case ETHERTYPE_IP:
#ifdef GATEWAY
		  if (ipflow_fastforward(m))
			return;
#endif
		  schednetisr(NETISR_IP);
		  inq = &ipintrq;
		  break;
#endif /* INET */
#ifdef INET6
	  case ETHERTYPE_IPV6:
		  schednetisr(NETISR_IPV6);
		  inq = &ip6intrq;
		  break;
#endif
	  default:
	      m_freem(m);
	      return;
	  }
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
atm_ifattach(ifp)
	register struct ifnet *ifp;
{
	register struct ifaddr *ifa;
	register struct sockaddr_dl *sdl;

	ifp->if_type = IFT_ATM;
	ifp->if_addrlen = 0;
	ifp->if_hdrlen = 0;
	ifp->if_mtu = ATMMTU;
	ifp->if_output = atm_output;
#if 0 /* XXX XXX XXX */
	ifp->if_input = atm_input;
#endif

#if defined(__NetBSD__) || defined(__OpenBSD__)
	for (ifa = ifp->if_addrlist.tqh_first; ifa != 0;
	    ifa = ifa->ifa_list.tqe_next)
#elif defined(__FreeBSD__) && ((__FreeBSD__ > 2) || defined(_NET_IF_VAR_H_))
/*
 * for FreeBSD-3.0.  3.0-SNAP-970124 still sets -D__FreeBSD__=2!
 * XXX -- for now, use newly-introduced "net/if_var.h" as an identifier.
 * need a better way to identify 3.0.  -- kjc
 */
	for (ifa = ifp->if_addrhead.tqh_first; ifa; 
	    ifa = ifa->ifa_link.tqe_next)
#elif defined(__FreeBSD__) || defined(__bsdi__)
	for (ifa = ifp->if_addrlist; ifa; ifa = ifa->ifa_next) 
#endif

		if ((sdl = (struct sockaddr_dl *)ifa->ifa_addr) &&
		    sdl->sdl_family == AF_LINK) {
			sdl->sdl_type = IFT_ATM;
			sdl->sdl_alen = ifp->if_addrlen;
#ifdef notyet /* if using ATMARP, store hardware address using the next line */
			bcopy(ifp->hw_addr, LLADDR(sdl), ifp->if_addrlen);
#endif
			break;
		}

}

#ifdef ATM_PVCEXT

static int pvc_max_number = 16;	/* max number of PVCs */
static int pvc_number = 0;	/* pvc unit number */

struct ifnet *
pvcsif_alloc()
{
	struct pvcsif *pvcsif;

	if (pvc_number >= pvc_max_number)
		return (NULL);
	MALLOC(pvcsif, struct pvcsif *, sizeof(struct pvcsif),
	       M_DEVBUF, M_WAITOK);
	if (pvcsif == NULL)
		return (NULL);
	bzero(pvcsif, sizeof(struct pvcsif));

#ifdef __NetBSD__
	sprintf(pvcsif->sif_if.if_xname, "pvc%d", pvc_number++);
#else
	pvcsif->sif_if.if_name = "pvc";
	pvcsif->sif_if.if_unit = pvc_number++;
#endif
	return (&pvcsif->sif_if);
}
#endif /* ATM_PVCEXT */
