/*	$NetBSD: ip_proxy.c,v 1.17.2.2 1999/12/20 21:07:31 he Exp $	*/

/*
 * Copyright (C) 1997-1998 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#if !defined(lint)
#if defined(__NetBSD__)
static const char rcsid[] = "$NetBSD: ip_proxy.c,v 1.17.2.2 1999/12/20 21:07:31 he Exp $";
#else
static const char rcsid[] = "@(#)Id: ip_proxy.c,v 2.2.2.1 1999/09/19 12:18:19 darrenr Exp ";
#endif
#endif

#if defined(__FreeBSD__) && defined(KERNEL) && !defined(_KERNEL)
# define	_KERNEL
#endif

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/file.h>
#if !defined(__FreeBSD_version)  
# include <sys/ioctl.h>      
#endif
#include <sys/fcntl.h>
#include <sys/uio.h>
#if !defined(_KERNEL) && !defined(KERNEL)
# include <stdio.h>
# include <string.h>
# include <stdlib.h>
#endif
#ifndef	linux
# include <sys/protosw.h>
#endif
#include <sys/socket.h>
#if defined(_KERNEL)
# if !defined(linux)
#  include <sys/systm.h>
# else
#  include <linux/string.h>
# endif
#endif
#if !defined(__SVR4) && !defined(__svr4__)
# ifndef linux
#  include <sys/mbuf.h>
# endif
#else
# include <sys/byteorder.h>
# ifdef _KERNEL
#  include <sys/dditypes.h>
# endif
# include <sys/stream.h>
# include <sys/kmem.h>
#endif
#if __FreeBSD__ > 2
# include <sys/queue.h>
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
#include "netinet/ip_state.h"
#if (__FreeBSD_version >= 300000)
# include <sys/malloc.h>
#endif


#ifndef MIN
#define MIN(a,b)        (((a)<(b))?(a):(b))
#endif

static ap_session_t *appr_new_session __P((aproxy_t *, ip_t *,
					   fr_info_t *, nat_t *));
static int appr_fixseqack __P((fr_info_t *, ip_t *, ap_session_t *, int ));


#define	AP_SESS_SIZE	53

#if defined(_KERNEL) && !defined(linux)
#include "netinet/ip_ftp_pxy.c"
#include "netinet/ip_rcmd_pxy.c"
#include "netinet/ip_raudio_pxy.c"
#endif

ap_session_t	*ap_sess_tab[AP_SESS_SIZE];
ap_session_t	*ap_sess_list = NULL;
aproxy_t	ap_proxies[] = {
#ifdef	IPF_FTP_PROXY
	{ "ftp", (char)IPPROTO_TCP, 0, 0, ippr_ftp_init, NULL,
	  ippr_ftp_in, ippr_ftp_out },
#endif
#ifdef	IPF_RCMD_PROXY
	{ "rcmd", (char)IPPROTO_TCP, 0, 0, ippr_rcmd_init, ippr_rcmd_new,
	  NULL, ippr_rcmd_out },
#endif
#ifdef	IPF_RAUDIO_PROXY
	{ "raudio", (char)IPPROTO_TCP, 0, 0, ippr_raudio_init,
	  ippr_raudio_new, ippr_raudio_in, ippr_raudio_out },
#endif
	{ "", '\0', 0, 0, NULL, NULL }
};


int appr_ok(ip, tcp, nat)
ip_t *ip;
tcphdr_t *tcp;
ipnat_t *nat;
{
	aproxy_t *apr = nat->in_apr;
	u_short dport = nat->in_dport;

	if (!apr || (apr->apr_flags & APR_DELETE) ||
	    (ip->ip_p != apr->apr_p))
		return 0;
	if ((tcp && (tcp->th_dport != dport)) || (!tcp && dport))
		return 0;
	return 1;
}


/*
 * Allocate a new application proxy structure and fill it in with the
 * relevant details.  call the init function once complete, prior to
 * returning.
 */
