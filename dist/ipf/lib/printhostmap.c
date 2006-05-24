/*	$NetBSD: printhostmap.c,v 1.1.1.1.12.1 2006/05/24 15:47:46 tron Exp $	*/

#include "ipf.h"

void printhostmap(hmp, hv)
hostmap_t *hmp;
u_int hv;
{
	struct in_addr in;

	printf("%s,", inet_ntoa(hmp->hm_srcip));
	printf("%s -> ", inet_ntoa(hmp->hm_dstip));
	in.s_addr = htonl(hmp->hm_mapip.s_addr);
	printf("%s ", inet_ntoa(in));
	printf("(use = %d hv = %u)\n", hmp->hm_ref, hv);
}
