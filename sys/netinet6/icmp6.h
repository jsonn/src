/*	$NetBSD: icmp6.h,v 1.2.2.3 1999/08/02 22:36:03 thorpej Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)ip_icmp.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _NETINET6_ICMPV6_H_
#define _NETINET6_ICMPV6_H_

#define ICMPV6_PLD_MAXLEN	1232	/* IPV6_MMTU - sizeof(struct ip6_hdr)
					   - sizeof(struct icmp6_hdr) */

struct icmp6_hdr {
	u_int8_t	icmp6_type;	/* type field */
	u_int8_t	icmp6_code;	/* code field */
	u_int16_t	icmp6_cksum;	/* checksum field */
	union {
		u_int32_t	icmp6_un_data32[1]; /* type-specific field */
		u_int16_t	icmp6_un_data16[2]; /* type-specific field */
		u_int8_t	icmp6_un_data8[4];  /* type-specific field */
	} icmp6_dataun;
};

#define icmp6_data32	icmp6_dataun.icmp6_un_data32
#define icmp6_data16	icmp6_dataun.icmp6_un_data16
#define icmp6_data8	icmp6_dataun.icmp6_un_data8
#define icmp6_pptr	icmp6_data32[0]		/* parameter prob */
#define icmp6_mtu	icmp6_data32[0]		/* packet too big */
#define icmp6_id	icmp6_data16[0]		/* echo request/reply */
#define icmp6_seq	icmp6_data16[1]		/* echo request/reply */
#define icmp6_maxdelay	icmp6_data16[0]		/* mcast group membership */

#define ICMP6_DST_UNREACH		1	/* dest unreachable, codes: */
#define ICMP6_PACKET_TOO_BIG		2	/* packet too big */
#define ICMP6_TIME_EXCEEDED		3	/* time exceeded, code: */
#define ICMP6_PARAM_PROB		4	/* ip6 header bad */

#define ICMP6_ECHO_REQUEST		128	/* echo service */
#define ICMP6_ECHO_REPLY		129	/* echo reply */
#define ICMP6_MEMBERSHIP_QUERY		130	/* group membership query */
#define MLD6_LISTENER_QUERY		130 	/* multicast listener query */
#define ICMP6_MEMBERSHIP_REPORT		131	/* group membership report */
#define MLD6_LISTENER_REPORT		131	/* multicast listener report */
#define ICMP6_MEMBERSHIP_REDUCTION	132	/* group membership termination */
#define MLD6_LISTENER_DONE		132	/* multicast listener done */

#define ND_ROUTER_SOLICIT		133	/* router solicitation */
#define ND_ROUTER_ADVERT		134	/* router advertisment */
#define ND_NEIGHBOR_SOLICIT		135	/* neighbor solicitation */
#define ND_NEIGHBOR_ADVERT		136	/* neighbor advertisment */
#define ND_REDIRECT			137	/* redirect */

#define ICMP6_ROUTER_RENUMBERING	138	/* router renumbering */

#define ICMP6_WRUREQUEST		139	/* who are you request */
#define ICMP6_WRUREPLY			140	/* who are you reply */
#define ICMP6_FQDN_QUERY		139	/* FQDN query */
#define ICMP6_FQDN_REPLY		140	/* FQDN reply */
#define ICMP6_NI_QUERY			139	/* node information request */
#define ICMP6_NI_REPLY			140	/* node information reply */

/* The definitions below are experimental. TBA */
#define MLD6_MTRACE_RESP		141	/* mtrace response(to sender) */
#define MLD6_MTRACE			142	/* mtrace messages */

#define ICMP6_MAXTYPE			142

#define ICMP6_DST_UNREACH_NOROUTE	0	/* no route to destination */
#define ICMP6_DST_UNREACH_ADMIN	 	1	/* administratively prohibited */
#define ICMP6_DST_UNREACH_NOTNEIGHBOR	2	/* not a neighbor(obsolete) */
#define ICMP6_DST_UNREACH_BEYONDSCOPE	2	/* beyond scope of source address */
#define ICMP6_DST_UNREACH_ADDR		3	/* address unreachable */
#define ICMP6_DST_UNREACH_NOPORT	4	/* port unreachable */

