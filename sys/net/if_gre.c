/*	$NetBSD: if_gre.c,v 1.98.2.2 2007/09/03 10:23:09 skrll Exp $ */

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Heiko W.Rupp <hwr@pilhuhn.de>
 *
 * IPv6-over-GRE contributed by Gert Doering <gert@greenie.muc.de>
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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

/*
 * Encapsulate L3 protocols into IP
 * See RFC 1701 and 1702 for more details.
 * If_gre is compatible with Cisco GRE tunnels, so you can
 * have a NetBSD box as the other end of a tunnel interface of a Cisco
 * router. See gre(4) for more details.
 * Also supported:  IP in IP encaps (proto 55) as of RFC 2004
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_gre.c,v 1.98.2.2 2007/09/03 10:23:09 skrll Exp $");

#include "opt_gre.h"
#include "opt_inet.h"
#include "bpfilter.h"

#ifdef INET
#include <sys/param.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#if __NetBSD__
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/kauth.h>
#endif

#include <sys/kernel.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/kthread.h>

#include <machine/cpu.h>

#include <net/ethertypes.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#else
#error "Huh? if_gre without inet?"
#endif


#ifdef NETATALK
#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/at_extern.h>
#endif

#if NBPFILTER > 0
#include <sys/time.h>
#include <net/bpf.h>
#endif

#include <net/if_gre.h>

#include <compat/sys/socket.h>
#include <compat/sys/sockio.h>
/*
 * It is not easy to calculate the right value for a GRE MTU.
 * We leave this task to the admin and use the same default that
 * other vendors use.
 */
#define GREMTU 1476

#ifdef GRE_DEBUG
int gre_debug = 0;
#define	GRE_DPRINTF(__sc, __fmt, ...)				\
	do {							\
		if (gre_debug || ((__sc)->sc_if.if_flags & IFF_DEBUG) != 0)\
			printf(__fmt, __VA_ARGS__);		\
	} while (/*CONSTCOND*/0)
#else
#define	GRE_DPRINTF(__sc, __fmt, ...)	do { } while (/*CONSTCOND*/0)
#endif /* GRE_DEBUG */

struct gre_softc_head gre_softc_list;
int ip_gre_ttl = GRE_TTL;

static int	gre_clone_create(struct if_clone *, int);
static int	gre_clone_destroy(struct ifnet *);

static struct if_clone gre_cloner =
    IF_CLONE_INITIALIZER("gre", gre_clone_create, gre_clone_destroy);

static int	gre_output(struct ifnet *, struct mbuf *,
			   const struct sockaddr *, struct rtentry *);
static int	gre_ioctl(struct ifnet *, u_long, void *);

static void gre_thread(void *);
static int	gre_compute_route(struct gre_softc *sc);

static void gre_closef(struct file **, struct lwp *);
static int gre_getsockname(struct socket *, struct mbuf *, struct lwp *);
static int gre_getpeername(struct socket *, struct mbuf *, struct lwp *);
static int gre_getnames(struct socket *, struct lwp *, struct sockaddr_in *,
    struct sockaddr_in *);

/* Calling thread must hold sc->sc_mtx. */
static void
gre_join(struct gre_softc *sc)
{
	while (sc->sc_running != 0)
		cv_wait(&sc->sc_join_cv, &sc->sc_mtx);
}

/* Calling thread must hold sc->sc_mtx. */
static void
gre_wakeup(struct gre_softc *sc)
{
	GRE_DPRINTF(sc, "%s: enter\n", __func__);
	sc->sc_haswork = 1;
	cv_signal(&sc->sc_work_cv);
}

static int
gre_clone_create(struct if_clone *ifc, int unit)
{
	struct gre_softc *sc;

	sc = malloc(sizeof(struct gre_softc), M_DEVBUF, M_WAITOK);
	memset(sc, 0, sizeof(struct gre_softc));
	mutex_init(&sc->sc_mtx, MUTEX_DRIVER, IPL_NET);
	cv_init(&sc->sc_work_cv, "gre work");
	cv_init(&sc->sc_join_cv, "gre join");
	cv_init(&sc->sc_soparm_cv, "gre soparm");

	snprintf(sc->sc_if.if_xname, sizeof(sc->sc_if.if_xname), "%s%d",
	    ifc->ifc_name, unit);
	sc->sc_if.if_softc = sc;
	sc->sc_if.if_type = IFT_TUNNEL;
	sc->sc_if.if_addrlen = 0;
	sc->sc_if.if_hdrlen = 24; /* IP + GRE */
	sc->sc_if.if_dlt = DLT_NULL;
	sc->sc_if.if_mtu = GREMTU;
	sc->sc_if.if_flags = IFF_POINTOPOINT|IFF_MULTICAST;
	sc->sc_if.if_output = gre_output;
	sc->sc_if.if_ioctl = gre_ioctl;
	sc->sc_dst.s_addr = sc->sc_src.s_addr = INADDR_ANY;
	sc->sc_dstport = sc->sc_srcport = 0;
	sc->sc_proto = IPPROTO_GRE;
	sc->sc_snd.ifq_maxlen = 256;
	sc->sc_if.if_flags |= IFF_LINK0;
	if_attach(&sc->sc_if);
	if_alloc_sadl(&sc->sc_if);
#if NBPFILTER > 0
	bpfattach(&sc->sc_if, DLT_NULL, sizeof(u_int32_t));
#endif
	sc->sc_running = 1;
	if (kthread_create(PRI_NONE, 0, NULL, gre_thread, sc,
	    NULL, sc->sc_if.if_xname) != 0)
		sc->sc_running = 0;
	LIST_INSERT_HEAD(&gre_softc_list, sc, sc_list);
	return 0;
}

