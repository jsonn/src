/*	$NetBSD: nfs_boot.c,v 1.30.2.2 1997/03/10 12:56:39 is Exp $	*/

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
	panic("nfs_boot_init: no ether");
}

void
nfs_boot_getfh(bpsin, key, ndmntp)
	struct sockaddr_in *bpsin;
	char *key;
	struct nfs_dlmount *ndmntp;
{
	/* can not get here */
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
	struct sockaddr_in *mdsin, char *servname, char *path));

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
		printf("nfs_boot: no suitable interface");
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
	if ((error = socreate(AF_INET, &so, SOCK_DGRAM, 0)) != 0)
		panic("nfs_boot: socreate, error=%d", error);
	error = ifioctl(so, SIOCGIFFLAGS, (caddr_t)&ireq, procp);
	if (error)
		panic("nfs_boot: GIFFLAGS, error=%d", error);
	ireq.ifr_flags |= IFF_UP;
	error = ifioctl(so, SIOCSIFFLAGS, (caddr_t)&ireq, procp);
	if (error)
		panic("nfs_boot: SIFFLAGS, error=%d", error);

	/*
	 * Do RARP for the interface address.
	 */
	if ((error = revarpwhoami(&my_ip, ifp)) != 0) {
		printf("revarp failed, error=%d", error);
		return (EIO);
	}
	printf("nfs_boot: client_addr=0x%x\n", (u_int32_t)ntohl(my_ip.s_addr));

	/*
	 * Do enough of ifconfig(8) so that the chosen interface
	 * can talk to the servers.  (just set the address)
	 */
	sin = (struct sockaddr_in *)&ireq.ifr_addr;
	bzero((caddr_t)sin, sizeof(*sin));
	sin->sin_len = sizeof(*sin);
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = my_ip.s_addr;
	error = ifioctl(so, SIOCSIFADDR, (caddr_t)&ireq, procp);
	if (error)
		panic("nfs_boot: set if addr, error=%d", error);

	soclose(so);

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

	/* this returns gateway IP address */
	error = bp_whoami(&bp_sin, &my_ip, &gw_ip);
	if (error) {
		printf("nfs_boot: bootparam whoami, error=%d", error);
		return (error);
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

	return (0);
}

void
nfs_boot_getfh(bpsin, key, ndmntp)
	struct sockaddr_in *bpsin;	/* bootparam server */
	char *key;			/* root or swap */
	struct nfs_dlmount *ndmntp;	/* output */
{
	struct nfs_args *args;
	char pathname[MAXPATHLEN];
	char *sp, *dp, *endp;
	struct sockaddr_in *sin;
	int error;

	args = &ndmntp->ndm_args;

	/* Initialize mount args. */
	bzero((caddr_t) args, sizeof(*args));
	args->addr     = &ndmntp->ndm_saddr;
	args->addrlen  = args->addr->sa_len;
#ifdef NFS_BOOT_TCP
	args->sotype   = SOCK_STREAM;
#else
	args->sotype   = SOCK_DGRAM;
#endif
	args->fh       = ndmntp->ndm_fh;
	args->hostname = ndmntp->ndm_host;
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

	sin = (void*)&ndmntp->ndm_saddr;

	/*
	 * Get server:pathname for "key" (root or swap)
	 * using RPC to bootparam/getfile
	 */
	error = bp_getfile(bpsin, key, sin, ndmntp->ndm_host, pathname);
	if (error)
		panic("nfs_boot: bootparam get %s: %d", key, error);

	/*
	 * Get file handle for "key" (root or swap)
	 * using RPC to mountd/mount
	 */
	error = md_mount(sin, pathname, args);
	if (error)
		panic("nfs_boot: mountd %s, error=%d", key, error);

	/* Set port number for NFS use. */
	/* XXX: NFS port is always 2049, right? */
#ifdef NFS_BOOT_TCP
retry:
#endif
	error = krpc_portmap(sin, NFS_PROG,
		    (args->flags & NFSMNT_NFSV3) ? NFS_VER3 : NFS_VER2,
		    (args->sotype == SOCK_STREAM) ? IPPROTO_TCP : IPPROTO_UDP,
		    &sin->sin_port);

	if (error || (sin->sin_port == htons(0))) {
#ifdef NFS_BOOT_TCP
		if (args->sotype == SOCK_STREAM) {
			args->sotype = SOCK_DGRAM;
			goto retry;
		}
#endif
		panic("nfs_boot: portmap NFS, error=%d", error);
	}

	/* Construct remote path (for getmntinfo(3)) */
	dp = ndmntp->ndm_host;
	endp = dp + MNAMELEN - 1;
	dp += strlen(dp);
	*dp++ = ':';
	for (sp = pathname; *sp && dp < endp;)
		*dp++ = *sp++;
	*dp = '\0';

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
bp_getfile(bpsin, key, md_sin, serv_name, pathname)
	struct sockaddr_in *bpsin;
	char *key;
	struct sockaddr_in *md_sin;
	char *serv_name;
	char *pathname;
{
	struct mbuf *m;
	struct sockaddr_in *sin;
	struct in_addr inaddr;
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
	sn_len = MNAMELEN-1;
	m = xdr_string_decode(m, serv_name, &sn_len);
	if (m == NULL)
		goto bad;

	/* server IP address (mountd/NFS) */
	m = xdr_inaddr_decode(m, &inaddr);
	if (m == NULL)
		goto bad;

	/* server pathname */
	path_len = MAXPATHLEN-1;
	m = xdr_string_decode(m, pathname, &path_len);
	if (m == NULL)
		goto bad;

	/* setup server socket address */
	sin = md_sin;
	bzero((caddr_t)sin, sizeof(*sin));
	sin->sin_len = sizeof(*sin);
	sin->sin_family = AF_INET;
	sin->sin_addr = inaddr;

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
