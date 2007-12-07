/*	$NetBSD: iso.c,v 1.35.12.6 2007/12/07 17:34:41 yamt Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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
 *	@(#)iso.c	8.3 (Berkeley) 1/9/95
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
/*
 * iso.c: miscellaneous routines to support the iso address family
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: iso.c,v 1.35.12.6 2007/12/07 17:34:41 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/kauth.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netiso/iso.h>
#include <netiso/iso_var.h>
#include <netiso/iso_snpac.h>
#include <netiso/iso_pcb.h>
#include <netiso/clnp.h>
#include <netiso/argo_debug.h>

#include "opt_iso.h"
#ifdef ISO

int             iso_interfaces = 0;	/* number of external interfaces */

/*
 * FUNCTION:		iso_addrmatch1
 *
 * PURPOSE:		decide if the two iso_addrs passed are equal
 *
 * RETURNS:		true if the addrs match, false if they do not
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 */
int
iso_addrmatch1(const struct iso_addr *isoaa, const struct iso_addr *isoab)
{
	u_int           compare_len;

#ifdef ARGO_DEBUG
	if (argo_debug[D_ROUTE]) {
		printf("iso_addrmatch1: comparing lengths: %d to %d\n", isoaa->isoa_len,
		    isoab->isoa_len);
		printf("a:\n");
		dump_buf(isoaa->isoa_genaddr, isoaa->isoa_len);
		printf("b:\n");
		dump_buf(isoab->isoa_genaddr, isoab->isoa_len);
	}
#endif

	if ((compare_len = isoaa->isoa_len) != isoab->isoa_len) {
#ifdef ARGO_DEBUG
		if (argo_debug[D_ROUTE]) {
			printf("iso_addrmatch1: returning false because of lengths\n");
		}
#endif
		return 0;
	}
#ifdef notdef
	/* TODO : generalize this to all afis with masks */
	if (isoaa->isoa_afi == AFI_37) {
		/*
		 * must not compare 2 least significant digits, or for that
		 * matter, the DSP
		 */
		compare_len = ADDR37_IDI_LEN - 1;
	}
#endif

#ifdef ARGO_DEBUG
	if (argo_debug[D_ROUTE]) {
		int             i;
		const char           *a, *b;

		a = isoaa->isoa_genaddr;
		b = isoab->isoa_genaddr;

		for (i = 0; i < compare_len; i++) {
			printf("<%x=%x>", a[i] & 0xff, b[i] & 0xff);
			if (a[i] != b[i]) {
				printf("\naddrs are not equal at byte %d\n", i);
				return (0);
			}
		}
		printf("\n");
		printf("addrs are equal\n");
		return (1);
	}
#endif
	return (!bcmp(isoaa->isoa_genaddr, isoab->isoa_genaddr, compare_len));
}

/*
 * FUNCTION:		iso_addrmatch
 *
 * PURPOSE:		decide if the two sockadrr_isos passed are equal
 *
 * RETURNS:		true if the addrs match, false if they do not
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 */
int
iso_addrmatch(const struct sockaddr_iso *sisoa,
	const struct sockaddr_iso *sisob)
{
	return (iso_addrmatch1(&sisoa->siso_addr, &sisob->siso_addr));
}
#ifdef notdef
/*
 * FUNCTION:		iso_netmatch
 *
 * PURPOSE:		similar to iso_addrmatch but takes sockaddr_iso
 *			as argument.
 *
 * RETURNS:		true if same net, false if not
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 */
int
iso_netmatch(const struct sockaddr_iso *sisoa,
	const struct sockaddr_iso *sisob)
{
	u_char          bufa[sizeof(struct sockaddr_iso)];
	u_char          bufb[sizeof(struct sockaddr_iso)];
	int    lena, lenb;

	lena = iso_netof(&sisoa->siso_addr, bufa);
	lenb = iso_netof(&sisob->siso_addr, bufb);

#ifdef ARGO_DEBUG
	if (argo_debug[D_ROUTE]) {
		printf("iso_netmatch: comparing lengths: %d to %d\n", lena, lenb);
		printf("a:\n");
		dump_buf(bufa, lena);
		printf("b:\n");
		dump_buf(bufb, lenb);
	}
#endif

	return ((lena == lenb) && (!bcmp(bufa, bufb, lena)));
}
#endif /* notdef */

