/*	$NetBSD: keysock.c,v 1.39.4.1 2005/04/29 11:29:36 kent Exp $	*/
/*	$KAME: keysock.c,v 1.32 2003/08/22 05:45:08 itojun Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: keysock.c,v 1.39.4.1 2005/04/29 11:29:36 kent Exp $");

#include "opt_inet.h"

/* This code has derived from sys/net/rtsock.c on FreeBSD2.2.5 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/queue.h>

#include <net/raw_cb.h>
#include <net/route.h>
#include <netinet/in.h>

#include <net/pfkeyv2.h>
#include <netkey/keydb.h>
#include <netkey/key.h>
#include <netkey/keysock.h>
#include <netkey/key_debug.h>

#include <machine/stdarg.h>

struct sockaddr key_dst = { 2, PF_KEY, };
struct sockaddr key_src = { 2, PF_KEY, };

struct pfkeystat pfkeystat;

static int key_receive __P((struct socket *, struct mbuf **, struct uio *,
	struct mbuf **, struct mbuf **, int *));

static int key_sendup0 __P((struct rawcb *, struct mbuf *, int, int));

static int
key_receive(struct socket *so, struct mbuf **paddr, struct uio *uio,
	struct mbuf **mp0, struct mbuf **controlp, int *flagsp)
{
	struct rawcb *rp = sotorawcb(so);
	struct keycb *kp = (struct keycb *)rp;
	int error;
	int s;

	error = (*kp->kp_receive)(so, paddr, uio, mp0, controlp, flagsp);

	/*
	 * now we might have enough receive buffer space.
	 * pull packets from kp_queue as many as possible.
	 */
	s = splsoftnet();
	while (/*CONSTCOND*/ 1) {
		struct mbuf *m;

		m = kp->kp_queue;
		if (m == NULL || sbspace(&so->so_rcv) < m->m_pkthdr.len)
			break;
		kp->kp_queue = m->m_nextpkt;
		m->m_nextpkt = NULL; /* safety */
		if (key_sendup0(rp, m, 0, 1))
			break;
	}
	splx(s);

	return error;
}

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
		kp = (struct keycb *)malloc(sizeof(*kp), M_PCB,
		    M_WAITOK|M_ZERO);
		so->so_pcb = (caddr_t)kp;
		kp->kp_receive = so->so_receive;
		so->so_receive = key_receive;
	}
	if (req == PRU_DETACH && kp) {
		int af = kp->kp_raw.rcb_proto.sp_protocol;
		struct mbuf *n;

		if (af == PF_KEY)
			key_cb.key_count--;
		key_cb.any_count--;

		key_freereg(so);

		while (kp->kp_queue) {
			n = kp->kp_queue->m_nextpkt;
			kp->kp_queue->m_nextpkt = NULL;
			m_freem(kp->kp_queue);
			kp->kp_queue = n;
		}
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

		if (af == PF_KEY)
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
		panic("key_output: NULL pointer was passed.");

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
key_sendup0(rp, m, promisc, canwait)
	struct rawcb *rp;
	struct mbuf *m;
	int promisc;
	int canwait;
{
	struct keycb *kp = (struct keycb *)rp;
	struct mbuf *n;
	int error = 0;

	if (promisc) {
		struct sadb_msg *pmsg;

		M_PREPEND(m, sizeof(struct sadb_msg), M_NOWAIT);
		if (m && m->m_len < sizeof(struct sadb_msg))
			m = m_pullup(m, sizeof(struct sadb_msg));
		if (!m) {
			pfkeystat.in_nomem++;
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

	if (canwait) {
		if (kp->kp_queue) {
			for (n = kp->kp_queue; n && n->m_nextpkt;
			    n = n->m_nextpkt)
				;
			n->m_nextpkt = m;
			m = kp->kp_queue;
			kp->kp_queue = NULL;
		} else
			m->m_nextpkt = NULL;	/* just for safety */
	} else
		m->m_nextpkt = NULL;

	for (; m && error == 0; m = n) {
		n = m->m_nextpkt;

		if (canwait &&
		    sbspace(&rp->rcb_socket->so_rcv) < m->m_pkthdr.len) {
			error = EAGAIN;
			goto recovery;
		}

		m->m_nextpkt = NULL;

		if (!sbappendaddr(&rp->rcb_socket->so_rcv,
		    (struct sockaddr *)&key_src, m, NULL)) {
			pfkeystat.in_nomem++;
			error = ENOBUFS;
			goto recovery;
		} else {
			sorwakeup(rp->rcb_socket);
			error = 0;
		}
	}
	return (error);

recovery:
	if (kp->kp_queue) {
		/*
		 * kp_queue != NULL implies !canwait.
		 */
		KASSERT(!canwait);
		KASSERT(m->m_nextpkt == NULL);
		/*
		 * insert m to the head of queue, as normally mbuf on the queue
		 * is less important than others.
		 */
		if (m) {
			m->m_nextpkt = kp->kp_queue;
			kp->kp_queue = m;
		}
	} else {
		/* recover the queue */
		if (!m) {
			/* first ENOBUFS case */
			kp->kp_queue = n;
		} else {
			kp->kp_queue = m;
			m->m_nextpkt = n;
		}
	}
	return (error);
}

/* so can be NULL if target != KEY_SENDUP_ONE */
int
key_sendup_mbuf(so, m, target)
	struct socket *so;
	struct mbuf *m;
	int target;
{
	struct mbuf *n;
	struct keycb *kp;
	int sendup;
	struct rawcb *rp;
	int error = 0;
	int canwait;

	if (m == NULL)
		panic("key_sendup_mbuf: NULL pointer was passed.");
	if (so == NULL && target == KEY_SENDUP_ONE)
		panic("key_sendup_mbuf: NULL pointer was passed.");

	canwait = target & KEY_SENDUP_CANWAIT;
	target &= ~KEY_SENDUP_CANWAIT;

	pfkeystat.in_total++;
	pfkeystat.in_bytes += m->m_pkthdr.len;
	if (m->m_len < sizeof(struct sadb_msg)) {
		m = m_pullup(m, sizeof(struct sadb_msg));
		if (m == NULL) {
			pfkeystat.in_nomem++;
			return ENOBUFS;
		}
	}
	if (m->m_len >= sizeof(struct sadb_msg)) {
		struct sadb_msg *msg;
		msg = mtod(m, struct sadb_msg *);
		pfkeystat.in_msgtype[msg->sadb_msg_type]++;
	}

	for (rp = rawcb.lh_first; rp; rp = rp->rcb_list.le_next)
	{
		if (rp->rcb_proto.sp_family != PF_KEY)
			continue;
		if (rp->rcb_proto.sp_protocol &&
		    rp->rcb_proto.sp_protocol != PF_KEY_V2) {
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
				(void)key_sendup0(rp, n, 1, canwait);
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
			if (kp->kp_registered)
				sendup++;
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

		/*
		 * ignore error even if queue is full.  PF_KEY does not
		 * guarantee the delivery of the message.
		 * this is important when target == KEY_SENDUP_ALL.
		 */
		key_sendup0(rp, n, 0, canwait);

		n = NULL;
	}

	if (so) {
		error = key_sendup0(sotorawcb(so), m, 0, canwait);
		m = NULL;
	} else {
		error = 0;
		m_freem(m);
	}
	return error;
}


/*
 * Definitions of protocols supported in the KEY domain.
 */

DOMAIN_DEFINE(keydomain);

const struct protosw keysw[] = {
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