#define ICMP6_TIME_EXCEED_TRANSIT 	0	/* ttl==0 in transit */
#define ICMP6_TIME_EXCEED_REASSEMBLY	1	/* ttl==0 in reass */

#define ICMP6_PARAMPROB_HEADER 	 	0	/* erroneous header field */
#define ICMP6_PARAMPROB_NEXTHEADER	1	/* unrecognized next header */
#define ICMP6_PARAMPROB_OPTION		2	/* unrecognized option */

#define ICMP6_INFOMSG_MASK		0x80	/* all informational messages */

#define ICMP6_NI_SUCESS		0	/* node information successful reply */
#define ICMP6_NI_REFUSED	1	/* node information request is refused */
#define ICMP6_NI_UNKNOWN	2	/* unknown Qtype */

#define ICMP6_ROUTER_RENUMBERING_COMMAND  0	/* rr command */
#define ICMP6_ROUTER_RENUMBERING_RESULT   1	/* rr result */
#define ICMP6_ROUTER_RENUMBERING_SEQNUM_RESET   255	/* rr seq num reset */

/* Used in kernel only */
#define ND_REDIRECT_ONLINK	0	/* redirect to an on-link node */
#define ND_REDIRECT_ROUTER	1	/* redirect to a better router */

/*
 * Multicast Listener Discovery
 */
struct mld6_hdr {
	struct icmp6_hdr	mld6_hdr;
	struct in6_addr		mld6_addr; /* multicast address */
};

#define mld6_type	mld6_hdr.icmp6_type
#define mld6_code	mld6_hdr.icmp6_code
#define mld6_cksum	mld6_hdr.icmp6_cksum
#define mld6_maxdelay	mld6_hdr.icmp6_data16[0]
#define mld6_reserved	mld6_hdr.icmp6_data16[1]

/*
 * Neighbor Discovery
 */

struct nd_router_solicit {	/* router solicitation */
	struct icmp6_hdr 	nd_rs_hdr;
	/* could be followed by options */
};

#define nd_rs_type	nd_rs_hdr.icmp6_type
#define nd_rs_code	nd_rs_hdr.icmp6_code
#define nd_rs_cksum	nd_rs_hdr.icmp6_cksum
#define nd_rs_reserved	nd_rs_hdr.icmp6_data32[0]

struct nd_router_advert {	/* router advertisement */
	struct icmp6_hdr	nd_ra_hdr;
	u_int32_t		nd_ra_reachable;	/* reachable time */
	u_int32_t		nd_ra_retransmit;	/* retransmit timer */
	/* could be followed by options */
};

#define nd_ra_type		nd_ra_hdr.icmp6_type
#define nd_ra_code		nd_ra_hdr.icmp6_code
#define nd_ra_cksum		nd_ra_hdr.icmp6_cksum
#define nd_ra_curhoplimit	nd_ra_hdr.icmp6_data8[0]
#define nd_ra_flags_reserved	nd_ra_hdr.icmp6_data8[1]
#define ND_RA_FLAG_MANAGED	0x80
#define ND_RA_FLAG_OTHER	0x40
#define nd_ra_router_lifetime	nd_ra_hdr.icmp6_data16[1]

struct nd_neighbor_solicit {	/* neighbor solicitation */
	struct icmp6_hdr	nd_ns_hdr;
	struct in6_addr		nd_ns_target;	/*target address */
	/* could be followed by options */
};

#define nd_ns_type		nd_ns_hdr.icmp6_type
#define nd_ns_code		nd_ns_hdr.icmp6_code
#define nd_ns_cksum		nd_ns_hdr.icmp6_cksum
#define nd_ns_reserved		nd_ns_hdr.icmp6_data32[0]

