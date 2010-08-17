/*	$NetBSD: ip_reass.c,v 1.2.4.2 2010/08/17 06:47:46 uebayasi Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1988, 1993
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
 *	@(#)ip_input.c	8.2 (Berkeley) 1/4/94
 */

/*
 * IP reassembly.
 *
 * Additive-Increase/Multiplicative-Decrease (AIMD) strategy for IP
 * reassembly queue buffer managment.
 *
 * We keep a count of total IP fragments (NB: not fragmented packets),
 * awaiting reassembly (ip_nfrags) and a limit (ip_maxfrags) on fragments.
 * If ip_nfrags exceeds ip_maxfrags the limit, we drop half the total
 * fragments in reassembly queues.  This AIMD policy avoids repeatedly
 * deleting single packets under heavy fragmentation load (e.g., from lossy
 * NFS peers).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ip_reass.c,v 1.2.4.2 2010/08/17 06:47:46 uebayasi Exp $");

#include <sys/param.h>
#include <sys/types.h>

#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/pool.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/in_proto.h>
#include <netinet/ip_private.h>
#include <netinet/in_var.h>

/*
 * IP datagram reassembly hashed queues, pool, lock and counters.
 */
#define	IPREASS_HASH_SHIFT	6
#define	IPREASS_HASH_SIZE	(1 << IPREASS_HASH_SHIFT)
#define	IPREASS_HASH_MASK	(IPREASS_HASH_SIZE - 1)
#define	IPREASS_HASH(x, y) \
	(((((x) & 0xf) | ((((x) >> 8) & 0xf) << 4)) ^ (y)) & IPREASS_HASH_MASK)

struct ipqhead	ipq[IPREASS_HASH_SIZE];
struct pool	ipqent_pool;
static int	ipq_locked;

static int	ip_nfragpackets;	/* packets in reass queue */
static int	ip_nfrags;		/* total fragments in reass queues */

static int	ip_maxfragpackets;	/* limit on packets. XXX sysctl */
static int	ip_maxfrags;		/* limit on fragments. XXX sysctl */

/*
 * IP reassembly queue structure.  Each fragment being reassembled is
 * attached to one of these structures.  They are timed out after ipq_ttl
 * drops to 0, and may also be reclaimed if memory becomes tight.
 */
struct ipq {
	LIST_ENTRY(ipq)	ipq_q;		/* to other reass headers */
	uint8_t		ipq_ttl;	/* time for reass q to live */
	uint8_t		ipq_p;		/* protocol of this fragment */
	uint16_t	ipq_id;		/* sequence id for reassembly */
	struct ipqehead	ipq_fragq;	/* to ip fragment queue */
	struct in_addr	ipq_src;
	struct in_addr	ipq_dst;
	uint16_t	ipq_nfrags;	/* frags in this queue entry */
	uint8_t 	ipq_tos;	/* TOS of this fragment */
};

/*
 * Cached copy of nmbclusters. If nbclusters is different,
 * recalculate IP parameters derived from nmbclusters.
 */
static int	ip_nmbclusters;			/* copy of nmbclusters */

/*
 * IP reassembly TTL machinery for multiplicative drop.
 */
static u_int	fragttl_histo[IPFRAGTTL + 1];

void		sysctl_ip_reass_setup(void);
static void	ip_nmbclusters_changed(void);

static struct ipq *	ip_reass_lookup(struct ip *, u_int *);
static struct mbuf *	ip_reass(struct ipqent *, struct ipq *, u_int);
static u_int		ip_reass_ttl_decr(u_int ticks);
static void		ip_reass_drophalf(void);
static void		ip_freef(struct ipq *);

/*
 * ip_reass_init:
 *
 *	Initialization of IP reassembly mechanism.
 */
