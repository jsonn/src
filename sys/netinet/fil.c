/*	$NetBSD: fil.c,v 1.15.2.2 1997/11/17 16:32:35 mrg Exp $	*/

/*
 * Copyright (C) 1993-1997 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#if !defined(lint)
static const char sccsid[] = "@(#)fil.c	1.36 6/5/96 (C) 1993-1996 Darren Reed";
static const char rcsid[] = "@(#)Id: fil.c,v 2.0.2.41.2.3 1997/11/12 10:44:22 darrenr Exp ";
#endif

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#if (defined(_KERNEL) || defined(KERNEL)) && !defined(linux)
# include <sys/systm.h>
#else
# include <stdio.h>
# include <string.h>
# ifdef __NetBSD__
#  include <stdlib.h>
# endif
#endif
#include <sys/uio.h>
#if !defined(__SVR4) && !defined(__svr4__)
# ifndef linux
#  include <sys/mbuf.h>
# endif
#else
# include <sys/byteorder.h>
# include <sys/dditypes.h>
# include <sys/stream.h>
#endif
#ifndef linux
# include <sys/protosw.h>
# include <sys/socket.h>
#endif
#include <net/if.h>
#ifdef sun
# include <net/af.h>
#endif
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#ifndef linux
# include <netinet/ip_var.h>
#endif
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include "netinet/ip_compat.h"
#include <netinet/tcpip.h>
#include "netinet/ip_fil.h"
#include "netinet/ip_proxy.h"
#include "netinet/ip_nat.h"
#include "netinet/ip_frag.h"
#include "netinet/ip_state.h"
#include "netinet/ip_auth.h"
#ifndef	MIN
#define	MIN(a,b)	(((a)<(b))?(a):(b))
#endif

#ifndef	_KERNEL
# include "ipf.h"
# include "ipt.h"
extern	int	opts;

# define	FR_IFVERBOSE(ex,second,verb_pr)	if (ex) { verbose verb_pr; \
							  second; }
# define	FR_IFDEBUG(ex,second,verb_pr)	if (ex) { debug verb_pr; \
							  second; }
# define	FR_VERBOSE(verb_pr)			verbose verb_pr
# define	FR_DEBUG(verb_pr)			debug verb_pr
# define	SEND_RESET(ip, qif, if)		send_reset(ip, if)
# define	IPLLOG(a, c, d, e)		ipllog()
#  define	FR_NEWAUTH(m, fi, ip, qif)	fr_newauth((mb_t *)m, fi, ip)
# if SOLARIS
#  define	ICMP_ERROR(b, ip, t, c, if, src) 	icmp_error(ip)
# else
#  define	ICMP_ERROR(b, ip, t, c, if, src) 	icmp_error(b, ip, if)
# endif
#else /* #ifndef _KERNEL */
# define	FR_IFVERBOSE(ex,second,verb_pr)	;
# define	FR_IFDEBUG(ex,second,verb_pr)	;
# define	FR_VERBOSE(verb_pr)
# define	FR_DEBUG(verb_pr)
# define	IPLLOG(a, c, d, e)		ipflog(a, c, d, e)
# if SOLARIS || defined(__sgi)
extern	kmutex_t	ipf_mutex, ipf_auth;
# endif
# if SOLARIS
#  define	FR_NEWAUTH(m, fi, ip, qif)	fr_newauth((mb_t *)m, fi, \
							   ip, qif)
#  define	SEND_RESET(ip, qif, if)		send_reset(ip, qif)
#  define	ICMP_ERROR(b, ip, t, c, if, src) \
			icmp_error(ip, t, c, if, src)
# else /* SOLARIS */
#  define	FR_NEWAUTH(m, fi, ip, qif)	fr_newauth((mb_t *)m, fi, ip)
#  define	SEND_RESET(ip, qif, if)	send_reset((struct tcpiphdr *)ip)
#  ifdef __sgi
#   define	ICMP_ERROR(b, ip, t, c, if, src) \
			icmp_error(b, t, c, if, src, if)
#  else
#   if BSD < 199103
#    ifdef linux
#     define	ICMP_ERROR(b, ip, t, c, if, src) 	icmp_send(b,t,c,0,if)
#    else
#     define	ICMP_ERROR(b, ip, t, c, if, src) \
			icmp_error(mtod(b, ip_t *), t, c, if, src)
#    endif /* linux */
#   else
#    define	ICMP_ERROR(b, ip, t, c, if, src) \
			icmp_error(b, t, c, (src).s_addr, if)
#   endif /* BSD < 199103 */
#  endif /* __sgi */
# endif /* SOLARIS || __sgi */
#endif /* _KERNEL */


struct	filterstats frstats[2] = {{0,0,0,0,0},{0,0,0,0,0}};
struct	frentry	*ipfilter[2][2] = { { NULL, NULL }, { NULL, NULL } },
		*ipacct[2][2] = { { NULL, NULL }, { NULL, NULL } };
struct	frgroup *ipfgroups[3][2];
int	fr_flags = IPF_LOGGING, fr_active = 0;
#if defined(IPFILTER_DEFAULT_BLOCK)
int	fr_pass = FR_NOMATCH|FR_BLOCK;
#else
int	fr_pass = (IPF_DEFAULT_PASS|FR_NOMATCH);
#endif

fr_info_t	frcache[2];

