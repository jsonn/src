/*	$NetBSD: aarp.c,v 1.3.4.1 1998/12/11 04:53:06 kenh Exp $	*/

/*
 * Copyright (c) 1990,1991 Regents of The University of Michigan.
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation, and that the name of The University
 * of Michigan not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. This software is supplied as is without expressed or
 * implied warranties of any kind.
 *
 * This product includes software developed by the University of
 * California, Berkeley and its contributors.
 *
 *	Research Systems Unix Group
 *	The University of Michigan
 *	c/o Wesley Craig
 *	535 W. William Street
 *	Ann Arbor, Michigan
 *	+1-313-764-2278
 *	netatalk@umich.edu
 */

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <net/if.h>
#include <net/route.h>
#include <net/if_ether.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#undef s_net

#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/aarp.h>
#include <netatalk/ddp_var.h>
#include <netatalk/phase2.h>
#include <netatalk/at_extern.h>

static struct aarptab *aarptnew __P((struct at_addr *));
static void aarptfree __P((struct aarptab *));
static void at_aarpinput __P((struct ifnet *, struct mbuf *));
static void aarptimer __P((void *));
static void aarpwhohas __P((struct ifnet *, struct sockaddr_at *));

#define AARPTAB_BSIZ	9
#define AARPTAB_NB	19
#define AARPTAB_SIZE	(AARPTAB_BSIZ * AARPTAB_NB)
struct aarptab  aarptab[AARPTAB_SIZE];
int             aarptab_size = AARPTAB_SIZE;

#define AARPTAB_HASH(a) \
    ((((a).s_net << 8 ) + (a).s_node ) % AARPTAB_NB )

#define AARPTAB_LOOK(aat,addr) { \
    int		n; \
    aat = &aarptab[ AARPTAB_HASH(addr) * AARPTAB_BSIZ ]; \
    for ( n = 0; n < AARPTAB_BSIZ; n++, aat++ ) \
	if ( aat->aat_ataddr.s_net == (addr).s_net && \
	     aat->aat_ataddr.s_node == (addr).s_node ) \
	    break; \
	if ( n >= AARPTAB_BSIZ ) \
	    aat = 0; \
}

#define AARPT_AGE	(60 * 1)
#define AARPT_KILLC	20
#define AARPT_KILLI	3

#if !defined( __FreeBSD__ )
extern u_char   etherbroadcastaddr[6];
#endif	/* __FreeBSD__ */

u_char atmulticastaddr[6] = {
	0x09, 0x00, 0x07, 0xff, 0xff, 0xff
};

u_char at_org_code[3] = {
	0x08, 0x00, 0x07
};
u_char aarp_org_code[3] = {
	0x00, 0x00, 0x00
};


static void
aarptimer(ignored)
	void *ignored;
{
	struct aarptab *aat;
	int             i, s;

	timeout(aarptimer, NULL, AARPT_AGE * hz);
	aat = aarptab;
	for (i = 0; i < AARPTAB_SIZE; i++, aat++) {
		int killtime = (aat->aat_flags & ATF_COM) ? AARPT_KILLC :
		    AARPT_KILLI;
		if (aat->aat_flags == 0 || (aat->aat_flags & ATF_PERM))
			continue;
		if (++aat->aat_timer < killtime)
			continue;
		s = splimp();
		aarptfree(aat);
		splx(s);
	}
}

/*
 * search through the network addresses to find one that includes the given
 * network.. remember to take netranges into consideration.
 */
struct ifaddr *
#ifdef _DEBUG_IFA_REF
at_ifawithnet1(sat, ifp, f)
	char *f;
#else
at_ifawithnet(sat, ifp)
#endif
	struct sockaddr_at *sat;
	struct ifnet *ifp;
{
	struct ifaddr  *ifa;
	struct sockaddr_at *sat2;
	struct netrange *nr;
	int s;

