/*	$NetBSD: nfs_boot.c,v 1.33.4.1 1997/08/23 07:14:22 thorpej Exp $	*/

/*
 * Copyright (c) 1995 Adam Glass, Gordon Ross
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <nfs/nfsdiskless.h>
#include <nfs/krpc.h>
#include <nfs/xdr_subs.h>
#include <nfs/nfs_var.h>

#include "ether.h"
#if NETHER == 0

int nfs_boot_init(nd, procp)
	struct nfs_diskless *nd;
	struct proc *procp;
{
	printf("nfs_boot: NETHER == 0\n");
	return (ENXIO);
}

int
nfs_boot_getfh(ndm)
	struct nfs_dlmount *ndm;
{
	return (ENXIO);
}

#else /* NETHER */

/*
 * Support for NFS diskless booting, specifically getting information
 * about where to boot from, what pathnames, etc.
 *
 * This implememtation uses RARP and the bootparam RPC.
 * We are forced to implement RPC anyway (to get file handles)
 * so we might as well take advantage of it for bootparam too.
 *
 * The diskless boot sequence goes as follows:
 * (1) Use RARP to get our interface address
 * (2) Use RPC/bootparam/whoami to get our hostname,
 *     our IP address, and the server's IP address.
 * (3) Use RPC/bootparam/getfile to get the root path
 * (4) Use RPC/mountd to get the root file handle
 * (5) Use RPC/bootparam/getfile to get the swap path
 * (6) Use RPC/mountd to get the swap file handle
 *
 * (This happens to be the way Sun does it too.)
 */

/* bootparam RPC */
static int bp_whoami __P((struct sockaddr_in *bpsin,
	struct in_addr *my_ip, struct in_addr *gw_ip));
static int bp_getfile __P((struct sockaddr_in *bpsin, char *key,
	struct nfs_dlmount *ndm));

/* mountd RPC */
static int md_mount __P((struct sockaddr_in *mdsin, char *path,
	struct nfs_args *argp));

/*
 * Called with an empty nfs_diskless struct to be filled in.
 */
int
nfs_boot_init(nd, procp)
	struct nfs_diskless *nd;
	struct proc *procp;
{
	struct ifreq ireq;
	struct ifaliasreq iareq;
	struct in_addr my_ip, gw_ip;
	struct sockaddr_in bp_sin;
	struct sockaddr_in *sin;
	struct ifnet *ifp;
	struct socket *so;
	int error;

	/*
	 * Find an interface, rarp for its ip address, stuff it, the
	 * implied broadcast addr, and netmask into a nfs_diskless struct.
	 *
	 * This was moved here from nfs_vfsops.c because this procedure
	 * would be quite different if someone decides to write (i.e.) a
	 * BOOTP version of this file (might not use RARP, etc.)
	 */

	/*
	 * Find a network interface.
	 */
	ifp = ifunit(root_device->dv_xname);
	if (ifp == NULL) {
		printf("nfs_boot: no suitable interface\n");
		return (ENXIO);
	}
	bcopy(ifp->if_xname, ireq.ifr_name, IFNAMSIZ);
	printf("nfs_boot: using network interface '%s'\n",
	    ireq.ifr_name);

	/*
	 * Bring up the interface.
	 *
	 * Get the old interface flags and or IFF_UP into them; if
	 * IFF_UP set blindly, interface selection can be clobbered.
	 */
	so = 0;
	error = socreate(AF_INET, &so, SOCK_DGRAM, 0);
	if (error) {
		printf("nfs_boot: socreate, error=%d\n", error);
		so = 0;
		goto out;
	}
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
	if ((error = revarpwhoami(&my_ip, ifp)) != 0) {
		printf("revarp failed, error=%d\n", error);
		error = EIO;	/* XXX */
		goto out;
	}
	printf("nfs_boot: client_addr=0x%x\n",
	       (u_int32_t)ntohl(my_ip.s_addr));

