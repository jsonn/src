/*	$NetBSD: ip_rcmd_pxy.c,v 1.1.1.1.4.1 1999/12/27 18:36:18 wrstuden Exp $	*/

/*
 * Simple RCMD transparent proxy for in-kernel use.  For use with the NAT
 * code.
 */
#if SOLARIS && defined(_KERNEL)
extern	kmutex_t	ipf_rw;
#endif

#define	isdigit(x)	((x) >= '0' && (x) <= '9')

#define	IPF_RCMD_PROXY


int ippr_rcmd_init __P((void));
int ippr_rcmd_new __P((fr_info_t *, ip_t *, ap_session_t *, nat_t *));
int ippr_rcmd_out __P((fr_info_t *, ip_t *, ap_session_t *, nat_t *));
u_short ipf_rcmd_atoi __P((char *));
int ippr_rcmd_portmsg __P((fr_info_t *, ip_t *, ap_session_t *, nat_t *));

static	frentry_t	rcmdfr;


/*
 * RCMD application proxy initialization.
 */
int ippr_rcmd_init()
{
	bzero((char *)&rcmdfr, sizeof(rcmdfr));
	rcmdfr.fr_ref = 1;
	rcmdfr.fr_flags = FR_INQUE|FR_PASS|FR_QUICK|FR_KEEPSTATE;
	return 0;
}


/*
 * Setup for a new RCMD proxy.
 */
int ippr_rcmd_new(fin, ip, aps, nat)
fr_info_t *fin;
ip_t *ip;
ap_session_t *aps;
nat_t *nat;
{
	tcphdr_t *tcp = (tcphdr_t *)fin->fin_dp;

	aps->aps_psiz = sizeof(u_32_t);
	KMALLOCS(aps->aps_data, u_32_t *, sizeof(u_32_t));
	if (aps->aps_data == NULL)
		return -1;
	*(u_32_t *)aps->aps_data = 0;
	aps->aps_sport = tcp->th_sport;
	aps->aps_dport = tcp->th_dport;
	return 0;
}


/*
 * ipf_rcmd_atoi - implement a simple version of atoi
 */
u_short ipf_rcmd_atoi(ptr)
char *ptr;
{
	register char *s = ptr, c;
	register u_short i = 0;

	while ((c = *s++) && isdigit(c)) {
		i *= 10;
		i += c - '0';
	}
	return i;
}


int ippr_rcmd_portmsg(fin, ip, aps, nat)
fr_info_t *fin;
ip_t *ip;
ap_session_t *aps;
nat_t *nat;
{
	char portbuf[8], *s;
	struct in_addr swip;
	u_short sp, dp;
	int off, dlen;
	tcphdr_t *tcp, tcph, *tcp2 = &tcph;
	fr_info_t fi;
	nat_t *ipn;
	mb_t *m;
#if	SOLARIS
	mb_t *m1;
#endif

	tcp = (tcphdr_t *)fin->fin_dp;
	off = (ip->ip_hl << 2) + (tcp->th_off << 2);
	m = *(mb_t **)fin->fin_mp;

#if	SOLARIS
	m = fin->fin_qfm;

	dlen = msgdsize(m) - off;
	bzero(portbuf, sizeof(portbuf));
	copyout_mblk(m, off, MIN(sizeof(portbuf), dlen), portbuf);
#else
	dlen = mbufchainlen(m) - off;
	bzero(portbuf, sizeof(portbuf));
	m_copydata(m, off, MIN(sizeof(portbuf), dlen), portbuf);
#endif
	if ((*(u_32_t *)aps->aps_data != 0) &&
	    (tcp->th_seq != *(u_32_t *)aps->aps_data))
		return 0;

	portbuf[sizeof(portbuf) - 1] = '\0';
	s = portbuf;
	sp = ipf_rcmd_atoi(s);
	if (!sp)
		return 0;

	/*
	 * Add skeleton NAT entry for connection which will come back the
	 * other way.
	 */
	sp = htons(sp);
	dp = htons(fin->fin_data[1]);
	ipn = nat_outlookup(fin->fin_ifp, IPN_TCP, nat->nat_p, nat->nat_inip,
			    ip->ip_dst, (dp << 16) | sp);
	if (ipn == NULL) {
		bcopy((char *)fin, (char *)&fi, sizeof(fi));
		bzero((char *)tcp2, sizeof(*tcp2));
		tcp2->th_win = htons(8192);
		tcp2->th_sport = sp;
		tcp2->th_dport = 0; /* XXX - don't specify remote port */
		fi.fin_data[0] = ntohs(sp);
		fi.fin_data[1] = 0;
		fi.fin_dp = (char *)tcp2;
		swip = ip->ip_src;
		ip->ip_src = nat->nat_inip;
		ipn = nat_new(nat->nat_ptr, ip, &fi, IPN_TCP|FI_W_DPORT,
			      NAT_OUTBOUND);
		if (ipn != NULL) {
			ipn->nat_age = fr_defnatage;
			fi.fin_fr = &rcmdfr;
			(void) fr_addstate(ip, &fi, FI_W_DPORT);
		}
		ip->ip_src = swip;
	}
	return 0;
}


int ippr_rcmd_out(fin, ip, aps, nat)
fr_info_t *fin;
ip_t *ip;
ap_session_t *aps;
nat_t *nat;
{
	return ippr_rcmd_portmsg(fin, ip, aps, nat);
}