	s = splimp();
	for (ifa = ifp->if_addrlist.tqh_first; ifa;
	    ifa = ifa->ifa_list.tqe_next) {
		if (ifa->ifa_addr->sa_family != AF_APPLETALK)
			continue;

		sat2 = satosat(ifa->ifa_addr);
		if (sat2->sat_addr.s_net == sat->sat_addr.s_net)
			break;

		nr = (struct netrange *) (sat2->sat_zero);
		if ((nr->nr_phase == 2)
		    && (nr->nr_firstnet <= sat->sat_addr.s_net)
		    && (nr->nr_lastnet >= sat->sat_addr.s_net))
			break;
	}
	if (ifa)
#ifdef _DEBUG_IFA_REF
		ifa_addref1(ifa, f);
#else
		ifa_addref(ifa);
#endif
	splx(s);
	return ifa;
}

static void
aarpwhohas(ifp, sat)
	struct ifnet *ifp;
	struct sockaddr_at *sat;
{
	struct mbuf    *m;
	struct ether_header *eh;
	struct ether_aarp *ea;
	struct at_ifaddr *aa;
	struct llc     *llc;
	struct sockaddr sa;

	if ((m = m_gethdr(M_DONTWAIT, MT_DATA)) == NULL)
		return;

	m->m_len = sizeof(*ea);
	m->m_pkthdr.len = sizeof(*ea);
	MH_ALIGN(m, sizeof(*ea));

	ea = mtod(m, struct ether_aarp *);
	bzero(ea, sizeof(*ea));

	ea->aarp_hrd = htons(AARPHRD_ETHER);
	ea->aarp_pro = htons(ETHERTYPE_ATALK);
	ea->aarp_hln = sizeof(ea->aarp_sha);
	ea->aarp_pln = sizeof(ea->aarp_spu);
	ea->aarp_op = htons(AARPOP_REQUEST);
	bcopy(LLADDR(ifp->if_sadl), ea->aarp_sha, sizeof(ea->aarp_sha));

	/*
         * We need to check whether the output ethernet type should
         * be phase 1 or 2. We have the interface that we'll be sending
         * the aarp out. We need to find an AppleTalk network on that
         * interface with the same address as we're looking for. If the
         * net is phase 2, generate an 802.2 and SNAP header.
         */
	if ((aa = (struct at_ifaddr *) at_ifawithnet(sat, ifp)) == NULL) {
		m_freem(m);
		return;
	}
	eh = (struct ether_header *) sa.sa_data;

	if (aa->aa_flags & AFA_PHASE2) {
		bcopy(atmulticastaddr, eh->ether_dhost,
		    sizeof(eh->ether_dhost));
		eh->ether_type = htons(sizeof(struct llc) +
		    sizeof(struct ether_aarp));
		M_PREPEND(m, sizeof(struct llc), M_WAIT);
		llc = mtod(m, struct llc *);
		llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
		llc->llc_control = LLC_UI;
		bcopy(aarp_org_code, llc->llc_org_code, sizeof(aarp_org_code));
		llc->llc_ether_type = htons(ETHERTYPE_AARP);

		bcopy(&AA_SAT(aa)->sat_addr.s_net, ea->aarp_spnet,
		      sizeof(ea->aarp_spnet));
		bcopy(&sat->sat_addr.s_net, ea->aarp_tpnet,
		      sizeof(ea->aarp_tpnet));
		ea->aarp_spnode = AA_SAT(aa)->sat_addr.s_node;
		ea->aarp_tpnode = sat->sat_addr.s_node;
	} else {
		bcopy(etherbroadcastaddr, eh->ether_dhost,
		    sizeof(eh->ether_dhost));
		eh->ether_type = htons(ETHERTYPE_AARP);

		ea->aarp_spa = AA_SAT(aa)->sat_addr.s_node;
		ea->aarp_tpa = sat->sat_addr.s_node;
	}

#ifdef NETATALKDEBUG
	printf("aarp: sending request via %u.%u seaking %u.%u\n",
	    ntohs(AA_SAT(aa)->sat_addr.s_net), AA_SAT(aa)->sat_addr.s_node,
	    ntohs(sat->sat_addr.s_net), sat->sat_addr.s_node);
#endif	/* NETATALKDEBUG */

