/*	$NetBSD: ip_ftp_pxy.c,v 1.16.4.3 2002/02/09 16:58:52 he Exp $	*/

#include <sys/cdefs.h>
__KERNEL_RCSID(1, "$NetBSD: ip_ftp_pxy.c,v 1.16.4.3 2002/02/09 16:58:52 he Exp $");

/*
 * Simple FTP transparent proxy for in-kernel use.  For use with the NAT
 * code.
 *
 * Id: ip_ftp_pxy.c,v 2.7.2.31 2002/01/09 09:30:04 darrenr Exp
 */
#if SOLARIS && defined(_KERNEL)
extern	kmutex_t	ipf_rw;
#endif

#define	isdigit(x)	((x) >= '0' && (x) <= '9')
#define	isupper(x)	(((unsigned)(x) >= 'A') && ((unsigned)(x) <= 'Z'))
#define	islower(x)	(((unsigned)(x) >= 'a') && ((unsigned)(x) <= 'z'))
#define	isalpha(x)	(isupper(x) || islower(x))
#define	toupper(x)	(isupper(x) ? (x) : (x) - 'a' + 'A')

#define	IPF_FTP_PROXY

#define	IPF_MINPORTLEN	18
#define	IPF_MAXPORTLEN	30
#define	IPF_MIN227LEN	39
#define	IPF_MAX227LEN	51
#define	IPF_FTPBUFSZ	96	/* This *MUST* be >= 53! */

#define	FTPXY_GO	0
#define	FTPXY_INIT	1
#define	FTPXY_USER_1	2
#define	FTPXY_USOK_1	3
#define	FTPXY_PASS_1	4
#define	FTPXY_PAOK_1	5
#define	FTPXY_AUTH_1	6
#define	FTPXY_AUOK_1	7
#define	FTPXY_ADAT_1	8
#define	FTPXY_ADOK_1	9
#define	FTPXY_ACCT_1	10
#define	FTPXY_ACOK_1	11
#define	FTPXY_USER_2	12
#define	FTPXY_USOK_2	13
#define	FTPXY_PASS_2	14
#define	FTPXY_PAOK_2	15

int ippr_ftp_client __P((fr_info_t *, ip_t *, nat_t *, ftpinfo_t *, int));
int ippr_ftp_complete __P((char *, size_t));
int ippr_ftp_in __P((fr_info_t *, ip_t *, ap_session_t *, nat_t *));
int ippr_ftp_init __P((void));
int ippr_ftp_new __P((fr_info_t *, ip_t *, ap_session_t *, nat_t *));
int ippr_ftp_out __P((fr_info_t *, ip_t *, ap_session_t *, nat_t *));
int ippr_ftp_pasv __P((fr_info_t *, ip_t *, nat_t *, ftpside_t *, int));
int ippr_ftp_port __P((fr_info_t *, ip_t *, nat_t *, ftpside_t *, int));
int ippr_ftp_process __P((fr_info_t *, ip_t *, nat_t *, ftpinfo_t *, int));
int ippr_ftp_server __P((fr_info_t *, ip_t *, nat_t *, ftpinfo_t *, int));
int ippr_ftp_valid __P((int, char *, size_t));
int ippr_ftp_server_valid __P((char *, size_t));
int ippr_ftp_client_valid __P((char *, size_t));
u_short ippr_ftp_atoi __P((char **));

static	frentry_t	ftppxyfr;
int	ippr_ftp_pasvonly = 0;
int	ippr_ftp_insecure = 0;


/*
 * Initialize local structures.
 */
int ippr_ftp_init()
{
	bzero((char *)&ftppxyfr, sizeof(ftppxyfr));
	ftppxyfr.fr_ref = 1;
	ftppxyfr.fr_flags = FR_INQUE|FR_PASS|FR_QUICK|FR_KEEPSTATE;
	return 0;
}


int ippr_ftp_new(fin, ip, aps, nat)
fr_info_t *fin;
ip_t *ip;
ap_session_t *aps;
nat_t *nat;
{
	ftpinfo_t *ftp;
	ftpside_t *f;

	KMALLOC(ftp, ftpinfo_t *);
	if (ftp == NULL)
		return -1;
	aps->aps_data = ftp;
	aps->aps_psiz = sizeof(ftpinfo_t);

	bzero((char *)ftp, sizeof(*ftp));
	f = &ftp->ftp_side[0];
	f->ftps_rptr = f->ftps_buf;
	f->ftps_wptr = f->ftps_buf;
	f = &ftp->ftp_side[1];
	f->ftps_rptr = f->ftps_buf;
	f->ftps_wptr = f->ftps_buf;
	ftp->ftp_passok = FTPXY_INIT;
	return 0;
}