/*
 * FUNCTION:		iso_hashchar
 *
 * PURPOSE:		Hash all character in the buffer specified into
 *			a long. Return the long.
 *
 * RETURNS:		The hash value.
 *
 * SIDE EFFECTS:
 *
 * NOTES:		The hash is achieved by exclusive ORing 4 byte
 *			quantities.
 */
u_long
iso_hashchar(void *bufv, int len)
{
	char *buf = bufv;
	u_long h = 0;
	int    i;

	for (i = 0; i < len; i += 4) {
		u_long l = 0;

		if ((len - i) < 4) {
			/* buffer not multiple of 4 */
			switch (len - i) {
			case 3:
				l |= buf[i + 2] << 8;
			case 2:
				l |= buf[i + 1] << 16;
			case 1:
				l |= buf[i] << 24;
				break;
			default:
				printf("iso_hashchar: unexpected value x%x\n", len - i);
				break;
			}
		} else {
			l |= buf[i] << 24;
			l |= buf[i + 1] << 16;
			l |= buf[i + 2] << 8;
			l |= buf[i + 3];
		}

		h ^= l;
	}

	h ^= (u_long) (len % 4);

	return (h);
}

#ifdef notdef
/*
 * FUNCTION:		iso_hash
 *
 * PURPOSE:		Fill in fields of afhash structure based upon addr
 *			passed.
 *
 * RETURNS:		none
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 */
void
iso_hash(
	struct sockaddr_iso *siso,	/* address to perform hash on */
	struct afhash  *hp)	/* RETURN: hash info here */
{
	u_long buf[sizeof(struct sockaddr_iso) / 4 + 1];
	int    bufsize;


	bzero(buf, sizeof(buf));

	bufsize = iso_netof(&siso->siso_addr, buf);
	hp->afh_nethash = iso_hashchar((void *) buf, bufsize);

#ifdef ARGO_DEBUG
	if (argo_debug[D_ROUTE]) {
		printf("iso_hash: iso_netof: bufsize = %d\n", bufsize);
	}
#endif

	hp->afh_hosthash = iso_hashchar((void *) & siso->siso_addr,
					siso->siso_addr.isoa_len);

#ifdef ARGO_DEBUG
	if (argo_debug[D_ROUTE]) {
		printf("iso_hash: %s: nethash = x%x, hosthash = x%x\n",
		    clnp_iso_addrp(&siso->siso_addr), hp->afh_nethash,
		    hp->afh_hosthash);
	}
#endif
}
/*
 * FUNCTION:		iso_netof
 *
 * PURPOSE:		Extract the network portion of the iso address.
 *			The network portion of the iso address varies depending
 *			on the type of address. The network portion of the
 *			address will include the IDP. The network portion is:
 *
 *			TYPE			DESC
 *			t37			The AFI and x.121 (IDI)
 *			osinet			The AFI, orgid, snetid
 *			rfc986			The AFI, vers and network part
 *						of internet address.
 *
 * RETURNS:		number of bytes placed into buf.
 *
 * SIDE EFFECTS:
 *
 * NOTES:		Buf is assumed to be big enough
 */
