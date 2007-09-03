/*	$NetBSD: printip.c,v 1.1.1.1.18.1.2.1 2007/09/03 06:54:31 wrstuden Exp $	*/

/*
 * Copyright (C) 2002-2005 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Id: printip.c,v 1.3.4.1 2006/06/16 17:21:12 darrenr Exp
 */

#include "ipf.h"


void	printip(addr)
u_32_t	*addr;
{
	struct in_addr ipa;

	ipa.s_addr = *addr;
	if (ntohl(ipa.s_addr) < 256)
		printf("%lu", (u_long)ntohl(ipa.s_addr));
	else
		printf("%s", inet_ntoa(ipa));
}
