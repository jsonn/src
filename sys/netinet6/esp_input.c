/*	$NetBSD: esp_input.c,v 1.1.1.1.4.2 2000/06/22 17:09:54 minoura Exp $	*/
/*	$KAME: esp_input.c,v 1.22 2000/03/21 05:14:49 itojun Exp $	*/

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

/*
 * RFC1827/2406 Encapsulated Security Payload.
 */

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/route.h>
#include <net/netisr.h>
#include <machine/cpu.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_var.h>
#include <netinet/ip_ecn.h>

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>
#endif

#include <netinet6/ipsec.h>
#include <netinet6/ah.h>
#include <netinet6/esp.h>
#include <netkey/key.h>
#include <netkey/keydb.h>
#include <netkey/key_debug.h>

#include <machine/stdarg.h>

#include <net/net_osdep.h>

#define IPLEN_FLIPPED

#ifdef INET
extern struct protosw inetsw[];
extern u_char ip_protox[];

#define ESPMAXLEN \
	(sizeof(struct esp) < sizeof(struct newesp) \
		? sizeof(struct newesp) : sizeof(struct esp))

void
#if __STDC__
esp4_input(struct mbuf *m, ...)
#else
esp4_input(m, va_alist)
	struct mbuf *m;
	va_dcl
#endif
{
	struct ip *ip;
	struct esp *esp;
	struct esptail esptail;
	u_int32_t spi;
	struct secasvar *sav = NULL;
	size_t taillen;
	u_int16_t nxt;
	struct esp_algorithm *algo;
	int ivlen;
	size_t hlen;
	size_t esplen;
	int s;
	va_list ap;
	int off, proto;

	va_start(ap, m);
	off = va_arg(ap, int);
	proto = va_arg(ap, int);
	va_end(ap);

	/* sanity check for alignment. */
	if (off % 4 != 0 || m->m_pkthdr.len % 4 != 0) {
		ipseclog((LOG_ERR, "IPv4 ESP input: packet alignment problem "
			"(off=%d, pktlen=%d)\n", off, m->m_pkthdr.len));
		ipsecstat.in_inval++;
		goto bad;
	}

	if (m->m_len < off + ESPMAXLEN) {
		m = m_pullup(m, off + ESPMAXLEN);
		if (!m) {
			ipseclog((LOG_DEBUG,
			    "IPv4 ESP input: can't pullup in esp4_input\n"));
			ipsecstat.in_inval++;
			goto bad;
		}
	}

	ip = mtod(m, struct ip *);
	esp = (struct esp *)(((u_int8_t *)ip) + off);
#ifdef _IP_VHL
	hlen = IP_VHL_HL(ip->ip_vhl) << 2;
#else
	hlen = ip->ip_hl << 2;
#endif

	/* find the sassoc. */
	spi = esp->esp_spi;

	if ((sav = key_allocsa(AF_INET,
	                      (caddr_t)&ip->ip_src, (caddr_t)&ip->ip_dst,
	                      IPPROTO_ESP, spi)) == 0) {
		ipseclog((LOG_WARNING,
		    "IPv4 ESP input: no key association found for spi %u\n",
		    (u_int32_t)ntohl(spi)));
		ipsecstat.in_nosa++;
		goto bad;
	}
	KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
		printf("DP esp4_input called to allocate SA:%p\n", sav));
	if (sav->state != SADB_SASTATE_MATURE
	 && sav->state != SADB_SASTATE_DYING) {
		ipseclog((LOG_DEBUG,
		    "IPv4 ESP input: non-mature/dying SA found for spi %u\n",
		    (u_int32_t)ntohl(spi)));
		ipsecstat.in_badspi++;
		goto bad;
	}
	if (sav->alg_enc == SADB_EALG_NONE) {
		ipseclog((LOG_DEBUG, "IPv4 ESP input: "
		    "unspecified encryption algorithm for spi %u\n",
		    (u_int32_t)ntohl(spi)));
		ipsecstat.in_badspi++;
		goto bad;
	}

	algo = &esp_algorithms[sav->alg_enc];	/*XXX*/

	/* check if we have proper ivlen information */
	ivlen = sav->ivlen;
	if (ivlen < 0) {
		ipseclog((LOG_ERR, "inproper ivlen in IPv4 ESP input: %s %s\n",
		    ipsec4_logpacketstr(ip, spi), ipsec_logsastr(sav)));
		ipsecstat.in_inval++;
		goto bad;
	}

	if (!((sav->flags & SADB_X_EXT_OLD) == 0 && sav->replay
	 && (sav->alg_auth && sav->key_auth)))
		goto noreplaycheck;

	if (sav->alg_auth == SADB_AALG_NULL)
		goto noreplaycheck;

	/*
	 * check for sequence number.
	 */
	if (ipsec_chkreplay(ntohl(((struct newesp *)esp)->esp_seq), sav))
		; /*okey*/
	else {
		ipsecstat.in_espreplay++;
		ipseclog((LOG_WARNING,
		    "replay packet in IPv4 ESP input: %s %s\n",
		    ipsec4_logpacketstr(ip, spi), ipsec_logsastr(sav)));
		goto bad;
	}

	/* check ICV */
    {
	u_char sum0[AH_MAXSUMSIZE];
	u_char sum[AH_MAXSUMSIZE];
	struct ah_algorithm *sumalgo;
	size_t siz;

	sumalgo = &ah_algorithms[sav->alg_auth];
	siz = (((*sumalgo->sumsiz)(sav) + 3) & ~(4 - 1));
	if (AH_MAXSUMSIZE < siz) {
		ipseclog((LOG_DEBUG,
		    "internal error: AH_MAXSUMSIZE must be larger than %lu\n",
		    (u_long)siz));
		ipsecstat.in_inval++;
		goto bad;
	}

	m_copydata(m, m->m_pkthdr.len - siz, siz, &sum0[0]);

	if (esp_auth(m, off, m->m_pkthdr.len - off - siz, sav, sum)) {
		ipseclog((LOG_WARNING, "auth fail in IPv4 ESP input: %s %s\n",
		    ipsec4_logpacketstr(ip, spi), ipsec_logsastr(sav)));
		ipsecstat.in_espauthfail++;
		goto bad;
	}

	if (bcmp(sum0, sum, siz) != 0) {
		ipseclog((LOG_WARNING, "auth fail in IPv4 ESP input: %s %s\n",
		    ipsec4_logpacketstr(ip, spi), ipsec_logsastr(sav)));
		ipsecstat.in_espauthfail++;
		goto bad;
	}

	/* strip off the authentication data */
	m_adj(m, -siz);
	ip = mtod(m, struct ip *);
#ifdef IPLEN_FLIPPED
	ip->ip_len = ip->ip_len - siz;
#else
	ip->ip_len = htons(ntohs(ip->ip_len) - siz);
#endif
	m->m_flags |= M_AUTHIPDGM;
	ipsecstat.in_espauthsucc++;
    }

	/*
	 * update sequence number.
	 */
	if ((sav->flags & SADB_X_EXT_OLD) == 0 && sav->replay) {
		if (ipsec_updatereplay(ntohl(((struct newesp *)esp)->esp_seq), sav)) {
			ipsecstat.in_espreplay++;
			goto bad;
		}
	}