static int
gre_clone_destroy(struct ifnet *ifp)
{
	struct gre_softc *sc = ifp->if_softc;

	LIST_REMOVE(sc, sc_list);
#if NBPFILTER > 0
	bpfdetach(ifp);
#endif
	if_detach(ifp);
	mutex_enter(&sc->sc_mtx);
	sc->sc_dying = 1;
	gre_wakeup(sc);
	gre_join(sc);
	mutex_exit(&sc->sc_mtx);
	rtcache_free(&sc->route);

	cv_destroy(&sc->sc_soparm_cv);
	cv_destroy(&sc->sc_join_cv);
	cv_destroy(&sc->sc_work_cv);
	mutex_destroy(&sc->sc_mtx);
	free(sc, M_DEVBUF);

	return 0;
}

static void
gre_receive(struct socket *so, void *arg, int waitflag)
{
	struct gre_softc *sc = (struct gre_softc *)arg;

	GRE_DPRINTF(sc, "%s: enter\n", __func__);

	gre_wakeup(sc);
}

static void
gre_upcall_add(struct socket *so, void *arg)
{
	/* XXX What if the kernel already set an upcall? */
	KASSERT((so->so_rcv.sb_flags & SB_UPCALL) == 0);
	so->so_upcallarg = arg;
	so->so_upcall = gre_receive;
	so->so_rcv.sb_flags |= SB_UPCALL;
}

static void
gre_upcall_remove(struct socket *so)
{
	/* XXX What if the kernel already set an upcall? */
	so->so_rcv.sb_flags &= ~SB_UPCALL;
	so->so_upcallarg = NULL;
	so->so_upcall = NULL;
}

static void
gre_sodestroy(struct socket **sop)
{
	gre_upcall_remove(*sop);
	soshutdown(*sop, SHUT_RDWR);
	soclose(*sop);
	*sop = NULL;
}

static struct mbuf *
gre_getsockmbuf(struct socket *so)
{
	struct mbuf *m;

	m = m_get(M_WAIT, MT_SONAME);
	if (m != NULL)
		MCLAIM(m, so->so_mowner);
	return m;
}

static int
gre_socreate1(struct gre_softc *sc, struct lwp *l, struct socket **sop)
{
	int rc;
	struct mbuf *m;
	struct sockaddr_in *sin;
	struct socket *so;

	GRE_DPRINTF(sc, "%s: enter\n", __func__);
	rc = socreate(AF_INET, sop, SOCK_DGRAM, IPPROTO_UDP, l);
	if (rc != 0) {
		GRE_DPRINTF(sc, "%s: socreate failed\n", __func__);
		return rc;
	}

	so = *sop;

	gre_upcall_add(so, sc);
	if ((m = gre_getsockmbuf(so)) == NULL) {
		rc = ENOBUFS;
		goto out;
	}
	sin = mtod(m, struct sockaddr_in *);
	sockaddr_in_init(sin, &sc->sc_src, sc->sc_srcport);
	m->m_len = sin->sin_len;

	GRE_DPRINTF(sc, "%s: bind 0x%08" PRIx32 " port %d\n", __func__,
	    sin->sin_addr.s_addr, ntohs(sin->sin_port));
	if ((rc = sobind(so, m, l)) != 0) {
		GRE_DPRINTF(sc, "%s: sobind failed\n", __func__);
		goto out;
	}

	if (sc->sc_srcport == 0) {
		if ((rc = gre_getsockname(so, m, l)) != 0) {
			GRE_DPRINTF(sc, "%s: gre_getsockname\n", __func__);
			goto out;
		}
		sc->sc_srcport = sin->sin_port;
	}

	sockaddr_in_init(sin, &sc->sc_dst, sc->sc_dstport);
	m->m_len = sin->sin_len;

	if ((rc = soconnect(so, m, l)) != 0) {
		GRE_DPRINTF(sc, "%s: soconnect failed\n", __func__);
		goto out;
	}

	*mtod(m, int *) = ip_gre_ttl;
	m->m_len = sizeof(int);
	KASSERT(so->so_proto && so->so_proto->pr_ctloutput);
	rc = (*so->so_proto->pr_ctloutput)(PRCO_SETOPT, so, IPPROTO_IP, IP_TTL,
	    &m);
	m = NULL;
	if (rc != 0) {
		GRE_DPRINTF(sc, "%s: setopt ttl failed\n", __func__);
		rc = 0;
	}
out:
	m_freem(m);

	if (rc != 0)
		gre_sodestroy(sop);
	else {
		sc->sc_if.if_flags |= IFF_RUNNING;
		sc->sc_soparm = sc->sc_newsoparm;
	}

	return rc;
}

