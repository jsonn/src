/*    $NetBSD: nfs_boot.c,v 1.6.2.3 1994/08/11 23:48:28 mycroft Exp $ */

/*
 * Copyright (c) 1994 Adam Glass, Gordon Ross
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
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/reboot.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsv2.h>
#include <nfs/nfs.h>
#include <nfs/nfsdiskless.h>

#include "ether.h"
#if NETHER > 0

/*
 * Support for NFS diskless booting, specifically getting information
 * about where to boot from, what pathnames, etc.
 *
 * This implememtation uses RARP and the bootparam RPC.
 * We are forced to implement RPC anyway (to get file handles)
 * so we might as well take advantage of it for bootparam too.
 *
 * The diskless boot sequence goes as follows:
 * (1) Get our interface address using RARP
 *     (also save the address of the RARP server)
 * (2) Get our hostname using RPC/bootparam/whoami
 *     (all boopararms RPCs to the RARP server)
 * (3) Get the root path using RPC/bootparam/getfile
 * (4) Get the root file handle using RPC/mountd
 * (5) Get the swap path using RPC/bootparam/getfile
 * (6) Get the swap file handle using RPC/mountd
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
	u_char *fh));

/* other helpers */
static void get_path_and_handle __P((struct sockaddr_in *bpsin,
	char *key, struct nfs_dlmount *ndmntp));

/*
 * Called with an empty nfs_diskless struct to be filled in.
 */
int
nfs_boot_init(nd, procp)
	struct nfs_diskless *nd;
	struct proc *procp;
{
	struct ifreq ireq;
	struct in_addr my_ip, srv_ip, gw_ip;
	struct sockaddr_in bp_sin;
	struct sockaddr_in *sin;
	struct ifnet *ifp;
	struct socket *so;
	int error, len;
	u_short port;

#if 0
	/*
	 * XXX time must be non-zero when we init the interface or else
	 * the arp code will wedge... (Fixed in if_ether.c -gwr)
	 */
	if (time.tv_sec == 0)
		time.tv_sec = 1;
#endif

	/*
	 * Find an interface, rarp for its ip address, stuff it, the
	 * implied broadcast addr, and netmask into a nfs_diskless struct.
	 *
	 * This was moved here from nfs_vfsops.c because this procedure
	 * would be quite different if someone decides to write (i.e.) a
	 * BOOTP version of this file (might not use RARP, etc.) -gwr
	 */

	/*
	 * Find a network interface.
	 * XXX - This should use the specified boot device.
	 */
	for (ifp = ifnet; ifp; ifp = ifp->if_next)
		if ((ifp->if_flags & (IFF_LOOPBACK|IFF_POINTOPOINT)) == 0)
			break;
	if (ifp == NULL)
		panic("nfs_boot: no suitable interface");
	sprintf(ireq.ifr_name, "%s%d", ifp->if_name, ifp->if_unit);
	printf("nfs_boot: using network interface '%s'\n",
	    ireq.ifr_name);

	/*
	 * Bring up the interface.
	 */
	if ((error = socreate(AF_INET, &so, SOCK_DGRAM, 0)) != 0)
		panic("nfs_boot: socreate, error=%d", error);
	ireq.ifr_flags = IFF_UP;
	error = ifioctl(so, SIOCSIFFLAGS, (caddr_t)&ireq, procp);
	if (error)
		panic("nfs_boot: SIFFLAGS, error=%d", error);

	/*
	 * Do RARP for the interface address.  Also
	 * save the server address for bootparam RPC.
	 */
	if ((error = revarpwhoarewe(ifp, &srv_ip, &my_ip)) != 0)
		panic("revarp failed, error=%d", error);
	printf("nfs_boot: client=0x%x, server=0x%x\n",
	    ntohl(my_ip.s_addr), ntohl(srv_ip.s_addr));

	/*
	 * Do enough of ifconfig(8) so that the chosen interface can
	 * talk to the server(s).  (also get brcast addr and netmask)
	 */
	/* Set interface address. */
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
	 */
	bzero((caddr_t)&bp_sin, sizeof(bp_sin));
	bp_sin.sin_len = sizeof(bp_sin);
	bp_sin.sin_family = AF_INET;
	bp_sin.sin_addr.s_addr = srv_ip.s_addr;
	hostnamelen = MAXHOSTNAMELEN;

	/* this returns gateway IP address */
	error = bp_whoami(&bp_sin, &my_ip, &gw_ip);
	if (error)
		panic("nfs_boot: bootparam whoami, error=%d", error);
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

	get_path_and_handle(&bp_sin, "root", &nd->nd_root);
	get_path_and_handle(&bp_sin, "swap", &nd->nd_swap);

	return (0);
}

static void
get_path_and_handle(bpsin, key, ndmntp)
	struct sockaddr_in *bpsin;	/* bootparam server */
	char *key;			/* root or swap */
	struct nfs_dlmount *ndmntp;	/* output */
{
	char pathname[MAXPATHLEN];
	char *sp, *dp, *endp;
	int error;

