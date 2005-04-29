/*	$NetBSD: if_tun.c,v 1.75.4.1 2005/04/29 11:29:31 kent Exp $	*/

/*
 * Copyright (c) 1988, Julian Onions <jpo@cs.nott.ac.uk>
 * Nottingham University 1987.
 *
 * This source may be freely distributed, however I would be interested
 * in any changes that are made.
 *
 * This driver takes packets off the IP i/f and hands them up to a
 * user process to have its wicked way with. This driver has its
 * roots in a similar driver written by Phil Cockcroft (formerly) at
 * UCL. This driver is based much more on read/write/poll mode of
 * operation though.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_tun.c,v 1.75.4.1 2005/04/29 11:29:31 kent Exp $");

#include "opt_inet.h"
#include "opt_ns.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/buf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <sys/file.h>
#include <sys/signalvar.h>
#include <sys/conf.h>

#include <machine/cpu.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>


#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_inarp.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#include "bpfilter.h"
#if NBPFILTER > 0
#include <sys/time.h>
#include <net/bpf.h>
#endif

#include <net/if_tun.h>

#define TUNDEBUG	if (tundebug) printf
int	tundebug = 0;

extern int ifqmaxlen;
void	tunattach __P((int));
LIST_HEAD(, tun_softc) tun_softc_list;
LIST_HEAD(, tun_softc) tunz_softc_list;
static struct simplelock tun_softc_lock;

int	tun_ioctl __P((struct ifnet *, u_long, caddr_t));
int	tun_output __P((struct ifnet *, struct mbuf *, struct sockaddr *,
		       struct rtentry *rt));
int	tun_clone_create __P((struct if_clone *, int));
int	tun_clone_destroy __P((struct ifnet *));

struct if_clone tun_cloner =
    IF_CLONE_INITIALIZER("tun", tun_clone_create, tun_clone_destroy);

static void tunattach0 __P((struct tun_softc *));
static void tuninit __P((struct tun_softc *));
#ifdef ALTQ
static void tunstart __P((struct ifnet *));
#endif
static struct tun_softc *tun_find_unit __P((dev_t));
static struct tun_softc *tun_find_zunit __P((int));

dev_type_open(tunopen);
dev_type_close(tunclose);
dev_type_read(tunread);
dev_type_write(tunwrite);
dev_type_ioctl(tunioctl);
dev_type_poll(tunpoll);
dev_type_kqfilter(tunkqfilter);

const struct cdevsw tun_cdevsw = {
	tunopen, tunclose, tunread, tunwrite, tunioctl,
	nostop, notty, tunpoll, nommap, tunkqfilter,
};

void
tunattach(unused)
	int unused;
{

	simple_lock_init(&tun_softc_lock);
	LIST_INIT(&tun_softc_list);
	LIST_INIT(&tunz_softc_list);
	if_clone_attach(&tun_cloner);
}

/*
 * Find driver instance from dev_t.
 * Call at splnet().
 * Returns with tp locked (if found).
 */
static struct tun_softc *
tun_find_unit(dev)
	dev_t dev;
{
	struct tun_softc *tp;
	int unit = minor(dev);

	simple_lock(&tun_softc_lock);
	LIST_FOREACH(tp, &tun_softc_list, tun_list)
		if (unit == tp->tun_unit)
			break;
	if (tp)
		simple_lock(&tp->tun_lock);
	simple_unlock(&tun_softc_lock);

	return (tp);
}

/*
 * Find zombie driver instance by unit number.
 * Call at splnet().
 * Remove tp from list and return it unlocked (if found).
 */
static struct tun_softc *
tun_find_zunit(unit)
	int unit;
{
	struct tun_softc *tp;

	simple_lock(&tun_softc_lock);
	LIST_FOREACH(tp, &tunz_softc_list, tun_list)
		if (unit == tp->tun_unit)
			break;
	if (tp)
		LIST_REMOVE(tp, tun_list);
	simple_unlock(&tun_softc_lock);
#ifdef DIAGNOSTIC
	if (tp != NULL && (tp->tun_flags & (TUN_INITED|TUN_OPEN)) != TUN_OPEN)
		printf("tun%d: inconsistent flags: %x\n", unit, tp->tun_flags);
#endif

	return (tp);
}