void
ip_reass_init(void)
{
	int i;

	pool_init(&ipqent_pool, sizeof(struct ipqent), 0, 0, 0, "ipqepl",
	    NULL, IPL_VM);

	for (i = 0; i < IPREASS_HASH_SIZE; i++) {
		LIST_INIT(&ipq[i]);
	}
	ip_maxfragpackets = 200;
	ip_maxfrags = 0;
	ip_nmbclusters_changed();

	sysctl_ip_reass_setup();
}

static struct sysctllog *ip_reass_sysctllog;

void
sysctl_ip_reass_setup(void)
{

	sysctl_createv(&ip_reass_sysctllog, 0, NULL, NULL,
		CTLFLAG_PERMANENT,
		CTLTYPE_NODE, "net", NULL,
		NULL, 0, NULL, 0,
		CTL_NET, CTL_EOL);
	sysctl_createv(&ip_reass_sysctllog, 0, NULL, NULL,
		CTLFLAG_PERMANENT,
		CTLTYPE_NODE, "inet",
		SYSCTL_DESCR("PF_INET related settings"),
		NULL, 0, NULL, 0,
		CTL_NET, PF_INET, CTL_EOL);
	sysctl_createv(&ip_reass_sysctllog, 0, NULL, NULL,
		CTLFLAG_PERMANENT,
		CTLTYPE_NODE, "ip",
		SYSCTL_DESCR("IPv4 related settings"),
		NULL, 0, NULL, 0,
		CTL_NET, PF_INET, IPPROTO_IP, CTL_EOL);

	sysctl_createv(&ip_reass_sysctllog, 0, NULL, NULL,
		CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		CTLTYPE_INT, "maxfragpackets",
		SYSCTL_DESCR("Maximum number of fragments to retain for "
			     "possible reassembly"),
		NULL, 0, &ip_maxfragpackets, 0,
		CTL_NET, PF_INET, IPPROTO_IP, IPCTL_MAXFRAGPACKETS, CTL_EOL);
}

#define CHECK_NMBCLUSTER_PARAMS()				\
do {								\
	if (__predict_false(ip_nmbclusters != nmbclusters))	\
		ip_nmbclusters_changed();			\
} while (/*CONSTCOND*/0)

/*
 * Compute IP limits derived from the value of nmbclusters.
 */
static void
ip_nmbclusters_changed(void)
{
	ip_maxfrags = nmbclusters / 4;
	ip_nmbclusters = nmbclusters;
}

static inline int	ipq_lock_try(void);
static inline void	ipq_unlock(void);

static inline int
ipq_lock_try(void)
{
	int s;

	/*
	 * Use splvm() -- we're blocking things that would cause
	 * mbuf allocation.
	 */
	s = splvm();
	if (ipq_locked) {
		splx(s);
		return (0);
	}
	ipq_locked = 1;
	splx(s);
	return (1);
}

static inline void
ipq_unlock(void)
{
	int s;

	s = splvm();
	ipq_locked = 0;
	splx(s);
}

#ifdef DIAGNOSTIC
#define	IPQ_LOCK()							\
do {									\
	if (ipq_lock_try() == 0) {					\
		printf("%s:%d: ipq already locked\n", __FILE__, __LINE__); \
		panic("ipq_lock");					\
	}								\
} while (/*CONSTCOND*/ 0)
#define	IPQ_LOCK_CHECK()						\
do {									\
	if (ipq_locked == 0) {						\
		printf("%s:%d: ipq lock not held\n", __FILE__, __LINE__); \
		panic("ipq lock check");				\
	}								\
} while (/*CONSTCOND*/ 0)
#else
#define	IPQ_LOCK()		(void) ipq_lock_try()
#define	IPQ_LOCK_CHECK()	/* nothing */
#endif

#define	IPQ_UNLOCK()		ipq_unlock()

/*
 * ip_reass_lookup:
 *
 *	Look for queue of fragments of this datagram.
 */