static void
gre_do_recv(struct gre_softc *sc, struct socket *so, lwp_t *l)
{
	for (;;) {
		int flags, rc;
		const struct gre_h *gh;
		struct mbuf *m;

		flags = MSG_DONTWAIT;
		sc->sc_uio.uio_resid = 1000000;
		rc = (*so->so_receive)(so, NULL, &sc->sc_uio, &m, NULL, &flags);
		/* TBD Back off if ECONNREFUSED (indicates
		 * ICMP Port Unreachable)?
		 */
		if (rc == EWOULDBLOCK) {
			GRE_DPRINTF(sc, "%s: so_receive EWOULDBLOCK\n",
			    __func__);
			break;
		} else if (rc != 0 || m == NULL) {
			GRE_DPRINTF(sc, "%s: rc %d m %p\n",
			    sc->sc_if.if_xname, rc, (void *)m);
			continue;
		} else
			GRE_DPRINTF(sc, "%s: so_receive ok\n", __func__);
		if (m->m_len < sizeof(*gh) &&
		    (m = m_pullup(m, sizeof(*gh))) == NULL) {
			GRE_DPRINTF(sc, "%s: m_pullup failed\n", __func__);
			continue;
		}
		gh = mtod(m, const struct gre_h *);

		if (gre_input3(sc, m, 0, gh, 0) == 0) {
			GRE_DPRINTF(sc, "%s: dropping unsupported\n", __func__);
			m_freem(m);
		}
	}
}

static void
gre_do_send(struct gre_softc *sc, struct socket *so, lwp_t *l)
{
	for (;;) {
		int rc;
		struct mbuf *m;

		mutex_enter(&sc->sc_mtx);
		IF_DEQUEUE(&sc->sc_snd, m);
		mutex_exit(&sc->sc_mtx);
		if (m == NULL)
			break;
		GRE_DPRINTF(sc, "%s: dequeue\n", __func__);
		if ((so->so_state & SS_ISCONNECTED) == 0) {
			GRE_DPRINTF(sc, "%s: not connected\n", __func__);
			m_freem(m);
			continue;
		}
		rc = (*so->so_send)(so, NULL, NULL, m, NULL, 0, l);
		/* XXX handle ENOBUFS? */
		if (rc != 0)
			GRE_DPRINTF(sc, "%s: so_send failed\n",
			    __func__);
	}
}

static struct socket *
gre_reconf(struct gre_softc *sc, struct socket *so, lwp_t *l)
{
	struct ifnet *ifp = &sc->sc_if;

	GRE_DPRINTF(sc, "%s: enter\n", __func__);

shutdown:
	if (sc->sc_soparm.sp_fp != NULL) {
		GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
		gre_upcall_remove(so);
		gre_closef(&sc->sc_soparm.sp_fp, curlwp);
		so = NULL;
	} else if (so != NULL)
		gre_sodestroy(&so);

	if (sc->sc_dying)
		GRE_DPRINTF(sc, "%s: dying\n", __func__);
	else if ((ifp->if_flags & IFF_UP) != IFF_UP)
		GRE_DPRINTF(sc, "%s: down\n", __func__);
	else if (sc->sc_proto != IPPROTO_UDP)
		GRE_DPRINTF(sc, "%s: not UDP\n", __func__);
	else if (sc->sc_newsoparm.sp_fp != NULL) {
		sc->sc_soparm = sc->sc_newsoparm;
		sc->sc_newsoparm.sp_fp = NULL;
		so = (struct socket *)sc->sc_soparm.sp_fp->f_data;
		gre_upcall_add(so, sc);
	} else if (gre_socreate1(sc, l, &so) != 0) {
		sc->sc_dying = 1;
		goto shutdown;
	}
	cv_signal(&sc->sc_soparm_cv);
	if (so != NULL)
		sc->sc_if.if_flags |= IFF_RUNNING;
	else if (sc->sc_proto == IPPROTO_UDP)
		sc->sc_if.if_flags &= ~IFF_RUNNING;
	return so;
}