noreplaycheck:

	/* process main esp header. */
	if (sav->flags & SADB_X_EXT_OLD) {
		/* RFC 1827 */
		esplen = sizeof(struct esp);
	} else {
		/* RFC 2406 */
		if (sav->flags & SADB_X_EXT_DERIV)
			esplen = sizeof(struct esp);
		else
			esplen = sizeof(struct newesp);
	}

	if (m->m_pkthdr.len < off + esplen + ivlen + sizeof(esptail)) {
		ipseclog((LOG_WARNING,
		    "IPv4 ESP input: packet too short\n"));
		ipsecstat.in_inval++;
		goto bad;
	}

	if (m->m_len < off + esplen + ivlen) {
		m = m_pullup(m, off + esplen + ivlen);
		if (!m) {
			ipseclog((LOG_DEBUG,
			    "IPv4 ESP input: can't pullup in esp4_input\n"));
			ipsecstat.in_inval++;
			goto bad;
		}
	}

    {
	/*
	 * decrypt the packet.
	 */
	if (!algo->decrypt)
		panic("internal error: no decrypt function");
	if ((*algo->decrypt)(m, off, sav, algo, ivlen)) {
		ipseclog((LOG_ERR, "decrypt fail in IPv4 ESP input: %s %s\n",
		    ipsec4_logpacketstr(ip, spi), ipsec_logsastr(sav)));
		ipsecstat.in_inval++;
		goto bad;
	}
	ipsecstat.in_esphist[sav->alg_enc]++;

	m->m_flags |= M_DECRYPTED;
    }

	/*
	 * find the trailer of the ESP.
	 */
	m_copydata(m, m->m_pkthdr.len - sizeof(esptail), sizeof(esptail),
	     (caddr_t)&esptail);
	nxt = esptail.esp_nxt;
	taillen = esptail.esp_padlen + sizeof(esptail);

	if (m->m_pkthdr.len < taillen
	 || m->m_pkthdr.len - taillen < hlen) {	/*?*/
		ipseclog((LOG_WARNING,
		    "bad pad length in IPv4 ESP input: %s %s\n",
		    ipsec4_logpacketstr(ip, spi), ipsec_logsastr(sav)));
		ipsecstat.in_inval++;
		goto bad;
	}

	/* strip off the trailing pad area. */
	m_adj(m, -taillen);