int
tun_clone_create(ifc, unit)
	struct if_clone *ifc;
	int unit;
{
	struct tun_softc *tp;

	if ((tp = tun_find_zunit(unit)) == NULL) {
		/* Allocate a new instance */
		tp = malloc(sizeof(struct tun_softc), M_DEVBUF, M_WAITOK);
		(void)memset(tp, 0, sizeof(struct tun_softc));

		tp->tun_unit = unit;
		simple_lock_init(&tp->tun_lock);
	} else {
		/* Revive tunnel instance; clear ifp part */
		(void)memset(&tp->tun_if, 0, sizeof(struct ifnet));
	}

	(void)snprintf(tp->tun_if.if_xname, sizeof(tp->tun_if.if_xname),
			"%s%d", ifc->ifc_name, unit);
	tunattach0(tp);
	tp->tun_flags |= TUN_INITED;

	simple_lock(&tun_softc_lock);
	LIST_INSERT_HEAD(&tun_softc_list, tp, tun_list);
	simple_unlock(&tun_softc_lock);

	return (0);
}

void
tunattach0(tp)
	struct tun_softc *tp;
{
	struct ifnet *ifp;

	ifp = &tp->tun_if;
	ifp->if_softc = tp;
	ifp->if_mtu = TUNMTU;
	ifp->if_ioctl = tun_ioctl;
	ifp->if_output = tun_output;
#ifdef ALTQ
	ifp->if_start = tunstart;
#endif
	ifp->if_flags = IFF_POINTOPOINT;
	ifp->if_type = IFT_TUNNEL;
	ifp->if_snd.ifq_maxlen = ifqmaxlen;
	ifp->if_collisions = 0;
	ifp->if_ierrors = 0;
	ifp->if_oerrors = 0;
	ifp->if_ipackets = 0;
	ifp->if_opackets = 0;
	ifp->if_ibytes   = 0;
	ifp->if_obytes   = 0;
	ifp->if_dlt = DLT_NULL;
	IFQ_SET_READY(&ifp->if_snd);
	if_attach(ifp);
	if_alloc_sadl(ifp);
#if NBPFILTER > 0
	bpfattach(ifp, DLT_NULL, sizeof(u_int32_t));
#endif
}

int
tun_clone_destroy(ifp)
	struct ifnet *ifp;
{
	struct tun_softc *tp = (void *)ifp;
	int s, zombie = 0;

	s = splnet();
	simple_lock(&tun_softc_lock);
	simple_lock(&tp->tun_lock);
	LIST_REMOVE(tp, tun_list);
	if (tp->tun_flags & TUN_OPEN) {
		/* Hang on to storage until last close */
		zombie = 1;
		tp->tun_flags &= ~TUN_INITED;
		LIST_INSERT_HEAD(&tunz_softc_list, tp, tun_list);
	}
	simple_unlock(&tun_softc_lock);

	IF_PURGE(&ifp->if_snd);
	ifp->if_flags &= ~IFF_RUNNING;

	if (tp->tun_flags & TUN_RWAIT) {
		tp->tun_flags &= ~TUN_RWAIT;
		wakeup((caddr_t)tp);
	}
	if (tp->tun_flags & TUN_ASYNC && tp->tun_pgid)
		fownsignal(tp->tun_pgid, SIGIO, POLL_HUP, 0, NULL);

	selwakeup(&tp->tun_rsel);

	simple_unlock(&tp->tun_lock);
	splx(s);

#if NBPFILTER > 0
	bpfdetach(ifp);
#endif
	if_detach(ifp);

	if (!zombie)
		free(tp, M_DEVBUF);

	return (0);
}

/*
 * tunnel open - must be superuser & the device must be
 * configured in
 */