static void
gre_thread1(struct gre_softc *sc, struct lwp *l)
{
	struct ifnet *ifp = &sc->sc_if;
	struct socket *so = NULL;

	GRE_DPRINTF(sc, "%s: enter\n", __func__);

	while (!sc->sc_dying) {
		while (sc->sc_haswork == 0) {
			GRE_DPRINTF(sc, "%s: sleeping\n", __func__);
			cv_wait(&sc->sc_work_cv, &sc->sc_mtx);
		}
		sc->sc_haswork = 0;

		GRE_DPRINTF(sc, "%s: awake\n", __func__);

		/* XXX optimize */ 
		if ((ifp->if_flags & IFF_UP) != IFF_UP ||
		    sc->sc_proto != IPPROTO_UDP || so == NULL ||
		    sc->sc_newsoparm.sp_fp != NULL ||
		    memcmp(&sc->sc_soparm, &sc->sc_newsoparm,
		           offsetof(struct gre_soparm, sp_fp)) != 0)
			so = gre_reconf(sc, so, l);
		mutex_exit(&sc->sc_mtx);
		if (so != NULL) {
			gre_do_recv(sc, so, l);
			gre_do_send(sc, so, l);
		}
		mutex_enter(&sc->sc_mtx);
	}
	sc->sc_running = 0;
	cv_signal(&sc->sc_join_cv);
}

static void
gre_thread(void *arg)
{
	struct gre_softc *sc = (struct gre_softc *)arg;

	mutex_enter(&sc->sc_mtx);
	gre_thread1(sc, curlwp);
	mutex_exit(&sc->sc_mtx);

	/* must not touch sc after this! */
	kthread_exit(0);
}

/* Calling thread must hold sc->sc_mtx. */
int
gre_input3(struct gre_softc *sc, struct mbuf *m, int hlen,
    const struct gre_h *gh, int mtx_held)
{
	u_int16_t flags;
#if NBPFILTER > 0
	u_int32_t af = AF_INET;		/* af passed to BPF tap */
#endif
	int isr;
	struct ifqueue *ifq;

	sc->sc_if.if_ipackets++;
	sc->sc_if.if_ibytes += m->m_pkthdr.len;

	hlen += sizeof(struct gre_h);

	/* process GRE flags as packet can be of variable len */
	flags = ntohs(gh->flags);

	/* Checksum & Offset are present */
	if ((flags & GRE_CP) | (flags & GRE_RP))
		hlen += 4;
	/* We don't support routing fields (variable length) */
	if (flags & GRE_RP) {
		sc->sc_if.if_ierrors++;
		return 0;
	}
	if (flags & GRE_KP)
		hlen += 4;
	if (flags & GRE_SP)
		hlen += 4;

	switch (ntohs(gh->ptype)) { /* ethertypes */
	case ETHERTYPE_IP: /* shouldn't need a schednetisr(), as */
		ifq = &ipintrq;          /* we are in ip_input */
		isr = NETISR_IP;
		break;
#ifdef NETATALK
	case ETHERTYPE_ATALK:
		ifq = &atintrq1;
		isr = NETISR_ATALK;
#if NBPFILTER > 0
		af = AF_APPLETALK;
#endif
		break;
#endif
#ifdef INET6
	case ETHERTYPE_IPV6:
		GRE_DPRINTF(sc, "%s: IPv6 packet\n", __func__);
		ifq = &ip6intrq;
		isr = NETISR_IPV6;
#if NBPFILTER > 0
		af = AF_INET6;
#endif
		break;
#endif
	default:	   /* others not yet supported */
		GRE_DPRINTF(sc, "%s: unhandled ethertype 0x%04x\n", __func__,
		    ntohs(gh->ptype));
		sc->sc_if.if_noproto++;
		return 0;
	}

	if (hlen > m->m_pkthdr.len) {
		m_freem(m);
		sc->sc_if.if_ierrors++;
		return EINVAL;
	}
	m_adj(m, hlen);

#if NBPFILTER > 0
	if (sc->sc_if.if_bpf != NULL)
		bpf_mtap_af(sc->sc_if.if_bpf, af, m);
#endif /*NBPFILTER > 0*/

	m->m_pkthdr.rcvif = &sc->sc_if;

	if (!mtx_held)
		mutex_enter(&sc->sc_mtx);
	if (IF_QFULL(ifq)) {
		IF_DROP(ifq);
		m_freem(m);
	} else {
		IF_ENQUEUE(ifq, m);
	}
	/* we need schednetisr since the address family may change */
	schednetisr(isr);
	if (!mtx_held)
		mutex_exit(&sc->sc_mtx);

	return 1;	/* packet is done, no further processing needed */
}

/*
 * The output routine. Takes a packet and encapsulates it in the protocol
 * given by sc->sc_proto. See also RFC 1701 and RFC 2004
 */
static int
gre_output(struct ifnet *ifp, struct mbuf *m, const struct sockaddr *dst,
	   struct rtentry *rt)
{
	int error = 0, hlen, msiz;
	struct gre_softc *sc = ifp->if_softc;
	struct greip *gi;
	struct gre_h *gh;
	struct ip *eip, *ip;
	u_int8_t ip_tos = 0;
	u_int16_t etype = 0;
	struct mobile_h mob_h;

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) !=
	    (IFF_UP | IFF_RUNNING) ||
	    sc->sc_src.s_addr == INADDR_ANY ||
	    sc->sc_dst.s_addr == INADDR_ANY) {
		m_freem(m);
		error = ENETDOWN;
		goto end;
	}

	gi = NULL;
	ip = NULL;