static struct ipq *
ip_reass_lookup(struct ip *ip, u_int *hashp)
{
	struct ipq *fp;
	u_int hash;

	IPQ_LOCK();
	hash = IPREASS_HASH(ip->ip_src.s_addr, ip->ip_id);
	LIST_FOREACH(fp, &ipq[hash], ipq_q) {
		if (ip->ip_id != fp->ipq_id)
			continue;
		if (!in_hosteq(ip->ip_src, fp->ipq_src))
			continue;
		if (!in_hosteq(ip->ip_dst, fp->ipq_dst))
			continue;
		if (ip->ip_p != fp->ipq_p)
			continue;
		break;
	}
	*hashp = hash;
	return fp;
}

/*
 * ip_reass:
 *
 *	Take incoming datagram fragment and try to reassemble it into whole
 *	datagram.  If a chain for reassembly of this datagram already exists,
 *	then it is given as 'fp'; otherwise have to make a chain.
 */
struct mbuf *
ip_reass(struct ipqent *ipqe, struct ipq *fp, u_int hash)
{
	struct ipqhead *ipqhead = &ipq[hash];
	const int hlen = ipqe->ipqe_ip->ip_hl << 2;
	struct mbuf *m = ipqe->ipqe_m, *t;
	struct ipqent *nq, *p, *q;
	struct ip *ip;
	int i, next, s;

	IPQ_LOCK_CHECK();

	/*
	 * Presence of header sizes in mbufs would confuse code below.
	 */
	m->m_data += hlen;
	m->m_len -= hlen;

#ifdef	notyet
	/* Make sure fragment limit is up-to-date. */
	CHECK_NMBCLUSTER_PARAMS();

	/* If we have too many fragments, drop the older half. */
	if (ip_nfrags >= ip_maxfrags) {
		ip_reass_drophalf(void);
	}
#endif

	/*
	 * We are about to add a fragment; increment frag count.
	 */
	ip_nfrags++;

	/*
	 * If first fragment to arrive, create a reassembly queue.
	 */
	if (fp == NULL) {
		/*
		 * Enforce upper bound on number of fragmented packets
		 * for which we attempt reassembly:  a) if maxfrag is 0,
		 * never accept fragments  b) if maxfrag is -1, accept
		 * all fragments without limitation.
		 */
		if (ip_maxfragpackets < 0)
			;
		else if (ip_nfragpackets >= ip_maxfragpackets) {
			goto dropfrag;
		}
		ip_nfragpackets++;
		fp = malloc(sizeof(struct ipq), M_FTABLE, M_NOWAIT);
		if (fp == NULL) {
			goto dropfrag;
		}
		LIST_INSERT_HEAD(ipqhead, fp, ipq_q);
		fp->ipq_nfrags = 1;
		fp->ipq_ttl = IPFRAGTTL;
		fp->ipq_p = ipqe->ipqe_ip->ip_p;
		fp->ipq_id = ipqe->ipqe_ip->ip_id;
		fp->ipq_tos = ipqe->ipqe_ip->ip_tos;
		TAILQ_INIT(&fp->ipq_fragq);
		fp->ipq_src = ipqe->ipqe_ip->ip_src;
		fp->ipq_dst = ipqe->ipqe_ip->ip_dst;
		p = NULL;
		goto insert;
	} else {
		fp->ipq_nfrags++;
	}

	/*
	 * Find a segment which begins after this one does.
	 */
	for (p = NULL, q = TAILQ_FIRST(&fp->ipq_fragq); q != NULL;
	    p = q, q = TAILQ_NEXT(q, ipqe_q))
		if (ntohs(q->ipqe_ip->ip_off) > ntohs(ipqe->ipqe_ip->ip_off))
			break;

	/*
	 * If there is a preceding segment, it may provide some of our
	 * data already.  If so, drop the data from the incoming segment.
	 * If it provides all of our data, drop us.
	 */
	if (p != NULL) {
		i = ntohs(p->ipqe_ip->ip_off) + ntohs(p->ipqe_ip->ip_len) -
		    ntohs(ipqe->ipqe_ip->ip_off);
		if (i > 0) {
			if (i >= ntohs(ipqe->ipqe_ip->ip_len)) {
				goto dropfrag;
			}
			m_adj(ipqe->ipqe_m, i);
			ipqe->ipqe_ip->ip_off =
			    htons(ntohs(ipqe->ipqe_ip->ip_off) + i);
			ipqe->ipqe_ip->ip_len =
			    htons(ntohs(ipqe->ipqe_ip->ip_len) - i);
		}
	}

	/*
	 * While we overlap succeeding segments trim them or, if they are
	 * completely covered, dequeue them.
	 */
	for (; q != NULL &&
	    ntohs(ipqe->ipqe_ip->ip_off) + ntohs(ipqe->ipqe_ip->ip_len) >
	    ntohs(q->ipqe_ip->ip_off); q = nq) {
		i = (ntohs(ipqe->ipqe_ip->ip_off) +
		    ntohs(ipqe->ipqe_ip->ip_len)) - ntohs(q->ipqe_ip->ip_off);
		if (i < ntohs(q->ipqe_ip->ip_len)) {
			q->ipqe_ip->ip_len =
			    htons(ntohs(q->ipqe_ip->ip_len) - i);
			q->ipqe_ip->ip_off =
			    htons(ntohs(q->ipqe_ip->ip_off) + i);
			m_adj(q->ipqe_m, i);
			break;
		}
		nq = TAILQ_NEXT(q, ipqe_q);
		m_freem(q->ipqe_m);
		TAILQ_REMOVE(&fp->ipq_fragq, q, ipqe_q);
		s = splvm();
		pool_put(&ipqent_pool, q);
		splx(s);
		fp->ipq_nfrags--;
		ip_nfrags--;
	}

insert:
	/*
	 * Stick new segment in its place; check for complete reassembly.
	 */
	if (p == NULL) {
		TAILQ_INSERT_HEAD(&fp->ipq_fragq, ipqe, ipqe_q);
	} else {
		TAILQ_INSERT_AFTER(&fp->ipq_fragq, p, ipqe, ipqe_q);
	}
	next = 0;
	for (p = NULL, q = TAILQ_FIRST(&fp->ipq_fragq); q != NULL;
	    p = q, q = TAILQ_NEXT(q, ipqe_q)) {
		if (ntohs(q->ipqe_ip->ip_off) != next) {
			IPQ_UNLOCK();
			return NULL;
		}
		next += ntohs(q->ipqe_ip->ip_len);
	}
	if (p->ipqe_mff) {
		IPQ_UNLOCK();
		return NULL;
	}
	/*
	 * Reassembly is complete.  Check for a bogus message size and
	 * concatenate fragments.
	 */
	q = TAILQ_FIRST(&fp->ipq_fragq);
	ip = q->ipqe_ip;
	if ((next + (ip->ip_hl << 2)) > IP_MAXPACKET) {
		IP_STATINC(IP_STAT_TOOLONG);
		ip_freef(fp);
		IPQ_UNLOCK();
		return NULL;
	}
	m = q->ipqe_m;
	t = m->m_next;
	m->m_next = NULL;
	m_cat(m, t);
	nq = TAILQ_NEXT(q, ipqe_q);
	s = splvm();
	pool_put(&ipqent_pool, q);
	splx(s);
	for (q = nq; q != NULL; q = nq) {
		t = q->ipqe_m;
		nq = TAILQ_NEXT(q, ipqe_q);
		s = splvm();
		pool_put(&ipqent_pool, q);
		splx(s);
		m_cat(m, t);
	}
	ip_nfrags -= fp->ipq_nfrags;

	/*
	 * Create header for new packet by modifying header of first
	 * packet.  Dequeue and discard fragment reassembly header.  Make
	 * header visible.
	 */
	ip->ip_len = htons((ip->ip_hl << 2) + next);
	ip->ip_src = fp->ipq_src;
	ip->ip_dst = fp->ipq_dst;

	LIST_REMOVE(fp, ipq_q);
	free(fp, M_FTABLE);
	ip_nfragpackets--;
	m->m_len += (ip->ip_hl << 2);
	m->m_data -= (ip->ip_hl << 2);
	/* some debugging cruft by sklower, below, will go away soon */
	if (m->m_flags & M_PKTHDR) { /* XXX this should be done elsewhere */
		int plen = 0;
		for (t = m; t; t = t->m_next) {
			plen += t->m_len;
		}
		m->m_pkthdr.len = plen;
		m->m_pkthdr.csum_flags = 0;
	}
	IPQ_UNLOCK();
	return m;

dropfrag:
	if (fp != NULL) {
		fp->ipq_nfrags--;
	}
	ip_nfrags--;
	IP_STATINC(IP_STAT_FRAGDROPPED);
	m_freem(m);
	s = splvm();
	pool_put(&ipqent_pool, ipqe);
	splx(s);
	IPQ_UNLOCK();
	return NULL;
}