int ippr_ftp_port(fin, ip, nat, f, dlen)
fr_info_t *fin;
ip_t *ip;
nat_t *nat;
ftpside_t *f;
int dlen;
{
	tcphdr_t *tcp, tcph, *tcp2 = &tcph;
	char newbuf[IPF_FTPBUFSZ], *s;
	u_int a1, a2, a3, a4;
	struct in_addr swip;
	u_short a5, a6, sp;
	size_t nlen, olen;
	fr_info_t fi;
	int inc, off;
	nat_t *ipn;
	mb_t *m;
#if	SOLARIS
	mb_t *m1;
#endif

	tcp = (tcphdr_t *)fin->fin_dp;
	/*
	 * Check for client sending out PORT message.
	 */
	if (dlen < IPF_MINPORTLEN)
		return 0;
	off = fin->fin_hlen + (tcp->th_off << 2);
	/*
	 * Skip the PORT command + space
	 */
	s = f->ftps_rptr + 5;
	/*
	 * Pick out the address components, two at a time.
	 */
	a1 = ippr_ftp_atoi(&s);
	if (!s)
		return 0;
	a2 = ippr_ftp_atoi(&s);
	if (!s)
		return 0;
	/*
	 * check that IP address in the PORT/PASV reply is the same as the
	 * sender of the command - prevents using PORT for port scanning.
	 */
	a1 <<= 16;
	a1 |= a2;
	if (a1 != ntohl(nat->nat_inip.s_addr))
		return 0;

	a5 = ippr_ftp_atoi(&s);
	if (!s)
		return 0;
	if (*s == ')')
		s++;

	/*
	 * check for CR-LF at the end.
	 */
	if (*s == '\n')
		s--;
	if ((*s == '\r') && (*(s + 1) == '\n')) {
		s += 2;
		a6 = a5 & 0xff;
	} else
		return 0;
	a5 >>= 8;
	a5 &= 0xff;
	/*
	 * Calculate new address parts for PORT command
	 */
	a1 = ntohl(ip->ip_src.s_addr);
	a2 = (a1 >> 16) & 0xff;
	a3 = (a1 >> 8) & 0xff;
	a4 = a1 & 0xff;
	a1 >>= 24;
	olen = s - f->ftps_rptr;
	/* DO NOT change this to snprintf! */
	(void) sprintf(newbuf, "%s %u,%u,%u,%u,%u,%u\r\n",
		       "PORT", a1, a2, a3, a4, a5, a6);

	nlen = strlen(newbuf);
	inc = nlen - olen;
	if ((inc + ip->ip_len) > 65535)
		return 0;

#if SOLARIS
	m = fin->fin_qfm;
	for (m1 = m; m1->b_cont; m1 = m1->b_cont)
		;
	if ((inc > 0) && (m1->b_datap->db_lim - m1->b_wptr < inc)) {
		mblk_t *nm;

		/* alloc enough to keep same trailer space for lower driver */
		nm = allocb(nlen, BPRI_MED);
		PANIC((!nm),("ippr_ftp_out: allocb failed"));

		nm->b_band = m1->b_band;
		nm->b_wptr += nlen;

		m1->b_wptr -= olen;
		PANIC((m1->b_wptr < m1->b_rptr),
		      ("ippr_ftp_out: cannot handle fragmented data block"));

		linkb(m1, nm);
	} else {
		if (m1->b_datap->db_struiolim == m1->b_wptr)
			m1->b_datap->db_struiolim += inc;
		m1->b_datap->db_struioflag &= ~STRUIO_IP;
		m1->b_wptr += inc;
	}
	copyin_mblk(m, off, nlen, newbuf);
#else
	m = *((mb_t **)fin->fin_mp);
	if (inc < 0)
		m_adj(m, inc);
	/* the mbuf chain will be extended if necessary by m_copyback() */
	m_copyback(m, off, nlen, newbuf);
# ifdef	M_PKTHDR
	if (!(m->m_flags & M_PKTHDR))
		m->m_pkthdr.len += inc;
# endif
#endif
	if (inc != 0) {
#if SOLARIS || defined(__sgi)
		register u_32_t	sum1, sum2;

		sum1 = ip->ip_len;
		sum2 = ip->ip_len + inc;

		/* Because ~1 == -2, We really need ~1 == -1 */
		if (sum1 > sum2)
			sum2--;
		sum2 -= sum1;
		sum2 = (sum2 & 0xffff) + (sum2 >> 16);

		fix_outcksum(fin, &ip->ip_sum, sum2);
#endif
		ip->ip_len += inc;
	}

	/*
	 * Add skeleton NAT entry for connection which will come back the
	 * other way.
	 */
	sp = (a5 << 8 | a6);
	/*
	 * Don't allow the PORT command to specify a port < 1024 due to
	 * security crap.
	 */
	if (sp < 1024)
		return 0;
	/*
	 * The server may not make the connection back from port 20, but
	 * it is the most likely so use it here to check for a conflicting
	 * mapping.
	 */
	bcopy((char *)fin, (char *)&fi, sizeof(fi));
	fi.fin_data[0] = sp;
	fi.fin_data[1] = fin->fin_data[1] - 1;
	ipn = nat_outlookup(&fi, IPN_TCP, nat->nat_p, nat->nat_inip,
			    ip->ip_dst, 0);
	if (ipn == NULL) {
		int slen;

		slen = ip->ip_len;
		ip->ip_len = fin->fin_hlen + sizeof(*tcp2);
		bzero((char *)tcp2, sizeof(*tcp2));
		tcp2->th_win = htons(8192);
		tcp2->th_sport = htons(sp);
		tcp2->th_off = 5;
		tcp2->th_dport = 0; /* XXX - don't specify remote port */
		fi.fin_data[1] = 0;
		fi.fin_dlen = sizeof(*tcp2);
		fi.fin_dp = (char *)tcp2;
		fi.fin_fr = &ftppxyfr;
		fi.fin_out = 1;
		swip = ip->ip_src;
		fi.fin_fi.fi_saddr = nat->nat_inip.s_addr;
		ip->ip_src = nat->nat_inip;
		ipn = nat_new(&fi, ip, nat->nat_ptr, NULL, IPN_TCP|FI_W_DPORT,
			      NAT_OUTBOUND);
		if (ipn != NULL) {
			ipn->nat_age = fr_defnatage;
			(void) fr_addstate(ip, &fi, NULL,
					   FI_W_DPORT|FI_IGNOREPKT);
		}
		ip->ip_len = slen;
		ip->ip_src = swip;
	}
	return APR_INC(inc);
}