#if NBPFILTER >0
	if (ifp->if_bpf)
		bpf_mtap_af(ifp->if_bpf, dst->sa_family, m);
#endif

	m->m_flags &= ~(M_BCAST|M_MCAST);

	switch (sc->sc_proto) {
	case IPPROTO_MOBILE:
		if (dst->sa_family != AF_INET) {
			IF_DROP(&ifp->if_snd);
			m_freem(m);
			error = EINVAL;
			goto end;
		}

		if (M_UNWRITABLE(m, sizeof(*ip)) &&
		    (m = m_pullup(m, sizeof(*ip))) == NULL) {
			error = ENOBUFS;
			goto end;
		}
		ip = mtod(m, struct ip *);

		memset(&mob_h, 0, MOB_H_SIZ_L);
		mob_h.proto = (ip->ip_p) << 8;
		mob_h.odst = ip->ip_dst.s_addr;
		ip->ip_dst.s_addr = sc->sc_dst.s_addr;

		/*
		 * If the packet comes from our host, we only change
		 * the destination address in the IP header.
		 * Else we also need to save and change the source
		 */
		if (in_hosteq(ip->ip_src, sc->sc_src))
			msiz = MOB_H_SIZ_S;
		else {
			mob_h.proto |= MOB_H_SBIT;
			mob_h.osrc = ip->ip_src.s_addr;
			ip->ip_src.s_addr = sc->sc_src.s_addr;
			msiz = MOB_H_SIZ_L;
		}
		HTONS(mob_h.proto);
		mob_h.hcrc = gre_in_cksum((u_int16_t *)&mob_h, msiz);

		M_PREPEND(m, msiz, M_DONTWAIT);
		if (m == NULL) {
			error = ENOBUFS;
			goto end;
		}
		/* XXX Assuming that ip does not dangle after
		 * M_PREPEND.  In practice, that's true, but
		 * that's not in M_PREPEND's contract.
		 */
		memmove(mtod(m, void *), ip, sizeof(*ip));
		ip = mtod(m, struct ip *);
		memcpy(ip + 1, &mob_h, (size_t)msiz);
		ip->ip_len = htons(ntohs(ip->ip_len) + msiz);
		break;
	case IPPROTO_UDP:
	case IPPROTO_GRE:
		GRE_DPRINTF(sc, "%s: dst->sa_family=%d\n", __func__,
		    dst->sa_family);
		switch (dst->sa_family) {
		case AF_INET:
			ip = mtod(m, struct ip *);
			ip_tos = ip->ip_tos;
			etype = ETHERTYPE_IP;
			break;
#ifdef NETATALK
		case AF_APPLETALK:
			etype = ETHERTYPE_ATALK;
			break;
#endif
#ifdef INET6
		case AF_INET6:
			etype = ETHERTYPE_IPV6;
			break;
#endif
		default:
			IF_DROP(&ifp->if_snd);
			m_freem(m);
			error = EAFNOSUPPORT;
			goto end;
		}
		break;
	default:
		IF_DROP(&ifp->if_snd);
		m_freem(m);
		error = EINVAL;
		goto end;
	}

	switch (sc->sc_proto) {
	case IPPROTO_GRE:
		hlen = sizeof(struct greip);
		break;
	case IPPROTO_UDP:
		hlen = sizeof(struct gre_h);
		break;
	default:
		hlen = 0;
		break;
	}

	M_PREPEND(m, hlen, M_DONTWAIT);

	if (m == NULL) {
		IF_DROP(&ifp->if_snd);
		error = ENOBUFS;
		goto end;
	}

	switch (sc->sc_proto) {
	case IPPROTO_UDP:
		gh = mtod(m, struct gre_h *);
		memset(gh, 0, sizeof(*gh));
		gh->ptype = htons(etype);
		/* XXX Need to handle IP ToS.  Look at how I handle IP TTL. */
		break;
	case IPPROTO_GRE:
		gi = mtod(m, struct greip *);
		gh = &gi->gi_g;
		eip = &gi->gi_i;
		/* we don't have any GRE flags for now */
		memset(gh, 0, sizeof(*gh));
		gh->ptype = htons(etype);
		eip->ip_src = sc->sc_src;
		eip->ip_dst = sc->sc_dst;
		eip->ip_hl = (sizeof(struct ip)) >> 2;
		eip->ip_ttl = ip_gre_ttl;
		eip->ip_tos = ip_tos;
		eip->ip_len = htons(m->m_pkthdr.len);
		eip->ip_p = sc->sc_proto;
		break;
	case IPPROTO_MOBILE:
		eip = mtod(m, struct ip *);
		eip->ip_p = sc->sc_proto;
		break;
	default:
		error = EPROTONOSUPPORT;
		m_freem(m);
		goto end;
	}

	ifp->if_opackets++;
	ifp->if_obytes += m->m_pkthdr.len;

	/* send it off */
	if (sc->sc_proto == IPPROTO_UDP) {
		if (IF_QFULL(&sc->sc_snd)) {
			IF_DROP(&sc->sc_snd);
			error = ENOBUFS;
			m_freem(m);
		} else {
			IF_ENQUEUE(&sc->sc_snd, m);
			gre_wakeup(sc);
			error = 0;
		}
		goto end;
	}
	if (sc->route.ro_rt == NULL)
		rtcache_init(&sc->route);
	else
		rtcache_check(&sc->route);
	if (sc->route.ro_rt == NULL) {
		m_freem(m);
		goto end;
	}
	if (sc->route.ro_rt->rt_ifp->if_softc == sc) {
		rtcache_clear(&sc->route);
		m_freem(m);
	} else
		error = ip_output(m, NULL, &sc->route, 0, NULL, NULL);
  end:
	if (error)
		ifp->if_oerrors++;
	return error;
}