/*
 * ip_freef:
 *
 *	Free a fragment reassembly header and all associated datagrams.
 */
static void
ip_freef(struct ipq *fp)
{
	struct ipqent *q, *p;
	u_int nfrags = 0;
	int s;

	IPQ_LOCK_CHECK();

	for (q = TAILQ_FIRST(&fp->ipq_fragq); q != NULL; q = p) {
		p = TAILQ_NEXT(q, ipqe_q);
		m_freem(q->ipqe_m);
		nfrags++;
		TAILQ_REMOVE(&fp->ipq_fragq, q, ipqe_q);
		s = splvm();
		pool_put(&ipqent_pool, q);
		splx(s);
	}

	if (nfrags != fp->ipq_nfrags) {
		printf("ip_freef: nfrags %d != %d\n", fp->ipq_nfrags, nfrags);
	}
	ip_nfrags -= nfrags;
	LIST_REMOVE(fp, ipq_q);
	free(fp, M_FTABLE);
	ip_nfragpackets--;
}

/*
 * ip_reass_ttl_decr:
 *
 *	Decrement TTL of all reasembly queue entries by `ticks'.  Count
 *	number of distinct fragments (as opposed to partial, fragmented
 *	datagrams) inthe reassembly queue.  While we  traverse the entire
 *	reassembly queue, compute and return the median TTL over all
 *	fragments.
 */