static	void	fr_makefrip __P((int, ip_t *, fr_info_t *));
static	int	fr_tcpudpchk __P((frentry_t *, fr_info_t *));
static	int	frflushlist __P((int, int, int *, frentry_t *, frentry_t **));


/*
 * bit values for identifying presence of individual IP options
 */
struct	optlist	ipopts[20] = {
	{ IPOPT_NOP,	0x000001 },
	{ IPOPT_RR,	0x000002 },
	{ IPOPT_ZSU,	0x000004 },
	{ IPOPT_MTUP,	0x000008 },
	{ IPOPT_MTUR,	0x000010 },
	{ IPOPT_ENCODE,	0x000020 },
	{ IPOPT_TS,	0x000040 },
	{ IPOPT_TR,	0x000080 },
	{ IPOPT_SECURITY, 0x000100 },
	{ IPOPT_LSRR,	0x000200 },
	{ IPOPT_E_SEC,	0x000400 },
	{ IPOPT_CIPSO,	0x000800 },
	{ IPOPT_SATID,	0x001000 },
	{ IPOPT_SSRR,	0x002000 },
	{ IPOPT_ADDEXT,	0x004000 },
	{ IPOPT_VISA,	0x008000 },
	{ IPOPT_IMITD,	0x010000 },
	{ IPOPT_EIP,	0x020000 },
	{ IPOPT_FINN,	0x040000 },
	{ 0,		0x000000 }
};

/*
 * bit values for identifying presence of individual IP security options
 */
struct	optlist	secopt[8] = {
	{ IPSO_CLASS_RES4,	0x01 },
	{ IPSO_CLASS_TOPS,	0x02 },
	{ IPSO_CLASS_SECR,	0x04 },
	{ IPSO_CLASS_RES3,	0x08 },
	{ IPSO_CLASS_CONF,	0x10 },
	{ IPSO_CLASS_UNCL,	0x20 },
	{ IPSO_CLASS_RES2,	0x40 },
	{ IPSO_CLASS_RES1,	0x80 }
};


/*
 * compact the IP header into a structure which contains just the info.
 * which is useful for comparing IP headers with.
 */
static	void	fr_makefrip(hlen, ip, fin)
int hlen;
ip_t *ip;
fr_info_t *fin;
{
	struct optlist *op;
	tcphdr_t *tcp;
	fr_ip_t *fi = &fin->fin_fi;
	u_short optmsk = 0, secmsk = 0, auth = 0;
	int i, mv, ol, off;
	u_char *s, opt;

	fin->fin_fr = NULL;
	fin->fin_tcpf = 0;
	fin->fin_data[0] = 0;
	fin->fin_data[1] = 0;
	fin->fin_rule = -1;
	fin->fin_group = -1;
	fin->fin_id = ip->ip_id;
#ifdef	_KERNEL
	fin->fin_icode = ipl_unreach;
#endif
	fi->fi_v = ip->ip_v;
	fi->fi_tos = ip->ip_tos;
	fin->fin_hlen = hlen;
	fin->fin_dlen = ip->ip_len - hlen;
	tcp = (tcphdr_t *)((char *)ip + hlen);
	fin->fin_dp = (void *)tcp;
	(*(((u_short *)fi) + 1)) = (*(((u_short *)ip) + 4));
	(*(((u_32_t *)fi) + 1)) = (*(((u_32_t *)ip) + 3));
	(*(((u_32_t *)fi) + 2)) = (*(((u_32_t *)ip) + 4));

	fi->fi_fl = (hlen > sizeof(ip_t)) ? FI_OPTIONS : 0;
	off = (ip->ip_off & 0x1fff) << 3;
	if (ip->ip_off & 0x3fff)
		fi->fi_fl |= FI_FRAG;
	switch (ip->ip_p)
	{
	case IPPROTO_ICMP :
		if ((!IPMINLEN(ip, icmp) && !off) ||
		    (off && off < sizeof(struct icmp)))
			fi->fi_fl |= FI_SHORT;
		if (fin->fin_dlen > 1)
			fin->fin_data[0] = *(u_short *)tcp;
		break;
	case IPPROTO_TCP :
		fi->fi_fl |= FI_TCPUDP;
		if ((!IPMINLEN(ip, tcphdr) && !off) ||
		    (off && off < sizeof(struct tcphdr)))
			fi->fi_fl |= FI_SHORT;
		if (!(fi->fi_fl & FI_SHORT) && !off)
			fin->fin_tcpf = tcp->th_flags;
		goto getports;
	case IPPROTO_UDP :
		fi->fi_fl |= FI_TCPUDP;
		if ((!IPMINLEN(ip, udphdr) && !off) ||
		    (off && off < sizeof(struct udphdr)))
			fi->fi_fl |= FI_SHORT;
getports:
		if (!off && (fin->fin_dlen > 3)) {
			fin->fin_data[0] = ntohs(tcp->th_sport);
			fin->fin_data[1] = ntohs(tcp->th_dport);
		}
		break;
	default :
		break;
	}


	for (s = (u_char *)(ip + 1), hlen -= sizeof(*ip); hlen; ) {
		if (!(opt = *s))
			break;
		ol = (opt == IPOPT_NOP) ? 1 : (int)*(s+1);
		if (opt > 1 && (ol < 2 || ol > hlen))
			break;
		for (i = 9, mv = 4; mv >= 0; ) {
			op = ipopts + i;
			if (opt == (u_char)op->ol_val) {
				optmsk |= op->ol_bit;
				if (opt == IPOPT_SECURITY) {
					struct optlist *sp;
					u_char	sec;
					int j, m;

					sec = *(s + 2);	/* classification */
					for (j = 3, m = 2; m >= 0; ) {
						sp = secopt + j;
						if (sec == sp->ol_val) {
							secmsk |= sp->ol_bit;
							auth = *(s + 3);
							auth *= 256;
							auth += *(s + 4);
							break;
						}
						if (sec < sp->ol_val)
							j -= m--;
						else
							j += m--;
					}
				}
				break;
			}
			if (opt < op->ol_val)
				i -= mv--;
			else
				i += mv--;
		}
		hlen -= ol;
		s += ol;
	}
	if (auth && !(auth & 0x0100))
		auth &= 0xff00;
	fi->fi_optmsk = optmsk;
	fi->fi_secmsk = secmsk;
	fi->fi_auth = auth;
}