/* Calling thread must hold sc->sc_mtx. */
static int
gre_kick(struct gre_softc *sc)
{
	if (!sc->sc_running)
		return EBUSY;
	gre_wakeup(sc);
	return 0;
}

/* Calling thread must hold sc->sc_mtx. */
static int
gre_getname(struct socket *so, int req, struct mbuf *nam, struct lwp *l)
{
	return (*so->so_proto->pr_usrreq)(so, req, NULL, nam, NULL, l);
}

/* Calling thread must hold sc->sc_mtx. */
static int
gre_getsockname(struct socket *so, struct mbuf *nam, struct lwp *l)
{
	return gre_getname(so, PRU_SOCKADDR, nam, l);
}

/* Calling thread must hold sc->sc_mtx. */
static int
gre_getpeername(struct socket *so, struct mbuf *nam, struct lwp *l)
{
	return gre_getname(so, PRU_PEERADDR, nam, l);
}

/* Calling thread must hold sc->sc_mtx. */
static int
gre_getnames(struct socket *so, struct lwp *l, struct sockaddr_in *src,
    struct sockaddr_in *dst)
{
	struct mbuf *m;
	struct sockaddr_in *sin;
	int rc;

	if ((m = gre_getsockmbuf(so)) == NULL)
		return ENOBUFS;

	sin = mtod(m, struct sockaddr_in *);

	if ((rc = gre_getsockname(so, m, l)) != 0)
		goto out;
	if (sin->sin_family != AF_INET) {
		rc = EAFNOSUPPORT;
		goto out;
	}
	*src = *sin;

	if ((rc = gre_getpeername(so, m, l)) != 0)
		goto out;
	if (sin->sin_family != AF_INET) {
		rc = EAFNOSUPPORT;
		goto out;
	}
	*dst = *sin;

out:
	m_freem(m);
	return rc;
}

static void
gre_closef(struct file **fpp, struct lwp *l)
{
	struct file *fp = *fpp;

	simple_lock(&fp->f_slock);
	FILE_USE(fp);
	closef(fp, l);
	*fpp = NULL;
}

