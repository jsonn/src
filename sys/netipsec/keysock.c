/*	$NetBSD: keysock.c,v 1.5.2.4 2004/09/21 13:37:48 skrll Exp $	*/
/*	$FreeBSD: src/sys/netipsec/keysock.c,v 1.3.2.1 2003/01/24 05:11:36 sam Exp $	*/
/*	$KAME: keysock.c,v 1.25 2001/08/13 20:07:41 itojun Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: keysock.c,v 1.5.2.4 2004/09/21 13:37:48 skrll Exp $");

#include "opt_ipsec.h"

/* This code has derived from sys/net/rtsock.c on FreeBSD2.2.5 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/domain.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <net/raw_cb.h>
#include <net/route.h>

#include <net/pfkeyv2.h>
#include <netipsec/key.h>
#include <netipsec/keysock.h>
#include <netipsec/key_debug.h>

#include <netipsec/ipsec_osdep.h>

#include <machine/stdarg.h>

typedef int	pr_output_t (struct mbuf *, struct socket *);

struct key_cb {
	int key_count;
	int any_count;
};
static struct key_cb key_cb;

static struct sockaddr key_dst = { 2, PF_KEY, };
static struct sockaddr key_src = { 2, PF_KEY, };


static int key_sendup0 __P((struct rawcb *, struct mbuf *, int, int));

struct pfkeystat pfkeystat;

int key_registered_sb_max = (NMBCLUSTERS * MHLEN); /* XXX arbitrary */

/* XXX sysctl */
#ifdef __FreeBSD__
SYSCTL_INT(_net_key, OID_AUTO, registered_sbmax, CTLFLAG_RD, 
    &key_registered_sb_max , 0, "Maximum kernel-to-user PFKEY datagram size");
#endif

/*
 * key_output()
 */
int
key_output(struct mbuf *m, ...)
{
	struct sadb_msg *msg;
	int len, error = 0;
	int s;
	struct socket *so;
	va_list ap;

	va_start(ap, m);
	so = va_arg(ap, struct socket *);
	va_end(ap);

	if (m == 0)
		panic("key_output: NULL pointer was passed.\n");

	pfkeystat.out_total++;
	pfkeystat.out_bytes += m->m_pkthdr.len;

	len = m->m_pkthdr.len;
	if (len < sizeof(struct sadb_msg)) {
		pfkeystat.out_tooshort++;
		error = EINVAL;
		goto end;
	}

	if (m->m_len < sizeof(struct sadb_msg)) {
		if ((m = m_pullup(m, sizeof(struct sadb_msg))) == 0) {
			pfkeystat.out_nomem++;
			error = ENOBUFS;
			goto end;
		}
	}

	if ((m->m_flags & M_PKTHDR) == 0)
		panic("key_output: not M_PKTHDR ??");

	KEYDEBUG(KEYDEBUG_KEY_DUMP, kdebug_mbuf(m));

	msg = mtod(m, struct sadb_msg *);
	pfkeystat.out_msgtype[msg->sadb_msg_type]++;
	if (len != PFKEY_UNUNIT64(msg->sadb_msg_len)) {
		pfkeystat.out_invlen++;
		error = EINVAL;
		goto end;
	}

	/*XXX giant lock*/
	s = splsoftnet();
	error = key_parse(m, so);
	m = NULL;
	splx(s);
end:
	if (m)
		m_freem(m);
	return error;
}

/*
 * send message to the socket.
 */
