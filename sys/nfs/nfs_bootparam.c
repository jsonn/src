/*	$NetBSD: nfs_bootparam.c,v 1.2.2.1 1997/12/14 23:11:30 mellon Exp $	*/

/*-
 * Copyright (c) 1995, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass and Gordon W. Ross.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Support for NFS diskless booting, Sun-style (RPC/bootparams)
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/mbuf.h>
#include <sys/reboot.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_ether.h>

#include <netinet/in.h>
#include <netinet/if_inarp.h>

#include <nfs/rpcv2.h>
#include <nfs/krpc.h>
#include <nfs/xdr_subs.h>

#include <nfs/nfsproto.h>
#include <nfs/nfsdiskless.h>

/*
 * There are two implementations of NFS diskless boot.
 * This implementation uses Sun RPC/bootparams, and the
 * the other uses BOOTP (RFC951 - see nfs_bootdhcp.c).
 *
 * The Sun-style boot sequence goes as follows:
 * (1) Use RARP to get our interface address
 * (2) Use RPC/bootparam/whoami to get our hostname,
 *     our IP address, and the server's IP address.
 * (3) Use RPC/bootparam/getfile to get the root path
 * (4) Use RPC/mountd to get the root file handle
 * (5) Use RPC/bootparam/getfile to get the swap path
 * (6) Use RPC/mountd to get the swap file handle
 */

/* bootparam RPC */
static int bp_whoami __P((struct sockaddr_in *bpsin,
	struct in_addr *my_ip, struct in_addr *gw_ip));
static int bp_getfile __P((struct sockaddr_in *bpsin, char *key,
	struct nfs_dlmount *ndm));


/*
 * Get client name, gateway address, then
 * get root and swap server:pathname info.
 * RPCs: bootparam/whoami, bootparam/getfile
 *
 * Use the old broadcast address for the WHOAMI
 * call because we do not yet know our netmask.
 * The server address returned by the WHOAMI call
 * is used for all subsequent booptaram RPCs.
 */
int
nfs_bootparam(ifp, nd, procp)
	struct ifnet *ifp;
	struct nfs_diskless *nd;
	struct proc *procp;
{
	struct ifreq ireq;
	struct in_addr my_ip, gw_ip;
	struct sockaddr_in bp_sin;
	struct sockaddr_in *sin;
	struct socket *so;
	struct nfs_dlmount *gw_ndm;
#if 0	/* XXX - not yet */
	char *p;
	u_int32_t mask;
#endif	/* XXX */
	int error;

	gw_ndm = 0;
	bzero(&ireq, sizeof(ireq));
	bcopy(ifp->if_xname, ireq.ifr_name, IFNAMSIZ);

	/*
	 * Get a socket to use for various things in here.
	 * After this, use "goto out" to cleanup and return.
	 */
	error = socreate(AF_INET, &so, SOCK_DGRAM, 0);
	if (error) {
		printf("nfs_boot: socreate, error=%d\n", error);
		return (error);
	}

	/*
	 * Bring up the interface. (just set the "up" flag)
	 * Get the old interface flags and or IFF_UP into them so
	 * things like media selection flags are not clobbered.
	 */
	error = ifioctl(so, SIOCGIFFLAGS, (caddr_t)&ireq, procp);
	if (error) {
		printf("nfs_boot: GIFFLAGS, error=%d\n", error);
		goto out;
	}
	ireq.ifr_flags |= IFF_UP;
	error = ifioctl(so, SIOCSIFFLAGS, (caddr_t)&ireq, procp);
	if (error) {
		printf("nfs_boot: SIFFLAGS, error=%d\n", error);
		goto out;
	}

	/*
	 * Do RARP for the interface address.
	 */
	error = revarpwhoami(&my_ip, ifp);
	if (error) {
		printf("revarp failed, error=%d\n", error);
		goto out;
	}
	nd->nd_myip.s_addr = my_ip.s_addr;
	printf("nfs_boot: client_addr=0x%x\n",
	       (u_int32_t)ntohl(my_ip.s_addr));

	/*
	 * Do enough of ifconfig(8) so that the chosen interface
	 * can talk to the servers.  (just set the address)
	 */
	sin = (struct sockaddr_in *)&ireq.ifr_addr;
	sin->sin_len = sizeof(*sin);
	sin->sin_family = AF_INET;
	sin->sin_addr = my_ip;
	error = ifioctl(so, SIOCSIFADDR, (caddr_t)&ireq, procp);
	if (error) {
		printf("nfs_boot: set ifaddr, error=%d\n", error);
		goto out;
	}

	/*
	 * Get client name and gateway address.
	 * RPC: bootparam/whoami
	 * Use the old broadcast address for the WHOAMI
	 * call because we do not yet know our netmask.
	 * The server address returned by the WHOAMI call
	 * is used for all subsequent booptaram RPCs.
	 */
	sin = &bp_sin;
	bzero((caddr_t)sin, sizeof(*sin));
	sin->sin_len = sizeof(*sin);
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = INADDR_BROADCAST;