int ippr_ftp_client(fin, ip, nat, ftp, dlen)
fr_info_t *fin;
nat_t *nat;
ftpinfo_t *ftp;
ip_t *ip;
int dlen;
{
	char *rptr, *wptr, cmd[6], c;
	ftpside_t *f;
	int inc, i;

	inc = 0;
	f = &ftp->ftp_side[0];
	rptr = f->ftps_rptr;
	wptr = f->ftps_wptr;

	for (i = 0; (i < 5) && (i < dlen); i++) {
		c = rptr[i];
		if (isalpha(c)) {
			cmd[i] = toupper(c);
		} else {
			cmd[i] = c;
		}
	}
	cmd[i] = '\0';

	ftp->ftp_incok = 0;
	if (!strncmp(cmd, "USER ", 5) || !strncmp(cmd, "XAUT ", 5)) {
		if (ftp->ftp_passok == FTPXY_ADOK_1 ||
		    ftp->ftp_passok == FTPXY_AUOK_1) {
			ftp->ftp_passok = FTPXY_USER_2;
			ftp->ftp_incok = 1;
		} else {
			ftp->ftp_passok = FTPXY_USER_1;
			ftp->ftp_incok = 1;
		}
	} else if (!strncmp(cmd, "AUTH ", 5)) {
		ftp->ftp_passok = FTPXY_AUTH_1;
		ftp->ftp_incok = 1;
	} else if (!strncmp(cmd, "PASS ", 5)) {
		if (ftp->ftp_passok == FTPXY_USOK_1) {
			ftp->ftp_passok = FTPXY_PASS_1;
			ftp->ftp_incok = 1;
		} else if (ftp->ftp_passok == FTPXY_USOK_2) {
			ftp->ftp_passok = FTPXY_PASS_2;
			ftp->ftp_incok = 1;
		}
	} else if ((ftp->ftp_passok == FTPXY_AUOK_1) &&
		   !strncmp(cmd, "ADAT ", 5)) {
		ftp->ftp_passok = FTPXY_ADAT_1;
		ftp->ftp_incok = 1;
	} else if ((ftp->ftp_passok == FTPXY_PAOK_2) &&
		 !strncmp(cmd, "ACCT ", 5)) {
		ftp->ftp_passok = FTPXY_ACCT_1;
		ftp->ftp_incok = 1;
	} else if ((ftp->ftp_passok == FTPXY_GO) && !ippr_ftp_pasvonly &&
		 !strncmp(cmd, "PORT ", 5)) {
		inc = ippr_ftp_port(fin, ip, nat, f, dlen);
	} else if (ippr_ftp_insecure && !ippr_ftp_pasvonly &&
		   !strncmp(cmd, "PORT ", 5)) {
		inc = ippr_ftp_port(fin, ip, nat, f, dlen);
	}

	while ((*rptr++ != '\n') && (rptr < wptr))
		;
	f->ftps_rptr = rptr;
	return inc;
}