u_int
iso_netof(
	struct iso_addr *isoa,	/* address */
	void *        buf)	/* RESULT: network portion of address here */
{
	u_int           len = 1;/* length of afi */

	switch (isoa->isoa_afi) {
	case AFI_37:
		/*
		 * Due to classic x.25 tunnel vision, there is no
		 * net portion of an x.121 address.  For our purposes
		 * the AFI will do, so that all x.25 -type addresses
		 * map to the single x.25 SNPA. (Cannot have more than
		 * one, obviously).
		 */

		break;

		/* case AFI_OSINET: */
	case AFI_RFC986:{
			u_short         idi;	/* value of idi */

			/* osinet and rfc986 have idi in the same place */
			CTOH(isoa->rfc986_idi[0], isoa->rfc986_idi[1], idi);

			if (idi == IDI_OSINET)
				/*
				 * Network portion of OSINET address can only
				 * be the IDI. Clearly, with one x25 interface,
				 * one could get to several orgids, and
				 * several snetids.
				 */
#if 0
				len += (ADDROSINET_IDI_LEN +
					OVLOSINET_ORGID_LEN +
					OVLOSINET_SNETID_LEN);
#endif
				len += ADDROSINET_IDI_LEN;
			else if (idi == IDI_RFC986) {
				struct ovl_rfc986 *o986 =
					(struct ovl_rfc986 *) isoa;

				/*
				 * bump len to include idi and version (1
				 * byte)
				 */
				len += ADDRRFC986_IDI_LEN + 1;

#ifdef ARGO_DEBUG
				if (argo_debug[D_ROUTE]) {
					printf("iso_netof: isoa ");
					dump_buf(isoa, sizeof(*isoa));
					printf("iso_netof: inetaddr 0x%x ",
					    inetaddr);
				}
#endif

				/*
				 * bump len by size of network portion of
				 * inet address
				 */
				if (IN_CLASSA(o986->o986_inetaddr)) {
					len += 4 - IN_CLASSA_NSHIFT / 8;
#ifdef ARGO_DEBUG
					if (argo_debug[D_ROUTE]) {
						printf("iso_netof: class A net len is now %d\n", len);
					}
#endif
				} else if (IN_CLASSB(o986->o986_inetaddr)) {
					len += 4 - IN_CLASSB_NSHIFT / 8;
#ifdef ARGO_DEBUG
					if (argo_debug[D_ROUTE]) {
						printf("iso_netof: class B net len is now %d\n", len);
					}
#endif
				} else {
					len += 4 - IN_CLASSC_NSHIFT / 8;
#ifdef ARGO_DEBUG
					if (argo_debug[D_ROUTE]) {
						printf("iso_netof: class C net len is now %d\n", len);
					}
#endif
				}
			} else
				len = 0;
		} break;

	default:
		len = 0;
	}

	bcopy((void *) isoa, buf, len);
#ifdef ARGO_DEBUG
	if (argo_debug[D_ROUTE]) {
		printf("iso_netof: isoa ");
		dump_buf(isoa, len);
		printf("iso_netof: net ");
		dump_buf(buf, len);
	}
#endif
	return len;
}
#endif				/* notdef */
/*
 * Generic iso control operations (ioctl's).
 * Ifp is 0 if not an interface-specific ioctl.
 */
/* ARGSUSED */
int
iso_control(struct socket *so, u_long cmd, void *data, struct ifnet *ifp,
	struct lwp *l)
{
	struct iso_ifreq *ifr = (struct iso_ifreq *) data;
	struct iso_ifaddr *ia = 0;
	struct iso_aliasreq *ifra = (struct iso_aliasreq *) data;
	int             error, hostIsNew, maskIsNew;

	/*
	 * Find address for this interface, if it exists.
	 */
	if (ifp)
		TAILQ_FOREACH(ia, &iso_ifaddr, ia_list)
			if (ia->ia_ifp == ifp)
				break;

