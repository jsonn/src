/*	$NetBSD: uipc_domain.c,v 1.62.4.2 2007/05/07 10:55:48 yamt Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1993
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
 *	@(#)uipc_domain.c	8.3 (Berkeley) 2/14/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uipc_domain.c,v 1.62.4.2 2007/05/07 10:55:48 yamt Exp $");

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/mbuf.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/queue.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/un.h>
#include <sys/unpcb.h>
#include <sys/file.h>
#include <sys/kauth.h>

void	pffasttimo(void *);
void	pfslowtimo(void *);

struct domainhead domains = STAILQ_HEAD_INITIALIZER(domains);
static struct domain *domain_array[AF_MAX];

struct callout pffasttimo_ch, pfslowtimo_ch;

/*
 * Current time values for fast and slow timeouts.  We can use u_int
 * relatively safely.  The fast timer will roll over in 27 years and
 * the slow timer in 68 years.
 */
u_int	pfslowtimo_now;
u_int	pffasttimo_now;

void
domaininit(void)
{
	__link_set_decl(domains, struct domain);
	struct domain * const * dpp;
	struct domain *rt_domain = NULL;

	/*
	 * Add all of the domains.  Make sure the PF_ROUTE
	 * domain is added last.
	 */
	__link_set_foreach(dpp, domains) {
		if ((*dpp)->dom_family == PF_ROUTE)
			rt_domain = *dpp;
		else
			domain_attach(*dpp);
	}
	if (rt_domain)
		domain_attach(rt_domain);

	callout_init(&pffasttimo_ch);
	callout_init(&pfslowtimo_ch);

	callout_reset(&pffasttimo_ch, 1, pffasttimo, NULL);
	callout_reset(&pfslowtimo_ch, 1, pfslowtimo, NULL);
}

void
domain_attach(struct domain *dp)
{
	const struct protosw *pr;

	STAILQ_INSERT_TAIL(&domains, dp, dom_link);
	if (dp->dom_family < __arraycount(domain_array))
		domain_array[dp->dom_family] = dp;

	if (dp->dom_init)
		(*dp->dom_init)();

#ifdef MBUFTRACE
	if (dp->dom_mowner.mo_name[0] == '\0') {
		strncpy(dp->dom_mowner.mo_name, dp->dom_name,
		    sizeof(dp->dom_mowner.mo_name));
		MOWNER_ATTACH(&dp->dom_mowner);
	}
#endif
	for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++) {
		if (pr->pr_init)
			(*pr->pr_init)();
	}

	if (max_linkhdr < 16)		/* XXX */
		max_linkhdr = 16;
	max_hdr = max_linkhdr + max_protohdr;
	max_datalen = MHLEN - max_hdr;
}

struct domain *
pffinddomain(int family)
{
	struct domain *dp;

	if (family < __arraycount(domain_array) && domain_array[family] != NULL)
		return domain_array[family];

	DOMAIN_FOREACH(dp)
		if (dp->dom_family == family)
			return (dp);
	return (NULL);
}

const struct protosw *
pffindtype(int family, int type)
{
	struct domain *dp;
	const struct protosw *pr;

	dp = pffinddomain(family);
	if (dp == NULL)
		return (NULL);

	for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
		if (pr->pr_type && pr->pr_type == type)
			return (pr);

	return (NULL);
}

const struct protosw *
pffindproto(int family, int protocol, int type)
{
	struct domain *dp;
	const struct protosw *pr;
	const struct protosw *maybe = NULL;

	if (family == 0)
		return (NULL);

	dp = pffinddomain(family);
	if (dp == NULL)
		return (NULL);

	for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++) {
		if ((pr->pr_protocol == protocol) && (pr->pr_type == type))
			return (pr);

		if (type == SOCK_RAW && pr->pr_type == SOCK_RAW &&
		    pr->pr_protocol == 0 && maybe == NULL)
			maybe = pr;
	}
	return (maybe);
}

struct sockaddr *
sockaddr_alloc(sa_family_t af, int flags)
{
	const struct domain *dom;
	struct sockaddr *sa;

	if ((dom = pffinddomain(af)) == NULL)
		return NULL;

	if ((sa = pool_get(dom->dom_sa_pool, flags)) == NULL)
		return NULL;

	sa->sa_family = af;
	sa->sa_len = dom->dom_sa_len;
	return sa;
}

struct sockaddr *
sockaddr_copy(struct sockaddr *dst, const struct sockaddr *src)
{
	KASSERT(dst->sa_family == src->sa_family);
	memcpy(dst, src, src->sa_len);
	return dst;
}