	/*
	 * Get server:pathname for "key" (root or swap)
	 * using RPC to bootparam/getfile
	 */
	error = bp_getfile(bpsin, key, &ndmntp->ndm_saddr,
	    ndmntp->ndm_host, pathname);
	if (error)
		panic("nfs_boot: bootparam get %s: %d", key, error);
	printf("%s on %s:%s\n", key, ndmntp->ndm_host, pathname);

	/*
	 * Get file handle for "key" (root or swap)
	 * using RPC to mountd/mount
	 */
	error = md_mount(&ndmntp->ndm_saddr, pathname, ndmntp->ndm_fh);
	if (error)
		panic("nfs_boot: mountd %s, error=%d", key, error);

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
 * Get an mbuf with the given length, and
 * initialize the pkthdr length field.
 */
static struct mbuf *
m_get_len(int msg_len)
{
	struct mbuf *m;
	m = m_gethdr(M_WAIT, MT_DATA);
	if (m == NULL)
		return NULL;
	if (msg_len > MHLEN) {
		if (msg_len > MCLBYTES)
			panic("nfs_boot: msg_len > MCLBYTES");
		MCLGET(m, M_WAIT);
		if (m == NULL)
			return NULL;
	}
	m->m_len = msg_len;
	m->m_pkthdr.len = m->m_len;
	return (m);
}


/*
 * String representation for RPC.
 */
struct rpc_string {
	u_long len;		/* length without null or padding */
	u_char data[4];	/* data (longer, of course) */
    /* data is padded to a long-word boundary */
};
/* Compute space used given string length. */
#define	RPC_STR_SIZE(slen) (4 + ((slen + 3) & ~3))

/*
 * Inet address in RPC messages
 * (Note, really four longs, NOT chars.  Blech.)
 */
struct bp_inaddr {
	u_long  atype;
	long	addr[4];
};


/*
 * RPC definitions for bootparamd
 * (XXX - move to a header file?)
 */
#define	BOOTPARAM_PROG		100026
#define	BOOTPARAM_VERS		1
#define BOOTPARAM_WHOAMI	1
#define BOOTPARAM_GETFILE	2


/*
 * RPC: bootparam/whoami
 * Given client IP address, get:
 *	client name	(hostname)
 *	domain name (domainname)
 *	gateway address
 *
 * Setting the hostname and domainname here may be somewhat
 * controvercial, but it is so easy to do it here. -gwr
 */
static int
bp_whoami(bpsin, my_ip, gw_ip)
	struct sockaddr_in *bpsin;
	struct in_addr *my_ip;
	struct in_addr *gw_ip;
{
	/* The RPC structures */
	struct bp_inaddr *bia;
	struct rpc_string *str;
	struct mbuf *m;
	struct sockaddr_in *sin;
	int error, msg_len;
	int cn_len, dn_len;
	u_char *p;

	/*
	 * Get message buffer of sufficient size.
	 */
	msg_len = sizeof(*bia);
	m = m_get_len(msg_len);
	if (m == NULL)
		return ENOBUFS;

	/*
	 * Build request message.
	 */
	/* client IP address */
	bia = mtod(m, struct bp_inaddr *);
	bia->atype = htonl(1);
	p = (u_char*)my_ip;	/* ugh! */
	bia->addr[0] = htonl(*p);
	p++;
	bia->addr[1] = htonl(*p);
	p++;
	bia->addr[2] = htonl(*p);
	p++;
	bia->addr[3] = htonl(*p);
	p++;

	/* RPC: bootparam/whoami */
	error = krpc_call((struct sockaddr *)bpsin,
	    BOOTPARAM_PROG, BOOTPARAM_VERS, BOOTPARAM_WHOAMI, &m);
	if (error)
		return error;

	/*
	 * Parse result message.
	 */
	msg_len = m->m_len;
	p = mtod(m, char *);

	/* client name */
	if (msg_len < sizeof(*str))
		goto bad;
	str = (struct rpc_string *)p;
	cn_len = ntohl(str->len);
	if (msg_len < cn_len)
		goto bad;
	if (cn_len >= MAXHOSTNAMELEN)
		goto bad;
	bcopy(str->data, hostname, cn_len);
	hostname[cn_len] = '\0';
	hostnamelen = cn_len;
	p += RPC_STR_SIZE(cn_len);
	msg_len -= RPC_STR_SIZE(cn_len);

	/* domain name */
	if (msg_len < sizeof(*str))
		goto bad;
	str = (struct rpc_string *)p;
	dn_len = ntohl(str->len);
	if (msg_len < dn_len)
		goto bad;
	if (dn_len >= MAXHOSTNAMELEN)
		goto bad;
	bcopy(str->data, domainname, dn_len);
	domainname[dn_len] = '\0';
	domainnamelen = dn_len;
	p += RPC_STR_SIZE(dn_len);
	msg_len -= RPC_STR_SIZE(dn_len);