/*
 * check an IP packet for TCP/UDP characteristics such as ports and flags.
 */
static int fr_tcpudpchk(fr, fin)
frentry_t *fr;
fr_info_t *fin;
{
	register u_short po, tup;
	register char i;
	register int err = 1;

	/*
	 * Both ports should *always* be in the first fragment.
	 * So far, I cannot find any cases where they can not be.
	 *
	 * compare destination ports
	 */
	if ((i = (int)fr->fr_dcmp)) {
		po = fr->fr_dport;
		tup = fin->fin_data[1];
		/*
		 * Do opposite test to that required and
		 * continue if that succeeds.
		 */
		if (!--i && tup != po) /* EQUAL */
			err = 0;
		else if (!--i && tup == po) /* NOTEQUAL */
			err = 0;
		else if (!--i && tup >= po) /* LESSTHAN */
			err = 0;
		else if (!--i && tup <= po) /* GREATERTHAN */
			err = 0;
		else if (!--i && tup > po) /* LT or EQ */
			err = 0;
		else if (!--i && tup < po) /* GT or EQ */
			err = 0;
		else if (!--i &&	   /* Out of range */
			 (tup >= po && tup <= fr->fr_dtop))
			err = 0;
		else if (!--i &&	   /* In range */
			 (tup <= po || tup >= fr->fr_dtop))
			err = 0;
	}
	/*
	 * compare source ports
	 */
	if (err && (i = (int)fr->fr_scmp)) {
		po = fr->fr_sport;
		tup = fin->fin_data[0];
		if (!--i && tup != po)
			err = 0;
		else if (!--i && tup == po)
			err = 0;
		else if (!--i && tup >= po)
			err = 0;
		else if (!--i && tup <= po)
			err = 0;
		else if (!--i && tup > po)
			err = 0;
		else if (!--i && tup < po)
			err = 0;
		else if (!--i &&	   /* Out of range */
			 (tup >= po && tup <= fr->fr_stop))
			err = 0;
		else if (!--i &&	   /* In range */
			 (tup <= po || tup >= fr->fr_stop))
			err = 0;
	}

	/*
	 * If we don't have all the TCP/UDP header, then how can we
	 * expect to do any sort of match on it ?  If we were looking for
	 * TCP flags, then NO match.  If not, then match (which should
	 * satisfy the "short" class too).
	 */
	if (err && (fin->fin_fi.fi_p == IPPROTO_TCP)) {
		if (fin->fin_fi.fi_fl & FI_SHORT)
			return !(fr->fr_tcpf | fr->fr_tcpfm);
		/*
		 * Match the flags ?  If not, abort this match.
		 */
		if (fr->fr_tcpf &&
		    fr->fr_tcpf != (fin->fin_tcpf & fr->fr_tcpfm)) {
			FR_DEBUG(("f. %#x & %#x != %#x\n", fin->fin_tcpf,
				 fr->fr_tcpfm, fr->fr_tcpf));
			err = 0;
		}
	}
	return err;
}

/*
 * Check the input/output list of rules for a match and result.
 * Could be per interface, but this gets real nasty when you don't have
 * kernel sauce.
 */