static int
key_sendup0(rp, m, promisc, sbprio)
	struct rawcb *rp;
	struct mbuf *m;
	int promisc;
{
	int error;
	int ok;

	if (promisc) {
		struct sadb_msg *pmsg;

		M_PREPEND(m, sizeof(struct sadb_msg), M_DONTWAIT);
		if (m && m->m_len < sizeof(struct sadb_msg))
			m = m_pullup(m, sizeof(struct sadb_msg));
		if (!m) {
			pfkeystat.in_nomem++;
			m_freem(m);
			return ENOBUFS;
		}
		m->m_pkthdr.len += sizeof(*pmsg);

		pmsg = mtod(m, struct sadb_msg *);
		bzero(pmsg, sizeof(*pmsg));
		pmsg->sadb_msg_version = PF_KEY_V2;
		pmsg->sadb_msg_type = SADB_X_PROMISC;
		pmsg->sadb_msg_len = PFKEY_UNIT64(m->m_pkthdr.len);
		/* pid and seq? */

		pfkeystat.in_msgtype[pmsg->sadb_msg_type]++;
	}

	if (sbprio == 0)
		ok = sbappendaddr(&rp->rcb_socket->so_rcv,
			       (struct sockaddr *)&key_src, m, NULL);
	else
		ok = sbappendaddrchain(&rp->rcb_socket->so_rcv,
			       (struct sockaddr *)&key_src, m, sbprio);

	  if (!ok) {
		pfkeystat.in_nomem++;
		m_freem(m);
		error = ENOBUFS;
	} else
		error = 0;
	sorwakeup(rp->rcb_socket);
	return error;
}

/* XXX this interface should be obsoleted. */
int
key_sendup(so, msg, len, target)
	struct socket *so;
	struct sadb_msg *msg;
	u_int len;
	int target;	/*target of the resulting message*/
{
	struct mbuf *m, *n, *mprev;
	int tlen;

	/* sanity check */
	if (so == 0 || msg == 0)
		panic("key_sendup: NULL pointer was passed.\n");

	KEYDEBUG(KEYDEBUG_KEY_DUMP,
		printf("key_sendup: \n");
		kdebug_sadb(msg));

	/*
	 * we increment statistics here, just in case we have ENOBUFS
	 * in this function.
	 */
	pfkeystat.in_total++;
	pfkeystat.in_bytes += len;
	pfkeystat.in_msgtype[msg->sadb_msg_type]++;

	/*
	 * Get mbuf chain whenever possible (not clusters),
	 * to save socket buffer.  We'll be generating many SADB_ACQUIRE
	 * messages to listening key sockets.  If we simply allocate clusters,
	 * sbappendaddr() will raise ENOBUFS due to too little sbspace().
	 * sbspace() computes # of actual data bytes AND mbuf region.
	 *
	 * TODO: SADB_ACQUIRE filters should be implemented.
	 */
	tlen = len;
	m = mprev = NULL;
	while (tlen > 0) {
		if (tlen == len) {
			MGETHDR(n, M_DONTWAIT, MT_DATA);
			n->m_len = MHLEN;
		} else {
			MGET(n, M_DONTWAIT, MT_DATA);
			n->m_len = MLEN;
		}
		if (!n) {
			pfkeystat.in_nomem++;
			return ENOBUFS;
		}
		if (tlen >= MCLBYTES) {	/*XXX better threshold? */
			MCLGET(n, M_DONTWAIT);
			if ((n->m_flags & M_EXT) == 0) {
				m_free(n);
				m_freem(m);
				pfkeystat.in_nomem++;
				return ENOBUFS;
			}
			n->m_len = MCLBYTES;
		}

		if (tlen < n->m_len)
			n->m_len = tlen;
		n->m_next = NULL;
		if (m == NULL)
			m = mprev = n;
		else {
			mprev->m_next = n;
			mprev = n;
		}
		tlen -= n->m_len;
		n = NULL;
	}
	m->m_pkthdr.len = len;
	m->m_pkthdr.rcvif = NULL;
	m_copyback(m, 0, len, (caddr_t)msg);

	/* avoid duplicated statistics */
	pfkeystat.in_total--;
	pfkeystat.in_bytes -= len;
	pfkeystat.in_msgtype[msg->sadb_msg_type]--;

	return key_sendup_mbuf(so, m, target);
}

