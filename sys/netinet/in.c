/*	$NetBSD: in.c,v 1.26 1996/02/13 23:41:39 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1991, 1993
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
 *	@(#)in.c	8.2 (Berkeley) 11/15/93
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netinet/ip_mroute.h>
#include <netinet/igmp_var.h>

#include "ether.h"

#ifdef INET

#ifndef SUBNETSARELOCAL
#define	SUBNETSARELOCAL	1
#endif
int subnetsarelocal = SUBNETSARELOCAL;
/*
 * Return 1 if an internet address is for a ``local'' host
 * (one to which we have a connection).  If subnetsarelocal
 * is true, this includes other subnets of the local net.
 * Otherwise, it includes only the directly-connected (sub)nets.
 */
int
in_localaddr(in)
	struct in_addr in;
{
	register struct in_ifaddr *ia;

	if (subnetsarelocal) {
		for (ia = in_ifaddr.tqh_first; ia != 0; ia = ia->ia_list.tqe_next)
			if ((in.s_addr & ia->ia_netmask) == ia->ia_net)
				return (1);
	} else {
		for (ia = in_ifaddr.tqh_first; ia != 0; ia = ia->ia_list.tqe_next)
			if ((in.s_addr & ia->ia_subnetmask) == ia->ia_subnet)
				return (1);
	}
	return (0);
}

/*
 * Determine whether an IP address is in a reserved set of addresses
 * that may not be forwarded, or whether datagrams to that destination
 * may be forwarded.
 */
int
in_canforward(in)
	struct in_addr in;
{
	register u_int32_t net;

	if (IN_EXPERIMENTAL(in.s_addr) || IN_MULTICAST(in.s_addr))
		return (0);
	if (IN_CLASSA(in.s_addr)) {
		net = in.s_addr & IN_CLASSA_NET;
		if (net == 0 || net == htonl(IN_LOOPBACKNET << IN_CLASSA_NSHIFT))
			return (0);
	}
	return (1);
}

/*
 * Trim a mask in a sockaddr
 */
void
in_socktrim(ap)
	struct sockaddr_in *ap;
{
	register char *cplim = (char *) &ap->sin_addr;
	register char *cp = (char *) (&ap->sin_addr + 1);

	ap->sin_len = 0;
	while (--cp >= cplim)
		if (*cp) {
			(ap)->sin_len = cp - (char *) (ap) + 1;
			break;
		}
}

int	in_interfaces;		/* number of external internet interfaces */

/*
 * Generic internet control operations (ioctl's).
 * Ifp is 0 if not an interface-specific ioctl.
 */