	/* gateway address */
	if (msg_len < sizeof(*bia))
		goto bad;
	bia = (struct bp_inaddr *)p;
	if (bia->atype != htonl(1))
		goto bad;
	p = (u_char*)gw_ip;
	*p++ = ntohl(bia->addr[0]);
	*p++ = ntohl(bia->addr[1]);
	*p++ = ntohl(bia->addr[2]);
	*p++ = ntohl(bia->addr[3]);
	goto out;

bad:
	printf("nfs_boot: bootparam_whoami: bad reply\n");
	error = EBADRPC;

out:
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
	struct rpc_string *str;
	struct mbuf *m;
	struct bp_inaddr *bia;
	struct sockaddr_in *sin;
	u_char *p, *q;
	int error, msg_len;
	int cn_len, key_len, sn_len, path_len;

	/*
	 * Get message buffer of sufficient size.
	 */
	cn_len = hostnamelen;
	key_len = strlen(key);
	msg_len = 0;
	msg_len += RPC_STR_SIZE(cn_len);
	msg_len += RPC_STR_SIZE(key_len);
	m = m_get_len(msg_len);
	if (m == NULL)
		return ENOBUFS;

	/*
	 * Build request message.
	 */
	p = mtod(m, u_char *);
	bzero(p, msg_len);
	/* client name (hostname) */
	str = (struct rpc_string *)p;
	str->len = htonl(cn_len);
	bcopy(hostname, str->data, cn_len);
	p += RPC_STR_SIZE(cn_len);
	/* key name (root or swap) */
	str = (struct rpc_string *)p;
	str->len = htonl(key_len);
	bcopy(key, str->data, key_len);

	/* RPC: bootparam/getfile */
	error = krpc_call((struct sockaddr *)bpsin,
	    BOOTPARAM_PROG, BOOTPARAM_VERS, BOOTPARAM_GETFILE, &m);
	if (error)
		return error;

	/*
	 * Parse result message.
	 */
	p = mtod(m, u_char *);
	msg_len = m->m_len;

	/* server name */
	if (msg_len < sizeof(*str))
		goto bad;
	str = (struct rpc_string *)p;
	sn_len = ntohl(str->len);
	if (msg_len < sn_len)
		goto bad;
	if (sn_len >= MNAMELEN)
		goto bad;
	bcopy(str->data, serv_name, sn_len);
	serv_name[sn_len] = '\0';
	p += RPC_STR_SIZE(sn_len);
	msg_len -= RPC_STR_SIZE(sn_len);

	/* server IP address (mountd) */
	if (msg_len < sizeof(*bia))
		goto bad;
	bia = (struct bp_inaddr *)p;
	if (bia->atype != htonl(1))
		goto bad;
	sin = md_sin;
	bzero((caddr_t)sin, sizeof(*sin));
	sin->sin_len = sizeof(*sin);
	sin->sin_family = AF_INET;
	q = (u_char*) &sin->sin_addr;
	*q++ = ntohl(bia->addr[0]);
	*q++ = ntohl(bia->addr[1]);
	*q++ = ntohl(bia->addr[2]);
	*q++ = ntohl(bia->addr[3]);
	p += sizeof(*bia);
	msg_len -= sizeof(*bia);

	/* server pathname */
	if (msg_len < sizeof(*str))
		goto bad;
	str = (struct rpc_string *)p;
	path_len = ntohl(str->len);
	if (msg_len < path_len)
		goto bad;
	if (path_len >= MAXPATHLEN)
		goto bad;
	bcopy(str->data, pathname, path_len);
	pathname[path_len] = '\0';
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
md_mount(mdsin, path, fhp)
	struct sockaddr_in *mdsin;		/* mountd server address */
	char *path;
	u_char *fhp;
{
	/* The RPC structures */
	struct rpc_string *str;
	struct rdata {
		u_long	errno;
		u_char	fh[NFS_FHSIZE];
	} *rdata;
	struct mbuf *m;
	int error, mlen, slen;

	slen = strlen(path);
	mlen = RPC_STR_SIZE(slen);

	m = m_get_len(mlen);
	if (m == NULL)
		return ENOBUFS;
	str = mtod(m, struct rpc_string *);
	str->len = htonl(slen);
	bcopy(path, str->data, slen);

	/* Do RPC to mountd. */
	error = krpc_call((struct sockaddr *)mdsin,
	    RPCPROG_MNT, RPCMNT_VER1, RPCMNT_MOUNT, &m);
	if (error)
		return error;	/* message already freed */

	mlen = m->m_len;
	if (mlen < sizeof(*rdata))
		goto bad;
	rdata = mtod(m, struct rdata *);
	error = ntohl(rdata->errno);
	if (error)
		goto bad;
	bcopy(rdata->fh, fhp, NFS_FHSIZE);

	/* Set port number for NFS use. */
	error = krpc_portmap((struct sockaddr *)mdsin,
	    NFS_PROG, NFS_VER2, &mdsin->sin_port);
	goto out;

bad:
	error = EBADRPC;

out:
	m_freem(m);
	return error;
}

#else /* NETHER */

int nfs_boot_init(nd, procp)
	struct nfs_diskless *nd;
	struct proc *procp;
{
	panic("nfs_boot_init: no ether");
}

#endif /* NETHER */