/* so can be NULL if target != KEY_SENDUP_ONE */
int
key_sendup_mbuf(so, m, target /*, sbprio */)
	struct socket *so;
	struct mbuf *m;
	int target;
{
	struct mbuf *n;
	struct keycb *kp;
	int sendup;
	struct rawcb *rp;
	int error = 0;
	int sbprio = 0; /* XXX should be a parameter */

	if (m == NULL)
		panic("key_sendup_mbuf: NULL pointer was passed.\n");
	if (so == NULL && target == KEY_SENDUP_ONE)
		panic("key_sendup_mbuf: NULL pointer was passed.\n");
	
	/*
	 * RFC 2367 says ACQUIRE and other kernel-generated messages
	 * are special. We treat all KEY_SENDUP_REGISTERED messages
	 * as special, delivering them to all registered sockets
	 * even if the socket is at or above its so->so_rcv.sb_max limits.
	 * The only constraint is that the  so_rcv data fall below
	 * key_registered_sb_max.
	 * Doing that check here avoids reworking every key_sendup_mbuf()
	 * in the short term. . The rework will be done after a technical
	 * conensus that this approach is appropriate.
 	 */
	if (target == KEY_SENDUP_REGISTERED) {
		sbprio = SB_PRIO_BESTEFFORT;
	}

	pfkeystat.in_total++;
	pfkeystat.in_bytes += m->m_pkthdr.len;
	if (m->m_len < sizeof(struct sadb_msg)) {
#if 1
		m = m_pullup(m, sizeof(struct sadb_msg));
		if (m == NULL) {
			pfkeystat.in_nomem++;
			return ENOBUFS;
		}
#else
		/* don't bother pulling it up just for stats */
#endif
	}
	if (m->m_len >= sizeof(struct sadb_msg)) {
		struct sadb_msg *msg;
		msg = mtod(m, struct sadb_msg *);
		pfkeystat.in_msgtype[msg->sadb_msg_type]++;
	}

	LIST_FOREACH(rp, &rawcb_list, rcb_list)
	{
		struct socket * kso = rp->rcb_socket;
		if (rp->rcb_proto.sp_family != PF_KEY)
			continue;
		if (rp->rcb_proto.sp_protocol
		 && rp->rcb_proto.sp_protocol != PF_KEY_V2) {
			continue;
		}

		kp = (struct keycb *)rp;

		/*
		 * If you are in promiscuous mode, and when you get broadcasted
		 * reply, you'll get two PF_KEY messages.
		 * (based on pf_key@inner.net message on 14 Oct 1998)
		 */
		if (((struct keycb *)rp)->kp_promisc) {
			if ((n = m_copy(m, 0, (int)M_COPYALL)) != NULL) {
				(void)key_sendup0(rp, n, 1, 0);
				n = NULL;
			}
		}

		/* the exact target will be processed later */
		if (so && sotorawcb(so) == rp)
			continue;

		sendup = 0;
		switch (target) {
		case KEY_SENDUP_ONE:
			/* the statement has no effect */
			if (so && sotorawcb(so) == rp)
				sendup++;
			break;
		case KEY_SENDUP_ALL:
			sendup++;
			break;
		case KEY_SENDUP_REGISTERED:
			if (kp->kp_registered) {
				if (kso->so_rcv.sb_cc <= key_registered_sb_max)
					sendup++;
			  	else
			  		printf("keysock: "
					       "registered sendup dropped, "
					       "sb_cc %ld max %d\n",
					       kso->so_rcv.sb_cc,
					       key_registered_sb_max);
			}
			break;
		}
		pfkeystat.in_msgtarget[target]++;

		if (!sendup)
			continue;

		if ((n = m_copy(m, 0, (int)M_COPYALL)) == NULL) {
			m_freem(m);
			pfkeystat.in_nomem++;
			return ENOBUFS;
		}

		if ((error = key_sendup0(rp, n, 0, 0)) != 0) {
			m_freem(m);
			return error;
		}

		n = NULL;
	}

	/* The 'later' time for processing the exact target has arrived */
	if (so) {
		error = key_sendup0(sotorawcb(so), m, 0, sbprio);
		m = NULL;
	} else {
		error = 0;
		m_freem(m);
	}
	return error;
}

#ifdef __FreeBSD__

/*
 * key_abort()
 * derived from net/rtsock.c:rts_abort()
 */
static int
key_abort(struct socket *so)
{
	int s, error;
	s = splnet(); 	/* FreeBSD */
	error = raw_usrreqs.pru_abort(so);
	splx(s);
	return error;
}

/*
 * key_attach()
 * derived from net/rtsock.c:rts_attach()
 */