#ifdef IPLEN_FLIPPED
	ip->ip_len = ip->ip_len - taillen;
#else
	ip->ip_len = htons(ntohs(ip->ip_len) - taillen);
#endif

	/* was it transmitted over the IPsec tunnel SA? */
	if (ipsec4_tunnel_validate(ip, nxt, sav)) {
		/*
		 * strip off all the headers that precedes ESP header.
		 *	IP4 xx ESP IP4' payload -> IP4' payload
		 *
		 * XXX more sanity checks
		 * XXX relationship with gif?
		 */
		u_int8_t tos;

		tos = ip->ip_tos;
		m_adj(m, off + esplen + ivlen);
		if (m->m_len < sizeof(*ip)) {
			m = m_pullup(m, sizeof(*ip));
			if (!m) {
				ipsecstat.in_inval++;
				goto bad;
			}
		}
		ip = mtod(m, struct ip *);
		/* ECN consideration. */
		ip_ecn_egress(ip4_ipsec_ecn, &tos, &ip->ip_tos);
		if (!key_checktunnelsanity(sav, AF_INET,
			    (caddr_t)&ip->ip_src, (caddr_t)&ip->ip_dst)) {
			ipseclog((LOG_ERR, "ipsec tunnel address mismatch "
			    "in IPv4 ESP input: %s %s\n",
			    ipsec4_logpacketstr(ip, spi), ipsec_logsastr(sav)));
			ipsecstat.in_inval++;
			goto bad;
		}

#if 0 /* XXX should call ipfw rather than ipsec_in_reject, shouldn't it ? */
		/* drop it if it does not match the default policy */
		if (ipsec4_in_reject(m, NULL)) {
			ipsecstat.in_polvio++;
			goto bad;
		}
#endif

		key_sa_recordxfer(sav, m);

		s = splimp();
		if (IF_QFULL(&ipintrq)) {
			ipsecstat.in_inval++;
			goto bad;
		}
		IF_ENQUEUE(&ipintrq, m);
		m = NULL;
		schednetisr(NETISR_IP); /*can be skipped but to make sure*/
		splx(s);
		nxt = IPPROTO_DONE;
	} else {
		/*
		 * strip off ESP header and IV.
		 * even in m_pulldown case, we need to strip off ESP so that
		 * we can always compute checksum for AH correctly.
		 */
		size_t stripsiz;

		stripsiz = esplen + ivlen;

		ip = mtod(m, struct ip *);
		ovbcopy((caddr_t)ip, (caddr_t)(((u_char *)ip) + stripsiz), off);
		m->m_data += stripsiz;
		m->m_len -= stripsiz;
		m->m_pkthdr.len -= stripsiz;

		ip = mtod(m, struct ip *);
#ifdef IPLEN_FLIPPED
		ip->ip_len = ip->ip_len - stripsiz;
#else
		ip->ip_len = htons(ntohs(ip->ip_len) - stripsiz);
#endif
		ip->ip_p = nxt;

		key_sa_recordxfer(sav, m);

		if (nxt != IPPROTO_DONE)
			(*inetsw[ip_protox[nxt]].pr_input)(m, off, nxt);
		else
			m_freem(m);
		m = NULL;
	}

	if (sav) {
		KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
			printf("DP esp4_input call free SA:%p\n", sav));
		key_freesav(sav);
	}
	ipsecstat.in_success++;
	return;

