/*	$NetBSD: altq_priq.c,v 1.2.2.3 2001/04/21 17:46:11 bouyer Exp $	*/
/*	$KAME: altq_priq.c,v 1.1 2000/10/18 09:15:23 kjc Exp $	*/
/*
 * Copyright (C) 2000
 *	Sony Computer Science Laboratories Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY SONY CSL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL SONY CSL OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * priority queue
 */

#if defined(__FreeBSD__) || defined(__NetBSD__)
#include "opt_altq.h"
#if (__FreeBSD__ != 2)
#include "opt_inet.h"
#ifdef __FreeBSD__
#include "opt_inet6.h"
#endif
#endif
#endif /* __FreeBSD__ || __NetBSD__ */

#ifdef ALTQ_PRIQ  /* priq is enabled by ALTQ_PRIQ option in opt_altq.h */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_types.h>

#include <altq/altq.h>
#include <altq/altq_conf.h>
#include <altq/altq_priq.h>

/*
 * function prototypes
 */
static struct priq_if *priq_attach __P((struct ifaltq *, u_int));
static int priq_detach __P((struct priq_if *));
static int priq_clear_interface __P((struct priq_if *));
static int priq_request __P((struct ifaltq *, int, void *));
static void priq_purge __P((struct priq_if *));
static struct priq_class *priq_class_create __P((struct priq_if *,
						 int, int, int));
static int priq_class_destroy __P((struct priq_class *));
static int priq_enqueue __P((struct ifaltq *, struct mbuf *,
			     struct altq_pktattr *));
static struct mbuf *priq_dequeue __P((struct ifaltq *, int));

static int priq_addq __P((struct priq_class *, struct mbuf *));
static struct mbuf *priq_getq __P((struct priq_class *));
static struct mbuf *priq_pollq __P((struct priq_class *));
static void priq_purgeq __P((struct priq_class *));

int priqopen __P((dev_t, int, int, struct proc *));
int priqclose __P((dev_t, int, int, struct proc *));
int priqioctl __P((dev_t, ioctlcmd_t, caddr_t, int, struct proc *));
static int priqcmd_if_attach __P((struct priq_interface *));
static int priqcmd_if_detach __P((struct priq_interface *));
static int priqcmd_add_class __P((struct priq_add_class *));
static int priqcmd_delete_class __P((struct priq_delete_class *));
static int priqcmd_modify_class __P((struct priq_modify_class *));
static int priqcmd_add_filter __P((struct priq_add_filter *));
static int priqcmd_delete_filter __P((struct priq_delete_filter *));
static int priqcmd_class_stats __P((struct priq_class_stats *));
static void get_class_stats __P((struct class_stats *, struct priq_class *));
static struct priq_class *clh_to_clp __P((struct priq_if *, u_long));
static u_long clp_to_clh __P((struct priq_class *));

/* pif_list keeps all priq_if's allocated. */
static struct priq_if *pif_list = NULL;

static struct priq_if *
priq_attach(ifq, bandwidth)
	struct ifaltq *ifq;
	u_int bandwidth;
{
	struct priq_if *pif;

	MALLOC(pif, struct priq_if *, sizeof(struct priq_if),
	       M_DEVBUF, M_WAITOK);
	if (pif == NULL)
		return (NULL);
	bzero(pif, sizeof(struct priq_if));
	pif->pif_bandwidth = bandwidth;
	pif->pif_maxpri = -1;
	pif->pif_ifq = ifq;

	/* add this state to the priq list */
	pif->pif_next = pif_list;
	pif_list = pif;

	return (pif);
}

static int
priq_detach(pif)
	struct priq_if *pif;
{
	(void)priq_clear_interface(pif);

	/* remove this interface from the pif list */
	if (pif_list == pif)
		pif_list = pif->pif_next;
	else {
		struct priq_if *p;
	
		for (p = pif_list; p != NULL; p = p->pif_next)
			if (p->pif_next == pif) {
				p->pif_next = pif->pif_next;
				break;
			}
		ASSERT(p != NULL);
	}

	FREE(pif, M_DEVBUF);
	return (0);
}