static ap_session_t *appr_new_session(apr, ip, fin, nat)
aproxy_t *apr;
ip_t *ip;
fr_info_t *fin;
nat_t *nat;
{
	register ap_session_t *aps;

	if (!apr || (apr->apr_flags & APR_DELETE) || (ip->ip_p != apr->apr_p))
		return NULL;

	KMALLOC(aps, ap_session_t *);
	if (!aps)
		return NULL;
	bzero((char *)aps, sizeof(*aps));
	aps->aps_next = ap_sess_list;
	aps->aps_p = ip->ip_p;
	aps->aps_data = NULL;
	aps->aps_apr = apr;
	aps->aps_psiz = 0;
	ap_sess_list = aps;
	aps->aps_nat = nat;
	nat->nat_aps = aps;
	if (apr->apr_new != NULL)
		(void) (*apr->apr_new)(fin, ip, aps, nat);
	return aps;
}


/*
 * check to see if a packet should be passed through an active proxy routine
 * if one has been setup for it.
 */
int appr_check(ip, fin, nat)
ip_t *ip;
fr_info_t *fin;
nat_t *nat;
{
	ap_session_t *aps;
	aproxy_t *apr;
	tcphdr_t *tcp = NULL;
	u_32_t sum;
	int err;

	if (nat->nat_aps == NULL)
		nat->nat_aps = appr_new_session(nat->nat_ptr->in_apr, ip,
						fin, nat);
	aps = nat->nat_aps;
	if ((aps != NULL) && (aps->aps_p == ip->ip_p)) {
		if (ip->ip_p == IPPROTO_TCP) {
			tcp = (tcphdr_t *)fin->fin_dp;
			/*
			 * verify that the checksum is correct.  If not, then
			 * don't do anything with this packet.
			 */
#if SOLARIS && defined(_KERNEL)
			sum = fr_tcpsum(fin->fin_qfm, ip, tcp);
#else
			sum = fr_tcpsum(*(mb_t **)fin->fin_mp, ip, tcp);
#endif
			if (sum != tcp->th_sum) {
				frstats[fin->fin_out].fr_tcpbad++;
				return -1;
			}
		}

		apr = aps->aps_apr;
		err = 0;
		if (fin->fin_out != 0) {
			if (apr->apr_outpkt != NULL)
				err = (*apr->apr_outpkt)(fin, ip, aps, nat);
		} else {
			if (apr->apr_inpkt != NULL)
				err = (*apr->apr_inpkt)(fin, ip, aps, nat);
		}

		if (tcp != NULL) {
			err = appr_fixseqack(fin, ip, aps, err);
#if SOLARIS && defined(_KERNEL)
			tcp->th_sum = fr_tcpsum(fin->fin_qfm, ip, tcp);
#else
			tcp->th_sum = fr_tcpsum(*(mb_t **)fin->fin_mp, ip, tcp);
#endif
		}
		aps->aps_bytes += ip->ip_len;
		aps->aps_pkts++;
		return 2;
	}
	return -1;
}


aproxy_t *appr_match(pr, name)
u_int pr;
char *name;
{
	aproxy_t *ap;

	for (ap = ap_proxies; ap->apr_p; ap++)
		if ((ap->apr_p == pr) &&
		    !strncmp(name, ap->apr_label, sizeof(ap->apr_label))) {
			ap->apr_ref++;
			return ap;
		}
	return NULL;
}


void appr_free(ap)
aproxy_t *ap;
{
	ap->apr_ref--;
}


void aps_free(aps)
ap_session_t *aps;
{
	ap_session_t *a, **ap;

	if (!aps)
		return;

	for (ap = &ap_sess_list; (a = *ap); ap = &a->aps_next)
		if (a == aps) {
			*ap = a->aps_next;
			break;
		}

	if (a) {
		if ((aps->aps_data != NULL) && (aps->aps_psiz != 0))
			KFREES(aps->aps_data, aps->aps_psiz);
		KFREE(aps);
	}
}