int
tunopen(dev, flag, mode, p)
	dev_t	dev;
	int	flag, mode;
	struct proc *p;
{
	struct ifnet	*ifp;
	struct tun_softc *tp;
	int	s, error;

	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);

	s = splnet();
	tp = tun_find_unit(dev);

	if (tp == NULL) {
		(void)tun_clone_create(&tun_cloner, minor(dev));
		tp = tun_find_unit(dev);
		if (tp == NULL) {
			error = ENXIO;
			goto out_nolock;
		}
	}

	if (tp->tun_flags & TUN_OPEN) {
		error = EBUSY;
		goto out;
	}

	ifp = &tp->tun_if;
	tp->tun_flags |= TUN_OPEN;
	TUNDEBUG("%s: open\n", ifp->if_xname);
out:
	simple_unlock(&tp->tun_lock);
out_nolock:
	splx(s);
	return (error);
}

/*
 * tunclose - close the device - mark i/f down & delete
 * routing info
 */
int
tunclose(dev, flag, mode, p)
	dev_t	dev;
	int	flag;
	int	mode;
	struct proc *p;
{
	int	s;
	struct tun_softc *tp;
	struct ifnet	*ifp;

	s = splnet();
	if ((tp = tun_find_zunit(minor(dev))) != NULL) {
		/* interface was "destroyed" before the close */
		free(tp, M_DEVBUF);
		goto out_nolock;
	}

	if ((tp = tun_find_unit(dev)) == NULL)
		goto out_nolock;

	ifp = &tp->tun_if;

	tp->tun_flags &= ~TUN_OPEN;

	/*
	 * junk all pending output
	 */
	IFQ_PURGE(&ifp->if_snd);

	if (ifp->if_flags & IFF_UP) {
		if_down(ifp);
		if (ifp->if_flags & IFF_RUNNING) {
			/* find internet addresses and delete routes */
			struct ifaddr *ifa;
			IFADDR_FOREACH(ifa, ifp) {
#ifdef INET
				if (ifa->ifa_addr->sa_family == AF_INET) {
					rtinit(ifa, (int)RTM_DELETE,
					       tp->tun_flags & TUN_DSTADDR
							? RTF_HOST
							: 0);
				}
#endif
			}
		}
	}
	tp->tun_pgid = 0;
	selnotify(&tp->tun_rsel, 0);

	TUNDEBUG ("%s: closed\n", ifp->if_xname);
	simple_unlock(&tp->tun_lock);
out_nolock:
	splx(s);
	return (0);
}

/*
 * Call at splnet() with tp locked.
 */
static void
tuninit(tp)
	struct tun_softc *tp;
{
	struct ifnet	*ifp = &tp->tun_if;
	struct ifaddr	*ifa;

	TUNDEBUG("%s: tuninit\n", ifp->if_xname);

	ifp->if_flags |= IFF_UP | IFF_RUNNING;

	tp->tun_flags &= ~(TUN_IASET|TUN_DSTADDR);
	IFADDR_FOREACH(ifa, ifp) {
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET) {
			struct sockaddr_in *sin;

			sin = satosin(ifa->ifa_addr);
			if (sin && sin->sin_addr.s_addr)
				tp->tun_flags |= TUN_IASET;

			if (ifp->if_flags & IFF_POINTOPOINT) {
				sin = satosin(ifa->ifa_dstaddr);
				if (sin && sin->sin_addr.s_addr)
					tp->tun_flags |= TUN_DSTADDR;
			}
		}
#endif
	}

	return;
}

/*
 * Process an ioctl request.
 */