int
sockaddr_cmp(const struct sockaddr *sa1, const struct sockaddr *sa2)
{
	int len, rc;
	struct domain *dom;

	if (sa1->sa_family != sa2->sa_family)
		return sa1->sa_family - sa2->sa_family;

	dom = pffinddomain(sa1->sa_family);

	if (dom != NULL && dom->dom_sockaddr_cmp != NULL)
		return (*dom->dom_sockaddr_cmp)(sa1, sa2);

	len = MIN(sa1->sa_len, sa2->sa_len);

	if (dom == NULL || dom->dom_sa_cmplen == 0) {
		if ((rc = memcmp(sa1, sa2, len)) != 0)
			return rc;
		return sa1->sa_len - sa2->sa_len;
	}

	if ((rc = memcmp((const char *)sa1 + dom->dom_sa_cmpofs,
		         (const char *)sa2 + dom->dom_sa_cmpofs,
			 MIN(dom->dom_sa_cmplen,
			     len - MIN(len, dom->dom_sa_cmpofs)))) != 0)
		return rc;

	return MIN(dom->dom_sa_cmplen + dom->dom_sa_cmpofs, sa1->sa_len) -
	       MIN(dom->dom_sa_cmplen + dom->dom_sa_cmpofs, sa2->sa_len);
}

struct sockaddr *
sockaddr_dup(const struct sockaddr *src, int flags)
{
	struct sockaddr *dst;

	if ((dst = sockaddr_alloc(src->sa_family, flags)) == NULL)
		return NULL;

	KASSERT(dst->sa_len >= src->sa_len);

	return sockaddr_copy(dst, src);
}

void
sockaddr_free(struct sockaddr *sa)
{
	const struct domain *dom;

	if ((dom = pffinddomain(sa->sa_family)) == NULL)
		panic("%s: no such domain %d\n", __func__, sa->sa_family);

	pool_put(dom->dom_sa_pool, sa);
}

/*
 * sysctl helper to stuff PF_LOCAL pcbs into sysctl structures
 */
static void
sysctl_dounpcb(struct kinfo_pcb *pcb, const struct socket *so)
{
	struct unpcb *unp = sotounpcb(so);
	struct sockaddr_un *un = unp->unp_addr;

	memset(pcb, 0, sizeof(*pcb));

	pcb->ki_family = so->so_proto->pr_domain->dom_family;
	pcb->ki_type = so->so_proto->pr_type;
	pcb->ki_protocol = so->so_proto->pr_protocol;
	pcb->ki_pflags = unp->unp_flags;

	pcb->ki_pcbaddr = PTRTOUINT64(unp);
	/* pcb->ki_ppcbaddr = unp has no ppcb... */
	pcb->ki_sockaddr = PTRTOUINT64(so);

	pcb->ki_sostate = so->so_state;
	/* pcb->ki_prstate = unp has no state... */

	pcb->ki_rcvq = so->so_rcv.sb_cc;
	pcb->ki_sndq = so->so_snd.sb_cc;

	un = (struct sockaddr_un *)&pcb->ki_src;
	/*
	 * local domain sockets may bind without having a local
	 * endpoint.  bleah!
	 */
	if (unp->unp_addr != NULL) {
		un->sun_len = unp->unp_addr->sun_len;
		un->sun_family = unp->unp_addr->sun_family;
		strlcpy(un->sun_path, unp->unp_addr->sun_path,
		    sizeof(pcb->ki_s));
	}
	else {
		un->sun_len = offsetof(struct sockaddr_un, sun_path);
		un->sun_family = pcb->ki_family;
	}
	if (unp->unp_conn != NULL) {
		un = (struct sockaddr_un *)&pcb->ki_dst;
		if (unp->unp_conn->unp_addr != NULL) {
			un->sun_len = unp->unp_conn->unp_addr->sun_len;
			un->sun_family = unp->unp_conn->unp_addr->sun_family;
			un->sun_family = unp->unp_conn->unp_addr->sun_family;
			strlcpy(un->sun_path, unp->unp_conn->unp_addr->sun_path,
				sizeof(pcb->ki_d));
		}
		else {
			un->sun_len = offsetof(struct sockaddr_un, sun_path);
			un->sun_family = pcb->ki_family;
		}
	}

	pcb->ki_inode = unp->unp_ino;
	pcb->ki_vnode = PTRTOUINT64(unp->unp_vnode);
	pcb->ki_conn = PTRTOUINT64(unp->unp_conn);
	pcb->ki_refs = PTRTOUINT64(unp->unp_refs);
	pcb->ki_nextref = PTRTOUINT64(unp->unp_nextref);
}