bad:
	if (sav) {
		KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
			printf("DP esp4_input call free SA:%p\n", sav));
		key_freesav(sav);
	}
	if (m)
		m_freem(m);
	return;
}
#endif /* INET */

#ifdef INET6
int
esp6_input(mp, offp, proto)
	struct mbuf **mp;
	int *offp, proto;
{
	struct mbuf *m = *mp;
	int off = *offp;
	struct ip6_hdr *ip6;
	struct esp *esp;
	struct esptail esptail;
	u_int32_t spi;
	struct secasvar *sav = NULL;
	size_t taillen;
	u_int16_t nxt;
	struct esp_algorithm *algo;
	int ivlen;
	size_t esplen;
	int s;

	/* sanity check for alignment. */
	if (off % 4 != 0 || m->m_pkthdr.len % 4 != 0) {
		ipseclog((LOG_ERR, "IPv6 ESP input: packet alignment problem "
			"(off=%d, pktlen=%d)\n", off, m->m_pkthdr.len));
		ipsec6stat.in_inval++;
		goto bad;
	}

#ifndef PULLDOWN_TEST
	IP6_EXTHDR_CHECK(m, off, ESPMAXLEN, IPPROTO_DONE);
	esp = (struct esp *)(mtod(m, caddr_t) + off);
#else
	IP6_EXTHDR_GET(esp, struct esp *, m, off, ESPMAXLEN);
	if (esp == NULL) {
		ipsec6stat.in_inval++;
		return IPPROTO_DONE;
	}
#endif
	ip6 = mtod(m, struct ip6_hdr *);

	if (ntohs(ip6->ip6_plen) == 0) {
		ipseclog((LOG_ERR, "IPv6 ESP input: "
		    "ESP with IPv6 jumbogram is not supported.\n"));
		ipsec6stat.in_inval++;
		goto bad;
	}

	/* find the sassoc. */
	spi = esp->esp_spi;

	if ((sav = key_allocsa(AF_INET6,
	                      (caddr_t)&ip6->ip6_src, (caddr_t)&ip6->ip6_dst,
	                      IPPROTO_ESP, spi)) == 0) {
		ipseclog((LOG_WARNING,
		    "IPv6 ESP input: no key association found for spi %u\n",
		    (u_int32_t)ntohl(spi)));
		ipsec6stat.in_nosa++;
		goto bad;
	}
	KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
		printf("DP esp6_input called to allocate SA:%p\n", sav));
	if (sav->state != SADB_SASTATE_MATURE
	 && sav->state != SADB_SASTATE_DYING) {
		ipseclog((LOG_DEBUG,
		    "IPv6 ESP input: non-mature/dying SA found for spi %u\n",
		    (u_int32_t)ntohl(spi)));
		ipsec6stat.in_badspi++;
		goto bad;
	}
	if (sav->alg_enc == SADB_EALG_NONE) {
		ipseclog((LOG_DEBUG, "IPv6 ESP input: "
		    "unspecified encryption algorithm for spi %u\n",
		    (u_int32_t)ntohl(spi)));
		ipsec6stat.in_badspi++;
		goto bad;
	}

	algo = &esp_algorithms[sav->alg_enc];	/*XXX*/

	/* check if we have proper ivlen information */
	ivlen = sav->ivlen;
	if (ivlen < 0) {
		ipseclog((LOG_ERR, "inproper ivlen in IPv6 ESP input: %s %s\n",
		    ipsec6_logpacketstr(ip6, spi), ipsec_logsastr(sav)));
		ipsec6stat.in_badspi++;
		goto bad;
	}

	if (!((sav->flags & SADB_X_EXT_OLD) == 0 && sav->replay
	 && (sav->alg_auth && sav->key_auth)))
		goto noreplaycheck;

	if (sav->alg_auth == SADB_AALG_NULL)
		goto noreplaycheck;

	/*
	 * check for sequence number.
	 */
	if (ipsec_chkreplay(ntohl(((struct newesp *)esp)->esp_seq), sav))
		; /*okey*/
	else {
		ipsec6stat.in_espreplay++;
		ipseclog((LOG_WARNING,
		    "replay packet in IPv6 ESP input: %s %s\n",
		    ipsec6_logpacketstr(ip6, spi), ipsec_logsastr(sav)));
		goto bad;
	}

	/* check ICV */
    {
	u_char sum0[AH_MAXSUMSIZE];
	u_char sum[AH_MAXSUMSIZE];
	struct ah_algorithm *sumalgo;
	size_t siz;

	sumalgo = &ah_algorithms[sav->alg_auth];
	siz = (((*sumalgo->sumsiz)(sav) + 3) & ~(4 - 1));
	if (AH_MAXSUMSIZE < siz) {
		ipseclog((LOG_DEBUG,
		    "internal error: AH_MAXSUMSIZE must be larger than %lu\n",
		    (u_long)siz));
		ipsec6stat.in_inval++;
		goto bad;
	}

	m_copydata(m, m->m_pkthdr.len - siz, siz, &sum0[0]);

	if (esp_auth(m, off, m->m_pkthdr.len - off - siz, sav, sum)) {
		ipseclog((LOG_WARNING, "auth fail in IPv6 ESP input: %s %s\n",
		    ipsec6_logpacketstr(ip6, spi), ipsec_logsastr(sav)));
		ipsec6stat.in_espauthfail++;
		goto bad;
	}

	if (bcmp(sum0, sum, siz) != 0) {
		ipseclog((LOG_WARNING, "auth fail in IPv6 ESP input: %s %s\n",
		    ipsec6_logpacketstr(ip6, spi), ipsec_logsastr(sav)));
		ipsec6stat.in_espauthfail++;
		goto bad;
	}

	/* strip off the authentication data */
	m_adj(m, -siz);
	ip6 = mtod(m, struct ip6_hdr *);
	ip6->ip6_plen = htons(ntohs(ip6->ip6_plen) - siz);

	m->m_flags |= M_AUTHIPDGM;
	ipsec6stat.in_espauthsucc++;
    }

	/*
	 * update sequence number.
	 */
	if ((sav->flags & SADB_X_EXT_OLD) == 0 && sav->replay) {
		if (ipsec_updatereplay(ntohl(((struct newesp *)esp)->esp_seq), sav)) {
			ipsec6stat.in_espreplay++;
			goto bad;
		}
	}