int
tun_ioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t	data;
{
	int		error = 0, s;
	struct tun_softc *tp = (struct tun_softc *)(ifp->if_softc);

	s = splnet();
	simple_lock(&tp->tun_lock);

	switch (cmd) {
	case SIOCSIFADDR:
		tuninit(tp);
		TUNDEBUG("%s: address set\n", ifp->if_xname);
		break;
	case SIOCSIFDSTADDR:
		tuninit(tp);
		TUNDEBUG("%s: destination address set\n", ifp->if_xname);
		break;
	case SIOCSIFBRDADDR:
		TUNDEBUG("%s: broadcast address set\n", ifp->if_xname);
		break;
	case SIOCSIFMTU: {
		struct ifreq *ifr = (struct ifreq *) data;
		if (ifr->ifr_mtu > TUNMTU || ifr->ifr_mtu < 576) {
		    error = EINVAL;
		    break;
		}
		TUNDEBUG("%s: interface mtu set\n", ifp->if_xname);
		ifp->if_mtu = ifr->ifr_mtu;
		break;
	}
	case SIOCADDMULTI:
	case SIOCDELMULTI: {
		struct ifreq *ifr = (struct ifreq *) data;
		if (ifr == 0) {
	        	error = EAFNOSUPPORT;           /* XXX */
			break;
		}
		switch (ifr->ifr_addr.sa_family) {
#ifdef INET
		case AF_INET:
			break;
#endif
		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;
	}
	case SIOCSIFFLAGS:
		break;
	default:
		error = EINVAL;
	}

	simple_unlock(&tp->tun_lock);
	splx(s);
	return (error);
}

/*
 * tun_output - queue packets from higher level ready to put out.
 */
int
tun_output(ifp, m0, dst, rt)
	struct ifnet   *ifp;
	struct mbuf    *m0;
	struct sockaddr *dst;
	struct rtentry *rt;
{
	struct tun_softc *tp = ifp->if_softc;
	int		s;
	int		error;
#ifdef INET
	int		mlen;
#endif
	ALTQ_DECL(struct altq_pktattr pktattr;)

	s = splnet();
	simple_lock(&tp->tun_lock);
	TUNDEBUG ("%s: tun_output\n", ifp->if_xname);

	if ((tp->tun_flags & TUN_READY) != TUN_READY) {
		TUNDEBUG ("%s: not ready 0%o\n", ifp->if_xname,
			  tp->tun_flags);
		m_freem (m0);
		error = EHOSTDOWN;
		goto out;
	}

	/*
	 * if the queueing discipline needs packet classification,
	 * do it before prepending link headers.
	 */
	IFQ_CLASSIFY(&ifp->if_snd, m0, dst->sa_family, &pktattr);

#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap_af(ifp->if_bpf, dst->sa_family, m0);
#endif

	switch(dst->sa_family) {
#ifdef INET
	case AF_INET:
		if (tp->tun_flags & TUN_PREPADDR) {
			/* Simple link-layer header */
			M_PREPEND(m0, dst->sa_len, M_DONTWAIT);
			if (m0 == NULL) {
				IF_DROP(&ifp->if_snd);
				error = ENOBUFS;
				goto out;
			}
			bcopy(dst, mtod(m0, char *), dst->sa_len);
		}
		/* FALLTHROUGH */
	case AF_UNSPEC:
		IFQ_ENQUEUE(&ifp->if_snd, m0, &pktattr, error);
		if (error) {
			ifp->if_collisions++;
			error = EAFNOSUPPORT;
			goto out;
		}
		mlen = m0->m_pkthdr.len;
		ifp->if_opackets++;
		ifp->if_obytes += mlen;
		break;
#endif
	default:
		m_freem(m0);
		error = EAFNOSUPPORT;
		goto out;
	}

	if (tp->tun_flags & TUN_RWAIT) {
		tp->tun_flags &= ~TUN_RWAIT;
		wakeup((caddr_t)tp);
	}
	if (tp->tun_flags & TUN_ASYNC && tp->tun_pgid)
		fownsignal(tp->tun_pgid, SIGIO, POLL_IN, POLLIN|POLLRDNORM,
		    NULL);

	selnotify(&tp->tun_rsel, 0);
out:
	simple_unlock(&tp->tun_lock);
	splx(s);
	return (0);
}

/*
 * the cdevsw interface is now pretty minimal.
 */
int
tunioctl(dev, cmd, data, flag, p)
	dev_t		dev;
	u_long		cmd;
	caddr_t		data;
	int		flag;
	struct proc	*p;
{
	struct tun_softc *tp;
	int s, error = 0;

	s = splnet();
	tp = tun_find_unit(dev);

	/* interface was "destroyed" already */
	if (tp == NULL) {
		error = ENXIO;
		goto out_nolock;
	}

	switch (cmd) {
	case TUNSDEBUG:
		tundebug = *(int *)data;
		break;

	case TUNGDEBUG:
		*(int *)data = tundebug;
		break;

	case TUNSIFMODE:
		switch (*(int *)data & (IFF_POINTOPOINT|IFF_BROADCAST)) {
		case IFF_POINTOPOINT:
		case IFF_BROADCAST:
			if (tp->tun_if.if_flags & IFF_UP) {
				error = EBUSY;
				goto out;
			}
			tp->tun_if.if_flags &=
				~(IFF_BROADCAST|IFF_POINTOPOINT|IFF_MULTICAST);
			tp->tun_if.if_flags |= *(int *)data;
			break;
		default:
			error = EINVAL;
			goto out;
		}
		break;

	case TUNSLMODE:
		if (*(int *)data)
			tp->tun_flags |= TUN_PREPADDR;
		else
			tp->tun_flags &= ~TUN_PREPADDR;
		break;

	case FIONBIO:
		if (*(int *)data)
			tp->tun_flags |= TUN_NBIO;
		else
			tp->tun_flags &= ~TUN_NBIO;
		break;

	case FIOASYNC:
		if (*(int *)data)
			tp->tun_flags |= TUN_ASYNC;
		else
			tp->tun_flags &= ~TUN_ASYNC;
		break;

	case FIONREAD:
		if (tp->tun_if.if_snd.ifq_head)
			*(int *)data = tp->tun_if.if_snd.ifq_head->m_pkthdr.len;
		else
			*(int *)data = 0;
		break;

	case TIOCSPGRP:
	case FIOSETOWN:
		error = fsetown(p, &tp->tun_pgid, cmd, data);
		break;

	case TIOCGPGRP:
	case FIOGETOWN:
		error = fgetown(p, tp->tun_pgid, cmd, data);
		break;

	default:
		error = ENOTTY;
	}

out:
	simple_unlock(&tp->tun_lock);
out_nolock:
	splx(s);
	return (error);
}

/*
 * The cdevsw read interface - reads a packet at a time, or at
 * least as much of a packet as can be read.
 */
int
tunread(dev, uio, ioflag)
	dev_t		dev;
	struct uio	*uio;
	int		ioflag;
{
	struct tun_softc *tp;
	struct ifnet	*ifp;
	struct mbuf	*m, *m0;
	int		error = 0, len, s, index;

	s = splnet();
	tp = tun_find_unit(dev);

	/* interface was "destroyed" already */
	if (tp == NULL) {
		error = ENXIO;
		goto out_nolock;
	}

	index = tp->tun_if.if_index;
	ifp = &tp->tun_if;

	TUNDEBUG ("%s: read\n", ifp->if_xname);
	if ((tp->tun_flags & TUN_READY) != TUN_READY) {
		TUNDEBUG ("%s: not ready 0%o\n", ifp->if_xname, tp->tun_flags);
		error = EHOSTDOWN;
		goto out;
	}

	tp->tun_flags &= ~TUN_RWAIT;

	do {
		IFQ_DEQUEUE(&ifp->if_snd, m0);
		if (m0 == 0) {
			if (tp->tun_flags & TUN_NBIO) {
				error = EWOULDBLOCK;
				goto out;
			}
			tp->tun_flags |= TUN_RWAIT;
			if (ltsleep((caddr_t)tp, PZERO|PCATCH|PNORELOCK,
					"tunread", 0, &tp->tun_lock) != 0) {
				error = EINTR;
				goto out_nolock;
			} else {
				/*
				 * Maybe the interface was destroyed while
				 * we were sleeping, so let's ensure that
				 * we're looking at the same (valid) tun
				 * interface before looping.
				 */
				tp = tun_find_unit(dev);
				if (tp == NULL) {
					error = ENXIO;
					goto out_nolock;
				}
				if (tp->tun_if.if_index != index) {
					error = ENXIO;
					goto out;
				}
			}
		}
	} while (m0 == 0);

	simple_unlock(&tp->tun_lock);
	splx(s);

	/* Copy the mbuf chain */
	while (m0 && uio->uio_resid > 0 && error == 0) {
		len = min(uio->uio_resid, m0->m_len);
		if (len != 0)
			error = uiomove(mtod(m0, caddr_t), len, uio);
		MFREE(m0, m);
		m0 = m;
	}

	if (m0) {
		TUNDEBUG("Dropping mbuf\n");
		m_freem(m0);
	}
	if (error)
		ifp->if_ierrors++;

	return (error);

out:
	simple_unlock(&tp->tun_lock);
out_nolock:
	splx(s);
	return (error);
}

/*
 * the cdevsw write interface - an atomic write is a packet - or else!
 */
int
tunwrite(dev, uio, ioflag)
	dev_t		dev;
	struct uio	*uio;
	int		ioflag;
{
	struct tun_softc *tp;
	struct ifnet	*ifp;
	struct mbuf	*top, **mp, *m;
	struct ifqueue	*ifq;
	struct sockaddr	dst;
	int		isr, error = 0, s, tlen, mlen;

	s = splnet();
	tp = tun_find_unit(dev);

	/* interface was "destroyed" already */
	if (tp == NULL) {
		error = ENXIO;
		goto out_nolock;
	}

	/* Unlock until we've got the data */
	simple_unlock(&tp->tun_lock);
	splx(s);

	ifp = &tp->tun_if;

	TUNDEBUG("%s: tunwrite\n", ifp->if_xname);

	if (tp->tun_flags & TUN_PREPADDR) {
		if (uio->uio_resid < sizeof(dst)) {
			error = EIO;
			goto out0;
		}
		error = uiomove((caddr_t)&dst, sizeof(dst), uio);
		if (dst.sa_len > sizeof(dst)) {
			/* Duh.. */
			char discard;
			int n = dst.sa_len - sizeof(dst);
			while (n--)
				if ((error = uiomove(&discard, 1, uio)) != 0) {
					goto out0;
				}
		}
	} else {
#ifdef INET
		dst.sa_family = AF_INET;
#endif
	}

	if (uio->uio_resid > TUNMTU) {
		TUNDEBUG("%s: len=%lu!\n", ifp->if_xname,
		    (unsigned long)uio->uio_resid);
		error = EIO;
		goto out0;
	}

	switch (dst.sa_family) {
#ifdef INET
	case AF_INET:
		ifq = &ipintrq;
		isr = NETISR_IP;
		break;
#endif
	default:
		error = EAFNOSUPPORT;
		goto out0;
	}

	tlen = uio->uio_resid;

	/* get a header mbuf */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		error = ENOBUFS;
		goto out0;
	}
	mlen = MHLEN;

	top = NULL;
	mp = &top;
	while (error == 0 && uio->uio_resid > 0) {
		m->m_len = min(mlen, uio->uio_resid);
		error = uiomove(mtod(m, caddr_t), m->m_len, uio);
		*mp = m;
		mp = &m->m_next;
		if (error == 0 && uio->uio_resid > 0) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				error = ENOBUFS;
				break;
			}
			mlen = MLEN;
		}
	}
	if (error) {
		if (top != NULL)
			m_freem (top);
		ifp->if_ierrors++;
		goto out0;
	}

	top->m_pkthdr.len = tlen;
	top->m_pkthdr.rcvif = ifp;