static int
sysctl_unpcblist(SYSCTLFN_ARGS)
{
	struct file *fp;
	struct socket *so;
	struct kinfo_pcb pcb;
	char *dp;
	u_int op, arg;
	size_t len, needed, elem_size, out_size;
	int error, elem_count, pf, type, pf2;

	if (namelen == 1 && name[0] == CTL_QUERY)
		return (sysctl_query(SYSCTLFN_CALL(rnode)));

	if (namelen != 4)
		return (EINVAL);

	if (oldp != NULL) {
		len = *oldlenp;
		elem_size = name[2];
		elem_count = name[3];
		if (elem_size != sizeof(pcb))
			return EINVAL;
	} else {
		len = 0;
		elem_size = sizeof(pcb);
		elem_count = INT_MAX;
	}
	error = 0;
	dp = oldp;
	op = name[0];
	arg = name[1];
	out_size = elem_size;
	needed = 0;

	if (name - oname != 4)
		return (EINVAL);

	pf = oname[1];
	type = oname[2];
	pf2 = (oldp == NULL) ? 0 : pf;

	/*
	 * there's no "list" of local domain sockets, so we have
	 * to walk the file list looking for them.  :-/
	 */
	LIST_FOREACH(fp, &filehead, f_list) {
		if (kauth_authorize_generic(l->l_cred,
		    KAUTH_GENERIC_CANSEE, fp->f_cred) != 0)
			continue;
		if (fp->f_type != DTYPE_SOCKET)
			continue;
		so = (struct socket *)fp->f_data;
		if (so->so_type != type)
			continue;
		if (so->so_proto->pr_domain->dom_family != pf)
			continue;
		if (len >= elem_size && elem_count > 0) {
			sysctl_dounpcb(&pcb, so);
			error = copyout(&pcb, dp, out_size);
			if (error)
				break;
			dp += elem_size;
			len -= elem_size;
		}
		if (elem_count > 0) {
			needed += elem_size;
			if (elem_count != INT_MAX)
				elem_count--;
		}
	}

	*oldlenp = needed;
	if (oldp == NULL)
		*oldlenp += PCB_SLOP * sizeof(struct kinfo_pcb);

	return (error);
}

SYSCTL_SETUP(sysctl_net_setup, "sysctl net subtree setup")
{
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "net", NULL,
		       NULL, 0, NULL, 0,
		       CTL_NET, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "local",
		       SYSCTL_DESCR("PF_LOCAL related settings"),
		       NULL, 0, NULL, 0,
		       CTL_NET, PF_LOCAL, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "stream",
		       SYSCTL_DESCR("SOCK_STREAM settings"),
		       NULL, 0, NULL, 0,
		       CTL_NET, PF_LOCAL, SOCK_STREAM, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "dgram",
		       SYSCTL_DESCR("SOCK_DGRAM settings"),
		       NULL, 0, NULL, 0,
		       CTL_NET, PF_LOCAL, SOCK_DGRAM, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "pcblist",
		       SYSCTL_DESCR("SOCK_STREAM protocol control block list"),
		       sysctl_unpcblist, 0, NULL, 0,
		       CTL_NET, PF_LOCAL, SOCK_STREAM, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "pcblist",
		       SYSCTL_DESCR("SOCK_DGRAM protocol control block list"),
		       sysctl_unpcblist, 0, NULL, 0,
		       CTL_NET, PF_LOCAL, SOCK_DGRAM, CTL_CREATE, CTL_EOL);
}

void
pfctlinput(int cmd, const struct sockaddr *sa)
{
	struct domain *dp;
	const struct protosw *pr;

	DOMAIN_FOREACH(dp) {
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++) {
			if (pr->pr_ctlinput != NULL)
				(*pr->pr_ctlinput)(cmd, sa, NULL);
		}
	}
}

void
pfctlinput2(int cmd, const struct sockaddr *sa, void *ctlparam)
{
	struct domain *dp;
	const struct protosw *pr;

	if (sa == NULL)
		return;

	DOMAIN_FOREACH(dp) {
		/*
		 * the check must be made by xx_ctlinput() anyways, to
		 * make sure we use data item pointed to by ctlparam in
		 * correct way.  the following check is made just for safety.
		 */
		if (dp->dom_family != sa->sa_family)
			continue;

		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++) {
			if (pr->pr_ctlinput != NULL)
				(*pr->pr_ctlinput)(cmd, sa, ctlparam);
		}
	}
}

void
pfslowtimo(void *arg)
{
	struct domain *dp;
	const struct protosw *pr;

	pfslowtimo_now++;

	DOMAIN_FOREACH(dp) {
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_slowtimo)
				(*pr->pr_slowtimo)();
	}
	callout_reset(&pfslowtimo_ch, hz / 2, pfslowtimo, NULL);
}

void
pffasttimo(void *arg)
{
	struct domain *dp;
	const struct protosw *pr;

	pffasttimo_now++;

	DOMAIN_FOREACH(dp) {
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_fasttimo)
				(*pr->pr_fasttimo)();
	}
	callout_reset(&pffasttimo_ch, hz / 5, pffasttimo, NULL);
}