	sa.sa_len = sizeof(struct sockaddr);
	sa.sa_family = AF_UNSPEC;
	(*ifp->if_output) (ifp, m, &sa, NULL);	/* XXX NULL should be routing */
						/* information */
	ifa_delref(&aa->aa_ifa);
}

int
aarpresolve(ifp, m, destsat, desten)
	struct ifnet   *ifp;
	struct mbuf    *m;
	struct sockaddr_at *destsat;
	u_char         *desten;
{
	struct at_ifaddr *aa;
	struct aarptab *aat;
	int             s;

	if (at_broadcast(destsat)) {
		aa = (struct at_ifaddr *) at_ifawithnet(destsat, ifp);
		if (aa == NULL) {
			m_freem(m);
			return (0);
		}
		if (aa->aa_flags & AFA_PHASE2)
			bcopy(atmulticastaddr, desten,
			    sizeof(atmulticastaddr));
		else
			bcopy(etherbroadcastaddr, desten,
			    sizeof(etherbroadcastaddr));
		ifa_delref(&aa->aa_ifa);
		return 1;
	}
	s = splimp();
	AARPTAB_LOOK(aat, destsat->sat_addr);
	if (aat == 0) {		/* No entry */
		aat = aarptnew(&destsat->sat_addr);
		if (aat == 0)
			panic("aarpresolve: no free entry");

		aat->aat_hold = m;
		aarpwhohas(ifp, destsat);
		splx(s);
		return 0;
	}

	/* found an entry */
	aat->aat_timer = 0;
	if (aat->aat_flags & ATF_COM) {	/* entry is COMplete */
		bcopy(aat->aat_enaddr, desten, sizeof(aat->aat_enaddr));
		splx(s);
		return 1;
	}

	/* entry has not completed */
	if (aat->aat_hold)
		m_freem(aat->aat_hold);
	aat->aat_hold = m;
	aarpwhohas(ifp, destsat);
	splx(s);

	return 0;
}

void
aarpinput(ifp, m)
	struct ifnet   *ifp;
	struct mbuf    *m;
{
	struct arphdr  *ar;

	if (ifp->if_flags & IFF_NOARP)
		goto out;

	if (m->m_len < sizeof(struct arphdr))
		goto out;

	ar = mtod(m, struct arphdr *);
	if (ntohs(ar->ar_hrd) != AARPHRD_ETHER)
		goto out;

	if (m->m_len < sizeof(struct arphdr) + 2 * ar->ar_hln + 2 * ar->ar_pln)
		goto out;

	switch (ntohs(ar->ar_pro)) {
	case ETHERTYPE_ATALK:
		at_aarpinput(ifp, m);
		return;

	default:
		break;
	}

out:
	m_freem(m);
}

static void
at_aarpinput(ifp, m)
	struct ifnet *ifp;
	struct mbuf *m;
{
	struct ether_aarp *ea;
	struct at_ifaddr *aa;
	struct aarptab *aat;
	struct ether_header *eh;
	struct llc     *llc;
	struct sockaddr_at sat;
	struct sockaddr sa;
	struct at_addr  spa, tpa, ma;
	int             op, s;
	u_int16_t       net;

	ea = mtod(m, struct ether_aarp *);

	/* Check to see if from my hardware address */
	if (!bcmp(ea->aarp_sha, LLADDR(ifp->if_sadl), sizeof(ea->aarp_sha))) {
		m_freem(m);
		return;
	}
	/* Check to see if from a broadcast address. */
	if (!bcmp(ea->aarp_sha, (caddr_t)etherbroadcastaddr,
		    sizeof( etherbroadcastaddr ))) {
		log( LOG_ERR, "aarp: source is broadcast!\n");
		m_free(m);
		return;
	}
	op = ntohs(ea->aarp_op);
	bcopy(ea->aarp_tpnet, &net, sizeof(net));