static int
gre_ioctl(struct ifnet *ifp, const u_long cmd, void *data)
{
	u_char oproto;
	struct file *fp;
	struct socket *so;
	struct sockaddr_in dst, src;
	struct proc *p = curproc;	/* XXX */
	struct lwp *l = curlwp;	/* XXX */
	struct ifreq *ifr;
	struct if_laddrreq *lifr = (struct if_laddrreq *)data;
	struct gre_softc *sc = ifp->if_softc;
	struct sockaddr_in si;
	const struct sockaddr *sa;
	int error = 0;

	ifr = data;

	switch (cmd) {
	case SIOCSIFFLAGS:
	case SIOCSIFMTU:
	case GRESPROTO:
	case GRESADDRD:
	case GRESADDRS:
	case GRESSOCK:
	case GREDSOCK:
	case SIOCSLIFPHYADDR:
	case SIOCDIFPHYADDR:
		if (kauth_authorize_network(l->l_cred, KAUTH_NETWORK_INTERFACE,
		    KAUTH_REQ_NETWORK_INTERFACE_SETPRIV, ifp, (void *)cmd,
		    NULL) != 0)
			return EPERM;
		break;
	default:
		break;
	}

	mutex_enter(&sc->sc_mtx);
	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		if ((error = gre_kick(sc)) != 0)
			ifp->if_flags &= ~IFF_UP;
		break;
	case SIOCSIFDSTADDR:
		break;
	case SIOCSIFFLAGS:
		oproto = sc->sc_proto;
		switch (ifr->ifr_flags & (IFF_LINK0|IFF_LINK2)) {
		case IFF_LINK0|IFF_LINK2:
			sc->sc_proto = IPPROTO_UDP;
			if (oproto != IPPROTO_UDP)
				ifp->if_flags &= ~IFF_RUNNING;
			error = gre_kick(sc);
			break;
		case IFF_LINK0:
			sc->sc_proto = IPPROTO_GRE;
			gre_wakeup(sc);
			goto recompute;
		case 0:
			sc->sc_proto = IPPROTO_MOBILE;
			gre_wakeup(sc);
			goto recompute;
		}
		break;
	case SIOCSIFMTU:
		if (ifr->ifr_mtu < 576) {
			error = EINVAL;
			break;
		}
		ifp->if_mtu = ifr->ifr_mtu;
		break;
	case SIOCGIFMTU:
		ifr->ifr_mtu = sc->sc_if.if_mtu;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifr == NULL) {
			error = EAFNOSUPPORT;
			break;
		}
		switch (ifreq_getaddr(cmd, ifr)->sa_family) {
#ifdef INET
		case AF_INET:
			break;
#endif
#ifdef INET6
		case AF_INET6:
			break;
#endif
		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;
	case GRESPROTO:
		oproto = sc->sc_proto;
		sc->sc_proto = ifr->ifr_flags;
		switch (sc->sc_proto) {
		case IPPROTO_UDP:
			ifp->if_flags |= IFF_LINK0|IFF_LINK2;
			if (oproto != IPPROTO_UDP)
				ifp->if_flags &= ~IFF_RUNNING;
			error = gre_kick(sc);
			break;
		case IPPROTO_GRE:
			ifp->if_flags |= IFF_LINK0;
			ifp->if_flags &= ~IFF_LINK2;
			goto recompute;
		case IPPROTO_MOBILE:
			ifp->if_flags &= ~(IFF_LINK0|IFF_LINK2);
			goto recompute;
		default:
			error = EPROTONOSUPPORT;
			break;
		}
		break;
	case GREGPROTO:
		ifr->ifr_flags = sc->sc_proto;
		break;
	case GRESADDRS:
	case GRESADDRD:
		/*
		 * set tunnel endpoints, compute a less specific route
		 * to the remote end and mark if as up
		 */
		sa = &ifr->ifr_addr;
		if (cmd == GRESADDRS) {
			sc->sc_src = satocsin(sa)->sin_addr;
			sc->sc_srcport = satocsin(sa)->sin_port;
		}
		if (cmd == GRESADDRD) {
			if (sc->sc_proto == IPPROTO_UDP &&
			    satocsin(sa)->sin_port == 0) {
				error = EINVAL;
				break;
			}
			sc->sc_dst = satocsin(sa)->sin_addr;
			sc->sc_dstport = satocsin(sa)->sin_port;
		}
	recompute:
		if (sc->sc_proto == IPPROTO_UDP ||
		    (sc->sc_src.s_addr != INADDR_ANY &&
		     sc->sc_dst.s_addr != INADDR_ANY)) {
			rtcache_free(&sc->route);
			if (sc->sc_proto == IPPROTO_UDP) {
				if ((error = gre_kick(sc)) == 0)
					ifp->if_flags |= IFF_RUNNING;
				else
					ifp->if_flags &= ~IFF_RUNNING;
			}
			else if (gre_compute_route(sc) == 0)
				ifp->if_flags |= IFF_RUNNING;
			else
				ifp->if_flags &= ~IFF_RUNNING;
		}
		break;
	case GREGADDRS:
		sockaddr_in_init(&si, &sc->sc_src,
		    (sc->sc_proto == IPPROTO_UDP) ? sc->sc_srcport : 0);
		ifr->ifr_addr = *sintosa(&si);
		break;
	case GREGADDRD:
		sockaddr_in_init(&si, &sc->sc_dst,
		    (sc->sc_proto == IPPROTO_UDP) ? sc->sc_dstport : 0);
		ifr->ifr_addr = *sintosa(&si);
		break;
	case GREDSOCK:
		if (sc->sc_proto != IPPROTO_UDP) {
			error = EINVAL;
			break;
		}
		ifp->if_flags &= ~IFF_UP;
		gre_wakeup(sc);
		break;
	case GRESSOCK:
		if (sc->sc_proto != IPPROTO_UDP) {
			GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
			error = EINVAL;
			break;
		}
		/* getsock() will FILE_USE() and unlock the descriptor for us */
		if ((error = getsock(p->p_fd, (int)ifr->ifr_value, &fp)) != 0) {
			GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
			error = EINVAL;
			break;
		}
		so = (struct socket *)fp->f_data;
		if (so->so_type != SOCK_DGRAM) {
			GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
			FILE_UNUSE(fp, NULL);
			error = EINVAL;
			break;
		}
		/* check address */
		if ((error = gre_getnames(so, curlwp, &src, &dst)) != 0) {
			GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
			FILE_UNUSE(fp, NULL);
			break;
		}

                /* Increase reference count.  Now that our reference
                 * to the file descriptor is counted, this thread
                 * can release our "use" of the descriptor, but it
                 * will not be destroyed by some other thread's
                 * action.  This thread needs to release its use,
                 * too, because one and only one thread can have
                 * use of the descriptor at once.  The kernel thread
                 * will pick up the use if it needs it.
		 */

		fp->f_count++;
		GRE_DPRINTF(sc, "%s: l.%d f_count %d\n", __func__, __LINE__,
		    fp->f_count);
		FILE_UNUSE(fp, NULL);

		while (sc->sc_newsoparm.sp_fp != NULL && error == 0) {
			GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
			error = cv_timedwait_sig(&sc->sc_soparm_cv, &sc->sc_mtx,
					         MAX(1, hz / 2));
		}
		if (error == 0) {
			GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
			sc->sc_newsoparm.sp_fp = fp;
			ifp->if_flags |= IFF_UP;
		}

		if (error != 0 || (error = gre_kick(sc)) != 0) {
			GRE_DPRINTF(sc, "%s: l.%d\n", __func__, __LINE__);
			gre_closef(&fp, l);
			break;
		}
		/* fp does not any longer belong to this thread. */
		sc->sc_src = src.sin_addr;
		sc->sc_srcport = src.sin_port;
		sc->sc_dst = dst.sin_addr;
		sc->sc_dstport = dst.sin_port;
		GRE_DPRINTF(sc, "%s: sock 0x%08" PRIx32 " port %d -> "
		    "0x%08" PRIx32 " port %d\n", __func__,
		    src.sin_addr.s_addr, ntohs(src.sin_port),
		    dst.sin_addr.s_addr, ntohs(dst.sin_port));
		break;
	case SIOCSLIFPHYADDR:
		if (lifr->addr.ss_family != AF_INET ||
		    lifr->dstaddr.ss_family != AF_INET) {
			error = EAFNOSUPPORT;
			break;
		}
		if (lifr->addr.ss_len != sizeof(si) ||
		    lifr->dstaddr.ss_len != sizeof(si)) {
			error = EINVAL;
			break;
		}
		sc->sc_src = satocsin(&lifr->addr)->sin_addr;
		sc->sc_dst = satocsin(&lifr->dstaddr)->sin_addr;
		sc->sc_srcport = satocsin(&lifr->addr)->sin_port;
		sc->sc_dstport = satocsin(&lifr->dstaddr)->sin_port;
		goto recompute;
	case SIOCDIFPHYADDR:
		sc->sc_src.s_addr = INADDR_ANY;
		sc->sc_dst.s_addr = INADDR_ANY;
		sc->sc_srcport = 0;
		sc->sc_dstport = 0;
		goto recompute;
	case SIOCGLIFPHYADDR:
		if (sc->sc_src.s_addr == INADDR_ANY ||
		    sc->sc_dst.s_addr == INADDR_ANY) {
			error = EADDRNOTAVAIL;
			break;
		}
		sockaddr_in_init(satosin(&lifr->addr), &sc->sc_src,
		    (sc->sc_proto == IPPROTO_UDP) ? sc->sc_srcport : 0);
		sockaddr_in_init(satosin(&lifr->dstaddr), &sc->sc_dst,
		    (sc->sc_proto == IPPROTO_UDP) ? sc->sc_dstport : 0);
		break;
	default:
		error = EINVAL;
		break;
	}
	mutex_exit(&sc->sc_mtx);
	return error;
}