	/*
	 * Do enough of ifconfig(8) so that the chosen interface
	 * can talk to the servers.  (just set the address)
	 */
	bzero(&iareq, sizeof(iareq));
	bcopy(ifp->if_xname, iareq.ifra_name, IFNAMSIZ);
	sin = (struct sockaddr_in *)&iareq.ifra_addr;
	sin->sin_len = sizeof(*sin);
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = my_ip.s_addr;
#ifdef NFS_BOOT_NETMASK
	sin = (struct sockaddr_in *)&iareq.ifra_mask;
	sin->sin_len = sizeof(*sin);
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = htonl(NFS_BOOT_NETMASK);
#endif
	error = ifioctl(so, SIOCAIFADDR, (caddr_t)&iareq, procp);
	if (error) {
		printf("nfs_boot: set if addr, error=%d\n", error);
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
	bzero((caddr_t)&bp_sin, sizeof(bp_sin));
	bp_sin.sin_len = sizeof(bp_sin);
	bp_sin.sin_family = AF_INET;
	bp_sin.sin_addr.s_addr = INADDR_BROADCAST;
	hostnamelen = MAXHOSTNAMELEN;

	/* Do the RPC/bootparam/whoami. */
	error = bp_whoami(&bp_sin, &my_ip, &gw_ip);
	if (error) {
		printf("nfs_boot: bootparam whoami, error=%d\n", error);
		goto out;
	}
	printf("nfs_boot: server_addr=0x%x\n",
		   (u_int32_t)ntohl(bp_sin.sin_addr.s_addr));
	printf("nfs_boot: hostname=%s\n", hostname);

#ifdef	NFS_BOOT_GATEWAY
	/*
	 * XXX - This code is conditionally compiled only because
	 * many bootparam servers (in particular, SunOS 4.1.3)
	 * always set the gateway address to their own address.
	 * The bootparam server is not necessarily the gateway.
	 * We could just believe the server, and at worst you would
	 * need to delete the incorrect default route before adding
	 * the correct one, but for simplicity, ignore the gateway.
	 * If your server is OK, you can turn on this option.
	 *
	 * If the gateway address is set, add a default route.
	 * (The mountd RPCs may go across a gateway.)
	 */
	if (gw_ip.s_addr) {
		struct sockaddr dst, gw, mask;
		/* Destination: (default) */
		bzero((caddr_t)&dst, sizeof(dst));
		dst.sa_len = sizeof(dst);
		dst.sa_family = AF_INET;
		/* Gateway: */
		bzero((caddr_t)&gw, sizeof(gw));
		sin = (struct sockaddr_in *)&gw;
		sin->sin_len = sizeof(gw);
		sin->sin_family = AF_INET;
		sin->sin_addr.s_addr = gw_ip.s_addr;
		/* Mask: (zero length) */
		bzero(&mask, sizeof(mask));

		printf("nfs_boot: gateway=0x%x\n", ntohl(gw_ip.s_addr));
		/* add, dest, gw, mask, flags, 0 */
		error = rtrequest(RTM_ADD, &dst, (struct sockaddr *)&gw,
		    &mask, (RTF_UP | RTF_GATEWAY | RTF_STATIC), NULL);
		if (error)
			printf("nfs_boot: add route, error=%d\n", error);
	}
#endif

	bcopy(&bp_sin, &nd->nd_boot, sizeof(bp_sin));

	/*
	 * Now fetch the server:pathname strings and server IP
	 * for root and swap.  Missing swap is not fatal.
	 */
	error = bp_getfile(&nd->nd_boot, "root", &nd->nd_root);
	if (error) {
		printf("nfs_boot: bootparam get root: %d\n", error);
		goto out;
	}

#if 0
	error = bp_getfile(&nd->nd_boot, "swap", &nd->nd_swap);
	if (error) {
		printf("nfs_boot: bootparam get swap: %d", error);
		error = 0;
	}
#endif

 out:
	if (so) soclose(so);

	return (error);
}

int
nfs_boot_getfh(ndm)
	struct nfs_dlmount *ndm;	/* output */
{
	struct nfs_args *args;
	struct sockaddr_in *sin;
	char *pathname;
	int error;

	args = &ndm->ndm_args;

	/* Initialize mount args. */
	bzero((caddr_t) args, sizeof(*args));
	args->addr     = &ndm->ndm_saddr;
	args->addrlen  = args->addr->sa_len;
#ifdef NFS_BOOT_TCP
	args->sotype   = SOCK_STREAM;
#else
	args->sotype   = SOCK_DGRAM;
#endif
	args->fh       = ndm->ndm_fh;
	args->hostname = ndm->ndm_host;
	args->flags    = NFSMNT_RESVPORT | NFSMNT_NFSV3;

#ifdef	NFS_BOOT_OPTIONS
	args->flags    |= NFS_BOOT_OPTIONS;
#endif
#ifdef	NFS_BOOT_RWSIZE
	/*
	 * Reduce rsize,wsize for interfaces that consistently
	 * drop fragments of long UDP messages.  (i.e. wd8003).
	 * You can always change these later via remount.
	 */
	args->flags   |= NFSMNT_WSIZE | NFSMNT_RSIZE;
	args->wsize    = NFS_BOOT_RWSIZE;
	args->rsize    = NFS_BOOT_RWSIZE;
#endif