	switch (cmd) {

	case SIOCAIFADDR_ISO:
	case SIOCDIFADDR_ISO:
		if (ifra->ifra_addr.siso_family == AF_ISO)
			for (; ia != 0; ia = ia->ia_list.tqe_next) {
				if (ia->ia_ifp == ifp &&
				    SAME_ISOADDR(&ia->ia_addr, &ifra->ifra_addr))
					break;
			}
		if (cmd == SIOCDIFADDR_ISO && ia == 0)
			return (EADDRNOTAVAIL);
		/* FALLTHROUGH */
#if 0
	case SIOCSIFADDR:
	case SIOCSIFNETMASK:
	case SIOCSIFDSTADDR:
#endif
		if (l == 0 || kauth_authorize_network(l->l_cred,
		    KAUTH_NETWORK_INTERFACE,
		    KAUTH_REQ_NETWORK_INTERFACE_SETPRIV, ifp, (void *)cmd,
		    NULL))
			return (EPERM);

		if (ifp == 0)
			panic("iso_control");
		if (ia == 0) {
			MALLOC(ia, struct iso_ifaddr *, sizeof(*ia),
			       M_IFADDR, M_WAITOK|M_ZERO);
			if (ia == 0)
				return (ENOBUFS);
			TAILQ_INSERT_TAIL(&iso_ifaddr, ia, ia_list);
			IFAREF(&ia->ia_ifa);
			ifa_insert(ifp, &ia->ia_ifa);
			ia->ia_ifa.ifa_addr = sisotosa(&ia->ia_addr);
			ia->ia_ifa.ifa_dstaddr = sisotosa(&ia->ia_dstaddr);
			ia->ia_ifa.ifa_netmask = sisotosa(&ia->ia_sockmask);
			ia->ia_ifp = ifp;
			if ((ifp->if_flags & IFF_LOOPBACK) == 0)
				iso_interfaces++;
		}
		break;

	case SIOCGIFADDR_ISO:
	case SIOCGIFNETMASK_ISO:
	case SIOCGIFDSTADDR_ISO:
		if (ia == 0)
			return (EADDRNOTAVAIL);
		break;
	}
	switch (cmd) {

	case SIOCGIFADDR_ISO:
		ifr->ifr_Addr = ia->ia_addr;
		break;

	case SIOCGIFDSTADDR_ISO:
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
			return (EINVAL);
		ifr->ifr_Addr = ia->ia_dstaddr;
		break;

	case SIOCGIFNETMASK_ISO:
		ifr->ifr_Addr = ia->ia_sockmask;
		break;

	case SIOCAIFADDR_ISO:
		maskIsNew = 0;
		hostIsNew = 1;
		error = 0;
		if (ia->ia_addr.siso_family == AF_ISO) {
			if (ifra->ifra_addr.siso_len == 0) {
				ifra->ifra_addr = ia->ia_addr;
				hostIsNew = 0;
			} else if (SAME_ISOADDR(&ia->ia_addr, &ifra->ifra_addr))
				hostIsNew = 0;
		}
		if (ifra->ifra_mask.siso_len) {
			iso_ifscrub(ifp, ia);
			ia->ia_sockmask = ifra->ifra_mask;
			maskIsNew = 1;
		}
		if ((ifp->if_flags & IFF_POINTOPOINT) &&
		    (ifra->ifra_dstaddr.siso_family == AF_ISO)) {
			iso_ifscrub(ifp, ia);
			ia->ia_dstaddr = ifra->ifra_dstaddr;
			maskIsNew = 1;	/* We lie; but the effect's the same */
		}
		if (ifra->ifra_addr.siso_family == AF_ISO &&
		    (hostIsNew || maskIsNew))
			error = iso_ifinit(ifp, ia, &ifra->ifra_addr, 0);
		if (ifra->ifra_snpaoffset)
			ia->ia_snpaoffset = ifra->ifra_snpaoffset;
		return (error);

	case SIOCDIFADDR_ISO:
		iso_purgeaddr(&ia->ia_ifa);
		break;

#define cmdbyte(x)	(((x) >> 8) & 0xff)
	default:
		if (cmdbyte(cmd) == 'a')
			return (snpac_ioctl(so, cmd, data, l));
		if (ifp == 0 || ifp->if_ioctl == 0)
			return (EOPNOTSUPP);
		return ((*ifp->if_ioctl)(ifp, cmd, data));
	}
	return (0);
}

void
iso_purgeaddr(struct ifaddr *ifa)
{
	struct ifnet *ifp = ifa->ifa_ifp;
	struct iso_ifaddr *ia = (void *) ifa;

	iso_ifscrub(ifp, ia);
	ifa_remove(ifp, &ia->ia_ifa);
	TAILQ_REMOVE(&iso_ifaddr, ia, ia_list);
	IFAFREE(&ia->ia_ifa);
}

void
iso_purgeif(struct ifnet *ifp)
{
	if_purgeaddrs(ifp, AF_ISO, iso_purgeaddr);
}

/*
 * Delete any existing route for an interface.
 */