static int
key_attach(struct socket *so, int proto, struct proc *td)
{
	struct keycb *kp;
	int s, error;

	if (sotorawcb(so) != 0)
		return EISCONN;	/* XXX panic? */
	kp = (struct keycb *)malloc(sizeof *kp, M_PCB, M_WAITOK|M_ZERO); /* XXX */
	if (kp == 0)
		return ENOBUFS;

	/*
	 * The spl[soft]net() is necessary to block protocols from sending
	 * error notifications (like RTM_REDIRECT or RTM_LOSING) while
	 * this PCB is extant but incompletely initialized.
	 * Probably we should try to do more of this work beforehand and
	 * eliminate the spl.
	 */
	s = splnet();	/* FreeBSD */
	so->so_pcb = (caddr_t)kp;
	error = raw_usrreqs.pru_attach(so, proto, td);
	kp = (struct keycb *)sotorawcb(so);
	if (error) {
		free(kp, M_PCB);
		so->so_pcb = (caddr_t) 0;
		splx(s);
		return error;
	}

	kp->kp_promisc = kp->kp_registered = 0;

	if (kp->kp_raw.rcb_proto.sp_protocol == PF_KEY) /* XXX: AF_KEY */
		key_cb.key_count++;
	key_cb.any_count++;
	kp->kp_raw.rcb_laddr = &key_src;
	kp->kp_raw.rcb_faddr = &key_dst;
	soisconnected(so);
	so->so_options |= SO_USELOOPBACK;

	splx(s);
	return 0;
}

/*
 * key_bind()
 * derived from net/rtsock.c:rts_bind()
 */
static int
key_bind(struct socket *so, struct sockaddr *nam, struct proc *td)
{
	int s, error;
	s = splnet();	/* FreeBSD */
	error = raw_usrreqs.pru_bind(so, nam, td); /* xxx just EINVAL */
	splx(s);
	return error;
}

/*
 * key_connect()
 * derived from net/rtsock.c:rts_connect()
 */
static int
key_connect(struct socket *so, struct sockaddr *nam, struct proc *td)
{
	int s, error;
	s = splnet();	/* FreeBSD */
	error = raw_usrreqs.pru_connect(so, nam, td); /* XXX just EINVAL */
	splx(s);
	return error;
}

/*
 * key_detach()
 * derived from net/rtsock.c:rts_detach()
 */
static int
key_detach(struct socket *so)
{
	struct keycb *kp = (struct keycb *)sotorawcb(so);
	int s, error;

	s = splnet();	/* FreeBSD */
	if (kp != 0) {
		if (kp->kp_raw.rcb_proto.sp_protocol
		    == PF_KEY) /* XXX: AF_KEY */
			key_cb.key_count--;
		key_cb.any_count--;

		key_freereg(so);
	}
	error = raw_usrreqs.pru_detach(so);
	splx(s);
	return error;
}

/*
 * key_disconnect()
 * derived from net/rtsock.c:key_disconnect()
 */
static int
key_disconnect(struct socket *so)
{
	int s, error;
	s = splnet();	/* FreeBSD */
	error = raw_usrreqs.pru_disconnect(so);
	splx(s);
	return error;
}

/*
 * key_peeraddr()
 * derived from net/rtsock.c:rts_peeraddr()
 */
static int
key_peeraddr(struct socket *so, struct sockaddr **nam)
{
	int s, error;
	s = splnet();	/* FreeBSD */
	error = raw_usrreqs.pru_peeraddr(so, nam);
	splx(s);
	return error;
}

/*
 * key_send()
 * derived from net/rtsock.c:rts_send()
 */
static int
key_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *nam,
	 struct mbuf *control, struct proc *td)
{
	int s, error;
	s = splnet();	/* FreeBSD */
	error = raw_usrreqs.pru_send(so, flags, m, nam, control, td);
	splx(s);
	return error;
}

/*
 * key_shutdown()
 * derived from net/rtsock.c:rts_shutdown()
 */
static int
key_shutdown(struct socket *so)
{
	int s, error;
	s = splnet();	/* FreeBSD */
	error = raw_usrreqs.pru_shutdown(so);
	splx(s);
	return error;
}

/*
 * key_sockaddr()
 * derived from net/rtsock.c:rts_sockaddr()
 */