/*
 * Compute a route to our destination.
 */
static int
gre_compute_route(struct gre_softc *sc)
{
	struct route *ro;
	union {
		struct sockaddr		dst;
		struct sockaddr_in	dst4;
	} u;

	ro = &sc->route;

	memset(ro, 0, sizeof(*ro));
	sockaddr_in_init(&u.dst4, &sc->sc_dst, 0);
	rtcache_setdst(ro, &u.dst);

	rtcache_init(ro);

	if (ro->ro_rt == NULL || ro->ro_rt->rt_ifp->if_softc == sc) {
		GRE_DPRINTF(sc, "%s: route to %s %s\n", sc->sc_if.if_xname,
		    inet_ntoa(u.dst4.sin_addr),
		    (ro->ro_rt == NULL)
		        ?  "does not exist"
			: "loops back to ourself");
		rtcache_free(ro);
		return EADDRNOTAVAIL;
	}

	return 0;
}

/*
 * do a checksum of a buffer - much like in_cksum, which operates on
 * mbufs.
 */
u_int16_t
gre_in_cksum(u_int16_t *p, u_int len)
{
	u_int32_t sum = 0;
	int nwords = len >> 1;

	while (nwords-- != 0)
		sum += *p++;

	if (len & 1) {
		union {
			u_short w;
			u_char c[2];
		} u;
		u.c[0] = *(u_char *)p;
		u.c[1] = 0;
		sum += u.w;
	}

	/* end-around-carry */
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	return ~sum;
}
#endif

void	greattach(int);

/* ARGSUSED */
void
greattach(int count)
{
#ifdef INET
	LIST_INIT(&gre_softc_list);
	if_clone_attach(&gre_cloner);
#endif
}