struct nd_neighbor_advert {	/* neighbor advertisement */
	struct icmp6_hdr	nd_na_hdr;
	struct in6_addr		nd_na_target;	/* target address */
	/* could be followed by options */
};

#define nd_na_type		nd_na_hdr.icmp6_type
#define nd_na_code		nd_na_hdr.icmp6_code
#define nd_na_cksum		nd_na_hdr.icmp6_cksum
#define nd_na_flags_reserved	nd_na_hdr.icmp6_data32[0]
#if BYTE_ORDER == BIG_ENDIAN
#define ND_NA_FLAG_ROUTER		0x80000000
#define ND_NA_FLAG_SOLICITED		0x40000000
#define ND_NA_FLAG_OVERRIDE		0x20000000
#elif BYTE_ORDER == LITTLE_ENDIAN
#define ND_NA_FLAG_ROUTER		0x80
#define ND_NA_FLAG_SOLICITED		0x40
#define ND_NA_FLAG_OVERRIDE		0x20
#endif

struct nd_redirect {		/* redirect */
	struct icmp6_hdr	nd_rd_hdr;
	struct in6_addr		nd_rd_target;	/* target address */
	struct in6_addr		nd_rd_dst;	/* destination address */
	/* could be followed by options */
};

#define nd_rd_type		nd_rd_hdr.icmp6_type
#define nd_rd_code		nd_rd_hdr.icmp6_code
#define nd_rd_cksum		nd_rd_hdr.icmp6_cksum
#define nd_rd_reserved		nd_rd_hdr.icmp6_data32[0]

struct nd_opt_hdr {		/* Neighbor discovery option header */
	u_int8_t	nd_opt_type;
	u_int8_t	nd_opt_len;
	/* followed by option specific data*/
};

#define ND_OPT_SOURCE_LINKADDR		1
#define ND_OPT_TARGET_LINKADDR		2
#define ND_OPT_PREFIX_INFORMATION	3
#define ND_OPT_REDIRECTED_HEADER	4
#define ND_OPT_MTU			5

struct nd_opt_prefix_info {	/* prefix information */
	u_int8_t	nd_opt_pi_type;
	u_int8_t	nd_opt_pi_len;
	u_int8_t	nd_opt_pi_prefix_len;
	u_int8_t	nd_opt_pi_flags_reserved;
	u_int32_t	nd_opt_pi_valid_time;
	u_int32_t	nd_opt_pi_preferred_time;
	u_int32_t	nd_opt_pi_reserved2;
	struct in6_addr	nd_opt_pi_prefix;
};

#define ND_OPT_PI_FLAG_ONLINK		0x80
#define ND_OPT_PI_FLAG_AUTO		0x40

struct nd_opt_rd_hdr {         /* redirected header */
	u_int8_t	nd_opt_rh_type;
	u_int8_t	nd_opt_rh_len;
	u_int16_t	nd_opt_rh_reserved1;
	u_int32_t	nd_opt_rh_reserved2;
	/* followed by IP header and data */
};

struct nd_opt_mtu {		/* MTU option */
	u_int8_t	nd_opt_mtu_type;
	u_int8_t	nd_opt_mtu_len;
	u_int16_t	nd_opt_mtu_reserved;
	u_int32_t	nd_opt_mtu_mtu;
};

/*
 * icmp6 namelookup
 */

struct icmp6_namelookup {
	struct icmp6_hdr 	icmp6_nl_hdr;
	u_int64_t	icmp6_nl_nonce;
	u_int32_t	icmp6_nl_ttl;
#if 0
	u_int8_t	icmp6_nl_len;
	u_int8_t	icmp6_nl_name[3];
#endif
	/* could be followed by options */
};

/*
 * icmp6 node information
 */
struct icmp6_nodeinfo {
	struct icmp6_hdr icmp6_ni_hdr;
	u_int64_t icmp6_ni_nonce;
	/* could be followed by reply data */
};