int fr_scanlist(pass, ip, fin, m)
int pass;
ip_t *ip;
register fr_info_t *fin;
void *m;
{
	register struct frentry *fr;
	register fr_ip_t *fi = &fin->fin_fi;
	int rulen, portcmp = 0, off, skip = 0;

	fr = fin->fin_fr;
	fin->fin_fr = NULL;
	fin->fin_rule = 0;
	fin->fin_group = 0;
	off = ip->ip_off & 0x1fff;
	pass |= (fi->fi_fl << 24);

	if ((fi->fi_fl & FI_TCPUDP) && (fin->fin_dlen > 3) && !off)
		portcmp = 1;

	for (rulen = 0; fr; fr = fr->fr_next, rulen++) {
		if (skip) {
			skip--;
			continue;
		}
		/*
		 * In all checks below, a null (zero) value in the
		 * filter struture is taken to mean a wildcard.
		 *
		 * check that we are working for the right interface
		 */
#ifdef	_KERNEL
		if (fr->fr_ifa && fr->fr_ifa != fin->fin_ifp)
			continue;
#else
		if (opts & (OPT_VERBOSE|OPT_DEBUG))
			printf("\n");
		FR_VERBOSE(("%c", (pass & FR_PASS) ? 'p' : 
				  (pass & FR_AUTH) ? 'a' : 'b'));
		if (fr->fr_ifa && fr->fr_ifa != fin->fin_ifp)
			continue;
		FR_VERBOSE((":i"));
#endif
		{
			register u_32_t	*ld, *lm, *lip;
			register int i;

			lip = (u_32_t *)fi;
			lm = (u_32_t *)&fr->fr_mip;
			ld = (u_32_t *)&fr->fr_ip;
			i = ((lip[0] & lm[0]) != ld[0]);
			FR_IFDEBUG(i,continue,("0. %#08x & %#08x != %#08x\n",
				   lip[0], lm[0], ld[0]));
			i |= ((lip[1] & lm[1]) != ld[1]) << 21;
			FR_IFDEBUG(i,continue,("1. %#08x & %#08x != %#08x\n",
				   lip[1], lm[1], ld[1]));
			i |= ((lip[2] & lm[2]) != ld[2]) << 22;
			FR_IFDEBUG(i,continue,("2. %#08x & %#08x != %#08x\n",
				   lip[2], lm[2], ld[2]));
			i |= ((lip[3] & lm[3]) != ld[3]);
			FR_IFDEBUG(i,continue,("3. %#08x & %#08x != %#08x\n",
				   lip[3], lm[3], ld[3]));
			i |= ((lip[4] & lm[4]) != ld[4]);
			FR_IFDEBUG(i,continue,("4. %#08x & %#08x != %#08x\n",
				   lip[4], lm[4], ld[4]));
			i ^= (fi->fi_fl & (FR_NOTSRCIP|FR_NOTDSTIP));
			if (i)
				continue;
		}

		/*
		 * If a fragment, then only the first has what we're looking
		 * for here...
		 */
		if (fi->fi_fl & FI_TCPUDP) {
			if (portcmp) {
				if (!fr_tcpudpchk(fr, fin))
					continue;
			} else if (fr->fr_dcmp || fr->fr_scmp || fr->fr_tcpf ||
				   fr->fr_tcpfm)
				continue;
		} else if (fi->fi_p == IPPROTO_ICMP) {
			if (!off && (fin->fin_dlen > 1)) {
				if ((fin->fin_data[0] & fr->fr_icmpm) !=
				    fr->fr_icmp) {
					FR_DEBUG(("i. %#x & %#x != %#x\n",
						 fin->fin_data[0],
						 fr->fr_icmpm, fr->fr_icmp));
					continue;
				}
			} else if (fr->fr_icmpm || fr->fr_icmp)
				continue;
		}
		FR_VERBOSE(("*"));
		/*
		 * Just log this packet...
		 */
		if (!(skip = fr->fr_skip))
			pass = fr->fr_flags;
		if ((pass & FR_CALLNOW) && fr->fr_func)
			pass = (*fr->fr_func)(pass, ip, fin);
#ifdef  IPFILTER_LOG
		if ((pass & FR_LOGMASK) == FR_LOG) {
			if (!IPLLOG(fr->fr_flags, ip, fin, m))
				frstats[fin->fin_out].fr_skip++;
			frstats[fin->fin_out].fr_pkl++;
		}
#endif /* IPFILTER_LOG */
		FR_DEBUG(("pass %#x\n", pass));
		fr->fr_hits++;
		if (pass & FR_ACCOUNT)
			fr->fr_bytes += (U_QUAD_T)ip->ip_len;
		else
			fin->fin_icode = fr->fr_icode;
		fin->fin_rule = rulen;
		fin->fin_group = fr->fr_group;
		fin->fin_fr = fr;
		if (fr->fr_grp) {
			fin->fin_fr = fr->fr_grp;
			pass = fr_scanlist(pass, ip, fin, m);
			if (fin->fin_fr == NULL) {
				fin->fin_rule = rulen;
				fin->fin_group = fr->fr_group;
				fin->fin_fr = fr;
			}
		}
		if (pass & FR_QUICK)
			break;
	}
	return pass;
}


/*
 * frcheck - filter check
 * check using source and destination addresses/pors in a packet whether
 * or not to pass it on or not.
 */