	/* Do the RPC/bootparam/whoami. */
	error = bp_whoami(sin, &my_ip, &gw_ip);
	if (error) {
		printf("nfs_boot: bootparam whoami, error=%d\n", error);
		goto out;
	}
	printf("nfs_boot: server_addr=0x%x\n",
		   (u_int32_t)ntohl(sin->sin_addr.s_addr));
	printf("nfs_boot: hostname=%s\n", hostname);

	/*
	 * Now fetch the server:pathname strings and server IP
	 * for root and swap.  Missing swap is not fatal.
	 */
	error = bp_getfile(sin, "root", &nd->nd_root);
	if (error) {
		printf("nfs_boot: bootparam get root: %d\n", error);
		goto out;
	}
#if 0
	error = bp_getfile(sin, "swap", &nd->nd_swap);
	if (error) {
		printf("nfs_boot: bootparam get swap: %d\n", error);
		error = 0;
	}
#endif

#ifdef	NFS_BOOT_GATEWAY
	/*
	 * Note: we normally ignore the gateway address returned
	 * by the "bootparam/whoami" RPC above, because many old
	 * bootparam servers supply a bogus gateway value.
	 *
	 * These deficiencies in the bootparam RPC interface are
	 * circumvented by using the bootparam/getfile RPC.  The
	 * parameter "gateway" is requested, and if its returned,
	 * we use the "server" part of the reply as the gateway,
	 * and use the "pathname" part of the reply as the mask.
	 * (The mask comes to us as a string.)
	 */
	if (gw_ip.s_addr) {
		/* Our caller will add the route. */
		nd->nd_gwip = gw_ip;
	}
#endif
#if 0	/* XXX - not yet */
	gw_ndm = malloc(sizeof(*gw_ndm), M_NFSMNT, M_WAITOK);
	bzero((caddr_t)gw_ndm, sizeof(*gw_ndm));
	error = bp_getfile(sin, "gateway", gw_ndm);
	if (error) {
		/* Ignore the error.  No gateway supplied. */
		error = 0;
		goto out;
	}
	printf("nfs_boot: gateway=%s\n", gw_ndm->ndm_host);
	sin = (struct sockaddr_in *) &gw_ndm->ndm_saddr;
	nd->nd_gwip = sin->sin_addr;
	/* Find the pathname part of the "mounted-on" string. */
	p = strchr(gw_ndm->ndm_host, ':');
	if (p == 0)
		goto out;
	/* have pathname */
	p++;	/* skip ':' */
	mask = inet_addr(p);	/* libkern */
	if (mask == 0) {
		printf("nfs_boot: gateway netmask missing\n");
		goto out;
	}

	/* Save our netmask and update the network interface. */
	nd->nd_mask.s_addr = mask;
	sin = (struct sockaddr_in *)&ireq.ifr_addr;
	sin->sin_len = sizeof(*sin);
	sin->sin_family = AF_INET;
	sin->sin_addr = nd->nd_mask;
	error = ifioctl(so, SIOCSIFNETMASK, (caddr_t)&ireq, procp);
	if (error) {
		printf("nfs_boot: set ifmask, error=%d\n", error);
		error = 0;	/* ignore it */
	}
#endif	/* XXX */

 out:
	if (gw_ndm)
		free(gw_ndm, M_NFSMNT);
	soclose(so);
	return (error);
}


/*
 * RPC: bootparam/whoami
 * Given client IP address, get:
 *	client name	(hostname)
 *	domain name (domainname)
 *	gateway address
 *
 * The hostname and domainname are set here for convenience.
 *
 * Note - bpsin is initialized to the broadcast address,
 * and will be replaced with the bootparam server address
 * after this call is complete.  Have to use PMAP_PROC_CALL
 * to make sure we get responses only from a servers that
 * know about us (don't want to broadcast a getport call).
 */
static int
bp_whoami(bpsin, my_ip, gw_ip)
	struct sockaddr_in *bpsin;
	struct in_addr *my_ip;
	struct in_addr *gw_ip;
{
	/* RPC structures for PMAPPROC_CALLIT */
	struct whoami_call {
		u_int32_t call_prog;
		u_int32_t call_vers;
		u_int32_t call_proc;
		u_int32_t call_arglen;
	} *call;
	struct callit_reply {
		u_int32_t port;
		u_int32_t encap_len;
		/* encapsulated data here */
	} *reply;

	struct mbuf *m, *from;
	struct sockaddr_in *sin;
	int error, msg_len;
	int16_t port;