#define ni_type		icmp6_ni_hdr.icmp6_type
#define ni_code		icmp6_ni_hdr.icmp6_code
#define ni_cksum	icmp6_ni_hdr.icmp6_cksum
#define ni_qtype	icmp6_ni_hdr.icmp6_data16[0]
#define ni_flags	icmp6_ni_hdr.icmp6_data16[1]


#define NI_QTYPE_NOOP		0 /* NOOP  */
#define NI_QTYPE_SUPTYPES	1 /* Supported Qtypes */
#define NI_QTYPE_FQDN		2 /* FQDN */
#define NI_QTYPE_NODEADDR	3 /* Node Addresses. XXX: spec says 2, but it may be a typo... */

#if BYTE_ORDER == BIG_ENDIAN
#define NI_SUPTYPE_FLAG_COMPRESS	0x1
#define NI_FQDN_FLAG_VALIDTTL		0x1
#define NI_NODEADDR_FLAG_LINKLOCAL	0x1
#define NI_NODEADDR_FLAG_SITELOCAL	0x2
#define NI_NODEADDR_FLAG_GLOBAL		0x4
#define NI_NODEADDR_FLAG_ALL		0x8
#define NI_NODEADDR_FLAG_TRUNCATE	0x10
#elif BYTE_ORDER == LITTLE_ENDIAN
#define NI_SUPTYPE_FLAG_COMPRESS	0x0100
#define NI_FQDN_FLAG_VALIDTTL		0x0100
#define NI_NODEADDR_FLAG_LINKLOCAL	0x0100
#define NI_NODEADDR_FLAG_SITELOCAL	0x0200
#define NI_NODEADDR_FLAG_GLOBAL		0x0400
#define NI_NODEADDR_FLAG_ALL		0x0800
#define NI_NODEADDR_FLAG_TRUNCATE	0x1000
#endif

struct ni_reply_fqdn {
	u_int32_t ni_fqdn_ttl;	/* TTL */
	u_int8_t ni_fqdn_namelen; /* length in octets of the FQDN */
	u_int8_t ni_fqdn_name[3]; /* XXX: alignment */
};

/*
 * Router Renumbering. as router-renum-08.txt
 */
#if BYTE_ORDER == BIG_ENDIAN /* net byte order */
struct icmp6_router_renum {	/* router renumbering header */
	struct icmp6_hdr	rr_hdr;
	u_int8_t		rr_segnum;
	u_int8_t		rr_test : 1;
	u_int8_t		rr_reqresult : 1;
	u_int8_t		rr_forceapply : 1;
	u_int8_t		rr_specsite : 1;
	u_int8_t		rr_prevdone : 1;
	u_int8_t		rr_flags_reserved : 3;
	u_int16_t		rr_maxdelay;
	u_int32_t		rr_reserved;
};
#elif BYTE_ORDER == LITTLE_ENDIAN
struct icmp6_router_renum {	/* router renumbering header */
	struct icmp6_hdr	rr_hdr;
	u_int8_t		rr_segnum;
	u_int8_t		rr_flags_reserved : 3;
	u_int8_t		rr_prevdone : 1;
	u_int8_t		rr_specsite : 1;
	u_int8_t		rr_forceapply : 1;
	u_int8_t		rr_reqresult : 1;
	u_int8_t		rr_test : 1;
	u_int16_t		rr_maxdelay;
	u_int32_t		rr_reserved;
};
#endif /* BYTE_ORDER */

#define rr_type			rr_hdr.icmp6_type
#define rr_code			rr_hdr.icmp6_code
#define rr_cksum		rr_hdr.icmp6_cksum
#define rr_seqnum 		rr_hdr.icmp6_data32[0]

struct rr_pco_match {		/* match prefix part */
	u_int8_t	rpm_code;
	u_int8_t	rpm_len;
	u_int8_t	rpm_ordinal;
	u_int8_t	rpm_matchlen;
	u_int8_t	rpm_minlen;
	u_int8_t	rpm_maxlen;
	u_int16_t	rpm_reserved;
	struct in6_addr	rpm_prefix;
};