#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap_af(ifp->if_bpf, AF_INET, top);
#endif

	s = splnet();
	simple_lock(&tp->tun_lock);
	if ((tp->tun_flags & TUN_INITED) == 0) {
		/* Interface was destroyed */
		error = ENXIO;
		goto out;
	}
	if (IF_QFULL(ifq)) {
		IF_DROP(ifq);
		ifp->if_collisions++;
		m_freem(top);
		error = ENOBUFS;
		goto out;
	}

	IF_ENQUEUE(ifq, top);
	ifp->if_ipackets++;
	ifp->if_ibytes += tlen;
	schednetisr(isr);
out:
	simple_unlock(&tp->tun_lock);
out_nolock:
	splx(s);
out0:
	return (error);
}

#ifdef ALTQ
/*
 * Start packet transmission on the interface.
 * when the interface queue is rate-limited by ALTQ or TBR,
 * if_start is needed to drain packets from the queue in order
 * to notify readers when outgoing packets become ready.
 *
 * Should be called at splnet.
 */
static void
tunstart(ifp)
	struct ifnet *ifp;
{
	struct tun_softc *tp = ifp->if_softc;

	if (!ALTQ_IS_ENABLED(&ifp->if_snd) && !TBR_IS_ENABLED(&ifp->if_snd))
		return;