int ippr_ftp_pasv(fin, ip, nat, f, dlen)
fr_info_t *fin;
ip_t *ip;
nat_t *nat;
ftpside_t *f;
int dlen;
{
	tcphdr_t *tcp, tcph, *tcp2 = &tcph;
	struct in_addr swip, swip2;
	u_short a5, a6, sp, dp;
	u_int a1, a2, a3, a4;
	fr_info_t fi;
	nat_t *ipn;
	int inc;
	char *s;

#define	PASV_REPLEN	24
	/*
	 * Check for PASV reply message.
	 */
	if (dlen < IPF_MIN227LEN)
		return 0;
	else if (strncmp(f->ftps_rptr, "227 Entering Passive Mod", PASV_REPLEN))
		return 0;

	tcp = (tcphdr_t *)fin->fin_dp;

	/*
	 * Skip the PORT command + space
	 */
	s = f->ftps_rptr + PASV_REPLEN;
	while (*s && !isdigit(*s))
		s++;
	/*
	 * Pick out the address components, two at a time.
	 */
	a1 = ippr_ftp_atoi(&s);
	if (!s)
		return 0;
	a2 = ippr_ftp_atoi(&s);
	if (!s)
		return 0;

	/*
	 * check that IP address in the PORT/PASV reply is the same as the
	 * sender of the command - prevents using PORT for port scanning.
	 */
	a1 <<= 16;
	a1 |= a2;
	if (a1 != ntohl(nat->nat_oip.s_addr))
		return 0;

	a5 = ippr_ftp_atoi(&s);
	if (!s)
		return 0;

	if (*s == ')')
		s++;
	if (*s == '.')
		s++;
	if (*s == '\n')
		s--;
	/*
	 * check for CR-LF at the end.
	 */
	if ((*s == '\r') && (*(s + 1) == '\n')) {
		s += 2;
		a6 = a5 & 0xff;
	} else
		return 0;
	a5 >>= 8;
	/*
	 * Calculate new address parts for 227 reply
	 */
	a1 = ntohl(ip->ip_src.s_addr);
	a2 = (a1 >> 16) & 0xff;
	a3 = (a1 >> 8) & 0xff;
	a4 = a1 & 0xff;
	a1 >>= 24;
	inc = 0;
#if 0
	olen = s - f->ftps_rptr;
	(void) sprintf(newbuf, "%s %u,%u,%u,%u,%u,%u\r\n",
		       "227 Entering Passive Mode", a1, a2, a3, a4, a5, a6);
	nlen = strlen(newbuf);
	inc = nlen - olen;
	if ((inc + ip->ip_len) > 65535)
		return 0;

#if SOLARIS
	m = fin->fin_qfm;
	for (m1 = m; m1->b_cont; m1 = m1->b_cont)
		;
	if ((inc > 0) && (m1->b_datap->db_lim - m1->b_wptr < inc)) {
		mblk_t *nm;

		/* alloc enough to keep same trailer space for lower driver */
		nm = allocb(nlen, BPRI_MED);
		PANIC((!nm),("ippr_ftp_out: allocb failed"));

		nm->b_band = m1->b_band;
		nm->b_wptr += nlen;

		m1->b_wptr -= olen;
		PANIC((m1->b_wptr < m1->b_rptr),
		      ("ippr_ftp_out: cannot handle fragmented data block"));

		linkb(m1, nm);
	} else {
		m1->b_wptr += inc;
	}
	/*copyin_mblk(m, off, nlen, newbuf);*/
#else /* SOLARIS */
	m = *((mb_t **)fin->fin_mp);
	if (inc < 0)
		m_adj(m, inc);
	/* the mbuf chain will be extended if necessary by m_copyback() */
	/*m_copyback(m, off, nlen, newbuf);*/
#endif /* SOLARIS */
	if (inc != 0) {
#if SOLARIS || defined(__sgi)
		register u_32_t	sum1, sum2;

		sum1 = ip->ip_len;
		sum2 = ip->ip_len + inc;

		/* Because ~1 == -2, We really need ~1 == -1 */
		if (sum1 > sum2)
			sum2--;
		sum2 -= sum1;
		sum2 = (sum2 & 0xffff) + (sum2 >> 16);

		fix_outcksum(fin, &ip->ip_sum, sum2);
#endif /* SOLARIS || defined(__sgi) */
		ip->ip_len += inc;
	}
#endif /* 0 */

	/*
	 * Add skeleton NAT entry for connection which will come back the
	 * other way.
	 */
	bcopy((char *)fin, (char *)&fi, sizeof(fi));
	sp = 0;
	fi.fin_data[0] = 0;
	dp = htons(fin->fin_data[1] - 1);
	fi.fin_data[1] = ntohs(dp);
	ipn = nat_outlookup(&fi, IPN_TCP, nat->nat_p, nat->nat_inip,
			    ip->ip_dst, 0);
	if (ipn == NULL) {
		int slen;

		slen = ip->ip_len;
		ip->ip_len = fin->fin_hlen + sizeof(*tcp2);
		bzero((char *)tcp2, sizeof(*tcp2));
		tcp2->th_win = htons(8192);
		tcp2->th_sport = 0;		/* XXX - fake it for nat_new */
		tcp2->th_off = 5;
		fi.fin_data[0] = a5 << 8 | a6;
		fi.fin_dlen = sizeof(*tcp2);
		tcp2->th_dport = htons(fi.fin_data[0]);
		fi.fin_data[1] = 0;
		fi.fin_dp = (char *)tcp2;
		fi.fin_fr = &ftppxyfr;
		fi.fin_out = 1;
		swip = ip->ip_src;
		swip2 = ip->ip_dst;
		fi.fin_fi.fi_daddr = ip->ip_src.s_addr;
		fi.fin_fi.fi_saddr = nat->nat_inip.s_addr;
		ip->ip_dst = ip->ip_src;
		ip->ip_src = nat->nat_inip;
		ipn = nat_new(&fi, ip, nat->nat_ptr, NULL, IPN_TCP|FI_W_SPORT,
			      NAT_OUTBOUND);
		if (ipn != NULL) {
			ipn->nat_age = fr_defnatage;
			(void) fr_addstate(ip, &fi, NULL,
					   FI_W_SPORT|FI_IGNOREPKT);
		}
		ip->ip_len = slen;
		ip->ip_src = swip;
		ip->ip_dst = swip2;
	}
	return inc;
}