static u_int
ip_reass_ttl_decr(u_int ticks)
{
	u_int nfrags, median, dropfraction, keepfraction;
	struct ipq *fp, *nfp;
	int i;

	nfrags = 0;
	memset(fragttl_histo, 0, sizeof(fragttl_histo));

	for (i = 0; i < IPREASS_HASH_SIZE; i++) {
		for (fp = LIST_FIRST(&ipq[i]); fp != NULL; fp = nfp) {
			fp->ipq_ttl = ((fp->ipq_ttl <= ticks) ?
			    0 : fp->ipq_ttl - ticks);
			nfp = LIST_NEXT(fp, ipq_q);
			if (fp->ipq_ttl == 0) {
				IP_STATINC(IP_STAT_FRAGTIMEOUT);
				ip_freef(fp);
			} else {
				nfrags += fp->ipq_nfrags;
				fragttl_histo[fp->ipq_ttl] += fp->ipq_nfrags;
			}
		}
	}

	KASSERT(ip_nfrags == nfrags);

	/* Find median (or other drop fraction) in histogram. */
	dropfraction = (ip_nfrags / 2);
	keepfraction = ip_nfrags - dropfraction;
	for (i = IPFRAGTTL, median = 0; i >= 0; i--) {
		median += fragttl_histo[i];
		if (median >= keepfraction)
			break;
	}

	/* Return TTL of median (or other fraction). */
	return (u_int)i;
}

static void
ip_reass_drophalf(void)
{
	u_int median_ticks;

	/*
	 * Compute median TTL of all fragments, and count frags
	 * with that TTL or lower (roughly half of all fragments).
	 */
	median_ticks = ip_reass_ttl_decr(0);

	/* Drop half. */
	median_ticks = ip_reass_ttl_decr(median_ticks);
}