void
iso_ifscrub(struct ifnet *ifp, struct iso_ifaddr *ia)
{
	int             nsellength = ia->ia_addr.siso_tlen;
	if ((ia->ia_flags & IFA_ROUTE) == 0)
		return;
	ia->ia_addr.siso_tlen = 0;
	if (ifp->if_flags & IFF_LOOPBACK)
		rtinit(&(ia->ia_ifa), (int) RTM_DELETE, RTF_HOST);
	else if (ifp->if_flags & IFF_POINTOPOINT)
		rtinit(&(ia->ia_ifa), (int) RTM_DELETE, RTF_HOST);
	else {
		rtinit(&(ia->ia_ifa), (int) RTM_DELETE, 0);
	}
	ia->ia_addr.siso_tlen = nsellength;
	ia->ia_flags &= ~IFA_ROUTE;
}

/*
 * Initialize an interface's internet address
 * and routing table entry.
 */
int
iso_ifinit(struct ifnet *ifp, struct iso_ifaddr *ia, struct sockaddr_iso *siso,
	int scrub)
{
	struct sockaddr_iso oldaddr;
	int             s = splnet(), error, nsellength;

	oldaddr = ia->ia_addr;
	ia->ia_addr = *siso;
	/*
	 * Give the interface a chance to initialize
	 * if this is its first address,
	 * and to validate the address if necessary.
	 */
	if (ifp->if_ioctl &&
	    (error = (*ifp->if_ioctl) (ifp, SIOCSIFADDR, (void *) ia))) {
		splx(s);
		ia->ia_addr = oldaddr;
		return (error);
	}
	if (scrub) {
		ia->ia_ifa.ifa_addr = sisotosa(&oldaddr);
		iso_ifscrub(ifp, ia);
		ia->ia_ifa.ifa_addr = sisotosa(&ia->ia_addr);
	}
	/*
	 * XXX -- The following is here temporarily out of laziness in not
	 * changing every ethernet driver's if_ioctl routine
	 */
	if (ifp->if_type == IFT_ETHER || ifp->if_type == IFT_FDDI) {
		ia->ia_ifa.ifa_rtrequest = llc_rtrequest;
		ia->ia_ifa.ifa_flags |= RTF_CLONING;
	}
	/*
	 * Add route for the network.
	 */
	nsellength = ia->ia_addr.siso_tlen;
	ia->ia_addr.siso_tlen = 0;
	if (ifp->if_flags & IFF_LOOPBACK) {
		ia->ia_ifa.ifa_dstaddr = ia->ia_ifa.ifa_addr;
		error = rtinit(&(ia->ia_ifa), (int) RTM_ADD, RTF_HOST | RTF_UP);
	} else if (ifp->if_flags & IFF_POINTOPOINT &&
		   ia->ia_dstaddr.siso_family == AF_ISO)
		error = rtinit(&(ia->ia_ifa), (int) RTM_ADD, RTF_HOST | RTF_UP);
	else {
		rt_maskedcopy(ia->ia_ifa.ifa_addr, ia->ia_ifa.ifa_dstaddr,
			      ia->ia_ifa.ifa_netmask);
		ia->ia_dstaddr.siso_nlen =
			min(ia->ia_addr.siso_nlen, (ia->ia_sockmask.siso_len - 6));
		error = rtinit(&(ia->ia_ifa), (int) RTM_ADD, RTF_UP);
	}
	ia->ia_addr.siso_tlen = nsellength;
	ia->ia_flags |= IFA_ROUTE;
	splx(s);
	return (error);
}
#ifdef notdef