#define RPM_PCO_ADD		1
#define RPM_PCO_CHANGE		2
#define RPM_PCO_SETGLOBAL	3
#define RPM_PCO_MAX		4

#if BYTE_ORDER == BIG_ENDIAN /* net byte order */
struct rr_pco_use {		/* use prefix part */
	u_int8_t	rpu_uselen;
	u_int8_t	rpu_keeplen;
	u_int8_t	rpu_mask_onlink : 1;
	u_int8_t	rpu_mask_autonomous : 1;
	u_int8_t	rpu_mask_reserved : 6;
	u_int8_t	rpu_onlink : 1;
	u_int8_t	rpu_autonomous : 1;
	u_int8_t	rpu_raflags_reserved : 6;
	u_int32_t	rpu_vltime;
	u_int32_t	rpu_pltime;
	u_int32_t	rpu_decr_vltime : 1;
	u_int32_t	rpu_decr_pltime : 1;
	u_int32_t	rpu_flags_reserved : 6;
	u_int32_t	rpu_reserved : 24;
	struct in6_addr rpu_prefix;
};
#elif BYTE_ORDER == LITTLE_ENDIAN
struct rr_pco_use {		/* use prefix part */
	u_int8_t	rpu_uselen;
	u_int8_t	rpu_keeplen;
	u_int8_t	rpu_mask_reserved : 6;
	u_int8_t	rpu_mask_autonomous : 1;
	u_int8_t	rpu_mask_onlink : 1;
	u_int8_t	rpu_raflags_reserved : 6;
	u_int8_t	rpu_autonomous : 1;
	u_int8_t	rpu_onlink : 1;
	u_int32_t	rpu_vltime;
	u_int32_t	rpu_pltime;
	u_int32_t	rpu_flags_reserved : 6;
	u_int32_t	rpu_decr_pltime : 1;
	u_int32_t	rpu_decr_vltime : 1;
	u_int32_t	rpu_reserved : 24;
	struct in6_addr rpu_prefix;
};
#endif /* BYTE_ORDER */

#if BYTE_ORDER == BIG_ENDIAN /* net byte order */
struct rr_result {		/* router renumbering result message */
	u_int8_t	rrr_reserved;
	u_int8_t	rrr_flags_reserved : 6;
	u_int8_t	rrr_outofbound : 1;
	u_int8_t	rrr_forbidden : 1;
	u_int8_t	rrr_ordinal;
	u_int8_t	rrr_matchedlen;
	u_int32_t	rrr_ifid;
	struct in6_addr rrr_prefix;
};
#elif BYTE_ORDER == LITTLE_ENDIAN
struct rr_result {		/* router renumbering result message */
	u_int8_t	rrr_reserved;
	u_int8_t	rrr_forbidden : 1;
	u_int8_t	rrr_outofbound : 1;
	u_int8_t	rrr_flags_reserved : 6;
	u_int8_t	rrr_ordinal;
	u_int8_t	rrr_matchedlen;
	u_int32_t	rrr_ifid;
	struct in6_addr rrr_prefix;
};
#endif /* BYTE_ORDER */

/*
 * icmp6 filter structures.
 */

struct icmp6_filter {
	u_int32_t icmp6_filter[8];
};

#ifdef _KERNEL
#define	ICMP6_FILTER_SETPASSALL(filterp) \
    {								\
	int i; u_char *p;					\
	p = (u_char *)filterp;					\
	for (i = 0; i < sizeof(struct icmp6_filter); i++)	\
		p[i] = 0xff;					\
    }
#define	ICMP6_FILTER_SETBLOCKALL(filterp) \
	bzero(filterp, sizeof(struct icmp6_filter))
#else /* _KERNEL */
#define	ICMP6_FILTER_SETPASSALL(filterp) \
	memset(filterp, 0xff, sizeof(struct icmp6_filter))
#define	ICMP6_FILTER_SETBLOCKALL(filterp) \
	memset(filterp, 0x00, sizeof(struct icmp6_filter))
#endif /* _KERNEL */