static int
key_sockaddr(struct socket *so, struct sockaddr **nam)
{
	int s, error;
	s = splnet();	/* FreeBSD */
	error = raw_usrreqs.pru_sockaddr(so, nam);
	splx(s);
	return error;
}
#else /*!__FreeBSD__ -- traditional proto_usrreq() switch */

/*
 * key_usrreq()
 * derived from net/rtsock.c:route_usrreq()
 */
int
key_usrreq(so, req, m, nam, control, p)
	struct socket *so;
	int req;
	struct mbuf *m, *nam, *control;
	struct proc *p;
{
	int error = 0;
	struct keycb *kp = (struct keycb *)sotorawcb(so);
	int s;

	s = splsoftnet();
	if (req == PRU_ATTACH) {
		kp = (struct keycb *)malloc(sizeof(*kp), M_PCB, M_WAITOK);
		so->so_pcb = (caddr_t)kp;
		if (so->so_pcb)
			bzero(so->so_pcb, sizeof(*kp));
	}
	if (req == PRU_DETACH && kp) {
		int af = kp->kp_raw.rcb_proto.sp_protocol;
		if (af == PF_KEY) /* XXX: AF_KEY */
			key_cb.key_count--;
		key_cb.any_count--;

		key_freereg(so);
	}

	error = raw_usrreq(so, req, m, nam, control, p);
	m = control = NULL;	/* reclaimed in raw_usrreq */
	kp = (struct keycb *)sotorawcb(so);
	if (req == PRU_ATTACH && kp) {
		int af = kp->kp_raw.rcb_proto.sp_protocol;
		if (error) {
			pfkeystat.sockerr++;
			free((caddr_t)kp, M_PCB);
			so->so_pcb = (caddr_t) 0;
			splx(s);
			return (error);
		}

		kp->kp_promisc = kp->kp_registered = 0;

		if (af == PF_KEY) /* XXX: AF_KEY */
			key_cb.key_count++;
		key_cb.any_count++;
		kp->kp_raw.rcb_laddr = &key_src;
		kp->kp_raw.rcb_faddr = &key_dst;
		soisconnected(so);
		so->so_options |= SO_USELOOPBACK;
	}
	splx(s);
	return (error);
}
#endif /*!__FreeBSD__*/

/* sysctl */
#ifdef SYSCTL_NODE
SYSCTL_NODE(_net, PF_KEY, key, CTLFLAG_RW, 0, "Key Family");
#endif /* SYSCTL_NODE */

/*
 * Definitions of protocols supported in the KEY domain.
 */

/* This extern declaration is all that's common... */
extern struct domain keydomain;

#ifdef __FreeBSD__
struct pr_usrreqs key_usrreqs = {
	key_abort, pru_accept_notsupp, key_attach, key_bind,
	key_connect,
	pru_connect2_notsupp, pru_control_notsupp, key_detach,
	key_disconnect, pru_listen_notsupp, key_peeraddr,
	pru_rcvd_notsupp,
	pru_rcvoob_notsupp, key_send, pru_sense_null, key_shutdown,
	key_sockaddr, sosend, soreceive, sopoll
};

struct protosw keysw[] = {
{ SOCK_RAW,	&keydomain,	PF_KEY_V2,	PR_ATOMIC|PR_ADDR,
  0,		(pr_output_t *)key_output,	raw_ctlinput, 0,
  0,
  raw_init,	0,		0,		0,
  &key_usrreqs
}
};

static void
key_init0(void)
{
	bzero((caddr_t)&key_cb, sizeof(key_cb));
	key_init();
}

struct domain keydomain =
    { PF_KEY, "key", key_init0, 0, 0,
      keysw, &keysw[sizeof(keysw)/sizeof(keysw[0])] };

DOMAIN_SET(key);

#else /* !__FreeBSD__ */


struct protosw keysw[] = {
{ SOCK_RAW,	&keydomain,	PF_KEY_V2,	PR_ATOMIC|PR_ADDR,
  0,		key_output,	raw_ctlinput,	0,
  key_usrreq,
  raw_init,	0,		0,		0,
  NULL,
}
};

struct domain keydomain =
    { PF_KEY, "key", key_init, 0, 0,
      keysw, &keysw[sizeof(keysw)/sizeof(keysw[0])] };

#endif