int ippr_ftp_server(fin, ip, nat, ftp, dlen)
fr_info_t *fin;
ip_t *ip;
nat_t *nat;
ftpinfo_t *ftp;
int dlen;
{
	char *rptr, *wptr;
	ftpside_t *f;
	int inc;

	inc = 0;
	f = &ftp->ftp_side[1];
	rptr = f->ftps_rptr;
	wptr = f->ftps_wptr;

	if (!isdigit(*rptr) || !isdigit(*(rptr + 1)) || !isdigit(*(rptr + 2)))
		return inc;
	if (ftp->ftp_passok == FTPXY_GO) {
		if (!strncmp(rptr, "227 ", 4))
			inc = ippr_ftp_pasv(fin, ip, nat, f, dlen);
	} else if (ippr_ftp_insecure && !strncmp(rptr, "227 ", 4)) {
		inc = ippr_ftp_pasv(fin, ip, nat, f, dlen);
	} else if (*rptr == '5' || *rptr == '4')
		ftp->ftp_passok = FTPXY_INIT;
	else if (ftp->ftp_incok) {
		if (*rptr == '3') {
			if (ftp->ftp_passok == FTPXY_ACCT_1)
				ftp->ftp_passok = FTPXY_GO;
			else
				ftp->ftp_passok++;
		} else if (*rptr == '2') {
			switch (ftp->ftp_passok)
			{
			case FTPXY_USER_1 :
			case FTPXY_USER_2 :
			case FTPXY_PASS_1 :
			case FTPXY_PASS_2 :
			case FTPXY_ACCT_1 :
				ftp->ftp_passok = FTPXY_GO;
				break;
			default :
				ftp->ftp_passok += 3;
				break;
			}
		}
	}
	ftp->ftp_incok = 0;
	while ((*rptr++ != '\n') && (rptr < wptr))
		;
	f->ftps_rptr = rptr;
	return inc;
}