/* ARGSUSED */
int
in_control(so, cmd, data, ifp)
	struct socket *so;
	u_long cmd;
	caddr_t data;
	register struct ifnet *ifp;
{
	register struct ifreq *ifr = (struct ifreq *)data;
	register struct in_ifaddr *ia = 0;
	struct in_aliasreq *ifra = (struct in_aliasreq *)data;
	struct sockaddr_in oldaddr;
	int error, hostIsNew, maskIsNew;

	/*
	 * Find address for this interface, if it exists.
	 */
	if (ifp)
		for (ia = in_ifaddr.tqh_first; ia; ia = ia->ia_list.tqe_next)
			if (ia->ia_ifp == ifp)
				break;

	switch (cmd) {

	case SIOCAIFADDR:
	case SIOCDIFADDR:
		if (ifra->ifra_addr.sin_family == AF_INET)
		    for (; ia != 0; ia = ia->ia_list.tqe_next) {
			if (ia->ia_ifp == ifp  &&
			    ia->ia_addr.sin_addr.s_addr ==
				ifra->ifra_addr.sin_addr.s_addr)
			    break;
		}
		if (cmd == SIOCDIFADDR && ia == 0)
			return (EADDRNOTAVAIL);
		/* FALLTHROUGH */
	case SIOCSIFADDR:
	case SIOCSIFNETMASK:
	case SIOCSIFDSTADDR:
		if ((so->so_state & SS_PRIV) == 0)
			return (EPERM);

		if (ifp == 0)
			panic("in_control");
		if (ia == (struct in_ifaddr *)0) {
			ia = (struct in_ifaddr *)
				malloc(sizeof *ia, M_IFADDR, M_WAITOK);
			if (ia == (struct in_ifaddr *)0)
				return (ENOBUFS);
			bzero((caddr_t)ia, sizeof *ia);
			TAILQ_INSERT_TAIL(&in_ifaddr, ia, ia_list);
			TAILQ_INSERT_TAIL(&ifp->if_addrlist, (struct ifaddr *)ia,
			    ifa_list);
			ia->ia_ifa.ifa_addr = sintosa(&ia->ia_addr);
			ia->ia_ifa.ifa_dstaddr = sintosa(&ia->ia_dstaddr);
			ia->ia_ifa.ifa_netmask = sintosa(&ia->ia_sockmask);
			ia->ia_sockmask.sin_len = 8;
			if (ifp->if_flags & IFF_BROADCAST) {
				ia->ia_broadaddr.sin_len = sizeof(ia->ia_addr);
				ia->ia_broadaddr.sin_family = AF_INET;
			}
			ia->ia_ifp = ifp;
			LIST_INIT(&ia->ia_multiaddrs);
			if ((ifp->if_flags & IFF_LOOPBACK) == 0)
				in_interfaces++;
		}
		break;

	case SIOCSIFBRDADDR:
		if ((so->so_state & SS_PRIV) == 0)
			return (EPERM);
		/* FALLTHROUGH */

	case SIOCGIFADDR:
	case SIOCGIFNETMASK:
	case SIOCGIFDSTADDR:
	case SIOCGIFBRDADDR:
		if (ia == (struct in_ifaddr *)0)
			return (EADDRNOTAVAIL);
		break;
	}
	switch (cmd) {

	case SIOCGIFADDR:
		*satosin(&ifr->ifr_addr) = ia->ia_addr;
		break;

	case SIOCGIFBRDADDR:
		if ((ifp->if_flags & IFF_BROADCAST) == 0)
			return (EINVAL);
		*satosin(&ifr->ifr_dstaddr) = ia->ia_broadaddr;
		break;

	case SIOCGIFDSTADDR:
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
			return (EINVAL);
		*satosin(&ifr->ifr_dstaddr) = ia->ia_dstaddr;
		break;

	case SIOCGIFNETMASK:
		*satosin(&ifr->ifr_addr) = ia->ia_sockmask;
		break;

	case SIOCSIFDSTADDR:
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
			return (EINVAL);
		oldaddr = ia->ia_dstaddr;
		ia->ia_dstaddr = *satosin(&ifr->ifr_dstaddr);
		if (ifp->if_ioctl && (error = (*ifp->if_ioctl)
					(ifp, SIOCSIFDSTADDR, (caddr_t)ia))) {
			ia->ia_dstaddr = oldaddr;
			return (error);
		}
		if (ia->ia_flags & IFA_ROUTE) {
			ia->ia_ifa.ifa_dstaddr = sintosa(&oldaddr);
			rtinit(&(ia->ia_ifa), (int)RTM_DELETE, RTF_HOST);
			ia->ia_ifa.ifa_dstaddr = sintosa(&ia->ia_dstaddr);
			rtinit(&(ia->ia_ifa), (int)RTM_ADD, RTF_HOST|RTF_UP);
		}
		break;

	case SIOCSIFBRDADDR:
		if ((ifp->if_flags & IFF_BROADCAST) == 0)
			return (EINVAL);
		ia->ia_broadaddr = *satosin(&ifr->ifr_broadaddr);
		break;

	case SIOCSIFADDR:
		return (in_ifinit(ifp, ia, satosin(&ifr->ifr_addr), 1));

	case SIOCSIFNETMASK:
		ia->ia_subnetmask = ia->ia_sockmask.sin_addr.s_addr =
		    ifra->ifra_addr.sin_addr.s_addr;
		break;

	case SIOCAIFADDR:
		maskIsNew = 0;
		hostIsNew = 1;
		error = 0;
		if (ia->ia_addr.sin_family == AF_INET) {
			if (ifra->ifra_addr.sin_len == 0) {
				ifra->ifra_addr = ia->ia_addr;
				hostIsNew = 0;
			} else if (ifra->ifra_addr.sin_addr.s_addr ==
					       ia->ia_addr.sin_addr.s_addr)
				hostIsNew = 0;
		}
		if (ifra->ifra_mask.sin_len) {
			in_ifscrub(ifp, ia);
			ia->ia_sockmask = ifra->ifra_mask;
			ia->ia_subnetmask = ia->ia_sockmask.sin_addr.s_addr;
			maskIsNew = 1;
		}
		if ((ifp->if_flags & IFF_POINTOPOINT) &&
		    (ifra->ifra_dstaddr.sin_family == AF_INET)) {
			in_ifscrub(ifp, ia);
			ia->ia_dstaddr = ifra->ifra_dstaddr;
			maskIsNew  = 1; /* We lie; but the effect's the same */
		}
		if (ifra->ifra_addr.sin_family == AF_INET &&
		    (hostIsNew || maskIsNew))
			error = in_ifinit(ifp, ia, &ifra->ifra_addr, 0);
		if ((ifp->if_flags & IFF_BROADCAST) &&
		    (ifra->ifra_broadaddr.sin_family == AF_INET))
			ia->ia_broadaddr = ifra->ifra_broadaddr;
		return (error);

	case SIOCDIFADDR:
		in_ifscrub(ifp, ia);
		TAILQ_REMOVE(&ifp->if_addrlist, (struct ifaddr *)ia, ifa_list);
		TAILQ_REMOVE(&in_ifaddr, ia, ia_list);
		IFAFREE((&ia->ia_ifa));
		break;

#ifdef MROUTING
	case SIOCGETVIFCNT:
	case SIOCGETSGCNT:
		return (mrt_ioctl(cmd, data));
#endif /* MROUTING */

	default:
		if (ifp == 0 || ifp->if_ioctl == 0)
			return (EOPNOTSUPP);
		return ((*ifp->if_ioctl)(ifp, cmd, data));
	}
	return (0);
}