	if (net != 0) {		/* should be ATADDR_ANYNET? */
		sat.sat_len = sizeof(struct sockaddr_at);
		sat.sat_family = AF_APPLETALK;
		sat.sat_addr.s_net = net;
		aa = (struct at_ifaddr *) at_ifawithnet(&sat, ifp);
		if (aa == NULL) {
			m_freem(m);
			return;
		}
		bcopy(ea->aarp_spnet, &spa.s_net, sizeof(spa.s_net));
		bcopy(ea->aarp_tpnet, &tpa.s_net, sizeof(tpa.s_net));
	} else {
		/*
		 * Since we don't know the net, we just look for the first
		 * phase 1 address on the interface.
		 */
		s = splimp();
		for (aa = (struct at_ifaddr *) ifp->if_addrlist.tqh_first; aa;
		    aa = (struct at_ifaddr *) aa->aa_ifa.ifa_list.tqe_next) {
			if (AA_SAT(aa)->sat_family == AF_APPLETALK &&
			    (aa->aa_flags & AFA_PHASE2) == 0)
				break;
		}
		if (aa == NULL) {
			splx(s);
			m_freem(m);
			return;
		}
		ifa_addref(&aa->aa_ifa);
		tpa.s_net = spa.s_net = AA_SAT(aa)->sat_addr.s_net;
		splx(s);
	}

	spa.s_node = ea->aarp_spnode;
	tpa.s_node = ea->aarp_tpnode;
	ma.s_net = AA_SAT(aa)->sat_addr.s_net;
	ma.s_node = AA_SAT(aa)->sat_addr.s_node;

	/*
         * This looks like it's from us.
         */
	if (spa.s_net == ma.s_net && spa.s_node == ma.s_node) {
		if (aa->aa_flags & AFA_PROBING) {
			/*
		         * We're probing, someone either responded to our
			 * probe, or probed for the same address we'd like
			 * to use. Change the address we're probing for.
		         */
			untimeout(aarpprobe, ifp);
			wakeup(aa);
			m_freem(m);
			ifa_delref(&aa->aa_ifa);
			return;
		} else if (op != AARPOP_PROBE) {
			/*
		         * This is not a probe, and we're not probing.
			 * This means that someone's saying they have the same
			 * source address as the one we're using. Get upset...
		         */
			log(LOG_ERR, "aarp: duplicate AT address!! %s\n",
			    ether_sprintf(ea->aarp_sha));
			m_freem(m);
			ifa_delref(&aa->aa_ifa);
			return;
		}
	}
	AARPTAB_LOOK(aat, spa);
	if (aat) {
		if (op == AARPOP_PROBE) {
			/*
		         * Someone's probing for spa, dealocate the one we've
			 * got, so that if the prober keeps the address, we'll
			 * be able to arp for him.
		         */
			aarptfree(aat);
			m_freem(m);
			ifa_delref(&aa->aa_ifa);
			return;
		}
		bcopy(ea->aarp_sha, aat->aat_enaddr, sizeof(ea->aarp_sha));
		aat->aat_flags |= ATF_COM;
		if (aat->aat_hold) {
			sat.sat_len = sizeof(struct sockaddr_at);
			sat.sat_family = AF_APPLETALK;
			sat.sat_addr = spa;
			(*ifp->if_output)(ifp, aat->aat_hold,
			    (struct sockaddr *) & sat, NULL);	/* XXX */
			aat->aat_hold = 0;
		}
	}
	if (aat == 0 && tpa.s_net == ma.s_net && tpa.s_node == ma.s_node
	    && op != AARPOP_PROBE) {
		if ((aat = aarptnew(&spa)) != NULL) {
			bcopy(ea->aarp_sha, aat->aat_enaddr,
			    sizeof(ea->aarp_sha));
			aat->aat_flags |= ATF_COM;
		}
	}
	/*
         * Don't respond to responses, and never respond if we're
         * still probing.
         */
	if (tpa.s_net != ma.s_net || tpa.s_node != ma.s_node ||
	    op == AARPOP_RESPONSE || (aa->aa_flags & AFA_PROBING)) {
		m_freem(m);
		ifa_delref(&aa->aa_ifa);
		return;
	}
	bcopy(ea->aarp_sha, ea->aarp_tha, sizeof(ea->aarp_sha));
	bcopy(LLADDR(ifp->if_sadl), ea->aarp_sha, sizeof(ea->aarp_sha));

