/*	$NetBSD: inet.c,v 1.2.6.1 1996/06/05 18:04:32 cgd Exp $	*/

/*
 * Copyright (c) 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char rcsid[] =
    "@(#) Header: inet.c,v 1.4 94/06/07 01:16:50 leres Exp (LBL)";
#endif

#include <sys/param.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#ifdef SOLARIS
#include <sys/sockio.h>
#endif

#include <net/if.h>
#include <netinet/in.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pcap.h>

/* Not all systems have IFF_LOOPBACK */
#ifdef IFF_LOOPBACK
#define ISLOOPBACK(p) ((p)->ifr_flags & IFF_LOOPBACK)
#else
#define ISLOOPBACK(p) (strcmp((p)->ifr_name, "lo0") == 0)
#endif

/*
 * Return the name of a network interface attached to the system, or NULL
 * if none can be found.  The interface must be configured up; the
 * lowest unit number is preferred; loopback is ignored.
 */
char *
pcap_lookupdev(errbuf)
	register char *errbuf;
{
	register int fd, minunit, n;
	register char *cp;
	register struct ifreq *ifrp, *ifend, *ifnext, *mp;
	struct ifconf ifc;
	struct ifreq ibuf[16], ifr;
	static char device[sizeof(ifrp->ifr_name) + 1];

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		(void)sprintf(errbuf, "socket: %s", pcap_strerror(errno));
		return (NULL);
	}
	ifc.ifc_len = sizeof ibuf;
	ifc.ifc_buf = (caddr_t)ibuf;

	if (ioctl(fd, SIOCGIFCONF, (char *)&ifc) < 0 ||
	    ifc.ifc_len < sizeof(struct ifreq)) {
		(void)sprintf(errbuf, "SIOCGIFCONF: %s", pcap_strerror(errno));
		(void)close(fd);
		return (NULL);
	}
	ifrp = ibuf;
	ifend = (struct ifreq *)((char *)ibuf + ifc.ifc_len);

	mp = NULL;
	minunit = 666;
	for (; ifrp < ifend; ifrp = ifnext) {
#if BSD - 0 >= 199006
		n = ifrp->ifr_addr.sa_len + sizeof(ifrp->ifr_name);
		if (n < sizeof(*ifrp))
			ifnext = ifrp + 1;
		else
			ifnext = (struct ifreq *)((char *)ifrp + n);
		if (ifrp->ifr_addr.sa_family != AF_INET)
			continue;
#else
		ifnext = ifrp + 1;
#endif
		/*
		 * Need a template to preserve address info that is
		 * used below to locate the next entry.  (Otherwise,
		 * SIOCGIFFLAGS stomps over it because the requests
		 * are returned in a union.)
		 */
		strncpy(ifr.ifr_name, ifrp->ifr_name, sizeof(ifr.ifr_name));
		if (ioctl(fd, SIOCGIFFLAGS, (char *)&ifr) < 0) {
			(void)sprintf(errbuf, "SIOCGIFFLAGS: %s",
			    pcap_strerror(errno));
			(void)close(fd);
			return (NULL);
		}

		/* Must be up and not the loopback */
		if ((ifr.ifr_flags & IFF_UP) == 0 || ISLOOPBACK(&ifr))
			continue;

		for (cp = ifrp->ifr_name; !isdigit(*cp); ++cp)
			continue;
		n = atoi(cp);
		if (n < minunit) {
			minunit = n;
			mp = ifrp;
		}
	}
	(void)close(fd);
	if (mp == NULL) {
		(void)strcpy(errbuf, "no suitable device found");
		return (NULL);
	}

	(void)strncpy(device, mp->ifr_name, sizeof(device) - 1);
	device[sizeof(device) - 1] = '\0';
	return (device);
}

int
pcap_lookupnet(device, netp, maskp, errbuf)
	register char *device;
	register u_int32_t *netp, *maskp;
	register char *errbuf;
{
	register int fd;
	register struct sockaddr_in *sin;
	struct ifreq ifr;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		(void)sprintf(errbuf, "socket: %s", pcap_strerror(errno));
		return (-1);
	}
	(void)strncpy(ifr.ifr_name, device, sizeof(ifr.ifr_name));
	if (ioctl(fd, SIOCGIFADDR, (char *)&ifr) < 0) {
		(void)sprintf(errbuf, "SIOCGIFADDR: %s: %s",
		    device, pcap_strerror(errno));
		(void)close(fd);
		return (-1);
	}
	sin = (struct sockaddr_in *)&ifr.ifr_addr;
	*netp = sin->sin_addr.s_addr;
	if (ioctl(fd, SIOCGIFNETMASK, (char *)&ifr) < 0) {
		(void)sprintf(errbuf, "SIOCGIFNETMASK: %s: %s",
		    device, pcap_strerror(errno));
		(void)close(fd);
		return (-1);
	}
	(void)close(fd);
	*maskp = sin->sin_addr.s_addr;
	if (*maskp == 0) {
		if (IN_CLASSA(*netp))
			*maskp = IN_CLASSA_NET;
		else if (IN_CLASSB(*netp))
			*maskp = IN_CLASSB_NET;
		else if (IN_CLASSC(*netp))
			*maskp = IN_CLASSC_NET;
		else {
			(void)sprintf(errbuf, "inet class for 0x%x unknown",
			    *netp);
			return (-1);
		}
	}
	*netp &= *maskp;
	return (0);
}