	/*
	 * Build request message for PMAPPROC_CALLIT.
	 */
	m = m_get(M_WAIT, MT_DATA);
	call = mtod(m, struct whoami_call *);
	m->m_len = sizeof(*call);
	call->call_prog = txdr_unsigned(BOOTPARAM_PROG);
	call->call_vers = txdr_unsigned(BOOTPARAM_VERS);
	call->call_proc = txdr_unsigned(BOOTPARAM_WHOAMI);

	/*
	 * append encapsulated data (client IP address)
	 */
	m->m_next = xdr_inaddr_encode(my_ip);
	call->call_arglen = txdr_unsigned(m->m_next->m_len);

	/* RPC: portmap/callit */
	bpsin->sin_port = htons(PMAPPORT);
	from = NULL;
	error = krpc_call(bpsin, PMAPPROG, PMAPVERS,
			PMAPPROC_CALLIT, &m, &from);
	if (error)
		return error;

	/*
	 * Parse result message.
	 */
	if (m->m_len < sizeof(*reply)) {
		m = m_pullup(m, sizeof(*reply));
		if (m == NULL)
			goto bad;
	}
	reply = mtod(m, struct callit_reply *);
	port = fxdr_unsigned(u_int32_t, reply->port);
	msg_len = fxdr_unsigned(u_int32_t, reply->encap_len);
	m_adj(m, sizeof(*reply));

	/*
	 * Save bootparam server address
	 */
	sin = mtod(from, struct sockaddr_in *);
	bpsin->sin_port = htons(port);
	bpsin->sin_addr.s_addr = sin->sin_addr.s_addr;

	/* client name */
	hostnamelen = MAXHOSTNAMELEN-1;
	m = xdr_string_decode(m, hostname, &hostnamelen);
	if (m == NULL)
		goto bad;

	/* domain name */
	domainnamelen = MAXHOSTNAMELEN-1;
	m = xdr_string_decode(m, domainname, &domainnamelen);
	if (m == NULL)
		goto bad;

	/* gateway address */
	m = xdr_inaddr_decode(m, gw_ip);
	if (m == NULL)
		goto bad;

	/* success */
	goto out;

bad:
	printf("nfs_boot: bootparam_whoami: bad reply\n");
	error = EBADRPC;

out:
	if (from)
		m_freem(from);
	if (m)
		m_freem(m);
	return(error);
}


/*
 * RPC: bootparam/getfile
 * Given client name and file "key", get:
 *	server name
 *	server IP address
 *	server pathname
 */
static int
bp_getfile(bpsin, key, ndm)
	struct sockaddr_in *bpsin;
	char *key;
	struct nfs_dlmount *ndm;
{
	char pathname[MNAMELEN];
	struct in_addr inaddr;
	struct sockaddr_in *sin;
	struct mbuf *m;
	char *serv_name;
	int error, sn_len, path_len;

	/*
	 * Build request message.
	 */

	/* client name (hostname) */
	m  = xdr_string_encode(hostname, hostnamelen);
	if (m == NULL)
		return (ENOMEM);

	/* key name (root or swap) */
	m->m_next = xdr_string_encode(key, strlen(key));
	if (m->m_next == NULL)
		return (ENOMEM);

	/* RPC: bootparam/getfile */
	error = krpc_call(bpsin, BOOTPARAM_PROG, BOOTPARAM_VERS,
	                  BOOTPARAM_GETFILE, &m, NULL);
	if (error)
		return error;

	/*
	 * Parse result message.
	 */

	/* server name */
	serv_name = &ndm->ndm_host[0];
	sn_len = sizeof(ndm->ndm_host) - 1;
	m = xdr_string_decode(m, serv_name, &sn_len);
	if (m == NULL)
		goto bad;

	/* server IP address (mountd/NFS) */
	m = xdr_inaddr_decode(m, &inaddr);
	if (m == NULL)
		goto bad;

	/* server pathname */
	path_len = sizeof(pathname) - 1;
	m = xdr_string_decode(m, pathname, &path_len);
	if (m == NULL)
		goto bad;

	/*
	 * Store the results in the nfs_dlmount.
	 * The strings become "server:pathname"
	 */
	sin = (struct sockaddr_in *) &ndm->ndm_saddr;
	bzero((caddr_t)sin, sizeof(*sin));
	sin->sin_len = sizeof(*sin);
	sin->sin_family = AF_INET;
	sin->sin_addr = inaddr;
	if ((sn_len + 1 + path_len + 1) > sizeof(ndm->ndm_host)) {
		printf("nfs_boot: getfile name too long\n");
		error = EIO;
		goto out;
	}
	ndm->ndm_host[sn_len] = ':';
	bcopy(pathname, ndm->ndm_host + sn_len + 1, path_len + 1);

	/* success */
	goto out;

bad:
	printf("nfs_boot: bootparam_getfile: bad reply\n");
	error = EBADRPC;

out:
	m_freem(m);
	return(0);
}