	/* XXX */
	eh = (struct ether_header *) sa.sa_data;
	bcopy(ea->aarp_tha, eh->ether_dhost, sizeof(eh->ether_dhost));

	if (aa->aa_flags & AFA_PHASE2) {
		eh->ether_type = htons(sizeof(struct llc) +
		    sizeof(struct ether_aarp));
		M_PREPEND(m, sizeof(struct llc), M_DONTWAIT);
		if (m == NULL) {
			ifa_delref(&aa->aa_ifa);
			return;
		}

		llc = mtod(m, struct llc *);
		llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
		llc->llc_control = LLC_UI;
		bcopy(aarp_org_code, llc->llc_org_code, sizeof(aarp_org_code));
		llc->llc_ether_type = htons(ETHERTYPE_AARP);

		bcopy(ea->aarp_spnet, ea->aarp_tpnet, sizeof(ea->aarp_tpnet));
		bcopy(&ma.s_net, ea->aarp_spnet, sizeof(ea->aarp_spnet));
	} else {
		eh->ether_type = htons(ETHERTYPE_AARP);
	}

	ea->aarp_tpnode = ea->aarp_spnode;
	ea->aarp_spnode = ma.s_node;
	ea->aarp_op = htons(AARPOP_RESPONSE);

	sa.sa_len = sizeof(struct sockaddr);
	sa.sa_family = AF_UNSPEC;
	(*ifp->if_output) (ifp, m, &sa, NULL);	/* XXX */
	ifa_delref(&aa->aa_ifa);
	return;
}

static void
aarptfree(aat)
	struct aarptab *aat;
{

	if (aat->aat_hold)
		m_freem(aat->aat_hold);
	aat->aat_hold = 0;
	aat->aat_timer = aat->aat_flags = 0;
	aat->aat_ataddr.s_net = 0;
	aat->aat_ataddr.s_node = 0;
}

static struct aarptab *
aarptnew(addr)
	struct at_addr *addr;
{
	int             n;
	int             oldest = -1;
	struct aarptab *aat, *aato = NULL;
	static int      first = 1;

	if (first) {
		first = 0;
		timeout(aarptimer, NULL, hz);
	}
	aat = &aarptab[AARPTAB_HASH(*addr) * AARPTAB_BSIZ];
	for (n = 0; n < AARPTAB_BSIZ; n++, aat++) {
		if (aat->aat_flags == 0)
			goto out;
		if (aat->aat_flags & ATF_PERM)
			continue;
		if ((int) aat->aat_timer > oldest) {
			oldest = aat->aat_timer;
			aato = aat;
		}
	}
	if (aato == NULL)
		return (NULL);
	aat = aato;
	aarptfree(aat);
out:
	aat->aat_ataddr = *addr;
	aat->aat_flags = ATF_INUSE;
	return (aat);
}