int fr_check(ip, hlen, ifp, out
#if defined(_KERNEL) && SOLARIS
, qif, mp)
qif_t *qif;
#else
, mp)
#endif
mb_t **mp;
ip_t *ip;
int hlen;
void *ifp;
int out;
{
	/*
	 * The above really sucks, but short of writing a diff
	 */
	fr_info_t frinfo, *fc;
	register fr_info_t *fin = &frinfo;
	frentry_t *fr = NULL;
	int pass, changed, apass;
#if !SOLARIS || !defined(_KERNEL)
	register mb_t *m = *mp;
#endif

#ifdef	_KERNEL
	mb_t *mc = NULL;
# if !defined(__SVR4) && !defined(__svr4__)
#  ifdef __sgi
	char hbuf[(0xf << 2) + sizeof(struct icmp) + sizeof(ip_t) + 8];
#  endif
	int up;

	if ((ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP ||
	     ip->ip_p == IPPROTO_ICMP)) {
		int plen = 0;

		switch(ip->ip_p)
		{
		case IPPROTO_TCP:
			plen = sizeof(tcphdr_t);
			break;
		case IPPROTO_UDP:
			plen = sizeof(udphdr_t);
			break;
		case IPPROTO_ICMP:
			/* 96 - enough for complete ICMP error IP header */
			plen = sizeof(struct icmp) + sizeof(ip_t) + 8;
			break;
		}
		up = MIN(hlen + plen, ip->ip_len);

		if (up > m->m_len) {
#ifdef __sgi /* Under IRIX, avoid m_pullup as it makes ping <hostname> panic */
			if ((up > sizeof(hbuf)) || (m_length(m) < up)) {
				frstats[out].fr_pull[1]++;
				return -1;
			}
			m_copydata(m, 0, up, hbuf);
			frstats[out].fr_pull[0]++;
			ip = (ip_t *)hbuf;
#else
# ifndef linux
			if ((*mp = m_pullup(m, up)) == 0) {
				frstats[out].fr_pull[1]++;
				return -1;
			} else {
				frstats[out].fr_pull[0]++;
				m = *mp;
				ip = mtod(m, ip_t *);
			}
# endif
#endif
		} else
			up = 0;
	} else
		up = 0;
# endif
# if SOLARIS
	mb_t *m = qif->qf_m;
# endif
#endif
	fr_makefrip(hlen, ip, fin);
	fin->fin_ifp = ifp;
	fin->fin_out = out;
	fin->fin_mp = mp;

	MUTEX_ENTER(&ipf_mutex);

	/*
	 * Check auth now.  This, combined with the check below to see if apass
	 * is 0 is to ensure that we don't count the packet twice, which can
	 * otherwise occur when we reprocess it.  As it is, we only count it
	 * after it has no auth. table matchup.  This also stops NAT from
	 * occuring until after the packet has been auth'd.
	 */
	apass = fr_checkauth(ip, fin);

	if (!out) {
		changed = ip_natin(ip, hlen, fin);
		if (!apass && (fin->fin_fr = ipacct[0][fr_active]) &&
		    (FR_SCANLIST(FR_NOMATCH, ip, fin, m) & FR_ACCOUNT))
			frstats[0].fr_acct++;
	}

	if (apass || (!(pass = ipfr_knownfrag(ip, fin)) &&
	    !(pass = fr_checkstate(ip, fin)))) {
		/*
		 * If a packet is found in the auth table, then skip checking
		 * the access lists for permission but we do need to consider
		 * the result as if it were from the ACL's.
		 */
		if (!apass) {
			fc = frcache + out;
			if (!bcmp((char *)fin, (char *)fc, FI_CSIZE)) {
				/*
				 * copy cached data so we can unlock the mutex
				 * earlier.
				 */
				bcopy((char *)fc, (char *)fin, FI_COPYSIZE);
				frstats[out].fr_chit++;
				if ((fr = fin->fin_fr)) {
					fr->fr_hits++;
					pass = fr->fr_flags;
				} else
					pass = fr_pass;
			} else {
				pass = fr_pass;
				if ((fin->fin_fr = ipfilter[out][fr_active]))
					pass = FR_SCANLIST(fr_pass, ip, fin, m);
				bcopy((char *)fin, (char *)fc, FI_COPYSIZE);
				if (pass & FR_NOMATCH)
					frstats[out].fr_nom++;
			}
			fr = fin->fin_fr;
		} else
			pass = apass;

		/*
		 * If we fail to add a packet to the authorization queue,
		 * then we drop the packet later.  However, if it was added
		 * then pretend we've dropped it already.
		 */
		if ((pass & FR_AUTH))
			if (FR_NEWAUTH(m, fin, ip, qif) != 0)
#ifdef	_KERNEL
				m = *mp = NULL;
#else
				;
#endif

		if (pass & FR_PREAUTH) {
			MUTEX_ENTER(&ipf_auth);
			if ((fin->fin_fr = ipauth) &&
			    (pass = FR_SCANLIST(0, ip, fin, m)))
				fr_authstats.fas_hits++;
			else
				fr_authstats.fas_miss++;
			MUTEX_EXIT(&ipf_auth);
		}

		if (pass & FR_KEEPFRAG) {
			if (fin->fin_fi.fi_fl & FI_FRAG) {
				if (ipfr_newfrag(ip, fin, pass) == -1)
					frstats[out].fr_bnfr++;
				else
					frstats[out].fr_nfr++;
			} else
				frstats[out].fr_cfr++;
		}
		if (pass & FR_KEEPSTATE) {
			if (fr_addstate(ip, fin, pass) == -1)
				frstats[out].fr_bads++;
			else
				frstats[out].fr_ads++;
		}
	}

	if (fr && fr->fr_func && !(pass & FR_CALLNOW))
		pass = (*fr->fr_func)(pass, ip, fin);

	/*
	 * Only count/translate packets which will be passed on, out the
	 * interface.
	 */
	if (out && (pass & FR_PASS)) {
		if ((fin->fin_fr = ipacct[1][fr_active]) &&
		    (FR_SCANLIST(FR_NOMATCH, ip, fin, m) & FR_ACCOUNT))
			frstats[1].fr_acct++;
		fin->fin_fr = NULL;
		changed = ip_natout(ip, hlen, fin);
	}
	fin->fin_fr = fr;
	MUTEX_EXIT(&ipf_mutex);

#ifdef	IPFILTER_LOG
	if ((fr_flags & FF_LOGGING) || (pass & FR_LOGMASK)) {
		if ((fr_flags & FF_LOGNOMATCH) && (pass & FR_NOMATCH)) {
			pass |= FF_LOGNOMATCH;
			frstats[out].fr_npkl++;
			goto logit;
		} else if (((pass & FR_LOGMASK) == FR_LOGP) ||
		    ((pass & FR_PASS) && (fr_flags & FF_LOGPASS))) {
			if ((pass & FR_LOGMASK) != FR_LOGP)
				pass |= FF_LOGPASS;
			frstats[out].fr_ppkl++;
			goto logit;
		} else if (((pass & FR_LOGMASK) == FR_LOGB) ||
			   ((pass & FR_BLOCK) && (fr_flags & FF_LOGBLOCK))) {
			if ((pass & FR_LOGMASK) != FR_LOGB)
				pass |= FF_LOGBLOCK;
			frstats[out].fr_bpkl++;
logit:
			if (!IPLLOG(pass, ip, fin, m)) {
				frstats[out].fr_skip++;
				if ((pass & (FR_PASS|FR_LOGORBLOCK)) ==
				    (FR_PASS|FR_LOGORBLOCK))
					pass ^= FR_PASS|FR_BLOCK;
			}
		}
	}
#endif /* IPFILTER_LOG */
#ifdef	_KERNEL
	/*
	 * Only allow FR_DUP to work if a rule matched - it makes no sense to
	 * set FR_DUP as a "default" as there are no instructions about where
	 * to send the packet.
	 */
	if (fr && (pass & FR_DUP))
# if	SOLARIS
		mc = dupmsg(m);
# else
#  ifndef linux
		mc = m_copy(m, 0, M_COPYALL);
#  endif
# endif
#endif

	if (pass & FR_PASS)
		frstats[out].fr_pass++;
	else if (pass & FR_BLOCK) {
		frstats[out].fr_block++;
		/*
		 * Should we return an ICMP packet to indicate error
		 * status passing through the packet filter ?
		 * WARNING: ICMP error packets AND TCP RST packets should
		 * ONLY be sent in repsonse to incoming packets.  Sending them
		 * in response to outbound packets can result in a panic on
		 * some operating systems.
		 */
		if (!out) {
#ifdef	_KERNEL
			if (pass & FR_RETICMP) {
# if SOLARIS
				ICMP_ERROR(q, ip, ICMP_UNREACH, fin->fin_icode,
					   qif, ip->ip_src);
# else
				ICMP_ERROR(m, ip, ICMP_UNREACH, fin->fin_icode,
					   ifp, ip->ip_src);
				m = *mp = NULL;	/* freed by icmp_error() */
# endif

				frstats[0].fr_ret++;
			} else if ((pass & FR_RETRST) &&
				   !(fin->fin_fi.fi_fl & FI_SHORT)) {
				if (SEND_RESET(ip, qif, ifp) == 0)
					frstats[1].fr_ret++;
			}
#else
			if (pass & FR_RETICMP) {
				verbose("- ICMP unreachable sent\n");
				frstats[0].fr_ret++;
			} else if ((pass & FR_RETRST) &&
				   !(fin->fin_fi.fi_fl & FI_SHORT)) {
				verbose("- TCP RST sent\n");
				frstats[1].fr_ret++;
			}
#endif
		}
	}

	/*
	 * If we didn't drop off the bottom of the list of rules (and thus
	 * the 'current' rule fr is not NULL), then we may have some extra
	 * instructions about what to do with a packet.
	 * Once we're finished return to our caller, freeing the packet if
	 * we are dropping it (* BSD ONLY *).
	 */
#if defined(_KERNEL)
# if !SOLARIS
#  if !defined(linux)
	if (fr) {
		frdest_t *fdp = &fr->fr_tif;

		if ((pass & FR_FASTROUTE) ||
		    (fdp->fd_ifp && fdp->fd_ifp != (struct ifnet *)-1)) {
			ipfr_fastroute(m, fin, fdp);
			m = *mp = NULL;
		}
		if (mc)
			ipfr_fastroute(mc, fin, &fr->fr_dif);
	}
	if (!(pass & FR_PASS) && m)
		m_freem(m);
#   ifdef __sgi
	else if (changed && up && m)
		m_copyback(m, 0, up, hbuf);
#   endif
#  endif /* !linux */
	return (pass & FR_PASS) ? 0 : -1;
#  else /* !SOLARIS */
	if (fr) {
		frdest_t *fdp = &fr->fr_tif;

		if ((pass & FR_FASTROUTE) ||
		    (fdp->fd_ifp && fdp->fd_ifp != (struct ifnet *)-1)) {
			ipfr_fastroute(qif, ip, m, mp, fin, fdp);
			m = *mp = NULL;
		}
		if (mc)
			ipfr_fastroute(qif, ip, mc, mp, fin, &fr->fr_dif);
	}
	return (pass & FR_PASS) ? changed : -1;
# endif /* !SOLARIS */
#else /* _KERNEL */
	if (pass & FR_NOMATCH)
		return 1;
	if (pass & FR_PASS)
		return 0;
	if (pass & FR_AUTH)
		return -2;
	return -1;
#endif /* _KERNEL */
}