/*
 * Look to see if the buffer starts with something which we recognise as
 * being the correct syntax for the FTP protocol.
 */
int ippr_ftp_client_valid(buf, len)
char *buf;
size_t len;
{
	register char *s, c;
	register size_t i = len;

	if (i < 5)
		return 2;
	s = buf;
	c = *s++;
	i--;

	if (isalpha(c)) {
		c = *s++;
		i--;
		if (isalpha(c)) {
			c = *s++;
			i--;
			if (isalpha(c)) {
				c = *s++;
				i--;
				if (isalpha(c)) {
					c = *s++;
					i--;
					if ((c != ' ') && (c != '\r'))
						return 1;
				} else if ((c != ' ') && (c != '\r'))
					return 1;
			} else
				return 1;
		} else
			return 1;
	} else
		return 1;
	for (; i; i--) {
		c = *s++;
		if (c == '\n')
			return 0;
	}
	return 2;
}


int ippr_ftp_server_valid(buf, len)
char *buf;
size_t len;
{
	register char *s, c;
	register size_t i = len;

	if (i < 5)
		return 2;
	s = buf;
	c = *s++;
	i--;

	if (isdigit(c)) {
		c = *s++;
		i--;
		if (isdigit(c)) {
			c = *s++;
			i--;
			if (isdigit(c)) {
				c = *s++;
				i--;
				if ((c != '-') && (c != ' '))
					return 1;
			} else
				return 1;
		} else
			return 1;
	} else
		return 1;
	for (; i; i--) {
		c = *s++;
		if (c == '\n')
			return 0;
	}
	return 2;
}


int ippr_ftp_valid(side, buf, len)
int side;
char *buf;
size_t len;
{
	int ret;

	if (side == 0)
		ret = ippr_ftp_client_valid(buf, len);
	else
		ret = ippr_ftp_server_valid(buf, len);
	return ret;
}