struct ifaddr  *
iso_ifwithidi(struct sockaddr *addr)
{
	struct ifnet *ifp;
	struct ifaddr *ifa;
	u_int  af = addr->sa_family;

	if (af != AF_ISO)
		return (0);
#ifdef ARGO_DEBUG
	if (argo_debug[D_ROUTE]) {
		printf(">>> iso_ifwithidi addr\n");
		dump_isoaddr(satosiso(addr));
		printf("\n");
	}
#endif
	TAILQ_FOREACH(ifp, &ifnet, if_list) {
#ifdef ARGO_DEBUG
		if (argo_debug[D_ROUTE]) {
			printf("iso_ifwithidi ifnet %s\n", ifp->if_name);
		}
#endif
		IFADDR_FOREACH(ifa, ifp) {
#ifdef ARGO_DEBUG
			if (argo_debug[D_ROUTE]) {
				printf("iso_ifwithidi address ");
				dump_isoaddr(satosiso(ifa->ifa_addr));
			}
#endif
			if (ifa->ifa_addr->sa_family != addr->sa_family)
				continue;

#ifdef ARGO_DEBUG
			if (argo_debug[D_ROUTE]) {
				printf(" af same, args to iso_eqtype:\n");
				printf("0x%x ", satosiso(ifa->ifa_addr)->siso_addr);
				printf(" 0x%x\n",
				    &satosiso(addr)->siso_addr));
			}
#endif

			if (iso_eqtype(&satosiso(ifa->ifa_addr)->siso_addr,
				       &satosiso(addr)->siso_addr)) {
#ifdef ARGO_DEBUG
				if (argo_debug[D_ROUTE]) {
					printf("ifa_ifwithidi: ifa found\n");
				}
#endif
				return (ifa);
			}
#ifdef ARGO_DEBUG
			if (argo_debug[D_ROUTE]) {
				printf(" iso_eqtype failed\n");
			}
#endif
		}
	}
	return ((struct ifaddr *) 0);
}

#endif				/* notdef */
/*
 * FUNCTION:		iso_ck_addr
 *
 * PURPOSE:		return true if the iso_addr passed is
 *			within the legal size limit for an iso address.
 *
 * RETURNS:		true or false
 *
 * SIDE EFFECTS:
 *
 */
int
iso_ck_addr(struct iso_addr *isoa)
{
	return (isoa->isoa_len <= 20);

}

#ifdef notdef
/*
 * FUNCTION:		iso_eqtype
 *
 * PURPOSE:		Determine if two iso addresses are of the same type.
 *			This is flaky.  Really we should consider all type
 *			47 addrs to be the same - but there do exist different
 *			structures for 47 addrs. Gosip adds a 3rd.
 *
 * RETURNS:		true if the addresses are the same type
 *
 * SIDE EFFECTS:
 *
 * NOTES:		By type, I mean rfc986, t37, or osinet
 *
 *			This will first compare afis. If they match, then
 *			if the addr is not t37, the idis must be compared.
 */
int
iso_eqtype(
	struct iso_addr *isoaa,	/* first addr to check */
	struct iso_addr *isoab)	/* other addr to check */
{
	if (isoaa->isoa_afi == isoab->isoa_afi) {
		if (isoaa->isoa_afi == AFI_37)
			return (1);
		else
			return (!bcmp(&isoaa->isoa_u, &isoab->isoa_u, 2));
	}
	return (0);
}
#endif /* notdef */
/*
 * FUNCTION:		iso_localifa()
 *
 * PURPOSE:		Find an interface addresss having a given destination
 *			or at least matching the net.
 *
 * RETURNS:		ptr to an interface address
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 */
struct iso_ifaddr *
iso_localifa(const struct sockaddr_iso *siso)
{
	struct iso_ifaddr *ia;
	const char *cp1, *cp2, *cp3;
	struct ifnet *ifp;
	struct iso_ifaddr *ia_maybe = 0;
	/*
	 * We make one pass looking for both net matches and an exact
	 * dst addr.
	 */
	TAILQ_FOREACH(ia, &iso_ifaddr, ia_list) {
		if ((ifp = ia->ia_ifp) == 0 || ((ifp->if_flags & IFF_UP) == 0))
			continue;
		if (ifp->if_flags & IFF_POINTOPOINT) {
			if ((ia->ia_dstaddr.siso_family == AF_ISO) &&
			    SAME_ISOADDR(&ia->ia_dstaddr, siso))
				return (ia);
			else if (SAME_ISOADDR(&ia->ia_addr, siso))
				ia_maybe = ia;
			continue;
		}
		if (ia->ia_sockmask.siso_len) {
			char *cplim = ia->ia_sockmask.siso_len +
				      (char *) &ia->ia_sockmask;
			cp1 = ia->ia_sockmask.siso_data;
			cp2 = siso->siso_data;
			cp3 = ia->ia_addr.siso_data;
			while (cp1 < cplim)
				if (*cp1++ & (*cp2++ ^ *cp3++))
					goto next;
			ia_maybe = ia;
		}
		if (SAME_ISOADDR(&ia->ia_addr, siso))
			return ia;
next:		;
	}
	return ia_maybe;
}