/*
 * ipf_cksum
 * addr should be 16bit aligned and len is in bytes.
 */
u_short ipf_cksum(addr, len)
register u_short *addr;
register int len;
{
	register u_long sum = 0;

	for (sum = 0; len > 1; len -= 2)
		sum += *addr++;

	/* mop up an odd byte, if necessary */
	if (len == 1)
		sum += *(u_char *)addr;

	/*
	 * add back carry outs from top 16 bits to low 16 bits
	 */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	return (u_short)(~sum);
}


/*
 * NB: This function assumes we've pullup'd enough for all of the IP header
 * and the TCP header.  We also assume that data blocks aren't allocated in
 * odd sizes.
 */
u_short fr_tcpsum(m, ip, tcp)
mb_t *m;
ip_t *ip;
tcphdr_t *tcp;
{
	union {
		u_char	c[2];
		u_short	s;
	} bytes;
	u_long sum;
	u_short	*sp;
	int len;
# if SOLARIS || defined(__sgi)
	int add, hlen;
# endif

# if SOLARIS
	/* skip any leading M_PROTOs */
	while(m && (MTYPE(m) != M_DATA))
		m = m->b_cont;
	PANIC((!m),("fr_tcpsum: no M_DATA"));
# endif

	/*
	 * Add up IP Header portion
	 */
	len = ip->ip_len - (ip->ip_hl << 2);
	bytes.c[0] = 0;
	bytes.c[1] = IPPROTO_TCP;
	sum = bytes.s;
	sum += htons((u_short)len);
	sp = (u_short *)&ip->ip_src;
	sum += *sp++;
	sum += *sp++;
	sum += *sp++;
	sum += *sp++;
	if (sp != (u_short *)tcp)
		sp = (u_short *)tcp;
	sum += *sp++;
	sum += *sp++;
	sum += *sp++;
	sum += *sp++;
	sum += *sp++;
	sum += *sp++;
	sum += *sp++;
	sum += *sp;
	sp += 2;	/* Skip over checksum */
	sum += *sp++;

#if SOLARIS
	/*
	 * In case we had to copy the IP & TCP header out of mblks,
	 * skip over the mblk bits which are the header
	 */
	if ((caddr_t)ip != (caddr_t)m->b_rptr) {
		hlen = (caddr_t)sp - (caddr_t)ip;
		while (hlen) {
			add = MIN(hlen, m->b_wptr - m->b_rptr);
			sp = (u_short *)((caddr_t)m->b_rptr + add);
			hlen -= add;
			if ((caddr_t)sp >= (caddr_t)m->b_wptr) {
				m = m->b_cont;
				PANIC((!m),("fr_tcpsum: not enough data"));
				if (!hlen)
					sp = (u_short *)m->b_rptr;
			}
		}
	}
#endif
#ifdef	__sgi
	/*
	 * In case we had to copy the IP & TCP header out of mbufs,
	 * skip over the mbuf bits which are the header
	 */
	if ((caddr_t)ip != mtod(m, caddr_t)) {
		hlen = (caddr_t)sp - (caddr_t)ip;
		while (hlen) {
			add = MIN(hlen, m->m_len);
			sp = (u_short *)(mtod(m, caddr_t) + add);
			hlen -= add;
			if (add >= m->m_len) {
				m = m->m_next;
				PANIC((!m),("fr_tcpsum: not enough data"));
				if (!hlen)
					sp = mtod(m, u_short *);
			}
		}
	}
#endif

	if (!(len -= sizeof(*tcp)))
		goto nodata;
	while (len > 0) {
#if SOLARIS
		while ((caddr_t)sp >= (caddr_t)m->b_wptr) {
			m = m->b_cont;
			PANIC((!m),("fr_tcpsum: not enough data"));
			sp = (u_short *)m->b_rptr;
		}
#else
		while (((caddr_t)sp - mtod(m, caddr_t)) >= m->m_len)
		{
			m = m->m_next;
			PANIC((!m),("fr_tcpsum: not enough data"));
			sp = mtod(m, u_short *);
		}
#endif /* SOLARIS */
		if (len < 2)
			break;
		if((u_long)sp & 1) {
			bcopy(sp++, &bytes.s, sizeof(bytes.s));
			sum += bytes.s;
		} else
			sum += *sp++;
		len -= 2;
	}
	if (len) {
		bytes.c[1] = 0;
		bytes.c[0] = *(u_char *)sp;
		sum += bytes.s;
	}
nodata:
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	sum = (u_short)((~sum) & 0xffff);
	return sum;
}