static int appr_fixseqack(fin, ip, aps, inc)
fr_info_t *fin;
ip_t *ip;
ap_session_t *aps;
int inc;
{
	int sel, ch = 0, out, nlen;
	u_32_t seq1, seq2;
	tcphdr_t *tcp;

	tcp = (tcphdr_t *)fin->fin_dp;
	out = fin->fin_out;
	nlen = ip->ip_len;
	nlen -= (ip->ip_hl << 2) + (tcp->th_off << 2);

	if (out != 0) {
		seq1 = (u_32_t)ntohl(tcp->th_seq);
		sel = aps->aps_sel[out];

		/* switch to other set ? */
		if ((aps->aps_seqmin[!sel] > aps->aps_seqmin[sel]) &&
		    (seq1 > aps->aps_seqmin[!sel]))
			sel = aps->aps_sel[out] = !sel;

		if (aps->aps_seqoff[sel]) {
			seq2 = aps->aps_seqmin[sel] - aps->aps_seqoff[sel];
			if (seq1 > seq2) {
				seq2 = aps->aps_seqoff[sel];
				seq1 += seq2;
				tcp->th_seq = htonl(seq1);
				ch = 1;
			}
		}

		if (inc && (seq1 > aps->aps_seqmin[!sel])) {
			aps->aps_seqmin[!sel] = seq1 + nlen - 1;
			aps->aps_seqoff[!sel] = aps->aps_seqoff[sel] + inc;
		}

		/***/

		seq1 = ntohl(tcp->th_ack);
		sel = aps->aps_sel[1 - out];

		/* switch to other set ? */
		if ((aps->aps_ackmin[!sel] > aps->aps_ackmin[sel]) &&
		    (seq1 > aps->aps_ackmin[!sel]))
			sel = aps->aps_sel[1 - out] = !sel;

		if (aps->aps_ackoff[sel] && (seq1 > aps->aps_ackmin[sel])) {
			seq2 = aps->aps_ackoff[sel];
			tcp->th_ack = htonl(seq1 - seq2);
			ch = 1;
		}
	} else {
		seq1 = ntohl(tcp->th_seq);
		sel = aps->aps_sel[out];

		/* switch to other set ? */
		if ((aps->aps_ackmin[!sel] > aps->aps_ackmin[sel]) &&
		    (seq1 > aps->aps_ackmin[!sel]))
			sel = aps->aps_sel[out] = !sel;

		if (aps->aps_ackoff[sel]) {
			seq2 = aps->aps_ackmin[sel] -
			       aps->aps_ackoff[sel];
			if (seq1 > seq2) {
				seq2 = aps->aps_ackoff[sel];
				seq1 += seq2;
				tcp->th_seq = htonl(seq1);
				ch = 1;
			}
		}

		if (inc && (seq1 > aps->aps_ackmin[!sel])) {
			aps->aps_ackmin[!sel] = seq1 + nlen - 1;
			aps->aps_ackoff[!sel] = aps->aps_ackoff[sel] + inc;
		}

		/***/

		seq1 = ntohl(tcp->th_ack);
		sel = aps->aps_sel[1 - out];

		/* switch to other set ? */
		if ((aps->aps_seqmin[!sel] > aps->aps_seqmin[sel]) &&
		    (seq1 > aps->aps_seqmin[!sel]))
			sel = aps->aps_sel[1 - out] = !sel;

		if (aps->aps_seqoff[sel] && (seq1 > aps->aps_seqmin[sel])) {
			seq2 = aps->aps_seqoff[sel];
			tcp->th_ack = htonl(seq1 - seq2);
			ch = 1;
		}
	}
	return ch ? 2 : 0;
}


int appr_init()
{
	aproxy_t *ap;
	int err = 0;

	for (ap = ap_proxies; ap->apr_p; ap++) {
		err = (*ap->apr_init)();
		if (err != 0)
			break;
	}
	return err;
}