void
aarpprobe(arp)
	void *arp;
{
	struct mbuf    *m;
	struct ether_header *eh;
	struct ether_aarp *ea;
	struct at_ifaddr *aa;
	struct llc     *llc;
	struct sockaddr sa;
	struct ifnet   *ifp = arp;
	int s;

	/*
         * We need to check whether the output ethernet type should
         * be phase 1 or 2. We have the interface that we'll be sending
         * the aarp out. We need to find an AppleTalk network on that
         * interface with the same address as we're looking for. If the
         * net is phase 2, generate an 802.2 and SNAP header.
         */
	s = splimp();
	for (aa = (struct at_ifaddr *) ifp->if_addrlist.tqh_first; aa;
	     aa = (struct at_ifaddr *) aa->aa_ifa.ifa_list.tqe_next) {
		if (AA_SAT(aa)->sat_family == AF_APPLETALK &&
		    (aa->aa_flags & AFA_PROBING))
			break;
	}
	if (aa == NULL) {	/* serious error XXX */
		splx(s);
		printf("aarpprobe why did this happen?!\n");
		return;
	}
	ifa_addref(&aa->aa_ifa);
	splx(s);
	if (aa->aa_probcnt <= 0) {
		aa->aa_flags &= ~AFA_PROBING;
		wakeup(aa);
		ifa_delref(&aa->aa_ifa);
		return;
	} else {
		timeout(aarpprobe, arp, hz / 5);
	}

	if ((m = m_gethdr(M_DONTWAIT, MT_DATA)) == NULL) {
		ifa_delref(&aa->aa_ifa);
		return;
	}
	m->m_len = sizeof(*ea);
	m->m_pkthdr.len = sizeof(*ea);
	MH_ALIGN(m, sizeof(*ea));

	ea = mtod(m, struct ether_aarp *);
	bzero(ea, sizeof(*ea));

	ea->aarp_hrd = htons(AARPHRD_ETHER);
	ea->aarp_pro = htons(ETHERTYPE_ATALK);
	ea->aarp_hln = sizeof(ea->aarp_sha);
	ea->aarp_pln = sizeof(ea->aarp_spu);
	ea->aarp_op = htons(AARPOP_PROBE);
	bcopy(LLADDR(ifp->if_sadl), ea->aarp_sha, sizeof(ea->aarp_sha));

	eh = (struct ether_header *) sa.sa_data;

	if (aa->aa_flags & AFA_PHASE2) {
		bcopy(atmulticastaddr, eh->ether_dhost,
		    sizeof(eh->ether_dhost));
		eh->ether_type = htons(sizeof(struct llc) +
		    sizeof(struct ether_aarp));
		M_PREPEND(m, sizeof(struct llc), M_WAIT);
		llc = mtod(m, struct llc *);
		llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
		llc->llc_control = LLC_UI;
		bcopy(aarp_org_code, llc->llc_org_code, sizeof(aarp_org_code));
		llc->llc_ether_type = htons(ETHERTYPE_AARP);

		bcopy(&AA_SAT(aa)->sat_addr.s_net, ea->aarp_spnet,
		      sizeof(ea->aarp_spnet));
		bcopy(&AA_SAT(aa)->sat_addr.s_net, ea->aarp_tpnet,
		      sizeof(ea->aarp_tpnet));
		ea->aarp_spnode = ea->aarp_tpnode =
		    AA_SAT(aa)->sat_addr.s_node;
	} else {
		bcopy(etherbroadcastaddr, eh->ether_dhost,
		    sizeof(eh->ether_dhost));
		eh->ether_type = htons(ETHERTYPE_AARP);
		ea->aarp_spa = ea->aarp_tpa = AA_SAT(aa)->sat_addr.s_node;
	}

#ifdef NETATALKDEBUG
	printf("aarp: sending probe for %u.%u\n",
	       ntohs(AA_SAT(aa)->sat_addr.s_net),
	       AA_SAT(aa)->sat_addr.s_node);
#endif	/* NETATALKDEBUG */

	sa.sa_len = sizeof(struct sockaddr);
	sa.sa_family = AF_UNSPEC;
	(*ifp->if_output) (ifp, m, &sa, NULL);	/* XXX */
	aa->aa_probcnt--;
	ifa_delref(&aa->aa_ifa);
}

void
aarp_clean()
{
	struct aarptab *aat;
	int             i;

	untimeout(aarptimer, 0);
	for (i = 0, aat = aarptab; i < AARPTAB_SIZE; i++, aat++)
		if (aat->aat_hold)
			m_freem(aat->aat_hold);
}