#if defined(_KERNEL) && ( ((BSD < 199306) && !SOLARIS) || defined(__sgi) )
/*
 * Copyright (c) 1982, 1986, 1988, 1991, 1993
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
 *	@(#)uipc_mbuf.c	8.2 (Berkeley) 1/4/94
 * Id: fil.c,v 2.0.2.41.2.3 1997/11/12 10:44:22 darrenr Exp 
 */
/*
 * Copy data from an mbuf chain starting "off" bytes from the beginning,
 * continuing for "len" bytes, into the indicated buffer.
 */
void
m_copydata(m, off, len, cp)
	register mb_t *m;
	register int off;
	register int len;
	caddr_t cp;
{
	register unsigned count;

	if (off < 0 || len < 0)
		panic("m_copydata");
	while (off > 0) {
		if (m == 0)
			panic("m_copydata");
		if (off < m->m_len)
			break;
		off -= m->m_len;
		m = m->m_next;
	}
	while (len > 0) {
		if (m == 0)
			panic("m_copydata");
		count = MIN(m->m_len - off, len);
		bcopy(mtod(m, caddr_t) + off, cp, count);
		len -= count;
		cp += count;
		off = 0;
		m = m->m_next;
	}
}


# ifndef linux
/*
 * Copy data from a buffer back into the indicated mbuf chain,
 * starting "off" bytes from the beginning, extending the mbuf
 * chain if necessary.
 */