int ippr_ftp_process(fin, ip, nat, ftp, rv)
fr_info_t *fin;
ip_t *ip;
nat_t *nat;
ftpinfo_t *ftp;
int rv;
{
	int mlen, len, off, inc, i, sel;
	char *rptr, *wptr;
	ftpside_t *f, *t;
	tcphdr_t *tcp;
	mb_t *m;

	tcp = (tcphdr_t *)fin->fin_dp;
	off = fin->fin_hlen + (tcp->th_off << 2);

#if	SOLARIS
	m = fin->fin_qfm;
#else
	m = *((mb_t **)fin->fin_mp);
#endif

#if	SOLARIS
	mlen = msgdsize(m) - off;
#else
	mlen = mbufchainlen(m) - off;
#endif

	t = &ftp->ftp_side[1 - rv];
	f = &ftp->ftp_side[rv];
	if (!mlen) {
		if (!t->ftps_seq ||
		    (int)ntohl(tcp->th_ack) - (int)t->ftps_seq > 0)
			t->ftps_seq = ntohl(tcp->th_ack);
		f->ftps_len = 0;
		return 0;
	}

	inc = 0;
	rptr = f->ftps_rptr;
	wptr = f->ftps_wptr;

	sel = nat->nat_aps->aps_sel[1 - rv];
	if (rv)
		i = nat->nat_aps->aps_ackoff[sel];
	else
		i = nat->nat_aps->aps_seqoff[sel];
	/*
	 * XXX - Ideally, this packet should get dropped because we now know
	 * that it is out of order (and there is no real danger in doing so
	 * apart from causing packets to go through here ordered).
	 */
	if (f->ftps_len + f->ftps_seq == ntohl(tcp->th_seq))
		f->ftps_seq = ntohl(tcp->th_seq);
	else if (ntohl(tcp->th_seq) + i != f->ftps_seq) {
		return APR_ERR(1);
	}
	f->ftps_len = mlen;

	while (mlen > 0) {
		len = MIN(mlen, FTP_BUFSZ / 2);

#if	SOLARIS
		copyout_mblk(m, off, len, wptr);
#else
		m_copydata(m, off, len, wptr);
#endif
		mlen -= len;
		off += len;
		wptr += len;
		f->ftps_wptr = wptr;
		if (f->ftps_junk == 2)
			f->ftps_junk = ippr_ftp_valid(rv, rptr, wptr - rptr);

		while ((f->ftps_junk == 0) && (wptr > rptr)) {
			f->ftps_junk = ippr_ftp_valid(rv, rptr, wptr - rptr);
			if (f->ftps_junk == 0) {
				f->ftps_cmds++;
				len = wptr - rptr;
				f->ftps_rptr = rptr;
				if (rv)
					inc += ippr_ftp_server(fin, ip, nat,
							       ftp, len);
				else
					inc += ippr_ftp_client(fin, ip, nat,
							       ftp, len);
				rptr = f->ftps_rptr;
				wptr = f->ftps_wptr;
			}
		}

		/*
		 * Off to a bad start so lets just forget about using the
		 * ftp proxy for this connection.
		 */
		if ((f->ftps_cmds == 0) && (f->ftps_junk == 1))
			return APR_ERR(2);

		while ((f->ftps_junk == 1) && (rptr < wptr)) {
			while ((rptr < wptr) && (*rptr != '\r'))
				rptr++;

			if (*rptr == '\r') {
				if (rptr + 1 < wptr) {
					if (*(rptr + 1) == '\n') {
						rptr += 2;
						f->ftps_junk = 0;
					} else
						rptr++;
				} else
					break;
			}
		}
		f->ftps_rptr = rptr;

		if (rptr == wptr) {
			rptr = wptr = f->ftps_buf;
		} else {
			if ((wptr > f->ftps_buf + FTP_BUFSZ / 2)) {
				i = wptr - rptr;
				if ((rptr == f->ftps_buf) ||
				    (wptr - rptr > FTP_BUFSZ / 2)) {
					f->ftps_junk = 1;
					rptr = wptr = f->ftps_buf;
				} else {
					bcopy(rptr, f->ftps_buf, i);
					wptr = f->ftps_buf + i;
					rptr = f->ftps_buf;
				}
			}
			f->ftps_rptr = rptr;
			f->ftps_wptr = wptr;
		}
	}

	t->ftps_seq = ntohl(tcp->th_ack);
	f->ftps_rptr = rptr;
	f->ftps_wptr = wptr;
	return APR_INC(inc);
}


int ippr_ftp_out(fin, ip, aps, nat)
fr_info_t *fin;
ip_t *ip;
ap_session_t *aps;
nat_t *nat;
{
	ftpinfo_t *ftp;

	ftp = aps->aps_data;
	if (ftp == NULL)
		return 0;
	return ippr_ftp_process(fin, ip, nat, ftp, 0);
}


int ippr_ftp_in(fin, ip, aps, nat)
fr_info_t *fin;
ip_t *ip;
ap_session_t *aps;
nat_t *nat;
{
	ftpinfo_t *ftp;

	ftp = aps->aps_data;
	if (ftp == NULL)
		return 0;
	return ippr_ftp_process(fin, ip, nat, ftp, 1);
}


/*
 * ippr_ftp_atoi - implement a version of atoi which processes numbers in
 * pairs separated by commas (which are expected to be in the range 0 - 255),
 * returning a 16 bit number combining either side of the , as the MSB and
 * LSB.
 */
u_short ippr_ftp_atoi(ptr)
char **ptr;
{
	register char *s = *ptr, c;
	register u_char i = 0, j = 0;

	while ((c = *s++) && isdigit(c)) {
		i *= 10;
		i += c - '0';
	}
	if (c != ',') {
		*ptr = NULL;
		return 0;
	}
	while ((c = *s++) && isdigit(c)) {
		j *= 10;
		j += c - '0';
	}
	*ptr = s;
	i &= 0xff;
	j &= 0xff;
	return (i << 8) | j;
}