#define	ICMP6_FILTER_SETPASS(type, filterp) \
	(((filterp)->icmp6_filter[(type) >> 5]) |= (1 << ((type) & 31)))
#define	ICMP6_FILTER_SETBLOCK(type, filterp) \
	(((filterp)->icmp6_filter[(type) >> 5]) &= ~(1 << ((type) & 31)))
#define	ICMP6_FILTER_WILLPASS(type, filterp) \
	((((filterp)->icmp6_filter[(type) >> 5]) & (1 << ((type) & 31))) != 0)
#define	ICMP6_FILTER_WILLBLOCK(type, filterp) \
	((((filterp)->icmp6_filter[(type) >> 5]) & (1 << ((type) & 31))) == 0)


#if defined(__FreeBSD__) || defined(__NetBSD__)
/*
 * Variables related to this implementation
 * of the internet control message protocol version 6.
 */
struct icmp6stat {
/* statistics related to icmp6 packets generated */
	u_long	icp6s_error;		/* # of calls to icmp6_error */
	u_long	icp6s_canterror;	/* no error 'cuz old was icmp */
	u_long	icp6s_toofreq;		/* no error 'cuz rate limitation */
	u_long	icp6s_outhist[256];
/* statistics related to input messages proccesed */
	u_long	icp6s_badcode;		/* icmp6_code out of range */
	u_long	icp6s_tooshort;		/* packet < sizeof(struct icmp6_hdr) */
	u_long	icp6s_checksum;		/* bad checksum */
	u_long	icp6s_badlen;		/* calculated bound mismatch */
	u_long	icp6s_reflect;		/* number of responses */
	u_long	icp6s_inhist[256];	
};

/*
 * Names for ICMP sysctl objects
 */
#define ICMPV6CTL_STATS		1
#define ICMPV6CTL_REDIRACCEPT	2	/* accept/process redirects */
#define ICMPV6CTL_REDIRTIMEOUT	3	/* redirect cache time */
#define ICMPV6CTL_PRINTFS	4
#define ICMPV6CTL_ERRRATELIMIT	5	/* ICMPv6 error rate limitation */
#define ICMPV6CTL_ND6_PRUNE	6
#define ICMPV6CTL_ND6_DELAY	8
#define ICMPV6CTL_ND6_UMAXTRIES	9
#define ICMPV6CTL_ND6_MMAXTRIES		10
#define ICMPV6CTL_ND6_USELOOPBACK	11
#define ICMPV6CTL_ND6_PROXYALL	12
#define ICMPV6CTL_MAXID		13

#define ICMPV6CTL_NAMES { \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ "rediraccept", CTLTYPE_INT }, \
	{ "redirtimeout", CTLTYPE_INT }, \
	{ "printfs", CTLTYPE_INT }, \
	{ "errratelimit", CTLTYPE_INT }, \
	{ "nd6_prune", CTLTYPE_INT }, \
	{ 0, 0 }, \
	{ "nd6_delay", CTLTYPE_INT }, \
	{ "nd6_umaxtries", CTLTYPE_INT }, \
	{ "nd6_mmaxtries", CTLTYPE_INT }, \
	{ "nd6_useloopback", CTLTYPE_INT }, \
	{ "nd6_proxyall", CTLTYPE_INT }, \
}
#endif /*__FreeBSD__*/
#ifdef __bsdi__
/*
 * Variables related to this implementation
 * of the internet control message protocol version 6.
 */
struct icmp6stat {
/* statistics related to icmp6 packets generated */
	u_quad_t icp6s_error;		/* # of calls to icmp6_error */
	u_quad_t icp6s_canterror;	/* no error 'cuz old was icmp */
	u_quad_t icp6s_toofreq;		/* no error 'cuz rate limitation */
	u_quad_t icp6s_outhist[256];
/* statistics related to input messages proccesed */
	u_quad_t icp6s_badcode;		/* icmp6_code out of range */
	u_quad_t icp6s_tooshort;	/* packet < sizeof(struct icmp6_hdr) */
	u_quad_t icp6s_checksum;	/* bad checksum */
	u_quad_t icp6s_badlen;		/* calculated bound mismatch */
	u_quad_t icp6s_reflect;		/* number of responses */
	u_quad_t icp6s_inhist[256];	
};