/*
 * Delete any existing route for an interface.
 */
void
in_ifscrub(ifp, ia)
	register struct ifnet *ifp;
	register struct in_ifaddr *ia;
{

	if ((ia->ia_flags & IFA_ROUTE) == 0)
		return;
	if (ifp->if_flags & (IFF_LOOPBACK|IFF_POINTOPOINT))
		rtinit(&(ia->ia_ifa), (int)RTM_DELETE, RTF_HOST);
	else
		rtinit(&(ia->ia_ifa), (int)RTM_DELETE, 0);
	ia->ia_flags &= ~IFA_ROUTE;
}

/*
 * Initialize an interface's internet address
 * and routing table entry.
 */
int
in_ifinit(ifp, ia, sin, scrub)
	register struct ifnet *ifp;
	register struct in_ifaddr *ia;
	struct sockaddr_in *sin;
	int scrub;
{
	register u_int32_t i = sin->sin_addr.s_addr;
	struct sockaddr_in oldaddr;
	int s = splimp(), flags = RTF_UP, error;

	oldaddr = ia->ia_addr;
	ia->ia_addr = *sin;
	/*
	 * Give the interface a chance to initialize
	 * if this is its first address,
	 * and to validate the address if necessary.
	 */
	if (ifp->if_ioctl &&
	    (error = (*ifp->if_ioctl)(ifp, SIOCSIFADDR, (caddr_t)ia))) {
		splx(s);
		ia->ia_addr = oldaddr;
		return (error);
	}
	splx(s);
	if (scrub) {
		ia->ia_ifa.ifa_addr = sintosa(&oldaddr);
		in_ifscrub(ifp, ia);
		ia->ia_ifa.ifa_addr = sintosa(&ia->ia_addr);
	}
	if (IN_CLASSA(i))
		ia->ia_netmask = IN_CLASSA_NET;
	else if (IN_CLASSB(i))
		ia->ia_netmask = IN_CLASSB_NET;
	else
		ia->ia_netmask = IN_CLASSC_NET;
	/*
	 * The subnet mask usually includes at least the standard network part,
	 * but may may be smaller in the case of supernetting.
	 * If it is set, we believe it.
	 */
	if (ia->ia_subnetmask == 0) {
		ia->ia_subnetmask = ia->ia_netmask;
		ia->ia_sockmask.sin_addr.s_addr = ia->ia_subnetmask;
	} else
		ia->ia_netmask &= ia->ia_subnetmask;
	ia->ia_net = i & ia->ia_netmask;
	ia->ia_subnet = i & ia->ia_subnetmask;
	in_socktrim(&ia->ia_sockmask);
	/*
	 * Add route for the network.
	 */
	ia->ia_ifa.ifa_metric = ifp->if_metric;
	if (ifp->if_flags & IFF_BROADCAST) {
		ia->ia_broadaddr.sin_addr.s_addr =
			ia->ia_subnet | ~ia->ia_subnetmask;
		ia->ia_netbroadcast.s_addr =
			ia->ia_net | ~ia->ia_netmask;
	} else if (ifp->if_flags & IFF_LOOPBACK) {
		ia->ia_ifa.ifa_dstaddr = ia->ia_ifa.ifa_addr;
		flags |= RTF_HOST;
	} else if (ifp->if_flags & IFF_POINTOPOINT) {
		if (ia->ia_dstaddr.sin_family != AF_INET)
			return (0);
		flags |= RTF_HOST;
	}
	if ((error = rtinit(&(ia->ia_ifa), (int)RTM_ADD, flags)) == 0)
		ia->ia_flags |= IFA_ROUTE;
	/*
	 * If the interface supports multicast, join the "all hosts"
	 * multicast group on that interface.
	 */
	if (ifp->if_flags & IFF_MULTICAST) {
		struct in_addr addr;

		addr.s_addr = INADDR_ALLHOSTS_GROUP;
		in_addmulti(&addr, ifp);
	}
	return (error);
}