/*
 * ip_reass_drain: drain off all datagram fragments.  Do not acquire
 * softnet_lock as can be called from hardware interrupt context.
 */
void
ip_reass_drain(void)
{

	/*
	 * We may be called from a device's interrupt context.  If
	 * the ipq is already busy, just bail out now.
	 */
	if (ipq_lock_try() != 0) {
		/*
		 * Drop half the total fragments now. If more mbufs are
		 * needed, we will be called again soon.
		 */
		ip_reass_drophalf();
		IPQ_UNLOCK();
	}
}

/*
 * ip_reass_slowtimo:
 *
 *	If a timer expires on a reassembly queue, discard it.
 */
void
ip_reass_slowtimo(void)
{
	static u_int dropscanidx = 0;
	u_int i, median_ttl;

	IPQ_LOCK();

	/* Age TTL of all fragments by 1 tick .*/
	median_ttl = ip_reass_ttl_decr(1);

	/* Make sure fragment limit is up-to-date. */
	CHECK_NMBCLUSTER_PARAMS();

	/* If we have too many fragments, drop the older half. */
	if (ip_nfrags > ip_maxfrags) {
		ip_reass_ttl_decr(median_ttl);
	}

	/*
	 * If we are over the maximum number of fragmented packets (due to
	 * the limit being lowered), drain off enough to get down to the
	 * new limit.  Start draining from the reassembly hashqueue most
	 * recently drained.
	 */
	if (ip_maxfragpackets < 0)
		;
	else {
		int wrapped = 0;

		i = dropscanidx;
		while (ip_nfragpackets > ip_maxfragpackets && wrapped == 0) {
			while (LIST_FIRST(&ipq[i]) != NULL) {
				ip_freef(LIST_FIRST(&ipq[i]));
			}
			if (++i >= IPREASS_HASH_SIZE) {
				i = 0;
			}
			/*
			 * Do not scan forever even if fragment counters are
			 * wrong: stop after scanning entire reassembly queue.
			 */
			if (i == dropscanidx) {
				wrapped = 1;
			}
		}
		dropscanidx = i;
	}
	IPQ_UNLOCK();
}

/*
 * ip_reass_packet: generic routine to perform IP reassembly.
 *
 * => Passed fragment should have IP_MF flag and/or offset set.
 * => Fragment should not have other than IP_MF flags set.
 *
 * => Returns 0 on success or error otherwise.  When reassembly is complete,
 *    m_final representing a constructed final packet is set.
 */
int
ip_reass_packet(struct mbuf *m, struct ip *ip, bool mff, struct mbuf **m_final)
{
	struct ipq *fp;
	struct ipqent *ipqe;
	u_int hash;

	/* Look for queue of fragments of this datagram. */
	fp = ip_reass_lookup(ip, &hash);

	/* Make sure that TOS matches previous fragments. */
	if (fp && fp->ipq_tos != ip->ip_tos) {
		IP_STATINC(IP_STAT_BADFRAGS);
		IPQ_UNLOCK();
		return EINVAL;
	}

	/*
	 * Create new entry and attempt to reassembly.
	 */
	IP_STATINC(IP_STAT_FRAGMENTS);
	int s = splvm();
	ipqe = pool_get(&ipqent_pool, PR_NOWAIT);
	splx(s);
	if (ipqe == NULL) {
		IP_STATINC(IP_STAT_RCVMEMDROP);
		IPQ_UNLOCK();
		return ENOMEM;
	}
	ipqe->ipqe_mff = mff;
	ipqe->ipqe_m = m;
	ipqe->ipqe_ip = ip;

	*m_final = ip_reass(ipqe, fp, hash);
	if (*m_final) {
		/* Note if finally reassembled. */
		IP_STATINC(IP_STAT_REASSEMBLED);
	}
	return 0;
}
