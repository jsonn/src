/*	$NetBSD: getsumd.c,v 1.1.1.1.18.2 2007/07/16 11:04:55 liamjfoy Exp $	*/

/*
 * Copyright (C) 2002 by Darren Reed.
 * 
 * See the IPFILTER.LICENCE file for details on licencing.  
 *   
 * Id: getsumd.c,v 1.2.4.1 2006/06/16 17:21:01 darrenr Exp 
 */     

#include "ipf.h"

char *getsumd(sum)
u_32_t sum;
{
	static char sumdbuf[17];

	if (sum & NAT_HW_CKSUM)
		sprintf(sumdbuf, "hw(%#0x)", sum & 0xffff);
	else
		sprintf(sumdbuf, "%#0x", sum);
	return sumdbuf;
}
