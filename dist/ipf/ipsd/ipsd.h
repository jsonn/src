/*	$NetBSD: ipsd.h,v 1.1.1.1.8.1 2002/02/09 16:55:13 he Exp $	*/

/*
 * (C)opyright 1995-1998 Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 *   The author of this software makes no garuntee about the
 * performance of this package or its suitability to fulfill any purpose.
 *
 * @(#)ipsd.h	1.3 12/3/95
 */

typedef	struct	{
	time_t	sh_date;
	struct	in_addr	sh_ip;
} sdhit_t;

typedef	struct	{
	u_int	sd_sz;
	u_int	sd_cnt;
	u_short	sd_port;
	sdhit_t	*sd_hit;
} ipsd_t;

typedef	struct	{
	struct	in_addr	ss_ip;
	int	ss_hits;
	u_long	ss_ports;
} ipss_t;