	simple_lock(&tp->tun_lock);
	if (!IF_IS_EMPTY(&ifp->if_snd)) {
		if (tp->tun_flags & TUN_RWAIT) {
			tp->tun_flags &= ~TUN_RWAIT;
			wakeup((caddr_t)tp);
		}
		if (tp->tun_flags & TUN_ASYNC && tp->tun_pgid)
			fownsignal(tp->tun_pgid, SIGIO, POLL_OUT,
				POLLOUT|POLLWRNORM, NULL);

		selwakeup(&tp->tun_rsel);
	}
	simple_unlock(&tp->tun_lock);
}
#endif /* ALTQ */
/*
 * tunpoll - the poll interface, this is only useful on reads
 * really. The write detect always returns true, write never blocks
 * anyway, it either accepts the packet or drops it.
 */
int
tunpoll(dev, events, p)
	dev_t		dev;
	int		events;
	struct proc	*p;
{
	struct tun_softc *tp;
	struct ifnet	*ifp;
	int		s, revents = 0;

	s = splnet();
	tp = tun_find_unit(dev);

	/* interface was "destroyed" already */
	if (tp == NULL)
		goto out_nolock;

	ifp = &tp->tun_if;

	TUNDEBUG("%s: tunpoll\n", ifp->if_xname);