/*
 * Return 1 if the address might be a local broadcast address.
 */
int
in_broadcast(in, ifp)
	struct in_addr in;
	struct ifnet *ifp;
{
	register struct ifaddr *ifa;

	if (in.s_addr == INADDR_BROADCAST ||
	    in.s_addr == INADDR_ANY)
		return 1;
	if ((ifp->if_flags & IFF_BROADCAST) == 0)
		return 0;
	/*
	 * Look through the list of addresses for a match
	 * with a broadcast address.
	 */
#define ia (ifatoia(ifa))
	for (ifa = ifp->if_addrlist.tqh_first; ifa; ifa = ifa->ifa_list.tqe_next)
		if (ifa->ifa_addr->sa_family == AF_INET &&
		    (in.s_addr == ia->ia_broadaddr.sin_addr.s_addr ||
		     in.s_addr == ia->ia_netbroadcast.s_addr ||
		     /*
		      * Check for old-style (host 0) broadcast.
		      */
		     in.s_addr == ia->ia_subnet ||
		     in.s_addr == ia->ia_net))
			    return 1;
	return (0);
#undef ia
}

/*
 * Add an address to the list of IP multicast addresses for a given interface.
 */
struct in_multi *
in_addmulti(ap, ifp)
	register struct in_addr *ap;
	register struct ifnet *ifp;
{
	register struct in_multi *inm;
	struct ifreq ifr;
	struct in_ifaddr *ia;
	int s = splsoftnet();

	/*
	 * See if address already in list.
	 */
	IN_LOOKUP_MULTI(*ap, ifp, inm);
	if (inm != NULL) {
		/*
		 * Found it; just increment the reference count.
		 */
		++inm->inm_refcount;
	} else {
		/*
		 * New address; allocate a new multicast record
		 * and link it into the interface's multicast list.
		 */
		inm = (struct in_multi *)malloc(sizeof(*inm),
		    M_IPMADDR, M_NOWAIT);
		if (inm == NULL) {
			splx(s);
			return (NULL);
		}
		inm->inm_addr = *ap;
		inm->inm_ifp = ifp;
		inm->inm_refcount = 1;
		IFP_TO_IA(ifp, ia);
		if (ia == NULL) {
			free(inm, M_IPMADDR);
			splx(s);
			return (NULL);
		}
		inm->inm_ia = ia;
		LIST_INSERT_HEAD(&ia->ia_multiaddrs, inm, inm_list);
		/*
		 * Ask the network driver to update its multicast reception
		 * filter appropriately for the new address.
		 */
		satosin(&ifr.ifr_addr)->sin_len = sizeof(struct sockaddr_in);
		satosin(&ifr.ifr_addr)->sin_family = AF_INET;
		satosin(&ifr.ifr_addr)->sin_addr = *ap;
		if ((ifp->if_ioctl == NULL) ||
		    (*ifp->if_ioctl)(ifp, SIOCADDMULTI,(caddr_t)&ifr) != 0) {
			LIST_REMOVE(inm, inm_list);
			free(inm, M_IPMADDR);
			splx(s);
			return (NULL);
		}
		/*
		 * Let IGMP know that we have joined a new IP multicast group.
		 */
		igmp_joingroup(inm);
	}
	splx(s);
	return (inm);
}

/*
 * Delete a multicast address record.
 */
void
in_delmulti(inm)
	register struct in_multi *inm;
{
	struct ifreq ifr;
	int s = splsoftnet();

	if (--inm->inm_refcount == 0) {
		/*
		 * No remaining claims to this record; let IGMP know that
		 * we are leaving the multicast group.
		 */
		igmp_leavegroup(inm);
		/*
		 * Unlink from list.
		 */
		LIST_REMOVE(inm, inm_list);
		/*
		 * Notify the network driver to update its multicast reception
		 * filter.
		 */
		satosin(&ifr.ifr_addr)->sin_family = AF_INET;
		satosin(&ifr.ifr_addr)->sin_addr = inm->inm_addr;
		(*inm->inm_ifp->if_ioctl)(inm->inm_ifp, SIOCDELMULTI,
							     (caddr_t)&ifr);
		free(inm, M_IPMADDR);
	}
	splx(s);
}

#endif
