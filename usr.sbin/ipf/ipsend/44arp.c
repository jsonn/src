/*	$NetBSD: 44arp.c,v 1.4.2.1 1997/10/30 07:17:07 mrg Exp $	*/

/*
 * Based upon 4.4BSD's /usr/sbin/arp
 */
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <nlist.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#if __FreeBSD_version >= 300000
# include <net/if_var.h>
#endif
#include "ipsend.h"


/*
 * lookup host and return
 * its IP address in address
 * (4 bytes)
 */
int	resolve(host, address) 
char	*host, *address;
{
        struct	hostent	*hp;
        u_long	add;

	add = inet_addr(host);
	if (add == -1)
	    {
		if (!(hp = gethostbyname(host)))
		    {
			fprintf(stderr, "unknown host: %s\n", host);
			return -1;
		    }
		bcopy((char *)hp->h_addr, (char *)address, 4);
		return 0;
	}
	bcopy((char*)&add, address, 4);
	return 0;
}


int	arp(addr, eaddr)
char	*addr, *eaddr;
{
	int	mib[6];
	size_t	needed;
	char	*lim, *buf, *next;
	struct	rt_msghdr	*rtm;
	struct	sockaddr_inarp	*sin;
	struct	sockaddr_dl	*sdl;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_FLAGS;
	mib[5] = RTF_LLINFO;
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) == -1)
	    {
		perror("route-sysctl-estimate");
		exit(-1);
	    }
	if ((buf = malloc(needed)) == NULL)
	    {
		perror("malloc");
		exit(-1);
	    }
	if (sysctl(mib, 6, buf, &needed, NULL, 0) == -1)
	    {
		perror("actual retrieval of routing table");
		exit(-1);
	    }
	lim = buf + needed;
	for (next = buf; next < lim; next += rtm->rtm_msglen)
	    {
		rtm = (struct rt_msghdr *)next;
		sin = (struct sockaddr_inarp *)(rtm + 1);
		sdl = (struct sockaddr_dl *)(sin + 1);
		if (addr && !bcmp(addr, (char *)&sin->sin_addr,
				  sizeof(struct in_addr)))
		    {
			bcopy(LLADDR(sdl), eaddr, sdl->sdl_alen);
			return 0;
		    }
	    }
	return -1;
}
