/*	$NetBSD: ip_proxy.h,v 1.15.4.1 2002/02/09 16:56:30 he Exp $	*/

/*
 * Copyright (C) 1997-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Id: ip_proxy.h,v 2.8.2.12 2002/01/01 13:41:43 darrenr Exp
 */

#ifndef _NETINET_IP_PROXY_H_
#define _NETINET_IP_PROXY_H_

#ifndef SOLARIS
#define SOLARIS (defined(sun) && (defined(__svr4__) || defined(__SVR4)))
#endif

#ifndef	APR_LABELLEN
#define	APR_LABELLEN	16
#endif
#define	AP_SESS_SIZE	53

struct	nat;
struct	ipnat;

typedef	struct	ap_tcp {
	u_short	apt_sport;	/* source port */
	u_short	apt_dport;	/* destination port */
	short	apt_sel[2];	/* {seq,ack}{off,min} set selector */
	short	apt_seqoff[2];	/* sequence # difference */
	tcp_seq	apt_seqmin[2];	/* don't change seq-off until after this */
	short	apt_ackoff[2];	/* sequence # difference */
	tcp_seq	apt_ackmin[2];	/* don't change seq-off until after this */
	u_char	apt_state[2];	/* connection state */
} ap_tcp_t;

typedef	struct	ap_udp {
	u_short	apu_sport;	/* source port */
	u_short	apu_dport;	/* destination port */
} ap_udp_t;

typedef	struct ap_session {
	struct	aproxy	*aps_apr;
	union {
		struct	ap_tcp	apu_tcp;
		struct	ap_udp	apu_udp;
	} aps_un;
	u_int	aps_flags;
	U_QUAD_T aps_bytes;	/* bytes sent */
	U_QUAD_T aps_pkts;	/* packets sent */
	void	*aps_nat;	/* pointer back to nat struct */
	void	*aps_data;	/* private data */
	int	aps_p;		/* protocol */
	int	aps_psiz;	/* size of private data */
	struct	ap_session	*aps_hnext;
	struct	ap_session	*aps_next;
} ap_session_t;

#define	aps_sport	aps_un.apu_tcp.apt_sport
#define	aps_dport	aps_un.apu_tcp.apt_dport
#define	aps_sel		aps_un.apu_tcp.apt_sel
#define	aps_seqoff	aps_un.apu_tcp.apt_seqoff
#define	aps_seqmin	aps_un.apu_tcp.apt_seqmin
#define	aps_state	aps_un.apu_tcp.apt_state
#define	aps_ackoff	aps_un.apu_tcp.apt_ackoff
#define	aps_ackmin	aps_un.apu_tcp.apt_ackmin


typedef	struct	aproxy	{
	struct	aproxy	*apr_next;
	char	apr_label[APR_LABELLEN];	/* Proxy label # */
	u_char	apr_p;		/* protocol */
	int	apr_ref;	/* +1 per rule referencing it */
	int	apr_flags;
	int	(* apr_init) __P((void));
	void	(* apr_fini) __P((void));
	int	(* apr_new) __P((fr_info_t *, ip_t *,
				 ap_session_t *, struct nat *));
	void	(* apr_del) __P((ap_session_t *));
	int	(* apr_inpkt) __P((fr_info_t *, ip_t *,
				   ap_session_t *, struct nat *));
	int	(* apr_outpkt) __P((fr_info_t *, ip_t *,
				    ap_session_t *, struct nat *));
	int	(* apr_match) __P((fr_info_t *, ap_session_t *, struct nat *));
} aproxy_t;

#define	APR_DELETE	1

#define	APR_ERR(x)	(((x) & 0xffff) << 16)
#define	APR_EXIT(x)	(((x) >> 16) & 0xffff)
#define	APR_INC(x)	((x) & 0xffff)

#define	FTP_BUFSZ	160
/*
 * For the ftp proxy.
 */
typedef struct  ftpside {
	char	*ftps_rptr;
	char	*ftps_wptr;
	u_32_t	ftps_seq;
	u_32_t	ftps_len;
	int	ftps_junk;
	int	ftps_cmds;
	char	ftps_buf[FTP_BUFSZ];
} ftpside_t;

typedef struct  ftpinfo {
	int 	  	ftp_passok;
	int		ftp_incok;
	ftpside_t	ftp_side[2];
} ftpinfo_t;

/*
 * Real audio proxy structure and #defines
 */
typedef	struct	raudio_s {
	int	rap_seenpna;
	int	rap_seenver;
	int	rap_version;
	int	rap_eos;	/* End Of Startup */
	int	rap_gotid;
	int	rap_gotlen;
	int	rap_mode;
	int	rap_sdone;
	u_short	rap_plport;
	u_short	rap_prport;
	u_short	rap_srport;
	char	rap_svr[19];
	u_32_t	rap_sbf;	/* flag to indicate which of the 19 bytes have
				 * been filled
				 */
	tcp_seq	rap_sseq;
} raudio_t;

#define	RA_ID_END	0
#define	RA_ID_UDP	1
#define	RA_ID_ROBUST	7

#define	RAP_M_UDP	1
#define	RAP_M_ROBUST	2
#define	RAP_M_TCP	4
#define	RAP_M_UDP_ROBUST	(RAP_M_UDP|RAP_M_ROBUST)

/*
 * IPSec proxy
 */
typedef	u_32_t	ipsec_cookie_t[2];

typedef struct ipsec_pxy {
	ipsec_cookie_t	ipsc_icookie;
	ipsec_cookie_t	ipsc_rcookie;
	int		ipsc_rckset;
	ipnat_t		ipsc_rule;
	nat_t		*ipsc_nat;
	ipstate_t	*ipsc_state;
} ipsec_pxy_t;

extern	ap_session_t	*ap_sess_tab[AP_SESS_SIZE];
extern	ap_session_t	*ap_sess_list;
extern	aproxy_t	ap_proxies[];
extern	int		ippr_ftp_pasvonly;

extern	int	appr_add __P((aproxy_t *));
extern	int	appr_del __P((aproxy_t *));
extern	int	appr_init __P((void));
extern	void	appr_unload __P((void));
extern	int	appr_ok __P((ip_t *, tcphdr_t *, struct ipnat *));
extern	int	appr_match __P((fr_info_t *, struct nat *));
extern	void	appr_free __P((aproxy_t *));
extern	void	aps_free __P((ap_session_t *));
extern	int	appr_check __P((ip_t *, fr_info_t *, struct nat *));
extern	aproxy_t	*appr_lookup __P((u_int, char *));
extern	int	appr_new __P((fr_info_t *, ip_t *, struct nat *));

#endif /* _NETINET_IP_PROXY_H_ */