noreplaycheck:

	/* process main esp header. */
	if (sav->flags & SADB_X_EXT_OLD) {
		/* RFC 1827 */
		esplen = sizeof(struct esp);
	} else {
		/* RFC 2406 */
		if (sav->flags & SADB_X_EXT_DERIV)
			esplen = sizeof(struct esp);
		else
			esplen = sizeof(struct newesp);
	}

	if (m->m_pkthdr.len < off + esplen + ivlen + sizeof(esptail)) {
		ipseclog((LOG_WARNING,
		    "IPv6 ESP input: packet too short\n"));
		ipsec6stat.in_inval++;
		goto bad;
	}

#ifndef PULLDOWN_TEST
	IP6_EXTHDR_CHECK(m, off, esplen + ivlen, IPPROTO_DONE);	/*XXX*/
#else
	IP6_EXTHDR_GET(esp, struct esp *, m, off, esplen + ivlen);
	if (esp == NULL) {
		ipsec6stat.in_inval++;
		m = NULL;
		goto bad;
	}
#endif
	ip6 = mtod(m, struct ip6_hdr *);	/*set it again just in case*/

	/*
	 * decrypt the packet.
	 */
	if (!algo->decrypt)
		panic("internal error: no decrypt function");
	if ((*algo->decrypt)(m, off, sav, algo, ivlen)) {
		ipseclog((LOG_ERR, "decrypt fail in IPv6 ESP input: %s %s\n",
		    ipsec6_logpacketstr(ip6, spi), ipsec_logsastr(sav)));
		ipsec6stat.in_inval++;
		goto bad;
	}
	ipsec6stat.in_esphist[sav->alg_enc]++;

	m->m_flags |= M_DECRYPTED;

	/*
	 * find the trailer of the ESP.
	 */
	m_copydata(m, m->m_pkthdr.len - sizeof(esptail), sizeof(esptail),
	     (caddr_t)&esptail);
	nxt = esptail.esp_nxt;
	taillen = esptail.esp_padlen + sizeof(esptail);

	if (m->m_pkthdr.len < taillen
	 || m->m_pkthdr.len - taillen < sizeof(struct ip6_hdr)) {	/*?*/
		ipseclog((LOG_WARNING,
		    "bad pad length in IPv6 ESP input: %s %s\n",
		    ipsec6_logpacketstr(ip6, spi), ipsec_logsastr(sav)));
		ipsec6stat.in_inval++;
		goto bad;
	}

	/* strip off the trailing pad area. */
	m_adj(m, -taillen);

	ip6->ip6_plen = htons(ntohs(ip6->ip6_plen) - taillen);

	/* was it transmitted over the IPsec tunnel SA? */
	if (ipsec6_tunnel_validate(ip6, nxt, sav)) {
		/*
		 * strip off all the headers that precedes ESP header.
		 *	IP6 xx ESP IP6' payload -> IP6' payload
		 *
		 * XXX more sanity checks
		 * XXX relationship with gif?
		 */
		u_int32_t flowinfo;	/*net endian*/
		flowinfo = ip6->ip6_flow;
		m_adj(m, off + esplen + ivlen);
		if (m->m_len < sizeof(*ip6)) {
#ifndef PULLDOWN_TEST
			/*
			 * m_pullup is prohibited in KAME IPv6 input processing
			 * but there's no other way!
			 */
#else
			/* okay to pullup in m_pulldown style */
#endif
			m = m_pullup(m, sizeof(*ip6));
			if (!m) {
				ipsec6stat.in_inval++;
				goto bad;
			}
		}
		ip6 = mtod(m, struct ip6_hdr *);
		/* ECN consideration. */
		ip6_ecn_egress(ip6_ipsec_ecn, &flowinfo, &ip6->ip6_flow);
		if (!key_checktunnelsanity(sav, AF_INET6,
			    (caddr_t)&ip6->ip6_src, (caddr_t)&ip6->ip6_dst)) {
			ipseclog((LOG_ERR, "ipsec tunnel address mismatch "
			    "in IPv6 ESP input: %s %s\n",
			    ipsec6_logpacketstr(ip6, spi),
			    ipsec_logsastr(sav)));
			ipsec6stat.in_inval++;
			goto bad;
		}

#if 0 /* XXX should call ipfw rather than ipsec_in_reject, shouldn't it ? */
		/* drop it if it does not match the default policy */
		if (ipsec6_in_reject(m, NULL)) {
			ipsec6stat.in_polvio++;
			goto bad;
		}
#endif

		key_sa_recordxfer(sav, m);

		s = splimp();
		if (IF_QFULL(&ip6intrq)) {
			ipsec6stat.in_inval++;
			goto bad;
		}
		IF_ENQUEUE(&ip6intrq, m);
		m = NULL;
		schednetisr(NETISR_IPV6); /*can be skipped but to make sure*/
		splx(s);
		nxt = IPPROTO_DONE;
	} else {
		/*
		 * strip off ESP header and IV.
		 * even in m_pulldown case, we need to strip off ESP so that
		 * we can always compute checksum for AH correctly.
		 */
		size_t stripsiz;
		char *prvnxtp;

		/*
		 * Set the next header field of the previous header correctly.
		 */
		prvnxtp = ip6_get_prevhdr(m, off); /* XXX */
		*prvnxtp = nxt;

		stripsiz = esplen + ivlen;

		ip6 = mtod(m, struct ip6_hdr *);
		if (m->m_len >= stripsiz + off) {
			ovbcopy((caddr_t)ip6, ((caddr_t)ip6) + stripsiz, off);
			m->m_data += stripsiz;
			m->m_len -= stripsiz;
			m->m_pkthdr.len -= stripsiz;
		} else {
			/* 
			 * this comes with no copy if the boundary is on
			 * cluster
			 */
			struct mbuf *n;

			n = m_split(m, off, M_DONTWAIT);
			if (n == NULL) {
				/* m is retained by m_split */
				goto bad;
			}
			m_adj(n, stripsiz);
			m_cat(m, n);
			/* m_cat does not update m_pkthdr.len */
			m->m_pkthdr.len += n->m_pkthdr.len;
		}

		ip6 = mtod(m, struct ip6_hdr *);
		ip6->ip6_plen = htons(ntohs(ip6->ip6_plen) - stripsiz);

		key_sa_recordxfer(sav, m);
	}

	*offp = off;
	*mp = m;

	if (sav) {
		KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
			printf("DP esp6_input call free SA:%p\n", sav));
		key_freesav(sav);
	}
	ipsec6stat.in_success++;
	return nxt;

bad:
	if (sav) {
		KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
			printf("DP esp6_input call free SA:%p\n", sav));
		key_freesav(sav);
	}
	if (m)
		m_freem(m);
	return IPPROTO_DONE;
}
#endif /* INET6 */