/*
 * bring the interface back to the initial state by discarding
 * all the filters and classes.
 */
static int
priq_clear_interface(pif)
	struct priq_if *pif;
{
	struct priq_class	*cl;
	int pri;

	/* free the filters for this interface */
	acc_discard_filters(&pif->pif_classifier, NULL, 1);

	/* clear out the classes */
	for (pri = 0; pri <= pif->pif_maxpri; pri++)
		if ((cl = pif->pif_classes[pri]) != NULL)
			priq_class_destroy(cl);

	return (0);
}

static int
priq_request(ifq, req, arg)
	struct ifaltq *ifq;
	int req;
	void *arg;
{
	struct priq_if	*pif = (struct priq_if *)ifq->altq_disc;

	switch (req) {
	case ALTRQ_PURGE:
		priq_purge(pif);
		break;
	}
	return (0);
}

/* discard all the queued packets on the interface */
static void
priq_purge(pif)
	struct priq_if *pif;
{
	struct priq_class *cl;
	int pri;

	for (pri = 0; pri <= pif->pif_maxpri; pri++) {
		if ((cl = pif->pif_classes[pri]) != NULL && !qempty(cl->cl_q))
			priq_purgeq(cl);
	}
	if (ALTQ_IS_ENABLED(pif->pif_ifq))
		pif->pif_ifq->ifq_len = 0;
}

static struct priq_class *
priq_class_create(pif, pri, qlimit, flags)
	struct priq_if *pif;
	int pri, qlimit, flags;
{
	struct priq_class *cl;
	int s;

#ifndef ALTQ_RED
	if (flags & PRCF_RED) {
		printf("priq_class_create: RED not configured for PRIQ!\n");
		return (NULL);
	}
#endif

	if ((cl = pif->pif_classes[pri]) != NULL) {
		/* modify the class instead of creating a new one */
		s = splnet();
		if (!qempty(cl->cl_q))
			priq_purgeq(cl);
		splx(s);
#ifdef ALTQ_RIO
		if (q_is_rio(cl->cl_q))
			rio_destroy((rio_t *)cl->cl_red);
#endif
#ifdef ALTQ_RED
		if (q_is_red(cl->cl_q))
			red_destroy(cl->cl_red);
#endif
	} else {
		MALLOC(cl, struct priq_class *, sizeof(struct priq_class),
		       M_DEVBUF, M_WAITOK);
		if (cl == NULL)
			return (NULL);
		bzero(cl, sizeof(struct priq_class));

		MALLOC(cl->cl_q, class_queue_t *, sizeof(class_queue_t),
		       M_DEVBUF, M_WAITOK);
		if (cl->cl_q == NULL)
			goto err_ret;
		bzero(cl->cl_q, sizeof(class_queue_t));
	}

	pif->pif_classes[pri] = cl;
	if (flags & PRCF_DEFAULTCLASS)
		pif->pif_default = cl;
	if (qlimit == 0)
		qlimit = 50;  /* use default */
	qlimit(cl->cl_q) = qlimit;
	qtype(cl->cl_q) = Q_DROPTAIL;
	qlen(cl->cl_q) = 0;
	cl->cl_flags = flags;
	cl->cl_pri = pri;
	if (pri > pif->pif_maxpri)
		pif->pif_maxpri = pri;
	cl->cl_pif = pif;
	cl->cl_handle = (u_long)cl;  /* XXX: just a pointer to this class */

#ifdef ALTQ_RED
	if (flags & (PRCF_RED|PRCF_RIO)) {
		int red_flags, red_pkttime;

		red_flags = 0;
		if (flags & PRCF_ECN)
			red_flags |= REDF_ECN;
#ifdef ALTQ_RIO
		if (flags & PRCF_CLEARDSCP)
			red_flags |= RIOF_CLEARDSCP;
#endif
		if (pif->pif_bandwidth == 0)
			red_pkttime = 1000 * 1000 * 1000; /* 1 sec */
		else
			red_pkttime = (int64_t)pif->pif_ifq->altq_ifp->if_mtu
			  * 1000 * 1000 * 1000 / (pif->pif_bandwidth / 8);
#ifdef ALTQ_RIO
		if (flags & PRCF_RIO) {
			cl->cl_red = (red_t *)rio_alloc(0, NULL,
						red_flags, red_pkttime);
			if (cl->cl_red != NULL)
				qtype(cl->cl_q) = Q_RIO;
		} else 
#endif
		if (flags & PRCF_RED) {
			cl->cl_red = red_alloc(0, 0, 0, 0,
					       red_flags, red_pkttime);
			if (cl->cl_red != NULL)
				qtype(cl->cl_q) = Q_RED;
		}
	}
#endif /* ALTQ_RED */

	return (cl);

 err_ret:
	if (cl->cl_red != NULL) {
#ifdef ALTQ_RIO
		if (q_is_rio(cl->cl_q))
			rio_destroy((rio_t *)cl->cl_red);
#endif
#ifdef ALTQ_RED
		if (q_is_red(cl->cl_q))
			red_destroy(cl->cl_red);
#endif
	}
	if (cl->cl_q != NULL)
		FREE(cl->cl_q, M_DEVBUF);
	FREE(cl, M_DEVBUF);
	return (NULL);
}