/*
 * Names for ICMP sysctl objects
 */
#define ICMPV6CTL_REDIRACCEPT		2	/* accept/process redirects */
#define ICMPV6CTL_REDIRTIMEOUT		3	/* redirect cache time */
#define ICMPV6CTL_PRINTFS		4	/* redirect cache time */
#define ICMPV6CTL_STATS			5	/* statistics */
#define ICMPV6CTL_ERRRATELIMIT		6	/* ICMPv6 error rate limitation */
#define ICMPV6CTL_ND6_PRUNE		7
#define ICMPV6CTL_ND6_DELAY		9
#define ICMPV6CTL_ND6_UMAXTRIES		10
#define ICMPV6CTL_ND6_MMAXTRIES		11
#define ICMPV6CTL_ND6_USELOOPBACK	12
#define ICMPV6CTL_ND6_PROXYALL		13
#define ICMPV6CTL_MAXID			14

#ifdef ICMP6PRINTFS
#define __ICMP6PRINTFS	&icmp6printfs
#else
#define __ICMP6PRINTFS	0
#endif

#define ICMPV6CTL_NAMES { \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ "rediraccept",   CTLTYPE_INT }, 	\
	{ "redirtimeout",  CTLTYPE_INT }, 	\
	{ "printfs", CTLTYPE_INT },		\
	{ 0, 0 },		\
	{ "errratelimit", CTLTYPE_INT }, \
	{ "nd6_prune", CTLTYPE_INT },		\
	{ 0, 0 }, \
	{ "nd6_delay", CTLTYPE_INT },		\
	{ "nd6_umaxtries", CTLTYPE_INT },	\
	{ "nd6_mmaxtries", CTLTYPE_INT },	\
	{ "nd6_useloopback", CTLTYPE_INT },	\
	{ "nd6_proxyall", CTLTYPE_INT },	\
}

#define ICMPV6CTL_VARS { \
	0, \
	0, \
	&icmp6_rediraccept,   \
	&icmp6_redirtimeout,  \
	__ICMP6PRINTFS, \
	0, \
	&icmp6errratelim, \
	&nd6_prune,	\
	0, \
	&nd6_delay,	\
	&nd6_umaxtries, \
	&nd6_mmaxtries,	\
	&nd6_useloopback, \
	&nd6_proxyall, \
}
#endif /*__bsdi__*/

#define RTF_PROBEMTU	RTF_PROTO1

#ifdef _KERNEL
# ifdef __STDC__
struct	rtentry;
struct	rttimer;
struct	in6_multi;
# endif
void	icmp6_init __P((void));
void	icmp6_paramerror __P((struct mbuf *, int));
void	icmp6_error __P((struct mbuf *, int, int, int));
int	icmp6_input __P((struct mbuf **, int *, int));
void	icmp6_fasttimo __P((void));
void	icmp6_reflect __P((struct mbuf *, size_t));
void	icmp6_prepare __P((struct mbuf *));
void	icmp6_redirect_input __P((struct mbuf *, int));
void	icmp6_redirect_output __P((struct mbuf *, struct rtentry *));
#ifdef __bsdi__
int	icmp6_sysctl __P((int *, u_int, void *, size_t *, void *, size_t));
void	icmp6_mtuexpire __P((struct rtentry *, struct rttimer *));
#endif /*__bsdi__*/
#ifdef __NetBSD__
int	icmp6_sysctl __P((int *, u_int, void *, size_t *, void *, size_t));
#endif /* __NetBSD__ */

extern struct	icmp6stat icmp6stat;
extern int	icmp6_rediraccept;	/* accept/process redirects */
extern int	icmp6_redirtimeout;	/* cache time for redirect routes */
#endif /* _KERNEL */

#endif /* not _NETINET6_ICMPV6_H_ */

