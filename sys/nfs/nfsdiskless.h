/*	$NetBSD: nfsdiskless.h,v 1.16.20.1 2002/12/11 06:46:52 thorpej Exp $	*/

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
 *
 *	@(#)nfsdiskless.h	8.1 (Berkeley) 6/10/93
 */
#ifndef _NFS_NFSDISKLESS_H_
#define _NFS_NFSDISKLESS_H_

/*
 * Structure holds parameters needed by nfs_mountroot(),
 * which are filled in by nfs_boot_init() using either
 * BOOTP (RFC951, RFC1048) or Sun RPC/bootparams.  These
 * parameters are INET specific because nfs_boot_init()
 * currently supports only AF_INET protocols.
 *
 * NB: All fields are stored in net byte order to avoid hassles
 * with client/server byte ordering differences.
 */
struct nfs_dlmount {
	struct nfs_args ndm_args;
	struct sockaddr ndm_saddr;  		/* Address of file server */
	char		ndm_host[MNAMELEN]; 	/* server:pathname */
	u_char		ndm_fh[NFSX_V3FHMAX]; 	/* The file's file handle */
};
struct nfs_diskless {
	/* the interface used */
	struct ifnet *nd_ifp;
	/* A collection of IP addresses, for convenience. */
	struct in_addr nd_myip; /* My IP address */
	struct in_addr nd_mask; /* My netmask */
	struct in_addr nd_gwip; /* My gateway */
	/* Information for each mount point we need. */
	struct nfs_dlmount nd_root; 	/* Mount info for root */
};

int nfs_boot_init __P((struct nfs_diskless *nd, struct proc *procp));
void nfs_boot_cleanup __P((struct nfs_diskless *nd, struct proc *procp));
int nfs_boot_ifupdown __P((struct ifnet *, struct proc *, int));
int nfs_boot_setaddress __P((struct ifnet *, struct proc *,
			     u_int32_t, u_int32_t, u_int32_t));
int nfs_boot_deladdress __P((struct ifnet *, struct proc *, u_int32_t));
void nfs_boot_flushrt __P((struct ifnet *));
int nfs_boot_setrecvtimo __P((struct socket *));
int nfs_boot_enbroadcast __P((struct socket *));
int nfs_boot_sobind_ipport __P((struct socket *, u_int16_t));
int nfs_boot_sendrecv __P((struct socket *, struct mbuf *,
			   int (*)(struct mbuf*, void*, int), struct mbuf*,
			   int (*)(struct mbuf*, void*), struct mbuf**,
			   struct mbuf**, void*));

int nfs_bootdhcp  __P((struct nfs_diskless *, struct proc *));
int nfs_bootparam __P((struct nfs_diskless *, struct proc *));

#endif /* _NFS_NFSDISKLESS_H_ */