static int
priq_class_destroy(cl)
	struct priq_class *cl;
{
	struct priq_if *pif;
	int s, pri;

	s = splnet();

	/* delete filters referencing to this class */
	acc_discard_filters(&cl->cl_pif->pif_classifier, cl, 0);

	if (!qempty(cl->cl_q))
		priq_purgeq(cl);

	pif = cl->cl_pif;
	pif->pif_classes[cl->cl_pri] = NULL;
	if (pif->pif_maxpri == cl->cl_pri) {
		for (pri = cl->cl_pri; pri >= 0; pri--)
			if (pif->pif_classes[pri] != NULL) {
				pif->pif_maxpri = pri;
				break;
			}
		if (pri < 0)
			pif->pif_maxpri = -1;
	}
	splx(s);

	if (cl->cl_red != NULL) {
#ifdef ALTQ_RIO
		if (q_is_rio(cl->cl_q))
			rio_destroy((rio_t *)cl->cl_red);
#endif
#ifdef ALTQ_RED
		if (q_is_red(cl->cl_q))
			red_destroy(cl->cl_red);
#endif
	}
	FREE(cl->cl_q, M_DEVBUF);
	FREE(cl, M_DEVBUF);
	return (0);
}

/*
 * priq_enqueue is an enqueue function to be registered to
 * (*altq_enqueue) in struct ifaltq.
 */
static int 
priq_enqueue(ifq, m, pktattr)
	struct ifaltq *ifq;
	struct mbuf *m;
	struct altq_pktattr *pktattr;
{
	struct priq_if	*pif = (struct priq_if *)ifq->altq_disc;
	struct priq_class *cl;
	int len;

	/* grab class set by classifier */
	if (pktattr == NULL || (cl = pktattr->pattr_class) == NULL)
		cl = pif->pif_default;
	cl->cl_pktattr = pktattr;  /* save proto hdr used by ECN */

	len = m_pktlen(m);
	if (priq_addq(cl, m) != 0) {
		/* drop occurred.  mbuf was freed in priq_addq. */
		PKTCNTR_ADD(&cl->cl_dropcnt, len);
		return (ENOBUFS);
	}
	IFQ_INC_LEN(ifq);

	/* successfully queued. */
	return (0);
}

/*
 * priq_dequeue is a dequeue function to be registered to
 * (*altq_dequeue) in struct ifaltq.
 *
 * note: ALTDQ_POLL returns the next packet without removing the packet
 *	from the queue.  ALTDQ_REMOVE is a normal dequeue operation.
 *	ALTDQ_REMOVE must return the same packet if called immediately
 *	after ALTDQ_POLL.
 */
