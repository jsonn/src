/*	$NetBSD: ip_frag.h,v 1.17.10.1 2002/10/24 09:33:57 lukem Exp $	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * @(#)ip_frag.h	1.5 3/24/96
 * Id: ip_frag.h,v 2.4.2.7 2002/07/06 14:17:51 darrenr Exp
 */

#ifndef _NETINET_IP_FRAG_H_
#define _NETINET_IP_FRAG_H_

#define	IPFT_SIZE	257

typedef	struct	ipfr	{
	struct	ipfr	*ipfr_next, *ipfr_prev;
	void	*ipfr_data;
	struct	in_addr	ipfr_src;
	struct	in_addr	ipfr_dst;
	void	*ipfr_ifp;
	u_32_t	ipfr_optmsk;
	u_short	ipfr_secmsk;
	u_short	ipfr_auth;
	u_short	ipfr_id;
	u_char	ipfr_p;
	u_char	ipfr_tos;
	u_short	ipfr_off;
	u_char	ipfr_ttl;
	u_char	ipfr_seen0;
	frentry_t *ipfr_rule;
} ipfr_t;


typedef	struct	ipfrstat {
	u_long	ifs_exists;	/* add & already exists */
	u_long	ifs_nomem;
	u_long	ifs_new;
	u_long	ifs_hits;
	u_long	ifs_expire;
	u_long	ifs_inuse;
	struct	ipfr	**ifs_table;
	struct	ipfr	**ifs_nattab;
} ipfrstat_t;

#define	IPFR_CMPSZ	(offsetof(ipfr_t, ipfr_off) - \
			 offsetof(ipfr_t, ipfr_src))

extern	int	fr_ipfrttl;
extern	int	fr_frag_lock;
extern	ipfrstat_t	*ipfr_fragstats __P((void));
extern	int	ipfr_newfrag __P((ip_t *, fr_info_t *));
extern	int	ipfr_nat_newfrag __P((ip_t *, fr_info_t *, struct nat *));
extern	nat_t	*ipfr_nat_knownfrag __P((ip_t *, fr_info_t *));
extern	frentry_t *ipfr_knownfrag __P((ip_t *, fr_info_t *));
extern	void	ipfr_forget __P((void *));
extern	void	ipfr_unload __P((void));
extern	void	ipfr_fragexpire __P((void));

#ifdef _KERNEL
# if     (BSD >= 199306) || SOLARIS || defined(__sgi)
#  if defined(SOLARIS2) && (SOLARIS2 < 7)
extern	void	ipfr_slowtimer __P((void));
#  else
extern	void	ipfr_slowtimer __P((void *));
#  endif
# else
extern	int	ipfr_slowtimer __P((void));
# endif /* (BSD >= 199306) || SOLARIS */
#else
extern	void	ipfr_slowtimer __P((void));
#endif /* _KERNEL */

#endif /* _NETINET_IP_FRAG_H_ */