#ifdef	TPCONS
#include <netiso/cons.h>
#endif	/* TPCONS */
/*
 * FUNCTION:		iso_nlctloutput
 *
 * PURPOSE:		Set options at the network level
 *
 * RETURNS:		E*
 *
 * SIDE EFFECTS:
 *
 * NOTES:		This could embody some of the functions of
 *				rclnp_ctloutput and cons_ctloutput.
 */
int
iso_nlctloutput(
	int             cmd,		/* command:set or get */
	int             optname,	/* option of interest */
	void *        pcb,		/* nl pcb */
	struct mbuf    *m)		/* data for set, buffer for get */
{
#ifdef TPCONS
	struct isopcb  *isop = (struct isopcb *) pcb;
#endif
	int             error = 0;	/* return value */
	void *        data;	/* data for option */
	int             data_len;	/* data's length */

#ifdef ARGO_DEBUG
	if (argo_debug[D_ISO]) {
		printf("iso_nlctloutput: cmd %x, opt %x, pcb %p, m %p\n",
		    cmd, optname, pcb, m);
	}
#endif

	if ((cmd != PRCO_GETOPT) && (cmd != PRCO_SETOPT))
		return (EOPNOTSUPP);

	data = mtod(m, void *);
	data_len = (m)->m_len;

#ifdef ARGO_DEBUG
	if (argo_debug[D_ISO]) {
		printf("iso_nlctloutput: data is:\n");
		dump_buf(data, data_len);
	}
#endif

	switch (optname) {

#ifdef	TPCONS
	case CONSOPT_X25CRUD:
		if (cmd == PRCO_GETOPT) {
			error = EOPNOTSUPP;
			break;
		}
		if (data_len > MAXX25CRUDLEN) {
			error = EINVAL;
			break;
		}
#ifdef ARGO_DEBUG
		if (argo_debug[D_ISO]) {
			printf("iso_nlctloutput: setting x25 crud\n");
		}
#endif

		bcopy(data, (void *) isop->isop_x25crud, (unsigned) data_len);
		isop->isop_x25crud_len = data_len;
		break;
#endif				/* TPCONS */

	default:
		error = EOPNOTSUPP;
	}
	if (cmd == PRCO_SETOPT)
		m_freem(m);
	return error;
}
#endif /* ISO */

#ifdef ARGO_DEBUG

/*
 * FUNCTION:		dump_isoaddr
 *
 * PURPOSE:		debugging
 *
 * RETURNS:		nada
 *
 */
void
dump_isoaddr(const struct sockaddr_iso *s)
{
	if (s->siso_family == AF_ISO) {
		printf("ISO address: suffixlen %d, %s\n",
		       s->siso_tlen, clnp_saddr_isop(s));
	} else if (s->siso_family == AF_INET) {
		/* hack */
		const struct sockaddr_in *sin = satocsin(s);

		printf("%d.%d.%d.%d: %d",
		       (sin->sin_addr.s_addr >> 24) & 0xff,
		       (sin->sin_addr.s_addr >> 16) & 0xff,
		       (sin->sin_addr.s_addr >> 8) & 0xff,
		       (sin->sin_addr.s_addr) & 0xff,
		       sin->sin_port);
	}
}

#endif /* ARGO_DEBUG */

struct queue {
	struct queue *q_next, *q_prev;
};

/*
 * FUNCTION:		iso_insque
 *
 * PURPOSE:		insert an element into a queue
 *
 * RETURNS:
 */
void
iso_insque(void *v1, void *v2)
{
	struct queue *elem = v1, *head = v2;
	struct queue *next;

	next = head->q_next;
	elem->q_next = next;
	head->q_next = elem;
	elem->q_prev = head;
	next->q_prev = elem;
}

/*
 * FUNCTION:		iso_remque
 *
 * PURPOSE:		remove an element from a queue
 *
 * RETURNS:
 */
void
iso_remque(void *v)
{
	struct queue *elem = v;
	struct queue *next, *prev;

	next = elem->q_next;
	prev = elem->q_prev;
	next->q_prev = prev;
	prev->q_next = next;
	elem->q_prev = NULL;
}