static struct mbuf *
priq_dequeue(ifq, op)
	struct ifaltq	*ifq;
	int		op;
{
	struct priq_if	*pif = (struct priq_if *)ifq->altq_disc;
	struct priq_class *cl;
	struct mbuf *m;
	int pri;

	if (IFQ_IS_EMPTY(ifq))
		/* no packet in the queue */
		return (NULL);

	for (pri = pif->pif_maxpri;  pri >= 0; pri--) {
		if ((cl = pif->pif_classes[pri]) != NULL &&
		    !qempty(cl->cl_q)) {
			if (op == ALTDQ_POLL)
				return (priq_pollq(cl));

			m = priq_getq(cl);
			if (m != NULL) {
				IFQ_DEC_LEN(ifq);
				if (qempty(cl->cl_q))
					cl->cl_period++;
				PKTCNTR_ADD(&cl->cl_xmitcnt, m_pktlen(m));
			}
			return (m);
		}
	}
	return (NULL);
}

static int
priq_addq(cl, m)
	struct priq_class *cl;
	struct mbuf *m;
{

#ifdef ALTQ_RIO
	if (q_is_rio(cl->cl_q))
		return rio_addq((rio_t *)cl->cl_red, cl->cl_q, m,
				cl->cl_pktattr);
#endif
#ifdef ALTQ_RED
	if (q_is_red(cl->cl_q))
		return red_addq(cl->cl_red, cl->cl_q, m, cl->cl_pktattr);
#endif
	if (qlen(cl->cl_q) >= qlimit(cl->cl_q)) {
		m_freem(m);
		return (-1);
	}
	
	if (cl->cl_flags & PRCF_CLEARDSCP)
		write_dsfield(m, cl->cl_pktattr, 0);

	_addq(cl->cl_q, m);

	return (0);
}

static struct mbuf *
priq_getq(cl)
	struct priq_class *cl;
{
#ifdef ALTQ_RIO
	if (q_is_rio(cl->cl_q))
		return rio_getq((rio_t *)cl->cl_red, cl->cl_q);
#endif
#ifdef ALTQ_RED
	if (q_is_red(cl->cl_q))
		return red_getq(cl->cl_red, cl->cl_q);
#endif
	return _getq(cl->cl_q);
}

static struct mbuf *
priq_pollq(cl)
	struct priq_class *cl;
{
	return qhead(cl->cl_q);
}

static void
priq_purgeq(cl)
	struct priq_class *cl;
{
	struct mbuf *m;

	if (qempty(cl->cl_q))
		return;

	while ((m = _getq(cl->cl_q)) != NULL) {
		PKTCNTR_ADD(&cl->cl_dropcnt, m_pktlen(m));
		m_freem(m);
	}
	ASSERT(qlen(cl->cl_q) == 0);
}

/*
 * priq device interface
 */