	/*
	 * Find the pathname part of the "server:pathname"
	 * string left in ndm->ndm_host by nfs_boot_init.
	 */
	pathname = strchr(ndm->ndm_host, ':');
	if (pathname == 0) {
		printf("nfs_boot: getfh - no pathname\n");
		return (EIO);
	}
	pathname++;

	/*
	 * Get file handle using RPC to mountd/mount
	 */
	sin = (struct sockaddr_in *)&ndm->ndm_saddr;
	error = md_mount(sin, pathname, args);
	if (error) {
		printf("nfs_boot: mountd `%s', error=%d\n",
		    ndm->ndm_host, error);
		return (error);
	}

	/* Set port number for NFS use. */
	/* XXX: NFS port is always 2049, right? */
#ifdef NFS_BOOT_TCP
retry:
#endif
	error = krpc_portmap(sin, NFS_PROG,
		    (args->flags & NFSMNT_NFSV3) ? NFS_VER3 : NFS_VER2,
		    (args->sotype == SOCK_STREAM) ? IPPROTO_TCP : IPPROTO_UDP,
		    &sin->sin_port);
	if (sin->sin_port == htons(0))
		error = EIO;
	if (error) { 
#ifdef NFS_BOOT_TCP
		if (args->sotype == SOCK_STREAM) {
			args->sotype = SOCK_DGRAM;
			goto retry;
		}
#endif
		printf("nfs_boot: portmap NFS, error=%d\n", error);
	}

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


/*
 * RPC: mountd/mount
 * Given a server pathname, get an NFS file handle.
 * Also, sets sin->sin_port to the NFS service port.
 */
static int
md_mount(mdsin, path, argp)
	struct sockaddr_in *mdsin;		/* mountd server address */
	char *path;
	struct nfs_args *argp;
{
	/* The RPC structures */
	struct rdata {
		u_int32_t errno;
		union {
			u_int8_t  v2fh[NFSX_V2FH];
			struct {
				u_int32_t fhlen;
				u_int8_t  fh[1];
			} v3fh;
		} fh;
	} *rdata;
	struct mbuf *m;
	u_int8_t *fh;
	int minlen, error;

retry:
	/* Get port number for MOUNTD. */
	error = krpc_portmap(mdsin, RPCPROG_MNT,
		     (argp->flags & NFSMNT_NFSV3) ? RPCMNT_VER3 : RPCMNT_VER1,
		     IPPROTO_UDP, &mdsin->sin_port);
	if (error)
		return error;

	m = xdr_string_encode(path, strlen(path));
	if (m == NULL)
		return ENOMEM;

	/* Do RPC to mountd. */
	error = krpc_call(mdsin, RPCPROG_MNT, (argp->flags & NFSMNT_NFSV3) ?
			  RPCMNT_VER3 : RPCMNT_VER1, RPCMNT_MOUNT, &m, NULL);
	if (error) {
		if ((error==RPC_PROGMISMATCH) && (argp->flags & NFSMNT_NFSV3)) {
			argp->flags &= ~NFSMNT_NFSV3;
			goto retry;
		}
		return error;	/* message already freed */
	}

	/* The reply might have only the errno. */
	if (m->m_len < 4)
		goto bad;
	/* Have at least errno, so check that. */
	rdata = mtod(m, struct rdata *);
	error = fxdr_unsigned(u_int32_t, rdata->errno);
	if (error)
		goto out;

	 /* Have errno==0, so the fh must be there. */
	if (argp->flags & NFSMNT_NFSV3){
		argp->fhsize   = fxdr_unsigned(u_int32_t, rdata->fh.v3fh.fhlen);
		if (argp->fhsize > NFSX_V3FHMAX)
			goto bad;
		minlen = 2 * sizeof(u_int32_t) + argp->fhsize;
	} else {
		argp->fhsize   = NFSX_V2FH;
		minlen = sizeof(u_int32_t) + argp->fhsize;
	}

	if (m->m_len < minlen) {
		m = m_pullup(m, minlen);
		if (m == NULL)
			return(EBADRPC);
		rdata = mtod(m, struct rdata *);
	}

	fh = (argp->flags & NFSMNT_NFSV3) ? rdata->fh.v3fh.fh : rdata->fh.v2fh;
	bcopy(fh, argp->fh, argp->fhsize);

	goto out;

bad:
	error = EBADRPC;

out:
	m_freem(m);
	return error;
}

#endif /* NETHER */