void
m_copyback(m0, off, len, cp)
	struct	mbuf *m0;
	register int off;
	register int len;
	caddr_t cp;
{
	register int mlen;
	register struct mbuf *m = m0, *n;
	int totlen = 0;

	if (m0 == 0)
		return;
	while (off > (mlen = m->m_len)) {
		off -= mlen;
		totlen += mlen;
		if (m->m_next == 0) {
			n = m_getclr(M_DONTWAIT, m->m_type);
			if (n == 0)
				goto out;
			n->m_len = min(MLEN, len + off);
			m->m_next = n;
		}
		m = m->m_next;
	}
	while (len > 0) {
		mlen = min (m->m_len - off, len);
		bcopy(cp, off + mtod(m, caddr_t), (unsigned)mlen);
		cp += mlen;
		len -= mlen;
		mlen += off;
		off = 0;
		totlen += mlen;
		if (len == 0)
			break;
		if (m->m_next == 0) {
			n = m_get(M_DONTWAIT, m->m_type);
			if (n == 0)
				break;
			n->m_len = min(MLEN, len);
			m->m_next = n;
		}
		m = m->m_next;
	}
out:
#if 0
	if (((m = m0)->m_flags & M_PKTHDR) && (m->m_pkthdr.len < totlen))
		m->m_pkthdr.len = totlen;
#endif
	return;
}
# endif /* linux */
#endif /* (_KERNEL) && ( ((BSD < 199306) && !SOLARIS) || __sgi) */


frgroup_t *fr_findgroup(num, flags, which, set, fgpp)
u_short num;
u_32_t flags;
int which, set;
frgroup_t ***fgpp;
{
	frgroup_t *fg, **fgp;

	if (which == IPL_LOGAUTH)
		fgp = &ipfgroups[2][set];
	else if (flags & FR_ACCOUNT)
		fgp = &ipfgroups[1][set];
	else if (flags & (FR_OUTQUE|FR_INQUE))
		fgp = &ipfgroups[0][set];
	else
		return NULL;

	while ((fg = *fgp))
		if (fg->fg_num == num)
			break;
		else
			fgp = &fg->fg_next;
	if (fgpp)
		*fgpp = fgp;
	return fg;
}


frgroup_t *fr_addgroup(num, fp, which, set)
u_short num;
frentry_t *fp;
int which, set;
{
	frgroup_t *fg, **fgp;

	if ((fg = fr_findgroup(num, fp->fr_flags, which, set, &fgp)))
		return fg;

	KMALLOC(fg, frgroup_t *, sizeof(*fg));
	if (fg) {
		fg->fg_num = num;
		fg->fg_next = *fgp;
		fg->fg_head = fp;
		fg->fg_start = &fp->fr_grp;
		*fgp = fg;
	}
	return fg;
}


void fr_delgroup(num, flags, which, set)
u_short num;
u_32_t flags;
int which, set;
{
	frgroup_t *fg, **fgp;
 
	if (!(fg = fr_findgroup(num, flags, which, set, &fgp)))
		return;
 
	*fgp = fg->fg_next;
	KFREE(fg);
}



/*
 * recursively flush rules from the list, descending groups as they are
 * encountered.  if a rule is the head of a group and it has lost all its
 * group members, then also delete the group reference.
 */
static int frflushlist(set, unit, nfreedp, list, listp)
int set, unit, *nfreedp;
frentry_t *list, **listp;
{
	register frentry_t *fp = list, *fpn;
	register int freed = 0;

	while (fp) {
		fpn = fp->fr_next;
		if (fp->fr_grp) {
			fp->fr_ref -= frflushlist(set, unit, nfreedp,
						  fp->fr_grp, &fp->fr_grp);
		}

		if (fp->fr_ref == 1) {
			if (fp->fr_grhead)
				fr_delgroup(fp->fr_grhead, fp->fr_flags, unit,
					    set);
			KFREE(fp);
			*listp = fpn;
			freed++;
		}
		fp = fpn;
	}
	*nfreedp += freed;
	return freed;
}


void frflush(unit, data)
int unit;
caddr_t data;
{
	int flags = *(int *)data, flushed = 0, set = fr_active;

	bzero((char *)frcache, sizeof(frcache[0]) * 2);

	if (flags & FR_INACTIVE)
		set = 1 - set;

	if (unit == IPL_LOGIPF) {
		if (flags & FR_OUTQUE) {
			(void) frflushlist(set, unit, &flushed,
					   ipfilter[1][set],
					   &ipfilter[1][set]);
			(void) frflushlist(set, unit, &flushed,
					   ipacct[1][set], &ipacct[1][set]);
		}
		if (flags & FR_INQUE) {
			(void) frflushlist(set, unit, &flushed,
					   ipfilter[0][set],
					   &ipfilter[0][set]);
			(void) frflushlist(set, unit, &flushed,
					   ipacct[0][set], &ipacct[0][set]);
		}
	}

	*(int *)data = flushed;
}