int
priqopen(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct proc *p;
{
	/* everything will be done when the queueing scheme is attached. */
	return 0;
}

int
priqclose(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct proc *p;
{
	struct priq_if *pif;
	int err, error = 0;

	while ((pif = pif_list) != NULL) {
		/* destroy all */
		if (ALTQ_IS_ENABLED(pif->pif_ifq))
			altq_disable(pif->pif_ifq);

		err = altq_detach(pif->pif_ifq);
		if (err == 0)
			err = priq_detach(pif);
		if (err != 0 && error == 0)
			error = err;
	}

	return error;
}

int
priqioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	ioctlcmd_t cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	struct priq_if *pif;
	struct priq_interface *ifacep;
	int	error = 0;

	/* check super-user privilege */
	switch (cmd) {
	case PRIQ_GETSTATS:
		break;
	default:
#if (__FreeBSD_version > 400000)
		if ((error = suser(p)) != 0)
			return (error);
#else
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			return (error);
#endif
		break;
	}
    
	switch (cmd) {

	case PRIQ_IF_ATTACH:
		error = priqcmd_if_attach((struct priq_interface *)addr);
		break;

	case PRIQ_IF_DETACH:
		error = priqcmd_if_detach((struct priq_interface *)addr);
		break;

	case PRIQ_ENABLE:
	case PRIQ_DISABLE:
	case PRIQ_CLEAR:
		ifacep = (struct priq_interface *)addr;
		if ((pif = altq_lookup(ifacep->ifname,
				       ALTQT_PRIQ)) == NULL) {
			error = EBADF;
			break;
		}

		switch (cmd) {
		case PRIQ_ENABLE:
			if (pif->pif_default == NULL) {
#if 1
				printf("priq: no default class\n");
#endif
				error = EINVAL;
				break;
			}
			error = altq_enable(pif->pif_ifq);
			break;

		case PRIQ_DISABLE:
			error = altq_disable(pif->pif_ifq);
			break;

		case PRIQ_CLEAR:
			priq_clear_interface(pif);
			break;
		}
		break;

	case PRIQ_ADD_CLASS:
		error = priqcmd_add_class((struct priq_add_class *)addr);
		break;

	case PRIQ_DEL_CLASS:
		error = priqcmd_delete_class((struct priq_delete_class *)addr);
		break;

	case PRIQ_MOD_CLASS:
		error = priqcmd_modify_class((struct priq_modify_class *)addr);
		break;

	case PRIQ_ADD_FILTER:
		error = priqcmd_add_filter((struct priq_add_filter *)addr);
		break;

	case PRIQ_DEL_FILTER:
		error = priqcmd_delete_filter((struct priq_delete_filter *)addr);
		break;

	case PRIQ_GETSTATS:
		error = priqcmd_class_stats((struct priq_class_stats *)addr);
		break;

	default:
		error = EINVAL;
		break;
	}
	return error;
}

static int
priqcmd_if_attach(ap)
	struct priq_interface *ap;
{
	struct priq_if *pif;
	struct ifnet *ifp;
	int error;
	
	if ((ifp = ifunit(ap->ifname)) == NULL)
		return (ENXIO);

	if ((pif = priq_attach(&ifp->if_snd, ap->arg)) == NULL)
		return (ENOMEM);
	
	/*
	 * set PRIQ to this ifnet structure.
	 */
	if ((error = altq_attach(&ifp->if_snd, ALTQT_PRIQ, pif,
				 priq_enqueue, priq_dequeue, priq_request,
				 &pif->pif_classifier, acc_classify)) != 0)
		(void)priq_detach(pif);

	return (error);
}

static int
priqcmd_if_detach(ap)
	struct priq_interface *ap;
{
	struct priq_if *pif;
	int error;

	if ((pif = altq_lookup(ap->ifname, ALTQT_PRIQ)) == NULL)
		return (EBADF);
	
	if (ALTQ_IS_ENABLED(pif->pif_ifq))
		altq_disable(pif->pif_ifq);

	if ((error = altq_detach(pif->pif_ifq)))
		return (error);

	return priq_detach(pif);
}

static int
priqcmd_add_class(ap)
	struct priq_add_class *ap;
{
	struct priq_if *pif;
	struct priq_class *cl;

	if ((pif = altq_lookup(ap->iface.ifname, ALTQT_PRIQ)) == NULL)
		return (EBADF);

	if (ap->pri < 0 || ap->pri >= PRIQ_MAXPRI)
		return (EINVAL);

	if ((cl = priq_class_create(pif, ap->pri,
				    ap->qlimit, ap->flags)) == NULL)
		return (ENOMEM);
		
	/* return a class handle to the user */
	ap->class_handle = clp_to_clh(cl);
	return (0);
}

static int
priqcmd_delete_class(ap)
	struct priq_delete_class *ap;
{
	struct priq_if *pif;
	struct priq_class *cl;

	if ((pif = altq_lookup(ap->iface.ifname, ALTQT_PRIQ)) == NULL)
		return (EBADF);

	if ((cl = clh_to_clp(pif, ap->class_handle)) == NULL)
		return (EINVAL);
	
	return priq_class_destroy(cl);
}

static int
priqcmd_modify_class(ap)
	struct priq_modify_class *ap;
{
	struct priq_if *pif;
	struct priq_class *cl;

	if ((pif = altq_lookup(ap->iface.ifname, ALTQT_PRIQ)) == NULL)
		return (EBADF);

	if (ap->pri < 0 || ap->pri >= PRIQ_MAXPRI)
		return (EINVAL);

	if ((cl = clh_to_clp(pif, ap->class_handle)) == NULL)
		return (EINVAL);

	/*
	 * if priority is changed, move the class to the new priority
	 */
	if (pif->pif_classes[ap->pri] != cl) {
		if (pif->pif_classes[ap->pri] != NULL)
			return (EEXIST);
		pif->pif_classes[cl->cl_pri] = NULL;
		pif->pif_classes[ap->pri] = cl;
		cl->cl_pri = ap->pri;
	}

	/* call priq_class_create to change class parameters */
	if ((cl = priq_class_create(pif, ap->pri,
				    ap->qlimit, ap->flags)) == NULL)
		return (ENOMEM);
	return 0;
}

static int
priqcmd_add_filter(ap)
	struct priq_add_filter *ap;
{
	struct priq_if *pif;
	struct priq_class *cl;

	if ((pif = altq_lookup(ap->iface.ifname, ALTQT_PRIQ)) == NULL)
		return (EBADF);

	if ((cl = clh_to_clp(pif, ap->class_handle)) == NULL)
		return (EINVAL);

	return acc_add_filter(&pif->pif_classifier, &ap->filter,
			      cl, &ap->filter_handle);
}

static int
priqcmd_delete_filter(ap)
	struct priq_delete_filter *ap;
{
	struct priq_if *pif;

	if ((pif = altq_lookup(ap->iface.ifname, ALTQT_PRIQ)) == NULL)
		return (EBADF);

	return acc_delete_filter(&pif->pif_classifier,
				 ap->filter_handle);
}

static int
priqcmd_class_stats(ap)
	struct priq_class_stats *ap;
{
	struct priq_if *pif;
	struct priq_class *cl;
	struct class_stats stats, *usp;
	int	pri, error;
	
	if ((pif = altq_lookup(ap->iface.ifname, ALTQT_PRIQ)) == NULL)
		return (EBADF);

	ap->maxpri = pif->pif_maxpri;

	/* then, read the next N classes in the tree */
	usp = ap->stats;
	for (pri = 0; pri <= pif->pif_maxpri; pri++) {
		cl = pif->pif_classes[pri];
		if (cl != NULL)
			get_class_stats(&stats, cl);
		else
			bzero(&stats, sizeof(stats));
		if ((error = copyout((caddr_t)&stats, (caddr_t)usp++,
				     sizeof(stats))) != 0)
			return (error);
	}
	return (0);
}

static void get_class_stats(sp, cl)
	struct class_stats *sp;
	struct priq_class *cl;
{
	sp->class_handle = clp_to_clh(cl);

	sp->qlength = qlen(cl->cl_q);
	sp->period = cl->cl_period;
	sp->xmitcnt = cl->cl_xmitcnt;
	sp->dropcnt = cl->cl_dropcnt;

	sp->qtype = qtype(cl->cl_q);
#ifdef ALTQ_RED
	if (q_is_red(cl->cl_q))
		red_getstats(cl->cl_red, &sp->red[0]);
#endif
#ifdef ALTQ_RIO
	if (q_is_rio(cl->cl_q))
		rio_getstats((rio_t *)cl->cl_red, &sp->red[0]);
#endif

}

/* convert a class handle to the corresponding class pointer */
static struct priq_class *
clh_to_clp(pif, chandle)
	struct priq_if *pif;
	u_long chandle;
{
	struct priq_class *cl;

	cl = (struct priq_class *)chandle;
	if (chandle != ALIGN(cl)) {
#if 1
		printf("clh_to_cl: unaligned pointer %p\n", cl);
#endif
		return (NULL);
	}

	if (cl == NULL || cl->cl_handle != chandle || cl->cl_pif != pif)
		return (NULL);
	return (cl);
}

/* convert a class pointer to the corresponding class handle */
static u_long
clp_to_clh(cl)
	struct priq_class *cl;
{
	return (cl->cl_handle);
}

#ifdef KLD_MODULE

static struct altqsw priq_sw =
	{"priq", priqopen, priqclose, priqioctl};

ALTQ_MODULE(altq_priq, ALTQT_PRIQ, &priq_sw);

#endif /* KLD_MODULE */

#endif /* ALTQ_PRIQ */