	if (events & (POLLIN | POLLRDNORM)) {
		if (!IFQ_IS_EMPTY(&ifp->if_snd)) {
			TUNDEBUG("%s: tunpoll q=%d\n", ifp->if_xname,
			    ifp->if_snd.ifq_len);
			revents |= events & (POLLIN | POLLRDNORM);
		} else {
			TUNDEBUG("%s: tunpoll waiting\n", ifp->if_xname);
			selrecord(p, &tp->tun_rsel);
		}
	}

	if (events & (POLLOUT | POLLWRNORM))
		revents |= events & (POLLOUT | POLLWRNORM);

	simple_unlock(&tp->tun_lock);
out_nolock:
	splx(s);
	return (revents);
}

static void
filt_tunrdetach(struct knote *kn)
{
	struct tun_softc *tp = kn->kn_hook;
	int s;

	s = splnet();
	SLIST_REMOVE(&tp->tun_rsel.sel_klist, kn, knote, kn_selnext);
	splx(s);
}

static int
filt_tunread(struct knote *kn, long hint)
{
	struct tun_softc *tp = kn->kn_hook;
	struct ifnet *ifp = &tp->tun_if;
	struct mbuf *m;
	int s;

	s = splnet();
	IF_POLL(&ifp->if_snd, m);
	if (m == NULL) {
		splx(s);
		return (0);
	}

	for (kn->kn_data = 0; m != NULL; m = m->m_next)
		kn->kn_data += m->m_len;

	splx(s);
	return (1);
}

static const struct filterops tunread_filtops =
	{ 1, NULL, filt_tunrdetach, filt_tunread };

static const struct filterops tun_seltrue_filtops =
	{ 1, NULL, filt_tunrdetach, filt_seltrue };

int
tunkqfilter(dev_t dev, struct knote *kn)
{
	struct tun_softc *tp;
	struct klist *klist;
	int rv = 0, s;

	s = splnet();
	tp = tun_find_unit(dev);
	if (tp == NULL)
		goto out_nolock;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		klist = &tp->tun_rsel.sel_klist;
		kn->kn_fop = &tunread_filtops;
		break;

	case EVFILT_WRITE:
		klist = &tp->tun_rsel.sel_klist;
		kn->kn_fop = &tun_seltrue_filtops;
		break;

	default:
		rv = 1;
		goto out;
	}

	kn->kn_hook = tp;

	SLIST_INSERT_HEAD(klist, kn, kn_selnext);

out:
	simple_unlock(&tp->tun_lock);
out_nolock:
	splx(s);
	return (rv);
}
